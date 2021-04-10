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

  switch (current_DhcpState.operation_type()) {
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

  if (overall_rc == EXIT_SUCCESS) {
    switch (current_DhcpState.operation_type()) {
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
