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
using namespace aca_dhcp_programming_if;

namespace aca_dhcp_server
{
ACA_Dhcp_Server::ACA_Dhcp_Server()
{
  try {
    _dhcp_db = new map<string, dhcp_entry_data>;
  } catch (const bad_alloc &e) {
    return;
  }

  _dhcp_entry_thresh = 0x10000; //10K
}

ACA_Dhcp_Server::~ACA_Dhcp_Server()
{
  delete _dhcp_db;
  _dhcp_db = nullptr;
}

int ACA_Dhcp_Server::initialize()
{
  return 0;
}

int ACA_Dhcp_Server::add_dhcp_entry(dhcp_config *dhcp_cfg_in)
{
  dhcp_entry_data stData;

  if (_validate_dhcp_entry(dhcp_cfg_in)) {
    ACA_LOG_ERROR("Valiate dhcp cfg failed! (mac = %s)\n",
                  dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  if (DHCP_DB_SIZE >= _dhcp_entry_thresh) {
    ACA_LOG_WARN("Exceed db threshold! (dhcp_db_size = %s)\n", DHCP_DB_SIZE);
  }

  DHCP_ENTRY_DATA_SET((dhcp_entry_data *)&stData, dhcp_cfg_in);

  if (_dhcp_db->end() != _dhcp_db->find(dhcp_cfg_in->mac_address)) {
    ACA_LOG_ERROR("Entry already existed! (mac = %s)\n",
                  dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  _dhcp_db_mutex.lock();
  _dhcp_db->insert(make_pair(dhcp_cfg_in->mac_address, stData));
  _dhcp_db_mutex.unlock();

  return EXIT_SUCCESS;
}

int ACA_Dhcp_Server::delete_dhcp_entry(dhcp_config *dhcp_cfg_in)
{
  if (_validate_dhcp_entry(dhcp_cfg_in)) {
    ACA_LOG_ERROR("Valiate dhcp cfg failed! (mac = %s)\n",
                  dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  if (0 >= DHCP_DB_SIZE) {
    ACA_LOG_WARN("DHCP DB is empty! (mac = %s)\n", dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  if (_dhcp_db->end() == _dhcp_db->find(dhcp_cfg_in->mac_address)) {
    ACA_LOG_INFO("Entry not exist!  (mac = %s)\n", dhcp_cfg_in->mac_address.c_str());
    return EXIT_SUCCESS;
  }

  _dhcp_db_mutex.lock();
  _dhcp_db->erase(dhcp_cfg_in->mac_address);
  _dhcp_db_mutex.unlock();

  return EXIT_SUCCESS;
}

int ACA_Dhcp_Server::update_dhcp_entry(dhcp_config *dhcp_cfg_in)
{
  //dhcp_entry_data stData = {0};
  std::map<string, dhcp_entry_data>::iterator pos;

  if (_validate_dhcp_entry(dhcp_cfg_in)) {
    ACA_LOG_ERROR("Valiate dhcp cfg failed! (mac = %s)\n",
                  dhcp_cfg_in->mac_address.c_str());
    return EXIT_FAILURE;
  }

  pos = _dhcp_db->find(dhcp_cfg_in->mac_address);
  if (_dhcp_db->end() == pos) {
    ACA_LOG_ERROR("Entry not exist! (mac = %s)\n", dhcp_cfg_in->mac_address.c_str());

    return EXIT_FAILURE;
  }

  _dhcp_db_mutex.lock();
  DHCP_ENTRY_DATA_SET((dhcp_entry_data *)&(pos->second), dhcp_cfg_in);
  _dhcp_db_mutex.unlock();

  return EXIT_SUCCESS;
}

void ACA_Dhcp_Server::_validate_mac_address(const char *mac_string)
{
  unsigned char mac[6];

  if (mac_string == nullptr) {
    throw std::invalid_argument("Input mac_string is null");
  }

  if (sscanf(mac_string, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1],
             &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
    return;
  }

  if (sscanf(mac_string, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx", &mac[0], &mac[1],
             &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
    return;
  }

  // nothing matched
  ACA_LOG_ERROR("Invalid mac address: %s\n", mac_string);

  throw std::invalid_argument("Input mac_string is not in the expect format");
}

void ACA_Dhcp_Server::_validate_ipv4_address(const char *ip_address)
{
  struct sockaddr_in sa;

  // inet_pton returns 1 for success 0 for failure
  if (inet_pton(AF_INET, ip_address, &(sa.sin_addr)) != 1) {
    throw std::invalid_argument("Virtual ipv4 address is not in the expect format");
  }
}

void ACA_Dhcp_Server::_validate_ipv6_address(const char *ip_address)
{
  struct sockaddr_in sa;

  // inet_pton returns 1 for success 0 for failure
  if (inet_pton(AF_INET6, ip_address, &(sa.sin_addr)) != 1) {
    throw std::invalid_argument("Virtual ipv6 address is not in the expect format");
  }
}

int ACA_Dhcp_Server::_validate_dhcp_entry(dhcp_config *dhcp_cfg_in)
{
  if (0 >= dhcp_cfg_in->mac_address.size()) {
    throw std::invalid_argument("Input mac_string is null");
  }

  _validate_mac_address(dhcp_cfg_in->mac_address.c_str());

  if (0 < dhcp_cfg_in->ipv4_address.size()) {
    _validate_ipv4_address(dhcp_cfg_in->ipv4_address.c_str());
  }

  if (0 < dhcp_cfg_in->ipv6_address.size()) {
    _validate_ipv6_address(dhcp_cfg_in->ipv4_address.c_str());
  }

  return EXIT_SUCCESS;
}

int ACA_Dhcp_Server::_get_db_size() const
{
  if (nullptr != _dhcp_db) {
    return _dhcp_db->size();
  } else {
    ACA_LOG_ERROR("DHCP-DB does not exist!\n");
    return EXIT_FAILURE;
  }
}

} //namespace aca_dhcp_server
