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
#include <sstream>
#include <iomanip>
#include "aca_ovs_control.h"
#include "aca_ovs_l2_programmer.h"

using namespace std;
using namespace aca_dhcp_programming_if;

namespace aca_dhcp_server
{
ACA_Dhcp_Server::ACA_Dhcp_Server()
{
  _init_dhcp_db();
  _init_dhcp_msg_ops();
  _init_dhcp_ofp();
}

ACA_Dhcp_Server::~ACA_Dhcp_Server()
{
  _deinit_dhcp_db();

  _deinit_dhcp_ofp();
}

void ACA_Dhcp_Server::_init_dhcp_db()
{
  try {
    _dhcp_db = new unordered_map<string, dhcp_entry_data>;
  } catch (const bad_alloc &e) {
    return;
  }

  _dhcp_entry_thresh = 0x10000; //10K
}

void ACA_Dhcp_Server::_deinit_dhcp_db()
{
  delete _dhcp_db;
  _dhcp_db = nullptr;
  _dhcp_entry_thresh = 0;
}

void ACA_Dhcp_Server::_init_dhcp_ofp()
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  // adding dhcp default flows
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          "add-flow br-int \"table=0,priority=25,udp,udp_src=68,udp_dst=67,actions=CONTROLLER\"",
          not_care_culminative_time, overall_rc);
  return;
}

void ACA_Dhcp_Server::_deinit_dhcp_ofp()
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  // deleting dhcp default flows
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          "del-flows br-int \"udp,udp_src=68,udp_dst=67\"",
          not_care_culminative_time, overall_rc);
  return;
}

ACA_Dhcp_Server &ACA_Dhcp_Server::get_instance()
{
  static ACA_Dhcp_Server instance;
  return instance;
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
    ACA_LOG_WARN("Exceed db threshold! (dhcp_db_size = %d)\n", DHCP_DB_SIZE);
  }

  DHCP_ENTRY_DATA_SET((dhcp_entry_data *)&stData, dhcp_cfg_in);

  _standardize_mac_address(dhcp_cfg_in->mac_address);

  if (_search_dhcp_entry(dhcp_cfg_in->mac_address)) {
    ACA_LOG_ERROR("Entry already existed! (mac = %s)\n",
                  dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  _dhcp_db_mutex.lock();
  _dhcp_db->insert(make_pair(dhcp_cfg_in->mac_address, stData));
  _dhcp_db_mutex.unlock();
  ACA_LOG_DEBUG("DHCP Entry with mac: %s added\n", dhcp_cfg_in->mac_address.c_str());

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

  _standardize_mac_address(dhcp_cfg_in->mac_address);

  if (!_search_dhcp_entry(dhcp_cfg_in->mac_address)) {
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
  std::unordered_map<string, dhcp_entry_data>::iterator pos;
  dhcp_entry_data *pData = nullptr;

  if (_validate_dhcp_entry(dhcp_cfg_in)) {
    ACA_LOG_ERROR("Valiate dhcp cfg failed! (mac = %s)\n",
                  dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  _standardize_mac_address(dhcp_cfg_in->mac_address);

  pData = _search_dhcp_entry(dhcp_cfg_in->mac_address);
  if (!pData) {
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
  std::unordered_map<string, dhcp_entry_data>::iterator pos;

  pos = _dhcp_db->find(mac_address);
  if (_dhcp_db->end() == pos) {
    return nullptr;
  }

  return (dhcp_entry_data *)&(pos->second);
}

void ACA_Dhcp_Server::_validate_mac_address(const char *mac_string)
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

void ACA_Dhcp_Server::_standardize_mac_address(string &mac_string)
{
  // standardize the mac address to aa:bb:cc:dd:ee:ff
  std::transform(mac_string.begin(), mac_string.end(), mac_string.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  std::replace(mac_string.begin(), mac_string.end(), '-', ':');
}

int ACA_Dhcp_Server::_get_db_size() const
{
  if (_dhcp_db) {
    return _dhcp_db->size();
  } else {
    ACA_LOG_ERROR("%s", "DHCP-DB does not exist!\n");
    return EXIT_FAILURE;
  }
}

/************* Operation and procedure for dataplane *******************/

void ACA_Dhcp_Server::dhcps_recv(uint32_t in_port, void *message)
{
  dhcp_message *dhcpmsg = nullptr;
  uint8_t msg_type = 0;

  if (!message) {
    ACA_LOG_ERROR("%s", "DHCP message is null!\n");
    return;
  }

  dhcpmsg = (dhcp_message *)message;

  if (_validate_dhcp_message(dhcpmsg)) {
    ACA_LOG_ERROR("%s", "Invalid DHCP message!\n");
    return;
  }

  msg_type = _get_message_type(dhcpmsg);
  (this->*_parse_dhcp_msg_ops[msg_type])(in_port, dhcpmsg);

  return;
}

void ACA_Dhcp_Server::dhcps_xmit(uint32_t inport, void *message)
{
  dhcp_message *dhcpmsg = nullptr;
  string bridge = "br-int";
  string in_port = "in_port=controller";
  string whitespace = " ";
  string action = "actions=output:" + to_string(inport);
  string packetpre = "packet=";
  string packet;
  string options;

  dhcpmsg = (dhcp_message *)message;
  if (!dhcpmsg) {
    return;
  }

  packet = _serialize_dhcp_message(dhcpmsg);
  if (packet.empty()) {
    return;
  }

  //bridge = "br-int" opts = "in_port=controller packet=<hex-string> actions=normal"
  options = in_port + whitespace + packetpre + packet + whitespace + action;

  aca_ovs_control::ACA_OVS_Control::get_instance().packet_out(bridge.c_str(),
                                                              options.c_str());

  delete dhcpmsg;
}

int ACA_Dhcp_Server::_validate_dhcp_message(dhcp_message *dhcpmsg)
{
  int retcode = 0;

  if (!dhcpmsg) {
    ACA_LOG_ERROR("%s", "DHCP message is null!\n");
    return EXIT_FAILURE;
  }

  do {
    if (BOOTP_MSG_BOOTREQUEST != dhcpmsg->op) {
      retcode = -1;
      ACA_LOG_ERROR("%s", "Invalid 'op' field for DHCP message!\n");
      break;
    }

    if (DHCP_MSG_HWTYPE_ETH == dhcpmsg->htype && 6 != dhcpmsg->hlen) {
      retcode = -1;
      ACA_LOG_ERROR("%s", "Invalid 'hlen' field for ethernet!\n");
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
      return &options[i];
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
  dhcp_message_options unopt;

  if (!dhcpmsg) {
    ACA_LOG_ERROR("%s", "DHCP message is null!\n");
    return DHCP_MSG_NONE;
  }

  unopt.dhcpmsgtype = (dhcp_message_type *)_get_option(dhcpmsg, DHCP_OPT_CODE_MSGTYPE);
  if (!unopt.dhcpmsgtype) {
    return DHCP_MSG_NONE;
  }

  return unopt.dhcpmsgtype->msg_type;
}

uint32_t ACA_Dhcp_Server::_get_server_id(dhcp_message *dhcpmsg)
{
  dhcp_message_options unopt;

  if (!dhcpmsg) {
    ACA_LOG_ERROR("%s", "DHCP message is null!\n");
    return 0;
  }

  unopt.serverid = (dhcp_server_id *)_get_option(dhcpmsg, DHCP_OPT_CODE_SERVER_ID);
  if (!unopt.serverid) {
    return 0;
  }

  unopt.serverid->sid = ntohl(unopt.serverid->sid);

  return unopt.serverid->sid;
}

uint32_t ACA_Dhcp_Server::_get_requested_ip(dhcp_message *dhcpmsg)
{
  dhcp_message_options unopt;

  if (!dhcpmsg) {
    ACA_LOG_ERROR("%s", "DHCP message is null!\n");
    return 0;
  }

  unopt.reqip = (dhcp_req_ip *)_get_option(dhcpmsg, DHCP_OPT_CODE_REQ_IP);
  if (!unopt.reqip) {
    return 0;
  }

  unopt.reqip->req_ip = ntohl(unopt.reqip->req_ip);

  return unopt.reqip->req_ip;
}

string ACA_Dhcp_Server::_get_client_id(dhcp_message *dhcpmsg)
{
  dhcp_message_options unopt;
  string cid;
  stringstream ss;

  if (!dhcpmsg) {
    ACA_LOG_ERROR("%s", "DHCP message is null!\n");
    return nullptr;
  }

  // get client identifier from option
  unopt.clientid = (dhcp_client_id *)_get_option(dhcpmsg, DHCP_OPT_CODE_CLIENT_ID);
  if (unopt.clientid && unopt.clientid->type == 1) {
    for (int i = 0; i < unopt.clientid->len - 1; i++) {
      ss << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<unsigned int>(unopt.clientid->cid[i]);
      ss << ":";
    }
    ss >> cid;
    cid.pop_back();

    return cid;
  }

  // get client identifier from chaddr
  for (int i = 0; i < dhcpmsg->hlen; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<unsigned int>(dhcpmsg->chaddr[i]);
    ss << ":";
  }
  ss >> cid;
  cid.pop_back();

  return cid;
}

void ACA_Dhcp_Server::_pack_dhcp_message(dhcp_message *rpl, dhcp_message *req)
{
  if (!rpl || !req) {
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
  if (!dhcpmsg) {
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

  if (!option) {
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

  if (!option) {
    return;
  }

  if (0 == lease) {
    lease = DHCP_OPT_DEFAULT_IP_LEASE_TIME;
  }

  lt = (dhcp_ip_lease_time *)option;
  lt->code = DHCP_OPT_CODE_IP_LEASE_TIME;
  lt->len = DHCP_OPT_LEN_4BYTE;
  lt->lease_time = htonl(lease);
}

void ACA_Dhcp_Server::_pack_dhcp_opt_server_id(uint8_t *option, uint32_t server_id)
{
  dhcp_server_id *sid = nullptr;

  if (!option) {
    return;
  }

  sid = (dhcp_server_id *)option;
  sid->code = DHCP_OPT_CODE_SERVER_ID;
  sid->len = DHCP_OPT_LEN_4BYTE;
  sid->sid = htonl(server_id);
}

void ACA_Dhcp_Server::_init_dhcp_msg_ops()
{
  _parse_dhcp_msg_ops[DHCP_MSG_NONE] = &aca_dhcp_server::ACA_Dhcp_Server::_parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPDISCOVER] =
          &aca_dhcp_server::ACA_Dhcp_Server::_parse_dhcp_discover;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPOFFER] = &aca_dhcp_server::ACA_Dhcp_Server::_parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPREQUEST] =
          &aca_dhcp_server::ACA_Dhcp_Server::_parse_dhcp_request;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPDECLINE] = &aca_dhcp_server::ACA_Dhcp_Server::_parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPACK] = &aca_dhcp_server::ACA_Dhcp_Server::_parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPNAK] = &aca_dhcp_server::ACA_Dhcp_Server::_parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPRELEASE] = &aca_dhcp_server::ACA_Dhcp_Server::_parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPINFORM] = &aca_dhcp_server::ACA_Dhcp_Server::_parse_dhcp_none;
}

void ACA_Dhcp_Server::_parse_dhcp_none(uint32_t /* in_port */ , dhcp_message *dhcpmsg)
{
  ACA_LOG_ERROR("Wrong DHCP message type! (Message type = %d)\n",
                _get_message_type(dhcpmsg));
  return;
}

void ACA_Dhcp_Server::_parse_dhcp_discover(uint32_t in_port, dhcp_message *dhcpmsg)
{
  string mac_address;
  dhcp_entry_data *pData = nullptr;
  dhcp_message *dhcpoffer = nullptr;

  mac_address = _get_client_id(dhcpmsg);
  _standardize_mac_address(mac_address);
  pData = _search_dhcp_entry(mac_address);
  if (!pData) {
    ACA_LOG_ERROR("DHCP entry does not exist! (mac = %s)\n", mac_address.c_str());
    return;
  }

  dhcpoffer = _pack_dhcp_offer(dhcpmsg, pData);
  if (!dhcpoffer) {
    return;
  }

  dhcps_xmit(in_port, dhcpoffer);
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

  if (inet_pton(AF_INET, pData->ipv4_address.c_str(), &(sa.sin_addr)) != 1) {
    throw std::invalid_argument("Virtual ipv4 address is not in the expect format");
  }
  dhcpoffer->yiaddr = htonl(sa.sin_addr.s_addr);

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

void ACA_Dhcp_Server::_parse_dhcp_request(uint32_t in_port, dhcp_message *dhcpmsg)
{
  string mac_address;
  dhcp_entry_data *pData = nullptr;
  dhcp_message *dhcpack = nullptr;
  dhcp_message *dhcpnak = nullptr;
  struct sockaddr_in sa;
  uint32_t self_sid = 2130706433;

  // Fetch the record in DB
  mac_address = _get_client_id(dhcpmsg);
  _standardize_mac_address(mac_address);
  pData = _search_dhcp_entry(mac_address);
  if (!pData) {
    ACA_LOG_ERROR("DHCP entry does not exist! (mac = %s)\n", mac_address.c_str());
    return;
  }

  //Verify client is requesting to myself
  //Need the fetch self server id here!!
  if (self_sid == _get_server_id(dhcpmsg)) { //request to me
    //Verify the ip address from client is the one assigned in DHCPOFFER
    if (inet_pton(AF_INET, pData->ipv4_address.c_str(), &(sa.sin_addr)) != 1) {
      throw std::invalid_argument("Virtual ipv4 address is not in the expect format");
    }
    if (htonl(sa.sin_addr.s_addr) != _get_requested_ip(dhcpmsg)) {
      ACA_LOG_ERROR("IP address %u in DHCP request is not same as the one in DB!",
                    sa.sin_addr.s_addr);
      dhcpnak = _pack_dhcp_nak(dhcpmsg);
      dhcps_xmit(in_port, dhcpnak);
      return;
    }

    dhcpack = _pack_dhcp_ack(dhcpmsg);
    dhcps_xmit(in_port, dhcpack);

  } else { //not to me
  }
}

dhcp_message *ACA_Dhcp_Server::_pack_dhcp_ack(dhcp_message *dhcpreq)
{
  dhcp_message *dhcpack = nullptr;
  uint8_t *pos = nullptr;
  int opts_len = 0;

  dhcpack = new dhcp_message();

  //Pack DHCP header
  _pack_dhcp_message(dhcpack, dhcpreq);

  dhcpack->yiaddr = htonl(_get_requested_ip(dhcpreq));

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

unsigned short ACA_Dhcp_Server::check_sum(unsigned char *a, int len)
{
  unsigned int sum = 0;
  unsigned short tmp = 16 * 16;
  while (len > 1) {
    unsigned short tmp1 = *a++ * tmp;
    sum += tmp1 + *a++;
    len -= 2;
  }

  if (len) {
    sum += *(unsigned char *)a;
  }

  while (sum >> 16) {
    sum = (sum >> 16) + (sum & 0xffff);
  }

  return (unsigned short)(~sum);
}

string ACA_Dhcp_Server::_serialize_dhcp_message(dhcp_message *dhcpmsg)
{
  string packet;

  if (!dhcpmsg) {
    return string();
  }

  //fix header
  char str[80];
  sprintf(str, "%02x", dhcpmsg->op);
  packet.append(str);
  sprintf(str, "%02x", dhcpmsg->htype);
  packet.append(str);
  sprintf(str, "%02x", dhcpmsg->hlen);
  packet.append(str);
  sprintf(str, "%02x", dhcpmsg->hops);
  packet.append(str);

  sprintf(str, "%08x", htonl(dhcpmsg->xid));
  packet.append(str);
  sprintf(str, "%04x", htons(dhcpmsg->secs));
  packet.append(str);
  sprintf(str, "%04x", htons(dhcpmsg->flags));
  packet.append(str);
  sprintf(str, "%08x", dhcpmsg->ciaddr);
  packet.append(str);
  sprintf(str, "%08x", dhcpmsg->yiaddr);
  packet.append(str);
  sprintf(str, "%08x", htonl(dhcpmsg->siaddr));
  packet.append(str);
  sprintf(str, "%08x", dhcpmsg->giaddr);
  packet.append(str);

  for (int i = 0; i < 16; i++) {
    sprintf(str, "%02x", dhcpmsg->chaddr[i]);
    packet.append(str);
  }
  for (int i = 0; i < 64; i++) {
    sprintf(str, "%02x", dhcpmsg->sname[i]);
    packet.append(str);
  }
  for (int i = 0; i < 128; i++) {
    sprintf(str, "%02x", dhcpmsg->file[i]);
    packet.append(str);
  }

  int len = 28 + 16 + 64 + 128 + 4;

  sprintf(str, "%08x", htonl(dhcpmsg->cookie));
  packet.append(str);

  //options part
  for (int i = 0; i < DHCP_MSG_OPTS_LENGTH;) {
    if (DHCP_OPT_END == dhcpmsg->options[i]) {
      sprintf(str, "%02x", dhcpmsg->options[i]);
      packet.append(str);
      len += 1;
      break;
    }
    sprintf(str, "%02x", dhcpmsg->options[i]);
    packet.append(str);
    sprintf(str, "%02x", dhcpmsg->options[i + 1]);
    packet.append(str);
    len += 2;
    if (DHCP_OPT_LEN_1BYTE == dhcpmsg->options[i + 1]) {
      sprintf(str, "%02x", dhcpmsg->options[i + 2]);
      packet.append(str);
      i += DHCP_OPT_CLV_HEADER + DHCP_OPT_LEN_1BYTE;
      len += 1;
      continue;
    }
    if (DHCP_OPT_LEN_4BYTE == dhcpmsg->options[i + 1]) {
      sprintf(str, "%08x", htonl(*(uint32_t *)(&dhcpmsg->options[i + 2])));
      packet.append(str);
      //packet.append(to_string(htonl(*(uint32_t *)(&dhcpmsg->options[i + 2]))));
      i += DHCP_OPT_CLV_HEADER + DHCP_OPT_LEN_4BYTE;
      len += 4;
      continue;
    }
  }

  struct iphear {
    uint8_t version;
    uint8_t ds;
    uint16_t total_len;
    uint16_t identi;
    uint16_t fregment;
    uint8_t tol;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t udp_checksum;
  };

  struct udphear {
    uint32_t srcIp;
    uint32_t dstIp;
    uint16_t udp_len;
    uint8_t rsv;
    uint8_t protocol;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t cookie;
    uint8_t options[308];
  };

  udphear udphdr;
  udphdr.srcIp = inet_addr("127.0.1.1");
  udphdr.dstIp = inet_addr("255.255.255.255");
  udphdr.udp_len = htons(8 + len);
  udphdr.protocol = 17;
  udphdr.rsv = 0;
  udphdr.src_port = htons(67);
  udphdr.dst_port = htons(68);
  udphdr.len = htons(8 + len);
  udphdr.checksum = htons(0);
  udphdr.op = dhcpmsg->op;
  udphdr.htype = dhcpmsg->htype;
  udphdr.hlen = dhcpmsg->hlen;
  udphdr.hops = dhcpmsg->hops;
  udphdr.xid = dhcpmsg->xid;
  udphdr.secs = htons(dhcpmsg->secs);
  udphdr.flags = htons(dhcpmsg->flags);
  udphdr.ciaddr = htonl(dhcpmsg->ciaddr);
  udphdr.yiaddr = htonl(dhcpmsg->yiaddr);
  udphdr.siaddr = htonl(dhcpmsg->siaddr);
  udphdr.giaddr = htonl(dhcpmsg->giaddr);
  memcpy(udphdr.chaddr, dhcpmsg->chaddr, 16);
  memcpy(udphdr.sname, dhcpmsg->sname, 64);
  memcpy(udphdr.file, dhcpmsg->file, 128);
  //udphdr.chaddr = dhcpmsg->chaddr;
  //udphdr.sname = dhcpmsg->sname;
  //udphdr.file = dhcpmsg->file;
  udphdr.cookie = dhcpmsg->cookie;
  //udphdr.options = dhcpmsg->options;
  memcpy(udphdr.options, dhcpmsg->options, 308);

  iphear iphr;
  iphr.version = 69;
  iphr.ds = 0;
  iphr.total_len = htons(28 + len);
  iphr.identi = htons(0);
  iphr.fregment = htons(16384);
  iphr.tol = 16;
  iphr.protocol = 17;
  iphr.checksum = 0;
  iphr.src_ip = inet_addr("127.0.1.1");
  iphr.dst_ip = inet_addr("255.255.255.255");
  iphr.src_port = htons(67);
  iphr.dst_port = htons(68);
  iphr.len = htons(8 + len);
  iphr.udp_checksum = htons(0);

  iphr.udp_checksum = check_sum((unsigned char *)&udphdr, 20 + len);

  iphr.checksum = check_sum((unsigned char *)&iphr, 20);
  string packet_header;
  packet_header.append("ffffffffffff");
  packet_header.append("60d755f7c209");
  packet_header.append("0800");
  sprintf(str, "%02x", iphr.version);
  packet_header.append(str);
  sprintf(str, "%02x", iphr.ds);
  packet_header.append(str);
  sprintf(str, "%04x", ntohs(iphr.total_len));
  packet_header.append(str);
  sprintf(str, "%04x", ntohs(iphr.identi));
  packet_header.append(str);
  sprintf(str, "%04x", ntohs(iphr.fregment));
  packet_header.append(str);
  sprintf(str, "%02x", iphr.tol);
  packet_header.append(str);
  sprintf(str, "%02x", iphr.protocol);
  packet_header.append(str);
  sprintf(str, "%04x", iphr.checksum);
  packet_header.append(str);
  sprintf(str, "%08x", htonl(iphr.src_ip));
  packet_header.append(str);
  sprintf(str, "%08x", htonl(iphr.dst_ip));
  packet_header.append(str);
  sprintf(str, "%04x", ntohs(iphr.src_port));
  packet_header.append(str);
  sprintf(str, "%04x", ntohs(iphr.dst_port));
  packet_header.append(str);
  sprintf(str, "%04x", ntohs(iphr.len));
  packet_header.append(str);
  sprintf(str, "%04x", iphr.udp_checksum);
  packet_header.append(str);

  packet.insert(0, packet_header);
  return packet;
}

} //namespace aca_dhcp_server
