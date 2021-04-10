// MIT License
// Copyright(c) 2020 Futurewei Cloud
//
//     Permission is hereby granted,
//     free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction,
//     including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons
//     to whom the Software is furnished to do so, subject to the following conditions:
//
//     The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
//     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//     WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "aca_message_consumer.h"
#include "goalstate.pb.h"
#include "cppkafka/utils/consumer_dispatcher.h"
#include "aca_comm_mgr.h"
#include "aca_log.h"

extern cppkafka::ConsumerDispatcher *dispatcher;

using aca_comm_manager::Aca_Comm_Manager;
using cppkafka::Configuration;
using cppkafka::Consumer;
using cppkafka::ConsumerDispatcher;
using cppkafka::Error;
using cppkafka::Message;
using cppkafka::TopicPartition;
using cppkafka::TopicPartitionList;

namespace messagemanager
{
MessageConsumer::MessageConsumer(string brokers, string group_id)
{
  setBrokers(brokers);
  setGroupId(group_id);

  // Construct the configuration
  this->config = { { "metadata.broker.list", this->brokers_list },
                   { "group.id", this->group_id },
                   // Disable auto commit
                   { "enable.auto.commit", false },
                   { "auto.offset.reset", "earliest" } };

  ACA_LOG_DEBUG("Broker list: %s\n", this->brokers_list.c_str());
  ACA_LOG_DEBUG("Consumer group.id: %s\n", this->group_id.c_str());

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
  alcor::schema::GoalState deserialized_GoalState;
  alcor::schema::GoalStateOperationReply gsOperationalReply;
  int rc;
  int overall_rc = EXIT_SUCCESS;

  this->ptr_consumer->subscribe({ topic });

  ACA_LOG_DEBUG("Dispatcher consuming messages from topic: %s\n", topic.c_str());

  // Create a consumer dispatcher
  dispatcher = new ConsumerDispatcher(*(this->ptr_consumer));

  // Now run the dispatcher, providing a callback to handle messages, one to handle
  // errors and another one to handle EOF on a partition
  dispatcher->run(
          // Callback executed whenever a new message is consumed
          [&](Message message) {
            // Print the key (if any)
            if (message.get_key()) {
              ACA_LOG_DEBUG("%s  -> ", string(message.get_key()).c_str());
            }
            // Print the payload
            ACA_LOG_DEBUG("\n<=====incoming message: %s\n",
                          string(message.get_payload()).c_str());

            rc = Aca_Comm_Manager::get_instance().deserialize(
                    message.get_payload().get_data(), message.get_payload().get_size(), deserialized_GoalState);
            if (rc == EXIT_SUCCESS) {
              rc = Aca_Comm_Manager::get_instance().update_goal_state(
                      deserialized_GoalState, gsOperationalReply);

              // TODO: send gsOperationalReply back to controller through Kafka

              if (rc != EXIT_SUCCESS) {
                ACA_LOG_ERROR("Failed to update host with latest goal state, rc=%d.\n", rc);
                overall_rc = rc;
              } else {
                ACA_LOG_INFO("Successfully updated host with latest goal state %d.\n", rc);
              }
            } else {
              ACA_LOG_ERROR("Deserialization failed with error code %d.\n", rc);
              overall_rc = rc;
            }

            // Now commit the message
            this->ptr_consumer->commit(message);
          },
          // Whenever there's an error (other than the EOF soft error)
          [](Error error) {
            ACA_LOG_ERROR("[+] Received error notification: %s\n",
                          error.to_string().c_str());
          },
          // Whenever EOF is reached on a partition, print this
          [](ConsumerDispatcher::EndOfFile, const TopicPartition &topic_partition) {
            ACA_LOG_DEBUG("Reached EOF on partition: %s\n",
                          topic_partition.get_topic().c_str());
          });

  return overall_rc;
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
