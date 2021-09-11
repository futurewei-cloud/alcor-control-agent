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