#ifndef KAFKA_PRODUCER_H
#define KAFKA_PRODUCER_H

#include "cppkafka/producer.h"
#include "cppkafka/configuration.h"

using namespace std;
using namespace cppkafka;

namespace messagemanager{ 

class MessageProducer {

private:

    string brokers_list;        //IP addresses of Kafka brokers, format: <Kafka_host_ip>:<port>, example:10.213.43.188:9092
    
    string topic_name;     	//A string representation of the topic name to be published, example: /hostid/00000000-0000-0000-0000-000000000000/netwconf/
    
    int partition_value = -1;   //Number of partitions used for a given topic, set by publisher 

    Configuration config;       //Configuration of a producer

    Producer* ptr_producer;     //A pointer to the kafka producer

public: 

    MessageProducer(string brokers, string topic, int partition); 

    ~MessageProducer();

    string getBrokers() const;

    string getTopicName() const;

    void setTopicName(string topic);

    int getPartitionValue() const;

    void setPartitionValue(int partition);
 
    void publish(string message);

private:

    void setBrokers(string brokers);
};

} // meesagemanager 

#endif
