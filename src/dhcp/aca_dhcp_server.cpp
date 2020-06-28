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

#include "aca_dhcp_server.h"
#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <errno.h>
#include <arpa/inet.h>

using namespace std;
using namespace alcorcontroller;
using namespace aca_dhcp_programming_if;

namespace aca_dhcp_server
{
ACA_Dhcp_Server::ACA_Dhcp_Server()
{
	try{
		_dhcp_db = new std::map<string, dhcp_entry_data, dhcp_entry_comp>;
	} catch (const bad_alloc &e){
		return;
	}

	_dhcp_entry_thresh = 0x10000; //10K
}

ACA_Dhcp_Server::~ACA_Dhcp_Server()
{
	delete _dhcp_db;
	_dhcp_db = NULL;
}


int ACA_Dhcp_Server::initialize()
{
	return 0;
}

int ACA_Dhcp_Server::add_dhcp_entry(dhcp_config *dhcp_cfg_in)
{
	dhcp_entry_data stData = {0};

	if (_validate_dhcp_entry(dhcp_cfg_in)){
		ACA_LOG_ERROR("Valiate dhcp cfg failed! (mac = %s\n", dhcp_cfg_in->mac_address);
		return EXIT_FAILURE;
	}

	if (DHCP_DB_SIZE >= _dhcp_entry_thresh){
		ACA_LOG_WARN("Exceed db threshold! (dhcp_db_size = %s\n)", DHCP_DB_SIZE);
	}

	DHCP_ENTRY_DATA_SET((dhcp_entry_data*)&stData, dhcp_cfg_in);

	if (_dhcp_db->end() != _dhcp_db->find(dhcp_cfg_in->mac_address)){
		ACA_LOG_ERROR("Entry already existed! (mac = %s\n", dhcp_cfg_in->mac_address);
		return EXIT_FAILURE;
	}

	_dhcp_db->insert(make_pair(dhcp_cfg_in->mac_address, stData));

	return EXIT_SUCCESS;
}

int ACA_Dhcp_Server::delete_dhcp_entry(dhcp_config *dhcp_cfg_in)
{
	if (_validate_dhcp_entry(dhcp_cfg_in)){
		ACA_LOG_ERROR("Valiate dhcp cfg failed! (mac = %s\n", dhcp_cfg_in->mac_address);
		return EXIT_FAILURE;
	}

	if (0 >= DHCP_DB_SIZE){
		ACA_LOG_WARN("DHCP DB is empty! (mac = %s\n", dhcp_cfg_in->mac_address);
		return EXIT_SUCCESS;
	}

	if (_dhcp_db->end() == _dhcp_db->find(dhcp_cfg_in->mac_address)){
		ACA_LOG_INFO("Entry not exist!  (mac = %s\n", dhcp_cfg_in->mac_address);
		return EXIT_SUCCESS;
	}

	_dhcp_db->erase(dhcp_cfg_in->mac_address);

	return EXIT_SUCCESS;
}

int ACA_Dhcp_Server::update_dhcp_entry(dhcp_config *dhcp_cfg_in)
{
	//dhcp_entry_data stData = {0};
	std::map<string, dhcp_entry_data>::iterator pos;

	if (_validate_dhcp_entry(dhcp_cfg_in)){
		ACA_LOG_ERROR("Valiate dhcp cfg failed! (mac = %s\n", dhcp_cfg_in->mac_address);
		return EXIT_FAILURE;
	}

	pos = _dhcp_db->find(dhcp_cfg_in->mac_address);
	if (_dhcp_db->end() == pos){
		ACA_LOG_ERROR("Entry not exist! (mac = %s\n", dhcp_cfg_in->mac_address);

		return EXIT_FAILURE;
	}

	DHCP_ENTRY_DATA_SET((dhcp_entry_data*)&(pos->second), dhcp_cfg_in);

	return EXIT_SUCCESS;
}

int ACA_Dhcp_Server::_validate_dhcp_entry(dhcp_config *dhcp_cfg_in)
{
	//validate ip_address

	//validate mac_address
	return 0;
}

int ACA_Dhcp_Server::_get_db_size() const
{
	if (NULL != _dhcp_db){
		return _dhcp_db->size();
	}else{
		return 0;
	}
}


}//namespace aca_dhcp_server
