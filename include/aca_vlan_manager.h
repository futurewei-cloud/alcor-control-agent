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
static atomic_uint current_available_group_id(1);

// Vlan Manager class
namespace aca_vlan_manager
{
struct auxgateway_entry {
  string auxGateway_id;
  uint oam_port;
  uint group_id;
};

struct vpc_table_entry {
  uint vlan_id;

  // list of ovs_ports names on this host in the same VPC to share the same internal vlan_id
  list<string> ovs_ports;

  // hashtable of output (e.g. vxlan) tunnel ports to the neighbor host communication
  // hashtable <key: outports string, value: list of neighbor port IDs>
  unordered_map<string, list<string> > outports_neighbors_table;

  auxgateway_entry auxGateway;
};

class ACA_Vlan_Manager {
  public:
  static ACA_Vlan_Manager &get_instance();

  void clear_all_data();

  uint get_or_create_vlan_id(uint tunnel_id);

  int create_ovs_port(string vpc_id, string ovs_port, uint tunnel_id, ulong &culminative_time);

  int delete_ovs_port(string vpc_id, string ovs_port, uint tunnel_id, ulong &culminative_time);

  int create_neighbor_outport(string neighbor_id, string vpc_id,
                              alcor::schema::NetworkType network_type, string remote_host_ip,
                              uint tunnel_id, ulong &culminative_time);

  int delete_neighbor_outport(string neighbor_id, uint tunnel_id,
                              string outport_name, ulong &culminative_time);

  int get_outports_unsafe(uint tunnel_id, string &outports);

  void set_auxgateway(uint tunnel_id, const string auxGateway_id, uint oam_port);

  int remove_auxgateway(uint tunnel_id);

  auxgateway_entry get_auxgateway_unsafe(uint tunnel_id);

  uint get_gateway_group_id(uint auxGateway_id);

  bool is_exist_auxgateway(const string auxGateway_id);

  bool is_exist_oam_port_rule(uint port_number);

  // compiler will flag error when below is called
  ACA_Vlan_Manager(ACA_Vlan_Manager const &) = delete;
  void operator=(ACA_Vlan_Manager const &) = delete;

  private:
  ACA_Vlan_Manager(){};
  ~ACA_Vlan_Manager(){};

  // hashtable <key: tunnel ID, value: vpc_table_entry>
  unordered_map<uint, vpc_table_entry> _vpcs_table;

  // mutex for reading and writing to _vpcs_table
  mutex _vpcs_table_mutex;

  void create_entry_unsafe(uint tunnel_id);

  int _create_oam_ofp(uint port_number);
  int _delete_oam_ofp(uint port_number);
};
} // namespace aca_vlan_manager
#endif // #ifndef ACA_VLAN_MANAGER_H
