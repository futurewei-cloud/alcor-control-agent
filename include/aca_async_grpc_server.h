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