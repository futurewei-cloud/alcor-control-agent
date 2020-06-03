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

#ifndef ACA_DATAPLANCE_OVS_H
#define ACA_DATAPLANCE_OVS_H

#include "aca_net_programming_if.h"

// OVS dataplane implementation class
namespace aca_dataplane_ovs
{
class ACA_Dataplane_OVS : public aca_net_programming_if::ACA_Core_Net_Programming_Interface {
  public:
  int initialize();

  int update_vpc_state_workitem(const alcor::schema::VpcState current_VpcState,
                                alcor::schema::GoalStateOperationReply &gsOperationReply);

  int update_subnet_state_workitem(const alcor::schema::SubnetState current_SubnetState,
                                   alcor::schema::GoalStateOperationReply &gsOperationReply);

  int update_port_state_workitem(const alcor::schema::PortState current_PortState,
                                 alcor::schema::GoalState &parsed_struct,
                                 alcor::schema::GoalStateOperationReply &gsOperationReply);
};
} // namespace aca_dataplane_ovs
#endif // #ifndef ACA_DATAPLANCE_OVS_H