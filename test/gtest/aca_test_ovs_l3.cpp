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

#include "aca_log.h"
#include "aca_util.h"
#include "aca_config.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_comm_mgr.h"
#include "gtest/gtest.h"
#include "goalstate.pb.h"
#include <unistd.h> /* for getopt */
#include <iostream>
#include <string>

using namespace std;
using namespace alcor::schema;
using aca_comm_manager::Aca_Comm_Manager;
using aca_net_config::Aca_Net_Config;
using aca_ovs_l2_programmer::ACA_OVS_L2_Programmer;

// extern the string and helper functions from aca_test_ovs_util.cpp
extern string project_id;
extern string vpc_id_1;
extern string vpc_id_2;
extern string subnet_id_1;
extern string subnet_id_2;
extern string port_id_1;
extern string port_id_2;
extern string port_id_3;
extern string port_id_4;
extern string port_name_1;
extern string port_name_2;
extern string port_name_3;
extern string port_name_4;
extern string vmac_address_1;
extern string vmac_address_2;
extern string vmac_address_3;
extern string vmac_address_4;
extern string vip_address_1;
extern string vip_address_2;
extern string vip_address_3;
extern string vip_address_4;
extern string remote_ip_1; // for docker network
extern string remote_ip_2; // for docker network
extern NetworkType vxlan_type;
extern string subnet1_gw_ip;
extern string subnet2_gw_ip;
extern string subnet1_gw_mac;
extern string subnet2_gw_mac;
extern bool g_demo_mode;

extern void aca_test_create_default_port_state(PortState *new_port_states);
extern void aca_test_create_default_subnet_state(SubnetState *new_subnet_states);
extern void aca_test_1_neighbor_CREATE_DELETE(NeighborType input_neighbor_type);
extern void aca_test_1_port_CREATE_plus_neighbor_CREATE(NeighborType input_neighbor_type);
extern void aca_test_10_neighbor_CREATE(NeighborType input_neighbor_type);
extern void aca_test_create_default_router_goal_state(GoalState *goalState_builder);

static string host1_dvr_mac_address = HOST_DVR_MAC_PREFIX + string("d7:f2:01");
static string host2_dvr_mac_address = HOST_DVR_MAC_PREFIX + string("d7:f2:02");

//
// Test suite: ovs_l3_test_cases
//
// Testing the ovs dataplane l3 implementation, including add/delete router and and
// test traffics routing.
// Note: traffic tests requires physical machine or VM setup (not container) because
// it needs to create docker containers, therefore traffic tests are DISABLED by default
//   it can be executed by:
//
//     one machine routing test (-c 10.213.43.188 -> IP of local machine):
//     aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_2_ports_ROUTING_test_traffic_one_machine -c 10.213.43.188
//
//     two machines routing test: child machine (-p 10.213.43.187 -> IP of parent machine):
//     aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_2_ports_ROUTING_test_traffic_CHILD -p 10.213.43.187
//     two machines routing test: parent machine (-c 10.213.43.188 -> IP of child machine):
//     aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_2_ports_ROUTING_test_traffic_PARENT -c 10.213.43.188
//
TEST(ovs_l3_test_cases, ADD_DELETE_ROUTER_test_no_traffic)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  // delete and add br-int and br-tun bridges to clear everything
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // program the router
  GoalState GoalState_builder2;
  RouterState *new_router_states = GoalState_builder2.add_router_states();
  SubnetState *new_subnet_states1 = GoalState_builder2.add_subnet_states();
  SubnetState *new_subnet_states2 = GoalState_builder2.add_subnet_states();

  // fill in router state structs
  RouterConfiguration *RouterConfiguration_builder =
          new_router_states->mutable_configuration();
  RouterConfiguration_builder->set_revision_number(1);

  RouterConfiguration_builder->set_id("router_id1");
  RouterConfiguration_builder->set_host_dvr_mac_address("fa:16:3e:d7:f2:02");

  auto *RouterConfiguration_SubnetRoutingTable_builder1 =
          RouterConfiguration_builder->add_subnet_routing_tables();
  RouterConfiguration_SubnetRoutingTable_builder1->set_subnet_id(subnet_id_1);
  auto *RouterConfiguration_SubnetRoutingRule_builder1 =
          RouterConfiguration_SubnetRoutingTable_builder1->add_routing_rules();
  RouterConfiguration_SubnetRoutingRule_builder1->set_operation_type(OperationType::CREATE);
  RouterConfiguration_SubnetRoutingRule_builder1->set_id("12345");
  RouterConfiguration_SubnetRoutingRule_builder1->set_name("routing_rule_1");
  RouterConfiguration_SubnetRoutingRule_builder1->set_destination("154.12.42.24/32");
  RouterConfiguration_SubnetRoutingRule_builder1->set_next_hop_ip("154.12.42.101");
  auto *routerConfiguration_RoutingRuleExtraInfo_builder1(new RouterConfiguration_RoutingRuleExtraInfo);
  routerConfiguration_RoutingRuleExtraInfo_builder1->set_destination_type(
          DestinationType::INTERNET);
  routerConfiguration_RoutingRuleExtraInfo_builder1->set_next_hop_mac("fa:16:3e:d7:aa:02");
  RouterConfiguration_SubnetRoutingRule_builder1->set_allocated_routing_rule_extra_info(
          routerConfiguration_RoutingRuleExtraInfo_builder1);

  auto *RouterConfiguration_SubnetRoutingTable_builder2 =
          RouterConfiguration_builder->add_subnet_routing_tables();
  RouterConfiguration_SubnetRoutingTable_builder2->set_subnet_id(subnet_id_2);
  auto *RouterConfiguration_SubnetRoutingRule_builder2 =
          RouterConfiguration_SubnetRoutingTable_builder2->add_routing_rules();
  RouterConfiguration_SubnetRoutingRule_builder2->set_operation_type(OperationType::UPDATE);
  RouterConfiguration_SubnetRoutingRule_builder2->set_id("23456");
  RouterConfiguration_SubnetRoutingRule_builder2->set_name("routing_rule_2");
  RouterConfiguration_SubnetRoutingRule_builder2->set_destination("154.12.54.24/32");
  RouterConfiguration_SubnetRoutingRule_builder2->set_next_hop_ip("154.12.54.101");
  auto *routerConfiguration_RoutingRuleExtraInfo_builder2(new RouterConfiguration_RoutingRuleExtraInfo);
  routerConfiguration_RoutingRuleExtraInfo_builder2->set_destination_type(DestinationType::VPC_GW);
  routerConfiguration_RoutingRuleExtraInfo_builder2->set_next_hop_mac("fa:16:3e:d7:bb:02");
  RouterConfiguration_SubnetRoutingRule_builder2->set_allocated_routing_rule_extra_info(
          routerConfiguration_RoutingRuleExtraInfo_builder2);

  // fill in subnet state1 structs
  aca_test_create_default_subnet_state(new_subnet_states1);

  // fill in subnet state2 structs
  new_subnet_states2->set_operation_type(OperationType::INFO);

  SubnetConfiguration *SubnetConiguration_builder2 =
          new_subnet_states2->mutable_configuration();
  SubnetConiguration_builder2->set_revision_number(1);
  SubnetConiguration_builder2->set_project_id(project_id);
  SubnetConiguration_builder2->set_vpc_id(vpc_id_2);
  SubnetConiguration_builder2->set_id(subnet_id_2);
  SubnetConiguration_builder2->set_cidr("10.10.1.0/24");
  SubnetConiguration_builder2->set_tunnel_id(30);

  auto *subnetConfig_GatewayBuilder2(new SubnetConfiguration_Gateway);
  subnetConfig_GatewayBuilder2->set_ip_address(subnet2_gw_ip);
  subnetConfig_GatewayBuilder2->set_mac_address(subnet2_gw_mac);
  SubnetConiguration_builder2->set_allocated_gateway(subnetConfig_GatewayBuilder2);

  GoalStateOperationReply gsOperationalReply;

  // try to delete a non-existant router now
  new_router_states->set_operation_type(OperationType::DELETE);
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder2, gsOperationalReply);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);

  // create the router
  new_router_states->set_operation_type(OperationType::CREATE);
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder2, gsOperationalReply);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // try to delete a valid router
  new_router_states->set_operation_type(OperationType::DELETE);
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder2, gsOperationalReply);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
}

TEST(ovs_l3_test_cases, 1_l3_neighbor_CREATE_DELETE)
{
  aca_test_1_neighbor_CREATE_DELETE(NeighborType::L3);
}

TEST(ovs_l3_test_cases, 1_port_CREATE_plus_l3_neighbor_CREATE)
{
  aca_test_1_port_CREATE_plus_neighbor_CREATE(NeighborType::L3);
}

TEST(ovs_l3_test_cases, 10_l3_neighbor_CREATE)
{
  aca_test_10_neighbor_CREATE(NeighborType::L3);
}

TEST(ovs_l3_test_cases, DISABLED_2_ports_ROUTING_test_traffic_one_machine)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  // delete and add br-int and br-tun bridges to clear everything
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // kill the docker instances just in case
  Aca_Net_Config::get_instance().execute_system_command("docker kill con3");

  Aca_Net_Config::get_instance().execute_system_command("docker rm con3");

  Aca_Net_Config::get_instance().execute_system_command("docker kill con4");

  Aca_Net_Config::get_instance().execute_system_command("docker rm con4");

  GoalState GoalState_builder;
  PortState *new_port_states = GoalState_builder.add_port_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  new_port_states->set_operation_type(OperationType::CREATE);

  // fill in port state structs for port 3
  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_message_type(MessageType::FULL);
  PortConfiguration_builder->set_id(port_id_3);

  PortConfiguration_builder->set_project_id(project_id);
  PortConfiguration_builder->set_vpc_id(vpc_id_1);
  PortConfiguration_builder->set_name(port_name_3);
  PortConfiguration_builder->set_mac_address(vmac_address_3);
  PortConfiguration_builder->set_admin_state_up(true);

  PortConfiguration_FixedIp *FixedIp_builder = PortConfiguration_builder->add_fixed_ips();
  FixedIp_builder->set_subnet_id(subnet_id_1);
  FixedIp_builder->set_ip_address(vip_address_3);

  PortConfiguration_SecurityGroupId *SecurityGroup_builder =
          PortConfiguration_builder->add_security_group_ids();
  SecurityGroup_builder->set_id("1");

  // fill in subnet state structs
  new_subnet_states->set_operation_type(OperationType::INFO);

  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_project_id(project_id);
  SubnetConiguration_builder->set_vpc_id(vpc_id_1);
  SubnetConiguration_builder->set_id(subnet_id_1);
  SubnetConiguration_builder->set_cidr("10.10.0.0/24");
  SubnetConiguration_builder->set_tunnel_id(20);

  // set demo mode to false because routing test needs real routable port
  // e.g. container port created by docker + ovs-docker
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = false;

  Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --name con3 --net=none busybox sh");

  cmd_string = "ovs-docker add-port br-int eth0 con3 --ipaddress=" + vip_address_3 +
               "/24 --gateway=" + subnet1_gw_ip + " --macaddress=" + vmac_address_3;
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ovs-docker set-vlan br-int eth0 con3 1";
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --name con4 --net=none busybox sh");

  cmd_string = "ovs-docker add-port br-int eth0 con4 --ipaddress=" + vip_address_4 +
               "/24 --gateway=" + subnet2_gw_ip + " --macaddress=" + vmac_address_4;
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ovs-docker set-vlan br-int eth0 con4 2";
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // configure new port 3
  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // setup the configuration for port 4
  PortConfiguration_builder->set_id(port_id_4);
  PortConfiguration_builder->set_vpc_id(vpc_id_2);
  PortConfiguration_builder->set_name(port_name_4);
  PortConfiguration_builder->set_mac_address(vmac_address_4);
  FixedIp_builder->set_ip_address(vip_address_4);
  FixedIp_builder->set_subnet_id(subnet_id_2);

  // fill in subnet state structs
  SubnetConiguration_builder->set_vpc_id(vpc_id_2);
  SubnetConiguration_builder->set_id(subnet_id_2);
  SubnetConiguration_builder->set_cidr("10.10.1.0/24");
  SubnetConiguration_builder->set_tunnel_id(30);

  // create a new port 4
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // restore demo mode
  g_demo_mode = previous_demo_mode;

  // should be able to ping itselves
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con3 ping -c1 " + vip_address_3);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con4 ping -c1 " + vip_address_4);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // test traffic between the two newly created ports, it should not work
  // because they are from different subnet
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con3 ping -c1 " + vip_address_4);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con4 ping -c1 " + vip_address_3);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_subnet_states->clear_configuration();

  // program the router
  GoalState GoalState_builder2;
  aca_test_create_default_router_goal_state(&GoalState_builder2);

  // create the router
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder2, gsOperationalReply);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // should be able to ping the GWs now
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con3 ping -c1 " + subnet1_gw_ip);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con4 ping -c1 " + subnet2_gw_ip);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // just clearing the router states and reuse the two subnet states in GoalState_builder2
  GoalState_builder2.clear_router_states();

  // program the L3 neighbor rules
  NeighborState *new_neighbor_states3 = GoalState_builder2.add_neighbor_states();
  NeighborState *new_neighbor_states4 = GoalState_builder2.add_neighbor_states();

  // fill in neighbor state structs for port 3
  new_neighbor_states3->set_operation_type(OperationType::CREATE);

  NeighborConfiguration *NeighborConfiguration_builder3 =
          new_neighbor_states3->mutable_configuration();
  NeighborConfiguration_builder3->set_revision_number(1);

  NeighborConfiguration_builder3->set_project_id(project_id);
  NeighborConfiguration_builder3->set_vpc_id(vpc_id_1);
  NeighborConfiguration_builder3->set_id(port_id_3);
  NeighborConfiguration_builder3->set_name(port_name_3);
  NeighborConfiguration_builder3->set_mac_address(vmac_address_3);
  NeighborConfiguration_builder3->set_host_ip_address(remote_ip_2);

  NeighborConfiguration_FixedIp *FixedIp_builder3 =
          NeighborConfiguration_builder3->add_fixed_ips();
  FixedIp_builder3->set_neighbor_type(NeighborType::L3);
  FixedIp_builder3->set_subnet_id(subnet_id_1);
  FixedIp_builder3->set_ip_address(vip_address_3);

  // fill in neighbor state structs for port 4
  new_neighbor_states4->set_operation_type(OperationType::CREATE);

  NeighborConfiguration *NeighborConfiguration_builder4 =
          new_neighbor_states4->mutable_configuration();
  NeighborConfiguration_builder4->set_revision_number(1);

  NeighborConfiguration_builder4->set_project_id(project_id);
  NeighborConfiguration_builder4->set_vpc_id(vpc_id_2);
  NeighborConfiguration_builder4->set_id(port_id_4);
  NeighborConfiguration_builder4->set_name(port_name_4);
  NeighborConfiguration_builder4->set_mac_address(vmac_address_4);
  NeighborConfiguration_builder4->set_host_ip_address(remote_ip_2);

  NeighborConfiguration_FixedIp *FixedIp_builder4 =
          NeighborConfiguration_builder4->add_fixed_ips();
  FixedIp_builder4->set_neighbor_type(NeighborType::L3);
  FixedIp_builder4->set_subnet_id(subnet_id_2);
  FixedIp_builder4->set_ip_address(vip_address_4);

  // create the L3 neighbors
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder2, gsOperationalReply);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // should be able to ping each other now using router
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con3 ping -c1 " + vip_address_4);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con4 ping -c1 " + vip_address_3);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // free the allocated configurations since we are done with it now
  new_neighbor_states3->clear_configuration();
  new_neighbor_states4->clear_configuration();

  // cleanup

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker kill con3");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker rm con3");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker kill con4");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker rm con4");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
}

TEST(ovs_l3_test_cases, DISABLED_2_ports_ROUTING_test_traffic_PARENT)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  // delete and add br-int and br-tun bridges to clear everything
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // kill the docker instances just in case
  Aca_Net_Config::get_instance().execute_system_command("docker kill con1");

  Aca_Net_Config::get_instance().execute_system_command("docker rm con1");

  Aca_Net_Config::get_instance().execute_system_command("docker kill con2");

  Aca_Net_Config::get_instance().execute_system_command("docker rm con2");

  GoalState GoalState_builder;
  PortState *new_port_states = GoalState_builder.add_port_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  new_port_states->set_operation_type(OperationType::CREATE);

  // fill in port state structs for port 1
  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_message_type(MessageType::FULL);
  PortConfiguration_builder->set_id(port_id_1);

  PortConfiguration_builder->set_project_id(project_id);
  PortConfiguration_builder->set_vpc_id(vpc_id_1);
  PortConfiguration_builder->set_name(port_name_1);
  PortConfiguration_builder->set_mac_address(vmac_address_1);
  PortConfiguration_builder->set_admin_state_up(true);

  PortConfiguration_FixedIp *FixedIp_builder = PortConfiguration_builder->add_fixed_ips();
  FixedIp_builder->set_subnet_id(subnet_id_1);
  FixedIp_builder->set_ip_address(vip_address_1);

  PortConfiguration_SecurityGroupId *SecurityGroup_builder =
          PortConfiguration_builder->add_security_group_ids();
  SecurityGroup_builder->set_id("1");

  // fill in subnet state structs
  new_subnet_states->set_operation_type(OperationType::INFO);

  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_project_id(project_id);
  SubnetConiguration_builder->set_vpc_id(vpc_id_1);
  SubnetConiguration_builder->set_id(subnet_id_1);
  SubnetConiguration_builder->set_cidr("10.10.0.0/24");
  SubnetConiguration_builder->set_tunnel_id(20);

  // set demo mode to false because routing test needs real routable port
  // e.g. container port created by docker + ovs-docker
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = false;

  Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --name con1 --net=none busybox sh");

  cmd_string = "ovs-docker add-port br-int eth0 con1 --ipaddress=" + vip_address_1 +
               "/24 --gateway=" + subnet1_gw_ip + " --macaddress=" + vmac_address_1;
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ovs-docker set-vlan br-int eth0 con1 1";
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --name con2 --net=none busybox sh");

  cmd_string = "ovs-docker add-port br-int eth0 con2 --ipaddress=" + vip_address_2 +
               "/24 --gateway=" + subnet2_gw_ip + " --macaddress=" + vmac_address_2;
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ovs-docker set-vlan br-int eth0 con2 2";
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // configure new port 1
  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // setup the configuration for port 2
  PortConfiguration_builder->set_id(port_id_2);
  PortConfiguration_builder->set_vpc_id(vpc_id_2);
  PortConfiguration_builder->set_name(port_name_2);
  PortConfiguration_builder->set_mac_address(vmac_address_2);
  FixedIp_builder->set_ip_address(vip_address_2);
  FixedIp_builder->set_subnet_id(subnet_id_2);

  // fill in subnet state structs
  SubnetConiguration_builder->set_vpc_id(vpc_id_2);
  SubnetConiguration_builder->set_id(subnet_id_2);
  SubnetConiguration_builder->set_cidr("10.10.1.0/24");
  SubnetConiguration_builder->set_tunnel_id(30);

  // create a new port 2
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // restore demo mode
  g_demo_mode = previous_demo_mode;

  // should be able to ping itselves
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con1 ping -c1 " + vip_address_1);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con2 ping -c1 " + vip_address_2);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // test traffic between the ports without router on same subnet
  // expect to fail before neighbor info is not programmed yet
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con1 ping -c1 " + vip_address_3);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con2 ping -c1 " + vip_address_4);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // test traffic between the ports without router on different subnet
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con1 ping -c1 " + vip_address_4);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con2 ping -c1 " + vip_address_3);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_subnet_states->clear_configuration();

  // program the router
  GoalState GoalState_builder2;
  RouterState *new_router_states = GoalState_builder2.add_router_states();
  SubnetState *new_subnet_states1 = GoalState_builder2.add_subnet_states();
  SubnetState *new_subnet_states2 = GoalState_builder2.add_subnet_states();

  new_router_states->set_operation_type(OperationType::CREATE);

  // fill in router state structs
  RouterConfiguration *RouterConfiguration_builder =
          new_router_states->mutable_configuration();
  RouterConfiguration_builder->set_revision_number(1);

  RouterConfiguration_builder->set_id("router_id1");
  RouterConfiguration_builder->set_host_dvr_mac_address(host1_dvr_mac_address);

  auto *RouterConfiguration_SubnetRoutingTable_builder =
          RouterConfiguration_builder->add_subnet_routing_tables();
  RouterConfiguration_SubnetRoutingTable_builder->set_subnet_id(subnet_id_1);
  auto *RouterConfiguration_SubnetRoutingTable_builder2 =
          RouterConfiguration_builder->add_subnet_routing_tables();
  RouterConfiguration_SubnetRoutingTable_builder2->set_subnet_id(subnet_id_2);

  // fill in subnet state structs
  aca_test_create_default_subnet_state(new_subnet_states1);

  // fill in subnet state structs
  new_subnet_states2->set_operation_type(OperationType::INFO);

  SubnetConfiguration *SubnetConiguration_builder2 =
          new_subnet_states2->mutable_configuration();
  SubnetConiguration_builder2->set_revision_number(1);
  SubnetConiguration_builder2->set_project_id(project_id);
  SubnetConiguration_builder2->set_vpc_id(vpc_id_2);
  SubnetConiguration_builder2->set_id(subnet_id_2);
  SubnetConiguration_builder2->set_cidr("10.10.1.0/24");
  SubnetConiguration_builder2->set_tunnel_id(30);

  auto *subnetConfig_GatewayBuilder2(new SubnetConfiguration_Gateway);
  subnetConfig_GatewayBuilder2->set_ip_address(subnet2_gw_ip);
  subnetConfig_GatewayBuilder2->set_mac_address(subnet2_gw_mac);
  SubnetConiguration_builder2->set_allocated_gateway(subnetConfig_GatewayBuilder2);

  // create the router
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder2, gsOperationalReply);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // should be able to ping the GWs now
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con1 ping -c1 " + subnet1_gw_ip);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con2 ping -c1 " + subnet2_gw_ip);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // clear the allocated router configurations since we are done with it now
  new_router_states->clear_configuration();

  // just clearing the router states and reuse the two subnet states in GoalState_builder2
  GoalState_builder2.clear_router_states();

  // program the L3 neighbor rules
  NeighborState *new_neighbor_states3 = GoalState_builder2.add_neighbor_states();
  NeighborState *new_neighbor_states4 = GoalState_builder2.add_neighbor_states();

  // fill in neighbor state structs for port 3
  new_neighbor_states3->set_operation_type(OperationType::CREATE);

  NeighborConfiguration *NeighborConfiguration_builder3 =
          new_neighbor_states3->mutable_configuration();
  NeighborConfiguration_builder3->set_revision_number(1);

  NeighborConfiguration_builder3->set_project_id(project_id);
  NeighborConfiguration_builder3->set_vpc_id(vpc_id_1);
  NeighborConfiguration_builder3->set_id(port_id_3);
  NeighborConfiguration_builder3->set_name(port_name_3);
  NeighborConfiguration_builder3->set_mac_address(vmac_address_3);
  NeighborConfiguration_builder3->set_host_ip_address(remote_ip_2);

  NeighborConfiguration_FixedIp *FixedIp_builder3 =
          NeighborConfiguration_builder3->add_fixed_ips();
  FixedIp_builder3->set_neighbor_type(NeighborType::L3);
  FixedIp_builder3->set_subnet_id(subnet_id_1);
  FixedIp_builder3->set_ip_address(vip_address_3);

  // fill in neighbor state structs for port 4
  new_neighbor_states4->set_operation_type(OperationType::CREATE);

  NeighborConfiguration *NeighborConfiguration_builder4 =
          new_neighbor_states4->mutable_configuration();
  NeighborConfiguration_builder4->set_revision_number(1);

  NeighborConfiguration_builder4->set_project_id(project_id);
  NeighborConfiguration_builder4->set_vpc_id(vpc_id_2);
  NeighborConfiguration_builder4->set_id(port_id_4);
  NeighborConfiguration_builder4->set_name(port_name_4);
  NeighborConfiguration_builder4->set_mac_address(vmac_address_4);
  NeighborConfiguration_builder4->set_host_ip_address(remote_ip_2);

  NeighborConfiguration_FixedIp *FixedIp_builder4 =
          NeighborConfiguration_builder4->add_fixed_ips();
  FixedIp_builder4->set_neighbor_type(NeighborType::L3);
  FixedIp_builder4->set_subnet_id(subnet_id_2);
  FixedIp_builder4->set_ip_address(vip_address_4);

  // create the L3 neighbors
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder2, gsOperationalReply);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // test traffic between the ports with router on same subnet
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con1 ping -c1 " + vip_address_3);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con2 ping -c1 " + vip_address_4);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // test traffic between the ports without router on different subnet
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con1 ping -c1 " + vip_address_4);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con2 ping -c1 " + vip_address_3);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // free the allocated configurations since we are done with it now
  new_neighbor_states3->clear_configuration();
  new_neighbor_states4->clear_configuration();
  new_subnet_states1->clear_configuration();
  new_subnet_states2->clear_configuration();

  // cleanup

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker kill con1");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker rm con1");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker kill con2");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker rm con2");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
}

TEST(ovs_l3_test_cases, DISABLED_2_ports_ROUTING_test_traffic_CHILD)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  // delete and add br-int and br-tun bridges to clear everything
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // kill the docker instances just in case
  Aca_Net_Config::get_instance().execute_system_command("docker kill con3");

  Aca_Net_Config::get_instance().execute_system_command("docker rm con3");

  Aca_Net_Config::get_instance().execute_system_command("docker kill con4");

  Aca_Net_Config::get_instance().execute_system_command("docker rm con4");

  GoalState GoalState_builder;
  PortState *new_port_states = GoalState_builder.add_port_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  new_port_states->set_operation_type(OperationType::CREATE);

  // fill in port state structs for port 3
  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_message_type(MessageType::FULL);
  PortConfiguration_builder->set_id(port_id_3);

  PortConfiguration_builder->set_project_id(project_id);
  PortConfiguration_builder->set_vpc_id(vpc_id_1);
  PortConfiguration_builder->set_name(port_name_3);
  PortConfiguration_builder->set_mac_address(vmac_address_3);
  PortConfiguration_builder->set_admin_state_up(true);

  PortConfiguration_FixedIp *FixedIp_builder = PortConfiguration_builder->add_fixed_ips();
  FixedIp_builder->set_subnet_id(subnet_id_1);
  FixedIp_builder->set_ip_address(vip_address_3);

  PortConfiguration_SecurityGroupId *SecurityGroup_builder =
          PortConfiguration_builder->add_security_group_ids();
  SecurityGroup_builder->set_id("1");

  // fill in subnet state structs
  new_subnet_states->set_operation_type(OperationType::INFO);

  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_project_id(project_id);
  SubnetConiguration_builder->set_vpc_id(vpc_id_1);
  SubnetConiguration_builder->set_id(subnet_id_1);
  SubnetConiguration_builder->set_cidr("10.10.0.0/24");
  SubnetConiguration_builder->set_tunnel_id(20);

  // set demo mode to false because routing test needs real routable port
  // e.g. container port created by docker + ovs-docker
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = false;

  Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --name con3 --net=none busybox sh");

  cmd_string = "ovs-docker add-port br-int eth0 con3 --ipaddress=" + vip_address_3 +
               "/24 --gateway=" + subnet1_gw_ip + " --macaddress=" + vmac_address_3;
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ovs-docker set-vlan br-int eth0 con3 1";
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --name con4 --net=none busybox sh");

  cmd_string = "ovs-docker add-port br-int eth0 con4 --ipaddress=" + vip_address_4 +
               "/24 --gateway=" + subnet2_gw_ip + " --macaddress=" + vmac_address_4;
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ovs-docker set-vlan br-int eth0 con4 2";
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // configure new port 3
  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // setup the configuration for port 4
  PortConfiguration_builder->set_id(port_id_4);
  PortConfiguration_builder->set_vpc_id(vpc_id_2);
  PortConfiguration_builder->set_name(port_name_4);
  PortConfiguration_builder->set_mac_address(vmac_address_4);
  FixedIp_builder->set_ip_address(vip_address_4);
  FixedIp_builder->set_subnet_id(subnet_id_2);

  // fill in subnet state structs
  SubnetConiguration_builder->set_vpc_id(vpc_id_2);
  SubnetConiguration_builder->set_id(subnet_id_2);
  SubnetConiguration_builder->set_cidr("10.10.1.0/24");
  SubnetConiguration_builder->set_tunnel_id(30);

  // create a new port 4
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // restore demo mode
  g_demo_mode = previous_demo_mode;

  // should be able to ping itselves
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con3 ping -c1 " + vip_address_3);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con4 ping -c1 " + vip_address_4);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_subnet_states->clear_configuration();

  // program the router
  GoalState GoalState_builder2;
  RouterState *new_router_states = GoalState_builder2.add_router_states();
  SubnetState *new_subnet_states1 = GoalState_builder2.add_subnet_states();
  SubnetState *new_subnet_states2 = GoalState_builder2.add_subnet_states();

  new_router_states->set_operation_type(OperationType::CREATE);

  // fill in router state structs
  RouterConfiguration *RouterConfiguration_builder =
          new_router_states->mutable_configuration();
  RouterConfiguration_builder->set_revision_number(1);

  RouterConfiguration_builder->set_id("router_id2");
  RouterConfiguration_builder->set_host_dvr_mac_address(host2_dvr_mac_address);

  auto *RouterConfiguration_SubnetRoutingTable_builder =
          RouterConfiguration_builder->add_subnet_routing_tables();
  RouterConfiguration_SubnetRoutingTable_builder->set_subnet_id(subnet_id_1);
  auto *RouterConfiguration_SubnetRoutingTable_builder2 =
          RouterConfiguration_builder->add_subnet_routing_tables();
  RouterConfiguration_SubnetRoutingTable_builder2->set_subnet_id(subnet_id_2);

  // fill in subnet state structs
  aca_test_create_default_subnet_state(new_subnet_states1);

  // fill in subnet state structs
  new_subnet_states2->set_operation_type(OperationType::INFO);

  SubnetConfiguration *SubnetConiguration_builder2 =
          new_subnet_states2->mutable_configuration();
  SubnetConiguration_builder2->set_revision_number(1);
  SubnetConiguration_builder2->set_project_id(project_id);
  SubnetConiguration_builder2->set_vpc_id(vpc_id_2);
  SubnetConiguration_builder2->set_id(subnet_id_2);
  SubnetConiguration_builder2->set_cidr("10.10.1.0/24");
  SubnetConiguration_builder2->set_tunnel_id(30);

  auto *subnetConfig_GatewayBuilder2(new SubnetConfiguration_Gateway);
  subnetConfig_GatewayBuilder2->set_ip_address(subnet2_gw_ip);
  subnetConfig_GatewayBuilder2->set_mac_address(subnet2_gw_mac);
  SubnetConiguration_builder2->set_allocated_gateway(subnetConfig_GatewayBuilder2);

  // create the router
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder2, gsOperationalReply);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // should be able to ping the GWs now
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con3 ping -c1 " + subnet1_gw_ip);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker exec con4 ping -c1 " + subnet2_gw_ip);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // clear the allocated router configurations since we are done with it now
  new_router_states->clear_configuration();

  // just clearing the router states and reuse the two subnet states in GoalState_builder2
  GoalState_builder2.clear_router_states();

  // program the L3 neighbor rules
  NeighborState *new_neighbor_states3 = GoalState_builder2.add_neighbor_states();
  NeighborState *new_neighbor_states4 = GoalState_builder2.add_neighbor_states();

  // fill in neighbor state structs for port 1
  new_neighbor_states3->set_operation_type(OperationType::CREATE);

  NeighborConfiguration *NeighborConfiguration_builder3 =
          new_neighbor_states3->mutable_configuration();
  NeighborConfiguration_builder3->set_revision_number(1);

  NeighborConfiguration_builder3->set_project_id(project_id);
  NeighborConfiguration_builder3->set_vpc_id(vpc_id_1);
  NeighborConfiguration_builder3->set_id(port_id_1);
  NeighborConfiguration_builder3->set_name(port_name_1);
  NeighborConfiguration_builder3->set_mac_address(vmac_address_1);
  NeighborConfiguration_builder3->set_host_ip_address(remote_ip_1);

  NeighborConfiguration_FixedIp *FixedIp_builder3 =
          NeighborConfiguration_builder3->add_fixed_ips();
  FixedIp_builder3->set_neighbor_type(NeighborType::L3);
  FixedIp_builder3->set_subnet_id(subnet_id_1);
  FixedIp_builder3->set_ip_address(vip_address_1);

  // fill in neighbor state structs for port 2
  new_neighbor_states4->set_operation_type(OperationType::CREATE);

  NeighborConfiguration *NeighborConfiguration_builder4 =
          new_neighbor_states4->mutable_configuration();
  NeighborConfiguration_builder4->set_revision_number(1);

  NeighborConfiguration_builder4->set_project_id(project_id);
  NeighborConfiguration_builder4->set_vpc_id(vpc_id_2);
  NeighborConfiguration_builder4->set_id(port_id_2);
  NeighborConfiguration_builder4->set_name(port_name_2);
  NeighborConfiguration_builder4->set_mac_address(vmac_address_2);
  NeighborConfiguration_builder4->set_host_ip_address(remote_ip_1);

  NeighborConfiguration_FixedIp *FixedIp_builder4 =
          NeighborConfiguration_builder4->add_fixed_ips();
  FixedIp_builder4->set_neighbor_type(NeighborType::L3);
  FixedIp_builder4->set_subnet_id(subnet_id_2);
  FixedIp_builder4->set_ip_address(vip_address_2);

  // create the L3 neighbors
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder2, gsOperationalReply);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // cleanup

  // free the allocated configurations since we are done with it now
  new_neighbor_states3->clear_configuration();
  new_neighbor_states4->clear_configuration();
  new_subnet_states1->clear_configuration();
  new_subnet_states2->clear_configuration();

  // don't delete br-int and br-tun bridges and the docker instance so that parent can ping
}
