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

  vpc_table_entry *new_vpc_table_entry = nullptr;
  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (!_vpcs_table.find(tunnel_id, new_vpc_table_entry)) {
    create_entry(tunnel_id);

    _vpcs_table.find(tunnel_id, new_vpc_table_entry);
  }
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----
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
  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  bool vpc_table_entry_found = _vpcs_table.find(tunnel_id, current_vpc_table_entry);

  if (!vpc_table_entry_found) {
    create_entry(tunnel_id);

    // vpc_table_entry_found used here just in case if
    // vpc_table_entry is deleted by another thread between
    // create_entry(tunnel_id) above and the _vpcs_table.find below
    vpc_table_entry_found = _vpcs_table.find(tunnel_id, current_vpc_table_entry);
  }
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----

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
    current_vpc_table_entry->ovs_ports.insert(ovs_port, nullptr);
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
    current_vpc_table_entry->ovs_ports.erase(ovs_port);

    // clean up the vpc_table entry if there is no port assoicated
    if (current_vpc_table_entry->ovs_ports.empty()) {
      _vpcs_table.erase(tunnel_id);

      // also delete the rule assoicated with the VPC:
      // table 4 = incoming vxlan, allow incoming vxlan traffic matching tunnel_id
      // to stamp with internal vlan and deliver to br-int
      string cmd_string = "del-flows br-tun \"table=4, priority=1,tun_id=" +
                          to_string(tunnel_id) + "\" --strict";

      ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
              cmd_string, culminative_time, overall_rc);
    }
  }

  ACA_LOG_DEBUG("ACA_Vlan_Manager::delete_ovs_port <--- Exiting, overall_rc = %d\n", overall_rc);

  return overall_rc;
}

int ACA_Vlan_Manager::create_l2_neighbor(string virtual_ip, string virtual_mac,
                                         string remote_host_ip, uint tunnel_id,
                                         ulong & /*culminative_time*/)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_l2_neighbor ---> Entering\n");

  int overall_rc;

  int internal_vlan_id = get_or_create_vlan_id(tunnel_id);

  arp_config stArpCfg;

  // match internal vlan based on VPC and destination neighbor mac,
  // strip the internal vlan, encap with tunnel id,
  // output to the neighbor host through vxlan-generic ovs port
  string match_string = "table=20,priority=50,dl_vlan=" + to_string(internal_vlan_id) +
                        ",dl_dst:" + virtual_mac;

  string action_string = ",actions=strip_vlan,load:" + to_string(tunnel_id) +
                         "->NXM_NX_TUN_ID[],set_field:" + remote_host_ip +
                         "->tun_dst,output:" + VXLAN_GENERIC_OUTPORT_NUMBER;
  std::chrono::_V2::steady_clock::time_point start_1 = std::chrono::steady_clock::now();

  overall_rc = ACA_OVS_Control::get_instance().add_flow(
          "br-tun", (match_string + action_string).c_str());
  std::chrono::_V2::steady_clock::time_point end_1 = std::chrono::steady_clock::now();
  auto message_total_operation_time_1 =
          std::chrono::duration_cast<std::chrono::microseconds>(end_1 - start_1).count();
  ACA_LOG_INFO("[create_l2_neighbor] Start adding ovs rule at: [%ld], finished at: [%ld]\nElapsed time for adding ovs rule for l2 neighbor took: %ld microseconds or %ld milliseconds\n",
               start_1, end_1, message_total_operation_time_1,
               (message_total_operation_time_1 / 1000));
  if (overall_rc != EXIT_SUCCESS) {
    ACA_LOG_ERROR("%s", "Failed to add L2 neighbor rule\n");
  };

  // create arp entry in arp responder for the l2 neighbor
  stArpCfg.mac_address = virtual_mac;
  stArpCfg.ipv4_address = virtual_ip;
  stArpCfg.vlan_id = internal_vlan_id;

  ACA_ARP_Responder::get_instance().create_or_update_arp_entry(&stArpCfg);

  ACA_LOG_DEBUG("create_l2_neighbor arp entry with ip = %s, vlan id = %u and mac = %s\n",
                virtual_ip.c_str(), internal_vlan_id, virtual_mac.c_str());

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_l2_neighbor <--- Exiting\n");

  return overall_rc;
}

// called when a L2 neighbor is deleted
int ACA_Vlan_Manager::delete_l2_neighbor(string virtual_ip, string virtual_mac,
                                         uint tunnel_id, ulong & /*culminative_time*/)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::delete_l2_neighbor ---> Entering\n");

  int rc;
  int overall_rc = EXIT_SUCCESS;

  int internal_vlan_id = get_or_create_vlan_id(tunnel_id);

  arp_config stArpCfg;

  // delete the rule l2 neighbor rule
  string match_string = "table=20,priority=50,dl_vlan=" + to_string(internal_vlan_id) +
                        ",dl_dst:" + virtual_mac;

  rc = ACA_OVS_Control::get_instance().del_flows("br-tun", match_string.c_str());

  if (rc != EXIT_SUCCESS) {
    ACA_LOG_ERROR("Failed to delete L2 neighbor rule, rc: %d\n", rc);
    overall_rc = EXIT_FAILURE;
  }

  // delete arp entry in arp responder for the l2 neighbor
  stArpCfg.mac_address = virtual_mac;
  stArpCfg.ipv4_address = virtual_ip;
  stArpCfg.vlan_id = internal_vlan_id;

  ACA_ARP_Responder::get_instance().delete_arp_entry(&stArpCfg);

  ACA_LOG_DEBUG("delete_l2_neighbor arp entry with ip = %s, vlan id = %u and mac = %s\n",
                virtual_ip.c_str(), internal_vlan_id, virtual_mac.c_str());

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::delete_l2_neighbor <--- Exiting\n");

  return overall_rc;
}

string ACA_Vlan_Manager::get_zeta_gateway_id(uint tunnel_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_zeta_gateway_id ---> Entering\n");

  vpc_table_entry *current_vpc_table_entry;
  string zeta_gateway_id;

  if (!_vpcs_table.find(tunnel_id, current_vpc_table_entry)) {
    ACA_LOG_ERROR("tunnel_id %u not found in vpc_table\n", tunnel_id);
  } else {
    zeta_gateway_id = current_vpc_table_entry->zeta_gateway_id;
  }

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_zeta_gateway_id <--- Entering\n");
  return zeta_gateway_id;
}

void ACA_Vlan_Manager::set_zeta_gateway(uint tunnel_id, const string auxGateway_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::set_zeta_gateway ---> Entering\n");

  vpc_table_entry *new_vpc_table_entry = nullptr;

  if (!_vpcs_table.find(tunnel_id, new_vpc_table_entry)) {
    create_entry(tunnel_id);

    _vpcs_table.find(tunnel_id, new_vpc_table_entry);
  }
  new_vpc_table_entry->zeta_gateway_id = auxGateway_id;

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::set_zeta_gateway <--- Exiting\n");
}

int ACA_Vlan_Manager::remove_zeta_gateway(uint tunnel_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::remove_zeta_gateway ---> Entering\n");
  int overall_rc = EXIT_SUCCESS;

  string zeta_gateway_id;
  vpc_table_entry *current_vpc_table_entry;

  if (!_vpcs_table.find(tunnel_id, current_vpc_table_entry)) {
    ACA_LOG_ERROR("tunnel_id %u not found in vpc_table\n", tunnel_id);
  } else {
    current_vpc_table_entry->zeta_gateway_id = "";
  }

  ACA_LOG_DEBUG("ACA_Vlan_Manager::remove_zeta_gateway <--- Exiting, overall_rc = %d\n",
                overall_rc);
  return overall_rc;
}

bool ACA_Vlan_Manager::is_exist_zeta_gateway(string zeta_gateway_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_aux_gateway_id ---> Entering\n");
  bool zeta_gateway_id_found = false;

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
        if (hash_node->getValue()->zeta_gateway_id == zeta_gateway_id) {
          zeta_gateway_id_found = true;
          break;
        }

        if (zeta_gateway_id_found) {
          break; // break out of for loop on _vpcs_table
        } else {
          // zeta_gateway_id not found yet, look at the next hash_node
          hash_node = hash_node->next;
        }
      }

      hash_bucket_lock.unlock();
      //-----End share lock to enable mutiple concurrent reads-----
    }
  }

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_aux_gateway_id <--- Exiting\n");

  return zeta_gateway_id_found;
}

uint ACA_Vlan_Manager::get_tunnelId_by_vlanId(uint vlan_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_tunnelId_by_vlanId ---> Entering\n");
  bool vlan_id_found = false;
  uint tunnel_id = 0;

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
        if (hash_node->getValue()->vlan_id == vlan_id) {
          vlan_id_found = true;
          tunnel_id = hash_node->getKey();
          break;
        }

        if (vlan_id_found) {
          break; // break out of for loop on _vpcs_table
        } else {
          // zeta_gateway_id not found yet, look at the next hash_node
          hash_node = hash_node->next;
        }
      }

      hash_bucket_lock.unlock();
      //-----End share lock to enable mutiple concurrent reads-----
    }
  }

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_tunnelId_by_vlanId <--- Exiting\n");

  return tunnel_id;
}

} // namespace aca_vlan_manager
