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
#include "aca_comm_mgr.h"
#include "aca_goal_state_handler.h"
#include "aca_dhcp_state_handler.h"
#include "goalstateprovisioner.grpc.pb.h"

using namespace std;
using namespace alcor::schema;
using namespace aca_goal_state_handler;
using namespace aca_dhcp_state_handler;

extern string g_rpc_server;
extern string g_rpc_protocol;
extern std::atomic_ulong g_total_update_GS_time;

namespace aca_comm_manager
{
Aca_Comm_Manager &Aca_Comm_Manager::get_instance()
{
  // It is instantiated on first use.
  // Allocated instance is destroyed when program exits.
  static Aca_Comm_Manager instance;
  return instance;
}

int Aca_Comm_Manager::deserialize(const cppkafka::Buffer *kafka_buffer, GoalState &parsed_struct)
{
  int rc;

  if (kafka_buffer->get_data() == NULL) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Empty kafka kafka_buffer data rc: %d\n", rc);
    return rc;
  }

  if (parsed_struct.IsInitialized() == false) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Uninitialized parsed_struct rc: %d\n", rc);
    return rc;
  }

  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (parsed_struct.ParseFromArray(kafka_buffer->get_data(), kafka_buffer->get_size())) {
    ACA_LOG_INFO("Successfully converted kafka buffer to protobuf struct\n");

    return EXIT_SUCCESS;
  } else {
    rc = -EXIT_FAILURE;
    ACA_LOG_ERROR("Failed to convert kafka message to protobuf struct rc: %d\n", rc);
    return rc;
  }
}

int Aca_Comm_Manager::update_goal_state(GoalState &goal_state_message,
                                        GoalStateOperationReply &gsOperationReply)
{
  int exec_command_rc;
  int rc = EXIT_SUCCESS;
  auto start = chrono::steady_clock::now();

  ACA_LOG_DEBUG("Starting to update goal state\n");

  ACA_LOG_INFO("[METRICS] Goal state message size is: %lu bytes\n",
               goal_state_message.ByteSizeLong());

  this->print_goal_state(goal_state_message);

  if (goal_state_message.router_states_size() > 0) {
    exec_command_rc = Aca_Goal_State_Handler::get_instance().update_router_states(
            goal_state_message, gsOperationReply);
    if (exec_command_rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully updated router states, rc: %d\n", exec_command_rc);
    } else {
      ACA_LOG_ERROR("Failed to update router states. rc: %d\n", exec_command_rc);
      rc = exec_command_rc;
    }
  }

  if (goal_state_message.port_states_size() > 0) {
    exec_command_rc = Aca_Goal_State_Handler::get_instance().update_port_states(
            goal_state_message, gsOperationReply);
    if (exec_command_rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully updated port states, rc: %d\n", exec_command_rc);
    } else if (exec_command_rc == EINPROGRESS) {
      ACA_LOG_INFO("Update port states returned pending, rc: %d\n", exec_command_rc);
      rc = exec_command_rc;
    } else {
      ACA_LOG_ERROR("Failed to update port states. rc: %d\n", exec_command_rc);
      rc = exec_command_rc;
    }
  }

  if (goal_state_message.neighbor_states_size() > 0) {
    exec_command_rc = Aca_Goal_State_Handler::get_instance().update_neighbor_states(
            goal_state_message, gsOperationReply);
    if (exec_command_rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully updated neighbor states, rc: %d\n", exec_command_rc);
    } else {
      ACA_LOG_ERROR("Failed to update neighbor states. rc: %d\n", exec_command_rc);
      rc = exec_command_rc;
    }
  }

  exec_command_rc = Aca_Dhcp_State_Handler::get_instance().update_dhcp_states(
          goal_state_message, gsOperationReply);
  if (exec_command_rc != EXIT_SUCCESS) {
    ACA_LOG_ERROR("Failed to update dhcp state. Failed with error code %d\n", exec_command_rc);
    rc = exec_command_rc;
  }

  auto end = chrono::steady_clock::now();

  auto message_total_operation_time = cast_to_nanoseconds(end - start).count();

  gsOperationReply.set_message_total_operation_time(message_total_operation_time);

  g_total_update_GS_time += message_total_operation_time;

  ACA_LOG_INFO("[METRICS] Elapsed time for message total operation took: %ld nanoseconds or %ld milliseconds\n",
               message_total_operation_time, message_total_operation_time / 1000000);

  return rc;
} // namespace aca_comm_manager

void Aca_Comm_Manager::print_goal_state(GoalState parsed_struct)
{
  if (g_debug_mode == false) {
    return;
  }

  for (int i = 0; i < parsed_struct.vpc_states_size(); i++) {
    fprintf(stdout, "parsed_struct.vpc_states(%d).operation_type(): %s\n", i,
            aca_get_operation_string(parsed_struct.vpc_states(i).operation_type()));

    VpcConfiguration current_VpcConfiguration =
            parsed_struct.vpc_states(i).configuration();

    fprintf(stdout, "current_VpcConfiguration.format_version(): %d\n",
            current_VpcConfiguration.format_version());

    fprintf(stdout, "current_VpcConfiguration.revision_number(): %d\n",
            current_VpcConfiguration.revision_number());

    fprintf(stdout, "current_VpcConfiguration.project_id(): %s\n",
            current_VpcConfiguration.project_id().c_str());

    fprintf(stdout, "current_VpcConfiguration.id(): %s\n",
            current_VpcConfiguration.id().c_str());

    fprintf(stdout, "current_VpcConfiguration.name(): %s \n",
            current_VpcConfiguration.name().c_str());

    fprintf(stdout, "current_VpcConfiguration.cidr(): %s \n",
            current_VpcConfiguration.cidr().c_str());

    fprintf(stdout, "current_VpcConfiguration.tunnel_id(): %ld \n",
            current_VpcConfiguration.tunnel_id());

    for (int j = 0; j < current_VpcConfiguration.subnet_ids_size(); j++) {
      fprintf(stdout, "current_VpcConfiguration.subnet_ids(%d): %s \n", j,
              current_VpcConfiguration.subnet_ids(j).id().c_str());
    }

    for (int k = 0; k < current_VpcConfiguration.routes_size(); k++) {
      fprintf(stdout,
              "current_VpcConfiguration.routes(%d).destination(): "
              "%s \n",
              k, current_VpcConfiguration.routes(k).destination().c_str());

      fprintf(stdout,
              "current_VpcConfiguration.routes(%d).next_hop(): "
              "%s \n",
              k, current_VpcConfiguration.routes(k).next_hop().c_str());
    }

    printf("\n");
  }

  for (int i = 0; i < parsed_struct.subnet_states_size(); i++) {
    fprintf(stdout, "parsed_struct.subnet_states(%d).operation_type(): %s\n", i,
            aca_get_operation_string(parsed_struct.subnet_states(i).operation_type()));

    SubnetConfiguration current_SubnetConfiguration =
            parsed_struct.subnet_states(i).configuration();

    fprintf(stdout, "current_SubnetConfiguration.format_version(): %d\n",
            current_SubnetConfiguration.format_version());

    fprintf(stdout, "current_SubnetConfiguration.revision_number(): %d\n",
            current_SubnetConfiguration.revision_number());

    fprintf(stdout, "current_SubnetConfiguration.id(): %s\n",
            current_SubnetConfiguration.id().c_str());

    fprintf(stdout, "current_SubnetConfiguration.project_id(): %s\n",
            current_SubnetConfiguration.project_id().c_str());

    fprintf(stdout, "current_SubnetConfiguration.vpc_id(): %s\n",
            current_SubnetConfiguration.vpc_id().c_str());

    fprintf(stdout, "current_SubnetConfiguration.name(): %s \n",
            current_SubnetConfiguration.name().c_str());

    fprintf(stdout, "current_SubnetConfiguration.cidr(): %s \n",
            current_SubnetConfiguration.cidr().c_str());

    fprintf(stdout, "current_SubnetConfiguration.tunnel_id(): %ld \n",
            current_SubnetConfiguration.tunnel_id());

    fprintf(stdout, "current_SubnetConfiguration.gateway().ip_address(): %s \n",
            current_SubnetConfiguration.gateway().ip_address().c_str());

    fprintf(stdout, "current_SubnetConfiguration.gateway().mac_address(): %s \n",
            current_SubnetConfiguration.gateway().mac_address().c_str());

    fprintf(stdout, "current_SubnetConfiguration.dhcp_enable(): %d \n",
            current_SubnetConfiguration.dhcp_enable());

    fprintf(stdout, "current_SubnetConfiguration.availability_zone(): %s \n",
            current_SubnetConfiguration.availability_zone().c_str());

    fprintf(stdout, "current_SubnetConfiguration.primary_dns(): %s \n",
            current_SubnetConfiguration.primary_dns().c_str());

    fprintf(stdout, "current_SubnetConfiguration.secondary_dns(): %s \n",
            current_SubnetConfiguration.secondary_dns().c_str());

    printf("\n");
  }

  for (int i = 0; i < parsed_struct.port_states_size(); i++) {
    fprintf(stdout, "parsed_struct.port_states(%d).operation_type(): %s\n", i,
            aca_get_operation_string(parsed_struct.port_states(i).operation_type()));

    PortConfiguration current_PortConfiguration =
            parsed_struct.port_states(i).configuration();

    fprintf(stdout, "current_PortConfiguration.format_version(): %d\n",
            current_PortConfiguration.format_version());

    fprintf(stdout, "current_PortConfiguration.revision_number(): %d\n",
            current_PortConfiguration.revision_number());

    fprintf(stdout, "current_PortConfiguration.message_type(): %d\n",
            current_PortConfiguration.message_type());

    fprintf(stdout, "current_PortConfiguration.id(): %s\n",
            current_PortConfiguration.id().c_str());

    fprintf(stdout, "current_PortConfiguration.network_type(): %d\n",
            current_PortConfiguration.network_type());

    fprintf(stdout, "current_PortConfiguration.project_id(): %s\n",
            current_PortConfiguration.project_id().c_str());

    fprintf(stdout, "current_PortConfiguration.vpc_id(): %s\n",
            current_PortConfiguration.vpc_id().c_str());

    fprintf(stdout, "current_PortConfiguration.name(): %s \n",
            current_PortConfiguration.name().c_str());

    fprintf(stdout, "current_PortConfiguration.mac_address(): %s \n",
            current_PortConfiguration.mac_address().c_str());

    fprintf(stdout, "current_PortConfiguration.admin_state_up(): %d \n",
            current_PortConfiguration.admin_state_up());

    fprintf(stdout, "current_PortConfiguration.host_info().ip_address(): %s \n",
            current_PortConfiguration.host_info().ip_address().c_str());

    fprintf(stdout, "current_PortConfiguration.host_info().mac_address(): %s \n",
            current_PortConfiguration.host_info().mac_address().c_str());

    fprintf(stdout, "current_PortConfiguration.fixed_ips_size(): %u \n",
            current_PortConfiguration.fixed_ips_size());

    for (int j = 0; j < current_PortConfiguration.fixed_ips_size(); j++) {
      fprintf(stdout, "current_PortConfiguration.fixed_ips(%d): subnet_id %s, ip_address %s \n",
              j, current_PortConfiguration.fixed_ips(j).subnet_id().c_str(),
              current_PortConfiguration.fixed_ips(j).ip_address().c_str());
    }

    for (int j = 0; j < current_PortConfiguration.allow_address_pairs_size(); j++) {
      fprintf(stdout, "current_PortConfiguration.allow_address_pairs(%d): ip_address %s, mac_address %s \n",
              j, current_PortConfiguration.allow_address_pairs(j).ip_address().c_str(),
              current_PortConfiguration.allow_address_pairs(j).mac_address().c_str());
    }

    for (int j = 0; j < current_PortConfiguration.security_group_ids_size(); j++) {
      fprintf(stdout, "current_PortConfiguration.security_group_ids(%d): id %s \n",
              j, current_PortConfiguration.security_group_ids(j).id().c_str());
    }

    printf("\n");
  }

  for (int i = 0; i < parsed_struct.neighbor_states_size(); i++) {
    fprintf(stdout, "parsed_struct.neighbor_states(%d).operation_type(): %s\n", i,
            aca_get_operation_string(parsed_struct.neighbor_states(i).operation_type()));

    NeighborConfiguration current_NeighborConfiguration =
            parsed_struct.neighbor_states(i).configuration();

    fprintf(stdout, "current_NeighborConfiguration.format_version(): %d\n",
            current_NeighborConfiguration.format_version());

    fprintf(stdout, "current_NeighborConfiguration.revision_number(): %d\n",
            current_NeighborConfiguration.revision_number());

    fprintf(stdout, "current_NeighborConfiguration.id(): %s\n",
            current_NeighborConfiguration.id().c_str());

    fprintf(stdout, "current_NeighborConfiguration.neighbor_type(): %d\n",
            current_NeighborConfiguration.neighbor_type());

    fprintf(stdout, "current_NeighborConfiguration.project_id(): %s\n",
            current_NeighborConfiguration.project_id().c_str());

    fprintf(stdout, "current_NeighborConfiguration.vpc_id(): %s\n",
            current_NeighborConfiguration.vpc_id().c_str());

    fprintf(stdout, "current_NeighborConfiguration.name(): %s \n",
            current_NeighborConfiguration.name().c_str());

    fprintf(stdout, "current_NeighborConfiguration.mac_address(): %s \n",
            current_NeighborConfiguration.mac_address().c_str());

    fprintf(stdout, "current_NeighborConfiguration.host_ip_address(): %s \n",
            current_NeighborConfiguration.host_ip_address().c_str());

    fprintf(stdout, "current_NeighborConfiguration.neighbor_host_dvr_mac(): %s \n",
            current_NeighborConfiguration.neighbor_host_dvr_mac().c_str());

    fprintf(stdout, "current_NeighborConfiguration.fixed_ips_size(): %u \n",
            current_NeighborConfiguration.fixed_ips_size());

    for (int j = 0; j < current_NeighborConfiguration.fixed_ips_size(); j++) {
      fprintf(stdout, "current_NeighborConfiguration.fixed_ips(%d): subnet_id %s, ip_address %s \n",
              j, current_NeighborConfiguration.fixed_ips(j).subnet_id().c_str(),
              current_NeighborConfiguration.fixed_ips(j).ip_address().c_str());
    }

    for (int j = 0; j < current_NeighborConfiguration.allow_address_pairs_size(); j++) {
      fprintf(stdout, "current_NeighborConfiguration.allow_address_pairs(%d): ip_address %s, mac_address %s \n",
              j,
              current_NeighborConfiguration.allow_address_pairs(j).ip_address().c_str(),
              current_NeighborConfiguration.allow_address_pairs(j).mac_address().c_str());
    }

    printf("\n");
  }

  for (int i = 0; i < parsed_struct.security_group_states_size(); i++) {
    fprintf(stdout, "parsed_struct.neighbor_states(%d).operation_type(): %s\n", i,
            aca_get_operation_string(parsed_struct.security_group_states(i).operation_type()));

    SecurityGroupConfiguration current_SecurityGroupConfiguration =
            parsed_struct.security_group_states(i).configuration();

    fprintf(stdout, "current_SecurityGroupConfiguration.format_version(): %d\n",
            current_SecurityGroupConfiguration.format_version());

    fprintf(stdout, "current_SecurityGroupConfiguration.revision_number(): %d\n",
            current_SecurityGroupConfiguration.revision_number());

    fprintf(stdout, "current_SecurityGroupConfiguration.id(): %s\n",
            current_SecurityGroupConfiguration.id().c_str());

    fprintf(stdout, "current_SecurityGroupConfiguration.project_id(): %s\n",
            current_SecurityGroupConfiguration.project_id().c_str());

    fprintf(stdout, "current_SecurityGroupConfiguration.vpc_id(): %s\n",
            current_SecurityGroupConfiguration.vpc_id().c_str());

    fprintf(stdout, "current_SecurityGroupConfiguration.name(): %s \n",
            current_SecurityGroupConfiguration.name().c_str());

    fprintf(stdout, "current_SecurityGroupConfiguration.security_group_rules_size(): %u \n",
            current_SecurityGroupConfiguration.security_group_rules_size());

    for (int j = 0;
         j < current_SecurityGroupConfiguration.security_group_rules_size(); j++) {
      fprintf(stdout,
              "current_SecurityGroupConfiguration.security_group_rules(%d): security_group_id: %s, id: %s, direction: %d, "
              "ethertype: %d, protocol: %d, port_range_min: %u, port_range_max: %u, remote_ip_prefix: %s, remote_group_id: %s\n",
              j,
              current_SecurityGroupConfiguration.security_group_rules(j)
                      .security_group_id()
                      .c_str(),
              current_SecurityGroupConfiguration.security_group_rules(j).id().c_str(),
              current_SecurityGroupConfiguration.security_group_rules(j).direction(),
              current_SecurityGroupConfiguration.security_group_rules(j).ethertype(),
              current_SecurityGroupConfiguration.security_group_rules(j).protocol(),
              current_SecurityGroupConfiguration.security_group_rules(j).port_range_min(),
              current_SecurityGroupConfiguration.security_group_rules(j).port_range_max(),
              current_SecurityGroupConfiguration.security_group_rules(j)
                      .remote_ip_prefix()
                      .c_str(),
              current_SecurityGroupConfiguration.security_group_rules(j)
                      .remote_group_id()
                      .c_str());
    }

    printf("\n");
  }

  // TODO: add the print out of DHCP message

  for (int i = 0; i < parsed_struct.router_states_size(); i++) {
    fprintf(stdout, "parsed_struct.router_states(%d).operation_type(): %s\n", i,
            aca_get_operation_string(parsed_struct.router_states(i).operation_type()));

    RouterConfiguration current_RouterConfiguration =
            parsed_struct.router_states(i).configuration();

    fprintf(stdout, "current_RouterConfiguration.format_version(): %d\n",
            current_RouterConfiguration.format_version());

    fprintf(stdout, "current_RouterConfiguration.revision_number(): %d\n",
            current_RouterConfiguration.revision_number());

    fprintf(stdout, "current_RouterConfiguration.id(): %s\n",
            current_RouterConfiguration.id().c_str());

    fprintf(stdout, "current_RouterConfiguration.host_dvr_mac_address(): %s \n",
            current_RouterConfiguration.host_dvr_mac_address().c_str());

    fprintf(stdout, "current_RouterConfiguration.subnet_ids_size(): %u \n",
            current_RouterConfiguration.subnet_ids_size());

    for (int j = 0; j < current_RouterConfiguration.subnet_ids_size(); j++) {
      fprintf(stdout, "current_RouterConfiguration.subnet_ids(%d): %s\n", j,
              current_RouterConfiguration.subnet_ids(j).c_str());
    }

    printf("\n");
  }
}

} // namespace aca_comm_manager
