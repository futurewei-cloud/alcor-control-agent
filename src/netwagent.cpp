
#include <unistd.h> /* for getopt */
#include <thread>
#include "aca_log.h"
#include "aca_util.h"
#include "goalstate.pb.h"
#include "messageconsumer.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_comm_mgr.h"
#include "cppkafka/utils/consumer_dispatcher.h"
#include <grpcpp/grpcpp.h>
#include "aca_async_grpc_server.h"

using messagemanager::MessageConsumer;
using std::string;

#define ACALOGNAME "AliothControlAgent"

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
long g_total_rpc_call_time = 0;
long g_total_rpc_client_time = 0;
long g_total_update_GS_time = 0;
bool g_debug_mode = false;
bool g_fastpath_mode = false;

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
    if (dispatcher != NULL) //Currently is always NULL
    {
        dispatcher->stop();
        delete dispatcher;
        dispatcher = NULL;
        ACA_LOG_INFO("Cleaned up Kafka dispatched consumer.\n");
    }
    else
    {
        ACA_LOG_ERROR("Unable to call delete, dispatcher pointer is null");
    }

    if (async_grpc_server != NULL)
    {
            async_grpc_server->StopServer();
            delete async_grpc_server;
            async_grpc_server = NULL;
            ACA_LOG_INFO("Cleaned up async grpc server.\n");
    }
    else
    {
        ACA_LOG_ERROR("Unable to call delete, async grpc server pointer is null.\n");
    }

    if(async_grpc_server_thread != NULL) {
        if(async_grpc_server_thread->joinable())
        {
            async_grpc_server_thread->join();
            ACA_LOG_INFO("Joined GRPC server thread.\n");
        }
        else
        {
            ACA_LOG_ERROR("Async grpc server thread is not joinable.\n");
        }
        delete async_grpc_server_thread;
        async_grpc_server_thread = NULL;
        ACA_LOG_INFO("Cleaned up async grpc server thread.\n");
    }
    else
    {
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
        default: /* the '?' case when the option is not recognized */
            fprintf(stderr,
                    "Usage: %s\n"
                    "\t\t[-b kafka broker list]\n"
                    "\t\t[-h kafka host topic to listen]\n"
                    "\t\t[-g kafka group id]\n"
                    "\t\t[-s transitd RPC server]\n"
                    "\t\t[-p transitd RPC protocol]\n"
                    "\t\t[-d enable debug mode]\n",
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

    async_grpc_server = new Aca_Async_GRPC_Server();
    async_grpc_server_thread = new std::thread( std::bind( &Aca_Async_GRPC_Server::Run, async_grpc_server ) );

    MessageConsumer network_config_consumer(g_broker_list, g_kafka_group_id);
    network_config_consumer.consumeDispatched(g_kafka_topic);

    aca_cleanup();
    return rc;
}