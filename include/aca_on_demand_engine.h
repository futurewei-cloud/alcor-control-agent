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

#ifndef ACA_ON_DEMAND_ENGINE_H
#define ACA_ON_DEMAND_ENGINE_H

#define IPTOS_PREC_INTERNETCONTROL 0xc0
#define DSCP_DEFAULT (IPTOS_PREC_INTERNETCONTROL >> 2)
#define STDOUT_FILENO   1   /* Standard output.  */

#include "common.pb.h"
#include <openvswitch/ofp-errors.h>
#include <openvswitch/ofp-packet.h>
#include <string>

using namespace alcor::schema;

// ACA on-demand engine implementation class
namespace aca_on_demand_engine
{
class ACA_On_Demand_Engine {
  public:
  static ACA_On_Demand_Engine &get_instance();
 
  /*
   * parse a received packet.
   * Input:
   *    uint32 in_port: the port received the packet
   *    void *packet: packet data.
   * example:
   *    ACA_ON_Demand_Engine::get_instance().parse_packet(1, packet) 
   */
  void parse_packet(uint32_t in_port, void *packet);

  /*
   * print out the contents of packet payload data.
   * Input:
   *    const u_char *payload: payload data
   *    int len: payload size.
   * example:
   *    Aca_On_Demand_Engine::get_instance().print_payload(packet, len)
   */
  void print_payload(const u_char *payload, int len);
  void print_hex_ascii_line(const u_char *payload, int len, int offset);
  void on_demand(OperationStatus status, uint32_t in_port, void *packet, 
                 int packet_size, Protocol protocol);
  OperationStatus unknown_recv(uint16_t vlan_id, string ip_src, string ip_dest,
                                int port_src, int port_dest, Protocol protocol);

  /* ethernet headers are always exactly 14 bytes [1] */
  #define SIZE_ETHERNET 14

  /* IP header */
  struct sniff_ip {
          u_char  ip_vhl;                 /* version << 4 | header length >> 2 */
          u_char  ip_tos;                 /* type of service */
          u_short ip_len;                 /* total length */
          u_short ip_id;                  /* identification */
          u_short ip_off;                 /* fragment offset field */
          #define IP_RF 0x8000            /* reserved fragment flag */
          #define IP_DF 0x4000            /* dont fragment flag */
          #define IP_MF 0x2000            /* more fragments flag */
          #define IP_OFFMASK 0x1fff       /* mask for fragmenting bits */
          u_char  ip_ttl;                 /* time to live */
          u_char  ip_p;                   /* protocol */
          u_short ip_sum;                 /* checksum */
          struct in_addr ip_src, ip_dst;  /* source and dest address */
  };
  /* The second-half of the first byte in ip_header contains the IP header length (IHL). */
  #define IP_HL(ip)               (((ip)->ip_vhl) & 0x0f)
  /* The IHL is number of 32-bit segments. Multiply by four to get a byte count for pointer arithmetic */
  #define IP_V(ip)                (((ip)->ip_vhl) >> 4)

  /* TCP header */
  typedef u_int tcp_seq;

  struct sniff_tcp {
          u_short th_sport;               /* source port */
          u_short th_dport;               /* destination port */
          tcp_seq th_seq;                 /* sequence number */
          tcp_seq th_ack;                 /* acknowledgement number */
          u_char  th_offx2;               /* data offset, rsvd */
  #define TH_OFF(th)      (((th)->th_offx2 & 0xf0) >> 4)
          u_char  th_flags;
          #define TH_FIN  0x01
          #define TH_SYN  0x02
          #define TH_RST  0x04
          #define TH_PUSH 0x08
          #define TH_ACK  0x10
          #define TH_URG  0x20
          #define TH_ECE  0x40
          #define TH_CWR  0x80
          #define TH_FLAGS        (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
          u_short th_win;                 /* window */
          u_short th_sum;                 /* checksum */
          u_short th_urp;                 /* urgent pointer */
  };

  /* UDP protocol header. */
  struct sniff_udp {
          u_short uh_sport;               /* source port */
          u_short uh_dport;               /* destination port */
          u_short uh_ulen;                /* udp length */
          u_short uh_sum;                 /* udp checksum */
  };

  // compiler will flag the error when below is called.
  ACA_On_Demand_Engine(ACA_On_Demand_Engine const &) = delete;
  void operator=(ACA_On_Demand_Engine const &) = delete;

  private:
  ACA_On_Demand_Engine(){};
  ~ACA_On_Demand_Engine(){};
};
} // namespace aca_on_demand_engine
#endif // #ifndef ACA_ON_DEMAND_ENGINE_H
