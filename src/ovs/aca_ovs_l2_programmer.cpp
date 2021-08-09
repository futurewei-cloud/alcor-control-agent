// MIT License
// Copyright(c) 2020 Futurewei Cloud
//
//     Permission is hereby granted,
//     free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction,
//     including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons
//     to whom the Software is furnished to do so, subject to the following conditions:
//
//     The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
//     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//     WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "aca_log.h"
#include "aca_config.h"
#include "aca_util.h"
#include "aca_net_config.h"
#include "aca_vlan_manager.h"
#include "aca_ovs_l2_programmer.h"
#include <chrono>
#include <thread>
#include <errno.h>

using namespace std;
using namespace aca_vlan_manager;

// mutex for reading and writing to ovs bridges (br-int and br-tun) setups
mutex setup_ovs_bridges_mutex;

extern std::atomic_ulong g_total_execute_ovsdb_time;
extern std::atomic_ulong g_total_execute_openflow_time;
extern bool g_demo_mode;

namespace aca_ovs_l2_programmer
{

static int aca_set_port_vlan_workitem(const string port_name, uint vlan_id)
{
  ACA_LOG_DEBUG("%s", "aca_set_port_vlan_workitem ---> Entering\n");

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
  ACA_LOG_DEBUG("%s", "ACA_OVS_L2_Programmer::setup_ovs_bridges_if_need ---> Entering\n");

  ulong not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  // -----critical section starts-----
  // (exclusive access to br-int and br-tun creation status signaled by locking setup_ovs_bridges_mutex):
  setup_ovs_bridges_mutex.lock();

  // check to see if br-int and br-tun is already there
  execute_ovsdb_command("br-exists br-int", not_care_culminative_time, overall_rc);
  bool br_int_existed = (overall_rc == EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  execute_ovsdb_command("br-exists br-tun", not_care_culminative_time, overall_rc);
  bool br_tun_existed = (overall_rc == EXIT_SUCCESS);
  ACA_LOG_INFO("Environment br-int=%d and br-tun=%d\n", br_int_existed, br_tun_existed);
  overall_rc = EXIT_SUCCESS;

  if (br_int_existed && br_tun_existed) {
    // case 1: both br-int and br-tun existed
    ACA_LOG_DEBUG("%s", "Both br-int and br-tun existed: do nothing\n");
    setup_ovs_bridges_mutex.unlock();
    // -----critical section ends-----

    ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::setup_ovs_bridges_if_need <--- Exiting, overall_rc = %d\n",
                  overall_rc);

    return overall_rc;
  }

  if (!br_int_existed && !br_tun_existed) {
    // case 2: both br-int and br-tun not existed
    ACA_LOG_DEBUG("%s", "Both br-int and br-tun not existed: create them\n");
    ACA_LOG_INFO("Environment br-int=%d and br-tun=%d\n", br_int_existed, br_tun_existed);

    execute_ovsdb_command("add-br br-int", not_care_culminative_time, overall_rc);

    execute_ovsdb_command("add-br br-tun", not_care_culminative_time, overall_rc);

    // create and connect the patch ports between br-int and br-tun
    execute_ovsdb_command("-- add-port br-int patch-tun "
                          "-- set interface patch-tun type=patch options:peer=patch-int "
                          "-- add-port br-tun patch-int "
                          "-- set interface patch-int type=patch options:peer=patch-tun",
                          not_care_culminative_time, overall_rc);

    // adding default flows
    // details at: https://github.com/futurewei-cloud/alcor-control-agent/wiki/Openflow-Tables-Explain

    execute_openflow_command("add-flow br-tun \"table=0,priority=50,arp,arp_op=1, actions=CONTROLLER\"",
                             not_care_culminative_time, overall_rc);

    execute_openflow_command("add-flow br-tun \"table=0,priority=1,in_port=\"patch-int\" actions=resubmit(,2)\"",
                             not_care_culminative_time, overall_rc);

    execute_openflow_command("add-flow br-tun \"table=2,priority=1,dl_dst=00:00:00:00:00:00/01:00:00:00:00:00 actions=resubmit(,20)\"",
                             not_care_culminative_time, overall_rc);

    execute_openflow_command("add-flow br-tun \"table=2,priority=1,dl_dst=01:00:00:00:00:00/01:00:00:00:00:00 actions=resubmit(,22)\"",
                             not_care_culminative_time, overall_rc);

    execute_openflow_command("add-flow br-tun \"table=20,priority=1 actions=CONTROLLER\"",
                             not_care_culminative_time, overall_rc);

    execute_openflow_command("add-flow br-tun \"table=2,priority=25,icmp,icmp_type=8,in_port=\"patch-int\" actions=resubmit(,52)\"",
                             not_care_culminative_time, overall_rc);

    execute_openflow_command("add-flow br-tun \"table=52,priority=1 actions=resubmit(,20)\"",
                             not_care_culminative_time, overall_rc);

    execute_ovsdb_command(
            string("--may-exist add-port br-tun vxlan-generic -- set interface vxlan-generic ofport_request=") +
                    VXLAN_GENERIC_OUTPORT_NUMBER +
                    " type=vxlan options:df_default=true options:egress_pkt_mark=0 options:in_key=flow options:out_key=flow options:remote_ip=flow",
            not_care_culminative_time, overall_rc);

    execute_openflow_command("add-flow br-tun \"table=0,priority=25,in_port=\"vxlan-generic\" actions=resubmit(,4)\"",
                             not_care_culminative_time, overall_rc);
    setup_ovs_bridges_mutex.unlock();
    // -----critical section ends-----

    ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::setup_ovs_bridges_if_need <--- Exiting, overall_rc = %d\n",
                  overall_rc);

    return overall_rc;
  }

  // case 3: only one of the br-int or br-tun is there,
  // Invalid environment so return an error
  if (br_int_existed) {
    ACA_LOG_DEBUG("You have br-int, but you don't have br-tun, please add br-tun by executing command 'ovs-vsctl add-br br-tun', or delete the existing br-int by executing command 'ovs-vsctl del-br br-int' and try again.\n",
                  br_tun_existed);
  }
  if (br_tun_existed) {
    ACA_LOG_DEBUG("You have br-tun, but you don't have br-int, please add br-int by executing command 'ovs-vsctl add-br br-int', or delete the existing br-tun by executing command 'ovs-vsctl del-br br-tun' and try again.\n",
                  br_tun_existed);
  }
  ACA_LOG_CRIT("Invalid environment br-int=%d and br-tun=%d, cannot proceed\n",
               br_int_existed, br_tun_existed);
  overall_rc = EXIT_FAILURE;

  setup_ovs_bridges_mutex.unlock();
  // -----critical section ends-----

  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::setup_ovs_bridges_if_need <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

int ACA_OVS_L2_Programmer::create_port(const string vpc_id, const string port_name,
                                       const string virtual_ip, const string virtual_mac,
                                       uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_OVS_L2_Programmer::create_port ---> Entering\n");

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

  if (virtual_mac.empty()) {
    throw std::invalid_argument("virtual_mac is empty");
  }

  if (tunnel_id == 0) {
    throw std::invalid_argument("tunnel_id is 0");
  }

  // TODO: if ovs_port already exist in vlan manager, that means it is a duplicated
  // port CREATE operation, we have the following options to handle it:
  // 1. Reject call with error message
  // 2. No ops with warning message
  // 3. Do it regardless => if there is anything wrong, controller's fault :-)

  // use vpc_id to query vlan_manager to lookup an existing vpc_id entry to get its
  // internal vlan id or to create a new vpc_id entry to get a new internal vlan id
  int internal_vlan_id = ACA_Vlan_Manager::get_instance().get_or_create_vlan_id(tunnel_id);

  ACA_Vlan_Manager::get_instance().create_ovs_port(vpc_id, port_name, tunnel_id, culminative_time);

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

    cmd_string = "ip link set dev " + port_name + " address " + virtual_mac;
    command_rc = aca_net_config::Aca_Net_Config::get_instance().execute_system_command(
            cmd_string, culminative_time);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;

    cmd_string = "ip link set " + port_name + " up";
    command_rc = aca_net_config::Aca_Net_Config::get_instance().execute_system_command(
            cmd_string, culminative_time);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;
  }

  // Disabling this code for testing, as test controller already sets the vlan,
  // and the port names generated by TC's ovs-docker is different from the port_name used here.
  else {
    // non-demo mode is for nova integration, where the vif and ovs port has been
    // created by nova compute agent running on the compute host

    // just need to set the vlan tag on the ovs port, the ovs port may be not created by nova yet
    string cmd_string = "set port " + port_name + " tag=" + to_string(internal_vlan_id);

    execute_ovsdb_command(cmd_string, culminative_time, overall_rc);

    // if the ovs port is not there to set to vlan, we will return PENDING as the result
    // and spin up the new thread to keep trying that in the backgroud
    if (overall_rc != EXIT_SUCCESS) {
      overall_rc = EINPROGRESS;

      // start a new background thread work set the port vlan
      std::thread t(aca_set_port_vlan_workitem, port_name, internal_vlan_id);

      // purposely not wait for this aca_set_port_vlan_workitem
      if (t.joinable()) {
        t.detach();
      }
    }
  }

  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::create_port <--- Exiting, overall_rc = %d\n", overall_rc);

  return overall_rc;
}

int ACA_OVS_L2_Programmer::delete_port(const string vpc_id, const string port_name,
                                       uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_OVS_L2_Programmer::delete_port ---> Entering\n");

  if (vpc_id.empty()) {
    throw std::invalid_argument("vpc_id is empty");
  }

  if (port_name.empty()) {
    throw std::invalid_argument("port_name is empty");
  }

  if (tunnel_id == 0) {
    throw std::invalid_argument("tunnel_id is 0");
  }

  int overall_rc = ACA_Vlan_Manager::get_instance().delete_ovs_port(
          vpc_id, port_name, tunnel_id, culminative_time);

  if (g_demo_mode) {
    string cmd_string = "del-port br-int " + port_name;

    execute_ovsdb_command(cmd_string, culminative_time, overall_rc);
  }

  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::delete_port <--- Exiting, overall_rc = %d\n", overall_rc);

  return overall_rc;
}

int ACA_OVS_L2_Programmer::create_or_update_l2_neighbor(const string virtual_ip,
                                                        const string virtual_mac,
                                                        const string remote_host_ip,
                                                        uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_OVS_L2_Programmer::create_or_update_l2_neighbor ---> Entering\n");

  if (virtual_ip.empty()) {
    throw std::invalid_argument("virtual_ip is empty");
  }

  if (virtual_mac.empty()) {
    throw std::invalid_argument("virtual_mac is empty");
  }

  if (remote_host_ip.empty()) {
    throw std::invalid_argument("remote_host_ip is empty");
  }

  if (tunnel_id == 0) {
    throw std::invalid_argument("tunnel_id is 0");
  }

  int overall_rc = ACA_Vlan_Manager::get_instance().create_l2_neighbor(
          virtual_ip, virtual_mac, remote_host_ip, tunnel_id, culminative_time);

  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::create_or_update_l2_neighbor <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

int ACA_OVS_L2_Programmer::delete_l2_neighbor(const string virtual_ip, const string virtual_mac,
                                              uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("%s", "ACA_OVS_L2_Programmer::delete_l2_neighbor ---> Entering\n");

  if (virtual_ip.empty()) {
    throw std::invalid_argument("virtual_ip is empty");
  }

  if (virtual_mac.empty()) {
    throw std::invalid_argument("virtual_mac is empty");
  }

  if (tunnel_id == 0) {
    throw std::invalid_argument("tunnel_id is 0");
  }

  int overall_rc = ACA_Vlan_Manager::get_instance().delete_l2_neighbor(
          virtual_ip, virtual_mac, tunnel_id, culminative_time);

  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::delete_l2_neighbor <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

void ACA_OVS_L2_Programmer::execute_ovsdb_command(const std::string cmd_string,
                                                  ulong &culminative_time, int &overall_rc)
{
  ACA_LOG_DEBUG("%s", "ACA_OVS_L2_Programmer::execute_ovsdb_command ---> Entering\n");

  auto ovsdb_client_start = chrono::steady_clock::now();

  string ovsdb_cmd_string = "ovs-vsctl " + cmd_string;
  int rc = aca_net_config::Aca_Net_Config::get_instance().execute_system_command(ovsdb_cmd_string);

  if (rc != EXIT_SUCCESS) {
    overall_rc = rc;
  }

  auto ovsdb_client_end = chrono::steady_clock::now();

  auto ovsdb_client_time_total_time =
          cast_to_microseconds(ovsdb_client_end - ovsdb_client_start).count();

  culminative_time += ovsdb_client_time_total_time;

  g_total_execute_ovsdb_time += ovsdb_client_time_total_time;

  ACA_LOG_INFO("Elapsed time for ovsdb client call took: %ld microseconds or %ld milliseconds. rc: %d, cmd: [%s]\n",
               ovsdb_client_time_total_time, us_to_ms(ovsdb_client_time_total_time),
               rc, ovsdb_cmd_string.c_str());

  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::execute_ovsdb_command <--- Exiting, rc = %d\n", rc);
}

void ACA_OVS_L2_Programmer::execute_openflow_command(const std::string cmd_string,
                                                     ulong &culminative_time, int &overall_rc)
{
  ACA_LOG_DEBUG("%s", "ACA_OVS_L2_Programmer::execute_openflow_command ---> Entering\n");

  auto openflow_client_start = chrono::steady_clock::now();

  string openflow_cmd_string = "ovs-ofctl " + cmd_string;
  int rc = aca_net_config::Aca_Net_Config::get_instance().execute_system_command(openflow_cmd_string);

  if (rc != EXIT_SUCCESS) {
    overall_rc = rc;
  }

  auto openflow_client_end = chrono::steady_clock::now();

  auto openflow_client_time_total_time =
          cast_to_microseconds(openflow_client_end - openflow_client_start).count();

  culminative_time += openflow_client_time_total_time;

  g_total_execute_openflow_time += openflow_client_time_total_time;

  ACA_LOG_INFO("Elapsed time for openflow client call took: %ld microseconds or %ld milliseconds. rc: %d\n",
               openflow_client_time_total_time,
               us_to_ms(openflow_client_time_total_time), rc);

  ACA_LOG_DEBUG("ACA_OVS_L2_Programmer::execute_openflow_command <--- Exiting, rc = %d\n", rc);
}

} // namespace aca_ovs_l2_programmer
