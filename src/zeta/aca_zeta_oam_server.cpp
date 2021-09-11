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

#include "aca_zeta_oam_server.h"
#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <errno.h>
#include <sstream>
#include <iomanip>
#include "aca_util.h"
#include "aca_vlan_manager.h"
#include "aca_zeta_programming.h"
#include <math.h>

#undef OFP_ASSERT
#undef CONTAINER_OF
#undef ARRAY_SIZE
#undef ROUND_UP
#include "aca_ovs_l2_programmer.h"
//#include "aca_ovs_control.h"

using namespace std;
//using namespace aca_ovs_control;

namespace aca_zeta_oam_server
{
ACA_Zeta_Oam_Server::ACA_Zeta_Oam_Server()
{
  _init_oam_msg_ops();
}

ACA_Zeta_Oam_Server::~ACA_Zeta_Oam_Server()
{
}

ACA_Zeta_Oam_Server &ACA_Zeta_Oam_Server::get_instance()
{
  static ACA_Zeta_Oam_Server instance;
  return instance;
}

bool ACA_Zeta_Oam_Server::_validate_oam_message(oam_message *oammsg)
{
  if (!oammsg) {
    ACA_LOG_ERROR("%s", "OAM message is null!\n");
    return false;
  }

  uint op_code = ntohl(oammsg->op_code);
  if (op_code != OAM_MSG_FLOW_INJECTION && op_code != OAM_MSG_FLOW_DELETION) {
    ACA_LOG_ERROR("%s", "Invalid 'op_code' field for OAM message!\n");
    return false;
  }

  return true;
}

void ACA_Zeta_Oam_Server::oams_recv(uint udp_dport, void *message)
{
  oam_message *oammsg = nullptr;

  if (!message) {
    ACA_LOG_ERROR("%s", "OAN message is null!\n");
    return;
  }

  oammsg = (oam_message *)message;

  if (!_validate_oam_message(oammsg)) {
    ACA_LOG_ERROR("%s", "Invalid OAM message!\n");
    return;
  }

  uint8_t msg_type = (uint8_t)_get_message_type(oammsg);

  (this->*_parse_oam_msg_ops[msg_type])(udp_dport, oammsg);

  return;
}

void ACA_Zeta_Oam_Server::_init_oam_msg_ops()
{
  _parse_oam_msg_ops[OAM_MSG_FLOW_INJECTION] =
          &aca_zeta_oam_server::ACA_Zeta_Oam_Server::_parse_oam_flow_injection;
  _parse_oam_msg_ops[OAM_MSG_FLOW_DELETION] =
          &aca_zeta_oam_server::ACA_Zeta_Oam_Server::_parse_oam_flow_deletion;
  // error: array subscript is above array bounds [-Werror=array-bounds]
  // because _parse_oam_msg_ops and defined to have three element so
  // doing _parse_oam_msg_ops[OAM_MSG_NONE(3)] in invalid
  // _parse_oam_msg_ops[OAM_MSG_NONE] = &aca_zeta_oam_server::ACA_Zeta_Oam_Server::_parse_oam_none;
}

uint8_t ACA_Zeta_Oam_Server::_get_message_type(oam_message *oammsg)
{
  if (!oammsg) {
    ACA_LOG_ERROR("%s", "OAM message is null!\n");
    return OAM_MSG_NONE;
  }

  uint8_t op_code = (uint8_t)(ntohl(oammsg->op_code));

  if (op_code != OAM_MSG_FLOW_INJECTION && op_code != OAM_MSG_FLOW_DELETION) {
    return OAM_MSG_NONE;
  }

  return op_code;
}

string ACA_Zeta_Oam_Server::_get_mac_addr(uint8_t *mac)
{
  string mac_string;
  stringstream ss;

  // Convert mac address to string
  // from uint8[6] to string
  for (int i = 0; i < 6; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<unsigned int>(mac[i]);
    ss << ":";
  }
  ss >> mac_string;
  mac_string.pop_back();

  return mac_string;
}

uint ACA_Zeta_Oam_Server::_get_tunnel_id(uint8_t *vni)
{
  uint tunnel_id;

  // Convert tunnel_id to uint
  // from uint8[3] to uint
  tunnel_id = ((uint)vni[0]) << 16 | ((uint)vni[1]) << 8 | ((uint)vni[2]);

  return tunnel_id;
}

//extract data for flow table matching from the oam message
oam_match ACA_Zeta_Oam_Server::_get_oam_match_field(oam_message *oammsg)
{
  oam_match match;

  flow_inject_msg msg_data = oammsg->data.msg_inject_flow;

  match.sip = inet_ntoa(msg_data.inner_src_ip);
  match.dip = inet_ntoa(msg_data.inner_dst_ip);
  match.sport = to_string(ntohs(msg_data.src_port));
  match.dport = to_string(ntohs(msg_data.dst_port));
  match.proto = to_string(msg_data.proto);
  // TODO: figure out the conversion from 3 bytes VNI to tunnel_id
  match.vni = _get_tunnel_id(msg_data.vni);

  return match;
}

//extract the data used for the flow table action from the oam message
oam_action ACA_Zeta_Oam_Server::_get_oam_action_field(oam_message *oammsg)
{
  oam_action action;

  flow_inject_msg msg_data = oammsg->data.msg_inject_flow;

  action.inst_nw_dst = inet_ntoa(msg_data.inst_dst_ip);
  action.node_nw_dst = inet_ntoa(msg_data.node_dst_ip);
  action.inst_dl_dst = _get_mac_addr(msg_data.inst_dst_mac);
  action.node_dl_dst = _get_mac_addr(msg_data.node_dst_mac);
  action.idle_timeout = to_string(msg_data.idle_timeout);

  return action;
}

//check whether the udp_dport is the oam server port of the vpc
bool ACA_Zeta_Oam_Server::_check_oam_server_port(uint udp_dport, oam_match match)
{
  uint tunnel_id = match.vni;
  string zeta_gateway_id =
          aca_vlan_manager::ACA_Vlan_Manager::get_instance().get_zeta_gateway_id(tunnel_id);

  uint oam_port = aca_zeta_programming::ACA_Zeta_Programming::get_instance().get_oam_port(
          zeta_gateway_id);

  if (udp_dport == oam_port) {
    ACA_LOG_INFO("%s", "oam port is correct!\n");
    return true;
  } else {
    ACA_LOG_ERROR("%s", "oam port is incorrect!!!");
    return false;
  }
}

void ACA_Zeta_Oam_Server::_parse_oam_flow_injection(uint udp_dport, oam_message *oammsg)
{
  int overall_rc;

  oam_match match = _get_oam_match_field(oammsg);

  // check whether the udp_dport is the oam server port of the vpc
  if (!_check_oam_server_port(udp_dport, match)) {
    return;
  }

  oam_action action = _get_oam_action_field(oammsg);

  overall_rc = _add_direct_path(match, action);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Flow injection succeeded!\n");
  } else {
    ACA_LOG_ERROR("Flow injection failed!!! overrall_rc: %d\n", overall_rc);
  }

  return;
}

void ACA_Zeta_Oam_Server::_parse_oam_flow_deletion(uint udp_dport, oam_message *oammsg)
{
  int overall_rc;
  oam_match match = _get_oam_match_field(oammsg);
  // check whether the udp_dport is the oam server port of the vpc
  if (!_check_oam_server_port(udp_dport, match)) {
    return;
  }

  overall_rc = _del_direct_path(match);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Flow deletion succeeded!\n");
  } else {
    ACA_LOG_ERROR("Flow_deletion failed!!! overrall_rc: %d\n", overall_rc);
  }

  return;
}

void ACA_Zeta_Oam_Server::_parse_oam_none(uint /* in_port */, oam_message *oammsg)
{
  ACA_LOG_ERROR("Wrong OAM message type! (Message type = %d)\n", _get_message_type(oammsg));
  return;
}

int ACA_Zeta_Oam_Server::_add_direct_path(oam_match match, oam_action action)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  string vlan_id = to_string(aca_vlan_manager::ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(
          match.vni));

  string source_port_cmd = "";

  string destination_port_cmd = "";

  if (match.sport != "0") {
    destination_port_cmd = ",tp_src=" + match.sport;
  }

  if (match.dport != "0") {
    source_port_cmd = ",tp_dst=" + match.dport;
  }

  string cmd_match = "ip,nw_proto=" + match.proto + ",nw_src=" + match.sip +
                     ",nw_dst=" + match.dip + source_port_cmd +
                     destination_port_cmd + ",dl_vlan=" + vlan_id;
  string cmd_action = ",actions=\"strip_vlan,load:" + to_string(match.vni) +
                      "->NXM_NX_TUN_ID[],set_field:" + action.node_nw_dst +
                      "->tun_dst,mod_dl_dst=" + action.inst_dl_dst +
                      ",mod_nw_dst=" + action.inst_nw_dst + ",output:vxlan-generic\"";

  // Adding unicast rules in table20
  string opt = "add-flow br-tun table=20,priority=50,idle_timeout=" + action.idle_timeout +
               "," + cmd_match + cmd_action;
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          opt, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Add direct path succeeded!\n");
  } else {
    ACA_LOG_ERROR("Add direct path failed!!! overrall_rc: %d\n", overall_rc);
  }

  return overall_rc;
}

int ACA_Zeta_Oam_Server::_del_direct_path(oam_match match)
{
  unsigned long not_care_culminative_time;
  int overall_rc;
  string vlan_id = to_string(aca_vlan_manager::ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(
          match.vni));

  string opt = "del-flows br-tun \"table=20,priority=50,ip,nw_proto=" + match.proto +
               ",nw_src=" + match.sip + ",nw_dst=" + match.dip +
               ",tp_src=" + match.sport + ",tp_dst=" + match.dport + ",dl_vlan=" + vlan_id + "\" --strict";

  // delete flow
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          opt, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Delete direct path succeeded!\n");
  } else {
    ACA_LOG_ERROR("Delete direct path failed!!! overrall_rc: %d\n", overall_rc);
  }

  return overall_rc;
}

} // namespace aca_zeta_oam_server
