
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

#include "aca_oam_port_manager.h"
#include "aca_log.h"
#include <errno.h>
#include "aca_ovs_control.h"

using namespace aca_ovs_control;

namespace aca_oam_port_manager
{
Aca_Oam_Port_Manager::Aca_Oam_Port_Manager()
{
}

Aca_Oam_Port_Manager::~Aca_Oam_Port_Manager()
{
  //clear all oam punt rules
  for (auto entry : _oam_ports_table) {
    _delete_oam_ofp(entry.first);
  }
  _clear_all_data();
}

Aca_Oam_Port_Manager &Aca_Oam_Port_Manager::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static Aca_Oam_Port_Manager instance;
  return instance;
}

// add the OAM punt rule
void Aca_Oam_Port_Manager::_create_oam_ofp(uint32_t port_id)
{
  int overall_rc;

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
  int overall_rc;

  string opt = "udp,udp_dst=" + to_string(port_id);

  overall_rc = ACA_OVS_Control::get_instance().del_flows("br-int", opt.c_str());

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "delete_oam_ofp succeeded!\n");
  } else {
    ACA_LOG_ERROR("delete_oam_ofp failed!!! overrall_rc: %d\n", overall_rc);
  }
  return overall_rc;
}

void Aca_Oam_Port_Manager::_clear_all_data()
{
  ACA_LOG_DEBUG("%s", "Aca_Oam_Port_Manager::clear_all_data ---> Entering\n");

  // -----critical section starts-----
  _oam_ports_table_mutex.lock();
  // All the elements in the unordered_map container are dropped:
  // their destructors are called, and they are removed from the container,
  // leaving _oam_ports_table with a size of 0.
  _oam_ports_table.clear();
  _oam_ports_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("%s", "Aca_Oam_Port_Manager::clear_all_data <--- Exiting\n");
}

// unsafe function, needs to be called inside oam_ports_table_mutex lock
// this function assumes there is no existing entry for port_id
void Aca_Oam_Port_Manager::create_entry_unsafe(uint32_t port_id)
{
  ACA_LOG_DEBUG("%s", "Aca_Oam_Port_Manager::create_entry_unsafe ---> Entering\n");

  unordered_set<uint32_t> tunnel_ids_table;
  _oam_ports_table.emplace(port_id, tunnel_ids_table);

  ACA_LOG_DEBUG("%s", "Aca_Oam_Port_Manager::create_entry_unsafe <--- Exiting\n");
}

// update oam_ports_table and add the OAM punt rule also if this is the first port in the VPC
void Aca_Oam_Port_Manager::add_vpc(uint32_t port_id, uint32_t tunnel_id)
{
  ACA_LOG_DEBUG("%s", "Aca_Oam_Port_Manager::add_vpc ---> Entering\n");
  // -----critical section starts-----
  _oam_ports_table_mutex.lock();
  if (_oam_ports_table.find(port_id) == _oam_ports_table.end()) {
    create_entry_unsafe(port_id);
    _create_oam_ofp(port_id);
  }
  _oam_ports_table[port_id].emplace(tunnel_id);

  _oam_ports_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("%s", "ACA_OVS_Programmer::add_vpc <--- Exiting\n");
}

// update oam_ports_table and delete the OAM punt rule if the last port in the VPC has been deleted
int Aca_Oam_Port_Manager::remove_vpc(uint32_t port_id, uint32_t tunnel_id)
{
  ACA_LOG_DEBUG("%s", "Aca_Oam_Port_Manager::remove_vpc ---> Entering\n");

  int overall_rc;

  // -----critical section starts-----
  _oam_ports_table_mutex.lock();
  if (_oam_ports_table.find(port_id) == _oam_ports_table.end()) {
    ACA_LOG_ERROR("port id %u not find in oam_ports_table\n", port_id);
    overall_rc = ENOENT;
  } else {
    _oam_ports_table[port_id].erase(tunnel_id);
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

  bool overall_rc;

  // -----critical section starts-----
  _oam_ports_table_mutex.lock();
  if (_oam_ports_table.find(port_id) == _oam_ports_table.end()) {
    ACA_LOG_ERROR("port id %u not find in oam_ports_table\n", port_id);
    overall_rc = false;
  } else {
    overall_rc = true;
  }
  _oam_ports_table_mutex.unlock();
  // -----critical section ends-----
  ACA_LOG_DEBUG("Aca_Oam_Port_Manager::is_oam_server_port <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

} // namespace aca_oam_port_manager