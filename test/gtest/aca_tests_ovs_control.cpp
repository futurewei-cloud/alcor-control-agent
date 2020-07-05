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
#include "aca_comm_mgr.h"
#include "aca_net_config.h"
#include "gtest/gtest.h"
#include "goalstate.pb.h"
#include <unistd.h> /* for getopt */
#include <iostream>
#include <string>

using namespace std;
using namespace alcor::schema;
using aca_comm_manager::Aca_Comm_Manager;

// Defines
#define ACALOGNAME "AlcorControlAgentTest"
static char LOCALHOST[] = "localhost";
static char UDP_PROTOCOL[] = "udp";

static char EMPTY_STRING[] = "";
//static char VALID_STRING[] = "VALID_STRING";
//static char DEFAULT_MTU[] = "9000";

static string project_id = "99d9d709-8478-4b46-9f3f-000000000000";
static string vpc_id = "99d9d709-8478-4b46-9f3f-111111111111";
static string subnet_id = "99d9d709-8478-4b46-9f3f-222222222222";
static string port_name_1 = "tap-33333333";
static string port_name_2 = "tap-44444444";
static string vmac_address_1 = "fa:16:3e:d7:f2:6c";
static string vmac_address_2 = "fa:16:3e:d7:f2:6d";
static string vip_address_1 = "10.0.0.1";
static string vip_address_2 = "10.0.0.2";
static string remote_ip_1 = "172.0.0.2";
static string ofctl_command = "monitor";
static string ofctl_target = "br-int";
//static NetworkType vxlan_type = NetworkType::VXLAN;

// Global variables
string g_rpc_server = EMPTY_STRING;
string g_rpc_protocol = EMPTY_STRING;
string g_ofctl_command = EMPTY_STRING;
string g_ofctl_target = EMPTY_STRING;
string g_ofctl_options = EMPTY_STRING;
std::atomic_ulong g_total_rpc_call_time(0);
std::atomic_ulong g_total_rpc_client_time(0);
std::atomic_ulong g_total_network_configuration_time(0);
std::atomic_ulong g_total_update_GS_time(0);
bool g_debug_mode = true;
bool g_demo_mode = false;
bool g_transitd_loaded = false;

using aca_net_config::Aca_Net_Config;
using aca_ovs_control::ACA_OVS_Control;

TEST(ovs_control_test_cases, simple_message)
{
  //ulong culminative_network_configuration_time = 0;
  int rc;

  rc = ACA_OVS_Control::get_instance().monitor("br-int", "");
  ASSERT_NE(rc, EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
  int option;

  ACA_LOG_INIT(ACALOGNAME);

  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  testing::InitGoogleTest(&argc, argv);

  while ((option = getopt(argc, argv, "s:p:c:t")) != -1) {
    switch (option) {
    case 's':
      g_rpc_server = optarg;
      break;
    case 'p':
      g_rpc_protocol = optarg;
      break;
    case 'c':
      g_ofctl_command = optarg;
      break;
    case 't':
      g_transitd_loaded = true;
      break;      
    default: /* the '?' case when the option is not recognized */
      fprintf(stderr,
              "Usage: %s\n"
              "\t\t[-s transitd RPC server]\n"
              "\t\t[-p transitd RPC protocol]\n"
              "\t\t[-c ofctl command]\n"
              "\t\t[-t transitd is loaded]\n",
              argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  // fill in the transitd RPC server and protocol if it is not provided in
  // command line arg
  if (g_rpc_server == EMPTY_STRING) {
    g_rpc_server = LOCALHOST;
  }
  if (g_rpc_protocol == EMPTY_STRING) {
    g_rpc_protocol = UDP_PROTOCOL;
  }
  if (g_ofctl_command == EMPTY_STRING) {
    g_ofctl_command = ofctl_command;
  }
  if (g_ofctl_target == EMPTY_STRING) {
    g_ofctl_target = ofctl_target;
  }

  int rc = 1; //RUN_ALL_TESTS();

  //aca_cleanup();

  return rc;
}
