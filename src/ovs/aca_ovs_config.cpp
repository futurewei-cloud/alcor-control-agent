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

#include "aca_ovs_config.h"
// #include "aca_net_state_handler.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_net_config.h"
#include "aca_log.h"
#include "aca_util.h"
#include <chrono>
#include <errno.h>

#define MAX_OUTPORT_NAME_POSTFIX 99999999

using namespace std;

extern bool g_demo_mode;

namespace aca_ovs_config
{
static inline string aca_get_operation_name(alcor::schema::NetworkType network_type)
{
  switch (network_type) {
  case alcor::schema::NetworkType::VXLAN:
    return "vxlan";
  case alcor::schema::NetworkType::VLAN:
    return "vlan";
  case alcor::schema::NetworkType::GRE:
    return "gre";
  case alcor::schema::NetworkType::GENEVE:
    return "geneve";
  case alcor::schema::NetworkType::VXLAN_GPE:
    return "vxlan_gpe";

  default:
    return "ERROR: unknown operation type!";
  }
}

static inline string
aca_get_outport_name(alcor::schema::NetworkType network_type, string port_name)
{
  std::hash<std::string> str_hash;

  auto hash_value = str_hash(port_name) % MAX_OUTPORT_NAME_POSTFIX;

  return aca_get_operation_name(network_type) + "-" + to_string(hash_value);
}

ACA_OVS_Config &ACA_OVS_Config::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static ACA_OVS_Config instance;
  return instance;
}

int ACA_OVS_Config::setup_bridges()
{
  ACA_LOG_DEBUG("ACA_OVS_Config::setup_bridges ---> Entering\n");

  ulong not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  // TODO: confirm OVS is up and running, if not, try to start it

  // TODO: see if br-int and br-tun is already there, one option is the delete
  // them and start everything from scratch

  // create br-int and br-tun bridges
  execute_ovsdb_command("add-br br-int", not_care_culminative_time, overall_rc);

  execute_ovsdb_command("add-br br-tun", not_care_culminative_time, overall_rc);

  execute_openflow_command("del-flows br-tun", not_care_culminative_time, overall_rc);

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
  execute_openflow_command("add-flow br-tun \"table=0, priority=1,in_port=\"patch-int\" actions=resubmit(,2)\"",
                           not_care_culminative_time, overall_rc);

  execute_openflow_command("add-flow br-tun \"table=2, priority=0 actions=resubmit(,22)\"",
                           not_care_culminative_time, overall_rc);

  ACA_LOG_DEBUG("ACA_OVS_Config::setup_bridges <--- Exiting, overall_rc = %d\n", overall_rc);

  return overall_rc;
}

int ACA_OVS_Config::port_configure(const string port_name, uint internal_vlan_id,
                                   const string virtual_ip, uint tunnel_id,
                                   ulong &culminative_time)
{
  ACA_LOG_DEBUG("ACA_OVS_Config::port_configure ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;

  // TODO: validate input parameters

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
  }

  execute_openflow_command(
          "add-flow br-tun \"table=4, priority=1,tun_id=" + to_string(tunnel_id) +
                  " actions=mod_vlan_vid:" + to_string(internal_vlan_id) + ",output:\"patch-int\"\"",
          culminative_time, overall_rc);

  ACA_LOG_DEBUG("ACA_OVS_Config::port_configure <--- Exiting, overall_rc = %d\n", overall_rc);

  return overall_rc;
}

int ACA_OVS_Config::port_neighbor_create_update(const string vpc_id,
                                                alcor::schema::NetworkType network_type,
                                                const string remote_ip, uint internal_vlan_id,
                                                uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("ACA_OVS_Config::port_neighbor_create_update ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;

  // TODO: validate input parameters

  string outport_name = aca_get_outport_name(network_type, remote_ip);

  string cmd_string = "add-port br-tun " + outport_name +
                      " type=" + aca_get_operation_name(network_type) +
                      " options:df_default=true options:egress_pkt_mark=0 options:in_key=flow options:out_key=flow options:remote_ip=" +
                      remote_ip;

  execute_ovsdb_command(cmd_string, culminative_time, overall_rc);

  // TODO: look up using the vpc_id to see if there is existing tunnels in this vpc
  // if yes, add this new tunnel into the list and use the full tunnel list to construct
  // the new flow rule

  // if no, construct the brand new flow rule below
  cmd_string = "add-flow br-tun \"table=22, priority=1,dl_vlan=" + to_string(internal_vlan_id) +
               " actions=strip_vlan,load:" + to_string(tunnel_id) +
               "->NXM_NX_TUN_ID[],output:\"" + outport_name + "\"\"";

  execute_openflow_command(cmd_string, culminative_time, overall_rc);

  cmd_string = "add-flow br-tun \"table=0, priority=1,in_port=\"" +
               outport_name + " actions=resubmit(,4)";

  execute_openflow_command(cmd_string, culminative_time, overall_rc);

  ACA_LOG_DEBUG("ACA_OVS_Config::port_neighbor_create_update <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

void ACA_OVS_Config::execute_ovsdb_command(const std::string cmd_string,
                                           ulong &culminative_time, int &overall_rc)
{
  ACA_LOG_DEBUG("ACA_OVS_Config::execute_ovsdb_command ---> Entering\n");

  auto ovsdb_client_start = chrono::steady_clock::now();

  string ovsdb_cmd_string = "ovs-vsctl " + cmd_string;
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

  ACA_LOG_DEBUG("ACA_OVS_Config::execute_ovsdb_command <--- Exiting, rc = %d\n", rc);
}

void ACA_OVS_Config::execute_openflow_command(const std::string cmd_string,
                                              ulong &culminative_time, int &overall_rc)
{
  ACA_LOG_DEBUG("ACA_OVS_Config::execute_openflow_command ---> Entering\n");

  auto openflow_client_start = chrono::steady_clock::now();

  string openflow_cmd_string = "ovs-ofctl " + cmd_string;
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

  ACA_LOG_DEBUG("ACA_OVS_Config::execute_openflow_command <--- Exiting, rc = %d\n", rc);
}

} // namespace aca_ovs_config
