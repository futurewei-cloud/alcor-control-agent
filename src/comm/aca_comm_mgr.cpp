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

int Aca_Comm_Manager::deserialize(const unsigned char *mq_buffer,
                                  size_t buffer_length, GoalStateV2 &parsed_struct)
{
    int rc;

    if (mq_buffer == NULL) {
        rc = -EINVAL;
        ACA_LOG_ERROR("Empty mq_buffer data rc: %d\n", rc);
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

    if (parsed_struct.ParseFromArray(mq_buffer, buffer_length)) {
        ACA_LOG_INFO("%s", "Successfully converted message to protobuf struct\n");

        return EXIT_SUCCESS;
    } else {
        rc = -EXIT_FAILURE;
        ACA_LOG_ERROR("Failed to convert message to protobuf struct rc: %d\n", rc);
        return rc;
    }
}

int Aca_Comm_Manager::deserialize(const unsigned char *mq_buffer,
                                  size_t buffer_length, GoalState &parsed_struct)
{
  int rc;

  if (mq_buffer == NULL) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Empty mq_buffer data rc: %d\n", rc);
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

  if (parsed_struct.ParseFromArray(mq_buffer, buffer_length)) {
    ACA_LOG_INFO("%s", "Successfully converted message to protobuf struct\n");

    return EXIT_SUCCESS;
  } else {
    rc = -EXIT_FAILURE;
    ACA_LOG_ERROR("Failed to convert message to protobuf struct rc: %d\n", rc);
    return rc;
  }
}

int Aca_Comm_Manager::update_goal_state(GoalState &goal_state_message,
                                        GoalStateOperationReply &gsOperationReply)
{
  int exec_command_rc;
  int rc = EXIT_SUCCESS;
  auto start = chrono::steady_clock::now();

  ACA_LOG_DEBUG("Starting to update goal state with format_version: %u\n",
                goal_state_message.format_version());

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
  auto router_update_finished_time = chrono::steady_clock::now();
  auto router_operation_time =
          cast_to_microseconds(router_update_finished_time - start).count();

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
  auto port_update_finished_time = chrono::steady_clock::now();
  auto port_operation_time =
          cast_to_microseconds(port_update_finished_time - router_update_finished_time)
                  .count();

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
  auto neighbor_update_finished_time = chrono::steady_clock::now();
  auto neighbor_operation_time =
          cast_to_microseconds(neighbor_update_finished_time - port_update_finished_time)
                  .count();
  exec_command_rc = Aca_Dhcp_State_Handler::get_instance().update_dhcp_states(
          goal_state_message, gsOperationReply);
  if (exec_command_rc != EXIT_SUCCESS) {
    ACA_LOG_ERROR("Failed to update dhcp state. Failed with error code %d\n", exec_command_rc);
    rc = exec_command_rc;
  }

  auto end = chrono::steady_clock::now();
  auto dhcp_operation_time =
          cast_to_microseconds(end - neighbor_update_finished_time).count();
  auto message_total_operation_time = cast_to_microseconds(end - start).count();

  ACA_LOG_INFO("[METRICS] Elapsed time for message total operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for router operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for port operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for neighbor operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for dhcp operation took: %ld microseconds or %ld milliseconds\n",
                message_total_operation_time, us_to_ms(message_total_operation_time),
                router_operation_time, us_to_ms(router_operation_time),
                port_operation_time, us_to_ms(port_operation_time),
                neighbor_operation_time, us_to_ms(neighbor_operation_time),
                dhcp_operation_time, us_to_ms(dhcp_operation_time));

  gsOperationReply.set_message_total_operation_time(
          message_total_operation_time + gsOperationReply.message_total_operation_time());

  g_total_update_GS_time += message_total_operation_time;

  return rc;
}

int Aca_Comm_Manager::update_goal_state(GoalStateV2 &goal_state_message,
                                        GoalStateOperationReply &gsOperationReply)
{
  int exec_command_rc;
  int rc = EXIT_SUCCESS;
  auto start = chrono::steady_clock::now();

  this->print_goal_state(goal_state_message);

  auto gs_printout_finished_time = chrono::steady_clock::now();
  auto gs_printout_operation_time =
          cast_to_microseconds(gs_printout_finished_time - start).count();

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
  auto router_update_finished_time = chrono::steady_clock::now();
  auto router_operation_time =
          cast_to_microseconds(router_update_finished_time - gs_printout_finished_time)
                  .count();

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
  auto port_update_finished_time = chrono::steady_clock::now();
  auto port_operation_time =
          cast_to_microseconds(port_update_finished_time - router_update_finished_time)
                  .count();
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
  auto neighbor_update_finished_time = chrono::steady_clock::now();
  auto neighbor_operation_time =
          cast_to_microseconds(neighbor_update_finished_time - port_update_finished_time)
                  .count();
  exec_command_rc = Aca_Dhcp_State_Handler::get_instance().update_dhcp_states(
          goal_state_message, gsOperationReply);
  if (exec_command_rc != EXIT_SUCCESS) {
    ACA_LOG_ERROR("Failed to update dhcp state. Failed with error code %d\n", exec_command_rc);
    rc = exec_command_rc;
  }

  auto end = chrono::steady_clock::now();

  auto dhcp_operation_time =
          cast_to_microseconds(end - neighbor_update_finished_time).count();
  auto message_total_operation_time = cast_to_microseconds(end - start).count();

  ACA_LOG_INFO("[METRICS] Elapsed time for message total operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for gs printout operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for router operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for port operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for neighbor operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for dhcp operation took: %ld microseconds or %ld milliseconds\n",
                message_total_operation_time, us_to_ms(message_total_operation_time),
                gs_printout_operation_time, us_to_ms(gs_printout_operation_time),
                router_operation_time, us_to_ms(router_operation_time),
                port_operation_time, us_to_ms(port_operation_time),
                neighbor_operation_time, us_to_ms(neighbor_operation_time),
                dhcp_operation_time, us_to_ms(dhcp_operation_time));

  gsOperationReply.set_message_total_operation_time(
          message_total_operation_time + gsOperationReply.message_total_operation_time());

  g_total_update_GS_time += message_total_operation_time;

  return rc;
}

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

    fprintf(stdout, "current_VpcConfiguration.revision_number(): %d\n",
            current_VpcConfiguration.revision_number());

    fprintf(stdout, "current_VpcConfiguration.request_id(): %s\n",
            current_VpcConfiguration.request_id().c_str());

    fprintf(stdout, "current_VpcConfiguration.id(): %s\n",
            current_VpcConfiguration.id().c_str());

    fprintf(stdout, "current_VpcConfiguration.update_type(): %d\n",
            current_VpcConfiguration.update_type());

    fprintf(stdout, "current_VpcConfiguration.project_id(): %s\n",
            current_VpcConfiguration.project_id().c_str());

    fprintf(stdout, "current_VpcConfiguration.name(): %s \n",
            current_VpcConfiguration.name().c_str());

    fprintf(stdout, "current_VpcConfiguration.cidr(): %s \n",
            current_VpcConfiguration.cidr().c_str());

    fprintf(stdout, "current_VpcConfiguration.tunnel_id(): %u \n",
            current_VpcConfiguration.tunnel_id());

    fprintf(stdout, "current_VpcConfiguration.tunnel_id(): %u \n",
            current_VpcConfiguration.tunnel_id());

    for (int j = 0; j < current_VpcConfiguration.gateway_ids_size(); j++) {
      fprintf(stdout, "current_VpcConfiguration.gateway_size(%d): %s \n", j,
              current_VpcConfiguration.gateway_ids(j).c_str());
    }

    printf("\n");
  }

  for (int i = 0; i < parsed_struct.subnet_states_size(); i++) {
    fprintf(stdout, "parsed_struct.subnet_states(%d).operation_type(): %s\n", i,
            aca_get_operation_string(parsed_struct.subnet_states(i).operation_type()));

    SubnetConfiguration current_SubnetConfiguration =
            parsed_struct.subnet_states(i).configuration();

    fprintf(stdout, "current_SubnetConfiguration.revision_number(): %d\n",
            current_SubnetConfiguration.revision_number());

    fprintf(stdout, "current_SubnetConfiguration.request_id(): %s\n",
            current_SubnetConfiguration.request_id().c_str());

    fprintf(stdout, "current_SubnetConfiguration.id(): %s\n",
            current_SubnetConfiguration.id().c_str());

    fprintf(stdout, "current_SubnetConfiguration.update_type(): %d\n",
            current_SubnetConfiguration.update_type());

    fprintf(stdout, "current_SubnetConfiguration.network_type(): %d\n",
            current_SubnetConfiguration.network_type());

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

    for (int j = 0; j < current_SubnetConfiguration.extra_dhcp_options_size(); j++) {
      fprintf(stdout, "current_SubnetConfiguration.extra_dhcp_options(%d): name: %s, value %s \n",
              j, current_SubnetConfiguration.extra_dhcp_options(j).name().c_str(),
              current_SubnetConfiguration.extra_dhcp_options(j).value().c_str());
    }

    for (int j = 0; j < current_SubnetConfiguration.dns_entry_list_size(); j++) {
      fprintf(stdout, "current_SubnetConfiguration.dns_entry_list(%d): entry %s \n",
              j, current_SubnetConfiguration.dns_entry_list(j).entry().c_str());
    }

    fprintf(stdout, "current_SubnetConfiguration.availability_zone(): %s \n",
            current_SubnetConfiguration.availability_zone().c_str());

    printf("\n");
  }

  for (int i = 0; i < parsed_struct.port_states_size(); i++) {
    fprintf(stdout, "parsed_struct.port_states(%d).operation_type(): %s\n", i,
            aca_get_operation_string(parsed_struct.port_states(i).operation_type()));

    PortConfiguration current_PortConfiguration =
            parsed_struct.port_states(i).configuration();

    fprintf(stdout, "current_PortConfiguration.revision_number(): %d\n",
            current_PortConfiguration.revision_number());

    fprintf(stdout, "current_PortConfiguration.request_id(): %s\n",
            current_PortConfiguration.request_id().c_str());

    fprintf(stdout, "current_PortConfiguration.update_type(): %d\n",
            current_PortConfiguration.update_type());

    fprintf(stdout, "current_PortConfiguration.id(): %s\n",
            current_PortConfiguration.id().c_str());

    fprintf(stdout, "current_PortConfiguration.vpc_id(): %s\n",
            current_PortConfiguration.vpc_id().c_str());

    fprintf(stdout, "current_PortConfiguration.name(): %s \n",
            current_PortConfiguration.name().c_str());

    fprintf(stdout, "current_PortConfiguration.device_id(): %s\n",
            current_PortConfiguration.device_id().c_str());

    fprintf(stdout, "current_PortConfiguration.device_owner(): %s \n",
            current_PortConfiguration.device_owner().c_str());

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

    fprintf(stdout, "current_NeighborConfiguration.revision_number(): %d\n",
            current_NeighborConfiguration.revision_number());

    fprintf(stdout, "current_NeighborConfiguration.id(): %s\n",
            current_NeighborConfiguration.id().c_str());

    fprintf(stdout, "current_NeighborConfiguration.request_id(): %s\n",
            current_NeighborConfiguration.request_id().c_str());

    fprintf(stdout, "current_NeighborConfiguration.update_type(): %d\n",
            current_NeighborConfiguration.update_type());

    fprintf(stdout, "current_NeighborConfiguration.vpc_id(): %s\n",
            current_NeighborConfiguration.vpc_id().c_str());

    fprintf(stdout, "current_NeighborConfiguration.name(): %s \n",
            current_NeighborConfiguration.name().c_str());

    fprintf(stdout, "current_NeighborConfiguration.mac_address(): %s \n",
            current_NeighborConfiguration.mac_address().c_str());

    fprintf(stdout, "current_NeighborConfiguration.host_ip_address(): %s \n",
            current_NeighborConfiguration.host_ip_address().c_str());

    fprintf(stdout, "current_NeighborConfiguration.fixed_ips_size(): %u \n",
            current_NeighborConfiguration.fixed_ips_size());

    for (int j = 0; j < current_NeighborConfiguration.fixed_ips_size(); j++) {
      fprintf(stdout, "current_NeighborConfiguration.fixed_ips(%d): neighbor_type: %d, subnet_id %s, ip_address %s \n",
              j, current_NeighborConfiguration.fixed_ips(j).neighbor_type(),
              current_NeighborConfiguration.fixed_ips(j).subnet_id().c_str(),
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
    fprintf(stdout, "parsed_struct.security_group_states(%d).operation_type(): %s\n", i,
            aca_get_operation_string(parsed_struct.security_group_states(i).operation_type()));

    SecurityGroupConfiguration current_SecurityGroupConfiguration =
            parsed_struct.security_group_states(i).configuration();

    fprintf(stdout, "current_SecurityGroupConfiguration.revision_number(): %d\n",
            current_SecurityGroupConfiguration.revision_number());

    fprintf(stdout, "current_SecurityGroupConfiguration.request_id(): %s\n",
            current_SecurityGroupConfiguration.request_id().c_str());

    fprintf(stdout, "current_SecurityGroupConfiguration.id(): %s\n",
            current_SecurityGroupConfiguration.id().c_str());

    fprintf(stdout, "current_SecurityGroupConfiguration.update_type(): %d\n",
            current_SecurityGroupConfiguration.update_type());

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

  for (int i = 0; i < parsed_struct.dhcp_states_size(); i++) {
    fprintf(stdout, "parsed_struct.dhcp_states(%d).operation_type(): %s\n", i,
            aca_get_operation_string(parsed_struct.dhcp_states(i).operation_type()));

    DHCPConfiguration current_DHCPConfiguration =
            parsed_struct.dhcp_states(i).configuration();

    fprintf(stdout, "current_DHCPConfiguration.revision_number(): %d\n",
            current_DHCPConfiguration.revision_number());

    fprintf(stdout, "current_DHCPConfiguration.request_id(): %s\n",
            current_DHCPConfiguration.request_id().c_str());

    fprintf(stdout, "current_DHCPConfiguration.subnet_id(): %s\n",
            current_DHCPConfiguration.subnet_id().c_str());

    fprintf(stdout, "current_DHCPConfiguration.mac_address(): %s\n",
            current_DHCPConfiguration.mac_address().c_str());

    fprintf(stdout, "current_DHCPConfiguration.ipv4_address(): %s\n",
            current_DHCPConfiguration.ipv4_address().c_str());

    fprintf(stdout, "current_DHCPConfiguration.ipv6_address(): %s\n",
            current_DHCPConfiguration.ipv6_address().c_str());

    fprintf(stdout, "current_DHCPConfiguration.port_host_name(): %s \n",
            current_DHCPConfiguration.port_host_name().c_str());

    printf("\n");
  }

  for (int i = 0; i < parsed_struct.router_states_size(); i++) {
    fprintf(stdout, "parsed_struct.router_states(%d).operation_type(): %s\n", i,
            aca_get_operation_string(parsed_struct.router_states(i).operation_type()));

    RouterConfiguration current_RouterConfiguration =
            parsed_struct.router_states(i).configuration();

    fprintf(stdout, "current_RouterConfiguration.revision_number(): %d\n",
            current_RouterConfiguration.revision_number());

    fprintf(stdout, "current_RouterConfiguration.request_id(): %s\n",
            current_RouterConfiguration.request_id().c_str());

    fprintf(stdout, "current_RouterConfiguration.id(): %s\n",
            current_RouterConfiguration.id().c_str());

    fprintf(stdout, "current_RouterConfiguration.host_dvr_mac_address(): %s \n",
            current_RouterConfiguration.host_dvr_mac_address().c_str());

    fprintf(stdout, "current_RouterConfiguration.subnet_routing_tables_size(): %u \n",
            current_RouterConfiguration.subnet_routing_tables_size());

    for (int j = 0; j < current_RouterConfiguration.subnet_routing_tables_size(); j++) {
      fprintf(stdout, "current_RouterConfiguration.subnet_routing_tables(%d).subnet_id: %s\n", j,
              current_RouterConfiguration.subnet_routing_tables(j).subnet_id().c_str());

      for (int k = 0;
           k < current_RouterConfiguration.subnet_routing_tables(j).routing_rules_size();
           k++) {
        auto current_routing_rule =
                current_RouterConfiguration.subnet_routing_tables(j).routing_rules(k);

        fprintf(stdout, "current_routing_rule(%d).operation_type(): %s\n", k,
                aca_get_operation_string(current_routing_rule.operation_type()));

        fprintf(stdout, "current_routing_rule(%d).id(): %s\n", k,
                current_routing_rule.id().c_str());

        fprintf(stdout, "current_routing_rule(%d).name(): %s\n", k,
                current_routing_rule.name().c_str());

        fprintf(stdout, "current_routing_rule(%d).destination(): %s\n", k,
                current_routing_rule.destination().c_str());

        fprintf(stdout, "current_routing_rule(%d).next_hop_ip(): %s\n", k,
                current_routing_rule.next_hop_ip().c_str());

        fprintf(stdout, "current_routing_rule(%d).routing_rule_extra_info().destination_type(): %d\n",
                k, current_routing_rule.routing_rule_extra_info().destination_type());

        fprintf(stdout, "current_routing_rule(%d).routing_rule_extra_info().next_hop_mac()): %s\n",
                k, current_routing_rule.routing_rule_extra_info().next_hop_mac().c_str());
      }
    }

    printf("\n");
  }

  for (int i = 0; i < parsed_struct.gateway_states_size(); i++) {
    fprintf(stdout, "parsed_struct.gateway_states(%d).operation_type(): %s\n", i,
            aca_get_operation_string(parsed_struct.gateway_states(i).operation_type()));

    GatewayConfiguration current_GatewayConfiguration =
            parsed_struct.gateway_states(i).configuration();

    fprintf(stdout, "current_GatewayConfiguration.gateway_type(): %d\n",
            current_GatewayConfiguration.gateway_type());

    fprintf(stdout, "current_GatewayConfiguration.id(): %s\n",
            current_GatewayConfiguration.id().c_str());

    for (int k = 0; k < current_GatewayConfiguration.destinations_size(); k++) {
      fprintf(stdout, "current_GatewayConfiguration.destinations(%d).ip_address(): %s \n",
              k, current_GatewayConfiguration.destinations(k).ip_address().c_str());

      fprintf(stdout, "current_GatewayConfiguration.destinations(%d).mac_address(): %s \n",
              k, current_GatewayConfiguration.destinations(k).mac_address().c_str());
    }

    if (current_GatewayConfiguration.has_zeta_info()) {
      fprintf(stdout, "current_GatewayConfiguration.zeta_info().port_inband_operation: %d\n",
              current_GatewayConfiguration.zeta_info().port_inband_operation());
    }

    printf("\n");
  }
}

void Aca_Comm_Manager::print_goal_state(GoalStateV2 parsed_struct)
{
  if (g_debug_mode == false) {
    return;
  }
  auto start = chrono::steady_clock::now();

  for (auto &[vpc_id, current_VpcState] : parsed_struct.vpc_states()) {
    fprintf(stdout, "current_VpcState.operation_type(): %s\n",
            aca_get_operation_string(current_VpcState.operation_type()));

    VpcConfiguration current_VpcConfiguration = current_VpcState.configuration();

    fprintf(stdout, "current_VpcConfiguration.revision_number(): %d\n",
            current_VpcConfiguration.revision_number());

    fprintf(stdout, "current_VpcConfiguration.request_id(): %s\n",
            current_VpcConfiguration.request_id().c_str());

    fprintf(stdout, "current_VpcConfiguration.id(): %s\n",
            current_VpcConfiguration.id().c_str());

    fprintf(stdout, "current_VpcConfiguration.update_type(): %d\n",
            current_VpcConfiguration.update_type());

    fprintf(stdout, "current_VpcConfiguration.project_id(): %s\n",
            current_VpcConfiguration.project_id().c_str());

    fprintf(stdout, "current_VpcConfiguration.name(): %s \n",
            current_VpcConfiguration.name().c_str());

    fprintf(stdout, "current_VpcConfiguration.cidr(): %s \n",
            current_VpcConfiguration.cidr().c_str());

    fprintf(stdout, "current_VpcConfiguration.tunnel_id(): %u \n",
            current_VpcConfiguration.tunnel_id());

    fprintf(stdout, "current_VpcConfiguration.tunnel_id(): %u \n",
            current_VpcConfiguration.tunnel_id());

    for (int j = 0; j < current_VpcConfiguration.gateway_ids_size(); j++) {
      fprintf(stdout, "current_VpcConfiguration.gateway_size(%d): %s \n", j,
              current_VpcConfiguration.gateway_ids(j).c_str());
    }

    printf("\n");
  }
  auto vpc_printout_finished_time = chrono::steady_clock::now();
  auto vpc_printout_elapsed_time =
          cast_to_microseconds(vpc_printout_finished_time - start).count();
  for (auto &[subnet_id, current_SubnetState] : parsed_struct.subnet_states()) {
    fprintf(stdout, "current_SubnetState.operation_type(): %s\n",
            aca_get_operation_string(current_SubnetState.operation_type()));

    SubnetConfiguration current_SubnetConfiguration = current_SubnetState.configuration();

    fprintf(stdout, "current_SubnetConfiguration.revision_number(): %d\n",
            current_SubnetConfiguration.revision_number());

    fprintf(stdout, "current_SubnetConfiguration.request_id(): %s\n",
            current_SubnetConfiguration.request_id().c_str());

    fprintf(stdout, "current_SubnetConfiguration.id(): %s\n",
            current_SubnetConfiguration.id().c_str());

    fprintf(stdout, "current_SubnetConfiguration.update_type(): %d\n",
            current_SubnetConfiguration.update_type());

    fprintf(stdout, "current_SubnetConfiguration.network_type(): %d\n",
            current_SubnetConfiguration.network_type());

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

    for (int j = 0; j < current_SubnetConfiguration.extra_dhcp_options_size(); j++) {
      fprintf(stdout, "current_SubnetConfiguration.extra_dhcp_options(%d): name: %s, value %s \n",
              j, current_SubnetConfiguration.extra_dhcp_options(j).name().c_str(),
              current_SubnetConfiguration.extra_dhcp_options(j).value().c_str());
    }

    for (int j = 0; j < current_SubnetConfiguration.dns_entry_list_size(); j++) {
      fprintf(stdout, "current_SubnetConfiguration.dns_entry_list(%d): entry %s \n",
              j, current_SubnetConfiguration.dns_entry_list(j).entry().c_str());
    }

    fprintf(stdout, "current_SubnetConfiguration.availability_zone(): %s \n",
            current_SubnetConfiguration.availability_zone().c_str());

    printf("\n");
  }
  auto subnet_printout_finished_time = chrono::steady_clock::now();
  auto subnet_printout_elapsed_time =
          cast_to_microseconds(subnet_printout_finished_time - vpc_printout_finished_time)
                  .count();
  for (auto &[port_id, current_PortState] : parsed_struct.port_states()) {
    fprintf(stdout, "current_PortState.operation_type(): %s\n",
            aca_get_operation_string(current_PortState.operation_type()));

    PortConfiguration current_PortConfiguration = current_PortState.configuration();

    fprintf(stdout, "current_PortConfiguration.revision_number(): %d\n",
            current_PortConfiguration.revision_number());

    fprintf(stdout, "current_PortConfiguration.request_id(): %s\n",
            current_PortConfiguration.request_id().c_str());

    fprintf(stdout, "current_PortConfiguration.update_type(): %d\n",
            current_PortConfiguration.update_type());

    fprintf(stdout, "current_PortConfiguration.id(): %s\n",
            current_PortConfiguration.id().c_str());

    fprintf(stdout, "current_PortConfiguration.vpc_id(): %s\n",
            current_PortConfiguration.vpc_id().c_str());

    fprintf(stdout, "current_PortConfiguration.name(): %s \n",
            current_PortConfiguration.name().c_str());

    fprintf(stdout, "current_PortConfiguration.device_id(): %s\n",
            current_PortConfiguration.device_id().c_str());

    fprintf(stdout, "current_PortConfiguration.device_owner(): %s \n",
            current_PortConfiguration.device_owner().c_str());

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
  auto port_printout_finished_time = chrono::steady_clock::now();
  auto port_printout_elapsed_time =
          cast_to_microseconds(port_printout_finished_time - subnet_printout_finished_time)
                  .count();
  for (auto &[neighbor_id, current_NeighborState] : parsed_struct.neighbor_states()) {
    fprintf(stdout, "current_NeighborState.operation_type(): %s\n",
            aca_get_operation_string(current_NeighborState.operation_type()));

    NeighborConfiguration current_NeighborConfiguration =
            current_NeighborState.configuration();

    fprintf(stdout, "current_NeighborConfiguration.revision_number(): %d\n",
            current_NeighborConfiguration.revision_number());

    fprintf(stdout, "current_NeighborConfiguration.id(): %s\n",
            current_NeighborConfiguration.id().c_str());

    fprintf(stdout, "current_NeighborConfiguration.request_id(): %s\n",
            current_NeighborConfiguration.request_id().c_str());

    fprintf(stdout, "current_NeighborConfiguration.update_type(): %d\n",
            current_NeighborConfiguration.update_type());

    fprintf(stdout, "current_NeighborConfiguration.vpc_id(): %s\n",
            current_NeighborConfiguration.vpc_id().c_str());

    fprintf(stdout, "current_NeighborConfiguration.name(): %s \n",
            current_NeighborConfiguration.name().c_str());

    fprintf(stdout, "current_NeighborConfiguration.mac_address(): %s \n",
            current_NeighborConfiguration.mac_address().c_str());

    fprintf(stdout, "current_NeighborConfiguration.host_ip_address(): %s \n",
            current_NeighborConfiguration.host_ip_address().c_str());

    fprintf(stdout, "current_NeighborConfiguration.fixed_ips_size(): %u \n",
            current_NeighborConfiguration.fixed_ips_size());

    for (int j = 0; j < current_NeighborConfiguration.fixed_ips_size(); j++) {
      fprintf(stdout, "current_NeighborConfiguration.fixed_ips(%d): neighbor_type: %d, subnet_id %s, ip_address %s \n",
              j, current_NeighborConfiguration.fixed_ips(j).neighbor_type(),
              current_NeighborConfiguration.fixed_ips(j).subnet_id().c_str(),
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
  auto neighbor_printout_finished_time = chrono::steady_clock::now();
  auto neighbor_printout_elapsed_time =
          cast_to_microseconds(neighbor_printout_finished_time - port_printout_finished_time)
                  .count();
  for (auto &[security_group_id, current_security_group_State] :
       parsed_struct.security_group_states()) {
    fprintf(stdout, "current_security_group_State.operation_type(): %s\n",
            aca_get_operation_string(current_security_group_State.operation_type()));

    SecurityGroupConfiguration current_SecurityGroupConfiguration =
            current_security_group_State.configuration();

    fprintf(stdout, "current_SecurityGroupConfiguration.revision_number(): %d\n",
            current_SecurityGroupConfiguration.revision_number());

    fprintf(stdout, "current_SecurityGroupConfiguration.request_id(): %s\n",
            current_SecurityGroupConfiguration.request_id().c_str());

    fprintf(stdout, "current_SecurityGroupConfiguration.id(): %s\n",
            current_SecurityGroupConfiguration.id().c_str());

    fprintf(stdout, "current_SecurityGroupConfiguration.update_type(): %d\n",
            current_SecurityGroupConfiguration.update_type());

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
  auto sg_printout_finished_time = chrono::steady_clock::now();
  auto sg_printout_elapsed_time =
          cast_to_microseconds(sg_printout_finished_time - neighbor_printout_finished_time)
                  .count();
  for (auto &[dhcp_id, current_dhcp_State] : parsed_struct.dhcp_states()) {
    fprintf(stdout, "current_dhcp_State.operation_type(): %s\n",
            aca_get_operation_string(current_dhcp_State.operation_type()));

    DHCPConfiguration current_DHCPConfiguration = current_dhcp_State.configuration();

    fprintf(stdout, "current_DHCPConfiguration.revision_number(): %d\n",
            current_DHCPConfiguration.revision_number());

    fprintf(stdout, "current_DHCPConfiguration.request_id(): %s\n",
            current_DHCPConfiguration.request_id().c_str());

    fprintf(stdout, "current_DHCPConfiguration.subnet_id(): %s\n",
            current_DHCPConfiguration.subnet_id().c_str());

    fprintf(stdout, "current_DHCPConfiguration.mac_address(): %s\n",
            current_DHCPConfiguration.mac_address().c_str());

    fprintf(stdout, "current_DHCPConfiguration.ipv4_address(): %s\n",
            current_DHCPConfiguration.ipv4_address().c_str());

    fprintf(stdout, "current_DHCPConfiguration.ipv6_address(): %s\n",
            current_DHCPConfiguration.ipv6_address().c_str());

    fprintf(stdout, "current_DHCPConfiguration.port_host_name(): %s \n",
            current_DHCPConfiguration.port_host_name().c_str());

    printf("\n");
  }
  auto dhcp_printout_finished_time = chrono::steady_clock::now();
  auto dhcp_printout_elapsed_time =
          cast_to_microseconds(dhcp_printout_finished_time - sg_printout_finished_time)
                  .count();
  for (auto &[router_id, current_router_State] : parsed_struct.router_states()) {
    fprintf(stdout, "current_router_State.operation_type(): %s\n",
            aca_get_operation_string(current_router_State.operation_type()));

    RouterConfiguration current_RouterConfiguration =
            current_router_State.configuration();

    fprintf(stdout, "current_RouterConfiguration.revision_number(): %d\n",
            current_RouterConfiguration.revision_number());

    fprintf(stdout, "current_RouterConfiguration.request_id(): %s\n",
            current_RouterConfiguration.request_id().c_str());

    fprintf(stdout, "current_RouterConfiguration.id(): %s\n",
            current_RouterConfiguration.id().c_str());

    fprintf(stdout, "current_RouterConfiguration.host_dvr_mac_address(): %s \n",
            current_RouterConfiguration.host_dvr_mac_address().c_str());

    fprintf(stdout, "current_RouterConfiguration.subnet_routing_tables_size(): %u \n",
            current_RouterConfiguration.subnet_routing_tables_size());

    for (int j = 0; j < current_RouterConfiguration.subnet_routing_tables_size(); j++) {
      fprintf(stdout, "current_RouterConfiguration.subnet_routing_tables(%d).subnet_id: %s\n", j,
              current_RouterConfiguration.subnet_routing_tables(j).subnet_id().c_str());

      for (int k = 0;
           k < current_RouterConfiguration.subnet_routing_tables(j).routing_rules_size();
           k++) {
        auto current_routing_rule =
                current_RouterConfiguration.subnet_routing_tables(j).routing_rules(k);

        fprintf(stdout, "current_routing_rule(%d).operation_type(): %s\n", k,
                aca_get_operation_string(current_routing_rule.operation_type()));

        fprintf(stdout, "current_routing_rule(%d).id(): %s\n", k,
                current_routing_rule.id().c_str());

        fprintf(stdout, "current_routing_rule(%d).name(): %s\n", k,
                current_routing_rule.name().c_str());

        fprintf(stdout, "current_routing_rule(%d).destination(): %s\n", k,
                current_routing_rule.destination().c_str());

        fprintf(stdout, "current_routing_rule(%d).next_hop_ip(): %s\n", k,
                current_routing_rule.next_hop_ip().c_str());

        fprintf(stdout, "current_routing_rule(%d).routing_rule_extra_info().destination_type(): %d\n",
                k, current_routing_rule.routing_rule_extra_info().destination_type());

        fprintf(stdout, "current_routing_rule(%d).routing_rule_extra_info().next_hop_mac()): %s\n",
                k, current_routing_rule.routing_rule_extra_info().next_hop_mac().c_str());
      }
    }

    printf("\n");
  }
  auto router_printout_finished_time = chrono::steady_clock::now();
  auto router_printout_elapsed_time =
          cast_to_microseconds(router_printout_finished_time - dhcp_printout_finished_time)
                  .count();
  for (auto &[gateway_id, current_gateway_State] : parsed_struct.gateway_states()) {
    fprintf(stdout, "current_gateway_State.operation_type(): %s\n",
            aca_get_operation_string(current_gateway_State.operation_type()));

    GatewayConfiguration current_GatewayConfiguration =
            current_gateway_State.configuration();

    fprintf(stdout, "current_GatewayConfiguration.gateway_type(): %d\n",
            current_GatewayConfiguration.gateway_type());

    fprintf(stdout, "current_GatewayConfiguration.id(): %s\n",
            current_GatewayConfiguration.id().c_str());

    for (int k = 0; k < current_GatewayConfiguration.destinations_size(); k++) {
      fprintf(stdout, "current_GatewayConfiguration.destinations(%d).ip_address(): %s \n",
              k, current_GatewayConfiguration.destinations(k).ip_address().c_str());

      fprintf(stdout, "current_GatewayConfiguration.destinations(%d).mac_address(): %s \n",
              k, current_GatewayConfiguration.destinations(k).mac_address().c_str());
    }

    if (current_GatewayConfiguration.has_zeta_info()) {
      fprintf(stdout, "current_GatewayConfiguration.zeta_info().port_inband_operation: %d\n",
              current_GatewayConfiguration.zeta_info().port_inband_operation());
    }

    printf("\n");
  }
  auto gw_printout_finished_time = chrono::steady_clock::now();
  auto gw_printout_elapsed_time =
          cast_to_microseconds(gw_printout_finished_time - router_printout_finished_time)
                  .count();

  auto end = chrono::steady_clock::now();

  auto message_total_operation_time = cast_to_microseconds(end - start).count();

  ACA_LOG_DEBUG(
          "[METRICS] Elapsed time for goalstateV2 printout took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for vpc printout operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for subnet printout operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for port printout operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for neighbor printout operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for security group printout operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for dhcp printout operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for router printout operation took: %ld microseconds or %ld milliseconds\n\
[METRICS] Elapsed time for gateway printout operation took: %ld microseconds or %ld milliseconds\n",
          message_total_operation_time, us_to_ms(message_total_operation_time),
          vpc_printout_elapsed_time, us_to_ms(vpc_printout_elapsed_time),
          subnet_printout_elapsed_time, us_to_ms(subnet_printout_elapsed_time),
          port_printout_elapsed_time, us_to_ms(port_printout_elapsed_time),
          neighbor_printout_elapsed_time, us_to_ms(neighbor_printout_elapsed_time),
          sg_printout_elapsed_time, us_to_ms(sg_printout_elapsed_time),
          dhcp_printout_elapsed_time, us_to_ms(dhcp_printout_elapsed_time),
          router_printout_elapsed_time, us_to_ms(router_printout_elapsed_time),
          gw_printout_elapsed_time, us_to_ms(gw_printout_elapsed_time));
}

} // namespace aca_comm_manager