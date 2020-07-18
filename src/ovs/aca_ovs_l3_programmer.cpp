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
#include "aca_net_config.h"
#include "aca_vlan_manager.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_ovs_l3_programmer.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <errno.h>

using namespace std;
using namespace aca_vlan_manager;
using namespace aca_ovs_l2_programmer;

// some mutex for reading and writing it internal data
// mutex setup_ovs_bridges_mutex;

namespace aca_ovs_l3_programmer
{
ACA_OVS_L3_Programmer &ACA_OVS_L3_Programmer::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static ACA_OVS_L3_Programmer instance;
  return instance;
}

int ACA_OVS_L3_Programmer::create_router(const string host_dvr_mac, const string router_id,
                                         unordered_map<std::string, subnet_table_entry> subnet_table,
                                         ulong &culminative_time)
{
  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_router ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;

  if (host_dvr_mac.empty()) {
    throw std::invalid_argument("host_dvr_mac is empty");
  }

  // if _host_dvr_mac is not set yet, set it
  if (_host_dvr_mac.empty()) {
    _host_dvr_mac = host_dvr_mac;
  }
  // else if it is set, and same as the input
  else if (_host_dvr_mac != host_dvr_mac) {
    throw std::invalid_argument("Trying to set a different host dvr mac, old: " + _host_dvr_mac +
                                "new:" + host_dvr_mac);
  }
  // do nothing for (_host_dvr_mac == host_dvr_mac)

  if (router_id.empty()) {
    throw std::invalid_argument("router_id is empty");
  }

  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();

  if (overall_rc != EXIT_SUCCESS) {
    throw std::runtime_error("Invalid environment with br-int and br-tun");
  }

  // -----critical section starts-----
  _routers_table_mutex.lock();
  if (_routers_table.find(router_id) == _routers_table.end()) {
    _routers_table.emplace(router_id, subnet_table);
  }
  _routers_table_mutex.unlock();
  // -----critical section ends-----

  // [James action] - create a new class for this router object
  // create the internal router object, store the needed info including
  // gateway port ip and mac into the object

  // the rule will look like for each gateway port in the subnet:

  // Program Arp and ICMP responder for the gateway port 10.0.0.1:

  // ovs-ofctl add-flow br-tun "table=51,priority=50,arp,dl_vlan=1,nw_dst=10.0.0.1 actions=move:NXM_OF_ETH_SRC[]->NXM_OF_ETH_DST[],mod_dl_src:02:42:ac:11:00:01,load:0x2->NXM_OF_ARP_OP[],move:NXM_NX_ARP_SHA[]->NXM_NX_ARP_THA[],move:NXM_OF_ARP_SPA[]->NXM_OF_ARP_TPA[],load:0x0242ac110001->NXM_NX_ARP_SHA[],load:0x0a000001->NXM_OF_ARP_SPA[],in_port"

  // ovs-ofctl add-flow br-tun "table=52,priority=50,icmp,dl_vlan=1,nw_dst=10.0.0.1 actions=move:NXM_OF_ETH_SRC[]->NXM_OF_ETH_DST[],mod_dl_src:02:42:ac:11:00:01,move:NXM_OF_IP_SRC[]->NXM_OF_IP_DST[],mod_nw_src:10.0.0.1,load:0xff->NXM_NX_IP_TTL[],load:0->NXM_OF_ICMP_TYPE[],in_port"

  // Should be able to ping the gateway now:

  // ping -c1 10.0.0.1

  // execute_openflow_command( - example
  //         "add-flow br-tun \"table=4, priority=1,tun_id=" + to_string(tunnel_id) +
  //                 " actions=mod_vlan_vid:" + to_string(internal_vlan_id) + ",output:\"patch-int\"\"",
  //         culminative_time, overall_rc);

  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_router <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

// this function is to teach this compute host about the neighbor host's dvr mac
// so that we can program the rule to restore it back to the VM's gateway mac
int ACA_OVS_L3_Programmer::create_neighbor_host_dvr(const string vpc_id,
                                                    alcor::schema::NetworkType network_type,
                                                    const string host_dvr_mac,
                                                    const string gateway_mac,
                                                    uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_neighbor_host_dvr ---> Entering\n");

  int overall_rc;

  if (vpc_id.empty()) {
    throw std::invalid_argument("vpc_id is empty");
  }

  if (host_dvr_mac.empty()) {
    throw std::invalid_argument("host_dvr_mac is empty");
  }

  if (gateway_mac.empty()) {
    throw std::invalid_argument("gateway_mac is empty");
  }

  if (tunnel_id == 0) {
    throw std::invalid_argument("tunnel_id is 0");
  }

  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();

  if (overall_rc != EXIT_SUCCESS) {
    throw std::runtime_error("Invalid environment with br-int and br-tun");
  }

  // the rule will look like:

  // we can get the internal vlan id from the vlan manager using vpc_id as input

  // is the below rule good enough? need to check if other plumbing is needed to handle
  // the case that this tunnel is not known to the compute host yet

  // we have: source host DVR mac
  //

  // cmd_string = "add-flow br-int \"table=0,priority=" + PRIORITY_MID +
  //              ",dl_vlan=" + to_string(internal_vlan_id) +
  //              " actions=strip_vlan,load:" + to_string(tunnel_id) +
  //              "->NXM_NX_TUN_ID[],output:\"" + full_outport_list + "\"\"";

  // execute_openflow_command(cmd_string, culminative_time, overall_rc);

  // ovs-ofctl add-flow br-int "table=0,priority=25,dl_vlan=1,dl_src=02:42:ac:11:00:03, actions=mod_dl_src:02:42:ac:11:00:01 output:NORMAL"

  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_neighbor_host_dvr <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

int ACA_OVS_L3_Programmer::create_neighbor_l3(const string vpc_id,
                                              alcor::schema::NetworkType network_type,
                                              const string virtual_ip, const string virtual_mac,
                                              uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_neighbor_l3 ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;

  if (vpc_id.empty()) {
    throw std::invalid_argument("vpc_id is empty");
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

  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();

  if (overall_rc != EXIT_SUCCESS) {
    throw std::runtime_error("Invalid environment with br-int and br-tun");
  }

  // with DVR, a cross subnet packet will be routed to the destination subnet using Alcor DVR.
  // that means a L3 neighbor will become a L2 neighbor

  // the goal state parsing logic already took care of that
  // we can consider doing this L2 neighbor creation as an on demand rule to support scale

  // when we are ready to put the DVR rule as on demand, we should put the L2 neighbor rule as
  // on demand also

  // for the first implementation, we will go ahead and program the on demand rules DVR here
  // in the future, the programming of the on demand rule will be triggered by the first packet
  // sent to openflow controller, that's ACA

  // the rule will look like:
  // ovs-ofctl add-flow br-tun "table=0,priority=50,ip,dl_vlan=1,nw_dst=10.0.1.106,dl_dst=02:42:ac:11:00:01 actions=mod_vlan_vid:2,mod_dl_src:02:42:ac:11:00:00,mod_dl_dst=c6:41:e9:81:56:91,resubmit(,2)"

  ACA_LOG_DEBUG("ACA_OVS_L3_Programmer::create_neighbor_l3 <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

} // namespace aca_ovs_l3_programmer
