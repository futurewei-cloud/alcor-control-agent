#include <string.h>
#include <arpa/inet.h>

#include "libfluid-base/OFClient.hh"
#include "libfluid-base/base/BaseOFConnection.hh"
#include "libfluid-base/base/BaseOFServer.hh"
#include "libfluid-base/OFConnection.hh"
#include "libfluid-base/OFServer.hh"
#include "libfluid-base/base/of.hh"

namespace fluid_base {

OFClient::OFClient(
        const std::string& addr,
        const bool domainsocket,
        const int port,
        const bool secure,
        const struct OFServerSettings ofsc) :
        BaseOFClient(addr, domainsocket, port, secure),
        OFConnectionProcessor(this) {}

OFClient::~OFClient() {}

bool OFClient::start(bool block) {
    return BaseOFClient::start(block);
}

void OFClient::stop() {
    if (conn) {
        conn->close();
    }
    // Stop BaseOFClient
    BaseOFClient::stop();
}

void OFClient::set_config(OFServerSettings ofsc) {
    OFConnectionProcessor::set_config(ofsc);
}

void OFClient::base_message_callback(BaseOFConnection* c, void* data, size_t len) {
    OFConnectionProcessor::base_message_callback(c, data, len);
}

void OFClient::free_data(void* data) {
    BaseOFClient::free_data(data);
}

void OFClient::base_connection_callback(BaseOFConnection* c, BaseOFConnection::Event event_type) {
    OFConnectionProcessor::base_connection_callback(c, event_type);
    if (event_type == BaseOFConnection::EVENT_CLOSED) {
        // reconnect
        if (!this->connect()) {
            fprintf(stderr, "OFClient reconnect failed");
        } else {
            fprintf(stderr, "OFClient reconnect success");
        }
    }
}

void OFClient::on_new_conn(OFConnection* cc) {
    if (conn) {
        conn->close();
    }
    conn.reset(cc);
}
}  // namespace fluid_base
