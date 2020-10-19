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

class Aca_Security_Group_Ovs {
public:
    static Aca_Security_Group_Ovs &get_instance();
	void init_port(Aca_Port &aca_port);
	int create_port_security_group_rule(Aca_Port &aca_port,
                                                    Aca_Security_Group_Rule &aca_sg_rule);
    int update_port_security_group_rule(Aca_Port &aca_port,
                                                    Aca_Security_Group_Rule &aca_sg_rule);
	int delete_port_security_group_rule(Aca_Port &aca_port,
                                                    Aca_Security_Group_Rule &aca_sg_rule);
private:
	int get_vlan_by_segment_id(const int segment_id);
    void init_port_egress_flows(Aca_Port &aca_port); 
    void init_port_ingress_flows(const Aca_Port &aca_port); 
    int get_flow_priority(Aca_Security_Group_Rule &aca_sg_rule, int conjunction);
    int get_dl_type_by_ether_type(uint32_t ether_type);
    string get_nw_proto_by_protocol(uint32_t protocol);
    void build_flows_from_sg_rule(Aca_Port &aca_port,
                                            Aca_Security_Group_Rule &aca_sg_rule,
                                            bool has_actions,
                                            vector<string> &flows);
};

}
#endif //ALCOR_CONTROL_AGENT_SECURITY_GROUP_OVS_H