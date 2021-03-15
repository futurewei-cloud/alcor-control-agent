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
#include "aca_ovs_control.h"
#include "aca_message_pulsar_consumer.h"
#include "aca_grpc.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_ovs_control.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <thread>
#include <unistd.h> /* for getopt */
#include <grpcpp/grpcpp.h>

using aca_message_pulsar::ACA_Message_Pulsar_Consumer;
using aca_ovs_control::ACA_OVS_Control;
using std::string;

// Defines
#define ACALOGNAME "AlcorControlAgent"
static char EMPTY_STRING[] = "";
static char BROKER_LIST[] = "pulsar://localhost:6502";
static char PULSAR_TOPIC[] = "Host-ts-1";
static char PULSAR_SUBSCRIPTION_NAME[] = "Test-Subscription";
static char GRPC_SERVER_PORT[] = "50001";
static char OFCTL_COMMAND[] = "monitor";
static char OFCTL_TARGET[] = "br-int";

using namespace std;

// Global variables
std::thread *g_grpc_server_thread = NULL;
std::thread *ovs_monitor_brtun_thread = NULL;
std::thread *ovs_monitor_brint_thread = NULL;
GoalStateProvisionerImpl *g_grpc_server = NULL;
string g_broker_list = EMPTY_STRING;
string g_pulsar_topic = EMPTY_STRING;
string g_pulsar_subsription_name = EMPTY_STRING;
string g_grpc_server_port = EMPTY_STRING;
string g_ofctl_command = EMPTY_STRING;
string g_ofctl_target = EMPTY_STRING;
string g_ofctl_options = EMPTY_STRING;
string g_ncm_address = EMPTY_STRING;
string g_ncm_port = EMPTY_STRING;

// total time for execute_system_command in microseconds
std::atomic_ulong g_total_execute_system_time(0);
// total time for execute_ovsdb_command in microseconds
std::atomic_ulong g_total_execute_ovsdb_time(0);
// total time for execute_openflow_command in microseconds
std::atomic_ulong g_total_execute_openflow_time(0);
// total time for vpcs_table_mutex in microseconds
std::atomic_ulong g_total_vpcs_table_mutex_time(0);
// total time for goal state update in microseconds
std::atomic_ulong g_total_update_GS_time(0);

bool g_demo_mode = false;
bool g_debug_mode = false;

static void aca_cleanup()
{
  ACA_LOG_DEBUG("g_total_execute_system_time = %lu microseconds or %lu milliseconds\n",
                g_total_execute_system_time.load(),
                us_to_ms(g_total_execute_system_time.load()));

  ACA_LOG_DEBUG("g_total_execute_ovsdb_time = %lu microseconds or %lu milliseconds\n",
                g_total_execute_ovsdb_time.load(),
                us_to_ms(g_total_execute_ovsdb_time.load()));

  ACA_LOG_DEBUG("g_total_execute_openflow_time = %lu microseconds or %lu milliseconds\n",
                g_total_execute_openflow_time.load(),
                us_to_ms(g_total_execute_openflow_time.load()));

  ACA_LOG_DEBUG("g_total_vpcs_table_mutex_time = %lu microseconds or %lu milliseconds\n",
                g_total_vpcs_table_mutex_time.load(),
                us_to_ms(g_total_vpcs_table_mutex_time.load()));

  ACA_LOG_DEBUG("g_total_update_GS_time = %lu microseconds or %lu milliseconds\n",
                g_total_update_GS_time.load(), us_to_ms(g_total_update_GS_time.load()));

  ACA_LOG_INFO("%s", "Program exiting, cleaning up...\n");

  // Optional: Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  // Stop sets a private variable running_ to False
  // The Dispatch checks the variable in a loop and stops when running is
  // no longer set to True.

  if (g_grpc_server != NULL) {
    g_grpc_server->ShutDownServer();
    delete g_grpc_server;
    g_grpc_server = NULL;
    ACA_LOG_INFO("%s", "Cleaned up grpc server.\n");
  } else {
    ACA_LOG_ERROR("%s", "Unable to call delete, grpc server pointer is null.\n");
  }

  if (g_grpc_server_thread != NULL) {
    delete g_grpc_server_thread;
    g_grpc_server_thread = NULL;
    ACA_LOG_INFO("%s", "Cleaned up grpc server thread.\n");
  } else {
    ACA_LOG_ERROR("%s", "Unable to call delete, grpc server thread pointer is null.\n");
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
  int rc = 0;

  ACA_LOG_INIT(ACALOGNAME);

  ACA_LOG_INFO("%s", "Alcor Control Agent started...\n");

  // Register the signal handlers
  signal(SIGINT, aca_signal_handler);
  signal(SIGTERM, aca_signal_handler);

  while ((option = getopt(argc, argv, "a:p:b:h:g:s:c:t:o:md")) != -1) {
    switch (option) {
    case 'a':
      g_ncm_address = optarg;
      break;
    case 'p':
      g_ncm_port = optarg;
      break;
    case 'b':
      g_broker_list = optarg;
      break;
    case 'h':
      g_pulsar_topic = optarg;
      break;
    case 'g':
      g_pulsar_subsription_name = optarg;
      break;
    case 's':
      g_grpc_server_port = optarg;
      break;
    case 'c':
      g_ofctl_command = optarg;
      break;
    case 't':
      g_ofctl_target = optarg;
      break;
    case 'o':
      g_ofctl_options = optarg;
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
              "\t\t[-a NCM IP Address]\n"
              "\t\t[-p NCM Port]\n"
              "\t\t[-b pulsar broker list]\n"
              "\t\t[-h pulsar host topic to listen]\n"
              "\t\t[-g pulsar subscription name]\n"
              "\t\t[-s gRPC server port\n"
              "\t\t[-c ofctl command]\n"
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
  if (g_pulsar_topic == EMPTY_STRING) {
    g_pulsar_topic = PULSAR_TOPIC;
  }
  if (g_pulsar_subsription_name == EMPTY_STRING) {
    g_pulsar_subsription_name = PULSAR_SUBSCRIPTION_NAME;
  }
  if (g_grpc_server_port == EMPTY_STRING) {
    g_grpc_server_port = GRPC_SERVER_PORT;
  }
  if (g_ofctl_command == EMPTY_STRING) {
    g_ofctl_command = OFCTL_COMMAND;
  }
  if (g_ofctl_target == EMPTY_STRING) {
    g_ofctl_target = OFCTL_TARGET;
  }

  g_grpc_server = new GoalStateProvisionerImpl();
  g_grpc_server_thread =
          new std::thread(std::bind(&GoalStateProvisionerImpl::RunServer, g_grpc_server));
  g_grpc_server_thread->detach();

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();

  // monitor br-int for dhcp request message
  ovs_monitor_brint_thread =
          new thread(bind(&ACA_OVS_Control::monitor,
                          &ACA_OVS_Control::get_instance(), "br-int", "resume"));
  ovs_monitor_brint_thread->detach();

  // monitor br-tun for arp request message
  ACA_OVS_Control::get_instance().monitor("br-tun", "resume");

  ACA_Message_Pulsar_Consumer network_config_consumer(g_broker_list, g_pulsar_subsription_name);
  rc = network_config_consumer.consumeDispatched(g_pulsar_topic);
  aca_cleanup();
  return rc;
}
