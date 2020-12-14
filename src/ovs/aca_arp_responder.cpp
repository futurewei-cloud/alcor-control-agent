#include "aca_arp_responder.h"
#include "aca_log.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_ovs_control.h"
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
  try{
    _arp_db = new unordered_map<arp_entry_data,string,arp_hash>;
  } catch(const bad_alloc &e){
    return;
  }
}

void ACA_ARP_Responder::_deinit_arp_db(){
  delete _arp_db;
  _arp_db = nullptr;
}

void ACA_ARP_Responder::_init_arp_ofp(){
  int overall_rc = EXIT_SUCCESS;
  unsigned long not_care_culminative_time;

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
    "add-flow br-tun \"table=0,priority=25,arp,arp_op=1, actions=CONTROLLER\"",
    not_care_culminative_time, overall_rc);

  return;
}

void ACA_ARP_Responder::_deinit_arp_ofp(){
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          "del-flows br-tun \"arp,arp_op=1\"",
          not_care_culminative_time, overall_rc);
  return;  
}

ACA_ARP_Responder &ACA_ARP_Responder::get_instance(){
  static ACA_ARP_Responder instance;
  return instance;
}


int ACA_ARP_Responder::add_arp_entry(arp_config *arp_cfg_in){
  arp_entry_data stData;

  if(_validate_arp_entry(arp_cfg_in)){
    ACA_LOG_ERROR("Valiate arp cfg failed! (mac = %s)\n",
                  arp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  ARP_ENTRY_DATA_SET((arp_entry_data *)&stData, arp_cfg_in);

  _standardize_mac_address(arp_cfg_in->mac_address);

  if (!_search_arp_entry(stData).empty()) {
    ACA_LOG_ERROR("Entry already existed! (ip = %s)\n",
                  arp_cfg_in->ipv4_address.c_str());
    return EXIT_FAILURE;
  }

  _arp_db_mutex.lock();
  _arp_db->insert(make_pair(stData,arp_cfg_in->mac_address));
  _arp_db_mutex.unlock();

  ACA_LOG_DEBUG("Arp Entry with ip: %s and vlan id %u added\n",arp_cfg_in->ipv4_address.c_str(),arp_cfg_in->vlan_id);

  return EXIT_SUCCESS;
}  

int ACA_ARP_Responder::update_arp_entry(arp_config *arp_cfg_in){
  arp_entry_data stData;

  if(_validate_arp_entry(arp_cfg_in)){
    ACA_LOG_ERROR("Validate arp cfg failed! (ip = %s and vlan id = %u)\n",
                  arp_cfg_in->ipv4_address.c_str(),arp_cfg_in->vlan_id);
    return EXIT_FAILURE;
  }

  _standardize_mac_address(arp_cfg_in->mac_address);
  ARP_ENTRY_DATA_SET((arp_entry_data *)&stData, arp_cfg_in);

  auto pos = _arp_db->find(stData);
  if(pos == _arp_db->end()){
    ACA_LOG_ERROR("Entry not exist! (ip = %s and vlan id = %u)\n",
                  arp_cfg_in->ipv4_address.c_str(),arp_cfg_in->vlan_id);    
    return EXIT_FAILURE;
  }
  _arp_db_mutex.lock();
  pos->second = arp_cfg_in->mac_address;
  _arp_db_mutex.unlock();

  return EXIT_SUCCESS;

}
int ACA_ARP_Responder::delete_arp_entry(arp_config *arp_cfg_in){
  arp_entry_data stData;

  if (_validate_arp_entry(arp_cfg_in)) {
    ACA_LOG_ERROR("Valiate arp cfg failed! (ip = %s and vlan id = %u)\n",
                  arp_cfg_in->ipv4_address.c_str(),arp_cfg_in->vlan_id);
    return EXIT_FAILURE;
  }

  if (_arp_db->empty()){
    ACA_LOG_WARN("%s","ARP DB is empty!\n");
    return EXIT_FAILURE;
  }

  ARP_ENTRY_DATA_SET((arp_entry_data *)&stData, arp_cfg_in);

  if (_search_arp_entry(stData).empty()) {
    ACA_LOG_ERROR("Entry not exist! (ip = %s and vlan id = %u)\n",
                  arp_cfg_in->ipv4_address.c_str(),arp_cfg_in->vlan_id);
    return EXIT_SUCCESS;
  }

  _arp_db_mutex.lock();
  _arp_db->erase(stData);
  _arp_db_mutex.unlock();

  return EXIT_SUCCESS;
}

string ACA_ARP_Responder::_search_arp_entry(arp_entry_data stData){
  auto pos = _arp_db->find(stData);

  if(pos == _arp_db->end()){
    return string();
  }
  return pos->second;

}

void ACA_ARP_Responder::_validate_mac_address(const char *mac_string)
{
  unsigned char mac[6];

  if (!mac_string) {
    throw std::invalid_argument("Input mac_string is null");
  }

  if (sscanf(mac_string, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1],
             &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
    return;
  }

  if (sscanf(mac_string, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx", &mac[0], &mac[1],
             &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
    return;
  }

  // nothing matched
  ACA_LOG_ERROR("Invalid mac address: %s\n", mac_string);

  throw std::invalid_argument("Input mac_string is not in the expect format");
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

  _validate_mac_address(arp_cfg_in->mac_address.c_str());

  if (0 < arp_cfg_in->ipv4_address.size()) {
    _validate_ipv4_address(arp_cfg_in->ipv4_address.c_str());
  }

  if (0 < arp_cfg_in->ipv6_address.size()) {
    _validate_ipv6_address(arp_cfg_in->ipv4_address.c_str());
  }

  return EXIT_SUCCESS;
}




void ACA_ARP_Responder::_standardize_mac_address(string &mac_string)
{
  // standardize the mac address to aa:bb:cc:dd:ee:ff
  std::transform(mac_string.begin(), mac_string.end(), mac_string.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  std::replace(mac_string.begin(), mac_string.end(), '-', ':');
}

/************* Operation and procedure for dataplane *******************/


void ACA_ARP_Responder::arp_recv(uint32_t in_port, void *vlan_hdr, void *message){
  arp_message *arpmsg = nullptr;
  vlan_message *vlanmsg = nullptr;

  printf("============== receive packet ==============\n");
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
    return;
  }

  packet = _serialize_arp_message((vlan_message *)vlanmsg, arpmsg);
  if(packet.empty()){
    return;
  }

  options = inport + whitespace + packetpre + packet +whitespace + action;

  aca_ovs_control::ACA_OVS_Control::get_instance().packet_out(bridge.c_str(),options.c_str());

  delete arpmsg;
}

void ACA_ARP_Responder::_parse_arp_request(uint32_t in_port, vlan_message *vlanmsg, arp_message *arpmsg){
  arp_entry_data stData;
  string mac_address;
  arp_message *arpreply;

  stData.ipv4_address = _get_requested_ip(arpmsg);
  if(vlanmsg){
    stData.vlan_id = ntohs(vlanmsg->vlan_tci) & 0x0111;
    
  }
  else{
    stData.vlan_id = 0;
  }
  mac_address = _search_arp_entry(stData);
  if(mac_address.empty()){
    ACA_LOG_DEBUG("ARP entry does not exist! (ip = %s and vlan id = %u)\n",
                  stData.ipv4_address.c_str(),stData.vlan_id);
    //TO DO
  }
  else{
    arpreply = _pack_arp_reply(arpmsg,mac_address);
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
    return 0;
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

  if(vlanmsg){
    sprintf(str,"%04x",vlanmsg->vlan_proto);
    packet.append(str);
    sprintf(str,"%04x",vlanmsg->vlan_tci);
    packet.append(str);
  }

  
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
  for (int i = 0; i < 6; i++) {
    sprintf(str, "%02x", arpmsg->sha[i]);
    packet.append(str);
  }
  sprintf(str,"%08x",ntohl(arpmsg->spa));
  packet.append(str);
  for (int i = 0; i < 6; i++) {
    sprintf(str, "%02x", arpmsg->tha[i]);
    packet.append(str);
  }
  sprintf(str,"%08x",ntohl(arpmsg->tpa));
  packet.append(str);


  string packet_header;
  for (int i = 0; i < 6; i++) {
    sprintf(str, "%02x", arpmsg->tha[i]);
    packet_header.append(str);
  }

  for (int i = 0; i < 6; i++){
    sprintf(str, "%02x", arpmsg->sha[i]);
    packet_header.append(str);
  }

  packet_header.append("0806");
  packet.insert(0,packet_header);
  return packet;
}
} // namespace aca_arp_responder

