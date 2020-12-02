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

using namespace std;
static atomic_uint current_available_group_id(1);

namespace aca_zeta_programming
{
struct zeta_config {
  string group_id;
  //
  list<string> zeta_buckets;
  uint32_t port_inband_operation;
};

struct aux_gateway_entry {
  uint oam_port;
  uint group_id;
};

class ACA_Zeta_Programming {
  public:
  static ACA_Zeta_Programming &get_instance();

  uint get_or_create_group_id(string auxGateway_id);

  int create_or_update_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                                   const string vpc_id, uint32_t tunnel_id);

  int delete_zeta_config(const alcor::schema::AuxGateway current_AuxGateway,
                         const string vpc_id, uint32_t tunnel_id);

  bool is_exist_group_rule(uint group_id);

  uint get_oam_server_port(string auxGateway_id);

  void set_oam_server_port(string auxGateway_id, uint port_number);

  bool is_exist_oam_port(uint port_number);

  private:
  int _create_or_update_zeta_group_entry(zeta_config *zeta_config_in);

  int _delete_zeta_group_entry(zeta_config *zeta_config_in);

  private:
  // unordered_map<aux_gateway_id, aux_gateway_entry>
  unordered_map<string, aux_gateway_entry> _zeta_gateways_table;

  mutex _zeta_gateways_table_mutex;

  void create_entry_unsafe(string auxGateway_id);
};
} // namespace aca_zeta_programming
#endif // #ifndef ACA_ZETA_PROGRAMMING_H