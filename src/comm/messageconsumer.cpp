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

bool MessageConsumer::cosumeDispatched(string topic)
{
	string *payload;
	aliothcontroller::GoalState deserialized_GoalState;
    int rc = EXIT_FAILURE;

	this->ptr_consumer->subscribe({topic});

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
			rc = Aca_Comm_Manager::get_instance().deserialize(*payload, deserialized_GoalState);
            if (rc == EXIT_SUCCESS)
            {
                // Call parse_goal_state
                rc = Aca_Comm_Manager::get_instance().update_goal_state(deserialized_GoalState);
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
