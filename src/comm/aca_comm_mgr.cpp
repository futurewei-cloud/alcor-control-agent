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
#include "cppkafka/utils/consumer_dispatcher.h"

using std::string;
using namespace std::chrono_literals;
using messagemanager::MessageConsumer;

extern string g_rpc_server;
extern string g_rpc_protocol;

namespace aca_comm_manager
{

Aca_Comm_Manager &Aca_Comm_Manager::get_instance()
{
    // instance is destroyed when program exits.
    // It is Instantiated on first use.
    static Aca_Comm_Manager instance;
    return instance;
}

int Aca_Comm_Manager::deserialize(const cppkafka::Buffer *kafka_buffer,
                                  aliothcontroller::GoalState &parsed_struct)
{
    int rc = -EXIT_FAILURE;

    if (kafka_buffer->get_data() == NULL)
    {
        return -EINVAL;
        ACA_LOG_ERROR("Empty kafka kafka_buffer data rc: %d\n", rc);
    }

    if (parsed_struct.IsInitialized() == false)
    {
        return -EINVAL;
        ACA_LOG_ERROR("Uninitialized parsed_struct rc: %d\n", rc);
    }

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    if (parsed_struct.ParseFromArray(kafka_buffer->get_data(),
                                     kafka_buffer->get_size()))
    {
        ACA_LOG_INFO("Successfully converted kafka buffer to protobuf struct\n");

        this->print_goal_state(parsed_struct);

        return EXIT_SUCCESS;
    }
    else
    {
        ACA_LOG_ERROR("Failed to convert kafka message to protobuf struct\n");
        return -EXIT_FAILURE;
    }
}

// Calls execute_command
int Aca_Comm_Manager::update_goal_state(
    aliothcontroller::GoalState &parsed_struct)
{
    int transitd_command = 0;
    void *transitd_input = NULL;
    int rc = -EXIT_FAILURE;
    int exec_command_rc = -EXIT_FAILURE;

    rpc_trn_endpoint_t endpoint_in;
    rpc_trn_agent_metadata_t agent_md_in;
    rpc_trn_network_t network_in;
    rpc_trn_vpc_t vpc_in;
    rpc_trn_endpoint_t substrate_in;

    bool tunnel_id_found = false;
    bool subnet_info_found = false;
    string my_ep_ip_address;
    string my_ep_host_ip_address;
    string my_cidr;
    string peer_name_string;
    string my_ip_address;
    string my_prefixlen;
    int slash_pos = 0;
    struct sockaddr_in sa;
    char veth_name[20];
    char peer_name[20];
    char hosted_interface[20];

    ACA_LOG_DEBUG("Starting to update goal state\n");

    for (int i = 0; i < parsed_struct.port_states_size(); i++)
    {
        ACA_LOG_DEBUG("=====>parsing port state #%d\n", i);

        aliothcontroller::PortConfiguration current_PortConfiguration =
            parsed_struct.port_states(i).configuration();

        switch (parsed_struct.port_states(i).operation_type())
        {
        case aliothcontroller::OperationType::CREATE:
        case aliothcontroller::OperationType::CREATE_UPDATE_SWITCH:
            transitd_command = UPDATE_EP;
            transitd_input = &endpoint_in;

            endpoint_in.interface = PHYSICAL_IF;

            assert(current_PortConfiguration.fixed_ips_size() == 1);
            my_ep_ip_address = current_PortConfiguration.fixed_ips(0).ip_address();
            struct sockaddr_in sa;
            // TODO: need to check return value, it returns 1 for success 0 for failure
            inet_pton(AF_INET, my_ep_ip_address.c_str(), &(sa.sin_addr));
            endpoint_in.ip = sa.sin_addr.s_addr;

            endpoint_in.eptype = TRAN_SIMPLE_EP;

            uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
            endpoint_in.remote_ips.remote_ips_val = remote_ips;
            endpoint_in.remote_ips.remote_ips_len = 1;
            inet_pton(AF_INET, current_PortConfiguration.host_info().ip_address().c_str(),
                      &(sa.sin_addr));
            endpoint_in.remote_ips.remote_ips_val[0] = sa.sin_addr.s_addr;

            if (sscanf(current_PortConfiguration.mac_address().c_str(),
                       "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &endpoint_in.mac[0],
                       &endpoint_in.mac[1],
                       &endpoint_in.mac[2],
                       &endpoint_in.mac[3],
                       &endpoint_in.mac[4],
                       &endpoint_in.mac[5]) != 6)
            {
                ACA_LOG_ERROR("Invalid mac input: %s.\n", current_PortConfiguration.mac_address().c_str());
            }

            // TODO: ensure the input name is 20 char or less
            strncpy(veth_name, current_PortConfiguration.name().c_str(),
                    strlen(current_PortConfiguration.name().c_str()) + 1);
            endpoint_in.veth = veth_name;

            if (parsed_struct.port_states(i).operation_type() == aliothcontroller::OperationType::CREATE_UPDATE_SWITCH)
            {
                endpoint_in.hosted_interface = EMPTY_STRING;
            }
            else // it must be OperationType::CREATE
            {
                peer_name_string = current_PortConfiguration.name() + PEER_POSTFIX;
                // TODO: ensure the input name is 20 char or less
                strncpy(peer_name, peer_name_string.c_str(),
                        strlen(peer_name_string.c_str()) + 1);
                // endpoint_in.hosted_interface = peer_name;
                endpoint_in.hosted_interface = (char *)"peer0";
            }

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
                        endpoint_in.tunid = current_SubnetConfiguration.tunnel_id();
                        tunnel_id_found = true;
                        break;
                    }
                }
            }
            if (!tunnel_id_found)
            {
                // TODO: print out more information for troubleshooting
                ACA_LOG_ERROR("Not able to find the tunnel ID information from subnet config.\n");
            }
            rc = EXIT_SUCCESS;
            break;

        case aliothcontroller::OperationType::FINALIZE:
            transitd_command = UPDATE_AGENT_MD;
            transitd_input = &agent_md_in;

            peer_name_string = current_PortConfiguration.name() + PEER_POSTFIX;
            // TODO: ensure the input name is 20 char or less
            strncpy(peer_name, peer_name_string.c_str(),
                    strlen(peer_name_string.c_str()) + 1);
            agent_md_in.interface = (char *)"peer0";

            agent_md_in.eth.interface = PHYSICAL_IF;

            agent_md_in.ep.interface = PHYSICAL_IF;
            assert(current_PortConfiguration.fixed_ips_size() == 1);
            my_ep_host_ip_address = current_PortConfiguration.host_info().ip_address();
            // TODO: need to check return value, it returns 1 for success 0 for failure
            inet_pton(AF_INET, my_ep_host_ip_address.c_str(), &(sa.sin_addr));
            agent_md_in.ep.ip = sa.sin_addr.s_addr;
            agent_md_in.ep.eptype = TRAN_SIMPLE_EP;

            uint32_t md_remote_ips[RPC_TRN_MAX_REMOTE_IPS];
            agent_md_in.ep.remote_ips.remote_ips_val = md_remote_ips;
            agent_md_in.ep.remote_ips.remote_ips_len = 1;
            inet_pton(AF_INET, current_PortConfiguration.host_info().ip_address().c_str(),
                      &(sa.sin_addr));
            agent_md_in.ep.remote_ips.remote_ips_val[0] = sa.sin_addr.s_addr;

            if (sscanf(current_PortConfiguration.mac_address().c_str(),
                       "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &agent_md_in.ep.mac[0],
                       &agent_md_in.ep.mac[1],
                       &agent_md_in.ep.mac[2],
                       &agent_md_in.ep.mac[3],
                       &agent_md_in.ep.mac[4],
                       &agent_md_in.ep.mac[5]) != 6)
            {
                ACA_LOG_ERROR("Invalid mac input: %s.\n", current_PortConfiguration.mac_address().c_str());
            }

            // TODO: ensure the input name is 20 char or less
            strncpy(veth_name, current_PortConfiguration.name().c_str(),
                    strlen(current_PortConfiguration.name().c_str()) + 1);
            agent_md_in.ep.veth = veth_name;

            agent_md_in.ep.hosted_interface = PHYSICAL_IF;

            // Look up the subnet configuration
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
                        agent_md_in.ep.tunid = current_SubnetConfiguration.tunnel_id();

                        agent_md_in.net.interface = PHYSICAL_IF;
                        agent_md_in.net.tunid = current_SubnetConfiguration.tunnel_id();

                        my_cidr = current_SubnetConfiguration.cidr();
                        int slash_pos = my_cidr.find("/");
                        // TODO: substr throw exceptions also
                        my_ip_address = my_cidr.substr(0, slash_pos);

                        struct sockaddr_in sa;
                        // TODO: need to check return value, it returns 1 for success 0 for failure
                        inet_pton(AF_INET, my_ip_address.c_str(), &(sa.sin_addr));
                        agent_md_in.net.netip = sa.sin_addr.s_addr;

                        my_prefixlen = my_cidr.substr(slash_pos + 1);
                        // TODO: stoi throw invalid argument exception when it cannot covert
                        agent_md_in.net.prefixlen = std::stoi(my_prefixlen);

                        agent_md_in.net.switches_ips.switches_ips_len =
                            current_SubnetConfiguration.transit_switches_size();
                        uint32_t switches[RPC_TRN_MAX_NET_SWITCHES];
                        agent_md_in.net.switches_ips.switches_ips_val = switches;

                        for (int k = 0; k < current_SubnetConfiguration.transit_switches_size(); k++)
                        {
                            struct sockaddr_in sa;
                            // TODO: need to check return value, it returns 1 for success 0 for failure
                            inet_pton(AF_INET, current_SubnetConfiguration.transit_switches(k).ip_address().c_str(),
                                      &(sa.sin_addr));
                            agent_md_in.net.switches_ips.switches_ips_val[k] =
                                sa.sin_addr.s_addr;
                        }

                        subnet_info_found = true;
                        break;
                    }
                }
            }
            if (!subnet_info_found)
            {
                ACA_LOG_ERROR("Not able to find the tunnel ID information from subnet config.\n");
            }

            rc = EXIT_SUCCESS;

            break;
        default:
            transitd_command = 0;
            ACA_LOG_DEBUG("Invalid port state operation type %d/n",
                          parsed_struct.port_states(i).operation_type());
            break;
        }

        if ((transitd_command != 0) && (rc == EXIT_SUCCESS))
        {
            exec_command_rc = this->execute_command(transitd_command, transitd_input);
            if (exec_command_rc == EXIT_SUCCESS)
            {
                ACA_LOG_INFO("Successfully executed the network controller command\n");
            }
            else
            {
                ACA_LOG_ERROR("Unable to execute the network controller command: %d\n",
                              exec_command_rc);
                // TODO: Notify the Network Controller if the command is not successful.
            }
        }

        // check if we need to call update substrate (MAC address)
        if (parsed_struct.port_states(i).operation_type() == aliothcontroller::OperationType::CREATE_UPDATE_SWITCH)
        {
            transitd_command = UPDATE_EP;
            transitd_input = &substrate_in;

            substrate_in.interface = PHYSICAL_IF;

            struct sockaddr_in sa;
            // TODO: need to check return value, it returns 1 for success 0 for failure
            inet_pton(AF_INET, current_PortConfiguration.host_info().ip_address().c_str(),
                      &(sa.sin_addr));
            substrate_in.ip = sa.sin_addr.s_addr;
            substrate_in.eptype = TRAN_SUBSTRT_EP;
            uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
            substrate_in.remote_ips.remote_ips_val = remote_ips;
            substrate_in.remote_ips.remote_ips_len = 0;

            if (sscanf(current_PortConfiguration.host_info().mac_address().c_str(),
                       "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &substrate_in.mac[0],
                       &substrate_in.mac[1],
                       &substrate_in.mac[2],
                       &substrate_in.mac[3],
                       &substrate_in.mac[4],
                       &substrate_in.mac[5]) != 6)
            {
                ACA_LOG_ERROR("Invalid mac input: %s.\n", current_PortConfiguration.host_info().mac_address().c_str());
            }
            substrate_in.hosted_interface = EMPTY_STRING;
            substrate_in.veth = EMPTY_STRING;
            substrate_in.tunid = TRAN_SUBSTRT_VNI;

            rc = EXIT_SUCCESS;

            if (rc == EXIT_SUCCESS)
            {
                exec_command_rc = this->execute_command(transitd_command, &substrate_in);
                if (exec_command_rc == EXIT_SUCCESS)
                {
                    ACA_LOG_INFO("Successfully updated substrate in transit daemon\n");
                }
                else
                {
                    ACA_LOG_ERROR("Unable to update substrate in transit daemon: %d\n", exec_command_rc);
                    // TODO: Notify the Network Controller if the command is not successful.
                }
            }
        }
        else if (parsed_struct.port_states(i).operation_type() == aliothcontroller::OperationType::FINALIZE)
        {
            transitd_command = UPDATE_AGENT_EP;

            // Look up the subnet info
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
                        for (int k = 0; k < current_SubnetConfiguration.transit_switches_size(); k++)
                        {
                            // substrate_in.interface = EMPTY_STRING;
                            substrate_in.interface = (char *)"peer0";

                            struct sockaddr_in sa;
                            // TODO: need to check return value, it returns 1 for success 0 for failure
                            inet_pton(AF_INET, current_SubnetConfiguration.transit_switches(k).ip_address().c_str(),
                                      &(sa.sin_addr));
                            substrate_in.ip = sa.sin_addr.s_addr;
                            substrate_in.eptype = TRAN_SUBSTRT_EP;
                            uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
                            substrate_in.remote_ips.remote_ips_val = remote_ips;
                            substrate_in.remote_ips.remote_ips_len = 0;
                            if (sscanf(current_SubnetConfiguration.transit_switches(k).mac_address().c_str(),
                                       "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                                       &substrate_in.mac[0],
                                       &substrate_in.mac[1],
                                       &substrate_in.mac[2],
                                       &substrate_in.mac[3],
                                       &substrate_in.mac[4],
                                       &substrate_in.mac[5]) != 6)
                            {
                                ACA_LOG_ERROR("Invalid mac input: %s.\n", current_SubnetConfiguration.transit_switches(k).mac_address().c_str());
                            }
                            substrate_in.hosted_interface = EMPTY_STRING;
                            substrate_in.veth = EMPTY_STRING;
                            substrate_in.tunid = TRAN_SUBSTRT_VNI;

                            rc = EXIT_SUCCESS;

                            if (rc == EXIT_SUCCESS)
                            {
                                exec_command_rc = this->execute_command(transitd_command, &substrate_in);
                                if (exec_command_rc == EXIT_SUCCESS)
                                {
                                    ACA_LOG_INFO("Successfully updated substrate in transit daemon\n");
                                }
                                else
                                {
                                    ACA_LOG_ERROR("Unable to update substrate in transit daemon: %d\n",
                                                  exec_command_rc);
                                    // TODO: Notify the Network Controller if the command is not successful.
                                }
                            }
                        } // for (int k = 0; k < current_SubnetConfiguration.transit_switches_size(); k++)

                        // found subnet information and completed the work, breaking out of the if condition
                        break;
                    }
                }
            }
        }

    } // for (int i = 0; i < parsed_struct.port_states_size(); i++)

    for (int i = 0; i < parsed_struct.subnet_states_size(); i++)
    {
        ACA_LOG_DEBUG("=====>parsing subnet state #%d\n", i);

        aliothcontroller::SubnetConfiguration current_SubnetConfiguration =
            parsed_struct.subnet_states(i).configuration();

        switch (parsed_struct.subnet_states(i).operation_type())
        {
        case aliothcontroller::OperationType::INFO:
            // information only, ignoring this.
            transitd_command = 0;
            break;
        case aliothcontroller::OperationType::CREATE_UPDATE_ROUTER:
            // this is to update the router host, only need to update substrate later.
            transitd_command = 0;
            break;
        case aliothcontroller::OperationType::CREATE:
        case aliothcontroller::OperationType::UPDATE:
            // TODO: There might be slight difference between Create and Update.
            // E.g. Create could require pre-check that if a subnet exists in this host etc.
            // TODO: not used based on the current simple scenario contract with controller.
            transitd_command = UPDATE_NET;
            transitd_input = &network_in;

            network_in.interface = PHYSICAL_IF;
            network_in.tunid = current_SubnetConfiguration.tunnel_id();

            my_cidr = current_SubnetConfiguration.cidr();
            slash_pos = my_cidr.find("/");
            // TODO: substr throw exceptions also
            my_ip_address = my_cidr.substr(0, slash_pos);

            // TODO: need to check return value, it returns 1 for success 0 for failure
            inet_pton(AF_INET, my_ip_address.c_str(), &(sa.sin_addr));
            network_in.netip = sa.sin_addr.s_addr;

            my_prefixlen = my_cidr.substr(slash_pos + 1);
            // TODO: stoi throw invalid argument exception when it cannot covert
            network_in.prefixlen = std::stoi(my_prefixlen);

            network_in.switches_ips.switches_ips_len =
                current_SubnetConfiguration.transit_switches_size();
            uint32_t switches[RPC_TRN_MAX_NET_SWITCHES];
            network_in.switches_ips.switches_ips_val = switches;

            for (int j = 0; j < current_SubnetConfiguration.transit_switches_size(); j++)
            {
                struct sockaddr_in sa;
                // TODO: need to check return value, it returns 1 for success 0 for failure
                inet_pton(AF_INET, current_SubnetConfiguration.transit_switches(j).ip_address().c_str(),
                          &(sa.sin_addr));
                network_in.switches_ips.switches_ips_val[j] =
                    sa.sin_addr.s_addr;
            }

            rc = EXIT_SUCCESS;

            break;
        default:
            transitd_command = 0;
            ACA_LOG_DEBUG("Invalid subnet state operation type %d/n",
                          parsed_struct.subnet_states(i).operation_type());
            break;
        }
        if ((transitd_command != 0) && (rc == EXIT_SUCCESS))
        {
            exec_command_rc = this->execute_command(transitd_command, transitd_input);
            if (exec_command_rc == EXIT_SUCCESS)
            {
                ACA_LOG_INFO("Successfully executed the network controller command\n");
            }
            else
            {
                ACA_LOG_ERROR("Unable to execute the network controller command: %d\n", exec_command_rc);
                // TODO: Notify the Network Controller if the command is not successful.
            }
        }

        // do we need to call update substrate?
        if (parsed_struct.subnet_states(i).operation_type() ==
            aliothcontroller::OperationType::CREATE_UPDATE_ROUTER)
        {
            transitd_command = UPDATE_EP;

            for (int j = 0; j < current_SubnetConfiguration.transit_switches_size(); j++)
            {
                substrate_in.interface = PHYSICAL_IF;

                struct sockaddr_in sa;
                // TODO: need to check return value, it returns 1 for success 0 for failure
                inet_pton(AF_INET, current_SubnetConfiguration.transit_switches(j).ip_address().c_str(),
                          &(sa.sin_addr));
                substrate_in.ip = sa.sin_addr.s_addr;
                substrate_in.eptype = TRAN_SUBSTRT_EP;
                uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
                substrate_in.remote_ips.remote_ips_val = remote_ips;
                substrate_in.remote_ips.remote_ips_len = 0;
                if (sscanf(current_SubnetConfiguration.transit_switches(j).mac_address().c_str(),
                           "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                           &substrate_in.mac[0],
                           &substrate_in.mac[1],
                           &substrate_in.mac[2],
                           &substrate_in.mac[3],
                           &substrate_in.mac[4],
                           &substrate_in.mac[5]) != 6)
                {
                    ACA_LOG_ERROR("Invalid mac input: %s.\n", current_SubnetConfiguration.transit_switches(j).mac_address().c_str());
                }
                substrate_in.hosted_interface = EMPTY_STRING;
                substrate_in.veth = EMPTY_STRING;
                substrate_in.tunid = TRAN_SUBSTRT_VNI;

                rc = EXIT_SUCCESS;

                if (rc == EXIT_SUCCESS)
                {
                    exec_command_rc = this->execute_command(transitd_command, &substrate_in);
                    if (exec_command_rc == EXIT_SUCCESS)
                    {
                        ACA_LOG_INFO("Successfully updated substrate in transit daemon\n");
                    }
                    else
                    {
                        ACA_LOG_ERROR("Unable to update substrate in transit daemon: %d\n",
                                      exec_command_rc);
                        // TODO: Notify the Network Controller if the command is not successful.
                    }
                }
            } // for (int j = 0; j < current_SubnetConfiguration.transit_switches_size(); j++)
        }

    } // for (int i = 0; i < parsed_struct.subnet_states_size(); i++)

    for (int i = 0; i < parsed_struct.vpc_states_size(); i++)
    {
        ACA_LOG_DEBUG("=====>parsing VPC state #%d\n", i);

        aliothcontroller::VpcConfiguration current_VpcConfiguration =
            parsed_struct.vpc_states(i).configuration();

        switch (parsed_struct.vpc_states(i).operation_type())
        {
        case aliothcontroller::OperationType::CREATE_UPDATE_SWITCH:
            transitd_command = UPDATE_VPC;
            transitd_input = &vpc_in;

            vpc_in.interface = PHYSICAL_IF;
            vpc_in.tunid = current_VpcConfiguration.tunnel_id();
            vpc_in.routers_ips.routers_ips_len =
                current_VpcConfiguration.transit_routers_size();
            uint32_t routers[RPC_TRN_MAX_VPC_ROUTERS];
            vpc_in.routers_ips.routers_ips_val = routers;

            for (int j = 0; j < current_VpcConfiguration.transit_routers_size(); j++)
            {
                struct sockaddr_in sa;
                // TODO: need to check return value, it returns 1 for success 0 for failure
                inet_pton(AF_INET, current_VpcConfiguration.transit_routers(j).ip_address().c_str(),
                          &(sa.sin_addr));
                vpc_in.routers_ips.routers_ips_val[j] =
                    sa.sin_addr.s_addr;
            }
            rc = EXIT_SUCCESS;

            break;
        default:
            transitd_command = 0;
            ACA_LOG_DEBUG("Invalid VPC state operation type %d\n",
                          parsed_struct.vpc_states(i).operation_type());
            break;
        }
        if ((transitd_command != 0) && (rc == EXIT_SUCCESS))
        {
            exec_command_rc = this->execute_command(transitd_command, transitd_input);
            if (exec_command_rc == EXIT_SUCCESS)
            {
                ACA_LOG_INFO("Successfully executed the network controller command\n");
            }
            else
            {
                ACA_LOG_ERROR("[update_goal_state] Unable to execute the network controller command: %d\n",
                              exec_command_rc);
                // TODO: Notify the Network Controller if the command is not successful.
            }
        }

        if (parsed_struct.vpc_states(i).operation_type() ==
            aliothcontroller::OperationType::CREATE_UPDATE_SWITCH)
        {
            // update substrate
            transitd_command = UPDATE_EP;

            for (int j = 0; j < current_VpcConfiguration.transit_routers_size(); j++)
            {
                substrate_in.interface = PHYSICAL_IF;

                struct sockaddr_in sa;
                // TODO: need to check return value, it returns 1 for success 0 for failure
                inet_pton(AF_INET, current_VpcConfiguration.transit_routers(j).ip_address().c_str(),
                          &(sa.sin_addr));
                substrate_in.ip = sa.sin_addr.s_addr;
                substrate_in.eptype = TRAN_SUBSTRT_EP;
                uint32_t remote_ips[RPC_TRN_MAX_REMOTE_IPS];
                substrate_in.remote_ips.remote_ips_val = remote_ips;
                substrate_in.remote_ips.remote_ips_len = 0;
                if (sscanf(current_VpcConfiguration.transit_routers(j).mac_address().c_str(),
                           "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                           &substrate_in.mac[0],
                           &substrate_in.mac[1],
                           &substrate_in.mac[2],
                           &substrate_in.mac[3],
                           &substrate_in.mac[4],
                           &substrate_in.mac[5]) != 6)
                {
                    ACA_LOG_ERROR("Invalid mac input: %s.\n", current_VpcConfiguration.transit_routers(j).mac_address().c_str());
                }
                substrate_in.hosted_interface = EMPTY_STRING;
                substrate_in.veth = EMPTY_STRING;
                substrate_in.tunid = TRAN_SUBSTRT_VNI;

                rc = EXIT_SUCCESS;

                if (rc == EXIT_SUCCESS)
                {
                    exec_command_rc = this->execute_command(transitd_command, &substrate_in);
                    if (exec_command_rc == EXIT_SUCCESS)
                    {
                        ACA_LOG_INFO("Successfully updated substrate in transit daemon\n");
                    }
                    else
                    {
                        ACA_LOG_ERROR("Unable to update substrate in transit daemon: %d\n",
                                      exec_command_rc);
                        // TODO: Notify the Network Controller if the command is not successful.
                    }
                }
            } // for (int j = 0; j < current_VpcConfiguration.transit_routers_size(); j++)
        }
    } // for (int i = 0; i < parsed_struct.vpc_states_size(); i++)

    return rc;
}

int Aca_Comm_Manager::execute_command(int command, void *input_struct)
{
    static CLIENT *client;
    int rc = EXIT_SUCCESS;
    int *transitd_return;

    ACA_LOG_INFO("Connecting to %s using %s protocol.\n", g_rpc_server.c_str(),
                 g_rpc_protocol.c_str());

    // TODO: We may change it to have a static client for health checking on
    // transit daemon in the future.
    client = clnt_create(g_rpc_server.c_str(), RPC_TRANSIT_REMOTE_PROTOCOL,
                         RPC_TRANSIT_ALFAZERO, g_rpc_protocol.c_str());

    if (client == NULL)
    {
        clnt_pcreateerror(g_rpc_server.c_str());
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
            transitd_return = update_agent_ep_1((rpc_trn_endpoint_t *)input_struct, client);
            break;
        case UPDATE_AGENT_MD:
            transitd_return = update_agent_md_1((rpc_trn_agent_metadata_t *)input_struct, client);
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
    if (g_debug_mode == false)
    {
        return;
    }

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
                "current_PortConfiguration.host_info().ip_address(): %s \n",
                current_PortConfiguration.host_info().ip_address().c_str());

        fprintf(stdout,
                "current_PortConfiguration.host_info().mac_address(): %s \n",
                current_PortConfiguration.host_info().mac_address().c_str());

        fprintf(stdout,
                "current_PortConfiguration.fixed_ips_size(): %u \n",
                current_PortConfiguration.fixed_ips_size());

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
                    "current_PortConfiguration.security_group_ids(%d): id %s \n", j,
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

        for (int j = 0; j < current_SubnetConfiguration.transit_switches_size(); j++)
        {
            fprintf(stdout,
                    "current_SubnetConfiguration.transit_switches(%d).vpc_id(): %s \n", j,
                    current_SubnetConfiguration.transit_switches(j).vpc_id().c_str());

            fprintf(stdout,
                    "current_SubnetConfiguration.transit_switches(%d).subnet_id(): %s \n", j,
                    current_SubnetConfiguration.transit_switches(j).subnet_id().c_str());

            fprintf(stdout,
                    "current_SubnetConfiguration.transit_switches(%d).ip_address(): %s \n", j,
                    current_SubnetConfiguration.transit_switches(j).ip_address().c_str());

            fprintf(stdout,
                    "current_SubnetConfiguration.transit_switches(%d).mac_address(): %s \n", j,
                    current_SubnetConfiguration.transit_switches(j).mac_address().c_str());
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

        for (int l = 0; l < current_VpcConfiguration.transit_routers_size(); l++)
        {
            fprintf(stdout,
                    "current_VpcConfiguration.transit_routers(%d).vpc_id(): "
                    "%s \n",
                    l,
                    current_VpcConfiguration.transit_routers(l).vpc_id().c_str());

            fprintf(stdout,
                    "current_VpcConfiguration.transit_routers(%d).ip_address(): "
                    "%s \n",
                    l,
                    current_VpcConfiguration.transit_routers(l).ip_address().c_str());

            fprintf(stdout,
                    "current_VpcConfiguration.transit_routers(%d).mac_address(): "
                    "%s \n",
                    l,
                    current_VpcConfiguration.transit_routers(l).mac_address().c_str());
        }
        printf("\n");
    }
}

} // namespace aca_comm_manager