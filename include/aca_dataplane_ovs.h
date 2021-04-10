// MIT License
// Copyright(c) 2020 Futurewei Cloud
//
//     Permission is hereby granted,
//     free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction,
//     including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons
//     to whom the Software is furnished to do so, subject to the following conditions:
//
//     The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
//     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//     WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

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

  int update_port_state_workitem(const alcor::schema::PortState current_PortState,
                                 alcor::schema::GoalStateV2 &parsed_struct,
                                 alcor::schema::GoalStateOperationReply &gsOperationReply);

  int update_neighbor_state_workitem(const alcor::schema::NeighborState current_NeighborState,
                                     alcor::schema::GoalState &parsed_struct,
                                     alcor::schema::GoalStateOperationReply &gsOperationReply);

  int update_neighbor_state_workitem(const alcor::schema::NeighborState current_NeighborState,
                                     alcor::schema::GoalStateV2 &parsed_struct,
                                     alcor::schema::GoalStateOperationReply &gsOperationReply);

  int update_router_state_workitem(const alcor::schema::RouterState current_RouterState,
                                   alcor::schema::GoalState &parsed_struct,
                                   alcor::schema::GoalStateOperationReply &gsOperationReply);

  int update_router_state_workitem(const alcor::schema::RouterState current_RouterState,
                                   alcor::schema::GoalStateV2 &parsed_struct,
                                   alcor::schema::GoalStateOperationReply &gsOperationReply);
};
} // namespace aca_dataplane_ovs
#endif // #ifndef ACA_DATAPLANCE_OVS_H