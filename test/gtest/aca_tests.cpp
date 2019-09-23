#include "aca_log.h"
#include "aca_util.h"
#include "aca_comm_mgr.h"
#include "aca_net_config.h"
#include "gtest/gtest.h"

#include "goalstate.pb.h"
#include <unistd.h> /* for getopt */
#include <iostream>
#include <string>

// #include <grpcpp/grpcpp.h>
// #include <grpc/support/log.h>
// #include "goalstateprovisioner.grpc.pb.h"

using namespace std;
using aca_comm_manager::Aca_Comm_Manager;

#define ACALOGNAME "AlcorControlAgentTest"

static char VALID_STRING[] = "VALID_STRING";
static char DEFAULT_MTU[] = "9000";

// Global variables
string g_rpc_server = EMPTY_STRING;
string g_rpc_protocol = EMPTY_STRING;
long g_total_rpc_call_time = 0;
long g_total_rpc_client_time = 0;
long g_total_update_GS_time = 0;
bool g_debug_mode = true;
bool g_demo_mode = false;

using aca_net_config::Aca_Net_Config;

TEST(net_config_test_cases, create_namespace_invalid)
{
  int rc;

  rc = Aca_Net_Config::get_instance().create_namespace(EMPTY_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, create_namespace_valid)
{
  string test_ns = "test_ns";
  string cmd_string;
  int rc;

  rc = Aca_Net_Config::get_instance().create_namespace(test_ns);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  // is the newly create namespace there?
  cmd_string = "ip netns list " + test_ns + " | grep " + test_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created ns
  cmd_string = "ip netns delete " + test_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the newly create namespace should be gone now
  cmd_string = "ip netns list " + test_ns + " | grep " + test_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, create_veth_pair_invalid)
{
  int rc;

  rc = Aca_Net_Config::get_instance().create_veth_pair(EMPTY_STRING, VALID_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().create_veth_pair(VALID_STRING, EMPTY_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, create_veth_pair_valid)
{
  string veth = "vethtest";
  string peer = "peertest";
  string cmd_string;
  int rc;

  // create the veth pair
  rc = Aca_Net_Config::get_instance().create_veth_pair(veth, peer);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  // is the newly created veth pair there?
  cmd_string = "ip link list " + veth + " | grep " + veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  cmd_string = "ip link list " + peer + " | grep " + peer;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created veth pair
  cmd_string = "ip link delete " + veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
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
  int rc;

  rc = Aca_Net_Config::get_instance().setup_peer_device(EMPTY_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, setup_peer_device_valid)
{
  string veth = "vethtest";
  string peer = "peertest";
  string cmd_string;
  int rc;

  // create the veth pair
  rc = Aca_Net_Config::get_instance().create_veth_pair(veth, peer);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().setup_peer_device(peer);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // was the MTU applied successfully?
  cmd_string = "ip link list " + peer + " | grep " + peer + " | grep " + DEFAULT_MTU;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created veth pair
  cmd_string = "ip link delete " + veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, move_to_namespace_invalid)
{
  int rc;

  rc = Aca_Net_Config::get_instance().move_to_namespace(EMPTY_STRING, VALID_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().move_to_namespace(VALID_STRING, EMPTY_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, move_to_namespace_valid)
{
  string veth = "vethtest";
  string peer = "peertest";
  string test_ns = "test_ns";
  string cmd_string;
  int rc;

  rc = Aca_Net_Config::get_instance().create_namespace(test_ns);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().create_veth_pair(veth, peer);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the veth should not be in the new namespace yet
  cmd_string = "ip netns exec " + test_ns + " ip link list | grep " + veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(rc, EXIT_SUCCESS);

  // move the veth into the new namespace
  rc = Aca_Net_Config::get_instance().move_to_namespace(veth, test_ns);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the veth should be in the new namespace now
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created veth pair
  cmd_string = "ip netns exec " + test_ns + " ip link delete " + veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created ns
  cmd_string = "ip netns delete " + test_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, setup_veth_device_invalid)
{
  int rc;

  rc = Aca_Net_Config::get_instance().setup_veth_device(
          EMPTY_STRING, VALID_STRING, VALID_STRING, VALID_STRING, VALID_STRING, VALID_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().setup_veth_device(
          VALID_STRING, EMPTY_STRING, VALID_STRING, VALID_STRING, VALID_STRING, VALID_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().setup_veth_device(
          VALID_STRING, VALID_STRING, EMPTY_STRING, VALID_STRING, VALID_STRING, VALID_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().setup_veth_device(
          VALID_STRING, VALID_STRING, VALID_STRING, EMPTY_STRING, VALID_STRING, VALID_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().setup_veth_device(
          VALID_STRING, VALID_STRING, VALID_STRING, VALID_STRING, EMPTY_STRING, VALID_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().setup_veth_device(
          VALID_STRING, VALID_STRING, VALID_STRING, VALID_STRING, VALID_STRING, EMPTY_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, setup_veth_device_valid)
{
  string veth = "vethtest";
  string peer = "peertest";
  string test_ns = "test_ns";
  string ip = "10.0.0.2";
  string prefix = "16";
  string mac = "aa:bb:cc:dd:ee:ff";
  string gw = "10.0.0.1";
  string cmd_string;
  int rc;

  rc = Aca_Net_Config::get_instance().create_namespace(test_ns);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().create_veth_pair(veth, peer);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the veth should not be in the new namespace yet
  cmd_string = "ip netns exec " + test_ns + " ip link list | grep " + veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(rc, EXIT_SUCCESS);

  // move the veth into the new namespace
  rc = Aca_Net_Config::get_instance().move_to_namespace(veth, test_ns);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the veth should be in the new namespace now
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  rc = Aca_Net_Config::get_instance().setup_veth_device(test_ns, veth, ip, prefix, mac, gw);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  g_demo_mode = previous_demo_mode;

  // was the ip set correctly?
  cmd_string = "ip netns exec " + test_ns + " ifconfig " + veth + " | grep " + ip;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // was the default gw set correctly?
  cmd_string = "ip netns exec " + test_ns + " ip route" + " | grep " + gw;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // was the mac set correctly?
  cmd_string = "ip netns exec " + test_ns + " ifconfig " + veth + " | grep " + mac;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created veth pair
  cmd_string = "ip netns exec " + test_ns + " ip link delete " + veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created ns
  cmd_string = "ip netns delete " + test_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, rename_veth_device_invalid)
{
  int rc;

  rc = Aca_Net_Config::get_instance().rename_veth_device(
          EMPTY_STRING, VALID_STRING, VALID_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().rename_veth_device(
          VALID_STRING, EMPTY_STRING, VALID_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().rename_veth_device(
          VALID_STRING, VALID_STRING, EMPTY_STRING);
  ASSERT_NE(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, rename_veth_device_valid)
{
  string old_veth = "vethtest";
  string peer = "peertest";
  string test_ns = "test_ns";
  string new_veth = "vethnew";
  string cmd_string;
  int rc;

  rc = Aca_Net_Config::get_instance().create_namespace(test_ns);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  rc = Aca_Net_Config::get_instance().create_veth_pair(old_veth, peer);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the veth should not be in the new namespace yet
  cmd_string = "ip netns exec " + test_ns + " ip link list | grep " + old_veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(rc, EXIT_SUCCESS);

  // move the veth into the new namespace
  rc = Aca_Net_Config::get_instance().move_to_namespace(old_veth, test_ns);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the veth should be in the new namespace now
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // rename the veth
  rc = Aca_Net_Config::get_instance().rename_veth_device(test_ns, old_veth, new_veth);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the new veth should be there
  cmd_string = "ip netns exec " + test_ns + " ip link list | grep " + new_veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // the old veth should not be there
  cmd_string = "ip netns exec " + test_ns + " ip link list | grep " + old_veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_NE(rc, EXIT_SUCCESS);

  // delete the newly created veth pair
  cmd_string = "ip netns exec " + test_ns + " ip link delete " + new_veth;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created ns
  cmd_string = "ip netns delete " + test_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);
}

TEST(net_config_test_cases, create_endpoint_integrated)
{
  string port_name = "11111111-2222-3333-4444-555555555555";
  string vpc_id = "99d9d709-8478-4b46-9f3f-2206b1023fd3";
  string vpc_ns = "vpc-ns-" + vpc_id;
  string ip_address = "10.0.0.2";
  string mac_address = "fa:16:3e:d7:f2:6c";
  string gw_ip = "10.0.0.1";
  string cmd_string;
  int rc;

  string truncated_port_id = port_name.substr(0, 11);
  string temp_name_string = "temp" + truncated_port_id;
  string veth_name_string = "veth" + truncated_port_id;

  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  aliothcontroller::GoalState GoalState_builder;
  aliothcontroller::PortState *new_port_states = GoalState_builder.add_port_states();
  aliothcontroller::SubnetState *new_subnet_states =
          GoalState_builder.add_subnet_states();

  // fill in port state structs
  new_port_states->set_operation_type(aliothcontroller::OperationType::CREATE);

  // this will allocate new PortConfiguration, will need to free it later
  aliothcontroller::PortConfiguration *PortConfiguration_builder =
          new_port_states->mutable_configuration();
  PortConfiguration_builder->set_version(1);
  PortConfiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
  PortConfiguration_builder->set_network_id("superSubnetID");
  PortConfiguration_builder->set_id(port_name);
  PortConfiguration_builder->set_name("FriendlyPortName");
  PortConfiguration_builder->set_admin_state_up(true);
  PortConfiguration_builder->set_mac_address(mac_address);
  PortConfiguration_builder->set_veth_name("veth0");

  aliothcontroller::PortConfiguration_HostInfo *portConfig_HostInfoBuilder(
          new aliothcontroller::PortConfiguration_HostInfo);
  portConfig_HostInfoBuilder->set_ip_address("172.0.0.2");
  portConfig_HostInfoBuilder->set_mac_address("aa-bb-cc-dd-ee-ff");
  PortConfiguration_builder->set_allocated_host_info(portConfig_HostInfoBuilder);

  // this will allocate new PortConfiguration_FixedIp may need to free later
  aliothcontroller::PortConfiguration_FixedIp *PortIp_builder =
          PortConfiguration_builder->add_fixed_ips();
  PortIp_builder->set_ip_address(ip_address);
  PortIp_builder->set_subnet_id("2");
  // this will allocate new PortConfiguration_SecurityGroupId may need to free later
  aliothcontroller::PortConfiguration_SecurityGroupId *SecurityGroup_builder =
          PortConfiguration_builder->add_security_group_ids();
  SecurityGroup_builder->set_id("1");
  // this will allocate new PortConfiguration_AllowAddressPair may need to free later
  aliothcontroller::PortConfiguration_AllowAddressPair *AddressPair_builder =
          PortConfiguration_builder->add_allow_address_pairs();
  AddressPair_builder->set_ip_address("10.0.0.5");
  AddressPair_builder->set_mac_address("fa:16:3e:d7:f2:9f");
  // this will allocate new PortConfiguration_ExtraDhcpOption may need to free later
  aliothcontroller::PortConfiguration_ExtraDhcpOption *ExtraDhcp_builder =
          PortConfiguration_builder->add_extra_dhcp_options();
  ExtraDhcp_builder->set_name("opt_1");
  ExtraDhcp_builder->set_value("12");

  // fill in the subnet state structs
  new_subnet_states->set_operation_type(aliothcontroller::OperationType::INFO);

  // this will allocate new SubnetConfiguration, will need to free it later
  aliothcontroller::SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_version(1);
  SubnetConiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
  // VpcConiguration_builder->set_id("99d9d709-8478-4b46-9f3f-2206b1023fd3");
  SubnetConiguration_builder->set_vpc_id("99d9d709-8478-4b46-9f3f-2206b1023fd3");
  SubnetConiguration_builder->set_id("superSubnetID");
  SubnetConiguration_builder->set_name("SuperSubnet");
  SubnetConiguration_builder->set_cidr("10.0.0.0/16");
  SubnetConiguration_builder->set_tunnel_id(22222);
  SubnetConiguration_builder->set_gateway_ip(gw_ip);
  // this will allocate new SubnetConfiguration_TransitSwitch, may need to free it later
  aliothcontroller::SubnetConfiguration_TransitSwitch *TransitSwitch_builder =
          SubnetConiguration_builder->add_transit_switches();
  TransitSwitch_builder->set_vpc_id(vpc_id);
  TransitSwitch_builder->set_subnet_id("superSubnet");
  TransitSwitch_builder->set_ip_address("172.0.0.1");
  TransitSwitch_builder->set_mac_address("cc:dd:ee:aa:bb:cc");

  bool previous_demo_mode = g_demo_mode;
  g_demo_mode = true;

  rc = Aca_Comm_Manager::get_instance().update_goal_state(GoalState_builder);
  ASSERT_EQ(rc, EXIT_SUCCESS);

  g_demo_mode = previous_demo_mode;

  // check to ensure the endpoint is created and setup correctly

  // the temp veth should be in the new namespace now
  cmd_string = "ip netns exec " + vpc_ns + " ip link list | grep " + temp_name_string;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // was the ip set correctly?
  cmd_string = "ip netns exec " + vpc_ns + " ifconfig " + temp_name_string +
               " | grep " + ip_address;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // was the default gw set correctly?
  cmd_string = "ip netns exec " + vpc_ns + " ip route" + " | grep " + gw_ip;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // was the mac set correctly?
  cmd_string = "ip netns exec " + vpc_ns + " ifconfig " + temp_name_string +
               " | grep " + mac_address;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // finalize the endpoint, which will rename the veth to the final name
  // ***TODO: why did would still go in as CREATE?
  aliothcontroller::PortState *final_port_states =
          GoalState_builder.mutable_port_states(0);
  final_port_states->set_operation_type(aliothcontroller::OperationType::FINALIZE);

  rc = Aca_Comm_Manager::get_instance().update_goal_state(GoalState_builder);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_subnet_states->clear_configuration();

  // check to ensure the end point has been renamed to the final name
  cmd_string = "ip netns exec " + vpc_ns + " ip link list | grep " + veth_name_string;
  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // clean up

  // delete the newly created veth pair
  cmd_string = "ip netns exec " + vpc_ns + " ip link delete " + veth_name_string;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);

  // delete the newly created ns
  cmd_string = "ip netns delete " + vpc_ns;

  rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
  EXPECT_EQ(rc, EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
  int option;

  ACA_LOG_INIT(ACALOGNAME);

  testing::InitGoogleTest(&argc, argv);

  while ((option = getopt(argc, argv, "s:p:")) != -1) {
    switch (option) {
    case 's':
      g_rpc_server = optarg;
      break;
    case 'p':
      g_rpc_protocol = optarg;
      break;
    default: /* the '?' case when the option is not recognized */
      fprintf(stderr,
              "Usage: %s\n"
              "\t\t[-s transitd RPC server]\n"
              "\t\t[-p transitd RPC protocol]\n",
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
    g_rpc_protocol = UDP;
  }

  int rc = RUN_ALL_TESTS();

  ACA_LOG_CLOSE();

  return rc;
}