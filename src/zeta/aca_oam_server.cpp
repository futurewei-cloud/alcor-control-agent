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

namespace aca_oam_server
{
ACA_Oam_Server::ACA_Oam_Server()
{
  _init_oam_msg_ops();
  _init_oam_ofp();
}

ACA_Oam_Server::~ACA_Oam_Server()
{
  _deinit_oam_ofp();
}

void ACA_Oam_Server::_init_oam_ofp()
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  // adding oam default flows
  string cmd = "add-flow br-int \"table=0,priority=25,udp,udp_dst=" + to_string(OAM_SOCKET_PORT) + 
          ",actions=CONTROLLER\"";

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd,not_care_culminative_time, overall_rc);
  return;
}

void ACA_Oam_Server::_deinit_oam_ofp()
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  string cmd = "del-flows br-int \"udp,udp_dst=" + to_string(OAM_SOCKET_PORT) + "\"";

  // deleting oam default flows
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd, not_care_culminative_time, overall_rc);
  return;
}

ACA_Oam_Server &ACA_Oam_Server::get_instance(){
  static ACA_Oam_Server instance;
  return instance;
};

// void ACA_Oam_Server::parse_msg_to_oam(void *message, oam_message *oammsg)
// {
//     //
    
// }

void ACA_Oam_Server::oams_recv(uint32_t in_port, void *message)
{
  oam_message *oammsg = nullptr;

  if (!message) {
    ACA_LOG_ERROR("OAM message is null!\n");
    return;
  }

  oammsg = (oam_message *)message;

  //parse_msg_to_oam( message, oammsg);

  // if (_validate_oam_message(oammsg)) {
  //   ACA_LOG_ERROR("Invalid OAM message!\n");
  //   return;
  // }
  
  uint8_t msg_type = (uint8_t)_get_message_type(oammsg);

  (this->*_parse_oam_msg_ops[msg_type])(in_port, oammsg);

  return;
};


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

  return (uint8_t)oammsg->op_code;
};

void ACA_Oam_Server::_standardize_mac_address(string &mac_string)
{
  // standardize the mac address to aa:bb:cc:dd:ee:ff
  std::transform(mac_string.begin(), mac_string.end(), mac_string.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  std::replace(mac_string.begin(), mac_string.end(), '-', ':');
}

string ACA_Oam_Server::get_tunnel_id(uint8_t *vni){
  string vpc_id;
  stringstream ss;

  for(int i = 0; i < 6; i++){
    ss <<  std::hex << std::setw(2) << std::setfill('0') << std::setbase(16)
         << static_cast<unsigned int>(vni[i]);
  }

  ss >> vpc_id;
  vpc_id.pop_back();

  return vpc_id;
}

string ACA_Oam_Server::get_vpc_id(uint8_t *vni){
  string vpc_id;
  stringstream ss;

  for(int i = 0; i < 6; i++){
    ss << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<unsigned int>(vni[i]);
  }

  ss >> vpc_id;
  vpc_id.pop_back();

  return vpc_id;
}

void ACA_Oam_Server::_parse_oam_flow_injection(uint32_t in_port, oam_message *oammsg)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  flow_inject_msg opdata = oammsg->data.msg_inject_flow;
  string remote_host_ip = inet_ntoa(opdata.node_dst_ip);
  string vpc_id = get_tunnel_id(opdata.vni);
  string tunnel_id = get_tunnel_id(opdata.vni);

  string inst_dst_mac = _standardize_mac_address(const_cast<string*>(ether_ntoa(opdata.inst_dst_mac)));

  alcor::schema::NetworkType network_type = alcor::schema::NetworkType::VXLAN;

  if(!aca_is_port_on_same_host(remote_host_ip)){
      ACA_LOG_INFO("port_neighbor not exist!\n");
      aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().create_update_neighbor_port(vpc_id,
                                                       network_type,
                                                       remote_host_ip,
                                                       (uint)stoi(vpc_id), 
                                                       not_care_culminative_time);
  }

  string vlan_id = to_string(aca_vlan_manager::ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(vpc_id));


  string outport_name = aca_get_outport_name(alcor::schema::NetworkType::VXLAN, remote_host_ip);

  string cmd_packet_in = "add-flow br-tun table=4,priority=1,tun_id=0x" + tunnel_id +
           ",actions=\"mod_vlan_vid:"+ vlan_id +",output:\"patch-int\"";

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd_packet_in, not_care_culminative_time, overall_rc);  

  overall_rc = aca_oam_server::ACA_Oam_Server::add_direct_path(network_type, 
            inst_dst_mac, vlan_id, tunnel_id, outport_name);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_DEBUG("Command failed!!! overrall_rc: %d\n", overall_rc);
  }

  return;
}

void ACA_Oam_Server::_parse_oam_flow_deletion(uint32_t in_port, oam_message *oammsg)
{
  int overall_rc = EXIT_SUCCESS;
  flow_del_msg opdata = oammsg->data.msg_del_flow;
  string vpc_id = get_vpc_id(opdata.vni);
  string vlan_id = to_string(aca_vlan_manager::ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(vpc_id));

  string inst_dst_mac = _standardize_mac_address(const_cast<string>(ether_ntoa(opdata.inst_dst_mac)));

  overall_rc = del_direct_path(inst_dst_mac, vlan_id);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_DEBUG("Command failed!!! overrall_rc: %d\n", overall_rc);
  }

  return;
}

int ACA_Oam_Server::add_direct_path(alcor::schema::NetworkType network_type, const string inst_dst_mac,
      const string vlan_id, const string tunnel_id, const string outport_name)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  string cmd_string = "add-flow br-tun table=0,priority=1,dl_vlan="
            + vlan_id + "dl_dst=" + inst_dst_mac + ",actions=\"strip_vlan,load:" + tunnel_id 
            + "->NXM_NX_TUN_ID[],output:" + outport_name + "\"";

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd_string, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_DEBUG("Command failed!!! overrall_rc: %d\n", overall_rc);
  }

  return overall_rc;
};

int del_direct_path(const string inst_dst_mac, const string vlan_id)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  string cmd = "del-flows br-tun table=0,in_port=\"patch-int\",dl_vlan="
            + vlan_id + "dl_dst=" + inst_dst_mac + ",priority=2";

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
        cmd, not_care_culminative_time, overall_rc);
  
  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_DEBUG("Command failed!!! overrall_rc: %d\n", overall_rc);
  }

  return overall_rc;
};
}