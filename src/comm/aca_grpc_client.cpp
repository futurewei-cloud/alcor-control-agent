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
#include <chrono>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_comm_mgr.h"
#include "aca_log.h"
#include "aca_grpc_client.h"
#include "aca_util.h"

// extern string g_grpc_server_port;
extern string g_ncm_address;
extern string g_ncm_port;

using namespace alcor::schema;
using aca_comm_manager::Aca_Comm_Manager;

void GoalStateProvisionerClientImpl::RequestGoalStates(HostRequest *request,
                                                       grpc::CompletionQueue *cq)
{
  std::chrono::_V2::steady_clock::time_point start = std::chrono::steady_clock::now();
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
  ACA_LOG_INFO("Sent hostOperationRequest on thread: %ld\n", std::this_thread::get_id());
  std::chrono::_V2::steady_clock::time_point end = std::chrono::steady_clock::now();
  auto send_host_operation_request_time =
          std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  ACA_LOG_DEBUG("[METRICS] RequestGoalStates: [%ld], update finished at: [%ld]\nElapsed time for sending hostOperationRequest took: %ld microseconds or %ld milliseconds\n",
                start, end, send_host_operation_request_time,
                (send_host_operation_request_time / 1000));
  return;
}

void GoalStateProvisionerClientImpl::ConnectToNCM()
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

void GoalStateProvisionerClientImpl::RunClient()
{
  ACA_LOG_INFO("Running a grpc client in a separate thread id: %ld\n",
               std::this_thread::get_id());
  this->ConnectToNCM();
}