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

using namespace std;
using namespace aca_dhcp_programming_if;

namespace aca_dhcp_server
{
ACA_Dhcp_Server::ACA_Dhcp_Server()
{
  try {
    _dhcp_db = new map<string, dhcp_entry_data>;
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

void ACA_Dhcp_Server::_init_dhcp_msg_ops()
{
  _parse_dhcp_msg_ops[DHCP_MSG_NONE] = _parse_dhcp_none;
  _parse_dhcp_msg_ops[DHCP_MSG_DHCPDISCOVER] = _parse_dhcp_discover;
}

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

    if (DHCP_MSG_FD_HTYPE_ETHERNET == dhcpmsg->htype && 6 != dhcpmsg->hlen) {
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
    switch (options[i]) {
    case code:
      return options + i;
    case DHCP_OPT_PAD:
      i++;
      break;
    case DHCP_OPT_END:
      break;
    default:
      i += options[DHCP_OPT_LEN + i] + 2;
      break;
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

  popt->dhcpmsgtype = _get_option(dhcpmsg, DHCP_OPT_MSG_TYPE);
  if (nullptr == popt->dhcpmsgtype) {
    return DHCP_MSG_NONE;
  }

  return popt->dhcpmsgtype->msg_type;
}

void ACA_Dhcp_Server::_parse_dhcp_none(dhcp_message *dhcpmsg)
{
  dhcpmsg = dhcpmsg;
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

  dhcpoffer = new dhcp_message();
}

dhcp_message *
ACA_Dhcp_Server::_pack_dhcp_offer(dhcp_message *dhcpdiscover, dhcp_entry_data *pData)
{
  dhcp_message *dhcpoffer = nullptr;
  struct sockaddr_in sa;
  dhcp_message_options dhcpmsgopts;
  uint8_t *pos = nullptr;
  int index = 0;
  int opts_len = 0;

  //DHCP Fix header
  dhcpoffer = new dhcp_message();
  dhcpoffer->op = BOOTP_MSG_BOOTREPLY;
  dhcpoffer->htype = dhcpdiscover->htype;
  dhcpoffer->hops = dhcpdiscover->hops;
  dhcpoffer->xid = dhcpdiscover->xid;
  dhcpoffer->ciaddr = 0;

  if (inet_pton(AF_INET, pData->ipv4_address, &(sa.sin_addr)) != 1) {
    throw std::invalid_argument("Virtual ipv4 address is not in the expect format");
  }
  dhcpoffer->yiaddr = sa.sin_addr;
  dhcpoffer->siaddr = 0;
  dhcpoffer->giaddr = 0;
  memcpy(dhcpoffer->chaddr, dhcpdiscover->chaddr, 16);

  //DHCP Options
  dhcpoffer->cookie = dhcpdiscover->cookie;
  pos = dhcpoffer->options;
  while (index < DHCP_MSG_OPTS_LENGTH) {
    //DHCP Options: dhcp message type
    PACK_DHCP_MESSAGE_TYPE((dhcp_message_type *)&(dhcpmsgopts.dhcpmsgtype), pos, index);

    //DHCP Options: ip address lease time
    PACK_DHCP_MESSAGE_TYPE((dhcp_message_type *)&(dhcpmsgopts.ipleasetime), pos, index);

    //DHCP Options: server identifier

    //DHCP Options: end
    pos[index] = DHCP_OPT_END;
  }
}

} //namespace aca_dhcp_server
