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

#define OAM_SOCKET_PORT 8300
#define FLOW_INJECTION (0x00000000)
#define FLOW_DELETION (0x00000001)

#include <cstdint>

struct op_data{
    uint32_t sip; // Inner Packet SIP
    uint32_t dip; // Inner Packet DIP
    uint16_t sport; // Inner Packet SPort
    uint16_t dport; // Inner Packet DPort
    uint8_t proto; // Inner Packet Protocol
    uint32_t vni; // Inner Packet Protocol
    uint32_t dest_inst_ip; // Destination Inst IP
    uint32_t dest_node_ip; // Destination Node IP
    struct eth_addr dest_inst_mac; // Destination Inst MAC
    struct eth_addr dest_node_mac; // Destination Node MAC
    uint64_t idle_timeout; // 0 - 65536s
};

struct OAM_message{
    uint64_t op_code;
    struct op_data *data;
};

namespace aca_oam_server
{
class ACA_Oam_Server {
  public:
  ACA_Oam_Server();
  ~ACA_Oam_Server();

  static ACA_Oam_Server &get_instance();
  
  void oams_recv(uint32_t in_port, void *message);

  uint8_t _get_message_type(OAM_message *oammsg);

  void _parse_oam_msg_ops(uint32_t in_port, OAM_message *oammsg);

  int add_direct_path(const char *bridge, op_data *opdata);

  int del_direct_path(const char *bridge, op_data *opdata);

};
}// namespace aca_oam_server
#endif // #ifndef ACA_OAM_SERVER_H