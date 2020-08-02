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
#include "aca_config.h"
#include "aca_util.h"
#include "aca_net_config.h"
#include "aca_vlan_manager.h"
#include "aca_ovs_l2_programmer.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <chrono>
#include <thread>
#include <errno.h>

using namespace std;
using namespace aca_vlan_manager;

// mutex for reading and writing to ovs bridges (br-int and br-tun) setups
mutex setup_ovs_bridges_mutex;

extern bool g_demo_mode;

namespace aca_ovs_l2_programmer
{
static int aca_set_port_vlan_workitem(const string port_name, uint vlan_id)
{
  ACA_LOG_DEBUG("aca_set_port_vlan_workitem ---> Entering\n");

  ulong not_care_culminative_time = 0;
  int overall_rc;

  if (port_name.empty()) {
    throw std::invalid_argument("port_name is empty");
  }

  if (vlan_id == 0 || vlan_id >= 4095) {
    throw std::invalid_argument("vlan_id is invalid: " + to_string(vlan_id));
  }

  uint retry_times = 0;
  string cmd_string = "set port " + port_name + " tag=" + to_string(vlan_id);

  do {
    std::this_thread::sleep_for(chrono::milliseconds(PORT_SCAN_SLEEP_INTERVAL));

    overall_rc = EXIT_SUCCESS;
    ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
            cmd_string, not_care_culminative_time, overall_rc);

    if (overall_rc == EXIT_SUCCESS)
      break;
  } while (++retry_times < MAX_PORT_SCAN_RETRY);

  if (overall_rc != EXIT_SUCCESS) {
    ACA_LOG_ERROR("Not able to set the vlan tag %d for port %s even after waiting\n",
                  vlan_id, port_name.c_str());
  }

  // TODO: after this workitem thread is done, it should provide the updated success/fail result back to DPM

  ACA_LOG_DEBUG("aca_set_port_vlan_workitem <--- Exiting, overall_rc = %d\n", overall_rc);

  return overall_rc;
}

ACA_OVS_L2_Programmer &ACA_OVS_L2_Programmer::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static ACA_OVS_L2_Programmer instance;
  return instance;
}

int ACA_OVS_L2_Programmer::setup_ovs_bridges_if_need()
{
  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::setup_ovs_bridges_if_need ---> Entering\n");

  ulong not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  // -----critical section starts-----
  // (exclusive access to br-int and br-tun creation status signaled by locking setup_ovs_bridges_mutex):
  setup_ovs_bridges_mutex.lock();

  // check to see if br-int and br-tun is already there
  execute_ovsdb_command("br-exists br-int", not_care_culminative_time, overall_rc);
  bool br_int_existed = (overall_rc == EXIT_SUCCESS);

  execute_ovsdb_command("br-exists br-tun", not_care_culminative_time, overall_rc);
  bool br_tun_existed = (overall_rc == EXIT_SUCCESS);

  overall_rc = EXIT_SUCCESS;

  if (br_int_existed && br_tun_existed) {
    // case 1: both br-int and br-tun exist
    ACA_LOG_DEBUG("Both br-int and br-tun existed: do nothing\n");
  } else if (!br_int_existed && !br_int_existed) {
    // case 2: both br-int and br-tun not there
    ACA_LOG_DEBUG("Both br-int and br-tun not existed: create them\n");

    execute_ovsdb_command("add-br br-int", not_care_culminative_time, overall_rc);

    execute_ovsdb_command("add-br br-tun", not_care_culminative_time, overall_rc);

    // create and connect the patch ports between br-int and br-tun
    execute_ovsdb_command("add-port br-int patch-tun", not_care_culminative_time, overall_rc);

    execute_ovsdb_command("set interface patch-tun type=patch",
                          not_care_culminative_time, overall_rc);

    execute_ovsdb_command("set interface patch-tun options:peer=patch-int",
                          not_care_culminative_time, overall_rc);

    execute_ovsdb_command("add-port br-tun patch-int", not_care_culminative_time, overall_rc);

    execute_ovsdb_command("set interface patch-int type=patch",
                          not_care_culminative_time, overall_rc);

    execute_ovsdb_command("set interface patch-int options:peer=patch-tun",
                          not_care_culminative_time, overall_rc);

    // adding default flows
    execute_openflow_command("add-flow br-tun \"table=0,priority=1,in_port=\"patch-int\" actions=resubmit(,2)\"",
                             not_care_culminative_time, overall_rc);

    execute_openflow_command("add-flow br-tun \"table=2,priority=1 actions=resubmit(,22)\"",
                             not_care_culminative_time, overall_rc);

    execute_openflow_command("add-flow br-tun \"table=0,priority=25,arp,arp_op=1,in_port=\"patch-int\" actions=resubmit(,51)\"",
                             not_care_culminative_time, overall_rc);

    execute_openflow_command("add-flow br-tun \"table=51,priority=1 actions=resubmit(,22)\"",
                             not_care_culminative_time, overall_rc);

    execute_openflow_command("add-flow br-tun \"table=0,priority=25,icmp,icmp_type=8,in_port=\"patch-int\" actions=resubmit(,52)\"",
                             not_care_culminative_time, overall_rc);

    execute_openflow_command("add-flow br-tun \"table=52,priority=1 actions=resubmit(,22)\"",
                             not_care_culminative_time, overall_rc);
  } else {
    // case 3: only one of the br-int or br-tun is there,
    // Invalid environment so return an error
    ACA_LOG_CRIT("Invalid environment br-int=%d and br-tun=%d, cannot proceed\n",
                 br_int_existed, br_tun_existed);
    overall_rc = EXIT_FAILURE;
  }

  setup_ovs_bridges_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::setup_ovs_bridges_if_need <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

int ACA_OVS_L2_Programmer::configure_port(const string vpc_id, const string port_name,
                                          const string virtual_ip,
                                          uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::port_configure ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;

  if (vpc_id.empty()) {
    throw std::invalid_argument("vpc_id is empty");
  }

  if (port_name.empty()) {
    throw std::invalid_argument("port_name is empty");
  }

  if (virtual_ip.empty()) {
    throw std::invalid_argument("virtual_ip is empty");
  }

  if (tunnel_id == 0) {
    throw std::invalid_argument("tunnel_id is 0");
  }

  overall_rc = setup_ovs_bridges_if_need();

  if (overall_rc != EXIT_SUCCESS) {
    throw std::runtime_error("Invalid environment with br-int and br-tun");
  }

  // TODO: if ovs_port already exist in vlan manager, that means it is a duplicated
  // port CREATE operation, we have the following options to handle it:
  // 1. Reject call with error message
  // 2. No ops with warning message
  // 3. Do it regardless => if there is anything wrong, controller's fault :-)

  // use vpc_id to query vlan_manager to lookup an existing vpc_id entry to get its
  // internal vlan id or to create a new vpc_id entry to get a new internal vlan id
  int internal_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(vpc_id);

  ACA_Vlan_Manager::get_instance().add_ovs_port(vpc_id, port_name);

  execute_openflow_command(
          "add-flow br-tun \"table=4, priority=1,tun_id=" + to_string(tunnel_id) +
                  " actions=mod_vlan_vid:" + to_string(internal_vlan_id) + ",output:\"patch-int\"\"",
          culminative_time, overall_rc);

  if (g_demo_mode) {
    string cmd_string = "add-port br-int " + port_name +
                        " tag=" + to_string(internal_vlan_id) +
                        " -- set Interface " + port_name + " type=internal";

    execute_ovsdb_command(cmd_string, culminative_time, overall_rc);

    cmd_string = "ip addr add " + virtual_ip + " dev " + port_name;
    int command_rc = aca_net_config::Aca_Net_Config::get_instance().execute_system_command(
            cmd_string, culminative_time);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;

    cmd_string = "ip link set " + port_name + " up";
    command_rc = aca_net_config::Aca_Net_Config::get_instance().execute_system_command(
            cmd_string, culminative_time);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;
  } else {
    // non-demo mode is for nova integration, where the vif and ovs port has been
    // created by nova compute agent running on the compute host

    // just need to set the vlan tag on the ovs port, the ovs port may be not created by nova yet
    string cmd_string = "set port " + port_name + " tag=" + to_string(internal_vlan_id);

    execute_ovsdb_command(cmd_string, culminative_time, overall_rc);

    // if the ovs port is not there to set to vlan, we will return PENDING as the result
    // and spin up the new thread to keep trying that in the backgroud
    if (overall_rc != EXIT_SUCCESS) {
      overall_rc = EINPROGRESS;

<<<<<<< HEAD:src/ovs/aca_ovs_l2_programmer.cpp
      // start a new background thread work set the port vlan
      std::thread t(aca_set_port_vlan_workitem, port_name, internal_vlan_id);

      // purposely not wait for this aca_set_port_vlan_workitem
      if (t.joinable()) {
        t.detach();
      }
    }
  }

=======
>>>>>>> 9f0cc4b3ea8c9a860503002a5a28e17f25578c8c:src/ovs/aca_ovs_l2_programmer.cpp
  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::port_configure <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

int ACA_OVS_L2_Programmer::create_update_neighbor_port(const string vpc_id,
                                                       alcor::schema::NetworkType network_type,
                                                       const string remote_host_ip,
                                                       uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::port_neighbor_create_update ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;

  if (vpc_id.empty()) {
    throw std::invalid_argument("vpc_id is empty");
  }

  if (remote_host_ip.empty()) {
    throw std::invalid_argument("remote_host_ip is empty");
  }

  if (tunnel_id == 0) {
    throw std::invalid_argument("tunnel_id is 0");
  }

  overall_rc = setup_ovs_bridges_if_need();

  if (overall_rc != EXIT_SUCCESS) {
    throw std::runtime_error("Invalid environment with br-int and br-tun");
  }

  string outport_name = aca_get_outport_name(network_type, remote_host_ip);

  string cmd_string =
          "--may-exist add-port br-tun " + outport_name + " -- set interface " +
          outport_name + " type=" + aca_get_network_type_string(network_type) +
          " options:df_default=true options:egress_pkt_mark=0 options:in_key=flow options:out_key=flow options:remote_ip=" +
          remote_host_ip;

  execute_ovsdb_command(cmd_string, culminative_time, overall_rc);

  // use vpc_id to query vlan_manager to lookup an existing vpc_id entry to get its
  // internal vlan id or to create a new vpc_id entry to get a new internal vlan id
  int internal_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(vpc_id);

  ACA_Vlan_Manager::get_instance().add_outport(vpc_id, outport_name);

  string full_outport_list;
  overall_rc = ACA_Vlan_Manager::get_instance().get_outports(vpc_id, full_outport_list);

  if (overall_rc != EXIT_SUCCESS) {
    throw std::runtime_error("vpc_id entry not find in vpc_table");
  }

  cmd_string = "add-flow br-tun \"table=22,priority=1,dl_vlan=" + to_string(internal_vlan_id) +
               " actions=strip_vlan,load:" + to_string(tunnel_id) +
               "->NXM_NX_TUN_ID[],output:\"" + full_outport_list + "\"\"";

  execute_openflow_command(cmd_string, culminative_time, overall_rc);

  cmd_string = "add-flow br-tun \"table=0,priority=1,in_port=\"" +
               outport_name + "\" actions=resubmit(,4)\"";

  execute_openflow_command(cmd_string, culminative_time, overall_rc);

  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::port_neighbor_create_update <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

void ACA_OVS_L2_Programmer::execute_ovsdb_command(const std::string cmd_string,
                                                  ulong &culminative_time, int &overall_rc)
{
  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::execute_ovsdb_command ---> Entering\n");

  auto ovsdb_client_start = chrono::steady_clock::now();

  string ovsdb_cmd_string = "/usr/bin/ovs-vsctl " + cmd_string;
  int rc = aca_net_config::Aca_Net_Config::get_instance().execute_system_command(ovsdb_cmd_string);

  if (rc != EXIT_SUCCESS) {
    overall_rc = rc;
  }

  auto ovsdb_client_end = chrono::steady_clock::now();

  auto ovsdb_client_time_total_time =
          cast_to_nanoseconds(ovsdb_client_end - ovsdb_client_start).count();

  culminative_time += ovsdb_client_time_total_time;

  ACA_LOG_INFO("Elapsed time for ovsdb client call took: %ld nanoseconds or %ld milliseconds. rc: %d\n",
               ovsdb_client_time_total_time, ovsdb_client_time_total_time / 1000000, rc);

  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::execute_ovsdb_command <--- Exiting, rc = %d\n", rc);
}

void ACA_OVS_L2_Programmer::execute_openflow_command(const std::string cmd_string,
                                                     ulong &culminative_time, int &overall_rc)
{
  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::execute_openflow_command ---> Entering\n");

  auto openflow_client_start = chrono::steady_clock::now();

  string openflow_cmd_string = "/usr/bin/ovs-ofctl " + cmd_string;
  int rc = aca_net_config::Aca_Net_Config::get_instance().execute_system_command(openflow_cmd_string);

  if (rc != EXIT_SUCCESS) {
    overall_rc = rc;
  }

  auto openflow_client_end = chrono::steady_clock::now();

  auto openflow_client_time_total_time =
          cast_to_nanoseconds(openflow_client_end - openflow_client_start).count();

  culminative_time += openflow_client_time_total_time;

  ACA_LOG_INFO("Elapsed time for openflow client call took: %ld nanoseconds or %ld milliseconds. rc: %d\n",
               openflow_client_time_total_time,
               openflow_client_time_total_time / 1000000, rc);

  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::execute_openflow_command <--- Exiting, rc = %d\n", rc);
}

} // namespace aca_ovs_l2_programmer
