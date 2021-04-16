/*
 *
 * Copyright 2015 gRPC authors.
 * Copyright 2019 The Alcor Authors - file modified.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_comm_mgr.h"
#include "aca_log.h"
#include "aca_grpc.h"

extern string g_grpc_server_port;
extern string g_ncm_address;
extern string g_ncm_port;

using namespace alcor::schema;
using aca_comm_manager::Aca_Comm_Manager;

void GoalStateProvisionerImpl::RequestGoalStates(HostRequest *request,
                                                 grpc::CompletionQueue *cq)
{
  grpc::ClientContext ctx;
  alcor::schema::HostRequestReply reply;

  // check current grpc channel state, try to connect if needed
  grpc_connectivity_state current_state = chan_->GetState(true);
  if (current_state == grpc_connectivity_state::GRPC_CHANNEL_SHUTDOWN ||
      current_state == grpc_connectivity_state::GRPC_CHANNEL_TRANSIENT_FAILURE) {
    ACA_LOG_INFO("%s, it is: [%d]\n",
                 "Channel state is not READY/CONNECTING/IDLE. Try to reconnnect.",
                 current_state);
    this->ConnectToNCM();
    reply.mutable_operation_statuses()->Add();
    reply.mutable_operation_statuses()->at(0).set_operation_status(OperationStatus::FAILURE);
    return;
  }
  AsyncClientCall *call = new AsyncClientCall;
  call->response_reader = stub_->AsyncRequestGoalStates(&call->context, *request, cq);
  call->response_reader->Finish(&call->reply, &call->status, (void *)call);
  // stub_->RequestGoalStates(&ctx, *request, &reply);
  return;
}

Status
GoalStateProvisionerImpl::PushNetworkResourceStates(ServerContext * /* context */,
                                                    const GoalState *goalState,
                                                    GoalStateOperationReply *goalStateOperationReply)
{
  GoalState received_goal_state = *goalState;

  int rc = Aca_Comm_Manager::get_instance().update_goal_state(
          received_goal_state, *goalStateOperationReply);
  if (rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("Control Fast Path synchronized - Successfully updated host with latest goal state %d.\n",
                 rc);
  } else if (rc == EINPROGRESS) {
    ACA_LOG_INFO("Control Fast Path synchronized - Update host with latest goal state returned pending, rc=%d.\n",
                 rc);
  } else {
    ACA_LOG_ERROR("Control Fast Path synchronized - Failed to update host with latest goal state, rc=%d.\n",
                  rc);
  }

  return Status::OK;
}

Status GoalStateProvisionerImpl::PushGoalStatesStream(
        ServerContext * /* context */,
        ServerReaderWriter<GoalStateOperationReply, GoalStateV2> *stream)
{
  GoalStateV2 goalStateV2;
  GoalStateOperationReply gsOperationReply;
  int rc = EXIT_FAILURE;

  while (stream->Read(&goalStateV2)) {
    rc = Aca_Comm_Manager::get_instance().update_goal_state(goalStateV2, gsOperationReply);
    if (rc == EXIT_SUCCESS) {
      ACA_LOG_INFO("Control Fast Path streaming - Successfully updated host with latest goal state %d.\n",
                   rc);
    } else if (rc == EINPROGRESS) {
      ACA_LOG_INFO("Control Fast Path streaming - Update host with latest goal state returned pending, rc=%d.\n",
                   rc);
    } else {
      ACA_LOG_ERROR("Control Fast Path streaming - Failed to update host with latest goal state, rc=%d.\n",
                    rc);
    }
    stream->Write(gsOperationReply);
    gsOperationReply.Clear();
  }

  return Status::OK;
}

Status GoalStateProvisionerImpl::ShutDownServer()
{
  ACA_LOG_INFO("%s", "Shutdown server");
  server->Shutdown();
  return Status::OK;
}

void GoalStateProvisionerImpl::ConnectToNCM()
{
  ACA_LOG_INFO("%s\n", "Trying to init a new sub to connect to the NCM");
  grpc::ChannelArguments args;
  // Channel does a keep alive ping every 10 seconds;
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 10000);
  // If the channel does receive the keep alive ping result in 20 seconds, it closes the connection
  args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 20000);

  // Allow keep alive ping even if there are no calls in flight
  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);

  chan_ = grpc::CreateCustomChannel(g_ncm_address + ":" + g_ncm_port,
                                    grpc::InsecureChannelCredentials(), args);
  stub_ = GoalStateProvisioner::NewStub(chan_);

  ACA_LOG_INFO("%s\n", "After initing a new sub to connect to the NCM");
}

void GoalStateProvisionerImpl::RunServer()
{
  this->ConnectToNCM();
  ServerBuilder builder;
  string GRPC_SERVER_ADDRESS = "0.0.0.0:" + g_grpc_server_port;
  builder.AddListeningPort(GRPC_SERVER_ADDRESS, grpc::InsecureServerCredentials());
  builder.RegisterService(this);
  server = builder.BuildAndStart();
  ACA_LOG_INFO("Streaming capable GRPC server listening on %s\n",
               GRPC_SERVER_ADDRESS.c_str());
  server->Wait();
}
