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
#include "aca_net_state_handler.h"
#include "goalstateprovisioner.grpc.pb.h"

using namespace std;
using namespace alcor::schema;
using namespace aca_net_state_handler;

extern string g_rpc_server;
extern string g_rpc_protocol;
extern std::atomic_ulong g_total_rpc_call_time;
extern std::atomic_ulong g_total_rpc_client_time;
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

    this->print_goal_state(parsed_struct);

    return EXIT_SUCCESS;
  } else {
    rc = -EXIT_FAILURE;
    ACA_LOG_ERROR("Failed to convert kafka message to protobuf struct rc: %d\n", rc);
    return rc;
  }
}

int Aca_Comm_Manager::update_goal_state(GoalState &parsed_struct,
                                        GoalStateOperationReply &gsOperationReply)
{
  int exec_command_rc = -EXIT_FAILURE;
  int rc = EXIT_SUCCESS;
  auto start = chrono::steady_clock::now();

  ACA_LOG_DEBUG("Starting to update goal state\n");

  exec_command_rc = Aca_Net_State_Handler::get_instance().update_vpc_states(
          parsed_struct, gsOperationReply);
  if (exec_command_rc != EXIT_SUCCESS) {
    ACA_LOG_ERROR("Failed to update vpc state. Failed with error code %d\n", exec_command_rc);
    rc = exec_command_rc;
  }

  exec_command_rc = Aca_Net_State_Handler::get_instance().update_subnet_states(
          parsed_struct, gsOperationReply);
  if (exec_command_rc != EXIT_SUCCESS) {
    ACA_LOG_ERROR("Failed to update subnet state. Failed with error code %d\n", exec_command_rc);
    rc = exec_command_rc;
  }

  exec_command_rc = Aca_Net_State_Handler::get_instance().update_port_states(
          parsed_struct, gsOperationReply);
  if (exec_command_rc != EXIT_SUCCESS) {
    ACA_LOG_ERROR("Failed to update port state. Failed with error code %d\n", exec_command_rc);
    rc = exec_command_rc;
  }

  auto end = chrono::steady_clock::now();

  auto message_total_operation_time = cast_to_nanoseconds(end - start).count();

  gsOperationReply.set_message_total_operation_time(message_total_operation_time);

  g_total_update_GS_time += message_total_operation_time;

  ACA_LOG_INFO("Elapsed time for message total operation took: %ld nanoseconds or %ld milliseconds.\n",
               message_total_operation_time, message_total_operation_time / 1000000);

  return rc;
}

void Aca_Comm_Manager::print_goal_state(GoalState parsed_struct)
{
  if (g_debug_mode == false) {
    return;
  }

  for (int i = 0; i < parsed_struct.port_states_size(); i++) {
    fprintf(stdout, "parsed_struct.port_states(%d).operation_type(): %d\n", i,
            parsed_struct.port_states(i).operation_type());

    PortConfiguration current_PortConfiguration =
            parsed_struct.port_states(i).configuration();

    fprintf(stdout, "current_PortConfiguration.version(): %d\n",
            current_PortConfiguration.version());

    fprintf(stdout, "current_PortConfiguration.project_id(): %s\n",
            current_PortConfiguration.project_id().c_str());

    fprintf(stdout, "current_PortConfiguration.network_id(): %s\n",
            current_PortConfiguration.network_id().c_str());

    fprintf(stdout, "current_PortConfiguration.id(): %s\n",
            current_PortConfiguration.id().c_str());

    fprintf(stdout, "current_PortConfiguration.name(): %s \n",
            current_PortConfiguration.name().c_str());

    fprintf(stdout, "current_PortConfiguration.network_ns(): %s \n",
            current_PortConfiguration.network_ns().c_str());

    fprintf(stdout, "current_PortConfiguration.mac_address(): %s \n",
            current_PortConfiguration.mac_address().c_str());

    fprintf(stdout, "current_PortConfiguration.veth_name(): %s \n",
            current_PortConfiguration.veth_name().c_str());

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

    for (int j = 0; j < current_PortConfiguration.security_group_ids_size(); j++) {
      fprintf(stdout, "current_PortConfiguration.security_group_ids(%d): id %s \n",
              j, current_PortConfiguration.security_group_ids(j).id().c_str());
    }

    for (int j = 0; j < current_PortConfiguration.allow_address_pairs_size(); j++) {
      fprintf(stdout, "current_PortConfiguration.allow_address_pairs(%d): ip_address %s, mac_address %s \n",
              j, current_PortConfiguration.allow_address_pairs(j).ip_address().c_str(),
              current_PortConfiguration.allow_address_pairs(j).mac_address().c_str());
    }

    printf("\n");
  }

  for (int i = 0; i < parsed_struct.subnet_states_size(); i++) {
    fprintf(stdout, "parsed_struct.subnet_states(%d).operation_type(): %d\n", i,
            parsed_struct.subnet_states(i).operation_type());

    SubnetConfiguration current_SubnetConfiguration =
            parsed_struct.subnet_states(i).configuration();

    fprintf(stdout, "current_SubnetConfiguration.version(): %d\n",
            current_SubnetConfiguration.version());

    fprintf(stdout, "current_SubnetConfiguration.project_id(): %s\n",
            current_SubnetConfiguration.project_id().c_str());

    fprintf(stdout, "current_SubnetConfiguration.vpc_id(): %s\n",
            current_SubnetConfiguration.vpc_id().c_str());

    fprintf(stdout, "current_SubnetConfiguration.id(): %s\n",
            current_SubnetConfiguration.id().c_str());

    fprintf(stdout, "current_SubnetConfiguration.name(): %s \n",
            current_SubnetConfiguration.name().c_str());

    fprintf(stdout, "current_SubnetConfiguration.cidr(): %s \n",
            current_SubnetConfiguration.cidr().c_str());

    fprintf(stdout, "current_SubnetConfiguration.tunnel_id(): %ld \n",
            current_SubnetConfiguration.tunnel_id());

    for (int j = 0; j < current_SubnetConfiguration.transit_switches_size(); j++) {
      fprintf(stdout, "current_SubnetConfiguration.transit_switches(%d).vpc_id(): %s \n",
              j, current_SubnetConfiguration.transit_switches(j).vpc_id().c_str());

      fprintf(stdout, "current_SubnetConfiguration.transit_switches(%d).subnet_id(): %s \n",
              j, current_SubnetConfiguration.transit_switches(j).subnet_id().c_str());

      fprintf(stdout, "current_SubnetConfiguration.transit_switches(%d).ip_address(): %s \n",
              j, current_SubnetConfiguration.transit_switches(j).ip_address().c_str());

      fprintf(stdout, "current_SubnetConfiguration.transit_switches(%d).mac_address(): %s \n",
              j, current_SubnetConfiguration.transit_switches(j).mac_address().c_str());
    }
    printf("\n");
  }

  for (int i = 0; i < parsed_struct.vpc_states_size(); i++) {
    fprintf(stdout, "parsed_struct.vpc_states(%d).operation_type(): %d\n", i,
            parsed_struct.vpc_states(i).operation_type());

    VpcConfiguration current_VpcConfiguration =
            parsed_struct.vpc_states(i).configuration();

    fprintf(stdout, "current_VpcConfiguration.version(): %d\n",
            current_VpcConfiguration.version());

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

    for (int l = 0; l < current_VpcConfiguration.transit_routers_size(); l++) {
      fprintf(stdout,
              "current_VpcConfiguration.transit_routers(%d).vpc_id(): "
              "%s \n",
              l, current_VpcConfiguration.transit_routers(l).vpc_id().c_str());

      fprintf(stdout,
              "current_VpcConfiguration.transit_routers(%d).ip_address(): "
              "%s \n",
              l, current_VpcConfiguration.transit_routers(l).ip_address().c_str());

      fprintf(stdout,
              "current_VpcConfiguration.transit_routers(%d).mac_address(): "
              "%s \n",
              l, current_VpcConfiguration.transit_routers(l).mac_address().c_str());
    }
    printf("\n");
  }
}

} // namespace aca_comm_manager
