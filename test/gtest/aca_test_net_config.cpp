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

#include "aca_net_config.h"
#include "gtest/gtest.h"

using aca_net_config::Aca_Net_Config;

static char EMPTY_STRING[] = "";
static char VALID_STRING[] = "VALID_STRING";
static char DEFAULT_MTU[] = "9000";

extern bool g_demo_mode;

//
// Test suite: net_config_test_cases
//
// Testing network configuration helper functions
//
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