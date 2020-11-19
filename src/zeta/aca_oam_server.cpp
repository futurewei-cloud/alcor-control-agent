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

#include "aca_oam_server.h"
#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <errno.h>
#include <sstream>
#include <iomanip>
#include "aca_ovs_l2_programmer.h"
#include "aca_util.h"
#include "aca_ovs_control.h"
#include "aca_vlan_manager.h"

using namespace std;
using namespace aca_ovs_control;

namespace aca_oam_server
{
ACA_Oam_Server::ACA_Oam_Server()
{
  _init_oam_msg_ops();
}

ACA_Oam_Server::~ACA_Oam_Server()
{
}

ACA_Oam_Server &ACA_Oam_Server::get_instance()
{
  static ACA_Oam_Server instance;
  return instance;
}

bool ACA_Oam_Server::_validate_oam_message(oam_message *oammsg)
{
  int retcode = 0;

  if (!oammsg) {
    ACA_LOG_ERROR("%s", "OAM message is null!\n");
    return false;
  }

  if (OAM_MSG_FLOW_INJECTION != oammsg->op_code || OAM_MSG_FLOW_DELETION != oammsg->op_code) {
    retcode = -1;
    ACA_LOG_ERROR("%s", "Invalid 'op_code' field for OAM message!\n");
  }

  if (0 != retcode) {
    return false;
  }

  return true;
}

void ACA_Oam_Server::oams_recv(void *message)
{
  oam_message *oammsg = nullptr;

  if (!message) {
    ACA_LOG_ERROR("%s", "DHCP message is null!\n");
    return;
  }

  oammsg = (oam_message *)message;

  if (_validate_oam_message(oammsg)) {
    ACA_LOG_ERROR("%s", "Invalid OAM message!\n");
    return;
  }

  uint8_t msg_type = (uint8_t)_get_message_type(oammsg);

  (this->*_parse_oam_msg_ops[msg_type])(oammsg);

  return;
}

void ACA_Oam_Server::_init_oam_msg_ops()
{
  _parse_oam_msg_ops[OAM_MSG_FLOW_INJECTION] =
          &aca_oam_server::ACA_Oam_Server::_parse_oam_flow_injection;
  _parse_oam_msg_ops[OAM_MSG_FLOW_DELETION] =
          &aca_oam_server::ACA_Oam_Server::_parse_oam_flow_deletion;
}

uint8_t ACA_Oam_Server::_get_message_type(oam_message *oammsg)
{
  if (!oammsg) {
    ACA_LOG_ERROR("%s", "OAM message is null!\n");
    return OAM_MSG_NONE;
  }

  if (!oammsg->op_code) {
    return OAM_MSG_NONE;
  }

  return (uint8_t)(ntohl(oammsg->op_code));
}

string ACA_Oam_Server::_get_mac_addr(uint8_t *mac)
{
  string mac_string;
  stringstream ss;

  //Convert mac address to string
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

void ACA_Oam_Server::_standardize_mac_address(string &mac_string)
{
  // standardize the mac address to aa:bb:cc:dd:ee:ff
  std::transform(mac_string.begin(), mac_string.end(), mac_string.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  std::replace(mac_string.begin(), mac_string.end(), '-', ':');
}

string ACA_Oam_Server::_get_vpc_id(uint8_t *vni)
{
  string vpc_id;
  stringstream ss;

  for (int i = 0; i < 3; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<unsigned int>(vni[i]);
  }

  ss >> vpc_id;
  vpc_id.pop_back();

  return vpc_id;
}

oam_match ACA_Oam_Server::_get_oam_match_field(oam_message *oammsg)
{
  oam_match match;

  flow_inject_msg msg_data = oammsg->data.msg_inject_flow;

  match.sip = inet_ntoa(msg_data.inner_src_ip);
  match.dip = inet_ntoa(msg_data.inner_dst_ip);
  match.sport = to_string(ntohs(msg_data.src_port));
  match.dport = to_string(ntohs(msg_data.dst_port));
  match.proto = to_string(msg_data.proto);
  match.vni = _get_vpc_id(msg_data.vni);

  return match;
}

oam_action ACA_Oam_Server::_get_oam_action_field(oam_message *oammsg)
{
  oam_action action;

  flow_inject_msg msg_data = oammsg->data.msg_inject_flow;

  action.inst_nw_dst = inet_ntoa(msg_data.inst_dst_ip);
  action.node_nw_dst = inet_ntoa(msg_data.node_dst_ip);
  action.inst_dl_dst = _get_mac_addr(msg_data.inst_dst_mac);
  _standardize_mac_address(action.inst_dl_dst);
  action.node_dl_dst = _get_mac_addr(msg_data.node_dst_mac);
  _standardize_mac_address(action.node_dl_dst);
  action.idle_timeout = to_string(msg_data.idle_timeout);

  return action;
}

void ACA_Oam_Server::_parse_oam_flow_injection(oam_message *oammsg)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  oam_match match = _get_oam_match_field(oammsg);
  oam_action action = _get_oam_action_field(oammsg);

  string remote_host_ip = action.node_nw_dst;
  string vpc_id = match.vni;
  alcor::schema::NetworkType network_type = alcor::schema::NetworkType::VXLAN;

  if (!aca_is_port_on_same_host(remote_host_ip)) {
    ACA_LOG_INFO("%s", "port_neighbor not exist!\n");
    string neighbor_id;
    // get netigbor_id

    //crate neighbor_port
    aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().create_or_update_neighbor_port(
            neighbor_id, vpc_id, network_type, remote_host_ip,
            (uint)stoi(vpc_id), not_care_culminative_time);
  }
  overall_rc = aca_oam_server::ACA_Oam_Server::_add_direct_path(match, action);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_ERROR("Command failed!!! overrall_rc: %d\n", overall_rc);
  }

  return;
}

void ACA_Oam_Server::_parse_oam_flow_deletion(oam_message *oammsg)
{
  int overall_rc = EXIT_SUCCESS;
  oam_match match = _get_oam_match_field(oammsg);

  overall_rc = _del_direct_path(match);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_ERROR("Command failed!!! overrall_rc: %d\n", overall_rc);
  }

  return;
}

int ACA_Oam_Server::_add_direct_path(oam_match match, oam_action action)
{
  int overall_rc = EXIT_SUCCESS;

  string vlan_id = to_string(aca_vlan_manager::ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(
          match.vni));
  string outport_name =
          aca_get_outport_name(alcor::schema::NetworkType::VXLAN, action.node_nw_dst);

  string cmd_match = "ip,nw_proto=" + match.proto + ",nw_src=" + match.sip +
                     ",nw_dst=" + match.dip + ",tp_src=" + match.sport +
                     ",tp_dst=" + match.dport + ",dl_vlan=" + vlan_id;
  string cmd_action = "action=\"strip_vlan,load:" + match.vni +
                      "->NXM_NX_TUN_ID[],mod_dl_dst=" + action.inst_dl_dst +
                      ",mod_nw_dst=" + action.inst_nw_dst +
                      ",idle_timeout=" + action.idle_timeout + ",output:" + outport_name;

  // Adding unicast rules in table20
  string opt = "table=20,priority=50," + cmd_match + "," + cmd_action;
  overall_rc = ACA_OVS_Control::get_instance().add_flow("br-tun", opt.c_str());

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Add direct path succeeded!\n");
  } else {
    ACA_LOG_ERROR("Add direct path failed!!! overrall_rc: %d\n", overall_rc);
  }

  return overall_rc;
}

int ACA_Oam_Server::_del_direct_path(oam_match match)
{
  int overall_rc = EXIT_SUCCESS;
  string vlan_id = to_string(aca_vlan_manager::ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(
          match.vni));

  string opt = "table=20,priority=50,ip,nw_proto=" + match.proto +
               ",nw_src=" + match.sip + ",nw_dst=" + match.dip +
               ",tp_src=" + match.sport + ",tp_dst=" + match.dport + ",dl_vlan=" + vlan_id;

  // delete flow
  overall_rc = ACA_OVS_Control::get_instance().del_flows("br-tun", opt.c_str());

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Delete direct path succeeded!\n");
  } else {
    ACA_LOG_ERROR("Delete direct path failed!!! overrall_rc: %d\n", overall_rc);
  }

  return overall_rc;
}

} // namespace aca_oam_server
