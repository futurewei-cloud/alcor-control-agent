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

class GoalStateProvisionerAsyncServer {
  public:
  ~GoalStateProvisionerAsyncServer()
  {
    this->keepReadingFromCq_ = false;
  }

  /*
    Base class that represents a gRPC call.
    When you have a new kind of rpc, add the corresponding enum to CallType
  */
  struct AsyncGoalStateProvionerCallBase {
    /* 
    each CallType represents a type of rpc call defined in goalstateprovisioner.proto,
    by having different CallTypes, we're able to identify which AsyncGoalStateProvionerCallBase
    is which kind of call, then we can static_cast it to the correct struct
    if you're adding a new rpc call, please add the corresponding CallType to this enum    
    */
    enum CallType { PUSH_NETWORK_RESOURCE_STATES, PUSH_GOAL_STATE_STREAM };
    /*
    Currently there are two types of CallStatus, INIT and SENT
    At the INIT state, a streaming/unary rpc call creates a new streaming/unary call instance, 
    requests the call and then processes the received data; 
    AT the SENT state, a streaming call doesn't do anything; but a unary call deletes its own instance,
    since this call is already done.
    */
    enum CallStatus { INIT, SENT };
    CallStatus status_;
    CallType type_;
    grpc::ServerContext ctx_;
  };

  //  struct for PushNetworkResourceStates, which is a unary gRPC call
  //  when adding a new unary rpc call, create a new struct just like PushNetworkResourceStatesAsyncCall
  struct PushNetworkResourceStatesAsyncCall : public AsyncGoalStateProvionerCallBase {
    //  Received GoalState
    GoalState goalState_;
    //  Reply to be sent
    GoalStateOperationReply gsOperationReply_;

    // Object to send reply to client
    grpc::ServerAsyncResponseWriter<alcor::schema::GoalStateOperationReply> responder_;

    // Constructor
    PushNetworkResourceStatesAsyncCall() : responder_(&ctx_)
    {
    }
  };

  //  struct for PushNetworkResourceStates, which is a bi-directional streaming gRPC call
  //  when adding a new streaming rpc call, create a new struct just like PushNetworkResourceStatesAsyncCall
  struct PushGoalStatesStreamAsyncCall : public AsyncGoalStateProvionerCallBase {
    //  Received GoalStateV2
    GoalStateV2 goalStateV2_;
    //  Reply to be sent
    GoalStateOperationReply gsOperationReply_;

    //  Has this call read from the stream yet? If not,
    //  we'd better read from the stream, or the goalStateV2_
    //  will be empty
    bool hasReadFromStream;

    // Object to send reply to client
    ServerAsyncReaderWriter<GoalStateOperationReply, GoalStateV2> stream_;

    // Constructor
    PushGoalStatesStreamAsyncCall() : stream_(&ctx_)
    {
      hasReadFromStream = false;
    }
  };
  std::unique_ptr<GoalStateProvisioner::Stub> stub_;
  std::shared_ptr<grpc_impl::Channel> chan_;

  Status ShutDownServer();
  void RunServer(int thread_pool_size);
  void AsyncWorkder();
  /*
    Add a corresponding function here to process a new kind of rpc call.
    For unary rpcs, please refer to ProcessPushNetworkResourceStatesAsyncCall
    For streaming rpcs, please refer to ProcessPushGoalStatesStreamAsyncCall
  */
  void ProcessPushNetworkResourceStatesAsyncCall(AsyncGoalStateProvionerCallBase *baseCall,
                                                 bool ok);
  void ProcessPushGoalStatesStreamAsyncCall(AsyncGoalStateProvionerCallBase *baseCall, bool ok);

  private:
  bool keepReadingFromCq_ = true;
  std::unique_ptr<Server> server_;
  std::unique_ptr<ServerCompletionQueue> cq_;
  GoalStateProvisioner::AsyncService service_;
  ctpl::thread_pool thread_pool_;
};
