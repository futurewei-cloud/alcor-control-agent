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

#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_comm_mgr.h"
#include "aca_log.h"
#include "aca_async_grpc_server.h"

static char GRPC_SERVER_ADDRESS[] = "0.0.0.0:50001";

using namespace alcorcontroller;
using aca_comm_manager::Aca_Comm_Manager;
using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

static std::atomic<bool> running;
Aca_Async_GRPC_Server::~Aca_Async_GRPC_Server()
{
  server_->Shutdown();
  cq_->Shutdown();
  void *tag;
  bool ok;
  while (cq_->Next(&tag, &ok)) {
  } //Drain the completion queue.
}

Aca_Async_GRPC_Server::Aca_Async_GRPC_Server()
{
  running = true;
}

void Aca_Async_GRPC_Server::Run()
{
  ServerBuilder builder;
  builder.AddListeningPort(GRPC_SERVER_ADDRESS, grpc::InsecureServerCredentials());
  builder.RegisterService(&service_);
  cq_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();
  ACA_LOG_INFO("Server listening on %s\n", GRPC_SERVER_ADDRESS);
  HandleRpcs();
}

void Aca_Async_GRPC_Server::StopServer()
{
  running = false;
}

Aca_Async_GRPC_Server::CallData::CallData(GoalStateProvisioner::AsyncService *service,
                                          ServerCompletionQueue *cq)
        : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE)
{
  Proceed();
}

void Aca_Async_GRPC_Server::CallData::Proceed()
{
  if (status_ == CREATE) {
    status_ = PROCESS;
    service_->RequestPushNetworkResourceStates(&ctx_, &request_, &responder_, cq_, cq_, this);
  } else if (status_ == PROCESS) {
    new CallData(service_, cq_);
    int rc = Aca_Comm_Manager::get_instance().update_goal_state(request_, reply_);
    if (rc != EXIT_SUCCESS) {
      ACA_LOG_ERROR("Control Fast Path - Failed to update host with latest goal state, rc=%d.\n",
                    rc);
    } else {
      ACA_LOG_INFO("Control Fast Path - Successfully updated host with latest goal state %d.\n",
                   rc);
    }
    status_ = FINISH;
    responder_.Finish(reply_, Status::OK, this);
  } else {
    GPR_ASSERT(status_ == FINISH);
    delete this;
  }
}

void Aca_Async_GRPC_Server::HandleRpcs()
{
  new CallData(&service_, cq_.get());
  void *tag;
  bool ok;
  bool cq_has_next;
  while (running) {
    cq_has_next = cq_->Next(&tag, &ok);
    if (running && cq_has_next) {
      GPR_ASSERT(ok);
      static_cast<CallData *>(tag)->Proceed();
    } else if (running && !cq_has_next)
      GPR_ASSERT(cq_has_next);
  }
}
