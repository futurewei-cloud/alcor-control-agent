#include <set>

#include "of_controller.h"
#include "aca_log.h"
#include "aca_util.h"

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
        }
    } else if (type == fluid_msg::of13::OFPT_BARRIER_REPLY) {
        auto t = std::chrono::high_resolution_clock::now();
        ACA_LOG_INFO("OFController::message_callback - recv OFPT_BARRIER_REPLY on %ld\n", t.time_since_epoch().count());
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
        remove_switch_from_conn_map(ofconn->get_id());
        remove_switch_from_conn_map(bridge);
    } else if (type == OFConnection::EVENT_DEAD) {
        std::string bridge = switch_id_map[ofconn->get_id()];
        ACA_LOG_WARN("OFController::connection_callback - ovs connection id=%d closed due to inactivity, remove %s from switch map\n", ofconn->get_id(), bridge.c_str());
        remove_switch_from_conn_map(ofconn->get_id());
        remove_switch_from_conn_map(bridge);
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

void OFController::add_switch_to_conn_map(std::string bridge, int ofconn_id, OFConnection* ofconn) {
    switch_map_mutex.lock();
    if (switch_conn_map.find(bridge) != switch_conn_map.end()) {
        // if existing already, remove then insert to update
        remove_switch_from_conn_map(bridge);
    }
    switch_conn_map[bridge] = ofconn;

    if (switch_id_map.find(ofconn_id) != switch_id_map.end()) {
        // if existing already, remove then insert to update
        remove_switch_from_conn_map(ofconn_id);
    }
    switch_id_map[ofconn_id] = bridge;
    switch_map_mutex.unlock();

    ACA_LOG_INFO("OFController::add_switch_to_conn_map - ovs connection id=%d bridge=%s added to switch map\n",
                 ofconn->get_id(), bridge.c_str());
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

void OFController::send_packet(OFConnection *ofconn, ofmsg_ptr_t &&p) {
    p->set_xid(xid.fetch_add(1));
    auto buf = p->pack();

    if (!buf) {
        return;
    }

    ofconn->send(buf->data(), buf->len());
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