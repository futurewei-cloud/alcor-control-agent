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

#ifndef PULSAR_CONSUMER_H
#define PULSAR_CONSUMER_H

#include "pulsar/Client.h"
#include "pulsar/Consumer.h"
#include "pulsar/ConsumerConfiguration.h"
#include "pulsar/Message.h"
#include "pulsar/Result.h"
#include "pulsar/ConsumerType.h"
#include "pulsar/KeySharedPolicy.h"


using namespace pulsar;
using std::string;

namespace aca_message_pulsar
{
class ACA_Message_Pulsar_Consumer {
  private:
    string brokers_list; // IP addresses of pulsar brokers, format: pulsar:://<pulsar_host_ip>:<port>, example: pulsar://10.213.43.188:9092

    string multicast_subscription_name; // Subscription name of the multicast pulsar consumer
    string unicast_subscription_name; // Subscription name of the unicast pulsar consumer

    string multicast_topic_name; //A string representation of the topic to be consumed, for example: /hostid/00000000-0000-0000-0000-000000000000/netwconf/
    string unicast_topic_name; 

    ConsumerConfiguration multicast_consumer_config; //Configuration of the mulitcast pulsar consumer
    ConsumerConfiguration unicast_consumer_config; //Configuration of the unicast pulsar consumer

    Client *ptr_multicast_client; //A pointer to the multicast pulsar client
    Client *ptr_unicast_client; //A pointer to the unicast pulsar client

    Consumer multicast_consumer;
    Consumer unicast_consumer;

    static string empty_topic;

  private:
    void setMulticastSubscriptionName(string subscription_name);

    void setUnicastSubscriptionName(string subscription_name);

    void setBrokers(string brokers);

    void setMulticastTopicName(string topic);

    void setUnicastTopicName(string topic);


  public:
      static string recovered_topic;

      static ACA_Message_Pulsar_Consumer &get_instance();

      ACA_Message_Pulsar_Consumer();

      ACA_Message_Pulsar_Consumer(string topic, string brokers, string subscription_name);

      ~ACA_Message_Pulsar_Consumer();

      void init(string topic, string brokers, string subscription_name);

    string getBrokers() const;

    string getMulticastTopicName() const;

    string getUnicastTopicName() const;

    string getMulticastSubscriptionName() const;

    string getUnicastSubscriptionName() const;

    static string getRecoveredTopicName();

    bool multicastConsumerDispatched();

    bool unicastConsumerDispatched(int stickyHash);

    bool unicastUnsubcribe();

    bool unicastResubscribe(string topic, int stickyHash);

    bool unicastResubscribe(bool isSubscribe, string topic="", string stickHash="");
};

} // namespace aca_message_pulsar
#endif
