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

#ifndef ACA_OAM_SERVER_H
#define ACA_OAM_SERVER_H

#include <cstdint>
#include <string>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/ether.h>

#include "goalstateprovisioner.grpc.pb.h"

using namespace std;
namespace aca_oam_server
{
//OAM Message Type
#define SIZE_OP_CODE 8
//#define OAM_SOCKET_PORT 8300
#define OAM_MSG_FLOW_INJECTION (0x0)
#define OAM_MSG_FLOW_DELETION (0x1)
#define OAM_MSG_NONE (0x3)
#define OAM_MSG_MAX (0x3)

struct oam_match {
  string sip;
  string dip;
  string sport;
  string dport;
  string proto;
  string vni;
};

struct oam_action {
  string inst_nw_dst;
  string node_nw_dst;
  string inst_dl_dst;
  string node_dl_dst;
  string idle_timeout;
};

struct flow_inject_msg {
  struct in_addr inner_src_ip; // Inner Packet SIP
  struct in_addr inner_dst_ip; // Inner Packet DIP
  uint16_t src_port; // Inner Packet SPort
  uint16_t dst_port; // Inner Packet DPort
  uint8_t proto; // Inner Packet Protocol
  uint8_t vni[4]; // Inner Packet Protocol
  struct in_addr inst_dst_ip; // Destination Inst IP
  struct in_addr node_dst_ip; // Destination Node IP
  uint8_t inst_dst_mac[6]; // Destination Inst MAC
  uint8_t node_dst_mac[6]; // Destination Node MAC
  uint16_t idle_timeout; // 0 - 65536s
};

struct flow_del_msg {
  struct in_addr inner_src_ip;
  struct in_addr inner_dst_ip;
  uint16_t src_port;
  uint16_t dst_port;
  uint8_t proto;
  uint8_t vni[4];
};

struct oam_message {
  uint32_t op_code;
  union op_data {
    struct flow_inject_msg msg_inject_flow;
    struct flow_del_msg msg_del_flow;
  } data;
};

class ACA_Oam_Server {
  public:
  ACA_Oam_Server();
  ~ACA_Oam_Server();

  static ACA_Oam_Server &get_instance();

  void _init_oam_ofp();

  void _deinit_oam_ofp();

  void oams_recv(void *message);

  uint8_t _get_message_type(oam_message *oammsg);

  oam_match get_oam_match_field(oam_message *oammsg);

  oam_action get_oam_action_field(oam_message *oammsg);

  int add_direct_path(oam_match match, oam_action action);

  int del_direct_path(oam_match match);

  void _init_oam_msg_ops();

  bool _validate_oam_message(oam_message *oammsg);

  void _standardize_mac_address(string &mac_string);

  void _parse_oam_flow_injection(oam_message *oammsg);

  void _parse_oam_flow_deletion(oam_message *oammsg);

  void (aca_oam_server::ACA_Oam_Server ::*_parse_oam_msg_ops[OAM_MSG_MAX])(oam_message *oammsg);

  string _get_mac_addr(uint8_t *mac);

  string get_vpc_id(uint8_t *vni);
};
} // namespace aca_oam_server
#endif // #ifndef ACA_OAM_SERVER_H