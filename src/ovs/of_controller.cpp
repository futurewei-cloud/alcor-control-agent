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

#include <set>

#include "of_controller.h"
#include "aca_log.h"
#include "aca_util.h"
#include "aca_on_demand_engine.h"

using namespace fluid_base;
using namespace fluid_msg;

void OFController::stop() {
    switch_map_mutex.lock();

    for (auto iter: switch_conn_map) {
        // close all OFConnection
        if (NULL != iter.second) {
            iter.second->close();
        }
    }
    switch_conn_map.clear();
    switch_id_map.clear();

    switch_map_mutex.unlock();
}

void OFController::message_callback(OFConnection* ofconn, uint8_t type, void* data, size_t len) {
    if (type == fluid_msg::of13::OFPT_FEATURES_REPLY) {
        ACA_LOG_INFO("OFController::message_callback - ovs connection id=%d up\n", ofconn->get_id());

        fluid_msg::of13::FeaturesReply reply;
        auto err = reply.unpack((uint8_t *) data);
        if (err != 0) {
            ACA_LOG_ERROR("%s", "OFController::message_callback - failed to parse feature reply\n");
            return;
        } else {
            uint64_t dpid = reply.datapath_id();
            ACA_LOG_INFO("OFController::message_callback - ovs connection %d with dpid %ld\n", ofconn->get_id(), dpid);

            // parse which bridge is the connection from
            std::string bridge_name = switch_dpid_map[dpid];
            add_switch_to_conn_map(bridge_name, ofconn->get_id(), ofconn);

            // setup default flows for each bridge
            if (bridge_name == "br-int") {
                setup_default_br_int_flows();
            }

            if (bridge_name == "br-tun") {
                setup_default_br_tun_flows();
            }
        }
    } else if (type == fluid_msg::of13::OFPT_BARRIER_REPLY) {
        auto t = std::chrono::high_resolution_clock::now();
        ACA_LOG_INFO("OFController::message_callback - recv OFPT_BARRIER_REPLY on %ld\n", t.time_since_epoch().count());
    } else if (type == fluid_msg::of13::OFPT_PACKET_IN) {
        fluid_msg::of13::PacketIn *pin = new fluid_msg::of13::PacketIn();
        pin->unpack((uint8_t *)data);
        uint32_t in_port = pin->match().in_port()->value();
        marl::schedule([=] {
            aca_on_demand_engine::ACA_On_Demand_Engine::get_instance().parse_packet(
                    in_port,
                    (void *)pin->data());
            delete pin;
        });
    } else if (type == 33) { // OFPRAW_OFPT14_BUNDLE_CONTROL
        auto t = std::chrono::high_resolution_clock::now();

        BundleReplyMessage bundle_reply;
        bundle_reply.unpack(data);
        ACA_LOG_INFO("OFController::message_callback - recv bundle_ctrl_reply of type %ld of bundle id %ld on %ld\n",
                     bundle_reply.get_type(),
                     bundle_reply.get_bundle_id(),
                     t.time_since_epoch().count());
    }
}

void OFController::connection_callback(OFConnection* ofconn, OFConnection::Event type) {
    if (type == OFConnection::EVENT_STARTED) {
        ACA_LOG_INFO("OFController::connection_callback - ovs connection id=%d started\n", ofconn->get_id());
    } else if (type == OFConnection::EVENT_ESTABLISHED) {
        ACA_LOG_INFO("OFController::connection_callback - ovs connection ver=%d id=%d established\n", ofconn->get_version(), ofconn->get_id());
    } else if (type == OFConnection::EVENT_FAILED_NEGOTIATION) {
        ACA_LOG_ERROR("OFController::connection_callback - ovs connection id=%d failed version negotiation\n", ofconn->get_id());
    } else if (type == OFConnection::EVENT_CLOSED) {
        std::string bridge = switch_id_map[ofconn->get_id()];
        ACA_LOG_WARN("OFController::connection_callback - ovs connection id=%d closed by user, remove %s from switch map\n", ofconn->get_id(), bridge.c_str());
        remove_switch_from_conn_maps(bridge, ofconn->get_id());
    } else if (type == OFConnection::EVENT_DEAD) {
        std::string bridge = switch_id_map[ofconn->get_id()];
        ACA_LOG_WARN("OFController::connection_callback - ovs connection id=%d closed due to inactivity, remove %s from switch map\n", ofconn->get_id(), bridge.c_str());
        remove_switch_from_conn_maps(bridge, ofconn->get_id());
    }
}

OFConnection* OFController::get_instance(std::string bridge) {
    OFConnection* ofconn = NULL;

    switch_map_mutex.lock();
    ofconn = switch_conn_map[bridge];
    switch_map_mutex.unlock();

    if (NULL == ofconn) {
        ACA_LOG_ERROR("OFController::get_instance - switch %s not found\n", bridge.c_str());
    }

    return ofconn;
}

// Adding a connection should be an atomic action, meaning that both adding to
// switch_con_map and switch_id_map should be added under the same lock
void OFController::add_switch_to_conn_map(std::string bridge, int ofconn_id, OFConnection* ofconn) {
    switch_map_mutex.lock();
    auto ofconn_iter = switch_conn_map.find(bridge);

    // if found, remove
    if (ofconn_iter != switch_conn_map.end()) {
        if (NULL != ofconn_iter->second) { // k is bridge name, v is OFConnection*
            ofconn_iter->second->close();
        }
        ACA_LOG_DEBUG("Removed ofconn_name: %s from switch_conn_map, when adding a new connection with the same name\n", bridge.c_str());
        switch_conn_map.erase(bridge);
    }

    switch_conn_map[bridge] = ofconn;
    
    if (switch_id_map.find(ofconn_id) != switch_id_map.end()) {
        switch_id_map.erase(ofconn_id);
    }

    switch_id_map[ofconn_id] = bridge;
    
    switch_map_mutex.unlock();


    ACA_LOG_INFO("OFController::add_switch_to_conn_map - ovs connection id=%d bridge=%s added to switch map\n",
                 ofconn->get_id(), bridge.c_str());
}

// Removing a connection should be an atomic action, meaning that both removing from
// switch_con_map and switch_id_map should be added under the same lock
void OFController::remove_switch_from_conn_maps(std::string bridge, int ofconn_id){
    switch_map_mutex.lock();

    // if found, remove
    if (switch_id_map.find(ofconn_id) != switch_id_map.end()) {
        switch_id_map.erase(ofconn_id);
    }

    auto ofconn_iter = switch_conn_map.find(bridge);

    // if found, remove
    if (ofconn_iter != switch_conn_map.end()) {
        if (NULL != ofconn_iter->second) { // k is bridge name, v is OFConnection*
            ofconn_iter->second->close();
        }
        switch_conn_map.erase(bridge);
    }
    
    switch_map_mutex.unlock();

    ACA_LOG_INFO("OFController::remove_switch_from_conn_map - ovs connection bridge=%s removed from switch map\n",
                 bridge.c_str());
}

void OFController::remove_switch_from_conn_map(std::string bridge) {
    switch_map_mutex.lock();
    auto ofconn_iter = switch_conn_map.find(bridge);

    // if found, remove
    if (ofconn_iter != switch_conn_map.end()) {
        if (NULL != ofconn_iter->second) { // k is bridge name, v is OFConnection*
            ofconn_iter->second->close();
        }
        switch_conn_map.erase(bridge);
    }
    switch_map_mutex.unlock();

    ACA_LOG_INFO("OFController::remove_switch_from_conn_map - ovs connection bridge=%s removed from switch map\n",
                 bridge.c_str());
}

void OFController::remove_switch_from_conn_map(int ofconn_id) {
    switch_map_mutex.lock();
    if (switch_id_map.find(ofconn_id) != switch_id_map.end()) {
        switch_id_map.erase(ofconn_id);
    }
    switch_map_mutex.unlock();

    ACA_LOG_INFO("OFController::remove_switch_from_conn_map - ovs connection id=%d removed from switch map\n",
                 ofconn_id);
}

void OFController::send_flow(OFConnection *ofconn, ofmsg_ptr_t &&p) {
    p->set_xid(xid.fetch_add(1));
    auto buf = p->pack();

    if (!buf) {
        return;
    }

    ofconn->send(buf->data(), buf->len());
}

void OFController::send_packet_out(OFConnection *ofconn, ofbuf_ptr_t &&po) {
    if (!po) {
        return;
    }
    
    ofconn->send(po->data(), po->len());
}

void OFController::send_bundle_flow_mods(OFConnection *ofconn, std::vector<ofmsg_ptr_t> flow_mods) {
    xid.fetch_add(1);
    BundleFlowModMessage bundle(flow_mods, &xid);
    auto buf_open_req = bundle.pack_open_req();
    ofconn->send(buf_open_req->data(), buf_open_req->len());
    ACA_LOG_INFO("OFController::send_bundle_flow_mods - ovs connection id=%d send bundle open request of bundle_id %ld\n",
                 ofconn->get_id(), bundle.get_bundle_id());

    // handle flow-mods
    for (auto flow_mod : bundle.pack_flow_mods()) {
        ofconn->send(flow_mod->data(), flow_mod->len());
    }

    auto buf_commit_req = bundle.pack_commit_req();
    ofconn->send(buf_commit_req->data(), buf_commit_req->len());
    ACA_LOG_INFO("OFController::send_bundle_flow_mods - ovs connection id=%d send bundle commit request of bundle_id %ld\n",
                 ofconn->get_id(), bundle.get_bundle_id());
}

void OFController::setup_default_br_int_flows() {
    OFConnection* ofconn_br_int = get_instance("br-int");

    if (NULL != ofconn_br_int) {
        send_flow(ofconn_br_int, create_add_flow("table=0,priority=0, actions=NORMAL"));
    } else {
        ACA_LOG_ERROR("OFController::setup_default_br_int_flows - ovs connection not found\n");
    }

    ofconn_br_int = NULL;
}

void OFController::setup_default_br_tun_flows() {
    OFConnection* ofconn_br_tun = get_instance("br-tun");

    if (NULL != ofconn_br_tun) {
        send_flow(ofconn_br_tun, create_add_flow("table=0,priority=0, actions=NORMAL"));
        /*  Commented out this line so that the arp requests doesn't trigger the on-demnand workflow with NCM,
         *  and the packets will be sent to Arion Wings.*/
//        send_flow(ofconn_br_tun, create_add_flow("table=0,priority=50,arp,arp_op=1, actions=CONTROLLER"));
        send_flow(ofconn_br_tun, create_add_flow("table=0,priority=1,in_port=" + port_id_map["patch-int"] + " actions=resubmit(,2)"));
        send_flow(ofconn_br_tun, create_add_flow("table=2,priority=1,dl_dst=00:00:00:00:00:00/01:00:00:00:00:00 actions=resubmit(,20)"));
        send_flow(ofconn_br_tun, create_add_flow("table=2,priority=1,dl_dst=01:00:00:00:00:00/01:00:00:00:00:00 actions=resubmit(,22)"));
        /*  Changed this from CONTROLLER to resubmit to table 22, which is the Arion Wing group.*/
        send_flow(ofconn_br_tun, create_add_flow("table=20,priority=1 actions=resubmit(,22)"));
        send_flow(ofconn_br_tun, create_add_flow("table=2,priority=25,icmp,icmp_type=8,in_port=" + port_id_map["patch-int"] + " actions=resubmit(,52)"));
        send_flow(ofconn_br_tun, create_add_flow("table=52,priority=1 actions=resubmit(,20)"));
        send_flow(ofconn_br_tun, create_add_flow("table=0,priority=25,in_port=" + port_id_map["vxlan-generic"] + " actions=resubmit(,4)"));
    } else {
        ACA_LOG_ERROR("OFController::setup_default_br_tun_flows - ovs connection not found\n");
    }

    ofconn_br_tun = NULL;
}

void OFController::execute_flow(const std::string br, const std::string flow_str, const std::string action) {
    OFConnection* ofconn_br = get_instance(br);

    if (NULL != ofconn_br) {
        if (action == "add") {
            send_flow(ofconn_br, create_add_flow(flow_str));
        } else if (action == "mod") {
            // --strict mod
            send_flow(ofconn_br, create_mod_flow(flow_str, true));
        } else if (action == "del") {
            // --strict del
            send_flow(ofconn_br, create_del_flow(flow_str, true));
        } else {
            ACA_LOG_ERROR("OFController::execute_flow - action %s not supported in flow %s\n", action.c_str(), flow_str.c_str());
        }
    } else {
        ACA_LOG_ERROR("OFController::execute_flow - ovs connection to bridge %s not found\n", br.c_str());
    }

    ofconn_br = NULL;
}

void OFController::packet_out(const char* br, const char* opt) {
    OFConnection* ofconn_br = get_instance(std::string(br));

    if (NULL != ofconn_br) {
        send_packet_out(ofconn_br, create_packet_out(opt));
    } else {
        ACA_LOG_ERROR("OFController::packet_out - ovs connection to bridge %s not found\n", br);
    }

    ofconn_br = NULL;
}
