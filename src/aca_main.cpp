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
#include "aca_message_pulsar_consumer.h"
#include "aca_grpc.h"
#include "aca_grpc_client.h"

#undef UNUSED
#include "of_controller.h"
#include "aca_ovs_l2_programmer.h"

#undef OFP_ASSERT
#undef CONTAINER_OF
#undef ARRAY_SIZE
#undef ROUND_UP
#include "aca_ovs_control.h"

#include "goalstateprovisioner.grpc.pb.h"
#include <thread>
#include <chrono>
#include <unistd.h> /* for getopt */
#include <grpcpp/grpcpp.h>
#include <cmath>

#include "marl/defer.h"
#include "marl/event.h"
#include "marl/scheduler.h"
#include "marl/waitgroup.h"

using aca_message_pulsar::ACA_Message_Pulsar_Consumer;
using aca_ovs_control::ACA_OVS_Control;
using std::string;

// Defines
#define ACALOGNAME "AlcorControlAgent"
static char EMPTY_STRING[] = "";
static char BROKER_LIST[] = "pulsar://localhost:6650";
static char PULSAR_TOPIC[] = "Host-ts-1";
static char PULSAR_SUBSCRIPTION_NAME[] = "Test-Subscription";
static char GRPC_SERVER_PORT[] = "50001";
static char OFCTL_COMMAND[] = "monitor";
static char OFCTL_TARGET[] = "br-int";

using namespace std;

// Global variables
std::thread *g_grpc_server_thread = NULL;
std::thread *g_grpc_client_thread = NULL;
GoalStateProvisionerAsyncServer *g_grpc_server = NULL;
GoalStateProvisionerClientImpl *g_grpc_client = NULL;
string g_broker_list = EMPTY_STRING;
string g_pulsar_topic = EMPTY_STRING;
string g_pulsar_subsription_name = EMPTY_STRING;
string g_pulsar_hashed_key = "0";
string g_grpc_server_port = EMPTY_STRING;
string g_ofctl_command = EMPTY_STRING;
string g_ofctl_target = EMPTY_STRING;
string g_ofctl_options = EMPTY_STRING;
string g_ncm_address = EMPTY_STRING;
string g_ncm_port = EMPTY_STRING;
string g_ovs_ctrl_address = "127.0.0.1";
int g_ovs_ctrl_port = 1234;

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
int processor_count = std::thread::hardware_concurrency();
/*
  From previous tests, we found that, for x number of cores,
  it is more efficient to set the size of both thread pools
  to be x * (2/3), which means the total size of the thread pools
  is x * (4/3). For example, for a host with 24 cores, we would 
  set the sizes of both thread pools to be 16.
*/
int thread_pools_size = (processor_count == 0) ? 1 : ((ceil(1.3 * processor_count)) / 2);

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

  // Stop the grpc client
  if (g_grpc_client != NULL) {
    delete g_grpc_client;
    g_grpc_client = NULL;
    ACA_LOG_INFO("%s", "Cleaned up grpc client.\n");
  } else {
    ACA_LOG_ERROR("%s", "Unable to call delete, grpc client pointer is null.\n");
  }

  if (g_grpc_client_thread != NULL) {
    delete g_grpc_client_thread;
    g_grpc_client_thread = NULL;
    ACA_LOG_INFO("%s", "Cleaned up grpc client thread.\n");
  } else {
    ACA_LOG_ERROR("%s", "Unable to call delete, grpc client thread pointer is null.\n");
  }

  // Stop the ovs controller and clean up
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().clean_up_ovs_controller();

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

  while ((option = getopt(argc, argv, "a:p:b:h:g:k:s:c:t:o:md")) != -1) {
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
    case 'k':
      g_pulsar_hashed_key = optarg;
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
    default: //the '?' case when the option is not recognized
      fprintf(stderr,
              "Usage: %s\n"
              "\t\t[-a NCM IP Address]\n"
              "\t\t[-p NCM Port]\n"
              "\t\t[-b pulsar broker list]\n"
              "\t\t[-h pulsar host topic to listen]\n"
              "\t\t[-g pulsar subscription name]\n"
              "\t\t[-k pulsar hashed key]\n"
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

  g_grpc_server = new GoalStateProvisionerAsyncServer();
  
  // Create a separate thread to run the grpc client.
  g_grpc_client = new GoalStateProvisionerClientImpl();

  // Create a marl scheduler using all the logical processors available to the process.
  // Bind this scheduler to the main thread so we can call marl::schedule()
  marl::Scheduler::Config cfg_bind_hw_cores;
  cfg_bind_hw_cores.setWorkerThreadCount(thread_pools_size * 2);
  marl::Scheduler task_scheduler(cfg_bind_hw_cores);
  task_scheduler.bind();
  defer(task_scheduler.unbind());

  marl::schedule([=]{
    g_grpc_server->RunServer(thread_pools_size);
  });

  marl::schedule([=]{
    g_grpc_client->RunClient();
  });

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().get_local_host_ips();

  rc = aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  if (rc == EXIT_FAILURE) {
    ACA_LOG_ERROR("%s \n", "ACA is not able to create the bridges, please check your environment");
    aca_cleanup();
    return rc;
  }

  // setup ovs controller with server ip address and port number, will be used for openflow operations
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().setup_ovs_controller(g_ovs_ctrl_address, g_ovs_ctrl_port);

  //// monitor br-int for dhcp request message
  //ovs_monitor_brint_thread =
  //        new thread(bind(&ACA_OVS_Control::monitor,
  //                        &ACA_OVS_Control::get_instance(), "br-int", "resume"));
  //ovs_monitor_brint_thread->detach();

  //// monitor br-tun for arp request message
  //ACA_OVS_Control::get_instance().monitor("br-tun", "resume");

  ACA_Message_Pulsar_Consumer network_config_consumer(g_pulsar_topic, g_broker_list, g_pulsar_subsription_name);
  //network_config_consumer.multicastConsumerDispatched();
  network_config_consumer.unicastConsumerDispatched(atoi(g_pulsar_hashed_key.c_str()));

  pause();
  aca_cleanup();

  return rc;
}
