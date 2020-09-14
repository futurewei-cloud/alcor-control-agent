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
#include "aca_config.h"
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

int ACA_OVS_L3_Programmer::create_or_update_router(RouterConfiguration &current_RouterConfiguration,
                                                   GoalState &parsed_struct,
                                                   ulong &dataplane_programming_time)
{
  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_or_update_router ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;
  bool is_router_exist;
  bool is_subnet_routing_table_exist = false;
  bool is_routing_rule_exist = false;
  string found_cidr;
  struct sockaddr_in sa;
  size_t slash_pos;
  string found_vpc_id;
  uint found_tunnel_id;
  string found_gateway_ip;
  string found_gateway_mac;
  bool subnet_info_found = false;
  int source_vlan_id;
  string current_gateway_mac;
  char hex_ip_buffer[HEX_IP_BUFFER_SIZE];
  int addr;
  string cmd_string;

  string router_id = current_RouterConfiguration.id();
  if (router_id.empty()) {
    ACA_LOG_ERROR("router_id is empty");
    return -EINVAL;
  }

  if (aca_validate_mac_address(
              current_RouterConfiguration.host_dvr_mac_address().c_str())) {
    ACA_LOG_ERROR("host_dvr_mac_address is invalid");
    return -EINVAL;
  }

  // if _host_dvr_mac is not set yet, set it
  if (_host_dvr_mac.empty()) {
    _host_dvr_mac = current_RouterConfiguration.host_dvr_mac_address();
  }
  // else if it is set, and same as the input
  else if (_host_dvr_mac != current_RouterConfiguration.host_dvr_mac_address()) {
    ACA_LOG_ERROR("Trying to set a different host dvr mac, old: %s, new: %s\n",
                  _host_dvr_mac.c_str(),
                  current_RouterConfiguration.host_dvr_mac_address().c_str());
    return -EINVAL;
  }
  // do nothing for (_host_dvr_mac == current_RouterConfiguration.host_dvr_mac_address())

  // -----critical section starts-----
  _routers_table_mutex.lock();
  if (_routers_table.find(router_id) == _routers_table.end()) {
    is_router_exist = false;
  } else {
    is_router_exist = true;
  }
  _routers_table_mutex.unlock();
  // -----critical section ends-----

  unordered_map<string, subnet_routing_table_entry> new_subnet_routing_tables;

  try {
    if (is_router_exist) {
      // depends on current_RouterConfiguration.message_type() == MessageType::DELTA or FULL
      if (current_RouterConfiguration.message_type() == MessageType::FULL) {
        // since message_type is full, we can simply remove the existing entry
        // and use the next (!is_router_exist) block to create a new entry
        overall_rc = delete_router(current_RouterConfiguration, dataplane_programming_time);
        if (overall_rc != EXIT_SUCCESS) {
          throw std::runtime_error("Failed to delete an existing router entry");
        }
      } else {
        // current_RouterConfiguration.message_type() == MessageType::DELTA
        // carefully update the existing entry with new information
        new_subnet_routing_tables = _routers_table[router_id];
      }
    }

    // it is okay for have subnet_routing_tables_size = 0
    for (int i = 0; i < current_RouterConfiguration.subnet_routing_tables_size(); i++) {
      auto current_subnet_routing_table =
              current_RouterConfiguration.subnet_routing_tables(i);

      string current_router_subnet_id = current_subnet_routing_table.subnet_id();

      ACA_LOG_DEBUG("Processing subnet ID: %s for router ID: %s.\n",
                    current_router_subnet_id.c_str(),
                    current_RouterConfiguration.id().c_str());

      // check if current_router_subnet_id already exist in new_subnet_routing_tables
      if (new_subnet_routing_tables.find(current_router_subnet_id) !=
          new_subnet_routing_tables.end()) {
        is_subnet_routing_table_exist = true;
      }

      // Look up the subnet configuration to query for additional info
      for (int j = 0; j < parsed_struct.subnet_states_size(); j++) {
        SubnetConfiguration current_SubnetConfiguration =
                parsed_struct.subnet_states(j).configuration();

        ACA_LOG_DEBUG("current_SubnetConfiguration subnet ID: %s.\n",
                      current_SubnetConfiguration.id().c_str());

        if ((parsed_struct.subnet_states(j).operation_type() == OperationType::INFO) &&
            (current_SubnetConfiguration.id() == current_router_subnet_id)) {
          found_vpc_id = current_SubnetConfiguration.vpc_id();

          found_cidr = current_SubnetConfiguration.cidr();

          slash_pos = found_cidr.find('/');
          if (slash_pos == string::npos) {
            throw std::invalid_argument("'/' not found in cidr");
          }

          found_tunnel_id = current_SubnetConfiguration.tunnel_id();
          if (!aca_validate_tunnel_id(found_tunnel_id)) {
            throw std::invalid_argument("found_tunnel_id is invalid");
          }

          // subnet info's gateway ip and mac needs to be there and valid
          found_gateway_ip = current_SubnetConfiguration.gateway().ip_address();

          // inet_pton returns 1 for success 0 for failure
          if (inet_pton(AF_INET, found_gateway_ip.c_str(), &(sa.sin_addr)) != 1) {
            throw std::invalid_argument("found gateway ip address is not in the expect format");
          }

          found_gateway_mac = current_SubnetConfiguration.gateway().mac_address();

          if (!aca_validate_mac_address(found_gateway_mac.c_str())) {
            throw std::invalid_argument("found_gateway_mac is invalid");
          }

          subnet_routing_table_entry new_subnet_routing_table_entry;

          if (is_subnet_routing_table_exist) {
            new_subnet_routing_table_entry =
                    new_subnet_routing_tables[current_router_subnet_id];
          }

          // update the subnet routing table entry
          new_subnet_routing_table_entry.vpc_id = found_vpc_id;
          new_subnet_routing_table_entry.network_type =
                  current_SubnetConfiguration.network_type();
          new_subnet_routing_table_entry.cidr = found_cidr;
          new_subnet_routing_table_entry.tunnel_id = found_tunnel_id;
          new_subnet_routing_table_entry.gateway_ip = found_gateway_ip;
          new_subnet_routing_table_entry.gateway_mac = found_gateway_mac;
          // TODO: handle the gateway_ip and gateway_mac changed, will that happen?
          // need to remove the previous openflow ARP and ICMP responder rules

          source_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(found_vpc_id);

          current_gateway_mac = found_gateway_mac;
          current_gateway_mac.erase(
                  remove(found_gateway_mac.begin(), found_gateway_mac.end(), ':'),
                  found_gateway_mac.end());

          addr = inet_network(found_gateway_ip.c_str());
          snprintf(hex_ip_buffer, HEX_IP_BUFFER_SIZE, "0x%08x", addr);

          // Program ARP responder:
          cmd_string = "add-flow br-tun \"table=51,priority=50,arp,dl_vlan=" +
                       to_string(source_vlan_id) + ",nw_dst=" + found_gateway_ip +
                       " actions=move:NXM_OF_ETH_SRC[]->NXM_OF_ETH_DST[],mod_dl_src:" + found_gateway_mac +
                       ",load:0x2->NXM_OF_ARP_OP[],move:NXM_NX_ARP_SHA[]->NXM_NX_ARP_THA[],move:NXM_OF_ARP_SPA[]->NXM_OF_ARP_TPA[],load:0x" +
                       current_gateway_mac +
                       "->NXM_NX_ARP_SHA[],load:" + string(hex_ip_buffer) +
                       "->NXM_OF_ARP_SPA[],in_port\"";

          ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
                  cmd_string, dataplane_programming_time, overall_rc);

          // Program ICMP responder:
          cmd_string =
                  "add-flow br-tun \"table=52,priority=50,icmp,dl_vlan=" +
                  to_string(source_vlan_id) + ",nw_dst=" + found_gateway_ip +
                  " actions=move:NXM_OF_ETH_SRC[]->NXM_OF_ETH_DST[],mod_dl_src:" + found_gateway_mac +
                  ",move:NXM_OF_IP_SRC[]->NXM_OF_IP_DST[],mod_nw_src:" + found_gateway_ip +
                  ",load:0xff->NXM_NX_IP_TTL[],load:0->NXM_OF_ICMP_TYPE[],in_port\"";

          ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
                  cmd_string, dataplane_programming_time, overall_rc);

          // Should be able to ping the gateway now

          for (int k = 0; k < current_subnet_routing_table.routing_rules_size(); k++) {
            auto current_routing_rule = current_subnet_routing_table.routing_rules(k);

            // check if current_routing_rule already exist in new_subnet_routing_tables
            if (new_subnet_routing_table_entry.routing_rules.find(
                        current_routing_rule.id()) !=
                new_subnet_routing_table_entry.routing_rules.end()) {
              is_routing_rule_exist = true;
            }

            // populate the routing_rule_table_entry and then add that to
            // new_subnet_routing_table_entry.routing_rules only if the
            // operation type for that routing_rule is CREATE/UPDATE/INFO
            if ((current_routing_rule.operation_type() == OperationType::CREATE) ||
                (current_routing_rule.operation_type() == OperationType::UPDATE) ||
                (current_routing_rule.operation_type() == OperationType::INFO)) {
              routing_rule_table_entry routing_rule_table_entry;

              if (is_routing_rule_exist) {
                routing_rule_table_entry =
                        new_subnet_routing_table_entry
                                .routing_rules[current_routing_rule.id()];
              }

              routing_rule_table_entry.next_hop_ip = current_routing_rule.next_hop_ip();
              routing_rule_table_entry.priority = current_routing_rule.priority();
              routing_rule_table_entry.destination_type =
                      current_routing_rule.routing_rule_extra_info().destination_type();
              routing_rule_table_entry.next_hop_mac =
                      current_routing_rule.routing_rule_extra_info().next_hop_mac();

              if (!is_routing_rule_exist) {
                new_subnet_routing_table_entry.routing_rules.emplace(
                        current_routing_rule.id(), routing_rule_table_entry);
              }

            } else if (current_routing_rule.operation_type() == OperationType::DELETE) {
              if (new_subnet_routing_table_entry.routing_rules.erase(
                          current_routing_rule.id())) {
                ACA_LOG_INFO("Successfuly cleaned up entry for router rule id %s\n",
                             current_routing_rule.id().c_str());
              } else {
                ACA_LOG_ERROR("Failed to clean up entry for router rule id %s\n",
                              current_routing_rule.id().c_str());
                overall_rc = EXIT_FAILURE;
              }
            } else {
              ACA_LOG_ERROR("Invalid operation_type: %d for router_rule with ID: %s.\n",
                            current_routing_rule.operation_type(),
                            current_routing_rule.id().c_str());
              overall_rc = EXIT_FAILURE;
            }
          }

          if (!is_subnet_routing_table_exist) {
            new_subnet_routing_tables.emplace(current_router_subnet_id,
                                              new_subnet_routing_table_entry);
          }

          subnet_info_found = true;
          break;
        }

        if (!subnet_info_found) {
          ACA_LOG_ERROR("Not able to find the info for router with subnet ID: %s.\n",
                        current_router_subnet_id.c_str());
          overall_rc = -EXIT_FAILURE;
        }
      } // for (int j = 0; j < parsed_struct.subnet_states_size(); j++)

      if (!is_router_exist ||
          (current_RouterConfiguration.message_type() == MessageType::FULL)) {
        // -----critical section starts-----
        _routers_table_mutex.lock();
        _routers_table.emplace(router_id, new_subnet_routing_tables);
        _routers_table_mutex.unlock();
        // -----critical section ends-----
      }

    } // for (int i = 0; i < current_RouterConfiguration.subnet_routing_tables_size(); i++)
  } catch (const std::invalid_argument &e) {
    ACA_LOG_ERROR("Invalid argument exception caught while parsing router configuration, message: %s.\n",
                  e.what());
    overall_rc = -EINVAL;
  } catch (const std::exception &e) {
    ACA_LOG_ERROR("Exception caught while parsing router configuration, message: %s.\n",
                  e.what());
    overall_rc = -EFAULT;
  } catch (...) {
    ACA_LOG_ERROR("Unknown exception caught while parsing router configuration, rethrowing.\n");
    overall_rc = -EFAULT;
  }

  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_or_update_router <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
} // namespace aca_ovs_l3_programmer

int ACA_OVS_L3_Programmer::delete_router(RouterConfiguration &current_RouterConfiguration,
                                         ulong &dataplane_programming_time)
{
  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::delete_router ---> Entering\n");

  int overall_rc;
  int source_vlan_id;
  string current_gateway_mac;
  string cmd_string;

  string router_id = current_RouterConfiguration.id();
  if (router_id.empty()) {
    ACA_LOG_ERROR("router_id is empty");
    return -EINVAL;
  }

  // -----critical section starts-----
  _routers_table_mutex.lock();
  if (_routers_table.find(router_id) == _routers_table.end()) {
    ACA_LOG_ERROR("Entry not found for router_id %s\n", router_id.c_str());
    overall_rc = ENOENT;
  } else {
    overall_rc = EXIT_SUCCESS;
  }
  _routers_table_mutex.unlock();
  // -----critical section ends-----

  if (overall_rc == ENOENT) {
    return overall_rc;
  }

  auto router_subnet_routing_tables = _routers_table[router_id];

  // for each subnet's gateway:
  for (auto subnet_it = router_subnet_routing_tables.begin();
       subnet_it != router_subnet_routing_tables.end(); subnet_it++) {
    ACA_LOG_DEBUG("Subnet_id:%s\n ", subnet_it->first.c_str());

    source_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(
            subnet_it->second.vpc_id);

    current_gateway_mac = subnet_it->second.gateway_mac;
    current_gateway_mac.erase(
            remove(current_gateway_mac.begin(), current_gateway_mac.end(), ':'),
            current_gateway_mac.end());

    // Delete Arp responder:
    cmd_string = "del-flows br-tun \"table=51,priority=50,arp,dl_vlan=" +
                 to_string(source_vlan_id) + ",nw_dst=" + subnet_it->second.gateway_ip;

    ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd_string, dataplane_programming_time, overall_rc);

    // Delete ICMP responder:
    cmd_string = "del-flows br-tun \"table=52,priority=50,icmp,dl_vlan=" +
                 to_string(source_vlan_id) + ",nw_dst=" + subnet_it->second.gateway_ip;

    ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd_string, dataplane_programming_time, overall_rc);
  }

  // -----critical section starts-----
  _routers_table_mutex.lock();
  if (_routers_table.erase(router_id)) {
    ACA_LOG_INFO("Successfuly cleaned up entry for router_id %s\n", router_id.c_str());
    overall_rc = EXIT_SUCCESS;
  } else {
    ACA_LOG_ERROR("Failed to clean up entry for router_id %s\n", router_id.c_str());
    overall_rc = EXIT_FAILURE;
  }
  _routers_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::delete_router <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

int ACA_OVS_L3_Programmer::create_neighbor_l3(const string vpc_id, const string subnet_id,
                                              alcor::schema::NetworkType network_type,
                                              const string virtual_ip, const string virtual_mac,
                                              const string remote_host_ip,
                                              uint tunnel_id, ulong &culminative_time)
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
    throw std::invalid_argument("subnet_id is empty");
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
