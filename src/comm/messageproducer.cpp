#include <iostream>
#include <stdexcept>
#include "messageproducer.h"

using namespace std;
using namespace cppkafka;

MessageProducer::MessageProducer(string brokers, string topic, int partition){
	setBrokers(brokers);
	setTopicName(topic);
	setPartitionValue(partition);

	// Construct the configuration
    	config = {
        	{ "metadata.broker.list", brokers_list }
    	};

    	// Create the producer
     	Producer producer(config);
    	m_producer = producer;
} 

string MessageProducer::getBrokers() const{
	return brokers_list;
}

string MessageProducer::getTopicName() const{
	return topic_name;
}

void MessageProducer::setTopicName(string topic){
	topic_name = topic;
}

int MessageProducer::getPartitionValue() const{
	return partition_value;
}

void MessageProducer::setPartitionValue(int partition){
    	if(partition < -1){
		throw invalid_argument("Negative partition value");
	}

	partition_value = partition;
}
 
void MessageProducer::publish(string message){
	 
	 // Create a message builder for this topic
         MessageBuilder builder(topic_name);

         // Get the partition we want to write to. If no partition is provided, this will be
         // an unassigned one
        if (partition_value != -1) {
           builder.partition(partition_value);
        }  

	// Set the payload on this builder
        builder.payload(message);

        // Actually produce the message we've built
        m_producer.produce(builder);
    
	// Flush all produced messages
    	m_producer.flush();
}

void MessageProducer::setBrokers(string brokers){
	//TODO: validate string as IP address
	brokers_list = brokers;
}
