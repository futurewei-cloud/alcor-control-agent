
#include <unistd.h> /* for getopt */
#include "aca_log.h"
#include "aca_util.h"
#include "goalstate.pb.h"
#include "messageconsumer.h"
#include "cppkafka/utils/consumer_dispatcher.h"

using messagemanager::MessageConsumer;
using std::string;

#define ACALOGNAME "AliothControlAgent"

using namespace std;

// Global variables
cppkafka::ConsumerDispatcher *dispatcher = NULL;
string g_broker_list = EMPTY_STRING;
string g_kafka_topic = EMPTY_STRING;
string g_kafka_group_id = EMPTY_STRING;
string g_rpc_server = EMPTY_STRING;
string g_rpc_protocol = EMPTY_STRING;
bool g_debug_mode = false;

static void aca_cleanup()
{
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

    while ((option = getopt(argc, argv, "b:h:g:s:p:d")) != -1)
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

    // Announce this host (agent) and register in every kafka cluster
    // P0, tracked by issue#12

    // Launch background threads to monitor and to emit network health status
    //	for customer VMs, containers, as well as
    //	infra host services including OVS, transit etc.
    // P1, tracked by issue#13

    // Upload or refresh the networking spec of this host
    // (including DPDK, SR-IOV, bandwidth etc.)
    // P1, tracked by issue#14
    // MessageProducer host_spec_producer(broker_list, topic_host_spec,
    // partition_value); cout << "broker list:" << host_spec_producer.getBrokers()
    // << endl; cout << "topic:" << host_spec_producer.getTopicName() << endl;
    // cout << "partition:" << host_spec_producer.getPartitionValue() << endl;

    // string host_network_spec = "fake config";
    // cout << "Prepare for publishing " << host_network_spec  <<endl;
    // host_spec_producer.publish(host_network_spec);
    // cout << "Publish completed" << endl;

    MessageConsumer network_config_consumer(g_broker_list, g_kafka_group_id);

    network_config_consumer.cosumeDispatched(g_kafka_topic);
    /* never reached */

    aca_cleanup();

    return rc;
}
