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

  bool publish(string message, string orderingKey);


  private:
  void setBrokers(string brokers);

};

} // aca_message_pulsar

#endif
