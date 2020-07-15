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

#ifndef ACA_OVS_L3_PROGRAMMER_H
#define ACA_OVS_L3_PROGRAMMER_H

#include "goalstateprovisioner.grpc.pb.h"
#include <unordered_map>
#include <string>

using namespace std;

struct subnet_table_entry {
  alcor::schema::NetworkType network_type;
  string cidr;
  uint tunnel_id;
  string gateway_ip;
  string gateway_mac;
};

// OVS L3 programmer implementation class
namespace aca_ovs_l3_programmer
{
class ACA_OVS_L3_Programmer {
  public:
  static ACA_OVS_L3_Programmer &get_instance();

  // [James action] - also need to add the corresponding update and delete operations
  // at least the prototype but ideally the full implementation

  int create_router(const string host_dvr_mac,
                    unordered_map<string, subnet_table_entry> subnet_table,
                    ulong &culminative_time);

  int create_neighbor_host_dvr(const string vpc_id, alcor::schema::NetworkType network_type,
                               const string remote_ip, uint tunnel_id,
                               ulong &culminative_time);

  int create_neighbor_l3(const string vpc_id, alcor::schema::NetworkType network_type,
                         const string remote_ip, uint tunnel_id, ulong &culminative_time);

  // compiler will flag the error when below is called.
  ACA_OVS_L3_Programmer(ACA_OVS_L3_Programmer const &) = delete;
  void operator=(ACA_OVS_L3_Programmer const &) = delete;

  private:
  ACA_OVS_L3_Programmer(){};
  ~ACA_OVS_L3_Programmer(){};
};
} // namespace aca_ovs_l3_programmer
#endif // #ifndef ACA_OVS_L3_PROGRAMMER_H