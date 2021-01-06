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

#include "aca_arp_responder.h"
#include "aca_log.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_ovs_control.h"
#include "aca_util.h"
#include <shared_mutex>
#include <arpa/inet.h>

using namespace std;

namespace aca_arp_responder
{
ACA_ARP_Responder::ACA_ARP_Responder(){
  _init_arp_db();
  _init_arp_ofp();
}

ACA_ARP_Responder::~ACA_ARP_Responder(){
  _deinit_arp_db();
  _deinit_arp_ofp();
}

void ACA_ARP_Responder::_init_arp_db(){
  _arp_db.clear();
}

void ACA_ARP_Responder::_deinit_arp_db(){
  _arp_db.clear();
}

void ACA_ARP_Responder::_init_arp_ofp(){
  int overall_rc = EXIT_SUCCESS;
  unsigned long not_care_culminative_time;

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
    "add-flow br-tun \"table=0,priority=50,arp,arp_op=1, actions=CONTROLLER\"",
    not_care_culminative_time, overall_rc);
  return;
}

void ACA_ARP_Responder::_deinit_arp_ofp(){
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          "del-flows br-tun \"arp,arp_op=1\"",
          not_care_culminative_time, overall_rc);
  
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          "add-flow br-tun \"table=2,priority=25,arp,arp_op=1,in_port=\"patch-int\" actions=resubmit(,51)\"",
          not_care_culminative_time, overall_rc);

  return;  
}

ACA_ARP_Responder &ACA_ARP_Responder::get_instance(){
  static ACA_ARP_Responder instance;
  return instance;
}


int ACA_ARP_Responder::add_arp_entry(arp_config *arp_cfg_in){

  arp_entry_data stData;
  arp_table_data *current_arp_data = new arp_table_data;

  try{
    _validate_arp_entry(arp_cfg_in);

    ARP_ENTRY_DATA_SET((arp_entry_data *)&stData, arp_cfg_in);
    ARP_TABLE_DATA_SET(current_arp_data, arp_cfg_in);

    if (_arp_db.find(stData,current_arp_data)) {
      ACA_LOG_ERROR("Entry already existed! (ip = %s and vlan id = %u)\n",
                    arp_cfg_in->ipv4_address.c_str(), arp_cfg_in->vlan_id);
      return EXIT_FAILURE;
    }

    _arp_db.insert(stData,current_arp_data);

    ACA_LOG_DEBUG("Arp Entry with ip: %s and vlan id %u added\n",arp_cfg_in->ipv4_address.c_str(),arp_cfg_in->vlan_id);

    return EXIT_SUCCESS;
  }
  catch(std::invalid_argument &ia){
    ACA_LOG_ERROR("%s,validate arp config failed! (ip = %s and vlan id = %u)\n",
                  ia.what(),arp_cfg_in->ipv4_address.c_str(),arp_cfg_in->vlan_id);
    return EXIT_FAILURE;
  }
}  

int ACA_ARP_Responder::create_or_update_arp_entry(arp_config *arp_cfg_in){
  arp_entry_data stData;
  arp_table_data *current_arp_data = new arp_table_data;

  try{
    _validate_arp_entry(arp_cfg_in);
    ARP_ENTRY_DATA_SET((arp_entry_data *)&stData, arp_cfg_in);
    ARP_TABLE_DATA_SET(current_arp_data, arp_cfg_in);

    if(!_arp_db.find(stData,current_arp_data)){
      ACA_LOG_DEBUG("Entry not exist! (ip = %s and vlan id = %u)\n",
                    arp_cfg_in->ipv4_address.c_str(),arp_cfg_in->vlan_id);    
      add_arp_entry(arp_cfg_in);
    }
    else{   
      current_arp_data->mac_address = arp_cfg_in->mac_address;
    }
    return EXIT_SUCCESS;
  }
  catch(std::invalid_argument &ia){
    ACA_LOG_ERROR("%s,validate arp config failed! (ip = %s and vlan id = %u)\n",
                  ia.what(),arp_cfg_in->ipv4_address.c_str(),arp_cfg_in->vlan_id);
    return EXIT_FAILURE;
  }

  

}
int ACA_ARP_Responder::delete_arp_entry(arp_config *arp_cfg_in){
  arp_entry_data stData;
  arp_table_data *current_arp_data = new arp_table_data;

  try{
    _validate_arp_entry(arp_cfg_in);
    ARP_ENTRY_DATA_SET((arp_entry_data *)&stData, arp_cfg_in);
    ARP_TABLE_DATA_SET(current_arp_data, arp_cfg_in);

    if (!_arp_db.find(stData,current_arp_data)) {
      ACA_LOG_DEBUG("Entry not exist! (ip = %s and vlan id = %u)\n",
                    arp_cfg_in->ipv4_address.c_str(),arp_cfg_in->vlan_id);
      return EXIT_SUCCESS;
    }
    _arp_db.erase(stData);
    return EXIT_SUCCESS;
  }
  catch(std::invalid_argument &ia){
    ACA_LOG_ERROR("%s,validate arp config failed! (ip = %s and vlan id = %u)\n",
                  ia.what(),arp_cfg_in->ipv4_address.c_str(),arp_cfg_in->vlan_id);
    return EXIT_FAILURE;
  }
}


void ACA_ARP_Responder::_validate_ipv4_address(const char *ip_address)
{
  struct sockaddr_in sa;

  // inet_pton returns 1 for success 0 for failure
  if (inet_pton(AF_INET, ip_address, &(sa.sin_addr)) != 1) {
    throw std::invalid_argument("Virtual ipv4 address is not in the expect format");
  }
}

void ACA_ARP_Responder::_validate_ipv6_address(const char *ip_address)
{
  struct sockaddr_in sa;

  // inet_pton returns 1 for success 0 for failure
  if (inet_pton(AF_INET6, ip_address, &(sa.sin_addr)) != 1) {
    throw std::invalid_argument("Virtual ipv6 address is not in the expect format");
  }
}

int ACA_ARP_Responder::_validate_arp_entry(arp_config *arp_cfg_in)
{
  if (0 >= arp_cfg_in->mac_address.size()) {
    throw std::invalid_argument("Input mac_string is null");
  }

  if(!aca_validate_mac_address(arp_cfg_in->mac_address.c_str())){
    throw std::invalid_argument("Virtual mac address is not in the expect format");
  }

  if (0 < arp_cfg_in->ipv4_address.size()) {
    _validate_ipv4_address(arp_cfg_in->ipv4_address.c_str());
  }

  if (0 < arp_cfg_in->ipv6_address.size()) {
    _validate_ipv6_address(arp_cfg_in->ipv4_address.c_str());
  }

  return EXIT_SUCCESS;
}




/************* Operation and procedure for dataplane *******************/


void ACA_ARP_Responder::arp_recv(uint32_t in_port, void *vlan_hdr, void *message){
  arp_message *arpmsg = nullptr;
  vlan_message *vlanmsg = nullptr;

  ACA_LOG_DEBUG("Receiving arp message from inport=%u\n",in_port);
  if (!message) {
    ACA_LOG_ERROR("%s", "ARP message is null!\n");
    return;
  }

  vlanmsg = (vlan_message *)vlan_hdr; 
  arpmsg = (arp_message *)message;


  if(_validate_arp_message(arpmsg)){
    ACA_LOG_ERROR("%s", "Invalid APR message!\n");
    return;
  }

  _parse_arp_request(in_port, vlanmsg, arpmsg);

  return;

}
void ACA_ARP_Responder::arp_xmit(uint32_t in_port, void  *vlanmsg, void *message){
  arp_message *arpmsg = nullptr;
  string bridge = "br-tun";
  string inport = "in_port=controller";
  string whitespace = " ";
  string action = "actions=output:" + to_string(in_port);
  string packetpre = "packet=";
  string packet;
  string options;

  arpmsg = (arp_message *)message;
  if(!arpmsg){
    ACA_LOG_ERROR("%s","ARP Reply is null!\n");
    return;
  }

  packet = _serialize_arp_message((vlan_message *)vlanmsg, arpmsg);
  if(packet.empty()){
    ACA_LOG_ERROR("%s","Serialized ARP Reply is null!\n");
    return;
  }

  options = inport + whitespace + packetpre + packet +whitespace + action;

  aca_ovs_control::ACA_OVS_Control::get_instance().packet_out(bridge.c_str(),options.c_str());

  delete arpmsg;
}

void ACA_ARP_Responder::_parse_arp_request(uint32_t in_port, vlan_message *vlanmsg, arp_message *arpmsg){
  arp_entry_data stData;
  arp_table_data *current_arp_data = new arp_table_data;
  arp_message *arpreply = nullptr;

  // get the ip address from arp message
  stData.ipv4_address = _get_requested_ip(arpmsg);

  // get the vlan id from vlan header
  if(vlanmsg){
    stData.vlan_id = ntohs(vlanmsg->vlan_tci) & 0x0111; 
  }
  else{
    stData.vlan_id = 0;
  }
  
  //find the corresponding mac address in the db based on ip and vlan id
  if(!_arp_db.find(stData,current_arp_data)){
    ACA_LOG_DEBUG("ARP entry does not exist! (ip = %s and vlan id = %u)\n",
                  stData.ipv4_address.c_str(),stData.vlan_id);
    //TO DO
  }
  else{
    ACA_LOG_DEBUG("ARP entry exist (ip = %s and vlan id = %u) with mac = %s\n",
                  stData.ipv4_address.c_str(),stData.vlan_id,current_arp_data->mac_address.c_str());
    arpreply = _pack_arp_reply(arpmsg,current_arp_data->mac_address);
  }
  if(!arpreply){
    return;
  }

  arp_xmit(in_port, vlanmsg, arpreply);
}

arp_message *ACA_ARP_Responder::_pack_arp_reply(arp_message *arpreq, string mac_address){
  arp_message *arpreply = nullptr;
  arpreply = new arp_message();
  unsigned int tmp_mac[6];

  //construct arp reply form arp request and mac address in the db
  arpreply->hrd = arpreq->hrd;
  arpreply->pro = arpreq->pro;
  arpreply->hln = arpreq->hln;
  arpreply->pln = arpreq->pln;
  arpreply->op = htons(2);
  memcpy(arpreply->tha,arpreq->sha,6);
  arpreply->spa = arpreq->tpa;
  sscanf(mac_address.c_str(),"%02x:%02x:%02x:%02x:%02x:%02x",
        tmp_mac,tmp_mac+1,tmp_mac+2,tmp_mac+3,tmp_mac+4,tmp_mac+5);
  for(int i = 0; i < 6; i++){
    arpreply->sha[i] = tmp_mac[i];
  }
  arpreply->tpa = arpreq->spa;
  
  return arpreply;
}


int ACA_ARP_Responder::_validate_arp_message(arp_message *arpmsg){
  if(!arpmsg){
    ACA_LOG_ERROR("%s", "ARP message is null!\n");
    return EXIT_FAILURE;
  }

  if(ntohs(arpmsg->op) != 1){
    ACA_LOG_ERROR("%s", "ARP message is not a ARP request!\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

string ACA_ARP_Responder::_get_requested_ip(arp_message *arpmsg){
  string requested_ip;
  struct in_addr inaddr;
  if(!arpmsg){
    ACA_LOG_ERROR("%s", "ARP message is null!\n");
    return string();
  }

  inaddr.s_addr = arpmsg->tpa;
  requested_ip = inet_ntoa(inaddr);

  return requested_ip;
}

string ACA_ARP_Responder::_serialize_arp_message(vlan_message *vlanmsg, arp_message *arpmsg){
  string packet;
  char str[80];
  if(!arpmsg){
    return string();
  }

  //fix arp header
  sprintf(str,"%04x",ntohs(arpmsg->hrd));
  packet.append(str);
  sprintf(str,"%04x",ntohs(arpmsg->pro));
  packet.append(str);
  sprintf(str,"%02x",arpmsg->hln);
  packet.append(str);
  sprintf(str,"%02x",arpmsg->pln);
  packet.append(str);
  sprintf(str,"%04x",ntohs(arpmsg->op));
  packet.append(str);

  //fix ip and mac address of source node
  for (int i = 0; i < 6; i++) {
    sprintf(str, "%02x", arpmsg->sha[i]);
    packet.append(str);
  }
  sprintf(str,"%08x",ntohl(arpmsg->spa));
  packet.append(str);

  //fix ip and mac address of target node
  for (int i = 0; i < 6; i++) {
    sprintf(str, "%02x", arpmsg->tha[i]);
    packet.append(str);
  }
  sprintf(str,"%08x",ntohl(arpmsg->tpa));
  packet.append(str);

  //fix the ethernet header
  string packet_header;
  for (int i = 0; i < 6; i++) {
    sprintf(str, "%02x", arpmsg->tha[i]);
    packet_header.append(str);
  }

  for (int i = 0; i < 6; i++){
    sprintf(str, "%02x", arpmsg->sha[i]);
    packet_header.append(str);
  }
  //fix the vlan header
  if(vlanmsg){
    sprintf(str,"%04x",ntohs(vlanmsg->vlan_proto));
    packet_header.append(str);
    sprintf(str,"%04x",ntohs(vlanmsg->vlan_tci));
    packet_header.append(str);
  }

  //arp protocol：0806
  packet_header.append("0806");
  packet.insert(0,packet_header);
  return packet;
}
} // namespace aca_arp_responder

