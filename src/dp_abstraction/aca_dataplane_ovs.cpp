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

#include "aca_dataplane_ovs.h"
#include "aca_goal_state_handler.h"
#include "aca_ovs_programmer.h"
#include "aca_net_config.h"
#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_util.h"
#include <errno.h>
#include <arpa/inet.h>

using namespace std;
using namespace alcor::schema;
using aca_ovs_programmer::ACA_OVS_Programmer;

static void aca_validate_mac_address(const char *mac_string)
{
  unsigned char mac[6];

  if (mac_string == nullptr) {
    throw std::invalid_argument("Input mac_string is null");
  }

  if (sscanf(mac_string, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1],
             &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
    return;
  }

  if (sscanf(mac_string, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx", &mac[0], &mac[1],
             &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
    return;
  }

  // nothing matched
  ACA_LOG_ERROR("Invalid mac address: %s\n", mac_string);

  throw std::invalid_argument("Input mac_string is not in the expect format");
}

static void aca_validate_tunnel_id(const uint tunnel_id)
{
  uint MAX_VALID_VNI = 16777215;

  if (tunnel_id == 0) {
    throw std::invalid_argument("Input tunnel_id is 0");
  }

  if (tunnel_id > MAX_VALID_VNI) {
    throw std::invalid_argument("Input tunnel_id is greater than valid maximun " +
                                to_string(MAX_VALID_VNI));
  }
}

namespace aca_dataplane_ovs
{
int ACA_Dataplane_OVS::initialize()
{
  // TODO: improve the logging system, and add logging to this module
  return ACA_OVS_Programmer::get_instance().setup_ovs_bridges_if_need();
}

int ACA_Dataplane_OVS::update_vpc_state_workitem(const VpcState current_VpcState,
                                                 GoalStateOperationReply &gsOperationReply)
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
          cast_to_nanoseconds(operation_end - operation_start).count();

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
  string port_name;
  struct sockaddr_in sa;
  string found_cidr;
  uint found_tunnel_id;
  size_t slash_pos;
  string virtual_ip_address;
  string virtual_mac_address;
  string host_ip_address;
  string found_prefix_len;
  bool subnet_info_found = false;
  string port_cidr;
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;

  auto operation_start = chrono::steady_clock::now();

  PortConfiguration current_PortConfiguration = current_PortState.configuration();

  assert(current_PortConfiguration.format_version() > 0);

  // TODO: need to design the usage of current_PortConfiguration.revision_number()
  assert(current_PortConfiguration.revision_number() > 0);

  // TODO: handle current_PortConfiguration.admin_state_up = false
  // need to look into the meaning of that during nova integration
  assert(current_PortConfiguration.admin_state_up() == true);

  switch (current_PortState.operation_type()) {
  case OperationType::CREATE:

    assert(current_PortConfiguration.message_type() == MessageType::FULL);

    assert(!current_PortConfiguration.id().empty());

    port_name = aca_get_port_name(current_PortConfiguration.id());

    try {
      // TODO: add support for more than one fixed_ips in the future
      assert(current_PortConfiguration.fixed_ips_size() == 1);
      virtual_ip_address = current_PortConfiguration.fixed_ips(0).ip_address();

      // inet_pton returns 1 for success 0 for failure
      if (inet_pton(AF_INET, virtual_ip_address.c_str(), &(sa.sin_addr)) != 1) {
        throw std::invalid_argument("Virtual ip address is not in the expect format");
      }

      virtual_mac_address = current_PortConfiguration.mac_address();
      // the below will throw invalid_argument exceptions if mac string is invalid
      aca_validate_mac_address(virtual_mac_address.c_str());

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
          if (current_SubnetConfiguration.id() ==
              current_PortConfiguration.fixed_ips(0).subnet_id()) {
            found_tunnel_id = current_SubnetConfiguration.tunnel_id();

            aca_validate_tunnel_id(found_tunnel_id);

            found_cidr = current_SubnetConfiguration.cidr();

            slash_pos = found_cidr.find('/');
            if (slash_pos == string::npos) {
              throw std::invalid_argument("'/' not found in cidr");
            }

            // substr can throw out_of_range and bad_alloc exceptions
            found_prefix_len = found_cidr.substr(slash_pos + 1);
          }
          subnet_info_found = true;
          break;
        }
      }

      if (!subnet_info_found) {
        ACA_LOG_ERROR("Not able to find the info for port with subnet ID: %s.\n",
                      current_PortConfiguration.fixed_ips(0).subnet_id().c_str());
        overall_rc = -EXIT_FAILURE;
      } else {
        overall_rc = EXIT_SUCCESS;
      }

      port_cidr = virtual_ip_address + "/" + found_prefix_len;

      if (overall_rc == EXIT_SUCCESS) {
        ACA_LOG_DEBUG("Port Operation: %s: port_id: %s, project_id:%s, vpc_id:%s, network_type:%d, "
                      "virtual_ip_address:%s, virtual_mac_address:%s, port_name: %s, port_cidr: %s, tunnel_id: %d\n",
                      aca_get_operation_string(current_PortState.operation_type()),
                      current_PortConfiguration.id().c_str(),
                      current_PortConfiguration.project_id().c_str(),
                      current_PortConfiguration.vpc_id().c_str(),
                      current_PortConfiguration.network_type(),
                      virtual_ip_address.c_str(), virtual_mac_address.c_str(),
                      port_name.c_str(), port_cidr.c_str(), found_tunnel_id);

        overall_rc = ACA_OVS_Programmer::get_instance().configure_port(
                current_PortConfiguration.vpc_id(), port_name, port_cidr,
                found_tunnel_id, culminative_dataplane_programming_time);
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
      ACA_LOG_ERROR("Unknown exception caught while parsing port configuration, rethrowing.\n");
      throw; // rethrowing
    }

    break;

  case OperationType::NEIGHBOR_CREATE_UPDATE:

    assert(current_PortConfiguration.message_type() == MessageType::DELTA);

    try {
      assert(current_PortConfiguration.fixed_ips_size() == 1);
      virtual_ip_address = current_PortConfiguration.fixed_ips(0).ip_address();

      // inet_pton returns 1 for success 0 for failure
      if (inet_pton(AF_INET, virtual_ip_address.c_str(), &(sa.sin_addr)) != 1) {
        throw std::invalid_argument("Virtual ip address is not in the expect format");
      }

      virtual_mac_address = current_PortConfiguration.mac_address();
      // the below will throw invalid_argument exceptions if mac string is invalid
      aca_validate_mac_address(virtual_mac_address.c_str());

      host_ip_address = current_PortConfiguration.host_info().ip_address();

      // inet_pton returns 1 for success 0 for failure
      if (inet_pton(AF_INET, host_ip_address.c_str(), &(sa.sin_addr)) != 1) {
        throw std::invalid_argument("Neighbor host ip address is not in the expect format");
      }

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
          if (current_SubnetConfiguration.id() ==
              current_PortConfiguration.fixed_ips(0).subnet_id()) {
            found_tunnel_id = current_SubnetConfiguration.tunnel_id();

            aca_validate_tunnel_id(found_tunnel_id);
          }
          subnet_info_found = true;
          break;
        }
      }

      if (!subnet_info_found) {
        ACA_LOG_ERROR("Not able to find the info for port with subnet ID: %s.\n",
                      current_PortConfiguration.fixed_ips(0).subnet_id().c_str());
        overall_rc = -EXIT_FAILURE;
      } else {
        overall_rc = EXIT_SUCCESS;
      }

      if (overall_rc == EXIT_SUCCESS) {
        ACA_LOG_DEBUG("Port Operation:%s: project_id:%s, vpc_id:%s, network_type:%d, virtual_ip_address:%s, "
                      "virtual_mac_address:%s, neighbor_host_ip_address:%s, tunnel_id:%d\n ",
                      aca_get_operation_string(current_PortState.operation_type()),
                      current_PortConfiguration.project_id().c_str(),
                      current_PortConfiguration.vpc_id().c_str(),
                      current_PortConfiguration.network_type(),
                      virtual_ip_address.c_str(), virtual_mac_address.c_str(),
                      host_ip_address.c_str(), found_tunnel_id);

        overall_rc = ACA_OVS_Programmer::get_instance().create_update_neighbor_port(
                current_PortConfiguration.vpc_id(),
                current_PortConfiguration.network_type(), host_ip_address,
                found_tunnel_id, culminative_dataplane_programming_time);
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
      ACA_LOG_ERROR("Unknown exception caught while parsing port configuration, rethrowing.\n");
      throw; // rethrowing
    }

    break;

  default:
    ACA_LOG_ERROR("Invalid port state operation type %d\n",
                  current_PortState.operation_type());
    overall_rc = -EXIT_FAILURE;
    break;
  }

  auto operation_end = chrono::steady_clock::now();

  auto operation_total_time =
          cast_to_nanoseconds(operation_end - operation_start).count();

  aca_goal_state_handler::Aca_Goal_State_Handler::get_instance().add_goal_state_operation_status(
          gsOperationReply, current_PortConfiguration.id(), PORT,
          current_PortState.operation_type(), overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("Successfully configured the port state.\n");
  } else {
    ACA_LOG_ERROR("Unable to configure the port state: rc=%d\n", overall_rc);
  }

  return overall_rc;
}

} // namespace aca_dataplane_ovs
