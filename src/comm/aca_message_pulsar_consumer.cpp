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
using pulsar::KeySharedPolicy;
using pulsar::StickyRange;


namespace aca_message_pulsar
{
string aca_message_pulsar::ACA_Message_Pulsar_Consumer::empty_topic="";

string aca_message_pulsar::ACA_Message_Pulsar_Consumer::recovered_topic=
        ACA_Message_Pulsar_Consumer::getRecoveredTopicName();

void listener(Consumer consumer, const Message& message){
  alcor::schema::GoalStateV2 deserialized_GoalState;
  alcor::schema::GoalStateOperationReply gsOperationalReply;
  int rc;
  Result result;

  ACA_LOG_DEBUG("\n<=====incoming message: %s\n",
                message.getDataAsString().c_str());

  rc = Aca_Comm_Manager::get_instance().deserialize(
          (unsigned char *)message.getData(), message.getLength(), deserialized_GoalState);
  if (rc == EXIT_SUCCESS) {
    rc = Aca_Comm_Manager::get_instance().update_goal_state(
                deserialized_GoalState, gsOperationalReply);


    if (rc != EXIT_SUCCESS) {
      ACA_LOG_ERROR("Failed to update host with latest goal state, rc=%d.\n", rc);
    } else {
      ACA_LOG_INFO("Successfully updated host with latest goal state %d.\n", rc);
    }

  } else {
    ACA_LOG_ERROR("Deserialization failed with error code %d.\n", rc);
  }

  // Now acknowledge message
  consumer.acknowledge(message.getMessageId());
}

ACA_Message_Pulsar_Consumer::ACA_Message_Pulsar_Consumer()
{
    string default_brokers = "pulsar://localhost:6650";
    string default_topic = "Host-ts-1";
    string default_subscription_name = "test_subscription";

    setUnicastTopicName(default_topic);
    recovered_topic=default_topic;
    setMulticastTopicName(default_topic);
    setBrokers(default_brokers);
    setUnicastSubscriptionName(default_subscription_name);
    setMulticastSubscriptionName(default_subscription_name);

    ACA_LOG_DEBUG("Broker list: %s\n", this->brokers_list.c_str());
    ACA_LOG_DEBUG("Unicast consumer topic name: %s\n", this->unicast_topic_name.c_str());
    ACA_LOG_DEBUG("Unicast consumer subscription name: %s\n", this->unicast_subscription_name.c_str());
    ACA_LOG_DEBUG("Multicast consumer topic name: %s\n", this->multicast_topic_name.c_str());
    ACA_LOG_DEBUG("Multicast consumer subscription name: %s\n", this->multicast_subscription_name.c_str());

    // Create the clients
    this->ptr_multicast_client= new Client(default_brokers);
    this->ptr_unicast_client = new Client(default_brokers);
}

ACA_Message_Pulsar_Consumer &ACA_Message_Pulsar_Consumer::get_instance()
{
    static ACA_Message_Pulsar_Consumer instance;
    return instance;
}
void ACA_Message_Pulsar_Consumer::init(string topic, string brokers, string subscription_name){
    setUnicastTopicName(topic);
    recovered_topic=topic;
    setMulticastTopicName(topic);
    setBrokers(brokers);
    setUnicastSubscriptionName(subscription_name);
    setMulticastSubscriptionName(subscription_name);

    ACA_LOG_DEBUG("Broker list: %s\n", this->brokers_list.c_str());
    ACA_LOG_DEBUG("Unicast consumer topic name: %s\n", this->unicast_topic_name.c_str());
    ACA_LOG_DEBUG("Unicast consumer subscription name: %s\n", this->unicast_subscription_name.c_str());
    ACA_LOG_DEBUG("Multicast consumer topic name: %s\n", this->multicast_topic_name.c_str());
    ACA_LOG_DEBUG("Multicast consumer subscription name: %s\n", this->multicast_subscription_name.c_str());

    // Create the clients
    this->ptr_multicast_client= new Client(brokers);
    this->ptr_unicast_client = new Client(brokers);
}
ACA_Message_Pulsar_Consumer::ACA_Message_Pulsar_Consumer(string topic, string brokers, string subscription_name)
{
  setUnicastTopicName(topic);
  recovered_topic=topic;
  setMulticastTopicName(topic);
  setBrokers(brokers);
  setUnicastSubscriptionName(subscription_name);
  setMulticastSubscriptionName(subscription_name);

  ACA_LOG_DEBUG("Broker list: %s\n", this->brokers_list.c_str());
  ACA_LOG_DEBUG("Unicast consumer topic name: %s\n", this->unicast_topic_name.c_str());
  ACA_LOG_DEBUG("Unicast consumer subscription name: %s\n", this->unicast_subscription_name.c_str());
  ACA_LOG_DEBUG("Multicast consumer topic name: %s\n", this->multicast_topic_name.c_str());
  ACA_LOG_DEBUG("Multicast consumer subscription name: %s\n", this->multicast_subscription_name.c_str());

  // Create the clients
  this->ptr_multicast_client= new Client(brokers);
  this->ptr_unicast_client = new Client(brokers);
}

ACA_Message_Pulsar_Consumer::~ACA_Message_Pulsar_Consumer()
{
  delete this->ptr_multicast_client;
  delete this->ptr_unicast_client;
}

string ACA_Message_Pulsar_Consumer::getBrokers() const
{
  return this->brokers_list;
}

string ACA_Message_Pulsar_Consumer::getMulticastTopicName() const
{
  return this->multicast_topic_name;
}

string ACA_Message_Pulsar_Consumer::getMulticastSubscriptionName() const
{
  return this->multicast_subscription_name;
}

string ACA_Message_Pulsar_Consumer::getUnicastTopicName() const
{
  return this->unicast_topic_name;
}

string ACA_Message_Pulsar_Consumer::getUnicastSubscriptionName() const
{
  return this->unicast_subscription_name;
}

string ACA_Message_Pulsar_Consumer::getRecoveredTopicName()
{
    return "recovered topic test";
}

bool ACA_Message_Pulsar_Consumer::unicastConsumerDispatched(int stickyHash){
  Result result;
  Consumer consumer;
  KeySharedPolicy keySharedPolicy;

  keySharedPolicy.setKeySharedMode(STICKY);
  // Set sticky ranges with specified hash value

  StickyRange stickyRange = std::make_pair(stickyHash,stickyHash);
  keySharedPolicy.setStickyRanges({stickyRange});

  //Use key shared mode
  this->unicast_consumer_config.setConsumerType(ConsumerKeyShared).setKeySharedPolicy(keySharedPolicy).setMessageListener(listener);
  ACA_LOG_INFO("%s\n",this->unicast_topic_name.c_str());
  result = this->ptr_unicast_client->subscribe(this->unicast_topic_name,this->unicast_subscription_name,this->unicast_consumer_config,this->unicast_consumer);
  if (result != Result::ResultOk){
    ACA_LOG_ERROR("Failed to subscribe unicast topic: %s\n", this->unicast_topic_name.c_str());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

bool ACA_Message_Pulsar_Consumer::multicastConsumerDispatched(){
  Result result;

  // Use the default exclusive mode
  this->multicast_consumer_config.setMessageListener(listener);
  result = this->ptr_multicast_client->subscribe(this->multicast_topic_name,this->multicast_subscription_name,this->multicast_consumer_config,this->multicast_consumer);
  if (result != Result::ResultOk){
    ACA_LOG_ERROR("Failed to subscribe multicast topic: %s\n", this->multicast_topic_name.c_str());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

bool ACA_Message_Pulsar_Consumer::unicastUnsubcribe()
{
    Result result;
    if(this->unicast_topic_name==empty_topic){
        ACA_LOG_INFO("The consumer already unsubscribe the unicast topic.");
        return EXIT_SUCCESS;
    }

    result=this->unicast_consumer.unsubscribe();
    if (result != Result::ResultOk){
        ACA_LOG_ERROR("Failed to unsubscribe unicast topic: %s\n", this->unicast_topic_name.c_str());
        return EXIT_FAILURE;
    }
    this->unicast_topic_name=empty_topic;
    return EXIT_SUCCESS;
}

bool ACA_Message_Pulsar_Consumer::unicastResubscribe(string topic, int stickyHash)
{
    bool result;

    result = unicastUnsubcribe();

    if (result==EXIT_SUCCESS){
        setUnicastTopicName(topic);
        recovered_topic=topic;
        result = unicastConsumerDispatched(stickyHash);
        if (result==EXIT_SUCCESS) {
            return EXIT_SUCCESS;
        }
    }
    ACA_LOG_ERROR("Failed to resubscribe unicast topic: %s\n", topic.c_str());
    return EXIT_FAILURE;
}


bool ACA_Message_Pulsar_Consumer::unicastResubscribe(bool isSubscribe, string topic, string stickHash)
{
    if(!isSubscribe){
        return unicastUnsubcribe();
    }
    else{
        return unicastResubscribe(topic, std::stoi(stickHash));
    }
}

void ACA_Message_Pulsar_Consumer::setBrokers(string brokers)
{
  this->brokers_list = brokers;
}

void ACA_Message_Pulsar_Consumer::setMulticastTopicName(string topic)
{
  this->multicast_topic_name = topic;
}

void ACA_Message_Pulsar_Consumer::setMulticastSubscriptionName(string subscription_name)
{
  this->multicast_subscription_name = subscription_name;
}

void ACA_Message_Pulsar_Consumer::setUnicastTopicName(string topic)
{
  this->unicast_topic_name = topic;
}

void ACA_Message_Pulsar_Consumer::setUnicastSubscriptionName(string subscription_name)
{
  this->unicast_subscription_name = subscription_name;
}


} // namespace aca_message_pulsar
