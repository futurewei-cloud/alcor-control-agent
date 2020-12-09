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
#include "aca_oam_server.h"

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

int ACA_Zeta_Programming::create_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                                             const string /*vpc_id*/, uint tunnel_id)
{
  int overall_rc = EXIT_SUCCESS;

  zeta_config stZetaCfg;

  for (auto destination : current_AuxGateway.destinations()) {
    stZetaCfg.zeta_buckets.push_back(destination.ip_address());
  }

  uint oam_port = current_AuxGateway.zeta_info().port_inband_operation();

  ACA_Vlan_Manager::get_instance().set_zeta_gateway(
          tunnel_id, current_AuxGateway.id(), oam_port);

  // get the current auxgateway status of vpc
  auxgateway_entry auxgateway =
          ACA_Vlan_Manager::get_instance().get_auxgateway_unsafe(tunnel_id);

  if (auxgateway.auxGateway_id.empty()) {
    aca_oam_server::ACA_Oam_Server::get_instance().add_oam_port_cache(oam_port);

    stZetaCfg.group_id = auxgateway.group_id;
    // add oam port number to cache
    aca_oam_server::ACA_Oam_Server::get_instance().add_oam_port_cache(oam_port);

    overall_rc = _create_zeta_group_entry(&stZetaCfg);
  } else {
    ACA_LOG_INFO("%s", "The vpc currently has an auxgateway set!\n");
  }

  return overall_rc;
}
int ACA_Zeta_Programming::delete_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                                             const string /*vpc_id*/, uint tunnel_id)
{
  zeta_config stZetaCfg;
  int overall_rc = EXIT_SUCCESS;

  auxgateway_entry auxGateway =
          ACA_Vlan_Manager::get_instance().get_auxgateway_unsafe(tunnel_id);
  stZetaCfg.group_id = auxGateway.group_id;

  if (auxGateway.auxGateway_id.empty()) {
    ACA_LOG_INFO("%s", "No auxgateway is currently set for this vpc!\n");
  } else {
    if (auxGateway.auxGateway_id != current_AuxGateway.id()) {
      ACA_LOG_ERROR("%s", "The auxgateway_id is inconsistent with the auxgateway_id currently set by the vpc!\n");
    } else {
      ACA_LOG_INFO("%s", "Reset auxGateway to empty!\n");
      overall_rc = ACA_Vlan_Manager::get_instance().remove_zeta_gateway(tunnel_id);
    }
  }

  return overall_rc;
}

int ACA_Zeta_Programming::_create_zeta_group_entry(zeta_config *zeta_cfg)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;
  if (is_exist_group_rule(zeta_cfg->group_id)) {
    return overall_rc;
  }
  //adding group table rule
  string cmd = "-O OpenFlow13 add-group br-tun group_id=" + to_string(zeta_cfg->group_id) +
               ",type=select";
  list<string>::iterator it;
  for (it = zeta_cfg->zeta_buckets.begin(); it != zeta_cfg->zeta_buckets.end(); it++) {
    string outport_name = aca_get_outport_name(alcor::schema::NetworkType::VXLAN, *it);
    cmd += ",bucket=\"set_field:" + *it + "->tun_dst,output:vxlan-generic\"";
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
  string cmd = "-O OpenFlow13 del-groups br-tun group_id=" + to_string(zeta_cfg->group_id);
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "delete_zeta_group_entry succeeded!\n");
  } else {
    ACA_LOG_ERROR("delete_zeta_group_entry failed!!! overrall_rc: %d\n", overall_rc);
  }

  return overall_rc;
}

// Determine whether the group table rule already exists?
// I am not sure if this approach is appropriate?
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

} // namespace aca_zeta_programming