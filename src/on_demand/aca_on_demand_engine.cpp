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

#include "aca_config.h"
#include "aca_net_config.h"
#include "aca_vlan_manager.h"
#include "aca_grpc.h"
#include "aca_grpc_client.h"
#include "aca_log.h"
#include "aca_util.h"
#include "aca_config.h"
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
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include "goalstateprovisioner.pb.h"
#include "aca_dhcp_server.h"
#include "aca_arp_responder.h"
#include "aca_ovs_l2_programmer.h"
#undef OFP_ASSERT
#undef CONTAINER_OF
#undef ARRAY_SIZE
#undef ROUND_UP
#include "aca_on_demand_engine.h"
#include <openvswitch/ofp-errors.h>
//#include <openvswitch/ofp-packet.h>
#include <openvswitch/ofp-util.h>

using namespace std;
using namespace aca_vlan_manager;
using namespace aca_arp_responder;
using namespace alcor::schema;

extern std::atomic_ulong g_total_execute_system_time;
extern bool g_demo_mode;
extern string g_ncm_address, g_ncm_port;
extern GoalStateProvisionerClientImpl *g_grpc_client;
extern std::atomic<int> packet_in_counter;
extern std::atomic<int> packet_out_counter;

namespace aca_on_demand_engine
{
ACA_On_Demand_Engine &ACA_On_Demand_Engine::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static ACA_On_Demand_Engine instance;
  return instance;
}

/* 
  This function checks request_uuid_on_demand_payload_map periodically and removes any 
  entry that has been staying in the map for more than ON_DEMAND_ENTRY_EXPIRATION_IN_MICROSECONDS
*/
void ACA_On_Demand_Engine::clean_remaining_payload()
{
  ACA_LOG_DEBUG("\n", "Entering clean_remaining_payload");
  last_time_cleaned_remaining_payload = std::chrono::steady_clock::now();

  while (true) {
    usleep(ON_DEMAND_ENTRY_CLEANUP_FREQUENCY_IN_MICROSECONDS);

    ACA_LOG_DEBUG("\n", "Checking if there's any leftover inside request_uuid_on_demand_payload_map");
    std::chrono::_V2::steady_clock::time_point start = std::chrono::steady_clock::now();
    /* Critical section begins */
    _payload_map_mutex.lock();
    auto size_before_cleanup = request_uuid_on_demand_payload_map.size();
    for (auto it = request_uuid_on_demand_payload_map.cbegin();
         it != request_uuid_on_demand_payload_map.cend();) {
      auto request_id = it->first;
      auto payload = it->second;
      ACA_LOG_DEBUG("key = %s", request_id.c_str());
      if (cast_to_microseconds(last_time_cleaned_remaining_payload - payload->insert_time)
                  .count() >= ON_DEMAND_ENTRY_EXPIRATION_IN_MICROSECONDS) {
        ACA_LOG_DEBUG("Need to cleanup this key: %d\n", request_id.c_str());
        request_uuid_on_demand_payload_map.erase(it++);
      } else {
        ++it;
      }
    }
    auto size_after_cleanup = request_uuid_on_demand_payload_map.size();
    _payload_map_mutex.unlock();
    /* Critical section ends */
    std::chrono::_V2::steady_clock::time_point end = std::chrono::steady_clock::now();

    auto cleanup_time = cast_to_microseconds(end - start).count();

    ACA_LOG_DEBUG("Cleaned up [%ld] entries in the map, which took [%ld]us, which is [%ld]ms\n",
                  size_after_cleanup - size_before_cleanup, cleanup_time,
                  us_to_ms(cleanup_time));

    ACA_LOG_DEBUG("%s\n", "request_uuid_on_demand_payload_map check finished, sleeping");
    last_time_cleaned_remaining_payload = std::chrono::steady_clock::now();
  }
}

void ACA_On_Demand_Engine::process_async_replies_asyncly(
        string request_id, OperationStatus replyStatus,
        std::chrono::_V2::high_resolution_clock::time_point received_ncm_reply_time)
{
  ACA_LOG_DEBUG("Trying to process this hostOperationReply in another thread id: [%ld]",
                std::this_thread::get_id());
  std::unordered_map<std::__cxx11::string, on_demand_payload *, std::hash<std::__cxx11::string> >::iterator found_data;
  on_demand_payload *request_payload;
  ACA_LOG_DEBUG("%s\n", "Got an GRPC reply that is OK, need to process it.");
  ACA_LOG_DEBUG("Return from NCM - Reply Status: %s\n", to_string(replyStatus).c_str());
  found_data = request_uuid_on_demand_payload_map.find(request_id);
  if (found_data != request_uuid_on_demand_payload_map.end()) {
    request_payload = found_data->second;
    ACA_LOG_DEBUG("Found data into the map, UUID: [%s], in_port: [%d], protocol: [%d]\n",
                  request_id.c_str(), request_payload->in_port, request_payload->protocol);

    ACA_LOG_DEBUG("%s\n", "Printing out stuffs inside the unordered_map.");

    on_demand(request_id, replyStatus, request_payload->in_port,
              request_payload->packet, request_payload->packet_size,
              request_payload->protocol, request_payload->insert_time, request_payload->of_connection_id);
    std::chrono::_V2::steady_clock::time_point start = std::chrono::steady_clock::now();
    /* Critical section begins */
    _payload_map_mutex.lock();
    request_uuid_on_demand_payload_map.erase(request_id);
    _payload_map_mutex.unlock();
    /* Critical section ends */
    std::chrono::_V2::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto end_high_rest = std::chrono::high_resolution_clock::now();
    auto cleanup_time = cast_to_microseconds(end - start).count();
    auto process_successful_host_operation_reply_time =
            cast_to_microseconds(end_high_rest - received_ncm_reply_time).count();
    ACA_LOG_DEBUG("Erasing one entry into request_uuid_on_demand_payload_map took [%ld]us, which is [%ld]ms\n",
                  cleanup_time, us_to_ms(cleanup_time));
    ACA_LOG_DEBUG("For UUID: [%s], processing a successful host operation reply took %ld milliseconds\n",
                 request_id.c_str(),
                 us_to_ms(process_successful_host_operation_reply_time));
  }
}

void ACA_On_Demand_Engine::process_async_grpc_replies()
{
  void *got_tag;
  bool ok = false;
  HostRequestReply_HostRequestOperationStatus hostOperationStatus;
  OperationStatus replyStatus;
  string request_id;
  ACA_LOG_DEBUG("%s\n", "Beginning of process_async_grpc_replies");
  std::chrono::_V2::high_resolution_clock::time_point received_ncm_reply_time_prev =
          std::chrono::high_resolution_clock::now();
  while (_cq.Next(&got_tag, &ok)) {
    std::chrono::_V2::high_resolution_clock::time_point received_ncm_reply_time =
            std::chrono::high_resolution_clock::now();

    if (ok) {
      ACA_LOG_DEBUG("%s\n", "_cq->Next is good, ready to static cast the Async Client Call");
      auto received_ncm_reply_interval =
              cast_to_microseconds(received_ncm_reply_time - received_ncm_reply_time_prev)
                      .count();
      ACA_LOG_DEBUG("[METRICS] Elapsed time between receiving the last and current hostOperationReply took: %ld microseconds or %ld milliseconds\n",
                   received_ncm_reply_interval, (received_ncm_reply_interval / 1000));
      received_ncm_reply_time_prev = received_ncm_reply_time;

      AsyncClientCall *call = static_cast<AsyncClientCall *>(got_tag);
      auto call_status_copy = call->status;
      auto call_reply_copy = call->reply;
      ACA_LOG_DEBUG("%s\n", "Async Client Call casted successfully.");

      if (call->status.ok()) {
        ACA_LOG_DEBUG("%s\n", "Got an GRPC reply that is OK, need to process it.");
        for (int i = 0; i < call->reply.operation_statuses_size(); i++) {
          hostOperationStatus = call->reply.operation_statuses(i);
          replyStatus = hostOperationStatus.operation_status();
          request_id = hostOperationStatus.request_id();
        }
        ACA_LOG_DEBUG("For UUID: [%s], NCM called returned at: %ld milliseconds\n",
                      request_id.c_str(),
                      chrono::duration_cast<chrono::milliseconds>(
                              received_ncm_reply_time.time_since_epoch())
                              .count());
        ACA_LOG_DEBUG("Return from NCM - Reply Status: %s\n",
                      to_string(replyStatus).c_str());
        ACA_LOG_DEBUG("Received hostOperationReply in thread id: [%ld]\n",
                     std::this_thread::get_id());
        marl::schedule([=]{
          process_async_replies_asyncly(request_id, replyStatus, received_ncm_reply_time);
        });
        //thread_pool_.push(std::bind(&ACA_On_Demand_Engine::process_async_replies_asyncly, this,
        //                     request_id, replyStatus, received_ncm_reply_time));
        //ACA_LOG_DEBUG("After using the thread pool, we have %ld idle threads in the pool, thread pool size: %ld\n",
        //              thread_pool_.n_idle(), thread_pool_.size());
      }
    } else {
      ACA_LOG_INFO("%s\n", "Got an GRPC reply that is NOT OK, don't need to process the data");
    }
  }
}

void ACA_On_Demand_Engine::unknown_recv(uint16_t vlan_id, string ip_src,
                                        string ip_dest, int port_src, int port_dest,
                                        Protocol protocol, char *uuid_str)
{
  HostRequest HostRequest_builder;
  HostRequest_ResourceStateRequest *new_state_requests =
          HostRequest_builder.add_state_requests();
  HostRequestReply hostRequestReply;

  uint tunnel_id = ACA_Vlan_Manager::get_instance().get_tunnelId_by_vlanId(vlan_id);
  new_state_requests->set_request_id(uuid_str);
  new_state_requests->set_tunnel_id(tunnel_id);
  new_state_requests->set_source_ip(ip_src);
  new_state_requests->set_source_port(port_src);
  new_state_requests->set_destination_ip(ip_dest);
  new_state_requests->set_destination_port(port_dest);
  new_state_requests->set_protocol(protocol);
  new_state_requests->set_ethertype(EtherType::IPV4);
  std::chrono::_V2::steady_clock::time_point call_ncm_time =
          std::chrono::steady_clock::now();
  ACA_LOG_DEBUG("For UUID: [%s], calling NCM for info of IP [%s] at: [%ld], tunnel_id: [%ld]\n",
                uuid_str, ip_dest.c_str(), call_ncm_time, tunnel_id);
  std::chrono::_V2::high_resolution_clock::time_point start =
          std::chrono::high_resolution_clock::now();
  // this is a timestamp in milliseconds
  ACA_LOG_DEBUG(
          "For UUID: [%s], on-demand sent on %ld milliseconds\n", uuid_str,
          chrono::duration_cast<chrono::milliseconds>(start.time_since_epoch()).count());
  g_grpc_client->RequestGoalStates(&HostRequest_builder, &_cq);
}

void ACA_On_Demand_Engine::on_demand(string uuid_for_call, OperationStatus status,
                                     uint32_t in_port, void *packet,
                                     int packet_size, Protocol protocol,
                                     std::chrono::_V2::steady_clock::time_point insert_time, int of_connection_id)
{
  ACA_LOG_INFO("%s\n", "Inside of on_demand function");
  string bridge = "br-tun";
  string inport = "in_port=controller";
  string whitespace = " ";
  string action = "actions=output:" + to_string(in_port);
  string rs_action = "actions=resubmit(,20)";
  string packetpre = "packet=";
  string options = "";
  string serialized_packet = "";
  const struct ether_header *eth_header = (struct ether_header *)packet;
  const u_char *ch = (const u_char *)packet;
  char str[10];

  if (status == OperationStatus::SUCCESS) {
    ACA_LOG_DEBUG("%s\n", "It was an succesful operation, let's wait a little bit, so that the goalstate is created/updated");

    if (protocol == Protocol::ARP) {
      char *base = (char *)packet;
      unsigned char *vlan_hdr = (unsigned char *)(base + 12);
      vlan_message *vlanmsg = (vlan_message *)vlan_hdr;
      unsigned char *arp_hdr = (unsigned char *)(base + SIZE_ETHERNET + 4);
      arp_message *arpmsg = (arp_message *)arp_hdr;
      arp_entry_data stData;
      // get the ip address from arp message
      stData.ipv4_address =
              aca_arp_responder::ACA_ARP_Responder::get_instance()._get_requested_ip(arpmsg);
      // get the vlan id from vlan header
      if (vlanmsg) {
        stData.vlan_id = ntohs(vlanmsg->vlan_tci) & 0x0fff;
      } else {
        stData.vlan_id = 0;
      }
      /*
        Implementing a "Smart" sleep here, which checks if the target arp entry exists,
        if exists, stop sleeping and go forward; 
        otherwise, sleep for another 1000 us (0.001) seconds and then check again.
        If arp still not found after one second, it lets the packet drop.
      */
      int wait_time = 1000000; // one second
      int check_frequency_us = 1000; // 0.001 seconds, or 10000 microseconds.
      bool found_arp_entry = false;
      int times_to_check = wait_time / check_frequency_us;
      int i = 0;
      std::chrono::_V2::steady_clock::time_point start =
              std::chrono::steady_clock::now();

      do {
        found_arp_entry =
                aca_arp_responder::ACA_ARP_Responder::get_instance().does_arp_entry_exist(stData);
        if (!found_arp_entry) {
          i++;
          usleep(check_frequency_us);
        }
      } while (!found_arp_entry && i < times_to_check);
      std::chrono::_V2::steady_clock::time_point end = std::chrono::steady_clock::now();
      auto total_time_slept = cast_to_microseconds(end - start).count();
      auto total_time_for_goalstate_from_send_gs_to_gs_received_and_programmed =
              cast_to_microseconds(end - insert_time).count();
      auto total_time_before_sending_grpc_request_to_before_wait_starts =
              cast_to_microseconds(start - insert_time).count();

      ACA_LOG_DEBUG(
              "For UUID: [%s], wait started at: [%ld] finished at: [%ld], took: %ld microseconds or %ld milliseconds\nThe whole operation took %ld microseconds or %ld milliseconds\nFrom before sending GRPC request to before waiting for GS ready (T3 - T1) took %ld microseconds or %ld milliseconds",
              uuid_for_call.c_str(), start, end, total_time_slept, us_to_ms(total_time_slept),
              total_time_for_goalstate_from_send_gs_to_gs_received_and_programmed,
              us_to_ms(total_time_for_goalstate_from_send_gs_to_gs_received_and_programmed),
              total_time_before_sending_grpc_request_to_before_wait_starts,
              us_to_ms(total_time_before_sending_grpc_request_to_before_wait_starts));

      int parse_arp_request_rc =
              aca_arp_responder::ACA_ARP_Responder::get_instance()._parse_arp_request(
                      in_port, vlanmsg, arpmsg, of_connection_id);
      if (parse_arp_request_rc == EXIT_SUCCESS) {
        ACA_LOG_DEBUG("%s", "On-demand arp request packet sent to arp_responder.\n");

      } else {
        ACA_LOG_DEBUG("%s", "On-demand arp request packet FAILED to send to arp_responder.\n");
      }
    } else {
      for (int i = 0; i < packet_size; i++) {
        sprintf(str, "%02x", *ch);
        serialized_packet.append(str);
        ch++;
      }
      options = inport + whitespace + packetpre + serialized_packet + whitespace + action;
      //aca_ovs_control::ACA_OVS_Control::get_instance().packet_out(
      //        bridge.c_str(), options.c_str());
      aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().packet_out(
              of_connection_id, options.c_str());
      ACA_LOG_DEBUG("On-demand packet with protocol %d sent to ovs: %s\n",
                    protocol, options.c_str());
    }
  } else {
    ACA_LOG_ERROR("Packet dropped from %s to %s\n",
                  ether_ntoa((ether_addr *)&eth_header->ether_shost),
                  ether_ntoa((ether_addr *)&eth_header->ether_dhost));
  }
}

void ACA_On_Demand_Engine::parse_packet(uint32_t in_port, void *packet, int of_connection_id)
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

  ACA_LOG_DEBUG("Source Mac: %s\n", ether_ntoa((ether_addr *)&eth_header->ether_shost));

  int vlan_len = 0;
  unsigned char *vlan_hdr = nullptr;
  char *base = (char *)packet;
  uint16_t vlan_id = 0;
  vlan_message *vlanmsg = nullptr;
  string ip_src, ip_dest;
  int port_src, port_dest, packet_size;
  Protocol _protocol = Protocol::Protocol_INT_MAX_SENTINEL_DO_NOT_USE_;

  uint16_t ether_type = ntohs(*(uint16_t *)(base + 12));

  if (ether_type == ETHERTYPE_VLAN) {
    // ACA_LOG_INFO("%s", "Ethernet Type: 802.1Q VLAN tagging (0x8100) \n");

    ether_type = ntohs(*(uint16_t *)(base + 16));
    vlan_len = 4;
    vlan_hdr = (unsigned char *)(base + 12);
    vlanmsg = (vlan_message *)vlan_hdr;
    // get the vlan id from vlan header

    if (vlanmsg) {
      vlan_id = ntohs(vlanmsg->vlan_tci) & 0x0fff;
      // ACA_LOG_INFO("vlan_id: %ld\n", vlan_id);
    }
  }

  if (ether_type == ETHERTYPE_ARP) {

    ACA_LOG_DEBUG("%s", "Ethernet Type: ARP (0x0806) \n");
    ACA_LOG_DEBUG("   From: %s\n", inet_ntoa(*(in_addr *)(base + 14 + vlan_len + 14)));
    ACA_LOG_DEBUG("     to: %s\n",
                  inet_ntoa(*(in_addr *)(base + 14 + vlan_len + 14 + 10)));
    /* compute arp message offset */
    unsigned char *arp_hdr = (unsigned char *)(base + SIZE_ETHERNET + vlan_len);
    /* arp request procedure,type = 1 */
    // ACA_LOG_INFO("ntohs(*(uint16_t *)(arp_hdr + 6)) == %ld\n", ntohs(*(uint16_t *)(arp_hdr + 6)));
    // ACA_LOG_INFO("arp_hdr + 6: %s", (arp_hdr + 6));
    // for (int i = 0; i <= 6; i ++ ) {
    //   ACA_LOG_INFO("arp_hdr + %ld = %ld\n", i , ntohs(*(uint16_t *)(arp_hdr + 6)));
    // }
    if (ntohs(*(uint16_t *)(arp_hdr + 6)) == 0x0001) {
      if (aca_arp_responder::ACA_ARP_Responder::get_instance().arp_recv(
                  in_port, vlan_hdr, arp_hdr, of_connection_id) == ENOTSUP) {
        _protocol = Protocol::ARP;
        arp_message *arpmsg = (arp_message *)arp_hdr;
        ip_src = aca_arp_responder::ACA_ARP_Responder::get_instance()._get_source_ip(arpmsg);
        ip_dest = aca_arp_responder::ACA_ARP_Responder::get_instance()._get_requested_ip(arpmsg);
        packet_size = SIZE_ETHERNET + vlan_len + 28;
        port_src = 0;
        port_dest = 0;
      }
    }
  } else if (ether_type == ETHERTYPE_IP) {
    ACA_LOG_DEBUG("%s", "Ethernet Type: IP (0x0800) \n");

    /* define/compute ip header offset */
    const struct sniff_ip *ip = (struct sniff_ip *)(base + SIZE_ETHERNET + vlan_len);
    int size_ip = IP_HL(ip) * 4;

    if (size_ip < 20) {
      ACA_LOG_ERROR("size_udp < 20: %d bytes\n", size_ip);
      return;
    } else {
      ip_src = string(inet_ntoa(ip->ip_src));
      ip_dest = string(inet_ntoa(ip->ip_dst));
      packet_size = SIZE_ETHERNET + vlan_len + size_ip;

      /* print source and destination IP addresses */
      ACA_LOG_DEBUG("       From: %s\n", inet_ntoa(ip->ip_src));
      ACA_LOG_DEBUG("         To: %s\n", inet_ntoa(ip->ip_dst));

      /* determine protocol */
      switch (ip->ip_p) {
      case IPPROTO_TCP:
        ACA_LOG_DEBUG("%s", "   Protocol: TCP\n");
        break;
      case IPPROTO_UDP:
        ACA_LOG_DEBUG("%s", "   Protocol: UDP\n");
        break;
      case IPPROTO_ICMP:
        ACA_LOG_DEBUG("%s", "   Protocol: ICMP\n");
        break;
      case IPPROTO_IP:
        ACA_LOG_DEBUG("%s", "   Protocol: IP\n");
        break;
      default:
        ACA_LOG_DEBUG("%s", "   Protocol: unknown\n");
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

        ACA_LOG_DEBUG("   Src port: %d\n", ntohs(tcp->th_sport));
        ACA_LOG_DEBUG("   Dst port: %d\n", ntohs(tcp->th_dport));

        /* define/compute tcp payload (segment) offset */
        //payload = (u_char *)(base + SIZE_ETHERNET + vlan_len + size_ip + size_tcp);

        /* compute tcp payload (segment) size */
        size_payload = ntohs(ip->ip_len) - (size_ip + size_tcp);

        /* Print payload data; */
        if (size_payload > 0) {
          ACA_LOG_DEBUG("   Payload (%d bytes):\n", size_payload);
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
        ACA_LOG_DEBUG("   Src port: %d\n", port_src);
        ACA_LOG_DEBUG("   Dst port: %d\n", port_dest);

        /* define/compute udp payload (daragram) offset */
        payload = (u_char *)(base + SIZE_ETHERNET + vlan_len + size_ip + 8);

        /* compute udp payload (datagram) size */
        size_payload = ntohs(ip->ip_len) - (size_ip + 8);

        /* Print payload data. */
        if (size_payload > 0) {
          ACA_LOG_DEBUG("   Payload (%d bytes):\n", size_payload);
          //print_payload(payload, size_payload);
        }

        /* dhcp message procedure */
        if (port_src == 68 && port_dest == 67) {
          ACA_LOG_DEBUG("%s", "   Message Type: DHCP\n");
          aca_dhcp_server::ACA_Dhcp_Server::get_instance().dhcps_recv(
                  in_port, const_cast<unsigned char *>(payload));
          _protocol = Protocol::Protocol_INT_MAX_SENTINEL_DO_NOT_USE_;
        }
      }
    } else if (ip->ip_p == IPPROTO_ICMP) {
      _protocol = Protocol::ICMP;
    }
  } else if (ether_type == ETHERTYPE_REVARP) {
    ACA_LOG_DEBUG("%s", "Ethernet Type: REVARP (0x8035) \n");
    _protocol = Protocol::Protocol_INT_MAX_SENTINEL_DO_NOT_USE_;
  } else {
    ACA_LOG_DEBUG("%s", "Ethernet Type: Cannot Tell!\n");
    return;
  }

  if (_protocol != Protocol::Protocol_INT_MAX_SENTINEL_DO_NOT_USE_) {
    packet_size = SIZE_ETHERNET + vlan_len + packet_size;
    uuid_t uuid;
    uuid_generate_time(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    on_demand_payload *data = new on_demand_payload;
    void *packet_copy = malloc(packet_size);
    memcpy(packet_copy, packet, packet_size);
    data->in_port = in_port;
    data->packet = packet_copy;
    data->packet_size = packet_size;
    data->protocol = _protocol;
    data->insert_time = std::chrono::steady_clock::now();
    data->of_connection_id = of_connection_id;
    std::chrono::_V2::steady_clock::time_point start = std::chrono::steady_clock::now();

    /* Sleep until the size is less than the max limit. */
    while (request_uuid_on_demand_payload_map.size() >= REQUEST_UUID_ON_DEMAND_PAYLOAD_MAP_MAX_SIZE) {
      usleep(REQUEST_UUID_ON_DEMAND_PAYLOAD_MAP_SIZE_CHECK_FREQUENCY_IN_MICROSECONDS);
    }

    /* Critical section begins */
    _payload_map_mutex.lock();
    request_uuid_on_demand_payload_map[uuid_str] = data;
    _payload_map_mutex.unlock();
    /* Critical section ends */
    std::chrono::_V2::steady_clock::time_point end = std::chrono::steady_clock::now();

    auto cleanup_time = cast_to_microseconds(end - start).count();

    ACA_LOG_DEBUG("Inserting one entry into request_uuid_on_demand_payload_map took [%ld]us, which is [%ld]ms\n",
                  cleanup_time, us_to_ms(cleanup_time));
    ACA_LOG_DEBUG("Inserted data into the map, UUID: [%s], in_port: [%d], protocol: [%d]\n",
                  uuid_str, in_port, _protocol);

    unknown_recv(vlan_id, ip_src, ip_dest, port_src, port_dest, _protocol, uuid_str);
  }
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