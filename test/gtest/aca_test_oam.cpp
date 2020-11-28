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
#include "aca_oam_server.h"
#include "aca_util.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <string.h>
#include "aca_ovs_control.h"
#include "aca_vlan_manager.h"

using namespace aca_oam_server;

extern string vmac_address_1;
extern string vmac_address_2;
extern string vmac_address_3;
extern string vmac_address_4;
extern string vip_address_1;
extern string vip_address_2;
extern string vip_address_3;
extern string vip_address_4;
extern string remote_ip_1; 
extern string remote_ip_2; 

alcor::schema::NetworkType network_type = alcor::schema::NetworkType::VXLAN;

string tunnel_id_1 = "555";
string tunnel_id_2 = "666";

string vlan_id_1 = "100";
string vlan_id_2 = "200";

string outport_name_1= "vx9999";

using namespace aca_ovs_control;

TEST(oam_message_test_cases, oams_recv_valid)
{
  int retcode = 0;
  oam_message stOamMsg;

  stOamMsg.op_code = 0x0;

  stOamMsg.data.msg_inject_flow.inner_src_ip.s_addr = 55;
  stOamMsg.data.msg_inject_flow.inner_dst_ip.s_addr = 66;
  stOamMsg.data.msg_inject_flow.src_port = 500;
  stOamMsg.data.msg_inject_flow.dst_port = 500;
  stOamMsg.data.msg_inject_flow.proto = 0;
  stOamMsg.data.msg_inject_flow.vni[0] = 0xaf;
  stOamMsg.data.msg_inject_flow.vni[1] = 0x02;
  stOamMsg.data.msg_inject_flow.vni[2] = 0xee;
  stOamMsg.data.msg_inject_flow.node_dst_ip.s_addr = 77;
  stOamMsg.data.msg_inject_flow.inst_dst_ip.s_addr = 88;
  stOamMsg.data.msg_inject_flow.inst_dst_mac[0] = 0x3c;
  stOamMsg.data.msg_inject_flow.inst_dst_mac[1] = 0xf0;
  stOamMsg.data.msg_inject_flow.inst_dst_mac[2] = 0x11;
  stOamMsg.data.msg_inject_flow.inst_dst_mac[3] = 0x12;
  stOamMsg.data.msg_inject_flow.inst_dst_mac[4] = 0x56;
  stOamMsg.data.msg_inject_flow.inst_dst_mac[5] = 0x65;

  retcode = ACA_Oam_Server::get_instance()._validate_oam_message(&stOamMsg);
  EXPECT_EQ(retcode, EXIT_SUCCESS);

  retcode = ACA_Oam_Server::get_instance()._get_message_type(&stOamMsg);
  EXPECT_EQ(retcode, OAM_MSG_FLOW_INJECTION);
}

TEST(oam_message_test_cases, add_direct_path_valid)
{
  int retcode = 0;
  
  oam_match match;
  oam_action action;

  match.sip = vip_address_1;
  match.dip = vip_address_2;
  match.sport = "55";
  match.dport = "77";
  match.vni = "300";
  match.proto = "6";

  action.inst_nw_dst = vip_address_3;
  action.node_nw_dst = remote_ip_1;
  action.inst_dl_dst = vmac_address_1;
  action.node_dl_dst = vmac_address_2;
  action.idle_timeout = 10;

  string vlan_id = to_string(aca_vlan_manager::ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(match.vni));

  aca_oam_server::ACA_Oam_Server::get_instance()._add_direct_path(match, action);

  string cmd = "table=55,priority=50,ip,nw_proto=" + match.proto + ",nw_src=" + match.sip + ",nw_dst=" + match.dip + ",tp_src=" + 
            match.sport + ",tp_dst=" + match.dport + ",dl_vlan=" + vlan_id;

  retcode = ACA_OVS_Control::get_instance().flow_exists("br-tun", cmd.c_str());
  EXPECT_EQ(retcode, EXIT_SUCCESS);
}

TEST(oam_message_test_cases, del_direct_path_valid)
{
  int retcode = 0;

  oam_match match;
  oam_action action;

  match.sip = vip_address_1;
  match.dip = vip_address_2;
  match.sport = "55";
  match.dport = "77";
  match.vni = "300";
  match.proto = "6";

  action.inst_nw_dst = vip_address_3;
  action.node_nw_dst = remote_ip_1;
  action.inst_dl_dst = vmac_address_1;
  action.node_dl_dst = vmac_address_2;
  action.idle_timeout = 10;

  string vlan_id = to_string(aca_vlan_manager::ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(match.vni));

  aca_oam_server::ACA_Oam_Server::get_instance()._add_direct_path(match, action);

  aca_oam_server::ACA_Oam_Server::get_instance()._del_direct_path(match);

  string cmd = "table=55,priority=50,ip,nw_proto=" + match.proto + ",nw_src=" + match.sip + ",nw_dst=" + match.dip + ",tp_src=" + 
            match.sport + ",tp_dst=" + match.dport + ",dl_vlan=" + vlan_id;

  retcode = ACA_OVS_Control::get_instance().flow_exists("br-tun", cmd.c_str());
  EXPECT_EQ(retcode, EXIT_SUCCESS);
}