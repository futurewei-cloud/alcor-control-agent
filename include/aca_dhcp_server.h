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

#ifndef ACA_DHCP_SERVER_H
#define ACA_DHCP_SERVER_H

#include "aca_dhcp_programming_if.h"
#include <map>
#include <mutex>

using namespace aca_dhcp_programming_if;

// dhcp server implementation class
namespace aca_dhcp_server
{
struct dhcp_entry_data {
  string ipv4_address;
  string ipv6_address;
  string port_host_name;
};

#define DHCP_ENTRY_DATA_SET(pData, pCfg)                                       \
  do {                                                                         \
    (pData)->ipv4_address = (pCfg)->ip_address;                                \
    (pData)->port_host_name = (pCfg)->port_host_name;                          \
  } while (0)

class ACA_Dhcp_Server : public aca_dhcp_programming_if::ACA_Dhcp_Programming_Interface {
  public:
  ACA_Dhcp_Server();

  ~ACA_Dhcp_Server();

  int initialize();

  int add_dhcp_entry(dhcp_config *dhcp_cfg_in);

  int update_dhcp_entry(dhcp_config *dhcp_cfg_in);

  int delete_dhcp_entry(dhcp_config *dhcp_cfg_in);

  private:
  void _validate_mac_address(const char *mac_string);

  void _validate_ipv4_address(const char *ip_address);

  void _validate_ipv6_address(const char *ip_address);

  int _validate_dhcp_entry(dhcp_config *dhcp_cfg_in);

  int _dhcp_entry_thresh;

  int _get_db_size() const;
#define DHCP_DB_SIZE _get_db_size()

  std::map<std::string, dhcp_entry_data> *_dhcp_db;

  std::mutex _dhcp_db_mutex;
};

} // namespace aca_dhcp_server
#endif // #ifndef ACA_DHCP_SERVER_H
