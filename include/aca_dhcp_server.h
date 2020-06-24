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

using namespace aca_dhcp_programming_if;

// dhcp server implementation class
namespace aca_dhcp_server
{
struct dhcp_entry_key{
	string network_id;
	string mac_address;
};

struct dhcp_entry_data{
	string ip_address;
	string ep_host_name;
};

struct dhcp_entry_comp{
	int operator() (const dhcp_entry_key &d, const dhcp_entry_key &k) const
	{
		if (d.network_id > k.network_id){
			return 0;
		}
		if ((d.network_id == k.network_id) && (d.mac_address >= k.mac_address)){
			return 0;
		}
		return 1;
	}
};

class ACA_Dhcp_Server : public aca_dhcp_programming_if::ACA_Dhcp_Programming_Interface {
  public:
	ACA_Dhcp_Server();

	~ACA_Dhcp_Server();

	int initialize();

	int add_dhcp_entry(dhcp_config *dhcp_cfg_in);

	int update_dhcp_entry(dhcp_config *dhcp_cfg_in);

	int delete_dhcp_entry(dhcp_config *dhcp_cfg_in);

  private:
	int _validate_dhcp_entry(dhcp_config *dhcp_cfg_in);

	int _dhcp_entry_thresh;

	int _get_db_size() const;
#define DHCP_DB_SIZE	_get_db_size()

	std::map<dhcp_entry_key, dhcp_entry_data, dhcp_entry_comp> *_dhcp_db;
};



} // namespace aca_dhcp_server
#endif // #ifndef ACA_DHCP_SERVER_H
