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

#include "aca_dataplane_ovs.h"
#include "aca_goal_state_handler.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_ovs_l3_programmer.h"
#include "aca_net_config.h"
#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_util.h"
#include <errno.h>
#include <arpa/inet.h>
#include "aca_zeta_programming.h"

using namespace std;
using namespace alcor::schema;
using namespace aca_ovs_l2_programmer;
using namespace aca_ovs_l3_programmer;
using namespace aca_zeta_programming;

namespace aca_dataplane_ovs
{
static bool aca_lookup_subnet_info(GoalState &parsed_struct, const string targeted_subnet_id,
                                   NetworkType &found_network_type,
                                   uint &found_tunnel_id, string &found_prefix_len)
{
  // TODO: cache the subnet information to a dictionary to provide
  // a faster look up for the next run, only use the below loop for
  // cache miss.
  // Look up the subnet configuration to query for tunnel_id
  for (int j = 0; j < parsed_struct.subnet_states_size(); j++) {
    SubnetConfiguration current_SubnetConfiguration =
            parsed_struct.subnet_states(j).configuration();

    ACA_LOG_DEBUG("current_SubnetConfiguration subnet ID: %s.\n",
                  current_SubnetConfiguration.id().c_str());

    if (parsed_struct.subnet_states(j).operation_type() == OperationType::INFO) {
      if (current_SubnetConfiguration.id() == targeted_subnet_id) {
        found_network_type = current_SubnetConfiguration.network_type();
        found_tunnel_id = current_SubnetConfiguration.tunnel_id();
        if (!aca_validate_tunnel_id(found_tunnel_id, found_network_type)) {
          throw std::invalid_argument("found_tunnel_id is invalid");
        }

        string found_cidr = current_SubnetConfiguration.cidr();
        size_t slash_pos = found_cidr.find('/');
        if (slash_pos == string::npos) {
          throw std::invalid_argument("'/' not found in cidr");
        }

        // substr can throw out_of_range and bad_alloc exceptions
        found_prefix_len = found_cidr.substr(slash_pos + 1);

        return true;
      }
    }
  }

  ACA_LOG_ERROR("Not able to find the info for port with subnet ID: %s.\n",
                targeted_subnet_id.c_str());
  return false;
}

static bool aca_lookup_zeta_gateway_info(GoalState &parsed_struct, const string targeted_vpc_id,
                                         GatewayConfiguration &found_zeta_gateway)
{
  // TODO: cache the zeta gateway information to a dictionary to provide
  // a faster look up for the next run, only use the below loop for
  // cache miss.
  // Look up the vpc configuration to query for zeta gateway
  for (int i = 0; i < parsed_struct.vpc_states_size(); i++) {
    VpcConfiguration current_VpcConfiguration =
            parsed_struct.vpc_states(i).configuration();

    ACA_LOG_DEBUG("current_VpcConfiguration Vpc ID: %s.\n",
                  current_VpcConfiguration.id().c_str());

    if (parsed_struct.vpc_states(i).operation_type() == OperationType::INFO) {
      if (current_VpcConfiguration.id() == targeted_vpc_id) {
        for (int j = 0; j < current_VpcConfiguration.gateway_ids_size(); j++) {
          for (int k = 0; k < parsed_struct.gateway_states_size(); k++) {
            if (parsed_struct.gateway_states(k).configuration().id() ==
                        current_VpcConfiguration.gateway_ids(j) &&
                parsed_struct.gateway_states(k).configuration().gateway_type() == ZETA) {
              found_zeta_gateway = parsed_struct.gateway_states(k).configuration();
              return true;
            }
          }
        }
      }
    }
  }

  ACA_LOG_ERROR("Not able to find zeta gateway info for port with vpc ID: %s.\n",
                targeted_vpc_id.c_str());
  return false;
}

int ACA_Dataplane_OVS::initialize()
{
  // TODO: improve the logging system, and add logging to this module
  return ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
}

int ACA_Dataplane_OVS::update_vpc_state_workitem(const VpcState /* current_VpcState */,
                                                 GoalStateOperationReply & /* gsOperationReply */)
{
  // TO BE IMPLEMENTED

  return ENOSYS;
}

int ACA_Dataplane_OVS::update_subnet_state_workitem(const SubnetState current_SubnetState,
                                                    GoalStateOperationReply &gsOperationReply)
{
  int overall_rc;
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;

  auto operation_start = chrono::steady_clock::now();

  SubnetConfiguration current_SubnetConfiguration = current_SubnetState.configuration();

  switch (current_SubnetState.operation_type()) {
  case OperationType::INFO:
    // information only, ignoring this.
    overall_rc = EXIT_SUCCESS;
    break;

  default:
    ACA_LOG_ERROR("Invalid subnet state operation type %d\n",
                  current_SubnetState.operation_type());
    overall_rc = -EXIT_FAILURE;
    break;
  }

  auto operation_end = chrono::steady_clock::now();

  auto operation_total_time =
          cast_to_microseconds(operation_end - operation_start).count();

  aca_goal_state_handler::Aca_Goal_State_Handler::get_instance().add_goal_state_operation_status(
          gsOperationReply, current_SubnetConfiguration.id(), SUBNET,
          current_SubnetState.operation_type(), overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);

  return overall_rc;
}

int ACA_Dataplane_OVS::update_port_state_workitem(const PortState current_PortState,
                                                  GoalState &parsed_struct,
                                                  GoalStateOperationReply &gsOperationReply)
{
  int overall_rc;
  string generated_port_name;
  struct sockaddr_in sa;
  uint found_tunnel_id;
  NetworkType found_network_type;
  string found_prefix_len;
  string virtual_ip_address;
  string virtual_mac_address;
  string host_ip_address;
  string port_cidr;
  GatewayConfiguration found_zeta_gateway;
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;

  auto operation_start = chrono::steady_clock::now();

  PortConfiguration current_PortConfiguration = current_PortState.configuration();

  try {
    // TODO: need to design the usage of current_PortConfiguration.revision_number()
    assert(current_PortConfiguration.revision_number() > 0);

    // TODO: handle current_PortConfiguration.admin_state_up = false
    // need to look into the meaning of that during nova integration
    assert(current_PortConfiguration.admin_state_up() == true);

    assert(!current_PortConfiguration.id().empty());

    generated_port_name = aca_get_port_name(current_PortConfiguration.id());

    if (!aca_validate_fixed_ips_size(current_PortConfiguration.fixed_ips_size())) {
      throw std::invalid_argument("PortConfiguration.fixed_ips_size is less than zero");
    }

    if (!aca_lookup_subnet_info(
                parsed_struct, current_PortConfiguration.fixed_ips(0).subnet_id(),
                found_network_type, found_tunnel_id, found_prefix_len)) {
      ACA_LOG_ERROR("Not able to find the info for port with subnet ID: %s.\n",
                    current_PortConfiguration.fixed_ips(0).subnet_id().c_str());
      overall_rc = -EXIT_FAILURE;
      goto EXIT;
    }

    if (!aca_lookup_zeta_gateway_info(parsed_struct, current_PortConfiguration.vpc_id(),
                                      found_zeta_gateway)) {
      // mark as warning for now to support the current workflow
      // the code should proceed assuming this is a non aux gateway (zeta) supported port
      ACA_LOG_WARN("Not able to find auxgateway info for port with vpc ID: %s.\n",
                   current_PortConfiguration.vpc_id().c_str());
    }
    alcor::schema::OperationType current_operation_type =
            current_PortState.operation_type();
    if (current_PortState.operation_type() == OperationType::UPDATE) {
      if (!current_PortConfiguration.device_id().empty() &&
          !current_PortConfiguration.device_owner().empty()) {
        current_operation_type = OperationType::CREATE;
      } else if (current_PortConfiguration.device_id().empty() &&
                 current_PortConfiguration.device_owner().empty()) {
        current_operation_type = OperationType::DELETE;
      }
    }
    switch (current_operation_type) {
    case OperationType::CREATE:
      if (current_PortConfiguration.update_type() != UpdateType::FULL) {
        throw std::invalid_argument("current_PortConfiguration.update_type should be UpdateType::FULL");
      }

      virtual_ip_address = current_PortConfiguration.fixed_ips(0).ip_address();

      // inet_pton returns 1 for success 0 for failure
      if (inet_pton(AF_INET, virtual_ip_address.c_str(), &(sa.sin_addr)) != 1) {
        throw std::invalid_argument("Virtual ip address is not in the expect format");
      }

      virtual_mac_address = current_PortConfiguration.mac_address();
      if (!aca_validate_mac_address(virtual_mac_address.c_str())) {
        throw std::invalid_argument("virtual_mac_address is invalid");
      }

      port_cidr = virtual_ip_address + "/" + found_prefix_len;

      ACA_LOG_DEBUG("Port Operation: %s: port_id: %s, vpc_id:%s, network_type:%d, "
                    "virtual_ip_address:%s, virtual_mac_address:%s, generated_port_name: %s, port_cidr: %s, tunnel_id: %d\n",
                    aca_get_operation_string(current_PortState.operation_type()),
                    current_PortConfiguration.id().c_str(),
                    current_PortConfiguration.vpc_id().c_str(), found_network_type,
                    virtual_ip_address.c_str(), virtual_mac_address.c_str(),
                    generated_port_name.c_str(), port_cidr.c_str(), found_tunnel_id);

      overall_rc = ACA_OVS_L2_Programmer::get_instance().create_port(
              current_PortConfiguration.vpc_id(), generated_port_name, port_cidr,
              virtual_mac_address, found_tunnel_id, culminative_dataplane_programming_time);

      if (found_zeta_gateway.gateway_type() == ZETA) {
        ACA_LOG_INFO("%s", "AuxGateway_type is zeta!\n");
        // Update the zeta settings of vpc
        overall_rc = ACA_Zeta_Programming::get_instance().create_zeta_config(
                found_zeta_gateway, found_tunnel_id);
      }

      break;
    case OperationType::UPDATE:
      // only delete scenario is supported now
      // VM was created with port specified, then delete the VM
      // ACA will receive update with no device_id and device_owner
      ACA_LOG_INFO("%s", "Port update is not yet supported. \n");
      break;
      // [[fallthrough]];
    case OperationType::DELETE:
      // another delete scenario is supported here
      // VM was created with without port specified, then delete the VM
      // ACA will receive port operation delete
      ACA_LOG_DEBUG("Port Operation: %s: port_id: %s vpc_id:%s, network_type:%d, "
                    "generated_port_name: %s, tunnel_id: %d\n",
                    aca_get_operation_string(current_PortState.operation_type()),
                    current_PortConfiguration.id().c_str(),
                    current_PortConfiguration.vpc_id().c_str(), found_network_type,
                    generated_port_name.c_str(), found_tunnel_id);

      overall_rc = ACA_OVS_L2_Programmer::get_instance().delete_port(
              current_PortConfiguration.vpc_id(), generated_port_name,
              found_tunnel_id, culminative_dataplane_programming_time);

      if (found_zeta_gateway.gateway_type() == ZETA) {
        ACA_LOG_INFO("%s", "AuxGateway_type is zeta!\n");
        // Delete the zeta settings of vpc
        overall_rc = ACA_Zeta_Programming::get_instance().delete_zeta_config(
                found_zeta_gateway, found_tunnel_id);
      }

      break;

    default:
      ACA_LOG_ERROR("Invalid port state operation type %d\n",
                    current_PortState.operation_type());
      overall_rc = -EXIT_FAILURE;
      break;
    }

  } catch (const std::invalid_argument &e) {
    ACA_LOG_ERROR("Invalid argument exception caught while parsing port configuration, message: %s.\n",
                  e.what());
    overall_rc = -EINVAL;
  } catch (const std::exception &e) {
    ACA_LOG_ERROR("Exception caught while parsing port configuration, message: %s.\n",
                  e.what());
    overall_rc = -EFAULT;
  } catch (...) {
    ACA_LOG_CRIT("%s", "Unknown exception caught while parsing port configuration.\n");
    overall_rc = -EFAULT;
  }

EXIT:

  auto operation_end = chrono::steady_clock::now();

  auto operation_total_time =
          cast_to_microseconds(operation_end - operation_start).count();

  aca_goal_state_handler::Aca_Goal_State_Handler::get_instance().add_goal_state_operation_status(
          gsOperationReply, current_PortConfiguration.id(), ResourceType::PORT,
          current_PortState.operation_type(), overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Successfully configured the port state.\n");
  } else if (overall_rc == EINPROGRESS) {
    ACA_LOG_INFO("Port state returned pending: rc=%d\n", overall_rc);
  } else {
    ACA_LOG_ERROR("Unable to configure the port state: rc=%d\n", overall_rc);
  }

  return overall_rc;
}

int ACA_Dataplane_OVS::update_port_state_workitem(const PortState current_PortState,
                                                  GoalStateV2 &parsed_struct,
                                                  GoalStateOperationReply &gsOperationReply)
{
  int overall_rc;
  string generated_port_name;
  struct sockaddr_in sa;
  uint found_tunnel_id;
  NetworkType found_network_type;
  string found_prefix_len;
  string virtual_ip_address;
  string virtual_mac_address;
  string host_ip_address;
  string port_cidr;
  GatewayConfiguration found_zeta_gateway;
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;

  auto operation_start = chrono::steady_clock::now();

  PortConfiguration current_PortConfiguration = current_PortState.configuration();

  try {
    // TODO: need to design the usage of current_PortConfiguration.revision_number()
    assert(current_PortConfiguration.revision_number() > 0);

    // TODO: handle current_PortConfiguration.admin_state_up = false
    // need to look into the meaning of that during nova integration
    assert(current_PortConfiguration.admin_state_up() == true);

    assert(!current_PortConfiguration.id().empty());

    generated_port_name = aca_get_port_name(current_PortConfiguration.id());

    if (!aca_validate_fixed_ips_size(current_PortConfiguration.fixed_ips_size())) {
      throw std::invalid_argument("PortConfiguration.fixed_ips_size is less than zero");
    }

    // lookup subnet information
    auto subnetStateFound = parsed_struct.subnet_states().find(
            current_PortConfiguration.fixed_ips(0).subnet_id());

    if (subnetStateFound != parsed_struct.subnet_states().end()) {
      SubnetState current_SubnetState = subnetStateFound->second;
      SubnetConfiguration current_SubnetConfiguration =
              current_SubnetState.configuration();

      ACA_LOG_DEBUG("current_SubnetConfiguration subnet ID: %s.\n",
                    current_SubnetConfiguration.id().c_str());

      found_network_type = current_SubnetConfiguration.network_type();
      found_tunnel_id = current_SubnetConfiguration.tunnel_id();
      if (!aca_validate_tunnel_id(found_tunnel_id, found_network_type)) {
        throw std::invalid_argument("found_tunnel_id is invalid");
      }

      string found_cidr = current_SubnetConfiguration.cidr();
      size_t slash_pos = found_cidr.find('/');
      if (slash_pos == string::npos) {
        throw std::invalid_argument("'/' not found in cidr");
      }

      // substr can throw out_of_range and bad_alloc exceptions
      found_prefix_len = found_cidr.substr(slash_pos + 1);
    } else {
      ACA_LOG_ERROR("Not able to find the info for port with subnet ID: %s.\n",
                    current_PortConfiguration.fixed_ips(0).subnet_id().c_str());
      overall_rc = -EXIT_FAILURE;
    }

    // lookup VPC information to see if it is zeta supported
    auto vpcStateFound =
            parsed_struct.vpc_states().find(current_PortConfiguration.vpc_id());

    if (vpcStateFound != parsed_struct.vpc_states().end()) {
      VpcState current_VpcState = vpcStateFound->second;
      VpcConfiguration current_VpcConfiguration = current_VpcState.configuration();

      ACA_LOG_DEBUG("current_VpcConfiguration VPC ID: %s.\n",
                    current_VpcConfiguration.id().c_str());

      for (int j = 0; j < current_VpcConfiguration.gateway_ids_size(); j++) {
        auto gatewayStateFound = parsed_struct.gateway_states().find(
                current_VpcConfiguration.gateway_ids(j));

        if (gatewayStateFound != parsed_struct.gateway_states().end()) {
          GatewayState current_GatewayState = gatewayStateFound->second;
          auto gateway_type = current_GatewayState.configuration().gateway_type();

          if (gateway_type == ZETA) {
            found_zeta_gateway = current_GatewayState.configuration();
            break;
          }
        }
      }
    }

    switch (current_PortState.operation_type()) {
    case OperationType::CREATE:
      if (current_PortConfiguration.update_type() != UpdateType::FULL) {
        throw std::invalid_argument("current_PortConfiguration.update_type should be UpdateType::FULL");
      }

      virtual_ip_address = current_PortConfiguration.fixed_ips(0).ip_address();

      // inet_pton returns 1 for success 0 for failure
      if (inet_pton(AF_INET, virtual_ip_address.c_str(), &(sa.sin_addr)) != 1) {
        throw std::invalid_argument("Virtual ip address is not in the expect format");
      }

      virtual_mac_address = current_PortConfiguration.mac_address();
      if (!aca_validate_mac_address(virtual_mac_address.c_str())) {
        throw std::invalid_argument("virtual_mac_address is invalid");
      }

      port_cidr = virtual_ip_address + "/" + found_prefix_len;

      ACA_LOG_DEBUG("Port Operation: %s: port_id: %s, vpc_id:%s, network_type:%d, "
                    "virtual_ip_address:%s, virtual_mac_address:%s, generated_port_name: %s, port_cidr: %s, tunnel_id: %d\n",
                    aca_get_operation_string(current_PortState.operation_type()),
                    current_PortConfiguration.id().c_str(),
                    current_PortConfiguration.vpc_id().c_str(), found_network_type,
                    virtual_ip_address.c_str(), virtual_mac_address.c_str(),
                    generated_port_name.c_str(), port_cidr.c_str(), found_tunnel_id);

      overall_rc = ACA_OVS_L2_Programmer::get_instance().create_port(
              current_PortConfiguration.vpc_id(), generated_port_name, port_cidr,
              virtual_mac_address, found_tunnel_id, culminative_dataplane_programming_time);

      if (found_zeta_gateway.gateway_type() == ZETA) {
        ACA_LOG_INFO("%s", "AuxGateway_type is zeta!\n");
        // Update the zeta settings of vpc
        overall_rc = ACA_Zeta_Programming::get_instance().create_zeta_config(
                found_zeta_gateway, found_tunnel_id);
      }

      break;

    case OperationType::UPDATE:
      // only delete scenario is supported now
      // VM was created with port specified, then delete the VM
      // ACA will receive update with no device_id and device_owner
      if (!current_PortConfiguration.device_id().empty() ||
          !current_PortConfiguration.device_owner().empty()) {
        throw std::invalid_argument("Port Operation: update but device_id or device_owner not empty");
      }
      [[fallthrough]];
    case OperationType::DELETE:
      // another delete scenario is supported here
      // VM was created with without port specified, then delete the VM
      // ACA will receive port operation delete
      ACA_LOG_DEBUG("Port Operation: %s: port_id: %s vpc_id:%s, network_type:%d, "
                    "generated_port_name: %s, tunnel_id: %d\n",
                    aca_get_operation_string(current_PortState.operation_type()),
                    current_PortConfiguration.id().c_str(),
                    current_PortConfiguration.vpc_id().c_str(), found_network_type,
                    generated_port_name.c_str(), found_tunnel_id);

      overall_rc = ACA_OVS_L2_Programmer::get_instance().delete_port(
              current_PortConfiguration.vpc_id(), generated_port_name,
              found_tunnel_id, culminative_dataplane_programming_time);

      if (found_zeta_gateway.gateway_type() == ZETA) {
        ACA_LOG_INFO("%s", "AuxGateway_type is zeta!\n");
        // Delete the zeta settings of vpc
        overall_rc = ACA_Zeta_Programming::get_instance().delete_zeta_config(
                found_zeta_gateway, found_tunnel_id);
      }

      break;

    default:
      ACA_LOG_ERROR("Invalid port state operation type %d\n",
                    current_PortState.operation_type());
      overall_rc = -EXIT_FAILURE;
      break;
    }

  } catch (const std::invalid_argument &e) {
    ACA_LOG_ERROR("Invalid argument exception caught while parsing port configuration, message: %s.\n",
                  e.what());
    overall_rc = -EINVAL;
  } catch (const std::exception &e) {
    ACA_LOG_ERROR("Exception caught while parsing port configuration, message: %s.\n",
                  e.what());
    overall_rc = -EFAULT;
  } catch (...) {
    ACA_LOG_CRIT("%s", "Unknown exception caught while parsing port configuration.\n");
    overall_rc = -EFAULT;
  }

  auto operation_end = chrono::steady_clock::now();

  auto operation_total_time =
          cast_to_microseconds(operation_end - operation_start).count();

  aca_goal_state_handler::Aca_Goal_State_Handler::get_instance().add_goal_state_operation_status(
          gsOperationReply, current_PortConfiguration.id(), ResourceType::PORT,
          current_PortState.operation_type(), overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Successfully configured the port state.\n");
  } else if (overall_rc == EINPROGRESS) {
    ACA_LOG_INFO("Port state returned pending: rc=%d\n", overall_rc);
  } else {
    ACA_LOG_ERROR("Unable to configure the port state: rc=%d\n", overall_rc);
  }

  return overall_rc;
}

int ACA_Dataplane_OVS::update_neighbor_state_workitem(NeighborState current_NeighborState,
                                                      GoalState &parsed_struct,
                                                      GoalStateOperationReply &gsOperationReply)
{
  int overall_rc;
  struct sockaddr_in sa;
  string virtual_ip_address;
  string virtual_mac_address;
  string host_ip_address;
  NetworkType found_network_type;
  uint found_tunnel_id = 0;
  string found_gateway_mac;
  bool subnet_info_found = false;
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;

  auto operation_start = chrono::steady_clock::now();

  NeighborConfiguration current_NeighborConfiguration =
          current_NeighborState.configuration();

  try {
    if (!aca_validate_fixed_ips_size(current_NeighborConfiguration.fixed_ips_size())) {
      throw std::invalid_argument("NeighborConfiguration.fixed_ips_size is less than zero");
    }

    // TODO: need to design the usage of current_NeighborConfiguration.revision_number()
    assert(current_NeighborConfiguration.revision_number() > 0);

    for (int ip_index = 0;
         ip_index < current_NeighborConfiguration.fixed_ips_size(); ip_index++) {
      auto current_fixed_ip = current_NeighborConfiguration.fixed_ips(ip_index);

      if (current_fixed_ip.neighbor_type() == NeighborType::L2 ||
          current_fixed_ip.neighbor_type() == NeighborType::L3) {
        virtual_ip_address = current_fixed_ip.ip_address();

        // inet_pton returns 1 for success 0 for failure
        if (inet_pton(AF_INET, virtual_ip_address.c_str(), &(sa.sin_addr)) != 1) {
          throw std::invalid_argument("Virtual ip address is not in the expect format");
        }

        virtual_mac_address = current_NeighborConfiguration.mac_address();
        if (!aca_validate_mac_address(virtual_mac_address.c_str())) {
          throw std::invalid_argument("virtual_mac_address is invalid");
        }

        host_ip_address = current_NeighborConfiguration.host_ip_address();

        // inet_pton returns 1 for success 0 for failure
        if (inet_pton(AF_INET, host_ip_address.c_str(), &(sa.sin_addr)) != 1) {
          throw std::invalid_argument("Neighbor host ip address is not in the expect format");
        }

        // Look up the subnet configuration to query for tunnel_id
        for (int j = 0; j < parsed_struct.subnet_states_size(); j++) {
          SubnetConfiguration current_SubnetConfiguration =
                  parsed_struct.subnet_states(j).configuration();

          ACA_LOG_DEBUG("current_SubnetConfiguration subnet ID: %s.\n",
                        current_SubnetConfiguration.id().c_str());

          if (parsed_struct.subnet_states(j).operation_type() == OperationType::INFO) {
            if (current_SubnetConfiguration.id() == current_fixed_ip.subnet_id()) {
              found_network_type = current_SubnetConfiguration.network_type();

              found_tunnel_id = current_SubnetConfiguration.tunnel_id();
              if (!aca_validate_tunnel_id(found_tunnel_id, found_network_type)) {
                throw std::invalid_argument("found_tunnel_id is invalid");
              }

              subnet_info_found = true;
              break;
            }
          }
        }

        if (!subnet_info_found) {
          ACA_LOG_ERROR("Not able to find the info for neighbor ip_index: %d with subnet ID: %s.\n",
                        ip_index, current_fixed_ip.subnet_id().c_str());
          overall_rc = -EXIT_FAILURE;
        } else {
          ACA_LOG_DEBUG(
                  "Neighbor Operation:%s: id: %s, neighbor_type:%s, vpc_id:%s, network_type:%d, ip_index: %d,"
                  "virtual_ip_address:%s, virtual_mac_address:%s, neighbor_host_ip_address:%s, tunnel_id:%d\n",
                  aca_get_operation_string(current_NeighborState.operation_type()),
                  current_NeighborConfiguration.id().c_str(),
                  aca_get_neighbor_type_string(current_fixed_ip.neighbor_type()),
                  current_NeighborConfiguration.vpc_id().c_str(), found_network_type,
                  ip_index, virtual_ip_address.c_str(), virtual_mac_address.c_str(),
                  host_ip_address.c_str(), found_tunnel_id);

          // with Alcor DVR, a cross subnet packet will be routed to the destination subnet.
          // that means a L3 neighbor will become a L2 neighbor, therefore, call the below
          // for both L2 and L3 neighbor update

          // only need to update L2 neighbor info if it is not on the same compute host
          bool is_neighbor_port_on_same_host = aca_is_port_on_same_host(host_ip_address);

          if (is_neighbor_port_on_same_host) {
            ACA_LOG_DEBUG("neighbor host: %s is on the same compute node, don't need to update L2 neighbor info.\n",
                          host_ip_address.c_str());
            overall_rc = EXIT_SUCCESS;
          } else {
            if ((current_NeighborState.operation_type() == OperationType::CREATE) ||
                (current_NeighborState.operation_type() == OperationType::UPDATE) ||
                (current_NeighborState.operation_type() == OperationType::INFO)) {
              overall_rc = ACA_OVS_L2_Programmer::get_instance().create_or_update_l2_neighbor(
                      virtual_ip_address, virtual_mac_address, host_ip_address,
                      found_tunnel_id, culminative_dataplane_programming_time);
              // we can consider doing this L2 neighbor creation as an on demand rule to support scale
              // when we are ready to put the DVR rule as on demand, we should put the L2 neighbor rule
              // as on demand also
            } else if (current_NeighborState.operation_type() == OperationType::DELETE) {
              overall_rc = ACA_OVS_L2_Programmer::get_instance().delete_l2_neighbor(
                      virtual_ip_address, virtual_mac_address, found_tunnel_id,
                      culminative_dataplane_programming_time);
            } else {
              ACA_LOG_ERROR("Invalid neighbor state operation type %d\n",
                            current_NeighborState.operation_type());
              overall_rc = -EXIT_FAILURE;
            }
          }

          if (overall_rc == EXIT_SUCCESS) {
            if (current_fixed_ip.neighbor_type() == NeighborType::L3) {
              if ((current_NeighborState.operation_type() == OperationType::CREATE) ||
                  (current_NeighborState.operation_type() == OperationType::UPDATE) ||
                  (current_NeighborState.operation_type() == OperationType::INFO)) {
                overall_rc = ACA_OVS_L3_Programmer::get_instance().create_or_update_l3_neighbor(
                        current_NeighborConfiguration.id(),
                        current_NeighborConfiguration.vpc_id(),
                        current_fixed_ip.subnet_id(), virtual_ip_address,
                        virtual_mac_address, host_ip_address, found_tunnel_id,
                        culminative_dataplane_programming_time);
              } else if (current_NeighborState.operation_type() == OperationType::DELETE) {
                overall_rc = ACA_OVS_L3_Programmer::get_instance().delete_l3_neighbor(
                        current_NeighborConfiguration.id(), current_fixed_ip.subnet_id(),
                        virtual_ip_address, culminative_dataplane_programming_time);
              } else {
                ACA_LOG_ERROR("Invalid neighbor state operation type %d\n",
                              current_NeighborState.operation_type());
                overall_rc = -EXIT_FAILURE;
              }
            }
          }
        }
      } else {
        ACA_LOG_ERROR("Unknown neighbor_type: %d.\n",
                      current_NeighborState.operation_type());
        overall_rc = -EINVAL;
      }

    } // for (int ip_index = 0; ip_index < current_NeighborConfiguration.fixed_ips_size(); ip_index++)

  } catch (const std::invalid_argument &e) {
    ACA_LOG_ERROR("Invalid argument exception caught while parsing neighbor configuration, message: %s.\n",
                  e.what());
    overall_rc = -EINVAL;
  } catch (const std::exception &e) {
    ACA_LOG_ERROR("Exception caught while parsing neighbor configuration, message: %s.\n",
                  e.what());
    overall_rc = -EFAULT;
  } catch (...) {
    ACA_LOG_CRIT("%s", "Unknown exception caught while parsing neighbor configuration.\n");
    overall_rc = -EFAULT;
  }

  auto operation_end = chrono::steady_clock::now();

  auto operation_total_time =
          cast_to_microseconds(operation_end - operation_start).count();

  aca_goal_state_handler::Aca_Goal_State_Handler::get_instance().add_goal_state_operation_status(
          gsOperationReply, current_NeighborConfiguration.id(),
          ResourceType::NEIGHBOR, current_NeighborState.operation_type(),
          overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Successfully configured the neighbor state.\n");
  } else {
    ACA_LOG_ERROR("Unable to configure the neighbor state: rc=%d\n", overall_rc);
  }

  return overall_rc;
}

int ACA_Dataplane_OVS::update_neighbor_state_workitem(NeighborState current_NeighborState,
                                                      GoalStateV2 &parsed_struct,
                                                      GoalStateOperationReply &gsOperationReply)
{
  int overall_rc;
  struct sockaddr_in sa;
  string virtual_ip_address;
  string virtual_mac_address;
  string host_ip_address;
  NetworkType found_network_type;
  uint found_tunnel_id = 0;
  string found_gateway_mac;
  bool subnet_info_found = false;
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;

  auto operation_start = chrono::steady_clock::now();

  NeighborConfiguration current_NeighborConfiguration =
          current_NeighborState.configuration();

  try {
    if (!aca_validate_fixed_ips_size(current_NeighborConfiguration.fixed_ips_size())) {
      throw std::invalid_argument("NeighborConfiguration.fixed_ips_size is less than zero");
    }

    // TODO: need to design the usage of current_NeighborConfiguration.revision_number()
    assert(current_NeighborConfiguration.revision_number() > 0);

    for (int ip_index = 0;
         ip_index < current_NeighborConfiguration.fixed_ips_size(); ip_index++) {
      auto current_fixed_ip = current_NeighborConfiguration.fixed_ips(ip_index);

      if (current_fixed_ip.neighbor_type() == NeighborType::L2 ||
          current_fixed_ip.neighbor_type() == NeighborType::L3) {
        virtual_ip_address = current_fixed_ip.ip_address();

        // inet_pton returns 1 for success 0 for failure
        if (inet_pton(AF_INET, virtual_ip_address.c_str(), &(sa.sin_addr)) != 1) {
          throw std::invalid_argument("Virtual ip address is not in the expect format");
        }

        virtual_mac_address = current_NeighborConfiguration.mac_address();
        if (!aca_validate_mac_address(virtual_mac_address.c_str())) {
          throw std::invalid_argument("virtual_mac_address is invalid");
        }

        host_ip_address = current_NeighborConfiguration.host_ip_address();

        // inet_pton returns 1 for success 0 for failure
        if (inet_pton(AF_INET, host_ip_address.c_str(), &(sa.sin_addr)) != 1) {
          throw std::invalid_argument("Neighbor host ip address is not in the expect format");
        }

        // Look up the subnet configuration to query for network type and tunnel_id
        auto subnetStateFound =
                parsed_struct.subnet_states().find(current_fixed_ip.subnet_id());

        if (subnetStateFound != parsed_struct.subnet_states().end()) {
          SubnetState current_SubnetState = subnetStateFound->second;
          SubnetConfiguration current_SubnetConfiguration =
                  current_SubnetState.configuration();

          ACA_LOG_DEBUG("current_SubnetConfiguration subnet ID: %s.\n",
                        current_SubnetConfiguration.id().c_str());

          found_network_type = current_SubnetConfiguration.network_type();

          found_tunnel_id = current_SubnetConfiguration.tunnel_id();
          if (!aca_validate_tunnel_id(found_tunnel_id, found_network_type)) {
            throw std::invalid_argument("found_tunnel_id is invalid");
          }

          subnet_info_found = true;
        }

        if (!subnet_info_found) {
          ACA_LOG_ERROR("Not able to find the info for neighbor ip_index: %d with subnet ID: %s.\n",
                        ip_index, current_fixed_ip.subnet_id().c_str());
          overall_rc = -EXIT_FAILURE;
        } else {
          ACA_LOG_DEBUG(
                  "Neighbor Operation:%s: id: %s, neighbor_type:%s, vpc_id:%s, network_type:%d, ip_index: %d,"
                  "virtual_ip_address:%s, virtual_mac_address:%s, neighbor_host_ip_address:%s, tunnel_id:%d\n",
                  aca_get_operation_string(current_NeighborState.operation_type()),
                  current_NeighborConfiguration.id().c_str(),
                  aca_get_neighbor_type_string(current_fixed_ip.neighbor_type()),
                  current_NeighborConfiguration.vpc_id().c_str(), found_network_type,
                  ip_index, virtual_ip_address.c_str(), virtual_mac_address.c_str(),
                  host_ip_address.c_str(), found_tunnel_id);

          // with Alcor DVR, a cross subnet packet will be routed to the destination subnet.
          // that means a L3 neighbor will become a L2 neighbor, therefore, call the below
          // for both L2 and L3 neighbor update

          // only need to update L2 neighbor info if it is not on the same compute host
          bool is_neighbor_port_on_same_host = aca_is_port_on_same_host(host_ip_address);

          if (is_neighbor_port_on_same_host) {
            ACA_LOG_DEBUG("neighbor host: %s is on the same compute node, don't need to update L2 neighbor info.\n",
                          host_ip_address.c_str());
            overall_rc = EXIT_SUCCESS;
          } else {
            if ((current_NeighborState.operation_type() == OperationType::CREATE) ||
                (current_NeighborState.operation_type() == OperationType::UPDATE) ||
                (current_NeighborState.operation_type() == OperationType::INFO)) {
              overall_rc = ACA_OVS_L2_Programmer::get_instance().create_or_update_l2_neighbor(
                      virtual_ip_address, virtual_mac_address, host_ip_address,
                      found_tunnel_id, culminative_dataplane_programming_time);
              // we can consider doing this L2 neighbor creation as an on demand rule to support scale
              // when we are ready to put the DVR rule as on demand, we should put the L2 neighbor rule
              // as on demand also
            } else if (current_NeighborState.operation_type() == OperationType::DELETE) {
              overall_rc = ACA_OVS_L2_Programmer::get_instance().delete_l2_neighbor(
                      virtual_ip_address, virtual_mac_address, found_tunnel_id,
                      culminative_dataplane_programming_time);
            } else {
              ACA_LOG_ERROR("Invalid neighbor state operation type %d\n",
                            current_NeighborState.operation_type());
              overall_rc = -EXIT_FAILURE;
            }
          }

          if (overall_rc == EXIT_SUCCESS) {
            if (current_fixed_ip.neighbor_type() == NeighborType::L3) {
              if ((current_NeighborState.operation_type() == OperationType::CREATE) ||
                  (current_NeighborState.operation_type() == OperationType::UPDATE) ||
                  (current_NeighborState.operation_type() == OperationType::INFO)) {
                overall_rc = ACA_OVS_L3_Programmer::get_instance().create_or_update_l3_neighbor(
                        current_NeighborConfiguration.id(),
                        current_NeighborConfiguration.vpc_id(),
                        current_fixed_ip.subnet_id(), virtual_ip_address,
                        virtual_mac_address, host_ip_address, found_tunnel_id,
                        culminative_dataplane_programming_time);
              } else if (current_NeighborState.operation_type() == OperationType::DELETE) {
                overall_rc = ACA_OVS_L3_Programmer::get_instance().delete_l3_neighbor(
                        current_NeighborConfiguration.id(), current_fixed_ip.subnet_id(),
                        virtual_ip_address, culminative_dataplane_programming_time);
              } else {
                ACA_LOG_ERROR("Invalid neighbor state operation type %d\n",
                              current_NeighborState.operation_type());
                overall_rc = -EXIT_FAILURE;
              }
            }
          }
        }
      } else {
        ACA_LOG_ERROR("Unknown neighbor_type: %d.\n",
                      current_NeighborState.operation_type());
        overall_rc = -EINVAL;
      }

    } // for (int ip_index = 0; ip_index < current_NeighborConfiguration.fixed_ips_size(); ip_index++)

  } catch (const std::invalid_argument &e) {
    ACA_LOG_ERROR("Invalid argument exception caught while parsing neighbor configuration, message: %s.\n",
                  e.what());
    overall_rc = -EINVAL;
  } catch (const std::exception &e) {
    ACA_LOG_ERROR("Exception caught while parsing neighbor configuration, message: %s.\n",
                  e.what());
    overall_rc = -EFAULT;
  } catch (...) {
    ACA_LOG_CRIT("%s", "Unknown exception caught while parsing neighbor configuration.\n");
    overall_rc = -EFAULT;
  }

  auto operation_end = chrono::steady_clock::now();

  auto operation_total_time =
          cast_to_microseconds(operation_end - operation_start).count();

  aca_goal_state_handler::Aca_Goal_State_Handler::get_instance().add_goal_state_operation_status(
          gsOperationReply, current_NeighborConfiguration.id(),
          ResourceType::NEIGHBOR, current_NeighborState.operation_type(),
          overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Successfully configured the neighbor state.\n");
  } else {
    ACA_LOG_ERROR("Unable to configure the neighbor state: rc=%d\n", overall_rc);
  }

  return overall_rc;
}

int ACA_Dataplane_OVS::update_router_state_workitem(RouterState current_RouterState,
                                                    GoalState &parsed_struct,
                                                    GoalStateOperationReply &gsOperationReply)
{
  int overall_rc;
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;

  auto operation_start = chrono::steady_clock::now();

  RouterConfiguration current_RouterConfiguration = current_RouterState.configuration();

  // TODO: need to design the usage of current_RouterConfiguration.revision_number()
  assert(current_RouterConfiguration.revision_number() > 0);

  switch (current_RouterState.operation_type()) {
  case OperationType::CREATE:
    [[fallthrough]];
  case OperationType::UPDATE:
    [[fallthrough]];
  case OperationType::INFO:
    overall_rc = ACA_OVS_L3_Programmer::get_instance().create_or_update_router(
            current_RouterConfiguration, parsed_struct, culminative_dataplane_programming_time);
    break;

  case OperationType::DELETE:
    overall_rc = ACA_OVS_L3_Programmer::get_instance().delete_router(
            current_RouterConfiguration, culminative_dataplane_programming_time);
    break;

  default:
    ACA_LOG_ERROR("Invalid router state operation type %d\n",
                  current_RouterState.operation_type());
    overall_rc = -EXIT_FAILURE;
    break;
  }

  auto operation_end = chrono::steady_clock::now();

  auto operation_total_time =
          cast_to_microseconds(operation_end - operation_start).count();

  aca_goal_state_handler::Aca_Goal_State_Handler::get_instance().add_goal_state_operation_status(
          gsOperationReply, current_RouterConfiguration.id(),
          ResourceType::ROUTER, current_RouterState.operation_type(),
          overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Successfully configured the router state.\n");
  } else {
    ACA_LOG_ERROR("Unable to configure the router state: rc=%d\n", overall_rc);
  }

  return overall_rc;
}

int ACA_Dataplane_OVS::update_router_state_workitem(RouterState current_RouterState,
                                                    GoalStateV2 &parsed_struct,
                                                    GoalStateOperationReply &gsOperationReply)
{
  int overall_rc;
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;

  auto operation_start = chrono::steady_clock::now();

  RouterConfiguration current_RouterConfiguration = current_RouterState.configuration();

  // TODO: need to design the usage of current_RouterConfiguration.revision_number()
  assert(current_RouterConfiguration.revision_number() > 0);

  switch (current_RouterState.operation_type()) {
  case OperationType::CREATE:
    [[fallthrough]];
  case OperationType::UPDATE:
    [[fallthrough]];
  case OperationType::INFO:
    overall_rc = ACA_OVS_L3_Programmer::get_instance().create_or_update_router(
            current_RouterConfiguration, parsed_struct, culminative_dataplane_programming_time);
    break;

  case OperationType::DELETE:
    overall_rc = ACA_OVS_L3_Programmer::get_instance().delete_router(
            current_RouterConfiguration, culminative_dataplane_programming_time);
    break;

  default:
    ACA_LOG_ERROR("Invalid router state operation type %d\n",
                  current_RouterState.operation_type());
    overall_rc = -EXIT_FAILURE;
    break;
  }

  auto operation_end = chrono::steady_clock::now();

  auto operation_total_time =
          cast_to_microseconds(operation_end - operation_start).count();

  aca_goal_state_handler::Aca_Goal_State_Handler::get_instance().add_goal_state_operation_status(
          gsOperationReply, current_RouterConfiguration.id(),
          ResourceType::ROUTER, current_RouterState.operation_type(),
          overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Successfully configured the router state.\n");
  } else {
    ACA_LOG_ERROR("Unable to configure the router state: rc=%d\n", overall_rc);
  }

  return overall_rc;
}

} // namespace aca_dataplane_ovs
