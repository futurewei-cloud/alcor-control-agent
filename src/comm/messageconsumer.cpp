#include <iostream>
#include <stdexcept>
#include "messageconsumer.h"
#include "goalstate.pb.h"
#include "cppkafka/utils/consumer_dispatcher.h"
#include "aca_comm_mgr.h"
#include "aca_log.h"

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
using aca_comm_manager::Aca_Comm_Manager;

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
		{"auto.offset.reset", "latest"}};

	cout << "Broker list " << this->brokers_list << endl;
	cout << "group.id " << this->group_id << endl;

	// Create the consumer
	this->ptr_consumer = new Consumer(this->config);
	// this->ptr_consumer->set_assignment_callback([](const TopicPartitionList &partitions) {
	// 	cout << "Got assigned: " << partitions << endl;
	// });
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

bool MessageConsumer::consume(string topic, string **ptr_payload)
{
	if (this->ptr_consumer == nullptr)
	{
		cout << "[MessageConsumer]: no consumer has been created" << endl;
		return false;
	}

	this->ptr_consumer->subscribe({topic});
	cout << "Consuming messages from topic " << topic << endl;
	setLastTopicName(topic);

	Message message = this->ptr_consumer->poll();
	if (!message)
	{
		cout << "[MessageConsumer]: consumer unable to poll messages from topic " << topic << endl;
		return false;
	}

	// If we managed to get a message
	if (message.get_error())
	{
		// Ignore EOF notifications from rdkafka
		if (!message.is_eof())
		{
			cout << "[+] Received error notification: " << message.get_error() << endl;
			return false;
		}
		else
		{
			cout << "message is eof" << endl;
			ptr_payload = nullptr;
			return false;
		}
	}

	// Print the key (if any) and payload
	if (message.get_key())
	{
		cout << message.get_key() << " -> ";
	}
	cout << "Received message : " << message.get_payload() << endl;

	*ptr_payload = new string(message.get_payload());
	this->ptr_consumer->commit(message);

	return true;
}

bool MessageConsumer::cosumeDispatched(string topic)
{
	string *payload;
	aliothcontroller::GoalState deserialized_GoalState;
    int rc = EXIT_FAILURE;

	this->ptr_consumer->subscribe({topic});
	Aca_Comm_Manager comm_manager;

	cout << "Dispatcher consuming messages from topic " << topic << endl;

	// Create a consumer dispatcher
	ConsumerDispatcher dispatcher(*(this->ptr_consumer));

	// Now run the dispatcher, providing a callback to handle messages, one to handle
	// errors and another one to handle EOF on a partition
	dispatcher.run(
		// Callback executed whenever a new message is consumed
		[&](Message message) {
			// Print the key (if any)
			if (message.get_key())
			{
				cout << message.get_key() << " -> ";
			}
			// Print the payload
			cout << message.get_payload() << endl;
			payload = new string(message.get_payload());
			// TODO: Check string allocation for errors.
			rc = comm_manager.deserialize(*payload, deserialized_GoalState);
            if (rc == EXIT_SUCCESS)
            {
                // Call parse_goal_state
                rc = comm_manager.update_goal_state(deserialized_GoalState);
                if (rc != EXIT_SUCCESS)
                {
					cout << "Failed to update transitd with goal state" << rc << endl;
                    ACA_LOG_ERROR("Failed to update transitd with goal state %d.\n", rc);
                }
                else
                {
					cout << "Successfully updated transitd with goal state" << rc << endl;
                    ACA_LOG_ERROR("Successfully updated transitd with goal state %d.\n",
                                  rc);
                }
            }
			else
			{
				cout << "Deserialization failed with error code" << rc << endl;
				ACA_LOG_ERROR("Deserialization failed with error code %d.\n", rc);
			}
            if (payload != nullptr)
            {
                delete payload;
				payload = nullptr;
            }
			// Now commit the message
			this->ptr_consumer->commit(message);
		},
		// Whenever there's an error (other than the EOF soft error)
		[](Error error) {
			cout << "[+] Received error notification: " << error << endl;
		},
		// Whenever EOF is reached on a partition, print this
		[](ConsumerDispatcher::EndOfFile, const TopicPartition &topic_partition) {
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
