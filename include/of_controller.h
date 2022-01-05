// MIT License
// Copyright(c) 2020 Futurewei Cloud
//
//     Permission is hereby granted,
//     free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction,
//     including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons
//     to whom the Software is furnished to do so, subject to the following conditions:
//
//     The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
//     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//     WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include "of_message.h"

#undef OFP_ASSERT
#undef CONTAINER_OF
#undef ARRAY_SIZE
#undef ROUND_UP

#include "libfluid-base/OFServer.hh"
#include "libfluid-msg/of10msg.hh"
#include "libfluid-msg/of13msg.hh"

#include "marl/defer.h"
#include "marl/event.h"
#include "marl/scheduler.h"
#include "marl/waitgroup.h"

#include <arpa/inet.h>
#include <atomic>
#include <vector>
#include <chrono>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <pthread.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <set>
#include <stdlib.h>
#include <unordered_map>
#include <chrono>


using namespace fluid_base;
using namespace fluid_msg;

class OFController : public OFServer {
public:
    std::atomic<int> packet_in_counter;
    void print_packet_in_counter(){
        while(true){
            auto current_counter = this->packet_in_counter.load();
            std::cout<<"Current packet_in counter: " << current_counter <<std::endl;
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
        }
    };
    OFController(const std::unordered_map<uint64_t, std::string> switch_dpid_map,
                 const std::unordered_map<std::string, std::string> port_id_map,
                 const char* address = "0.0.0.0",
                 const int port = 1234,
                 const int nthreads = 4,
                 bool secure = false) :
            xid(0),
            switch_dpid_map(switch_dpid_map),
            port_id_map(port_id_map),
            OFServer(address, port, nthreads, secure,
                     OFServerSettings()
                         .supported_version(1) // OF version 1 is OF 1.0
                         .echo_interval(30)
                         .keep_data_ownership(false)) {
                             packet_in_counter = 0;
                            //  ACA_LOG_INFO("%s\n", "Inited packet_in_counter to zero");
                            std::cout<<"Inited packet_in_counter to zero"<<std::endl; 
                               auto packet_in_counter_thread = new std::thread(std::bind(
          &OFController::print_packet_in_counter, this));
                            packet_in_counter_thread->detach();
                          }

    ~OFController() = default;

    void stop() override;

    void message_callback(OFConnection* ofconn, uint8_t type, void* data, size_t len) override;

    void connection_callback(OFConnection* ofconn, OFConnection::Event type) override;

    OFConnection* get_instance(std::string bridge);

    OFConnection* get_instance(int of_connection_id);

    void add_switch_to_conn_map(std::string bridge, int ofconn_id, OFConnection* ofconn);

    void remove_switch_from_conn_map(std::string bridge);

    void remove_switch_from_conn_map(int ofconn_id);

    void remove_switch_from_conn_maps(std::string bridge, int ofconn_id);

    void setup_default_br_int_flows();

    void setup_default_br_tun_flows();

    void execute_flow(const std::string br, const std::string flow_str, const std::string action = "add");

    void packet_out(const char* br, const char* opt);

    void packet_out(int of_connection_id, const char* opt);


private:

    // tracking xid (ovs transaction id)
    std::atomic<uint32_t> xid;

    // k is bridge name like 'br-int', v is OFConnection* obj
    std::unordered_map<std::string, OFConnection*> switch_conn_map;

    // k is dpid like 1, 2, 3, v is OFConnection* obj
    std::unordered_map<int, OFConnection*> switch_id_connection_map;

    // k is ofconnection id like '0', v is bridge name associated with it
    std::unordered_map<int, std::string> switch_id_map;

    // k is dpid (query from ovs), v is bridge name associated with it
    std::unordered_map<uint64_t, std::string> switch_dpid_map;

    // k is port name like (patch-int/tun and vxlan-generic), v is ofport id of it from ovsdb
    std::unordered_map<std::string, std::string> port_id_map;

    std::mutex switch_map_mutex;
    std::mutex switch_id_connection_map_mutex;

    void send_flow(OFConnection *ofconn, ofmsg_ptr_t &&p);

    void send_packet_out(OFConnection *ofconn, ofbuf_ptr_t &&po);

    void send_bundle_flow_mods(OFConnection *ofconn, std::vector<ofmsg_ptr_t> flow_mods);
};
