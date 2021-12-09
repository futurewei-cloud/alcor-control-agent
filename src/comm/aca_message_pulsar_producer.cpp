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

#include <iostream>
#include <stdexcept>
#include "aca_message_pulsar_producer.h"
#include "aca_log.h"


using namespace std;
using namespace pulsar;

namespace aca_message_pulsar
{
ACA_Message_Pulsar_Producer::ACA_Message_Pulsar_Producer(string brokers, string topic)
{
  setBrokers(brokers);
  setTopicName(topic);

  // Create client
  this->ptr_client= new Client(brokers,this->client_config);
}

ACA_Message_Pulsar_Producer::~ACA_Message_Pulsar_Producer()
{
  delete this->ptr_client;
}

string ACA_Message_Pulsar_Producer::getBrokers() const
{
  return this->brokers_list;
}

string ACA_Message_Pulsar_Producer::getTopicName() const
{
  return this->topic_name;
}

void ACA_Message_Pulsar_Producer::setTopicName(string topic)
{
  this->topic_name = topic;
}


bool ACA_Message_Pulsar_Producer::publish(string message)
{
  Result result;

  // Create a producer
  Producer producer;
  result = this->ptr_client->createProducer(this->topic_name,producer);
  if(result != ResultOk){
    ACA_LOG_ERROR("Failed to create producer, result=%d.\n", result);
    return EXIT_FAILURE;
  }

  // Create a message
  Message msg = MessageBuilder().setContent(message).build();
  result = producer.send(msg);
  if(result != ResultOk){
    ACA_LOG_ERROR("Failed to send message %s.\n", message.c_str());
    return EXIT_FAILURE;
  }

  ACA_LOG_INFO("Successfully send message %s\n", message.c_str());

  // Flush all produced messages
  producer.flush();
  producer.close();
  return EXIT_SUCCESS;

}

bool ACA_Message_Pulsar_Producer::publish(string message, string orderingKey)
{
    Result result;

    // Create a producer
    Producer producer;
    result = this->ptr_client->createProducer(this->topic_name,producer);
    if(result != ResultOk){
        ACA_LOG_ERROR("Failed to create producer, result=%d.\n", result);
        return EXIT_FAILURE;
    }

    // Create a message
    Message msg = MessageBuilder().setContent(message).setOrderingKey(orderingKey).build();
    result = producer.send(msg);
    if(result != ResultOk){
        ACA_LOG_ERROR("Failed to send message %s.\n", message.c_str());
        return EXIT_FAILURE;
    }

    ACA_LOG_INFO("Successfully send message %s\n", message.c_str());

    // Flush all produced messages
    producer.flush();
    producer.close();
    return EXIT_SUCCESS;

}
void ACA_Message_Pulsar_Producer::setBrokers(string brokers)
{
  //TODO: validate string as IP address
  this->brokers_list = brokers;
}

} // namespace aca_message_pulsar
