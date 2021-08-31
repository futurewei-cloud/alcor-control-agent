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

#ifndef ACA_ZETA_OAM_SERVER_H
#define ACA_ZETA_OAM_SERVER_H

#include <cstdint>
#include <string>
#include <arpa/inet.h>
#include <net/ethernet.h>
//#include <netinet/ether.h>
#include "hashmap/HashMap.h"
#include "goalstateprovisioner.grpc.pb.h"

using namespace std;
namespace aca_zeta_oam_server
{
//OAM Message Type
#define OAM_MSG_FLOW_INJECTION (0)
#define OAM_MSG_FLOW_DELETION (1)
#define OAM_MSG_NONE (2)
#define OAM_MSG_MAX (3)

struct oam_match {
  string sip;
  string dip;
  string sport;
  string dport;
  string proto;
  uint vni;
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
  uint8_t vni[3]; // tunnel_id, 3bytes
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
  uint8_t vni[3];
};

struct oam_message {
  uint32_t op_code;
  union op_data {
    struct flow_inject_msg msg_inject_flow;
    struct flow_del_msg msg_del_flow;
  } data;
};

class ACA_Zeta_Oam_Server {
  public:
  ACA_Zeta_Oam_Server();
  ~ACA_Zeta_Oam_Server();

  static ACA_Zeta_Oam_Server &get_instance();
  void oams_recv(uint32_t udp_dport, void *message);

  private:
  uint8_t _get_message_type(oam_message *oammsg);
  string _get_mac_addr(uint8_t *mac);
  uint _get_tunnel_id(uint8_t *vni);
  bool _validate_oam_message(oam_message *oammsg);
  bool _check_oam_server_port(uint32_t udp_dport, oam_match match);
  oam_match _get_oam_match_field(oam_message *oammsg);
  oam_action _get_oam_action_field(oam_message *oammsg);

  int _add_direct_path(oam_match match, oam_action action);
  int _del_direct_path(oam_match match);

  void _init_oam_msg_ops();
  void _parse_oam_flow_injection(uint32_t udp_dport, oam_message *oammsg);
  void _parse_oam_flow_deletion(uint32_t udp_dport, oam_message *oammsg);
  void _parse_oam_none(uint32_t /* in_port */, oam_message *oammsg);

  void (aca_zeta_oam_server::ACA_Zeta_Oam_Server ::*_parse_oam_msg_ops[OAM_MSG_MAX])(
          uint32_t udp_dpost, oam_message *oammsg);
};
} // namespace aca_zeta_oam_server
#endif // #ifndef ACA_Zeta_OAM_SERVER_H