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

#ifndef ACA_NET_STATE_HANDLER_H
#define ACA_NET_STATE_HANDLER_H

#include "aca_dataplane_mizar.h"
#include "goalstateprovisioner.grpc.pb.h"

namespace aca_net_state_handler
{
class Aca_Net_State_Handler {
  public:
  static Aca_Net_State_Handler &get_instance();

  // process ONE VPC state
  int update_vpc_state_workitem(const alcorcontroller::VpcState current_VpcState,
                                alcorcontroller::GoalStateOperationReply &gsOperationReply);

  // process 0 to N VPC states
  int update_vpc_states(alcorcontroller::GoalState &parsed_struct,
                        alcorcontroller::GoalStateOperationReply &gsOperationReply);

  // process ONE subnet state
  int update_subnet_state_workitem(const alcorcontroller::SubnetState current_SubnetState,
                                   alcorcontroller::GoalStateOperationReply &gsOperationReply);

  // process 0 to N subnet states
  int update_subnet_states(alcorcontroller::GoalState &parsed_struct,
                           alcorcontroller::GoalStateOperationReply &gsOperationReply);

  // process ONE port state
  int update_port_state_workitem(const alcorcontroller::PortState current_PortState,
                                 alcorcontroller::GoalState &parsed_struct,
                                 alcorcontroller::GoalStateOperationReply &gsOperationReply);

  // process 0 to N port states
  int update_port_states(alcorcontroller::GoalState &parsed_struct,
                         alcorcontroller::GoalStateOperationReply &gsOperationReply);

  int update_goal_state(alcorcontroller::GoalState &parsed_struct,
                        alcorcontroller::GoalStateOperationReply &gsOperationReply);

  void add_goal_state_operation_status(
          alcorcontroller::GoalStateOperationReply &gsOperationReply,
          std::string id, alcorcontroller::ResourceType resource_type,
          alcorcontroller::OperationType operation_type, int operation_rc,
          ulong culminative_dataplane_programming_time,
          ulong culminative_network_configuration_time, ulong state_elapse_time);

  // compiler will flag error when below is called
  Aca_Net_State_Handler(Aca_Net_State_Handler const &) = delete;
  void operator=(Aca_Net_State_Handler const &) = delete;

  private:
  // constructor and destructor marked as private so that noone can call it
  // for the singleton implementation
  Aca_Net_State_Handler();
  ~Aca_Net_State_Handler();

  aca_net_programming_if::ACA_Core_Net_Programming_Interface *core_net_programming_if;
};
} // namespace aca_net_state_handler
#endif // @ifndef ACA_NET_STATE_HANDLER_H
