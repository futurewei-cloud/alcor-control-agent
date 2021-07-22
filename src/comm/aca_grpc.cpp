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
#include "aca_grpc.h"

extern string g_grpc_server_port;
extern string g_ncm_address;
extern string g_ncm_port;

using namespace alcor::schema;
using aca_comm_manager::Aca_Comm_Manager;

Status GoalStateProvisionerAsyncServer::ShutDownServer()
{
  ACA_LOG_INFO("%s", "Shutdown server");
  server_->Shutdown();
  // cq_->Shutdown();
  thread_pool_.stop();
  return Status::OK;
}

void GoalStateProvisionerAsyncServer::AsyncWorker(int cq_index)
{
  void *got_tag;
  bool ok;
  new GoalStateProvisionerAsyncInstance(&service_, cq_vector_.at(cq_index).get());
  while (true) {
    if (!cq_vector_.at(cq_index)->Next(&got_tag, &ok)) {
      ACA_LOG_DEBUG("Completion Queue Shut. Quitting\n");
      break;
    }
    static_cast<GoalStateProvisionerAsyncInstance *>(got_tag)->PushGoalStatesStream(ok);
  }
}

void GoalStateProvisionerAsyncServer::RunServer(int thread_pool_size)
{
  ServerBuilder builder;
  string GRPC_SERVER_ADDRESS = "0.0.0.0:" + g_grpc_server_port;
  builder.AddListeningPort(GRPC_SERVER_ADDRESS, grpc::InsecureServerCredentials());
  builder.RegisterService(&service_);
  for (int i = 0; i < thread_pool_size; i++) {
    // std::unique_ptr<grpc_impl::ServerCompletionQueue> cq = builder.AddCompletionQueue();
    cq_vector_.push_back(builder.AddCompletionQueue());
  }
  ACA_LOG_DEBUG("This async server has %ld CQs\n", cq_vector_.size());
  // cq_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();
  ACA_LOG_INFO("Async GRPC: Streaming capable GRPC server listening on %s\n",
               GRPC_SERVER_ADDRESS.c_str());

  thread_pool_.resize(thread_pool_size);
  /* wait for thread pool to initialize*/
  while (thread_pool_.n_idle() != thread_pool_.size()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };

  for (int i = 0; i < thread_pool_size; i++) {
    thread_pool_.push(std::bind(&GoalStateProvisionerAsyncServer::AsyncWorker, this, i));
  }
  ACA_LOG_DEBUG("After using the thread pool, we have %ld idle threads in the pool, thread pool size: %ld\n",
                thread_pool_.n_idle(), thread_pool_.size());
  server_->Wait();
}

void GoalStateProvisionerAsyncInstance::PushGoalStatesStream(bool ok)
{
  if (!ok) {
    if (status_ == READY_TO_WRITE) {
      ACA_LOG_DEBUG("Finishing the stream (Async GRPC)\n");
      stream_->Finish(Status::OK, this);
      status_ = DONE;
    } else
      ACA_LOG_DEBUG("Unexpected GRPC Failure with %ld\n", status_);
  } else {
    switch (status_) {
    case READY_TO_CONNECT: {
      ACA_LOG_DEBUG("Ready to connect (Async GRPC)\n");
      service_->RequestPushGoalStatesStream(&ctx_, stream_, cq_, cq_, this);
      // this shouldn't happen in normal cases
      ctx_.AsyncNotifyWhenDone(this);
      status_ = READY_TO_READ;
      break;
    }
    case READY_TO_WRITE: {
      if (goalStateV2_.neighbor_states_size() == 1) {
        // if there's only one neighbor state, it means that it is pushed
        // because of the on-demand request
        auto received_gs_time_high_res = std::chrono::high_resolution_clock::now();
        auto neighbor_id = goalStateV2_.neighbor_states().begin()->first.c_str();
        ACA_LOG_INFO("Neighbor ID: %s received at: %ld milliseconds\n", neighbor_id,
                     std::chrono::duration_cast<std::chrono::milliseconds>(
                             received_gs_time_high_res.time_since_epoch())
                             .count());
      }
      std::chrono::_V2::steady_clock::time_point start =
              std::chrono::steady_clock::now();
      int rc = Aca_Comm_Manager::get_instance().update_goal_state(goalStateV2_, gsOperationReply_);
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
      std::chrono::_V2::steady_clock::time_point end = std::chrono::steady_clock::now();
      auto message_total_operation_time =
              std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

      ACA_LOG_DEBUG("[METRICS] Received goalstate at: [%ld], update finished at: [%ld]\nElapsed time for update goalstate operation took: %ld microseconds or %ld milliseconds\n",
                    start, end, message_total_operation_time,
                    (message_total_operation_time / 1000));
      new GoalStateProvisionerAsyncInstance(service_, cq_);
      stream_->Write(gsOperationReply_, this);
      status_ = READY_TO_READ;
      gsOperationReply_.Clear();
      break;
    }
    case READY_TO_READ: {
      ACA_LOG_DEBUG("Reading a new message (Async GRPC)\n");
      stream_->Read(&goalStateV2_, this);
      status_ = READY_TO_WRITE;
      break;
    }
    case DONE: {
      ACA_LOG_DEBUG("Killing the stream (Async GRPC)\n");
      delete this;
      break;
    }
    }
  }
}
