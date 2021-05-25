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

#include "aca_log.h"
#include "aca_util.h"
#include "aca_config.h"
#include "aca_net_config.h"
#include "aca_vlan_manager.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_ovs_l3_programmer.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_arp_responder.h"
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <errno.h>
#include <arpa/inet.h>

using namespace std;
using namespace aca_vlan_manager;
using namespace aca_ovs_l2_programmer;
using namespace aca_arp_responder;

namespace aca_ovs_l3_programmer
{
ACA_OVS_L3_Programmer &ACA_OVS_L3_Programmer::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static ACA_OVS_L3_Programmer instance;
  return instance;
}

void ACA_OVS_L3_Programmer::clear_all_data()
{
  ACA_LOG_DEBUG("%s", "ACA_OVS_L3_Programmer::clear_all_data ---> Entering\n");

  // -----critical section starts-----
  _routers_table_mutex.lock();
  // All the elements in the unordered_map container are dropped:
  // their destructors are called, and they are removed from the container,
  // leaving _routers_table with a size of 0.
  _routers_table.clear();
  _routers_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("%s", "ACA_OVS_L3_Programmer::clear_all_data <--- Exiting\n");
}

int ACA_OVS_L3_Programmer::create_or_update_router(RouterConfiguration &current_RouterConfiguration,
                                                   GoalState &parsed_struct,
                                                   ulong &dataplane_programming_time)
{
  ACA_LOG_DEBUG("%s", "ACA_OVS_L3_Programmer::create_or_update_router ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;
  bool is_router_exist;
  bool is_subnet_routing_table_exist = false;
  bool is_routing_rule_exist = false;
  string found_cidr;
  struct sockaddr_in sa;
  size_t slash_pos;
  string found_vpc_id;
  NetworkType found_network_type;
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
    ACA_LOG_ERROR("%s", "router_id is empty");
    return -EINVAL;
  }

  if (!aca_validate_mac_address(
              current_RouterConfiguration.host_dvr_mac_address().c_str())) {
    ACA_LOG_ERROR("%s", "host_dvr_mac_address is invalid\n");
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
      if (current_RouterConfiguration.update_type() == UpdateType::FULL) {
        // since update_type is full, we can simply remove the existing entry and use
        // the next router entry population block
        overall_rc = delete_router(current_RouterConfiguration, dataplane_programming_time);
        if (overall_rc != EXIT_SUCCESS) {
          throw std::runtime_error("Failed to delete an existing router entry");
        }
      } else {
        // current_RouterConfiguration.update_type() == UpdateType::DELTA
        // carefully update the existing entry with new information
        new_subnet_routing_tables = _routers_table[router_id];
      }
    }

    // ==============================
    // router entry population block
    // ==============================

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
          found_network_type = current_SubnetConfiguration.network_type();
          found_tunnel_id = current_SubnetConfiguration.tunnel_id();
          if (!aca_validate_tunnel_id(found_tunnel_id, found_network_type)) {
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
          new_subnet_routing_table_entry.network_type = found_network_type;
          new_subnet_routing_table_entry.cidr = found_cidr;
          new_subnet_routing_table_entry.tunnel_id = found_tunnel_id;
          new_subnet_routing_table_entry.gateway_ip = found_gateway_ip;
          new_subnet_routing_table_entry.gateway_mac = found_gateway_mac;
          // don't need to handle the gateway_ip and gateway_mac change, because that will
          // require the subnet to remove the gateway port and add in a new one

          source_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(found_tunnel_id);

          current_gateway_mac = found_gateway_mac;
          current_gateway_mac.erase(
                  remove(current_gateway_mac.begin(), current_gateway_mac.end(), ':'),
                  current_gateway_mac.end());

          addr = inet_network(found_gateway_ip.c_str());
          snprintf(hex_ip_buffer, HEX_IP_BUFFER_SIZE, "0x%08x", addr);

          // Program ARP responder:
          arp_config stArpCfg;

          stArpCfg.mac_address = found_gateway_mac;
          stArpCfg.ipv4_address = found_gateway_ip;
          stArpCfg.vlan_id = source_vlan_id;

          ACA_ARP_Responder::get_instance().create_or_update_arp_entry(&stArpCfg);

          ACA_LOG_DEBUG("Add arp entry for gateway: ip = %s,vlan id = %u and mac = %s",
                        found_gateway_ip.c_str(), source_vlan_id,
                        found_gateway_mac.c_str());

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

          // add essential rule to restore from neighbor host DVR mac to destination GW mac:
          // Note: all port from the same subnet on current host will share this rule
          cmd_string = "add-flow br-int \"table=0,priority=25,dl_vlan=" +
                       to_string(source_vlan_id) + ",dl_src=" + HOST_DVR_MAC_MATCH +
                       " actions=mod_dl_src:" + found_gateway_mac + " output:NORMAL\"";

          ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
                  cmd_string, dataplane_programming_time, overall_rc);

          for (int k = 0; k < current_subnet_routing_table.routing_rules_size(); k++) {
            auto current_routing_rule = current_subnet_routing_table.routing_rules(k);

            // check if current_routing_rule already exist in new_subnet_routing_tables
            if (new_subnet_routing_table_entry.routing_rules.find(
                        current_routing_rule.id()) !=
                new_subnet_routing_table_entry.routing_rules.end()) {
              is_routing_rule_exist = true;
            }

            // populate the routing_rule_entry and add that to
            // new_subnet_routing_table_entry.routing_rules only if the
            // operation type for that routing_rule is CREATE/UPDATE/INFO
            if ((current_routing_rule.operation_type() == OperationType::CREATE) ||
                (current_routing_rule.operation_type() == OperationType::UPDATE) ||
                (current_routing_rule.operation_type() == OperationType::INFO)) {
              routing_rule_entry new_routing_rule_entry;

              if (is_routing_rule_exist) {
                new_routing_rule_entry =
                        new_subnet_routing_table_entry
                                .routing_rules[current_routing_rule.id()];
              }

              new_routing_rule_entry.next_hop_ip = current_routing_rule.next_hop_ip();
              new_routing_rule_entry.priority = current_routing_rule.priority();
              new_routing_rule_entry.destination_type =
                      current_routing_rule.routing_rule_extra_info().destination_type();
              new_routing_rule_entry.next_hop_mac =
                      current_routing_rule.routing_rule_extra_info().next_hop_mac();

              if (!is_routing_rule_exist) {
                new_subnet_routing_table_entry.routing_rules.emplace(
                        current_routing_rule.id(), new_routing_rule_entry);

                ACA_LOG_INFO("Added routing table entry for routering rule id %s\n",
                             current_routing_rule.id().c_str());
              } else {
                ACA_LOG_INFO("Using existing routing table entry for routering rule id %s\n",
                             current_routing_rule.id().c_str());
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

            ACA_LOG_INFO("Added router subnet table entry for subnet id %s\n",
                         current_router_subnet_id.c_str());
          } else {
            ACA_LOG_INFO("Using existing router subnet table entry for subnet id %s\n",
                         current_router_subnet_id.c_str());
            new_subnet_routing_tables[current_router_subnet_id] = new_subnet_routing_table_entry;
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
    } // for (int i = 0; i < current_RouterConfiguration.subnet_routing_tables_size(); i++)

    if (!is_router_exist || (current_RouterConfiguration.update_type() == UpdateType::FULL)) {
      // -----critical section starts-----
      _routers_table_mutex.lock();
      _routers_table.emplace(router_id, new_subnet_routing_tables);
      _routers_table_mutex.unlock();
      // -----critical section ends-----
      ACA_LOG_INFO("Added router entry for router id %s\n", router_id.c_str());
    } else {
      // -----critical section starts-----
      _routers_table_mutex.lock();
      _routers_table[router_id] = new_subnet_routing_tables;
      _routers_table_mutex.unlock();
      // -----critical section ends-----
    }

  } catch (const std::invalid_argument &e) {
    ACA_LOG_ERROR("Invalid argument exception caught while parsing router configuration, message: %s.\n",
                  e.what());
    overall_rc = -EINVAL;
  } catch (const std::exception &e) {
    ACA_LOG_ERROR("Exception caught while parsing router configuration, message: %s.\n",
                  e.what());
    overall_rc = -EFAULT;
  } catch (...) {
    ACA_LOG_CRIT("%s", "Unknown exception caught while parsing router configuration.\n");
    overall_rc = -EFAULT;
  }

  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_or_update_router <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
} // namespace aca_ovs_l3_programmer

int ACA_OVS_L3_Programmer::delete_router(RouterConfiguration &current_RouterConfiguration,
                                         ulong &dataplane_programming_time)
{
  ACA_LOG_DEBUG("%s", "ACA_OVS_L3_Programmer::delete_router ---> Entering\n");

  int overall_rc;
  int source_vlan_id;
  string current_gateway_mac;
  string cmd_string;

  string router_id = current_RouterConfiguration.id();
  if (router_id.empty()) {
    ACA_LOG_ERROR("%s", "router_id is empty");
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

  // for each connected subnet's gateway:
  for (auto subnet_it = router_subnet_routing_tables.begin();
       subnet_it != router_subnet_routing_tables.end(); subnet_it++) {
    string subnet_entry_to_delete = subnet_it->first.c_str();
    ACA_LOG_DEBUG("Subnet_id entry to delete:%s\n", subnet_entry_to_delete.c_str());

    source_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(
            subnet_it->second.tunnel_id);

    current_gateway_mac = subnet_it->second.gateway_mac;
    current_gateway_mac.erase(
            remove(current_gateway_mac.begin(), current_gateway_mac.end(), ':'),
            current_gateway_mac.end());

    // Program ARP responder:
    arp_config stArpCfg;

    stArpCfg.mac_address = current_gateway_mac;
    stArpCfg.ipv4_address = subnet_it->second.gateway_ip;
    stArpCfg.vlan_id = source_vlan_id;

    ACA_ARP_Responder::get_instance().delete_arp_entry(&stArpCfg);

    ACA_LOG_DEBUG("Delete arp entry for gateway: ip = %s,vlan id = %u",
                  stArpCfg.ipv4_address.c_str(), source_vlan_id);

    // Delete ICMP responder:
    cmd_string = "del-flows br-tun \"table=52,icmp,dl_vlan=" + to_string(source_vlan_id) +
                 ",nw_dst=" + subnet_it->second.gateway_ip + "\"";

    ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd_string, dataplane_programming_time, overall_rc);

    // remove essential rule which restore from neighbor host DVR mac to destination GW mac

    // Note: all port from the same subnet on current host will share this rule
    cmd_string = "del-flows br-int \"table=0,priority=25,dl_vlan=" + to_string(source_vlan_id) +
                 ",dl_src=" + HOST_DVR_MAC_MATCH + "\" --strict";

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

int ACA_OVS_L3_Programmer::create_or_update_router(RouterConfiguration &current_RouterConfiguration,
                                                   GoalStateV2 &parsed_struct,
                                                   ulong &dataplane_programming_time)
{
  ACA_LOG_DEBUG("%s", "ACA_OVS_L3_Programmer::create_or_update_router ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;
  bool is_router_exist;
  bool is_subnet_routing_table_exist = false;
  bool is_routing_rule_exist = false;
  string found_cidr;
  struct sockaddr_in sa;
  size_t slash_pos;
  string found_vpc_id;
  NetworkType found_network_type;
  uint found_tunnel_id;
  string found_gateway_ip;
  string found_gateway_mac;
  int source_vlan_id;
  string current_gateway_mac;
  char hex_ip_buffer[HEX_IP_BUFFER_SIZE];
  int addr;
  string cmd_string;

  string router_id = current_RouterConfiguration.id();
  if (router_id.empty()) {
    ACA_LOG_ERROR("%s", "router_id is empty");
    return -EINVAL;
  }

  if (!aca_validate_mac_address(
              current_RouterConfiguration.host_dvr_mac_address().c_str())) {
    ACA_LOG_ERROR("%s", "host_dvr_mac_address is invalid\n");
    return -EINVAL;
  }

  // if _host_dvr_mac is not set yet, set it
  if (_host_dvr_mac.empty()) {
    _host_dvr_mac = current_RouterConfiguration.host_dvr_mac_address();
  }
  // else if it is set, return error if different from the input
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
      if (current_RouterConfiguration.update_type() == UpdateType::FULL) {
        // since update_type is full, we can simply remove the existing entry and use
        // the next router entry population block
        overall_rc = delete_router(current_RouterConfiguration, dataplane_programming_time);
        if (overall_rc != EXIT_SUCCESS) {
          throw std::runtime_error("Failed to delete an existing router entry");
        }
      } else {
        // current_RouterConfiguration.update_type() == UpdateType::DELTA
        // carefully update the existing entry with new information
        new_subnet_routing_tables = _routers_table[router_id];
      }
    }

    // ==============================
    // router entry population block
    // ==============================

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
      auto subnetStateFound = parsed_struct.subnet_states().find(current_router_subnet_id);

      if (subnetStateFound != parsed_struct.subnet_states().end()) {
        SubnetState current_SubnetState = subnetStateFound->second;
        SubnetConfiguration current_SubnetConfiguration =
                current_SubnetState.configuration();

        ACA_LOG_DEBUG("current_SubnetConfiguration subnet ID: %s.\n",
                      current_SubnetConfiguration.id().c_str());

        found_vpc_id = current_SubnetConfiguration.vpc_id();

        found_cidr = current_SubnetConfiguration.cidr();

        slash_pos = found_cidr.find('/');
        if (slash_pos == string::npos) {
          throw std::invalid_argument("'/' not found in cidr");
        }
        found_network_type = current_SubnetConfiguration.network_type();
        found_tunnel_id = current_SubnetConfiguration.tunnel_id();
        if (!aca_validate_tunnel_id(found_tunnel_id, found_network_type)) {
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
        new_subnet_routing_table_entry.network_type = found_network_type;
        new_subnet_routing_table_entry.cidr = found_cidr;
        new_subnet_routing_table_entry.tunnel_id = found_tunnel_id;
        new_subnet_routing_table_entry.gateway_ip = found_gateway_ip;
        new_subnet_routing_table_entry.gateway_mac = found_gateway_mac;
        // don't need to handle the gateway_ip and gateway_mac change, because that will
        // require the subnet to remove the gateway port and add in a new one

        source_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(found_tunnel_id);

        current_gateway_mac = found_gateway_mac;
        current_gateway_mac.erase(
                remove(current_gateway_mac.begin(), current_gateway_mac.end(), ':'),
                current_gateway_mac.end());

        addr = inet_network(found_gateway_ip.c_str());
        snprintf(hex_ip_buffer, HEX_IP_BUFFER_SIZE, "0x%08x", addr);

        // Program ARP responder:
        arp_config stArpCfg;

        stArpCfg.mac_address = found_gateway_mac;
        stArpCfg.ipv4_address = found_gateway_ip;
        stArpCfg.vlan_id = source_vlan_id;

        ACA_ARP_Responder::get_instance().create_or_update_arp_entry(&stArpCfg);

        ACA_LOG_DEBUG("Add arp entry for gateway: ip = %s,vlan id = %u and mac = %s",
                      found_gateway_ip.c_str(), source_vlan_id,
                      found_gateway_mac.c_str());

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

        // add essential rule to restore from neighbor host DVR mac to destination GW mac:
        // Note: all port from the same subnet on current host will share this rule
        cmd_string = "add-flow br-int \"table=0,priority=25,dl_vlan=" +
                     to_string(source_vlan_id) + ",dl_src=" + HOST_DVR_MAC_MATCH +
                     " actions=mod_dl_src:" + found_gateway_mac + " output:NORMAL\"";

        ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
                cmd_string, dataplane_programming_time, overall_rc);

        for (int k = 0; k < current_subnet_routing_table.routing_rules_size(); k++) {
          auto current_routing_rule = current_subnet_routing_table.routing_rules(k);

          // check if current_routing_rule already exist in new_subnet_routing_tables
          if (new_subnet_routing_table_entry.routing_rules.find(
                      current_routing_rule.id()) !=
              new_subnet_routing_table_entry.routing_rules.end()) {
            is_routing_rule_exist = true;
          }

          // populate the routing_rule_entry and add that to
          // new_subnet_routing_table_entry.routing_rules only if the
          // operation type for that routing_rule is CREATE/UPDATE/INFO
          if ((current_routing_rule.operation_type() == OperationType::CREATE) ||
              (current_routing_rule.operation_type() == OperationType::UPDATE) ||
              (current_routing_rule.operation_type() == OperationType::INFO)) {
            routing_rule_entry new_routing_rule_entry;

            if (is_routing_rule_exist) {
              new_routing_rule_entry =
                      new_subnet_routing_table_entry
                              .routing_rules[current_routing_rule.id()];
            }

            new_routing_rule_entry.next_hop_ip = current_routing_rule.next_hop_ip();
            new_routing_rule_entry.priority = current_routing_rule.priority();
            new_routing_rule_entry.destination_type =
                    current_routing_rule.routing_rule_extra_info().destination_type();
            new_routing_rule_entry.next_hop_mac =
                    current_routing_rule.routing_rule_extra_info().next_hop_mac();

            if (!is_routing_rule_exist) {
              new_subnet_routing_table_entry.routing_rules.emplace(
                      current_routing_rule.id(), new_routing_rule_entry);

              ACA_LOG_INFO("Added routing table entry for routering rule id %s\n",
                           current_routing_rule.id().c_str());
            } else {
              ACA_LOG_INFO("Using existing routing table entry for routering rule id %s\n",
                           current_routing_rule.id().c_str());
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

          ACA_LOG_INFO("Added router subnet table entry for subnet id %s\n",
                       current_router_subnet_id.c_str());
        } else {
          ACA_LOG_INFO("Using existing router subnet table entry for subnet id %s\n",
                       current_router_subnet_id.c_str());
          new_subnet_routing_tables[current_router_subnet_id] = new_subnet_routing_table_entry;
        }
      } else {
        ACA_LOG_ERROR("Not able to find the info for router with subnet ID: %s.\n",
                      current_router_subnet_id.c_str());
        overall_rc = -EXIT_FAILURE;
      }

    } // for (int i = 0; i < current_RouterConfiguration.subnet_routing_tables_size(); i++)

    if (!is_router_exist || (current_RouterConfiguration.update_type() == UpdateType::FULL)) {
      // -----critical section starts-----
      _routers_table_mutex.lock();
      _routers_table.emplace(router_id, new_subnet_routing_tables);
      _routers_table_mutex.unlock();
      // -----critical section ends-----
      ACA_LOG_INFO("Added router entry for router id %s\n", router_id.c_str());
    } else {
      ACA_LOG_INFO("Using existing router entry for router id %s\n", router_id.c_str());
      // -----critical section starts-----
      _routers_table_mutex.lock();
      _routers_table[router_id] = new_subnet_routing_tables;
      _routers_table_mutex.unlock();
      // -----critical section ends-----
    }

  } catch (const std::invalid_argument &e) {
    ACA_LOG_ERROR("Invalid argument exception caught while parsing router configuration, message: %s.\n",
                  e.what());
    overall_rc = -EINVAL;
  } catch (const std::exception &e) {
    ACA_LOG_ERROR("Exception caught while parsing router configuration, message: %s.\n",
                  e.what());
    overall_rc = -EFAULT;
  } catch (...) {
    ACA_LOG_CRIT("%s", "Unknown exception caught while parsing router configuration.\n");
    overall_rc = -EFAULT;
  }

  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_or_update_router <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

int ACA_OVS_L3_Programmer::create_or_update_l3_neighbor(
        const string neighbor_id, const string vpc_id, const string subnet_id,
        const string virtual_ip, const string virtual_mac,
        const string remote_host_ip, uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_OVS_L3_Programmer::create_or_update_l3_neighbor ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;
  bool found_subnet_in_router = false;
  int source_vlan_id;
  int destination_vlan_id;
  string cmd_string;

  if (neighbor_id.empty()) {
    throw std::invalid_argument("neighbor_id is empty");
  }

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

  if (remote_host_ip.empty()) {
    throw std::invalid_argument("remote_host_ip is empty");
  }

  if (tunnel_id == 0) {
    throw std::invalid_argument("tunnel_id is 0");
  }

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
      string destination_gw_mac = found_subnet->second.gateway_mac;

      // for each other subnet connected to this router, create the routing rule
      for (auto subnet_it = router_it->second.begin();
           subnet_it != router_it->second.end(); subnet_it++) {
        if (subnet_it->first == subnet_id) {
          // for the destination subnet, add the neighbor port to track it
          neighbor_port_table_entry new_neighbor_port_table_entry;
          new_neighbor_port_table_entry.virtual_ip = virtual_ip;
          new_neighbor_port_table_entry.virtual_mac = virtual_mac;
          new_neighbor_port_table_entry.host_ip = remote_host_ip;
          subnet_it->second.neighbor_ports.emplace(neighbor_id, new_neighbor_port_table_entry);

          // skip the destination neighbor subnet for the static routing rule below
          // because routing rule are for source packet transformation
          continue;
        }
        ACA_LOG_DEBUG("Found L3 neighbor subnet_id:%s\n ", subnet_it->first.c_str());

        source_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(
                subnet_it->second.tunnel_id);

        destination_vlan_id =
                ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(tunnel_id);

        // for the first implementation, we will go ahead and program the on demand routing rule here
        // in the future, the programming of the on demand rule will be triggered by the first packet
        // sent to openflow controller, that's ACA

        // the openflow rule depends on whether the hosting ip is on this compute host or not
        if (is_port_on_same_host) {
          cmd_string = "add-flow br-tun \"table=0,priority=50,ip,dl_vlan=" +
                       to_string(source_vlan_id) + ",nw_dst=" + virtual_ip +
                       ",dl_dst=" + subnet_it->second.gateway_mac +
                       " actions=mod_vlan_vid:" + to_string(destination_vlan_id) +
                       ",mod_dl_src:" + destination_gw_mac +
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
    ACA_LOG_ERROR("subnet_id %s not found in our local routers\n", subnet_id.c_str());
    overall_rc = ENOENT;
  }

  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_or_update_l3_neighbor <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

int ACA_OVS_L3_Programmer::delete_l3_neighbor(const string neighbor_id, const string subnet_id,
                                              const string virtual_ip, ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_OVS_L3_Programmer::delete_l3_neighbor ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;
  bool found_subnet_in_router = false;
  int source_vlan_id;

  if (neighbor_id.empty()) {
    throw std::invalid_argument("neighbor_id is empty");
  }

  if (subnet_id.empty()) {
    throw std::invalid_argument("subnet_id is empty");
  }

  if (virtual_ip.empty()) {
    throw std::invalid_argument("virtual_ip is empty");
  }

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

      // for each other subnet connected to this router, delete the routing rule
      for (auto subnet_it = router_it->second.begin();
           subnet_it != router_it->second.end(); subnet_it++) {
        if (subnet_it->first == subnet_id) {
          // for the destination subnet, remove the tracking neighbor port
          if (subnet_it->second.neighbor_ports.erase(neighbor_id)) {
            ACA_LOG_INFO("Successfuly cleaned up entry for neighbor_id %s\n",
                         neighbor_id.c_str());
            overall_rc = EXIT_SUCCESS;
          } else {
            ACA_LOG_ERROR("Failed to clean up entry for neighbor_id %s\n",
                          neighbor_id.c_str());
            overall_rc = EXIT_FAILURE;
          }

          // skip the destination neighbor subnet for the static routing rule below
          // because routing rule are for source packet transformation
          continue;
        }
        ACA_LOG_DEBUG("subnet_id:%s\n ", subnet_it->first.c_str());

        source_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(
                subnet_it->second.tunnel_id);

        // for the first implementation with static routing rules (non on-demand)
        // go ahead to remove it
        string cmd_string = "del-flows br-tun \"table=0,priority=50,ip,dl_vlan=" +
                            to_string(source_vlan_id) + ",nw_dst=" + virtual_ip + "\" --strict";

        ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
                cmd_string, culminative_time, overall_rc);

        // once we have the on demand routing rule implemented, we will need remove any
        // on demand routing rule assoicated this deleted neighbor to stop the traffic
        // immediately, we cannot rely on the rule's idle timout
      }
      // we found our interested router from _routers_table which has the destination subnet GW connected to it.
      // Since each subnet GW can only be connected to one router, therefore, there is no point to look at other
      // routers on the higher level for (auto router_it = _routers_table.begin();...) loop
      break;
    }
  }

  if (!found_subnet_in_router) {
    ACA_LOG_ERROR("subnet_id %s not found in our local routers\n", subnet_id.c_str());
    overall_rc = ENOENT;
  }

  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::delete_l3_neighbor <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

} // namespace aca_ovs_l3_programmer
