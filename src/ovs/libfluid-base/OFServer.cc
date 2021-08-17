#include <string.h>
#include <arpa/inet.h>

#include "libfluid-base/base/BaseOFConnection.hh"
#include "libfluid-base/base/BaseOFServer.hh"
#include "libfluid-base/OFConnection.hh"
#include "libfluid-base/OFServer.hh"
#include "libfluid-base/base/of.hh"

namespace fluid_base {
OFConnectionProcessor::OFConnectionProcessor(OFHandler* h) : _handler(h) {}

void OFConnectionProcessor::set_config(OFServerSettings ofsc) {
    this->ofsc = ofsc;
}

void OFConnectionProcessor::free_data(void* data) {
    _handler->free_data(data);
}

OFServer::OFServer(
        const char* address,
        const int port,
        const int nthreads,
        const bool secure,
        const OFServerSettings ofsc) :
        BaseOFServer(address, port, nthreads, secure),OFConnectionProcessor(this) {
    pthread_mutex_init(&ofconnections_lock, NULL);
    this->set_config(ofsc);
}

OFServer::~OFServer() {
    this->lock_ofconnections();
    while (!this->ofconnections.empty()) {
        OFConnection* ofconn = this->ofconnections.begin()->second;
        this->ofconnections.erase(this->ofconnections.begin());
        delete ofconn;
    }
    this->ofconnections.clear();
    this->unlock_ofconnections();
}

bool OFServer::start(bool block) {
    return BaseOFServer::start(block);
}

void OFServer::stop() {
    // Close all connections
    this->lock_ofconnections();
    for (std::map<int, OFConnection*>::iterator it = this->ofconnections.begin();
         it != this->ofconnections.end();
         it++) {
         it->second->close();
    }
    this->unlock_ofconnections();

    // Stop BaseOFServer
    BaseOFServer::stop();
}

OFConnection* OFServer::get_ofconnection(int id) {
    this->lock_ofconnections();
    OFConnection* cc = ofconnections[id];
    this->unlock_ofconnections();
    return cc;
}

void OFServer::set_config(OFServerSettings ofsc) {
    this->ofsc = ofsc;
    OFConnectionProcessor::set_config(ofsc);
}

static uint32_t version_bitmap_from_version(uint8_t ofp_version) {
    return ((ofp_version < 32 ? 1u << ofp_version : 0) - 1) << 1;
}

void OFServer::base_message_callback(BaseOFConnection* c, void* data, size_t len) {
    OFConnectionProcessor::base_message_callback(c, data, len);
}

void OFConnectionProcessor::base_message_callback(BaseOFConnection* c, void* data, size_t len) {
    uint8_t version = ((uint8_t*) data)[0];
    uint8_t type = ((uint8_t*) data)[1];
    OFConnection* cc = (OFConnection*) c->get_manager();
    
    // We trust that the other end is using the negotiated protocol version
    // after the handshake is done. Should we?

    // Should we only answer echo requests after a features reply? The
    // specification isn't clear about that, so we answer whenever an echo
    // request arrives.

    // Handle echo requests
    if (type == OFPT_ECHO_REQUEST) {
        // Just change the type and send back
        ((uint8_t*) data)[1] = OFPT_ECHO_REPLY;
        c->send(data, ntohs(((uint16_t*) data)[1]));

        if (ofsc.dispatch_all_messages()) goto dispatch; else goto done;
    }

    // Handle hello messages
    if (ofsc.handshake() and type == OFPT_HELLO) {

        uint32_t client_supported_versions;

        if (ofsc.use_hello_elements() &&
            len > 8 &&
            ntohs(((uint16_t*) data)[4]) == OFPHET_VERSIONBITMAP &&
            ntohs(((uint16_t*) data)[5]) >= 8) {
            client_supported_versions = ntohl(((uint32_t*) data)[3]);
        }
        else {
            client_supported_versions = version_bitmap_from_version(version);
        }

        if (*this->ofsc.supported_versions() & client_supported_versions) {
            struct ofp_fluid_header msg;
            //msg.version = ((uint8_t*) data)[0];
            msg.version = this->ofsc.max_supported_version();
            msg.type = OFPT_FEATURES_REQUEST;
            msg.length = htons(8);
            msg.xid = ((uint32_t*) data)[1];
            c->send(&msg, 8);
        }
        else {
            struct ofp_fluid_error_msg msg;
            msg.header.version = version;
            msg.header.type = OFPT_ERROR;
            msg.header.length = htons(12);
            msg.header.xid = ((uint32_t*) data)[1];
            msg.type = htons(OFPET_HELLO_FAILED);
            msg.code = htons(OFPHFC_INCOMPATIBLE);
            cc->send(&msg, 12);

            cc->close();
            cc->set_state(OFConnection::STATE_FAILED);
            _handler->connection_callback(cc, OFConnection::EVENT_FAILED_NEGOTIATION);
        }

        if (ofsc.dispatch_all_messages()) goto dispatch; else goto done;
    }

    // Handle echo replies (by registering them)
    if (ofsc.liveness_check() and type == OFPT_ECHO_REPLY) {
        if (ntohl(((uint32_t*) data)[1]) == ECHO_XID) {
            cc->set_alive(true);
        }

        if (ofsc.dispatch_all_messages()) goto dispatch; else goto done;
    }

    // Handle feature replies
    if (ofsc.handshake() and type == OFPT_FEATURES_REPLY) {
        cc->set_version(((uint8_t*) data)[0]);
        cc->set_state(OFConnection::STATE_RUNNING);
        if (ofsc.liveness_check())
            c->add_timed_callback(send_echo, ofsc.echo_interval() * 1000, cc);
        _handler->connection_callback(cc, OFConnection::EVENT_ESTABLISHED);

        goto dispatch;
    }

    goto dispatch;

    // Dispatch a message to the user callback and goto done
    dispatch:
        _handler->message_callback(cc, type, data, len);
        if (this->ofsc.keep_data_ownership())
            this->free_data(data);
        return;
        
    // Free the message (if necessary) and return
    done:
        this->free_data(data);
        return;
}

void OFServer::free_data(void* data) {
    BaseOFServer::free_data(data);
}

void OFServer::base_connection_callback(BaseOFConnection* c, BaseOFConnection::Event event_type) {
    OFConnectionProcessor::base_connection_callback(c, event_type);
}
void OFConnectionProcessor::base_connection_callback(BaseOFConnection* c, BaseOFConnection::Event event_type) {
    // If the connection was closed, destroy it
    // (BaseOFServer::base_connection_callback will do it for us).
    // There's no need to notify the user, since a BaseOFConnection::EVENT_DOWN
    // event already means a BaseOFConnection::EVENT_CLOSED will happen and
    // nothing should be expected from the connection anymore.
    if (event_type == BaseOFConnection::EVENT_CLOSED) {
        delete c;
        // TODO: delete the OFConnection? Currently we keep track of all
        // connections that have been started and their status. When a
        // connection is closed, pretty much all of its data is freed already,
        // so this isn't a big overhead, and so we keep the references to old
        // connections for the user.
        return;
    }

    if (event_type == BaseOFConnection::EVENT_UP) {
        if (ofsc.handshake()) {
            int msglen = 8;
            if (ofsc.use_hello_elements()) {
                msglen = 16;
            }

            uint8_t msg[msglen];

            struct ofp_hello* hello = (struct ofp_hello*) &msg;
            hello->header.version = this->ofsc.max_supported_version();
            hello->header.type = OFPT_HELLO;
            hello->header.length = htons(msglen);
            hello->header.xid = htonl(HELLO_XID);

            if (this->ofsc.max_supported_version() >= 4 && ofsc.use_hello_elements()) {
                struct ofp_hello_elem_versionbitmap* elm =
                    (struct ofp_hello_elem_versionbitmap*) (&msg[8]);
                elm->type = htons(OFPHET_VERSIONBITMAP);
                elm->length = htons(8);

                uint32_t* bitmaps = (uint32_t*) (&msg[12]);
                *bitmaps = htonl(*this->ofsc.supported_versions());
            }

            c->send(&msg, msglen);
        }
        
        OFConnection* cc = new OFConnection(c, _handler);
        on_new_conn(cc);
        _handler->connection_callback(cc, OFConnection::EVENT_STARTED);
    }
    else if (event_type == BaseOFConnection::EVENT_DOWN) {
        auto cc = static_cast<OFConnection*>(c->get_manager());
        _handler->connection_callback(cc, OFConnection::EVENT_CLOSED);
        cc->close();
    }
}

void OFServer::on_new_conn(OFConnection* cc) {
    lock_ofconnections();
    ofconnections[cc->get_id()] = cc;
    unlock_ofconnections();
}

/** This method will periodically send echo requests. */
void* OFConnectionProcessor::send_echo(void* arg) {
    OFConnection* cc = static_cast<OFConnection*>(arg);

    if (!cc->is_alive()) {
        cc->close();
        cc->get_ofhandler()->connection_callback(cc, OFConnection::EVENT_DEAD);
        return NULL;
    }

    uint8_t msg[8];
    memset((void*) msg, 0, 8);
    msg[0] = (uint8_t) cc->get_version();
    msg[1] = OFPT_ECHO_REQUEST;
    ((uint16_t*) msg)[1] = htons(8);
    ((uint32_t*) msg)[1] = htonl(ECHO_XID);

    cc->set_alive(false);
    cc->send(msg, 8);

    return NULL;
}

}
