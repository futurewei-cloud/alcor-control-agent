#include <iostream>
#include <stdexcept>
#include "messageconsumer.h"

using std::string;
using std::exception;
using std::cout;
using std::endl;
using cppkafka::Consumer;
using cppkafka::Configuration;
using cppkafka::Message;
using cppkafka::TopicPartitionList;

namespace messagemanager{ 

MessageConsumer::MessageConsumer(string brokers, string group_id){
	setBrokers(brokers);
	setGroupId(group_id);

	// Construct the configuration
        this->config = {
                { "metadata.broker.list", this->brokers_list },
		{ "group.id", this->group_id },
		// Disable auto commit
		{ "enable.auto.commit", false }
        };

        // Create the consumer
        this->ptr_consumer = new Consumer(this->config);
}

MessageConsumer::~MessageConsumer(){
	delete ptr_consumer;
}

string MessageConsumer::getBrokers() const{
	return this->brokers_list;
}

string MessageConsumer::getLastTopicName() const{
	return this->topic_name;
}

string MessageConsumer::getGroupId() const{
	return this->group_id;
}

void MessageConsumer::setGroupId(string group_id){
	this->group_id = group_id;
}
 
bool MessageConsumer::consume(string topic, string** ptr_payload){
	if(this->ptr_consumer == nullptr){
		cout << "[MessageConsumer]: no consumer has been created" << endl;
		return false;
	}

	this->ptr_consumer->subscribe({topic});
	setLastTopicName(topic);

	Message message = this->ptr_consumer->poll();
	if(!message){
                cout << "[MessageConsumer]: consumer unable to poll messages from topic " << topic << endl;
		return false;
	}

	// If we managed to get a message
	if (message.get_error()){
		// Ignore EOF notifications from rdkafka
		if (!message.is_eof()) {
		      cout << "[+] Received error notification: " << message.get_error() << endl;
		      return false;
		}
		else{
		      cout << "message is eof" << endl;
		      ptr_payload = nullptr;
		      return false;
		}
        }

	// Print the key (if any) and payload
	if (message.get_key()) {
	    cout << message.get_key() << " -> ";
	}
	cout << "Received message : " << message.get_payload() << endl;

	*ptr_payload = new string(message.get_payload());
	this->ptr_consumer->commit(message);

	return true;
}

void MessageConsumer::setBrokers(string brokers){
        //TODO: validate string as IP address
        this->brokers_list = brokers;
}

void MessageConsumer::setLastTopicName(string topic){
	this->topic_name = topic;
}

} // meesagemanager
