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
using namespace aca_net_config;

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
string auxGateway_id_from_aca_data;

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

void create_container(string container_name, string vip_address, string vmac_address)
{
  std::cout << "Creating container with name: " << container_name
            << ", VIP: " << vip_address << ", VMAC: " << vmac_address << std::endl;
  int overall_rc = 0;
  string create_container_cmd = "docker run -itd --name " + container_name +
                                " --net=none --label test=zeta busybox sh";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(create_container_cmd);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  string cmd_string_assign_ip_mac =
          "ovs-docker add-port br-int eth0 " + container_name +
          " --ipaddress=" + vip_address + "/16 --macaddress=" + vmac_address;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string_assign_ip_mac);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  string cmd_string_set_vlan = "ovs-docker set-vlan br-int eth0 " + container_name + " 1";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string_set_vlan);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
}

void aca_test_zeta_setup_container(string zeta_gateway_path_config_file)
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
  GatewayState *new_gateway_states = GoalState_builder.add_gateway_states();

  new_vpc_states->set_operation_type(OperationType::INFO);
  new_gateway_states->set_operation_type(OperationType::CREATE);

  // fill in vpc state structs
  VpcConfiguration *VpcConfiguration_builder = new_vpc_states->mutable_configuration();
  cout << "Filling in vni: " << zeta_data["vpc_response"]["vni"] << endl;
  VpcConfiguration_builder->set_tunnel_id(zeta_data["vpc_response"]["vni"]); //vni
  cout << "Filling in vpc_id: " << zeta_data["vpc_response"]["vpc_id"] << endl;
  VpcConfiguration_builder->set_id(zeta_data["vpc_response"]["vpc_id"]); // vpc_id
  cout << "Filling in gateway_id: " << zeta_data["vpc_response"]["zgc_id"] << endl;
  string *GatewayId_builder = VpcConfiguration_builder->add_gateway_ids();
  *GatewayId_builder = zeta_data["vpc_response"]["zgc_id"];

  // fill in auxgateway state structs
  GatewayConfiguration *GatewayConfiguration_builder =
          new_gateway_states->mutable_configuration();
  GatewayConfiguration_builder->set_gateway_type(GatewayType::ZETA);
  GatewayConfiguration_builder->set_revision_number(1);

  cout << "Filling in zgc_id: " << zeta_data["vpc_response"]["zgc_id"] << endl;

  auxGateway_id_from_aca_data = zeta_data["vpc_response"]["zgc_id"];

  GatewayConfiguration_builder->set_id(zeta_data["vpc_response"]["zgc_id"]); //zgc_id

  GatewayConfiguration_zeta *zeta_info =
          GatewayConfiguration_builder->mutable_zeta_info();
  cout << "Filling in port_ibo: " << zeta_data["vpc_response"]["port_ibo"] << endl; // should be int, not string

  string port_ibo_string = zeta_data["vpc_response"]["port_ibo"];
  uint port_ibo_unit = std::stoul(port_ibo_string, nullptr, 10);
  cout << "uint in port_ibo: " << port_ibo_unit << endl;
  zeta_info->set_port_inband_operation(port_ibo_unit); //port_ibo

  GatewayConfiguration_destination *destination;

  cout << "Try to fill in gw ips/macs now" << endl;

  nlohmann::json gw_array = zeta_data["vpc_response"]["gws"];

  for (nlohmann::json::iterator it = gw_array.begin(); it != gw_array.end(); ++it) {
    cout << "Filling in: " << *it << "to the destination" << endl;
    destination = GatewayConfiguration_builder->add_destinations();
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
    string vip = (*it)["ips_port"][0]["ip"];
    string vmac = (*it)["mac_port"];
    if (aca_is_port_on_same_host(ip_node)) { //  if this ip_node is an ip on one of the interfaces on this machine ...
      cout << "IP: " << ip_node
           << " is on this same machine, add port states to it." << endl;
      PortState *new_port_states = GoalState_builder.add_port_states();
      // fill in port state structs
      aca_test_create_default_port_state_with_zeta_data(new_port_states, *it); // use 0th position for now, but need to check all ports on this host.

      // create containers
      string container_name = "con-" + vip;
      create_container(container_name, vip, vmac);
    }
  }

  GoalStateOperationReply gsOperationalReply;
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
}

// test if the IP in gws is included in the group entry or not
bool test_gws_info_correct(string zeta_gateway_path_config_file, uint group_id)
{
  bool overall_rc;
  ifstream ifs(zeta_gateway_path_config_file);
  if (!ifs)
    cout << zeta_gateway_path_config_file << "open error" << endl;
  nlohmann::json zeta_data = nlohmann::json::parse(ifs);
  nlohmann::json gw_array = zeta_data["vpc_response"]["gws"];
  for (nlohmann::json::iterator it = gw_array.begin(); it != gw_array.end(); ++it) {
    string gws_ip = (*it)["ip"];
    string gws_mac = (*it)["mac"];
    overall_rc = ACA_Zeta_Programming::get_instance().group_rule_info_correct(
            group_id, gws_ip, gws_mac);
    if (overall_rc) {
      cout << "gws_ip:" << gws_ip << ",gws_mac:" << gws_mac << " is in group rule." << endl;
    } else {
      cout << "gws_ip:" << gws_ip << ",gws_mac:" << gws_mac
           << " is not in group rule." << endl;
      return false;
    }
  }
  return true;
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
  GatewayState *new_gateway_states = GoalState_builder.add_gateway_states();

  new_vpc_states->set_operation_type(OperationType::INFO);
  new_gateway_states->set_operation_type(OperationType::CREATE);

  // fill in vpc state structs
  VpcConfiguration *VpcConfiguration_builder = new_vpc_states->mutable_configuration();
  cout << "Filling in vni: " << zeta_data["vpc_response"]["vni"] << endl;
  VpcConfiguration_builder->set_tunnel_id(zeta_data["vpc_response"]["vni"]); //vni
  cout << "Filling in vpc_id: " << zeta_data["vpc_response"]["vpc_id"] << endl;
  VpcConfiguration_builder->set_id(zeta_data["vpc_response"]["vpc_id"]); // vpc_id
  cout << "Filling in gateway_id: " << zeta_data["vpc_response"]["zgc_id"] << endl;
  string *GatewayId_builder = VpcConfiguration_builder->add_gateway_ids();
  *GatewayId_builder = zeta_data["vpc_response"]["zgc_id"];

  // fill in auxgateway state structs
  GatewayConfiguration *GatewayConfiguration_builder =
          new_gateway_states->mutable_configuration();
  GatewayConfiguration_builder->set_gateway_type(GatewayType::ZETA);
  GatewayConfiguration_builder->set_revision_number(1);
  cout << "Filling in zgc_id: " << zeta_data["vpc_response"]["zgc_id"] << endl;

  GatewayConfiguration_builder->set_id(zeta_data["vpc_response"]["zgc_id"]); //zgc_id

  GatewayConfiguration_zeta *zeta_info =
          GatewayConfiguration_builder->mutable_zeta_info();
  cout << "Filling in port_ibo: " << zeta_data["vpc_response"]["port_ibo"] << endl; // should be int, not string

  string port_ibo_string = zeta_data["vpc_response"]["port_ibo"];
  uint port_ibo_unit = std::stoul(port_ibo_string, nullptr, 10);
  cout << "uint in port_ibo: " << port_ibo_unit << endl;
  zeta_info->set_port_inband_operation(port_ibo_unit); //port_ibo

  GatewayConfiguration_destination *destination;

  cout << "Try to fill in gw ips/macs now" << endl;

  nlohmann::json gw_array = zeta_data["vpc_response"]["gws"];

  for (nlohmann::json::iterator it = gw_array.begin(); it != gw_array.end(); ++it) {
    cout << "Filling in: " << *it << "to the destination" << endl;
    destination = GatewayConfiguration_builder->add_destinations();
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

TEST(zeta_programming_test_cases, create_zeta_config_valid)
{
  int retcode = 0;

  // fill in auxgateway state structs
  GatewayConfiguration GatewayConfiguration_builder;
  GatewayConfiguration_builder.set_id(auxGateway_id_1);
  GatewayConfiguration_zeta *zeta_info = GatewayConfiguration_builder.mutable_zeta_info();
  zeta_info->set_port_inband_operation(oam_port_1);
  GatewayConfiguration_destination *destination;

  destination = GatewayConfiguration_builder.add_destinations();
  destination->set_ip_address(remote_ip_1);
  destination->set_mac_address(node_mac_address_3);

  destination = GatewayConfiguration_builder.add_destinations();
  destination->set_ip_address(remote_ip_2);
  destination->set_mac_address(node_mac_address_4);

  retcode = ACA_Zeta_Programming::get_instance().create_zeta_config(
          GatewayConfiguration_builder, tunnel_id_2);

  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(zeta_programming_test_cases, delete_zeta_config_valid)
{
  int retcode = 0;

  // fill in auxgateway state structs
  GatewayConfiguration GatewayConfiguration_builder;
  GatewayConfiguration_builder.set_id(auxGateway_id_1);
  GatewayConfiguration_zeta *zeta_info = GatewayConfiguration_builder.mutable_zeta_info();
  zeta_info->set_port_inband_operation(oam_port_1);
  GatewayConfiguration_destination *destination;

  destination = GatewayConfiguration_builder.add_destinations();
  destination->set_ip_address(remote_ip_1);
  destination->set_mac_address(node_mac_address_3);

  destination = GatewayConfiguration_builder.add_destinations();
  destination->set_ip_address(remote_ip_2);
  destination->set_mac_address(node_mac_address_4);

  retcode = ACA_Zeta_Programming::get_instance().delete_zeta_config(
          GatewayConfiguration_builder, tunnel_id_2);

  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(zeta_programming_test_cases, DISABLED_create_auxgateway_test)
{
  ulong not_care_culminative_time = 0;
  int overall_rc;
  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  // from here.
  int retcode;
  GoalState GoalState_builder;
  VpcState *new_vpc_states = GoalState_builder.add_vpc_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
  PortState *new_port_states = GoalState_builder.add_port_states();
  GatewayState *new_gateway_states = GoalState_builder.add_gateway_states();

  new_vpc_states->set_operation_type(OperationType::INFO);
  new_gateway_states->set_operation_type(OperationType::CREATE);

  // fill in vpc state structs
  VpcConfiguration *VpcConfiguration_builder = new_vpc_states->mutable_configuration();
  VpcConfiguration_builder->set_tunnel_id(tunnel_id_1); //vni
  VpcConfiguration_builder->set_id(vpc_id_1); // vpc_id
  string *GatewayId_builder = VpcConfiguration_builder->add_gateway_ids();
  *GatewayId_builder = auxGateway_id_2; // zgc_id

  // fill in auxgateway state structs
  GatewayConfiguration *GatewayConfiguration_builder =
          new_gateway_states->mutable_configuration();
  GatewayConfiguration_builder->set_gateway_type(GatewayType::ZETA);
  GatewayConfiguration_builder->set_revision_number(1);
  GatewayConfiguration_builder->set_id(auxGateway_id_2); //zgc_id

  GatewayConfiguration_zeta *zeta_info =
          GatewayConfiguration_builder->mutable_zeta_info();
  zeta_info->set_port_inband_operation(oam_port_2); //port_ibo

  GatewayConfiguration_destination *destination;

  destination = GatewayConfiguration_builder->add_destinations();
  destination->set_ip_address(remote_ip_1); //gw.ip
  destination->set_mac_address(node_mac_address_1); //gw.mac

  destination = GatewayConfiguration_builder->add_destinations();
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
  sleep(120);
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
  sleep(120);
  // restore demo mode
  g_demo_mode = previous_demo_mode;
}

TEST(zeta_programming_test_cases, DISABLED_zeta_scale_container)
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
  g_demo_mode = false;
  // construct the GoalState from the json file
  string zeta_gateway_path_CHILD_config_file = "./test/gtest/aca_data.json";
  aca_test_zeta_setup_container(zeta_gateway_path_CHILD_config_file);

  // do some validate
  uint group_id = ACA_Zeta_Programming::get_instance().get_group_id(auxGateway_id_from_aca_data);
  if (group_id == 0) {
    cout << "group_id:" << group_id << " not exist" << endl;
  } else {
    int retcode1 = 0, retcode2 = 0;
    retcode1 = ACA_Zeta_Programming::get_instance().group_rule_exists(group_id);
    if (retcode1) {
      cout << "group rule exist" << endl;
      // Further validate if the ip in gws is included in the group entry or not
      retcode2 = test_gws_info_correct(zeta_gateway_path_CHILD_config_file, group_id);
      if (retcode2) {
        cout << "group rule is right" << endl;
      } else {
        cout << "group rule is not right" << endl;
      }
    } else {
      cout << "group rule not exist" << endl;
    }
  }

  // restore demo mode
  g_demo_mode = previous_demo_mode;
}