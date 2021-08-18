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
  cq_->Shutdown();
  thread_pool_.stop();
  return Status::OK;
}

void GoalStateProvisionerAsyncServer::AsyncWorker()
{
  void *got_tag;
  bool ok;
  new GoalStateProvisionerAsyncInstance(&service_, cq_.get());
  while (true) {
    if (!cq_->Next(&got_tag, &ok)) {
      ACA_LOG_DEBUG("Completion Queue Shut. Quitting\n");
      break;
    }
    static_cast<GoalStateProvisionerAsyncInstance *>(got_tag)->PushGoalStatesStream(ok);
  }
}

void GoalStateProvisionerAsyncServer::RunserverNew(int thread_pool_size)
{
  ACA_LOG_INFO("Start of RunServerNew, pool size %ld\n", thread_pool_size);

  thread_pool_.resize(thread_pool_size);
  ACA_LOG_DEBUG("Async GRPC SERVER: Resized thread pool to %ld threads, start waiting for the pool to have enough threads\n",
                thread_pool_size);
  /* wait for thread pool to initialize*/
  while (thread_pool_.n_idle() != thread_pool_.size()) {
    ACA_LOG_DEBUG("%s\n", "Still waiting...sleep 1 ms");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };
  ACA_LOG_DEBUG("Async GRPC SERVER: finised resizing thread pool to %ld threads\n",
                thread_pool_size);
  //  Create the server
  ServerBuilder builder;
  string GRPC_SERVER_ADDRESS = "0.0.0.0:" + g_grpc_server_port;
  builder.AddListeningPort(GRPC_SERVER_ADDRESS, grpc::InsecureServerCredentials());
  builder.RegisterService(&service_);
  cq_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();

  //  Test with one thread first, will move to multiple threads later
  for (int i = 0; i < thread_pool_size; i++) {
    //  Instantiate a new PushNetworkResourceStates call object, and set its type
    PushNetworkResourceStatesAsyncCall *pushNetworkResourceStatesAsyncCallInstance =
            new PushNetworkResourceStatesAsyncCall;
    pushNetworkResourceStatesAsyncCallInstance->type_ =
            AsyncGoalStateProvionerCallBase::CallType::PUSH_NETWORK_RESOURCE_STATES;
    pushNetworkResourceStatesAsyncCallInstance->status_ =
            AsyncGoalStateProvionerCallBase::CallStatus::INIT;
    //  Request for the call
    service_.RequestPushNetworkResourceStates(
            &pushNetworkResourceStatesAsyncCallInstance->ctx_, /*Context of this call*/
            &pushNetworkResourceStatesAsyncCallInstance->goalState_, /*GoalState to receive*/
            &pushNetworkResourceStatesAsyncCallInstance->responder_, /*Responder of call*/
            cq_.get(), /*CQ for new call*/
            cq_.get(), /*CQ for finished call*/
            pushNetworkResourceStatesAsyncCallInstance /*The unique tag for the call*/
    );
    //  Instantiate a new PushGoalStatesStream call object, and set its type
    PushGoalStatesStreamAsyncCall *pushGoalStatesStreamAsyncCallInstance =
            new PushGoalStatesStreamAsyncCall;
    pushGoalStatesStreamAsyncCallInstance->type_ =
            AsyncGoalStateProvionerCallBase::CallType::PUSH_GOAL_STATE_STREAM;
    pushGoalStatesStreamAsyncCallInstance->status_ =
            AsyncGoalStateProvionerCallBase::CallStatus::INIT;
    //  Request for the call
    service_.RequestPushGoalStatesStream(
            &pushGoalStatesStreamAsyncCallInstance->ctx_,
            &pushGoalStatesStreamAsyncCallInstance->stream_, cq_.get(),
            cq_.get(), pushGoalStatesStreamAsyncCallInstance);
  }

  while (true) {
    ACA_LOG_DEBUG("%s\n", "At the start of the while loop");
    AsyncGoalStateProvionerCallBase *asyncCallBase = NULL;
    bool ok = false;
    if (!cq_.get()->Next((void **)&asyncCallBase, &ok)) {
      ACA_LOG_DEBUG("Completion Queue Shut. Quitting\n");
      break;
    }
    ACA_LOG_DEBUG("Got message from CQ, is it OK? %ld\n", ok);
    auto call_type = asyncCallBase->type_;
    auto call_status = asyncCallBase->status_;

    if (call_status == AsyncGoalStateProvionerCallBase::CallStatus::SENT) {
      // In the SENT state, we simply delete this call object, and move on
      switch (call_type) {
      case AsyncGoalStateProvionerCallBase::CallType::PUSH_NETWORK_RESOURCE_STATES:
        ACA_LOG_DEBUG("Finished processing gRPC call type %ld, deleting it.\n", call_type);
        delete (PushNetworkResourceStatesAsyncCall *)asyncCallBase;
        break;
      // case AsyncGoalStateProvionerCallBase::CallType::PUSH_GOAL_STATE_STREAM:
      //   ACA_LOG_DEBUG("Finished processing gRPC call type %ld, deleting it.\n", call_type);
      //   delete (PushGoalStatesStreamAsyncCall *)asyncCallBase;
      //   break;
      default:
        ACA_LOG_ERROR("Cannot delete unknown gRPC call type %ld, \n", call_type);
        break;
      }
      //  After deleting this call instance, go back to the start of this while loop to get another one to process.
      continue;
    }
    //  Create new calls, before processing this call
    ACA_LOG_DEBUG("%s\n", "Need to process this call, since its status is INIT");
    switch (call_type) {
    case AsyncGoalStateProvionerCallBase::CallType::PUSH_NETWORK_RESOURCE_STATES:
      ACA_LOG_DEBUG("%s\n", "Initing a new PushNetworkResourceStates, before processing the current one");
      {
        PushNetworkResourceStatesAsyncCall *newPushNetworkResourceStatesAsyncCallInstance =
                new PushNetworkResourceStatesAsyncCall;
        newPushNetworkResourceStatesAsyncCallInstance->type_ =
                AsyncGoalStateProvionerCallBase::CallType::PUSH_NETWORK_RESOURCE_STATES;
        newPushNetworkResourceStatesAsyncCallInstance->status_ =
                AsyncGoalStateProvionerCallBase::CallStatus::INIT;
        //  Request for the call
        service_.RequestPushNetworkResourceStates(
                &newPushNetworkResourceStatesAsyncCallInstance->ctx_, /*Context of this call*/
                &newPushNetworkResourceStatesAsyncCallInstance->goalState_, /*GoalState to receive*/
                &newPushNetworkResourceStatesAsyncCallInstance->responder_, /*Responder of call*/
                cq_.get(), /*CQ for new call*/
                cq_.get(), /*CQ for finished call*/
                newPushNetworkResourceStatesAsyncCallInstance /*The unique tag for the call*/
        );
      }
      break;
    case AsyncGoalStateProvionerCallBase::CallType::PUSH_GOAL_STATE_STREAM: {
      PushGoalStatesStreamAsyncCall *temp =
              static_cast<PushGoalStatesStreamAsyncCall *>(asyncCallBase);
      if (temp->hasReadFromStream) {
        ACA_LOG_DEBUG("%s\n", "Initing a new PushGoalStatesStream, before processing the current one");
        {
          PushGoalStatesStreamAsyncCall *newPushGoalStatesStreamAsyncCallInstance =
                  new PushGoalStatesStreamAsyncCall;
          newPushGoalStatesStreamAsyncCallInstance->type_ =
                  AsyncGoalStateProvionerCallBase::CallType::PUSH_GOAL_STATE_STREAM;
          newPushGoalStatesStreamAsyncCallInstance->status_ =
                  AsyncGoalStateProvionerCallBase::CallStatus::INIT;
          //  Request for the call
          service_.RequestPushGoalStatesStream(
                  &newPushGoalStatesStreamAsyncCallInstance->ctx_,
                  &newPushGoalStatesStreamAsyncCallInstance->stream_, cq_.get(),
                  cq_.get(), newPushGoalStatesStreamAsyncCallInstance);
        }
      }
    }

    break;
    default:
      ACA_LOG_ERROR("Cannot instantiate a new unknown gRPC call type %ld, \n", call_type);
      break;
    }
    ACA_LOG_DEBUG("Time to push the call to the thread pool, thread pool has %ld idle threads before pushing\n",
                  thread_pool_.n_idle());
    // Push goalstate procssing into the thread pool, then call it a day.
    thread_pool_.push([call_type, asyncCallBase](int /**/) {
      ACA_LOG_DEBUG("Processing an async call with type %ld and status %ld\n",
                    call_type, asyncCallBase->status_);
      switch (call_type) {
      case AsyncGoalStateProvionerCallBase::CallType::PUSH_NETWORK_RESOURCE_STATES:
        ACA_LOG_DEBUG("%s\n", "Processing a PushNetworkResourceStates call...");
        {
          PushNetworkResourceStatesAsyncCall *unaryCall =
                  (PushNetworkResourceStatesAsyncCall *)asyncCallBase;
          ACA_LOG_DEBUG("%s\n", "V1: Received a GSV1, need to process it");

          int rc = Aca_Comm_Manager::get_instance().update_goal_state(
                  unaryCall->goalState_, unaryCall->gsOperationReply_);
          if (rc == EXIT_SUCCESS) {
            ACA_LOG_INFO("V1: Control Fast Path synchronized - Successfully updated host with latest goal state %d.\n",
                         rc);
          } else if (rc == EINPROGRESS) {
            ACA_LOG_INFO("V1: Control Fast Path synchronized - Update host with latest goal state returned pending, rc=%d.\n",
                         rc);
          } else {
            ACA_LOG_ERROR("V1: Control Fast Path synchronized - Failed to update host with latest goal state, rc=%d.\n",
                          rc);
          }
          unaryCall->status_ = AsyncGoalStateProvionerCallBase::CallStatus::SENT;
          unaryCall->responder_.Finish(unaryCall->gsOperationReply_, Status::OK, asyncCallBase);
          ACA_LOG_DEBUG("%s\n", "V1: responder_->Finish called");
        }
        break;
      case AsyncGoalStateProvionerCallBase::CallType::PUSH_GOAL_STATE_STREAM:
        ACA_LOG_DEBUG("%s\n", "Processing a PushGoalStateStream call...");
        {
          PushGoalStatesStreamAsyncCall *streamingCall =
                  static_cast<PushGoalStatesStreamAsyncCall *>(asyncCallBase);
          if (!streamingCall->hasReadFromStream) {
            ACA_LOG_DEBUG("%s\n", "This call needs to read from the stream first");
            streamingCall->stream_.Read(&streamingCall->goalStateV2_, asyncCallBase);
            streamingCall->hasReadFromStream = true;
          } else {
            //  It has read from the stream, now to GoalStateV2 should not be empty
            //  and we need to process it.
            ACA_LOG_DEBUG("%s\n", "This call has already read from the stream, now we process the gsv2...");

            if (streamingCall->goalStateV2_.neighbor_states_size() == 1) {
              // if there's only one neighbor state, it means that it is pushed
              // because of the on-demand request
              auto received_gs_time_high_res = std::chrono::high_resolution_clock::now();
              auto neighbor_id = streamingCall->goalStateV2_.neighbor_states()
                                         .begin()
                                         ->first.c_str();
              ACA_LOG_INFO("Neighbor ID: %s received at: %ld milliseconds\n", neighbor_id,
                           std::chrono::duration_cast<std::chrono::milliseconds>(
                                   received_gs_time_high_res.time_since_epoch())
                                   .count());
            }
            std::chrono::_V2::steady_clock::time_point start =
                    std::chrono::steady_clock::now();
            int rc = Aca_Comm_Manager::get_instance().update_goal_state(
                    streamingCall->goalStateV2_, streamingCall->gsOperationReply_);
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
            std::chrono::_V2::steady_clock::time_point end =
                    std::chrono::steady_clock::now();
            auto message_total_operation_time =
                    std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                            .count();

            ACA_LOG_DEBUG("[METRICS] Received goalstate at: [%ld], update finished at: [%ld]\nElapsed time for update goalstate operation took: %ld microseconds or %ld milliseconds\n",
                          start, end, message_total_operation_time,
                          (message_total_operation_time / 1000));

            streamingCall->status_ = AsyncGoalStateProvionerCallBase::CallStatus::SENT;
            streamingCall->hasReadFromStream = false;
            streamingCall->stream_.Write(streamingCall->gsOperationReply_, asyncCallBase);
            streamingCall->gsOperationReply_.Clear();
          }
        }
        break;
      default:
        ACA_LOG_WARN("Unknown call type %ld when processing async call, breaking...\n", call_type);
        break;
      }
    });
    ACA_LOG_DEBUG("After pushing the call to the thread pool, thread pool has %ld idle threads before pushing\n",
                  thread_pool_.n_idle());
  }
  ACA_LOG_DEBUG("%s\n", "Out of the for loop, this should not happen");
}

void GoalStateProvisionerAsyncServer::RunServer(int thread_pool_size)
{
  ServerBuilder builder;
  string GRPC_SERVER_ADDRESS = "0.0.0.0:" + g_grpc_server_port;
  builder.AddListeningPort(GRPC_SERVER_ADDRESS, grpc::InsecureServerCredentials());
  builder.RegisterService(&service_);
  cq_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();
  ACA_LOG_INFO("Async GRPC: Streaming capable GRPC server listening on %s\n",
               GRPC_SERVER_ADDRESS.c_str());

  thread_pool_.resize(thread_pool_size);
  /* wait for thread pool to initialize*/
  while (thread_pool_.n_idle() != thread_pool_.size()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };

  for (int i = 0; i < thread_pool_size; i++) {
    thread_pool_.push(std::bind(&GoalStateProvisionerAsyncServer::AsyncWorker, this));
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
