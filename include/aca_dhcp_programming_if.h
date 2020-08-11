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

#ifndef ACA_DHCP_PROGRAMMING_IF_H
#define ACA_DHCP_PROGRAMMING_IF_H

#include <string>

using namespace std;

namespace aca_dhcp_programming_if
{
struct dhcp_config {
  string mac_address;
  string ipv4_address;
  string ipv6_address;
  string port_host_name;
};

// DHCP programming interface class
class ACA_Dhcp_Programming_Interface {
  public:
  // pure virtual functions providing interface framework.
  virtual int add_dhcp_entry(dhcp_config *dhcp_config_in) = 0;

  virtual int update_dhcp_entry(dhcp_config *dhcp_config_in) = 0;

  virtual int delete_dhcp_entry(dhcp_config *dhcp_config_in) = 0;

  virtual ~ACA_Dhcp_Programming_Interface() = 0;
};
} // namespace aca_dhcp_programming_if
#endif // #ifndef ACA_DHCP_PROGRAMMING_IF_H
