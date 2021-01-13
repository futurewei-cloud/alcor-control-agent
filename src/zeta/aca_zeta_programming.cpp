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
  // fetch the value first to used for new_zeta_cfg->group_id
  // then add 1 after, doing both atomically
  // std::memory_order_relaxed option won't help much for x86 but other
  // CPU architecture can take advantage of it

  new_zeta_cfg->oam_port = oam_port;
  new_zeta_cfg->group_id =
          current_available_group_id.fetch_add(1, std::memory_order_relaxed);

  // fill in the ip_address and mac_address of fwds
  for (auto destination : current_AuxGateway.destinations()) {
    fwd_info *new_fwd =
            new fwd_info(destination.ip_address(), destination.mac_address());
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

// add the OAM punt rule
int ACA_Zeta_Programming::_create_oam_ofp(uint port_number)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::_create_oam_ofp ---> Entering\n");
  int overall_rc;

  string opt = "table=0,priority=25,udp,udp_dst=" + to_string(port_number) + ",actions=CONTROLLER";
  overall_rc = ACA_OVS_Control::get_instance().add_flow("br-int", opt.c_str());

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "creat_oam_ofp succeeded!\n");
  } else {
    ACA_LOG_ERROR("creat_oam_ofp failed!!! overrall_rc: %d\n", overall_rc);
  }

  ACA_LOG_DEBUG("ACA_Zeta_Programming::_create_oam_ofp <--- Exiting, overall_rc = %d\n",
                overall_rc);
  return overall_rc;
}

// delete the OAM punt rule
int ACA_Zeta_Programming::_delete_oam_ofp(uint port_number)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::_delete_oam_ofp ---> Entering\n");
  int overall_rc;

  string opt = "udp,udp_dst=" + to_string(port_number);

  overall_rc = ACA_OVS_Control::get_instance().del_flows("br-int", opt.c_str());

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "delete_oam_ofp succeeded!\n");
  } else {
    ACA_LOG_ERROR("delete_oam_ofp failed!!! overrall_rc: %d\n", overall_rc);
  }

  ACA_LOG_DEBUG("ACA_Zeta_Programming::_delete_oam_ofp <--- Exiting, overall_rc = %d\n",
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

int ACA_Zeta_Programming::create_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                                             uint tunnel_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::create_zeta_config ---> Entering\n");
  int overall_rc = EXIT_SUCCESS;
  zeta_config *current_zeta_cfg;
  bool bucket_not_found = false;
  CTSL::HashMap<fwd_info *, int *> new_zeta_buckets;

  uint oam_port = current_AuxGateway.zeta_info().port_inband_operation();

  if (!_zeta_config_table.find(current_AuxGateway.id(), current_zeta_cfg)) {
    create_entry(current_AuxGateway.id(), oam_port, current_AuxGateway);
    _zeta_config_table.find(current_AuxGateway.id(), current_zeta_cfg);
    
    //-----Start unique lock-----
    std::unique_lock<std::timed_mutex> group_entry_lock(_group_operation_mutex);
    overall_rc = _create_zeta_group_entry(current_zeta_cfg);
    group_entry_lock.unlock();
    //-----End unique lock-----

    _create_oam_ofp(oam_port);
    // add oam port number to cache
    aca_zeta_oam_server::ACA_Zeta_Oam_Server::get_instance().add_oam_port_cache(oam_port);
  } else {
    if (current_zeta_cfg->zeta_buckets.hashSize !=
        (uint)current_AuxGateway.destinations().size()) {
      bucket_not_found = true;
    } else {
      for (auto destination : current_AuxGateway.destinations()) {
        fwd_info *target_fwd =
                new fwd_info(destination.ip_address(), destination.mac_address());

        int *found = nullptr;

        if (current_zeta_cfg->zeta_buckets.find(target_fwd, found)) {
          continue;
        } else {
          bucket_not_found |= true;
          break;
        }
        new_zeta_buckets.insert(target_fwd, nullptr);
      }
    }

    // If the buckets have changed, update the buckets and group table rules.
    if (bucket_not_found == true) {
      current_zeta_cfg->zeta_buckets.clear();
      for (auto destination : current_AuxGateway.destinations()) {
        fwd_info *target_fwd =
                new fwd_info(destination.ip_address(), destination.mac_address());

        current_zeta_cfg->zeta_buckets.insert(target_fwd, nullptr);
      }

      //-----Start unique lock-----
      std::unique_lock<std::timed_mutex> group_entry_lock(_group_operation_mutex);
      overall_rc = _update_zeta_group_entry(current_zeta_cfg);
      group_entry_lock.unlock();
      //-----End unique lock-----
    }
  }

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
      _delete_oam_ofp(current_zeta_cfg->oam_port);
      _zeta_config_table.erase(current_AuxGateway.id());
    }
  }

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
        cmd += ",bucket=\"set_field:" + hash_node->getKey()->ip_addr +
               "->tun_dst,mod_dl_dst:" + hash_node->getKey()->mac_addr +
               ",output:vxlan-generic\"";
        hash_node = hash_node->next;
      }
      hash_bucket_lock.unlock();
      //-----End share lock to enable mutiple concurrent reads-----
    }
  }

  // add group table rule
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd, not_care_culminative_time, overall_rc);

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
        cmd += ",bucket=\"set_field:" + hash_node->getKey()->ip_addr +
               "->tun_dst,mod_dl_dst:" + hash_node->getKey()->mac_addr +
               ",output:vxlan-generic\"";
        hash_node = hash_node->next;
      }
      hash_bucket_lock.unlock();
      //-----End share lock to enable mutiple concurrent reads-----
    }
  }

  // add group table rule
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd, not_care_culminative_time, overall_rc);

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

bool ACA_Zeta_Programming::oam_port_rule_exists(uint port_number)
{
  int overall_rc = EXIT_FAILURE;

  string opt = "table=0,udp,udp_dst=" + to_string(port_number);

  overall_rc = ACA_OVS_Control::get_instance().flow_exists("br_tun", opt.c_str());
  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Oam port rule is exist!\n");
    return true;
  } else {
    ACA_LOG_INFO("%s", "Oam port rule is not exist!\n");
    return false;
  }
}

uint ACA_Zeta_Programming::get_group_id(string zeta_gateway_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::get_group_id ---> Entering\n");
  uint group_id = 0;
  zeta_config *current_zeta_cfg;

  if (!_zeta_config_table.find(zeta_gateway_id, current_zeta_cfg)) {
    group_id = current_zeta_cfg->group_id;
  } else {
    ACA_LOG_ERROR("zeta_gateway_id %s not found in zeta_config_table\n",
                  zeta_gateway_id.c_str());
  }

  return group_id;

  ACA_LOG_DEBUG("ACA_Zeta_Programming::get_group_id <--- Exiting, overall_rc = %u\n", group_id);
}

} // namespace aca_zeta_programming