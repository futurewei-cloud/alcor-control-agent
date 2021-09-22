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

#ifndef ACA_OVS_CONTROL_H
#define ACA_OVS_CONTROL_H

#define IPTOS_PREC_INTERNETCONTROL 0xc0
#define DSCP_DEFAULT (IPTOS_PREC_INTERNETCONTROL >> 2)
#define STDOUT_FILENO   1   /* Standard output.  */

#include <openvswitch/ofp-errors.h>
//#include <openvswitch/ofp-packet.h>
#include <openvswitch/ofp-util.h>
#include <string>

// OVS monitor implementation class
namespace aca_ovs_control
{
class ACA_OVS_Control {
  public:
  static ACA_OVS_Control &get_instance();

  /* 
   * main function to call other function through command line
   * It accepts three options:
   *   -c <commands>, available commands are dump-flows, add-flow, del-flows, mod-flows, monitor, packet_out
   *   -t <target bridge>, eg. br-int, br-tun
   *   -o <additional options>, depends on the command
   */
  int control();

  /* 
   * create a monitor channel connecting to ovs.
   * Input: 
   *    const char *bridge: bridge name
   *    cnost char *opt: option for monitor.  
   * Availabe options: 
   *    <miss-len>: a number larger than 1. 
   *    resume: continually intercept packet-in message
   */
  void monitor(const char *bridge, const char *opt);
  
  /*
   * send a packet back to ovs.
   * Input:
   *    const char *bridge: bridge name
   *    cnost char *opt: option for packet_out
   * Availabe options:
   *    in_port=<in_port>: (required) in_port number on ovs, or controller, local
   *    packet=<hex string>: (required) the hex string representation of a packet 
   *    actions=<actions>: actions can be normal, control, or resumbit(,2) to a table 
   * example:
   *    ACA_OVS_Control::get_instance().packet_out("br-tun", 
   *                            "in_port=controller packet=<hex-string> actions=normal"
   */
  void packet_out(const char *bridge, const char *opt);

  /*
   * check if a flow exists in the ovsdb.
   * Input:
   *    const char *bridge: bridge name
   *    cnost char *flow: flow to be checked
   * Output:
   *    int: EXIT_SUCCESS - flow matched, EXIT_FAILURE - no any flow matched
   * example:
   *    ACA_OVS_Control::get_instance().flow_exists("br-tun", "table=10,ip,nw_dst=192.168.0.1")
   * comment: The function retrives flow without show-stats
   */
  int flow_exists(const char *bridge, const char *flow); 

  /*
   * dump matched flows.
   * Input:
   *    const char *bridge: bridge name
   *    cnost char *opt: flow to be matched. if no opt specified, the function return all flows
   * Output:
   *    int: EXIT_SUCCESS - flow matched, EXIT_FAILURE - no any flow matched
   * example:
   *    ACA_OVS_Control::get_instance().dump_flows("br-tun", "table=10,ip,nw_dst=192.168.0.1")
   * comment: 
   *    The function retrives flow with show-stats
   *    if no flow in opt, this function dump all flows
   */
  int dump_flows(const char *bridge, const char *opt); 

  /*
   * add a flow.
   * Input:
   *    const char *bridge: bridge name
   *    cnost char *opt: flow to be added.
   * Output:
   *    int: EXIT_SUCCESS or EXIT_FAILURE
   * example:
   *    ACA_OVS_Control::get_instance().add_flow("br-tun", "table=1,tcp,nw_dst=192.168.0.1,priority=1,actions=drop")
   * comment: 
   *    actions field is required in the opt.
   */
  int add_flow(const char *bridge, const char *opt);

  /*
   * modify matched flows.
   * Input:
   *    const char *bridge: bridge name
   *    cnost char *opt: flow to be matched and modified field values.
   * Output:
   *    int: EXIT_SUCCESS - flow matched, EXIT_FAILURE - no any flow matched
   * example:
   *    ACA_OVS_Control::get_instance().mod_flows("br-tun", "table=1,tcp,nw_dst=192.168.0.1,priority=1,actions=resubmit(,2)")
   * comment: 
   *    the matching flow uses --strict option. 
   *    actions field is required in the opt.
   */ 
  int mod_flows(const char *bridge, const char *opt); 

  /*
   * delete matched flows.
   * Input:
   *    const char *bridge: bridge name
   *    cnost char *opt: flow to be matched.
   * Output:
   *    int: EXIT_SUCCESS - flow matched, EXIT_FAILURE - no any flow matched
   * example:
   *    ACA_OVS_Control::get_instance().del_flows("br-tun", "table=1,tcp,nw_dst=192.168.0.1,priority=1")
   * comment: 
   *    the matching flow uses --strict option. 
   */ 
  int del_flows(const char *bridge, const char *opt);
 
  /*
   * parse a received packet.
   * Input:
   *    uint32 in_port: the port received the packet
   *    void *packet: packet data.
   * example:
   *    ACA_OVS_Control::get_instance().parse_packet(1, packet) 
   */
  void parse_packet(uint32_t in_port, void *packet);

  // compiler will flag the error when below is called.
  ACA_OVS_Control(ACA_OVS_Control const &) = delete;
  void operator=(ACA_OVS_Control const &) = delete;

  private:
  ACA_OVS_Control(){};
  ~ACA_OVS_Control(){};
};
} // namespace aca_ovs_control
#endif // #ifndef ACA_OVS_CONTROL_H
