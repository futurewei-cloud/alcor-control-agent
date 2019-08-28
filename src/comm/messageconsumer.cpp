#include <iostream>
#include <stdexcept>
#include "messageconsumer.h"
#include "goalstate.pb.h"
#include "cppkafka/utils/consumer_dispatcher.h"
#include "aca_comm_mgr.h"
#include "aca_log.h"

extern bool g_debug_mode;
extern cppkafka::ConsumerDispatcher *dispatcher;

using aca_comm_manager::Aca_Comm_Manager;
using cppkafka::Configuration;
using cppkafka::Consumer;
using cppkafka::ConsumerDispatcher;
using cppkafka::Error;
using cppkafka::Message;
using cppkafka::TopicPartition;
using cppkafka::TopicPartitionList;
using std::cout;
using std::endl;
using std::exception;
using std::string;

namespace messagemanager
{

MessageConsumer::MessageConsumer(string brokers, string group_id)
{
	setBrokers(brokers);
	setGroupId(group_id);

	// Construct the configuration
	this->config = {
		{"metadata.broker.list", this->brokers_list},
		{"group.id", this->group_id},
		// Disable auto commit
		{"enable.auto.commit", false},
		{"auto.offset.reset", "earliest"}};

	if (g_debug_mode)
	{
		cout << "Broker list " << this->brokers_list << endl;
		cout << "Consumer group.id " << this->group_id << endl;
	}

	// Create the consumer
	this->ptr_consumer = new Consumer(this->config);
}

MessageConsumer::~MessageConsumer()
{
	delete ptr_consumer;
}

string MessageConsumer::getBrokers() const
{
	return this->brokers_list;
}

string MessageConsumer::getLastTopicName() const
{
	return this->topic_name;
}

string MessageConsumer::getGroupId() const
{
	return this->group_id;
}

void MessageConsumer::setGroupId(string group_id)
{
	this->group_id = group_id;
}

bool MessageConsumer::consumeDispatched(string topic)
{
	aliothcontroller::GoalState deserialized_GoalState;
	int rc = EXIT_FAILURE;

	this->ptr_consumer->subscribe({topic});

	if (g_debug_mode)
	{
		cout << "Dispatcher consuming messages from topic " << topic << endl;
	}

	// Create a consumer dispatcher
	dispatcher = new ConsumerDispatcher(*(this->ptr_consumer));

	// Now run the dispatcher, providing a callback to handle messages, one to handle
	// errors and another one to handle EOF on a partition
	dispatcher->run(
		// Callback executed whenever a new message is consumed
		[&](Message message) {
			// Print the key (if any)
			if (g_debug_mode)
			{
				if (message.get_key())
				{
					cout << message.get_key() << " -> ";
				}
				// Print the payload
				cout << endl << "<=====incoming message: " << message.get_payload() << endl;
			}

			rc = Aca_Comm_Manager::get_instance().deserialize(&(message.get_payload()), deserialized_GoalState);
			if (rc == EXIT_SUCCESS)
			{
				rc = Aca_Comm_Manager::get_instance().update_goal_state(deserialized_GoalState);
				if (rc != EXIT_SUCCESS)
				{
					ACA_LOG_ERROR("Failed to update transitd with latest goal state, rc=%d.\n", rc);
				}
				else
				{
					ACA_LOG_INFO("Successfully updated transitd with latest goal state %d.\n", rc);
				}
			}
			else
			{
				ACA_LOG_ERROR("Deserialization failed with error code %d.\n", rc);
			}

			// Now commit the message
			this->ptr_consumer->commit(message);
		},
		// Whenever there's an error (other than the EOF soft error)
		[](Error error) {
			if (g_debug_mode)
				cout << "[+] Received error notification: " << error << endl;
		},
		// Whenever EOF is reached on a partition, print this
		[](ConsumerDispatcher::EndOfFile, const TopicPartition &topic_partition) {
			if (g_debug_mode)
				cout << "Reached EOF on partition " << topic_partition << endl;
		});
}

void MessageConsumer::setBrokers(string brokers)
{
	//TODO: validate string as IP address
	this->brokers_list = brokers;
}

void MessageConsumer::setLastTopicName(string topic)
{
	this->topic_name = topic;
}

} // namespace messagemanager
