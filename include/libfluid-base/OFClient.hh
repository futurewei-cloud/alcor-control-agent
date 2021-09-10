// Copyright (c) 2014 Open Networking Foundation
// Copyright 2020 Futurewei Cloud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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