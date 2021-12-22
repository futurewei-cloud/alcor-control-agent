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
#include "ctpl/ctpl_stl.h"

using namespace alcor::schema;
using grpc::Server;
using grpc::ServerAsyncReader;
using grpc::ServerAsyncReaderWriter;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

class SubscribeInfoProvisionerAsyncServer {
  public:
  ~SubscribeInfoProvisionerAsyncServer()
  {
    this->keepReadingFromCq_ = false;
  }

  /*
    Base class that represents a gRPC call.
    When you have a new kind of rpc, add the corresponding enum to CallType
  */
  struct AsyncSubscribeInfoProvionerCallBase {
    /*
    Currently there are two types of CallStatus, INIT and SENT
    At the INIT state, a streaming/unary rpc call creates a new streaming/unary call instance, 
    requests the call and then processes the received data; 
    AT the SENT state, a streaming call doesn't do anything; but a unary call deletes its own instance,
    since this call is already done.
    */
    enum CallStatus { INIT, SENT, DESTROY };
    CallStatus status_;
    grpc::ServerContext ctx_;
  };

  //  struct for PushNodeSubscribeInfoAsyncCall, which is a unary gRPC call
  //  when adding a new unary rpc call, create a new struct just like PushNodeSubscribeInfoAsyncCall
  struct PushNodeSubscribeInfoAsyncCall : public AsyncSubscribeInfoProvionerCallBase {
    //  Received SubscribeInfo
    NodeSubscribeInfo subscribeInfo_;
    //  Reply to be sent
    SubscribeOperationReply subscribeOperationReply_;

    // Object to send reply to client
    grpc::ServerAsyncResponseWriter<alcor::schema::SubscribeOperationReply> responder_;

    // Constructor
    PushNodeSubscribeInfoAsyncCall() : responder_(&ctx_)
    {
    }
  };

  std::unique_ptr<SubscribeInfoProvisioner::Stub> stub_;
  std::shared_ptr<grpc_impl::Channel> chan_;

  Status ShutDownServer();
  void RunServer(int thread_pool_size);
  void AsyncWorkder();
  /*
    Add a corresponding function here to process a new kind of rpc call.
    For unary rpcs, please refer to ProcessNodeSubscribeInfoAsyncCall

  */
  void ProcessPushNodeSubscribeInfoAsyncCall(AsyncSubscribeInfoProvionerCallBase *baseCall,
                                                 bool ok);

  private:
  bool keepReadingFromCq_ = true;
  std::unique_ptr<Server> server_;
  std::unique_ptr<ServerCompletionQueue> cq_;
  SubscribeInfoProvisioner::AsyncService service_;
  ctpl::thread_pool thread_pool_;
};
