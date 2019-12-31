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

#ifndef ACA_COMM_MGR_H
#define ACA_COMM_MGR_H

#include "cppkafka/buffer.h"
#include "goalstateprovisioner.grpc.pb.h"

using std::string;

namespace aca_comm_manager
{
class Aca_Comm_Manager {
  public:
  // constructor and destructor purposely omitted to use the default one
  // provided by the compiler
  // Aca_Comm_Manager();
  // ~Aca_Comm_Manager();

  static Aca_Comm_Manager &get_instance();

  int deserialize(const cppkafka::Buffer *kafka_buffer,
                  alcorcontroller::GoalState &parsed_struct);

  // process ONE VPC state
  int update_vpc_state_workitem(const alcorcontroller::VpcState current_VpcState,
                                alcorcontroller::GoalStateOperationReply &gsOperationReply);

  // process 0 to N VPC states
  int update_vpc_states(const alcorcontroller::GoalState &parsed_struct,
                        alcorcontroller::GoalStateOperationReply &gsOperationReply);

  // process ONE subnet state
  int update_subnet_state_workitem(const alcorcontroller::SubnetState current_SubnetState,
                                   alcorcontroller::GoalStateOperationReply &gsOperationReply);

  // process 0 to N subnet states
  int update_subnet_states(const alcorcontroller::GoalState &parsed_struct,
                           alcorcontroller::GoalStateOperationReply &gsOperationReply);

  // process ONE port state
  int update_port_state_workitem(const alcorcontroller::PortState current_PortState,
                                 const alcorcontroller::GoalState &parsed_struct,
                                 alcorcontroller::GoalStateOperationReply &gsOperationReply);

  // process 0 to N port states
  int update_port_states(const alcorcontroller::GoalState &parsed_struct,
                         alcorcontroller::GoalStateOperationReply &gsOperationReply);

  int update_goal_state(const alcorcontroller::GoalState &parsed_struct,
                        alcorcontroller::GoalStateOperationReply &gsOperationReply);

  int unload_agent_xdp(string interface, ulong &culminative_time);

  // compiler will flag the error when below is called.
  Aca_Comm_Manager(Aca_Comm_Manager const &) = delete;
  void operator=(Aca_Comm_Manager const &) = delete;

  private:
  Aca_Comm_Manager(){};
  ~Aca_Comm_Manager(){};

  void
  add_goal_state_operation_status(alcorcontroller::GoalStateOperationReply &gsOperationReply,
                                  string id, alcorcontroller::ResourceType resource_type,
                                  alcorcontroller::OperationType operation_type,
                                  int operation_rc, ulong culminative_dataplane_programming_time,
                                  ulong culminative_network_configuration_time,
                                  ulong state_elapse_time);

  int load_agent_xdp(string interface, ulong &culminative_time);

  int execute_command(int command, void *input_struct, ulong &culminative_time);

  void print_goal_state(alcorcontroller::GoalState parsed_struct);
};
} // namespace aca_comm_manager
#endif
