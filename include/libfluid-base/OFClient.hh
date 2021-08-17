#pragma once

#include "base/BaseOFConnection.hh"
#include "base/BaseOFClient.hh"
#include "OFServer.hh"
#include "OFConnection.hh"
#include "OFServerSettings.hh"
#include <memory>

namespace fluid_base {

class OFClient : private BaseOFClient, private OFConnectionProcessor, public OFHandler {
public:
    OFClient(
            const std::string& addr,
            const bool domainsocket,
            const int port,
            const bool secure,
            const struct OFServerSettings ofsc = OFServerSettings());
    virtual ~OFClient();

    virtual bool start(bool block = false);

    virtual void stop();

    void set_config(OFServerSettings ofsc);

    // virtual void connection_callback(OFConnection *conn, OFConnection::Event event_type){};
    // virtual void message_callback(OFConnection *conn, uint8_t type, void *data, size_t len){};
    virtual void free_data(void* data) final;

protected:
    void base_message_callback(BaseOFConnection* c, void* data, size_t len) final;
    void base_connection_callback(BaseOFConnection* c, BaseOFConnection::Event event_type) final;

    void on_new_conn(OFConnection* cc) final;

    std::unique_ptr<OFConnection> conn;
};

}  // namespace fluid_base