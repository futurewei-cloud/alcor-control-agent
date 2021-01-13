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

#ifndef ACA_ZETA_PROGRAMMING_H
#define ACA_ZETA_PROGRAMMING_H

#include <string>
#include <list>
#include <atomic>
#include "goalstateprovisioner.grpc.pb.h"
#include "hashmap/HashMap.h"

using namespace std;

static atomic_uint current_available_group_id(1);

namespace aca_zeta_programming
{
struct fwd_info {
  string ip_addr;
  string mac_addr;
  fwd_info(string ip_addr = "", string mac_addr = "")
          : ip_addr(ip_addr), mac_addr(mac_addr){};

  // Overload "=="for hash operation
  bool operator==(const fwd_info &other) const
  {
    if (ip_addr == other.ip_addr && mac_addr == other.mac_addr) {
      return true;
    } else {
      return false;
    }
  };
};
struct zeta_config {
  uint group_id;
  uint oam_port;

  // CTSL::HashMap <key: fwd_info, value: int* (not used)>
  CTSL::HashMap<fwd_info *, int *> zeta_buckets;
};

class ACA_Zeta_Programming {
  public:
  ACA_Zeta_Programming();
  ~ACA_Zeta_Programming();
  static ACA_Zeta_Programming &get_instance();

  void create_entry(string zeta_gateway_id, uint oam_port,
                    alcor::schema::AuxGateway current_AuxGateway);

  void clear_all_data();

  int create_zeta_config(const alcor::schema::AuxGateway current_AuxGateway, uint tunnel_id);

  int delete_zeta_config(const alcor::schema::AuxGateway current_AuxGateway, uint tunnel_id);

  bool oam_port_rule_exists(uint port_number);

  bool group_rule_exists(uint group_id);

  uint get_oam_port(string zeta_gateway_id);

  uint get_group_id(string zeta_gateway_id);

  private:
  int _create_oam_ofp(uint port_number);
  int _delete_oam_ofp(uint port_number);

  int _create_group_punt_rule(uint tunnel_id, uint group_id);
  int _delete_group_punt_rule(uint tunnel_id);

  int _create_zeta_group_entry(zeta_config *zeta_config_in);
  int _update_zeta_group_entry(zeta_config *zeta_cfg);
  int _delete_zeta_group_entry(zeta_config *zeta_config_in);

  // hashtable <key: zeta_gateway_id, value: zeta_config>
  CTSL::HashMap<string, zeta_config *> _zeta_config_table;
  
  //The mutex for modifying group table entry
  std::timed_mutex _group_operation_mutex;
};
} // namespace aca_zeta_programming
#endif // #ifndef ACA_ZETA_PROGRAMMING_H