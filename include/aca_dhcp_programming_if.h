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

#ifndef ACA_DHCP_PROGRAMMING_IF_H
#define ACA_DHCP_PROGRAMMING_IF_H

#include "aca_config.h"
#include <string>

using namespace std;

namespace aca_dhcp_programming_if
{

struct dhcp_config {
  string mac_address;
  string ipv4_address;
  string ipv6_address;
  string subnet_mask;
  string gateway_address;
  string port_host_name;
  string dns_addresses[DHCP_MSG_OPTS_DNS_LENGTH];
};

// DHCP programming interface class
class ACA_Dhcp_Programming_Interface {
  public:
  // pure virtual functions providing interface framework.
  virtual int add_dhcp_entry(dhcp_config *dhcp_config_in) = 0;

  virtual int update_dhcp_entry(dhcp_config *dhcp_config_in) = 0;

  virtual int delete_dhcp_entry(dhcp_config *dhcp_config_in) = 0;
};
} // namespace aca_dhcp_programming_if
#endif // #ifndef ACA_DHCP_PROGRAMMING_IF_H
