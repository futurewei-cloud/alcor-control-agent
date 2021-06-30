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
#include "goalstateprovisioner.grpc.pb.h"
#include "ctpl/ctpl_stl.h"

using namespace alcor::schema;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerAsyncReader;
using grpc::ServerAsyncReaderWriter;
using grpc::ServerCompletionQueue;
using grpc::ServerWriter;
using grpc::Status;

class GoalStateProvisionerImpl final : public GoalStateProvisioner::Service {
  public:
  std::unique_ptr<GoalStateProvisioner::Stub> stub_;
  std::shared_ptr<grpc_impl::Channel> chan_;
  std::unique_ptr<ServerCompletionQueue> cq_;
  ctpl::thread_pool thread_pool;

  /* This thread is responsible for dispatching GoalStatesV2 to the thread pool */
  // std::thread *dispatcher;
  // std::unique_ptr<ServerCompletionQueue> cq_;
  // ctpl::thread_pool thread_pool;
  // GoalStateProvisioner::AsyncService service_;

  void RequestGoalStates(HostRequest *request, grpc::CompletionQueue *cq);

  // ~GoalStateProvisionerImpl();
  explicit GoalStateProvisionerImpl(){};
  Status PushNetworkResourceStates(ServerContext *context, const GoalState *goalState,
                                   GoalStateOperationReply *goalStateOperationReply) override;

  // Status
  // PushGoalStatesStream(ServerContext *context,
  //                      ServerReaderWriter<GoalStateOperationReply, GoalStateV2> *stream) override;
  Status ShutDownServer();

  void RunServer();

  void ConnectToNCM();

  private:
  std::unique_ptr<Server> server;
};

class GoalStateProvisionerAsyncImpl final : public GoalStateProvisioner::AsyncService {
  public:
  std::unique_ptr<GoalStateProvisioner::Stub> stub_;
  std::shared_ptr<grpc_impl::Channel> chan_;

  /* This thread is responsible for dispatching GoalStatesV2 to the thread pool */
  // std::thread *dispatcher;
  std::unique_ptr<ServerCompletionQueue> cq_;
  std::unique_ptr<ServerAsyncReaderWriter<GoalStateOperationReply,  GoalStateV2>> stream_;
  ctpl::thread_pool thread_pool;

  // Status
  // PushGoalStatesStream(ServerContext *context,
  //                      ServerAsyncReaderWriter<GoalStateOperationReply, GoalStateV2> *stream) /*override */;
  void
  PushGoalStatesWorker(GoalStateV2 &goalStateV2, 
                       ServerAsyncReaderWriter<GoalStateOperationReply, GoalStateV2> *stream,
                       std::chrono::_V2::steady_clock::time_point start,
                       void *tag);

  Status ShutDownServer();

  void RunServer();

  void ConnectToNCM();

  private:
  std::unique_ptr<Server> server;
};


struct AsyncClientCall {
  alcor::schema::HostRequestReply reply;
  grpc::ClientContext context;
  grpc::Status status;
  std::unique_ptr<grpc::ClientAsyncResponseReader<alcor::schema::HostRequestReply> > response_reader;
};
