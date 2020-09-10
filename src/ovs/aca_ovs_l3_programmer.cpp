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

#include "aca_log.h"
#include "aca_util.h"
#include "aca_net_config.h"
#include "aca_vlan_manager.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_ovs_l3_programmer.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <errno.h>
#include <arpa/inet.h>

#define HEX_IP_BUFFER_SIZE 12

// prefix to indicate it is an Alcor Distributed Router host mac
// (aka host DVR mac)
#define HOST_DVR_MAC_PREFIX "fe:16:11:"
#define HOST_DVR_MAC_MATCH HOST_DVR_MAC_PREFIX + "00:00:00/ff:ff:ff:00:00:00"

using namespace std;
using namespace aca_vlan_manager;
using namespace aca_ovs_l2_programmer;

namespace aca_ovs_l3_programmer
{
ACA_OVS_L3_Programmer &ACA_OVS_L3_Programmer::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static ACA_OVS_L3_Programmer instance;
  return instance;
}

int ACA_OVS_L3_Programmer::create_router(const string host_dvr_mac, const string router_id,
                                         unordered_map<std::string, subnet_table_entry> subnets_table,
                                         ulong &culminative_time)
{
  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_router ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;
  int source_vlan_id;
  string current_gateway_mac;
  char hex_ip_buffer[HEX_IP_BUFFER_SIZE];
  int addr;
  string cmd_string;

  if (host_dvr_mac.empty()) {
    throw std::invalid_argument("host_dvr_mac is empty");
  }

  // if _host_dvr_mac is not set yet, set it
  if (_host_dvr_mac.empty()) {
    _host_dvr_mac = host_dvr_mac;
  }
  // else if it is set, and same as the input
  else if (_host_dvr_mac != host_dvr_mac) {
    throw std::invalid_argument("Trying to set a different host dvr mac, old: " + _host_dvr_mac +
                                "new:" + host_dvr_mac);
  }
  // do nothing for (_host_dvr_mac == host_dvr_mac)

  if (router_id.empty()) {
    throw std::invalid_argument("router_id is empty");
  }

  // -----critical section starts-----
  _routers_table_mutex.lock();
  if (_routers_table.find(router_id) == _routers_table.end()) {
    _routers_table.emplace(router_id, subnets_table);
  }
  _routers_table_mutex.unlock();
  // -----critical section ends-----

  // for each subnet's gateway:
  for (auto subnet_it = subnets_table.begin(); subnet_it != subnets_table.end(); subnet_it++) {
    ACA_LOG_DEBUG("subnet_id:%s\n ", subnet_it->first.c_str());

    source_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(
            subnet_it->second.vpc_id);

    current_gateway_mac = subnet_it->second.gateway_mac;
    current_gateway_mac.erase(
            remove(current_gateway_mac.begin(), current_gateway_mac.end(), ':'),
            current_gateway_mac.end());

    addr = inet_network(subnet_it->second.gateway_ip.c_str());
    snprintf(hex_ip_buffer, HEX_IP_BUFFER_SIZE, "0x%08x", addr);

    // Program Arp responder:
    cmd_string = "add-flow br-tun \"table=51,priority=50,arp,dl_vlan=" +
                 to_string(source_vlan_id) + ",nw_dst=" + subnet_it->second.gateway_ip +
                 " actions=move:NXM_OF_ETH_SRC[]->NXM_OF_ETH_DST[],mod_dl_src:" +
                 subnet_it->second.gateway_mac +
                 ",load:0x2->NXM_OF_ARP_OP[],move:NXM_NX_ARP_SHA[]->NXM_NX_ARP_THA[],move:NXM_OF_ARP_SPA[]->NXM_OF_ARP_TPA[],load:0x" +
                 current_gateway_mac + "->NXM_NX_ARP_SHA[],load:" + string(hex_ip_buffer) +
                 "->NXM_OF_ARP_SPA[],in_port\"";

    ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd_string, culminative_time, overall_rc);

    // Program ICMP responder:
    cmd_string = "add-flow br-tun \"table=52,priority=50,icmp,dl_vlan=" +
                 to_string(source_vlan_id) + ",nw_dst=" + subnet_it->second.gateway_ip +
                 " actions=move:NXM_OF_ETH_SRC[]->NXM_OF_ETH_DST[],mod_dl_src:" +
                 subnet_it->second.gateway_mac +
                 ",move:NXM_OF_IP_SRC[]->NXM_OF_IP_DST[],mod_nw_src:" +
                 subnet_it->second.gateway_ip +
                 ",load:0xff->NXM_NX_IP_TTL[],load:0->NXM_OF_ICMP_TYPE[],in_port\"";

    ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd_string, culminative_time, overall_rc);

    // Should be able to ping the gateway now
  }

  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_router <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

int ACA_OVS_L3_Programmer::create_neighbor_l3(
        const string vpc_id, const string subnet_id,
        alcor::schema::NetworkType network_type, const string virtual_ip,
        const string virtual_mac, const string remote_host_ip, uint tunnel_id,
        const string neighbor_host_dvr_mac, ulong &culminative_time)
{
  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_neighbor_l3 ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;
  bool found_subnet_in_router = false;
  int source_vlan_id;
  int destination_vlan_id;
  string cmd_string;

  if (vpc_id.empty()) {
    throw std::invalid_argument("vpc_id is empty");
  }

  if (subnet_id.empty()) {
    throw std::invalid_argument("vpc_id is empty");
  }

  if (virtual_ip.empty()) {
    throw std::invalid_argument("virtual_ip is empty");
  }

  if (virtual_mac.empty()) {
    throw std::invalid_argument("virtual_mac is empty");
  }

  if (tunnel_id == 0) {
    throw std::invalid_argument("tunnel_id is 0");
  }

  // TODO: insert the port info into _routers_table, for use with on demand rule programming later

  bool is_port_on_same_host = aca_is_port_on_same_host(remote_host_ip);

  // going through our list of routers
  for (auto router_it = _routers_table.begin();
       router_it != _routers_table.end(); router_it++) {
    ACA_LOG_DEBUG("router ID:%s\n ", router_it->first.c_str());

    // try to see if the destination subnet GW is connected to the current router
    auto found_subnet = router_it->second.find(subnet_id);

    if (found_subnet == router_it->second.end()) {
      // subnet not found in this router, go look at the next router
      continue;
    } else {
      // destination subnet found!
      found_subnet_in_router = true;

      // for each other subnet connected to this router, create the routing rule
      for (auto subnet_it = router_it->second.begin();
           subnet_it != router_it->second.end(); subnet_it++) {
        // skip the destination neighbor subnet
        if (subnet_it->first == subnet_id) {
          continue;
        }
        ACA_LOG_DEBUG("subnet_id:%s\n ", subnet_it->first.c_str());

        source_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(
                subnet_it->second.vpc_id);

        // essential rule to restore from neighbor host DVR mac to destination GW mac:
        // not needed if the neighbor port is at the same compute host
        if (!is_port_on_same_host) {
          // Note: all port from the same subnet on current host will share this rule
          cmd_string = "add-flow br-int \"table=0,priority=25,dl_vlan=" +
                       to_string(source_vlan_id) + ",dl_src=" + HOST_DVR_MAC_MATCH +
                       " actions=mod_dl_src:" + subnet_it->second.gateway_mac +
                       " output:NORMAL\"";

          ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
                  cmd_string, culminative_time, overall_rc);
        }

        destination_vlan_id =
                ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(vpc_id);

        // for the first implementation, we will go ahead and program the on demand routing rule here
        // in the future, the programming of the on demand rule will be triggered by the first packet
        // sent to openflow controller, that's ACA

        // the openflow rule depends on whether the hosting ip is on this compute host or not
        if (is_port_on_same_host) {
          cmd_string = "add-flow br-tun \"table=0,priority=50,ip,dl_vlan=" +
                       to_string(source_vlan_id) + ",nw_dst=" + virtual_ip +
                       ",dl_dst=" + subnet_it->second.gateway_mac +
                       " actions=mod_vlan_vid:" + to_string(destination_vlan_id) +
                       ",mod_dl_dst:" + virtual_mac + ",output:IN_PORT\"";
        } else {
          cmd_string = "add-flow br-tun \"table=0,priority=50,ip,dl_vlan=" +
                       to_string(source_vlan_id) + ",nw_dst=" + virtual_ip +
                       ",dl_dst=" + subnet_it->second.gateway_mac +
                       " actions=mod_vlan_vid:" + to_string(destination_vlan_id) +
                       ",mod_dl_src:" + _host_dvr_mac +
                       ",mod_dl_dst:" + virtual_mac + ",resubmit(,2)\"";
        }

        ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
                cmd_string, culminative_time, overall_rc);
      }
      // we found our interested router from _routers_table which has the destination subnet GW connected to it.
      // Since each subnet GW can only be connected to one router, therefore, there is no point to look at other
      // routers on the higher level for (auto router_it = _routers_table.begin();...) loop
      break;
    }
  }

  if (!found_subnet_in_router) {
    ACA_LOG_ERROR("subnet_id %s not find in our local routers\n", subnet_id.c_str());
    overall_rc = ENOENT;
  }

  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_neighbor_l3 <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

} // namespace aca_ovs_l3_programmer
