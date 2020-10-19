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

#ifndef ALCOR_CONTROL_AGENT_ACA_SG_STATE_HANDLER_H
#define ALCOR_CONTROL_AGENT_ACA_SG_STATE_HANDLER_H
#include "aca_security_group.h"
#include "goalstateprovisioner.grpc.pb.h"

namespace aca_security_group
{
class Aca_Sg_State_Handler {
public:
    static Aca_Sg_State_Handler &get_instance();
    int update_security_group_states(const alcor::schema::GoalState &goal_state,
                         alcor::schema::GoalStateOperationReply &reply);

private:
    // constructor and destructor marked as private so that noone can call it
    // for the singleton implementation
    Aca_Sg_State_Handler();
    ~Aca_Sg_State_Handler();    
	Aca_Port * parse_port_state(const alcor::schema::PortState &port_state);
    void parse_security_group_states(const alcor::schema::GoalState &goal_state, std::map<string, Aca_Security_Group *> &sg_state_map);
	OperationType get_operation_type(alcor::schema::OperationType operation_type);
	Direction get_direction(alcor::schema::SecurityGroupConfiguration::Direction direction);
	Ethertype get_ethertype(alcor::schema::EtherType ethertype);
	Protocol get_protocol(alcor::schema::Protocol protocol);
 	int handle_port_security_group(Aca_Port &aca_port, Aca_Security_Group &aca_sg);
};
}
#endif //ALCOR_CONTROL_AGENT_ACA_SG_STATE_HANDLER_H
