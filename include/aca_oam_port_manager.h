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

  void create_entry_unsafe(uint port_number);
  void add_oam_port_rule(uint port_number);
  int remove_oam_port_rule(uint port_number);



  //Determine whether the port is an oam_server_port
  bool is_oam_server_port(uint port_number);

  bool is_exist_oam_port_rule(uint port_number);
  private:
  int _create_oam_ofp(uint port_number);
  int _delete_oam_ofp(uint port_number);
  void _clear_all_data();

  // unordered_set<oam_port_number>
  unordered_set<uint> _oam_ports_cache;

  // mutex for reading and writing to _oam_ports_cache
  mutex _oam_ports_cache_mutex;
};

} // namespace aca_oam_port_manager
#endif //#ifndef ACA_OAM_PORT_MANAGER_H