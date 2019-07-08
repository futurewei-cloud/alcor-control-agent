// c includes
#include "aca_log.h"
#include "trn_rpc_protocol.h"

// cpp includes
#include <iostream>
#include <chrono>
#include <thread>
#include "messageproducer.h"
#include "messageconsumer.h"

#define ACALOGNAME "NetworkControlAgent"

using namespace std;
using namespace std::chrono_literals;
using messagemanager::MessageConsumer;
using messagemanager::MessageProducer;

// function to parse and process the command from network controller
int aca_parse_and_program(string raw_string)
{
    static CLIENT *clnt;
    char LOCALHOST[] = "localhost";
    char UDP[] = "udp";
    char *server = LOCALHOST;
    char *protocol = UDP;
    uint controller_command = 0;
    int *rc;

    *rc = -1;

    //deserialize any new configuration
    //P0, tracked by issue#16

    //Depending on different operations, program XDP through corresponding RPC
    //apis by transit daemon
    //P0, tracked by issue#17
    ACA_LOG_INFO("Connecting to %s using %s protocol.\n", server, protocol);

    clnt = clnt_create(server, RPC_TRANSIT_REMOTE_PROTOCOL,
                       RPC_TRANSIT_ALFAZERO, protocol);

    if (clnt == NULL)
    {
        clnt_pcreateerror(server);
        ACA_LOG_EMERG("Not able to create the RPC connection to Transit daemon.\n");
    }

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
        *rc = -1;
        break;
    }

    if (rc == (int *)NULL)
    {
        clnt_perror(clnt, "Call failed to update Transit daemon");
        ACA_LOG_EMERG("Call failed to update Transit daemon, command: %d.\n",
                      controller_command);
    }
    else if (*rc != 0)
    {
        ACA_LOG_EMERG("Fatal error for command: %d, see transitd logs for details.\n",
                      controller_command);
        // TODO: report the error back to network controller
    }

    ACA_LOG_INFO("Successfully updated transitd with command %d.\n",
                 controller_command);
    // TODO: can print out more command specific info

    clnt_destroy(clnt);

    return *rc;
}

int aca_comm_mgr_listen(bool keep_listening)
{
    //Preload network agent configuration
    //TODO: load it from configuration file
    string host_id = "00000000-0000-0000-0000-000000000000";
    string broker_list = "10.213.43.188:9092";
    string topic_host_spec = "kafka_test2"; //"/hostid/" + host_id + "/hostspec/";
    int partition_value = 0;
    int rc = -1;

    //Listen to Kafka clusters for any network configuration operations
    //P0, tracked by issue#15
    MessageConsumer network_config_consumer(broker_list, "test");
    string **payload;

    bool pool_res = false;

    while (keep_listening)
    {
        pool_res = network_config_consumer.consume(topic_host_spec, payload);
        if (pool_res)
        {
            cout << "Processing payload....: " << **payload << endl;
        }
        else
        {
            cout << "pool fails" << endl;
            break;
        }

        rc = aca_parse_and_program(**payload);

        if (pool_res && (payload != nullptr) && (*payload != nullptr))
        {
            delete *payload;
        }

        std::this_thread::sleep_for(5s);
    }
}

int main(int argc, char **argv)
{
    int rc = -1;

    ACA_LOG_INIT(ACALOGNAME);

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

    rc = aca_comm_mgr_listen(true);

    ACA_LOG_CLOSE();

    return rc;
}
