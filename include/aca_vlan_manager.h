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
  // list of output (e.g. vxlan) tunnel ports to the neighbor host communication in the same VPC
  list<string> outports;
};

class ACA_Vlan_Manager {
  public:
  static ACA_Vlan_Manager &get_instance();

  uint get_or_create_vlan_id(string vpc_id);

  void add_ovs_port(string vpc_id, string ovs_port);

  int remove_ovs_port(string vpc_id, string ovs_port);

  void add_outport(string vpc_id, string outport);

  int remove_outport(string vpc_id, string outport);

  int get_outports(string vpc_id, string &outports);

  // compiler will flag error when below is called
  ACA_Vlan_Manager(ACA_Vlan_Manager const &) = delete;
  void operator=(ACA_Vlan_Manager const &) = delete;

  private:
  ACA_Vlan_Manager(){};
  ~ACA_Vlan_Manager(){};

  unordered_map<string, vpc_table_entry> vpc_table;

  // mutex for reading and writing to vpc_table
  mutex vpc_table_mutex;

  bool check_entry_existed_unsafe(string vpc_id);

  void create_entry_unsafe(string vpc_id);
};
} // namespace aca_vlan_manager
#endif // #ifndef ACA_VLAN_MANAGER_H
