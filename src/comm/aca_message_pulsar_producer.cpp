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
#include "aca_message_pulsar_producer.h"
#include "aca_log.h"


using namespace std;
using namespace pulsar;

namespace aca_message_pulsar
ACA_Message_Pulsar_Producer::ACA_Message_Pulsar_Producer(string brokers, string topic)
{
  setBrokers(brokers);
  setTopicName(topic);

  // Create client
  this->ptr_client= new Client(brokers,this->client_config);
}

ACA_Message_Pulsar_Producer::~ACA_Message_Pulsar_Producer()
{
  delete this->ptr_client;
}

string ACA_Message_Pulsar_Producer::getBrokers() const
{
  return this->brokers_list;
}

string ACA_Message_Pulsar_Producer::getTopicName() const
{
  return this->topic_name;
}

void ACA_Message_Pulsar_Producer::setTopicName(string topic)
{
  this->topic_name = topic;
}


bool ACA_Message_Pulsar_Producer::publish(string message)
{
  Result result;

  // Create a producer
  Producer producer;
  result = this->ptr_client.createProducer(this->topic,producer);
  if(result != ResultOk){
    ACA_LOG_ERROR("Failed to create producer, result=%d.\n", result);
    return EXIT_FAILURE;
  }

  // Create a message
  Message msg = MessageBuilder().setContent(message).build();
  result = producer.send(msg);
  if(result != ResultOk){
    ACA_LOG_ERROR("Failed to send message %s.\n", message.c_str());
    return EXIT_FAILURE;
  }

  ACA_LOG_INFO("Successfully send message %s\n", message.c_str());

  // Flush all produced messages
  producer.flush();
  return EXIT_SUCCESS;

}

void ACA_Message_Pulsar_Producer::setBrokers(string brokers)
{
  //TODO: validate string as IP address
  this->brokers_list = brokers;
}

} // namespace aca_message_pulsar