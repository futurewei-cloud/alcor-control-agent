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

#include "aca_ovs_control.h"
#include "ovs_control.h"
#include "aca_log.h"
#include "aca_util.h"
#include <iostream>
#include <vector>
#include <unordered_map>

using namespace std;
using namespace ovs_control;

extern string g_ofctl_command;
extern string g_ofctl_target;
extern string g_ofctl_options;

namespace aca_ovs_control
{
ACA_OVS_Control &ACA_OVS_Control::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static ACA_OVS_Control instance;

  return instance;
}

int ACA_OVS_Control::control()
{
  int overall_rc = EXIT_SUCCESS;

  // char target[g_ofctl_target.size() + 1];
  char *target = new char[g_ofctl_target.size() + 1];
  g_ofctl_target.copy(target, g_ofctl_target.size() + 1);
  target[g_ofctl_target.size()] = '\0';

  // char options[g_ofctl_options.size() + 1];
  char *options = new char[g_ofctl_options.size() + 1];
  g_ofctl_options.copy(options, g_ofctl_options.size() + 1);
  options[g_ofctl_options.size()] = '\0';

  if (g_ofctl_command.compare("monitor") == 0) {
    monitor(target, options);
  } else if (g_ofctl_command.compare("dump-flows") == 0) {
    dump_flows(target, options);
  } else if (g_ofctl_command.compare("flow-exists") == 0) {
    flow_exists(target, options);
  } else if (g_ofctl_command.compare("add-flow") == 0) {
    add_flow(target, options);
  } else if (g_ofctl_command.compare("mod-flows") == 0) {
    mod_flows(target, options);
  } else if (g_ofctl_command.compare("del-flows") == 0) {
    del_flows(target, options);
  } else if (g_ofctl_command.compare("packet-out") == 0) {
    packet_out(target, options);
  } else {
    cout << "Usage: -c <command> -t <target> -o <options>" << endl;
    cout << "   commands: monitor, dump-flows, add-flow, mod-flows, del-flows, packet-out..."
         << endl;
    cout << "   target: swtich name, such as br-int, br-tun, ..." << endl;
    cout << "   options: " << endl;
    cout << "      moinor: \"[miss-len] [invalid-ttl] [resume] [watch:format]\"" << endl;
    cout << "      packet-out: \"in_port=<in_port> packet=<hex string> [actions=<actions>]\""
         << endl;
  }

  delete target;
  delete options;

  return overall_rc;
}

int ACA_OVS_Control::dump_flows(const char *bridge, const char *opt)
{
  return OVS_Control::get_instance().dump_flows(bridge, opt);
}

int ACA_OVS_Control::flow_exists(const char *bridge, const char *flow)
{
  return OVS_Control::get_instance().dump_flows(bridge, flow, false);
}

int ACA_OVS_Control::add_flow(const char *bridge, const char *opt)
{
  return OVS_Control::get_instance().add_flow(bridge, opt);
}

int ACA_OVS_Control::mod_flows(const char *bridge, const char *opt)
{
  bool strict = true;
  return OVS_Control::get_instance().mod_flows(bridge, opt, strict);
}

int ACA_OVS_Control::del_flows(const char *bridge, const char *opt)
{
  bool strict = true;
  return OVS_Control::get_instance().del_flows(bridge, opt, strict);
}

void ACA_OVS_Control::monitor(const char *bridge, const char *opt)
{
  OVS_Control::get_instance().monitor(bridge, opt);
}

void ACA_OVS_Control::packet_out(const char *bridge, const char *opt)
{
  OVS_Control::get_instance().packet_out(bridge, opt);
}

} // namespace aca_ovs_control
