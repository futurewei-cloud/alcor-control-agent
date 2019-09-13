#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_comm_mgr.h"
#include "aca_log.h"


using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using aliothcontroller::GoalStateProvisioner;
using aliothcontroller::GoalStateRequest;
using aliothcontroller::GoalStateOperationReply;
using aca_comm_manager::Aca_Comm_Manager;

class Aca_Async_GRPC_Server final {
 public:
  ~Aca_Async_GRPC_Server();
  Aca_Async_GRPC_Server();
  void Run();
  void StopServer();

 private:
  class CallData {
   public:
    CallData(GoalStateProvisioner::AsyncService* service, ServerCompletionQueue* cq);
    void Proceed();
   private:
    GoalStateProvisioner::AsyncService* service_;
    ServerCompletionQueue* cq_;
    ServerContext ctx_;
    aliothcontroller::GoalState request_;
    aliothcontroller::GoalStateOperationReply reply_;
    ServerAsyncResponseWriter<aliothcontroller::GoalStateOperationReply> responder_;

    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus status_;
  };

  void HandleRpcs();
  std::unique_ptr<ServerCompletionQueue> cq_;
  GoalStateProvisioner::AsyncService service_;
  std::unique_ptr<Server> server_;
};