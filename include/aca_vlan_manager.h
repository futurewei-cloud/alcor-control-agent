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
  // CTSL::HashMap <key: ovs_port name, value: int* (not used)>
  CTSL::HashMap<string, int *> ovs_ports;

  string zeta_gateway_id;
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

  void set_zeta_gateway(uint tunnel_id, const string auxGateway_id);

  int remove_zeta_gateway(uint tunnel_id);

  string get_zeta_gateway_id(uint tunnel_id);

  bool is_exist_zeta_gateway(const string auxGateway_id);

  uint get_tunnelId_by_vlanId(uint vlan_id);

  // compiler will flag error when below is called
  ACA_Vlan_Manager(ACA_Vlan_Manager const &) = delete;
  void operator=(ACA_Vlan_Manager const &) = delete;

  private:
  ACA_Vlan_Manager(){};
  ~ACA_Vlan_Manager(){};

  // CTSL::HashMap <key: tunnel ID, value: vpc_table_entry>
  CTSL::HashMap<uint, vpc_table_entry *> _vpcs_table;
  // mutex for reading and writing to _vpcs_table
  mutex _vpcs_table_mutex;
  void create_entry(uint tunnel_id);
};
} // namespace aca_vlan_manager
#endif // #ifndef ACA_VLAN_MANAGER_H
