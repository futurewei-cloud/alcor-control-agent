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

#include "aca_comm_mgr.h"
#include "aca_net_config.h"
#include "aca_util.h"
#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "trn_rpc_protocol.h"
#include <chrono>
#include <future>
#include <errno.h>
#include <arpa/inet.h>
#include <algorithm>

using namespace std;
using namespace alcorcontroller;
using aca_net_config::Aca_Net_Config;

std::mutex gs_reply_mutex; // mutex for writing gs reply object
std::mutex rpc_client_call_mutex; // mutex to protect the RPC client and call

#define cast_to_nanoseconds(x) chrono::duration_cast<chrono::nanoseconds>(x)

static char ACA_PREFIX[] = "aca_";
static char PHYSICAL_IF[] = "eth0";
static char VPC_NS_PREFIX[] = "vpc-ns-";
static uint PORT_ID_TRUNCATION_LEN = 11;
static uint VETH_NAME_TRUNCATION_LEN = 15;
static char TEMP_PREFIX[] = "temp";
static char PEER_PREFIX[] = "peer";
static char agent_xdp_path[] = "/trn_xdp/trn_agent_xdp_ebpf_debug.o";
static char agent_pcap_file[] = "/bpffs/agent_xdp.pcap";

extern string g_rpc_server;
extern string g_rpc_protocol;
extern std::atomic_ulong g_total_rpc_call_time;
extern std::atomic_ulong g_total_rpc_client_time;
extern std::atomic_ulong g_total_update_GS_time;
extern bool g_demo_mode;

static inline const char *aca_get_operation_name(OperationType operation)
{
  switch (operation) {
  case OperationType::CREATE:
    return "CREATE";
  case OperationType::UPDATE:
    return "UPDATE";
  case OperationType::GET:
    return "GET";
  case OperationType::DELETE:
    return "DELETE";
  case OperationType::INFO:
    return "INFO";
  case OperationType::FINALIZE:
    return "FINALIZE";
  case OperationType::CREATE_UPDATE_SWITCH:
    return "CREATE_UPDATE_SWITCH";
  case OperationType::CREATE_UPDATE_ROUTER:
    return "CREATE_UPDATE_ROUTER";
  case OperationType::CREATE_UPDATE_GATEWAY:
    return "CREATE_UPDATE_GATEWAY";
  default:
    return "ERROR: unknown operation type!";
  }
}

static inline void aca_truncate_device_name(string &device_name, uint truncation_len)
{
  if (!device_name.empty()) {
    if (device_name.length() > truncation_len) {
      device_name = device_name.substr(0, truncation_len);
    }
    // else length <= truncation_len
    // do nothing since the name is good already
  } else {
    throw std::invalid_argument("Input device_name is null");
  }
}

static inline bool aca_is_ep_on_same_host(const string ep_host_ip)
{
  if (ep_host_ip.empty()) {
    return false;
  }

  const string IFCONFIG_PREFIX = "ifconfig ";
  string cmd_string = IFCONFIG_PREFIX + PHYSICAL_IF + " | grep " + ep_host_ip;
  int rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);

  return (rc == EXIT_SUCCESS);
}

static inline int
aca_fix_namespace_name(string &namespace_name, const string vpc_id, bool create_namespace,
                       ulong &culminative_network_configuration_time)
{
  int rc = EXIT_SUCCESS;
  bool need_to_create_namespace = false;
  size_t last_slash_pos;
  string cmd_string;

  if (!namespace_name.empty()) {
    ACA_LOG_INFO("Namespace string from GS not empty: %s\n", namespace_name.c_str());
    string ProvidedNamespacePath = namespace_name;

    // TODO: make "/var/run/netns/" as constant
    if (namespace_name.rfind("/var/run/netns/", 0) == 0) {
      // if namespace start with /var/run/netns/, e.g. for arktos
      // container runtime, strip that out
      // substr can throw out_of_range and bad_alloc exceptions
      namespace_name = namespace_name.substr(15);

      // throw except if there is '/' in the remaining string
      last_slash_pos = namespace_name.rfind('/');
      if (last_slash_pos != string::npos) {
        throw std::invalid_argument("Remaining namespace name contains '/'");
      }
      ACA_LOG_INFO("Using namespace string: %s\n", namespace_name.c_str());
      need_to_create_namespace = create_namespace;
    }

    else if (namespace_name.rfind("/run/netns/", 0) == 0) {
      // else if namespace start with /run/netns/, strip that out
      // substr can throw out_of_range and bad_alloc exceptions
      namespace_name = namespace_name.substr(11);

      // throw except if there is '/' in the remaining string
      last_slash_pos = namespace_name.rfind('/');
      if (last_slash_pos != string::npos) {
        throw std::invalid_argument("Remaining namespace name contains '/'");
      }
      ACA_LOG_INFO("Using namespace string: %s\n", namespace_name.c_str());
      need_to_create_namespace = create_namespace;
    }

    else {
      // generate a string based on namespace name
      std::replace(namespace_name.begin(), namespace_name.end(), '/', '_');
      namespace_name = ACA_PREFIX + namespace_name;

      if (create_namespace) {
        // touch /var/run/netns/uniqueName
        cmd_string = "touch /var/run/netns/" + namespace_name;
        rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);

        if (rc == EXIT_SUCCESS) {
          // mount --bind $ProvidedNamespacePath  /var/run/netns/uniqueName
          cmd_string = "mount --bind " + ProvidedNamespacePath +
                       " /var/run/netns/" + namespace_name;
          rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
        }
        need_to_create_namespace = false;
      }
    }
  } else {
    namespace_name = VPC_NS_PREFIX + vpc_id;
    ACA_LOG_INFO("Namespace string empty, using: %s instead\n", namespace_name.c_str());
    need_to_create_namespace = create_namespace;
  }

  // create the namespace using ip netns if needed
  if (need_to_create_namespace) {
    rc = Aca_Net_Config::get_instance().create_namespace(
            namespace_name, culminative_network_configuration_time);
  }

  return rc;
}

static void aca_convert_to_mac_array(const char *mac_string, u_char *mac)
{
  if (mac_string == nullptr) {
    throw std::invalid_argument("Input mac_string is null");
  }

  if (mac == nullptr) {
    throw std::invalid_argument("Input mac is null");
  }

  if (sscanf(mac_string, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1],
             &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
    return;
  }

  if (sscanf(mac_string, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx", &mac[0], &mac[1],
             &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
    return;
  }

  // nothing matched
  ACA_LOG_ERROR("Invalid mac address: %s\n", mac_string);

  throw std::invalid_argument("Input mac_string is not in the expect format");
}

namespace aca_comm_manager
{
Aca_Comm_Manager &Aca_Comm_Manager::get_instance()
{
  // instance is destroyed when program exits.
  // It is Instantiated on first use.
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

int Aca_Comm_Manager::update_vpc_state_workitem(const VpcState current_VpcState,
                                                GoalStateOperationReply &gsOperationReply)
{
  int transitd_command;
  void *transitd_input;
  int exec_command_rc;
  int overall_rc;
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;

  rpc_trn_vpc_t vpc_in;
  rpc_trn_endpoint_t substrate_in;
  struct sockaddr_in sa;

  auto operation_start = chrono::steady_clock::now();

  VpcConfiguration current_VpcConfiguration = current_VpcState.configuration();

  switch (current_VpcState.operation_type()) {
  case OperationType::CREATE_UPDATE_SWITCH:
    transitd_command = UPDATE_VPC;
    transitd_input = &vpc_in;

    try {
      vpc_in.interface = PHYSICAL_IF;
      vpc_in.tunid = current_VpcConfiguration.tunnel_id();
      vpc_in.routers_ips.routers_ips_len =
              current_VpcConfiguration.transit_routers_size();
      uint32_t routers[RPC_TRN_MAX_VPC_ROUTERS];
      vpc_in.routers_ips.routers_ips_val = routers;

      for (int j = 0; j < current_VpcConfiguration.transit_routers_size(); j++) {
        // inet_pton returns 1 for success 0 for failure
        if (inet_pton(AF_INET,
                      current_VpcConfiguration.transit_routers(j).ip_address().c_str(),
                      &(sa.sin_addr)) != 1) {
          throw std::invalid_argument("Transit switch ip address is not in the expect format");
        }
        vpc_in.routers_ips.routers_ips_val[j] = sa.sin_addr.s_addr;

        ACA_LOG_DEBUG("VPC operation: CREATE_UPDATE_SWITCH, update vpc, IP: %s, tunnel_id: %ld\n",
                      current_VpcConfiguration.transit_routers(j).ip_address().c_str(),
                      current_VpcConfiguration.tunnel_id());

        ACA_LOG_DEBUG("[Before execute_command] routers_ips_val[%d]: routers_ips_val: %u .\n",
                      j, vpc_in.routers_ips.routers_ips_val[j]);
      }
      overall_rc = EXIT_SUCCESS;

      ACA_LOG_DEBUG("VPC Operation: %s: interface: %s, transit_routers_size: %d, tunid:%ld\n",
                    aca_get_operation_name(current_VpcState.operation_type()),
                    vpc_in.interface,
                    current_VpcConfiguration.transit_routers_size(), vpc_in.tunid);
    } catch (const std::invalid_argument &e) {
      ACA_LOG_ERROR("Invalid argument exception caught while parsing CREATE_UPDATE_SWITCH VPC configuration, message: %s.\n",
                    e.what());
      overall_rc = -EXIT_FAILURE;
      // TODO: Notify the Network Controller the goal state configuration has invalid data
    } catch (...) {
      ACA_LOG_ERROR("Unknown exception caught while parsing CREATE_UPDATE_SWITCH VPC configuration, rethrowing.\n");
      throw; // rethrowing
    }

    break;
  default:
    transitd_command = 0;
    ACA_LOG_DEBUG("Invalid VPC state operation type %d\n",
                  current_VpcState.operation_type());
    overall_rc = -EXIT_FAILURE;
    break;
  }

  if ((transitd_command != 0) && (overall_rc == EXIT_SUCCESS)) {
    exec_command_rc = this->execute_command(transitd_command, transitd_input,
                                            culminative_dataplane_programming_time);
    if (exec_command_rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully executed the network controller command\n");
    } else {
      ACA_LOG_ERROR("[update_vpc_states] Unable to execute the network controller command: %d\n",
                    exec_command_rc);
      overall_rc = exec_command_rc;
    }
  }

  if (current_VpcState.operation_type() == OperationType::CREATE_UPDATE_SWITCH) {
    // update substrate
    transitd_command = UPDATE_EP;

    try {
      for (int j = 0; j < current_VpcConfiguration.transit_routers_size(); j++) {
        substrate_in.interface = PHYSICAL_IF;

        ACA_LOG_DEBUG(
                "VPC operation: CREATE_UPDATE_SWITCH, update substrate, IP: %s, mac: %s\n",
                current_VpcConfiguration.transit_routers(j).ip_address().c_str(),
                current_VpcConfiguration.transit_routers(j).mac_address().c_str());

        // inet_pton returns 1 for success 0 for failure
        if (inet_pton(AF_INET,
                      current_VpcConfiguration.transit_routers(j).ip_address().c_str(),
                      &(sa.sin_addr)) != 1) {
          throw std::invalid_argument("Transit router ip address is not in the expect format");
        }
        substrate_in.ip = sa.sin_addr.s_addr;
        substrate_in.eptype = TRAN_SUBSTRT_EP;
        uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
        substrate_in.remote_ips.remote_ips_val = remote_ips;
        substrate_in.remote_ips.remote_ips_len = 0;
        // the below will throw invalid_argument exceptions when it cannot convert the mac string
        aca_convert_to_mac_array(
                current_VpcConfiguration.transit_routers(j).mac_address().c_str(),
                substrate_in.mac);
        substrate_in.hosted_interface = EMPTY_STRING;
        substrate_in.veth = EMPTY_STRING;
        substrate_in.tunid = TRAN_SUBSTRT_VNI;

        exec_command_rc = this->execute_command(transitd_command, &substrate_in,
                                                culminative_dataplane_programming_time);
        if (exec_command_rc == EXIT_SUCCESS) {
          ACA_LOG_INFO("Successfully updated substrate in transit daemon\n");
        } else {
          ACA_LOG_ERROR("Unable to update substrate in transit daemon: %d\n", exec_command_rc);
          overall_rc = exec_command_rc;
        }

      } // for (int j = 0; j < current_VpcConfiguration.transit_routers_size(); j++)
    } catch (const std::invalid_argument &e) {
      ACA_LOG_ERROR("Invalid argument exception caught while parsing substrate VPC configuration, message: %s.\n",
                    e.what());
      overall_rc = -EXIT_FAILURE;
    } catch (...) {
      ACA_LOG_ERROR("Unknown exception caught while parsing substrate VPC configuration, rethrowing.\n");
      throw; // rethrowing
    }
  }

  auto operation_end = chrono::steady_clock::now();

  auto operation_total_time =
          cast_to_nanoseconds(operation_end - operation_start).count();

  add_goal_state_operation_status(
          gsOperationReply, current_VpcConfiguration.id(), VPC,
          current_VpcState.operation_type(), overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);

  return overall_rc;
}

int Aca_Comm_Manager::update_vpc_states(const GoalState &parsed_struct,
                                        GoalStateOperationReply &gsOperationReply)
{
  std::vector<std::future<int> > workitem_future;
  int rc;
  int overall_rc = EXIT_SUCCESS;

  // if (parsed_struct.vpc_states_size() == 0)
  //   overall_rc = EXIT_SUCCESS in the current logic

  for (int i = 0; i < parsed_struct.vpc_states_size(); i++) {
    ACA_LOG_DEBUG("=====>parsing vpc states #%d\n", i);

    VpcState current_VPCState = parsed_struct.vpc_states(i);

    workitem_future.push_back(std::async(
            std::launch::async, &Aca_Comm_Manager::update_vpc_state_workitem,
            this, current_VPCState, std::ref(gsOperationReply)));

    // keeping below just in case if we want to call it serially
    // rc = update_vpc_state_workitem(current_VPCState, gsOperationReply);
    // if (rc != EXIT_SUCCESS)
    //   overall_rc = rc;
  } // for (int i = 0; i < parsed_struct.vpc_states_size(); i++)

  for (int i = 0; i < parsed_struct.vpc_states_size(); i++) {
    rc = workitem_future[i].get();
    if (rc != EXIT_SUCCESS)
      overall_rc = rc;
  } // for (int i = 0; i < parsed_struct.vpc_states_size(); i++)

  return overall_rc;
}

int Aca_Comm_Manager::update_subnet_state_workitem(const SubnetState current_SubnetState,
                                                   GoalStateOperationReply &gsOperationReply)
{
  int transitd_command;
  void *transitd_input;
  int exec_command_rc;
  int overall_rc;
  string my_cidr;
  string my_ip_address;
  string my_prefixlen;
  size_t slash_pos = 0;
  struct sockaddr_in sa;
  char hosted_interface[20];
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;

  rpc_trn_network_t network_in;
  rpc_trn_endpoint_t endpoint_in;
  rpc_trn_endpoint_t substrate_in;

  auto operation_start = chrono::steady_clock::now();

  SubnetConfiguration current_SubnetConfiguration = current_SubnetState.configuration();

  switch (current_SubnetState.operation_type()) {
  case OperationType::INFO:
    // information only, ignoring this.
    transitd_command = 0;
    overall_rc = EXIT_SUCCESS;
    break;
  case OperationType::CREATE_UPDATE_ROUTER:
    // this is to update the router host, need to update substrate later.
    transitd_command = UPDATE_NET;
    transitd_input = &network_in;

    try {
      network_in.interface = PHYSICAL_IF;
      network_in.tunid = current_SubnetConfiguration.tunnel_id();

      my_cidr = current_SubnetConfiguration.cidr();
      slash_pos = my_cidr.find('/');
      if (slash_pos == string::npos) {
        throw std::invalid_argument("'/' not found in cidr");
      }
      // substr can throw out_of_range and bad_alloc exceptions
      my_ip_address = my_cidr.substr(0, slash_pos);

      // inet_pton returns 1 for success 0 for failure
      if (inet_pton(AF_INET, my_ip_address.c_str(), &(sa.sin_addr)) != 1) {
        throw std::invalid_argument("Ip address is not in the expect format");
      }
      network_in.netip = sa.sin_addr.s_addr;

      my_prefixlen = my_cidr.substr(slash_pos + 1);
      // stoi can throw invalid argument exception when it cannot covert
      network_in.prefixlen = std::stoi(my_prefixlen);

      network_in.switches_ips.switches_ips_len =
              current_SubnetConfiguration.transit_switches_size();
      uint32_t switches[RPC_TRN_MAX_NET_SWITCHES];
      network_in.switches_ips.switches_ips_val = switches;

      for (int j = 0; j < current_SubnetConfiguration.transit_switches_size(); j++) {
        // inet_pton returns 1 for success 0 for failure
        if (inet_pton(
                    AF_INET,
                    current_SubnetConfiguration.transit_switches(j).ip_address().c_str(),
                    &(sa.sin_addr)) != 1) {
          throw std::invalid_argument("Transit switch ip address is not in the expect format");
        }
        network_in.switches_ips.switches_ips_val[j] = sa.sin_addr.s_addr;
      }
      overall_rc = EXIT_SUCCESS;

      ACA_LOG_DEBUG(
              "Subnet Operation: %s: interface: %s, cidr: %s, transit switch size: %d, tunid:%ld\n",
              aca_get_operation_name(current_SubnetState.operation_type()),
              network_in.interface, current_SubnetConfiguration.cidr().c_str(),
              current_SubnetConfiguration.transit_switches_size(), network_in.tunid);
    } catch (const std::invalid_argument &e) {
      ACA_LOG_ERROR("Invalid argument exception caught while parsing CREATE_UPDATE_ROUTER subnet configuration, message: %s.\n",
                    e.what());
      overall_rc = -EXIT_FAILURE;
      // TODO: Notify the Network Controller the goal state configuration has invalid data
    } catch (const std::exception &e) {
      ACA_LOG_ERROR("Exception caught while parsing CREATE_UPDATE_ROUTER subnet configuration, message: %s.\n",
                    e.what());
      overall_rc = -EXIT_FAILURE;
    } catch (...) {
      ACA_LOG_ERROR("Unknown exception caught while parsing CREATE_UPDATE_ROUTER subnet configuration, rethrowing.\n");
      throw; // rethrowing
    }
    break;

  case OperationType::CREATE_UPDATE_GATEWAY:
    // this is to update the phantom gateway, no need to update substrate later.
    transitd_command = UPDATE_EP;
    transitd_input = &endpoint_in;

    try {
      endpoint_in.interface = PHYSICAL_IF;

      // inet_pton returns 1 for success 0 for failure
      if (inet_pton(AF_INET, current_SubnetConfiguration.gateway().ip_address().c_str(),
                    &(sa.sin_addr)) != 1) {
        throw std::invalid_argument("Ip address is not in the expect format");
      }
      endpoint_in.ip = sa.sin_addr.s_addr;
      endpoint_in.eptype = TRAN_SIMPLE_EP;

      uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
      endpoint_in.remote_ips.remote_ips_val = remote_ips;
      endpoint_in.remote_ips.remote_ips_len = 1;

      // the below will throw invalid_argument exceptions when it cannot convert the mac string
      aca_convert_to_mac_array(
              current_SubnetConfiguration.gateway().mac_address().c_str(),
              endpoint_in.mac);

      endpoint_in.hosted_interface = EMPTY_STRING;
      endpoint_in.veth = EMPTY_STRING; // not needed for transit
      endpoint_in.tunid = current_SubnetConfiguration.tunnel_id();
      overall_rc = EXIT_SUCCESS;

      ACA_LOG_DEBUG("Subnet Operation: %s: interface: %s, gw_ip: %s, gw_mac: %s, hosted_interface: %s, veth_name:%s, tunid:%ld\n",
                    aca_get_operation_name(current_SubnetState.operation_type()),
                    endpoint_in.interface,
                    current_SubnetConfiguration.gateway().ip_address().c_str(),
                    current_SubnetConfiguration.gateway().mac_address().c_str(),
                    endpoint_in.hosted_interface, endpoint_in.veth, endpoint_in.tunid);
    } catch (const std::invalid_argument &e) {
      ACA_LOG_ERROR("Invalid argument exception caught while parsing CREATE_UPDATE_GATEWAY subnet configuration, message: %s.\n",
                    e.what());
      overall_rc = -EXIT_FAILURE;
      // TODO: Notify the Network Controller the goal state configuration has invalid data
    } catch (const std::exception &e) {
      ACA_LOG_ERROR("Exception caught while parsing CREATE_UPDATE_GATEWAY subnet configuration, message: %s.\n",
                    e.what());
      overall_rc = -EXIT_FAILURE;
    } catch (...) {
      ACA_LOG_ERROR("Unknown exception caught while parsing CREATE_UPDATE_ROUTER subnet configuration, rethrowing.\n");
      throw; // rethrowing
    }
    break;

  default:
    transitd_command = 0;
    ACA_LOG_DEBUG("Invalid subnet state operation type %d/n",
                  current_SubnetState.operation_type());
    overall_rc = -EXIT_FAILURE;
    break;
  }
  if ((transitd_command != 0) && (overall_rc == EXIT_SUCCESS)) {
    exec_command_rc = this->execute_command(transitd_command, transitd_input,
                                            culminative_dataplane_programming_time);
    if (exec_command_rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully executed the network controller command\n");
    } else {
      ACA_LOG_ERROR("[update_subnet_state_workitem] Unable to execute the network controller command: %d\n",
                    exec_command_rc);
      overall_rc = exec_command_rc;
      // TODO: Notify the Network Controller if the command is not successful.
    }
  }

  if (current_SubnetState.operation_type() == OperationType::CREATE_UPDATE_ROUTER) {
    // update substrate
    transitd_command = UPDATE_EP;

    try {
      for (int j = 0; j < current_SubnetConfiguration.transit_switches_size(); j++) {
        ACA_LOG_DEBUG(
                "Subnet operation: CREATE_UPDATE_ROUTER, update substrate, IP: %s, mac: %s\n",
                current_SubnetConfiguration.transit_switches(j).ip_address().c_str(),
                current_SubnetConfiguration.transit_switches(j).mac_address().c_str());

        substrate_in.interface = PHYSICAL_IF;

        // inet_pton returns 1 for success 0 for failure
        if (inet_pton(
                    AF_INET,
                    current_SubnetConfiguration.transit_switches(j).ip_address().c_str(),
                    &(sa.sin_addr)) != 1) {
          throw std::invalid_argument("Transit switch ip address is not in the expect format");
        }
        substrate_in.ip = sa.sin_addr.s_addr;
        substrate_in.eptype = TRAN_SUBSTRT_EP;
        uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
        substrate_in.remote_ips.remote_ips_val = remote_ips;
        substrate_in.remote_ips.remote_ips_len = 0;
        // the below will throw invalid_argument exceptions when it cannot convert the mac string
        aca_convert_to_mac_array(
                current_SubnetConfiguration.transit_switches(j).mac_address().c_str(),
                substrate_in.mac);
        substrate_in.hosted_interface = EMPTY_STRING;
        substrate_in.veth = EMPTY_STRING;
        substrate_in.tunid = TRAN_SUBSTRT_VNI;

        exec_command_rc = this->execute_command(transitd_command, &substrate_in,
                                                culminative_dataplane_programming_time);
        if (exec_command_rc == EXIT_SUCCESS) {
          ACA_LOG_INFO("Successfully updated substrate in transit daemon\n");
        } else {
          ACA_LOG_ERROR("Unable to update substrate in transit daemon: %d\n", exec_command_rc);
          overall_rc = exec_command_rc;
        }

      } // for (int j = 0; j < current_SubnetConfiguration.transit_switches_size(); j++)
    } catch (const std::invalid_argument &e) {
      ACA_LOG_ERROR("Invalid argument exception caught while parsing substrate subnet configuration, message: %s.\n",
                    e.what());
      overall_rc = -EXIT_FAILURE;
    } catch (...) {
      ACA_LOG_ERROR("Unknown exception caught while parsing substrate subnet configuration, rethrowing.\n");
      throw; // rethrowing
    }
  }

  auto operation_end = chrono::steady_clock::now();

  auto operation_total_time =
          cast_to_nanoseconds(operation_end - operation_start).count();

  add_goal_state_operation_status(
          gsOperationReply, current_SubnetConfiguration.id(), SUBNET,
          current_SubnetState.operation_type(), overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);

  return overall_rc;
}

int Aca_Comm_Manager::update_subnet_states(const GoalState &parsed_struct,
                                           GoalStateOperationReply &gsOperationReply)
{
  std::vector<std::future<int> > workitem_future;
  int rc;
  int overall_rc = EXIT_SUCCESS;

  // if (parsed_struct.subnet_states_size() == 0)
  //   overall_rc = EXIT_SUCCESS in the current logic

  for (int i = 0; i < parsed_struct.subnet_states_size(); i++) {
    ACA_LOG_DEBUG("=====>parsing subnet states #%d\n", i);

    SubnetState current_SubnetState = parsed_struct.subnet_states(i);

    workitem_future.push_back(std::async(
            std::launch::async, &Aca_Comm_Manager::update_subnet_state_workitem,
            this, current_SubnetState, std::ref(gsOperationReply)));

    // keeping below just in case if we want to call it serially
    // rc = update_subnet_state_workitem(current_SubnetState, gsOperationReply);
    // if (rc != EXIT_SUCCESS)
    //   overall_rc = rc;
  } // for (int i = 0; i < parsed_struct.subnet_states_size(); i++)

  for (int i = 0; i < parsed_struct.subnet_states_size(); i++) {
    rc = workitem_future[i].get();
    if (rc != EXIT_SUCCESS)
      overall_rc = rc;
  } // for (int i = 0; i < parsed_struct.subnet_states_size(); i++)

  return overall_rc;
}

int Aca_Comm_Manager::update_port_state_workitem(const PortState current_PortState,
                                                 const alcorcontroller::GoalState &parsed_struct,
                                                 GoalStateOperationReply &gsOperationReply)
{
  int transitd_command;
  void *transitd_input;
  int net_config_rc;
  int exec_command_rc;
  int overall_rc;

  bool subnet_info_found = false;
  string my_ep_ip_address;
  string my_ep_mac_address;
  string my_ep_host_ip_address;
  string my_cidr;
  string namespace_name;
  string vpc_id;
  string my_ip_address;
  string my_gw_address;
  string my_prefixlen;
  size_t slash_pos = 0;
  struct sockaddr_in sa;
  char veth_name[20];
  char peer_name[20];
  char hosted_interface[20];
  ulong culminative_dataplane_programming_time = 0;
  ulong culminative_network_configuration_time = 0;

  rpc_trn_endpoint_t endpoint_in;
  rpc_trn_agent_metadata_t agent_md_in;
  rpc_trn_endpoint_t substrate_in;

  auto operation_start = chrono::steady_clock::now();

  PortConfiguration current_PortConfiguration = current_PortState.configuration();

  string veth_name_string = current_PortConfiguration.veth_name();
  aca_truncate_device_name(veth_name_string, VETH_NAME_TRUNCATION_LEN);

  string port_id = current_PortConfiguration.id();
  aca_truncate_device_name(port_id, PORT_ID_TRUNCATION_LEN);
  string temp_name_string = TEMP_PREFIX + port_id;
  string peer_name_string = PEER_PREFIX + port_id;

  switch (current_PortState.operation_type()) {
  case OperationType::CREATE:
  case OperationType::CREATE_UPDATE_SWITCH:
    transitd_command = UPDATE_EP;
    transitd_input = &endpoint_in;

    try {
      endpoint_in.interface = PHYSICAL_IF;

      assert(current_PortConfiguration.fixed_ips_size() == 1);
      my_ep_ip_address = current_PortConfiguration.fixed_ips(0).ip_address();
      // inet_pton returns 1 for success 0 for failure
      if (inet_pton(AF_INET, my_ep_ip_address.c_str(), &(sa.sin_addr)) != 1) {
        throw std::invalid_argument("EP ip address is not in the expect format");
      }
      endpoint_in.ip = sa.sin_addr.s_addr;

      endpoint_in.eptype = TRAN_SIMPLE_EP;

      uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
      endpoint_in.remote_ips.remote_ips_val = remote_ips;
      endpoint_in.remote_ips.remote_ips_len = 1;
      // inet_pton returns 1 for success 0 for failure
      if (inet_pton(AF_INET, current_PortConfiguration.host_info().ip_address().c_str(),
                    &(sa.sin_addr)) != 1) {
        throw std::invalid_argument("EP host ip address is not in the expect format");
      }
      endpoint_in.remote_ips.remote_ips_val[0] = sa.sin_addr.s_addr;

      my_ep_mac_address = current_PortConfiguration.mac_address();
      // the below will throw invalid_argument exceptions when it cannot convert the mac string
      aca_convert_to_mac_array(current_PortConfiguration.mac_address().c_str(),
                               endpoint_in.mac);

      strncpy(veth_name, veth_name_string.c_str(), strlen(veth_name_string.c_str()) + 1);
      endpoint_in.veth = veth_name;

      endpoint_in.hosted_interface = EMPTY_STRING;

      if (current_PortState.operation_type() == OperationType::CREATE_UPDATE_SWITCH) {
        // if the CREATE_UPDATE_SWITCH is called on the same EP host
        // also need to provide peer interface info for mizar programming
        // Mizar controller code: if (ep.host and self.ip == ep.host.ip):
        if (aca_is_ep_on_same_host(
                    current_PortConfiguration.host_info().ip_address().c_str())) {
          strncpy(peer_name, peer_name_string.c_str(),
                  strlen(peer_name_string.c_str()) + 1);
          endpoint_in.hosted_interface = peer_name;
          ACA_LOG_DEBUG("OperationType::CREATE_UPDATE_SWITCH called on the same EP host, adding peer_name based on mizar logic.\n");
        }
      } else // it must be OperationType::CREATE
      {
        strncpy(peer_name, peer_name_string.c_str(), strlen(peer_name_string.c_str()) + 1);
        endpoint_in.hosted_interface = peer_name;
      }

      // TODO: cache the subnet information to a dictionary to provide
      // a faster look up for the next run, only use the below loop for
      // cache miss.
      // Look up the subnet configuration to query for tunnel_id
      for (int j = 0; j < parsed_struct.subnet_states_size(); j++) {
        SubnetConfiguration current_SubnetConfiguration =
                parsed_struct.subnet_states(j).configuration();

        ACA_LOG_DEBUG("current_SubnetConfiguration subnet ID: %s.\n",
                      current_SubnetConfiguration.id().c_str());

        if (parsed_struct.subnet_states(j).operation_type() == OperationType::INFO) {
          if (current_SubnetConfiguration.id() == current_PortConfiguration.network_id()) {
            if (current_PortState.operation_type() == OperationType::CREATE) {
              if (current_SubnetConfiguration.vpc_id().empty()) {
                throw std::invalid_argument("vpc_id is empty");
              }
              vpc_id = current_SubnetConfiguration.vpc_id();

              if (current_SubnetConfiguration.gateway().ip_address().empty()) {
                throw std::invalid_argument("gateway ip address is empty");
              }
              my_gw_address = current_SubnetConfiguration.gateway().ip_address();

              if (current_SubnetConfiguration.tunnel_id() == 0) {
                throw std::invalid_argument("tunnel id is 0");
              }
              endpoint_in.tunid = current_SubnetConfiguration.tunnel_id();

              my_cidr = current_SubnetConfiguration.cidr();

              slash_pos = my_cidr.find('/');
              if (slash_pos == string::npos) {
                throw std::invalid_argument("'/' not found in cidr");
              }

              // substr can throw out_of_range and bad_alloc exceptions
              my_prefixlen = my_cidr.substr(slash_pos + 1);
            } else if (current_PortState.operation_type() == OperationType::CREATE_UPDATE_SWITCH) {
              if (current_SubnetConfiguration.tunnel_id() == 0) {
                throw std::invalid_argument("tunnel id is 0");
              }
              endpoint_in.tunid = current_SubnetConfiguration.tunnel_id();
            }

            subnet_info_found = true;
            break;
          }
        }
      }
      if (!subnet_info_found) {
        ACA_LOG_ERROR("Not able to find the info for port with subnet ID: %s.\n",
                      current_PortConfiguration.network_id().c_str());
        overall_rc = -EXIT_FAILURE;
      } else {
        overall_rc = EXIT_SUCCESS;
      }

      ACA_LOG_DEBUG("Endpoint Operation: %s: interface: %s, ep_ip: %s, mac: %s, hosted_interface: %s, veth_name:%s, tunid:%ld\n",
                    aca_get_operation_name(current_PortState.operation_type()),
                    endpoint_in.interface, my_ep_ip_address.c_str(),
                    current_PortConfiguration.mac_address().c_str(),
                    endpoint_in.hosted_interface, endpoint_in.veth, endpoint_in.tunid);
    } catch (const std::invalid_argument &e) {
      ACA_LOG_ERROR("Invalid argument exception caught while parsing port configuration, message: %s.\n",
                    e.what());
      overall_rc = -EINVAL;
      // TODO: Notify the Network Controller the goal state configuration has invalid data
    } catch (const std::exception &e) {
      ACA_LOG_ERROR("Exception caught while parsing port configuration, message: %s.\n",
                    e.what());
      overall_rc = -EXIT_FAILURE;
    } catch (...) {
      ACA_LOG_ERROR("Unknown exception caught while parsing port configuration, rethrowing.\n");
      throw; // rethrowing
    }
    break;

  case OperationType::FINALIZE:
    transitd_command = UPDATE_AGENT_MD;
    transitd_input = &agent_md_in;

    try {
      strncpy(peer_name, peer_name_string.c_str(), strlen(peer_name_string.c_str()) + 1);
      agent_md_in.interface = peer_name;

      // fill in agent_md_in.eth
      agent_md_in.eth.interface = PHYSICAL_IF;

      my_ep_host_ip_address = current_PortConfiguration.host_info().ip_address();
      // inet_pton returns 1 for success 0 for failure
      if (inet_pton(AF_INET, my_ep_host_ip_address.c_str(), &(sa.sin_addr)) != 1) {
        throw std::invalid_argument("EP host ip address is not in the expect format");
      }

      agent_md_in.eth.ip = sa.sin_addr.s_addr;
      ACA_LOG_DEBUG("my_ep_host_ip_address string: %s converted to uint: %u and assigned to agent_md_in.eth.ip\n",
                    my_ep_host_ip_address.c_str(), agent_md_in.eth.ip);

      // the below will throw exceptions when it cannot convert the mac string
      aca_convert_to_mac_array(
              current_PortConfiguration.host_info().mac_address().c_str(),
              agent_md_in.eth.mac);

      // fill in agent_md_in.ep
      agent_md_in.ep.interface = PHYSICAL_IF;

      uint32_t md_remote_ips[RPC_TRN_MAX_REMOTE_IPS];
      agent_md_in.ep.remote_ips.remote_ips_val = md_remote_ips;
      agent_md_in.ep.remote_ips.remote_ips_len = 1;
      // using the previously converted host IP value
      agent_md_in.ep.remote_ips.remote_ips_val[0] = sa.sin_addr.s_addr;
      ACA_LOG_DEBUG("my_ep_host_ip_address string: %s converted to uint: %u and assigned to agent_md_in.ep.remote_ips.remote_ips_val[0]\n",
                    my_ep_host_ip_address.c_str(),
                    agent_md_in.ep.remote_ips.remote_ips_val[0]);

      assert(current_PortConfiguration.fixed_ips_size() == 1);
      my_ep_ip_address = current_PortConfiguration.fixed_ips(0).ip_address();
      // inet_pton returns 1 for success 0 for failure
      if (inet_pton(AF_INET, my_ep_ip_address.c_str(), &(sa.sin_addr)) != 1) {
        throw std::invalid_argument("EP ip address is not in the expect format");
      }

      agent_md_in.ep.ip = sa.sin_addr.s_addr;
      ACA_LOG_DEBUG("my_ep_ip_address string: %s converted to uint: %u and assigned to agent_md_in.ep.ip\n",
                    my_ep_ip_address.c_str(), agent_md_in.ep.ip);

      agent_md_in.ep.eptype = TRAN_SIMPLE_EP;

      // the below will throw invalid_argument exceptions when it cannot convert the mac string
      aca_convert_to_mac_array(current_PortConfiguration.mac_address().c_str(),
                               agent_md_in.ep.mac);

      strncpy(veth_name, veth_name_string.c_str(), strlen(veth_name_string.c_str()) + 1);
      agent_md_in.ep.veth = veth_name;

      agent_md_in.ep.hosted_interface = PHYSICAL_IF;

      // Look up the subnet configuration
      for (int j = 0; j < parsed_struct.subnet_states_size(); j++) {
        SubnetConfiguration current_SubnetConfiguration =
                parsed_struct.subnet_states(j).configuration();

        ACA_LOG_DEBUG("current_SubnetConfiguration subnet ID: %s.\n",
                      current_SubnetConfiguration.id().c_str());

        if (parsed_struct.subnet_states(j).operation_type() == OperationType::INFO) {
          if (current_SubnetConfiguration.id() == current_PortConfiguration.network_id()) {
            if (current_SubnetConfiguration.vpc_id().empty()) {
              throw std::invalid_argument("vpc_id is empty");
            }
            vpc_id = current_SubnetConfiguration.vpc_id();

            if (current_SubnetConfiguration.gateway().ip_address().empty()) {
              throw std::invalid_argument("gateway ip address is empty");
            }
            my_gw_address = current_SubnetConfiguration.gateway().ip_address();

            if (current_SubnetConfiguration.tunnel_id() == 0) {
              throw std::invalid_argument("tunnel id is 0");
            }
            agent_md_in.ep.tunid = current_SubnetConfiguration.tunnel_id();

            // fill in agent_md_in.ep
            agent_md_in.net.interface = PHYSICAL_IF;
            agent_md_in.net.tunid = current_SubnetConfiguration.tunnel_id();

            my_cidr = current_SubnetConfiguration.cidr();

            slash_pos = my_cidr.find('/');
            if (slash_pos == string::npos) {
              throw std::invalid_argument("'/' not found in cidr");
            }

            // substr can throw out_of_range and bad_alloc exceptions
            my_ip_address = my_cidr.substr(0, slash_pos);

            // inet_pton returns 1 for success 0 for failure
            if (inet_pton(AF_INET, my_ip_address.c_str(), &(sa.sin_addr)) != 1) {
              throw std::invalid_argument("Ip address is not in the expect format");
            }
            agent_md_in.net.netip = sa.sin_addr.s_addr;
            ACA_LOG_DEBUG("my_ip_address() from cidr: %s converted to agent_md_in.net.netip: %u\n",
                          my_ip_address.c_str(), agent_md_in.net.netip);

            my_prefixlen = my_cidr.substr(slash_pos + 1);
            // stoi throw invalid argument exception when it cannot covert
            agent_md_in.net.prefixlen = std::stoi(my_prefixlen);

            agent_md_in.net.switches_ips.switches_ips_len =
                    current_SubnetConfiguration.transit_switches_size();
            uint32_t switches[RPC_TRN_MAX_NET_SWITCHES];
            agent_md_in.net.switches_ips.switches_ips_val = switches;

            for (int k = 0; k < current_SubnetConfiguration.transit_switches_size(); k++) {
              // inet_pton returns 1 for success 0 for failure
              if (inet_pton(AF_INET,
                            current_SubnetConfiguration.transit_switches(k)
                                    .ip_address()
                                    .c_str(),
                            &(sa.sin_addr)) != 1) {
                throw std::invalid_argument("Transit switch ip address is not in the expect format");
              }
              agent_md_in.net.switches_ips.switches_ips_val[k] = sa.sin_addr.s_addr;
            }

            subnet_info_found = true;
            break;
          }
        }
      }
      if (!subnet_info_found) {
        ACA_LOG_ERROR("Not able to find the tunnel ID for port subnet ID: %s.\n",
                      current_PortConfiguration.network_id().c_str());
        overall_rc = -EXIT_FAILURE;
        // TODO: Notify the Network Controller the goal state configuration
        //       has invalid data
      } else {
        overall_rc = EXIT_SUCCESS;
      }
      ACA_LOG_DEBUG("Endpoint Operation: %s: interface: %s, ep_ip: %s, mac: %s, hosted_interface: %s, veth_name:%s, tunid:%ld\n",
                    aca_get_operation_name(current_PortState.operation_type()),
                    agent_md_in.ep.interface, my_ep_ip_address.c_str(),
                    current_PortConfiguration.mac_address().c_str(),
                    agent_md_in.ep.hosted_interface, agent_md_in.ep.veth,
                    agent_md_in.ep.tunid);
    } catch (const std::invalid_argument &e) {
      ACA_LOG_ERROR("Invalid argument exception caught while parsing FINALIZE port configuration, message: %s.\n",
                    e.what());
      overall_rc = -EINVAL;
      // TODO: Notify the Network Controller the goal state configuration
      //       has invalid data
    } catch (const std::exception &e) {
      ACA_LOG_ERROR("Exception caught while parsing FINALIZE port configuration, message: %s.\n",
                    e.what());
      overall_rc = -EXIT_FAILURE;
    } catch (...) {
      ACA_LOG_ERROR("Unknown exception caught while parsing FINALIZE port configuration, rethrowing.\n");
      throw; // rethrowing
    }

    break;
  default:
    transitd_command = 0;
    ACA_LOG_DEBUG("Invalid port state operation type %d/n",
                  current_PortState.operation_type());
    overall_rc = -EXIT_FAILURE;
    break;
  }

  if ((overall_rc == EXIT_SUCCESS) &&
      (current_PortState.operation_type() == OperationType::CREATE)) {
    // use the namespace string if available
    namespace_name = current_PortConfiguration.network_ns();

    net_config_rc = aca_fix_namespace_name(
            namespace_name, vpc_id, true, culminative_network_configuration_time);

    if (net_config_rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully created namespace: %s\n", namespace_name.c_str());
    } else {
      // it is okay if the namespace is already created
      ACA_LOG_WARN("Unable to create namespace: %s\n", namespace_name.c_str());
    }

    net_config_rc = Aca_Net_Config::get_instance().create_veth_pair(
            temp_name_string, peer_name_string, culminative_network_configuration_time);
    if (net_config_rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully created temp veth pair, veth: %s, peer: %s\n",
                   temp_name_string.c_str(), peer_name_string.c_str());
    } else {
      ACA_LOG_ERROR("Unable to create temp veth pair, veth: %s, peer: %s\n",
                    temp_name_string.c_str(), peer_name_string.c_str());
      overall_rc = net_config_rc;
    }

    net_config_rc = Aca_Net_Config::get_instance().setup_peer_device(
            peer_name_string, culminative_network_configuration_time);
    if (net_config_rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully setup the peer device: %s\n", peer_name_string.c_str());
    } else {
      ACA_LOG_ERROR("Unable to setup the peer device: %s\n", peer_name_string.c_str());
      overall_rc = net_config_rc;
    }

    // load transit agent XDP on the peer device
    exec_command_rc = load_agent_xdp(peer_name_string, culminative_dataplane_programming_time);
    if (exec_command_rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully loaded transit agent xdp on the peer device: %s\n",
                   peer_name_string.c_str());
    } else {
      ACA_LOG_ERROR("Unable to load transit agent xdp on the peer device: %s\n",
                    peer_name_string.c_str());
      overall_rc = exec_command_rc;
    }

    net_config_rc = Aca_Net_Config::get_instance().move_to_namespace(
            temp_name_string, namespace_name, culminative_network_configuration_time);
    if (net_config_rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully moved veth: %s, to namespace: %s\n",
                   temp_name_string.c_str(), namespace_name.c_str());
    } else {
      ACA_LOG_ERROR("Unable to move veth: %s, to namespace: %s\n",
                    temp_name_string.c_str(), namespace_name.c_str());
      overall_rc = net_config_rc;
    }

    veth_config new_veth_config;
    new_veth_config.veth_name = temp_name_string;
    new_veth_config.ip = my_ep_ip_address;
    new_veth_config.prefix_len = my_prefixlen;
    new_veth_config.mac = my_ep_mac_address;
    new_veth_config.gateway_ip = my_gw_address;

    net_config_rc = Aca_Net_Config::get_instance().setup_veth_device(
            namespace_name, new_veth_config, culminative_network_configuration_time);
    if (net_config_rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully setup veth device: %s, veth: %s, ip: %s, prefix: %s, mac: %s, gw: %s\n",
                   namespace_name.c_str(), temp_name_string.c_str(),
                   my_ep_ip_address.c_str(), my_prefixlen.c_str(),
                   my_ep_mac_address.c_str(), my_gw_address.c_str());
    } else {
      ACA_LOG_ERROR("Unable to setup veth device: %s, veth: %s, ip: %s, prefix: %s, mac: %s, gw: %s\n",
                    namespace_name.c_str(), temp_name_string.c_str(),
                    my_ep_ip_address.c_str(), my_prefixlen.c_str(),
                    my_ep_mac_address.c_str(), my_gw_address.c_str());
      overall_rc = net_config_rc;
    }
  }

  if ((transitd_command != 0) && (overall_rc == EXIT_SUCCESS)) {
    exec_command_rc = this->execute_command(transitd_command, transitd_input,
                                            culminative_dataplane_programming_time);
    if (exec_command_rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully executed the network controller command\n");
    } else {
      ACA_LOG_ERROR("[update_port_state_workitem] Unable to execute the network controller command: %d\n",
                    exec_command_rc);
      overall_rc = exec_command_rc;
    }
  }

  if (current_PortState.operation_type() == OperationType::CREATE_UPDATE_SWITCH) {
    // update substrate
    ACA_LOG_DEBUG("port operation: CREATE_UPDATE_SWITCH, update substrate from host_info(), IP: %s, mac: %s\n",
                  current_PortConfiguration.host_info().ip_address().c_str(),
                  current_PortConfiguration.host_info().mac_address().c_str());

    transitd_command = UPDATE_EP;

    try {
      substrate_in.interface = PHYSICAL_IF;

      // inet_pton returns 1 for success 0 for failure
      if (inet_pton(AF_INET, current_PortConfiguration.host_info().ip_address().c_str(),
                    &(sa.sin_addr)) != 1) {
        throw std::invalid_argument("Host ip address is not in the expect format");
      }
      substrate_in.ip = sa.sin_addr.s_addr;
      substrate_in.eptype = TRAN_SUBSTRT_EP;
      uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
      substrate_in.remote_ips.remote_ips_val = remote_ips;
      substrate_in.remote_ips.remote_ips_len = 0;
      // the below will throw invalid_argument exceptions when it cannot convert the mac string
      aca_convert_to_mac_array(
              current_PortConfiguration.host_info().mac_address().c_str(),
              substrate_in.mac);
      substrate_in.hosted_interface = EMPTY_STRING;
      substrate_in.veth = EMPTY_STRING;
      substrate_in.tunid = TRAN_SUBSTRT_VNI;

      exec_command_rc = this->execute_command(transitd_command, &substrate_in,
                                              culminative_dataplane_programming_time);
      if (exec_command_rc == EXIT_SUCCESS) {
        ACA_LOG_INFO("Successfully updated substrate in transit daemon\n");
      } else {
        ACA_LOG_ERROR("Unable to update substrate in transit daemon: %d\n", exec_command_rc);
        overall_rc = exec_command_rc;
      }
    } catch (const std::invalid_argument &e) {
      ACA_LOG_ERROR("Invalid argument exception caught while parsing CREATE_UPDATE_SWITCH substrate port configuration, message: %s.\n",
                    e.what());
      overall_rc = -EINVAL;
      // TODO: Notify the Network Controller the goal state configuration has invalid data
    } catch (...) {
      ACA_LOG_ERROR("Unknown exception caught while parsing CREATE_UPDATE_SWITCH substrate port configuration, rethrowing.\n");
      throw; // rethrowing
    }
  } else if (current_PortState.operation_type() == OperationType::FINALIZE) {
    transitd_command = UPDATE_AGENT_EP;

    try {
      // Look up the subnet info
      for (int j = 0; j < parsed_struct.subnet_states_size(); j++) {
        SubnetConfiguration current_SubnetConfiguration =
                parsed_struct.subnet_states(j).configuration();

        ACA_LOG_DEBUG("current_SubnetConfiguration subnet ID: %s.\n",
                      current_SubnetConfiguration.id().c_str());

        if (parsed_struct.subnet_states(j).operation_type() == OperationType::INFO) {
          if (current_SubnetConfiguration.id() == current_PortConfiguration.network_id()) {
            for (int k = 0; k < current_SubnetConfiguration.transit_switches_size(); k++) {
              ACA_LOG_DEBUG("port operation: FINALIZE, update substrate, IP: %s, mac: %s\n",
                            current_SubnetConfiguration.transit_switches(k)
                                    .ip_address()
                                    .c_str(),
                            current_SubnetConfiguration.transit_switches(k)
                                    .mac_address()
                                    .c_str());

              strncpy(peer_name, peer_name_string.c_str(),
                      strlen(peer_name_string.c_str()) + 1);
              substrate_in.interface = peer_name;

              // inet_pton returns 1 for success 0 for failure
              if (inet_pton(AF_INET,
                            current_SubnetConfiguration.transit_switches(k)
                                    .ip_address()
                                    .c_str(),
                            &(sa.sin_addr)) != 1) {
                throw std::invalid_argument("Transit switch ip address is not in the expect format");
              }
              substrate_in.ip = sa.sin_addr.s_addr;
              substrate_in.eptype = TRAN_SUBSTRT_EP;
              uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
              substrate_in.remote_ips.remote_ips_val = remote_ips;
              substrate_in.remote_ips.remote_ips_len = 0;
              // the below will throw invalid_argument exceptions when it cannot convert the mac string
              aca_convert_to_mac_array(
                      current_SubnetConfiguration.transit_switches(k)
                              .mac_address()
                              .c_str(),
                      substrate_in.mac);
              substrate_in.hosted_interface = EMPTY_STRING;
              substrate_in.veth = EMPTY_STRING;
              substrate_in.tunid = TRAN_SUBSTRT_VNI;

              exec_command_rc = this->execute_command(
                      transitd_command, &substrate_in, culminative_dataplane_programming_time);
              if (exec_command_rc == EXIT_SUCCESS) {
                ACA_LOG_INFO("Successfully updated substrate in transit daemon\n");
              } else {
                ACA_LOG_ERROR("Unable to update substrate in transit daemon: %d\n",
                              exec_command_rc);
                overall_rc = exec_command_rc;
              }
            } // for (int k = 0; k < current_SubnetConfiguration.transit_switches_size(); k++)

            namespace_name = current_PortConfiguration.network_ns();

            // aca_fix_namespace_name always return true when not creating a namespace
            aca_fix_namespace_name(namespace_name, vpc_id, false,
                                   culminative_network_configuration_time);

            net_config_rc = Aca_Net_Config::get_instance().rename_veth_device(
                    namespace_name, temp_name_string, veth_name_string,
                    culminative_network_configuration_time);
            if (net_config_rc == EXIT_SUCCESS) {
              ACA_LOG_INFO("Successfully renamed in ns: %s, old_veth: %s, new_veth: %s\n",
                           namespace_name.c_str(), temp_name_string.c_str(),
                           veth_name_string.c_str());
            } else {
              ACA_LOG_ERROR("Unable to renamed in ns: %s, old_veth: %s, new_veth: %s\n",
                            namespace_name.c_str(), temp_name_string.c_str(),
                            veth_name_string.c_str());
              overall_rc = net_config_rc;
            }

            // found subnet information and completed the work, breaking out
            break;
          }
        }
      } // for (int j = 0; j < parsed_struct.subnet_states_size(); j++)
    } catch (const std::invalid_argument &e) {
      ACA_LOG_ERROR("Invalid argument exception caught while parsing FINALIZE substrate port configuration, message: %s.\n",
                    e.what());
      overall_rc = -EINVAL;
    } catch (const std::exception &e) {
      ACA_LOG_ERROR("Exception caught while parsing FINALIZE substrate port configuration, message: %s.\n",
                    e.what());
      overall_rc = -EXIT_FAILURE;
    } catch (...) {
      ACA_LOG_ERROR("Unknown exception caught while parsing FINALIZE substrate port configuration, rethrowing.\n");
      throw; // rethrowing
    }
  }

  auto operation_end = chrono::steady_clock::now();

  auto operation_total_time =
          cast_to_nanoseconds(operation_end - operation_start).count();

  add_goal_state_operation_status(
          gsOperationReply, port_id, PORT, current_PortState.operation_type(),
          overall_rc, culminative_dataplane_programming_time,
          culminative_network_configuration_time, operation_total_time);

  return overall_rc;
}

int Aca_Comm_Manager::update_port_states(const GoalState &parsed_struct,
                                         GoalStateOperationReply &gsOperationReply)
{
  std::vector<std::future<int> > workitem_future;
  int rc;
  int overall_rc = EXIT_SUCCESS;

  // if (parsed_struct.port_states_size() == 0)
  //   overall_rc = EXIT_SUCCESS in the current logic

  for (int i = 0; i < parsed_struct.port_states_size(); i++) {
    ACA_LOG_DEBUG("=====>parsing port states #%d\n", i);

    PortState current_PortState = parsed_struct.port_states(i);

    workitem_future.push_back(std::async(
            std::launch::async, &Aca_Comm_Manager::update_port_state_workitem, this,
            current_PortState, std::ref(parsed_struct), std::ref(gsOperationReply)));

    // keeping below just in case if we want to call it serially
    // rc = update_port_state_workitem(current_PortState, parsed_struct, gsOperationReply);
    // if (rc != EXIT_SUCCESS)
    //   overall_rc = rc;
  } // for (int i = 0; i < parsed_struct.port_states_size(); i++)

  for (int i = 0; i < parsed_struct.port_states_size(); i++) {
    rc = workitem_future[i].get();
    if (rc != EXIT_SUCCESS)
      overall_rc = rc;
  } // for (int i = 0; i < parsed_struct.port_states_size(); i++)

  return overall_rc;
}

int Aca_Comm_Manager::update_goal_state(const GoalState &parsed_struct,
                                        GoalStateOperationReply &gsOperationReply)
{
  int exec_command_rc = -EXIT_FAILURE;
  int rc = EXIT_SUCCESS;
  auto start = chrono::steady_clock::now();

  ACA_LOG_DEBUG("Starting to update goal state\n");

  exec_command_rc = update_vpc_states(parsed_struct, gsOperationReply);
  if (exec_command_rc != EXIT_SUCCESS) {
    ACA_LOG_ERROR("Failed to update vpc state. Failed with error code %d\n", exec_command_rc);
    rc = exec_command_rc;
  }

  exec_command_rc = update_subnet_states(parsed_struct, gsOperationReply);
  if (exec_command_rc != EXIT_SUCCESS) {
    ACA_LOG_ERROR("Failed to update subnet state. Failed with error code %d\n", exec_command_rc);
    rc = exec_command_rc;
  }

  exec_command_rc = update_port_states(parsed_struct, gsOperationReply);
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

void Aca_Comm_Manager::add_goal_state_operation_status(
        alcorcontroller::GoalStateOperationReply &gsOperationReply, string id,
        alcorcontroller::ResourceType resource_type, alcorcontroller::OperationType operation_type,
        int operation_rc, ulong culminative_dataplane_programming_time,
        ulong culminative_network_configuration_time, ulong state_elapse_time)
{
  OperationStatus overall_operation_status;

  if (operation_rc == EXIT_SUCCESS)
    overall_operation_status = OperationStatus::SUCCESS;
  else if (operation_rc == -EINVAL)
    overall_operation_status = OperationStatus::INVALID_ARG;
  else
    overall_operation_status = OperationStatus::FAILURE;

  ACA_LOG_DEBUG("gsOperationReply - resource_id: %s\n", id.c_str());
  ACA_LOG_DEBUG("gsOperationReply - resource_type: %d\n", resource_type);
  ACA_LOG_DEBUG("gsOperationReply - operation_type: %d\n", operation_type);
  ACA_LOG_DEBUG("gsOperationReply - operation_status: %d\n", overall_operation_status);
  ACA_LOG_DEBUG("gsOperationReply - dataplane_programming_time: %lu\n",
                culminative_dataplane_programming_time);
  ACA_LOG_DEBUG("gsOperationReply - network_configuration_time: %lu\n",
                culminative_network_configuration_time);
  ACA_LOG_DEBUG("gsOperationReply - total_operation_time: %lu\n", state_elapse_time);

  // -----critical section starts-----
  // (exclusive write access to gsOperationReply signaled by locking gs_reply_mutex):
  gs_reply_mutex.lock();
  GoalStateOperationReply_GoalStateOperationStatus *new_operation_statuses =
          gsOperationReply.add_operation_statuses();
  new_operation_statuses->set_resource_id(id);
  new_operation_statuses->set_resource_type(resource_type);
  new_operation_statuses->set_operation_type(operation_type);
  new_operation_statuses->set_operation_status(overall_operation_status);
  new_operation_statuses->set_dataplane_programming_time(culminative_dataplane_programming_time);
  new_operation_statuses->set_network_configuration_time(culminative_network_configuration_time);
  new_operation_statuses->set_state_elapse_time(state_elapse_time);
  gs_reply_mutex.unlock();
  // -----critical section ends-----
}

int Aca_Comm_Manager::load_agent_xdp(string interface, ulong &culminative_time)
{
  int rc;
  rpc_trn_xdp_intf_t xdp_inf_in;
  char inf[20];

  int transitd_command = LOAD_TRANSIT_AGENT_XDP;

  strncpy(inf, interface.c_str(), strlen(interface.c_str()) + 1);
  xdp_inf_in.interface = inf;
  xdp_inf_in.xdp_path = agent_xdp_path;
  xdp_inf_in.pcapfile = agent_pcap_file;

  rc = this->execute_command(transitd_command, &xdp_inf_in, culminative_time);
  if (rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("Successfully loaded transit agent on interface: %s\n",
                 xdp_inf_in.interface);
  } else {
    ACA_LOG_ERROR("Unable to load transit agent on interface: %s, rc: %d\n",
                  xdp_inf_in.interface, rc);
  }

  return rc;
}

int Aca_Comm_Manager::unload_agent_xdp(string interface, ulong &culminative_time)
{
  int rc;
  rpc_intf_t inf_in;
  char inf[20];

  int transitd_command = UNLOAD_TRANSIT_AGENT_XDP;

  strncpy(inf, interface.c_str(), strlen(interface.c_str()) + 1);
  inf_in.interface = inf;

  rc = this->execute_command(transitd_command, &inf_in, culminative_time);
  if (rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("Successfully unloaded transit agent on interface: %s\n", inf_in.interface);
  } else {
    ACA_LOG_ERROR("Unable to unload transit agent on interface: %s, rc: %d\n",
                  inf_in.interface, rc);
  }

  return rc;
}

int Aca_Comm_Manager::execute_command(int command, void *input_struct, ulong &culminative_time)
{
  CLIENT *client;
  int rc = EXIT_SUCCESS;
  int *transitd_return;

  ACA_LOG_INFO("Connecting to %s using %s protocol.\n", g_rpc_server.c_str(),
               g_rpc_protocol.c_str());

  if (g_debug_mode) {
    // This is for debugging only, to be removed after stabilization
    switch (command) {
    case UPDATE_VPC: {
      rpc_trn_vpc_t *rpc_trn_vpc_t_in = (rpc_trn_vpc_t *)input_struct;
      ACA_LOG_DEBUG("[execute_command] Calling UPDATE VPC with interface %s, tunid %lu, routers_ips_len %u .\n",
                    rpc_trn_vpc_t_in->interface, rpc_trn_vpc_t_in->tunid,
                    rpc_trn_vpc_t_in->routers_ips.routers_ips_len);
      for (uint i = 0; i < rpc_trn_vpc_t_in->routers_ips.routers_ips_len; i++) {
        ACA_LOG_DEBUG("[execute_command] routers_ips_val[%d]: %u .\n", i,
                      rpc_trn_vpc_t_in->routers_ips.routers_ips_val[i]);
      }
      break;
    }
    case UPDATE_NET: {
      rpc_trn_network_t *rpc_trn_network_t_in = (rpc_trn_network_t *)input_struct;
      ACA_LOG_DEBUG("[execute_command] Calling UPDATE NET with interface %s, prefixlen %u, tunid %lu, netip: %u, switches_ips_len %u .\n",
                    rpc_trn_network_t_in->interface, rpc_trn_network_t_in->prefixlen,
                    rpc_trn_network_t_in->tunid, rpc_trn_network_t_in->netip,
                    rpc_trn_network_t_in->switches_ips.switches_ips_len);
      for (uint i = 0; i < rpc_trn_network_t_in->switches_ips.switches_ips_len; i++) {
        ACA_LOG_DEBUG("[execute_command] switches_ips_val[%d]: %u .\n", i,
                      rpc_trn_network_t_in->switches_ips.switches_ips_val[i]);
      }
      break;
    }
    case UPDATE_EP: {
      rpc_trn_endpoint_t *rpc_trn_endpoint_t_in = (rpc_trn_endpoint_t *)input_struct;
      ACA_LOG_DEBUG("[execute_command] Calling UPDATE EP with interface %s, ip %u, "
                    "eptype %u, mac: %02x:%02x:%02x:%02x:%02x:%02x, hosted_interface: %s, "
                    "veth: %s, tunid: %lu, remote_ips_len %u .\n",
                    rpc_trn_endpoint_t_in->interface, rpc_trn_endpoint_t_in->ip,
                    rpc_trn_endpoint_t_in->eptype, rpc_trn_endpoint_t_in->mac[0],
                    rpc_trn_endpoint_t_in->mac[1], rpc_trn_endpoint_t_in->mac[2],
                    rpc_trn_endpoint_t_in->mac[3], rpc_trn_endpoint_t_in->mac[4],
                    rpc_trn_endpoint_t_in->mac[5], rpc_trn_endpoint_t_in->hosted_interface,
                    rpc_trn_endpoint_t_in->veth, rpc_trn_endpoint_t_in->tunid,
                    rpc_trn_endpoint_t_in->remote_ips.remote_ips_len);
      for (uint i = 0; i < rpc_trn_endpoint_t_in->remote_ips.remote_ips_len; i++) {
        ACA_LOG_DEBUG("[execute_command] remote_ips_val[%d]: %u .\n", i,
                      rpc_trn_endpoint_t_in->remote_ips.remote_ips_val[i]);
      }
      break;
    }
    case UPDATE_AGENT_EP: {
      rpc_trn_endpoint_t *rpc_trn_endpoint_t_in = (rpc_trn_endpoint_t *)input_struct;
      ACA_LOG_DEBUG("[execute_command] Calling UPDATE AGENT EP with interface %s, ip %u, "
                    "eptype %u, mac: %02x:%02x:%02x:%02x:%02x:%02x, hosted_interface: %s, "
                    "veth: %s, tunid: %lu, remote_ips_len %u .\n",
                    rpc_trn_endpoint_t_in->interface, rpc_trn_endpoint_t_in->ip,
                    rpc_trn_endpoint_t_in->eptype, rpc_trn_endpoint_t_in->mac[0],
                    rpc_trn_endpoint_t_in->mac[1], rpc_trn_endpoint_t_in->mac[2],
                    rpc_trn_endpoint_t_in->mac[3], rpc_trn_endpoint_t_in->mac[4],
                    rpc_trn_endpoint_t_in->mac[5], rpc_trn_endpoint_t_in->hosted_interface,
                    rpc_trn_endpoint_t_in->veth, rpc_trn_endpoint_t_in->tunid,
                    rpc_trn_endpoint_t_in->remote_ips.remote_ips_len);
      for (uint i = 0; i < rpc_trn_endpoint_t_in->remote_ips.remote_ips_len; i++) {
        ACA_LOG_DEBUG("[execute_command] remote_ips_val[%d]: %u .\n", i,
                      rpc_trn_endpoint_t_in->remote_ips.remote_ips_val[i]);
      }
      break;
    }
    case UPDATE_AGENT_MD: {
      rpc_trn_agent_metadata_t *rpc_trn_agent_metadata_t_in =
              (rpc_trn_agent_metadata_t *)input_struct;
      rpc_trn_tun_intf_t *rpc_trn_tun_intf_t_in = &(rpc_trn_agent_metadata_t_in->eth);
      rpc_trn_endpoint_t *rpc_trn_endpoint_t_in = &(rpc_trn_agent_metadata_t_in->ep);
      rpc_trn_network_t *rpc_trn_network_t_in = &(rpc_trn_agent_metadata_t_in->net);

      ACA_LOG_DEBUG("[execute_command] Calling UPDATE AGENT MD with eth interface %s, intf.interface %s, intf. ip %u, "
                    "mac: %02x:%02x:%02x:%02x:%02x:%02x \n",
                    rpc_trn_agent_metadata_t_in->interface,
                    rpc_trn_tun_intf_t_in->interface, rpc_trn_tun_intf_t_in->ip,
                    rpc_trn_tun_intf_t_in->mac[0], rpc_trn_tun_intf_t_in->mac[1],
                    rpc_trn_tun_intf_t_in->mac[2], rpc_trn_tun_intf_t_in->mac[3],
                    rpc_trn_tun_intf_t_in->mac[4], rpc_trn_tun_intf_t_in->mac[5]);

      ACA_LOG_DEBUG("[execute_command] Calling UPDATE AGENT MD with ep interface %s, ip %u, "
                    "eptype %u, mac: %02x:%02x:%02x:%02x:%02x:%02x, hosted_interface: %s, "
                    "veth: %s, tunid: %lu, remote_ips_len %u .\n",
                    rpc_trn_endpoint_t_in->interface, rpc_trn_endpoint_t_in->ip,
                    rpc_trn_endpoint_t_in->eptype, rpc_trn_endpoint_t_in->mac[0],
                    rpc_trn_endpoint_t_in->mac[1], rpc_trn_endpoint_t_in->mac[2],
                    rpc_trn_endpoint_t_in->mac[3], rpc_trn_endpoint_t_in->mac[4],
                    rpc_trn_endpoint_t_in->mac[5], rpc_trn_endpoint_t_in->hosted_interface,
                    rpc_trn_endpoint_t_in->veth, rpc_trn_endpoint_t_in->tunid,
                    rpc_trn_endpoint_t_in->remote_ips.remote_ips_len);
      for (uint i = 0; i < rpc_trn_endpoint_t_in->remote_ips.remote_ips_len; i++) {
        ACA_LOG_DEBUG("[execute_command] remote_ips_val[%d]: %u .\n", i,
                      rpc_trn_endpoint_t_in->remote_ips.remote_ips_val[i]);
      }

      ACA_LOG_DEBUG("[execute_command] Calling UPDATE AGENT MD with net interface: "
                    " %s prefixlen: %u, tunid: %lu, netip: %u, switches_ips_len: %u .\n",
                    rpc_trn_network_t_in->interface, rpc_trn_network_t_in->prefixlen,
                    rpc_trn_network_t_in->tunid, rpc_trn_network_t_in->netip,
                    rpc_trn_network_t_in->switches_ips.switches_ips_len);
      for (uint i = 0; i < rpc_trn_network_t_in->switches_ips.switches_ips_len; i++) {
        ACA_LOG_DEBUG("[execute_command] switches_ips_val[%d]: %u .\n", i,
                      rpc_trn_network_t_in->switches_ips.switches_ips_val[i]);
      }

      break;
    }

    case LOAD_TRANSIT_AGENT_XDP: {
      // add debug print if needed
      break;
    }

    case UNLOAD_TRANSIT_AGENT_XDP: {
      // add debug print if needed
      break;
    }

    default:
      ACA_LOG_ERROR("Unknown controller command in debug print: %d\n", command);
      rc = EXIT_FAILURE;
      break;
    }
  }

  auto rpc_client_start = chrono::steady_clock::now();

  // -----critical section starts-----
  // (exclusive access to rpc client and call):
  rpc_client_call_mutex.lock();

  // TODO: We may change it to have a static client for health checking on
  // transit daemon in the future.
  client = clnt_create((char *)g_rpc_server.c_str(), RPC_TRANSIT_REMOTE_PROTOCOL,
                       RPC_TRANSIT_ALFAZERO, (char *)g_rpc_protocol.c_str());

  if (client == NULL) {
    clnt_pcreateerror((char *)g_rpc_server.c_str());
    ACA_LOG_EMERG("Not able to create the RPC connection to Transit daemon.\n");
    rc = EXIT_FAILURE;
  } else {
    auto rpc_call_start = chrono::steady_clock::now();

    switch (command) {
    case UPDATE_VPC:
      transitd_return = update_vpc_1((rpc_trn_vpc_t *)input_struct, client);
      break;
    case UPDATE_NET:
      transitd_return = update_net_1((rpc_trn_network_t *)input_struct, client);
      break;
    case UPDATE_EP:
      transitd_return = update_ep_1((rpc_trn_endpoint_t *)input_struct, client);
      break;
    case UPDATE_AGENT_EP:
      transitd_return = update_agent_ep_1((rpc_trn_endpoint_t *)input_struct, client);
      break;
    case UPDATE_AGENT_MD:
      transitd_return = update_agent_md_1((rpc_trn_agent_metadata_t *)input_struct, client);
      break;
    case DELETE_NET:
      // rc = DELETE_NET ...
      break;
    case DELETE_EP:
      // rc = DELETE_EP ...
      break;
    case DELETE_AGENT_EP:
      // rc = DELETE_AGENT_EP ...
      break;
    case DELETE_AGENT_MD:
      // rc = DELETE_AGENT_MD ...
      break;
    case GET_VPC:
      // rpc_trn_vpc_t = GET_VPC ...
      break;
    case GET_NET:
      // rpc_trn_vpc_t = GET_NET ...
      break;
    case GET_EP:
      // rpc_trn_vpc_t = GET_EP ...
      break;
    case GET_AGENT_EP:
      // rpc_trn_vpc_t = GET_AGENT_EP ...
      break;
    case GET_AGENT_MD:
      // rpc_trn_vpc_t = GET_AGENT_MD ...
      break;
    case LOAD_TRANSIT_XDP:
      // rc = LOAD_TRANSIT_XDP ...
      break;
    case LOAD_TRANSIT_AGENT_XDP:
      transitd_return = load_transit_agent_xdp_1((rpc_trn_xdp_intf_t *)input_struct, client);
      break;
    case UNLOAD_TRANSIT_XDP:
      // rc = UNLOAD_TRANSIT_XDP ...
      break;
    case UNLOAD_TRANSIT_AGENT_XDP:
      transitd_return = unload_transit_agent_xdp_1((rpc_intf_t *)input_struct, client);
      break;
    default:
      ACA_LOG_ERROR("Unknown controller command: %d\n", command);
      rc = -EXIT_FAILURE;
      break;
    }

    auto rpc_call_end = chrono::steady_clock::now();

    auto rpc_call_time_total_time =
            cast_to_nanoseconds(rpc_call_end - rpc_call_start).count();

    g_total_rpc_call_time += rpc_call_time_total_time;

    ACA_LOG_INFO("Elapsed time for transit daemon command %d took: %ld nanoseconds or %ld milliseconds.\n",
                 command, rpc_call_time_total_time, rpc_call_time_total_time / 1000000);

    if (transitd_return == (int *)NULL) {
      clnt_perror(client, "Call failed to program Transit daemon");
      ACA_LOG_EMERG("Call failed to program Transit daemon, command: %d.\n", command);
      rc = -EXIT_FAILURE;
    } else if (*transitd_return != EXIT_SUCCESS) {
      ACA_LOG_EMERG("Call failed to program Transit daemon, command %d, transitd_return: %d.\n",
                    command, *transitd_return);
      rc = -EXIT_FAILURE;
    }
    if (rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully updated transitd with command %d.\n", command);
    }

    clnt_destroy(client);
  }

  rpc_client_call_mutex.unlock();
  // -----critical section ends-----

  auto rpc_client_end = chrono::steady_clock::now();

  auto rpc_client_time_total_time =
          cast_to_nanoseconds(rpc_client_end - rpc_client_start).count();

  culminative_time += rpc_client_time_total_time;

  g_total_rpc_client_time += rpc_client_time_total_time;

  ACA_LOG_INFO("Elapsed time for both RPC client create/destroy and transit "
               "daemon command %d took: %ld nanoseconds or %ld milliseconds.\n",
               command, rpc_client_time_total_time, rpc_client_time_total_time / 1000000);

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

    for (int j = 0; j < current_PortConfiguration.extra_dhcp_options_size(); j++) {
      fprintf(stdout, "current_PortConfiguration.extra_dhcp_options(%d): name %s, value %s \n",
              j, current_PortConfiguration.extra_dhcp_options(j).name().c_str(),
              current_PortConfiguration.extra_dhcp_options(j).value().c_str());
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
