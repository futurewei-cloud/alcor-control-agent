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

#ifndef KAFKA_PRODUCER_H
#define KAFKA_PRODUCER_H

#include "cppkafka/producer.h"
#include "cppkafka/configuration.h"

using namespace std;
using namespace cppkafka;

namespace messagemanager
{
class MessageProducer {
  private:
  string brokers_list; //IP addresses of Kafka brokers, format: <Kafka_host_ip>:<port>, example:10.213.43.188:9092

  string topic_name; //A string representation of the topic name to be published, example: /hostid/00000000-0000-0000-0000-000000000000/netwconf/

  int partition_value = -1; //Number of partitions used for a given topic, set by publisher

  Configuration config; //Configuration of a producer

  Producer *ptr_producer; //A pointer to the kafka producer

  public:
  MessageProducer(string brokers, string topic, int partition);

  ~MessageProducer();

  string getBrokers() const;

  string getTopicName() const;

  void setTopicName(string topic);

  int getPartitionValue() const;

  void setPartitionValue(int partition);

  void publish(string message);

  private:
  void setBrokers(string brokers);
};

} // namespace messagemanager

#endif
