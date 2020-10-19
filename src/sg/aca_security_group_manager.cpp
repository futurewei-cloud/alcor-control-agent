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

int Aca_Security_Group_Manager::create_security_group_rule(Aca_Port &aca_port,
													  Aca_Security_Group &sg,
                                                      Aca_Security_Group_Rule &sg_rule) 
{
	string rule_id;
	Aca_Security_Group_Rule *aca_sg_rule;
	Aca_Security_Group_Ovs &sg_ovs = Aca_Security_Group_Ovs::get_instance();
	rule_id = sg_rule.get_id();

	aca_sg_rule = sg.get_security_group_rule(rule_id);
	if (aca_sg_rule != NULL) {
		TRN_LOG_WARN("Security group rule(id:%s) already exist", rule_id.data());
		return update_security_group_rule(aca_port, sg, sg_rule); 
	}

	sg_rule.set_cookie(0);
	sg_ovs.create_port_security_group_rule(aca_port, sg_rule);
	sg.add_security_group_rule(&sg_rule); 

	return EXIT_SUCCESS;
}

int Aca_Security_Group_Manager::update_security_group_rule(Aca_Port &aca_port,
													  Aca_Security_Group &sg,
                                                      Aca_Security_Group_Rule &sg_rule) 
{	
	string rule_id;
	Aca_Security_Group_Rule *aca_sg_rule;
	Aca_Security_Group_Ovs &sg_ovs = Aca_Security_Group_Ovs::get_instance();
	rule_id = sg_rule.get_id();
	
	aca_sg_rule = sg.get_security_group_rule(rule_id);
	if (aca_sg_rule == NULL) {
		ACA_LOG_ERROR("Can not find security group rule by id:%s", rule_id.data());
		return EXIT_FAILURE; 
	}

	sg_rule.set_cookie(aca_sg_rule->get_cookie() + 1);
	sg_ovs.update_port_security_group_rule(aca_port, sg_rule);
	sg.update_security_group_rule(&sg_rule); 

	return EXIT_SUCCESS;
}

int Aca_Security_Group_Manager::delete_security_group_rule(Aca_Port &aca_port,
													  Aca_Security_Group &sg,
                                                      Aca_Security_Group_Rule &sg_rule) 
{
	string rule_id;
	Aca_Security_Group_Rule *aca_sg_rule;
	Aca_Security_Group_Ovs &sg_ovs = Aca_Security_Group_Ovs::get_instance();
	rule_id = sg_rule.get_id();

	aca_sg_rule = sg.get_security_group_rule(rule_id);
	if (aca_sg_rule == NULL) {
		ACA_LOG_ERROR("Can not find security group rule by id:%s", rule_id.data());
		return EXIT_FAILURE; 
	}

	sg_ovs.delete_port_security_group_rule(aca_port, *aca_sg_rule);
	sg.delete_security_group_rule(sg_rule.get_id()); 

	return EXIT_SUCCESS;
}

int Aca_Security_Group_Manager::create_security_group(Aca_Port &aca_port,
                                                 Aca_Security_Group &aca_sg) 
 {
 	map<string, Aca_Port *>::iterator piter;
	map<string, Aca_Security_Group *>::iterator siter;
	Aca_Security_Group *sg;
	Aca_Security_Group_Ovs &sg_ovs = Aca_Security_Group_Ovs::get_instance();
	string port_id = aca_port.get_id();
	string sg_id = aca_sg.get_id();

	piter = this->ports.find(port_id);
    if (piter == this->ports.end()) {
        sg_ovs.init_port(aca_port);
    }

    siter = this->security_groups.find(sg_id);
    if (siter == this->security_groups.end()) {
        sg = new Aca_Security_Group();
    } else {
		sg = siter->second;
    }

    for (auto &it : aca_sg.get_security_group_rules()) {
        create_security_group_rule(aca_port, *sg, *(it.second));
    }

	if (piter == this->ports.end()) {
		this->ports[port_id] = &aca_port;
	} else {
		Aca_Port * aca_port = piter->second;
		aca_port->add_security_group_id(sg_id);
	}

	//TODO: maybe multi threads access this map
	if (siter == this->security_groups.end()) {
		this->security_groups[sg_id] = sg;
	}
			
    return EXIT_SUCCESS;
}

int Aca_Security_Group_Manager::update_security_group(Aca_Port &aca_port,
                                                      Aca_Security_Group &aca_sg) 
{
	map<string, Aca_Port *>::iterator piter;
	map<string, Aca_Security_Group *>::iterator siter;

	string port_id = aca_port.get_id();
	string sg_id = aca_sg.get_id();

	piter = this->ports.find(port_id);
    if (piter == this->ports.end()) {
        ACA_LOG_ERROR("Can not find port by id:%s", port_id.data());
        return EXIT_FAILURE;
    }
    
    siter = this->security_groups.find(sg_id);
    if (siter == this->security_groups.end()) {
        ACA_LOG_ERROR("Can not find security group by id:%s", sg_id.data());
        return EXIT_FAILURE;
    }
    
    for (auto &it : aca_sg.get_security_group_rules()) {
        Aca_Security_Group_Rule *sg_rule = it.second;
		switch (sg_rule->get_operation_type()) {
			case CREATE:
				create_security_group_rule(aca_port, *(siter->second), *sg_rule);
				break;
			case UPDATE:
				update_security_group_rule(aca_port, *(siter->second), *sg_rule);
				break;
			case DELETE:
				delete_security_group_rule(aca_port, *(siter->second), *sg_rule);
				break;
			default:
				ACA_LOG_ERROR("Invalid security group rule operation type");
		}
    }
	
    return EXIT_SUCCESS;
}

int Aca_Security_Group_Manager::delete_security_group(Aca_Port &aca_port,
                                                      Aca_Security_Group &aca_sg) 
{
	map<string, Aca_Port *>::iterator piter;
	map<string, Aca_Security_Group *>::iterator siter;

	string port_id = aca_port.get_id();
	string sg_id = aca_sg.get_id();

	piter = this->ports.find(port_id);
    if (piter == this->ports.end()) {
        ACA_LOG_ERROR("Can not find port by id:%s", port_id.data());
        return EXIT_FAILURE;
    }

    siter = this->security_groups.find(sg_id);
    if (siter == this->security_groups.end()) {
        ACA_LOG_ERROR("Can not find security group by id:%s", sg_id.data());
        return EXIT_FAILURE;
    }

    
    for (auto &it : aca_sg.get_security_group_rules()) {
        Aca_Security_Group_Rule *sg_rule = it.second;
		delete_security_group_rule(aca_port, *(siter->second), *sg_rule);
	}

	aca_port.delete_security_group_id(sg_id);
	
	if (aca_port.get_security_group_id_num() == 0) {
		Aca_Port *port = piter->second;
		this->ports.erase(port_id);
		delete port;
	}

	//TODO: only delete sg when never be used by any port
	//this->security_groups.erase(aca_sg.get_id());
		
    return EXIT_SUCCESS;
}

}
