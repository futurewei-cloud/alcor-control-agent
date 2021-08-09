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
