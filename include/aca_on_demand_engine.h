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

#ifndef ACA_ON_DEMAND_ENGINE_H
#define ACA_ON_DEMAND_ENGINE_H

#define IPTOS_PREC_INTERNETCONTROL 0xc0
#define DSCP_DEFAULT (IPTOS_PREC_INTERNETCONTROL >> 2)
#define STDOUT_FILENO 1 /* Standard output.  */

#include "common.pb.h"
#include <openvswitch/ofp-errors.h>
#include <openvswitch/ofp-packet.h>
#include <string>
#include <thread>
#include "hashmap/HashMap.h"
#include <grpcpp/grpcpp.h>
#include <unordered_map>

using namespace alcor::schema;
using namespace std;
// using namespace grpc;
struct data_for_on_demand_call {
  //     alcor::schema::OperationStatus on_demand_reply;
  uint32_t in_port;
  void *packet;
  int packet_size;
  alcor::schema::Protocol protocol;
};
// ACA on-demand engine implementation class
namespace aca_on_demand_engine
{
class ACA_On_Demand_Engine {
  public:
  std::thread *on_demand_reply_processing_thread;
  grpc::CompletionQueue cq_;
  unordered_map<std::string, data_for_on_demand_call *, std::hash<std::string> > request_uuid_on_demand_data_map;
  unordered_map<std::string, std::chrono::_V2::steady_clock::time_point *, std::hash<std::string> > uuid_call_ncm_time_map;
  unordered_map<std::string, std::chrono::_V2::steady_clock::time_point *, std::hash<std::string> > uuid_ncm_reply_time_map;
  unordered_map<std::string, std::chrono::_V2::steady_clock::time_point *, std::hash<std::string> > uuid_wait_done_time_map;

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
  void on_demand(string uuid_for_call, OperationStatus status, uint32_t in_port,
                 void *packet, int packet_size, Protocol protocol);
  void unknown_recv(uint16_t vlan_id, string ip_src, string ip_dest, int port_src,
                    int port_dest, Protocol protocol, char *uuid_str);
  void process_async_grpc_replies();
/* ethernet headers are always exactly 14 bytes [1] */
#define SIZE_ETHERNET 14

  /* IP header */
  struct sniff_ip {
    u_char ip_vhl; /* version << 4 | header length >> 2 */
    u_char ip_tos; /* type of service */
    u_short ip_len; /* total length */
    u_short ip_id; /* identification */
    u_short ip_off; /* fragment offset field */
#define IP_RF 0x8000 /* reserved fragment flag */
#define IP_DF 0x4000 /* dont fragment flag */
#define IP_MF 0x2000 /* more fragments flag */
#define IP_OFFMASK 0x1fff /* mask for fragmenting bits */
    u_char ip_ttl; /* time to live */
    u_char ip_p; /* protocol */
    u_short ip_sum; /* checksum */
    struct in_addr ip_src, ip_dst; /* source and dest address */
  };
/* The second-half of the first byte in ip_header contains the IP header length (IHL). */
#define IP_HL(ip) (((ip)->ip_vhl) & 0x0f)
/* The IHL is number of 32-bit segments. Multiply by four to get a byte count for pointer arithmetic */
#define IP_V(ip) (((ip)->ip_vhl) >> 4)

  /* TCP header */
  typedef u_int tcp_seq;

  struct sniff_tcp {
    u_short th_sport; /* source port */
    u_short th_dport; /* destination port */
    tcp_seq th_seq; /* sequence number */
    tcp_seq th_ack; /* acknowledgement number */
    u_char th_offx2; /* data offset, rsvd */
#define TH_OFF(th) (((th)->th_offx2 & 0xf0) >> 4)
    u_char th_flags;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
#define TH_ECE 0x40
#define TH_CWR 0x80
#define TH_FLAGS (TH_FIN | TH_SYN | TH_RST | TH_ACK | TH_URG | TH_ECE | TH_CWR)
    u_short th_win; /* window */
    u_short th_sum; /* checksum */
    u_short th_urp; /* urgent pointer */
  };

  /* UDP protocol header. */
  struct sniff_udp {
    u_short uh_sport; /* source port */
    u_short uh_dport; /* destination port */
    u_short uh_ulen; /* udp length */
    u_short uh_sum; /* udp checksum */
  };

  // compiler will flag the error when below is called.
  ACA_On_Demand_Engine(ACA_On_Demand_Engine const &) = delete;
  void operator=(ACA_On_Demand_Engine const &) = delete;

  private:
  ACA_On_Demand_Engine()
  {
    std::cout << "Constructor of a new on demand engine, need to create a new thread to process the grpc replies"
              << std::endl;
    on_demand_reply_processing_thread = new std::thread(
            std::bind(&ACA_On_Demand_Engine::process_async_grpc_replies, this));
    on_demand_reply_processing_thread->detach();
  };
  ~ACA_On_Demand_Engine()
  {
    cq_.Shutdown();
    request_uuid_on_demand_data_map.clear();
    delete on_demand_reply_processing_thread;
  };
};
} // namespace aca_on_demand_engine
#endif // #ifndef ACA_ON_DEMAND_ENGINE_H
