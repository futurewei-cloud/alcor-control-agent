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
#include "aca_vlan_manager.h"
#include <errno.h>
#include <algorithm>

namespace aca_vlan_manager
{
ACA_Vlan_Manager &ACA_Vlan_Manager::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static ACA_Vlan_Manager instance;
  return instance;
}

// unsafe function, needs to be called inside vpc_table_mutex lock
// this function assumes there is no existing entry for vpc_id
void ACA_Vlan_Manager::create_entry_unsafe(string vpc_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_entry_unsafe ---> Entering\n");

  vpc_table_entry new_table_entry;
  new_table_entry.vlan_id = current_available_vlan_id.load();
  current_available_vlan_id++;

  _vpcs_table.emplace(vpc_id, new_table_entry);

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_entry_unsafe <--- Exiting\n");
}

uint ACA_Vlan_Manager::get_or_create_vlan_id(string vpc_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::acquire_vlan_id ---> Entering\n");

  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (_vpcs_table.find(vpc_id) == _vpcs_table.end()) {
    create_entry_unsafe(vpc_id);
  }
  uint acquired_vlan_id = _vpcs_table[vpc_id].vlan_id;
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::acquire_vlan_id <--- Exiting\n");

  return acquired_vlan_id;
}

void ACA_Vlan_Manager::add_ovs_port(string vpc_id, string ovs_port)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::add_ovs_port ---> Entering\n");

  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (_vpcs_table.find(vpc_id) == _vpcs_table.end()) {
    create_entry_unsafe(vpc_id);
  }
  _vpcs_table[vpc_id].ovs_ports.push_back(ovs_port);
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::add_ovs_port <--- Exiting\n");
}

// called when a port associated with a vpc on this host is deleted
int ACA_Vlan_Manager::remove_ovs_port(string vpc_id, string ovs_port)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::remove_ovs_port ---> Entering\n");

  int overall_rc;

  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (_vpcs_table.find(vpc_id) == _vpcs_table.end()) {
    ACA_LOG_ERROR("vpc_id %s not find in vpc_table\n", vpc_id.c_str());
    overall_rc = ENOENT;
  } else {
    _vpcs_table[vpc_id].ovs_ports.remove(ovs_port);
    // clean up the vpc_table entry if there is no port assoicated
    if (_vpcs_table[vpc_id].ovs_ports.empty()) {
      if (_vpcs_table.erase(vpc_id) == 1) {
        ACA_LOG_INFO("Successfuly cleaned up entry for vpc_id %s\n", vpc_id.c_str());
        overall_rc = EXIT_SUCCESS;
      } else {
        ACA_LOG_ERROR("Failed to clean up entry for vpc_id %s\n", vpc_id.c_str());
        overall_rc = EXIT_FAILURE;
      }
    }
  }
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("ACA_Vlan_Manager::remove_ovs_port <--- Exiting, overall_rc = %d\n", overall_rc);

  return overall_rc;
}

void ACA_Vlan_Manager::add_outport(string vpc_id, string outport)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::add_outport ---> Entering\n");

  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (_vpcs_table.find(vpc_id) == _vpcs_table.end()) {
    create_entry_unsafe(vpc_id);
    _vpcs_table[vpc_id].outports.push_back(outport);
  } else {
    auto current_outports = _vpcs_table[vpc_id].outports;
    auto it = std::find(current_outports.begin(), current_outports.end(), outport);
    if (it == current_outports.end()) {
      _vpcs_table[vpc_id].outports.push_back(outport);
    } // else nothing to do if outport is already there
  }
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::add_outport <--- Exiting\n");
}

// called when a neighbor info is deleted
int ACA_Vlan_Manager::remove_outport(string vpc_id, string outport)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::remove_outport ---> Entering\n");

  int overall_rc;

  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (_vpcs_table.find(vpc_id) == _vpcs_table.end()) {
    ACA_LOG_ERROR("vpc_id %s not find in vpc_table\n", vpc_id.c_str());
    overall_rc = ENOENT;
  } else {
    _vpcs_table[vpc_id].outports.remove(outport);
    overall_rc = EXIT_SUCCESS;
  }
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("ACA_Vlan_Manager::remove_outport <--- Exiting, overall_rc = %d\n", overall_rc);

  return overall_rc;
}

int ACA_Vlan_Manager::get_outports(string vpc_id, string &outports)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_outports ---> Entering\n");

  int overall_rc;
  static string OUTPUT = "output:\"";

  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (_vpcs_table.find(vpc_id) == _vpcs_table.end()) {
    ACA_LOG_ERROR("vpc_id %s not find in vpc_table\n", vpc_id.c_str());
    overall_rc = ENOENT;
  } else {
    outports.clear();
    auto current_outports = _vpcs_table[vpc_id].outports;

    for (auto it = current_outports.begin(); it != current_outports.end(); it++) {
      if (it == current_outports.begin()) {
        outports = OUTPUT + *it + "\"";
      } else {
        outports += ',' + OUTPUT + *it + "\"";
      }
    }
    overall_rc = EXIT_SUCCESS;
  }
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("ACA_Vlan_Manager::get_outports <--- Exiting, overall_rc = %d\n", overall_rc);

  return overall_rc;
}

} // namespace aca_vlan_manager
