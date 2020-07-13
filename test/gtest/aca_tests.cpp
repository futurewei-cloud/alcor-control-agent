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
#include "aca_ovs_l2_programmer.h"
// #include "aca_ovs_l3_programmer.h"
#include "aca_comm_mgr.h"
#include "aca_net_config.h"
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

// Defines
#define ACALOGNAME "AlcorControlAgentTest"

static char EMPTY_STRING[] = "";
static char VALID_STRING[] = "VALID_STRING";
static char DEFAULT_MTU[] = "9000";

static string project_id = "99d9d709-8478-4b46-9f3f-000000000000";
static string vpc_id_1 = "1b08a5bc-b718-11ea-b3de-111111111111";
static string vpc_id_2 = "1b08a5bc-b718-11ea-b3de-222222222222";
static string subnet_id_1 = "27330ae4-b718-11ea-b3de-111111111111";
static string subnet_id_2 = "27330ae4-b718-11ea-b3de-222222222222";
static string port_id_1 = "01111111-b718-11ea-b3de-111111111111";
static string port_id_2 = "02222222-b718-11ea-b3de-111111111111";
static string port_id_3 = "03333333-b718-11ea-b3de-111111111111";
static string port_id_4 = "04444444-b718-11ea-b3de-111111111111";
static string port_name_1 = aca_get_port_name(port_id_1);
static string port_name_2 = aca_get_port_name(port_id_2);
static string port_name_3 = aca_get_port_name(port_id_3);
static string port_name_4 = aca_get_port_name(port_id_4);
static string vmac_address_1 = "fa:16:3e:d7:f2:6c";
static string vmac_address_2 = "fa:16:3e:d7:f2:6d";
static string vmac_address_3 = "fa:16:3e:d7:f2:6e";
static string vmac_address_4 = "fa:16:3e:d7:f2:6f";
static string vip_address_1 = "10.10.0.1";
static string vip_address_2 = "10.10.1.2";
static string vip_address_3 = "10.10.0.3";
static string vip_address_4 = "10.10.1.4";
static string remote_ip_1 = "172.17.0.2"; // for docker network
static string remote_ip_2 = "172.17.0.3"; // for docker network
static NetworkType vxlan_type = NetworkType::VXLAN;

// Global variables
std::atomic_ulong g_total_network_configuration_time(0);
std::atomic_ulong g_total_update_GS_time(0);
bool g_debug_mode = true;
bool g_demo_mode = false;

// TODO: setup bridge when br-int is up and br-tun is gone

// TODO: invalid IP

// TODO: invalid mac

// TODO: tunnel ID

// TODO: subnet info not available

// TODO: neighbor invalid IP

// TODO: neighbor invalid mac

static void aca_cleanup()
{
  ACA_LOG_DEBUG("g_total_network_configuration_time = %lu nanoseconds or %lu milliseconds\n",
                g_total_network_configuration_time.load(),
                g_total_network_configuration_time.load() / 1000000);

  ACA_LOG_DEBUG("g_total_update_GS_time = %lu nanoseconds or %lu milliseconds\n",
                g_total_update_GS_time.load(), g_total_update_GS_time.load() / 1000000);

  ACA_LOG_INFO("Program exiting, cleaning up...\n");

  // Optional:  Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  ACA_LOG_CLOSE();
}

static void aca_test_create_default_port_state(PortState *new_port_states)
{
  new_port_states->set_operation_type(OperationType::CREATE);

  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_format_version(1);
  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_message_type(MessageType::FULL);
  // PortConfiguration_builder->set_network_type(NetworkType::VXLAN); // should default to VXLAN
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
}

static void aca_test_create_default_subnet_state(SubnetState *new_subnet_states)
{
  new_subnet_states->set_operation_type(OperationType::INFO);

  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_format_version(1);
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_project_id(project_id);
  SubnetConiguration_builder->set_vpc_id(vpc_id_1);
  SubnetConiguration_builder->set_id(subnet_id_1);
  SubnetConiguration_builder->set_cidr("10.0.0.0/24");
  SubnetConiguration_builder->set_tunnel_id(20);
}

TEST(ovs_dataplane_test_cases, 2_ports_config_test_traffic)
{
  // ulong culminative_network_configuration_time = 0;
  ulong not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // confirm br-int and br-tun bridges are not there
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "br-exists br-int", not_care_culminative_time, overall_rc);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "br-exists br-tun", not_care_culminative_time, overall_rc);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // are the newly created bridges there?
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "br-exists br-int", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "br-exists br-tun", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // set demo mode
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  string prefix_len = "/24";

  // create two ports (using demo mode) and configure them
  overall_rc = ACA_OVS_L2_Programmer::get_instance().configure_port(
          vpc_id_1, port_name_1, vip_address_1 + prefix_len, 20, not_care_culminative_time);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = ACA_OVS_L2_Programmer::get_instance().configure_port(
          vpc_id_1, port_name_2, vip_address_2 + prefix_len, 20, not_care_culminative_time);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // restore demo mode
  g_demo_mode = previous_demo_mode;

  // are the newly created ports there?
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_1 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_2 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // test traffic between the two newly created ports
  string cmd_string = "ping -I " + vip_address_1 + " -c1 " + vip_address_2;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ping -I " + vip_address_2 + " -c1 " + vip_address_1;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  string outport_name = aca_get_outport_name(vxlan_type, remote_ip_1);

  // insert neighbor info
  overall_rc = ACA_OVS_L2_Programmer::get_instance().create_update_neighbor_port(
          vpc_id_1, vxlan_type, remote_ip_1, 20, not_care_culminative_time);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // check if the outport has been created on br-tun
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          " list-ports br-tun | grep " + outport_name, not_care_culminative_time, overall_rc);
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

  // confirm br-int and br-tun bridges are not there
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "br-exists br-int", not_care_culminative_time, overall_rc);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "br-exists br-tun", not_care_culminative_time, overall_rc);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
}

TEST(ovs_dataplane_test_cases, 1_port_CREATE)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  GoalState GoalState_builder;
  PortState *new_port_states = GoalState_builder.add_port_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  // fill in port state structs
  aca_test_create_default_port_state(new_port_states);

  // fill in subnet state structs
  aca_test_create_default_subnet_state(new_subnet_states);

  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // set demo mode
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  // create a new port in demo mode
  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // restore demo mode
  g_demo_mode = previous_demo_mode;

  // check to ensure the port is created and setup correctly
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_1 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // clean up

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_subnet_states->clear_configuration();

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

TEST(ovs_dataplane_test_cases, 1_port_NEIGHBOR_CREATE_UPDATE)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  GoalState GoalState_builder;
  PortState *new_port_states = GoalState_builder.add_port_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  new_port_states->set_operation_type(OperationType::NEIGHBOR_CREATE_UPDATE);

  // fill in port state structs
  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_format_version(2);
  PortConfiguration_builder->set_revision_number(2);
  PortConfiguration_builder->set_message_type(MessageType::DELTA);
  PortConfiguration_builder->set_network_type(vxlan_type);

  PortConfiguration_builder->set_project_id(project_id);
  PortConfiguration_builder->set_vpc_id(vpc_id_1);
  PortConfiguration_builder->set_mac_address(vmac_address_3);
  PortConfiguration_builder->set_admin_state_up(true);

  PortConfiguration_HostInfo *portConfig_HostInfoBuilder(new PortConfiguration_HostInfo);
  portConfig_HostInfoBuilder->set_ip_address(remote_ip_1);
  PortConfiguration_builder->set_allocated_host_info(portConfig_HostInfoBuilder);

  PortConfiguration_FixedIp *FixedIp_builder = PortConfiguration_builder->add_fixed_ips();
  FixedIp_builder->set_ip_address(vip_address_3);
  FixedIp_builder->set_subnet_id(subnet_id_1);

  // fill in subnet state structs
  aca_test_create_default_subnet_state(new_subnet_states);

  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // NEIGHBOR_CREATE_UPDATE
  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  string outport_name = aca_get_outport_name(vxlan_type, remote_ip_1);

  // check if the outport has been created on br-tun
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          " list-ports br-tun | grep " + outport_name, not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // clean up

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_subnet_states->clear_configuration();

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

TEST(ovs_dataplane_test_cases, 1_port_CREATE_plus_NEIGHBOR_CREATE_UPDATE)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  GoalState GoalState_builder;
  PortState *new_port_states = GoalState_builder.add_port_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  // fill in port state structs
  aca_test_create_default_port_state(new_port_states);

  // fill in subnet state structs
  aca_test_create_default_subnet_state(new_subnet_states);

  // add a new port state with NEIGHBOR_CREATE_UPDATE
  PortState *new_port_neighbor_states = GoalState_builder.add_port_states();

  new_port_neighbor_states->set_operation_type(OperationType::NEIGHBOR_CREATE_UPDATE);

  // fill in port state structs for NEIGHBOR_CREATE_UPDATE
  PortConfiguration *PortConfiguration_builder2 =
          new_port_neighbor_states->mutable_configuration();
  PortConfiguration_builder2->set_format_version(2);
  PortConfiguration_builder2->set_revision_number(2);
  PortConfiguration_builder2->set_message_type(MessageType::DELTA);
  PortConfiguration_builder2->set_network_type(vxlan_type);

  PortConfiguration_builder2->set_project_id(project_id);
  PortConfiguration_builder2->set_vpc_id(vpc_id_1);
  PortConfiguration_builder2->set_mac_address(vmac_address_3);
  PortConfiguration_builder2->set_admin_state_up(true);

  PortConfiguration_HostInfo *portConfig_HostInfoBuilder2(new PortConfiguration_HostInfo);
  portConfig_HostInfoBuilder2->set_ip_address(remote_ip_1);
  PortConfiguration_builder2->set_allocated_host_info(portConfig_HostInfoBuilder2);

  PortConfiguration_FixedIp *FixedIp_builder2 =
          PortConfiguration_builder2->add_fixed_ips();
  FixedIp_builder2->set_subnet_id(subnet_id_1);
  FixedIp_builder2->set_ip_address(vip_address_3);

  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

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

  string outport_name = aca_get_outport_name(vxlan_type, remote_ip_1);

  // check if the outport has been created on br-tun
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          " list-ports br-tun | grep " + outport_name, not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // clean up

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_port_neighbor_states->clear_configuration();
  new_subnet_states->clear_configuration();

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

TEST(ovs_dataplane_test_cases, 2_ports_CREATE_test_traffic)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  GoalState GoalState_builder;
  PortState *new_port_states = GoalState_builder.add_port_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  new_port_states->set_operation_type(OperationType::CREATE);

  // fill in port state structs
  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_format_version(1);
  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_message_type(MessageType::FULL);
  // PortConfiguration_builder->set_network_type(NetworkType::VXLAN); // should default to VXLAN
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
  aca_test_create_default_subnet_state(new_subnet_states);

  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // set demo mode
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  // create a new port 1 in demo mode
  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // check to ensure the port 1 is created and setup correctly
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_1 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // setup the configuration for port 2
  PortConfiguration_builder->set_id(port_id_2);
  PortConfiguration_builder->set_name(port_name_2);
  PortConfiguration_builder->set_mac_address(vmac_address_2);
  FixedIp_builder->set_ip_address(vip_address_2);

  // create a new port 2 in demo mode
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // restore demo mode
  g_demo_mode = previous_demo_mode;

  // check to ensure the port 2 is created and setup correctly
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_2 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // test traffic between the two newly created ports
  cmd_string = "ping -I " + vip_address_1 + " -c1 " + vip_address_2;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ping -I " + vip_address_2 + " -c1 " + vip_address_1;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // clean up

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_subnet_states->clear_configuration();

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

TEST(ovs_dataplane_test_cases, DISABLED_2_ports_CREATE_test_traffic_MASTER)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  GoalState GoalState_builder;
  PortState *new_port_states = GoalState_builder.add_port_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  new_port_states->set_operation_type(OperationType::CREATE);

  // fill in port state structs for port 1
  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_format_version(1);
  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_message_type(MessageType::FULL);
  // PortConfiguration_builder->set_network_type(NetworkType::VXLAN); // should default to VXLAN
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
  SubnetConiguration_builder->set_format_version(1);
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_project_id(project_id);
  SubnetConiguration_builder->set_vpc_id(vpc_id_1);
  SubnetConiguration_builder->set_id(subnet_id_1);
  SubnetConiguration_builder->set_cidr("10.0.0.0/24");
  SubnetConiguration_builder->set_tunnel_id(20);

  // add a new port state with NEIGHBOR_CREATE_UPDATE
  PortState *new_port_neighbor_states = GoalState_builder.add_port_states();

  new_port_neighbor_states->set_operation_type(OperationType::NEIGHBOR_CREATE_UPDATE);

  // fill in port state structs for NEIGHBOR_CREATE_UPDATE for port 3
  PortConfiguration *PortConfiguration_builder2 =
          new_port_neighbor_states->mutable_configuration();
  PortConfiguration_builder2->set_format_version(2);
  PortConfiguration_builder2->set_revision_number(2);
  PortConfiguration_builder2->set_message_type(MessageType::DELTA);
  PortConfiguration_builder2->set_network_type(vxlan_type);

  PortConfiguration_builder2->set_project_id(project_id);
  PortConfiguration_builder2->set_vpc_id(vpc_id_1);
  PortConfiguration_builder2->set_mac_address(vmac_address_3);
  PortConfiguration_builder2->set_admin_state_up(true);

  PortConfiguration_HostInfo *portConfig_HostInfoBuilder2(new PortConfiguration_HostInfo);
  portConfig_HostInfoBuilder2->set_ip_address(remote_ip_2);
  PortConfiguration_builder2->set_allocated_host_info(portConfig_HostInfoBuilder2);

  PortConfiguration_FixedIp *FixedIp_builder2 =
          PortConfiguration_builder2->add_fixed_ips();
  FixedIp_builder2->set_subnet_id(subnet_id_1);
  FixedIp_builder2->set_ip_address(vip_address_3);

  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // set demo mode
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  // create a new port 1 + port 3 neighbor in demo mode
  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // check to ensure the port 1 is created and setup correctly
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_1 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
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
  SubnetConiguration_builder->set_cidr("10.0.1.0/24");
  SubnetConiguration_builder->set_tunnel_id(30);

  // fill in port state structs for NEIGHBOR_CREATE_UPDATE for port 4
  PortConfiguration_builder2->set_vpc_id(vpc_id_2);
  PortConfiguration_builder2->set_mac_address(vmac_address_4);
  FixedIp_builder2->set_subnet_id(subnet_id_2);
  FixedIp_builder2->set_ip_address(vip_address_4);

  // create a new port 2 + port 4 neighbor in demo mode
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // restore demo mode
  g_demo_mode = previous_demo_mode;

  // check to ensure the port 2 is created and setup correctly
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_2 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // test traffic between the two newly created ports
  cmd_string = "ping -I " + vip_address_1 + " -c1 " + vip_address_2;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ping -I " + vip_address_2 + " -c1 " + vip_address_1;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // test valid traffic from master to slave
  cmd_string = "ping -I " + vip_address_1 + " -c1 " + vip_address_3;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ping -I " + vip_address_2 + " -c1 " + vip_address_4;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // test invalid traffic from master to slave with different subnets
  cmd_string = "ping -I " + vip_address_1 + " -c1 " + vip_address_4;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ping -I " + vip_address_2 + " -c1 " + vip_address_3;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // clean up

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_port_neighbor_states->clear_configuration();
  new_subnet_states->clear_configuration();

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

TEST(ovs_dataplane_test_cases, DISABLED_2_ports_CREATE_test_traffic_SLAVE)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  GoalState GoalState_builder;
  PortState *new_port_states = GoalState_builder.add_port_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  new_port_states->set_operation_type(OperationType::CREATE);

  // fill in port state structs for port 3
  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_format_version(1);
  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_message_type(MessageType::FULL);
  // PortConfiguration_builder->set_network_type(NetworkType::VXLAN); // should default to VXLAN
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
  SubnetConiguration_builder->set_format_version(1);
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_project_id(project_id);
  SubnetConiguration_builder->set_vpc_id(vpc_id_1);
  SubnetConiguration_builder->set_id(subnet_id_1);
  SubnetConiguration_builder->set_cidr("10.0.0.0/24");
  SubnetConiguration_builder->set_tunnel_id(20);

  // add a new port state with NEIGHBOR_CREATE_UPDATE
  PortState *new_port_neighbor_states = GoalState_builder.add_port_states();

  new_port_neighbor_states->set_operation_type(OperationType::NEIGHBOR_CREATE_UPDATE);

  // fill in port state structs for NEIGHBOR_CREATE_UPDATE for port 1
  PortConfiguration *PortConfiguration_builder2 =
          new_port_neighbor_states->mutable_configuration();
  PortConfiguration_builder2->set_format_version(2);
  PortConfiguration_builder2->set_revision_number(2);
  PortConfiguration_builder2->set_message_type(MessageType::DELTA);
  PortConfiguration_builder2->set_network_type(vxlan_type);

  PortConfiguration_builder2->set_project_id(project_id);
  PortConfiguration_builder2->set_vpc_id(vpc_id_1);
  PortConfiguration_builder2->set_mac_address(vmac_address_1);
  PortConfiguration_builder2->set_admin_state_up(true);

  PortConfiguration_HostInfo *portConfig_HostInfoBuilder2(new PortConfiguration_HostInfo);
  portConfig_HostInfoBuilder2->set_ip_address(remote_ip_1);
  PortConfiguration_builder2->set_allocated_host_info(portConfig_HostInfoBuilder2);

  PortConfiguration_FixedIp *FixedIp_builder2 =
          PortConfiguration_builder2->add_fixed_ips();
  FixedIp_builder2->set_subnet_id(subnet_id_1);
  FixedIp_builder2->set_ip_address(vip_address_1);

  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // set demo mode
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  // create a new port 3 + port 1 neighbor in demo mode
  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // check to ensure the port 3 is created and setup correctly
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_3 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
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
  SubnetConiguration_builder->set_cidr("10.0.1.0/24");
  SubnetConiguration_builder->set_tunnel_id(30);

  // fill in port state structs for NEIGHBOR_CREATE_UPDATE for port 2
  PortConfiguration_builder2->set_vpc_id(vpc_id_2);
  PortConfiguration_builder2->set_mac_address(vmac_address_2);
  FixedIp_builder2->set_subnet_id(subnet_id_2);
  FixedIp_builder2->set_ip_address(vip_address_2);

  // create a new port 4 + port 2 neighbor in demo mode
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // restore demo mode
  g_demo_mode = previous_demo_mode;

  // check to ensure the port 4 is created and setup correctly
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_4 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // test traffic between the two newly created ports
  cmd_string = "ping -I " + vip_address_3 + " -c1 " + vip_address_4;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ping -I " + vip_address_4 + " -c1 " + vip_address_3;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // clean up

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_port_neighbor_states->clear_configuration();
  new_subnet_states->clear_configuration();

  // not deleting br-int and br-tun bridges so that master can ping the two new ports
}

TEST(net_config_test_cases, create_namespace_invalid)
{
  ulong culminative_network_configuration_time = 0;
  int rc;

  rc = Aca_Net_Config::get_instance().create_namespace(
          EMPTY_STRING, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, create_namespace_valid)
{
  string test_ns = "test_ns";
  string cmd_string;
  ulong culminative_network_configuration_time = 0;
  int rc;

  rc = Aca_Net_Config::get_instance().create_namespace(
          test_ns, culminative_network_configuration_time);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  // is the newly create namespace there?
  cmd_string = IP_NETNS_PREFIX + "list " + test_ns + " | grep " + test_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created ns
  cmd_string = IP_NETNS_PREFIX + "delete " + test_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the newly create namespace should be gone now
  cmd_string = IP_NETNS_PREFIX + "list " + test_ns + " | grep " + test_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, create_veth_pair_invalid)
{
  ulong culminative_network_configuration_time = 0;
  int rc;

  rc = Aca_Net_Config::get_instance().create_veth_pair(
          EMPTY_STRING, VALID_STRING, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().create_veth_pair(
          VALID_STRING, EMPTY_STRING, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, create_veth_pair_valid)
{
  string veth = "vethtest";
  string peer = "peertest";
  string cmd_string;
  ulong culminative_network_configuration_time = 0;
  int rc;

  // create the veth pair
  rc = Aca_Net_Config::get_instance().create_veth_pair(
          veth, peer, culminative_network_configuration_time);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  // is the newly created veth pair there?
  cmd_string = "ip link list " + veth + " | grep " + veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  cmd_string = "ip link list " + peer + " | grep " + peer;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created veth pair
  rc = Aca_Net_Config::get_instance().delete_veth_pair(
          peer, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the newly created veth pair should be gone now
  cmd_string = "ip link list " + veth + " | grep " + veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(rc, EXIT_SUCCESS);

  cmd_string = "ip link list " + peer + " | grep " + peer;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, setup_peer_device_invalid)
{
  ulong culminative_network_configuration_time = 0;
  int rc;

  rc = Aca_Net_Config::get_instance().setup_peer_device(
          EMPTY_STRING, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, setup_peer_device_valid)
{
  string veth = "vethtest";
  string peer = "peertest";
  string cmd_string;
  ulong culminative_network_configuration_time = 0;
  int rc;

  // create the veth pair
  rc = Aca_Net_Config::get_instance().create_veth_pair(
          veth, peer, culminative_network_configuration_time);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().setup_peer_device(
          peer, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // was the MTU applied successfully?
  cmd_string = "ip link list " + peer + " | grep " + peer + " | grep " + DEFAULT_MTU;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created veth pair
  rc = Aca_Net_Config::get_instance().delete_veth_pair(
          peer, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, move_to_namespace_invalid)
{
  ulong culminative_network_configuration_time = 0;
  int rc;

  rc = Aca_Net_Config::get_instance().move_to_namespace(
          EMPTY_STRING, VALID_STRING, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().move_to_namespace(
          VALID_STRING, EMPTY_STRING, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, move_to_namespace_valid)
{
  string veth = "vethtest";
  string peer = "peertest";
  string test_ns = "test_ns";
  string cmd_string;
  ulong culminative_network_configuration_time = 0;
  int rc;

  rc = Aca_Net_Config::get_instance().create_namespace(
          test_ns, culminative_network_configuration_time);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().create_veth_pair(
          veth, peer, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the veth should not be in the new namespace yet
  cmd_string = IP_NETNS_PREFIX + "exec " + test_ns + " ip link list | grep " + veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(rc, EXIT_SUCCESS);

  // move the veth into the new namespace
  rc = Aca_Net_Config::get_instance().move_to_namespace(
          veth, test_ns, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the veth should be in the new namespace now
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created veth pair
  rc = Aca_Net_Config::get_instance().delete_veth_pair(
          peer, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created ns
  cmd_string = IP_NETNS_PREFIX + "delete " + test_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, setup_veth_device_invalid)
{
  ulong culminative_network_configuration_time = 0;
  int rc;

  veth_config new_veth_config;
  new_veth_config.veth_name = VALID_STRING;
  new_veth_config.ip = VALID_STRING;
  new_veth_config.prefix_len = VALID_STRING;
  new_veth_config.mac = VALID_STRING;
  new_veth_config.gateway_ip = VALID_STRING;

  rc = Aca_Net_Config::get_instance().setup_veth_device(
          EMPTY_STRING, new_veth_config, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);

  new_veth_config.veth_name = EMPTY_STRING;
  rc = Aca_Net_Config::get_instance().setup_veth_device(
          EMPTY_STRING, new_veth_config, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);

  new_veth_config.veth_name = VALID_STRING;
  new_veth_config.ip = EMPTY_STRING;
  rc = Aca_Net_Config::get_instance().setup_veth_device(
          EMPTY_STRING, new_veth_config, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);

  new_veth_config.ip = VALID_STRING;
  new_veth_config.prefix_len = EMPTY_STRING;
  rc = Aca_Net_Config::get_instance().setup_veth_device(
          EMPTY_STRING, new_veth_config, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);

  new_veth_config.prefix_len = VALID_STRING;
  new_veth_config.mac = EMPTY_STRING;
  rc = Aca_Net_Config::get_instance().setup_veth_device(
          EMPTY_STRING, new_veth_config, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);

  new_veth_config.mac = VALID_STRING;
  new_veth_config.gateway_ip = EMPTY_STRING;
  rc = Aca_Net_Config::get_instance().setup_veth_device(
          EMPTY_STRING, new_veth_config, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, setup_veth_device_valid)
{
  string veth = "vethtest";
  string peer = "peertest";
  string test_ns = "test_ns";
  string ip = "10.0.0.2";
  string prefix_len = "16";
  string mac = "aa:bb:cc:dd:ee:ff";
  string gateway = "10.0.0.1";
  string cmd_string;
  ulong culminative_network_configuration_time = 0;
  int rc;

  rc = Aca_Net_Config::get_instance().create_namespace(
          test_ns, culminative_network_configuration_time);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().create_veth_pair(
          veth, peer, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the veth should not be in the new namespace yet
  cmd_string = IP_NETNS_PREFIX + "exec " + test_ns + " ip link list | grep " + veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(rc, EXIT_SUCCESS);

  // move the veth into the new namespace
  rc = Aca_Net_Config::get_instance().move_to_namespace(
          veth, test_ns, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the veth should be in the new namespace now
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  veth_config new_veth_config;
  new_veth_config.veth_name = veth;
  new_veth_config.ip = ip;
  new_veth_config.prefix_len = prefix_len;
  new_veth_config.mac = mac;
  new_veth_config.gateway_ip = gateway;
  rc = Aca_Net_Config::get_instance().setup_veth_device(
          test_ns, new_veth_config, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  g_demo_mode = previous_demo_mode;

  // was the ip set correctly?
  cmd_string = IP_NETNS_PREFIX + "exec " + test_ns + " ifconfig " + veth + " | grep " + ip;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // was the default gw set correctly?
  cmd_string = IP_NETNS_PREFIX + "exec " + test_ns + " ip route" + " | grep " + gateway;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // was the mac set correctly?
  cmd_string = IP_NETNS_PREFIX + "exec " + test_ns + " ifconfig " + veth + " | grep " + mac;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created veth pair
  rc = Aca_Net_Config::get_instance().delete_veth_pair(
          peer, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created ns
  cmd_string = IP_NETNS_PREFIX + "delete " + test_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, rename_veth_device_invalid)
{
  ulong culminative_network_configuration_time = 0;
  int rc;

  rc = Aca_Net_Config::get_instance().rename_veth_device(
          EMPTY_STRING, VALID_STRING, VALID_STRING, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().rename_veth_device(
          VALID_STRING, EMPTY_STRING, VALID_STRING, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().rename_veth_device(
          VALID_STRING, VALID_STRING, EMPTY_STRING, culminative_network_configuration_time);
  ASSERT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, rename_veth_device_valid)
{
  string old_veth = "vethtest";
  string peer = "peertest";
  string test_ns = "test_ns";
  string new_veth = "vethnew";
  string cmd_string;
  ulong culminative_network_configuration_time = 0;
  int rc;

  rc = Aca_Net_Config::get_instance().create_namespace(
          test_ns, culminative_network_configuration_time);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().create_veth_pair(
          old_veth, peer, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the veth should not be in the new namespace yet
  cmd_string = IP_NETNS_PREFIX + "exec " + test_ns + " ip link list | grep " + old_veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(rc, EXIT_SUCCESS);

  // move the veth into the new namespace
  rc = Aca_Net_Config::get_instance().move_to_namespace(
          old_veth, test_ns, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the veth should be in the new namespace now
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // rename the veth
  rc = Aca_Net_Config::get_instance().rename_veth_device(
          test_ns, old_veth, new_veth, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the new veth should be there
  cmd_string = IP_NETNS_PREFIX + "exec " + test_ns + " ip link list | grep " + new_veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the old veth should not be there
  cmd_string = IP_NETNS_PREFIX + "exec " + test_ns + " ip link list | grep " + old_veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(rc, EXIT_SUCCESS);

  // delete the newly created veth pair
  rc = Aca_Net_Config::get_instance().delete_veth_pair(
          peer, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created ns
  cmd_string = IP_NETNS_PREFIX + "delete " + test_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
  int option;

  ACA_LOG_INIT(ACALOGNAME);

  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  testing::InitGoogleTest(&argc, argv);

  while ((option = getopt(argc, argv, "m:s:")) != -1) {
    switch (option) {
    case 'm':
      remote_ip_1 = optarg;
      break;
    case 's':
      remote_ip_2 = optarg;
      break;
    default: /* the '?' case when the option is not recognized */
      fprintf(stderr,
              "Usage: %s\n"
              "\t\t[-m master machine IP]\n"
              "\t\t[-s slave machine IP]\n",
              argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  int rc = RUN_ALL_TESTS();

  aca_cleanup();

  return rc;
}
