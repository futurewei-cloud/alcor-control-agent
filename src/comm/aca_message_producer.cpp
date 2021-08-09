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

#include <iostream>
#include <stdexcept>
#include "aca_message_producer.h"

using namespace std;
using namespace cppkafka;

namespace messagemanager
{
MessageProducer::MessageProducer(string brokers, string topic, int partition)
{
  setBrokers(brokers);
  setTopicName(topic);
  setPartitionValue(partition);

  // Construct the configuration
  this->config = { { "metadata.broker.list", this->brokers_list } };

  // Create the producer
  this->ptr_producer = new Producer(this->config);
}

MessageProducer::~MessageProducer()
{
  delete ptr_producer;
}

string MessageProducer::getBrokers() const
{
  return this->brokers_list;
}

string MessageProducer::getTopicName() const
{
  return this->topic_name;
}

void MessageProducer::setTopicName(string topic)
{
  this->topic_name = topic;
}

int MessageProducer::getPartitionValue() const
{
  return this->partition_value;
}

void MessageProducer::setPartitionValue(int partition)
{
  if (partition < -1) {
    throw invalid_argument("Negative partition value");
  }

  this->partition_value = partition;
}

void MessageProducer::publish(string message)
{
  // Create a message builder for this topic
  MessageBuilder builder(this->topic_name);

  // Get the partition. If no partition is provided, this will be an unassigned one
  if (partition_value != -1) {
    builder.partition(this->partition_value);
  }

  // Set the payload on this builder
  builder.payload(message);

  // Actually produce the message
  this->ptr_producer->produce(builder);

  // Flush all produced messages
  this->ptr_producer->flush();
}

void MessageProducer::setBrokers(string brokers)
{
  //TODO: validate string as IP address
  this->brokers_list = brokers;
}

} // namespace messagemanager
