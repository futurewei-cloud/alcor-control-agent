// Copyright 2029 The Alcor Authors.
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

#ifndef PULSAR_CONSUMER_H
#define PULSAR_CONSUMER_H

#include "pulsar/Client.h"
#include "pulsar/Consumer.h"
#include "pulsar/ClientConfiguration.h"
#include "pulsar/ConsumerConfiguration.h"
#include "pulsar/Message.h"
#include "pulsar/Result.h"
#include "pulsar/MessageBuilder.h"

using namespace pulsar;
using std::string;

namespace aca_message_pulsar
{
class ACA_Message_Pulsar_Consumer {
  private:
    string brokers_list; //IP addresses of pulsar brokers, format: pulsar:://<pulsar_host_ip>:<port>, example: pulsar://10.213.43.188:9092

    string subscription_name; //Subscription name of the pulsar consumer

    string topic_name; //A string representation of the topic to be consumed, for example: /hostid/00000000-0000-0000-0000-000000000000/netwconf/

    ClientConfiguration client_config; //Configuration of the pulsar client

    ConsumerConfiguration consumer_config; //Configuration of the pulsar consumer

    Consumer *ptr_consumer; //A pointer to the pulsar consumer

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
