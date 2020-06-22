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

#ifndef ACA_OVS_PROGRAMMER_H
#define ACA_OVS_PROGRAMMER_H

#include "goalstateprovisioner.grpc.pb.h"
#include <string>

// OVS programmer implementation class
namespace aca_ovs_programmer
{
class ACA_OVS_Programmer {
  public:
  static ACA_OVS_Programmer &get_instance();

  int setup_bridges();

  int configure_port(const std::string vpc_id, const std::string port_name,
                     const std::string virtual_ip, uint tunnel_id, ulong &culminative_time);

  int create_update_neighbor_port(const std::string vpc_id,
                                  alcor::schema::NetworkType network_type,
                                  const std::string remote_ip, uint tunnel_id,
                                  ulong &culminative_time);

  void execute_ovsdb_command(const std::string cmd_string,
                             ulong &culminative_time, int &overall_rc);

  void execute_openflow_command(const std::string cmd_string,
                                ulong &culminative_time, int &overall_rc);

  // compiler will flag the error when below is called.
  ACA_OVS_Programmer(ACA_OVS_Programmer const &) = delete;
  void operator=(ACA_OVS_Programmer const &) = delete;

  private:
  ACA_OVS_Programmer(){};
  ~ACA_OVS_Programmer(){};
};
} // namespace aca_ovs_programmer
#endif // #ifndef ACA_OVS_PROGRAMMER_H