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

// Note: mizar dataplane end to end support is no longer maintained going forward

#ifndef ACA_DATAPLANCE_MIZAR_H
#define ACA_DATAPLANCE_MIZAR_H

#include "aca_net_programming_if.h"

// mizar dataplane implementation class
namespace aca_dataplane_mizar
{
class ACA_Dataplane_Mizar : public aca_net_programming_if::ACA_Core_Net_Programming_Interface {
  public:
  int initialize();

  int update_vpc_state_workitem(const alcor::schema::VpcState current_VpcState,
                                alcor::schema::GoalStateOperationReply &gsOperationReply);

  int update_subnet_state_workitem(const alcor::schema::SubnetState current_SubnetState,
                                   alcor::schema::GoalStateOperationReply &gsOperationReply);

  int update_port_state_workitem(const alcor::schema::PortState current_PortState,
                                 alcor::schema::GoalState &parsed_struct,
                                 alcor::schema::GoalStateOperationReply &gsOperationReply);

  private:
  int load_agent_xdp(std::string interface, ulong &culminative_time);

  int unload_agent_xdp(std::string interface, ulong &culminative_time);

  int execute_command(int command, void *input_struct, ulong &culminative_time);
};
} // namespace aca_dataplane_mizar
#endif // #ifndef ACA_DATAPLANCE_MIZAR_H