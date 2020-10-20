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

#include "aca_log.h"
#include "aca_security_group.h"
#include "aca_security_group_manager.h"
#include "aca_security_group_ovs.h"

using namespace std;

namespace aca_security_group {

Aca_Security_Group_Manager &Aca_Security_Group_Manager::get_instance()
{
	// It is instantiated on first use.
	// allocated instance is destroyed when program exits.
	static Aca_Security_Group_Manager instance;
	return instance;
}

map<string, Aca_Port *> &Aca_Security_Group_Manager::get_ports(void) 
{
	return this->ports;
}

map<string, Aca_Security_Group *> &Aca_Security_Group_Manager::get_security_groups(void) 
{
	return this->security_groups;
}

int Aca_Security_Group_Manager::set_remote_group(Aca_Security_Group_Rule &sg_rule)
{
	map<string, Aca_Security_Group *>::iterator iter;
	string remote_grou_id = sg_rule.get_remote_group_id();
	
	if (remote_grou_id != "") {
		iter = this->security_groups.find(remote_grou_id);
		if (iter == this->security_groups.end()) {
			return EXIT_FAILURE;
		}
		
		sg_rule.set_remote_group(iter->second);
	}

	return EXIT_SUCCESS;
}

int Aca_Security_Group_Manager::create_security_group_rule(Aca_Port &port,
													  Aca_Security_Group &sg,
                                                      Aca_Security_Group_Rule &sg_rule) 
{
	Aca_Security_Group_Ovs &sg_ovs = Aca_Security_Group_Ovs::get_instance();
	string rule_id;
	Aca_Security_Group_Rule *aca_sg_rule;
	rule_id = sg_rule.get_id();

	aca_sg_rule = sg.get_security_group_rule(rule_id);
	if (aca_sg_rule != NULL) {
		TRN_LOG_WARN("Security group rule(id:%s) already exist", rule_id.data());
		return update_security_group_rule(port, sg, sg_rule); 
	}

	if (set_remote_group(sg_rule) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	sg_rule.set_cookie(0);
	sg_ovs.create_port_security_group_rule(port, sg_rule);
	sg.add_security_group_rule(&sg_rule); 

	return EXIT_SUCCESS;
}

int Aca_Security_Group_Manager::update_security_group_rule(Aca_Port &port,
													  Aca_Security_Group &sg,
                                                      Aca_Security_Group_Rule &sg_rule) 
{	
	Aca_Security_Group_Ovs &sg_ovs = Aca_Security_Group_Ovs::get_instance();
	string rule_id;
	Aca_Security_Group_Rule *aca_sg_rule;
	rule_id = sg_rule.get_id();
	
	aca_sg_rule = sg.get_security_group_rule(rule_id);
	if (aca_sg_rule == NULL) {
		ACA_LOG_ERROR("Can not find security group rule by id:%s", rule_id.data());
		return EXIT_FAILURE; 
	}

	if (set_remote_group(sg_rule) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	sg_rule.set_cookie(aca_sg_rule->get_cookie() + 1);
	sg_ovs.update_port_security_group_rule(port, sg_rule);
	sg.update_security_group_rule(&sg_rule); 

	return EXIT_SUCCESS;
}

int Aca_Security_Group_Manager::delete_security_group_rule(Aca_Port &port,
													  Aca_Security_Group &sg,
                                                      Aca_Security_Group_Rule &sg_rule) 
{
	Aca_Security_Group_Ovs &sg_ovs = Aca_Security_Group_Ovs::get_instance();
	string rule_id;
	Aca_Security_Group_Rule *aca_sg_rule;
	rule_id = sg_rule.get_id();

	aca_sg_rule = sg.get_security_group_rule(rule_id);
	if (aca_sg_rule == NULL) {
		ACA_LOG_ERROR("Can not find security group rule by id:%s", rule_id.data());
		return EXIT_FAILURE; 
	}
	
	if (set_remote_group(sg_rule) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	sg_ovs.delete_port_security_group_rule(port, *aca_sg_rule);
	sg.delete_security_group_rule(sg_rule.get_id()); 

	return EXIT_SUCCESS;
}

int Aca_Security_Group_Manager::create_security_group(Aca_Port &input_port,
                                                 Aca_Security_Group &input_sg) 
 {
 	Aca_Security_Group_Ovs &sg_ovs = Aca_Security_Group_Ovs::get_instance();
 	map<string, Aca_Port *>::iterator piter;
	map<string, Aca_Security_Group *>::iterator siter;
	Aca_Security_Group *aca_sg;
	Aca_Port * aca_port;
	string port_id = input_port.get_id();
	string sg_id = input_sg.get_id();

	//TODO: maybe multi threads access this map
	piter = this->ports.find(port_id);
    if (piter == this->ports.end()) {
    	aca_port = new Aca_Port(input_port);
    	this->ports[port_id] = aca_port;
        sg_ovs.init_port(*aca_port);
    } else {
		aca_port = piter->second;
    }

    siter = this->security_groups.find(sg_id);
    if (siter == this->security_groups.end()) {
        aca_sg = new Aca_Security_Group(input_sg);
        this->security_groups[sg_id] = aca_sg;
    } else {
		aca_sg = siter->second;
    }

    aca_sg->add_port_id(port_id);
	aca_port->add_security_group_id(sg_id);

    for (auto &it : input_sg.get_security_group_rules()) {
        create_security_group_rule(*aca_port, *aca_sg, *(it.second));
    }
			
    return EXIT_SUCCESS;
}

int Aca_Security_Group_Manager::update_security_group(Aca_Port &input_port,
                                                      Aca_Security_Group &input_sg) 
{
	map<string, Aca_Port *>::iterator piter;
	map<string, Aca_Security_Group *>::iterator siter;
	Aca_Security_Group *aca_sg;
	Aca_Port * aca_port;

	string port_id = input_port.get_id();
	string sg_id = input_sg.get_id();

	//TODO: do we need to update the port ?
	piter = this->ports.find(port_id);
    if (piter == this->ports.end()) {
        ACA_LOG_ERROR("Can not find port by id:%s", port_id.data());
        return EXIT_FAILURE;
    }

    aca_port = piter->second;
    
    siter = this->security_groups.find(sg_id);
    if (siter == this->security_groups.end()) {
        ACA_LOG_ERROR("Can not find security group by id:%s", sg_id.data());
        return EXIT_FAILURE;
    }

    aca_sg = siter->second;
    
    for (auto &it : input_sg.get_security_group_rules()) {
        Aca_Security_Group_Rule *sg_rule = it.second;
		switch (sg_rule->get_operation_type()) {
			case CREATE:
				create_security_group_rule(*aca_port, *aca_sg, *sg_rule);
				break;
			case UPDATE:
				update_security_group_rule(*aca_port, *aca_sg, *sg_rule);
				break;
			case DELETE:
				delete_security_group_rule(*aca_port, *aca_sg, *sg_rule);
				break;
			default:
				ACA_LOG_ERROR("Invalid security group rule operation type");
		}
    }
	
    return EXIT_SUCCESS;
}

int Aca_Security_Group_Manager::delete_security_group(Aca_Port &input_port,
                                                      Aca_Security_Group &input_sg) 
{
	map<string, Aca_Port *>::iterator piter;
	map<string, Aca_Security_Group *>::iterator siter;
	Aca_Security_Group *aca_sg;
	Aca_Port *aca_port;

	string port_id = input_port.get_id();
	string sg_id = input_sg.get_id();

	piter = this->ports.find(port_id);
    if (piter == this->ports.end()) {
        ACA_LOG_ERROR("Can not find port by id:%s", port_id.data());
        return EXIT_FAILURE;
    }

	aca_port = piter->second;

    siter = this->security_groups.find(sg_id);
    if (siter == this->security_groups.end()) {
        ACA_LOG_ERROR("Can not find security group by id:%s", sg_id.data());
        return EXIT_FAILURE;
    }

    aca_sg = siter->second;

    //TODO: Special processing needs when overlapping security group rules are deleted
    for (auto &it : input_sg.get_security_group_rules()) {
        Aca_Security_Group_Rule *sg_rule = it.second;
		delete_security_group_rule(*aca_port, *aca_sg, *sg_rule);
	}

	aca_port->delete_security_group_id(sg_id);
	if (aca_port->get_security_group_num() == 0) {
		this->ports.erase(port_id);
		delete aca_port;
	}

	aca_sg->delete_port_id(port_id);
	if (aca_sg->get_port_num() == 0) {
		this->security_groups.erase(sg_id);
		delete aca_sg;
	}
		
    return EXIT_SUCCESS;
}

}
