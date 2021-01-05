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
#include "aca_ovs_control.h"
#include "aca_vlan_manager.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_ovs_l3_programmer.h"
#include "aca_comm_mgr.h"
#include "gtest/gtest.h"
#include "goalstate.pb.h"
#include <unistd.h> /* for getopt */
#include <iostream>
#include <string>

using namespace std;
using namespace alcor::schema;
using namespace aca_comm_manager;
using namespace aca_net_config;
using namespace aca_ovs_control;
using namespace aca_vlan_manager;
using namespace aca_ovs_l2_programmer;
using namespace aca_ovs_l3_programmer;

string project_id = "99d9d709-8478-4b46-9f3f-000000000000";
string vpc_id_1 = "1b08a5bc-b718-11ea-b3de-111111111111";
string vpc_id_2 = "1b08a5bc-b718-11ea-b3de-222222222222";
string subnet_id_1 = "27330ae4-b718-11ea-b3de-111111111111";
string subnet_id_2 = "27330ae4-b718-11ea-b3de-222222222222";
string port_id_1 = "01111111-b718-11ea-b3de-111111111111";
string port_id_2 = "02222222-b718-11ea-b3de-111111111111";
string port_id_3 = "03333333-b718-11ea-b3de-111111111111";
string port_id_4 = "04444444-b718-11ea-b3de-111111111111";
string port_name_1 = aca_get_port_name(port_id_1);
string port_name_2 = aca_get_port_name(port_id_2);
string port_name_3 = aca_get_port_name(port_id_3);
string port_name_4 = aca_get_port_name(port_id_4);
string vmac_address_1 = "fa:16:3e:d7:f2:6c";
string vmac_address_2 = "fa:16:3e:d7:f2:6d";
string vmac_address_3 = "fa:16:3e:d7:f2:6e";
string vmac_address_4 = "fa:16:3e:d7:f2:6f";
string node_mac_address_1 = "fa:17:3e:d7:f2:6c";
string node_mac_address_2 = "fa:17:3e:d7:f2:6d";
string node_mac_address_3 = "fa:17:3e:d7:f2:6e";
string node_mac_address_4 = "fa:17:3e:d7:f2:6f";
string vip_address_1 = "10.10.0.101";
string vip_address_2 = "10.10.1.102";
string vip_address_3 = "10.10.0.103";
string vip_address_4 = "10.10.1.104";
NetworkType vxlan_type = NetworkType::VXLAN;
string subnet1_gw_ip = "10.10.0.1";
string subnet2_gw_ip = "10.10.1.1";
string subnet1_gw_mac = "fa:16:3e:d7:f2:11";
string subnet2_gw_mac = "fa:16:3e:d7:f2:21";

extern bool g_demo_mode;
// total time for execute_system_command in microseconds
extern std::atomic_ulong g_initialize_execute_system_time;
// total time for execute_ovsdb_command in microseconds
extern std::atomic_ulong g_initialize_execute_ovsdb_time;
// total time for execute_openflow_command in microseconds
extern std::atomic_ulong g_initialize_execute_openflow_time;
// total time for execute_system_command in microseconds
extern std::atomic_ulong g_total_execute_system_time;
// total time for execute_ovsdb_command in microseconds
extern std::atomic_ulong g_total_execute_ovsdb_time;
// total time for execute_openflow_command in microseconds
extern std::atomic_ulong g_total_execute_openflow_time;
// total time for goal state update in microseconds
extern std::atomic_ulong g_total_update_GS_time;

void aca_test_reset_environment()
{
  ulong not_care_culminative_time = 0;
  int overall_rc = EXIT_SUCCESS;

  ACA_Vlan_Manager::get_instance().clear_all_data();

  ACA_OVS_L3_Programmer::get_instance().clear_all_data();

  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // test init/reset environment completed, store the time spend
  g_initialize_execute_system_time = g_total_execute_system_time.load();
  g_initialize_execute_ovsdb_time = g_total_execute_ovsdb_time.load();
  g_initialize_execute_openflow_time = g_total_execute_openflow_time.load();

  // reset the total time counters
  g_total_execute_system_time = 0;
  g_total_execute_ovsdb_time = 0;
  g_total_execute_openflow_time = 0;
}

void aca_test_create_default_port_state(PortState *new_port_states)
{
  new_port_states->set_operation_type(OperationType::CREATE);

  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_update_type(UpdateType::FULL);
  PortConfiguration_builder->set_id(port_id_1);

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
}

void aca_test_create_default_subnet_state(SubnetState *new_subnet_states)
{
  new_subnet_states->set_operation_type(OperationType::INFO);

  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_vpc_id(vpc_id_1);
  SubnetConiguration_builder->set_id(subnet_id_1);
  SubnetConiguration_builder->set_cidr("10.0.0.0/24");
  SubnetConiguration_builder->set_tunnel_id(20);

  auto *subnetConfig_GatewayBuilder(new SubnetConfiguration_Gateway);
  subnetConfig_GatewayBuilder->set_ip_address(subnet1_gw_ip);
  subnetConfig_GatewayBuilder->set_mac_address(subnet1_gw_mac);
  SubnetConiguration_builder->set_allocated_gateway(subnetConfig_GatewayBuilder);
}

void aca_test_create_default_router_goal_state(GoalState *goalState_builder)
{
  RouterState *new_router_states = goalState_builder->add_router_states();
  SubnetState *new_subnet_states1 = goalState_builder->add_subnet_states();
  SubnetState *new_subnet_states2 = goalState_builder->add_subnet_states();

  new_router_states->set_operation_type(OperationType::CREATE);

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
  SubnetConiguration_builder2->set_vpc_id(vpc_id_2);
  SubnetConiguration_builder2->set_id(subnet_id_2);
  SubnetConiguration_builder2->set_cidr("10.10.1.0/24");
  SubnetConiguration_builder2->set_tunnel_id(30);

  auto *subnetConfig_GatewayBuilder2(new SubnetConfiguration_Gateway);
  subnetConfig_GatewayBuilder2->set_ip_address(subnet2_gw_ip);
  subnetConfig_GatewayBuilder2->set_mac_address(subnet2_gw_mac);
  SubnetConiguration_builder2->set_allocated_gateway(subnetConfig_GatewayBuilder2);
}

void aca_test_1_neighbor_CREATE_DELETE(NeighborType input_neighbor_type)
{
  string cmd_string;
  int overall_rc;

  aca_test_reset_environment();

  GoalState GoalState_builder;

  if (input_neighbor_type == NeighborType::L3) {
    aca_test_create_default_router_goal_state(&GoalState_builder);
  } else {
    // fill in subnet state structs for L2 path only
    // because the L3 path above already added it
    SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
    aca_test_create_default_subnet_state(new_subnet_states);
  }

  NeighborState *new_neighbor_states = GoalState_builder.add_neighbor_states();

  new_neighbor_states->set_operation_type(OperationType::CREATE);

  // fill in neighbor state structs
  NeighborConfiguration *NeighborConfiguration_builder =
          new_neighbor_states->mutable_configuration();
  NeighborConfiguration_builder->set_revision_number(1);

  NeighborConfiguration_builder->set_vpc_id(vpc_id_1);
  NeighborConfiguration_builder->set_id(port_id_3);
  NeighborConfiguration_builder->set_mac_address(vmac_address_3);
  NeighborConfiguration_builder->set_host_ip_address("172.17.111.222");

  NeighborConfiguration_FixedIp *FixedIp_builder =
          NeighborConfiguration_builder->add_fixed_ips();
  FixedIp_builder->set_neighbor_type(input_neighbor_type);
  FixedIp_builder->set_subnet_id(subnet_id_1);
  FixedIp_builder->set_ip_address(vip_address_3);

  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // check if the neighbor rules has been created on br-tun
  string neighbor_opt = "table=20,dl_dst:" + vmac_address_3;
  overall_rc = ACA_OVS_Control::get_instance().flow_exists(
          "br-tun", neighbor_opt.c_str());
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  string arp_opt = "table=51,arp,nw_dst=" + vip_address_3;
  overall_rc = ACA_OVS_Control::get_instance().flow_exists("br-tun", arp_opt.c_str());
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // delete neighbor info
  new_neighbor_states->set_operation_type(OperationType::DELETE);
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // check if the neighbor rules has been deleted on br-tun
  overall_rc = ACA_OVS_Control::get_instance().flow_exists(
          "br-tun", neighbor_opt.c_str());
  EXPECT_NE(overall_rc, EXIT_SUCCESS);

  overall_rc = ACA_OVS_Control::get_instance().flow_exists("br-tun", arp_opt.c_str());
  EXPECT_NE(overall_rc, EXIT_SUCCESS);

  // clean up

  // free the allocated configurations since we are done with it now
  new_neighbor_states->clear_configuration();
}

void aca_test_1_port_CREATE_plus_neighbor_CREATE(NeighborType input_neighbor_type)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  aca_test_reset_environment();

  GoalState GoalState_builder;

  if (input_neighbor_type == NeighborType::L3) {
    aca_test_create_default_router_goal_state(&GoalState_builder);
  } else {
    // fill in subnet state structs for L2 path only
    // because the L3 path above already added it
    SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
    aca_test_create_default_subnet_state(new_subnet_states);
  }

  PortState *new_port_states = GoalState_builder.add_port_states();

  // fill in port state structs
  aca_test_create_default_port_state(new_port_states);

  // add a new neighbor state with CREATE
  NeighborState *new_neighbor_states = GoalState_builder.add_neighbor_states();
  new_neighbor_states->set_operation_type(OperationType::CREATE);

  // fill in neighbor state structs
  NeighborConfiguration *NeighborConfiguration_builder =
          new_neighbor_states->mutable_configuration();
  NeighborConfiguration_builder->set_revision_number(1);

  NeighborConfiguration_builder->set_vpc_id(vpc_id_1);
  NeighborConfiguration_builder->set_id(port_id_3);
  NeighborConfiguration_builder->set_mac_address(vmac_address_3);
  NeighborConfiguration_builder->set_host_ip_address("172.17.111.222");

  NeighborConfiguration_FixedIp *FixedIp_builder =
          NeighborConfiguration_builder->add_fixed_ips();
  FixedIp_builder->set_neighbor_type(input_neighbor_type);
  FixedIp_builder->set_subnet_id(subnet_id_1);
  FixedIp_builder->set_ip_address(vip_address_3);

  // set demo mode
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  // configure the whole goal state in demo mode
  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // restore demo mode
  g_demo_mode = previous_demo_mode;

  // check to ensure the port 1 is created and setup correctly
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_1 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // check if the neighbor rules has been created on br-tun
  string neighbor_opt = "table=20,dl_dst:" + vmac_address_3;
  overall_rc = ACA_OVS_Control::get_instance().flow_exists(
          "br-tun", neighbor_opt.c_str());
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  string arp_opt = "table=51,arp,nw_dst=" + vip_address_3;
  overall_rc = ACA_OVS_Control::get_instance().flow_exists("br-tun", arp_opt.c_str());
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // clean up

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_neighbor_states->clear_configuration();
}

void aca_test_10_neighbor_CREATE(NeighborType input_neighbor_type)
{
  string port_name_postfix = "-2222-3333-4444-555555555555";
  string neighbor_id_postfix = "-baad-f00d-4444-555555555555";
  string ip_address_prefix = "10.0.0.";
  string remote_ip_address_prefix = "123.0.0.";
  int overall_rc;

  aca_test_reset_environment();

  GoalState GoalState_builder;
  NeighborState *new_neighbor_states;

  if (input_neighbor_type == NeighborType::L3) {
    aca_test_create_default_router_goal_state(&GoalState_builder);
  } else {
    // fill in subnet state structs for L2 path only
    // because the L3 path above already added it
    SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
    aca_test_create_default_subnet_state(new_subnet_states);
  }

  const int NEIGHBORS_TO_CREATE = 10;

  for (int i = 0; i < NEIGHBORS_TO_CREATE; i++) {
    string i_string = std::to_string(i);
    string neighbor_id = i_string + neighbor_id_postfix;
    string port_name = i_string + port_name_postfix;

    new_neighbor_states = GoalState_builder.add_neighbor_states();
    new_neighbor_states->set_operation_type(OperationType::CREATE);

    NeighborConfiguration *NeighborConfiguration_builder =
            new_neighbor_states->mutable_configuration();
    NeighborConfiguration_builder->set_revision_number(1);

    NeighborConfiguration_builder->set_vpc_id("1b08a5bc-b718-11ea-b3de-111122223333");
    NeighborConfiguration_builder->set_id(neighbor_id);
    NeighborConfiguration_builder->set_name(port_name);
    NeighborConfiguration_builder->set_mac_address(vmac_address_1);
    NeighborConfiguration_builder->set_host_ip_address(remote_ip_address_prefix + i_string);

    NeighborConfiguration_FixedIp *FixedIp_builder =
            NeighborConfiguration_builder->add_fixed_ips();
    FixedIp_builder->set_neighbor_type(input_neighbor_type);
    FixedIp_builder->set_subnet_id(subnet_id_1);
    FixedIp_builder->set_ip_address(ip_address_prefix + i_string);
  }

  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = false;

  GoalStateOperationReply gsOperationReply;
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationReply);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  g_demo_mode = previous_demo_mode;

  // calculate the average latency
  ulong total_neighbor_create_time = 0;

  for (int i = 0; i < NEIGHBORS_TO_CREATE; i++) {
    ACA_LOG_DEBUG("Neighbor State(%d) took: %u microseconds or %u milliseconds\n",
                  i, gsOperationReply.operation_statuses(i).state_elapse_time(),
                  us_to_ms(gsOperationReply.operation_statuses(i).state_elapse_time()));

    total_neighbor_create_time +=
            gsOperationReply.operation_statuses(i).state_elapse_time();
  }

  ulong average_neighbor_create_time = total_neighbor_create_time / NEIGHBORS_TO_CREATE;

  ACA_LOG_INFO("Average NeighborType: %d Create of %d took: %lu microseconds or %lu milliseconds\n",
               input_neighbor_type, NEIGHBORS_TO_CREATE, average_neighbor_create_time,
               us_to_ms(average_neighbor_create_time));

  ACA_LOG_INFO("[TEST METRICS] Elapsed time for message total operation took: %u microseconds or %u milliseconds\n",
               gsOperationReply.message_total_operation_time(),
               gsOperationReply.us_to_ms(message_total_operation_time()));
}

void aca_test_1_port_CREATE_plus_N_neighbors_CREATE(NeighborType input_neighbor_type,
                                                    uint neighbors_to_create)
{
  string port_name_postfix = "-2222-3333-4444-555555555555";
  string neighbor_id_postfix = "-baad-f00d-4444-555555555555";
  string ip_address_prefix = "10.";
  string mac_address_prefix = "6c:dd:ee:";
  string remote_ip_address_prefix = "123.";
  int overall_rc;

  // current algorithm only supports up to 1,000,000 neighbors
  if (neighbors_to_create > 1000000) {
    ACA_LOG_DEBUG("Number of Neighbors is too large: %u\n", neighbors_to_create);
    ASSERT_FALSE(true);
  }

  aca_test_reset_environment();

  GoalState GoalState_builder;
  NeighborState *new_neighbor_states;

  if (input_neighbor_type == NeighborType::L3) {
    aca_test_create_default_router_goal_state(&GoalState_builder);
  } else {
    // fill in subnet state structs for L2 path only
    // because the L3 path above already added it
    SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
    aca_test_create_default_subnet_state(new_subnet_states);
  }

  PortState *new_port_states = GoalState_builder.add_port_states();

  // fill in port state structs
  aca_test_create_default_port_state(new_port_states);

  for (uint i = 0; i < neighbors_to_create; i++) {
    string i_string = std::to_string(i);
    string neighbor_id = i_string + neighbor_id_postfix;
    string port_name = i_string + port_name_postfix;

    string ip_2nd_octet = std::to_string(i / 10000);
    string ip_3rd_octet = std::to_string(i % 10000 / 100);
    string ip_4th_octet = std::to_string(i % 100);

    string ip_postfix = ip_2nd_octet + "." + ip_3rd_octet + "." + ip_4th_octet;
    string virtual_ip = ip_address_prefix + ip_postfix;
    string mac_postfix = ip_2nd_octet + ":" + ip_3rd_octet + ":" + ip_4th_octet;
    string virtual_mac = mac_address_prefix + mac_postfix;
    string remote_ip = remote_ip_address_prefix + ip_postfix;

    new_neighbor_states = GoalState_builder.add_neighbor_states();
    new_neighbor_states->set_operation_type(OperationType::CREATE);

    NeighborConfiguration *NeighborConfiguration_builder =
            new_neighbor_states->mutable_configuration();
    NeighborConfiguration_builder->set_revision_number(1);

    NeighborConfiguration_builder->set_vpc_id("1b08a5bc-b718-11ea-b3de-111122223333");
    NeighborConfiguration_builder->set_id(neighbor_id);
    NeighborConfiguration_builder->set_name(port_name);
    NeighborConfiguration_builder->set_mac_address(virtual_mac);
    NeighborConfiguration_builder->set_host_ip_address(remote_ip);

    NeighborConfiguration_FixedIp *FixedIp_builder =
            NeighborConfiguration_builder->add_fixed_ips();
    FixedIp_builder->set_neighbor_type(input_neighbor_type);
    FixedIp_builder->set_subnet_id(subnet_id_1);
    FixedIp_builder->set_ip_address(virtual_ip);
  }

  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  GoalStateOperationReply gsOperationReply;
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationReply);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  g_demo_mode = previous_demo_mode;

  // calculate the average latency
  ulong total_neighbor_create_time = 0;

  // we have neighbors_to_create + 1 port state created
  for (uint i = 0; i < neighbors_to_create + 1; i++) {
    ACA_LOG_DEBUG("Number: %d Resource Type: %d with id: %s took: %u microseconds or %u milliseconds\n",
                  i, gsOperationReply.operation_statuses(i).resource_type(),
                  gsOperationReply.operation_statuses(i).resource_id().c_str(),
                  gsOperationReply.operation_statuses(i).state_elapse_time(),
                  us_to_ms(gsOperationReply.operation_statuses(i).state_elapse_time()));

    total_neighbor_create_time +=
            gsOperationReply.operation_statuses(i).state_elapse_time();
  }

  ulong average_neighbor_create_time = total_neighbor_create_time / neighbors_to_create;

  ACA_LOG_INFO("Average port + neighbor states with NeighborType: %d Create of %d took: %lu microseconds or %lu milliseconds\n",
               input_neighbor_type, neighbors_to_create, average_neighbor_create_time,
               us_to_ms(average_neighbor_create_time));

  ACA_LOG_INFO("[TEST METRICS] Elapsed time for message total operation took: %u microseconds or %u milliseconds\n",
               gsOperationReply.message_total_operation_time(),
               us_to_ms(gsOperationReply.message_total_operation_time()));
}