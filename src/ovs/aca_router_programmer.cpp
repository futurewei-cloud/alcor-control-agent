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
#include "aca_router_programmer.h"
// #include "goalstateprovisioner.grpc.pb.h"
// #include <chrono>
#include <errno.h>

using namespace std;
using namespace aca_vlan_manager;

// some mutex for reading and writing it internal data
// mutex setup_ovs_bridges_mutex;

namespace aca_router_programmer
{
ACA_Router_Programmer &ACA_Router_Programmer::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static ACA_Router_Programmer instance;
  return instance;
}

// [James action] - close down on the input param
int ACA_Router_Programmer::create_router(const string vpc_id, const string port_name,
                                         const string virtual_ip,
                                         uint tunnel_id, ulong &culminative_time)
{
  ACA_LOG_DEBUG("ACA_Router_Programmer::create_router ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;

  // check the input paremeters
  // if (vpc_id.empty()) {
  //   throw std::invalid_argument("vpc_id is empty");
  // }

  // do we need the below?
  // overall_rc = setup_ovs_bridges_if_need();

  // if (overall_rc != EXIT_SUCCESS) {
  //   throw std::runtime_error("Invalid environment with br-int and br-tun");
  // }

  // [James action] - create a new class for this router object
  // create the internal router object, store the needed info including
  // gateway port ip and mac into the object

  // the rule will look like:

  // Program Arp and ICMP responder for the gateway port 10.0.0.1:

  // ovs-ofctl add-flow br-tun "table=51,priority=50,arp,dl_vlan=1,nw_dst=10.0.0.1 actions=move:NXM_OF_ETH_SRC[]->NXM_OF_ETH_DST[],mod_dl_src:02:42:ac:11:00:01,load:0x2->NXM_OF_ARP_OP[],move:NXM_NX_ARP_SHA[]->NXM_NX_ARP_THA[],move:NXM_OF_ARP_SPA[]->NXM_OF_ARP_TPA[],load:0x0242ac110001->NXM_NX_ARP_SHA[],load:0x0a000001->NXM_OF_ARP_SPA[],in_port"

  // ovs-ofctl add-flow br-tun "table=52,priority=50,icmp,dl_vlan=1,nw_dst=10.0.0.1 actions=move:NXM_OF_ETH_SRC[]->NXM_OF_ETH_DST[],mod_dl_src:02:42:ac:11:00:01,move:NXM_OF_IP_SRC[]->NXM_OF_IP_DST[],mod_nw_src:10.0.0.1,load:0xff->NXM_NX_IP_TTL[],load:0->NXM_OF_ICMP_TYPE[],in_port"

  // Should be able to ping the gateway now:

  // ping -c1 10.0.0.1

  // execute_openflow_command( - example
  //         "add-flow br-tun \"table=4, priority=1,tun_id=" + to_string(tunnel_id) +
  //                 " actions=mod_vlan_vid:" + to_string(internal_vlan_id) + ",output:\"patch-int\"\"",
  //         culminative_time, overall_rc);

  ACA_LOG_DEBUG("ACA_Router_Programmer::create_router <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

// this function is to teach this compute host about the neighbor host's dvr mac
// so that we can problem the rule to restore it back to the VM's gateway mac
// [James action] - close down on the input param
int ACA_Router_Programmer::create_neighbor_host_dvr(const string vpc_id,
                                                    alcor::schema::NetworkType network_type,
                                                    const string remote_ip, uint tunnel_id,
                                                    ulong &culminative_time)
{
  ACA_LOG_DEBUG("ACA_Router_Programmer::create_neighbor_host_dvr ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;

  // the rule will look like:

  // ovs-ofctl add-flow br-int "table=0,priority=25,dl_vlan=1,dl_src=02:42:ac:11:00:03, actions=mod_dl_src:02:42:ac:11:00:01 output:NORMAL"

  ACA_LOG_DEBUG("ACA_Router_Programmer::create_neighbor_host_dvr <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

// [James action] - close down on the input param
int ACA_Router_Programmer::create_neighbor_L3(const string vpc_id,
                                              alcor::schema::NetworkType network_type,
                                              const string remote_ip, uint tunnel_id,
                                              ulong &culminative_time)
{
  ACA_LOG_DEBUG("ACA_Router_Programmer::create_neighbor_L3 ---> Entering\n");

  int overall_rc = EXIT_SUCCESS;

  // with DVR, a cross subnet packet will be routed to the same subnet usign Alcor DVR.
  // that means a L3 neighbor will become a L2 neighbor

  // we will need each this compute host about L2 neighbor info in case if it is not known already
  // we can consider doing this L2 neighbor creation as an on demand rule to support scale

  // when we are ready to put the DVR rule as on demand, we should put the L2 neighbor rule as
  // on demand also

  // the code here can simply call the existing code to create L2 neighbor, that is:
  // ACA_OVS_Programmer::get_instance().create_update_neighbor_port

  // for the first implementation, we will go ahead and program the on demand rules DVR here
  // in the future, the programming of the on demand rule will be triggered by the first packet
  // sent to openflow controller, that's ACA

  // the rule will look like:
  // ovs-ofctl add-flow br-tun "table=0,priority=50,ip,dl_vlan=1,nw_dst=10.0.1.106,dl_dst=02:42:ac:11:00:01 actions=mod_vlan_vid:2,mod_dl_src:02:42:ac:11:00:00,mod_dl_dst=c6:41:e9:81:56:91,resubmit(,2)"

  ACA_LOG_DEBUG("ACA_Router_Programmer::create_neighbor_L3 <--- Exiting, overall_rc = %d\n",
                overall_rc);

  return overall_rc;
}

} // namespace aca_router_programmer
