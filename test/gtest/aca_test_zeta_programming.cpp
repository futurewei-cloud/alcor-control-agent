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
#include "aca_ovs_l2_programmer.h"
#include "aca_comm_mgr.h"
#include "aca_zeta_programming.h"
#include "aca_util.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <typeinfo>

using namespace std;
using namespace aca_comm_manager;
using namespace alcor::schema;
using namespace aca_zeta_programming;
using namespace aca_ovs_l2_programmer;

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
extern string port_name_1;

extern string subnet_id_1;
extern string subnet1_gw_mac;

extern bool g_demo_mode;

extern void aca_test_create_default_port_state(PortState *new_port_states);
extern void aca_test_create_default_subnet_state(SubnetState *new_subnet_states);

extern string auxGateway_id_1;
extern string auxGateway_id_2;

extern uint tunnel_id_1;
extern uint tunnel_id_2;
extern uint oam_port_1;
extern uint oam_port_2;
uint parent_position_in_port_info = 0;
uint child_position_in_port_info = 1;

void aca_test_create_default_port_state_with_zeta_data(PortState *new_port_states,
                                                       nlohmann::json port_data)
{
  new_port_states->set_operation_type(OperationType::CREATE);

  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_update_type(UpdateType::FULL);
  PortConfiguration_builder->set_id(port_data["port_id"]); //port_id

  PortConfiguration_builder->set_vpc_id(port_data["vpc_id"]); //vpc_id
  PortConfiguration_builder->set_name(port_name_1); // make it up.
  PortConfiguration_builder->set_mac_address(port_data["mac_port"]); //mac_port
  PortConfiguration_builder->set_admin_state_up(true);

  PortConfiguration_FixedIp *FixedIp_builder = PortConfiguration_builder->add_fixed_ips();
  FixedIp_builder->set_subnet_id(subnet_id_1); // made up previously
  FixedIp_builder->set_ip_address(port_data["ips_port"][0]["ip"]); // ips_port.ip

  PortConfiguration_SecurityGroupId *SecurityGroup_builder =
          PortConfiguration_builder->add_security_group_ids();
  SecurityGroup_builder->set_id("1");
}

void aca_test_create_default_subnet_state_with_zeta_data(SubnetState *new_subnet_states,
                                                         nlohmann::json vpc_data)
{
  new_subnet_states->set_operation_type(OperationType::INFO);

  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_revision_number(1); // always 1
  SubnetConiguration_builder->set_vpc_id(vpc_data["vpc_id"]); // pass in
  SubnetConiguration_builder->set_id(subnet_id_1); // make one up
  SubnetConiguration_builder->set_cidr("10.10.0.0/16"); // 10.10.0.0/16, need to figure out a way to extract it from the data.
  SubnetConiguration_builder->set_tunnel_id(vpc_data["vni"]); // same tunnel id

  auto *subnetConfig_GatewayBuilder(new SubnetConfiguration_Gateway);
  subnetConfig_GatewayBuilder->set_ip_address("10.10.0.1"); // the 1st ip of cidr, need to figure out a way to extract it from the data.
  subnetConfig_GatewayBuilder->set_mac_address(subnet1_gw_mac); // make it up
  SubnetConiguration_builder->set_allocated_gateway(subnetConfig_GatewayBuilder);
}

void aca_test_zeta_setup(string zeta_gateway_path_config_file)
{
  ifstream ifs(zeta_gateway_path_config_file);
  if (!ifs)
    cout << zeta_gateway_path_config_file << "open error" << endl;

  nlohmann::json zeta_data = nlohmann::json::parse(ifs);
  cout << zeta_data << endl;
  // print something to check if read json succeed
  cout << "VPC DATA: " << endl;
  cout << zeta_data["vpc_response"]["port_ibo"] << endl;
  cout << zeta_data["vpc_response"]["vni"] << endl;
  cout << zeta_data["vpc_response"]["vpc_id"] << endl;
  cout << zeta_data["vpc_response"]["zgc_id"] << endl;
  cout << "PORT DATA: " << endl;
  cout << zeta_data["port_response"][0]["ip_node"] << endl;
  cout << zeta_data["port_response"][0]["mac_port"] << endl;

  int overall_rc;

  // from here.

  GoalState GoalState_builder;
  VpcState *new_vpc_states = GoalState_builder.add_vpc_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  new_vpc_states->set_operation_type(OperationType::INFO);

  // fill in vpc state structs
  VpcConfiguration *VpcConfiguration_builder = new_vpc_states->mutable_configuration();
  cout << "Filling in vni: " << zeta_data["vpc_response"]["vni"] << endl;
  VpcConfiguration_builder->set_tunnel_id(zeta_data["vpc_response"]["vni"]); //vni
  cout << "Filling in vpc_id: " << zeta_data["vpc_response"]["vpc_id"] << endl;
  VpcConfiguration_builder->set_id(zeta_data["vpc_response"]["vpc_id"]); // vpc_id

  // fill in auxgateway state structs
  AuxGateway *auxGateway = VpcConfiguration_builder->mutable_auxiliary_gateway();
  auxGateway->set_aux_gateway_type(AuxGatewayType::ZETA);
  cout << "Filling in zgc_id: " << zeta_data["vpc_response"]["zgc_id"] << endl;

  auxGateway->set_id(zeta_data["vpc_response"]["zgc_id"]); //zgc_id

  AuxGateway_zeta *zeta_info = auxGateway->mutable_zeta_info();
  cout << "Filling in port_ibo: " << zeta_data["vpc_response"]["port_ibo"] << endl; // should be int, not string

  string port_ibo_string = zeta_data["vpc_response"]["port_ibo"];
  uint port_ibo_unit = std::stoul(port_ibo_string, nullptr, 10);
  cout << "uint in port_ibo: " << port_ibo_unit << endl;
  zeta_info->set_port_inband_operation(port_ibo_unit); //port_ibo

  AuxGateway_destination *destination;

  cout << "Try to fill in gw ips/macs now" << endl;

  nlohmann::json gw_array = zeta_data["vpc_response"]["gws"];

  for (nlohmann::json::iterator it = gw_array.begin(); it != gw_array.end(); ++it) {
    cout << "Filling in: " << *it << "to the destination" << endl;
    destination = auxGateway->add_destinations();
    destination->set_ip_address((*it)["ip"]);
    destination->set_mac_address((*it)["mac"]);
  }

  // fill in subnet state structs
  aca_test_create_default_subnet_state_with_zeta_data(
          new_subnet_states, zeta_data["vpc_response"]);

  nlohmann::json port_response_array = zeta_data["port_response"];

  for (nlohmann::json::iterator it = port_response_array.begin();
       it != port_response_array.end(); ++it) {
    string ip_node = (*it)["ip_node"];
    if (aca_is_port_on_same_host(ip_node)) { //  if this ip_node is an ip on one of the interfaces on this machine ...
      cout << "IP: " << ip_node
           << " is on this same machine, add port states to it." << endl;
      PortState *new_port_states = GoalState_builder.add_port_states();
      // fill in port state structs
      aca_test_create_default_port_state_with_zeta_data(new_port_states, *it); // use 0th position for now, but need to check all ports on this host.
    }
  }

  GoalStateOperationReply gsOperationalReply;
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
}

<<<<<<< HEAD
TEST(zeta_programming_test_cases, create_zeta_config_valid)
=======
// test if the IP in gws is included in the group entry or not
bool test_gws_ip_correct(string zeta_gateway_path_config_file, uint group_id)
{
  bool overall_rc;
  ifstream ifs(zeta_gateway_path_config_file);
  if (!ifs)
    cout << zeta_gateway_path_config_file << "open error" << endl;
  nlohmann::json zeta_data = nlohmann::json::parse(ifs);
  nlohmann::json gw_array = zeta_data["vpc_response"]["gws"];
  // Construct query command
  string dump_flows = "ovs-ofctl -O OpenFlow13 dump-groups br-tun";
  string opt1 = "group_id=" + to_string(group_id);
  const string tail = "->tun_dst";
  for (nlohmann::json::iterator it = gw_array.begin(); it != gw_array.end(); ++it) 
  {
    string opt2 = "set_field:" + (*it)["ip"] + tail;
    string cmd_string = dump_flows + " | grep " + opt1 + " | grep " + opt2;
    overall_rc = aca_net_config::Aca_Net_Config::get_instance().execute_system_command(cmd_string);
    if (overall_rc == EXIT_SUCCESS) {
      printf("gws_ip %s is in group rule.\n",(*it)["ip"]);
      return true;
    } else {
      printf("gws_ip %s is not in group rule.\n",(*it)["ip"]);
      return false;
    }
  }
}

TEST(zeta_programming_test_cases, create_or_update_zeta_config_valid)
>>>>>>> [validation]: group entry exist & gws ip right -[1]
{
  int retcode = 0;

  // fill in auxgateway state structs
  AuxGateway new_auxGateway;
  new_auxGateway.set_id(auxGateway_id_1);
  AuxGateway_zeta *zeta_info = new_auxGateway.mutable_zeta_info();
  zeta_info->set_port_inband_operation(oam_port_1);
  AuxGateway_destination *destination;

  destination = new_auxGateway.add_destinations();
  destination->set_ip_address(remote_ip_1);
  destination->set_mac_address(node_mac_address_3);

  destination = new_auxGateway.add_destinations();
  destination->set_ip_address(remote_ip_2);
  destination->set_mac_address(node_mac_address_4);

  retcode = ACA_Zeta_Programming::get_instance().create_zeta_config(new_auxGateway, tunnel_id_2);

  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(zeta_programming_test_cases, delete_zeta_config_valid)
{
  int retcode = 0;

  // fill in auxgateway state structs
  AuxGateway new_auxGateway;
  new_auxGateway.set_id(auxGateway_id_1);
  AuxGateway_zeta *zeta_info = new_auxGateway.mutable_zeta_info();
  zeta_info->set_port_inband_operation(oam_port_1);
  AuxGateway_destination *destination;

  destination = new_auxGateway.add_destinations();
  destination->set_ip_address(remote_ip_1);
  destination->set_mac_address(node_mac_address_3);

  destination = new_auxGateway.add_destinations();
  destination->set_ip_address(remote_ip_2);
  destination->set_mac_address(node_mac_address_4);

  retcode = ACA_Zeta_Programming::get_instance().delete_zeta_config(new_auxGateway, tunnel_id_2);

  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(zeta_programming_test_cases, create_auxgateway_test)
{
  // from here.
  int retcode;
  GoalState GoalState_builder;
  VpcState *new_vpc_states = GoalState_builder.add_vpc_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
  PortState *new_port_states = GoalState_builder.add_port_states();

  new_vpc_states->set_operation_type(OperationType::INFO);

  // fill in vpc state structs
  VpcConfiguration *VpcConfiguration_builder = new_vpc_states->mutable_configuration();
  VpcConfiguration_builder->set_tunnel_id(tunnel_id_1); //vni
  VpcConfiguration_builder->set_id(vpc_id_1); // vpc_id

  // fill in auxgateway state structs
  AuxGateway *auxGateway = VpcConfiguration_builder->mutable_auxiliary_gateway();
  auxGateway->set_aux_gateway_type(AuxGatewayType::ZETA);
  auxGateway->set_id(auxGateway_id_2); //zgc_id

  AuxGateway_zeta *zeta_info = auxGateway->mutable_zeta_info();
  zeta_info->set_port_inband_operation(oam_port_2); //port_ibo

  AuxGateway_destination *destination;

  destination = auxGateway->add_destinations();
  destination->set_ip_address(remote_ip_1); //gw.ip
  destination->set_mac_address(node_mac_address_1); //gw.mac

  destination = auxGateway->add_destinations();
  destination->set_ip_address(remote_ip_2);
  destination->set_mac_address(node_mac_address_2);

  // fill in subnet state structs
  aca_test_create_default_subnet_state(new_subnet_states);

  // fill in port state structs
  aca_test_create_default_port_state(new_port_states);

  GoalStateOperationReply gsOperationalReply;

  //  all the way to here

  retcode = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);

  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(zeta_programming_test_cases, DISABLED_zeta_gateway_path_CHILD)
{
  // TODO: The relative path of the CHILD configuration file
  string zeta_gateway_path_CHILD_config_file = "./test/gtest/aca_data.json";
  aca_test_zeta_setup(zeta_gateway_path_CHILD_config_file);

  // do some validate
  // Simply verify if the group table entry exists or not
  uint group_id = ACA_Zeta_Programming::get_instance().get_group_id(auxGateway_id_1);
  int retcode1,retcode2;
  retcode1 = ACA_Zeta_Programming::get_instance().group_rule_exists(group_id);
  if (retcode1){
    // Further validate if the ip in gws is included in the group entry or not
    retcode2 = test_gws_ip_correct(zeta_gateway_path_CHILD_config_file, group_id);
    EXPECT_EQ(retcode2, true);
  }
  EXPECT_EQ(retcode1, true);
}

TEST(zeta_programming_test_cases, DISABLED_zeta_gateway_path_PARENT)
{
  // TODO: The relative path of the PARENT configuration file
  string zeta_gateway_path_PARENT_config_file = "./test/gtest/aca_data.json";
  aca_test_zeta_setup(zeta_gateway_path_PARENT_config_file);

  // do some validate
  // Simply verify if the group table entry exists or not
  uint group_id = ACA_Zeta_Programming::get_instance().get_group_id(auxGateway_id_1);
  int retcode1,retcode2;
  retcode1 = ACA_Zeta_Programming::get_instance().group_rule_exists(group_id);
  if (retcode1){
    // Further validate if the ip in gws is included in the group entry or not
    retcode2 = test_gws_ip_correct(zeta_gateway_path_PARENT_config_file, group_id);
    EXPECT_EQ(retcode2, true);
  }
  EXPECT_EQ(retcode1, true);
}

TEST(zeta_programming_test_cases, DISABLED_zeta_scale_CHILD)
{
  // ulong culminative_network_configuration_time = 0;
  ulong not_care_culminative_time = 0;
  int overall_rc = EXIT_SUCCESS;
  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
  // set demo mode
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;
  // construct the GoalState from the json file
  string zeta_gateway_path_CHILD_config_file = "./test/gtest/aca_data.json";
  aca_test_zeta_setup(zeta_gateway_path_CHILD_config_file);
  // restore demo mode
  g_demo_mode = previous_demo_mode;
}

TEST(zeta_programming_test_cases, DISABLED_zeta_scale_PARENT)
{
  // ulong culminative_network_configuration_time = 0;
  ulong not_care_culminative_time = 0;
  int overall_rc = EXIT_SUCCESS;
  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
  // set demo mode
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;
  // construct the GoalState from the json file
  string zeta_gateway_path_CHILD_config_file = "./test/gtest/aca_data.json";
  aca_test_zeta_setup(zeta_gateway_path_CHILD_config_file);
  // restore demo mode
  g_demo_mode = previous_demo_mode;
}