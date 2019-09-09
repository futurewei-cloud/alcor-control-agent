// c includes
#include "aca_comm_mgr.h"
#include "aca_log.h"
#include "aca_util.h"
#include "goalstate.pb.h"
#include "cppkafka/buffer.h"
#include <unistd.h> /* for getopt */
#include <chrono>
#include <string.h>
#include <thread>
#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include "goalstateprovisioner.grpc.pb.h"

#define ACALOGNAME "AliothControlAgentTest"

using namespace std;
using aca_comm_manager::Aca_Comm_Manager;

// Global variables
string g_rpc_server = EMPTY_STRING;
string g_rpc_protocol = EMPTY_STRING;
long g_total_rpc_call_time = 0;
long g_total_rpc_client_time = 0;
long g_total_update_GS_time = 0;
bool g_debug_mode = false;

using std::string;

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using aliothcontroller::GoalStateProvisioner;
using aliothcontroller::GoalStateRequest;
using aliothcontroller::GoalStateOperationReply;


static void aca_cleanup()
{
    ACA_LOG_DEBUG("g_total_rpc_call_time = %ld nanoseconds or %ld milliseconds\n",
                  g_total_rpc_call_time, g_total_rpc_call_time / 1000000);

    ACA_LOG_DEBUG("g_total_rpc_client_time = %ld nanoseconds or %ld milliseconds\n",
                  g_total_rpc_client_time, g_total_rpc_client_time / 1000000);

    ACA_LOG_DEBUG("g_total_update_GS_time = %ld nanoseconds or %ld milliseconds\n",
                  g_total_update_GS_time, g_total_update_GS_time / 1000000);

    ACA_LOG_INFO("Program exiting, cleaning up...\n");

    // Optional:  Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    ACA_LOG_CLOSE();
}

class GoalStateProvisionerClient {
 public:
  explicit GoalStateProvisionerClient(std::shared_ptr<Channel> channel)
      : stub_(GoalStateProvisioner::NewStub(channel)) {}

    int send_goalstate(aliothcontroller::GoalState& GoalState) {
    GoalStateOperationReply reply;
    ClientContext context;
    CompletionQueue cq;
    Status status;

    std::unique_ptr<ClientAsyncResponseReader<GoalStateOperationReply>> rpc(
        stub_->PrepareAsyncPushNetworkResourceStates(&context, GoalState, &cq));

    rpc->StartCall();
    rpc->Finish(&reply, &status, (void*)1);
    void* got_tag;
    bool ok = false;

    GPR_ASSERT(cq.Next(&got_tag, &ok));
    GPR_ASSERT(got_tag == (void*)1);
    GPR_ASSERT(ok);

    if (status.ok()) {
      return EXIT_SUCCESS;
    } else {
      return EXIT_FAILURE;
    }
  }

 private:
  std::unique_ptr<GoalStateProvisioner::Stub> stub_;
};

// function to handle ctrl-c and kill process
static void aca_signal_handler(int sig_num)
{
    fprintf(stdout, "Caught signal: %d\n", sig_num);

    // perform all the necessary cleanup here
    aca_cleanup();

    exit(sig_num);
}

int parse_goalstate (aliothcontroller::GoalState parsed_struct, aliothcontroller::GoalState GoalState_builder) {
        assert(parsed_struct.port_states_size() ==
              GoalState_builder.port_states_size());
        for (int i = 0; i < parsed_struct.port_states_size(); i++)
        {
            assert(parsed_struct.port_states(i).operation_type() ==
                   GoalState_builder.port_states(i).operation_type());

            assert(parsed_struct.port_states(i).configuration().version() ==
                   GoalState_builder.port_states(i).configuration().version());

            assert(parsed_struct.port_states(i).configuration().project_id() ==
                   GoalState_builder.port_states(i).configuration().project_id());

            assert(parsed_struct.port_states(i).configuration().network_id() ==
                   GoalState_builder.port_states(i).configuration().network_id());

            assert(parsed_struct.port_states(i).configuration().id() ==
                   GoalState_builder.port_states(i).configuration().id());

            assert(parsed_struct.port_states(i).configuration().name() ==
                   GoalState_builder.port_states(i).configuration().name());

            assert(parsed_struct.port_states(i).configuration().name() ==
                   GoalState_builder.port_states(i).configuration().name());

            assert(parsed_struct.port_states(i).configuration().admin_state_up() ==
                   GoalState_builder.port_states(i).configuration().admin_state_up());

            assert(parsed_struct.port_states(i).configuration().mac_address() ==
                   GoalState_builder.port_states(i).configuration().mac_address());

            assert(parsed_struct.port_states(i).configuration().veth_name() ==
                   GoalState_builder.port_states(i).configuration().veth_name());

            assert(parsed_struct.port_states(i).configuration().host_info().ip_address() ==
                   GoalState_builder.port_states(i).configuration().host_info().ip_address());

            assert(parsed_struct.port_states(i).configuration().host_info().mac_address() ==
                   GoalState_builder.port_states(i).configuration().host_info().mac_address());

            assert(parsed_struct.port_states(i).configuration().fixed_ips_size() ==
                   GoalState_builder.port_states(i).configuration().fixed_ips_size());
            for (int j = 0; j < parsed_struct.port_states(i).configuration().fixed_ips_size(); j++)
            {
                assert(parsed_struct.port_states(i).configuration().fixed_ips(j).subnet_id() ==
                       GoalState_builder.port_states(i).configuration().fixed_ips(j).subnet_id());

                assert(parsed_struct.port_states(i).configuration().fixed_ips(j).ip_address() ==
                       GoalState_builder.port_states(i).configuration().fixed_ips(j).ip_address());
            }

            assert(parsed_struct.port_states(i).configuration().security_group_ids_size() ==
                   GoalState_builder.port_states(i).configuration().security_group_ids_size());
            for (int j = 0; j < parsed_struct.port_states(i).configuration().security_group_ids_size(); j++)
            {
                assert(parsed_struct.port_states(i).configuration().security_group_ids(j).id() ==
                       GoalState_builder.port_states(i).configuration().security_group_ids(j).id());
            }

            assert(parsed_struct.port_states(i).configuration().allow_address_pairs_size() ==
                   GoalState_builder.port_states(i).configuration().allow_address_pairs_size());
            for (int j = 0; j < parsed_struct.port_states(i).configuration().allow_address_pairs_size(); j++)
            {
                assert(parsed_struct.port_states(i).configuration().allow_address_pairs(j).ip_address() ==
                       GoalState_builder.port_states(i).configuration().allow_address_pairs(j).ip_address());

                assert(parsed_struct.port_states(i).configuration().allow_address_pairs(j).mac_address() ==
                       GoalState_builder.port_states(i).configuration().allow_address_pairs(j).mac_address());
            }

            assert(parsed_struct.port_states(i).configuration().extra_dhcp_options_size() ==
                   GoalState_builder.port_states(i).configuration().extra_dhcp_options_size());
            for (int j = 0; j < parsed_struct.port_states(i).configuration().extra_dhcp_options_size(); j++)
            {
                assert(parsed_struct.port_states(i).configuration().extra_dhcp_options(j).name() ==
                       GoalState_builder.port_states(i).configuration().extra_dhcp_options(j).name());

                assert(parsed_struct.port_states(i).configuration().extra_dhcp_options(j).value() ==
                       GoalState_builder.port_states(i).configuration().extra_dhcp_options(j).value());
            }
        }
        assert(parsed_struct.subnet_states_size() ==
               GoalState_builder.subnet_states_size());

        for (int i = 0; i < parsed_struct.subnet_states_size(); i++)
        {

            assert(parsed_struct.subnet_states(i).operation_type() ==
                   GoalState_builder.subnet_states(i).operation_type());

            assert(parsed_struct.subnet_states(i).configuration().version() ==
                   GoalState_builder.subnet_states(i).configuration().version());

            assert(parsed_struct.subnet_states(i).configuration().project_id() ==
                   GoalState_builder.subnet_states(i).configuration().project_id());

            assert(parsed_struct.subnet_states(i).configuration().vpc_id() ==
                   GoalState_builder.subnet_states(i).configuration().vpc_id());

            assert(parsed_struct.subnet_states(i).configuration().id() ==
                   GoalState_builder.subnet_states(i).configuration().id());

            assert(parsed_struct.subnet_states(i).configuration().name() ==
                   GoalState_builder.subnet_states(i).configuration().name());

            assert(parsed_struct.subnet_states(i).configuration().cidr() ==
                   GoalState_builder.subnet_states(i).configuration().cidr());

            assert(parsed_struct.subnet_states(i).configuration().transit_switches_size() ==
                   GoalState_builder.subnet_states(i).configuration().transit_switches_size());

            for (int j = 0; j < parsed_struct.subnet_states(i).configuration().transit_switches_size(); j++)
            {
                assert(parsed_struct.subnet_states(i).configuration().transit_switches(j).vpc_id() ==
                       GoalState_builder.subnet_states(i).configuration().transit_switches(j).vpc_id());

                assert(parsed_struct.subnet_states(i).configuration().transit_switches(j).subnet_id() ==
                       GoalState_builder.subnet_states(i).configuration().transit_switches(j).subnet_id());

                assert(parsed_struct.subnet_states(i).configuration().transit_switches(j).ip_address() ==
                       GoalState_builder.subnet_states(i).configuration().transit_switches(j).ip_address());

                assert(parsed_struct.subnet_states(i).configuration().transit_switches(j).mac_address() ==
                       GoalState_builder.subnet_states(i).configuration().transit_switches(j).mac_address());
            }
        }

        assert(parsed_struct.vpc_states_size() ==
               GoalState_builder.vpc_states_size());

        for (int i = 0; i < parsed_struct.vpc_states_size(); i++)
        {
            assert(parsed_struct.vpc_states(i).operation_type() ==
                   GoalState_builder.vpc_states(i).operation_type());

            assert(parsed_struct.vpc_states(i).configuration().version() ==
                   GoalState_builder.vpc_states(i).configuration().version());

            assert(parsed_struct.vpc_states(i).configuration().project_id() ==
                   GoalState_builder.vpc_states(i).configuration().project_id());

            assert(parsed_struct.vpc_states(i).configuration().id() ==
                   GoalState_builder.vpc_states(i).configuration().id());

            assert(parsed_struct.vpc_states(i).configuration().name() ==
                   GoalState_builder.vpc_states(i).configuration().name());

            assert(parsed_struct.vpc_states(i).configuration().cidr() ==
                   GoalState_builder.vpc_states(i).configuration().cidr());

            assert(parsed_struct.vpc_states(i).configuration().tunnel_id() ==
                   GoalState_builder.vpc_states(i).configuration().tunnel_id());

            assert(parsed_struct.vpc_states(i).configuration().subnet_ids_size() ==
                   GoalState_builder.vpc_states(i).configuration().subnet_ids_size());

            for (int j = 0; j < parsed_struct.vpc_states(i).configuration().subnet_ids_size(); j++)
            {
                assert(parsed_struct.vpc_states(i).configuration().subnet_ids(j).id() ==
                       GoalState_builder.vpc_states(i).configuration().subnet_ids(j).id());
            }

            assert(parsed_struct.vpc_states(i).configuration().routes_size() ==
                   GoalState_builder.vpc_states(i).configuration().routes_size());

            for (int k = 0; k < parsed_struct.vpc_states(i).configuration().routes_size(); k++)
            {
                assert(parsed_struct.vpc_states(i).configuration().routes(k).destination() ==
                       GoalState_builder.vpc_states(i).configuration().routes(k).destination());

                assert(parsed_struct.vpc_states(i).configuration().routes(k).next_hop() ==
                       GoalState_builder.vpc_states(i).configuration().routes(k).next_hop());
            }

            assert(parsed_struct.vpc_states(i).configuration().transit_routers_size() ==
                   GoalState_builder.vpc_states(i).configuration().transit_routers_size());

            for (int l = 0; l < parsed_struct.vpc_states(i).configuration().transit_routers_size(); l++)
            {
                assert(parsed_struct.vpc_states(i).configuration().transit_routers(l).vpc_id() ==
                       GoalState_builder.vpc_states(i).configuration().transit_routers(l).vpc_id());

                assert(parsed_struct.vpc_states(i).configuration().transit_routers(l).ip_address() ==
                       GoalState_builder.vpc_states(i).configuration().transit_routers(l).ip_address());

                assert(parsed_struct.vpc_states(i).configuration().transit_routers(l).mac_address() ==
                       GoalState_builder.vpc_states(i).configuration().transit_routers(l).mac_address());
            }
        }

        fprintf(stdout, "All content matched, sending the parsed_struct to update_goal_state...\n");

        int rc = Aca_Comm_Manager::get_instance().update_goal_state(parsed_struct);
        return rc;
}
int main(int argc, char *argv[])
{
    int option;
    int rc;
    ACA_LOG_INIT(ACALOGNAME);

    // Register the signal handlers
    signal(SIGINT, aca_signal_handler);
    signal(SIGTERM, aca_signal_handler);

    while ((option = getopt(argc, argv, "s:p:d")) != -1)
    {
        switch (option)
        {
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
                    "\t\t[-s transitd RPC server]\n"
                    "\t\t[-p transitd RPC protocol]\n"
                    "\t\t[-d enable debug mode]\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // fill in the RPC server and protocol if it is not provided in command line arg
    if (g_rpc_server == EMPTY_STRING)
    {
        g_rpc_server = LOCALHOST;
    }
    if (g_rpc_protocol == EMPTY_STRING)
    {
        g_rpc_protocol = UDP;
    }

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    aliothcontroller::GoalState GoalState_builder;
    // aliothcontroller::PortState *new_port_states =
    //     GoalState_builder.add_port_states();
    // aliothcontroller::SubnetState *new_subnet_states =
    //     GoalState_builder.add_subnet_states();
    // aliothcontroller::VpcState *new_vpc_states =
    //     GoalState_builder.add_vpc_states();

    // fill in port state structs
    /*
    new_port_states->set_operation_type(aliothcontroller::OperationType::FINALIZE);

    // this will allocate new PortConfiguration, will need to free it later
    aliothcontroller::PortConfiguration *PortConfiguration_builder =
        new_port_states->mutable_configuration();
    PortConfiguration_builder->set_version(1);
    PortConfiguration_builder->set_project_id(
        "dbf72700-5106-4a7a-918f-111111111111");
    PortConfiguration_builder->set_network_id("superSubnetID");
    PortConfiguration_builder->set_id("dd12d1dadad2g4h");
    PortConfiguration_builder->set_name("Peer1");
    PortConfiguration_builder->set_admin_state_up(true);
    PortConfiguration_builder->set_mac_address("fa:16:3e:d7:f2:6c");
    PortConfiguration_builder->set_veth_name("veth0");

    aliothcontroller::PortConfiguration_HostInfo *portConfig_HostInfoBuilder(new aliothcontroller::PortConfiguration_HostInfo);
    portConfig_HostInfoBuilder->set_ip_address("172.0.0.2");
    portConfig_HostInfoBuilder->set_mac_address("aa-bb-cc-dd-ee-ff");
    PortConfiguration_builder->set_allocated_host_info(portConfig_HostInfoBuilder);

    // this will allocate new PortConfiguration_FixedIp may need to free later
    aliothcontroller::PortConfiguration_FixedIp *PortIp_builder =
        PortConfiguration_builder->add_fixed_ips();
    PortIp_builder->set_ip_address("10.0.0.2");
    PortIp_builder->set_subnet_id("2");
    // this will allocate new PortConfiguration_SecurityGroupId may need to free later
    aliothcontroller::PortConfiguration_SecurityGroupId *SecurityGroup_builder =
        PortConfiguration_builder->add_security_group_ids();
    SecurityGroup_builder->set_id("1");
    // this will allocate new PortConfiguration_AllowAddressPair may need to free later
    aliothcontroller::PortConfiguration_AllowAddressPair
        *AddressPair_builder =
            PortConfiguration_builder->add_allow_address_pairs();
    AddressPair_builder->set_ip_address("10.0.0.5");
    AddressPair_builder->set_mac_address("fa:16:3e:d7:f2:9f");
    // this will allocate new PortConfiguration_ExtraDhcpOption may need to free later
    aliothcontroller::PortConfiguration_ExtraDhcpOption *ExtraDhcp_builder =
        PortConfiguration_builder->add_extra_dhcp_options();
    ExtraDhcp_builder->set_name("opt_1");
    ExtraDhcp_builder->set_value("12");

    // fill in the subnet state structs

    new_subnet_states->set_operation_type(aliothcontroller::OperationType::INFO);

    // this will allocate new SubnetConfiguration, will need to free it later
    aliothcontroller::SubnetConfiguration *SubnetConiguration_builder =
        new_subnet_states->mutable_configuration();
    SubnetConiguration_builder->set_version(1);
    SubnetConiguration_builder->set_project_id(
        "dbf72700-5106-4a7a-918f-111111111111");
    // VpcConiguration_builder->set_id("99d9d709-8478-4b46-9f3f-2206b1023fd3");
    SubnetConiguration_builder->set_vpc_id("99d9d709-8478-4b46-9f3f-2206b1023fd3");
    SubnetConiguration_builder->set_id("superSubnetID");
    SubnetConiguration_builder->set_name("SuperSubnet");
    SubnetConiguration_builder->set_cidr("10.0.0.1/16");
    SubnetConiguration_builder->set_tunnel_id(22222);
    // this will allocate new SubnetConfiguration_TransitSwitch, may need to free it later
    aliothcontroller::SubnetConfiguration_TransitSwitch *TransitSwitch_builder =
        SubnetConiguration_builder->add_transit_switches();
    TransitSwitch_builder->set_vpc_id("99d9d709-8478-4b46-9f3f-2206b1023fd3");
    TransitSwitch_builder->set_subnet_id("superSubnet");
    TransitSwitch_builder->set_ip_address("172.0.0.1");
    TransitSwitch_builder->set_mac_address("cc:dd:ee:aa:bb:cc");
*/
    // fill in the vpc state structs

    aliothcontroller::VpcState *new_vpc_states;

    for (int i = 0; i < 1000; i++)
    {
        new_vpc_states = GoalState_builder.add_vpc_states();

        new_vpc_states->set_operation_type(aliothcontroller::OperationType::CREATE_UPDATE_SWITCH);

        // this will allocate new VpcConfiguration, will need to free it later
        aliothcontroller::VpcConfiguration *VpcConiguration_builder =
            new_vpc_states->mutable_configuration();
        VpcConiguration_builder->set_version(1);
        VpcConiguration_builder->set_project_id(
            "dbf72700-5106-4a7a-918f-a016853911f8");
        // VpcConiguration_builder->set_id("99d9d709-8478-4b46-9f3f-2206b1023fd3");
        VpcConiguration_builder->set_id("1");
        VpcConiguration_builder->set_name("SuperVpc");
        VpcConiguration_builder->set_cidr("192.168.0.0/24");
        VpcConiguration_builder->set_tunnel_id(11111);
        // this will allocate new VpcConfiguration_TransitRouter, may need to free it later
        aliothcontroller::VpcConfiguration_TransitRouter *TransitRouter_builder =
            VpcConiguration_builder->add_transit_routers();
        TransitRouter_builder->set_vpc_id("12345");
        TransitRouter_builder->set_ip_address("10.0.0.2");
        TransitRouter_builder->set_mac_address("aa-bb-cc-dd-ee-ff");
    }

    string string_message;

    // Serialize it to string
    GoalState_builder.SerializeToString(&string_message);
    fprintf(stdout, "(NOT USED) Serialized protobuf string: %s\n",
            string_message.c_str());

    // Serialize it to binary array
    size_t size = GoalState_builder.ByteSize();
    char *buffer = (char *)malloc(size);
    GoalState_builder.SerializeToArray(buffer, size);
    string binary_message(buffer, size);
    fprintf(stdout, "(USING THIS) Serialized protobuf binary array: %s\n",
            binary_message.c_str());

    aliothcontroller::GoalState parsed_struct;

    cppkafka::Buffer kafka_buffer(buffer, size);

    rc = Aca_Comm_Manager::get_instance().deserialize(&kafka_buffer, parsed_struct);

    if (buffer != NULL)
    {
        free(buffer);
        buffer = NULL;
    }

    if (rc == EXIT_SUCCESS)
    {
        fprintf(stdout, "Deserialize succeeded, comparing the content now...\n");
        rc = parse_goalstate(parsed_struct, GoalState_builder);
        if (rc == EXIT_SUCCESS) {
               fprintf(stdout, "[Functional test] Successfully executed the network controller command\n");
        } else {
               fprintf(stdout,
                    "[Funtional test] Unable to execute the network controller command: %d\n", rc);
        }

    }
    else
    {
        fprintf(stdout, "Deserialize failed with error code: %u\n", rc);
    }
    // free the allocated VpcConfiguration since we are done with it now
    // new_port_states->clear_configuration();
    // new_subnet_states->clear_configuration();
    new_vpc_states->clear_configuration();

    GoalStateProvisionerClient async_client(grpc::CreateChannel(
        "localhost:50001", grpc::InsecureChannelCredentials()));

    rc = async_client.send_goalstate(GoalState_builder);
    if (rc == EXIT_SUCCESS)
    {
        fprintf(stdout, "RPC Sucess\n");
    }
    else
    {
        fprintf(stdout, "RPC Failure\n");
    }

    aca_cleanup();

    return rc;
}