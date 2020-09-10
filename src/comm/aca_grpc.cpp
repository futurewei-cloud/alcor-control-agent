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

using namespace alcor::schema;
using aca_comm_manager::Aca_Comm_Manager;

Status GoalStateProvisionerImpl::PushNetworkResourceStates(ServerContext *context,
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

Status GoalStateProvisionerImpl::PushNetworkResourceStatesStream(
        ServerContext *context, ServerReaderWriter<GoalStateOperationReply, GoalState> *stream)
{
  GoalState goalState;
  GoalStateOperationReply gsOperationReply;
  int rc = EXIT_FAILURE;

  while (stream->Read(&goalState)) {
    rc = Aca_Comm_Manager::get_instance().update_goal_state(goalState, gsOperationReply);
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

void GoalStateProvisionerImpl::RunServer()
{
  ServerBuilder builder;
  string GRPC_SERVER_ADDRESS = "0.0.0.0:" + g_grpc_server_port;
  builder.AddListeningPort(GRPC_SERVER_ADDRESS, grpc::InsecureServerCredentials());
  builder.RegisterService(this);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  ACA_LOG_INFO("Streaming capable GRPC server listening on %s\n",
               GRPC_SERVER_ADDRESS.c_str());
  server->Wait();
}
