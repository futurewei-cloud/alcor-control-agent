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

int ACA_Zeta_Programming::create_or_update_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                                             const string vpc_id)
{
  zeta_config stZetaCfg;
  int overall_rc = EXIT_SUCCESS;

  stZetaCfg.group_id = current_AuxGateway.id();
  for (auto destination : current_AuxGateway.destinations()) {
    stZetaCfg.zeta_buckets.push_back(destination.ip_address());
  }
  uint32_t oam_server_port = current_AuxGateway.zeta_info().port_inband_operation();

  uint32_t port;
  ACA_Vlan_Manager::get_instance().get_oam_server_port(vpc_id, &port);
  // oam_server_port is not set
  if (port == 0) {
    ACA_Vlan_Manager::get_instance().set_oam_server_port(vpc_id, oam_server_port);

    //update oam_ports_table and add the OAM punt rule also if this is the first port in the VPC
    Aca_Oam_Port_Manager::get_instance().add_vpc(oam_server_port, vpc_id);
  }
  // add the group bucket rule
  overall_rc = _update_zeta_group_entry(&stZetaCfg);

  return overall_rc;
}

int ACA_Zeta_Programming::delete_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                                             const string vpc_id)
{
  zeta_config stZetaCfg;
  int overall_rc = EXIT_SUCCESS;

  stZetaCfg.group_id = current_AuxGateway.id();
  for (auto destination : current_AuxGateway.destinations()) {
    stZetaCfg.zeta_buckets.push_back(destination.ip_address());
  }
  uint32_t oam_server_port = current_AuxGateway.zeta_info().port_inband_operation();

  // Reset oam_server_port to 0
  ACA_Vlan_Manager::get_instance().set_oam_server_port(vpc_id, 0);

  // update oam_ports_table and delete the OAM punt rule if the last port in the VPC has been deleted
  Aca_Oam_Port_Manager::get_instance().remove_vpc(oam_server_port, vpc_id);

  // delete the group bucket rule
  overall_rc = _delete_zeta_group_entry(&stZetaCfg);

  return overall_rc;
}

int ACA_Zeta_Programming::_create_or_update_zeta_group_entry(zeta_config *zeta_cfg)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  //adding group table
  string cmd = "-O OpenFlow13 add-group br-tun group_id=" + zeta_cfg->group_id + ",type=select";
  list<string>::iterator it;
  for (it = zeta_cfg->zeta_buckets.begin(); it != zeta_cfg->zeta_buckets.end(); it++) {
    string outport_name = aca_get_outport_name(alcor::schema::NetworkType::VXLAN, *it);
    cmd += ",bucket=output:" + outport_name;
  }

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

  //deleting group table
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

} // namespace aca_zeta_programming