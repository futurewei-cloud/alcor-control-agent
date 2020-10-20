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

#ifndef ALCOR_CONTROL_AGENT_SECURITY_GROUP_H
#define ALCOR_CONTROL_AGENT_SECURITY_GROUP_H
#include <stdint.h>
#include <string>
#include <map>  
#include <vector>  

using namespace std;

namespace aca_security_group
{

enum OperationType {
	CREATE=1, 
	UPDATE, 
	DELETE,
	UNKNOWN_OPERATION,
};

enum Direction {
	INGRESS=1, 
	EGRESS, 
	UNKNOWN_DIRECTION,
};

enum Ethertype {
	IPV4=0x0800, 
	ARP=0x0806, 
	IPV6=0x86dd,
	UNKNOWN_ETHERTYPE,
};

enum Protocol {
	TCP=6, 
	UDP=17, 
	ICMP=1,
	UNKNOWN_PROTO
};

class Aca_Security_Group;

class Aca_Security_Group_Rule {
public:
    static Aca_Security_Group_Rule &get_instance();
    void set_id(string id);
	string get_id(void);
	void set_name(string name);
	string get_name(void);
	void set_cookie(uint64_t cookie);
	uint64_t get_cookie(void);
	void set_direction(Direction direction);
	Direction get_direction(void);
	void set_ethertype(Ethertype ethertype);
	Ethertype get_ethertype(void);
	void set_protocol(Protocol protocol);
	Protocol get_protocol(void);
	uint32_t get_port_range_min(void);
	void set_port_range_min(uint32_t port_range_min);
	uint32_t get_port_range_max(void);
	void set_port_range_max(uint32_t port_range_max);
	string get_remote_ip_prefix(void);
	void set_remote_ip_prefix(string remote_ip_prefix);
	void set_remote_group_id(string remote_group_id);
	string get_remote_group_id(void);
	void set_operation_type(OperationType operation_type);
	OperationType get_operation_type(void);
	void set_remote_group(Aca_Security_Group * remote_group);
	Aca_Security_Group * get_remote_group(void);

private:
	string id;
	string name;
	uint64_t cookie;
	Direction direction;
	Ethertype ethertype;
	Protocol protocol;
	uint32_t port_range_min;
	uint32_t port_range_max;
	string remote_ip_prefix;
	string remote_group_id;
	OperationType operation_type;
	Aca_Security_Group *remote_group;
};

class Aca_Security_Group {
public:
    void set_id(string id);
	string get_id(void);
	void set_name(string name);
	string get_name(void);
	void set_format_version(uint32_t format_version);
	uint32_t get_format_version(void);
	void set_revision_number(uint32_t revision_number);
	uint32_t get_revision_number(void);
	void set_vpc_id(string vpc_id);
	string get_vpc_id(void);
	void set_operation_type(OperationType operation_type);
	OperationType get_operation_type(void);
	void add_port_id(string port_id);
	void delete_port_id(string port_id);
	int get_port_num(void);
	vector<string> &get_port_ids(void);
	void add_security_group_rule(Aca_Security_Group_Rule *sg_rule);
	void update_security_group_rule(Aca_Security_Group_Rule *sg_rule);
	void delete_security_group_rule(string sg_rule_id);
	Aca_Security_Group_Rule* get_security_group_rule(string sg_rule_id);
	map<string, Aca_Security_Group_Rule *> get_security_group_rules();
	
private:
	string id;
	string name;
	uint32_t format_version;
	uint32_t revision_number;
	string vpc_id;
	OperationType operation_type;
	vector<string> port_ids;
	map<string, Aca_Security_Group_Rule *> rules;
};

class Aca_Port {
public:
    void set_id(string id);
	string get_id(void);
	void set_name(string name);
	string get_name(void);
	void set_ofport(uint32_t ofport);
	uint32_t get_ofport(void);
	void set_format_version(uint32_t format_version);
	uint32_t get_format_version(void);
	void set_revision_number(uint32_t revision_number);
	uint32_t get_revision_number(void);
	void set_vpc_id(string vpc_id);
	string get_vpc_id(void);
	void set_mac_address(string mac_address);
	string get_mac_address(void);
	void add_fixed_ip(string fixed_ip);
	vector<string> &get_fixed_ip(void);
	void add_security_group_id(string security_group_id);
	void delete_security_group_id(string security_group_id);
	int get_security_group_num(void);
	void add_allow_address_pair(string ip_address, string mac_address);
	int allow_address_pairs_size(void);
	vector<pair<string, string>> get_allow_address_pairs(void);
	void add_security_group(Aca_Security_Group *security_group);
	Aca_Security_Group *get_security_group(string sg_id);

private:
	string id;
	string name;
	uint32_t ofport;
	uint32_t vni;
	uint32_t format_version;
	uint32_t revision_number;
	string vpc_id;
	string mac_address;
	vector<string> fixed_ips;
	vector<string> security_group_ids;
	vector<pair<string, string>> allow_address_pairs;
	map<string, Aca_Security_Group *> security_groups;
};

}

#endif //ALCOR_CONTROL_AGENT_SECURITY_GROUP_H
