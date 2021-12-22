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

#ifndef ACA_COMM_MGR_H
#define ACA_COMM_MGR_H

#include "goalstateprovisioner.grpc.pb.h"
#include "subscribeinfoprovisioner.grpc.pb.h"

using std::string;

namespace aca_comm_manager
{
class Aca_Comm_Manager {
  public:
  static Aca_Comm_Manager &get_instance();

  int deserialize(const unsigned char *mq_buffer, size_t buffer_length,
                  alcor::schema::GoalState &parsed_struct);

  int deserialize(const unsigned char *mq_buffer, size_t buffer_length,
                  alcor::schema::GoalStateV2 &parsed_struct);

  int update_goal_state(alcor::schema::GoalState &goal_state_message,
                        alcor::schema::GoalStateOperationReply &gsOperationReply);

  int update_goal_state(alcor::schema::GoalStateV2 &goal_state_message,
                        alcor::schema::GoalStateOperationReply &gsOperationReply);

  int update_subscribe_info(alcor::schema::NodeSubscribeInfo &subscribe_info_message,
                            alcor::schema::SubscribeOperationReply &subscribeOperationReply);

  // compiler will flag error when below is called
  Aca_Comm_Manager(Aca_Comm_Manager const &) = delete;
  void operator=(Aca_Comm_Manager const &) = delete;

  private:
  // constructor and destructor marked as private so that noone can call it
  // for the singleton implementation
  Aca_Comm_Manager(){};
  ~Aca_Comm_Manager(){};

  void print_goal_state(alcor::schema::GoalState parsed_struct);

  void print_goal_state(alcor::schema::GoalStateV2 parsed_struct);

};
} // namespace aca_comm_manager
#endif
