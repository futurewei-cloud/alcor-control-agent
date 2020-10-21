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

using namespace aca_dhcp_server;
using namespace aca_dhcp_programming_if;

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
  dhcp_message stDhcpMsg = { 0 };

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
  dhcp_message stDhcpMsg = { 0 };

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