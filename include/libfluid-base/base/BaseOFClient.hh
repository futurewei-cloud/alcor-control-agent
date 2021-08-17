#pragma once

#include <pthread.h>
#include <string>

#include "EventLoop.hh"
#include "BaseOFConnection.hh"

#include <stdio.h>

namespace fluid_base {

class BaseOFClient : public BaseOFHandler {
public:
    BaseOFClient(
            const std::string& addr,
            const bool domainsocket,
            const int port,
            const bool secure);
    virtual ~BaseOFClient();

    bool start(bool block = false);
    void stop();

    // BaseOFHandler methods
    // virtual void base_connection_callback(
    //         BaseOFConnection* conn,
    //         BaseOFConnection::Event event_type) override;
    // virtual void base_message_callback(BaseOFConnection* conn, void* data, size_t len);
    virtual void free_data(void* data) override;

protected:
    bool connect();

private:
    const std::string address;
    const bool domainsocket;
    const int port;
    const bool secure;

    bool blocking;

    EventLoop* evloop;
    pthread_t evthread;
    int nconn;

    class LibEventBaseOFClient;
    friend class LibEventBaseOFClient;
    LibEventBaseOFClient* m_implementation;
};

}  // namespace fluid_base