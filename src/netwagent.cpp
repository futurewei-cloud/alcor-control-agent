// c includes
#include "nca_log.h"
#include "../../Transit/src/rpcgen/trn_rpc_protocol.h" // TODO: need to use a better path

// cpp includes
#include <iostream>
#include "messageproducer.h"

#define NCALOGNAME "NetworkControlAgent"

using namespace std;
using messagemanager::MessageProducer;

int main(int argc, char **argv)
{
    NCA_LOG_INIT(NCALOGNAME);

    //Preload network agent configuration 
    //TODO: load it from configuration file 
    string host_id = "00000000-0000-0000-0000-000000000000";
    string broker_list = "10.213.43.188:9092";
    string topic_host_spec = "kafka_test";  //"/hostid/" + host_id + "/hostspec/";
    int partition_value = 0;

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
    
    //Upload or refresh the networking spec of this host (including DPDK, SR-IOV, bandwidth etc.)
    //P1, tracked by issue#14
    MessageProducer host_spec_producer(broker_list, topic_host_spec, partition_value);
    cout << "broker list:" << host_spec_producer.getBrokers() << endl;
    cout << "topic:" << host_spec_producer.getTopicName() << endl;
    cout << "partition:" << host_spec_producer.getPartitionValue() << endl;

    string host_network_spec = "fake config";
    cout << "Prepare for publishing " << host_network_spec  <<endl;
    host_spec_producer.publish(host_network_spec);
    cout << "Publish completed" << endl;

    //Listen to Kafka clusters for any network configuration operations
    //P0, tracked by issue#15
    
    //Receive and deserialize any new configuration
    //P0, tracked by issue#16
    
    //Depending on different operations, program XDP through corresponding RPC apis by transit daemon
    //P0, tracked by issue#17

    NCA_LOG_CLOSE();
}

