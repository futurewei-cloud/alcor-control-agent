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

#ifndef ALCOR_CONTROL_AGENT_SECURITY_GROUP_MANAGER_H
#define ALCOR_CONTROL_AGENT_SECURITY_GROUP_MANAGER_H
#include "aca_security_group.h"


namespace aca_security_group {

class Aca_Security_Group_Manager {
public:
    static Aca_Security_Group_Manager &get_instance();
    int create_security_group_rule(Aca_Port &port_state, Aca_Security_Group &sg, Aca_Security_Group_Rule &sg_rule); 
	int update_security_group_rule(Aca_Port &port_state, Aca_Security_Group &sg, Aca_Security_Group_Rule &sg_rule);
	int delete_security_group_rule(Aca_Port &port_state, Aca_Security_Group &sg, Aca_Security_Group_Rule &sg_rule);
	int create_security_group(Aca_Port &port_state, Aca_Security_Group &sg); 
	int update_security_group(Aca_Port &port_state, Aca_Security_Group &sg);
	int delete_security_group(Aca_Port &port_state, Aca_Security_Group &sg);
	
private:
	map<string, Aca_Port *> ports;
	map<string, Aca_Security_Group *> security_groups;
};

}
#endif //ALCOR_CONTROL_AGENT_SECURITY_GROUP_MANAGER_H