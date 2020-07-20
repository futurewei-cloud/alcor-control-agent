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

#ifndef ACA_UTIL_H
#define ACA_UTIL_H

#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <string>

// the hashed digits for the outport postfix string is based on remote IP string,
// it should be more than enough for the number of hosts in a region
#define MAX_OUTPORT_NAME_POSTFIX 99999999

#define cast_to_nanoseconds(x) chrono::duration_cast<chrono::nanoseconds>(x)

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
  case alcor::schema::OperationType::NEIGHBOR_CREATE_UPDATE:
    return "NEIGHBOR_CREATE_UPDATE";
  case alcor::schema::OperationType::NEIGHBOR_GET:
    return "NEIGHBOR_GET";
  case alcor::schema::OperationType::NEIGHBOR_DELETE:
    return "NEIGHBOR_DELETE";

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

#endif
