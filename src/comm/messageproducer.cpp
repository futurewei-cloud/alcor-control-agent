// Copyright 2019 The Alcor Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <stdexcept>
#include "messageproducer.h"

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
