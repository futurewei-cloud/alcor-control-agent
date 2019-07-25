// c includes
#include "aca_comm_mgr.h"
#include "aca_log.h"
#include "goalstate.pb.h"
#include "trn_rpc_protocol.h"
#include <unistd.h> /* for getopt */
#define ACALOGNAME "AliothControlAgent"
#include "messageconsumer.h"
#include "messageproducer.h"
#include <chrono>
#include <string.h>
#include <thread>

using namespace std;
using aca_comm_manager::Aca_Comm_Manager;
using messagemanager::MessageConsumer;

// Defines
static char LOCALHOST[] = "localhost";
static char UDP[] = "udp";

// Global variables
bool g_load_mode = false;
char *g_test_message = NULL;
char *g_rpc_server = NULL;
char *g_rpc_protocol = NULL;

using std::string;

static inline void aca_free(void *pointer)
{
    if (pointer != NULL)
    {
        free(pointer);
        pointer = NULL;
    }
}

static void aca_cleanup()
{
    // Optional:  Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    aca_free(g_test_message);
    aca_free(g_rpc_server);
    aca_free(g_rpc_protocol);

    fprintf(stdout, "Program exiting, cleaning up...\n");

    ACA_LOG_INFO("Program exiting, cleaning up...\n");
    ACA_LOG_CLOSE();
}

// function to handle ctrl-c and kill process
static void aca_signal_handler(int sig_num)
{
    fprintf(stdout, "Caught signal: %d\n", sig_num);

    // perform all the necessary cleanup here
    aca_cleanup();

    exit(sig_num);
}

int main(int argc, char *argv[])
{
    int option;
    int rc;
    ACA_LOG_INIT(ACALOGNAME);

    // Register the signal handlers
    signal(SIGINT, aca_signal_handler);
    signal(SIGTERM, aca_signal_handler);

    while ((option = getopt(argc, argv, "ls:p:")) != -1)
    {
        switch (option)
        {
        case 'l':
            g_load_mode = true;
            break;
        case 's':
            g_rpc_server = (char *)malloc(sizeof(char) * strlen(optarg) + 1);
            if (g_rpc_server != NULL)
            {
                strncpy(g_rpc_server, optarg, strlen(optarg) + 1);
            }
            else
            {
                fprintf(stdout,
                        "Out of memory when allocating string with size: %lu.\n",
                        (sizeof(char) * strlen(optarg) + 1));
                exit(EXIT_FAILURE);
            }
            break;
        case 'p':
            g_rpc_protocol = (char *)malloc(sizeof(char) * strlen(optarg) + 1);
            if (g_rpc_protocol != NULL)
            {
                strncpy(g_rpc_protocol, optarg, strlen(optarg) + 1);
            }
            else
            {
                fprintf(stdout,
                        "Out of memory when allocating string with size: %lu.\n",
                        (sizeof(char) * strlen(optarg) + 1));
                exit(EXIT_FAILURE);
            }
            break;
        default: /* the '?' case when the option is not recognized */
            fprintf(stderr,
                    "Usage: %s\n"
                    "\t\t[-l command to load transit daemon]\n"
                    "\t\t[-s transitd RPC server]\n"
                    "\t\t[-p transitd RPC protocol]\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // fill in the RPC server and protocol if it is not provided in command line
    // arg
    if (g_rpc_server == NULL)
    {
        g_rpc_server = (char *)malloc(sizeof(char) * strlen(LOCALHOST) + 1);
        if (g_rpc_server != NULL)
        {
            strncpy(g_rpc_server, LOCALHOST, strlen(LOCALHOST) + 1);
        }
        else
        {
            fprintf(stdout, "Out of memory when allocating string with size: %lu.\n",
                    (sizeof(char) * strlen(LOCALHOST) + 1));
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
            fprintf(stdout, "Out of memory when allocating string with size: %lu.\n",
                    (sizeof(char) * strlen(UDP) + 1));
            exit(EXIT_FAILURE);
        }
    }

    if (g_load_mode)
    {
        // we can just execute command here
        // rc = comm_manager.process_messages();
        /*
    static CLIENT *client;
    client = clnt_create(g_rpc_server, RPC_TRANSIT_REMOTE_PROTOCOL,
                         RPC_TRANSIT_ALFAZERO, g_rpc_protocol);
    rpc_trn_xdp_intf_t *intf =
        (rpc_trn_xdp_intf_t *)(malloc(sizeof(rpc_trn_xdp_intf_t)));
    intf->interface = (char *)"eth0";
    intf->xdp_path = (char *)"/mnt/Transit/build/xdp/transit_xdp_ebpf.o";
    intf->pcapfile = (char *)"/sys/fs/bpf/transitxdp.pcap";
    int *rc = load_transit_xdp_1(intf, client);
    fprintf(stdout, "Return code for load_transit_xdp is %d\n", *rc);
    */
        string broker_list = "10.213.43.188:9092";
        //string topic_host_spec = "hostid-bb009e95-3839-4a9d-abd9-9ad70b538112";  //"/hostid/" + host_id + "/hostspec/";
        string topic_host_spec = "my_topic";
        //int partition_value = 0;
        bool keep_listen = true;

        MessageConsumer network_config_consumer(broker_list, "test");
        //MessageProducer network_config_producer(broker_list, topic_host_spec, 0);

        bool pool_res = network_config_consumer.cosumeDispatched(topic_host_spec);
        //network_config_producer.publish("Hello");
        // while (keep_listen)
        // {

        //   bool pool_res = network_config_consumer.consume(topic_host_spec, payload);
        //   if (pool_res)
        //   {
        //     fprintf(stdout, "Processing payload....%s: ", (**payload).c_str());
        //   }
        //   else
        //   {
        //     //cout << "pool fails" << endl;
        //   }

        //   //if(payload != nullptr && *payload != nullptr){
        //   //	delete *payload;
        //   //}
        //   std::this_thread::sleep_for(5s);
        // }
    }
    else
    {
        // Verify that the version of the library that we linked against is
        // compatible with the version of the headers we compiled against.
        GOOGLE_PROTOBUF_VERIFY_VERSION;

        aliothcontroller::GoalState GoalState_builder;
        aliothcontroller::VpcState *new_vpc_states =
            GoalState_builder.add_vpc_states();
        new_vpc_states->set_operation_type(aliothcontroller::OperationType::CREATE);

        // this will allocate new VpcConfiguration, need to free it later
        aliothcontroller::VpcConfiguration *VpcConiguration_builder =
            new_vpc_states->mutable_configuration();
        VpcConiguration_builder->set_project_id(
            "dbf72700-5106-4a7a-918f-a016853911f8");
        // VpcConiguration_builder->set_id("99d9d709-8478-4b46-9f3f-2206b1023fd3");
        VpcConiguration_builder->set_id("1");
        VpcConiguration_builder->set_name("SuperVpc");
        VpcConiguration_builder->set_cidr("192.168.0.0/24");
        // this will allocate new VpcConfiguration_TransitRouterIp, may to free it later
        aliothcontroller::VpcConfiguration_TransitRouterIp *TransitRouterIp_builder =
            VpcConiguration_builder->add_transit_router_ips();
        TransitRouterIp_builder->set_vpc_id("12345");
        TransitRouterIp_builder->set_ip_address("10.0.0.2");

        string string_message;

        // Serialize it to string
        GoalState_builder.SerializeToString(&string_message);
        fprintf(stdout, "(NOTE USED) Serialized protobuf string: %s\n",
                string_message.c_str());

        // Serialize it to binary array
        size_t size = GoalState_builder.ByteSize();
        char *buffer = (char *)malloc(size);
        GoalState_builder.SerializeToArray(buffer, size);
        string binary_message(buffer, size);
        fprintf(stdout, "Serialized protobuf binary array: %s\n",
                binary_message.c_str());

        Aca_Comm_Manager comm_manager;

        aliothcontroller::GoalState parsed_struct;

        rc = comm_manager.deserialize(binary_message, parsed_struct);

        aca_free(buffer);

        if (rc == EXIT_SUCCESS)
        {

            fprintf(stdout, "Deserialize succeed, comparing the content now...\n");

            fprintf(stdout,
                    "parsed_struct.vpc_states_size() = %d; \n"
                    "GoalState_builder.vpc_states_size() = %d\n",
                    parsed_struct.vpc_states_size(),
                    GoalState_builder.vpc_states_size());

            assert(parsed_struct.vpc_states_size() ==
                   GoalState_builder.vpc_states_size());

            for (int i = 0; i < parsed_struct.vpc_states_size(); i++)
            {

                assert(parsed_struct.vpc_states(i).operation_type() ==
                       GoalState_builder.vpc_states(i).operation_type());
                fprintf(stdout,
                        "parsed_struct.vpc_states(%d).operation_type(): %d\n", i,
                        parsed_struct.vpc_states(i).operation_type());

                assert(
                    parsed_struct.vpc_states(i).configuration().project_id() ==
                    GoalState_builder.vpc_states(i).configuration().project_id());
                fprintf(stdout,
                        "parsed_struct.vpc_states(%d).configuration().project_id(): %s\n", i,
                        parsed_struct.vpc_states(i).configuration().project_id().c_str());

                assert(parsed_struct.vpc_states(i).configuration().id() ==
                       GoalState_builder.vpc_states(i).configuration().id());
                fprintf(stdout,
                        "parsed_struct.vpc_states(%d).configuration().id(): %s\n", i,
                        parsed_struct.vpc_states(i).configuration().id().c_str());

                assert(parsed_struct.vpc_states(i).configuration().name() ==
                       GoalState_builder.vpc_states(i).configuration().name());
                fprintf(stdout,
                        "parsed_struct.vpc_states(%d).configuration().name(): %s \n", i,
                        parsed_struct.vpc_states(i)
                            .configuration()
                            .name()
                            .c_str());

                assert(parsed_struct.vpc_states(i).configuration().cidr() ==
                       GoalState_builder.vpc_states(i).configuration().cidr());
                fprintf(stdout,
                        "parsed_struct.vpc_states(%d).configuration().cidr(): %s \n", i,
                        parsed_struct.vpc_states(i)
                            .configuration()
                            .cidr()
                            .c_str());

                for (int j = 0; j < parsed_struct.vpc_states(i).configuration().subnet_ids_size(); j++)
                {
                    assert(parsed_struct.vpc_states(i).configuration().subnet_ids(j).id() ==
                           GoalState_builder.vpc_states(i).configuration().subnet_ids(j).id());
                    fprintf(stdout,
                            "parsed_struct.vpc_states(%d).configuration().subnet_ids(%d): %s \n",
                            i, j,
                            parsed_struct.vpc_states(i)
                                .configuration()
                                .subnet_ids(j)
                                .id()
                                .c_str());
                }

                for (int k = 0; k < parsed_struct.vpc_states(i).configuration().routes_size(); k++)
                {
                    assert(parsed_struct.vpc_states(i).configuration().routes(k).destination() ==
                           GoalState_builder.vpc_states(i).configuration().routes(k).destination());
                    fprintf(stdout,
                            "parsed_struct.vpc_states(%d).configuration().routes(%d).destination(): "
                            "%s \n",
                            i, k,
                            parsed_struct.vpc_states(i)
                                .configuration()
                                .routes(k)
                                .destination()
                                .c_str());

                    assert(parsed_struct.vpc_states(i).configuration().routes(k).next_hop() ==
                           GoalState_builder.vpc_states(i).configuration().routes(k).next_hop());
                    fprintf(stdout,
                            "parsed_struct.vpc_states(%d).configuration().routes(%d).next_hop(): "
                            "%s \n",
                            i, k,
                            parsed_struct.vpc_states(i)
                                .configuration()
                                .routes(k)
                                .next_hop()
                                .c_str());
                }

                for (int l = 0; l < parsed_struct.vpc_states(i).configuration().transit_router_ips_size(); l++)
                {
                    assert(parsed_struct.vpc_states(i).configuration().transit_router_ips(l).vpc_id() ==
                           GoalState_builder.vpc_states(i).configuration().transit_router_ips(l).vpc_id());
                    fprintf(stdout,
                            "parsed_struct.vpc_states(%d).configuration().transit_router_ips(%d).vpc_id(): "
                            "%s \n",
                            i, l,
                            parsed_struct.vpc_states(i)
                                .configuration()
                                .transit_router_ips(l)
                                .vpc_id()
                                .c_str());

                    assert(parsed_struct.vpc_states(i).configuration().transit_router_ips(l).ip_address() ==
                           GoalState_builder.vpc_states(i).configuration().transit_router_ips(l).ip_address());
                    fprintf(stdout,
                            "parsed_struct.vpc_states(%d).configuration().transit_router_ips(%d).ip_address(): "
                            "%s \n",
                            i, l,
                            parsed_struct.vpc_states(i)
                                .configuration()
                                .transit_router_ips(l)
                                .ip_address()
                                .c_str());
                }
            }

            int rc = comm_manager.update_goal_state(parsed_struct);
            if (rc == EXIT_SUCCESS)
            {
                fprintf(stdout, "Successfully executed the network controller command");
            }
            else
            {
                fprintf(stdout,
                        "Unable to execute the network controller command: %d\n", rc);
            }
        }
        else
        {
            fprintf(stdout, "Deserialize failed with error code: %u\n", rc);
        }
        // free the allocated VpcConfiguration since we are done with it now
        new_vpc_states->clear_configuration();
    }
    /*
      if ((payload != nullptr) && (*payload != nullptr))
      {
          delete *payload;
      }
  */

    aca_cleanup();

    return rc;
}
