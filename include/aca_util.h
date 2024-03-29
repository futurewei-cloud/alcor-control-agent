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

#ifndef ACA_UTIL_H
#define ACA_UTIL_H

#include "aca_net_config.h"
#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <string>
#include <arpa/inet.h>

// the hashed digits for the outport postfix string is based on remote IP string,
// it should be more than enough for the number of hosts in a region
#define MAX_OUTPORT_NAME_POSTFIX 99999999

#define TAP_PREFIX "tap" // vm tap device prefix
#define PORT_NAME_LEN 14 // Nova generated port name length

// the number of characters needed to store the HEX form of IP address
#define HEX_IP_BUFFER_SIZE 12

// vxlan-generic openflow outport number
#define VXLAN_GENERIC_OUTPORT_NUMBER "100"

// maximun valid value of a VNI, that (2^24) - 1
// applicable for VxLAN, GRE, VxLAN-GPE and Geneve
#define MAX_VALID_VNI 16777215

#define MAX_VALID_VLAN_ID 4094

#define cast_to_nanoseconds(x) chrono::duration_cast<chrono::nanoseconds>(x)
#define cast_to_microseconds(x) chrono::duration_cast<chrono::microseconds>(x)
#define us_to_ms(x) x / 1000 // convert from microseconds to millseconds

static inline const char *aca_get_operation_string(alcor::schema::OperationType operation)
{
  switch (operation) {
  case alcor::schema::OperationType::CREATE:
    return "CREATE";
  case alcor::schema::OperationType::UPDATE:
    return "UPDATE";
  case alcor::schema::OperationType::GET:
    return "GET";
  case alcor::schema::OperationType::DELETE:
    return "DELETE";
  case alcor::schema::OperationType::INFO:
    return "INFO";

  default:
    return "ERROR: unknown operation type!";
  }
}

static inline std::string aca_get_network_type_string(alcor::schema::NetworkType network_type)
{
  switch (network_type) {
  case alcor::schema::NetworkType::VXLAN:
    return "vxlan";
  case alcor::schema::NetworkType::VLAN:
    return "vlan";
  case alcor::schema::NetworkType::GRE:
    return "gre";
  case alcor::schema::NetworkType::GENEVE:
    return "geneve";
  case alcor::schema::NetworkType::VXLAN_GPE:
    return "vxlang";

  default:
    return "ERROR: unknown network type!";
  }
}

static inline std::string
aca_get_outport_name(alcor::schema::NetworkType network_type, std::string remote_ip)
{
  std::hash<std::string> str_hash;

  // TODO: change to hex encoding to get more possible combinations
  auto hash_value = str_hash(remote_ip) % MAX_OUTPORT_NAME_POSTFIX;

  return aca_get_network_type_string(network_type) + "-" + std::to_string(hash_value);
}

static inline std::string aca_get_port_name(std::string port_id)
{
  std::string port_name = TAP_PREFIX + port_id;
  port_name = port_name.substr(0, PORT_NAME_LEN);

  return port_name;
}

static inline const char *aca_get_neighbor_type_string(alcor::schema::NeighborType neighbor_type)
{
  switch (neighbor_type) {
  case alcor::schema::NeighborType::L2:
    return "L2";
  case alcor::schema::NeighborType::L3:
    return "L3";

  default:
    return "ERROR: unknown neighbor type!";
  }
}

static inline bool aca_validate_fixed_ips_size(const int fixed_ips_size)
{
  if (fixed_ips_size <= 0) {
    ACA_LOG_ERROR("fixed_ips_size: %d is less than zero\n", fixed_ips_size);
    return false;
  }
  return true;
}

static inline bool aca_validate_mac_address(const char *mac_string)
{
  unsigned char mac[6];

  if (mac_string == nullptr) {
    ACA_LOG_ERROR("%s", "Input mac_string is null\n");
    return false;
  }

  if (sscanf(mac_string, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1],
             &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
    return true;
  }

  if (sscanf(mac_string, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx", &mac[0], &mac[1],
             &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
    return true;
  }

  // nothing matched
  ACA_LOG_ERROR("Invalid mac address: %s\n", mac_string);
  return false;
}

static inline bool
aca_validate_tunnel_id(const uint tunnel_id, alcor::schema::NetworkType network_type)
{
  if (tunnel_id == 0) {
    ACA_LOG_ERROR("%s", "Input tunnel_id is 0\n");
    return false;
  }

  switch (network_type) {
  case alcor::schema::NetworkType::VXLAN:
    [[fallthrough]];
  case alcor::schema::NetworkType::VXLAN_GPE:
    [[fallthrough]];
  case alcor::schema::NetworkType::GRE:
    [[fallthrough]];
  case alcor::schema::NetworkType::GENEVE:
    if (tunnel_id <= MAX_VALID_VNI) {
      return true;
    } else {
      ACA_LOG_ERROR("Input tunnel_id: %d is greater than valid maximun: %s\n",
                    tunnel_id, std::to_string(MAX_VALID_VNI).c_str());
      return false;
    }

  case alcor::schema::NetworkType::VLAN:
    if (tunnel_id <= MAX_VALID_VLAN_ID) {
      return true;
    } else {
      ACA_LOG_ERROR("Input tunnel_id: %d is greater than valid maximun: %s\n",
                    tunnel_id, std::to_string(MAX_VALID_VNI).c_str());
      return false;
    }

  default:
    ACA_LOG_ERROR("%s", "ERROR: unknown network type!\n");
    return false;
  }
}

static inline string aca_convert_cidr_to_netmask(const std::string cidr)
{
  if (cidr.empty()) {
    throw std::invalid_argument("cidr is empty");
  }

  // get cidr netmask length str
  size_t slash_pos = cidr.find("/");
  if (slash_pos == string::npos) {
    throw std::invalid_argument("'/' not found in cidr");
  }

  int netmask_num = std::stoi(cidr.substr(slash_pos + 1));
  string netmask = "";
  bool over = false; // mark whether handle all netmask_num
  for (int i = 1; i < 5; i++) {
    if (over) {
      // if overed the remain is 0.
      netmask += "0.";
      continue;
    }

    if (i * 8 <= netmask_num) {
      // before fixed is 255
      netmask += "255.";
    } else {
      over = true;
      int tmp = 0;
      // dynamic netmask handle
      for (int m = 1; m < netmask_num % 8 + 1; m++) {
        tmp += 1 << (8 - m);
      }
      netmask += std::to_string(tmp) + ".";
    }
  }
  return netmask.substr(0, netmask.size() - 1);
}

static inline long ip4tol(const string ip)
{
  struct sockaddr_in sa;
  if (inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 1) {
    throw std::invalid_argument("Virtual ipv4 address is not in the expect format");
  }
  return sa.sin_addr.s_addr;
}
#endif
