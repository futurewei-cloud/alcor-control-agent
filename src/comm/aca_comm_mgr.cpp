#include "aca_log.h"
#include "trn_rpc_protocol.h"
#include <iostream>
#include <chrono>
#include <thread>
#include "aca_comm_mgr.h"
#include "messageconsumer.h"
#include "aca_interface.pb.h"

using std::string;
using namespace std::chrono_literals;
using messagemanager::MessageConsumer;

extern bool g_test_mode;
extern char *g_rpc_server;
extern char *g_rpc_protocol;

namespace aca_comm_manager
{

int Aca_Comm_Manager::process_messages()
{

    void *parsed_struct;
    int rc = EXIT_FAILURE;

    //Preload network agent configuration
    //TODO: load it from configuration file
    // string host_id = "00000000-0000-0000-0000-000000000000";
    string broker_list = "10.213.43.188:9092";
    string topic_host_spec = "kafka_test2"; //"/hostid/" + host_id + "/hostspec/";
    // int partition_value = 0;

    //Listen to Kafka clusters for any network configuration operations
    //P0, tracked by issue#15
    MessageConsumer network_config_consumer(broker_list, "test");
    string **payload;
    Aca_Comm_Manager comm_manager;

    ACA_LOG_DEBUG("Going into keep listening loop, press ctrl-C or kill process ID #: "
                  "%d to exit.\n",
                  getpid());

    do
    {
        bool pool_res = network_config_consumer.consume(topic_host_spec, payload);
        if (pool_res)
        {
            ACA_LOG_INFO("Processing payload....: %s.\n", (**payload).c_str());

            rc = comm_manager.deserialize(**payload, parsed_struct);

            if (rc == EXIT_SUCCESS)
            {
                rc = comm_manager.execute_command(parsed_struct);

                if (rc == EXIT_SUCCESS)
                {
                    ACA_LOG_INFO("Successfully executed the network controller command");

                    // TODO: need to free parsed_struct since we are done with it
                }
                else
                {
                    ACA_LOG_ERROR("Unable to execute the network controller command: %d\n",
                                  rc);
                }
            }

            if ((payload != nullptr) && (*payload != nullptr))
            {
                delete *payload;
            }
        }
        else
        {
            ACA_LOG_ERROR("Consume message failed.\n");
        }

        std::this_thread::sleep_for(5s);

    } while (true);

    /* never reached */
    return rc;
}

int Aca_Comm_Manager::deserialize(string kafka_message, void *parsed_struct)
{
    //deserialize any new configuration
    //P0, tracked by issue#16

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // pb_load_transit_xdp_interface pb_load_transit_xdp_inf;

    // pb_load_transit_xdp_inf.ParseFromString(kafka_message);

    return EXIT_FAILURE;
}

int Aca_Comm_Manager::execute_command(void *parsed_struct)
{
    static CLIENT *client;
    uint controller_command = 0;
    int *rc;

    *rc = EXIT_FAILURE;

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

} // namespace aca_comm_manager
