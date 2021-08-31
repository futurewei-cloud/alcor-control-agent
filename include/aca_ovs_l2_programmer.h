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

#ifndef ACA_OVS_L2_PROGRAMMER_H
#define ACA_OVS_L2_PROGRAMMER_H

#include "goalstateprovisioner.grpc.pb.h"
#undef UNUSED
#include "of_controller.h"
#include <string>
#include <unordered_map>

#define PRIORITY_HIGH 50
#define PRIORITY_MID 25
#define PRIORITY_LOW 1

// OVS L2 programmer implementation class
namespace aca_ovs_l2_programmer
{
class ACA_OVS_L2_Programmer {
  public:
  static ACA_OVS_L2_Programmer &get_instance();

  std::vector<std::string> host_ips_vector;

  void get_local_host_ips();

  bool is_ip_on_the_same_host(const std::string hosting_port_ip);

  int setup_ovs_bridges_if_need();

  int setup_ovs_controller(const std::string ctrler_ip, const int ctrler_port);

  void set_openflow_controller(OFController* ofctrl);

  std::unordered_map<uint64_t, std::string> get_ovs_bridge_mapping();

  int create_port(const std::string vpc_id, const std::string port_name,
                  const std::string virtual_ip, const std::string virtual_mac,
                  uint tunnel_id, ulong &culminative_time);

  int delete_port(const std::string vpc_id, const std::string port_name,
                  uint tunnel_id, ulong &culminative_time);

  int create_or_update_l2_neighbor(const std::string virtual_ip, const std::string virtual_mac,
                                   const std::string remote_host_ip,
                                   uint tunnel_id, ulong &culminative_time);

  int delete_l2_neighbor(const std::string virtual_ip, const std::string virtual_mac,
                         uint tunnel_id, ulong &culminative_time);

  void execute_ovsdb_command(const std::string cmd_string,
                             ulong &culminative_time, int &overall_rc);

  void execute_openflow_command(const std::string cmd_string,
                                ulong &culminative_time, int &overall_rc);

  void execute_openflow(ulong &culminative_time,
                        const std::string bridge,
                        const std::string flow_string,
                        const std::string action = "add");

  // compiler will flag the error when below is called.
  ACA_OVS_L2_Programmer(ACA_OVS_L2_Programmer const &) = delete;
  void operator=(ACA_OVS_L2_Programmer const &) = delete;

  private:
  OFController* ofctrl;

  ACA_OVS_L2_Programmer(){};
  ~ACA_OVS_L2_Programmer(){};
};
} // namespace aca_ovs_l2_programmer
#endif // #ifndef ACA_OVS_L2_PROGRAMMER_H