
#include <unistd.h> /* for getopt */
#include "aca_log.h"
#include "aca_util.h"
#include "goalstate.pb.h"
#include "messageconsumer.h"

using messagemanager::MessageConsumer;
using std::string;

#define ACALOGNAME "AliothControlAgent"

using namespace std;

// Defines
static char BROKER_LIST[] = "10.213.43.158:9092";
static char KAFKA_TOPIC[] = "Host-ts-1";
static char LOCALHOST[] = "localhost";
static char UDP[] = "udp";

// Global variables
char *g_broker_list = NULL;
char *g_kafka_topic = NULL;
char *g_rpc_server = NULL;
char *g_rpc_protocol = NULL;
bool g_test_mode = false;
char *g_test_message = NULL;
bool g_debug_mode = false;

static void aca_cleanup()
{
    // Optional: Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    aca_free(g_broker_list);
    aca_free(g_kafka_topic);
    aca_free(g_rpc_server);
    aca_free(g_rpc_protocol);
    aca_free(g_test_message);

    ACA_LOG_INFO("Program exiting, cleaning up...\n");

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

    // Register the signal handlers
    signal(SIGINT, aca_signal_handler);
    signal(SIGTERM, aca_signal_handler);

    while ((option = getopt(argc, argv, "b:h:s:p:t:d")) != -1)
    {
        switch (option)
        {
        case 'b':
            g_broker_list = (char *)malloc(sizeof(optarg));
            if (g_broker_list != NULL)
            {
                strncpy(g_broker_list, optarg, strlen(optarg) + 1);
            }
            else
            {
                ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                              sizeof(optarg));
                exit(EXIT_FAILURE);
            }
            break;
        case 'h':
            g_kafka_topic = (char *)malloc(sizeof(optarg));
            if (g_kafka_topic != NULL)
            {
                strncpy(g_kafka_topic, optarg, strlen(optarg) + 1);
            }
            else
            {
                ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                              sizeof(optarg));
                exit(EXIT_FAILURE);
            }
            break;
        case 's':
            g_rpc_server = (char *)malloc(sizeof(optarg));
            if (g_rpc_server != NULL)
            {
                strncpy(g_rpc_server, optarg, strlen(optarg) + 1);
            }
            else
            {
                ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                              sizeof(optarg));
                exit(EXIT_FAILURE);
            }
            break;
        case 'p':
            g_rpc_protocol = (char *)malloc(sizeof(optarg));
            if (g_rpc_protocol != NULL)
            {
                strncpy(g_rpc_protocol, optarg, strlen(optarg) + 1);
            }
            else
            {
                ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                              sizeof(optarg));
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            g_test_mode = true;
            g_test_message = (char *)malloc(sizeof(optarg));
            if (g_test_message != NULL)
            {
                strncpy(g_test_message, optarg, strlen(optarg) + 1);
            }
            else
            {
                ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                              sizeof(optarg));
                exit(EXIT_FAILURE);
            }
            break;
        case 'd':
            g_debug_mode = true;
            break;
        default: /* the '?' case when the option is not recognized */
            fprintf(stderr,
                    "Usage: %s\n"
                    "\t\t[-b kafka broker list]\n"
                    "\t\t[-h kafka host topic to listen]\n"
                    "\t\t[-s transitd RPC server]\n"
                    "\t\t[-p transitd RPC protocol]\n"
                    "\t\t[-t test message to parse and enable test mode]\n"
                    "\t\t[-d enable debug mode]\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // fill in the information if not provided in command line args
    if (g_broker_list == NULL)
    {
        g_broker_list = (char *)malloc(sizeof(BROKER_LIST));
        if (g_broker_list != NULL)
        {
            strncpy(g_broker_list, BROKER_LIST, strlen(BROKER_LIST) + 1);
        }
        else
        {
            ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                          sizeof(BROKER_LIST));
            exit(EXIT_FAILURE);
        }
    }
    if (g_kafka_topic == NULL)
    {
        g_kafka_topic = (char *)malloc(sizeof(KAFKA_TOPIC));
        if (g_kafka_topic != NULL)
        {
            strncpy(g_kafka_topic, KAFKA_TOPIC, strlen(KAFKA_TOPIC) + 1);
        }
        else
        {
            ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                          sizeof(KAFKA_TOPIC));
            exit(EXIT_FAILURE);
        }
    }
    if (g_rpc_server == NULL)
    {
        g_rpc_server = (char *)malloc(sizeof(LOCALHOST));
        if (g_rpc_server != NULL)
        {
            strncpy(g_rpc_server, LOCALHOST, strlen(LOCALHOST) + 1);
        }
        else
        {
            ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                          sizeof(LOCALHOST));
            exit(EXIT_FAILURE);
        }
    }
    if (g_rpc_protocol == NULL)
    {
        g_rpc_protocol = (char *)malloc(sizeof(UDP));
        if (g_rpc_protocol != NULL)
        {
            strncpy(g_rpc_protocol, UDP, strlen(UDP) + 1);
        }
        else
        {
            ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                          sizeof(UDP));
            exit(EXIT_FAILURE);
        }
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

    if (g_test_mode == false)
    {
        string broker_list(g_broker_list);
        string topic_host_spec(g_kafka_topic);

        MessageConsumer network_config_consumer(broker_list, "my-test");

        network_config_consumer.cosumeDispatched(topic_host_spec);
        /* never reached */
    }
    else // g_test_mode == TRUE
    {
        // TODO:...
    }

    aca_cleanup();

    return rc;
}
