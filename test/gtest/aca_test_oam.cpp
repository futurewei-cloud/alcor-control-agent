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

#include "gtest/gtest.h"
#include "goalstateprovisioner.grpc.pb.h"
#define private public
#include "aca_zeta_oam_server.h"
#include "aca_util.h"
#include <string.h>
#include "aca_vlan_manager.h"
#include "aca_zeta_programming.h"
#include "aca_ovs_l2_programmer.h"

#undef OFP_ASSERT
#undef CONTAINER_OF
#undef ARRAY_SIZE
#undef ROUND_UP
#include "aca_ovs_control.h"

using namespace aca_zeta_oam_server;
using namespace aca_ovs_control;
using namespace aca_zeta_programming;
using namespace aca_ovs_l2_programmer;
using namespace alcor::schema;

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

extern string auxGateway_id_1;
extern string auxGateway_id_2;

extern uint tunnel_id_1;
extern uint tunnel_id_2;
extern uint oam_port_1;
extern uint oam_port_2;

TEST(oam_message_test_cases, oams_recv_valid)
{
  int retcode = 0;
  ulong not_care_culminative_time = 0;
  int overall_rc;

  // delete and add br-int and br-tun bridges to clear everything
  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-int", not_care_culminative_time, overall_rc);

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          "del-br br-tun", not_care_culminative_time, overall_rc);

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  oam_message stOamMsg;

  stOamMsg.data.msg_inject_flow.inner_src_ip.s_addr = 55; // 55.0.0.0
  stOamMsg.data.msg_inject_flow.inner_dst_ip.s_addr = 66; // 66.0.0.0
  stOamMsg.data.msg_inject_flow.src_port = 500; // 62465
  stOamMsg.data.msg_inject_flow.dst_port = 500; // 62455
  stOamMsg.data.msg_inject_flow.proto = 0;
  // fill VNI: 197295
  stOamMsg.data.msg_inject_flow.vni[0] = 0xaf;
  stOamMsg.data.msg_inject_flow.vni[1] = 0x02;
  stOamMsg.data.msg_inject_flow.vni[2] = 0x03;
  stOamMsg.data.msg_inject_flow.node_dst_ip.s_addr = 77;
  stOamMsg.data.msg_inject_flow.inst_dst_ip.s_addr = 88;
  stOamMsg.data.msg_inject_flow.inst_dst_mac[0] = 0x3c;
  stOamMsg.data.msg_inject_flow.inst_dst_mac[1] = 0xf0;
  stOamMsg.data.msg_inject_flow.inst_dst_mac[2] = 0x11;
  stOamMsg.data.msg_inject_flow.inst_dst_mac[3] = 0x12;
  stOamMsg.data.msg_inject_flow.inst_dst_mac[4] = 0x56;
  stOamMsg.data.msg_inject_flow.inst_dst_mac[5] = 0x65;
  stOamMsg.data.msg_inject_flow.idle_timeout = 120;

  oam_match match = ACA_Zeta_Oam_Server::get_instance()._get_oam_match_field(&stOamMsg);

  // fill vpc_tables
  aca_vlan_manager::ACA_Vlan_Manager::get_instance().create_entry(match.vni);
  aca_vlan_manager::ACA_Vlan_Manager::get_instance().set_zeta_gateway(match.vni, auxGateway_id_1);

  // fill in auxgateway state structs
  GatewayConfiguration new_auxGateway;
  new_auxGateway.set_id(auxGateway_id_1);
  GatewayConfiguration_zeta *zeta_info = new_auxGateway.mutable_zeta_info();
  zeta_info->set_port_inband_operation(oam_port_1); //port_ibo

  // fill zeta_config_table
  ACA_Zeta_Programming::get_instance().create_entry(auxGateway_id_1, oam_port_1, new_auxGateway);

  // flow injection
  stOamMsg.op_code = htonl(0);
  ACA_Zeta_Oam_Server::get_instance().oams_recv(oam_port_1, &stOamMsg);
  retcode = ACA_Zeta_Oam_Server::get_instance()._get_message_type(&stOamMsg);
  EXPECT_EQ(retcode, OAM_MSG_FLOW_INJECTION);

  // flwo deletion
  stOamMsg.op_code = htonl(1);
  ACA_Zeta_Oam_Server::get_instance().oams_recv(oam_port_1, &stOamMsg);
  retcode = ACA_Zeta_Oam_Server::get_instance()._get_message_type(&stOamMsg);
  EXPECT_EQ(retcode, OAM_MSG_FLOW_DELETION);

  retcode = ACA_Zeta_Oam_Server::get_instance()._validate_oam_message(&stOamMsg);
  EXPECT_EQ(retcode, true);

  ACA_Zeta_Programming::get_instance().clear_all_data();
  aca_vlan_manager::ACA_Vlan_Manager::get_instance().clear_all_data();
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
  match.vni = 300;
  match.proto = "6";

  action.inst_nw_dst = vip_address_3;
  action.node_nw_dst = remote_ip_1;
  action.inst_dl_dst = vmac_address_1;
  action.node_dl_dst = vmac_address_2;
  action.idle_timeout = "120";

  string vlan_id = to_string(aca_vlan_manager::ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(
          match.vni));

  // add unicast rule
  ACA_Zeta_Oam_Server::get_instance()._add_direct_path(match, action);

  string cmd = "table=20,ip,nw_proto=" + match.proto + ",nw_src=" + match.sip +
               ",nw_dst=" + match.dip + ",tp_src=" + match.sport +
               ",tp_dst=" + match.dport + ",dl_vlan=" + vlan_id;

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
  match.vni = 400;
  match.proto = "6";

  action.inst_nw_dst = vip_address_3;
  action.node_nw_dst = remote_ip_2;
  action.inst_dl_dst = vmac_address_1;
  action.node_dl_dst = vmac_address_2;
  action.idle_timeout = "120";

  string vlan_id = to_string(aca_vlan_manager::ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(
          match.vni));

  // add unicast rule
  ACA_Zeta_Oam_Server::get_instance()._add_direct_path(match, action);

  ACA_Zeta_Oam_Server::get_instance()._del_direct_path(match);

  string cmd = "table=20,ip,nw_proto=" + match.proto + ",nw_src=" + match.sip +
               ",nw_dst=" + match.dip + ",tp_src=" + match.sport +
               ",tp_dst=" + match.dport + ",dl_vlan=" + vlan_id;

  retcode = ACA_OVS_Control::get_instance().flow_exists("br-tun", cmd.c_str());
  EXPECT_EQ(retcode, EXIT_FAILURE);
}
