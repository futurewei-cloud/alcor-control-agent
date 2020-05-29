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

#ifndef ACA_OVS_CONFIG_H
#define ACA_OVS_CONFIG_H

#include "goalstateprovisioner.grpc.pb.h"
#include <string>

// OVS Configuration implementation class
namespace aca_ovs_config
{
class ACA_OVS_Config {
  public:
  int setup_bridges();

  int port_configure(const std::string port_name, uint internal_vlan_id,
                     const std::string virtual_ip, uint tunnel_id, ulong &culminative_time);

  int port_neighbor_create_update(alcor::schema::NetworkType network_type,
                                  const std::string remote_ip, uint internal_vlan_id,
                                  uint tunnel_id, ulong &culminative_time);

  private:
  void execute_ovsdb_command(const std::string cmd_string,
                             ulong &culminative_time, int &overall_rc);

  void execute_openflow_command(const std::string cmd_string,
                                ulong &culminative_time, int &overall_rc);
};
} // namespace aca_ovs_config
#endif // #ifndef ACA_OVS_CONFIG_H