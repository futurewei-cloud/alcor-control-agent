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

#include "aca_util.h"
#include "gtest/gtest.h"
#include "aca_ovs_control.h"
#include "ovs_control.h"
#include "aca_ovs_l2_programmer.h"
#include <string>

using namespace std;
using namespace aca_ovs_control;
using namespace ovs_control;
using aca_ovs_l2_programmer::ACA_OVS_L2_Programmer;

extern string vmac_address_1;
extern string vip_address_1;
extern string remote_ip_1;

//
// Test suite: ovs_flow_mod_cases
//
// Testing the openflow helper functions for add/mod/delete flows and flow_exists
//
TEST(ovs_flow_mod_cases, add_flows)
{
  int overall_rc;

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // add flow
  ACA_OVS_Control::get_instance().add_flow(
          "br-tun", "ip,nw_dst=192.168.0.1,priority=1,actions=drop");

  overall_rc = ACA_OVS_Control::get_instance().flow_exists("br-tun", "ip,nw_dst=192.168.0.1");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
}

TEST(ovs_flow_mod_cases, mod_flows)
{
  int overall_rc;

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // add flow
  ACA_OVS_Control::get_instance().add_flow(
          "br-tun", "tcp,nw_dst=192.168.0.1,priority=1,actions=drop");

  // modify flow
  ACA_OVS_Control::get_instance().mod_flows(
          "br-tun", "tcp,nw_dst=192.168.0.1,priority=1,actions=resubmit(,2)");

  overall_rc = ACA_OVS_Control::get_instance().flow_exists("br-tun", "tcp,nw_dst=192.168.0.1");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
}

TEST(ovs_flow_mod_cases, del_flows)
{
  int overall_rc;

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // add flow
  ACA_OVS_Control::get_instance().add_flow(
          "br-tun", "tcp,nw_dst=192.168.0.9,priority=1,actions=drop");

  // delete flow
  ACA_OVS_Control::get_instance().del_flows("br-tun", "tcp,nw_dst=192.168.0.9,priority=1");

  overall_rc = ACA_OVS_Control::get_instance().flow_exists("br-tun", "tcp,nw_dst=192.168.0.9");
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
}

//
// Test suite: add_delete_flows_l2_neighbor_cases
//
// Testing the openflow helper functions for add/delete flows and flow_exists related to L2 neighbors
//
TEST(ovs_flow_mod_cases, add_delete_flows_l2_neighbor)
{
  ulong not_care_culminative_time;
  int overall_rc;

  // delete br-int and br-tun bridges
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  int internal_vlan_id = 100;
  int tunnel_id = 8888;

  // match internal vlan based on VPC and destination neighbor mac,
  // strip the internal vlan, encap with tunnel id,
  // output to the neighbor host through vxlan-generic ovs port
  string match_string = "table=20,priority=50,dl_vlan=" + to_string(internal_vlan_id) +
                        ",dl_dst:" + vmac_address_1;

  string action_string = ",actions=strip_vlan,load:" + to_string(tunnel_id) +
                         "->NXM_NX_TUN_ID[],set_field:" + remote_ip_1 +
                         "->tun_dst,output:" + VXLAN_GENERIC_OUTPORT_NUMBER;

  overall_rc = ACA_OVS_Control::get_instance().add_flow(
          "br-tun", (match_string + action_string).c_str());
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  string flow_exists_match_string =
          "table=20,dl_vlan=" + to_string(internal_vlan_id) + ",dl_dst:" + vmac_address_1;

  // confirm the flow has been added
  overall_rc = ACA_OVS_Control::get_instance().flow_exists(
          "br-tun", flow_exists_match_string.c_str());
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // add the static arp responder for this l2 neighbor
  string current_virtual_mac = vmac_address_1;
  current_virtual_mac.erase(
          remove(current_virtual_mac.begin(), current_virtual_mac.end(), ':'),
          current_virtual_mac.end());

  int addr = inet_network(vip_address_1.c_str());
  char hex_ip_buffer[HEX_IP_BUFFER_SIZE];
  snprintf(hex_ip_buffer, HEX_IP_BUFFER_SIZE, "0x%08x", addr);

  string arp_match_string =
          "table=51,priority=50,arp,dl_vlan=" + to_string(internal_vlan_id) +
          ",nw_dst=" + vip_address_1;

  string arp_action_string =
          " actions=move:NXM_OF_ETH_SRC[]->NXM_OF_ETH_DST[],mod_dl_src:" + vmac_address_1 +
          ",load:0x2->NXM_OF_ARP_OP[],move:NXM_NX_ARP_SHA[]->NXM_NX_ARP_THA[],move:NXM_OF_ARP_SPA[]->NXM_OF_ARP_TPA[],load:0x" +
          current_virtual_mac + "->NXM_NX_ARP_SHA[],load:" + string(hex_ip_buffer) +
          "->NXM_OF_ARP_SPA[],in_port";

  overall_rc = ACA_OVS_Control::get_instance().add_flow(
          "br-tun", (arp_match_string + arp_action_string).c_str());
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  string flow_exists_arp_match_string =
          "table=51,arp,dl_vlan=" + to_string(internal_vlan_id) + ",nw_dst=" + vip_address_1;

  // confirm the arp flow has been added
  overall_rc = ACA_OVS_Control::get_instance().flow_exists(
          "br-tun", flow_exists_arp_match_string.c_str());
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // delete the arp flows
  overall_rc = ACA_OVS_Control::get_instance().del_flows(
          "br-tun", arp_match_string.c_str());
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // confirm the arp flow has been deleted
  overall_rc = ACA_OVS_Control::get_instance().flow_exists(
          "br-tun", flow_exists_arp_match_string.c_str());
  EXPECT_NE(overall_rc, EXIT_SUCCESS);

  // delete the l2 neighbor flows
  overall_rc = ACA_OVS_Control::get_instance().del_flows("br-tun", match_string.c_str());
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);

  // confirm the l2 neighbor flow has been deleted
  overall_rc = ACA_OVS_Control::get_instance().flow_exists(
          "br-tun", flow_exists_match_string.c_str());
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
}