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

#ifndef ACA_ARP_RESPONDER_H
#define ACA_ARP_RESPONDER_H

#include <string>
#include <unordered_map>
#include "hashmap/HashMap.h"
#include <mutex>

using namespace std;

namespace aca_arp_responder
{

#define ARP_ENTRY_DATA_SET(pData,pConfig)                           \
  do {                                                              \
    (pData)->ipv4_address = (pConfig)->ipv4_address;                \
    (pData)->vlan_id = (pConfig)->vlan_id;                          \
  } while (0)                                                       

#define ARP_TABLE_DATA_SET(pData,pConfig)                           \
  do {                                                              \
    (pData)->mac_address = (pConfig)->mac_address;                  \
  } while (0)  


struct arp_config{
  string mac_address;
  string ipv4_address;
  string ipv6_address; 
  uint16_t vlan_id;
  string port_host_name;
};

struct arp_entry_data{
  string ipv4_address;
  string ipv6_address; 
  uint16_t vlan_id;
  bool operator==(const arp_entry_data &p) const{
    return ipv4_address==p.ipv4_address && vlan_id==p.vlan_id;
  }

  bool operator!=(const arp_entry_data &p) const{
    return ipv4_address!=p.ipv4_address || vlan_id!=p.vlan_id;
  }
};


struct arp_hash{
  size_t operator() (const arp_entry_data &p) const{
    return hash<string>()(p.ipv4_address) ^ (hash<uint16_t>()(p.vlan_id) << 1);
  }
};

struct arp_table_data{
  string mac_address;
};

struct arp_message{
  uint16_t hrd;
  uint16_t pro;
  uint8_t hln;
  uint8_t pln;
  uint16_t op;
  uint8_t sha[6];
  uint32_t spa;
  uint8_t tha[6];
  uint32_t tpa;
} __attribute__ ((__packed__));

struct vlan_message{
  uint16_t vlan_proto; //should always be 0x8100
  uint16_t vlan_tci;
};

//ARP Message Type
#define ARP_MSG_ARPREQUEST (0x1)
#define ARP_MSG_ARPREPLY (0x2)

//ARP Message Fields
#define ARP_MSG_HRD_TYPE (0x1)
#define ARP_MSG_PRO_TYPE (0x0800)
#define ARP_MSG_HRD_LEN (0x6)
#define ARP_MSG_PRO_LEN (0x4)

class ACA_ARP_Responder{
  public:  
    static ACA_ARP_Responder &get_instance();

    /* Managemet Plane Ops*/
    int add_arp_entry(arp_config *arp_config_in);
    int create_or_update_arp_entry(arp_config *arp_config_in);
    int delete_arp_entry(arp_config *arp_config_in);

    /* Data plane Ops */
    int arp_recv(uint32_t in_port, void *vlanmsg, void *message);
    void arp_xmit(uint32_t in_port, void *vlanmsg, void *message, int is_find);
    string _get_requested_ip(arp_message *arpmsg);
    string _get_source_ip(arp_message *arpmsg);
    int _parse_arp_request(uint32_t in_port, vlan_message *vlanmsg, arp_message *arpmsg);

  private:
    ACA_ARP_Responder();
    ~ACA_ARP_Responder();

    CTSL::HashMap<arp_entry_data,arp_table_data *,arp_hash> _arp_db;
    



    /*************** Initialization and De-initialization ***********************/
    void _init_arp_db();
    void _deinit_arp_db();
    void _init_arp_ofp();
    void _deinit_arp_ofp();

    /*************** Management plane operations ***********************/
    void _validate_mac_address(const char *mac_string);
    void _validate_ipv4_address(const char *ip_address);
    void _validate_ipv6_address(const char *ip_address);
    int _validate_arp_entry(arp_config *arp_cfg_in);

    /**************** Data plane operations *********************/
    int _validate_arp_message(arp_message *arpmsg);

    arp_message *_pack_arp_reply(arp_message *arpreq, string mac_address);

    string _serialize_arp_message(vlan_message *vlanmsg, arp_message *arpmsg);

};
} // namespace aca_arp_responder
#endif // #ifndef ACA_ARP_RESPONDER_H

