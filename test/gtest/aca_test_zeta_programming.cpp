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

#include "gtest/gtest.h"
#define private public
#include "aca_util.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <string.h>
#include "aca_comm_mgr.h"
#include "aca_zeta_programming.h"

using namespace aca_comm_manager;
using namespace alcor::schema;
using namespace aca_zeta_programming;

extern string vmac_address_1;
extern string vmac_address_2;
extern string vmac_address_3;
extern string vmac_address_4;
extern string vip_address_1;
extern string vip_address_2;
extern string vip_address_3;
extern string vip_address_4;
extern string remote_ip_1;
extern string remote_ip_2;

extern string node_mac_address_1;
extern string node_mac_address_2;
extern string node_mac_address_3;
extern string node_mac_address_4;

extern string vpc_id_1;
extern string vpc_id_2;

string auxGateway_id_1 = "11";
string auxGateway_id_2 = "22";

uint tunnel_id_1 = 555;
uint tunnel_id_2 = 666;

TEST(zeta_programming_test_cases, create_or_update_zeta_config_valid)
{
  int retcode = 0;

  AuxGateway new_auxGateway;
  new_auxGateway.set_id(auxGateway_id_1);

  AuxGateway_destination *destinaton;

  destinaton = new_auxGateway.add_destinations();
  destinaton->set_ip_address(remote_ip_1);
  destinaton->set_mac_address(node_mac_address_3);

  destinaton = new_auxGateway.add_destinations();
  destinaton->set_ip_address(remote_ip_2);
  destinaton->set_mac_address(node_mac_address_4);

  retcode = ACA_Zeta_Programming::get_instance().create_or_update_zeta_config(
          new_auxGateway, vpc_id_2, tunnel_id_2);

  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(zeta_programming_test_cases, delete_zeta_config_valid)
{
  int retcode = 0;

  AuxGateway new_auxGateway;
  new_auxGateway.set_id(auxGateway_id_1);

  AuxGateway_destination *destinaton;

  destinaton = new_auxGateway.add_destinations();
  destinaton->set_ip_address(remote_ip_1);
  destinaton->set_mac_address(node_mac_address_3);

  destinaton = new_auxGateway.add_destinations();
  destinaton->set_ip_address(remote_ip_2);
  destinaton->set_mac_address(node_mac_address_4);

  retcode = ACA_Zeta_Programming::get_instance().delete_zeta_config(
          new_auxGateway, vpc_id_2, tunnel_id_2);

  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(zeta_programming_test_cases, DISABLED_l2_auxgateway_test)
{
  int retcode;
  GoalState GoalState_builder;
  VpcState *new_vpc_states = GoalState_builder.add_vpc_states();
  PortState *new_port_states = GoalState_builder.add_port_states();

  VpcConfiguration *VpcConfiguration_builder = new_vpc_states->mutable_configuration();

  VpcConfiguration_builder->set_tunnel_id(tunnel_id_1);
  VpcConfiguration_builder->set_id(vpc_id_1);
  AuxGateway *auxGateway = VpcConfiguration_builder->mutable_auxiliary_gateway();

  auxGateway->set_id(auxGateway_id_2);

  AuxGateway_destination *destinaton;

  destinaton = auxGateway->add_destinations();
  destinaton->set_ip_address(remote_ip_1);
  destinaton->set_mac_address(node_mac_address_1);

  destinaton = auxGateway->add_destinations();
  destinaton->set_ip_address(remote_ip_2);
  destinaton->set_mac_address(node_mac_address_2);

  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();

  PortConfiguration_builder->set_vpc_id(vpc_id_1);

  new_port_states->set_operation_type(OperationType::CREATE);

  GoalStateOperationReply gsOperationalReply;

  retcode = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);

  EXPECT_EQ(retcode, EXIT_SUCCESS);
}
