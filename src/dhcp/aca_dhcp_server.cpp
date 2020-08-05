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

#include "aca_dhcp_server.h"
#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <errno.h>
#include <arpa/inet.h>
#include "aca_ovs_control.h"

using namespace std;
using namespace aca_dhcp_programming_if;

namespace aca_dhcp_server
{
ACA_Dhcp_Server::ACA_Dhcp_Server()
{
  try {
    _dhcp_db = new unordered_map<string, dhcp_entry_data>;
  } catch (const bad_alloc &e) {
    return;
  }

  _dhcp_entry_thresh = 0x10000; //10K

  _init_dhcp_msg_ops();
}

ACA_Dhcp_Server::~ACA_Dhcp_Server()
{
  delete _dhcp_db;
  _dhcp_db = nullptr;
}

int ACA_Dhcp_Server::initialize()
{
  return 0;
}

int ACA_Dhcp_Server::add_dhcp_entry(dhcp_config *dhcp_cfg_in)
{
  dhcp_entry_data stData;

  if (_validate_dhcp_entry(dhcp_cfg_in)) {
    ACA_LOG_ERROR("Valiate dhcp cfg failed! (mac = %s)\n",
                  dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  if (DHCP_DB_SIZE >= _dhcp_entry_thresh) {
    ACA_LOG_WARN("Exceed db threshold! (dhcp_db_size = %s)\n", DHCP_DB_SIZE);
  }

  DHCP_ENTRY_DATA_SET((dhcp_entry_data *)&stData, dhcp_cfg_in);

  if (nullptr != _search_dhcp_entry(dhcp_cfg_in->mac_address)) {
    ACA_LOG_ERROR("Entry already existed! (mac = %s)\n",
                  dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  _dhcp_db_mutex.lock();
  _dhcp_db->insert(make_pair(dhcp_cfg_in->mac_address, stData));
  _dhcp_db_mutex.unlock();

  return EXIT_SUCCESS;
}

int ACA_Dhcp_Server::delete_dhcp_entry(dhcp_config *dhcp_cfg_in)
{
  if (_validate_dhcp_entry(dhcp_cfg_in)) {
    ACA_LOG_ERROR("Valiate dhcp cfg failed! (mac = %s)\n",
                  dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  if (0 >= DHCP_DB_SIZE) {
    ACA_LOG_WARN("DHCP DB is empty! (mac = %s)\n", dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  if (nullptr == _search_dhcp_entry(dhcp_cfg_in->mac_address)) {
    ACA_LOG_INFO("Entry not exist!  (mac = %s)\n", dhcp_cfg_in->mac_address.c_str());
    return EXIT_SUCCESS;
  }

  _dhcp_db_mutex.lock();
  _dhcp_db->erase(dhcp_cfg_in->mac_address);
  _dhcp_db_mutex.unlock();

  return EXIT_SUCCESS;
}

int ACA_Dhcp_Server::update_dhcp_entry(dhcp_config *dhcp_cfg_in)
{
  //dhcp_entry_data stData = {0};
  std::map<string, dhcp_entry_data>::iterator pos;
  dhcp_entry_data *pData = nullptr;

  if (_validate_dhcp_entry(dhcp_cfg_in)) {
    ACA_LOG_ERROR("Valiate dhcp cfg failed! (mac = %s)\n",
                  dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  pData = _search_dhcp_entry(dhcp_cfg_in->mac_address);
  if (nullptr == pData) {
    ACA_LOG_ERROR("Entry not exist! (mac = %s)\n", dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  _dhcp_db_mutex.lock();
  DHCP_ENTRY_DATA_SET(pData, dhcp_cfg_in);
  _dhcp_db_mutex.unlock();

  return EXIT_SUCCESS;
}

dhcp_entry_data *ACA_Dhcp_Server::_search_dhcp_entry(string mac_address)
{
  std::map<string, dhcp_entry_data>::iterator pos;

  pos = _dhcp_db->find(mac_address);
  if (_dhcp_db->end() == pos) {
    return nullptr;
  }

  return (dhcp_entry_data *)&(pos->second);
}

void ACA_Dhcp_Server::_validate_mac_address(const char *mac_string)
{
  unsigned char mac[6];

  if (mac_string == nullptr) {
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

void ACA_Dhcp_Server::_validate_ipv4_address(const char *ip_address)
{
  struct sockaddr_in sa;

  // inet_pton returns 1 for success 0 for failure
  if (inet_pton(AF_INET, ip_address, &(sa.sin_addr)) != 1) {
    throw std::invalid_argument("Virtual ipv4 address is not in the expect format");
  }
}

void ACA_Dhcp_Server::_validate_ipv6_address(const char *ip_address)
{
  struct sockaddr_in sa;

  // inet_pton returns 1 for success 0 for failure
  if (inet_pton(AF_INET6, ip_address, &(sa.sin_addr)) != 1) {
    throw std::invalid_argument("Virtual ipv6 address is not in the expect format");
  }
}

int ACA_Dhcp_Server::_validate_dhcp_entry(dhcp_config *dhcp_cfg_in)
{
  if (0 >= dhcp_cfg_in->mac_address.size()) {
    throw std::invalid_argument("Input mac_string is null");
  }

  _validate_mac_address(dhcp_cfg_in->mac_address.c_str());

  if (0 < dhcp_cfg_in->ipv4_address.size()) {
    _validate_ipv4_address(dhcp_cfg_in->ipv4_address.c_str());
  }

  if (0 < dhcp_cfg_in->ipv6_address.size()) {
    _validate_ipv6_address(dhcp_cfg_in->ipv4_address.c_str());
  }

  return EXIT_SUCCESS;
}

int ACA_Dhcp_Server::_get_db_size() const
{
  if (nullptr != _dhcp_db) {
    return _dhcp_db->size();
  } else {
    ACA_LOG_ERROR("DHCP-DB does not exist!\n");
    return EXIT_FAILURE;
  }
}

/************* Operation and procedure for dataplane *******************/

void ACA_Dhcp_Server::dhcps_recv(void *message)
{
  const dhcp_message *dhcpmsg = nullptr;
  uint8_t msg_type = 0;

  if (nullptr == message) {
    ACA_LOG_ERROR("DHCP message is null!\n");
    return;
  }

  dhcpmsg = (dhcp_message *)message;

  if (_validate_dhcp_message(dhcpmsg)) {
    ACA_LOG_ERROR("Invalid DHCP message!\n");
    return;
  }

  msg_type = _get_message_type(dhcpmsg);
  _parse_dhcp_msg_ops[msg_type](dhcpmsg);

  return;
}

void ACA_Dhcp_Server::dhcps_xmit(void *message)
{
  string bridge = "br-int";
  string in_port = "in port=controller";
  string whitespace = " ";
  string action = "actions=normal";
  string packetpre = "packet=";
  string packet;
  string options;

  if (nullptr == message) {
    return;
  }

  packet = _serialize_dhcp_message((dhcp_message *)message);
  if (nullptr == packet) {
    return;
  }

  //bridge = "br-int" opts = "in_port=controller packet=<hex-string> actions=normal"
  options = in_port + whitespace + packetpre + packet + whitespace + action;

  aca_ovs_control::ACA_OVS_Control::get_instance().packet_out(bridge.c_str(),
                                                              options.c_str());

  delete message;
}

int ACA_Dhcp_Server::_validate_dhcp_message(dhcp_message *dhcpmsg)
{
  int retcode = 0;

  if (nullptr == dhcpmsg) {
    ACA_LOG_ERROR("DHCP message is null!\n");
    return EXIT_FAILURE;
  }

  do {
    if (BOOTP_MSG_BOOTREQUEST != dhcpmsg->op) {
      retcode = -1;
      ACA_LOG_ERROR("Invalid 'op' field for DHCP message!\n");
      break;
    }

    if (DHCP_MSG_HWTYPE_ETH == dhcpmsg->htype && 6 != dhcpmsg->hlen) {
      retcode = -1;
      ACA_LOG_ERROR("Invalid 'hlen' field for ethernet!\n");
      break;
    }

  } while (0);

  if (0 != retcode) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

uint8_t *ACA_Dhcp_Server::_get_option(dhcp_message *dhcpmsg, uint8_t code)
{
  uint8_t *options = nullptr;

  options = dhcpmsg->options;

  for (int i = 0; i < DHCP_MSG_OPTS_LENGTH;) {
    if (options[i] == code) {
      return options + i;
    } else if (options[i] == DHCP_OPT_PAD) {
      i++;
    } else if (options[i] == DHCP_OPT_END) {
      break;
    } else {
      i += options[i + 1] + DHCP_OPT_CLV_HEADER;
    }
  }

  return nullptr;
}

uint8_t ACA_Dhcp_Server::_get_message_type(dhcp_message *dhcpmsg)
{
  dhcp_message_options *popt = nullptr;

  if (nullptr == dhcpmsg) {
    ACA_LOG_ERROR("DHCP message is null!\n");
    return DHCP_MSG_NONE;
  }

  popt->dhcpmsgtype = (dhcp_message_type *)_get_option(dhcpmsg, DHCP_OPT_CODE_MSGTYPE);
  if (nullptr == popt->dhcpmsgtype) {
    return DHCP_MSG_NONE;
  }

  return popt->dhcpmsgtype->msg_type;
}

uint32_t ACA_Dhcp_Server::_get_server_id(dhcp_message *dhcpmsg)
{
  dhcp_message_options *popt = nullptr;

  if (nullptr == dhcpmsg) {
    ACA_LOG_ERROR("DHCP message is null!\n");
    return 0;
  }

  popt->serverid = (dhcp_server_id *)_get_option(dhcpmsg, DHCP_OPT_CODE_SERVER_ID);
  if (nullptr == popt->serverid) {
    return 0;
  }

  return popt->serverid->sid;
}

uint32_t ACA_Dhcp_Server::_get_requested_ip(dhcp_message *dhcpmsg)
{
  dhcp_message_options *popt = nullptr;

  if (nullptr == dhcpmsg) {
    ACA_LOG_ERROR("DHCP message is null!\n");
    return 0;
  }

  popt->reqip = (dhcp_req_ip *)_get_option(dhcpmsg, DHCP_OPT_CODE_REQ_IP);
  if (nullptr == popt->reqip) {
    return 0;
  }

  return popt->reqip->req_ip;
}

void ACA_Dhcp_Server::_pack_dhcp_message(dhcp_message *rpl, dhcp_message *req)
{
  if (nullptr == rpl || nullptr == req) {
    return;
  }

  //DHCP Fix header
  _pack_dhcp_header(rpl);

  rpl->hops = req->hops;
  rpl->xid = req->xid;
  rpl->ciaddr = 0;
  rpl->siaddr = 0;
  rpl->giaddr = 0;
  memcpy(rpl->chaddr, req->chaddr, 16);
}

void ACA_Dhcp_Server::_pack_dhcp_header(dhcp_message *dhcpmsg)
{
  if (nullptr == dhcpmsg) {
    return;
  }

  dhcpmsg->op = BOOTP_MSG_BOOTREPLY;
  dhcpmsg->htype = DHCP_MSG_HWTYPE_ETH;
  dhcpmsg->hlen = DHCP_MSG_HWTYPE_ETH_LEN;
  dhcpmsg->cookie = htonl(DHCP_MSG_MAGIC_COOKIE);
}

void ACA_Dhcp_Server::_pack_dhcp_opt_msgtype(uint8_t *option, uint8_t msg_type)
{
  dhcp_message_type *msgtype = nullptr;

  if (nullptr == option) {
    return;
  }

  msgtype = (dhcp_message_type *)option;
  msgtype->code = DHCP_OPT_CODE_MSGTYPE;
  msgtype->len = DHCP_OPT_LEN_1BYTE;
  msgtype->msg_type = msg_type;
}

void ACA_Dhcp_Server::_pack_dhcp_opt_ip_lease_time(uint8_t *option, uint32_t lease)
{
  dhcp_ip_lease_time *lt = nullptr;

  if (nullptr == option) {
    return;
  }

  if (0 == lease) {
    lease = DHCP_OPT_DEFAULT_IP_LEASE_TIME;
  }

  lt = (dhcp_ip_lease_time *)option;
  lt->code = DHCP_OPT_CODE_IP_LEASE_TIME;
  lt->len = DHCP_OPT_LEN_4BYTE;
  lt->lease_time = lease;
}

void ACA_Dhcp_Server::_pack_dhcp_opt_server_id(uint8_t *option, uint32_t server_id)
{
  dhcp_server_id *sid = nullptr;

  if (nullptr == option) {
    return;
  }

  sid = (dhcp_server_id *)option;
  sid->code = DHCP_OPT_CODE_SERVER_ID;
  sid->len = DHCP_OPT_LEN_4BYTE;
  sid->sid = server_id;
}

void ACA_Dhcp_Server::_init_dhcp_msg_ops()
{
  _parse_dhcp_msg_ops[DHCP_MSG_NONE] = _parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPDISCOVER] = _parse_dhcp_discover;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPOFFER] = _parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPREQUEST] = _parse_dhcp_request;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPDECLINE] = _parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPACK] = _parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPNAK] = _parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPRELEASE] = _parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPINFORM] = _parse_dhcp_none;
}

void ACA_Dhcp_Server::_parse_dhcp_none(dhcp_message *dhcpmsg)
{
  ACA_LOG_ERROR("Wrong DHCP message type! (Message type = %d)\n",
                _get_message_type(dhcpmsg));
  return;
}

void ACA_Dhcp_Server::_parse_dhcp_discover(dhcp_message *dhcpmsg)
{
  string mac_address;
  dhcp_entry_data *pData = nullptr;
  dhcp_message *dhcpoffer = nullptr;

  mac_address = dhcpmsg->chaddr;
  mac_address.substr(0, dhcpmsg->hlen);
  pData = _search_dhcp_entry(mac_address);
  if (nullptr == pData) {
    ACA_LOG_ERROR("DHCP entry does not exist! (mac = %s)\n", mac_address.c_str());
    return;
  }

  dhcpoffer = _pack_dhcp_offer(dhcpmsg, pData);
  if (nullptr == dhcpoffer) {
    return;
  }

  dhcps_xmit(dhcpoffer);
}

dhcp_message *
ACA_Dhcp_Server::_pack_dhcp_offer(dhcp_message *dhcpdiscover, dhcp_entry_data *pData)
{
  dhcp_message *dhcpoffer = nullptr;
  struct sockaddr_in sa;
  uint8_t *pos = nullptr;
  int opts_len = 0;

  dhcpoffer = new dhcp_message();

  //Pack DHCP header
  _pack_dhcp_message(dhcpoffer, dhcpdiscover);

  if (inet_pton(AF_INET, pData->ipv4_address, &(sa.sin_addr)) != 1) {
    throw std::invalid_argument("Virtual ipv4 address is not in the expect format");
  }
  dhcpoffer->yiaddr = htonl(sa.sin_addr);

  //DHCP Options
  pos = dhcpoffer->options;

  //DHCP Options: dhcp message type
  _pack_dhcp_opt_msgtype(&pos[opts_len], DHCP_MSG_DHCPOFFER);
  opts_len += DHCP_OPT_CLV_HEADER + DHCP_OPT_LEN_1BYTE;

  //DHCP Options: ip address lease time
  _pack_dhcp_opt_ip_lease_time(&pos[opts_len], DHCP_OPT_DEFAULT_IP_LEASE_TIME);
  opts_len += DHCP_OPT_CLV_HEADER + DHCP_OPT_LEN_4BYTE;

  //DHCP Options: server identifier
  _pack_dhcp_opt_server_id(&pos[opts_len], 2130706433); //Hard coded for 127.0.0.1
  opts_len += DHCP_OPT_CLV_HEADER + DHCP_OPT_LEN_4BYTE;

  //DHCP Options: end
  pos[opts_len] = DHCP_OPT_END;

  return dhcpoffer;
}

void ACA_Dhcp_Server::_parse_dhcp_request(dhcp_message *dhcpmsg)
{
  string mac_address;
  dhcp_entry_data *pData = nullptr;
  dhcp_message *dhcpack = nullptr;
  dhcp_message *dhcpnak = nullptr;
  struct sockaddr_in sa;
  uint32_t self_sid = 0;

  // Fetch the record in DB
  mac_address = dhcpmsg->chaddr;
  mac_address.substr(0, dhcpmsg->hlen);
  pData = _search_dhcp_entry(mac_address);
  if (nullptr == pData) {
    ACA_LOG_ERROR("DHCP entry does not exist! (mac = %s)\n", mac_address.c_str());
    return;
  }

  //Verify client is requesting to myself
  //Need the fetch self server id here!!
  if (self_sid == _get_server_id(dhcpmsg)) { //request to me
    //Verify the ip address from client is the one assigned in DHCPOFFER
    if (inet_pton(AF_INET, pData->ipv4_address, &(sa.sin_addr)) != 1) {
      throw std::invalid_argument("Virtual ipv4 address is not in the expect format");
    }
    if (sa.sin_addr != _get_requested_ip(dhcpmsg)) {
      ACA_LOG_ERROR("IP address %d in DHCP request is not same as the one in DB!", sa.sin_addr);
      dhcpnak = _pack_dhcp_nak(dhcpmsg);
      dhcps_xmit(dhcpnak);
      return;
    }

    dhcpack = _pack_dhcp_ack(dhcpmsg);
    dhcps_xmit(dhcpack);

  } else { //not to me
  }
}

dhcp_message *ACA_Dhcp_Server::_pack_dhcp_ack(dhcp_message *dhcpreq)
{
  dhcp_message *dhcpack = nullptr;
  struct sockaddr_in sa;
  uint8_t *pos = nullptr;
  int opts_len = 0;

  dhcpack = new dhcp_message();

  //Pack DHCP header
  _pack_dhcp_message(dhcpack, dhcpreq);

  if (inet_pton(AF_INET, pData->ipv4_address, &(sa.sin_addr)) != 1) {
    throw std::invalid_argument("Virtual ipv4 address is not in the expect format");
  }
  dhcpack->yiaddr = htonl(sa.sin_addr);

  //DHCP Options
  pos = dhcpack->options;

  //DHCP Options: dhcp message type
  _pack_dhcp_opt_msgtype(&pos[opts_len], DHCP_MSG_DHCPACK);
  opts_len += DHCP_OPT_CLV_HEADER + DHCP_OPT_LEN_1BYTE;

  //DHCP Options: ip address lease time
  _pack_dhcp_opt_ip_lease_time(&pos[opts_len], DHCP_OPT_DEFAULT_IP_LEASE_TIME);
  opts_len += DHCP_OPT_CLV_HEADER + DHCP_OPT_LEN_4BYTE;

  //DHCP Options: server identifier
  _pack_dhcp_opt_server_id(&pos[opts_len], 2130706433); //Hard coded for 127.0.0.1
  opts_len += DHCP_OPT_CLV_HEADER + DHCP_OPT_LEN_4BYTE;

  //DHCP Options: end
  pos[opts_len] = DHCP_OPT_END;

  return dhcpack;
}

dhcp_message *ACA_Dhcp_Server::_pack_dhcp_nak(dhcp_message *dhcpreq)
{
  dhcp_message *dhcpnak = nullptr;
  struct sockaddr_in sa;
  uint8_t *pos = nullptr;
  int opts_len = 0;

  dhcpnak = new dhcp_message();

  //Pack DHCP header
  _pack_dhcp_message(dhcpnak, dhcpreq);

  dhcpnak->yiaddr = 0;

  //DHCP Options
  pos = dhcpnak->options;

  //DHCP Options: dhcp message type
  _pack_dhcp_opt_msgtype(&pos[opts_len], DHCP_MSG_DHCPNAK);
  opts_len += DHCP_OPT_CLV_HEADER + DHCP_OPT_LEN_1BYTE;

  //DHCP Options: server identifier
  _pack_dhcp_opt_server_id(&pos[opts_len], 2130706433); //Hard coded for 127.0.0.1
  opts_len += DHCP_OPT_CLV_HEADER + DHCP_OPT_LEN_4BYTE;

  //DHCP Options: end
  pos[opts_len] = DHCP_OPT_END;

  return dhcpnak;
}

string ACA_Dhcp_Server::_serialize_dhcp_message(dhcp_message *dhcpmsg)
{
  string packet;

  if (nullptr == dhcpmsg) {
    return nullptr;
  }

  //fix header
  packet.append(to_string(dhcpmsg->op));
  packet.append(to_string(dhcpmsg->htype));
  packet.append(to_string(dhcpmsg->hlen));
  packet.append(to_string(dhcpmsg->hops));

  packet.append(to_string(htonl(dhcpmsg->xid)));
  packet.append(to_string(htons(dhcpmsg->secs)));
  packet.append(to_string(htons(dhcpmsg->flags)));
  packet.append(to_string(htonl(dhcpmsg->ciaddr)));
  packet.append(to_string(htonl(dhcpmsg->yiaddr)));
  packet.append(to_string(htonl(dhcpmsg->siaddr)));
  packet.append(to_string(htonl(dhcpmsg->giaddr)));

  for (int i = 0; i < 16; i++) {
    packet.append(to_string(dhcpmsg->chaddr[i]));
  }
  for (int i = 0; i < 64; i++) {
    packet.append(to_string(dhcpmsg->sname[i]));
  }
  for (int i = 0; i < 128; i++) {
    packet.append(to_string(dhcpmsg->file[i]));
  }

  packet.append(to_string(htonl(dhcpmsg->cookie)));

  //options part
  for (int i = 0; i < DHCP_MSG_OPTS_LENGTH;) {
    if (DHCP_OPT_END == dhcpmsg->options[i]) {
      packet.append(to_string(dhcpmsg->options[i]));
      break;
    }
    packet.append(to_string(dhcpmsg->options[i]));
    packet.append(to_string(dhcpmsg->options[i + 1]));
    if (DHCP_OPT_LEN_1BYTE == dhcpmsg->options[i + 1]) {
      packet.append(to_string(dhcpmsg->options[i + 2]));
      i += DHCP_OPT_CLV_HEADER + DHCP_OPT_LEN_1BYTE;
      continue;
    }
    if (DHCP_OPT_LEN_4BYTE == dhcpmsg->options[i + 1]) {
      packet.append(to_string(htonl(*(uint32_t *)(&dhcpmsg->options[i + 2]))));
      i += DHCP_OPT_CLV_HEADER + DHCP_OPT_LEN_4BYTE;
      continue;
    }
  }

  return packet;
}

} //namespace aca_dhcp_server
