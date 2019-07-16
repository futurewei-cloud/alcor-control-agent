// c includes
#include "aca_log.h"
#include "trn_rpc_protocol.h"

// cpp includes
#include <chrono>
#include <thread>
#include "messageproducer.h"
#include "messageconsumer.h"

#define ACALOGNAME "AliothControlAgent"

using namespace std;
using namespace std::chrono_literals;
using messagemanager::MessageConsumer;
using messagemanager::MessageProducer;

// Defines
static char LOCALHOST[] = "localhost";
static char UDP[] = "udp";

// Global variables
static bool g_test_mode = false;
static char *g_test_message;
static char *g_rpc_server = LOCALHOST;
static char *g_rpc_protocol = UDP;

// function to handle ctrl-c and kill process
static void aca_signal_handler(int sig_num)
{
    ACA_LOG_ERROR("Caught signal: %d\n", sig_num);

    // perform all the necessary cleanup here
    ACA_LOG_CLOSE();

    exit(sig_num);
}

// function to parse and process the command from network controller
static int aca_parse_and_program(string raw_string)
{
    static CLIENT *client;
    uint controller_command = 0;
    int *rc;

    *rc = EXIT_FAILURE;

    //deserialize any new configuration
    //P0, tracked by issue#16

    //Depending on different operations, program XDP through corresponding RPC
    //apis by transit daemon
    //P0, tracked by issue#17
    ACA_LOG_INFO("Connecting to %s using %s protocol.\n", g_rpc_server, g_rpc_protocol);

    client = clnt_create(g_rpc_server, RPC_TRANSIT_REMOTE_PROTOCOL,
                         RPC_TRANSIT_ALFAZERO, g_rpc_protocol);

    if (client == NULL)
    {
        clnt_pcreateerror(g_rpc_server);
        ACA_LOG_EMERG("Not able to create the RPC connection to Transit daemon.\n");
        *rc = EXIT_FAILURE;
    }
    else
    {
        switch (controller_command)
        {
        case UPDATE_VPC:
            // rc = update_vpc_1 ...
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
            ACA_LOG_ERROR("Unknown controller command: %d\n", controller_command);
            *rc = EXIT_FAILURE;
            break;
        }

        if (rc == (int *)NULL)
        {
            clnt_perror(client, "Call failed to program Transit daemon");
            ACA_LOG_EMERG("Call failed to program Transit daemon, command: %d.\n",
                          controller_command);
        }
        else if (*rc != EXIT_SUCCESS)
        {
            ACA_LOG_EMERG("Fatal error for command: %d.\n",
                          controller_command);
            // TODO: report the error back to network controller
        }

        ACA_LOG_INFO("Successfully updated transitd with command %d.\n",
                     controller_command);
        // TODO: can print out more command specific info

        clnt_destroy(client);
    }

    return *rc;
}

static int aca_comm_mgr_listen()
{
    //Preload network agent configuration
    //TODO: load it from configuration file
    string host_id = "00000000-0000-0000-0000-000000000000";
    string broker_list = "10.213.43.188:9092";
    string topic_host_spec = "kafka_test2"; //"/hostid/" + host_id + "/hostspec/";
    int partition_value = 0;
    bool pool_res = false;
    int rc = EXIT_FAILURE;

    //Listen to Kafka clusters for any network configuration operations
    //P0, tracked by issue#15
    MessageConsumer network_config_consumer(broker_list, "test");
    string **payload;

    if (g_test_mode == FALSE)
    {
        ACA_LOG_ERROR("Going into keep listening loop, press ctrl-C or kill process ID #: "
                        "%d to exit.\n", getpid());

        do
        {
            pool_res = network_config_consumer.consume(topic_host_spec, payload);
            if (pool_res)
            {
                ACA_LOG_INFO("Processing payload....: %s.\n", (**payload).c_str());

                // TODO: need to break down the parse and program into two functions
                rc = aca_parse_and_program(**payload);
            }
            else
            {
                ACA_LOG_ERROR("pool fails.\n");
            }

            if (pool_res && (payload != nullptr) && (*payload != nullptr))
            {
                delete *payload;
            }

            std::this_thread::sleep_for(5s);

        } while (true);
    }
    else // g_test_mode == TRUE
    {
        
    }

    return rc;
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
            fprintf(stderr, "Usage: %s\n"
                                    "\t\t[-t test message to parse and enable test mode]\n"
                                    "\t\t[-s transitd RPC server]\n"
                                    "\t\t[-p transitd RPC protocol]\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    //Check if OVS and/or OVS daemon exists; if not, launch the program
    //tracked by issue#10, may not needed based on discussion with XiaoNing

    //Check if transit program exists on Physical NIC; if not, launch the program
    //P0, tracked by issue#11

    //Announce this host (agent) and register in every kafka cluster
    //P0, tracked by issue#12

    //Launch background threads to monitor and to emit network health status
    //	for customer VMs, containers, as well as
    //	infra host services including OVS, transit etc.
    //P1, tracked by issue#13

    //Upload or refresh the networking spec of this host
    // (including DPDK, SR-IOV, bandwidth etc.)
    //P1, tracked by issue#14
    // MessageProducer host_spec_producer(broker_list, topic_host_spec, partition_value);
    // cout << "broker list:" << host_spec_producer.getBrokers() << endl;
    // cout << "topic:" << host_spec_producer.getTopicName() << endl;
    // cout << "partition:" << host_spec_producer.getPartitionValue() << endl;

    // string host_network_spec = "fake config";
    //cout << "Prepare for publishing " << host_network_spec  <<endl;
    //host_spec_producer.publish(host_network_spec);
    //cout << "Publish completed" << endl;

    rc = aca_comm_mgr_listen();

    ACA_LOG_CLOSE();

    return rc;
}
