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

#include <future>
#include "aca_log.h"
#include "aca_util.h"
#include "aca_sg_state_handler.h"
#include "aca_goal_state_handler.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_security_group_manager.h"

using namespace std;
using namespace alcor::schema;

namespace aca_security_group
{
Aca_Sg_State_Handler::Aca_Sg_State_Handler()
{
    ACA_LOG_INFO("Sg State Handler: using sg server\n");
}

Aca_Sg_State_Handler::~Aca_Sg_State_Handler()
{
    // allocated sg_programming_if is destroyed when program exits.
}

Aca_Sg_State_Handler &Aca_Sg_State_Handler::get_instance()
{
    // It is instantiated on first use.
    // allocated instance is destroyed when program exits.
    static Aca_Sg_State_Handler instance;
    return instance;
}

int Aca_Sg_State_Handler::handle_port_security_group(Aca_Port &aca_port,
                                                      		 Aca_Security_Group &aca_sg)
{
	Aca_Security_Group_Manager &sg_manager = Aca_Security_Group_Manager::get_instance();
	OperationType operation_type = aca_sg.get_operation_type();

    switch (operation_type) {
    	case CREATE:
            return sg_manager.create_security_group(aca_port, aca_sg);
    	case UPDATE:
            return sg_manager.update_security_group(aca_port, aca_sg);
    	case DELETE:
            return sg_manager.delete_security_group(aca_port, aca_sg);
    	default:
    		ACA_LOG_ERROR("=====>wrong security group operation\n");
    }

	delete &aca_sg;

    return EXIT_FAILURE;
}

Aca_Port * Aca_Sg_State_Handler::parse_port_state(const PortState &port_state)
{
	PortConfiguration port_config = port_state.configuration();

	//TODO: verify fields of port_state
	Aca_Port *aca_port = new Aca_Port();
	aca_port->set_id(port_config.id());
	aca_port->set_mac_address(port_config.mac_address());
	
	for (int i = 0; i < port_config.fixed_ips_size(); i++) {
		aca_port->add_fixed_ip(port_config.fixed_ips(i).ip_address());
	}

	for (int i = 0; i < port_config.allow_address_pairs_size(); i++) {
		PortConfiguration::AllowAddressPair pair = port_config.allow_address_pairs(i);
		aca_port->add_allow_address_pair(pair.ip_address(), pair.mac_address());
	}

	for (int i = 0; i < port_config.security_group_ids_size(); i++) {
		aca_port->add_security_group_id(port_config.security_group_ids(i).id());
	}

	return aca_port;
}

OperationType Aca_Sg_State_Handler::get_operation_type(alcor::schema::OperationType operation_type)
{
	switch(operation_type) {
		case alcor::schema::OperationType::CREATE:
			return CREATE;
		case alcor::schema::OperationType::UPDATE:
			return UPDATE;
		case alcor::schema::OperationType::DELETE:
			return DELETE;
		default:
			ACA_LOG_ERROR("Invalid operation type");
	}

	throw "Invalid operation type";
}

Direction Aca_Sg_State_Handler::get_direction(SecurityGroupConfiguration::Direction direction)
{
	switch(direction) {
		case SecurityGroupConfiguration::EGRESS:
			return EGRESS;
		case SecurityGroupConfiguration::INGRESS:
			return INGRESS;
		default:
			ACA_LOG_ERROR("Invalid direction");
	}

	
	throw "Invalid direction";
}

Ethertype Aca_Sg_State_Handler::get_ethertype(alcor::schema::EtherType ethertype)
{
	switch(ethertype) {
		case alcor::schema::EtherType::IPV4:
			return IPV4;
		case alcor::schema::EtherType::IPV6:
			return IPV6;
		default:
			ACA_LOG_ERROR("Invalid ethertype");
	}

	throw "Invalid ethertype";
}

Protocol Aca_Sg_State_Handler::get_protocol(alcor::schema::Protocol protocol)
{
	switch(protocol) {
		case alcor::schema::Protocol::TCP:
			return TCP;
		case alcor::schema::Protocol::UDP:
			return UDP;
		case alcor::schema::Protocol::ICMP:
			return ICMP;
		default:
			ACA_LOG_ERROR("Invalid protocol");
	}

	throw "Invalid protocol";
}


void Aca_Sg_State_Handler::parse_security_group_states(const GoalState &goal_state, 
                                                map<string, Aca_Security_Group *> &aca_sg_map) 
{
	//TODO: verify fields of port_state
    for (int i = 0; i < goal_state.security_group_states_size(); i++) {
    	ACA_LOG_DEBUG("=====>parsing security group states #%d\n", i);

		Aca_Security_Group *aca_sg;
    	SecurityGroupState sg_state = goal_state.security_group_states(i);
		SecurityGroupConfiguration sg_config = sg_state.configuration();    

		if (sg_config.id() == "") {
			ACA_LOG_ERROR("Security group id is empty");
			continue;
		}

		aca_sg = new Aca_Security_Group();

		aca_sg->set_id(sg_config.id());
		aca_sg->set_name(sg_config.name());
		aca_sg->set_vpc_id(sg_config.vpc_id());
		aca_sg->set_format_version(sg_config.format_version());
		aca_sg->set_revision_number(sg_config.revision_number());
		aca_sg->set_operation_type(get_operation_type(sg_state.operation_type()));

		for (int j = 0; j < sg_config.security_group_rules_size(); j++) {
			SecurityGroupConfiguration::SecurityGroupRule sg_rule = sg_config.security_group_rules(i);
			Aca_Security_Group_Rule *aca_sg_rule = new Aca_Security_Group_Rule();
			
			if (sg_config.id() == "") {
				ACA_LOG_ERROR("Security group rule id is empty");
				continue;
			}
			
			aca_sg_rule->set_id(sg_rule.id());
			aca_sg_rule->set_direction(get_direction(sg_rule.direction()));
			aca_sg_rule->set_ethertype(get_ethertype(sg_rule.ethertype()));
			aca_sg_rule->set_protocol(get_protocol(sg_rule.protocol()));
			aca_sg_rule->set_port_range_min(sg_rule.port_range_min());
			aca_sg_rule->set_port_range_max(sg_rule.port_range_max());
			aca_sg_rule->set_remote_ip_prefix(sg_rule.remote_ip_prefix());
			aca_sg_rule->set_remote_group_id(sg_rule.remote_group_id());
			aca_sg_rule->set_operation_type(get_operation_type(sg_rule.operation_type()));

			aca_sg->add_security_group_rule(aca_sg_rule);
		}		
        
        aca_sg_map[sg_config.id()] = aca_sg;
    }
}

int Aca_Sg_State_Handler::update_security_group_states(const GoalState &goal_state,
                                                    GoalStateOperationReply &reply)
{
    std::vector<std::future<int>> futures;
    int rc;
    int overall_rc = EXIT_SUCCESS;
    map<string, Aca_Security_Group *> aca_sg_map;

    parse_security_group_states(goal_state, aca_sg_map);
    
    for (int i = 0; i < goal_state.port_states_size(); i++) {
        PortState port_state = goal_state.port_states(i);
        PortConfiguration port_config = port_state.configuration();
        
        Aca_Port *aca_port = parse_port_state(port_state);

        for (int j = 0; j < port_config.security_group_ids_size(); j++) {
            PortConfiguration::SecurityGroupId security_group_id = port_config.security_group_ids(j);
            string sg_id = security_group_id.id();

            map<string, Aca_Security_Group *>::iterator iterator = aca_sg_map.find(sg_id);
            if (iterator == aca_sg_map.end()) {
                ACA_LOG_ERROR("Can not find security group by id:%s", sg_id.data());
                continue;
            }
							
			futures.push_back(std::async(
					std::launch::async, &Aca_Sg_State_Handler::handle_port_security_group,
					this, std::ref(*aca_port), std::ref(*(iterator->second))));
        }
    }

    for (uint32_t i = 0; i < futures.size(); i++) {
        rc = futures[i].get();
        if (rc != EXIT_SUCCESS) {
            overall_rc = rc;
        }
    }

    return overall_rc;
}

}
