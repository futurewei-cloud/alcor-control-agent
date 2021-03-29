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

#include "aca_log.h"
#include "aca_util.h"
#include "aca_dhcp_server.h"
#include "aca_dhcp_state_handler.h"
#include "aca_goal_state_handler.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <future>
#include <string>

using namespace aca_dhcp_programming_if;
using namespace alcor::schema;

namespace aca_dhcp_state_handler
{
Aca_Dhcp_State_Handler::Aca_Dhcp_State_Handler()
{
  ACA_LOG_INFO("%s", "DHCP State Handler: using dhcp server\n");
  this->dhcp_programming_if = &(aca_dhcp_server::ACA_Dhcp_Server::get_instance());
}

Aca_Dhcp_State_Handler::~Aca_Dhcp_State_Handler()
{
  // allocated dhcp_programming_if is destroyed when program exits.
}

Aca_Dhcp_State_Handler &Aca_Dhcp_State_Handler::get_instance()
{
  // It is instantiated on first use.
  // allocated instance is destroyed when program exits.
  static Aca_Dhcp_State_Handler instance;
  return instance;
}

int Aca_Dhcp_State_Handler::update_dhcp_state_workitem(const DHCPState current_DhcpState,
                                                       GoalState &parsed_struct,
                                                       GoalStateOperationReply &gsOperationReply)
{
  dhcp_config stDhcpCfg;
  int overall_rc = EXIT_SUCCESS;
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;
  auto operation_start = chrono::steady_clock::now();
  DHCPConfiguration current_DhcpConfiguration = current_DhcpState.configuration();
  stDhcpCfg.mac_address = current_DhcpConfiguration.mac_address();
  stDhcpCfg.ipv4_address = current_DhcpConfiguration.ipv4_address();
  stDhcpCfg.ipv6_address = current_DhcpConfiguration.ipv6_address();
  stDhcpCfg.port_host_name = current_DhcpConfiguration.port_host_name();
  string subnet_id = current_DhcpConfiguration.subnet_id();
  for (int i = 0; i < parsed_struct.subnet_states_size(); i++) {
    SubnetState current_SubnetState = parsed_struct.subnet_states(i);
    SubnetConfiguration current_SubnetConfiguration = current_SubnetState.configuration();
    if (subnet_id == current_SubnetConfiguration.id()) {
      stDhcpCfg.gateway_address = current_SubnetConfiguration.gateway().ip_address();
      stDhcpCfg.subnet_mask =
              aca_convert_cidr_to_netmask(current_SubnetConfiguration.cidr());
      // handle dhcp dns entries
      for (int j = 0; j < current_SubnetConfiguration.dns_entry_list_size() && j < DHCP_MSG_OPTS_DNS_LENGTH;
           j++) {
        stDhcpCfg.dns_addresses[j] =
                current_SubnetConfiguration.dns_entry_list(j).entry();
      }
      break;
    }
  }
  alcor::schema::OperationType current_operation_type =
          current_DhcpState.operation_type();
  ACA_LOG_INFO("%s, port_states size: %d\n", "Searching in port states\n",
               parsed_struct.port_states().size());

  if (current_operation_type == alcor::schema::OperationType::UPDATE) {
    for (int i = 0; i < parsed_struct.port_states().size(); i++) {
      ACA_LOG_INFO(
              "Port %d MAC: %s, device_ID: %s, device_owner: %s\n", i,
              (parsed_struct.port_states(i).configuration().mac_address()).c_str(),
              (parsed_struct.port_states(i).configuration().device_id()).c_str(),
              (parsed_struct.port_states(i).configuration().device_owner()).c_str());
      if (parsed_struct.port_states(i).configuration().mac_address() ==
                  current_DhcpConfiguration.mac_address() &&
          !parsed_struct.port_states(i).configuration().device_id().empty() &&
          !parsed_struct.port_states(i).configuration().device_owner().empty()) {
        current_operation_type = alcor::schema::OperationType::CREATE;
        break;
      }
    }
  }
  /*
    In the current test environment, Nova may send DHCP UPDATE, when it tries to 
    CREATE a new port, we are now adding this logic to the ACA as a temporary 
    fix, if this logic is proved to be successful, this logic may be moved to the 
    Port Manger/Dataplane Manager.
  */
  switch (current_operation_type) {
  case OperationType::CREATE:
    overall_rc = this->dhcp_programming_if->add_dhcp_entry(&stDhcpCfg);
    break;
  case OperationType::UPDATE:
    overall_rc = this->dhcp_programming_if->update_dhcp_entry(&stDhcpCfg);
    break;
  case OperationType::DELETE:
    overall_rc = this->dhcp_programming_if->delete_dhcp_entry(&stDhcpCfg);
    break;
  default:
    ACA_LOG_ERROR("%s", "=====>wrong dhcp operation\n");
    overall_rc = EXIT_FAILURE;
    break;
  }
  auto operation_end = chrono::steady_clock::now();
  auto operation_total_time =
          cast_to_microseconds(operation_end - operation_start).count();
  aca_goal_state_handler::Aca_Goal_State_Handler::get_instance().add_goal_state_operation_status(
          gsOperationReply, current_DhcpConfiguration.id(), DHCP,
          current_DhcpState.operation_type(), overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);
  return overall_rc;
}

int Aca_Dhcp_State_Handler::update_dhcp_states(GoalState &parsed_struct,
                                               GoalStateOperationReply &gsOperationReply)
{
  std::vector<std::future<int> > workitem_future;
  int rc;
  int overall_rc = EXIT_SUCCESS;

  for (int i = 0; i < parsed_struct.dhcp_states_size(); i++) {
    ACA_LOG_DEBUG("=====>parsing dhcp states #%d\n", i);

    DHCPState current_DhcpState = parsed_struct.dhcp_states(i);

    workitem_future.push_back(std::async(
            std::launch::async, &Aca_Dhcp_State_Handler::update_dhcp_state_workitem, this,
            current_DhcpState, std::ref(parsed_struct), std::ref(gsOperationReply)));
  } // for (int i = 0; i < parsed_struct.dhcp_states_size(); i++)

  for (int i = 0; i < parsed_struct.dhcp_states_size(); i++) {
    rc = workitem_future[i].get();
    if (rc != EXIT_SUCCESS)
      overall_rc = rc;
  } // for (int i = 0; i < parsed_struct.dhcp_states_size(); i++)

  return overall_rc;
}

int Aca_Dhcp_State_Handler::update_dhcp_state_workitem_v2(const DHCPState current_DhcpState,
                                                          GoalStateV2 &parsed_struct,
                                                          GoalStateOperationReply &gsOperationReply)
{
  dhcp_config stDhcpCfg;
  int overall_rc = EXIT_SUCCESS;
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;
  auto operation_start = chrono::steady_clock::now();
  DHCPConfiguration current_DhcpConfiguration = current_DhcpState.configuration();
  stDhcpCfg.mac_address = current_DhcpConfiguration.mac_address();
  stDhcpCfg.ipv4_address = current_DhcpConfiguration.ipv4_address();
  stDhcpCfg.ipv6_address = current_DhcpConfiguration.ipv6_address();
  stDhcpCfg.port_host_name = current_DhcpConfiguration.port_host_name();
  string subnet_id = current_DhcpConfiguration.subnet_id();
  auto subnetStateFound = parsed_struct.subnet_states().find(subnet_id);
  if (subnetStateFound != parsed_struct.subnet_states().end()) {
    SubnetState current_SubnetState = subnetStateFound->second;
    SubnetConfiguration current_SubnetConfiguration = current_SubnetState.configuration();
    stDhcpCfg.gateway_address = current_SubnetConfiguration.gateway().ip_address();
    stDhcpCfg.subnet_mask =
            aca_convert_cidr_to_netmask(current_SubnetConfiguration.cidr());
    // handle dhcp dns entries
    for (int j = 0; j < current_SubnetConfiguration.dns_entry_list_size() && j < DHCP_MSG_OPTS_DNS_LENGTH;
         j++) {
      stDhcpCfg.dns_addresses[j] = current_SubnetConfiguration.dns_entry_list(j).entry();
    }
  } else {
    ACA_LOG_ERROR("Not able to find the info for DHCP with subnet ID: %s.\n",
                  subnet_id.c_str());
    overall_rc = EXIT_FAILURE;
  }
  alcor::schema::OperationType current_operation_type =
          current_DhcpState.operation_type();
  /*
    In the current test environment, Nova may send DHCP UPDATE, when it tries to 
    CREATE a new port, we are now adding this logic to the ACA as a temporary 
    fix, if this logic is proved to be successful, this logic may be moved to the 
    Port Manger/Dataplane Manager.
  */
  if (current_operation_type == alcor::schema::OperationType::UPDATE) {
    auto target_port_state =
            parsed_struct.port_states().find(current_DhcpConfiguration.mac_address());
    // target_port_state.first is the key (resource_id)
    // target_port_state.second is the value (PortState)
    if (target_port_state != parsed_struct.port_states().end() &&
        !target_port_state->second.configuration().device_id().empty() &&
        !target_port_state->second.configuration().device_owner().empty()) {
      current_operation_type = alcor::schema::OperationType::CREATE;
    }
  }
  if (overall_rc == EXIT_SUCCESS) {
    switch (current_operation_type) {
    case OperationType::CREATE:
      overall_rc = this->dhcp_programming_if->add_dhcp_entry(&stDhcpCfg);
      break;
    case OperationType::UPDATE:
      overall_rc = this->dhcp_programming_if->update_dhcp_entry(&stDhcpCfg);
      break;
    case OperationType::DELETE:
      overall_rc = this->dhcp_programming_if->delete_dhcp_entry(&stDhcpCfg);
      break;
    default:
      ACA_LOG_ERROR("%s", "=====>wrong dhcp operation\n");
      overall_rc = EXIT_FAILURE;
    }
  }
  auto operation_end = chrono::steady_clock::now();
  auto operation_total_time =
          cast_to_microseconds(operation_end - operation_start).count();
  aca_goal_state_handler::Aca_Goal_State_Handler::get_instance().add_goal_state_operation_status(
          gsOperationReply, current_DhcpConfiguration.id(), DHCP,
          current_DhcpState.operation_type(), overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);
  return overall_rc;
}

int Aca_Dhcp_State_Handler::update_dhcp_states(GoalStateV2 &parsed_struct,
                                               GoalStateOperationReply &gsOperationReply)
{
  std::vector<std::future<int> > workitem_future;
  int rc;
  int overall_rc = EXIT_SUCCESS;

  for (auto &[dhcp_id, current_DhcpState] : parsed_struct.dhcp_states()) {
    ACA_LOG_DEBUG("=====>parsing dhcp state: %s\n", dhcp_id.c_str());

    workitem_future.push_back(std::async(
            std::launch::async, &Aca_Dhcp_State_Handler::update_dhcp_state_workitem_v2, this,
            current_DhcpState, std::ref(parsed_struct), std::ref(gsOperationReply)));
  } // for (int i = 0; i < parsed_struct.dhcp_states_size(); i++)

  for (int i = 0; i < parsed_struct.dhcp_states_size(); i++) {
    rc = workitem_future[i].get();
    if (rc != EXIT_SUCCESS)
      overall_rc = rc;
  } // for (int i = 0; i < parsed_struct.dhcp_states_size(); i++)

  return overall_rc;
}

} // namespace aca_dhcp_state_handler
