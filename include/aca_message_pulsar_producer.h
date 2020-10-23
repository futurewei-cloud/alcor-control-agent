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

#ifndef PULSAR_PRODUCER_H
#define PULSAR_PRODUCER_H

#include "pulsar/Client.h"
#include "pulsar/Producer.h"
#include "pulsar/ProducerConfiguration.h"
#include "pulsar/Message.h"
#include "pulsar/MessageBuilder.h"
#include "pulsar/Result.h"

using namespace std;
using namespace pulsar;

namespace aca_message_pulsar
{
class ACA_Message_Pulsar_Producer {
  private:
  string brokers_list; //IP addresses of Pulsar brokers, format: pulsar://<pulsar_host_ip>:<port>, example:pulsar://10.213.43.188:9092

  string topic_name; //A string representation of the topic name to be published, example: /hostid/00000000-0000-0000-0000-000000000000/netwconf/

  ClientConfiguration client_config; //Configuration of the pulsar client

  Client *ptr_client; //A pointer to the pulsar client

  public:
  ACA_Message_Pulsar_Producer(string brokers, string topic);

  ~ACA_Message_Pulsar_Producer();

  string getBrokers() const;

  string getTopicName() const;

  void setTopicName(string topic);

  bool publish(string message);

  private:
  void setBrokers(string brokers);
};

} // aca_message_pulsar

#endif