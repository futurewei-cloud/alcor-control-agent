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

#include "aca_config.h"
#include "aca_net_config.h"
#include "aca_on_demand_engine.h"
#include "aca_vlan_manager.h"
#include "aca_ovs_control.h"
#include "aca_grpc.h"
#include "aca_log.h"
#include "aca_util.h"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <chrono>
#include <errno.h>
#include <stdexcept>
#include <inttypes.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>
#include "goalstateprovisioner.pb.h"
#include "aca_dhcp_server.h"
#include "aca_arp_responder.h"

using namespace std;
using namespace aca_vlan_manager;
using namespace aca_arp_responder;
using namespace alcor::schema;

extern std::atomic_ulong g_total_execute_system_time;
extern bool g_demo_mode;
extern GoalStateProvisionerImpl *g_grpc_server;

namespace aca_on_demand_engine
{
ACA_On_Demand_Engine &ACA_On_Demand_Engine::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static ACA_On_Demand_Engine instance;
  return instance;
}

OperationStatus ACA_On_Demand_Engine::unknown_recv(uint16_t vlan_id, string ip_src, string ip_dest,
                                        int port_src, int port_dest, Protocol protocol) {
  HostRequest HostRequest_builder;
  HostRequest_ResourceStateRequest *new_state_requests = HostRequest_builder.add_state_requests();
  HostRequestReply hostRequestReply;
  HostRequestReply_HostRequestOperationStatus hostOperationStatus;
  OperationStatus replyStatus = OperationStatus::FAILURE;
  
  uuid_t uuid;
  uuid_generate_time(uuid);
  char uuid_str[37];
  uuid_unparse_lower(uuid, uuid_str);
  uint tunnel_id = ACA_Vlan_Manager::get_instance().get_tunnelId_by_vlanId(vlan_id);
  new_state_requests->set_request_id(uuid_str);
  new_state_requests->set_tunnel_id(tunnel_id);
  new_state_requests->set_source_ip(ip_src);
  new_state_requests->set_source_port(port_src);
  new_state_requests->set_destination_ip(ip_dest);
  new_state_requests->set_destination_port(port_dest);
  new_state_requests->set_protocol(protocol);
  new_state_requests->set_ethertype(EtherType::IPV4);
  
  hostRequestReply = g_grpc_server->RequestGoalStates(&HostRequest_builder);
  for (int i = 0; i < hostRequestReply.operation_statuses_size(); i++) {
    hostOperationStatus = hostRequestReply.operation_statuses(i);
    replyStatus = hostOperationStatus.operation_status();
  }
  return replyStatus;
}

void ACA_On_Demand_Engine::on_demand(OperationStatus status, uint32_t in_port, void *packet, int packet_size) 
{
  string bridge = "br-tun";
  string inport = "in_port=controller";
  string whitespace = " ";
  string action = "actions=output:" + to_string(in_port);
  string rs_action = "actions=resubmit(,20)";
  string packetpre = "packet=";
  string options;
  string serialized_packet = "";  
  const struct ether_header *eth_header = (struct ether_header *)packet;
  const u_char *ch = (const u_char *) packet;
  char str[10];
  
  if (status == OperationStatus::SUCCESS) {
    for (int i = 0; i < packet_size; i++) {
      sprintf(str, "%02x", *ch);
      serialized_packet.append(str);
    }
    options = inport + whitespace + packetpre + serialized_packet + whitespace + action;
    aca_ovs_control::ACA_OVS_Control::get_instance().packet_out(bridge.c_str(), options.c_str());
  } else {
    ACA_LOG_ERROR("Packet dropped from %s to %s\n", ether_ntoa((ether_addr *)&eth_header->ether_shost),
                                                    ether_ntoa((ether_addr *)&eth_header->ether_dhost));
  }
}

void ACA_On_Demand_Engine::parse_packet(uint32_t in_port, void *packet)
{
  const struct ether_header *eth_header;
  /* The packet is larger than the ether_header struct,
    but we just want to look at the first part of the packet
    that contains the header. We force the compiler
    to treat the pointer to the packet as just a pointer
    to the ether_header. The data payload of the packet comes
    after the headers. Different packet types have different header
    lengths though, but the ethernet header is always the same (14 bytes) */
  eth_header = (struct ether_header *)packet;

  ACA_LOG_INFO("Source Mac: %s\n", ether_ntoa((ether_addr *)&eth_header->ether_shost));

  int vlan_len = 0;
  unsigned char *vlan_hdr = nullptr;
  char *base = (char *)packet;
  uint16_t vlan_id = 0;
  vlan_message *vlanmsg = nullptr;
  string ip_src, ip_dest;
  int port_src, port_dest;
  Protocol _protocol = Protocol::TCP;
  OperationStatus on_demand_reply = OperationStatus::FAILURE;

  uint16_t ether_type = ntohs(*(uint16_t *)(base + 12));
  if (ether_type == ETHERTYPE_VLAN) {
    ACA_LOG_INFO("%s", "Ethernet Type: 802.1Q VLAN tagging (0x8100) \n");
    ether_type = ntohs(*(uint16_t *)(base + 16));
    vlan_len = 4;
    vlan_hdr = (unsigned char *)(base + 12);
    vlanmsg = (vlan_message *)vlan_hdr;
    // get the vlan id from vlan header
    if (vlanmsg) {
      vlan_id = ntohs(vlanmsg->vlan_tci) & 0x0fff;
    }
  }

  if (ether_type == ETHERTYPE_ARP) {
    ACA_LOG_INFO("%s", "Ethernet Type: ARP (0x0806) \n");
    ACA_LOG_INFO("   From: %s\n", inet_ntoa(*(in_addr *)(base + 14 + vlan_len + 14)));
    ACA_LOG_INFO("     to: %s\n", inet_ntoa(*(in_addr *)(base + 14 + vlan_len + 14 + 10)));
    /* compute arp message offset */
    unsigned char *arp_hdr= (unsigned char *)(base + SIZE_ETHERNET + vlan_len);
    /* arp request procedure,type = 1 */
    if(ntohs(*(uint16_t *)(arp_hdr + 6)) == 0x0001){
      aca_arp_responder::ACA_ARP_Responder::get_instance().arp_recv(in_port,vlan_hdr,arp_hdr);
    }
  } else if (ether_type == ETHERTYPE_IP) {
    ACA_LOG_INFO("%s", "Ethernet Type: IP (0x0800) \n");
  } else if (ether_type == ETHERTYPE_REVARP) {
    ACA_LOG_INFO("%s", "Ethernet Type: REVARP (0x8035) \n");
  } else {
    ACA_LOG_INFO("%s", "Ethernet Type: Cannot Tell!\n");
    return;
  }

  /* define/compute ip header offset */
  const struct sniff_ip *ip = (struct sniff_ip *)(base + SIZE_ETHERNET + vlan_len);
  int size_ip = IP_HL(ip) * 4;

  if (size_ip < 20) {
    ACA_LOG_ERROR("size_udp < 20: %d bytes\n", size_ip);
    return;
  } else {
    ip_src = string(inet_ntoa(ip->ip_src));
    ip_dest = string(inet_ntoa(ip->ip_dst));

    /* print source and destination IP addresses */
    ACA_LOG_INFO("       From: %s\n", inet_ntoa(ip->ip_src));
    ACA_LOG_INFO("         To: %s\n", inet_ntoa(ip->ip_dst));

    /* determine protocol */
    switch (ip->ip_p) {
    case IPPROTO_TCP:
      ACA_LOG_INFO("%s", "   Protocol: TCP\n");
      break;
    case IPPROTO_UDP:
      ACA_LOG_INFO("%s", "   Protocol: UDP\n");
      break;
    case IPPROTO_ICMP:
      ACA_LOG_INFO("%s", "   Protocol: ICMP\n");
      break;
    case IPPROTO_IP:
      ACA_LOG_INFO("%s", "   Protocol: IP\n");
      break;
    default:
      ACA_LOG_INFO("%s", "   Protocol: unknown\n");
    }
  }

  if (ip->ip_p == IPPROTO_TCP) {
    /* define/compute tcp header offset */
    const struct sniff_tcp *tcp =
            (struct sniff_tcp *)(base + SIZE_ETHERNET + vlan_len + size_ip);
    //const unsigned char *payload;
    int size_payload;
    int size_tcp = TH_OFF(tcp) * 4;
    _protocol = Protocol::TCP;

    if (size_tcp < 20) {
      ACA_LOG_ERROR("size_tcp < 20: %d bytes \n", size_tcp);
      return;
    } else {
      port_src = ntohs(tcp->th_sport);
      port_dest = ntohs(tcp->th_dport);

      ACA_LOG_INFO("   Src port: %d\n", ntohs(tcp->th_sport));
      ACA_LOG_INFO("   Dst port: %d\n", ntohs(tcp->th_dport));

      /* define/compute tcp payload (segment) offset */
      //payload = (u_char *)(base + SIZE_ETHERNET + vlan_len + size_ip + size_tcp);

      /* compute tcp payload (segment) size */
      size_payload = ntohs(ip->ip_len) - (size_ip + size_tcp);

      /* Print payload data; */
      if (size_payload > 0) {
        ACA_LOG_INFO("   Payload (%d bytes):\n", size_payload);
        // print_payload(payload, size_payload);
      }
    }
  } else if (ip->ip_p == IPPROTO_UDP) {
    /* define/compute udp header offset */
    const struct sniff_udp *udp =
            (struct sniff_udp *)(base + SIZE_ETHERNET + vlan_len + size_ip);
    const unsigned char *payload;
    int size_payload;
    int size_udp = ntohs(udp->uh_ulen);
    _protocol = Protocol::UDP;

    if (size_udp < 20) {
      ACA_LOG_ERROR("size_udp < 20: %d bytes \n", size_udp);
      return;
    } else {
      port_src = ntohs(udp->uh_sport);
      port_dest = ntohs(udp->uh_dport);
      ACA_LOG_INFO("   Src port: %d\n", port_src);
      ACA_LOG_INFO("   Dst port: %d\n", port_dest);

      /* define/compute udp payload (daragram) offset */
      payload = (u_char *)(base + SIZE_ETHERNET + vlan_len + size_ip + 8);

      /* compute udp payload (datagram) size */
      size_payload = ntohs(ip->ip_len) - (size_ip + 8);

      /* Print payload data. */
      if (size_payload > 0) {
        ACA_LOG_INFO("   Payload (%d bytes):\n", size_payload);
        //print_payload(payload, size_payload);
      }

      /* dhcp message procedure */
      if (port_src == 68 && port_dest == 67) {
        ACA_LOG_INFO("%s", "   Message Type: DHCP\n");
        aca_dhcp_server::ACA_Dhcp_Server::get_instance().dhcps_recv(
                in_port, const_cast<unsigned char *>(payload));
      }
    }
  } else if (ip->ip_p == IPPROTO_ICMP) {
    _protocol = Protocol::ICMP;
  }
  
  on_demand_reply = unknown_recv(vlan_id, ip_src, ip_dest, port_src, port_dest, _protocol);
  on_demand(on_demand_reply, in_port, packet, SIZE_ETHERNET + vlan_len + size_ip + 4);
}

/*
 * print packet payload data (avoid printing binary data)
 */
void ACA_On_Demand_Engine::print_payload(const u_char *payload, int len)
{
  int len_rem = len;
  int line_width = 16; /* number of bytes per line */
  int line_len;
  int offset = 0; /* zero-based offset counter */
  const u_char *ch = payload;

  if (len <= 0)
    return;

  /* data fits on one line */
  if (len <= line_width) {
    print_hex_ascii_line(ch, len, offset);
  } else {
    /* data spans multiple lines */
    for (;;) {
      /* compute current line length */
      line_len = line_width % len_rem;
      /* print line */
      print_hex_ascii_line(ch, line_len, offset);
      /* compute total remaining */
      len_rem = len_rem - line_len;
      /* shift pointer to remaining bytes to print */
      ch = ch + line_len;
      /* add offset */
      offset = offset + line_width;
      /* check if we have line width chars or less */
      if (len_rem <= line_width) {
        /* print last line and get out */
        print_hex_ascii_line(ch, len_rem, offset);
        break;
      }
    }
  }
}

/*
 * print data in rows of 16 bytes: offset   hex   ascii
 * 00000   47 45 54 20 2f 20 48 54  54 50 2f 31 2e 31 0d 0a   GET / HTTP/1.1..
 */
void ACA_On_Demand_Engine::print_hex_ascii_line(const u_char *payload, int len, int offset)
{
  int i;
  int gap;
  const u_char *ch;

  /* offset */
  ACA_LOG_INFO("%05d   ", offset);

  /* hex */
  ch = payload;
  for (i = 0; i < len; i++) {
    ACA_LOG_INFO("%02x ", *ch);
    ch++;
    /* print extra space after 8th byte for visual aid */
    if (i == 7)
      ACA_LOG_INFO("%s", " ");
  }
  /* print space to handle line less than 8 bytes */
  if (len < 8)
    ACA_LOG_INFO("%s", " ");

  /* fill hex gap with spaces if not full line */
  if (len < 16) {
    gap = 16 - len;
    for (i = 0; i < gap; i++) {
      ACA_LOG_INFO("%s", "   ");
    }
  }
  ACA_LOG_INFO("%s", "   ");

  /* ascii (if printable) */
  ch = payload;
  for (i = 0; i < len; i++) {
    if (isprint(*ch))
      ACA_LOG_INFO("%c", *ch);
    else
      ACA_LOG_INFO("%s", ".");
    ch++;
  }
  ACA_LOG_INFO("%s", "\n");
  return;
}

} // namespace aca_on_demand_engine