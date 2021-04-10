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