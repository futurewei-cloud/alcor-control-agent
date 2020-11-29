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

// c includes
#include "aca_log.h"
#include "aca_util.h"
#include "aca_comm_mgr.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "goalstate.pb.h"
#include "cppkafka/buffer.h"
#include <unistd.h> /* for getopt */
#include <chrono>
#include <string.h>
#include <thread>
#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

#define ACALOGNAME "AlcorControlAgentTest"
static char EMPTY_STRING[] = "";
static char LOCALHOST[] = "localhost";
static char GRPC_PORT[] = "50001";

using namespace std;
using namespace alcor::schema;
using aca_comm_manager::Aca_Comm_Manager;

// Global variables
string g_grpc_server = EMPTY_STRING;
string g_grpc_port = EMPTY_STRING;
string g_ofctl_command = EMPTY_STRING;
string g_ofctl_target = EMPTY_STRING;
string g_ofctl_options = EMPTY_STRING;
std::atomic_ulong g_total_network_configuration_time(0);
std::atomic_ulong g_total_update_GS_time(0);
std::atomic_ulong g_total_ACA_Message_time(0);
bool g_demo_mode = false;
bool g_debug_mode = false;

static string project_id = "99d9d709-8478-4b46-9f3f-000000000000";
static string vpc_id_1 = "1b08a5bc-b718-11ea-b3de-111111111111";
static string subnet_id_1 = "27330ae4-b718-11ea-b3de-111111111111";
static string subnet1_gw_ip = "10.10.0.1";
static string subnet1_gw_mac = "fa:16:3e:d7:f2:11";
static string vmac_address_1 = "fa:16:3e:d7:f2:6c";

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::CompletionQueue;
using grpc::Status;
using std::string;

static void aca_cleanup()
{
  ACA_LOG_DEBUG("g_total_network_configuration_time = %lu nanoseconds or %lu milliseconds\n",
                g_total_network_configuration_time.load(),
                g_total_network_configuration_time.load() / 1000000);

  ACA_LOG_DEBUG("g_total_update_GS_time = %lu nanoseconds or %lu milliseconds\n",
                g_total_update_GS_time.load(), g_total_update_GS_time.load() / 1000000);

  ACA_LOG_INFO("%s", "Program exiting, cleaning up...\n");

  // Optional:  Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  ACA_LOG_CLOSE();
}

void print_goalstateReply(GoalStateOperationReply gsOperationReply)
{
  for (int i = 0; i < gsOperationReply.operation_statuses_size(); i++) {
    // ACA_LOG_DEBUG("gsOperationReply(%d) - resource_id: %s\n", i,
    //               gsOperationReply.operation_statuses(i).resource_id().c_str());
    ACA_LOG_DEBUG("gsOperationReply(%d) - resource_type: %d\n", i,
                  gsOperationReply.operation_statuses(i).resource_type());
    ACA_LOG_DEBUG("gsOperationReply(%d) - operation_type: %d\n", i,
                  gsOperationReply.operation_statuses(i).operation_type());
    ACA_LOG_DEBUG("gsOperationReply(%d) - operation_status: %d\n", i,
                  gsOperationReply.operation_statuses(i).operation_status());
    ACA_LOG_DEBUG("gsOperationReply(%d) - total_operation_time: %u nanoseconds or %u milliseconds\n",
                  i, gsOperationReply.operation_statuses(i).state_elapse_time(),
                  gsOperationReply.operation_statuses(i).state_elapse_time() / 1000000);
  }

  ACA_LOG_DEBUG("[METRICS] ACA message_total_operation_time: %u nanoseconds or %u milliseconds\n",
                gsOperationReply.message_total_operation_time(),
                gsOperationReply.message_total_operation_time() / 1000000);

  g_total_ACA_Message_time += gsOperationReply.message_total_operation_time();
}

class GoalStateProvisionerClient {
  public:
  explicit GoalStateProvisionerClient(std::shared_ptr<Channel> channel)
          : stub_(GoalStateProvisioner::NewStub(channel))
  {
  }

  int send_goalstate_async(GoalState &GoalState, GoalStateOperationReply &reply)
  {
    ClientContext context;
    CompletionQueue cq;
    Status status;

    auto before_rpc_ptr = std::chrono::steady_clock::now();

    std::unique_ptr<ClientAsyncResponseReader<GoalStateOperationReply> > rpc(
            stub_->PrepareAsyncPushNetworkResourceStates(&context, GoalState, &cq));

    auto after_rpc_ptr = std::chrono::steady_clock::now();

    auto rpc_ptr_ns = cast_to_nanoseconds(after_rpc_ptr - before_rpc_ptr).count();

    ACA_LOG_INFO("[METRICS] rpc_ptr took: %ld nanoseconds or %ld milliseconds\n",
                 rpc_ptr_ns, rpc_ptr_ns / 1000000);

    rpc->StartCall();

    auto after_start_call = std::chrono::steady_clock::now();

    auto start_call_ns = cast_to_nanoseconds(after_start_call - after_rpc_ptr).count();

    ACA_LOG_INFO("[METRICS] start_call took: %ld nanoseconds or %ld milliseconds\n",
                 start_call_ns, start_call_ns / 1000000);

    rpc->Finish(&reply, &status, (void *)1);

    auto after_finish = std::chrono::steady_clock::now();

    auto finish_ns = cast_to_nanoseconds(after_finish - after_start_call).count();

    ACA_LOG_INFO("[METRICS] finish took: %ld nanoseconds or %ld milliseconds\n",
                 finish_ns, finish_ns / 1000000);

    auto total_ns = cast_to_nanoseconds(after_finish - before_rpc_ptr).count();

    ACA_LOG_INFO("[METRICS] total async took: %ld nanoseconds or %ld milliseconds\n",
                 total_ns, total_ns / 1000000);

    void *got_tag;
    bool ok = false;

    GPR_ASSERT(cq.Next(&got_tag, &ok));
    GPR_ASSERT(got_tag == (void *)1);
    GPR_ASSERT(ok);

    if (status.ok()) {
      return EXIT_SUCCESS;
    } else {
      return EXIT_FAILURE;
    }
  }

  void pushNetworkResourceStates_sync(GoalState &GoalState, GoalStateOperationReply &reply)
  {
    ClientContext context;

    auto before_sync_call = std::chrono::steady_clock::now();

    Status status = stub_->PushNetworkResourceStates(&context, GoalState, &reply);

    auto after_sync_call = std::chrono::steady_clock::now();

    auto sync_call_ns = cast_to_nanoseconds(after_sync_call - before_sync_call).count();

    ACA_LOG_INFO("[METRICS] PushNetworkResourceStates sync call took: %ld nanoseconds or %ld milliseconds\n",
                 sync_call_ns, sync_call_ns / 1000000);

    if (!status.ok()) {
      ACA_LOG_ERROR("%s", "RPC call failed\n");
    }
  }

  // working ip prefix = 1-254
  void send_goalstate_sync(uint states_to_create, string ip_prefix)
  {
    ClientContext context;

    g_total_ACA_Message_time = 0;

    auto before_send_goalstate = std::chrono::steady_clock::now();

    GoalState GoalState_builder;

    SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
    new_subnet_states->set_operation_type(OperationType::INFO);

    SubnetConfiguration *SubnetConiguration_builder =
            new_subnet_states->mutable_configuration();
    SubnetConiguration_builder->set_revision_number(1);
    SubnetConiguration_builder->set_vpc_id(vpc_id_1);
    SubnetConiguration_builder->set_id(subnet_id_1);
    SubnetConiguration_builder->set_cidr("10.0.0.0/24");
    SubnetConiguration_builder->set_tunnel_id(states_to_create);

    auto *subnetConfig_GatewayBuilder(new SubnetConfiguration_Gateway);
    subnetConfig_GatewayBuilder->set_ip_address(subnet1_gw_ip);
    subnetConfig_GatewayBuilder->set_mac_address(subnet1_gw_mac);
    SubnetConiguration_builder->set_allocated_gateway(subnetConfig_GatewayBuilder);

    NeighborState *new_neighbor_states = GoalState_builder.add_neighbor_states();
    new_neighbor_states->set_operation_type(OperationType::CREATE);
    NeighborConfiguration *NeighborConfiguration_builder =
            new_neighbor_states->mutable_configuration();
    NeighborConfiguration_builder->set_revision_number(1);

    NeighborConfiguration_builder->set_vpc_id(vpc_id_1);
    NeighborConfiguration_builder->set_mac_address(vmac_address_1);

    NeighborConfiguration_FixedIp *FixedIp_builder =
            NeighborConfiguration_builder->add_fixed_ips();
    FixedIp_builder->set_neighbor_type(NeighborType::L2);
    FixedIp_builder->set_subnet_id(subnet_id_1);

    for (uint i = 0; i < states_to_create; i++) {
      string i_string = std::to_string(i);
      string port_name = ip_prefix + "-port-" + i_string;
      GoalStateOperationReply reply;

      NeighborConfiguration_builder->set_name(port_name);
      NeighborConfiguration_builder->set_host_ip_address(ip_prefix + ".0.0." + i_string);

      FixedIp_builder->set_ip_address(ip_prefix + ".0.0." + i_string);

      pushNetworkResourceStates_sync(GoalState_builder, reply);

      print_goalstateReply(reply);
    }

    auto after_send_goalstate = std::chrono::steady_clock::now();

    auto send_goalstate_ns =
            cast_to_nanoseconds(after_send_goalstate - before_send_goalstate).count();

    ACA_LOG_INFO("[***METRICS***] Grand ACA message_total_operation_time: %lu nanoseconds or %lu milliseconds\n",
                 g_total_ACA_Message_time.load(), g_total_ACA_Message_time.load() / 1000000);

    ACA_LOG_INFO("[***METRICS***] GRPC E2E send_goalstate_sync call took: %ld nanoseconds or %ld milliseconds\n",
                 send_goalstate_ns, send_goalstate_ns / 1000000);

    ACA_LOG_INFO("[***METRICS***] Total GRPC latency/usage for sync call: %ld nanoseconds or %ld milliseconds\n",
                 send_goalstate_ns - g_total_ACA_Message_time.load(),
                 (send_goalstate_ns - g_total_ACA_Message_time.load()) / 1000000);
  }

  int send_goalstate_stream_one(GoalState &goalState, GoalStateOperationReply &gsOperationReply)
  {
    ClientContext context;

    auto before_stream_create = std::chrono::steady_clock::now();

    std::shared_ptr<ClientReaderWriter<GoalState, GoalStateOperationReply> > stream(
            stub_->PushNetworkResourceStatesStream(&context));

    auto after_stream_create = std::chrono::steady_clock::now();

    auto stream_create_ns =
            cast_to_nanoseconds(after_stream_create - before_stream_create).count();

    ACA_LOG_INFO("[METRICS] stream_create call took: %ld nanoseconds or %ld milliseconds\n",
                 stream_create_ns, stream_create_ns / 1000000);

    std::thread writer([stream, goalState]() {
      stream->Write(goalState);
      stream->WritesDone();
    });

    auto after_write_done = std::chrono::steady_clock::now();

    auto write_done_ns =
            cast_to_nanoseconds(after_write_done - after_stream_create).count();

    ACA_LOG_INFO("[METRICS] write_done call took: %ld nanoseconds or %ld milliseconds\n",
                 write_done_ns, write_done_ns / 1000000);

    while (stream->Read(&gsOperationReply)) {
      ACA_LOG_INFO("%s", "Received one streaming GoalStateOperationReply\n");
    }

    writer.join();
    Status status = stream->Finish();

    if (status.ok()) {
      return EXIT_SUCCESS;
    } else {
      return EXIT_FAILURE;
    }
  }

  // working ip prefix = 1-254
  void send_goalstate_stream(uint states_to_create, string ip_prefix)
  {
    ClientContext context;

    g_total_ACA_Message_time = 0;

    auto before_send_goalstate = std::chrono::steady_clock::now();

    auto before_stream_create = std::chrono::steady_clock::now();

    std::shared_ptr<ClientReaderWriter<GoalState, GoalStateOperationReply> > stream(
            stub_->PushNetworkResourceStatesStream(&context));

    auto after_stream_create = std::chrono::steady_clock::now();

    auto stream_create_ns =
            cast_to_nanoseconds(after_stream_create - before_stream_create).count();

    ACA_LOG_INFO("[METRICS] stream_create call took: %ld nanoseconds or %ld milliseconds\n",
                 stream_create_ns, stream_create_ns / 1000000);

    std::thread writer([stream, states_to_create, ip_prefix]() {
      GoalState GoalState_builder;

      SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
      new_subnet_states->set_operation_type(OperationType::INFO);

      SubnetConfiguration *SubnetConiguration_builder =
              new_subnet_states->mutable_configuration();
      SubnetConiguration_builder->set_revision_number(1);
      SubnetConiguration_builder->set_vpc_id(vpc_id_1);
      SubnetConiguration_builder->set_id(subnet_id_1);
      SubnetConiguration_builder->set_cidr("10.0.0.0/24");
      SubnetConiguration_builder->set_tunnel_id(states_to_create);

      auto *subnetConfig_GatewayBuilder(new SubnetConfiguration_Gateway);
      subnetConfig_GatewayBuilder->set_ip_address(subnet1_gw_ip);
      subnetConfig_GatewayBuilder->set_mac_address(subnet1_gw_mac);
      SubnetConiguration_builder->set_allocated_gateway(subnetConfig_GatewayBuilder);

      NeighborState *new_neighbor_states = GoalState_builder.add_neighbor_states();
      new_neighbor_states->set_operation_type(OperationType::CREATE);
      NeighborConfiguration *NeighborConfiguration_builder =
              new_neighbor_states->mutable_configuration();
      NeighborConfiguration_builder->set_revision_number(1);

      NeighborConfiguration_builder->set_vpc_id(vpc_id_1);
      NeighborConfiguration_builder->set_mac_address(vmac_address_1);

      NeighborConfiguration_FixedIp *FixedIp_builder =
              NeighborConfiguration_builder->add_fixed_ips();
      FixedIp_builder->set_neighbor_type(NeighborType::L2);
      FixedIp_builder->set_subnet_id(subnet_id_1);

      for (uint i = 0; i < states_to_create; i++) {
        string i_string = std::to_string(i);
        string port_name = ip_prefix + "-port-" + i_string;
        GoalStateOperationReply reply;

        NeighborConfiguration_builder->set_name(port_name);
        NeighborConfiguration_builder->set_host_ip_address(ip_prefix + ".0.0." + i_string);

        FixedIp_builder->set_ip_address(ip_prefix + ".0.0." + i_string);

        stream->Write(GoalState_builder);
      }

      stream->WritesDone();
    });

    auto after_write_done = std::chrono::steady_clock::now();

    auto write_done_ns =
            cast_to_nanoseconds(after_write_done - after_stream_create).count();

    ACA_LOG_INFO("[METRICS] write_done call took: %ld nanoseconds or %ld milliseconds\n",
                 write_done_ns, write_done_ns / 1000000);

    GoalStateOperationReply gsOperationReply;
    while (stream->Read(&gsOperationReply)) {
      // ACA_LOG_INFO("Received one streaming GoalStateOperationReply\n");
      print_goalstateReply(gsOperationReply);
    }

    writer.join();
    Status status = stream->Finish();

    auto after_send_goalstate = std::chrono::steady_clock::now();

    auto send_goalstate_ns =
            cast_to_nanoseconds(after_send_goalstate - before_send_goalstate).count();

    ACA_LOG_INFO("[***METRICS***] Grand ACA message_total_operation_time: %lu nanoseconds or %lu milliseconds\n",
                 g_total_ACA_Message_time.load(), g_total_ACA_Message_time.load() / 1000000);

    ACA_LOG_INFO("[***METRICS***] Grand send_goalstate_sync call took: %ld nanoseconds or %ld milliseconds\n",
                 send_goalstate_ns, send_goalstate_ns / 1000000);

    ACA_LOG_INFO("[***METRICS***] Total GRPC latency/usage for stream call: %ld nanoseconds or %ld milliseconds\n",
                 send_goalstate_ns - g_total_ACA_Message_time.load(),
                 (send_goalstate_ns - g_total_ACA_Message_time.load()) / 1000000);

    if (!status.ok()) {
      ACA_LOG_ERROR("%s", "RPC call failed\n");
    }
  }

  private:
  std::unique_ptr<GoalStateProvisioner::Stub> stub_;
};

// function to handle ctrl-c and kill process
static void aca_signal_handler(int sig_num)
{
  fprintf(stdout, "Caught signal: %d\n", sig_num);

  // perform all the necessary cleanup here
  aca_cleanup();

  exit(sig_num);
}

void parse_goalstate(GoalState parsed_struct, GoalState GoalState_builder)
{
  assert(parsed_struct.port_states_size() == GoalState_builder.port_states_size());
  for (int i = 0; i < parsed_struct.port_states_size(); i++) {
    assert(parsed_struct.port_states(i).operation_type() ==
           GoalState_builder.port_states(i).operation_type());

    assert(parsed_struct.port_states(i).configuration().id() ==
           GoalState_builder.port_states(i).configuration().id());

    assert(parsed_struct.port_states(i).configuration().name() ==
           GoalState_builder.port_states(i).configuration().name());

    assert(parsed_struct.port_states(i).configuration().name() ==
           GoalState_builder.port_states(i).configuration().name());

    assert(parsed_struct.port_states(i).configuration().mac_address() ==
           GoalState_builder.port_states(i).configuration().mac_address());

    assert(parsed_struct.port_states(i).configuration().host_info().ip_address() ==
           GoalState_builder.port_states(i).configuration().host_info().ip_address());

    assert(parsed_struct.port_states(i).configuration().host_info().mac_address() ==
           GoalState_builder.port_states(i).configuration().host_info().mac_address());

    assert(parsed_struct.port_states(i).configuration().fixed_ips_size() ==
           GoalState_builder.port_states(i).configuration().fixed_ips_size());
    for (int j = 0;
         j < parsed_struct.port_states(i).configuration().fixed_ips_size(); j++) {
      assert(parsed_struct.port_states(i).configuration().fixed_ips(j).subnet_id() ==
             GoalState_builder.port_states(i).configuration().fixed_ips(j).subnet_id());

      assert(parsed_struct.port_states(i).configuration().fixed_ips(j).ip_address() ==
             GoalState_builder.port_states(i).configuration().fixed_ips(j).ip_address());
    }

    assert(parsed_struct.port_states(i).configuration().security_group_ids_size() ==
           GoalState_builder.port_states(i).configuration().security_group_ids_size());
    for (int j = 0;
         j < parsed_struct.port_states(i).configuration().security_group_ids_size(); j++) {
      assert(parsed_struct.port_states(i).configuration().security_group_ids(j).id() ==
             GoalState_builder.port_states(i)
                     .configuration()
                     .security_group_ids(j)
                     .id());
    }

    assert(parsed_struct.port_states(i).configuration().allow_address_pairs_size() ==
           GoalState_builder.port_states(i).configuration().allow_address_pairs_size());
    for (int j = 0;
         j < parsed_struct.port_states(i).configuration().allow_address_pairs_size(); j++) {
      assert(parsed_struct.port_states(i).configuration().allow_address_pairs(j).ip_address() ==
             GoalState_builder.port_states(i)
                     .configuration()
                     .allow_address_pairs(j)
                     .ip_address());

      assert(parsed_struct.port_states(i).configuration().allow_address_pairs(j).mac_address() ==
             GoalState_builder.port_states(i)
                     .configuration()
                     .allow_address_pairs(j)
                     .mac_address());
    }
  }
  assert(parsed_struct.subnet_states_size() == GoalState_builder.subnet_states_size());

  for (int i = 0; i < parsed_struct.subnet_states_size(); i++) {
    assert(parsed_struct.subnet_states(i).operation_type() ==
           GoalState_builder.subnet_states(i).operation_type());

    assert(parsed_struct.subnet_states(i).configuration().vpc_id() ==
           GoalState_builder.subnet_states(i).configuration().vpc_id());

    assert(parsed_struct.subnet_states(i).configuration().id() ==
           GoalState_builder.subnet_states(i).configuration().id());

    assert(parsed_struct.subnet_states(i).configuration().name() ==
           GoalState_builder.subnet_states(i).configuration().name());

    assert(parsed_struct.subnet_states(i).configuration().cidr() ==
           GoalState_builder.subnet_states(i).configuration().cidr());
  }

  assert(parsed_struct.vpc_states_size() == GoalState_builder.vpc_states_size());

  for (int i = 0; i < parsed_struct.vpc_states_size(); i++) {
    assert(parsed_struct.vpc_states(i).operation_type() ==
           GoalState_builder.vpc_states(i).operation_type());

    assert(parsed_struct.vpc_states(i).configuration().project_id() ==
           GoalState_builder.vpc_states(i).configuration().project_id());

    assert(parsed_struct.vpc_states(i).configuration().id() ==
           GoalState_builder.vpc_states(i).configuration().id());

    assert(parsed_struct.vpc_states(i).configuration().name() ==
           GoalState_builder.vpc_states(i).configuration().name());

    assert(parsed_struct.vpc_states(i).configuration().cidr() ==
           GoalState_builder.vpc_states(i).configuration().cidr());

    assert(parsed_struct.vpc_states(i).configuration().tunnel_id() ==
           GoalState_builder.vpc_states(i).configuration().tunnel_id());

    assert(parsed_struct.vpc_states(i).configuration().subnet_ids_size() ==
           GoalState_builder.vpc_states(i).configuration().subnet_ids_size());

    for (int j = 0;
         j < parsed_struct.vpc_states(i).configuration().subnet_ids_size(); j++) {
      assert(parsed_struct.vpc_states(i).configuration().subnet_ids(j).id() ==
             GoalState_builder.vpc_states(i).configuration().subnet_ids(j).id());
    }
  }

  fprintf(stdout, "All content matched!\n");
}

int main(int argc, char *argv[])
{
  int option;
  int rc;
  ACA_LOG_INIT(ACALOGNAME);

  // Register the signal handlers
  signal(SIGINT, aca_signal_handler);
  signal(SIGTERM, aca_signal_handler);

  while ((option = getopt(argc, argv, "s:p:d")) != -1) {
    switch (option) {
    case 's':
      g_grpc_server = optarg;
      break;
    case 'p':
      g_grpc_port = optarg;
      break;
    case 'd':
      g_debug_mode = true;
      break;
    default: /* the '?' case when the option is not recognized */
      fprintf(stderr,
              "Usage: %s\n"
              "\t\t[-s grpc server]\n"
              "\t\t[-p grpc port]\n"
              "\t\t[-d enable debug mode]\n",
              argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  // fill in the grpc server and protocol if it is not provided in
  // command line arg
  if (g_grpc_server == EMPTY_STRING) {
    g_grpc_server = LOCALHOST;
  }
  if (g_grpc_port == EMPTY_STRING) {
    g_grpc_port = GRPC_PORT;
  }

  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  string port_name_postfix = "11111111-2222-3333-4444-555555555555";
  string ip_address_prefix = "10.0.0.";
  string remote_ip_address_prefix = "123.0.0.";

  GoalState GoalState_builder;
  NeighborState *new_neighbor_states;
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
  new_subnet_states->set_operation_type(OperationType::INFO);

  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_revision_number(1);
  SubnetConiguration_builder->set_vpc_id("99d9d709-8478-4b46-9f3f-000000000000");
  SubnetConiguration_builder->set_id("27330ae4-b718-11ea-b3de-111111111111");
  SubnetConiguration_builder->set_cidr("10.0.0.0/24");
  SubnetConiguration_builder->set_tunnel_id(123);

  auto *subnetConfig_GatewayBuilder(new SubnetConfiguration_Gateway);
  subnetConfig_GatewayBuilder->set_ip_address("10.10.0.1");
  subnetConfig_GatewayBuilder->set_mac_address("fa:16:3e:d7:f2:11");
  SubnetConiguration_builder->set_allocated_gateway(subnetConfig_GatewayBuilder);

  new_neighbor_states = GoalState_builder.add_neighbor_states();
  new_neighbor_states->set_operation_type(OperationType::CREATE);

  NeighborConfiguration *NeighborConfiguration_builder =
          new_neighbor_states->mutable_configuration();
  NeighborConfiguration_builder->set_revision_number(1);

  NeighborConfiguration_builder->set_vpc_id("1b08a5bc-b718-11ea-b3de-111122223333");
  NeighborConfiguration_builder->set_name("portname1");
  NeighborConfiguration_builder->set_mac_address("fa:16:3e:d7:f2:6c");
  NeighborConfiguration_builder->set_host_ip_address("111.0.0.11");

  NeighborConfiguration_FixedIp *FixedIp_builder =
          NeighborConfiguration_builder->add_fixed_ips();
  FixedIp_builder->set_neighbor_type(NeighborType::L2);
  FixedIp_builder->set_subnet_id("27330ae4-b718-11ea-b3de-111111111111");
  FixedIp_builder->set_ip_address("11.0.0.11");

  string string_message;

  // Serialize it to string
  GoalState_builder.SerializeToString(&string_message);
  fprintf(stdout, "(NOT USED) Serialized protobuf string: %s\n", string_message.c_str());

  // Serialize it to binary array
  size_t size = GoalState_builder.ByteSize();
  char *buffer = (char *)malloc(size);
  GoalState_builder.SerializeToArray(buffer, size);
  string binary_message(buffer, size);
  fprintf(stdout, "(USING THIS) Serialized protobuf binary array: %s\n",
          binary_message.c_str());

  GoalState parsed_struct;

  rc = Aca_Comm_Manager::get_instance().deserialize(
          (const unsigned char *)buffer, size, parsed_struct);

  if (buffer != NULL) {
    free(buffer);
    buffer = NULL;
  }

  if (rc == EXIT_SUCCESS) {
    fprintf(stdout, "Deserialize succeeded, comparing the content now...\n");
    parse_goalstate(parsed_struct, GoalState_builder);
  } else {
    fprintf(stdout, "Deserialize failed with error code: %u\n", rc);
  }

  ACA_LOG_INFO("%s", "-------------- setup local grpc client --------------\n");

  auto before_grpc_client = std::chrono::steady_clock::now();

  GoalStateProvisionerClient grpc_client(grpc::CreateChannel(
          g_grpc_server + ":" + g_grpc_port, grpc::InsecureChannelCredentials()));

  auto after_grpc_client = std::chrono::steady_clock::now();

  auto async_client_ns =
          cast_to_nanoseconds(after_grpc_client - before_grpc_client).count();

  ACA_LOG_INFO("[METRICS] grpc_client took: %ld nanoseconds or %ld milliseconds\n",
               async_client_ns, async_client_ns / 1000000);

  ACA_LOG_INFO("%s", "-------------- sending one goal state async --------------\n");

  GoalStateOperationReply async_reply;

  auto before_send_goalstate = std::chrono::steady_clock::now();

  rc = grpc_client.send_goalstate_async(GoalState_builder, async_reply);

  auto after_send_goalstate = std::chrono::steady_clock::now();

  auto send_goalstate_ns =
          cast_to_nanoseconds(after_send_goalstate - before_send_goalstate).count();

  ACA_LOG_INFO("[***METRICS***] send_goalstate_async call took: %ld nanoseconds or %ld milliseconds\n",
               send_goalstate_ns, send_goalstate_ns / 1000000);

  print_goalstateReply(async_reply);

  if (rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "1 goal state async grpc call succeed\n");
  } else {
    ACA_LOG_INFO("%s", "1 goal state async grpc call failed!!!\n");
  }

  const uint TEST_ITERATIONS = 10;

  for (uint i = 0; i < TEST_ITERATIONS; i++) {
    ACA_LOG_INFO("************** Iteration #%u **************\n", i);

    ACA_LOG_INFO("%s", "-------------- sending one goal state sync --------------\n");

    grpc_client.send_goalstate_sync(1, "21");

    ACA_LOG_INFO("%s", "-------------- sending 10 goal state sync --------------\n");

    grpc_client.send_goalstate_sync(10, "22");

    ACA_LOG_INFO("%s", "-------------- sending 1 goal state stream --------------\n");

    NeighborConfiguration_builder->set_name("portname3");
    NeighborConfiguration_builder->set_host_ip_address("223.0.0.33");
    FixedIp_builder->set_ip_address("33.0.0.33");

    GoalStateOperationReply stream_reply;

    before_send_goalstate = std::chrono::steady_clock::now();

    rc = grpc_client.send_goalstate_stream_one(GoalState_builder, stream_reply);

    after_send_goalstate = std::chrono::steady_clock::now();

    send_goalstate_ns =
            cast_to_nanoseconds(after_send_goalstate - before_send_goalstate).count();

    ACA_LOG_INFO("[***METRICS***] send_goalstate_stream_one call took: %ld nanoseconds or %ld milliseconds\n",
                 send_goalstate_ns, send_goalstate_ns / 1000000);

    print_goalstateReply(stream_reply);

    if (rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("%s", "1 goal state stream grpc call succeed\n");
    } else {
      ACA_LOG_INFO("%s", "1 goal state stream grpc call failed!!!\n");
    }

    ACA_LOG_INFO("%s", "-------------- sending 10 goal state stream --------------\n");

    grpc_client.send_goalstate_stream(10, "32");
  }

  aca_cleanup();

  return rc;
}