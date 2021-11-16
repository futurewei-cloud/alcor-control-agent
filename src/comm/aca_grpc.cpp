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
  keepReadingFromCq_ = false;
  return Status::OK;
}

void GoalStateProvisionerAsyncServer::ProcessPushNetworkResourceStatesAsyncCall(
        AsyncGoalStateProvionerCallBase *baseCall, bool ok)
{
  ACA_LOG_DEBUG("Start of ProcessPushNetworkResourceStateAsyncCall, OK: %ld, call_status: %ld\n",
                ok, baseCall->status_);
  PushNetworkResourceStatesAsyncCall *unaryCall =
          static_cast<PushNetworkResourceStatesAsyncCall *>(baseCall);
  if (!ok) {
    // maybe delete the instance and init a new one?
    ACA_LOG_DEBUG("%s\n", "Got a PushNetworkResourceStates call that is NOT OK.");
    delete (PushNetworkResourceStatesAsyncCall *)baseCall;
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
  } else {
    switch (unaryCall->status_) {
    case AsyncGoalStateProvionerCallBase::CallStatus::INIT: {
      ACA_LOG_DEBUG("%s\n", "Initing a new PushNetworkResourceStates, before processing the current one");
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
      //  process goalstate in the thread pool
      ACA_LOG_DEBUG("%s\n", "Processing a PushNetworkResourceStates call...");
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
      unaryCall->responder_.Finish(unaryCall->gsOperationReply_, Status::OK, baseCall);
      ACA_LOG_DEBUG("%s\n", "V1: responder_->Finish called");

    } break;
    case AsyncGoalStateProvionerCallBase::CallStatus::SENT: {
      ACA_LOG_DEBUG("Finished processing %s gRPC call, deleting it.\n",
                    "PushNetworkResourceStates");
      delete unaryCall;
    } break;
    default:
      break;
    }
  }
}

void GoalStateProvisionerAsyncServer::ProcessPushGoalStatesStreamAsyncCall(
        AsyncGoalStateProvionerCallBase *baseCall, bool ok)
{
  ACA_LOG_DEBUG("Start of ProcessPushGoalStatesStreamAsyncCall, OK: %ld, call_status: %ld\n",
                ok, baseCall->status_);
  PushGoalStatesStreamAsyncCall *streamingCall =
          static_cast<PushGoalStatesStreamAsyncCall *>(baseCall);
  if (!ok) {
    // maybe delete the instance and init a new one?
    ACA_LOG_DEBUG("%s\n", "Got a PushGoalStatesStream call that is NOT OK.");
    delete (PushGoalStatesStreamAsyncCall *)baseCall;
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
  } else {
    switch (streamingCall->status_) {
    case AsyncGoalStateProvionerCallBase::CallStatus::INIT:
      if (streamingCall->hasReadFromStream) {
        ACA_LOG_DEBUG("%s\n", "Initing a new PushGoalStatesStream, before processing the current one");
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
      ACA_LOG_DEBUG("%s\n", "Processing a PushGoalStateStream call...");
      {
        if (!streamingCall->hasReadFromStream) {
          ACA_LOG_DEBUG("%s\n", "This call needs to read from the stream first");
          streamingCall->stream_.Read(&streamingCall->goalStateV2_, baseCall);
          streamingCall->hasReadFromStream = true;
        } else {
          //  process goalstateV2 in the thread pool
          //  It has read from the stream, now to GoalStateV2 should not be empty
          //  and we need to process it.
          ACA_LOG_DEBUG("%s\n", "This call has already read from the stream, now we process the gsv2...");

          if (streamingCall->goalStateV2_.neighbor_states_size() == 1) {
            // if there's only one neighbor state, it means that it is pushed
            // because of the on-demand request
            auto received_gs_time_high_res = std::chrono::high_resolution_clock::now();
            auto neighbor_id =
                    streamingCall->goalStateV2_.neighbor_states().begin()->first.c_str();
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
          streamingCall->stream_.Write(streamingCall->gsOperationReply_, baseCall);
        }
      }
      break;
    case AsyncGoalStateProvionerCallBase::CallStatus::SENT:
      ACA_LOG_DEBUG("%s\n", "In SENT state, calling .Finish");
      streamingCall->status_ = AsyncGoalStateProvionerCallBase::CallStatus::DESTROY;
      streamingCall->stream_.Finish(Status::OK, baseCall);
      break;
    case AsyncGoalStateProvionerCallBase::CallStatus::DESTROY:
      ACA_LOG_DEBUG("%s\n", "In DESTROY state, calling .Finish");
      ACA_LOG_DEBUG("%s\n", "In DESTROY state, calling gsOperationReply_.Clear()");
      streamingCall->gsOperationReply_.Clear();
      ACA_LOG_DEBUG("%s\n", "In DESTROY state, calling delete baseCall");
      delete (PushGoalStatesStreamAsyncCall *)baseCall;
      break;
    default:
      ACA_LOG_DEBUG("%s\n", "PushGoalStatesStream call in at DEFAULT state");
      break;
    }
  }
}

void GoalStateProvisionerAsyncServer::AsyncWorkder()
{
  while (keepReadingFromCq_) {
    ACA_LOG_DEBUG("%s\n", "At the start of the while loop");
    AsyncGoalStateProvionerCallBase *asyncCallBase = NULL;
    bool ok = false;
    if (!cq_.get()->Next((void **)&asyncCallBase, &ok)) {
      ACA_LOG_DEBUG("Completion Queue Shut. Quitting\n");
      break;
    }
    ACA_LOG_DEBUG("Got message from CQ, is it OK? %ld\n", ok);
    auto call_type = asyncCallBase->type_;

    /*
      When adding a new rpc, please add its own case in the following switch statement,
      also, please add a corresponding process function so that you can push it to the 
      thread pool.
      For unary rpc example, please refer to ProcessPushNetworkResourceStatesAsyncCall.
      For streaming rpc example, please refer to ProcessPushGoalStatesStreamAsyncCall
    */
    switch (call_type) {
    case AsyncGoalStateProvionerCallBase::CallType::PUSH_NETWORK_RESOURCE_STATES:
      // thread_pool_.push(std::bind(&GoalStateProvisionerAsyncServer::ProcessPushNetworkResourceStatesAsyncCall,
      //                             this, asyncCallBase, &ok, 1));
      ProcessPushNetworkResourceStatesAsyncCall(asyncCallBase, ok);
      break;
    case AsyncGoalStateProvionerCallBase::CallType::PUSH_GOAL_STATE_STREAM:
      // thread_pool_.push(std::bind(&GoalStateProvisionerAsyncServer::ProcessPushGoalStatesStreamAsyncCall,
      //                             this, asyncCallBase, &ok, 1));
      ProcessPushGoalStatesStreamAsyncCall(asyncCallBase, ok);
      break;
    default:
      ACA_LOG_DEBUG("Unsupported async call type: %ld, please check your input\n",
                    asyncCallBase->type_);
      break;
    }
  }
  ACA_LOG_DEBUG("%s\n", "Out of the for loop, seems like this server is shutting down.");
}

void GoalStateProvisionerAsyncServer::RunServer(int thread_pool_size)
{
  ACA_LOG_INFO("Start of RunServer, pool size %ld\n", thread_pool_size);

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
  builder.SetMaxMessageSize(INT_MAX);
  builder.SetMaxReceiveMessageSize(INT_MAX);
  builder.SetMaxSendMessageSize(INT_MAX);
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

  for (int i = 0; i < thread_pool_size; i++) {
    ACA_LOG_DEBUG("Pushing the %ldth async worker into the pool", i);
    thread_pool_.push(std::bind(&GoalStateProvisionerAsyncServer::AsyncWorkder, this));
  }
}
