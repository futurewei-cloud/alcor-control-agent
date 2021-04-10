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
class FWD_Info {
  public:
  string ip_addr;
  string mac_addr;

  FWD_Info(){};
  ~FWD_Info(){};
  FWD_Info(string ip_addr, string mac_addr)
  {
    this->ip_addr = ip_addr;
    this->mac_addr = mac_addr;
  }

  // Overload "==" for hash operation
  bool operator==(const FWD_Info &other) const
  {
    return ((ip_addr == other.ip_addr) && (mac_addr == other.mac_addr));
  };

  // Overload "!=" for hash operation
  bool operator!=(const FWD_Info &other) const
  {
    return ((ip_addr != other.ip_addr) || (mac_addr != other.mac_addr));
  };
};

//Implement the hash of FWD_info based on std::hash<>
class FWD_Info_Hash {
  public:
  std::size_t operator()(const FWD_Info &rhs) const
  {
    return std::hash<string>()(rhs.ip_addr + rhs.mac_addr);
  }
}; // namespace aca_zeta_programming

struct zeta_config {
  uint group_id;
  uint oam_port;

  // CTSL::HashMap <key: FWD_Info, value: int* (not used), hash: FWD_Info_Hash>
  CTSL::HashMap<FWD_Info, int *, FWD_Info_Hash> zeta_buckets;
};

class ACA_Zeta_Programming {
  public:
  ACA_Zeta_Programming();
  ~ACA_Zeta_Programming();
  static ACA_Zeta_Programming &get_instance();

  void create_entry(string zeta_gateway_id, uint oam_port,
                    alcor::schema::GatewayConfiguration current_AuxGateway);

  void clear_all_data();

  int create_zeta_config(const alcor::schema::GatewayConfiguration current_AuxGateway,
                         uint tunnel_id);

  int delete_zeta_config(const alcor::schema::GatewayConfiguration current_AuxGateway,
                         uint tunnel_id);

  bool group_rule_exists(uint group_id);

  uint get_oam_port(string zeta_gateway_id);

  uint get_group_id(string zeta_gateway_id);

  bool group_rule_info_correct(uint group_id, string gws_ip, string gws_mac);

  private:
  int _create_group_punt_rule(uint tunnel_id, uint group_id);
  int _delete_group_punt_rule(uint tunnel_id);

  int _create_zeta_group_entry(zeta_config *zeta_config_in);
  int _update_zeta_group_entry(zeta_config *zeta_config_in);
  int _delete_zeta_group_entry(zeta_config *zeta_config_in);

  // hashtable <key: zeta_gateway_id, value: zeta_config>
  CTSL::HashMap<string, zeta_config *> _zeta_config_table;

  //The mutex for modifying group table entry
  std::timed_mutex _group_operation_mutex;

  // mutex for reading and writing to _zeta_config_table
  mutex _zeta_config_table_mutex;
};
} // namespace aca_zeta_programming
#endif // #ifndef ACA_ZETA_PROGRAMMING_H