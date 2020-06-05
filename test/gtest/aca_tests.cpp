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
#include "aca_ovs_config.h"
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

// Defines
#define ACALOGNAME "AlcorControlAgentTest"
static char LOCALHOST[] = "localhost";
static char UDP_PROTOCOL[] = "udp";

static char EMPTY_STRING[] = "";
static char VALID_STRING[] = "VALID_STRING";
static char DEFAULT_MTU[] = "9000";

static string project_id = "99d9d709-8478-4b46-9f3f-000000000000";
static string vpc_id = "99d9d709-8478-4b46-9f3f-111111111111";
static string subnet_id = "99d9d709-8478-4b46-9f3f-222222222222";
static string port_name_1 = "tap-33333333";
static string port_name_2 = "tap-44444444";
static string vmac_address_1 = "fa:16:3e:d7:f2:6c";
static string vmac_address_2 = "fa:16:3e:d7:f2:6d";
static string vip_address_1 = "10.0.0.1";
static string vip_address_2 = "10.0.0.2";

// Global variables
string g_rpc_server = EMPTY_STRING;
string g_rpc_protocol = EMPTY_STRING;
std::atomic_ulong g_total_rpc_call_time(0);
std::atomic_ulong g_total_rpc_client_time(0);
std::atomic_ulong g_total_network_configuration_time(0);
std::atomic_ulong g_total_update_GS_time(0);
bool g_debug_mode = true;
bool g_demo_mode = false;
bool g_transitd_loaded = false;

using aca_net_config::Aca_Net_Config;
using aca_ovs_config::ACA_OVS_Config;

static void aca_cleanup()
{
  ACA_LOG_DEBUG("g_total_rpc_call_time = %lu nanoseconds or %lu milliseconds\n",
                g_total_rpc_call_time.load(), g_total_rpc_call_time.load() / 1000000);

  ACA_LOG_DEBUG("g_total_rpc_client_time = %lu nanoseconds or %lu milliseconds\n",
                g_total_rpc_client_time.load(), g_total_rpc_client_time.load() / 1000000);

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

TEST(ovs_config_test_cases, 2_ports_create_test_traffic)
{
  // ulong culminative_network_configuration_time = 0;
  ulong not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  // delete br-int and br-tun bridges
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // confirm br-int and br-tun bridges are not there
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "br-exists br-int", not_care_culminative_time, overall_rc);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "br-exists br-tun", not_care_culminative_time, overall_rc);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_Config::get_instance().setup_bridges();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // are the newly created bridges there?
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "br-exists br-int", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "br-exists br-tun", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // set demo mode
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  string prefix_len = "/24";

  // create two ports (using demo mode) and configure them
  overall_rc = ACA_OVS_Config::get_instance().port_configure(
          vpc_id, port_name_1, vip_address_1 + prefix_len, 20, not_care_culminative_time);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  overall_rc = ACA_OVS_Config::get_instance().port_configure(
          vpc_id, port_name_2, vip_address_2 + prefix_len, 20, not_care_culminative_time);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // restore demo mode
  g_demo_mode = previous_demo_mode;

  // are the newly created ports there?
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_1 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_Config::get_instance().execute_ovsdb_command(
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

  NetworkType vxlan_type = NetworkType::VXLAN;
  string remote_ip = "10.0.0.2";
  string outport_name = aca_get_outport_name(vxlan_type, remote_ip);

  // insert neighbor info
  overall_rc = ACA_OVS_Config::get_instance().port_neighbor_create_update(
          vpc_id, vxlan_type, remote_ip, 20, not_care_culminative_time);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // check if the outport has been created on br-tun
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          " list-ports br-tun | grep " + outport_name, not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // delete br-int and br-tun bridges
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // confirm br-int and br-tun bridges are not there
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "br-exists br-int", not_care_culminative_time, overall_rc);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "br-exists br-tun", not_care_culminative_time, overall_rc);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
}

static void aca_test_create_default_port_state(PortState *new_port_states)
{
  new_port_states->set_operation_type(OperationType::CREATE);

  // this will allocate new PortConfiguration, will need to free it later
  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_format_version(1);
  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_message_type(MessageType::FULL);
  // PortConfiguration_builder->set_network_type(NetworkType::VXLAN); // should default to VXLAN

  PortConfiguration_builder->set_project_id(project_id);
  PortConfiguration_builder->set_vpc_id(vpc_id);
  PortConfiguration_builder->set_name(port_name_1);
  PortConfiguration_builder->set_mac_address(vmac_address_1);
  PortConfiguration_builder->set_admin_state_up(true);

  // this will allocate new PortConfiguration_FixedIp may need to free later
  PortConfiguration_FixedIp *FixedIp_builder = PortConfiguration_builder->add_fixed_ips();
  FixedIp_builder->set_subnet_id(subnet_id);
  FixedIp_builder->set_ip_address(vip_address_1);

  // this will allocate new PortConfiguration_SecurityGroupId may need to free later
  PortConfiguration_SecurityGroupId *SecurityGroup_builder =
          PortConfiguration_builder->add_security_group_ids();
  SecurityGroup_builder->set_id("1");
}

static void aca_test_create_default_subnet_state(SubnetState *new_subnet_states)
{
  new_subnet_states->set_operation_type(OperationType::INFO);

  // this will allocate new SubnetConfiguration, will need to free it later
  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_format_version(1);
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_project_id(project_id);
  SubnetConiguration_builder->set_vpc_id(vpc_id);
  SubnetConiguration_builder->set_id(subnet_id);
  SubnetConiguration_builder->set_cidr("10.0.0.0/24");
  SubnetConiguration_builder->set_tunnel_id(20);
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
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_Config::get_instance().execute_ovsdb_command(
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
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_1 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // clean up

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_subnet_states->clear_configuration();

  // delete br-int and br-tun bridges
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
}

// TEST for neighbor info only
// TEST for new port 2 + neighbor info for port 1

TEST(ovs_dataplane_test_cases, 2_ports_create_test_traffic)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  GoalState GoalState_builder;
  PortState *new_port_states = GoalState_builder.add_port_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  // fill in port state structs

  // this will allocate new PortConfiguration, will need to free it later
  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_format_version(1);
  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_message_type(MessageType::FULL);
  // PortConfiguration_builder->set_network_type(NetworkType::VXLAN); // should default to VXLAN

  PortConfiguration_builder->set_project_id(project_id);
  PortConfiguration_builder->set_vpc_id(vpc_id);
  PortConfiguration_builder->set_name(port_name_1);
  PortConfiguration_builder->set_mac_address(vmac_address_1);
  PortConfiguration_builder->set_admin_state_up(true);

  // this will allocate new PortConfiguration_FixedIp may need to free later
  PortConfiguration_FixedIp *FixedIp_builder = PortConfiguration_builder->add_fixed_ips();
  FixedIp_builder->set_subnet_id(subnet_id);
  FixedIp_builder->set_ip_address(vip_address_1);

  // this will allocate new PortConfiguration_SecurityGroupId may need to free later
  PortConfiguration_SecurityGroupId *SecurityGroup_builder =
          PortConfiguration_builder->add_security_group_ids();
  SecurityGroup_builder->set_id("1");

  // fill in subnet state structs
  aca_test_create_default_subnet_state(new_subnet_states);

  // delete br-int and br-tun bridges
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_Config::get_instance().setup_bridges();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // set demo mode
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  // create a new port 1 in demo mode
  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // check to ensure the port 1 is created and setup correctly
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "get Interface " + port_name_1 + " ofport", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // setup the configuration for port 2
  PortConfiguration_builder->set_name(port_name_2);
  PortConfiguration_builder->set_mac_address(vmac_address_2);
  FixedIp_builder->set_ip_address(vip_address_2);

  // create a new port 2 in demo mode
  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // restore demo mode
  g_demo_mode = previous_demo_mode;

  // check to ensure the port 1 is created and setup correctly
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
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
  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  ACA_OVS_Config::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
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

#if 0 // disable the mizar test cases
TEST(mizar_config_test_cases, subnet_CREATE_UPDATE_ROUTER)
{
  string vpc_id = "99d9d709-8478-4b46-9f3f-2206b1023fd3";
  string gateway_ip = "10.0.0.1";
  string gateway_mac = "fa:16:3e:d7:f2:00";
  int rc;

  GoalState GoalState_builder;
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  // fill in the subnet state structs
  new_subnet_states->set_operation_type(OperationType::CREATE_UPDATE_ROUTER);

  // this will allocate new SubnetConfiguration, will need to free it later
  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_format_version(1);
  SubnetConiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
  SubnetConiguration_builder->set_vpc_id(vpc_id);
  SubnetConiguration_builder->set_id("superSubnetID");
  SubnetConiguration_builder->set_name("SuperSubnet");
  SubnetConiguration_builder->set_cidr("10.0.0.0/16");
  SubnetConiguration_builder->set_tunnel_id(22222);
  SubnetConfiguration_Gateway *SubnetConiguration_Gateway_builder =
          SubnetConiguration_builder->mutable_gateway();
  SubnetConiguration_Gateway_builder->set_ip_address(gateway_ip);
  SubnetConiguration_Gateway_builder->set_mac_address(gateway_mac);
  // this will allocate new SubnetConfiguration_TransitSwitch, may need to free it later
  SubnetConfiguration_TransitSwitch *TransitSwitch_builder =
          SubnetConiguration_builder->add_transit_switches();
  TransitSwitch_builder->set_vpc_id(vpc_id);
  TransitSwitch_builder->set_subnet_id("superSubnet");
  TransitSwitch_builder->set_ip_address("172.0.0.1");
  TransitSwitch_builder->set_mac_address("cc:dd:ee:aa:bb:cc");

  GoalStateOperationReply gsOperationalReply;

  rc = Aca_Comm_Manager::get_instance().update_goal_state(GoalState_builder, gsOperationalReply);
  // rc can be error if transitd is not loaded
  if (g_transitd_loaded) {
    ASSERT_EQ(rc, EXIT_SUCCESS);
  }

  new_subnet_states->clear_configuration();
}

TEST(mizar_config_test_cases, subnet_CREATE_UPDATE_GATEWAY)
{
  string vpc_id = "99d9d709-8478-4b46-9f3f-2206b1023fd3";
  string gateway_ip = "10.0.0.1";
  string gateway_mac = "fa:16:3e:d7:f2:00";
  int rc;

  GoalState GoalState_builder;
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  // fill in the subnet state structs
  new_subnet_states->set_operation_type(OperationType::CREATE_UPDATE_GATEWAY);

  // this will allocate new SubnetConfiguration, will need to free it later
  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_format_version(1);
  SubnetConiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
  SubnetConiguration_builder->set_vpc_id(vpc_id);
  SubnetConiguration_builder->set_id("superSubnetID");
  SubnetConiguration_builder->set_name("SuperSubnet");
  SubnetConiguration_builder->set_cidr("10.0.0.0/16");
  SubnetConiguration_builder->set_tunnel_id(22222);
  SubnetConfiguration_Gateway *SubnetConiguration_Gateway_builder =
          SubnetConiguration_builder->mutable_gateway();
  SubnetConiguration_Gateway_builder->set_ip_address(gateway_ip);
  SubnetConiguration_Gateway_builder->set_mac_address(gateway_mac);
  // this will allocate new SubnetConfiguration_TransitSwitch, may need to free it later
  SubnetConfiguration_TransitSwitch *TransitSwitch_builder =
          SubnetConiguration_builder->add_transit_switches();
  TransitSwitch_builder->set_vpc_id(vpc_id);
  TransitSwitch_builder->set_subnet_id("superSubnet");
  TransitSwitch_builder->set_ip_address("172.0.0.1");
  TransitSwitch_builder->set_mac_address("cc:dd:ee:aa:bb:cc");

  GoalStateOperationReply gsOperationalReply;
  rc = Aca_Comm_Manager::get_instance().update_goal_state(GoalState_builder, gsOperationalReply);
  // rc can be error if transitd is not loaded
  if (g_transitd_loaded) {
    ASSERT_EQ(rc, EXIT_SUCCESS);
  }

  new_subnet_states->clear_configuration();
}

TEST(mizar_config_test_cases, subnet_CREATE_UPDATE_GATEWAY_100)
{
  string vpc_id = "99d9d709-8478-4b46-9f3f-2206b1023fd3";
  string gateway_ip_postfix = ".0.0.1";
  string gateway_mac_prefix = "fa:16:3e:d7:f2:";
  int rc;

  GoalState GoalState_builder;
  SubnetState *new_subnet_states;

  for (int i = 0; i < 100; i++) {
    string i_string = std::to_string(i);

    new_subnet_states = GoalState_builder.add_subnet_states();
    new_subnet_states->set_operation_type(OperationType::CREATE_UPDATE_GATEWAY);

    // this will allocate new SubnetConfiguration, will need to free it later
    SubnetConfiguration *SubnetConiguration_builder =
            new_subnet_states->mutable_configuration();
    SubnetConiguration_builder->set_format_version(1);
    SubnetConiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
    SubnetConiguration_builder->set_vpc_id(vpc_id);
    SubnetConiguration_builder->set_id("superSubnetID" + i_string);
    SubnetConiguration_builder->set_name("SuperSubnet");
    SubnetConiguration_builder->set_cidr(i_string + ".0.0.0/16");
    SubnetConiguration_builder->set_tunnel_id(22222);
    SubnetConfiguration_Gateway *SubnetConiguration_Gateway_builder =
            SubnetConiguration_builder->mutable_gateway();
    SubnetConiguration_Gateway_builder->set_ip_address(i_string + gateway_ip_postfix);
    SubnetConiguration_Gateway_builder->set_mac_address(gateway_mac_prefix + i_string);
    // this will allocate new SubnetConfiguration_TransitSwitch, may need to free it later
    SubnetConfiguration_TransitSwitch *TransitSwitch_builder =
            SubnetConiguration_builder->add_transit_switches();
    TransitSwitch_builder->set_vpc_id(vpc_id);
    TransitSwitch_builder->set_subnet_id("superSubnet");
    TransitSwitch_builder->set_ip_address("172.0.0.1");
    TransitSwitch_builder->set_mac_address("cc:dd:ee:aa:bb:cc");
  }

  GoalStateOperationReply gsOperationalReply;
  rc = Aca_Comm_Manager::get_instance().update_goal_state(GoalState_builder, gsOperationalReply);
  // rc can be error if transitd is not loaded
  if (g_transitd_loaded) {
    ASSERT_EQ(rc, EXIT_SUCCESS);
  }

  new_subnet_states->clear_configuration();
}

TEST(mizar_config_test_cases, port_CREATE_UPDATE_SWITCH_100)
{
  string port_name = "11111111-2222-3333-4444-555555555555";
  string vpc_id = "99d9d709-8478-4b46-9f3f-2206b1023fd3";
  string vpc_ns = "vpc-ns-" + vpc_id;
  string ip_address = "10.0.0.2";
  string mac_address = "fa:16:3e:d7:f2:6c";
  string gateway_ip = "10.0.0.1";
  string gateway_mac = "fa:16:3e:d7:f2:00";
  string cmd_string;
  int rc;

  string truncated_port_id = port_name.substr(0, 11);
  string temp_name_string = "temp" + truncated_port_id;
  string veth_name_string = "veth" + truncated_port_id;

  GoalState GoalState_builder;
  PortState *new_port_states;
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  for (int i = 0; i < 100; i++) {
    new_port_states = GoalState_builder.add_port_states();
    new_port_states->set_operation_type(OperationType::CREATE_UPDATE_SWITCH);

    // this will allocate new PortConfiguration, may need to free it later
    PortConfiguration *PortConfiguration_builder =
            new_port_states->mutable_configuration();
    PortConfiguration_builder->set_format_version(1);
    PortConfiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
    PortConfiguration_builder->set_id(port_name);
    PortConfiguration_builder->set_name("FriendlyPortName");
    PortConfiguration_builder->set_network_ns("");
    PortConfiguration_builder->set_mac_address(mac_address);
    PortConfiguration_builder->set_veth_name("veth0");

    PortConfiguration_HostInfo *portConfig_HostInfoBuilder(new PortConfiguration_HostInfo);
    portConfig_HostInfoBuilder->set_ip_address("172.0.0.2");
    portConfig_HostInfoBuilder->set_mac_address("aa-bb-cc-dd-ee-ff");
    PortConfiguration_builder->set_allocated_host_info(portConfig_HostInfoBuilder);

    // this will allocate new PortConfiguration_FixedIp may need to free later
    PortConfiguration_FixedIp *PortIp_builder =
            PortConfiguration_builder->add_fixed_ips();
    PortIp_builder->set_ip_address(ip_address);
    PortIp_builder->set_subnet_id("superSubnetID");
    // this will allocate new PortConfiguration_SecurityGroupId may need to free later
    PortConfiguration_SecurityGroupId *SecurityGroup_builder =
            PortConfiguration_builder->add_security_group_ids();
    SecurityGroup_builder->set_id("1");
    // this will allocate new PortConfiguration_AllowAddressPair may need to free later
    PortConfiguration_AllowAddressPair *AddressPair_builder =
            PortConfiguration_builder->add_allow_address_pairs();
    AddressPair_builder->set_ip_address("10.0.0.5");
    AddressPair_builder->set_mac_address("fa:16:3e:d7:f2:9f");
  }

  // fill in the subnet state structs
  new_subnet_states->set_operation_type(OperationType::INFO);

  // this will allocate new SubnetConfiguration, will need to free it later
  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_format_version(1);
  SubnetConiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
  SubnetConiguration_builder->set_vpc_id(vpc_id);
  SubnetConiguration_builder->set_id("superSubnetID");
  SubnetConiguration_builder->set_name("SuperSubnet");
  SubnetConiguration_builder->set_cidr("10.0.0.0/16");
  SubnetConiguration_builder->set_tunnel_id(22222);
  SubnetConfiguration_Gateway *SubnetConiguration_Gateway_builder =
          SubnetConiguration_builder->mutable_gateway();
  SubnetConiguration_Gateway_builder->set_ip_address(gateway_ip);
  SubnetConiguration_Gateway_builder->set_mac_address(gateway_mac);
  // this will allocate new SubnetConfiguration_TransitSwitch, may need to free it later
  SubnetConfiguration_TransitSwitch *TransitSwitch_builder =
          SubnetConiguration_builder->add_transit_switches();
  TransitSwitch_builder->set_vpc_id(vpc_id);
  TransitSwitch_builder->set_subnet_id("superSubnet");
  TransitSwitch_builder->set_ip_address("172.0.0.1");
  TransitSwitch_builder->set_mac_address("cc:dd:ee:aa:bb:cc");

  GoalStateOperationReply gsOperationalReply;
  rc = Aca_Comm_Manager::get_instance().update_goal_state(GoalState_builder, gsOperationalReply);
  // rc can be error if transitd is not loaded
  if (g_transitd_loaded) {
    EXPECT_EQ(rc, EXIT_SUCCESS);
  }

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_subnet_states->clear_configuration();
}

TEST(mizar_config_test_cases, port_CREATE_integrated)
{
  string port_name = "11111111-2222-3333-4444-555555555555";
  string vpc_id = "99d9d709-8478-4b46-9f3f-2206b1023fd3";
  string vpc_ns = "vpc-ns-" + vpc_id;
  string ip_address = "10.0.0.2";
  string mac_address = "fa:16:3e:d7:f2:6c";
  string gateway_ip = "10.0.0.1";
  string gateway_mac = "fa:16:3e:d7:f2:00";
  string veth_name = "veth0";
  string cmd_string;
  int rc;

  string truncated_port_id = port_name.substr(0, 11);
  string temp_name_string = "temp" + truncated_port_id;
  string peer_name_string = "peer" + truncated_port_id;

  GoalState GoalState_builder;
  PortState *new_port_states = GoalState_builder.add_port_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  // fill in port state structs
  new_port_states->set_operation_type(OperationType::CREATE);

  // this will allocate new PortConfiguration, will need to free it later
  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_format_version(1);
  PortConfiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
  PortConfiguration_builder->set_id(port_name);
  PortConfiguration_builder->set_name("FriendlyPortName");
  PortConfiguration_builder->set_network_ns("");
  PortConfiguration_builder->set_mac_address(mac_address);
  PortConfiguration_builder->set_veth_name(veth_name);

  PortConfiguration_HostInfo *portConfig_HostInfoBuilder(new PortConfiguration_HostInfo);
  portConfig_HostInfoBuilder->set_ip_address("172.0.0.2");
  portConfig_HostInfoBuilder->set_mac_address("aa-bb-cc-dd-ee-ff");
  PortConfiguration_builder->set_allocated_host_info(portConfig_HostInfoBuilder);

  // this will allocate new PortConfiguration_FixedIp may need to free later
  PortConfiguration_FixedIp *PortIp_builder = PortConfiguration_builder->add_fixed_ips();
  PortIp_builder->set_ip_address(ip_address);
  PortIp_builder->set_subnet_id("superSubnetID");
  // this will allocate new PortConfiguration_SecurityGroupId may need to free later
  PortConfiguration_SecurityGroupId *SecurityGroup_builder =
          PortConfiguration_builder->add_security_group_ids();
  SecurityGroup_builder->set_id("1");
  // this will allocate new PortConfiguration_AllowAddressPair may need to free later
  PortConfiguration_AllowAddressPair *AddressPair_builder =
          PortConfiguration_builder->add_allow_address_pairs();
  AddressPair_builder->set_ip_address("10.0.0.5");
  AddressPair_builder->set_mac_address("fa:16:3e:d7:f2:9f");

  // fill in the subnet state structs
  new_subnet_states->set_operation_type(OperationType::INFO);

  // this will allocate new SubnetConfiguration, will need to free it later
  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_format_version(1);
  SubnetConiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
  SubnetConiguration_builder->set_vpc_id(vpc_id);
  SubnetConiguration_builder->set_id("superSubnetID");
  SubnetConiguration_builder->set_name("SuperSubnet");
  SubnetConiguration_builder->set_cidr("10.0.0.0/16");
  SubnetConiguration_builder->set_tunnel_id(22222);
  SubnetConfiguration_Gateway *SubnetConiguration_Gateway_builder =
          SubnetConiguration_builder->mutable_gateway();
  SubnetConiguration_Gateway_builder->set_ip_address(gateway_ip);
  SubnetConiguration_Gateway_builder->set_mac_address(gateway_mac);
  // this will allocate new SubnetConfiguration_TransitSwitch, may need to free it later
  SubnetConfiguration_TransitSwitch *TransitSwitch_builder =
          SubnetConiguration_builder->add_transit_switches();
  TransitSwitch_builder->set_vpc_id(vpc_id);
  TransitSwitch_builder->set_subnet_id("superSubnet");
  TransitSwitch_builder->set_ip_address("172.0.0.1");
  TransitSwitch_builder->set_mac_address("cc:dd:ee:aa:bb:cc");

  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  GoalStateOperationReply gsOperationalReply;
  rc = Aca_Comm_Manager::get_instance().update_goal_state(GoalState_builder, gsOperationalReply);
  // rc can be error if transitd is not loaded
  if (g_transitd_loaded) {
    ASSERT_EQ(rc, EXIT_SUCCESS);
  }

  g_demo_mode = previous_demo_mode;

  // check to ensure the endpoint is created and setup correctly

  // the temp veth should be in the new namespace now
  cmd_string = IP_NETNS_PREFIX + "exec " + vpc_ns + " ip link list | grep " + temp_name_string;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // was the ip set correctly?
  cmd_string = IP_NETNS_PREFIX + "exec " + vpc_ns + " ifconfig " +
               temp_name_string + " | grep " + ip_address;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // was the default gw set correctly?
  cmd_string = IP_NETNS_PREFIX + "exec " + vpc_ns + " ip route" + " | grep " + gateway_ip;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // was the mac set correctly?
  cmd_string = IP_NETNS_PREFIX + "exec " + vpc_ns + " ifconfig " +
               temp_name_string + " | grep " + mac_address;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // finalize the endpoint, which will rename the veth to the final name
  PortState *final_port_states = GoalState_builder.mutable_port_states(0);
  final_port_states->set_operation_type(OperationType::FINALIZE);

  rc = Aca_Comm_Manager::get_instance().update_goal_state(GoalState_builder, gsOperationalReply);
  // rc can be error if transitd is not loaded
  if (g_transitd_loaded) {
    EXPECT_EQ(rc, EXIT_SUCCESS);
  }

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_subnet_states->clear_configuration();

  // check to ensure the end point has been renamed to the final name
  cmd_string = IP_NETNS_PREFIX + "exec " + vpc_ns + " ip link list | grep " + veth_name;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // clean up

  // TODO: should the test wait before cleaning things up?

  ulong culminative_network_configuration_time = 0;

  // delete the newly created veth pair
  rc = Aca_Net_Config::get_instance().delete_veth_pair(
          peer_name_string, culminative_network_configuration_time);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created ns
  cmd_string = IP_NETNS_PREFIX + "delete " + vpc_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);
}

TEST(mizar_config_test_cases, port_CREATE_10)
{
  string network_id = "superSubnetID";
  string port_name_postfix = "11111111-2222-3333-4444-555555555555";
  string vpc_id = "99d9d709-8478-4b46-9f3f-2206b1023fa";
  string vpc_ns = "vpc-ns-" + vpc_id;
  string ip_address_prefix = "10.0.0.";
  string mac_address_prefix = "fa:16:3e:d7:f2:6";
  string gateway_ip = "10.0.0.1";
  string gateway_mac = "fa:16:3e:d7:f2:00";
  string cmd_string;
  ulong culminative_network_configuration_time = 0;
  int rc;

  GoalState GoalState_builder;
  PortState *new_port_states;
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  const int PORTS_TO_CREATE = 10;

  for (int i = 0; i < PORTS_TO_CREATE; i++) {
    string i_string = std::to_string(i);
    string port_name = i_string + port_name_postfix;

    new_port_states = GoalState_builder.add_port_states();
    new_port_states->set_operation_type(OperationType::CREATE);

    // this will allocate new PortConfiguration, may need to free it later
    PortConfiguration *PortConfiguration_builder =
            new_port_states->mutable_configuration();
    PortConfiguration_builder->set_format_version(1);
    PortConfiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
    PortConfiguration_builder->set_id(port_name);
    PortConfiguration_builder->set_name("FriendlyPortName");
    PortConfiguration_builder->set_network_ns("");
    PortConfiguration_builder->set_mac_address(mac_address_prefix + i_string);
    PortConfiguration_builder->set_veth_name("veth0");

    PortConfiguration_HostInfo *portConfig_HostInfoBuilder(new PortConfiguration_HostInfo);
    portConfig_HostInfoBuilder->set_ip_address("172.0.0.2");
    portConfig_HostInfoBuilder->set_mac_address("aa-bb-cc-dd-ee-ff");
    PortConfiguration_builder->set_allocated_host_info(portConfig_HostInfoBuilder);

    // this will allocate new PortConfiguration_FixedIp may need to free later
    PortConfiguration_FixedIp *PortIp_builder =
            PortConfiguration_builder->add_fixed_ips();
    PortIp_builder->set_ip_address(ip_address_prefix + i_string);
    PortIp_builder->set_subnet_id("superSubnetID");
    // this will allocate new PortConfiguration_SecurityGroupId may need to free later
    PortConfiguration_SecurityGroupId *SecurityGroup_builder =
            PortConfiguration_builder->add_security_group_ids();
    SecurityGroup_builder->set_id("1");
    // this will allocate new PortConfiguration_AllowAddressPair may need to free later
    PortConfiguration_AllowAddressPair *AddressPair_builder =
            PortConfiguration_builder->add_allow_address_pairs();
    AddressPair_builder->set_ip_address("10.0.0.5");
    AddressPair_builder->set_mac_address("fa:16:3e:d7:f2:9f");
  }

  // fill in the subnet state structs
  new_subnet_states->set_operation_type(OperationType::INFO);

  // this will allocate new SubnetConfiguration, will need to free it later
  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_format_version(1);
  SubnetConiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
  SubnetConiguration_builder->set_vpc_id(vpc_id);
  SubnetConiguration_builder->set_id(network_id);
  SubnetConiguration_builder->set_name("SuperSubnet");
  SubnetConiguration_builder->set_cidr("10.0.0.0/16");
  SubnetConiguration_builder->set_tunnel_id(22222);
  SubnetConfiguration_Gateway *SubnetConiguration_Gateway_builder =
          SubnetConiguration_builder->mutable_gateway();
  SubnetConiguration_Gateway_builder->set_ip_address(gateway_ip);
  SubnetConiguration_Gateway_builder->set_mac_address(gateway_mac);
  // this will allocate new SubnetConfiguration_TransitSwitch, may need to free it later
  SubnetConfiguration_TransitSwitch *TransitSwitch_builder =
          SubnetConiguration_builder->add_transit_switches();
  TransitSwitch_builder->set_vpc_id(vpc_id);
  TransitSwitch_builder->set_subnet_id("superSubnet");
  TransitSwitch_builder->set_ip_address("172.0.0.1");
  TransitSwitch_builder->set_mac_address("cc:dd:ee:aa:bb:cc");

  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = false;

  GoalStateOperationReply gsOperationalReply;
  rc = Aca_Comm_Manager::get_instance().update_goal_state(GoalState_builder, gsOperationalReply);
  // rc can be error if transitd is not loaded
  if (g_transitd_loaded) {
    EXPECT_EQ(rc, EXIT_SUCCESS);
  }

  g_demo_mode = previous_demo_mode;

  // clean up
  for (int i = 0; i < PORTS_TO_CREATE; i++) {
    string i_string = std::to_string(i);
    string port_name = i_string + port_name_postfix;

    string truncated_port_id = port_name.substr(0, 11);
    string peer_name_string = "peer" + truncated_port_id;

    // delete the newly created veth pair
    rc = Aca_Net_Config::get_instance().delete_veth_pair(
            peer_name_string, culminative_network_configuration_time);
    EXPECT_EQ(rc, EXIT_SUCCESS);
  }

  // delete the newly created ns
  cmd_string = IP_NETNS_PREFIX + "delete " + vpc_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);
}
#endif

int main(int argc, char **argv)
{
  int option;

  ACA_LOG_INIT(ACALOGNAME);

  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  testing::InitGoogleTest(&argc, argv);

  while ((option = getopt(argc, argv, "s:p:t")) != -1) {
    switch (option) {
    case 's':
      g_rpc_server = optarg;
      break;
    case 'p':
      g_rpc_protocol = optarg;
      break;
    case 't':
      g_transitd_loaded = true;
      break;
    default: /* the '?' case when the option is not recognized */
      fprintf(stderr,
              "Usage: %s\n"
              "\t\t[-s transitd RPC server]\n"
              "\t\t[-p transitd RPC protocol]\n"
              "\t\t[-t transitd is loaded]\n",
              argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  // fill in the transitd RPC server and protocol if it is not provided in
  // command line arg
  if (g_rpc_server == EMPTY_STRING) {
    g_rpc_server = LOCALHOST;
  }
  if (g_rpc_protocol == EMPTY_STRING) {
    g_rpc_protocol = UDP_PROTOCOL;
  }

  int rc = RUN_ALL_TESTS();

  aca_cleanup();

  return rc;
}
