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
#include "aca_oam_port_manager.h"
#include "aca_vlan_manager.h"
#include "aca_ovs_control.h"

using namespace aca_oam_port_manager;
using namespace alcor::schema;
using namespace aca_ovs_control;
using namespace aca_vlan_manager;
namespace aca_zeta_programming
{
ACA_Zeta_Programming &ACA_Zeta_Programming::get_instance()
{
  static ACA_Zeta_Programming instance;
  return instance;
}

void ACA_Zeta_Programming::create_entry_unsafe(string auxGateway_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::create_entry_unsafe ---> Entering\n");
  aux_gateway_entry new_table_entry;
  new_table_entry.group_id = current_available_group_id.load();
  _zeta_gateways_table.emplace(auxGateway_id, new_table_entry);
  current_available_group_id++;

  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::create_entry_unsafe <--- Exiting\n");
}

uint ACA_Zeta_Programming::get_or_create_group_id(string auxGateway_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::get_or_create_group_id ---> Entering\n");

  // -----critical section starts-----
  _zeta_gateways_table_mutex.lock();
  if (_zeta_gateways_table.find(auxGateway_id) == _zeta_gateways_table.end()) {
    create_entry_unsafe(auxGateway_id);
  }
  uint acquired_group_id = _zeta_gateways_table[auxGateway_id].group_id;
  _zeta_gateways_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::get_or_create_group_id <--- Exiting\n");

  return acquired_group_id;
}

int ACA_Zeta_Programming::create_or_update_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                                                       const string /*vpc_id*/, uint tunnel_id)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  zeta_config stZetaCfg;
  uint group_id = get_or_create_group_id(current_AuxGateway.id());

  stZetaCfg.group_id = to_string(group_id);
  for (auto destination : current_AuxGateway.destinations()) {
    stZetaCfg.zeta_buckets.push_back(destination.ip_address());
    string remote_host_ip = destination.ip_address();
    if (!aca_is_port_on_same_host(remote_host_ip)) {
      ACA_LOG_INFO("%s", "port_neighbor not exist!\n");
      //crate neighbor_port
      ACA_Vlan_Manager::get_instance().create_neighbor_outport(
              alcor::schema::NetworkType::VXLAN, remote_host_ip, tunnel_id,
              not_care_culminative_time);
    }
  }

  uint oam_server_port = current_AuxGateway.zeta_info().port_inband_operation();
  string auxGateway_id = ACA_Vlan_Manager::get_instance().get_aux_gateway_id(tunnel_id);

  // auxGateway is not set
  if (auxGateway_id.empty()) {
    ACA_LOG_INFO("%s", "auxGateway_id is empty!\n");

    if (!is_exist_group_rule(group_id)) {
      // add the group bucket rule
      overall_rc = _create_or_update_zeta_group_entry(&stZetaCfg);
    }

    if (!Aca_Oam_Port_Manager::get_instance().is_exist_oam_port_rule(oam_server_port)) {
      //update oam_ports_cache and add the OAM punt rule also
      Aca_Oam_Port_Manager::get_instance().add_oam_port_rule(oam_server_port);
    }

    ACA_Vlan_Manager::get_instance().set_aux_gateway(tunnel_id, auxGateway_id);

    ACA_Zeta_Programming::get_instance().set_oam_server_port(auxGateway_id, oam_server_port);

  } else {
    ACA_LOG_INFO("%s", "auxGateway_id is not empty!\n");
  }

  return overall_rc;
}
int ACA_Zeta_Programming::delete_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                                             const string /*vpc_id*/, uint tunnel_id)
{
  zeta_config stZetaCfg;
  int overall_rc = EXIT_SUCCESS;

  uint group_id = get_or_create_group_id(current_AuxGateway.id());
  stZetaCfg.group_id = to_string(group_id);

  string auxGateway_id = current_AuxGateway.id();

  for (auto destination : current_AuxGateway.destinations()) {
    stZetaCfg.zeta_buckets.push_back(destination.ip_address());
  }
  uint oam_port = current_AuxGateway.zeta_info().port_inband_operation();

  // Reset auxGateway_id to empty
  ACA_Vlan_Manager::get_instance().set_aux_gateway(tunnel_id, "");

  if (!ACA_Vlan_Manager::get_instance().is_exist_aux_gateway(auxGateway_id)) {
    // update oam_ports_cache and delete the OAM punt rule
    Aca_Oam_Port_Manager::get_instance().remove_oam_port_rule(oam_port);
    // delete the group bucket rule
    overall_rc = _delete_zeta_group_entry(&stZetaCfg);
  }

  return overall_rc;
}

int ACA_Zeta_Programming::_create_or_update_zeta_group_entry(zeta_config *zeta_cfg)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  //adding group table rule
  string cmd = "-O OpenFlow13 add-group br-tun group_id=" + zeta_cfg->group_id + ",type=select";
  list<string>::iterator it;
  for (it = zeta_cfg->zeta_buckets.begin(); it != zeta_cfg->zeta_buckets.end(); it++) {
    string outport_name = aca_get_outport_name(alcor::schema::NetworkType::VXLAN, *it);
    cmd += ",bucket=output:" + outport_name;
  }

  //add group table rule
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "update_zeta_group_entry succeeded!\n");
  } else {
    ACA_LOG_ERROR("update_zeta_group_entry failed!!! overrall_rc: %d\n", overall_rc);
  }

  return overall_rc;
}

int ACA_Zeta_Programming::_delete_zeta_group_entry(zeta_config *zeta_cfg)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  //delete group table rule
  string cmd = "-O OpenFlow13 del-groups br-tun group_id=" + zeta_cfg->group_id;
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "delete_zeta_group_entry succeeded!\n");
  } else {
    ACA_LOG_ERROR("delete_zeta_group_entry failed!!! overrall_rc: %d\n", overall_rc);
  }

  return overall_rc;
}

bool ACA_Zeta_Programming::is_exist_group_rule(uint group_id)
{
  bool overall_rc;

  string dump_flows = "ovs-ofctl -O OpenFlow13 dump-groups br-tun";
  string opt = "group_id=" + to_string(group_id);

  string cmd_string = dump_flows + " | grep " + opt;
  overall_rc = aca_net_config::Aca_Net_Config::get_instance().execute_system_command(cmd_string);

  if (overall_rc == EXIT_SUCCESS) {
    return true;
  } else {
    return false;
  }
}

// query oam_port with auxGateway_id
uint ACA_Zeta_Programming::get_oam_server_port(string auxGateway_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::get_oam_server_port ---> Entering\n");

  uint port_number;

  // -----critical section starts-----
  _zeta_gateways_table_mutex.lock();
  if (_zeta_gateways_table.find(auxGateway_id) == _zeta_gateways_table.end()) {
    ACA_LOG_ERROR("auxGateway_id %s not find in _zeta_gateways_table\n",
                  auxGateway_id.c_str());
    // If the tunnel_id cannot be found, set the port number to 0.
    port_number = 0;
  } else {
    port_number = _zeta_gateways_table[auxGateway_id].oam_port;
  }
  _zeta_gateways_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("ACA_Zeta_Programming::get_oam_server_port <--- Exiting, port_number=%u\n",
                port_number);

  return port_number;
}

// Bind oam_server_port to auxGateway
void ACA_Zeta_Programming::set_oam_server_port(string auxGateway_id, uint port_number)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::set_oam_server_port ---> Entering\n");

  // -----critical section starts-----
  _zeta_gateways_table_mutex.lock();
  if (_zeta_gateways_table.find(auxGateway_id) == _zeta_gateways_table.end()) {
    create_entry_unsafe(auxGateway_id);
  }
  _zeta_gateways_table[auxGateway_id].oam_port = port_number;
  _zeta_gateways_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::set_oam_server_port <--- Exiting\n");
}

bool ACA_Zeta_Programming::is_exist_oam_port(uint port_number)
{
  bool rc = false;

  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::is_exist_oam_port ---> Entering\n");
  // -----critical section starts-----
  _zeta_gateways_table_mutex.lock();
  for (auto entry : _zeta_gateways_table) {
    aux_gateway_entry vpc_entry = entry.second;
    if (vpc_entry.oam_port == port_number) {
      rc = true;
      break;
    }
  }
  _zeta_gateways_table_mutex.unlock();
  // -----critical section ends-----
  ACA_LOG_DEBUG("ACA_Zeta_Programming::is_exist_oam_port <--- Exiting, rc = %d\n", rc);
  return rc;
}

} // namespace aca_zeta_programming