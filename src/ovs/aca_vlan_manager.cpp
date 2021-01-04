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
#include "aca_arp_responder.h"
#include <errno.h>
#include <algorithm>
#include <shared_mutex>
#include <arpa/inet.h>

using namespace aca_ovs_control;
using namespace aca_ovs_l2_programmer;
using namespace aca_arp_responder;

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
  int overall_rc = EXIT_FAILURE;

  bool vpc_table_entry_found = _vpcs_table.find(tunnel_id, current_vpc_table_entry);

  if (!vpc_table_entry_found) {
    create_entry(tunnel_id);

    // vpc_table_entry_found used here just in case if
    // vpc_table_entry is deleted by another thread between
    // create_entry(tunnel_id) above and the _vpcs_table.find below
    vpc_table_entry_found = _vpcs_table.find(tunnel_id, current_vpc_table_entry);
  }

  if (vpc_table_entry_found) {
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
  }

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

  arp_config stArpCfg;

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


  // create arp entry in arp responder for the l2 neighbor
  stArpCfg.mac_address = virtual_mac;
  stArpCfg.ipv4_address = virtual_ip;
  stArpCfg.vlan_id = internal_vlan_id;

  ACA_ARP_Responder::get_instance().create_or_update_arp_entry(&stArpCfg);
  
  ACA_LOG_DEBUG("create_l2_neighbor arp entry with ip = %s, vlan id = %u and mac = %s\n",virtual_ip.c_str(),internal_vlan_id, virtual_mac.c_str());

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

  arp_config stArpCfg;

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

  // delete arp entry in arp responder for the l2 neighbor
  stArpCfg.mac_address = virtual_mac;
  stArpCfg.ipv4_address = virtual_ip;
  stArpCfg.vlan_id = internal_vlan_id;

  ACA_ARP_Responder::get_instance().delete_arp_entry(&stArpCfg);
  ACA_LOG_DEBUG("delete_l2_neighbor arp entry with ip = %s, vlan id = %u and mac = %s\n",virtual_ip.c_str(),internal_vlan_id, virtual_mac.c_str());


  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::delete_l2_neighbor <--- Exiting\n");

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
