
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
#include "aca_oam_port_manager.h"
#include <errno.h>
#include <algorithm>
#include "aca_ovs_control.h"

using namespace aca_ovs_control;

namespace aca_oam_port_manager
{
// unsafe function, needs to be called inside oam_ports_table_mutex lock
// this function assumes there is no existing entry for port_id
void Aca_Oam_Port_Manager::create_entry_unsafe(uint32_t port_id)
{
  ACA_LOG_DEBUG("%s", "Aca_Oam_Port_Manager::create_entry_unsafe ---> Entering\n");

  unordered_set<string> vpcs_table;
  _oam_ports_table.emplace(port_id, vpcs_table);

  ACA_LOG_DEBUG("%s", "Aca_Oam_Port_Manager::create_entry_unsafe <--- Exiting\n");
}

// add the OAM punt rule
void Aca_Oam_Port_Manager::_creat_oam_ofp(uint32_t port_id)
{
  int overall_rc = EXIT_SUCCESS;

   string opt = "table=0,priority=25,udp,udp_dst=" + to_string(port_id) + ",actions=CONTROLLER";

  overall_rc = ACA_OVS_Control::get_instance().add_flow("br-int", opt.c_str());

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "creat_oam_ofp succeeded!\n");
  } else {
    ACA_LOG_ERROR("creat_oam_ofp failed!!! overrall_rc: %d\n", overall_rc);
  }

  return;
}

// delete the OAM punt rule
int Aca_Oam_Port_Manager::_delete_oam_ofp(uint32_t port_id)
{
  int overall_rc = EXIT_SUCCESS;

  string opt = "udp,udp_dst=" + to_string(port_id);

  overall_rc = ACA_OVS_Control::get_instance().del_flows("br-int", opt.c_str());

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "delete_oam_ofp succeeded!\n");
  } else {
    ACA_LOG_ERROR("delete_oam_ofp failed!!! overrall_rc: %d\n", overall_rc);
  }
  return overall_rc;
}

// update oam_ports_table and add the OAM punt rule also if this is the first port in the VPC
void Aca_Oam_Port_Manager::add_vpc(uint32_t port_id, string vpc_id)
{
  ACA_LOG_DEBUG("%s", "Aca_Oam_Port_Manager::add_vpc ---> Entering\n");
  // -----critical section starts-----
  _oam_ports_table_mutex.lock();
  if (_oam_ports_table.find(port_id) == _oam_ports_table.end()) {
    create_entry_unsafe(port_id);
    _creat_oam_ofp(port_id);
  }
  _oam_ports_table[port_id].emplace(vpc_id);

  _oam_ports_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("%s", "ACA_OVS_Programmer::add_vpc <--- Exiting\n");
}

// update oam_ports_table and delete the OAM punt rule if the last port in the VPC has been deleted
int Aca_Oam_Port_Manager::remove_vpc(uint32_t port_id, string vpc_id)
{
  ACA_LOG_DEBUG("%s", "Aca_Oam_Port_Manager::remove_vpc ---> Entering\n");

  int overall_rc;

  // -----critical section starts-----
  _oam_ports_table_mutex.lock();
  if (_oam_ports_table.find(port_id) == _oam_ports_table.end()) {
    ACA_LOG_ERROR("port id %u not find in oam_ports_table\n", port_id);
    overall_rc = ENOENT;
  } else {
    _oam_ports_table[port_id].erase(vpc_id);
    // clean up the oam_ports_table entry and oam flow rule if there is no port assoicated
    if (_oam_ports_table[port_id].empty()) {
      if (_oam_ports_table.erase(port_id) == 1 && _delete_oam_ofp(port_id) == EXIT_SUCCESS) {
        ACA_LOG_INFO("Successfuly cleaned up entry for port_id %u\n", port_id);
        overall_rc = EXIT_SUCCESS;
      } else {
        ACA_LOG_ERROR("Failed to clean up entry for port_id %u\n", port_id);
        overall_rc = EXIT_FAILURE;
      }
    }
  }
  _oam_ports_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("Aca_Oam_Port_Manager::remove_vpc <--- Exiting, overall_rc = %d\n", overall_rc);

  return overall_rc;
}

// Determine whether the port is oam server port
bool Aca_Oam_Port_Manager::is_oam_server_port(uint32_t port_id)
{
  ACA_LOG_DEBUG("%s", "Aca_Oam_Port_Manager::is_oam_server_port ---> Entering\n");

  bool overall_rc = true;

  // -----critical section starts-----
  _oam_ports_table_mutex.lock();
  if (_oam_ports_table.find(port_id) == _oam_ports_table.end()) {
    ACA_LOG_ERROR("port id %u not find in oam_ports_table\n", port_id);
    overall_rc = false;
  }
  _oam_ports_table_mutex.unlock();
  // -----critical section ends-----
  ACA_LOG_DEBUG("Aca_Oam_Port_Manager::is_oam_server_port <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

} // namespace aca_oam_port_manager