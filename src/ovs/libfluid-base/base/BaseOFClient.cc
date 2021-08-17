#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>

#include <event2/thread.h>
#include <event2/util.h>
#include <event2/listener.h>

#include <string.h>

#include "libfluid-base/base/BaseOFClient.hh"
#include "libfluid-base/base/BaseOFConnection.hh"
#include "libfluid-base/base/EventLoop.hh"
#include "libfluid-base/TLS.hh"

#include <string>
#include <sstream>

namespace fluid_base {
#define OVS_CONN_SELECT_TIMEOUT 20

static bool evthread_use_pthreads_called = false;

class BaseOFClient::LibEventBaseOFClient {
private:
    friend class BaseOFClient;

    static void conn_cb(evutil_socket_t fd, const std::string &peer_address, void *arg);
    static void conn_error_cb(void *arg);
};

BaseOFClient::BaseOFClient(const std::string &addr, const bool d, const int p, const bool s) :
        address(addr),
        domainsocket(d),
        port(p),
        secure(s),
        blocking(false),
        evloop(nullptr),
        evthread(0),
        nconn(0),
        m_implementation(nullptr) {
    // Prepare libevent for threads
    // This will leave a small, insignificant leak for us.
    // See: http://archives.seul.org/libevent/users/Jul-2011/msg00028.html
    if (!evthread_use_pthreads_called) {
        evthread_use_pthreads();
        evthread_use_pthreads_called = true;
    }

    // Ignore SIGPIPE so it becomes an EPIPE
    signal(SIGPIPE, SIG_IGN);

    m_implementation = new BaseOFClient::LibEventBaseOFClient;

#if defined(HAVE_TLS)
    if (this->secure && tls_obj == NULL) {
        fprintf(stderr, "To establish secure connections, call libfluid_tls_init first.\n");
    }
#endif
}

BaseOFClient::~BaseOFClient() {
    delete this->m_implementation;
    delete this->evloop;
}

bool BaseOFClient::start(bool block) {
    this->blocking = block;

    this->evloop = new EventLoop(0);

    // connect to ovs-db server and assign it to the event loop
    if (!this->connect()) {
        return false;
    }
    if (this->secure) {
        fprintf(stderr, "Secure ");
    }
    fprintf(stderr, "ovs client started (%s)\n", this->address.c_str());

    // start a new thread for event loop
    pthread_create(&evthread, NULL, EventLoop::thread_adapter, evloop);

    return true;
}

void BaseOFClient::stop() {
    // ask event loop to stop
    if (evloop) {
        evloop->stop();
    }

    // wait for event loop thread to finish
    if (evthread > 0) {
        pthread_join(evthread, NULL);
    }
}

bool BaseOFClient::connect() {
    int status = 0;
    evutil_socket_t fd;

    if (this->domainsocket) {
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
    } else {
        fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    }

    if (fd < 0) {
        fprintf(stderr, "Could not create socket to %s\n", this->address.c_str());
        close(fd);

        return false;
    }

    // if connect is non-blocking
    if (!this->blocking) {
        // set non-blocking mode socket flags
        int flags = fcntl(fd, F_GETFL, 0);
        int nonblocking_flags = flags | O_NONBLOCK;

        // set fd as non-blocking
        fcntl(fd, F_SETFL, nonblocking_flags);

        if (this->domainsocket) {
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(struct sockaddr_un));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, this->address.c_str(), sizeof(addr.sun_path) - 1);
            status = ::connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
        } else {
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr(this->address.c_str());
            addr.sin_port = htons(this->port);
            status = ::connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
        }

        if (status == 0) {
            // connect successfully immediately
            fcntl(fd, F_SETFL, flags);
        } else {
            if (errno != EINPROGRESS) {
                // if it did not connect immediately and errno != EINPROGRESS, it means there is
                // error
                fprintf(stderr,
                        "Could not connect to openflow server %s in non-blocking way, errno != "
                        "EINPROGRESS but = %d\n",
                        this->address.c_str(),
                        errno);
                close(fd);

                return false;
            } else {
                fd_set read_fds;
                fd_set write_fds;
                struct timeval select_timeout;

                FD_ZERO(&read_fds);
                FD_ZERO(&write_fds);
                FD_SET(fd, &read_fds);
                FD_SET(fd, &write_fds);

                select_timeout.tv_sec = OVS_CONN_SELECT_TIMEOUT;
                select_timeout.tv_usec = 0;

                status = ::select(fd + 1, &read_fds, &write_fds, NULL, &select_timeout);

                if (status <= 0) {
                    fprintf(stderr,
                            "Could not connect to openflow server %s in non-blocking way, select "
                            "timeout or error %d\n",
                            this->address.c_str(),
                            status);
                    close(fd);

                    return false;
                }

                if (FD_ISSET(fd, &write_fds)) {
                    if (FD_ISSET(fd, &read_fds)) {
                        if (this->domainsocket) {
                            struct sockaddr_un addr;
                            memset(&addr, 0, sizeof(struct sockaddr_un));
                            addr.sun_family = AF_UNIX;
                            strncpy(addr.sun_path, this->address.c_str(), sizeof(addr.sun_path) - 1);
                            status = ::connect(
                                    fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
                        } else {
                            struct sockaddr_in addr;
                            memset(&addr, 0, sizeof(addr));
                            addr.sin_family = AF_INET;
                            addr.sin_addr.s_addr = inet_addr(this->address.c_str());
                            addr.sin_port = htons(this->port);
                            status = ::connect(
                                    fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                        }

                        if (status != 0) {
                            int error = 0;
                            socklen_t len = sizeof(errno);

                            // use getsockopt() to get fd error
                            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                                fprintf(stderr,
                                        "Could not get socket option of %s\n",
                                        this->address.c_str());
                                close(fd);

                                return false;
                            }

                            if (error != EISCONN) {
                                fprintf(stderr,
                                        "Could not connect to %s and error != EISCONN\n",
                                        this->address.c_str());
                                close(fd);

                                return false;
                            }
                        }
                    }
                } else {
                    fprintf(stderr,
                            "Could not connect to openflow server %s in non-blocking way\n",
                            this->address.c_str());
                    close(fd);

                    return false;
                }

                // connect successfully after select
                fcntl(fd, F_SETFL, flags);
            }
        }
    } else {
        if (this->domainsocket) {
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(struct sockaddr_un));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, this->address.c_str(), sizeof(addr.sun_path) - 1);
            status = ::connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
        } else {
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr(this->address.c_str());
            addr.sin_port = htons(this->port);
            status = ::connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
        }

        if (status < 0) {
            fprintf(stderr,
                    "Could not connect to openflow server %s, status is %d\n",
                    this->address.c_str(),
                    status);
            close(fd);

            return false;
        }
    }

    // make the socket non-blocking
    evutil_make_socket_nonblocking(fd);

    // create OvsdbConnection with connected socket fd and evloop
    this->m_implementation->conn_cb(fd, address, this);

    return true;
}

void BaseOFClient::free_data(void *data) {
    BaseOFConnection::free_data(data);
}

/* Internal libevent callbacks */
void BaseOFClient::LibEventBaseOFClient::conn_cb(
        evutil_socket_t fd,
        const std::string &peer_address,
        void *arg) {
    auto client = static_cast<BaseOFClient *>(arg);
    int id = client->nconn++;

    BaseOFConnection *c =
            new BaseOFConnection(id, client, client->evloop, fd, client->secure, peer_address);
}

void BaseOFClient::LibEventBaseOFClient::conn_error_cb(void *arg) {
    int err = EVUTIL_SOCKET_ERROR();
    fprintf(stderr,
            "BaseOFClient connection error (%d: %s)",
            err,
            evutil_socket_error_to_string(err));
}

}  // namespace fluid_base
