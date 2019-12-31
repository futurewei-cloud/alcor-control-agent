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

#ifndef ACA_NET_CONFIG_H
#define ACA_NET_CONFIG_H

#include <string>

using std::string;

const string IP_NETNS_PREFIX = "ip netns ";

struct veth_config {
  string veth_name;
  string ip;
  string prefix_len;
  string mac;
  string gateway_ip;
};

namespace aca_net_config
{
class Aca_Net_Config {
  public:
  // constructor and destructor purposely omitted to use the default one
  // provided by the compiler
  // Aca_Net_Config();
  // ~Aca_Net_Config();

  static Aca_Net_Config &get_instance();

  int create_namespace(string ns_name, ulong &culminative_time);

  int create_veth_pair(string veth_name, string peer_name, ulong &culminative_time);

  int delete_veth_pair(string peer_name, ulong &culminative_time);

  int setup_peer_device(string peer_name, ulong &culminative_time);

  int move_to_namespace(string veth_name, string ns_name, ulong &culminative_time);

  int setup_veth_device(string ns_name, veth_config new_veth_config, ulong &culminative_time);

  int rename_veth_device(string ns_name, string org_veth_name,
                         string new_veth_name, ulong &culminative_time);

  int add_gw(string ns_name, string gateway_ip, ulong &culminative_time);

  int execute_system_command(string cmd_string);

  int execute_system_command(string cmd_string, ulong &culminative_time);

  // compiler will flag the error when below is called.
  Aca_Net_Config(Aca_Net_Config const &) = delete;
  void operator=(Aca_Net_Config const &) = delete;

  private:
  Aca_Net_Config(){};
  ~Aca_Net_Config(){};
};

} // namespace aca_net_config
#endif