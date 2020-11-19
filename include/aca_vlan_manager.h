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
#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
#include <atomic>

using namespace std;

// TODO: implement a better available internal vlan ids
// when we have port delete implemented
static atomic_uint current_available_vlan_id(1);

// Vlan Manager class
namespace aca_vlan_manager
{
struct vpc_table_entry {
  uint vlan_id;
  // list of ovs_ports names on this host in the same VPC to share the same internal vlan_id
  list<string> ovs_ports;
  
  uint32_t oam_server_port;
  // hashtable of output (e.g. vxlan) tunnel ports to the neighbor host communication
  // to neighbor port ID mapping in this VPC
  // unordered_map <outports, list of neighbor port IDs>
  unordered_map<string, list<string> > outports_neighbors_table;
};

class ACA_Vlan_Manager {
  public:
  static ACA_Vlan_Manager &get_instance();

  void clear_all_data();

  uint get_or_create_vlan_id(string vpc_id);

  int create_ovs_port(string vpc_id, string ovs_port, uint tunnel_id, ulong &culminative_time);

  int delete_ovs_port(string vpc_id, string ovs_port, uint tunnel_id, ulong &culminative_time);

  int create_neighbor_outport(string neighbor_id, string vpc_id,
                              alcor::schema::NetworkType network_type, string remote_host_ip,
                              uint tunnel_id, ulong &culminative_time);

  int delete_neighbor_outport(string neighbor_id, string vpc_id,
                              string outport_name, ulong &culminative_time);

  int get_outports_unsafe(string vpc_id, string &outports);

  int get_oam_server_port(string vpc_id, uint32_t *port);

  void set_oam_server_port(string vpc_id, uint32_t port);

  // compiler will flag error when below is called
  ACA_Vlan_Manager(ACA_Vlan_Manager const &) = delete;
  void operator=(ACA_Vlan_Manager const &) = delete;

  private:
  ACA_Vlan_Manager(){};
  ~ACA_Vlan_Manager(){};

  unordered_map<string, vpc_table_entry> _vpcs_table;

  // mutex for reading and writing to _vpcs_table
  mutex _vpcs_table_mutex;

  void create_entry_unsafe(string vpc_id);
};
} // namespace aca_vlan_manager
#endif // #ifndef ACA_VLAN_MANAGER_H
