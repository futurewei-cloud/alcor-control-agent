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

void ACA_Oam_Server::oams_recv(uint32_t udp_dport, void *message)
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

  (this->*_parse_oam_msg_ops[msg_type])(udp_dport, oammsg);

  return;
}

void ACA_Oam_Server::_init_oam_msg_ops()
{
  _parse_oam_msg_ops[OAM_MSG_FLOW_INJECTION] =
          &aca_oam_server::ACA_Oam_Server::_parse_oam_flow_injection;
  _parse_oam_msg_ops[OAM_MSG_FLOW_DELETION] =
          &aca_oam_server::ACA_Oam_Server::_parse_oam_flow_deletion;
  _parse_oam_msg_ops[OAM_MSG_NONE] = &aca_oam_server::ACA_Oam_Server::_parse_oam_none;
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

//extract data for flow table matching from the oam message
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

//extract the data used for the flow table action from the oam message
oam_action ACA_Oam_Server::_get_oam_action_field(oam_message *oammsg)
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
bool ACA_Oam_Server::_check_oam_server_port(uint32_t udp_dport, oam_match match)
{
  uint32_t oam_port_of_vpc;
  aca_vlan_manager::ACA_Vlan_Manager::get_instance().get_oam_server_port(
          match.vni, &oam_port_of_vpc);

  if (udp_dport == oam_port_of_vpc) {
    ACA_LOG_INFO("%s", "oam server port is correct!\n");
    return true;
  } else {
    ACA_LOG_ERROR("%s", "oam server port is incorrect!!!");
    return false;
  }
}

void ACA_Oam_Server::_parse_oam_flow_injection(uint32_t udp_dport, oam_message *oammsg)
{
  unsigned long not_care_culminative_time;
  int overall_rc;

  oam_match match = _get_oam_match_field(oammsg);

  // check whether the udp_dport is the oam server port of the vpc
  if (!_check_oam_server_port(udp_dport, match)) {
    return;
  }

  oam_action action = _get_oam_action_field(oammsg);

  string remote_host_ip = action.node_nw_dst;
  string tunnel_id = match.vni;
  alcor::schema::NetworkType network_type = alcor::schema::NetworkType::VXLAN;

  if (!aca_is_port_on_same_host(remote_host_ip)) {
    ACA_LOG_INFO("%s", "port_neighbor not exist!\n");
    string neighbor_id;
    // get netigbor_id

    //crate neighbor_port
    aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().create_or_update_l2_neighbor(
            neighbor_id, vpc_id, network_type, remote_host_ip,
            (uint)stoi(tunnel_id), not_care_culminative_time);
  }
  overall_rc = aca_oam_server::ACA_Oam_Server::_add_direct_path(match, action);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_ERROR("Command failed!!! overrall_rc: %d\n", overall_rc);
  }

  return;
}

void ACA_Oam_Server::_parse_oam_flow_deletion(uint32_t udp_dport, oam_message *oammsg)
{
  int overall_rc;
  oam_match match = _get_oam_match_field(oammsg);
  // check whether the udp_dport is the oam server port of the vpc
  if (!_check_oam_server_port(udp_dport, match)) {
    return;
  }

  overall_rc = _del_direct_path(match);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_ERROR("Command failed!!! overrall_rc: %d\n", overall_rc);
  }

  return;
}

void ACA_Oam_Server::_parse_oam_none(uint32_t /* in_port */, oam_message *oammsg)
{
  ACA_LOG_ERROR("Wrong OAM message type! (Message type = %d)\n", _get_message_type(oammsg));
  return;
}

int ACA_Oam_Server::_add_direct_path(oam_match match, oam_action action)
{
  int overall_rc;

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
  int overall_rc;
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

int ACA_Vlan_Manager::create_neighbor_outport_no_neighbor(alcor::schema::NetworkType network_type,
                                              string remote_host_ip, uint tunnel_id,
                                              ulong &culminative_time)
{
  int overall_rc;

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_neighbor_outport ---> Entering\n");

  string outport_name = aca_get_outport_name(network_type, remote_host_ip);

  // use tunnel_id to query vlan_manager to lookup an existing tunnel_id entry to get its
  // internal vlan id or to create a new tunnel_id entry to get a new internal vlan id
  int internal_vlan_id = this->get_or_create_vlan_id(tunnel_id);

  // -----critical section starts-----
  _vpcs_table_mutex.lock();

  // if the vpc entry is not there, create it first
  if (_vpcs_table.find(tunnel_id) == _vpcs_table.end()) {
    create_entry_unsafe(tunnel_id);
  }

  auto current_outports_neighbors_table = _vpcs_table[tunnel_id].outports_neighbors_table;

  if (current_outports_neighbors_table.find(outport_name) ==
      current_outports_neighbors_table.end()) {
    // outport is not there yet, need to create a new entry
    std::list<string> neighbors(1, neighbor_id);
    _vpcs_table[tunnel_id].outports_neighbors_table.emplace(outport_name, neighbors);

    // since this is a new outport, configure OVS and openflow rule
    string cmd_string =
            "--may-exist add-port br-tun " + outport_name + " -- set interface " +
            outport_name + " type=" + aca_get_network_type_string(network_type) +
            " options:df_default=true options:egress_pkt_mark=0 options:in_key=flow options:out_key=flow options:remote_ip=" +
            remote_host_ip;

    ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
            cmd_string, culminative_time, overall_rc);

    // incoming from neighbor through vxlan port (based on remote IP)
    cmd_string = "add-flow br-tun \"table=0,priority=25,in_port=\"" +
                 outport_name + "\" actions=resubmit(,4)\"";

    ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd_string, culminative_time, overall_rc);

    if (overall_rc == EXIT_SUCCESS) {
      string full_outport_list;
      this->get_outports_unsafe(tunnel_id, full_outport_list);

      // match internal vlan based on VPC, output for all outports based on the same
      // tunnel ID (multicast traffic)
      cmd_string = "add-flow br-tun \"table=22,priority=1,dl_vlan=" + to_string(internal_vlan_id) +
                   " actions=strip_vlan,load:" + to_string(tunnel_id) +
                   "->NXM_NX_TUN_ID[]," + full_outport_list + "\"";

      ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
              cmd_string, culminative_time, overall_rc);
    }
  } else {
    // else outport is already there, simply insert the neighbor id into outports_neighbors_table
    _vpcs_table[tunnel_id].outports_neighbors_table[outport_name].push_back(neighbor_id);
  }

  _vpcs_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_neighbor_outport <--- Exiting\n");

  return overall_rc;
}


} // namespace aca_oam_server
