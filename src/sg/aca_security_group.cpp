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
#include "aca_security_group.h"

using namespace std;


namespace aca_security_group {

void Aca_Security_Group_Rule::set_id(string id) {
	this->id = id;
}

string Aca_Security_Group_Rule::get_id(void) {
	return this->id;
}

void Aca_Security_Group_Rule::set_name(string name) {
	this->name = name;
}


string Aca_Security_Group_Rule::get_name(void) {
	return this->name;
}

void Aca_Security_Group_Rule::set_cookie(uint64_t cookie) {
	this->cookie = cookie;
}

uint64_t Aca_Security_Group_Rule::get_cookie(void) {
	return this->cookie;
}

void Aca_Security_Group_Rule::set_direction(Direction direction) {
	this->direction = direction;
}

Direction Aca_Security_Group_Rule::get_direction(void) {
	return this->direction;
}

void Aca_Security_Group_Rule::set_ethertype(Ethertype ethertype) {
	this->ethertype = ethertype;

}
	
Ethertype Aca_Security_Group_Rule::get_ethertype(void) {
	return this->ethertype;
}

void Aca_Security_Group_Rule::set_protocol(Protocol protocol) {
	this->protocol = protocol;

}
Protocol Aca_Security_Group_Rule::get_protocol(void) {
	return this->protocol;
}

void Aca_Security_Group_Rule::set_port_range_min(uint32_t port_range_min) {
	this->port_range_min = port_range_min;
}

uint32_t Aca_Security_Group_Rule::get_port_range_min(void) {
	return this->port_range_min;
}

void Aca_Security_Group_Rule::set_port_range_max(uint32_t port_range_max) {
	this->port_range_max = port_range_max;
}

uint32_t Aca_Security_Group_Rule::get_port_range_max(void) {
	return this->port_range_max;
}

void Aca_Security_Group_Rule::set_remote_ip_prefix(string remote_ip_prefix) {
	this->remote_ip_prefix = remote_ip_prefix;
}

string Aca_Security_Group_Rule::get_remote_ip_prefix(void) {
	return this->remote_ip_prefix;
}

void Aca_Security_Group_Rule::set_remote_group_id(string remote_group_id) {
	this->remote_group_id = remote_group_id;
}

string Aca_Security_Group_Rule::get_remote_group_id(void) {
	return this->remote_group_id;
}

void Aca_Security_Group_Rule::set_operation_type(OperationType operation_type) {
	this->operation_type = operation_type;
}

OperationType Aca_Security_Group_Rule::get_operation_type(void) {
	return this->operation_type;
}

void Aca_Security_Group_Rule::set_remote_group(Aca_Security_Group * remote_group) {
	this->remote_group = remote_group;
}

Aca_Security_Group * Aca_Security_Group_Rule::get_remote_group(void) {
	return this->remote_group;
}

Aca_Security_Group::Aca_Security_Group() {
}

Aca_Security_Group::Aca_Security_Group(Aca_Security_Group &sg) {
	this->id = sg.get_id();
	this->name = sg.get_name();
	this->format_version = sg.get_format_version();
	this->revision_number = sg.get_revision_number();
	this->vpc_id = sg.get_vpc_id();
	this->operation_type = sg.get_operation_type();
}

void Aca_Security_Group::set_id(string id) {
	this->id = id;
}

string Aca_Security_Group::get_id(void) {
	return this->id;
}

void Aca_Security_Group::set_name(string name) {
	this->name = name;
}

string Aca_Security_Group::get_name(void) {
	return this->name;
}
void Aca_Security_Group::set_format_version(uint32_t format_version) {
	this->format_version = format_version;
}
uint32_t Aca_Security_Group::get_format_version(void) {
	return this->format_version;
}
void Aca_Security_Group::set_revision_number(uint32_t revision_number) {
	this->revision_number = revision_number;
}
uint32_t Aca_Security_Group::get_revision_number(void) {
	return this->revision_number;
}

void Aca_Security_Group::set_vpc_id(string vpc_id) {
	this->vpc_id = vpc_id;
}
string Aca_Security_Group::get_vpc_id(void) {
	return this->vpc_id;
}

void Aca_Security_Group::set_operation_type(OperationType operation_type) {
	this->operation_type = operation_type;
}

OperationType Aca_Security_Group::get_operation_type(void) {
	return this->operation_type;
}

void Aca_Security_Group::add_port_id(string port_id) {
	this->port_ids.insert(port_id);
}

void Aca_Security_Group::delete_port_id(string port_id) {
	this->port_ids.erase(port_id);
}

int Aca_Security_Group::get_port_num(void) {
	return this->port_ids.size();
}

set<string> &Aca_Security_Group::get_port_ids(void) {
	return this->port_ids;
}

void Aca_Security_Group::add_security_group_rule(Aca_Security_Group_Rule *sg_rule) {
	Aca_Security_Group_Rule *new_sg_rule = new Aca_Security_Group_Rule(*sg_rule);
	this->rules[new_sg_rule->get_id()] = new_sg_rule;
}

void Aca_Security_Group::update_security_group_rule(Aca_Security_Group_Rule *sg_rule) {
	Aca_Security_Group_Rule *old_sg_rule = get_security_group_rule(sg_rule->get_id());
	if (old_sg_rule != NULL) {
		delete old_sg_rule;
	}

	add_security_group_rule(sg_rule);
}

void Aca_Security_Group::delete_security_group_rule(string sg_rule_id) {
	Aca_Security_Group_Rule *old_sg_rule = get_security_group_rule(sg_rule_id);
	if (old_sg_rule != NULL) {
		this->rules.erase(sg_rule_id);
		delete old_sg_rule;
	}
}


Aca_Security_Group_Rule* Aca_Security_Group::get_security_group_rule(string sg_rule_id) {
	map<string, Aca_Security_Group_Rule *>::iterator iterator = this->rules.find(sg_rule_id);
	if (iterator != this->rules.end()) {
		return iterator->second;
	}

	return NULL;
}
map<string, Aca_Security_Group_Rule *> Aca_Security_Group::get_security_group_rules(void) {
	return this->rules;
}

Aca_Port::Aca_Port() {
}

Aca_Port::Aca_Port(Aca_Port &port) {
	this->id = port.get_id();
	this->name = port.get_name();
	this->ofport = port.get_ofport();
	this->vni = port.get_vni();
	this->format_version = port.get_format_version();
	this->revision_number = port.get_revision_number();
	this->vpc_id = port.get_vpc_id();
	this->mac_address = port.get_mac_address();
	this->fixed_ips = port.get_fixed_ip();
}

void Aca_Port::set_id(string id) {
	this->id = id;
}

string Aca_Port::get_id(void) {
	return this->id;
}

void Aca_Port::set_name(string name) {
	this->name = name;
}

string Aca_Port::get_name(void) {
	return this->name;
}

void Aca_Port::set_ofport(uint32_t ofport) {
	this->ofport = ofport;
}

uint32_t Aca_Port::get_ofport(void) {
	return this->ofport;
}

void Aca_Port::set_vni(uint32_t vni) {
	this->vni = vni;
}

uint32_t Aca_Port::get_vni(void) {
	return this->vni;
}

void Aca_Port::set_format_version(uint32_t format_version) {
	this->format_version = format_version;
}
uint32_t Aca_Port::get_format_version(void) {
	return this->format_version;
}
void Aca_Port::set_revision_number(uint32_t revision_number) {
	this->revision_number = revision_number;
}
uint32_t Aca_Port::get_revision_number(void) {
	return this->revision_number;
}

void Aca_Port::set_vpc_id(string vpc_id) {
	this->vpc_id = vpc_id;
}

string Aca_Port::get_vpc_id(void) {
	return this->vpc_id;
}

void Aca_Port::set_mac_address(string mac_address) {
	this->mac_address = mac_address;
}

string Aca_Port::get_mac_address(void) {
	return this->mac_address;
}

void Aca_Port::add_fixed_ip(string fixed_ip) {
	this->fixed_ips.push_back(fixed_ip);
}

vector<string> &Aca_Port::get_fixed_ip(void) {
	return this->fixed_ips;
}

void Aca_Port::add_security_group_id(string security_group_id) {
	this->security_group_ids.insert(security_group_id);
}

void Aca_Port::delete_security_group_id(string security_group_id) {
	this->security_group_ids.erase(security_group_id);
}

int Aca_Port::get_security_group_num(void) {
	return this->security_group_ids.size();
}

void Aca_Port::add_allow_address_pair(string ip_address, string mac_address) {
	pair<string, string> allow_address_pair(ip_address, mac_address);
	this->allow_address_pairs.push_back(allow_address_pair);
}

int Aca_Port::allow_address_pairs_size(void)
{
	return this->allow_address_pairs.size();
}

vector<pair<string, string>> Aca_Port::get_allow_address_pairs()
{
	return this->allow_address_pairs;
}

void Aca_Port::add_security_group(Aca_Security_Group *security_group) {
	this->security_groups.insert(pair<string, Aca_Security_Group *>(security_group->get_id(), security_group));
}

Aca_Security_Group *Aca_Port::get_security_group(string sg_id) {
	map<string, Aca_Security_Group *>::iterator iterator = this->security_groups.find(sg_id);
	if (iterator != this->security_groups.end()) {
		return iterator->second;
	}

	return NULL;
}


}
