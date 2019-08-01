#include "aca_comm_mgr.h"
#include "aca_util.h"
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

Aca_Comm_Manager &Aca_Comm_Manager::get_instance()
{
    // instance is destroyed when program exits.
    // It is Instantiated on first use.
    static Aca_Comm_Manager instance;
    return instance;
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

// Calls execute_command
int Aca_Comm_Manager::update_goal_state(
    aliothcontroller::GoalState &parsed_struct)
{
    int transitd_command = 0;
    void *transitd_input;
    int rc = EXIT_FAILURE;

    for (int i = 0; i < parsed_struct.port_states_size(); i++)
    {
        aliothcontroller::PortConfiguration current_PortConfiguration =
            parsed_struct.port_states(i).configuration();

        switch (parsed_struct.port_states(i).operation_type())
        {
        case aliothcontroller::OperationType::CREATE:
            transitd_command = UPDATE_EP;
            transitd_input = (rpc_trn_endpoint_t *)malloc(sizeof(rpc_trn_endpoint_t));
            if (transitd_input != NULL)
            {
                rpc_trn_endpoint_t *endpoint_input = (rpc_trn_endpoint_t *)transitd_input;
                endpoint_input->interface = (char *)"eth0";

                assert(current_PortConfiguration.fixed_ips_size() == 1);
                string my_ep_ip_address = current_PortConfiguration.fixed_ips(0).ip_address();
                struct sockaddr_in sa;
                // TODO: need to check return value, it returns 1 for success 0 for failure
                inet_pton(AF_INET, my_ep_ip_address.c_str(), &(sa.sin_addr));
                endpoint_input->ip = sa.sin_addr.s_addr;

                endpoint_input->eptype = 0;

                endpoint_input->remote_ips.remote_ips_len = 1;
                // TODO: needs to confirm it is the same ip as the local host
                inet_pton(AF_INET, current_PortConfiguration.host_ip().c_str(),
                          &(sa.sin_addr));
                endpoint_input->remote_ips.remote_ips_val[0] = sa.sin_addr.s_addr;

                if (sscanf(current_PortConfiguration.mac_address().c_str(),
                           "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                           &endpoint_input->mac[0],
                           &endpoint_input->mac[1],
                           &endpoint_input->mac[2],
                           &endpoint_input->mac[3],
                           &endpoint_input->mac[4],
                           &endpoint_input->mac[5]) != 6)
                {
                    ACA_LOG_ERROR("Invalid mac input: %s.\n", current_PortConfiguration.mac_address().c_str());
                }

                int port_name_size = sizeof((current_PortConfiguration).name().c_str());
                endpoint_input->veth = (char *)malloc(port_name_size);
                strncpy(endpoint_input->veth, current_PortConfiguration.name().c_str(),
                        strlen(current_PortConfiguration.name().c_str() + 1));

                string peer_name = current_PortConfiguration.name() + "_peer";
                int peer_name_size = sizeof(peer_name.c_str());
                endpoint_input->hosted_interface = (char *)malloc(peer_name_size);
                strncpy(endpoint_input->hosted_interface, peer_name.c_str(),
                        strlen(peer_name.c_str()) + 1);

                bool tunnel_id_found = false;

                // Look up the subnet configuration to query for tunnel_id
                for (int j = 0; j < parsed_struct.subnet_states_size(); j++)
                {

                    aliothcontroller::SubnetConfiguration current_SubnetConfiguration =
                        parsed_struct.subnet_states(j).configuration();

                    if (parsed_struct.subnet_states(j).operation_type() ==
                        aliothcontroller::OperationType::INFO)
                    {
                        if (current_SubnetConfiguration.id() ==
                            current_PortConfiguration.network_id())
                        {
                            endpoint_input->tunid = current_SubnetConfiguration.tunnel_id();
                            tunnel_id_found = true;
                            break;
                        }
                    }
                }

                if (!tunnel_id_found)
                {
                    ACA_LOG_ERROR("Not able to find the tunnel ID information from subnet config.\n");
                }

                rc = EXIT_SUCCESS;
            }
            else
            {
                ACA_LOG_EMERG("Out of memory when allocating with size: %lu.\n",
                              sizeof(rpc_trn_endpoint_t));
                rc = ENOMEM;
            }
            break;
        default:
            ACA_LOG_DEBUG("Invalid port state operation type %d/n",
                          parsed_struct.port_states(i).operation_type());
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
        }

        aca_free(((rpc_trn_endpoint_t *)transitd_input)->veth);
        aca_free(((rpc_trn_endpoint_t *)transitd_input)->hosted_interface);
        aca_free(transitd_input);
    } // for (int i = 0; i < parsed_struct.port_states_size(); i++)

    for (int i = 0; i < parsed_struct.subnet_states_size(); i++)
    {

        aliothcontroller::SubnetConfiguration current_SubnetConfiguration =
            parsed_struct.subnet_states(i).configuration();

        switch (parsed_struct.subnet_states(i).operation_type())
        {
        case aliothcontroller::OperationType::INFO:
            // information only, ignoring this.
            break;
        case aliothcontroller::OperationType::CREATE_UPDATE_ROUTER:
            // this is to update the router host, only need to update substrate later.
            break;
        case aliothcontroller::OperationType::CREATE:
        case aliothcontroller::OperationType::UPDATE:
            // TODO: There might be slight difference between Create and Update.
            // E.g. Create could require pre-check that if a subnet exists in this host etc.
            // TODO: not sure if below is needed based on the latest contract with controller.
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
                inet_pton(AF_INET, my_ip_address.c_str(), &(sa.sin_addr));
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
        }
        aca_free(transitd_input);

        // do we need to call update substrate?
        if (parsed_struct.subnet_states(i).operation_type() ==
            aliothcontroller::OperationType::CREATE_UPDATE_ROUTER)
        {
            transitd_command = UPDATE_EP;

            for (int j = 0; j < current_SubnetConfiguration.transit_switch_ips_size(); j++)
            {
                rpc_trn_endpoint_t *substrate_input = (rpc_trn_endpoint_t *)malloc(sizeof(rpc_trn_endpoint_t));

                if (substrate_input != NULL)
                {
                    substrate_input->interface = (char *)"";

                    struct sockaddr_in sa;
                    // TODO: need to check return value, it returns 1 for success 0 for failure
                    inet_pton(AF_INET, current_SubnetConfiguration.transit_switch_ips(j).ip_address().c_str(),
                              &(sa.sin_addr));
                    substrate_input->ip = sa.sin_addr.s_addr;
                    substrate_input->eptype = 0;
                    uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
                    substrate_input->remote_ips.remote_ips_val = remote_ips;
                    substrate_input->remote_ips.remote_ips_len = 0;
                    if (sscanf("hh:ii:jj:kk:ll:mm",
                               "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                               &substrate_input->mac[0],
                               &substrate_input->mac[1],
                               &substrate_input->mac[2],
                               &substrate_input->mac[3],
                               &substrate_input->mac[4],
                               &substrate_input->mac[5]) != 6)
                    {
                        ACA_LOG_ERROR("Invalid mac input: TBD.\n");
                    }
                    substrate_input->hosted_interface = (char *)"";
                    substrate_input->veth = (char *)"";
                    substrate_input->tunid = (uint64_t) "";

                    rc = EXIT_SUCCESS;
                }
                else
                {
                    ACA_LOG_EMERG("Out of memory when allocating with size: %lu.\n",
                                  sizeof(rpc_trn_endpoint_t));
                    rc = ENOMEM;
                }

                if (rc == EXIT_SUCCESS)
                {
                    rc = this->execute_command(transitd_command, substrate_input);
                    if (rc == EXIT_SUCCESS)
                    {
                        ACA_LOG_INFO("Successfully updated substrate in transit daemon");
                    }
                    else
                    {
                        ACA_LOG_ERROR("Unable to update substrate in transit daemon: %d\n",
                                      rc);
                        // TODO: Notify the Network Controller if the command is not successful.
                    }
                }

                aca_free(substrate_input);
            } // for (int j = 0; j < current_VpcConfiguration.transit_router_ips_size(); j++)
        }

    } // for (int i = 0; i < parsed_struct.subnet_states_size(); i++)

    for (int i = 0; i < parsed_struct.vpc_states_size(); i++)
    {
        aliothcontroller::VpcConfiguration current_VpcConfiguration =
            parsed_struct.vpc_states(i).configuration();

        switch (parsed_struct.vpc_states(i).operation_type())
        {
        case aliothcontroller::OperationType::CREATE_UPDATE_SWITCH:
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
                ACA_LOG_INFO("Successfully executed the network controller command\n");
            }
            else
            {
                ACA_LOG_ERROR("[update_goal_state] Unable to execute the network controller command: %d\n",
                              rc);
                // TODO: Notify the Network Controller if the command is not successful.
            }
        }
        aca_free(transitd_input);

        // do we need to call update substrate?
        if (parsed_struct.vpc_states(i).operation_type() ==
            aliothcontroller::OperationType::CREATE_UPDATE_SWITCH)
        {
            transitd_command = UPDATE_EP;

            for (int j = 0; j < current_VpcConfiguration.transit_router_ips_size(); j++)
            {
                rpc_trn_endpoint_t *substrate_input = (rpc_trn_endpoint_t *)malloc(sizeof(rpc_trn_endpoint_t));

                if (substrate_input != NULL)
                {
                    substrate_input->interface = (char *)"";

                    struct sockaddr_in sa;
                    // TODO: need to check return value, it returns 1 for success 0 for failure
                    inet_pton(AF_INET, current_VpcConfiguration.transit_router_ips(j).ip_address().c_str(),
                              &(sa.sin_addr));
                    substrate_input->ip = sa.sin_addr.s_addr;
                    substrate_input->eptype = 0;
                    uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
                    substrate_input->remote_ips.remote_ips_val = remote_ips;
                    substrate_input->remote_ips.remote_ips_len = 0;
                    if (sscanf("aa:bb:cc:dd:ee:ff",
                               "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                               &substrate_input->mac[0],
                               &substrate_input->mac[1],
                               &substrate_input->mac[2],
                               &substrate_input->mac[3],
                               &substrate_input->mac[4],
                               &substrate_input->mac[5]) != 6)
                    {
                        ACA_LOG_ERROR("Invalid mac input: TBD.\n");
                    }
                    substrate_input->hosted_interface = (char *)"";
                    substrate_input->veth = (char *)"";
                    substrate_input->tunid = (uint64_t) "";

                    rc = EXIT_SUCCESS;
                }
                else
                {
                    ACA_LOG_EMERG("Out of memory when allocating with size: %lu.\n",
                                  sizeof(rpc_trn_endpoint_t));
                    rc = ENOMEM;
                }

                if (rc == EXIT_SUCCESS)
                {
                    rc = this->execute_command(transitd_command, substrate_input);
                    if (rc == EXIT_SUCCESS)
                    {
                        ACA_LOG_INFO("Successfully updated substrate in transit daemon\n");
                    }
                    else
                    {
                        ACA_LOG_ERROR("Unable to update substrate in transit daemon: %d\n",
                                      rc);
                        // TODO: Notify the Network Controller if the command is not successful.
                    }
                }
                aca_free(substrate_input);
            } // for (int j = 0; j < current_VpcConfiguration.transit_router_ips_size(); j++)
        }
    } // for (int i = 0; i < parsed_struct.vpc_states_size(); i++)

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
            transitd_return = update_ep_1((rpc_trn_endpoint_t *)input_struct, client);
            break;
        case UPDATE_AGENT_EP:
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
    for (int i = 0; i < parsed_struct.port_states_size(); i++)
    {
        fprintf(stdout,
                "parsed_struct.subnet_states(%d).operation_type(): %d\n", i,
                parsed_struct.port_states(i).operation_type());

        aliothcontroller::PortConfiguration current_PortConfiguration =
            parsed_struct.port_states(i).configuration();

        fprintf(stdout,
                "current_PortConfiguration.version(): %d\n",
                current_PortConfiguration.version());

        fprintf(stdout,
                "current_PortConfiguration.project_id(): %s\n",
                current_PortConfiguration.project_id().c_str());

        fprintf(stdout,
                "current_PortConfiguration.network_id(): %s\n",
                current_PortConfiguration.network_id().c_str());

        fprintf(stdout,
                "current_PortConfiguration.id(): %s\n",
                current_PortConfiguration.id().c_str());

        fprintf(stdout,
                "current_PortConfiguration.name(): %s \n",
                current_PortConfiguration.name().c_str());

        fprintf(stdout,
                "current_PortConfiguration.admin_state_up(): %d \n",
                current_PortConfiguration.admin_state_up());

        fprintf(stdout,
                "current_PortConfiguration.mac_address(): %s \n",
                current_PortConfiguration.mac_address().c_str());

        fprintf(stdout,
                "current_PortConfiguration.veth_name(): %s \n",
                current_PortConfiguration.veth_name().c_str());

        fprintf(stdout,
                "current_PortConfiguration.host_ip(): %s \n",
                current_PortConfiguration.host_ip().c_str());

        for (int j = 0; j < current_PortConfiguration.fixed_ips_size(); j++)
        {
            fprintf(stdout,
                    "current_PortConfiguration.fixed_ips(%d): subnet_id %s, ip_address %s \n", j,
                    current_PortConfiguration.fixed_ips(j).subnet_id().c_str(),
                    current_PortConfiguration.fixed_ips(j).ip_address().c_str());
        }

        for (int j = 0; j < current_PortConfiguration.security_group_ids_size(); j++)
        {
            fprintf(stdout,
                    "current_PortConfiguration.security_group_ids(%d): id %s, \n", j,
                    current_PortConfiguration.security_group_ids(j).id().c_str());
        }

        for (int j = 0; j < current_PortConfiguration.allow_address_pairs_size(); j++)
        {
            fprintf(stdout,
                    "current_PortConfiguration.allow_address_pairs(%d): ip_address %s, mac_address %s \n", j,
                    current_PortConfiguration.allow_address_pairs(j).ip_address().c_str(),
                    current_PortConfiguration.allow_address_pairs(j).mac_address().c_str());
        }

        for (int j = 0; j < current_PortConfiguration.extra_dhcp_options_size(); j++)
        {
            fprintf(stdout,
                    "current_PortConfiguration.extra_dhcp_options(%d): name %s, value %s \n", j,
                    current_PortConfiguration.extra_dhcp_options(j).name().c_str(),
                    current_PortConfiguration.extra_dhcp_options(j).value().c_str());
        }
        printf("\n");
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
        printf("\n");
    }

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
        printf("\n");
    }
}

} // namespace aca_comm_manager