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

#ifndef ACA_VLAN_MANAGER_H
#define ACA_VLAN_MANAGER_H

#include "goalstateprovisioner.grpc.pb.h"
#include "hashmap/HashMap.h"
#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
#include <atomic>

using namespace std;

// TODO: implement a better available internal vlan ids
static atomic_uint current_available_vlan_id(1);

// Vlan Manager class
namespace aca_vlan_manager
{
struct vpc_table_entry {
  uint vlan_id;

  // list of ovs_ports names on this host in the same VPC to share the same internal vlan_id
  list<string> ovs_ports;

  // mutex to protect ovs_ports
  mutable std::shared_timed_mutex ovs_ports_mutex;

  // hashtable of output (e.g. vxlan) tunnel ports to the neighbor host communication
  // hashtable <key: outports string, value: list of neighbor port IDs>
  unordered_map<string, list<string> > outports_neighbors_table;

  // mutex to protect outports_neighbors_table
  mutable std::shared_timed_mutex outports_neighbors_table_mutex;

  string auxGateway_id;
};

class ACA_Vlan_Manager {
  public:
  static ACA_Vlan_Manager &get_instance();

  void clear_all_data();

  uint get_or_create_vlan_id(uint tunnel_id);

  int create_ovs_port(string vpc_id, string ovs_port, uint tunnel_id, ulong &culminative_time);

  int delete_ovs_port(string vpc_id, string ovs_port, uint tunnel_id, ulong &culminative_time);

  int create_l2_neighbor(string virtual_ip, string virtual_mac, string remote_host_ip,
                         uint tunnel_id, ulong &culminative_time);

  int delete_l2_neighbor(string virtual_ip, string virtual_mac, uint tunnel_id,
                         ulong &culminative_time);

  // the below three "outport" functions are deprecated and not used
  // keeping them below to avoid merge conflict with other ACA change in PR
  int create_neighbor_outport(string neighbor_id, string vpc_id,
                              alcor::schema::NetworkType network_type, string remote_host_ip,
                              uint tunnel_id, ulong &culminative_time);

  // create a neighbor port without specifying vpc_id and neighbor ID
  int create_neighbor_outport(alcor::schema::NetworkType network_type, string remote_host_ip,
                              uint tunnel_id, ulong &culminative_time);

  int delete_neighbor_outport(string neighbor_id, uint tunnel_id,
                              string outport_name, ulong &culminative_time);

  int get_outports_unsafe(uint tunnel_id, string &outports);

  void set_aux_gateway(uint tunnel_id, const string auxGateway_id);

  string get_aux_gateway_id(uint tunnel_id);

  bool is_exist_aux_gateway(const string auxGateway_id);

  // compiler will flag error when below is called
  ACA_Vlan_Manager(ACA_Vlan_Manager const &) = delete;
  void operator=(ACA_Vlan_Manager const &) = delete;

  private:
  ACA_Vlan_Manager(){};
  ~ACA_Vlan_Manager(){};

  // hashtable <key: tunnel ID, value: vpc_table_entry>
  CTSL::HashMap<uint, vpc_table_entry *> _vpcs_table;

  // mutex for reading and writing to _vpcs_table
  // mutex _vpcs_table_mutex;

  void create_entry(uint tunnel_id);
};
} // namespace aca_vlan_manager
#endif // #ifndef ACA_VLAN_MANAGER_H
