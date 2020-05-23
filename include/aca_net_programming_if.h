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

#ifndef ACA_NET_PROGRAMMING_IF_H
#define ACA_NET_PROGRAMMING_IF_H

#include "goalstateprovisioner.grpc.pb.h"

// Core Network Programming Interface class
namespace aca_net_programming_if
{
class ACA_Core_Net_Programming_Interface {
  public:
  // pure virtual functions providing interface framework.
  virtual int initialize() = 0;

  virtual int
  update_vpc_state_workitem(const alcor::schema::VpcState current_VpcState,
                            alcor::schema::GoalStateOperationReply &gsOperationReply) = 0;

  virtual int
  update_subnet_state_workitem(const alcor::schema::SubnetState current_SubnetState,
                               alcor::schema::GoalStateOperationReply &gsOperationReply) = 0;

  virtual int
  update_port_state_workitem(const alcor::schema::PortState current_PortState,
                             alcor::schema::GoalState &parsed_struct,
                             alcor::schema::GoalStateOperationReply &gsOperationReply) = 0;
};
} // namespace aca_net_programming_if
#endif // #ifndef ACA_NET_PROGRAMMING_IF_H
