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

#ifndef ACA_OVS_L3_PROGRAMMER_H
#define ACA_OVS_L3_PROGRAMMER_H

#include "goalstateprovisioner.grpc.pb.h"
#include <unordered_map>
#include <string>

using namespace std;
using namespace alcor::schema;

// port id is stored as the key to ports table
struct neighbor_port_table_entry {
  string virtual_ip;
  string virtual_mac;
  string host_ip;
};

// routing rule id is stored as the key to routing_rules table
struct routing_rule_entry {
  string destination; // destination IP, could be 154.12.42.24/32 (host address) or 0.0.0.0/0 (network address)
  alcor::schema::DestinationType destination_type;
  string next_hop_ip;
  string next_hop_mac;
  uint priority;
};

// subnet id is stored as the key to subnet_routing_table
struct subnet_routing_table_entry {
  string vpc_id;
  alcor::schema::NetworkType network_type;
  string cidr;
  uint tunnel_id;
  string gateway_ip;
  string gateway_mac;
  // list of neighbor ports within the subnet
  // hashtable <key: neighbor ID, value: neighbor_port_table_entry>
  unordered_map<string, neighbor_port_table_entry> neighbor_ports;
  // list of routing rules for this subnet
  // hashtable <key: routing rule ID, value: routing_rule_entry>
  unordered_map<string, routing_rule_entry> routing_rules;
};

// OVS L3 programmer implementation class
namespace aca_ovs_l3_programmer
{
class ACA_OVS_L3_Programmer {
  public:
  static ACA_OVS_L3_Programmer &get_instance();

  void clear_all_data();

  int create_or_update_router(RouterConfiguration &current_RouterConfiguration,
                              GoalState &parsed_struct,
                              ulong &culminative_time_dataplane_programming_time);

  int create_or_update_router(RouterConfiguration &current_RouterConfiguration,
                              GoalStateV2 &parsed_struct,
                              ulong &culminative_time_dataplane_programming_time);

  int delete_router(RouterConfiguration &current_RouterConfiguration,
                    ulong &culminative_time_dataplane_programming_time);

  int create_or_update_l3_neighbor(const string neighbor_id, const string vpc_id,
                                   const string subnet_id, const string virtual_ip,
                                   const string virtual_mac,
                                   const string remote_host_ip, uint tunnel_id,
                                   ulong &culminative_time_dataplane_programming_time);

  int delete_l3_neighbor(const string neighbor_id, const string subnet_id,
                         const string virtual_ip, ulong &culminative_time);

  // compiler will flag the error when below is called.
  ACA_OVS_L3_Programmer(ACA_OVS_L3_Programmer const &) = delete;
  void operator=(ACA_OVS_L3_Programmer const &) = delete;

  private:
  ACA_OVS_L3_Programmer(){};
  ~ACA_OVS_L3_Programmer(){};

  string _host_dvr_mac;

  // hashtable <key: router IDs, value: hashtable <key: subnet IDs, value: subnet_routing_table_entry> >
  unordered_map<string, unordered_map<string, subnet_routing_table_entry> > _routers_table;

  // mutex for reading and writing to routers_table
  // consider using a read / write lock to improve performance
  mutex _routers_table_mutex;
};
} // namespace aca_ovs_l3_programmer
#endif // #ifndef ACA_OVS_L3_PROGRAMMER_H