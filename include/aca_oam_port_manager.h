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

#ifndef ACA_OAM_PORT_MANAGER_H
#define ACA_OAM_PORT_MANAGER_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>

using namespace std;

namespace aca_oam_port_manager
{
class Aca_Oam_Port_Manager {
  public:
  Aca_Oam_Port_Manager();
  ~Aca_Oam_Port_Manager();

  static Aca_Oam_Port_Manager &get_instance();

  void create_entry_unsafe(uint32_t port_id);
  void add_vpc(uint32_t port, const string vpc_id);
  int remove_vpc(uint32_t port, const string vpc_id);

  //Determine whether the port is an oam_server_port
  bool is_oam_server_port(uint32_t port_id);

  private:
  void _create_oam_ofp(uint32_t port_id);
  int _delete_oam_ofp(uint32_t port_id);
  void _clear_all_data();

  unordered_map<uint32_t, unordered_set<uint32_t> > _oam_ports_table;

  // mutex for reading and writing to _oam_ports_table
  mutex _oam_ports_table_mutex;
};

} // namespace aca_oam_port_manager
#endif //#ifndef ACA_OAM_PORT_MANAGER_H