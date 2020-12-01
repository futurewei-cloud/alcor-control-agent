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
#include "aca_util.h"
#include "aca_vlan_manager.h"
#include "aca_ovs_control.h"
#include "aca_ovs_l2_programmer.h"
#include <errno.h>
#include <algorithm>

using namespace aca_ovs_control;
using namespace aca_ovs_l2_programmer;

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

  auto vpcs_table_mutex_start = chrono::steady_clock::now();
  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  // All the elements in the unordered_map container are dropped:
  // their destructors are called, and they are removed from the container,
  // leaving _vpcs_table with a size of 0.
  _vpcs_table.clear();
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----
  auto vpcs_table_mutex_end = chrono::steady_clock::now();

  g_total_vpcs_table_mutex_time +=
          cast_to_microseconds(vpcs_table_mutex_end - vpcs_table_mutex_start).count();

  // auto vpcs_table_mutex_elapse_time =
  //         cast_to_microseconds(vpcs_table_mutex_end - vpcs_table_mutex_start).count();

  // g_total_vpcs_table_mutex_time += vpcs_table_mutex_elapse_time;

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::clear_all_data <--- Exiting\n");
}

// unsafe function, needs to be called inside vpc_table_mutex lock
// this function assumes there is no existing entry for vpc_id
void ACA_Vlan_Manager::create_entry_unsafe(uint tunnel_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_entry_unsafe ---> Entering\n");

  vpc_table_entry new_table_entry;
  new_table_entry.vlan_id = current_available_vlan_id.load();
  current_available_vlan_id++;

  _vpcs_table.emplace(tunnel_id, new_table_entry);

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_entry_unsafe <--- Exiting\n");
}

uint ACA_Vlan_Manager::get_or_create_vlan_id(uint tunnel_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_or_create_vlan_id ---> Entering\n");

  auto vpcs_table_mutex_start = chrono::steady_clock::now();
  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (_vpcs_table.find(tunnel_id) == _vpcs_table.end()) {
    create_entry_unsafe(tunnel_id);
  }
  uint acquired_vlan_id = _vpcs_table[tunnel_id].vlan_id;
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----
  auto vpcs_table_mutex_end = chrono::steady_clock::now();

  g_total_vpcs_table_mutex_time +=
          cast_to_microseconds(vpcs_table_mutex_end - vpcs_table_mutex_start).count();

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_or_create_vlan_id <--- Exiting\n");

  return acquired_vlan_id;
}

int ACA_Vlan_Manager::create_ovs_port(string /*vpc_id*/, string ovs_port,
                                      uint tunnel_id, ulong &culminative_time)
{
  int overall_rc = EXIT_SUCCESS;

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_ovs_port ---> Entering\n");

  // use vpc_id to query vlan_manager to lookup an existing vpc_id entry to get its
  // internal vlan id
  int internal_vlan_id = this->get_or_create_vlan_id(tunnel_id);

  string cmd_string =
          "add-flow br-tun \"table=4, priority=1,tun_id=" + to_string(tunnel_id) +
          " actions=mod_vlan_vid:" + to_string(internal_vlan_id) + ",output:\"patch-int\"\"";

  auto vpcs_table_mutex_start = chrono::steady_clock::now();
  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (_vpcs_table.find(tunnel_id) == _vpcs_table.end()) {
    create_entry_unsafe(tunnel_id);
  }
  // first port in the VPC will add the below rule:
  // table 4 = incoming vxlan, allow incoming vxlan traffic matching tunnel_id
  // to stamp with internal vlan and deliver to br-int
  if (_vpcs_table[tunnel_id].ovs_ports.empty()) {
    ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd_string, culminative_time, overall_rc);
  }
  _vpcs_table[tunnel_id].ovs_ports.push_back(ovs_port);
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----
  auto vpcs_table_mutex_end = chrono::steady_clock::now();

  g_total_vpcs_table_mutex_time +=
          cast_to_microseconds(vpcs_table_mutex_end - vpcs_table_mutex_start).count();

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_ovs_port <--- Exiting\n");

  return overall_rc;
}

// called when a port associated with a vpc on this host is deleted
int ACA_Vlan_Manager::delete_ovs_port(string /*vpc_id*/, string ovs_port,
                                      uint tunnel_id, ulong &culminative_time)
{
  int overall_rc = EXIT_SUCCESS;

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::delete_ovs_port ---> Entering\n");

  // use vpc_id to query vlan_manager to lookup an existing vpc_id entry to get its
  // internal vlan id
  int internal_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(tunnel_id);

  auto vpcs_table_mutex_start = chrono::steady_clock::now();
  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (_vpcs_table.find(tunnel_id) == _vpcs_table.end()) {
    ACA_LOG_ERROR("tunnel_id %u not found in vpc_table\n", tunnel_id);
    overall_rc = ENOENT;
  } else {
    _vpcs_table[tunnel_id].ovs_ports.remove(ovs_port);
    // clean up the vpc_table entry if there is no port assoicated
    if (_vpcs_table[tunnel_id].ovs_ports.empty()) {
      if (_vpcs_table.erase(tunnel_id) == 1) {
        ACA_LOG_INFO("Successfuly cleaned up entry for tunnel_id %u\n", tunnel_id);

        // also delete the rule assoicated with the VPC:
        // table 4 = incoming vxlan, allow incoming vxlan traffic matching tunnel_id
        // to stamp with internal vlan and deliver to br-int
        string cmd_string =
                "del-flows br-tun \"table=4, priority=1,tun_id=" + to_string(tunnel_id) +
                " actions=mod_vlan_vid:" + to_string(internal_vlan_id) + "\" --strict";

        ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
                cmd_string, culminative_time, overall_rc);
      } else {
        ACA_LOG_ERROR("Failed to clean up entry for tunnel_id %d\n", tunnel_id);
        overall_rc = EXIT_FAILURE;
      }
    }
  }
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----
  auto vpcs_table_mutex_end = chrono::steady_clock::now();

  g_total_vpcs_table_mutex_time +=
          cast_to_microseconds(vpcs_table_mutex_end - vpcs_table_mutex_start).count();

  ACA_LOG_DEBUG("ACA_Vlan_Manager::delete_ovs_port <--- Exiting, overall_rc = %d\n", overall_rc);

  return overall_rc;
} // namespace aca_vlan_manager

int ACA_Vlan_Manager::create_neighbor_outport(string neighbor_id, string /*vpc_id*/,
                                              alcor::schema::NetworkType network_type,
                                              string remote_host_ip, uint tunnel_id,
                                              ulong &culminative_time)
{
  int overall_rc = EXIT_SUCCESS;

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_neighbor_outport ---> Entering\n");

  string outport_name = aca_get_outport_name(network_type, remote_host_ip);

  // use vpc_id to query vlan_manager to lookup an existing vpc_id entry to get its
  // internal vlan id or to create a new vpc_id entry to get a new internal vlan id
  int internal_vlan_id = this->get_or_create_vlan_id(tunnel_id);

  auto vpcs_table_mutex_start = chrono::steady_clock::now();
  // -----critical section starts-----
  _vpcs_table_mutex.lock();

  // if the vpc entry is not there, create it first
  if (_vpcs_table.find(tunnel_id) == _vpcs_table.end()) {
    create_entry_unsafe(tunnel_id);
  }

  auto current_outports_neighbors_table = _vpcs_table[tunnel_id].outports_neighbors_table;

  if (current_outports_neighbors_table.find(outport_name) ==
      current_outports_neighbors_table.end()) {
    // outport is not there yet, need to create a new entry
    std::list<string> neighbors(1, neighbor_id);
    _vpcs_table[tunnel_id].outports_neighbors_table.emplace(outport_name, neighbors);

    // since this is a new outport, configure OVS and openflow rule
    string cmd_string =
            "--may-exist add-port br-tun " + outport_name + " -- set interface " +
            outport_name + " type=" + aca_get_network_type_string(network_type) +
            " options:df_default=true options:egress_pkt_mark=0 options:in_key=flow options:out_key=flow options:remote_ip=" +
            remote_host_ip;

    ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
            cmd_string, culminative_time, overall_rc);

    // incoming from neighbor through vxlan port (based on remote IP)
    cmd_string = "add-flow br-tun \"table=0,priority=25,in_port=\"" +
                 outport_name + "\" actions=resubmit(,4)\"";

    ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd_string, culminative_time, overall_rc);

    if (overall_rc == EXIT_SUCCESS) {
      string full_outport_list;
      this->get_outports_unsafe(tunnel_id, full_outport_list);

      // match internal vlan based on VPC, output for all outports based on the same
      // tunnel ID (multicast traffic)
      cmd_string = "add-flow br-tun \"table=22,priority=1,dl_vlan=" + to_string(internal_vlan_id) +
                   " actions=strip_vlan,load:" + to_string(tunnel_id) +
                   "->NXM_NX_TUN_ID[]," + full_outport_list + "\"";

      ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
              cmd_string, culminative_time, overall_rc);
    }
  } else {
    // else outport is already there, simply insert the neighbor id into outports_neighbors_table
    _vpcs_table[tunnel_id].outports_neighbors_table[outport_name].push_back(neighbor_id);
  }

  _vpcs_table_mutex.unlock();
  // -----critical section ends-----
  auto vpcs_table_mutex_end = chrono::steady_clock::now();

  g_total_vpcs_table_mutex_time +=
          cast_to_microseconds(vpcs_table_mutex_end - vpcs_table_mutex_start).count();

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_neighbor_outport <--- Exiting\n");

  return overall_rc;
}

// called when a L2 neighbor info is deleted
int ACA_Vlan_Manager::delete_neighbor_outport(string neighbor_id, uint tunnel_id,
                                              string outport_name, ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::delete_neighbor_outport ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;
  string cmd_string;

  int internal_vlan_id = this->get_or_create_vlan_id(tunnel_id);

  auto vpcs_table_mutex_start = chrono::steady_clock::now();
  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (_vpcs_table.find(tunnel_id) == _vpcs_table.end()) {
    ACA_LOG_ERROR("tunnel_id %u not found in vpc_table\n", tunnel_id);
    overall_rc = ENOENT;
  } else {
    auto current_outports_neighbors_table = _vpcs_table[tunnel_id].outports_neighbors_table;

    if (current_outports_neighbors_table.find(outport_name) !=
        current_outports_neighbors_table.end()) {
      current_outports_neighbors_table[outport_name].remove(neighbor_id);

      // if the particular outport has no more neighbor assoicated with it
      // clean up the ovs port and openflow rules
      if (current_outports_neighbors_table[outport_name].empty()) {
        // remove the rule that match internal vlan based on VPC, output for all outports
        // based on the same tunnel ID (multicast traffic)
        cmd_string = "del-flows br-tun \"table=22,priority=1,dl_vlan=" +
                     to_string(internal_vlan_id) + "\" --strict";

        ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
                cmd_string, culminative_time, overall_rc);

        // delete the outports_neighbors_table[outport_name] entry
        if (_vpcs_table[tunnel_id].outports_neighbors_table.erase(outport_name) != 1) {
          ACA_LOG_ERROR("Failed to clean up entry for outport_name: %s\n",
                        outport_name.c_str());
          overall_rc = EXIT_FAILURE;
        }
      }

      // see if there is still *any* other vpc still using this outport
      bool outports_found = false;
      for (auto it : _vpcs_table) {
        auto current_outports_neighbors_table = it.second.outports_neighbors_table;
        if (current_outports_neighbors_table.find(outport_name) !=
            current_outports_neighbors_table.end()) {
          ACA_LOG_ERROR("outports_found! outport_name: %s\n", outport_name.c_str());
          outports_found = true;
          break;
        } else {
          continue;
        }
      }

      if (!outports_found) {
        // there is no one else using the outport for *all* vpcs,
        // go head to clean up the openflow rule then outport

        // remove the rule for incoming from neighbor through vxlan port
        cmd_string = "del-flows br-tun \"table=0,priority=1,in_port=\"" +
                     outport_name + "\"\" --strict";

        ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
                cmd_string, culminative_time, overall_rc);

        cmd_string = "del-port br-tun " + outport_name;

        ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
                cmd_string, culminative_time, overall_rc);
      }

    } else {
      ACA_LOG_ERROR("outport_name %s not found in outports_neighbors_table\n",
                    outport_name.c_str());
      overall_rc = ENOENT;
    }
  }
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----
  auto vpcs_table_mutex_end = chrono::steady_clock::now();

  g_total_vpcs_table_mutex_time +=
          cast_to_microseconds(vpcs_table_mutex_end - vpcs_table_mutex_start).count();

  ACA_LOG_DEBUG("ACA_Vlan_Manager::delete_neighbor_outport <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

// unsafe function, needs to be called under vpc_table_mutex lock
int ACA_Vlan_Manager::get_outports_unsafe(uint tunnel_id, string &outports)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_outports_unsafe ---> Entering\n");

  int overall_rc;
  static string OUTPUT = "output:\"";

  if (_vpcs_table.find(tunnel_id) == _vpcs_table.end()) {
    ACA_LOG_ERROR("tunnel_id %u not found in vpc_table\n", tunnel_id);
    overall_rc = ENOENT;
  } else {
    outports.clear();
    auto current_outports_neighbors_table = _vpcs_table[tunnel_id].outports_neighbors_table;

    for (auto it : current_outports_neighbors_table) {
      if (outports.empty()) {
        outports = OUTPUT + it.first + "\"";
      } else {
        outports += ',' + OUTPUT + it.first + "\"";
      }
    }
    overall_rc = EXIT_SUCCESS;
  }

  ACA_LOG_DEBUG("ACA_Vlan_Manager::get_outports_unsafe <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

// query oam_port with tunnel_id
uint ACA_Vlan_Manager::get_oam_server_port(uint tunnel_id)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::get_oam_server_port ---> Entering\n");

  uint port_number;

  auto vpcs_table_mutex_start = chrono::steady_clock::now();
  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (_vpcs_table.find(tunnel_id) == _vpcs_table.end()) {
    ACA_LOG_ERROR("tunnel_id %u not find in vpc_table\n", tunnel_id);
    // If the tunnel_id cannot be found, set the port number to 0.
    port_number = 0;
  } else {
    port_number = _vpcs_table[tunnel_id].oam_server_port;
  }
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----
  auto vpcs_table_mutex_end = chrono::steady_clock::now();

  g_total_vpcs_table_mutex_time +=
          cast_to_microseconds(vpcs_table_mutex_end - vpcs_table_mutex_start).count();

  ACA_LOG_DEBUG("ACA_Vlan_Manager::get_oam_server_port <--- Exiting, port_number=%u\n",
                port_number);

  return port_number;
}

// Bind oam_server_port to vpc
void ACA_Vlan_Manager::set_oam_server_port(uint tunnel_id, uint port_number)
{
  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::set_oam_server_port ---> Entering\n");

  auto vpcs_table_mutex_start = chrono::steady_clock::now();
  // -----critical section starts-----
  _vpcs_table_mutex.lock();
  if (_vpcs_table.find(tunnel_id) == _vpcs_table.end()) {
    create_entry_unsafe(tunnel_id);
  }
  _vpcs_table[tunnel_id].oam_server_port = port_number;
  _vpcs_table_mutex.unlock();
  // -----critical section ends-----
  auto vpcs_table_mutex_end = chrono::steady_clock::now();

  g_total_vpcs_table_mutex_time +=
          cast_to_microseconds(vpcs_table_mutex_end - vpcs_table_mutex_start).count();

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::set_oam_server_port <--- Exiting\n");
}

#ifdef __GNUC__
#define UNUSE(x) UNUSE_##x __attribute__((__unused__))
#else
#define UNUSE(x) UNUSE_##x
#endif

// create a neighbor port without specifying vpc_id and neighbor ID
int ACA_Vlan_Manager::create_neighbor_outport(alcor::schema::NetworkType UNUSE(network_type),
                                              string UNUSE(remote_host_ip),
                                              uint UNUSE(tunnel_id),
                                              ulong &UNUSE(culminative_time))
{
  int overall_rc = EXIT_FAILURE;
  // TBD.

  ACA_LOG_DEBUG("%s", "ACA_Vlan_Manager::create_neighbor_outport <--- Exiting\n");

  return overall_rc;
}

} // namespace aca_vlan_manager
