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

#include "aca_message_pulsar_consumer.h"
#include "goalstate.pb.h"
#include "aca_comm_mgr.h"
#include "aca_log.h"

using aca_comm_manager::Aca_Comm_Manager;
using pulsar::Client;
using pulsar::ClientConfiguration;
using pulsar::ConsumerConfiguration;
using pulsar::Consumer;
using pulsar::Message;
using pulsar::Result;


namespace aca_message_pulsar
{
ACA_Message_Pulsar_Consumer::ACA_Message_Pulsar_Consumer(string brokers, string subscription_name)
{
  setBrokers(brokers);
  setSubscriptionName(subscription_name);

  // TO DO: Construct the configuration

  ACA_LOG_DEBUG("Broker list: %s\n", this->brokers_list.c_str());
  ACA_LOG_DEBUG("Consumer subscription name: %s\n", this->subscription_name.c_str());

  // Create the consumer and client
  this->ptr_consumer = new Consumer();
  this->ptr_client= new Client(brokers,this->client_config);
}

ACA_Message_Pulsar_Consumer::~ACA_Message_Pulsar_Consumer()
{
  //delete ptr_consumer;
  delete ptr_client;
}

string ACA_Message_Pulsar_Consumer::getBrokers() const
{
  return this->brokers_list;
}

string ACA_Message_Pulsar_Consumer::getLastTopicName() const
{
  return this->topic_name;
}

string ACA_Message_Pulsar_Consumer::getSubscriptionName() const
{
  return this->subscription_name;
}

void ACA_Message_Pulsar_Consumer::setSubscriptionName(string subscription_name)
{
  this->subscription_name = subscription_name;
}

bool ACA_Message_Pulsar_Consumer::consumeDispatched(string topic)
{
  alcor::schema::GoalState deserialized_GoalState;
  alcor::schema::GoalStateOperationReply gsOperationalReply;
  int rc;
  int overall_rc = EXIT_SUCCESS;
  Result result;
  Message message;

  result = this->ptr_client->subscribe(topic,this->subscription_name,this->consumer_config,*(this->ptr_consumer));

  if (result != Result::ResultOk){
    ACA_LOG_ERROR("Failed to subscribe topic: %s\n", topic.c_str());
    return EXIT_FAILURE;
  }

  ACA_LOG_DEBUG("Consumer consuming messages from topic: %s\n", topic.c_str());

  
  //Receive message
  result = this->ptr_consumer->receive(message);

  if (result != Result::ResultOk) {
    ACA_LOG_ERROR("Failed to receive message from topic: %s\n",topic.c_str());
    return EXIT_FAILURE;
  }

  else{
    // Print the ordering key (if any)
    if (message.hasOrderingKey()) {
      ACA_LOG_DEBUG("%s  -> ", message.getOrderingKey().c_str());
    }
    // Print the payload
    ACA_LOG_DEBUG("\n<=====incoming message: %s\n",
                  message.getDataAsString().c_str());

    rc = Aca_Comm_Manager::get_instance().deserialize(
            (unsigned char *)message.getData(),message.getLength(), deserialized_GoalState);
    if (rc == EXIT_SUCCESS) {
      rc = Aca_Comm_Manager::get_instance().update_goal_state(
                  deserialized_GoalState, gsOperationalReply);

    // TODO: send gsOperationalReply back to controller 

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

    // Now acknowledge message
    this->ptr_consumer->acknowledge(message);
  }
  return overall_rc;
}

void ACA_Message_Pulsar_Consumer::setBrokers(string brokers)
{
  //TODO: validate string as IP address
  this->brokers_list = brokers;
}

void ACA_Message_Pulsar_Consumer::setLastTopicName(string topic)
{
  this->topic_name = topic;
}

} // namespace aca_message_pulsar
