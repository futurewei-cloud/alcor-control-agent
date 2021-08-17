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

  std::string execute_system_command_with_return(string cmd_string);

  // compiler will flag error when below is called
  Aca_Net_Config(Aca_Net_Config const &) = delete;
  void operator=(Aca_Net_Config const &) = delete;

  private:
  Aca_Net_Config(){};
  ~Aca_Net_Config(){};
};

} // namespace aca_net_config
#endif