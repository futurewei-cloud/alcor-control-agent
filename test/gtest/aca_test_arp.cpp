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
#include "aca_arp_responder.h"
#include "aca_net_config.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_comm_mgr.h"
#include "aca_util.h"
#include "goalstate.pb.h"
#include "aca_ovs_control.h"
#include <thread>

using namespace std;
using namespace alcor::schema;
using namespace aca_arp_responder;
using aca_comm_manager::Aca_Comm_Manager;
using aca_net_config::Aca_Net_Config;
using aca_ovs_l2_programmer::ACA_OVS_L2_Programmer;
using aca_ovs_control::ACA_OVS_Control;

extern thread *ovs_monitor_thread;

// extern the string and helper functions from aca_test_arp.cpp
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
extern string subnet1_gw_mac;
extern string subnet2_gw_mac;
extern string remote_ip_1; // for docker network
extern string remote_ip_2; // for docker network

extern bool g_demo_mode;
extern void aca_test_reset_environment();

static string arp_test_router_namespace = "arp_test_router";

//
// Test suite: arp_config_test_cases
//
// Testing the Arp implementation on add/update/delete arp entry
// and other internal functions
//
TEST(arp_config_test_cases, add_arp_entry_valid)
{
  int retcode = 0;
  arp_config stArpCfgIn;

  stArpCfgIn.ipv4_address = "10.0.1.1";
  stArpCfgIn.mac_address = "AA:BB:CC:DD:EE:FF";
  stArpCfgIn.vlan_id = 1201;

  retcode = ACA_ARP_Responder::get_instance().add_arp_entry(&stArpCfgIn);
  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(arp_config_test_cases, add_arp_entry_invalid)
{
  int retcode = 0;
  arp_config stArpCfgIn1;
  arp_config stArpCfgIn2;

  stArpCfgIn1.ipv4_address = "10.0.0.1";
  stArpCfgIn1.mac_address = "AA:BB:CC:DD:EE:FF";
  stArpCfgIn1.vlan_id = 1201;

  stArpCfgIn2.ipv4_address = "10.0.0.1";
  stArpCfgIn2.mac_address = "AA:BB:CC:DD:EE:EF";
  stArpCfgIn2.vlan_id = 1201;

  (void)ACA_ARP_Responder::get_instance().add_arp_entry(&stArpCfgIn1);

  retcode = ACA_ARP_Responder::get_instance().add_arp_entry(&stArpCfgIn2);
  EXPECT_EQ(retcode, EXIT_FAILURE);
}

TEST(arp_config_test_cases, delete_arp_entry_valid)
{
  int retcode = 0;
  arp_config stArpCfgIn;

  stArpCfgIn.ipv4_address = "10.0.0.1";
  stArpCfgIn.mac_address = "AA:BB:CC:DD:EE:FF";
  stArpCfgIn.vlan_id = 1201;

  (void)ACA_ARP_Responder::get_instance().add_arp_entry(&stArpCfgIn);

  retcode = ACA_ARP_Responder::get_instance().delete_arp_entry(&stArpCfgIn);
  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(arp_config_test_cases, update_arp_entry_valid)
{
  int retcode = 0;
  arp_config stArpCfgIn;

  stArpCfgIn.ipv4_address = "10.0.0.1";
  stArpCfgIn.mac_address = "AA:BB:CC:DD:EE:FF";
  stArpCfgIn.vlan_id = 1201;

  (void)ACA_ARP_Responder::get_instance().add_arp_entry(&stArpCfgIn);

  stArpCfgIn.mac_address= "AA:BB:CC:DD:EE:EF";
  retcode = ACA_ARP_Responder::get_instance().create_or_update_arp_entry(&stArpCfgIn);
  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(arp_config_test_cases, update_arp_entry_invalid)
{
  int retcode = 0;
  arp_config stArpCfgIn;

  stArpCfgIn.ipv4_address = "10.0.0.1";
  stArpCfgIn.mac_address = "AA:BB:CC:DD:EE:FF";
  stArpCfgIn.vlan_id = 1201;

  (void)ACA_ARP_Responder::get_instance().delete_arp_entry(&stArpCfgIn);
  retcode = ACA_ARP_Responder::get_instance().create_or_update_arp_entry(&stArpCfgIn);
  EXPECT_EQ(retcode, EXIT_SUCCESS);
}


TEST(arp_request_test_cases, arps_recv_valid)
{
  int retcode = 0;
  arp_message stArpMsg;

  stArpMsg.hrd = ARP_MSG_HRD_TYPE;
  stArpMsg.pro = ARP_MSG_PRO_TYPE;
  stArpMsg.hln = ARP_MSG_HRD_LEN;
  stArpMsg.pln = ARP_MSG_PRO_LEN;
  stArpMsg.op = htons(ARP_MSG_ARPREQUEST);
  stArpMsg.sha[0] = 0x3c;
  stArpMsg.sha[1] = 0xf0;
  stArpMsg.sha[2] = 0x11;
  stArpMsg.sha[3] = 0x12;
  stArpMsg.sha[4] = 0x56;
  stArpMsg.sha[5] = 0x65;
  stArpMsg.spa = 0xc0a8010f;
  stArpMsg.tha[0] = 0x0;
  stArpMsg.tha[1] = 0x0;
  stArpMsg.tha[2] = 0x0;
  stArpMsg.tha[3] = 0x0;
  stArpMsg.tha[4] = 0x0;
  stArpMsg.tha[5] = 0x0;
  stArpMsg.tpa = 0xc0a80102;

  retcode = ACA_ARP_Responder::get_instance()._validate_arp_message(&stArpMsg);
  EXPECT_EQ(retcode, EXIT_SUCCESS);
}


TEST(arp_request_test_cases, DISABLED_l2_arp_test_one_machine)
{
  string cmd_string;
  arp_config stArpCfgIn;
  int overall_rc;

  aca_test_reset_environment();

  // monitor br-tun for arp request message
  ovs_monitor_thread = 
    new thread(bind(&ACA_OVS_Control::monitor, &ACA_OVS_Control::get_instance(), "br-tun", "resume"));
  ovs_monitor_thread->detach();

  GoalState GoalState_builder;

  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
  new_subnet_states->set_operation_type(OperationType::INFO);
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

  NeighborState *new_neighbor_states = GoalState_builder.add_neighbor_states();

  new_neighbor_states->set_operation_type(OperationType::CREATE);

  // fill in neighbor state structs
  NeighborConfiguration *NeighborConfiguration_builder =
          new_neighbor_states->mutable_configuration();
  NeighborConfiguration_builder->set_revision_number(1);

  NeighborConfiguration_builder->set_vpc_id(vpc_id_1);
  NeighborConfiguration_builder->set_id(port_id_3);
  NeighborConfiguration_builder->set_mac_address(vmac_address_3);
  NeighborConfiguration_builder->set_host_ip_address("172.16.62.158");

  NeighborConfiguration_FixedIp *FixedIp_builder =
          NeighborConfiguration_builder->add_fixed_ips();
  FixedIp_builder->set_neighbor_type(NeighborType::L2);
  FixedIp_builder->set_subnet_id(subnet_id_1);
  FixedIp_builder->set_ip_address(vip_address_3);

  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  NeighborConfiguration_builder->set_mac_address(vmac_address_1);
  FixedIp_builder->set_ip_address(vip_address_1);

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);


  // create docker instances for test
  // con1
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --cap-add=NET_ADMIN --name con1 --net=none alpine sh");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ovs-docker add-port br-int eth1 con1 --macaddress=" + vmac_address_1 + " --ipaddress="+vip_address_1 + "/24";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ovs-docker set-vlan br-int eth1 con1 1";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;


  // con2
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --cap-add=NET_ADMIN --name con2 --net=none alpine sh");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ovs-docker add-port br-int eth1 con2 --macaddress=" + vmac_address_3 + " --ipaddress="+vip_address_3 + "/24";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ovs-docker set-vlan br-int eth1 con2 1";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
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


  //clean up
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
              "ovs-docker del-ports br-int con1");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
              "ovs-docker del-ports br-int con2");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker rm con1 -f");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker rm con2 -f");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
}



TEST(arp_request_test_cases, DISABLED_l2_arp_test_PARENT)
{
  string cmd_string;
  arp_config stArpCfgIn;
  int overall_rc;

  aca_test_reset_environment();

  // monitor br-tun for arp request message
  ovs_monitor_thread = 
    new thread(bind(&ACA_OVS_Control::monitor, &ACA_OVS_Control::get_instance(), "br-tun", "resume"));
  ovs_monitor_thread->detach();

  GoalState GoalState_builder;

  PortState *new_port_states = GoalState_builder.add_port_states();
  new_port_states->set_operation_type(OperationType::CREATE);

  // fill in port state structs for port 1
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

  
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
  new_subnet_states->set_operation_type(OperationType::INFO);

  // fill in subnet state structs
  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_vpc_id(vpc_id_1);
  SubnetConiguration_builder->set_id(subnet_id_1);
  SubnetConiguration_builder->set_cidr("10.0.0.0/24");
  SubnetConiguration_builder->set_tunnel_id(20);

  NeighborState *new_neighbor_states = GoalState_builder.add_neighbor_states();
  new_neighbor_states->set_operation_type(OperationType::CREATE);

  // fill in neighbor state structs
  NeighborConfiguration *NeighborConfiguration_builder =
          new_neighbor_states->mutable_configuration();
  NeighborConfiguration_builder->set_revision_number(1);

  NeighborConfiguration_builder->set_vpc_id(vpc_id_1);
  NeighborConfiguration_builder->set_id(port_id_3);
  NeighborConfiguration_builder->set_mac_address(vmac_address_3);
  NeighborConfiguration_builder->set_host_ip_address(remote_ip_2);

  NeighborConfiguration_FixedIp *FixedIp_builder2 =
          NeighborConfiguration_builder->add_fixed_ips();
  FixedIp_builder2->set_neighbor_type(NeighborType::L2);
  FixedIp_builder2->set_subnet_id(subnet_id_1);
  FixedIp_builder2->set_ip_address(vip_address_3);

  
  // set demo mode to false because routing test needs real routable port
  // e.g. container port created by docker + ovs-docker
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = false;

  // create docker instances for test
  // con3
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --cap-add=NET_ADMIN --name con3 --net=none alpine sh");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ovs-docker add-port br-int eth1 con3 --macaddress=" + vmac_address_1 + " --ipaddress="+ vip_address_1 + "/24";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ovs-docker set-vlan br-int eth1 con3 1";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);

  // restore demo mode
  g_demo_mode = previous_demo_mode;

  // test ping 
  cmd_string = "docker exec con3 ping -c1 " + vip_address_3;
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;


  //clean up
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
              "ovs-docker del-ports br-int con3");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker rm con3 -f");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
}

TEST(arp_request_test_cases, DISABLED_l2_arp_test_CHILD)
{
  string cmd_string;
  arp_config stArpCfgIn;
  int overall_rc;

  aca_test_reset_environment();

  // kill the docker instances just in case
  Aca_Net_Config::get_instance().execute_system_command("docker rm -f con4");

  GoalState GoalState_builder;

  PortState *new_port_states = GoalState_builder.add_port_states();
  new_port_states->set_operation_type(OperationType::CREATE);
  
  // fill in port state structs for port 3
  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_update_type(UpdateType::FULL);
  PortConfiguration_builder->set_id(port_id_3);

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

  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
  new_subnet_states->set_operation_type(OperationType::INFO);
  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_vpc_id(vpc_id_1);
  SubnetConiguration_builder->set_id(subnet_id_1);
  SubnetConiguration_builder->set_cidr("10.0.0.0/24");
  SubnetConiguration_builder->set_tunnel_id(20);

  NeighborState *new_neighbor_states = GoalState_builder.add_neighbor_states();

  new_neighbor_states->set_operation_type(OperationType::CREATE);

  // fill in neighbor state structs
  NeighborConfiguration *NeighborConfiguration_builder =
          new_neighbor_states->mutable_configuration();
  NeighborConfiguration_builder->set_revision_number(1);

  NeighborConfiguration_builder->set_vpc_id(vpc_id_1);
  NeighborConfiguration_builder->set_id(port_id_1);
  NeighborConfiguration_builder->set_mac_address(vmac_address_1);
  NeighborConfiguration_builder->set_host_ip_address(remote_ip_1);

  NeighborConfiguration_FixedIp *FixedIp_builder2 =
          NeighborConfiguration_builder->add_fixed_ips();
  FixedIp_builder2->set_neighbor_type(NeighborType::L2);
  FixedIp_builder2->set_subnet_id(subnet_id_1);
  FixedIp_builder2->set_ip_address(vip_address_1);


  // set demo mode to false because routing test needs real routable port
  // e.g. container port created by docker + ovs-docker
  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = false;


  // create docker instances for test
  // con4
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(
          "docker run -itd --cap-add=NET_ADMIN --name con4 --net=none alpine sh");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  cmd_string = "ovs-docker add-port br-int eth0 con4 --macaddress=" + vmac_address_3 + " --ipaddress="+ vip_address_3 + "/24";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  cmd_string = "ovs-docker set-vlan br-int eth0 con4 1";
  overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  GoalStateOperationReply gsOperationalReply;

  overall_rc = Aca_Comm_Manager::get_instance().update_goal_state(
          GoalState_builder, gsOperationalReply);
  EXPECT_NE(overall_rc, EXIT_SUCCESS);

    // restore demo mode
  g_demo_mode = previous_demo_mode;

}