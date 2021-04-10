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


using namespace pulsar;
using std::string;

namespace aca_message_pulsar
{
class ACA_Message_Pulsar_Consumer {
  private:
    string brokers_list; //IP addresses of pulsar brokers, format: pulsar:://<pulsar_host_ip>:<port>, example: pulsar://10.213.43.188:9092

    string subscription_name; //Subscription name of the pulsar consumer

    string topic_name; //A string representation of the topic to be consumed, for example: /hostid/00000000-0000-0000-0000-000000000000/netwconf/

    ConsumerConfiguration consumer_config; //Configuration of the pulsar consumer

    Client *ptr_client; //A pointer to the pulsar client

  public:
    ACA_Message_Pulsar_Consumer(string brokers, string subscription_name);

    ~ACA_Message_Pulsar_Consumer();

    string getBrokers() const;

    string getLastTopicName() const;

    string getSubscriptionName() const;

    void setSubscriptionName(string subscription_name);

    bool consumeDispatched(string topic);

  private:
    void setBrokers(string brokers);

    void setLastTopicName(string topic);
};

} // namespace aca_message_pulsar
#endif
