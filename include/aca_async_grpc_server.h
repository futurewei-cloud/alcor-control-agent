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

#include <iostream>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include "goalstateprovisioner.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

class Aca_Async_GRPC_Server final {
  public:
  ~Aca_Async_GRPC_Server();
  Aca_Async_GRPC_Server();
  void Run();
  void StopServer();

  private:
  class CallData {
public:
    CallData(alcorcontroller::GoalStateProvisioner::AsyncService *service,
             ServerCompletionQueue *cq);
    void Proceed();

private:
    alcorcontroller::GoalStateProvisioner::AsyncService *service_;
    ServerCompletionQueue *cq_;
    ServerContext ctx_;
    alcorcontroller::GoalState request_;
    alcorcontroller::GoalStateOperationReply reply_;
    ServerAsyncResponseWriter<alcorcontroller::GoalStateOperationReply> responder_;

    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus status_;
  };

  void HandleRpcs();
  std::unique_ptr<ServerCompletionQueue> cq_;
  alcorcontroller::GoalStateProvisioner::AsyncService service_;
  std::unique_ptr<Server> server_;
};