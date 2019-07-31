#include "aca_log.h"
#include "goalstate.pb.h"
#include "messageconsumer.h"
#include <unistd.h> /* for getopt */

using std::string;
using messagemanager::MessageConsumer;

#define ACALOGNAME "AliothControlAgent"

using namespace std;

// Defines
static char LOCALHOST[] = "localhost";
static char UDP[] = "udp";

// Global variables
bool g_test_mode = false;
char *g_test_message = NULL;
char *g_rpc_server = NULL;
char *g_rpc_protocol = NULL;

static void aca_cleanup()
{
    // Optional:  Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    if (g_test_message != NULL)
    {
        free(g_test_message);
        g_test_message = NULL;
    }

    if (g_rpc_server != NULL)
    {
        free(g_rpc_server);
        g_rpc_server = NULL;
    }

    if (g_rpc_protocol != NULL)
    {
        free(g_rpc_protocol);
        g_rpc_protocol = NULL;
    }

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

    while ((option = getopt(argc, argv, "t:s:p:")) != -1)
    {
        switch (option)
        {
        case 't':
            g_test_mode = true;

            g_test_message = (char *)malloc(sizeof(char) * strlen(optarg));
            if (g_test_message != NULL)
            {
                strncpy(g_test_message, optarg, strlen(optarg));
            }
            else
            {
                ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                              (sizeof(char) * strlen(optarg)));
                exit(EXIT_FAILURE);
            }
            break;
        case 's':
            g_rpc_server = (char *)malloc(sizeof(char) * strlen(optarg));
            if (g_rpc_server != NULL)
            {
                strncpy(g_rpc_server, optarg, strlen(optarg));
            }
            else
            {
                ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                              (sizeof(char) * strlen(optarg)));
                exit(EXIT_FAILURE);
            }
            break;
        case 'p':
            g_rpc_protocol = (char *)malloc(sizeof(char) * strlen(optarg));
            if (g_rpc_protocol != NULL)
            {
                strncpy(g_rpc_protocol, optarg, strlen(optarg));
            }
            else
            {
                ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                              (sizeof(char) * strlen(optarg)));
                exit(EXIT_FAILURE);
            }
            break;
        default: /* the '?' case when the option is not recognized */
            fprintf(stderr,
                    "Usage: %s\n"
                    "\t\t[-t test message to parse and enable test mode]\n"
                    "\t\t[-s transitd RPC server]\n"
                    "\t\t[-p transitd RPC protocol]\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // fill in the RPC server and protocol if not provided in command line args
    if (g_rpc_server == NULL)
    {
        g_rpc_server = (char *)malloc(sizeof(char) * strlen(LOCALHOST) + 1);
        if (g_rpc_server != NULL)
        {
            strncpy(g_rpc_server, LOCALHOST, strlen(LOCALHOST) + 1);
        }
        else
        {
            ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                          (sizeof(char) * strlen(LOCALHOST)));
            exit(EXIT_FAILURE);
        }
    }
    if (g_rpc_protocol == NULL)
    {
        g_rpc_protocol = (char *)malloc(sizeof(char) * strlen(UDP) + 1);
        if (g_rpc_protocol != NULL)
        {
            strncpy(g_rpc_protocol, UDP, strlen(UDP) + 1);
        }
        else
        {
            ACA_LOG_EMERG("Out of memory when allocating string with size: %lu.\n",
                          (sizeof(char) * strlen(UDP)));
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
        string broker_list = "10.213.43.188:9092";
        string topic_host_spec = "my_topic";

        MessageConsumer network_config_consumer(broker_list, "test");

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
