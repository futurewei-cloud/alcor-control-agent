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

#include "aca_zeta_programming.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_util.h"
#include "aca_log.h"
#include "aca_vlan_manager.h"
#include "aca_ovs_control.h"
#include "aca_zeta_oam_server.h"
#include <thread>

using namespace alcor::schema;
using namespace aca_ovs_control;
using namespace aca_vlan_manager;
using namespace aca_ovs_l2_programmer;

namespace aca_zeta_programming
{
ACA_Zeta_Programming::ACA_Zeta_Programming()
{
}

ACA_Zeta_Programming::~ACA_Zeta_Programming()
{
  clear_all_data();
}

ACA_Zeta_Programming &ACA_Zeta_Programming::get_instance()
{
  static ACA_Zeta_Programming instance;
  return instance;
}

void ACA_Zeta_Programming::create_entry(string zeta_gateway_id, uint oam_port,
                                        alcor::schema::AuxGateway current_AuxGateway)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::create_entry ---> Entering\n");

  zeta_config *new_zeta_cfg = new zeta_config;
  new_zeta_cfg->oam_port = oam_port;
  // fetch the value first to used for new_zeta_cfg->group_id
  // then add 1 after, doing both atomically
  // std::memory_order_relaxed option won't help much for x86 but other
  // CPU architecture can take advantage of it
  new_zeta_cfg->group_id =
          current_available_group_id.fetch_add(1, std::memory_order_relaxed);

  // fill in the ip_address and mac_address of fwds
  for (auto destination : current_AuxGateway.destinations()) {
    FWD_Info *new_fwd =
            new FWD_Info(destination.ip_address(), destination.mac_address());
    new_zeta_cfg->zeta_buckets.insert(new_fwd, nullptr);
  }

  _zeta_config_table.insert(zeta_gateway_id, new_zeta_cfg);

  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::create_entry <--- Exiting\n");
}

void ACA_Zeta_Programming::clear_all_data()
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::clear_all_data ---> Entering\n");

  // All the elements in the container are deleted:
  // their destructors are called, and they are removed from the container,
  // leaving an empty _zeta_config_table table.
  _zeta_config_table.clear();

  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::clear_all_data <--- Exiting\n");
}

int ACA_Zeta_Programming::_create_group_punt_rule(uint tunnel_id, uint group_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::_create_group_punt_rule ---> Entering\n");

  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  uint vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(tunnel_id);

  string opt = "add-flow br-tun table=22,priority=50,dl_vlan=" + to_string(vlan_id) +
               ",actions=\"strip_vlan,load:" + to_string(tunnel_id) +
               "->NXM_NX_TUN_ID[],group:" + to_string(group_id) + "\"";

  ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          opt, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "_create_group_punt_rule succeeded!\n");
  } else {
    ACA_LOG_ERROR("_create_group_punt_rule failed!!! overrall_rc: %d\n", overall_rc);
  }

  ACA_LOG_DEBUG("ACA_Zeta_Programming::_create_group_punt_rule <--- Exiting, overall_rc = %d\n",
                overall_rc);
  return overall_rc;
}

int ACA_Zeta_Programming::_delete_group_punt_rule(uint tunnel_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::_delete_group_punt_rule ---> Entering\n");
  int overall_rc;

  uint vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(tunnel_id);
  string opt = "table=22,priority=50,dl_vlan=" + to_string(vlan_id);

  overall_rc = ACA_OVS_Control::get_instance().del_flows("br-tun", opt.c_str());

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "_delete_group_punt_rule succeeded!\n");
  } else {
    ACA_LOG_ERROR("_delete_group_punt_rule failed!!! overrall_rc: %d\n", overall_rc);
  }

  ACA_LOG_DEBUG("ACA_Zeta_Programming::_delete_group_punt_rule <--- Exiting, overall_rc = %d\n",
                overall_rc);
  return overall_rc;
}

uint ACA_Zeta_Programming::get_oam_port(string zeta_gateway_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::get_oam_port ---> Entering\n");
  zeta_config *current_zeta_cfg;
  uint oam_port = 0;
  if (_zeta_config_table.find(zeta_gateway_id, current_zeta_cfg)) {
    oam_port = current_zeta_cfg->oam_port;
  } else {
    ACA_LOG_ERROR("zeta_gateway_id %s not found in zeta_config_table\n",
                  zeta_gateway_id.c_str());
  }

  ACA_LOG_DEBUG("ACA_Zeta_Programming::get_oam_port <--- Exiting, oam_port = %d\n", oam_port);
  return oam_port;
}

void start_upd_listener(uint oam_port_number)
{
  ACA_LOG_INFO("Starting a listener for port %d\n", oam_port_number);
  int packet_length;
  struct sockaddr_in portList;
  int len_inet;
  char packet_content[512];

  int socket_instance = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_instance == -1) {
    ACA_LOG_ERROR("Socket creation error: %d\n", errno);
  }
  memset(&portList, 0, sizeof portList);
  portList.sin_family = AF_INET;
  portList.sin_port = htons(oam_port_number);
  // listen to all interfaces
  portList.sin_addr.s_addr = htonl(INADDR_ANY);

  len_inet = sizeof portList;
  int bind_rc = bind(socket_instance, (struct sockaddr *)&portList, len_inet);
  if (bind_rc == -1) {
    ACA_LOG_ERROR("Socket binding error: %d\n", errno);
  }

  for (;;) {
    packet_length = recv(socket_instance, packet_content, sizeof packet_content, 0);
    if (packet_length < 0) {
      ACA_LOG_ERROR("Packet receiving error: %d\n", errno);
    }
    ACA_LOG_INFO("Packet length is %d\n", packet_length);
    ACA_LOG_INFO("Got this udp packet when listening to port %d\n", oam_port_number);
    ACA_OVS_Control::get_instance().print_payload(
            reinterpret_cast<const unsigned char *>(packet_content), packet_length);
    aca_zeta_oam_server::ACA_Zeta_Oam_Server::get_instance().oams_recv(
            (uint32_t)oam_port_number, packet_content);
  }
}

int ACA_Zeta_Programming::create_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                                             uint tunnel_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::create_zeta_config ---> Entering\n");
  int overall_rc = EXIT_SUCCESS;
  zeta_config *current_zeta_cfg;
  bool bucket_not_found = false;
  CTSL::HashMap<FWD_Info *, int *> new_zeta_buckets;

  uint oam_port = current_AuxGateway.zeta_info().port_inband_operation();

  // -----critical section starts-----
  _zeta_config_table_mutex.lock();
  if (!_zeta_config_table.find(current_AuxGateway.id(), current_zeta_cfg)) {
    create_entry(current_AuxGateway.id(), oam_port, current_AuxGateway);
    _zeta_config_table.find(current_AuxGateway.id(), current_zeta_cfg);
    overall_rc = _create_zeta_group_entry(current_zeta_cfg);

    ACA_LOG_INFO("Creating thread for port %d.\n", oam_port);
    std::thread *oam_port_listener_thread = NULL;
    // TODO: Need to track, and kill this thread when this zeta config is deleted.
    oam_port_listener_thread = new std::thread(std::bind(&start_upd_listener, oam_port));
    oam_port_listener_thread->detach();
    ACA_LOG_INFO("Created thread for port %d and it is detached.\n", oam_port);
  } else {
    for (auto destination : current_AuxGateway.destinations()) {
      FWD_Info *target_fwd =
              new FWD_Info(destination.ip_address(), destination.mac_address());

      int *not_used = nullptr;

      if (current_zeta_cfg->zeta_buckets.find(target_fwd, not_used)) {
        continue;
      } else {
        bucket_not_found |= true;
        break;
      }
      new_zeta_buckets.insert(target_fwd, nullptr);
    }

    // If the buckets have changed, update the buckets and group table rules.
    if (bucket_not_found == true) {
      current_zeta_cfg->zeta_buckets.clear();
      for (auto destination : current_AuxGateway.destinations()) {
        FWD_Info *target_fwd =
                new FWD_Info(destination.ip_address(), destination.mac_address());

        current_zeta_cfg->zeta_buckets.insert(target_fwd, nullptr);
      }

      overall_rc = _update_zeta_group_entry(current_zeta_cfg);
    }
  }
  _zeta_config_table_mutex.unlock();
  // -----critical section ends-----

  // get the current auxgateway_id of vpc
  string current_zeta_id = ACA_Vlan_Manager::get_instance().get_zeta_gateway_id(tunnel_id);
  if (current_zeta_id.empty()) {
    ACA_LOG_INFO("%s", "The vpc currently has not auxgateway set!\n");
    ACA_Vlan_Manager::get_instance().set_zeta_gateway(tunnel_id,
                                                      current_AuxGateway.id());
    _create_group_punt_rule(tunnel_id, current_zeta_cfg->group_id);
  } else {
    ACA_LOG_INFO("%s", "The vpc currently has an auxgateway set!\n");
  }

  ACA_LOG_DEBUG("ACA_Zeta_Programming::create_zeta_config <--- Exiting, overall_rc = %d\n",
                overall_rc);
  return overall_rc;
}

int ACA_Zeta_Programming::delete_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                                             uint tunnel_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::delete_zeta_config ---> Entering\n");
  zeta_config stZetaCfg;
  int overall_rc = EXIT_SUCCESS;

  zeta_config *current_zeta_cfg;

  // -----critical section starts-----
  _zeta_config_table_mutex.lock();
  if (!_zeta_config_table.find(current_AuxGateway.id(), current_zeta_cfg)) {
    ACA_LOG_ERROR("zeta_gateway_id %s not found in zeta_config_table\n",
                  current_AuxGateway.id().c_str());
  } else {
    string current_zeta_gateway_id =
            ACA_Vlan_Manager::get_instance().get_zeta_gateway_id(tunnel_id);

    if (current_zeta_gateway_id.empty()) {
      ACA_LOG_INFO("%s", "No auxgateway is currently set for this vpc!\n");
    } else {
      if (current_zeta_gateway_id != current_AuxGateway.id()) {
        ACA_LOG_ERROR("%s", "The auxgateway_id is inconsistent with the auxgateway_id currently set by the vpc!\n");
      } else {
        ACA_LOG_INFO("%s", "Reset auxGateway to empty!\n");
        _delete_group_punt_rule(tunnel_id);
        overall_rc = ACA_Vlan_Manager::get_instance().remove_zeta_gateway(tunnel_id);
      }
    }

    if (!ACA_Vlan_Manager::get_instance().is_exist_zeta_gateway(
                current_AuxGateway.id())) {
      overall_rc = _delete_zeta_group_entry(current_zeta_cfg);
      _zeta_config_table.erase(current_AuxGateway.id());
    }
  }
  _zeta_config_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("ACA_Zeta_Programming::delete_zeta_config <--- Exiting, overall_rc = %d\n",
                overall_rc);
  return overall_rc;
}

int ACA_Zeta_Programming::_create_zeta_group_entry(zeta_config *zeta_cfg)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::_create_zeta_group_entry ---> Entering\n");
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;
  // adding group table rule
  string cmd = "-O OpenFlow13 add-group br-tun group_id=" + to_string(zeta_cfg->group_id) +
               ",type=select";

  for (size_t i = 0; i < zeta_cfg->zeta_buckets.hashSize; i++) {
    auto hash_node = zeta_cfg->zeta_buckets.hashTable[i].head;
    if (hash_node == nullptr) {
      continue;
    } else {
      //-----Start share lock to enable mutiple concurrent reads-----
      std::shared_lock<std::shared_timed_mutex> hash_bucket_lock(
              (zeta_cfg->zeta_buckets.hashTable[i]).mutex_);

      while (hash_node != nullptr) {
        // add the static arp entries
        string static_arp_string = "arp -s " + hash_node->getKey()->ip_addr +
                                   " " + hash_node->getKey()->mac_addr;
        aca_net_config::Aca_Net_Config::get_instance().execute_system_command(static_arp_string);

        // fill zeta_gws
        cmd += ",bucket=\"set_field:" + hash_node->getKey()->ip_addr +
               "->tun_dst,mod_dl_dst:" + hash_node->getKey()->mac_addr +
               ",output:vxlan-generic\"";
        hash_node = hash_node->next;
      }
      hash_bucket_lock.unlock();
      //-----End share lock to enable mutiple concurrent reads-----
    }
  }

  //-----Start unique lock-----
  std::unique_lock<std::timed_mutex> group_entry_lock(_group_operation_mutex);
  // add group table rule
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd, not_care_culminative_time, overall_rc);
  group_entry_lock.unlock();
  //-----End unique lock-----

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "create_zeta_group_entry succeeded!\n");
  } else {
    ACA_LOG_ERROR("create_zeta_group_entry failed!!! overrall_rc: %d\n", overall_rc);
  }

  ACA_LOG_DEBUG("ACA_Zeta_Programming::_create_zeta_group_entry <--- Exiting, overall_rc = %d\n",
                overall_rc);
  return overall_rc;
}

int ACA_Zeta_Programming::_update_zeta_group_entry(zeta_config *zeta_cfg)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::_update_zeta_group_entry ---> Entering\n");
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;
  // adding group table rule
  string cmd = "-O OpenFlow13 mod-group br-tun group_id=" + to_string(zeta_cfg->group_id) +
               ",type=select";

  for (size_t i = 0; i < zeta_cfg->zeta_buckets.hashSize; i++) {
    auto hash_node = zeta_cfg->zeta_buckets.hashTable[i].head;
    if (hash_node == nullptr) {
      continue;
    } else {
      //-----Start share lock to enable mutiple concurrent reads-----
      std::shared_lock<std::shared_timed_mutex> hash_bucket_lock(
              (zeta_cfg->zeta_buckets.hashTable[i]).mutex_);

      while (hash_node != nullptr) {
        // add the static arp entries
        string static_arp_string = "arp -s " + hash_node->getKey()->ip_addr +
                                   " " + hash_node->getKey()->mac_addr;
        aca_net_config::Aca_Net_Config::get_instance().execute_system_command(static_arp_string);

        cmd += ",bucket=\"set_field:" + hash_node->getKey()->ip_addr +
               "->tun_dst,mod_dl_dst:" + hash_node->getKey()->mac_addr +
               ",output:vxlan-generic\"";
        hash_node = hash_node->next;
      }
      hash_bucket_lock.unlock();
      //-----End share lock to enable mutiple concurrent reads-----
    }
  }

  //-----Start unique lock-----
  std::unique_lock<std::timed_mutex> group_entry_lock(_group_operation_mutex);
  // add group table rule
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd, not_care_culminative_time, overall_rc);
  group_entry_lock.unlock();
  //-----End unique lock-----

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "update_zeta_group_entry succeeded!\n");
  } else {
    ACA_LOG_ERROR("update_zeta_group_entry failed!!! overrall_rc: %d\n", overall_rc);
  }

  ACA_LOG_DEBUG("ACA_Zeta_Programming::_update_zeta_group_entry <--- Exiting, overall_rc = %d\n",
                overall_rc);
  return overall_rc;
}

int ACA_Zeta_Programming::_delete_zeta_group_entry(zeta_config *zeta_cfg)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::_delete_zeta_group_entry ---> Entering\n");
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  // delete group table rule
  string cmd = "-O OpenFlow13 del-groups br-tun group_id=" + to_string(zeta_cfg->group_id);

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "delete_zeta_group_entry succeeded!\n");
  } else {
    ACA_LOG_ERROR("delete_zeta_group_entry failed!!! overrall_rc: %d\n", overall_rc);
  }

  for (size_t i = 0; i < zeta_cfg->zeta_buckets.hashSize; i++) {
    auto hash_node = zeta_cfg->zeta_buckets.hashTable[i].head;
    if (hash_node == nullptr) {
      continue;
    } else {
      //-----Start share lock to enable mutiple concurrent reads-----
      std::shared_lock<std::shared_timed_mutex> hash_bucket_lock(
              (zeta_cfg->zeta_buckets.hashTable[i]).mutex_);

      while (hash_node != nullptr) {
        // delete the static arp entries
        string static_arp_string = "arp -d " + hash_node->getKey()->ip_addr;
        aca_net_config::Aca_Net_Config::get_instance().execute_system_command(static_arp_string);
        hash_node = hash_node->next;
      }
      hash_bucket_lock.unlock();
      //-----End share lock to enable mutiple concurrent reads-----
    }
  }

  ACA_LOG_DEBUG("ACA_Zeta_Programming::_delete_zeta_group_entry <--- Exiting, overall_rc = %d\n",
                overall_rc);
  return overall_rc;
}

// Determine whether the group table rule already exists?
bool ACA_Zeta_Programming::group_rule_exists(uint group_id)
{
  bool overall_rc;

  string dump_flows = "ovs-ofctl -O OpenFlow13 dump-groups br-tun";
  string opt = "group_id=" + to_string(group_id);

  string cmd_string = dump_flows + " | grep " + opt;
  overall_rc = aca_net_config::Aca_Net_Config::get_instance().execute_system_command(cmd_string);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "group rule is exist!\n");
    return true;
  } else {
    ACA_LOG_INFO("%s", "group rule is not exist!\n");
    return false;
  }
}

uint ACA_Zeta_Programming::get_group_id(string zeta_gateway_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::get_group_id ---> Entering\n");
  uint group_id = 0;
  zeta_config *current_zeta_cfg;

  if (_zeta_config_table.find(zeta_gateway_id, current_zeta_cfg)) {
    group_id = current_zeta_cfg->group_id;
  } else {
    ACA_LOG_ERROR("zeta_gateway_id %s not found in zeta_config_table\n",
                  zeta_gateway_id.c_str());
  }

  return group_id;

  ACA_LOG_DEBUG("ACA_Zeta_Programming::get_group_id <--- Exiting, overall_rc = %u\n", group_id);
}

} // namespace aca_zeta_programming