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

#include "aca_arp_responder.h"
#include "aca_log.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_ovs_control.h"
#include "aca_util.h"
#include <shared_mutex>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <fmt/core.h>
#include <fmt/compile.h>

using namespace std;

namespace aca_arp_responder
{
ACA_ARP_Responder::ACA_ARP_Responder()
{
  _init_arp_db();
  _init_arp_ofp();
}

ACA_ARP_Responder::~ACA_ARP_Responder()
{
  _deinit_arp_db();
  _deinit_arp_ofp();
}

void ACA_ARP_Responder::_init_arp_db()
{
  _arp_db.clear();
}

void ACA_ARP_Responder::_deinit_arp_db()
{
  _arp_db.clear();
}

void ACA_ARP_Responder::_init_arp_ofp()
{
  // int overall_rc = EXIT_SUCCESS;
  // unsigned long not_care_culminative_time;
  // remove the following
  // aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
  //         "add-flow br-tun \"table=0,priority=50,arp,arp_op=1, actions=CONTROLLER\"",
  //         not_care_culminative_time, overall_rc);
  return;
}

void ACA_ARP_Responder::_deinit_arp_ofp()
{
  unsigned long not_care_culminative_time;

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow(not_care_culminative_time,
          "br-tun",
          "arp,arp_op=1",
          "del");
  return;
}

ACA_ARP_Responder &ACA_ARP_Responder::get_instance()
{
  static ACA_ARP_Responder instance;
  return instance;
}
bool ACA_ARP_Responder::does_arp_entry_exist(arp_entry_data stData)
{
  bool entry_exist = false;
  arp_table_data *current_arp_data = nullptr;
  entry_exist = _arp_db.find(stData, current_arp_data);
  return entry_exist;
}

int ACA_ARP_Responder::add_arp_entry(arp_config *arp_cfg_in)
{
  arp_entry_data stData;
  arp_table_data *current_arp_data = new arp_table_data;

  try {
    _validate_arp_entry(arp_cfg_in);

    ARP_ENTRY_DATA_SET((arp_entry_data *)&stData, arp_cfg_in);
    ARP_TABLE_DATA_SET(current_arp_data, arp_cfg_in);

    if (_arp_db.find(stData, current_arp_data)) {
      ACA_LOG_ERROR("Entry already existed! (ip = %s and vlan id = %u)\n",
                    arp_cfg_in->ipv4_address.c_str(), arp_cfg_in->vlan_id);
      return EXIT_FAILURE;
    }

    _arp_db.insert(stData, current_arp_data);

    // ACA_LOG_DEBUG("Arp Entry with ip: %s and vlan id %u added\n",
    //               arp_cfg_in->ipv4_address.c_str(), arp_cfg_in->vlan_id);

    return EXIT_SUCCESS;
  } catch (std::invalid_argument &ia) {
    ACA_LOG_ERROR("%s,validate arp config failed! (ip = %s and vlan id = %u)\n",
                  ia.what(), arp_cfg_in->ipv4_address.c_str(), arp_cfg_in->vlan_id);
    return EXIT_FAILURE;
  }
}

int ACA_ARP_Responder::create_or_update_arp_entry(arp_config *arp_cfg_in)
{
  arp_entry_data stData;
  arp_table_data *current_arp_data = new arp_table_data;

  try {
    _validate_arp_entry(arp_cfg_in);
    ARP_ENTRY_DATA_SET((arp_entry_data *)&stData, arp_cfg_in);
    ARP_TABLE_DATA_SET(current_arp_data, arp_cfg_in);

    if (!_arp_db.find(stData, current_arp_data)) {
      ACA_LOG_DEBUG("Entry not exist! (ip = %s and vlan id = %u)\n",
                    arp_cfg_in->ipv4_address.c_str(), arp_cfg_in->vlan_id);
      add_arp_entry(arp_cfg_in);
    } else {
      current_arp_data->mac_address = arp_cfg_in->mac_address;
    }
    return EXIT_SUCCESS;
  } catch (std::invalid_argument &ia) {
    ACA_LOG_ERROR("%s,validate arp config failed! (ip = %s and vlan id = %u)\n",
                  ia.what(), arp_cfg_in->ipv4_address.c_str(), arp_cfg_in->vlan_id);
    return EXIT_FAILURE;
  }
}
int ACA_ARP_Responder::delete_arp_entry(arp_config *arp_cfg_in)
{
  arp_entry_data stData;
  arp_table_data *current_arp_data = new arp_table_data;

  try {
    _validate_arp_entry(arp_cfg_in);
    ARP_ENTRY_DATA_SET((arp_entry_data *)&stData, arp_cfg_in);
    ARP_TABLE_DATA_SET(current_arp_data, arp_cfg_in);

    if (!_arp_db.find(stData, current_arp_data)) {
      ACA_LOG_DEBUG("Entry not exist! (ip = %s and vlan id = %u)\n",
                    arp_cfg_in->ipv4_address.c_str(), arp_cfg_in->vlan_id);
      return EXIT_SUCCESS;
    }
    _arp_db.erase(stData);
    return EXIT_SUCCESS;
  } catch (std::invalid_argument &ia) {
    ACA_LOG_ERROR("%s,validate arp config failed! (ip = %s and vlan id = %u)\n",
                  ia.what(), arp_cfg_in->ipv4_address.c_str(), arp_cfg_in->vlan_id);
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

  if (!aca_validate_mac_address(arp_cfg_in->mac_address.c_str())) {
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

int ACA_ARP_Responder::arp_recv(uint32_t in_port, void *vlan_hdr, void *message, int of_connection_id)
{
  arp_message *arpmsg = nullptr;
  vlan_message *vlanmsg = nullptr;

  ACA_LOG_DEBUG("Receiving arp message from inport=%u\n", in_port);
  if (!message) {
    ACA_LOG_ERROR("%s", "ARP message is null!\n");
    return EXIT_FAILURE;
  }

  vlanmsg = (vlan_message *)vlan_hdr;
  arpmsg = (arp_message *)message;

  if (_validate_arp_message(arpmsg)) {
    ACA_LOG_ERROR("%s", "Invalid APR message!\n");
    return EXIT_FAILURE;
  }

  return _parse_arp_request(in_port, vlanmsg, arpmsg, of_connection_id);
}

void ACA_ARP_Responder::arp_xmit(uint32_t in_port, void *vlanmsg, void *message, int is_found, int of_connection_id)
{
  arp_message *arpmsg = nullptr;
  string bridge = "br-tun";
  string inport = "in_port=controller";
  string whitespace = " ";
  string action = "actions=output:" + to_string(in_port);
  string rs_action = "actions=resubmit(,22)";
  string packetpre = "packet=";
  string packet;
  string options;

  arpmsg = (arp_message *)message;
  if (!arpmsg) {
    ACA_LOG_ERROR("%s", "ARP Reply is null!\n");
    return;
  }

  packet = _serialize_arp_message((vlan_message *)vlanmsg, arpmsg);
  if (packet.empty()) {
    //ACA_LOG_ERROR("%s", "Serialized ARP Reply is null!\n");
    return;
  }

  if (is_found) {
    options = inport + whitespace + packetpre + packet + whitespace + action;
    //delete the constructed arp reply
    delete arpmsg;
  } else {
    options = inport + whitespace + packetpre + packet + whitespace + rs_action;
  }

  ACA_LOG_DEBUG("ACA_ARP_Responder sent arp packet to ovs: %s\n", options.c_str());
  //aca_ovs_control::ACA_OVS_Control::get_instance().packet_out(bridge.c_str(),
  //                                                            options.c_str());
  // aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().packet_out(bridge.c_str(),
  //                                                                         options.c_str());

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().packet_out(of_connection_id,
                                                                          options.c_str());
}

int ACA_ARP_Responder::_parse_arp_request(uint32_t in_port, vlan_message *vlanmsg,
                                          arp_message *arpmsg, int of_connection_id)
{
  arp_entry_data stData;
  arp_table_data *current_arp_data = new arp_table_data;
  arp_message *arpreply = nullptr;

  // get the ip address from arp message
  stData.ipv4_address = _get_requested_ip(arpmsg);

  // get the vlan id from vlan header
  if (vlanmsg) {
    stData.vlan_id = ntohs(vlanmsg->vlan_tci) & 0x0fff;
  } else {
    stData.vlan_id = 0;
  }

  // auto requested_ip = _get_requested_ip(arpmsg);

  current_arp_data->mac_address = "6c:dd:ee:00:11:22";

  // put this .find here just to make sure that there's a lookup operation.
  assert(!_arp_db.find(stData, current_arp_data));

  arpreply = _pack_arp_reply(arpmsg, current_arp_data->mac_address);
  arp_xmit(in_port, vlanmsg, arpreply, 1, of_connection_id);
  return EXIT_SUCCESS;

  /*
  // if not find the corresponding mac address in the db based on ip and vlan id, resubmit to table 22
  // else construct an arp reply
  if (!_arp_db.find(stData, current_arp_data)) {
    ACA_LOG_DEBUG("ARP entry does not exist! (ip = %s and vlan id = %u)\n",
                  stData.ipv4_address.c_str(), stData.vlan_id);
    return ENOTSUP;
  } else {
    ACA_LOG_DEBUG("ARP entry exist (ip = %s and vlan id = %u) with mac = %s\n",
                  stData.ipv4_address.c_str(), stData.vlan_id,
                  current_arp_data->mac_address.c_str());
    arpreply = _pack_arp_reply(arpmsg, current_arp_data->mac_address);
    arp_xmit(in_port, vlanmsg, arpreply, 1, of_connection_id);
    return EXIT_SUCCESS;
  }
  */
}

arp_message *ACA_ARP_Responder::_pack_arp_reply(arp_message *arpreq, string mac_address)
{
  arp_message *arpreply = nullptr;
  arpreply = new arp_message();
  unsigned int tmp_mac[6];

  //construct arp reply form arp request and mac address in the db
  arpreply->hrd = arpreq->hrd;
  arpreply->pro = arpreq->pro;
  arpreply->hln = arpreq->hln;
  arpreply->pln = arpreq->pln;
  arpreply->op = htons(2);
  memcpy(arpreply->tha, arpreq->sha, 6);
  arpreply->spa = arpreq->tpa;
  sscanf(mac_address.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", tmp_mac,
         tmp_mac + 1, tmp_mac + 2, tmp_mac + 3, tmp_mac + 4, tmp_mac + 5);
  for (int i = 0; i < 6; i++) {
    arpreply->sha[i] = tmp_mac[i];
  }
  arpreply->tpa = arpreq->spa;

  return arpreply;
}

int ACA_ARP_Responder::_validate_arp_message(arp_message *arpmsg)
{
  if (!arpmsg) {
    ACA_LOG_ERROR("%s", "ARP message is null!\n");
    return EXIT_FAILURE;
  }

  if (ntohs(arpmsg->op) != 1) {
    ACA_LOG_ERROR("%s", "ARP message is not a ARP request!\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

string ACA_ARP_Responder::_get_requested_ip(arp_message *arpmsg)
{
  string requested_ip;
  struct in_addr inaddr;
  if (!arpmsg) {
    ACA_LOG_ERROR("%s", "ARP message is null!\n");
    return string();
  }

  inaddr.s_addr = arpmsg->tpa;
  requested_ip = inet_ntoa(inaddr);

  return requested_ip;
}

string ACA_ARP_Responder::_get_source_ip(arp_message *arpmsg)
{
  string source_ip;
  struct in_addr inaddr;
  if (!arpmsg) {
    ACA_LOG_ERROR("%s", "ARP message is null!\n");
    return string();
  }

  inaddr.s_addr = arpmsg->spa;
  source_ip = inet_ntoa(inaddr);

  return source_ip;
}

string ACA_ARP_Responder::_serialize_arp_message(vlan_message *vlanmsg, arp_message *arpmsg)
{
  string packet;
  char str[80];
  if (!arpmsg) {
    return string();
  }

  auto out = fmt::memory_buffer();
  fmt::format_to(std::back_inserter(out),FMT_COMPILE("{:04x}{:04x}{:02x}{:02x}{:04x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:08x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:08x}")
  , ntohs(arpmsg->hrd), ntohs(arpmsg->pro), arpmsg->hln, arpmsg->pln, ntohs(arpmsg->op)
  , arpmsg->sha[0], arpmsg->sha[1], arpmsg->sha[2], arpmsg->sha[3], arpmsg->sha[4], arpmsg->sha[5], ntohl(arpmsg->spa)
  , arpmsg->tha[0], arpmsg->tha[1], arpmsg->tha[2], arpmsg->tha[3], arpmsg->tha[4], arpmsg->tha[5], ntohl(arpmsg->tpa)
  );

  /*
  //fix arp header
  fmt::format_to(std::back_inserter(out), "{:04x}", ntohs(arpmsg->hrd));
  fmt::format_to(std::back_inserter(out), "{:04x}", ntohs(arpmsg->pro));
  fmt::format_to(std::back_inserter(out), "{:02x}", arpmsg->hln);
  fmt::format_to(std::back_inserter(out), "{:02x}", arpmsg->pln);
  fmt::format_to(std::back_inserter(out), "{:04x}", ntohs(arpmsg->op));

  //fix ip and mac address of source node
  for (int i = 0; i < 6; i++){
    fmt::format_to(std::back_inserter(out), "{:02x}", arpmsg->sha[i]);
  }
  fmt::format_to(std::back_inserter(out), "{:08x}", ntohl(arpmsg->spa));

  //fix ip and mac address of target node
  for (int i = 0; i < 6; i++){
    fmt::format_to(std::back_inserter(out), "{:02x}", arpmsg->tha[i]);
  }
  fmt::format_to(std::back_inserter(out), "{:08x}", ntohl(arpmsg->tpa));
  */

  //fix the ethernet header
  auto packet_header = fmt::memory_buffer();
  fmt::format_to(std::back_inserter(packet_header), FMT_COMPILE("{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}")
  , arpmsg->tha[0], arpmsg->tha[1], arpmsg->tha[2], arpmsg->tha[3], arpmsg->tha[4], arpmsg->tha[5]
  , arpmsg->sha[0], arpmsg->sha[1], arpmsg->sha[2], arpmsg->sha[3], arpmsg->sha[4], arpmsg->sha[5]
  );
  /*
  for (int i = 0; i < 6; i++){
    fmt::format_to(std::back_inserter(packet_header), "{:02x}", arpmsg->tha[i]);
  }
  for (int i = 0; i < 6; i++){
    fmt::format_to(std::back_inserter(packet_header), "{:02x}", arpmsg->sha[i]);
  }
  */
  if (vlanmsg){
    fmt::format_to(std::back_inserter(packet_header), FMT_COMPILE("{:04x}"), ntohs(vlanmsg->vlan_proto));
    fmt::format_to(std::back_inserter(packet_header), FMT_COMPILE("{:04x}"), ntohs(vlanmsg->vlan_tci));
  }
  
  fmt::format_to(std::back_inserter(packet_header), FMT_COMPILE("{}"), "8086");

  packet_header.append(out);

  return fmt::to_string(packet_header);
  /*
  //fix arp header
  sprintf(str, "%04x", ntohs(arpmsg->hrd));
  packet.append(str);
  sprintf(str, "%04x", ntohs(arpmsg->pro));
  packet.append(str);
  sprintf(str, "%02x", arpmsg->hln);
  packet.append(str);
  sprintf(str, "%02x", arpmsg->pln);
  packet.append(str);
  sprintf(str, "%04x", ntohs(arpmsg->op));
  packet.append(str);

  //fix ip and mac address of source node
  for (int i = 0; i < 6; i++) {
    sprintf(str, "%02x", arpmsg->sha[i]);
    packet.append(str);
  }
  sprintf(str, "%08x", ntohl(arpmsg->spa));
  packet.append(str);

  //fix ip and mac address of target node
  for (int i = 0; i < 6; i++) {
    sprintf(str, "%02x", arpmsg->tha[i]);
    packet.append(str);
  }
  sprintf(str, "%08x", ntohl(arpmsg->tpa));
  packet.append(str);

  //fix the ethernet header
  string packet_header;
  for (int i = 0; i < 6; i++) {
    sprintf(str, "%02x", arpmsg->tha[i]);
    packet_header.append(str);
  }

  for (int i = 0; i < 6; i++) {
    sprintf(str, "%02x", arpmsg->sha[i]);
    packet_header.append(str);
  }

  //fix the vlan header
  if (vlanmsg) {
    sprintf(str, "%04x", ntohs(vlanmsg->vlan_proto));
    packet_header.append(str);
    sprintf(str, "%04x", ntohs(vlanmsg->vlan_tci));
    packet_header.append(str);
  }

  //arp protocol：0806
  packet_header.append("0806");
  packet.insert(0, packet_header);

  return packet;
  */
}
} // namespace aca_arp_responder
