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
  string subnet_mask;
  string gateway_address;
  string dns_addresses[DHCP_MSG_OPTS_DNS_LENGTH];
};

#define DHCP_ENTRY_DATA_SET(pData, pCfg)                                       \
  do {                                                                         \
    (pData)->ipv4_address = (pCfg)->ipv4_address;                              \
    (pData)->port_host_name = (pCfg)->port_host_name;                          \
    (pData)->subnet_mask = (pCfg)->subnet_mask;                                \
    (pData)->gateway_address = (pCfg)->gateway_address;                        \
    for (int i = 0; i < DHCP_MSG_OPTS_DNS_LENGTH; i++) {                       \
      if ((pCfg)->dns_addresses[i].size() <= 0) {                                \
        break;                                                                 \
      }                                                                        \
      (pData)->dns_addresses[i] = (pCfg)->dns_addresses[i];                        \
    }                                                                          \
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
#define DHCP_MSG_SERVER_ID (0x7f000001) //Hard coded for 127.0.0.1

#pragma pack(push, 1)

// define ip header
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

// define upd header
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

//DHCP message ip header field
#define DHCP_MSG_IP_HEADER_SRC_IP (0x7f000101) // hard code for dhcp src ip 127.0.1.1
#define DHCP_MSG_IP_HEADER_DEST_IP (0xffffffff)
#define DHCP_MSG_IP_HEADER_SRC_PORT (67)
#define DHCP_MSG_IP_HEADER_DEST_PORT (68)
#define DHCP_MSG_IP_HEADER_VERSION (69) //ip version 4 + ip header length 0100 + 0101(length is base 4 bytes, all length=5*4=20)
#define DHCP_MSG_IP_HEADER_PROTOCOL (17) //udp protocol
#define DHCP_MSG_IP_HEADER_FREGMENT (0x4000) //don't fregment
#define DHCP_MSG_IP_HEADER_TOL (16) //time to live
#define DHCP_MSG_IP_HEADER_IDENTI (0) //identification 
#define DHCP_MSG_IP_HEADER_DS (0) //different service field

//DHCP message l2 layer
#define DHCP_MSG_L2_HEADER_DEST_MAC ("ffffffffffff")
#define DHCP_MSG_L2_HEADER_SRC_MAC ("60d755f7c209") // hard code for l2 src mac 60:d7:55:f7:c2:09
#define DHCP_MSG_L2_HEADER_TYPE ("0800")

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
#define DHCP_OPT_CODE_CLIENT_ID (0x3d)
#define DHCP_OPT_CODE_SUBNET_MASK (0x1)
#define DHCP_OPT_CODE_ROUTER (0x3)
#define DHCP_OPT_CODE_DNS_NAME_SERVER (0x6)
#define DHCP_OPT_CODE_DNS_DOMAIN_NAME (0xf)

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

struct dhcp_subnet_mask {
  uint8_t code;
  uint8_t len;
  uint32_t subnet_mask;
};

struct dhcp_router {
  uint8_t code;
  uint8_t len;
  uint32_t router_address;
};

struct dhcp_dns {
  uint8_t code;
  uint8_t len;
  uint8_t dns[DHCP_MSG_OPTS_DNS_LENGTH*4]; // expand for dns array to 4 power,ip = 4*bytes
};

struct dhcp_req_ip {
  uint8_t code;
  uint8_t len;
  uint32_t req_ip;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
struct dhcp_client_id {
  uint8_t code;
  uint8_t len;
  uint8_t type;
  uint8_t cid[0];
};
#pragma GCC diagnostic pop
#pragma pack(pop)

union dhcp_message_options {
  dhcp_message_type *dhcpmsgtype;
  dhcp_ip_lease_time *ipleasetime;
  dhcp_server_id *serverid;
  dhcp_req_ip *reqip;
  dhcp_client_id *clientid;
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
  void dhcps_recv(uint32_t in_port, void *message);
  void dhcps_xmit(uint32_t in_port, void *message);

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
  void _standardize_mac_address(string &mac_string);

  /**************** Data plane operations *********************/
  int _validate_dhcp_message(dhcp_message *dhcpmsg);
  void _init_dhcp_msg_ops();
  uint8_t *_get_option(dhcp_message *dhcpmsg, uint8_t code);
  uint8_t _get_message_type(dhcp_message *dhcpmsg);
  uint32_t _get_server_id(dhcp_message *dhcpmsg);
  uint32_t _get_requested_ip(dhcp_message *dhcpmsg);
  string _get_client_id(dhcp_message *dhcpmsg);

  void _parse_dhcp_none(uint32_t in_port, dhcp_message *dhcpmsg);
  void _parse_dhcp_discover(uint32_t in_port, dhcp_message *dhcpmsg);
  void _parse_dhcp_request(uint32_t in_port, dhcp_message *dhcpmsg);

  dhcp_message *_pack_dhcp_offer(dhcp_message *dhcpdiscover, dhcp_entry_data *pData);
  dhcp_message *_pack_dhcp_ack(dhcp_message *dhcpreq, dhcp_entry_data *pData);
  dhcp_message *_pack_dhcp_nak(dhcp_message *dhcpreq);
  void _pack_dhcp_message(dhcp_message *rpl, dhcp_message *req);
  void _pack_dhcp_header(dhcp_message *dhcpmsg);
  void _pack_dhcp_opt_msgtype(uint8_t *option, uint8_t msg_type);
  void _pack_dhcp_opt_ip_lease_time(uint8_t *option, uint32_t lease);
  void _pack_dhcp_opt_server_id(uint8_t *option, uint32_t server_id);
  void _pack_dhcp_opt_subnet_mask(uint8_t *option, string subnet_mask);
  void _pack_dhcp_opt_router(uint8_t *option, string router_address);
  int _pack_dhcp_opt_dns(uint8_t *option, string dns_addresses[]);

  unsigned short check_sum(unsigned char *a, int len);
  string _serialize_dhcp_message(dhcp_message *dhcpmsg);
  string _serialize_dhcp_ip_header_message(dhcp_message *dhcpmsg, int dhcp_message_len);

  /****************** Private variables ******************/
  int _dhcp_entry_thresh;
  int _get_db_size() const;
#define DHCP_DB_SIZE _get_db_size()

  std::unordered_map<std::string, dhcp_entry_data> *_dhcp_db;
  std::mutex _dhcp_db_mutex;

  void (aca_dhcp_server::ACA_Dhcp_Server ::*_parse_dhcp_msg_ops[DHCP_MSG_MAX])(
          uint32_t in_port, dhcp_message *dhcpmsg);
};

} // namespace aca_dhcp_server
#endif // #ifndef ACA_DHCP_SERVER_H
