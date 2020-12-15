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
#include "aca_dhcp_server.h"
#include "aca_dhcp_programming_if.h"
#include "aca_net_config.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_comm_mgr.h"
#include "aca_util.h"
#include "goalstate.pb.h"
#include "aca_ovs_control.h"
#include <thread>

using namespace std;
using namespace alcor::schema;
using namespace aca_dhcp_server;
using namespace aca_dhcp_programming_if;
using aca_comm_manager::Aca_Comm_Manager;
using aca_net_config::Aca_Net_Config;
using aca_ovs_control::ACA_OVS_Control;
using aca_ovs_l2_programmer::ACA_OVS_L2_Programmer;

thread *ovs_monitor_thread = NULL;

// extern the string and helper functions from aca_test_ovs_l2.cpp
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
extern string subnet1_gw_ip;
extern string subnet2_gw_ip;
extern string subnet1_gw_mac;
extern string subnet2_gw_mac;
static string subnet1_cidr = "10.10.0.0/24";
static string subnet2_cidr = "10.10.1.0/24";
static string subnet1_primary_dns = "8.8.8.8";
static string subnet1_second_dns = "114.114.114.114";
static string subnet2_primary_dns = "8.8.8.8";
static string subnet2_second_dns = "114.114.114.114";
static string port_dns_entry = "1.2.3.4";

static string dhcp_test_router_namespace = "dhcp_test_router";

//
// Test suite: dhcp_config_test_cases
//
// Testing the DHCP implementation on add/delete dhcp entry
// and other internal functions
//
TEST(dhcp_config_test_cases, add_dhcp_entry_valid)
{
  int retcode = 0;
  dhcp_config stDhcpCfgIn;

  stDhcpCfgIn.ipv4_address = "10.0.0.1";
  stDhcpCfgIn.mac_address = "AA:BB:CC:DD:EE:FF";
  stDhcpCfgIn.port_host_name = "Port1";

  retcode = ACA_Dhcp_Server::get_instance().add_dhcp_entry(&stDhcpCfgIn);
  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(dhcp_config_test_cases, add_dhcp_entry_invalid)
{
  int retcode = 0;
  dhcp_config stDhcpCfgIn1;
  dhcp_config stDhcpCfgIn2;

  stDhcpCfgIn1.ipv4_address = "10.0.0.1";
  stDhcpCfgIn1.mac_address = "AA:BB:CC:DD:EE:FF";
  stDhcpCfgIn1.port_host_name = "Port1";

  stDhcpCfgIn2.ipv4_address = "10.0.0.2";
  stDhcpCfgIn2.mac_address = "AA:BB:CC:DD:EE:FF";
  stDhcpCfgIn2.port_host_name = "Port2";

  (void)ACA_Dhcp_Server::get_instance().add_dhcp_entry(&stDhcpCfgIn1);

  retcode = ACA_Dhcp_Server::get_instance().add_dhcp_entry(&stDhcpCfgIn2);
  EXPECT_EQ(retcode, EXIT_FAILURE);
}

TEST(dhcp_config_test_cases, delete_dhcp_entry_valid)
{
  int retcode = 0;
  dhcp_config stDhcpCfgIn;

  stDhcpCfgIn.ipv4_address = "10.0.0.1";
  stDhcpCfgIn.mac_address = "AA:BB:CC:DD:EE:FF";
  stDhcpCfgIn.port_host_name = "Port1";

  (void)ACA_Dhcp_Server::get_instance().add_dhcp_entry(&stDhcpCfgIn);

  retcode = ACA_Dhcp_Server::get_instance().delete_dhcp_entry(&stDhcpCfgIn);
  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(dhcp_config_test_cases, update_dhcp_entry_valid)
{
  int retcode = 0;
  dhcp_config stDhcpCfgIn;

  stDhcpCfgIn.ipv4_address = "10.0.0.1";
  stDhcpCfgIn.mac_address = "AA:BB:CC:DD:EE:FF";
  stDhcpCfgIn.port_host_name = "Port1";

  (void)ACA_Dhcp_Server::get_instance().add_dhcp_entry(&stDhcpCfgIn);

  stDhcpCfgIn.ipv4_address = "10.0.0.2";
  retcode = ACA_Dhcp_Server::get_instance().update_dhcp_entry(&stDhcpCfgIn);
  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(dhcp_config_test_cases, update_dhcp_entry_invalid)
{
  int retcode = 0;
  dhcp_config stDhcpCfgIn;

  stDhcpCfgIn.ipv4_address = "10.0.0.1";
  stDhcpCfgIn.mac_address = "AA:BB:CC:DD:EE:FF";
  stDhcpCfgIn.port_host_name = "Port1";

  (void)ACA_Dhcp_Server::get_instance().delete_dhcp_entry(&stDhcpCfgIn);
  retcode = ACA_Dhcp_Server::get_instance().update_dhcp_entry(&stDhcpCfgIn);
  EXPECT_EQ(retcode, EXIT_FAILURE);
}

TEST(dhcp_message_test_cases, dhcps_recv_valid)
{
  int retcode = 0;
  dhcp_message stDhcpMsg;

  stDhcpMsg.op = BOOTP_MSG_BOOTREQUEST;
  stDhcpMsg.htype = DHCP_MSG_HWTYPE_ETH;
  stDhcpMsg.hlen = DHCP_MSG_HWTYPE_ETH_LEN;
  stDhcpMsg.xid = 12345;
  stDhcpMsg.flags = 0x8000;
  stDhcpMsg.chaddr[0] = 0x3c;
  stDhcpMsg.chaddr[1] = 0xf0;
  stDhcpMsg.chaddr[2] = 0x11;
  stDhcpMsg.chaddr[3] = 0x12;
  stDhcpMsg.chaddr[4] = 0x56;
  stDhcpMsg.chaddr[5] = 0x65;
  stDhcpMsg.cookie = DHCP_MSG_MAGIC_COOKIE;
  stDhcpMsg.options[0] = DHCP_OPT_CODE_MSGTYPE;
  stDhcpMsg.options[1] = DHCP_OPT_LEN_1BYTE;
  stDhcpMsg.options[2] = DHCP_MSG_DHCPDISCOVER;
  stDhcpMsg.options[3] = DHCP_OPT_END;

  retcode = ACA_Dhcp_Server::get_instance()._validate_dhcp_message(&stDhcpMsg);
  EXPECT_EQ(retcode, EXIT_SUCCESS);

  retcode = ACA_Dhcp_Server::get_instance()._get_message_type(&stDhcpMsg);
  EXPECT_EQ(retcode, DHCP_MSG_DHCPDISCOVER);
}

TEST(dhcp_message_test_cases, get_options_valid)
{
  int retcode = 0;
  dhcp_message stDhcpMsg;

  stDhcpMsg.op = BOOTP_MSG_BOOTREQUEST;
  stDhcpMsg.htype = DHCP_MSG_HWTYPE_ETH;
  stDhcpMsg.hlen = DHCP_MSG_HWTYPE_ETH_LEN;
  stDhcpMsg.xid = 12345;
  stDhcpMsg.flags = 0x8000;
  stDhcpMsg.chaddr[0] = 0x3c;
  stDhcpMsg.chaddr[1] = 0xf0;
  stDhcpMsg.chaddr[2] = 0x11;
  stDhcpMsg.chaddr[3] = 0x12;
  stDhcpMsg.chaddr[4] = 0x56;
  stDhcpMsg.chaddr[5] = 0x65;
  stDhcpMsg.cookie = DHCP_MSG_MAGIC_COOKIE;

  stDhcpMsg.options[0] = DHCP_OPT_CODE_MSGTYPE;
  stDhcpMsg.options[1] = DHCP_OPT_LEN_1BYTE;
  stDhcpMsg.options[2] = DHCP_MSG_DHCPDISCOVER;

  stDhcpMsg.options[3] = DHCP_OPT_CODE_SERVER_ID;
  stDhcpMsg.options[4] = DHCP_OPT_LEN_4BYTE;
  stDhcpMsg.options[5] = 0x7f;
  stDhcpMsg.options[6] = 0x00;
  stDhcpMsg.options[7] = 0x00;
  stDhcpMsg.options[8] = 0x01;

  stDhcpMsg.options[9] = DHCP_OPT_CODE_REQ_IP;
  stDhcpMsg.options[10] = DHCP_OPT_LEN_4BYTE;
  stDhcpMsg.options[11] = 0x0a;
  stDhcpMsg.options[12] = 0x00;
  stDhcpMsg.options[13] = 0x00;
  stDhcpMsg.options[14] = 0x01;

  stDhcpMsg.options[15] = DHCP_OPT_END;

  retcode = ACA_Dhcp_Server::get_instance()._get_message_type(&stDhcpMsg);
  EXPECT_EQ(retcode, DHCP_MSG_DHCPDISCOVER);

  retcode = ACA_Dhcp_Server::get_instance()._get_server_id(&stDhcpMsg);
  EXPECT_EQ(retcode, 0x7f000001);

  retcode = ACA_Dhcp_Server::get_instance()._get_requested_ip(&stDhcpMsg);
  EXPECT_EQ(retcode, 0x0a000001);
}

TEST(dhcp_request_test_case, DISABLED_l2_dhcp_test)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  // kill the docker instances just in case
  Aca_Net_Config::get_instance().execute_system_command("docker rm -f con1");

  Aca_Net_Config::get_instance().execute_system_command("docker rm -f con2");

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // re adding dhcp default flows
  ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          "add-flow br-int \"table=0,priority=25,udp,udp_src=68,udp_dst=67,actions=CONTROLLER\"",
          not_care_culminative_time, overall_rc);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // monitor br-int for dhcp request message
  ovs_monitor_thread =
          new thread(bind(&ACA_OVS_Control::monitor,
                          &ACA_OVS_Control::get_instance(), "br-int", "resume"));
  ovs_monitor_thread->detach();

  GoalState GoalState_builder;
  DHCPState *new_dhcp_states = GoalState_builder.add_dhcp_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  new_dhcp_states->set_operation_type(OperationType::CREATE);

  // fill in port state structs
  DHCPConfiguration *DHCPConfiguration_builder = new_dhcp_states->mutable_configuration();
  DHCPConfiguration_builder->set_revision_number(1);
  DHCPConfiguration_builder->set_update_type(UpdateType::FULL);

  DHCPConfiguration_builder->set_subnet_id(subnet_id_1);
  DHCPConfiguration_builder->set_mac_address(vmac_address_1);
  DHCPConfiguration_builder->set_ipv4_address(vip_address_1);
  DHCPConfiguration_builder->set_port_host_name(port_name_1);

  // fill in subnet state structs
  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_vpc_id(vpc_id_1);
  SubnetConiguration_builder->set_id(subnet_id_1);
  SubnetConiguration_builder->set_cidr(subnet1_cidr);
  SubnetConiguration_builder->set_tunnel_id(123);

  auto *subnetConfig_GatewayBuilder(new SubnetConfiguration_Gateway);
  subnetConfig_GatewayBuilder->set_ip_address(subnet1_gw_ip);
  subnetConfig_GatewayBuilder->set_mac_address(subnet1_gw_mac);
  SubnetConiguration_builder->set_allocated_gateway(subnetConfig_GatewayBuilder);

  // dns entry
  SubnetConfiguration_DnsEntry *DnsEntry_builder =
          SubnetConiguration_builder->add_dns_entry_list();
  DnsEntry_builder->set_entry(port_dns_entry);

  // create a new port 1 in demo mode
  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // create a new port 2 in demo mode
  DHCPConfiguration_builder->set_mac_address(vmac_address_3);
  DHCPConfiguration_builder->set_ipv4_address(vip_address_3);
  DHCPConfiguration_builder->set_port_host_name(port_name_3);

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // send goal state ok, now create a docker instance test
  // con1
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --cap-add=NET_ADMIN --name con1 --net=none alpine sh");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ovs-docker add-port br-int eth1 con1 --macaddress=" + vmac_address_1;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ovs-docker set-vlan br-int eth1 con1 1";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "docker exec con1 udhcpc -i eth1";
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // con2
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --cap-add=NET_ADMIN --name con2 --net=none alpine sh");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ovs-docker add-port br-int eth1 con2 --macaddress=" + vmac_address_3;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ovs-docker set-vlan br-int eth1 con2 1";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "docker exec con2 udhcpc -i eth1";
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // test ping
  cmd_string = "docker exec con1 ping -c1 " + vip_address_3;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "docker exec con2 ping -c1 " + vip_address_1;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // clean up
  overall_rc = Aca_Net_Config::get_instance().execute_system_command("ovs-docker del-ports br-int con1");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("ovs-docker del-ports br-int con2");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker rm con1 -f");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker rm con2 -f");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // free the allocated configurations since we are done with it now
  new_dhcp_states->clear_configuration();
  new_subnet_states->clear_configuration();
}

TEST(dhcp_request_test_case, DISABLED_l3_dhcp_test)
{
  ulong not_care_culminative_time = 0;
  string cmd_string;
  int overall_rc;

  // kill the docker instances just in case
  Aca_Net_Config::get_instance().execute_system_command("docker rm -f con1");

  Aca_Net_Config::get_instance().execute_system_command("docker rm -f con2");

  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // re-adding dhcp default flows
  ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          "add-flow br-int \"table=0,priority=25,udp,udp_src=68,udp_dst=67,actions=CONTROLLER\"",
          not_care_culminative_time, overall_rc);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // monitor br-int for dhcp request message
  ovs_monitor_thread =
          new thread(bind(&ACA_OVS_Control::monitor,
                          &ACA_OVS_Control::get_instance(), "br-int", "resume"));
  ovs_monitor_thread->detach();

  GoalState GoalState_builder;
  DHCPState *new_dhcp_states = GoalState_builder.add_dhcp_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();

  new_dhcp_states->set_operation_type(OperationType::CREATE);

  // fill in port state structs
  DHCPConfiguration *DHCPConfiguration_builder = new_dhcp_states->mutable_configuration();
  DHCPConfiguration_builder->set_revision_number(1);
  DHCPConfiguration_builder->set_update_type(UpdateType::FULL);

  DHCPConfiguration_builder->set_subnet_id(subnet_id_1);
  DHCPConfiguration_builder->set_mac_address(vmac_address_1);
  DHCPConfiguration_builder->set_ipv4_address(vip_address_1);
  DHCPConfiguration_builder->set_port_host_name(port_name_1);

  // fill in subnet state structs
  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_vpc_id(vpc_id_1);
  SubnetConiguration_builder->set_id(subnet_id_1);
  SubnetConiguration_builder->set_cidr(subnet1_cidr);
  SubnetConiguration_builder->set_tunnel_id(123);

  auto *subnetConfig_GatewayBuilder(new SubnetConfiguration_Gateway);
  subnetConfig_GatewayBuilder->set_ip_address(subnet1_gw_ip);
  subnetConfig_GatewayBuilder->set_mac_address(subnet1_gw_mac);
  SubnetConiguration_builder->set_allocated_gateway(subnetConfig_GatewayBuilder);

  // create a new port 1 in demo mode
  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // create a new port 2 in demo mode
  DHCPConfiguration_builder->set_subnet_id(subnet_id_2);
  DHCPConfiguration_builder->set_mac_address(vmac_address_2);
  DHCPConfiguration_builder->set_ipv4_address(vip_address_2);
  DHCPConfiguration_builder->set_port_host_name(port_name_2);

  SubnetConiguration_builder->set_id(subnet_id_2);
  SubnetConiguration_builder->set_cidr(subnet2_cidr);
  SubnetConiguration_builder->set_tunnel_id(124);

  subnetConfig_GatewayBuilder->set_ip_address(subnet2_gw_ip);
  subnetConfig_GatewayBuilder->set_mac_address(subnet2_gw_mac);

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // send goal state ok, now create a docker instance test
  // con1
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --cap-add=NET_ADMIN --name con1 --net=none alpine sh");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ovs-docker add-port br-int eth1 con1 --macaddress=" + vmac_address_1;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ovs-docker set-vlan br-int eth1 con1 1";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "docker exec con1 udhcpc -i eth1";
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // con2
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --cap-add=NET_ADMIN --name con2 --net=none alpine sh");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ovs-docker add-port br-int eth1 con2 --macaddress=" + vmac_address_2;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ovs-docker set-vlan br-int eth1 con2 2";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "docker exec con2 udhcpc -i eth1";
  Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // enable ip netns for router
  // add dhcp router namespace
  cmd_string = "ip netns add " + dhcp_test_router_namespace;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ip link add gw1_ovs type veth peer name gw1_ns";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ip link add gw2_ovs type veth peer name gw2_ns";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ip link set gw1_ns netns " + dhcp_test_router_namespace;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ip link set gw2_ns netns " + dhcp_test_router_namespace;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // up veth
  cmd_string = "ip netns exec " + dhcp_test_router_namespace + " ip link set gw1_ns up";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ip netns exec " + dhcp_test_router_namespace + " ip link set gw2_ns up";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ip netns exec " + dhcp_test_router_namespace +
               " ifconfig gw1_ns " + subnet1_gw_ip + "/24";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ip netns exec " + dhcp_test_router_namespace +
               " ifconfig gw2_ns " + subnet2_gw_ip + "/24";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "add-port br-int gw1_ovs", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "add-port br-int gw2_ovs", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ip link set gw1_ovs up";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ip link set gw2_ovs up";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // set ovs port vlan
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "set port gw1_ovs tag=1", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "set port gw2_ovs tag=2", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // test ping
  cmd_string = "docker exec con1 ping -c1 " + vip_address_2;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "docker exec con2 ping -c1 " + vip_address_1;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // clean up
  // delete br-int gw ports
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-port br-int gw1_ovs", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-port br-int gw2_ovs", not_care_culminative_time, overall_rc);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // clear docker ovs ports
  overall_rc = Aca_Net_Config::get_instance().execute_system_command("ovs-docker del-ports br-int con1");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("ovs-docker del-ports br-int con2");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // clear ip link and namespace
  cmd_string = "ip netns del " + dhcp_test_router_namespace;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker rm con1 -f");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker rm con2 -f");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // free the allocated configurations since we are done with it now
  new_dhcp_states->clear_configuration();
  new_subnet_states->clear_configuration();
}