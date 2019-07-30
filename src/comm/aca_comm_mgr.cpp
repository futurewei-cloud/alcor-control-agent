#include "aca_comm_mgr.h"
#include "aca_log.h"
#include "goalstate.pb.h"
#include "messageconsumer.h"
#include "trn_rpc_protocol.h"
#include <chrono>
#include <errno.h>
#include <iostream>
#include <thread>
#include <arpa/inet.h>

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

    aliothcontroller::GoalState deserialized_GoalState;
    int rc = EXIT_FAILURE;

    // Preload network agent configuration
    // TODO: load it from configuration file
    // string host_id = "00000000-0000-0000-0000-000000000000";
    string broker_list = "10.213.43.188:9092";
    // string topic_host_spec = "hostid-696239f7-bff2-4b34-9923-ef904eacd77a"; //"/hostid/" + host_id + "/hostspec/";
    string topic_host_spec = "my_topic"; //"/hostid/" + host_id + "/hostspec/";
    // int partition_value = 1;

    // Listen to Kafka clusters for any network configuration operations
    // P0, tracked by issue#15
    MessageConsumer network_config_consumer(broker_list, "test");
    string **payload;
    ACA_LOG_DEBUG(
        "Going into keep listening loop, press ctrl-C or kill process ID #: "
        "%d to exit.\n",
        getpid());

    do
    {
        bool poll_res = network_config_consumer.consume(topic_host_spec, payload);
        if (poll_res)
        {
            ACA_LOG_INFO("Processing payload....: %s.\n", (**payload).c_str());

            rc = this->deserialize(**payload, deserialized_GoalState);

            if (rc == EXIT_SUCCESS)
            {
                // Call parse_goal_state
                rc = update_goal_state(deserialized_GoalState);
                if (rc != EXIT_SUCCESS)
                {
                    ACA_LOG_ERROR("Failed to update transitd with goal state %d.\n", rc);
                }
                else
                {
                    ACA_LOG_ERROR("Successfully updated transitd with goal state %d.\n",
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

int Aca_Comm_Manager::deserialize(string kafka_message,
                                  aliothcontroller::GoalState &parsed_struct)
{
    int rc = EXIT_FAILURE;

    // deserialize any new configuration
    // P0, tracked by issue#16

    if (kafka_message.empty())
    {
        ACA_LOG_ERROR("Empty kafka message rc: %d\n", rc);
        return EINVAL;
    }

    if (parsed_struct.IsInitialized() == false)
    {
        ACA_LOG_ERROR("Uninitialized parsed_struct rc: %d\n", rc);
        return EINVAL;
    }

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    if (parsed_struct.ParseFromArray(kafka_message.c_str(),
                                     kafka_message.size()))
    {
        ACA_LOG_INFO("Successfully converted kafka message to protobuf struct\n");

        this->print_goal_state(parsed_struct);

        return EXIT_SUCCESS;
    }
    else
    {
        ACA_LOG_ERROR("Failed to convert kafka message to protobuf struct\n");
        return EXIT_FAILURE;
    }
}

// Calls execute
int Aca_Comm_Manager::update_goal_state(
    aliothcontroller::GoalState &parsed_struct)
{
    int transitd_command = 0;
    void *transitd_input;
    int rc = EXIT_FAILURE;

    for (int i = 0; i < parsed_struct.vpc_states_size(); i++)
    {
        aliothcontroller::VpcConfiguration current_VpcConfiguration =
            parsed_struct.vpc_states(i).configuration();

        switch (parsed_struct.vpc_states(i).operation_type())
        {
        case aliothcontroller::OperationType::CREATE:
        case aliothcontroller::OperationType::UPDATE:
            // TODO: There might be slight difference between Create and Update.
            // E.g. Create could require pre-check that if a VPC exists in this host etc.
            transitd_command = UPDATE_VPC;
            transitd_input = (rpc_trn_vpc_t *)malloc(sizeof(rpc_trn_vpc_t));
            if (transitd_input != NULL)
            {
                rpc_trn_vpc_t *vpc_input = (rpc_trn_vpc_t *)transitd_input;
                vpc_input->interface = (char *)"eth0";
                vpc_input->tunid = current_VpcConfiguration.tunnel_id();                
                vpc_input->routers_ips.routers_ips_len =
                    current_VpcConfiguration.transit_router_ips_size();
                uint32_t routers[RPC_TRN_MAX_VPC_ROUTERS];
                vpc_input->routers_ips.routers_ips_val = routers;

                for (int j = 0; j < current_VpcConfiguration.transit_router_ips_size(); j++)
                {
                    struct sockaddr_in sa;
                    // TODO: need to check return value, it returns 1 for success 0 for failure
                    inet_pton(AF_INET, current_VpcConfiguration.transit_router_ips(j).ip_address().c_str(),
                              &(sa.sin_addr));
                    vpc_input->routers_ips.routers_ips_val[j] =
                        sa.sin_addr.s_addr;
                }
                rc = EXIT_SUCCESS;
            }
            else
            {
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
                          parsed_struct.vpc_states(i).operation_type());
            break;
        }
        if (rc == EXIT_SUCCESS)
        {
            rc = this->execute_command(transitd_command, transitd_input);
            if (rc == EXIT_SUCCESS)
            {
                ACA_LOG_INFO("Successfully executed the network controller command");
            }
            else
            {
                ACA_LOG_ERROR("[update_goal_state] Unable to execute the network controller command: %d\n",
                              rc);
                // TODO: Notify the Network Controller if the command is not successful.
            }
            if (transitd_input)
            {
                free(transitd_input);
                transitd_input = NULL;
            }
        }
    }

    for (int i = 0; i < parsed_struct.subnet_states_size(); i++)
    {

        aliothcontroller::SubnetConfiguration current_SubnetConfiguration =
            parsed_struct.subnet_states(i).configuration();

        switch (parsed_struct.subnet_states(i).operation_type())
        {
        case aliothcontroller::OperationType::CREATE:
        case aliothcontroller::OperationType::UPDATE:
            // TODO: There might be slight difference between Create and Update.
            // E.g. Create could require pre-check that if a subnet exists in this host etc.        
            transitd_command = UPDATE_NET;
            transitd_input = (rpc_trn_network_t *)malloc(sizeof(rpc_trn_network_t));
            if (transitd_input != NULL)
            {
                rpc_trn_network_t *network_input = (rpc_trn_network_t *)transitd_input;
                network_input->interface = (char *)"eth0";
                network_input->tunid = current_SubnetConfiguration.tunnel_id();

                string my_cidr = current_SubnetConfiguration.cidr();
                int slash_pos = my_cidr.find("/");
                // TODO: substr throw exceptions also
                string my_ip_address = my_cidr.substr(0, slash_pos);

                struct sockaddr_in sa;
                // TODO: need to check return value, it returns 1 for success 0 for failure
                inet_pton(AF_INET, my_ip_address.c_str(),
                          &(sa.sin_addr));
                network_input->netip = sa.sin_addr.s_addr;

                string my_prefixlen = my_cidr.substr(slash_pos + 1);
                // TODO: stoi throw invalid argument exception when it cannot covert
                network_input->prefixlen = std::stoi(my_prefixlen);

                network_input->switches_ips.switches_ips_len =
                    current_SubnetConfiguration.transit_switch_ips_size();
                uint32_t switches[RPC_TRN_MAX_NET_SWITCHES];
                network_input->switches_ips.switches_ips_val = switches;

                for (int j = 0; j < current_SubnetConfiguration.transit_switch_ips_size(); j++)
                {
                    struct sockaddr_in sa;
                    // TODO: need to check return value, it returns 1 for success 0 for failure
                    inet_pton(AF_INET, current_SubnetConfiguration.transit_switch_ips(j).ip_address().c_str(),
                              &(sa.sin_addr));
                    network_input->switches_ips.switches_ips_val[j] =
                        sa.sin_addr.s_addr;
                }
                rc = EXIT_SUCCESS;
            }
            else
            {
                ACA_LOG_EMERG("Out of memory when allocating with size: %lu.\n",
                              sizeof(rpc_trn_network_t));
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
            ACA_LOG_DEBUG("Invalid subnet state operation type %d/n",
                          parsed_struct.subnet_states(i).operation_type());
            break;
        }
        if (rc == EXIT_SUCCESS)
        {
            rc = this->execute_command(transitd_command, transitd_input);
            if (rc == EXIT_SUCCESS)
            {
                ACA_LOG_INFO("Successfully executed the network controller command");
            }
            else
            {
                ACA_LOG_ERROR("Unable to execute the network controller command: %d\n",
                              rc);
                // TODO: Notify the Network Controller if the command is not successful.
            }
            if (transitd_input)
            {
                free(transitd_input);
                transitd_input = NULL;
            }
        }
    }

    return rc;
}

// TODO: fix the memory leaks introduced when the RPC call failed
int Aca_Comm_Manager::execute_command(int command, void *input_struct)
{
    static CLIENT *client;
    int rc = EXIT_SUCCESS;
    int *transitd_return;

    ACA_LOG_INFO("Connecting to %s using %s protocol.\n", g_rpc_server,
                 g_rpc_protocol);

    // TODO: We may change it to have a static client for health checking on
    // transit daemon in the future.
    client = clnt_create(g_rpc_server, RPC_TRANSIT_REMOTE_PROTOCOL,
                         RPC_TRANSIT_ALFAZERO, g_rpc_protocol);

    if (client == NULL)
    {
        clnt_pcreateerror(g_rpc_server);
        ACA_LOG_EMERG("Not able to create the RPC connection to Transit daemon.\n");
        rc = EXIT_FAILURE;
    }
    else
    {
        switch (command)
        {
        case UPDATE_VPC:
            transitd_return = update_vpc_1((rpc_trn_vpc_t *)input_struct, client);
            break;
        case UPDATE_NET:
            transitd_return = update_net_1((rpc_trn_network_t *)input_struct, client);
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

        if (transitd_return == (int *)NULL)
        {
            clnt_perror(client, "Call failed to program Transit daemon");
            ACA_LOG_EMERG("Call failed to program Transit daemon, command: %d.\n",
                          command);
            rc = EXIT_FAILURE;
        }
        else if (*transitd_return != EXIT_SUCCESS)
        {
            // TODO: report the error back to network controller
            rc = EXIT_FAILURE;
        }
        if (rc == EXIT_SUCCESS)
        {
            ACA_LOG_INFO("Successfully updated transitd with command %d.\n",
                         command);
        }
        // TODO: can print out more command specific info

        clnt_destroy(client);
    }

    return rc;
}

// TODO: only print it during debug mode
void Aca_Comm_Manager::print_goal_state(aliothcontroller::GoalState parsed_struct)
{
    for (int i = 0; i < parsed_struct.vpc_states_size(); i++)
    {
        fprintf(stdout,
                "parsed_struct.vpc_states(%d).operation_type(): %d\n", i,
                parsed_struct.vpc_states(i).operation_type());

        aliothcontroller::VpcConfiguration current_VpcConfiguration =
            parsed_struct.vpc_states(i).configuration();

        fprintf(stdout,
                "current_VpcConfiguration.version(): %d\n",
                current_VpcConfiguration.version());

        fprintf(stdout,
                "current_VpcConfiguration.project_id(): %s\n",
                current_VpcConfiguration.project_id().c_str());

        fprintf(stdout,
                "current_VpcConfiguration.id(): %s\n",
                current_VpcConfiguration.id().c_str());

        fprintf(stdout,
                "current_VpcConfiguration.name(): %s \n",
                current_VpcConfiguration.name().c_str());

        fprintf(stdout,
                "current_VpcConfiguration.cidr(): %s \n",
                current_VpcConfiguration.cidr().c_str());

        fprintf(stdout,
                "current_VpcConfiguration.tunnel_id(): %ld \n",
                current_VpcConfiguration.tunnel_id());

        for (int j = 0; j < current_VpcConfiguration.subnet_ids_size(); j++)
        {
            fprintf(stdout,
                    "current_VpcConfiguration.subnet_ids(%d): %s \n", j,
                    current_VpcConfiguration.subnet_ids(j).id().c_str());
        }

        for (int k = 0; k < current_VpcConfiguration.routes_size(); k++)
        {
            fprintf(stdout,
                    "current_VpcConfiguration.routes(%d).destination(): "
                    "%s \n",
                    k,
                    current_VpcConfiguration.routes(k).destination().c_str());

            fprintf(stdout,
                    "current_VpcConfiguration.routes(%d).next_hop(): "
                    "%s \n",
                    k,
                    current_VpcConfiguration.routes(k).next_hop().c_str());
        }

        for (int l = 0; l < current_VpcConfiguration.transit_router_ips_size(); l++)
        {
            fprintf(stdout,
                    "current_VpcConfiguration.transit_router_ips(%d).vpc_id(): "
                    "%s \n",
                    l,
                    current_VpcConfiguration.transit_router_ips(l).vpc_id().c_str());

            fprintf(stdout,
                    "current_VpcConfiguration.transit_router_ips(%d).ip_address(): "
                    "%s \n",
                    l,
                    current_VpcConfiguration.transit_router_ips(l).ip_address().c_str());
        }
    }

    for (int i = 0; i < parsed_struct.subnet_states_size(); i++)
    {
        fprintf(stdout,
                "parsed_struct.subnet_states(%d).operation_type(): %d\n", i,
                parsed_struct.subnet_states(i).operation_type());

        aliothcontroller::SubnetConfiguration current_SubnetConfiguration =
            parsed_struct.subnet_states(i).configuration();

        fprintf(stdout,
                "current_SubnetConfiguration.version(): %d\n",
                current_SubnetConfiguration.version());

        fprintf(stdout,
                "current_SubnetConfiguration.project_id(): %s\n",
                current_SubnetConfiguration.project_id().c_str());

        fprintf(stdout,
                "current_SubnetConfiguration.vpc_id(): %s\n",
                current_SubnetConfiguration.vpc_id().c_str());

        fprintf(stdout,
                "current_SubnetConfiguration.id(): %s\n",
                current_SubnetConfiguration.id().c_str());

        fprintf(stdout,
                "current_SubnetConfiguration.name(): %s \n",
                current_SubnetConfiguration.name().c_str());

        fprintf(stdout,
                "current_SubnetConfiguration.cidr(): %s \n",
                current_SubnetConfiguration.cidr().c_str());

        fprintf(stdout,
                "current_SubnetConfiguration.tunnel_id(): %ld \n",
                current_SubnetConfiguration.tunnel_id());

        for (int j = 0; j < current_SubnetConfiguration.transit_switch_ips_size(); j++)
        {
            fprintf(stdout,
                    "current_SubnetConfiguration.transit_switch_ips(%d): %s \n", j,
                    current_SubnetConfiguration.transit_switch_ips(j).ip_address().c_str());
        }
    }
}

} // namespace aca_comm_manager