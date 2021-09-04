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

#include "aca_message_pulsar_consumer.h"
#include "goalstate.pb.h"
#include "aca_comm_mgr.h"
#include "aca_log.h"


using aca_comm_manager::Aca_Comm_Manager;
using pulsar::Client;
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

  ACA_LOG_DEBUG("Broker list: %s\n", this->brokers_list.c_str());
  ACA_LOG_DEBUG("Consumer subscription name: %s\n", this->subscription_name.c_str());

  // Create the client
  this->ptr_client= new Client(brokers);
}

ACA_Message_Pulsar_Consumer::~ACA_Message_Pulsar_Consumer()
{
  delete this->ptr_client;
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


// void listener(Consumer consumer, const Message& message){
//   alcor::schema::GoalState deserialized_GoalState;
//   alcor::schema::GoalStateOperationReply gsOperationalReply;
//   int rc;
//   int overall_rc = EXIT_SUCCESS;
//   Result result;
//   Message message;
//   Consumer consumer;

//   ACA_LOG_DEBUG("\n<=====incoming message: %s\n",
//                 message.getDataAsString().c_str());

//   rc = Aca_Comm_Manager::get_instance().deserialize(
//           (unsigned char *)message.getData(), message.getLength(), deserialized_GoalState);
//   if (rc == EXIT_SUCCESS) {
//     rc = Aca_Comm_Manager::get_instance().update_goal_state(
//                 deserialized_GoalState, gsOperationalReply);

//   // TODO: send gsOperationalReply back to controller 

//     if (rc != EXIT_SUCCESS) {
//       ACA_LOG_ERROR("Failed to update host with latest goal state, rc=%d.\n", rc);
//       overall_rc = rc;
//     } else {
//       ACA_LOG_INFO("Successfully updated host with latest goal state %d.\n", rc);
//     }

//   } else {
//     ACA_LOG_ERROR("Deserialization failed with error code %d.\n", rc);
//     overall_rc = rc;
//   }

//   // Now acknowledge message
//   consumer.acknowledge(message.getMessageId());
// }


bool ACA_Message_Pulsar_Consumer::consumeDispatched(string topic)
{
  alcor::schema::GoalState deserialized_GoalState;
  alcor::schema::GoalStateOperationReply gsOperationalReply;
  int rc;
  int overall_rc = EXIT_SUCCESS;
  Result result;
  Message message;
  Consumer consumer;
  //this->consumer_config.setConsumerType(ConsumerKeyShared).setKeySharedPolicy(KeySharedPolicy).setMessageListener(messageListener)
  result = this->ptr_client->subscribe(topic,this->subscription_name,this->consumer_config,consumer);

  if (result != Result::ResultOk){
    ACA_LOG_ERROR("Failed to subscribe topic: %s\n", topic.c_str());
    return EXIT_FAILURE;
  }

  ACA_LOG_DEBUG("Consumer consuming messages from topic: %s\n", topic.c_str());

  //Receive message
  while(true){
    result = consumer.receive(message);

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
              (unsigned char *)message.getData(), message.getLength(), deserialized_GoalState);
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
      consumer.acknowledge(message);
    }
  }
  return overall_rc;
}

void ACA_Message_Pulsar_Consumer::setBrokers(string brokers)
{
  this->brokers_list = brokers;
}

void ACA_Message_Pulsar_Consumer::setLastTopicName(string topic)
{
  this->topic_name = topic;
}

} // namespace aca_message_pulsar
