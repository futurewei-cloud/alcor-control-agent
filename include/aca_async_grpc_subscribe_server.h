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

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include "subscribeinfoprovisioner.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

class Aca_Async_GRPC_Subscribe_Server final {
  public:
  ~Aca_Async_GRPC_Subscribe_Server();
  Aca_Async_GRPC_Subscribe_Server();
  void Run();
  void StopServer();

  private:
  class CallData {
public:
    CallData(alcor::schema::SubscribeInfoProvisioner::AsyncService *service,
             ServerCompletionQueue *cq);
    void Proceed();

private:
    alcor::schema::SubscribeInfoProvisioner::AsyncService *service_;
    ServerCompletionQueue *cq_;
    ServerContext ctx_;
    alcor::schema::GoalState request_;
    alcor::schema::GoalStateOperationReply reply_;
    ServerAsyncResponseWriter<alcor::schema::GoalStateOperationReply> responder_;

    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus status_;
  };

  void HandleRpcs();
  std::unique_ptr<ServerCompletionQueue> cq_;
  alcor::schema::GoalStateProvisioner::AsyncService service_;
  std::unique_ptr<Server> server_;
};