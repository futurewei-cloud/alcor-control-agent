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
#include "messageconsumer.h"
#include "aca_async_grpc_server.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "cppkafka/utils/consumer_dispatcher.h"
#include <thread>
#include <unistd.h> /* for getopt */
#include <grpcpp/grpcpp.h>

using messagemanager::MessageConsumer;
using std::string;

// Defines
#define ACALOGNAME "AlcorControlAgent"
static char BROKER_LIST[] = "172.17.0.1:9092";
static char KAFKA_TOPIC[] = "Host-ts-1";
static char KAFKA_GROUP_ID[] = "test-group-id";
static char LOCALHOST[] = "localhost";
static char UDP_PROTOCOL[] = "udp";

using aca_comm_manager::Aca_Comm_Manager;
using namespace std;

// Global variables
cppkafka::ConsumerDispatcher *dispatcher = NULL;
std::thread *async_grpc_server_thread = NULL;
Aca_Async_GRPC_Server *async_grpc_server = NULL;
string g_broker_list = EMPTY_STRING;
string g_kafka_topic = EMPTY_STRING;
string g_kafka_group_id = EMPTY_STRING;
string g_rpc_server = EMPTY_STRING;
string g_rpc_protocol = EMPTY_STRING;
std::atomic_ulong g_total_rpc_call_time(0);
std::atomic_ulong g_total_rpc_client_time(0);
std::atomic_ulong g_total_network_configuration_time(0);
std::atomic_ulong g_total_update_GS_time(0);
bool g_demo_mode = false;
bool g_debug_mode = false;

static void aca_cleanup()
{
  ACA_LOG_DEBUG("g_total_rpc_call_time = %lu nanoseconds or %lu milliseconds\n",
                g_total_rpc_call_time.load(), g_total_rpc_call_time.load() / 1000000);

  ACA_LOG_DEBUG("g_total_rpc_client_time = %lu nanoseconds or %lu milliseconds\n",
                g_total_rpc_client_time.load(), g_total_rpc_client_time.load() / 1000000);

  ACA_LOG_DEBUG("g_total_network_configuration_time = %lu nanoseconds or %lu milliseconds\n",
                g_total_network_configuration_time.load(),
                g_total_network_configuration_time.load() / 1000000);

  ACA_LOG_DEBUG("g_total_update_GS_time = %lu nanoseconds or %lu milliseconds\n",
                g_total_update_GS_time.load(), g_total_update_GS_time.load() / 1000000);

  ACA_LOG_INFO("Program exiting, cleaning up...\n");

  // Optional: Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  // Stop sets a private variable running_ to False
  // The Dispatch checks the variable in a loop and stops when running is
  // no longer set to True.
  if (dispatcher != NULL) //Currently is always NULL
  {
    dispatcher->stop();
    delete dispatcher;
    dispatcher = NULL;
    ACA_LOG_INFO("Cleaned up Kafka dispatched consumer.\n");
  } else {
    ACA_LOG_ERROR("Unable to call delete, dispatcher pointer is null");
  }

  if (async_grpc_server != NULL) {
    async_grpc_server->StopServer();
    delete async_grpc_server;
    async_grpc_server = NULL;
    ACA_LOG_INFO("Cleaned up async grpc server.\n");
  } else {
    ACA_LOG_ERROR("Unable to call delete, async grpc server pointer is null.\n");
  }

  if (async_grpc_server_thread != NULL) {
    if (async_grpc_server_thread->joinable()) {
      async_grpc_server_thread->join();
      ACA_LOG_INFO("Joined GRPC server thread.\n");
    } else {
      ACA_LOG_ERROR("Async grpc server thread is not joinable.\n");
    }
    delete async_grpc_server_thread;
    async_grpc_server_thread = NULL;
    ACA_LOG_INFO("Cleaned up async grpc server thread.\n");
  } else {
    ACA_LOG_ERROR("Unable to call delete, async grpc server thread pointer is null.\n");
  }
  ACA_LOG_CLOSE();
}

// function to handle ctrl-c and kill process
static void aca_signal_handler(int sig_num)
{
  ACA_LOG_ERROR("Caught signal: %d\n", sig_num);

  // perform all the necessary cleanup here
  aca_cleanup();
  exit(sig_num);
}

int main(int argc, char *argv[])
{
  int option;
  int rc;

  ACA_LOG_INIT(ACALOGNAME);

  ACA_LOG_INFO("Alcor Control Agent started...\n");

  // Register the signal handlers
  signal(SIGINT, aca_signal_handler);
  signal(SIGTERM, aca_signal_handler);

  while ((option = getopt(argc, argv, "b:h:g:s:p:df")) != -1) {
    switch (option) {
    case 'b':
      g_broker_list = optarg;
      break;
    case 'h':
      g_kafka_topic = optarg;
      break;
    case 'g':
      g_kafka_group_id = optarg;
      break;
    case 's':
      g_rpc_server = optarg;
      break;
    case 'p':
      g_rpc_protocol = optarg;
      break;
    case 'm':
      g_demo_mode = true;
      break;
    case 'd':
      g_debug_mode = true;
      break;
    default: /* the '?' case when the option is not recognized */
      fprintf(stderr,
              "Usage: %s\n"
              "\t\t[-b kafka broker list]\n"
              "\t\t[-h kafka host topic to listen]\n"
              "\t\t[-g kafka group id]\n"
              "\t\t[-s transitd RPC server]\n"
              "\t\t[-p transitd RPC protocol]\n"
              "\t\t[-m enable demo mode]\n"
              "\t\t[-d enable debug mode]\n",
              argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  // fill in the information if not provided in command line args
  if (g_broker_list == EMPTY_STRING) {
    g_broker_list = BROKER_LIST;
  }
  if (g_kafka_topic == EMPTY_STRING) {
    g_kafka_topic = KAFKA_TOPIC;
  }
  if (g_kafka_group_id == EMPTY_STRING) {
    g_kafka_group_id = KAFKA_GROUP_ID;
  }
  if (g_rpc_server == EMPTY_STRING) {
    g_rpc_server = LOCALHOST;
  }
  if (g_rpc_protocol == EMPTY_STRING) {
    g_rpc_protocol = UDP_PROTOCOL;
  }

  async_grpc_server = new Aca_Async_GRPC_Server();
  async_grpc_server_thread =
          new std::thread(std::bind(&Aca_Async_GRPC_Server::Run, async_grpc_server));

  MessageConsumer network_config_consumer(g_broker_list, g_kafka_group_id);
  rc = network_config_consumer.consumeDispatched(g_kafka_topic);

  aca_cleanup();
  return rc;
}