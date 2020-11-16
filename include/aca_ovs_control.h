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

#ifndef ACA_OVS_CONTROL_H
#define ACA_OVS_CONTROL_H

#define IPTOS_PREC_INTERNETCONTROL 0xc0
#define DSCP_DEFAULT (IPTOS_PREC_INTERNETCONTROL >> 2)
#define STDOUT_FILENO   1   /* Standard output.  */

#include <openvswitch/ofp-errors.h>
#include <openvswitch/ofp-packet.h>
#include <string>

// OVS monitor implementation class
namespace aca_ovs_control
{
class ACA_OVS_Control {
  public:
  static ACA_OVS_Control &get_instance();

  /* 
   * main function to call other function through command line
   * It accepts three options:
   *   -c <commands>, available commands are dump-flows, add-flow, del-flows, mod-flows, monitor, packet_out
   *   -t <target bridge>, eg. br-int, br-tun
   *   -o <additional options>, depends on the command
   */
  int control();

  /* 
   * create a monitor channel connecting to ovs.
   * Input: 
   *    const char *bridge: bridge name
   *    cnost char *opt: option for monitor.  
   * Availabe options: 
   *    <miss-len>: a number larger than 1. 
   *    resume: continually intercept packet-in message
   */
  void monitor(const char *bridge, const char *opt);
  
  /*
   * send a packet back to ovs.
   * Input:
   *    const char *bridge: bridge name
   *    cnost char *opt: option for packet_out
   * Availabe options:
   *    in_port=<in_port>: (required) in_port number on ovs, or controller, local
   *    packet=<hex string>: (required) the hex string representation of a packet 
   *    actions=<actions>: actions can be normal, control, or resumbit(,2) to a table 
   * example:
   *    ACA_OVS_Control::get_instance().packet_out("br-tun", 
   *                            "in_port=controller packet=<hex-string> actions=normal"
   */
  void packet_out(const char *bridge, const char *opt);

  /*
   * check if a flow exists in the ovsdb.
   * Input:
   *    const char *bridge: bridge name
   *    cnost char *flow: flow to be checked
   * Output:
   *    int: EXIT_SUCCESS - flow matched, EXIT_FAILURE - no any flow matched
   * example:
   *    ACA_OVS_Control::get_instance().flow_exists("br-tun", "table=10,ip,nw_dst=192.168.0.1")
   * comment: The function retrives flow without show-stats
   */
  int flow_exists(const char *bridge, const char *flow); 

  /*
   * dump matched flows.
   * Input:
   *    const char *bridge: bridge name
   *    cnost char *opt: flow to be matched. if no opt specified, the function return all flows
   * Output:
   *    int: EXIT_SUCCESS - flow matched, EXIT_FAILURE - no any flow matched
   * example:
   *    ACA_OVS_Control::get_instance().dump_flows("br-tun", "table=10,ip,nw_dst=192.168.0.1")
   * comment: 
   *    The function retrives flow with show-stats
   *    if no flow in opt, this function dump all flows
   */
  int dump_flows(const char *bridge, const char *opt); 

  /*
   * add a flow.
   * Input:
   *    const char *bridge: bridge name
   *    cnost char *opt: flow to be added.
   * Output:
   *    int: EXIT_SUCCESS or EXIT_FAILURE
   * example:
   *    ACA_OVS_Control::get_instance().add_flow("br-tun", "table=1,tcp,nw_dst=192.168.0.1,priority=1,actions=drop")
   * comment: 
   *    actions field is required in the opt.
   */
  int add_flow(const char *bridge, const char *opt);

  /*
   * modify matched flows.
   * Input:
   *    const char *bridge: bridge name
   *    cnost char *opt: flow to be matched and modified field values.
   * Output:
   *    int: EXIT_SUCCESS - flow matched, EXIT_FAILURE - no any flow matched
   * example:
   *    ACA_OVS_Control::get_instance().mod_flows("br-tun", "table=1,tcp,nw_dst=192.168.0.1,priority=1,actions=resubmit(,2)")
   * comment: 
   *    the matching flow uses --strict option. 
   *    actions field is required in the opt.
   */ 
  int mod_flows(const char *bridge, const char *opt); 

  /*
   * delete matched flows.
   * Input:
   *    const char *bridge: bridge name
   *    cnost char *opt: flow to be matched.
   * Output:
   *    int: EXIT_SUCCESS - flow matched, EXIT_FAILURE - no any flow matched
   * example:
   *    ACA_OVS_Control::get_instance().del_flows("br-tun", "table=1,tcp,nw_dst=192.168.0.1,priority=1")
   * comment: 
   *    the matching flow uses --strict option. 
   */ 
  int del_flows(const char *bridge, const char *opt);
 
  /*
   * parse a received packet.
   * Input:
   *    uint32 in_port: the port received the packet
   *    void *packet: packet data.
   * example:
   *    ACA_OVS_Control::get_instance().parse_packet(1, packet) 
   */
  void parse_packet(uint32_t in_port, void *packet);

  /*
   * print out the contents of packet payload data.
   * Input:
   *    const u_char *payload: payload data
   *    int len: payload size.
   * example:
   *    ACA_OVS_Control::get_instance().parse_packet(1, packet)
   */
  void print_payload(const u_char *payload, int len);
  void print_hex_ascii_line(const u_char *payload, int len, int offset);

  void set_oam_server_port(uint32_t port);
  uint32_t get_oam_server_port();

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
  ACA_OVS_Control(ACA_OVS_Control const &) = delete;
  void operator=(ACA_OVS_Control const &) = delete;

  private:
  ACA_OVS_Control(){};
  ~ACA_OVS_Control(){};

  uint32_t oam_server_port = 8300;
};
} // namespace aca_ovs_control
#endif // #ifndef ACA_OVS_CONTROL_H
