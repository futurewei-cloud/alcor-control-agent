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
#include "aca_oam_port_manager.h"

using namespace aca_vlan_manager;
using namespace aca_oam_port_manager;
using namespace alcor::schema;
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

  _zeta_gateways_table.emplace(auxGateway_id, current_available_group_id.load());
  current_available_group_id++;

  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::create_entry_unsafe <--- Exiting\n");
}

uint ACA_Zeta_Programming::get_or_create_group_id(string auxGateway_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::get_or_create_vlan_id ---> Entering\n");

  // -----critical section starts-----
  _zeta_gateways_table_mutex.lock();
  if (_zeta_gateways_table.find(auxGateway_id) == _zeta_gateways_table.end()) {
    create_entry_unsafe(auxGateway_id);
  }
  uint acquired_group_id = _zeta_gateways_table[auxGateway_id];
  _zeta_gateways_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("%s", "ACA_Zeta_Programming::get_or_create_vlan_id <--- Exiting\n");

  return acquired_group_id;
}

int ACA_Zeta_Programming::create_or_update_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                                                       const string /*vpc_id*/, uint tunnel_id)
{
  unsigned long not_care_culminative_time;
  int overall_rc;
  zeta_config stZetaCfg;

  stZetaCfg.group_id = current_AuxGateway.id();
  for (auto destination : current_AuxGateway.destinations()) {
    stZetaCfg.zeta_buckets.push_back(destination.ip_address());
    string remote_host_ip = destination.ip_address();
    if (!aca_is_port_on_same_host(remote_host_ip)) {
      ACA_LOG_INFO("%s", "port_neighbor not exist!\n");
      //crate neighbor_port
      aca_vlan_manager::ACA_Vlan_Manager::get_instance().create_neighbor_outport(
              alcor::schema::NetworkType::VXLAN, remote_host_ip, tunnel_id,
              not_care_culminative_time);
    }
  }
  uint oam_server_port = current_AuxGateway.zeta_info().port_inband_operation();

  uint oam_port = ACA_Vlan_Manager::get_instance().get_oam_server_port(tunnel_id);
  // oam_server_port is not set
  if (oam_port == 0) {
    ACA_Vlan_Manager::get_instance().set_oam_server_port(tunnel_id, oam_server_port);

    //update oam_ports_cache and add the OAM punt rule also if this is the first port in the VPC
    Aca_Oam_Port_Manager::get_instance().add_oam_port_rule(oam_server_port);
  }
  // add the group bucket rule
  overall_rc = _create_or_update_zeta_group_entry(&stZetaCfg);

  return overall_rc;
}




int ACA_Zeta_Programming::delete_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                                             const string /*vpc_id*/, uint tunnel_id)
{
  zeta_config stZetaCfg;
  int overall_rc;

  stZetaCfg.group_id = current_AuxGateway.id();
  for (auto destination : current_AuxGateway.destinations()) {
    stZetaCfg.zeta_buckets.push_back(destination.ip_address());
  }
  uint oam_server_port = current_AuxGateway.zeta_info().port_inband_operation();

  // Reset oam_server_port to 0
  ACA_Vlan_Manager::get_instance().set_oam_server_port(tunnel_id, 0);

  // update oam_ports_cache and delete the OAM punt rule if the last port in the VPC has been deleted
  if (!ACA_Vlan_Manager::get_instance().is_exist_oam_port(oam_server_port)) {
    Aca_Oam_Port_Manager::get_instance().remove_oam_port_rule(oam_server_port);
  }
  // delete the group bucket rule
  overall_rc = _delete_zeta_group_entry(&stZetaCfg);

  return overall_rc;
}

int ACA_Zeta_Programming::_create_or_update_zeta_group_entry(zeta_config *zeta_cfg)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  uint group_id = get_or_create_group_id(zeta_cfg->group_id);

  //adding group table rule
  string cmd = "-O OpenFlow13 add-group br-tun group_id=" + to_string(group_id) + ",type=select";
  list<string>::iterator it;
  for (it = zeta_cfg->zeta_buckets.begin(); it != zeta_cfg->zeta_buckets.end(); it++) {
    string outport_name = aca_get_outport_name(alcor::schema::NetworkType::VXLAN, *it);
    cmd += ",bucket=output:" + outport_name;
  }

  //add flow for i
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
  uint group_id = get_or_create_group_id(zeta_cfg->group_id);

  //deleting group table
  string cmd = "-O OpenFlow13 del-groups br-tun group_id=" + to_string(group_id);
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "delete_zeta_group_entry succeeded!\n");
  } else {
    ACA_LOG_ERROR("delete_zeta_group_entry failed!!! overrall_rc: %d\n", overall_rc);
  }

  return overall_rc;
}

} // namespace aca_zeta_programming