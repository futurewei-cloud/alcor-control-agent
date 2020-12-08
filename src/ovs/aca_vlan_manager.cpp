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
#include "aca_vlan_manager.h"
#include "aca_ovs_control.h"
#include "aca_ovs_l2_programmer.h"
#include <errno.h>
#include <algorithm>
#include <shared_mutex>
#include <arpa/inet.h>

using namespace aca_ovs_control;
using namespace aca_ovs_l2_programmer;

extern std::atomic_ulong g_total_vpcs_table_mutex_time;

namespace aca_vlan_manager
{
ACA_Vlan_Manager &ACA_Vlan_Manager::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static ACA_Vlan_Manager instance;
  return instance;
}

void ACA_Vlan_Manager::clear_all_data()
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::clear_all_data ---> Entering\n");

  // All the elements in the container are deleted:
  // their destructors are called, and they are removed from the container,
  // leaving an empty _vpcs_table.
  _vpcs_table.clear();

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::clear_all_data <--- Exiting\n");
}

// this function assumes there is no existing entry for vpc_id
void ACA_Vlan_Manager::create_entry(uint tunnel_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_entry ---> Entering\n");

  vpc_table_entry *new_vpc_table_entry = new vpc_table_entry;
  // fetch the value first to used for new_vpc_table_entry->vlan_id
  // then add 1 after, doing both atomically
  // std::memory_order_relaxed option won't help much for x86 but other
  // CPU architecture can take advantage of it
  new_vpc_table_entry->vlan_id =
          current_available_vlan_id.fetch_add(1, std::memory_order_relaxed);

  _vpcs_table.insert(tunnel_id, new_vpc_table_entry);

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_entry <--- Exiting\n");
}

uint ACA_Vlan_Manager::get_or_create_vlan_id(uint tunnel_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_or_create_vlan_id ---> Entering\n");

  vpc_table_entry *new_vpc_table_entry;

  if (!_vpcs_table.find(tunnel_id, new_vpc_table_entry)) {
    create_entry(tunnel_id);

    _vpcs_table.find(tunnel_id, new_vpc_table_entry);
  }
  uint acquired_vlan_id = new_vpc_table_entry->vlan_id;

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_or_create_vlan_id <--- Exiting\n");

  return acquired_vlan_id;
}

int ACA_Vlan_Manager::create_ovs_port(string /*vpc_id*/, string ovs_port,
                                      uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_ovs_port ---> Entering\n");

  vpc_table_entry *current_vpc_table_entry;
  int overall_rc = EXIT_SUCCESS;

  if (!_vpcs_table.find(tunnel_id, current_vpc_table_entry)) {
    create_entry(tunnel_id);

    _vpcs_table.find(tunnel_id, current_vpc_table_entry);
  }

  // first port in the VPC will add the below rule:
  // table 4 = incoming vxlan, allow incoming vxlan traffic matching tunnel_id
  // to stamp with internal vlan and deliver to br-int
  if (current_vpc_table_entry->ovs_ports.empty()) {
    int internal_vlan_id = current_vpc_table_entry->vlan_id;

    string cmd_string =
            "add-flow br-tun \"table=4, priority=1,tun_id=" + to_string(tunnel_id) +
            " actions=mod_vlan_vid:" + to_string(internal_vlan_id) + ",output:\"patch-int\"\"";

    ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd_string, culminative_time, overall_rc);
  }
  //-----Start exclusive lock to enable single write-----
  std::unique_lock<std::shared_timed_mutex> lock(current_vpc_table_entry->ovs_ports_mutex);
  current_vpc_table_entry->ovs_ports.push_back(ovs_port);
  lock.unlock();
  //-----End exclusive lock to enable single write-----

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_ovs_port <--- Exiting\n");

  return overall_rc;
}

// called when a port associated with a vpc on this host is deleted
int ACA_Vlan_Manager::delete_ovs_port(string /*vpc_id*/, string ovs_port,
                                      uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::delete_ovs_port ---> Entering\n");

  vpc_table_entry *current_vpc_table_entry;
  int overall_rc = EXIT_SUCCESS;

  if (!_vpcs_table.find(tunnel_id, current_vpc_table_entry)) {
    ACA_LOG_ERROR("tunnel_id %u not found in vpc_table\n", tunnel_id);
    overall_rc = ENOENT;
  } else {
    //-----Start exclusive lock to enable single writer-----
    std::unique_lock<std::shared_timed_mutex> lock(current_vpc_table_entry->ovs_ports_mutex);
    current_vpc_table_entry->ovs_ports.remove(ovs_port);

    // clean up the vpc_table entry if there is no port assoicated
    if (current_vpc_table_entry->ovs_ports.empty()) {
      _vpcs_table.erase(tunnel_id);

      int internal_vlan_id = current_vpc_table_entry->vlan_id;

      // also delete the rule assoicated with the VPC:
      // table 4 = incoming vxlan, allow incoming vxlan traffic matching tunnel_id
      // to stamp with internal vlan and deliver to br-int
      string cmd_string =
              "del-flows br-tun \"table=4, priority=1,tun_id=" + to_string(tunnel_id) +
              " actions=mod_vlan_vid:" + to_string(internal_vlan_id) + "\" --strict";

      ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
              cmd_string, culminative_time, overall_rc);
    }
    lock.unlock();
    //-----End exclusive lock to enable single writer-----
  }

  ACA_LOG_DEBUG("ACA_Vlan_Manager::delete_ovs_port <--- Exiting, overall_rc = %d\n", overall_rc);

  return overall_rc;
}

int ACA_Vlan_Manager::create_l2_neighbor(string virtual_ip, string virtual_mac,
                                         string remote_host_ip, uint tunnel_id,
                                         ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_l2_neighbor ---> Entering\n");

  char hex_ip_buffer[HEX_IP_BUFFER_SIZE];
  int overall_rc = EXIT_SUCCESS;

  int internal_vlan_id = get_or_create_vlan_id(tunnel_id);

  // match internal vlan based on VPC and destination neighbor mac,
  // strip the internal vlan, encap with tunnel id,
  // output to the neighbor host through vxlan-generic ovs port
  string cmd_string = "add-flow br-tun table=20,priority=50,dl_vlan=" +
                      to_string(internal_vlan_id) + ",dl_dst:" + virtual_mac +
                      ",\"actions=strip_vlan,load:" + to_string(tunnel_id) +
                      "->NXM_NX_TUN_ID[],set_field:" + remote_host_ip +
                      "->tun_dst,output:vxlan-generic\"";

  ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd_string, culminative_time, overall_rc);

  // add the static arp responder for this l2 neighbor
  string current_virtual_mac = virtual_mac;
  current_virtual_mac.erase(
          remove(current_virtual_mac.begin(), current_virtual_mac.end(), ':'),
          current_virtual_mac.end());

  int addr = inet_network(virtual_ip.c_str());
  snprintf(hex_ip_buffer, HEX_IP_BUFFER_SIZE, "0x%08x", addr);

  cmd_string = "add-flow br-tun \"table=51,priority=50,arp,dl_vlan=" +
               to_string(internal_vlan_id) + ",nw_dst=" + virtual_ip +
               " actions=move:NXM_OF_ETH_SRC[]->NXM_OF_ETH_DST[],mod_dl_src:" + virtual_mac +
               ",load:0x2->NXM_OF_ARP_OP[],move:NXM_NX_ARP_SHA[]->NXM_NX_ARP_THA[],move:NXM_OF_ARP_SPA[]->NXM_OF_ARP_TPA[],load:0x" +
               current_virtual_mac + "->NXM_NX_ARP_SHA[],load:" + string(hex_ip_buffer) +
               "->NXM_OF_ARP_SPA[],in_port\"";

  ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd_string, culminative_time, overall_rc);

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_l2_neighbor <--- Exiting\n");

  return overall_rc;
}

// called when a L2 neighbor is deleted
int ACA_Vlan_Manager::delete_l2_neighbor(string virtual_ip, string virtual_mac,
                                         uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::delete_l2_neighbor ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;

  int internal_vlan_id = get_or_create_vlan_id(tunnel_id);

  // delete the rule l2 neighbor rule
  string cmd_string = "del-flows br-tun \"table=20,priority=50,dl_vlan=" +
                      to_string(internal_vlan_id) + ",dl_dst:" + virtual_mac + "\" --strict";

  ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd_string, culminative_time, overall_rc);

  // delete the static arp responder for this l2 neighbor
  cmd_string = "del-flows br-tun \"table=51,arp,dl_vlan=" + to_string(internal_vlan_id) +
               ",nw_dst=" + virtual_ip + "\"";

  ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd_string, culminative_time, overall_rc);

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::delete_l2_neighbor <--- Exiting\n");

  return overall_rc;
}

// the below three "outport" functions are deprecated and not used
// keeping them below to avoid merge conflict with other ACA change in PR
int ACA_Vlan_Manager::create_neighbor_outport(string neighbor_id, string /*vpc_id*/,
                                              alcor::schema::NetworkType network_type,
                                              string remote_host_ip, uint tunnel_id,
                                              ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_neighbor_outport ---> Entering\n");

  vpc_table_entry *current_vpc_table_entry;
  int overall_rc = EXIT_SUCCESS;

  string outport_name = aca_get_outport_name(network_type, remote_host_ip);

  if (!_vpcs_table.find(tunnel_id, current_vpc_table_entry)) {
    create_entry(tunnel_id);

    _vpcs_table.find(tunnel_id, current_vpc_table_entry);
  }

  auto current_outports_neighbors_table = current_vpc_table_entry->outports_neighbors_table;

  auto vpcs_table_mutex_start = chrono::steady_clock::now();
  //-----Start exclusive lock to enable single writer-----
  std::unique_lock<std::shared_timed_mutex> lock(
          current_vpc_table_entry->outports_neighbors_table_mutex);
  if (current_outports_neighbors_table.find(outport_name) ==
      current_outports_neighbors_table.end()) {
    // outport is not there yet, need to create a new entry
    std::list<string> neighbors(1, neighbor_id);
    current_vpc_table_entry->outports_neighbors_table.emplace(outport_name, neighbors);

    // since this is a new outport, configure OVS and openflow rule
    string cmd_string =
            "--may-exist add-port br-tun " + outport_name + " -- set interface " +
            outport_name + " type=" + aca_get_network_type_string(network_type) +
            " options:df_default=true options:egress_pkt_mark=0 options:in_key=flow options:out_key=flow options:remote_ip=" +
            remote_host_ip;

    ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
            cmd_string, culminative_time, overall_rc);

    // incoming from neighbor through vxlan port (based on remote IP)
    cmd_string = "add-flow br-tun \"table=0,priority=25,in_port=\"" +
                 outport_name + "\" actions=resubmit(,4)\"";

    ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd_string, culminative_time, overall_rc);

    if (overall_rc == EXIT_SUCCESS) {
      int internal_vlan_id = current_vpc_table_entry->vlan_id;

      string full_outport_list;
      this->get_outports_unsafe(tunnel_id, full_outport_list);

      // match internal vlan based on VPC, output for all outports based on the same
      // tunnel ID (multicast traffic)
      cmd_string = "add-flow br-tun \"table=22,priority=1,dl_vlan=" + to_string(internal_vlan_id) +
                   " actions=strip_vlan,load:" + to_string(tunnel_id) +
                   "->NXM_NX_TUN_ID[]," + full_outport_list + "\"";

      ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
              cmd_string, culminative_time, overall_rc);
    }
  } else {
    // else outport is already there, simply insert the neighbor id into outports_neighbors_table
    current_vpc_table_entry->outports_neighbors_table[outport_name].push_back(neighbor_id);
  }
  lock.unlock();
  //-----End exclusive lock to enable single writer-----
  auto vpcs_table_mutex_end = chrono::steady_clock::now();

  g_total_vpcs_table_mutex_time +=
          cast_to_microseconds(vpcs_table_mutex_end - vpcs_table_mutex_start).count();

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_neighbor_outport <--- Exiting\n");

  return overall_rc;
}

// called when a L2 neighbor info is deleted
int ACA_Vlan_Manager::delete_neighbor_outport(string neighbor_id, uint tunnel_id,
                                              string outport_name, ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::delete_neighbor_outport ---> Entering\n");

  vpc_table_entry *current_vpc_table_entry;
  int overall_rc = EXIT_SUCCESS;
  string cmd_string;

  if (!_vpcs_table.find(tunnel_id, current_vpc_table_entry)) {
    ACA_LOG_ERROR("tunnel_id %u not found in vpc_table\n", tunnel_id);
    overall_rc = ENOENT;
  } else {
    auto current_outports_neighbors_table = current_vpc_table_entry->outports_neighbors_table;

    auto vpcs_table_mutex_start = chrono::steady_clock::now();
    //-----Start exclusive lock to enable single writer-----
    std::unique_lock<std::shared_timed_mutex> lock(
            current_vpc_table_entry->outports_neighbors_table_mutex);
    if (current_outports_neighbors_table.find(outport_name) !=
        current_outports_neighbors_table.end()) {
      current_outports_neighbors_table[outport_name].remove(neighbor_id);

      // if the particular outport has no more neighbor assoicated with it
      // clean up the ovs port and openflow rules
      if (current_outports_neighbors_table[outport_name].empty()) {
        int internal_vlan_id = current_vpc_table_entry->vlan_id;

        // remove the rule that match internal vlan based on VPC, output for all outports
        // based on the same tunnel ID (multicast traffic)
        cmd_string = "del-flows br-tun \"table=22,priority=1,dl_vlan=" +
                     to_string(internal_vlan_id) + "\" --strict";

        ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
                cmd_string, culminative_time, overall_rc);

        // delete the outports_neighbors_table[outport_name] entry
        if (current_vpc_table_entry->outports_neighbors_table.erase(outport_name) != 1) {
          ACA_LOG_ERROR("Failed to clean up entry for outport_name: %s\n",
                        outport_name.c_str());
          overall_rc = EXIT_FAILURE;
        }
      }

      // see if there is still *any* other vpc still using this outport
      bool outports_found = false;

      for (size_t i = 0; i < _vpcs_table.hashSize; i++) {
        auto hash_node = (_vpcs_table.hashTable[i]).head;
        if (hash_node == nullptr) {
          // no entry for this hashtable bucket, go look at the next bucket
          continue;
        } else {
          // found entry in this hashtable bucket, need to look into the list of hash_nodes
          //-----Start share lock to enable mutiple concurrent reads-----
          std::shared_lock<std::shared_timed_mutex> hash_bucket_lock(
                  (_vpcs_table.hashTable[i]).mutex_);

          while (hash_node != nullptr) {
            auto current_vpc_table_entry = hash_node->getValue();
            auto current_outports_neighbors_table =
                    current_vpc_table_entry->outports_neighbors_table;
            if (current_outports_neighbors_table.find(outport_name) !=
                current_outports_neighbors_table.end()) {
              ACA_LOG_ERROR("outports_found! outport_name: %s\n", outport_name.c_str());
              outports_found = true;
              break; // break out of the while loop on hash_nodes list
            }

            if (outports_found) {
              break; // break out of for loop on _vpcs_table
            } else {
              // outport not found yet, look at the next hash_node
              hash_node = hash_node->next;
            }
          }

          hash_bucket_lock.unlock();
          //-----End share lock to enable mutiple concurrent reads-----
        }
      }

      if (!outports_found) {
        // there is no one else using the outport for *all* vpcs,
        // go head to clean up the openflow rule then outport

        // remove the rule for incoming from neighbor through vxlan port
        cmd_string = "del-flows br-tun \"table=0,priority=1,in_port=\"" +
                     outport_name + "\"\" --strict";

        ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
                cmd_string, culminative_time, overall_rc);

        cmd_string = "del-port br-tun " + outport_name;

        ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
                cmd_string, culminative_time, overall_rc);
      }

      lock.unlock();
      //-----End exclusive lock to enable single writer-----
      auto vpcs_table_mutex_end = chrono::steady_clock::now();

      g_total_vpcs_table_mutex_time +=
              cast_to_microseconds(vpcs_table_mutex_end - vpcs_table_mutex_start)
                      .count();
    } else {
      ACA_LOG_ERROR("outport_name %s not found in outports_neighbors_table\n",
                    outport_name.c_str());
      overall_rc = ENOENT;
    }
  }

  ACA_LOG_DEBUG("ACA_Vlan_Manager::delete_neighbor_outport <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

// unsafe function, needs to be called under vpc_table_mutex lock
int ACA_Vlan_Manager::get_outports_unsafe(uint tunnel_id, string &outports)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_outports_unsafe ---> Entering\n");

  vpc_table_entry *current_vpc_table_entry;
  int overall_rc;
  static string OUTPUT = "output:\"";

  if (!_vpcs_table.find(tunnel_id, current_vpc_table_entry)) {
    ACA_LOG_ERROR("tunnel_id %u not found in vpc_table\n", tunnel_id);
    overall_rc = ENOENT;
  } else {
    outports.clear();
    auto current_outports_neighbors_table = current_vpc_table_entry->outports_neighbors_table;

    for (auto it : current_outports_neighbors_table) {
      if (outports.empty()) {
        outports = OUTPUT + it.first + "\"";
      } else {
        outports += ',' + OUTPUT + it.first + "\"";
      }
    }
    overall_rc = EXIT_SUCCESS;
  }

  ACA_LOG_DEBUG("ACA_Vlan_Manager::get_outports_unsafe <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

// create a neighbor port without specifying vpc_id and neighbor ID
int ACA_Vlan_Manager::create_neighbor_outport(alcor::schema::NetworkType network_type,
                                              string remote_host_ip, uint /*tunnel_id*/,
                                              ulong &culminative_time)
{
  int overall_rc = EXIT_SUCCESS;

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_neighbor_outport ---> Entering\n");

  string outport_name = aca_get_outport_name(network_type, remote_host_ip);

  // since this is a new outport, configure OVS and openflow rule
  string cmd_string =
          "--may-exist add-port br-tun " + outport_name + " -- set interface " +
          outport_name + " type=" + aca_get_network_type_string(network_type) +
          " options:df_default=true options:egress_pkt_mark=0 options:in_key=flow options:out_key=flow options:remote_ip=" +
          remote_host_ip;

  ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
          cmd_string, culminative_time, overall_rc);

  // incoming from neighbor through vxlan port (based on remote IP)
  cmd_string = "add-flow br-tun \"table=0,priority=25,in_port=\"" +
               outport_name + "\" actions=resubmit(,4)\"";

  ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd_string, culminative_time, overall_rc);

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_neighbor_outport <--- Exiting\n");

  return overall_rc;
}

void ACA_Vlan_Manager::set_aux_gateway(uint tunnel_id, const string auxGateway_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::set_aux_gateway ---> Entering\n");

  vpc_table_entry *new_vpc_table_entry;

  if (!_vpcs_table.find(tunnel_id, new_vpc_table_entry)) {
    create_entry(tunnel_id);

    _vpcs_table.find(tunnel_id, new_vpc_table_entry);
  }
  new_vpc_table_entry->auxGateway_id = auxGateway_id;

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::set_aux_gateway <--- Exiting\n");
}

string ACA_Vlan_Manager::get_aux_gateway_id(uint tunnel_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_aux_gateway_id ---> Entering\n");

  vpc_table_entry *current_vpc_table_entry;
  string auxGateway_id;

  if (!_vpcs_table.find(tunnel_id, current_vpc_table_entry)) {
    ACA_LOG_ERROR("tunnel_id %u not found in vpc_table\n", tunnel_id);
  } else {
    auxGateway_id = current_vpc_table_entry->auxGateway_id;
  }

  ACA_LOG_DEBUG("ACA_Vlan_Manager::get_aux_gateway_id <--- Exiting, auxGateway_id =%s\n",
                auxGateway_id.c_str());

  return auxGateway_id;
}

bool ACA_Vlan_Manager::is_exist_aux_gateway(string auxGateway_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_aux_gateway_id ---> Entering\n");
  bool auxGateway_id_found = false;

  for (size_t i = 0; i < _vpcs_table.hashSize; i++) {
    auto hash_node = (_vpcs_table.hashTable[i]).head;
    if (hash_node == nullptr) {
      // no entry for this hashtable bucket, go look at the next bucket
      continue;
    } else {
      // found entry in this hashtable bucket, need to look into the list of hash_nodes
      //-----Start share lock to enable mutiple concurrent reads-----
      std::shared_lock<std::shared_timed_mutex> hash_bucket_lock(
              (_vpcs_table.hashTable[i]).mutex_);

      while (hash_node != nullptr) {
        auto current_vpc_table_entry = hash_node->getValue();
        auto current_outports_neighbors_table = current_vpc_table_entry->outports_neighbors_table;
        if (hash_node->getValue()->auxGateway_id == auxGateway_id) {
          auxGateway_id_found = true;
          break;
        }

        if (auxGateway_id_found) {
          break; // break out of for loop on _vpcs_table
        } else {
          // aux_Gateway_id not found yet, look at the next hash_node
          hash_node = hash_node->next;
        }
      }

      hash_bucket_lock.unlock();
      //-----End share lock to enable mutiple concurrent reads-----
    }
  }

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_aux_gateway_id ---> Entering\n");

  return auxGateway_id_found;
}

} // namespace aca_vlan_manager
