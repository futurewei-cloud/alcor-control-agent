//
// Created by Administrator on 2020/10/12.
//
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

#ifndef ALCOR_CONTROL_AGENT_SECURITY_GROUP_OVS_H
#define ALCOR_CONTROL_AGENT_SECURITY_GROUP_OVS_H

#include "aca_security_group.h"


namespace aca_security_group {

#define TRANSIENT_TABLE 60
#define BASE_EGRESS_TABLE 71
#define RULES_EGRESS_TABLE 72
#define ACCEPT_OR_INGRESS_TABLE 73
#define BASE_INGRESS_TABLE 81
#define RULES_INGRESS_TABLE 82
#define ACCEPTED_EGRESS_TRAFFIC_TABLE 91
#define ACCEPTED_INGRESS_TRAFFIC_TABLE 92
#define DROPPED_TRAFFIC_TABLE 93
#define ACCEPTED_EGRESS_TRAFFIC_NORMAL_TABLE 94

#define ETHERTYPE_IP  0x0800
#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_IPV6 0x86dd

#define PROTO_NUM_ICMP 1
#define PROTO_NUM_TCP 6
#define PROTO_NUM_UDP 17

#define REG_PORT 5
#define REG_NET 6

#define BR_INT "br-int"
#define BR_TUN "br-tun"

#define FLOW_PRIORITY_BASE 70

class Aca_Security_Group_Ovs {
public:
    static Aca_Security_Group_Ovs &get_instance();
	void init_port_flows(Aca_Port &port);
	void clear_port_flows(Aca_Port &port);
	int create_port_security_group_rule(Aca_Port &port,
                                                    Aca_Security_Group_Rule &sg_rule);
    int update_port_security_group_rule(Aca_Port &port, Aca_Security_Group_Rule &new_sg_rule, Aca_Security_Group_Rule &old_sg_rule);
	int delete_port_security_group_rule(Aca_Port &port, Aca_Security_Group_Rule &sg_rule);
private:
	int get_vlan_by_segment_id(const int segment_id);
    void init_port_egress_flows(Aca_Port &port); 
    void init_port_ingress_flows(const Aca_Port &port); 
    int flow_priority_offset(Aca_Security_Group_Rule &sg_rule, bool conjunction);
    int get_dl_type_by_ether_type(uint32_t ether_type);
    string get_nw_proto_by_protocol(uint32_t protocol);
	int get_remote_group_conj_id(Aca_Security_Group_Rule &sg_rule);
    int build_flows_by_sg_rule(Aca_Port &port,Aca_Security_Group_Rule &sg_rule,bool has_actions, vector<string> &flows);
	int build_conjunction_flows(Aca_Port &port, Aca_Security_Group_Rule &sg_rule,bool need_actions, vector<string> &flows);
	int get_remote_group_ips(Aca_Security_Group *remote_group, vector<string> remote_ips);
	int build_flows_by_remote_ip(Aca_Port &port, Aca_Security_Group_Rule &sg_rule, string remote_ip, int conj_id, vector<string> &flows);
	int build_normal_flows(Aca_Port &port,Aca_Security_Group_Rule &sg_rule,bool need_actions, vector<string> &flows);
	int add_conjunction_actions(string _flow, int conj_id, int dimension, vector<string> &flows);
	int build_accept_flows(Aca_Port &port,Aca_Security_Group_Rule &sg_rule, int conj_id, vector<string> &flows);

	uint64_t conj_id_base;
	map<string, uint64_t> conj_ids;
};

}
#endif //ALCOR_CONTROL_AGENT_SECURITY_GROUP_OVS_H
