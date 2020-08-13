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

#ifndef ACA_DHCP_SERVER_H
#define ACA_DHCP_SERVER_H

#include "aca_dhcp_programming_if.h"
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <cstdlib>
#include <string>

using namespace aca_dhcp_programming_if;

// dhcp server implementation class
namespace aca_dhcp_server
{
struct dhcp_entry_data {
  string ipv4_address;
  string ipv6_address;
  string port_host_name;
};

#define DHCP_ENTRY_DATA_SET(pData, pCfg)                                       \
  do {                                                                         \
    (pData)->ipv4_address = (pCfg)->ipv4_address;                              \
    (pData)->port_host_name = (pCfg)->port_host_name;                          \
  } while (0)

//BOOTP Message Type
#define BOOTP_MSG_BOOTREQUEST (0x1)
#define BOOTP_MSG_BOOTREPLY (0x2)

//DHCP Message Type
#define DHCP_MSG_NONE (0x0)
#define DHCP_MSG_DHCPDISCOVER (0x1)
#define DHCP_MSG_DHCPOFFER (0x2)
#define DHCP_MSG_DHCPREQUEST (0x3)
#define DHCP_MSG_DHCPDECLINE (0x4)
#define DHCP_MSG_DHCPACK (0x5)
#define DHCP_MSG_DHCPNAK (0x6)
#define DHCP_MSG_DHCPRELEASE (0x7)
#define DHCP_MSG_DHCPINFORM (0x8)
#define DHCP_MSG_MAX (0x9)

//DHCP Message Fields
#define DHCP_MSG_OPTS_LENGTH (308)
#define DHCP_MSG_HWTYPE_ETH (0x1)
#define DHCP_MSG_HWTYPE_ETH_LEN (0x6)
#define DHCP_MSG_MAGIC_COOKIE (0x63825363) //magic cookie per RFC2131

struct dhcp_message {
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
  uint8_t options[DHCP_MSG_OPTS_LENGTH]; // 321 - cookie
};

// DHCP Message Options Code
#define DHCP_OPT_PAD (0x0)
#define DHCP_OPT_END (0xff)
#define DHCP_OPT_LEN_1BYTE (0x1)
#define DHCP_OPT_LEN_4BYTE (0x4)
#define DHCP_OPT_CODE_MSGTYPE (0x35)
#define DHCP_OPT_CODE_IP_LEASE_TIME (0x33)
#define DHCP_OPT_CODE_SERVER_ID (0x36)
#define DHCP_OPT_CODE_REQ_IP (0x32)

#define DHCP_OPT_CLV_HEADER (0x2) //CLV = Code + Length + Value
#define DHCP_OPT_DEFAULT_IP_LEASE_TIME (86400) //One day

struct dhcp_message_type {
  uint8_t code;
  uint8_t len;
  uint8_t msg_type;
};

struct dhcp_ip_lease_time {
  uint8_t code;
  uint8_t len;
  uint32_t lease_time;
};

struct dhcp_server_id {
  uint8_t code;
  uint8_t len;
  uint32_t sid;
};

struct dhcp_req_ip {
  uint8_t code;
  uint8_t len;
  uint32_t req_ip;
};

union dhcp_message_options {
  dhcp_message_type *dhcpmsgtype;
  dhcp_ip_lease_time *ipleasetime;
  dhcp_server_id *serverid;
  dhcp_req_ip *reqip;
};

class ACA_Dhcp_Server : public aca_dhcp_programming_if::ACA_Dhcp_Programming_Interface {
  public:
  ACA_Dhcp_Server();
  virtual ~ACA_Dhcp_Server();

  static ACA_Dhcp_Server &get_instance();

  /* Management Plane Ops */
  int add_dhcp_entry(dhcp_config *dhcp_cfg_in);
  int update_dhcp_entry(dhcp_config *dhcp_cfg_in);
  int delete_dhcp_entry(dhcp_config *dhcp_cfg_in);

  /* Dataplane Ops */
  void dhcps_recv(void *message);
  void dhcps_xmit(void *message);

  private:
  /*************** Initialization and De-initialization ***********************/
  void _init_dhcp_db();
  void _deinit_dhcp_db();
  void _init_dhcp_ofp();
  void _deinit_dhcp_ofp();

  /*************** Management plane operations ***********************/
  dhcp_entry_data *_search_dhcp_entry(string mac_address);
  void _validate_mac_address(const char *mac_string);
  void _validate_ipv4_address(const char *ip_address);
  void _validate_ipv6_address(const char *ip_address);
  int _validate_dhcp_entry(dhcp_config *dhcp_cfg_in);

  /**************** Data plane operations *********************/
  int _validate_dhcp_message(dhcp_message *dhcpmsg);
  void _init_dhcp_msg_ops();
  uint8_t *_get_option(dhcp_message *dhcpmsg, uint8_t code);
  uint8_t _get_message_type(dhcp_message *dhcpmsg);
  uint32_t _get_server_id(dhcp_message *dhcpmsg);
  uint32_t _get_requested_ip(dhcp_message *dhcpmsg);

  void _parse_dhcp_none(dhcp_message *dhcpmsg);
  void _parse_dhcp_discover(dhcp_message *dhcpmsg);
  void _parse_dhcp_request(dhcp_message *dhcpmsg);

  dhcp_message *_pack_dhcp_offer(dhcp_message *dhcpdiscover, dhcp_entry_data *pData);
  dhcp_message *_pack_dhcp_ack(dhcp_message *dhcpreq);
  dhcp_message *_pack_dhcp_nak(dhcp_message *dhcpreq);
  void _pack_dhcp_message(dhcp_message *rpl, dhcp_message *req);
  void _pack_dhcp_header(dhcp_message *dhcpmsg);
  void _pack_dhcp_opt_msgtype(uint8_t *option, uint8_t msg_type);
  void _pack_dhcp_opt_ip_lease_time(uint8_t *option, uint32_t lease);
  void _pack_dhcp_opt_server_id(uint8_t *option, uint32_t server_id);

  string _serialize_dhcp_message(dhcp_message *dhcpmsg);

  /****************** Private variables ******************/
  int _dhcp_entry_thresh;
  int _get_db_size() const;
#define DHCP_DB_SIZE _get_db_size()

  std::unordered_map<std::string, dhcp_entry_data> *_dhcp_db;
  std::mutex _dhcp_db_mutex;

  void (aca_dhcp_server::ACA_Dhcp_Server ::*_parse_dhcp_msg_ops[DHCP_MSG_MAX])(dhcp_message *dhcpmsg);
};

} // namespace aca_dhcp_server
#endif // #ifndef ACA_DHCP_SERVER_H
