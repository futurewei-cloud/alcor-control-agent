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

// #include "aca_net_state_handler.h"
#include "cppkafka/buffer.h"
#include "goalstateprovisioner.grpc.pb.h"

using std::string;

namespace aca_comm_manager
{
class Aca_Comm_Manager {
  public:
  static Aca_Comm_Manager &get_instance();

  int deserialize(const cppkafka::Buffer *kafka_buffer,
                  alcorcontroller::GoalState &parsed_struct);

  int update_goal_state(alcorcontroller::GoalState &parsed_struct,
                        alcorcontroller::GoalStateOperationReply &gsOperationReply);

  // compiler will flag error when below is called
  Aca_Comm_Manager(Aca_Comm_Manager const &) = delete;
  void operator=(Aca_Comm_Manager const &) = delete;

  private:
  // constructor and destructor marked as private so that noone can call it
  // for the singleton implementation
  Aca_Comm_Manager(){};
  ~Aca_Comm_Manager(){};

  void print_goal_state(alcorcontroller::GoalState parsed_struct);
};
} // namespace aca_comm_manager
#endif
