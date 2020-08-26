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
std::atomic_ulong g_total_network_configuration_time(0);
std::atomic_ulong g_total_update_GS_time(0);
bool g_demo_mode = false;
bool g_debug_mode = false;

string g_ofctl_command = EMPTY_STRING;
string g_ofctl_target = EMPTY_STRING;
string g_ofctl_options = EMPTY_STRING;

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
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

  ACA_LOG_INFO("Program exiting, cleaning up...\n");

  // Optional:  Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  ACA_LOG_CLOSE();
}

class GoalStateProvisionerClient {
  public:
  explicit GoalStateProvisionerClient(std::shared_ptr<Channel> channel)
          : stub_(GoalStateProvisioner::NewStub(channel))
  {
  }

  int send_goalstate(GoalState &GoalState)
  {
    GoalStateOperationReply reply;
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

    assert(parsed_struct.port_states(i).configuration().format_version() ==
           GoalState_builder.port_states(i).configuration().format_version());

    assert(parsed_struct.port_states(i).configuration().project_id() ==
           GoalState_builder.port_states(i).configuration().project_id());

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

    assert(parsed_struct.subnet_states(i).configuration().format_version() ==
           GoalState_builder.subnet_states(i).configuration().format_version());

    assert(parsed_struct.subnet_states(i).configuration().project_id() ==
           GoalState_builder.subnet_states(i).configuration().project_id());

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

    assert(parsed_struct.vpc_states(i).configuration().format_version() ==
           GoalState_builder.vpc_states(i).configuration().format_version());

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

    assert(parsed_struct.vpc_states(i).configuration().routes_size() ==
           GoalState_builder.vpc_states(i).configuration().routes_size());

    for (int k = 0; k < parsed_struct.vpc_states(i).configuration().routes_size(); k++) {
      assert(parsed_struct.vpc_states(i).configuration().routes(k).destination() ==
             GoalState_builder.vpc_states(i).configuration().routes(k).destination());

      assert(parsed_struct.vpc_states(i).configuration().routes(k).next_hop() ==
             GoalState_builder.vpc_states(i).configuration().routes(k).next_hop());
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

  GoalState GoalState_builder;
  PortState *new_port_states = GoalState_builder.add_port_states();
  SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
  VpcState *new_vpc_states = GoalState_builder.add_vpc_states();

  // fill in port state structs
  new_port_states->set_operation_type(OperationType::CREATE);

  // this will allocate new PortConfiguration, will need to free it later
  PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
  PortConfiguration_builder->set_format_version(1);

  PortConfiguration_builder->set_revision_number(1);
  PortConfiguration_builder->set_message_type(MessageType::FULL);
  PortConfiguration_builder->set_id("dd12d1dadad2g4h");

  PortConfiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
  PortConfiguration_builder->set_vpc_id("vpc1");
  PortConfiguration_builder->set_name("Peer1");
  PortConfiguration_builder->set_mac_address("fa:16:3e:d7:f2:6c");
  PortConfiguration_builder->set_admin_state_up(true);

  // this will allocate new PortConfiguration_FixedIp may need to free later
  PortConfiguration_FixedIp *PortIp_builder = PortConfiguration_builder->add_fixed_ips();
  PortIp_builder->set_ip_address("10.0.0.2");
  PortIp_builder->set_subnet_id("superSubnetID");
  // this will allocate new PortConfiguration_SecurityGroupId may need to free later
  PortConfiguration_SecurityGroupId *SecurityGroup_builder =
          PortConfiguration_builder->add_security_group_ids();
  SecurityGroup_builder->set_id("1");
  // this will allocate new PortConfiguration_AllowAddressPair may need to free later
  PortConfiguration_AllowAddressPair *AddressPair_builder =
          PortConfiguration_builder->add_allow_address_pairs();
  AddressPair_builder->set_ip_address("10.0.0.5");
  AddressPair_builder->set_mac_address("fa:16:3e:d7:f2:9f");

  // fill in the subnet state structs
  new_subnet_states->set_operation_type(OperationType::INFO);

  // this will allocate new SubnetConfiguration, will need to free it later
  SubnetConfiguration *SubnetConiguration_builder =
          new_subnet_states->mutable_configuration();
  SubnetConiguration_builder->set_format_version(1);
  SubnetConiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-111111111111");
  // VpcConiguration_builder->set_id("99d9d709-8478-4b46-9f3f-2206b1023fd3");
  SubnetConiguration_builder->set_vpc_id("99d9d709-8478-4b46-9f3f-2206b1023fd3");
  SubnetConiguration_builder->set_id("superSubnetID");
  SubnetConiguration_builder->set_name("SuperSubnet");
  SubnetConiguration_builder->set_cidr("10.0.0.1/16");
  SubnetConiguration_builder->set_tunnel_id(22222);

  // fill in the vpc state structs
  new_vpc_states->set_operation_type(OperationType::CREATE);

  // this will allocate new VpcConfiguration, will need to free it later
  VpcConfiguration *VpcConiguration_builder = new_vpc_states->mutable_configuration();
  VpcConiguration_builder->set_format_version(1);
  VpcConiguration_builder->set_project_id("dbf72700-5106-4a7a-918f-a016853911f8");
  // VpcConiguration_builder->set_id("99d9d709-8478-4b46-9f3f-2206b1023fd3");
  VpcConiguration_builder->set_id("vpc1");
  VpcConiguration_builder->set_name("SuperVpc");
  VpcConiguration_builder->set_cidr("192.168.0.0/24");
  VpcConiguration_builder->set_tunnel_id(11111);

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

  cppkafka::Buffer kafka_buffer(buffer, size);

  rc = Aca_Comm_Manager::get_instance().deserialize(&kafka_buffer, parsed_struct);

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

  // send the goal state to a local client via grpc for testing
  GoalStateProvisionerClient async_client(grpc::CreateChannel(
          g_grpc_server + ":" + g_grpc_port, grpc::InsecureChannelCredentials()));

  rc = async_client.send_goalstate(GoalState_builder);
  if (rc == EXIT_SUCCESS) {
    fprintf(stdout, "RPC Sucess\n");
  } else {
    fprintf(stdout, "RPC Failure\n");
  }

  // free the allocated configurations since we are done with it now
  new_port_states->clear_configuration();
  new_subnet_states->clear_configuration();
  new_vpc_states->clear_configuration();

  aca_cleanup();

  return rc;
}
