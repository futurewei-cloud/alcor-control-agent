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

#ifndef ACA_DHCP_STATE_HANDLER_H
#define ACA_DHCP_STATE_HANDLER_H

#include "aca_dhcp_programming_if.h"
#include "goalstateprovisioner.grpc.pb.h"

namespace aca_dhcp_state_handler
{
class Aca_Dhcp_State_Handler {
  public:
  static Aca_Dhcp_State_Handler &get_instance();

  // process ONE DHCP state
  int update_dhcp_state_workitem(const alcor::schema::DHCPState current_DHCPState,
                                 alcor::schema::GoalState &parsed_struct,
                                 alcor::schema::GoalStateOperationReply &gsOperationReply);

  // process 0 to N DHCP states
  int update_dhcp_states(alcor::schema::GoalState &parsed_struct,
                         alcor::schema::GoalStateOperationReply &gsOperationReply);

  // process ONE DHCP state
  int update_dhcp_state_workitem_v2(const alcor::schema::DHCPState current_DHCPState,
                                    alcor::schema::GoalStateV2 &parsed_struct,
                                    alcor::schema::GoalStateOperationReply &gsOperationReply);

  // process 0 to N DHCP states
  int update_dhcp_states(alcor::schema::GoalStateV2 &parsed_struct,
                         alcor::schema::GoalStateOperationReply &gsOperationReply);

  private:
  // constructor and destructor marked as private so that noone can call it
  // for the singleton implementation
  Aca_Dhcp_State_Handler();
  ~Aca_Dhcp_State_Handler();

  aca_dhcp_programming_if::ACA_Dhcp_Programming_Interface *dhcp_programming_if;
};
} // namespace aca_dhcp_state_handler
#endif // @ifndef ACA_DHCP_STATE_HANDLER_H
