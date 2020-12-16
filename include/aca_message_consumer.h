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

#ifndef KAFKA_CONSUMER_H
#define KAFKA_CONSUMER_H

#include "cppkafka/consumer.h"
#include "cppkafka/configuration.h"

using cppkafka::Configuration;
using cppkafka::Consumer;
using std::string;

namespace messagemanager
{
class MessageConsumer {
  private:
  string brokers_list; //IP addresses of Kafka brokers, format: <Kafka_host_ip>:<port>, example:10.213.43.188:9092

  string group_id; //Group id of the Kafka consumer

  string topic_name; //A string representation of the topic to be consumed, for example: /hostid/00000000-0000-0000-0000-000000000000/netwconf/

  Configuration config; //Configuration of the Kafka consumer

  Consumer *ptr_consumer; //A pointer to the Kafka consumer

  public:
  MessageConsumer(string brokers, string group_id);

  ~MessageConsumer();

  string getBrokers() const;

  string getLastTopicName() const;

  string getGroupId() const;

  void setGroupId(string group_id);

  bool consumeDispatched(string topic);

  private:
  void setBrokers(string brokers);

  void setLastTopicName(string topic);
};

} // namespace messagemanager

#endif
