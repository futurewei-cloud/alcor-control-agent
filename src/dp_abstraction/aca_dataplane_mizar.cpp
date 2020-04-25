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

#include "aca_dataplane_mizar.h"

//#include "aca_comm_mgr.h"
#include "aca_net_config.h"
// #include "aca_util.h"
#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "trn_rpc_protocol.h"

//#include <chrono>
//#include <future>
//#include <errno.h>
#include <arpa/inet.h>
//#include <algorithm>

/* TODO, uncomment when bring mizar code
static char EMPTY_STRING[] = "";

// copying the defines until it is defined in transit RPC interface
#define TRAN_SUBSTRT_VNI 0
#define TRAN_SUBSTRT_EP 0
#define TRAN_SIMPLE_EP 1
*/

using namespace std;
// using namespace aca_net_programming_if;
using namespace alcorcontroller;

// mizar dataplane implementation class
namespace aca_dataplane_mizar
{
int ACA_Dataplane_Mizar::initialize()
{
  // For mizar, nothing to initialize
  return EXIT_SUCCESS;
}

int ACA_Dataplane_Mizar::update_vpc_state_workitem(const VpcState current_VpcState,
                                                   const GoalStateOperationReply &gsOperationReply)
{
  // To be implemented
  return EXIT_SUCCESS;
}

int ACA_Dataplane_Mizar::update_subnet_state_workitem(const SubnetState current_SubnetState,
                                                      const GoalStateOperationReply &gsOperationReply)
{
  // To be implemented
  return EXIT_SUCCESS;
}

int ACA_Dataplane_Mizar::update_port_state_workitem(const PortState current_PortState,
                                                    const GoalState &parsed_struct,
                                                    const GoalStateOperationReply &gsOperationReply)
{
  // To be implemented
  return EXIT_SUCCESS;
}

}; // namespace aca_dataplane_mizar
