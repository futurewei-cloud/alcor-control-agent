// Copyright 2019 The Alcor Authors.
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

#include<sstream>
#include "aca_log.h"
#include "aca_security_group_ovs.h"
#include "aca_security_group_manager.h"
#include "aca_ovs_control.h"

using namespace aca_ovs_control;

namespace aca_security_group {

static const char *ct_state[] = {
	"+est-rel-rpl",
	"+new-est"
};

Aca_Security_Group_Ovs &Aca_Security_Group_Ovs::get_instance()
{
    // It is instantiated on first use.
    // allocated instance is destroyed when program exits.
    static Aca_Security_Group_Ovs instance;
    return instance;
}

int Aca_Security_Group_Ovs::get_vlan_by_segment_id(const int segment_id)
{
    return segment_id;
}

void Aca_Security_Group_Ovs::init_port_egress_flows(Aca_Port &port) 
{
    ACA_OVS_Control &controller = ACA_OVS_Control::get_instance();
    char flow[128] = {0};

    string port_id = port.get_id();
    string vpc_id = port.get_vpc_id();
    const char *mac_address = port.get_mac_address().data();
    int ofport = 12;
    int patch_ofport = 13;

    int vni = 10;
    int local_vlan = get_vlan_by_segment_id(vni);


    // DHCP discovery
    sprintf(flow, "table=%d,priority=80,in_port=%d,reg%d=%d,dl_type=0x%04x,nw_proto=%d,"
        "tp_src=68,tp_dst=67,actions=resubmit(,%d)", BASE_EGRESS_TABLE, ofport, REG_PORT, 
        ofport, ETHERTYPE_IP, PROTO_NUM_UDP, ACCEPT_OR_INGRESS_TABLE);
    controller.add_flow(BR_INT, flow);

    // Drop DHCP response from vm
    sprintf(flow, "table=%d,priority=70,in_port=%d,reg%d=%d,dl_type=0x%04x,nw_proto=%d,"
        "tp_src=67,tp_dst=68,actions=resubmit(,%d)", BASE_EGRESS_TABLE, ofport, REG_PORT, 
        ofport, ETHERTYPE_IP, PROTO_NUM_UDP, DROPPED_TRAFFIC_TABLE);
    controller.add_flow(BR_INT, flow);

    // Drop all remaining egress connections
    sprintf(flow, "table=%d,priority=10,in_port=%d,reg%d=%d,actions=ct_clear,resubmit(,%d)", 
        BASE_EGRESS_TABLE, ofport, REG_PORT, ofport, DROPPED_TRAFFIC_TABLE);
    controller.add_flow(BR_INT, flow);

    sprintf(flow, "table=%d,priority=90,dl_type=0x%04x,reg%d=%d,ct_state=+new-est,"
        "actions=ct(commit,zone=NXM_NX_REG%d[0..15]),resubmit(,%d)", 
        ACCEPT_OR_INGRESS_TABLE, ETHERTYPE_IP, REG_PORT, ofport, REG_NET, ACCEPTED_EGRESS_TRAFFIC_TABLE);
    controller.add_flow(BR_INT, flow);
    
    sprintf(flow, "table=%d,priority=80,reg%d=%d,actions=resubmit(,%d)", 
        ACCEPT_OR_INGRESS_TABLE, REG_PORT, ofport, ACCEPTED_EGRESS_TRAFFIC_NORMAL_TABLE);
    controller.add_flow(BR_INT, flow);

    sprintf(flow, "table=%d,priority=12,dl_dst=%s,reg%d=%d,actions=output:%d", 
		ACCEPTED_EGRESS_TRAFFIC_NORMAL_TABLE, mac_address, REG_NET, local_vlan, ofport);
    controller.add_flow(BR_INT, flow);

    sprintf(flow, "table=%d,priority=10,dl_src=%s,dl_dst=00:00:00:00:00:00/01:00:00:00:00:00,"
        "reg%d=%d,actions=output:%d", 
        ACCEPTED_EGRESS_TRAFFIC_NORMAL_TABLE, mac_address, REG_NET, local_vlan, patch_ofport);
    controller.add_flow(BR_INT, flow);
}

void Aca_Security_Group_Ovs::init_port_ingress_flows(const Aca_Port &port) 
{
    ACA_OVS_Control &controller = ACA_OVS_Control::get_instance();
    char flow[128] = {0};

    int ofport = 12;
    
    sprintf(flow, "table=%d,priority=100,dl_type=0x%04x,reg%d=%d,actions=output:%d",
        BASE_INGRESS_TABLE, ETHERTYPE_ARP, REG_PORT, ofport, ofport);
    controller.add_flow(BR_INT, flow);

    sprintf(flow, "table=%d,priority=95,reg%d=%d,dl_type=0x%04x,nw_proto=%d,"
        "tp_src=67,tp_dst=68,actions=output:%d",
        BASE_INGRESS_TABLE, REG_PORT, ofport, ETHERTYPE_IP, PROTO_NUM_UDP, ofport);
    controller.add_flow(BR_INT, flow);

    sprintf(flow, "table=%d,priority=90,reg%d=%d,dl_type=0x%04x,ct_state=-trk,"
        "actions=ct(table=%d,zone=NXM_NX_REG%d[0..15])",
        BASE_INGRESS_TABLE, REG_PORT, ofport, ETHERTYPE_IP, RULES_INGRESS_TABLE, REG_NET);
    controller.add_flow(BR_INT, flow);

    sprintf(flow, "table=%d,ct_state=+trk,priority=80,reg%d=%d,actions=resubmit(,%d)",
        BASE_INGRESS_TABLE, REG_PORT, ofport, RULES_INGRESS_TABLE);
    controller.add_flow(BR_INT, flow);
}

void Aca_Security_Group_Ovs::init_port_flows(Aca_Port &port)
{
    ACA_OVS_Control &controller = ACA_OVS_Control::get_instance();
    char flow[128] = {0};
    
    string port_id = port.get_id();
    string vpc_id = port.get_vpc_id();
    int ofport = 12;
    int vni = 10;
    int local_vlan = get_vlan_by_segment_id(vni);
    
    sprintf(flow, "table=%d,priority=100,in_port=%d,actions=set_field:%d->reg%d,"
        "set_field:%d->reg%d,resubmit(,%d)", TRANSIENT_TABLE, ofport, ofport, 
        REG_PORT, local_vlan, REG_NET, BASE_EGRESS_TABLE);
    controller.add_flow(BR_INT, flow);

    // Allow address pairs
    vector<pair<string, string>> address_pairs = port.get_allow_address_pairs();
    for (auto iter = address_pairs.cbegin(); iter != address_pairs.cend(); iter++) {
        //pair<string, string> address_pair = aca_port.allow_address_pairs(i);
        const char * ip_address = iter->first.data();    
        const char *mac_address = iter->second.data(); 

        sprintf(flow, "table=%d,priority=90,dl_dst=%s,dl_vlan=%d,actions=set_field:%d->reg%d,"
            "set_field:%d->reg%d,strip_vlan,resubmit(,%d)", TRANSIENT_TABLE, mac_address, 
            local_vlan, ofport, REG_PORT, local_vlan, REG_NET, BASE_INGRESS_TABLE);
        controller.add_flow(BR_INT, flow);

        sprintf(flow, "table=%d,priority=95,in_port=%d,reg%d=%d,dl_src=%s,dl_type=0x%04x,"
            "arp_spa=%s,actions=resubmit(,%d)", BASE_EGRESS_TABLE, ofport, REG_PORT, ofport,
            mac_address, ETHERTYPE_ARP, ip_address, ACCEPTED_EGRESS_TRAFFIC_NORMAL_TABLE);
        controller.add_flow(BR_INT, flow);

        sprintf(flow, "table=%d,priority=65,in_port=%d,reg%d=%d,dl_src=%s,dl_type=0x%04x,"
            "nw_src=%s,actions=ct(table=%d,zone=NXM_NX_REG%d[0..15])", BASE_EGRESS_TABLE, ofport, 
            REG_PORT, ofport, mac_address, ETHERTYPE_IP, ip_address, RULES_EGRESS_TABLE, REG_NET);
        controller.add_flow(BR_INT, flow);

        sprintf(flow, "table=%d,priority=100,dl_dst=%s,reg_net=%d,"
            "actions=set_field:%d->reg%d,resubmit(,%d)", 
            ACCEPT_OR_INGRESS_TABLE, mac_address, local_vlan, ofport, REG_PORT, BASE_INGRESS_TABLE);
        controller.add_flow(BR_INT, flow);
    }
    
    init_port_egress_flows(port);

    init_port_ingress_flows(port);
}

void Aca_Security_Group_Ovs::clear_port_flows(Aca_Port &port)
{
	//TODO: delete all flows of this port
}
int Aca_Security_Group_Ovs::flow_priority_offset(Aca_Security_Group_Rule &sg_rule, 
														bool conjunction)
{
    
    int conj_offset = 4;
    Aca_Security_Group * remote_group = sg_rule.get_remote_group();
    Protocol protocol = sg_rule.get_protocol();
    
    if (remote_group == NULL || conjunction) {
        conj_offset = 0;
    } 

    if (protocol == UNKNOWN_PROTO) {
		return conj_offset;
    }

    uint32_t port_range_min = sg_rule.get_port_range_min();
    uint32_t port_range_max = sg_rule.get_port_range_max();

    if (protocol == PROTO_NUM_ICMP) {
        if (port_range_min == 0) {
            return conj_offset + 1;
        } else if (port_range_max == 0) {
            return conj_offset + 2;
        }
    }

    return conj_offset + 3;
}

int Aca_Security_Group_Ovs::get_dl_type_by_ether_type(uint32_t ethertype)
{
    return ethertype;
}

string Aca_Security_Group_Ovs::get_nw_proto_by_protocol(uint32_t protocol)
{
    switch (protocol) {
    	case PROTO_NUM_TCP:
			return "tcp";
    	case PROTO_NUM_UDP:
    	    return "udp";	
    	case PROTO_NUM_ICMP:
            return "icmp";
    	default:
    		ACA_LOG_ERROR("=====>wrong ether type\n");
    }

    return "";
}

int Aca_Security_Group_Ovs::build_normal_flows(Aca_Port &port,
                                                    Aca_Security_Group_Rule &sg_rule,
                                                    bool need_actions,
                                                    vector<string> &flows)
{
    stringstream match_fileds;
    int priority;
    string actions;
    string nw_src_dst;
    string tcp_udp_dst;
    
    uint64_t cookie = sg_rule.get_cookie();
    uint32_t dl_type = sg_rule.get_ethertype();
    uint32_t nw_proto = sg_rule.get_protocol();
    Direction direction = sg_rule.get_direction();
    uint32_t port_range_min = sg_rule.get_port_range_min();
    uint32_t port_range_max = sg_rule.get_port_range_max();
    string remote_ip_prefix = sg_rule.get_remote_ip_prefix();
    Aca_Security_Group * remote_group = sg_rule.get_remote_group();
    uint32_t ofport = port.get_ofport();

    priority = flow_priority_offset(sg_rule, remote_group != NULL) + FLOW_PRIORITY_BASE;
    //dl_type = get_dl_type_by_ether_type(ethertype);
    tcp_udp_dst = get_nw_proto_by_protocol(nw_proto);

    if (direction == INGRESS) {
        actions = "actions=output:" + ofport;
        match_fileds << "table=" << RULES_INGRESS_TABLE << ",";
    } else {
        actions = "actions=resubmit(," + to_string(ACCEPT_OR_INGRESS_TABLE) + ")";  
        match_fileds << "table=" << RULES_EGRESS_TABLE << ",";
    }

	if (need_actions) {
		match_fileds << "cookie=" << cookie << ",";
	} else {
		match_fileds << "cookie=" << cookie << "/-1,";
	}
	
    match_fileds << "priority=" << priority << ",";
    match_fileds << "reg" << REG_PORT << "=" << ofport << ",";

    if (dl_type > 0) {
		match_fileds << "dl_type=" << dl_type << ",";
    }

    if (remote_ip_prefix != "") {
        if (direction == INGRESS) {
            nw_src_dst = "nw_src=" + remote_ip_prefix ;
            match_fileds << "nw_src=" << remote_ip_prefix << ",";
        } else {
            nw_src_dst = "nw_dst=" + remote_ip_prefix;  
            match_fileds << "nw_dst=" << remote_ip_prefix << ",";
        }
    }
    
    if (nw_proto > 0) {
        match_fileds << "nw_proto=" << nw_proto << ",";
    }

	if (nw_proto == ICMP) {
		stringstream ss;
        string flow;

        if (port_range_min > 0) {
			ss << "icmp_type=" << port_range_min;
        }

        if (port_range_max > 0) {
			ss << "icmp_code=" << port_range_max;
        }
        
        flow = match_fileds.str() + ss.str();
        
        if (need_actions) {
			flow = flow + "," + actions;
        }
        
		flows.push_back(flow);
	} else {
	    for (uint32_t i = port_range_min; i <= port_range_max; i++) {
	        stringstream ss;
	        string flow;
	        
	        ss << tcp_udp_dst << "_dst=" << i;
	        flow = match_fileds.str() + ss.str();
	        
	        if (need_actions) {
				flow = flow + "," + actions;
	        }
	        
	        flows.push_back(flow);
	    }
    }

    return EXIT_SUCCESS;
}

int Aca_Security_Group_Ovs::get_remote_group_ips(Aca_Security_Group *remote_group, 
															vector<string> remote_ips)
{
	Aca_Security_Group_Manager manager = Aca_Security_Group_Manager::get_instance();
	set<string> &port_ids = remote_group->get_port_ids();

	for(set<string>::iterator it = port_ids.begin(); it != port_ids.end(); it++) {
        string port_id = *it;
		map<string, Aca_Port *> &ports = manager.get_ports();
		map<string, Aca_Port *>::iterator iter;
		Aca_Port *port;
		
		iter = ports.find(port_id);
		if (iter == ports.end()) {
			return EXIT_FAILURE;
		}

		port = iter->second;
		vector<string> &fixed_ips = port->get_fixed_ip();

		for (uint32_t j = 0; j < fixed_ips.size(); j++) {
			remote_ips.push_back(fixed_ips[j]);
		}
	}
	
	return EXIT_SUCCESS;
}

int Aca_Security_Group_Ovs::get_remote_group_conj_id(Aca_Security_Group_Rule &sg_rule)
{
	int offset;
	int conj_id;
	string key;
	map<string, uint64_t>::iterator iter;
	
	offset = flow_priority_offset(sg_rule, true);

	//TODU: use (sg_id, remote_sg_id, direction, ethertype) as key
	key = sg_rule.get_remote_group_id();
	
	iter = this->conj_ids.find(key);
	if (iter == this->conj_ids.end()) {
		this->conj_id_base += 8;
		conj_id = this->conj_id_base +  offset * 2;
		this->conj_ids[key] = conj_id;
	} else {
		conj_id = iter->second;
	}
	
	return conj_id;
}

int Aca_Security_Group_Ovs::build_flows_by_remote_ip(Aca_Port &port,
                                                    Aca_Security_Group_Rule &sg_rule,
                                                    string remote_ip,
                                                    int conj_id,
                                                    vector<string> &flows)
{
    stringstream match_fileds;
    string actions;
    int priority;
    int vni = 10;
    
    uint64_t cookie = sg_rule.get_cookie();
    uint32_t dl_type = sg_rule.get_ethertype();
    Direction direction = sg_rule.get_direction();
    int local_vlan = get_vlan_by_segment_id(vni);
    
    if (direction == INGRESS) {
        match_fileds << "table=" << RULES_INGRESS_TABLE << ",";
    } else {
        match_fileds << "table=" << RULES_EGRESS_TABLE << ",";
    }

    priority = 70 + (conj_id % 8) / 2;

	match_fileds << "cookie=" << cookie << ",";
	match_fileds << "priority=" << priority << ",";
	match_fileds << "dl_type=" << dl_type << ",";
	match_fileds << "reg" << REG_NET << "=" << local_vlan << ",";

    if (direction == INGRESS) {
        match_fileds << "nw_src=" << remote_ip << ",";
    } else {
        match_fileds << "nw_dst=" << remote_ip << ",";
    }
    
	return add_conjunction_actions(match_fileds.str(), conj_id, 1, flows);
}

int Aca_Security_Group_Ovs::add_conjunction_actions(string flow, 
													int conj_id, 
													int dimension, 
													vector<string> &flows)
{
	for (int i = 0; i < 2; i++) {
    	stringstream s_flow;
    	string flow;
    	
    	s_flow << "ct_state=" << ct_state[i] << ",actions=conjunction(" \
    		<< conj_id << "," << dimension << "/2)";
		
		flows.push_back(flow + s_flow.str());
    } 

    return EXIT_SUCCESS;
}

int Aca_Security_Group_Ovs::build_accept_flows(Aca_Port &port,
                                                    Aca_Security_Group_Rule &sg_rule,
                                                    int conj_id,
                                                    vector<string> &flows)
{	
    stringstream match_fileds;
    int priority;
	string actions;
	string flow;
	int vni = 10;
	
    uint64_t cookie = sg_rule.get_cookie();
    uint32_t dl_type = sg_rule.get_ethertype();
    Direction direction = sg_rule.get_direction();
    uint32_t ofport = port.get_ofport();
    int local_vlan = get_vlan_by_segment_id(vni);

    if (direction == INGRESS) {
        match_fileds << "table=" << RULES_INGRESS_TABLE << ",";
        actions = "actions=output:" + ofport;
    } else {
        match_fileds << "table=" << RULES_EGRESS_TABLE << ",";
        actions = "actions=resubmit(," + to_string(ACCEPT_OR_INGRESS_TABLE) + ")";  
    }

    priority = (conj_id % 8) / 2;

   	match_fileds << "cookie=" << cookie << ",";
    match_fileds << "priority=" << priority << ",";
	match_fileds << "dl_type=" << dl_type << ",";
	match_fileds << "reg" << REG_NET << "=" << local_vlan << ",";
	
	for (int i = 0; i < 2; i++) {
		stringstream s_flow;

		match_fileds << "ct_state=" << ct_state[i] << ",";
		if (i == 1 && direction == INGRESS) {
			s_flow << match_fileds.str() << "conj_id=" << conj_id + 1 << ",";
			s_flow << "ct(comit,zone=NXM_NX_REG" << REG_NET << "[0..15])," << actions \
					<< ",resubmit(," << ACCEPTED_INGRESS_TRAFFIC_TABLE << ")";
		} else {
			s_flow << match_fileds.str() << "conj_id=" << conj_id << "," << actions;
		}

		flows.push_back(s_flow.str());
	}

	return EXIT_SUCCESS;
}

int Aca_Security_Group_Ovs::build_conjunction_flows(Aca_Port &port,
                                                    Aca_Security_Group_Rule &sg_rule,
                                                    bool need_actions,
                                                    vector<string> &flows)
{
	int conj_id;
	vector<string> remote_ips;
	vector<string> normal_flows;
	Aca_Security_Group *remote_group;
	
	remote_group = sg_rule.get_remote_group();
	if (remote_group == NULL) {
		return EXIT_FAILURE;
	}

	get_remote_group_ips(remote_group, remote_ips);
	conj_id = get_remote_group_conj_id(sg_rule);

	for (uint32_t i = 0; i < remote_ips.size(); i++) {
		build_flows_by_remote_ip(port, sg_rule, remote_ips[i], conj_id, flows);
	}

	build_normal_flows(port, sg_rule, true, normal_flows);

	for (uint32_t i = 0; i < normal_flows.size(); i++) {
		add_conjunction_actions(normal_flows[i], conj_id, 2, flows);
	}

	return build_accept_flows(port, sg_rule, conj_id, flows);
}


int Aca_Security_Group_Ovs::build_flows_by_sg_rule(Aca_Port &port,
                                                    Aca_Security_Group_Rule &sg_rule,
                                                    bool need_actions,
                                                    vector<string> &flows)
{
	if (sg_rule.get_remote_group() != NULL) {
		return build_conjunction_flows(port, sg_rule, need_actions, flows);
	} 

	return build_normal_flows(port, sg_rule, need_actions, flows);
}

int Aca_Security_Group_Ovs::create_port_security_group_rule(Aca_Port &port,
                                                    			Aca_Security_Group_Rule &sg_rule)
{
	vector<string> flows;
	ACA_OVS_Control &controller = ACA_OVS_Control::get_instance();

	build_flows_by_sg_rule(port, sg_rule, true, flows);

	for (uint32_t i = 0; i < flows.size(); i++) {
        controller.add_flow(BR_INT, flows[i].data());
    }

    return EXIT_SUCCESS;
}

int Aca_Security_Group_Ovs::update_port_security_group_rule(Aca_Port &port,
                                                    	Aca_Security_Group_Rule &new_sg_rule, 
                                                    	Aca_Security_Group_Rule &old_sg_rule)
{
	vector<string> flows;
	ACA_OVS_Control &controller = ACA_OVS_Control::get_instance();

    build_flows_by_sg_rule(port, new_sg_rule, true, flows);

	for (uint32_t i = 0; i < flows.size(); i++) {
        controller.add_flow(BR_INT, flows[i].data());
    }

    //TODO: Consider port update
    delete_port_security_group_rule(port, old_sg_rule);

    return EXIT_SUCCESS;
}

int Aca_Security_Group_Ovs::delete_port_security_group_rule(Aca_Port &port,
                                                    			Aca_Security_Group_Rule &sg_rule)
{
	vector<string> flows;
	ACA_OVS_Control &controller = ACA_OVS_Control::get_instance();

	build_flows_by_sg_rule(port, sg_rule, false, flows);

	for (uint32_t i = 0; i < flows.size(); i++) {
        controller.del_flows(BR_INT, flows[i].data());
    }

    return EXIT_SUCCESS;
}

}
