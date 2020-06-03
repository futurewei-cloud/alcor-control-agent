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

#include "aca_dataplane_ovs.h"
#include "aca_net_state_handler.h"
#include "aca_ovs_config.h"
// #include "aca_net_config.h"
#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <errno.h>

using namespace std;
using namespace alcor::schema;
// using aca_net_config::Aca_Net_Config;

namespace aca_dataplane_ovs
{
int ACA_Dataplane_OVS::initialize()
{
  // For OVS, we need to make sure OVS is started and bridges are setup correctly
  // else, do it here

  // TO BE IMPLEMENTED
  return ENOSYS;
}

int ACA_Dataplane_OVS::update_vpc_state_workitem(const VpcState current_VpcState,
                                                 GoalStateOperationReply &gsOperationReply)
{
  // TO BE IMPLEMENTED
  return ENOSYS;
}

int ACA_Dataplane_OVS::update_subnet_state_workitem(const SubnetState current_SubnetState,
                                                    GoalStateOperationReply &gsOperationReply)
{
  // TO BE IMPLEMENTED
  return ENOSYS;
}

int ACA_Dataplane_OVS::update_port_state_workitem(const PortState current_PortState,
                                                  GoalState &parsed_struct,
                                                  GoalStateOperationReply &gsOperationReply)
{
  // TO BE IMPLEMENTED
  return ENOSYS;
}

} // namespace aca_dataplane_ovs
