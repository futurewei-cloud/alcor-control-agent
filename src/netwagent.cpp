#include <iostream>
#include "messageproducer.h"

using namespace std;
using messagemanager::MessageProducer;

int main() {
    
    //Preload network agent configuration 
    //TODO: load it from configuration file 
    string host_id = "00000000-0000-0000-0000-000000000000";
    string broker_list = "10.213.43.188:9092";
    string topic_host_spec = "kafka_test";  //"/hostid/" + host_id + "/hostspec/";
    int partition_value = 0;

    //Check if OVS and/or OVS daemon exists; if not, launch the program
    
    //Check if transit program exists; if not, launch the program 

    //Announce this host (agent) and register in every kafka cluster
    
    //Launch background threads to monitor and to emit network health status 
    //	for customer VMs, containers, as well as
    //	infra host services including OVS, transit etc.   
    
    //Upload or refresh the networking spec of this host (including DPDK, SR-IOV, bandwitdh etc.)
    MessageProducer host_spec_producer(broker_list, topic_host_spec, partition_value);
    cout << "broker list:" << host_spec_producer.getBrokers() << endl;
    cout << "topic:" << host_spec_producer.getTopicName() << endl;
    cout << "partition:" << host_spec_producer.getPartitionValue() << endl;

    string host_network_spec = "fake config";
    cout << "Prepare for publishing " << host_network_spec  <<endl;
    host_spec_producer.publish(host_network_spec);
    cout << "Publish completed" << endl;

    //Listen to Kafka clusters for any network configuration operations
    
    //Receive and deserialize any new configuration
    
    //Depending on different operations, program XDP through corresponding RPC apis by transit daemon
}

