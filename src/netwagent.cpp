
#include <unistd.h> /* for getopt */
#include "aca_log.h"
#include "aca_util.h"
#include "goalstate.pb.h"
#include "messageconsumer.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_comm_mgr.h"
#include "cppkafka/utils/consumer_dispatcher.h"
#include <grpcpp/grpcpp.h>

using messagemanager::MessageConsumer;
using std::string;

#define ACALOGNAME "AliothControlAgent"

using aca_comm_manager::Aca_Comm_Manager;
using namespace std;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// Global variables
cppkafka::ConsumerDispatcher *dispatcher = NULL;
string g_broker_list = EMPTY_STRING;
string g_kafka_topic = EMPTY_STRING;
string g_kafka_group_id = EMPTY_STRING;
string g_rpc_server = EMPTY_STRING;
string g_rpc_protocol = EMPTY_STRING;
long g_total_rpc_call_time = 0;
long g_total_rpc_client_time = 0;
long g_total_update_GS_time = 0;
bool g_debug_mode = false;
bool g_fastpath_mode = false;

// Logic and data behind the server's behavior.
class GreeterServiceImpl final : public aliothcontroller::GoalStateProvisioner::Service
{
    Status PushNetworkResourceStates(ServerContext *context, const aliothcontroller::GoalState *requestedGoalState,
                                     aliothcontroller::GoalStateOperationReply *OperationReply) override
    {

        int rc = Aca_Comm_Manager::get_instance().update_goal_state(*requestedGoalState);
        if (rc != EXIT_SUCCESS)
        {
            ACA_LOG_ERROR("Control Fast Path - Failed to update transitd with latest goal state, rc=%d.\n", rc);
        }
        else
        {
            ACA_LOG_INFO("Control Fast Path - Successfully updated transitd with latest goal state %d.\n", rc);
        }

        // TODO: OperationReply already initiated, need to fill it in.

        return Status::OK;
    }
};

void RunServer()
{
    std::string server_address("0.0.0.0:50001");
    GreeterServiceImpl service;

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    // Finally assemble the server.
    std::unique_ptr<Server> server(builder.BuildAndStart());
    ACA_LOG_INFO("GRPC Server listening on %s \n", server_address.c_str());
    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}

static void aca_cleanup()
{
    ACA_LOG_DEBUG("g_total_rpc_call_time = %ld nanoseconds or %ld milliseconds\n",
                  g_total_rpc_call_time, g_total_rpc_call_time / 1000000);

    ACA_LOG_DEBUG("g_total_rpc_client_time = %ld nanoseconds or %ld milliseconds\n",
                  g_total_rpc_client_time, g_total_rpc_client_time / 1000000);

    ACA_LOG_DEBUG("g_total_update_GS_time = %ld nanoseconds or %ld milliseconds\n",
                  g_total_update_GS_time, g_total_update_GS_time / 1000000);

    ACA_LOG_INFO("Program exiting, cleaning up...\n");

    // Optional: Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    // Stop sets a private variable running_ to False
    // The Dispatch checks the variable in a loop and stops when running is
    // no longer set to True.
    if (dispatcher != NULL)
    {
        dispatcher->stop();
        delete dispatcher;
        dispatcher = NULL;
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
    int rc = EXIT_FAILURE;

    ACA_LOG_INIT(ACALOGNAME);

    ACA_LOG_INFO("Network Control Agent started...\n");

    // Register the signal handlers
    signal(SIGINT, aca_signal_handler);
    signal(SIGTERM, aca_signal_handler);

    while ((option = getopt(argc, argv, "b:h:g:s:p:df")) != -1)
    {
        switch (option)
        {
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
        case 'd':
            g_debug_mode = true;
            break;
        case 'f':
            g_fastpath_mode = true;
            break;
        default: /* the '?' case when the option is not recognized */
            fprintf(stderr,
                    "Usage: %s\n"
                    "\t\t[-b kafka broker list]\n"
                    "\t\t[-h kafka host topic to listen]\n"
                    "\t\t[-g kafka group id]\n"
                    "\t\t[-s transitd RPC server]\n"
                    "\t\t[-p transitd RPC protocol]\n"
                    "\t\t[-d enable debug mode]\n"
                    "\t\t[-f enable controller fastpath mode]\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // fill in the information if not provided in command line args
    if (g_broker_list == EMPTY_STRING)
    {
        g_broker_list = BROKER_LIST;
    }
    if (g_kafka_topic == EMPTY_STRING)
    {
        g_kafka_topic = KAFKA_TOPIC;
    }
    if (g_kafka_group_id == EMPTY_STRING)
    {
        g_kafka_group_id = KAFKA_GROUP_ID;
    }
    if (g_rpc_server == EMPTY_STRING)
    {
        g_rpc_server = LOCALHOST;
    }
    if (g_rpc_protocol == EMPTY_STRING)
    {
        g_rpc_protocol = UDP;
    }

    if (g_fastpath_mode)
    {
        RunServer();
        // TODO: need to do this in a seperate thread
    }
    else
    {
        MessageConsumer network_config_consumer(g_broker_list, g_kafka_group_id);

        network_config_consumer.cosumeDispatched(g_kafka_topic);
        /* never reached */
    }

    aca_cleanup();

    return rc;
}
