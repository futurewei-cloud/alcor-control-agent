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
#include "gtest/gtest.h"
#include "goalstate.pb.h"
#include "aca_grpc.h"
#include "aca_grpc_client.h"
#include "aca_message_pulsar_producer.h"
#include <unistd.h> /* for getopt */
#include <grpcpp/grpcpp.h>
#include <thread>

using namespace std;
using namespace aca_message_pulsar;

#define ACALOGNAME "AlcorControlAgentTest"

// Global variables
static char EMPTY_STRING[] = "";
string g_ofctl_command = EMPTY_STRING;
string g_ofctl_target = EMPTY_STRING;
string g_ofctl_options = EMPTY_STRING;
string g_ncm_address = EMPTY_STRING;
string g_ncm_port = EMPTY_STRING;
string g_grpc_server_port = EMPTY_STRING;
std::thread *g_grpc_server_thread = NULL;
GoalStateProvisionerAsyncServer *g_grpc_server = NULL;
std::thread *g_grpc_client_thread = NULL;
GoalStateProvisionerClientImpl *g_grpc_client = NULL;

// total time for execute_system_command in microseconds
std::atomic_ulong g_initialize_execute_system_time(0);
// total time for execute_ovsdb_command in microseconds
std::atomic_ulong g_initialize_execute_ovsdb_time(0);
// total time for execute_openflow_command in microseconds
std::atomic_ulong g_initialize_execute_openflow_time(0);
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

bool g_debug_mode = true;
bool g_demo_mode = false;

string remote_ip_1 = "172.17.0.2"; // for docker network
string remote_ip_2 = "172.17.0.3"; // for docker network
uint neighbors_to_create = 10;

static string mq_broker_ip = "pulsar://localhost:6650"; //for the broker running in localhost
static string mq_test_topic = "my-topic";

//
// Test suite: pulsar_test_cases
//
// Testing the pulsar implementation where AlcorControlAgent is the consumer
// and aca_test is acting as producer
// Note: it will require a pulsar setup on localhost therefore this test is DISABLED by default
//   it can be executed by:
//
//     aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_pulsar_consumer_test
//
TEST(pulsar_test_cases, DISABLED_pulsar_consumer_test)
{
  int retcode = 0;
  const int MESSAGES_TO_SEND = 10;
  string message = "Test Message";

  ACA_Message_Pulsar_Producer producer(mq_broker_ip, mq_test_topic);

  for (int i = 0; i < MESSAGES_TO_SEND; i++) {
    retcode = producer.publish(message);
    EXPECT_EQ(retcode, EXIT_SUCCESS);
  }
}

static void aca_cleanup()
{
  ACA_LOG_DEBUG("%s", "==========INIT TIMES:==========\n");

  ACA_LOG_DEBUG("g_initialize_execute_system_time = %lu microseconds or %lu milliseconds\n",
                g_initialize_execute_system_time.load(),
                us_to_ms(g_initialize_execute_system_time.load()));

  ACA_LOG_DEBUG("g_initialize_execute_ovsdb_time = %lu microseconds or %lu milliseconds\n",
                g_initialize_execute_ovsdb_time.load(),
                us_to_ms(g_initialize_execute_ovsdb_time.load()));

  ACA_LOG_DEBUG("g_initialize_execute_openflow_time = %lu microseconds or %lu milliseconds\n",
                g_initialize_execute_openflow_time.load(),
                us_to_ms(g_initialize_execute_openflow_time.load()));

  ACA_LOG_DEBUG("%s", "==========EXECUTION TIMES:==========\n");

  ACA_LOG_DEBUG("g_total_execute_system_time = %lu microseconds or %lu milliseconds\n",
                g_total_execute_system_time.load(),
                us_to_ms(g_total_execute_system_time.load()));

  ACA_LOG_DEBUG("g_total_execute_ovsdb_time = %lu microseconds or %lu milliseconds\n",
                g_total_execute_ovsdb_time.load(),
                us_to_ms(g_total_execute_ovsdb_time.load()));

  ACA_LOG_DEBUG("g_total_execute_openflow_time = %lu microseconds or %lu milliseconds\n",
                g_total_execute_openflow_time.load(),
                us_to_ms(g_total_execute_openflow_time.load()));

  ACA_LOG_DEBUG("%s", "==========UPDATE GS TIMES:==========\n");

  ACA_LOG_DEBUG("g_total_update_GS_time = %lu microseconds or %lu milliseconds\n",
                g_total_update_GS_time.load(), us_to_ms(g_total_update_GS_time.load()));

  ACA_LOG_INFO("%s", "Program exiting, cleaning up...\n");

  // Optional:  Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  ACA_LOG_CLOSE();
}

int main(int argc, char **argv)
{
  int option;

  ACA_LOG_INIT(ACALOGNAME);

  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  testing::InitGoogleTest(&argc, argv);

  while ((option = getopt(argc, argv, "a:p:m:c:n:")) != -1) {
    switch (option) {
    case 'a':
      g_ncm_address = optarg;
      break;
    case 'p':
      g_ncm_port = optarg;
      break;
    case 'm':
      remote_ip_1 = optarg;
      break;
    case 'c':
      remote_ip_2 = optarg;
      break;
    case 'n':
      neighbors_to_create = std::stoi(optarg);
      break;
    default: /* the '?' case when the option is not recognized */
      fprintf(stderr,
              "Usage: %s\n"
              "\t\t[-a NCM IP Address]\n"
              "\t\t[-p NCM Port]\n"
              "\t\t[-m parent machine IP]\n"
              "\t\t[-c child machine IP]\n"
              "\t\t[-n neighbors to create (default: 10)]\n",
              argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  g_grpc_server = new GoalStateProvisionerAsyncServer();
  int g_grpc_server_pool_size = 3;
  g_grpc_server_thread =
          new std::thread(std::bind(&GoalStateProvisionerAsyncServer::RunServer, g_grpc_server, g_grpc_server_pool_size));
  g_grpc_server_thread->detach();

  g_grpc_client = new GoalStateProvisionerClientImpl();
  g_grpc_client_thread = new std::thread(
          std::bind(&GoalStateProvisionerClientImpl::RunClient, g_grpc_client));
  g_grpc_client_thread->detach();

  int rc = RUN_ALL_TESTS();

  aca_cleanup();

  return rc;
}