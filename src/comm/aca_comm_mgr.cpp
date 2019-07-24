#include "aca_comm_mgr.h"
#include "aca_log.h"
#include "goalstate.pb.h"
#include "messageconsumer.h"
#include "trn_rpc_protocol.h"
#include <chrono>
#include <errno.h>
#include <iostream>
#include <thread>

using std::string;
using namespace std::chrono_literals;
using messagemanager::MessageConsumer;

extern bool g_test_mode;
extern char *g_rpc_server;
extern char *g_rpc_protocol;

namespace aca_comm_manager {

int Aca_Comm_Manager::process_messages() {

  aliothcontroller::GoalState deserialized_GoalState;
  int rc = EXIT_FAILURE;

  // Preload network agent configuration
  // TODO: load it from configuration file
  // string host_id = "00000000-0000-0000-0000-000000000000";
  string broker_list = "10.213.43.188:9092";
  string topic_host_spec = "kafka_test2"; //"/hostid/" + host_id + "/hostspec/";
  // int partition_value = 0;

  // Listen to Kafka clusters for any network configuration operations
  // P0, tracked by issue#15
  MessageConsumer network_config_consumer(broker_list, "test");
  string **payload;

  ACA_LOG_DEBUG(
      "Going into keep listening loop, press ctrl-C or kill process ID #: "
      "%d to exit.\n",
      getpid());

  do {
    bool poll_res = network_config_consumer.consume(topic_host_spec, payload);
    if (poll_res) {
      ACA_LOG_INFO("Processing payload....: %s.\n", (**payload).c_str());

      rc = this->deserialize(**payload, deserialized_GoalState);

      if (rc == EXIT_SUCCESS) {
        fprintf(stdout, "deserialized_GoalState.vpc_states_size() = %d; \n",
                deserialized_GoalState.vpc_states_size());
        // Call parse_goal_state
        rc = update_goal_state(deserialized_GoalState);
        if (rc != EXIT_SUCCESS) {
          ACA_LOG_ERROR("Failed to update transitd with goal state %d.\n", rc);
        } else {
          ACA_LOG_ERROR("Successfully updated transitd with goal state %d.\n",
                        rc);
        }
      }
      if ((payload != nullptr) && (*payload != nullptr)) {
        delete *payload;
      }
    } else {
      ACA_LOG_ERROR("Consume message failed.\n");
    }

    std::this_thread::sleep_for(5s);

  } while (true);

  /* never reached */
  return rc;
}

int Aca_Comm_Manager::deserialize(string kafka_message,
                                  aliothcontroller::GoalState &parsed_struct) {
  int rc = EXIT_FAILURE;

  // deserialize any new configuration
  // P0, tracked by issue#16

  if (kafka_message.empty()) {
    ACA_LOG_ERROR("Empty kafka message rc: %d\n", rc);
    return EINVAL;
  }

  if (parsed_struct.IsInitialized() == false) {
    ACA_LOG_ERROR("Uninitialized parsed_struct rc: %d\n", rc);
    return EINVAL;
  }

  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (parsed_struct.ParseFromArray(kafka_message.c_str(),
                                   kafka_message.size())) {
    ACA_LOG_INFO("Successfully converted kafka message to protobuf struct\n");
    return EXIT_SUCCESS;
  } else {
    ACA_LOG_ERROR("Failed to convert kafka message to protobuf struct\n");
    return EXIT_FAILURE;
  }
}

// Calls execute
int Aca_Comm_Manager::update_goal_state(
    aliothcontroller::GoalState &deserialized_GoalState) {
  int rc = EXIT_FAILURE;
  for (int i = 0; i < deserialized_GoalState.vpc_states_size(); i++) {

    fprintf(
        stdout,
        "deserialized_GoalState.vpc_states(i).operation_type(): %d matched \n",
        deserialized_GoalState.vpc_states(i).operation_type());

    fprintf(stdout,
            "deserialized_GoalState.vpc_states(i).configuration().project_id():"
            " %s matched \n",
            deserialized_GoalState.vpc_states(i)
                .configuration()
                .project_id()
                .c_str());

    fprintf(stdout,
            "deserialized_GoalState.vpc_states(i).configuration().id(): %s "
            "matched \n",
            deserialized_GoalState.vpc_states(i).configuration().id().c_str());

    fprintf(
        stdout,
        "deserialized_GoalState.vpc_states(i).configuration().name(): %s "
        "matched \n",
        deserialized_GoalState.vpc_states(i).configuration().name().c_str());

    fprintf(
        stdout,
        "deserialized_GoalState.vpc_states(i).configuration().cidr(): %s "
        "matched \n",
        deserialized_GoalState.vpc_states(i).configuration().cidr().c_str());
    int transitd_command = 0;
    void *transitd_input;

    switch (deserialized_GoalState.vpc_states(i).operation_type()) {
    case aliothcontroller::OperationType::CREATE:
    case aliothcontroller::OperationType::UPDATE:
      transitd_command = UPDATE_VPC;
      transitd_input = (rpc_trn_vpc_t *)malloc(sizeof(rpc_trn_vpc_t));
      if (transitd_input != NULL) {
        rpc_trn_vpc_t *vpc_input = (rpc_trn_vpc_t *)transitd_input;
        vpc_input->interface = (char *)"eth0";
        vpc_input->tunid = std::stoi(
            deserialized_GoalState.vpc_states(i).configuration().id().c_str());
        rc = EXIT_SUCCESS;
      } else {
        ACA_LOG_EMERG("Out of memory when allocating with size: %lu.\n",
                      sizeof(rpc_trn_vpc_t));
        rc = ENOMEM;
      }
      break;
    case aliothcontroller::OperationType::DELETE:
      /* code */
      break;
    case aliothcontroller::OperationType::GET:
      /* code */
      break;
    default:
      ACA_LOG_DEBUG("Invalid VPC state operation type %d/n",
                    deserialized_GoalState.vpc_states(i).operation_type());
      break;
    }
    if (rc == EXIT_SUCCESS) {
      rc = this->execute_command(transitd_command, transitd_input);
      if (rc == EXIT_SUCCESS) {
        ACA_LOG_INFO("Successfully executed the network controller command");
      } else {
        ACA_LOG_ERROR("Unable to execute the network controller command: %d\n",
                      rc);
        // TODO: Notify the Network Controller if the command is not successful.
      }
      if (transitd_input) {
        free(transitd_input);
        transitd_input = NULL;
      }
    }
  }
  return rc;
}

int Aca_Comm_Manager::execute_command(int command, void *input_struct) {
  static CLIENT *client;
  int rc = EXIT_SUCCESS;
  int *transitd_return;
  // Depending on different operations, program XDP through corresponding RPC
  // apis by transit daemon
  // P0, tracked by issue#17
  ACA_LOG_INFO("Connecting to %s using %s protocol.\n", g_rpc_server,
               g_rpc_protocol);

  client = clnt_create(g_rpc_server, RPC_TRANSIT_REMOTE_PROTOCOL,
                       RPC_TRANSIT_ALFAZERO, g_rpc_protocol);

  if (client == NULL) {
    clnt_pcreateerror(g_rpc_server);
    ACA_LOG_EMERG("Not able to create the RPC connection to Transit daemon.\n");
    rc = EXIT_FAILURE;
  } else {
    switch (command) {
    case UPDATE_VPC:
      transitd_return = update_vpc_1((rpc_trn_vpc_t *)input_struct, client);
      break;
    case UPDATE_NET:
      // rc = UPDATE_NET_1 ...
      break;
    case UPDATE_EP:
      // rc = UPDATE_EP_1 ...
      break;
    case UPDATE_AGENT_EP:
      // rc = UPDATE_AGENT_EP_1 ...
      break;
    case UPDATE_AGENT_MD:
      // rc = UPDATE_AGENT_MD ...
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
      // rc = LOAD_TRANSIT_AGENT_XDP ...
      break;
    case UNLOAD_TRANSIT_XDP:
      // rc = UNLOAD_TRANSIT_XDP ...
      break;
    case UNLOAD_TRANSIT_AGENT_XDP:
      // rc = UNLOAD_TRANSIT_AGENT_XDP ...
      break;

    default:
      ACA_LOG_ERROR("Unknown controller command: %d\n", command);
      rc = EXIT_FAILURE;
      break;
    }

    if (transitd_return == (int *)NULL) {
      clnt_perror(client, "Call failed to program Transit daemon");
      ACA_LOG_EMERG("Call failed to program Transit daemon, command: %d.\n",
                    command);
      rc = EXIT_FAILURE;
    } else if (transitd_return != EXIT_SUCCESS) {
      ACA_LOG_EMERG("Fatal error for command: %d.\n", command);
      // TODO: report the error back to network controller
      rc = EXIT_FAILURE;
    }
    if (rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Successfully updated transitd with command %d.\n",
                   command);
    }
    // TODO: can print out more command specific info

    clnt_destroy(client);
  }

  return rc;
}
} // namespace aca_comm_manager