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
#include "subscribeinfoprovisioner.grpc.pb.h"
#include "aca_comm_mgr.h"
#include "aca_log.h"
#include "aca_grpc_subscribe.h"

extern string g_grpc_subscribe_server_port;


using namespace alcor::schema;
using aca_comm_manager::Aca_Comm_Manager;

Status SubscribeInfoProvisionerAsyncServer::ShutDownServer()
{
  ACA_LOG_INFO("%s", "Shutdown server");
  server_->Shutdown();
  cq_->Shutdown();
  thread_pool_.stop();
  keepReadingFromCq_ = false;
  return Status::OK;
}

void SubscribeInfoProvisionerAsyncServer::ProcessPushNodeSubscribeInfoAsyncCall(
        AsyncSubscribeInfoProvionerCallBase *baseCall, bool ok)
{
  ACA_LOG_DEBUG("Start of ProcessPushNodeSubscribeInfoAsyncCall, OK: %ld, call_status: %ld\n",
                ok, baseCall->status_);
  PushNodeSubscribeInfoAsyncCall *unaryCall =
          static_cast<PushNodeSubscribeInfoAsyncCall *>(baseCall);
  if (!ok) {
    // maybe delete the instance and init a new one?
    ACA_LOG_DEBUG("%s\n", "Got a PushNodeSubscribeInfoAsync Call that is NOT OK.");
    delete (PushNodeSubscribeInfoAsyncCall *)baseCall;
    PushNodeSubscribeInfoAsyncCall *newPushNodeSubscribeInfoAsyncCallInstance =
            new PushNodeSubscribeInfoAsyncCall;
    newPushNodeSubscribeInfoAsyncCallInstance->status_ =
            AsyncSubscribeInfoProvionerCallBase::CallStatus::INIT;
    //  Request for the call
    service_.RequestPushNodeSubscribeInfo(
            &newPushNodeSubscribeInfoAsyncCallInstance->ctx_, /*Context of this call*/
            &newPushNodeSubscribeInfoAsyncCallInstance->subscribeInfo_, /*SubscribeInfo to receive*/
            &newPushNodeSubscribeInfoAsyncCallInstance->responder_, /*Responder of call*/
            cq_.get(), /*CQ for new call*/
            cq_.get(), /*CQ for finished call*/
            newPushNodeSubscribeInfoAsyncCallInstance /*The unique tag for the call*/
    );
  } else {
    switch (unaryCall->status_) {
    case AsyncSubscribeInfoProvionerCallBase::CallStatus::INIT: {
      ACA_LOG_DEBUG("%s\n", "Initing a new PushNodeSubscribeInfoAsyncCallInstance, before processing the current one");
      PushNodeSubscribeInfoAsyncCall *newPushNodeSubscribeInfoAsyncCallInstance =
              new PushNodeSubscribeInfoAsyncCall;
      newPushNodeSubscribeInfoAsyncCallInstance->status_ =
              AsyncSubscribeInfoProvionerCallBase::CallStatus::INIT;
      //  Request for the call
      service_.RequestPushNodeSubscribeInfo(
              &newPushNodeSubscribeInfoAsyncCallInstance->ctx_, /*Context of this call*/
              &newPushNodeSubscribeInfoAsyncCallInstance->subscribeInfo_, /*SubscribeInfo to receive*/
              &newPushNodeSubscribeInfoAsyncCallInstance->responder_, /*Responder of call*/
              cq_.get(), /*CQ for new call*/
              cq_.get(), /*CQ for finished call*/
              newPushNodeSubscribeInfoAsyncCallInstance /*The unique tag for the call*/
      );
      //  process SubscribeInfo in the thread pool
      ACA_LOG_DEBUG("%s\n", "Processing a PushNodeSubscribeInfo call...");
      ACA_LOG_DEBUG("%s\n", "Received a SubscribeInfo, need to process it");

      int rc = Aca_Comm_Manager::get_instance().update_subscribe_info(
              unaryCall->subscribeInfo_, unaryCall->subscribeOperationReply_);
      if (rc == EXIT_SUCCESS) {
        ACA_LOG_INFO("Successfully updated host with latest subscribe info %d.\n",
                     rc);
      } else if (rc == EINPROGRESS) {
        ACA_LOG_INFO("Update host with latest subscribe info returned pending, rc=%d.\n",
                     rc);
      } else {
        ACA_LOG_ERROR("Failed to update host with latest subscribe info , rc=%d.\n",
                      rc);
      }
      unaryCall->status_ = AsyncSubscribeInfoProvionerCallBase::CallStatus::SENT;
      unaryCall->responder_.Finish(unaryCall->subscribeOperationReply_, Status::OK, baseCall);
      ACA_LOG_DEBUG("%s\n", "V1: responder_->Finish called");

    } break;
    case AsyncSubscribeInfoProvionerCallBase::CallStatus::SENT: {
      ACA_LOG_DEBUG("Finished processing %s gRPC call, deleting it.\n",
                    "PushNodeSubscribeInfo");
      delete unaryCall;
    } break;
    default:
      break;
    }
  }
}



void SubscribeInfoProvisionerAsyncServer::AsyncWorkder()
{
  while (keepReadingFromCq_) {
    ACA_LOG_DEBUG("%s\n", "At the start of the while loop");
    AsyncSubscribeInfoProvionerCallBase *asyncCallBase = NULL;
    bool ok = false;
    if (!cq_.get()->Next((void **)&asyncCallBase, &ok)) {
      ACA_LOG_DEBUG("Completion Queue Shut. Quitting\n");
      break;
    }
    ACA_LOG_DEBUG("Got message from CQ, is it OK? %ld\n", ok);

    ProcessPushNodeSubscribeInfoAsyncCall(asyncCallBase, ok);
  }
  ACA_LOG_DEBUG("%s\n", "Out of the for loop, seems like this server is shutting down.");
}

void SubscribeInfoProvisionerAsyncServer::RunServer(int thread_pool_size)
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
  string GRPC_SERVER_ADDRESS = "0.0.0.0:" + g_grpc_subscribe_server_port;
  builder.AddListeningPort(GRPC_SERVER_ADDRESS, grpc::InsecureServerCredentials());
  builder.SetMaxMessageSize(INT_MAX);
  builder.SetMaxReceiveMessageSize(INT_MAX);
  builder.SetMaxSendMessageSize(INT_MAX);
  builder.RegisterService(&service_);
  cq_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();

  //  Test with one thread first, will move to multiple threads later
  for (int i = 0; i < thread_pool_size; i++) {
    //  Instantiate a new PushNodeSubscribeInfo call object, and set its status
    PushNodeSubscribeInfoAsyncCall *pushNodeSubscribeInfoAsyncCallInstance =
            new PushNodeSubscribeInfoAsyncCall;
    pushNodeSubscribeInfoAsyncCallInstance->status_ =
            AsyncSubscribeInfoProvionerCallBase::CallStatus::INIT;
    //  Request for the call
    service_.RequestPushNodeSubscribeInfo(
            &pushNodeSubscribeInfoAsyncCallInstance->ctx_, /*Context of this call*/
            &pushNodeSubscribeInfoAsyncCallInstance->subscribeInfo_, /*Subscribe Info to receive*/
            &pushNodeSubscribeInfoAsyncCallInstance->responder_, /*Responder of call*/
            cq_.get(), /*CQ for new call*/
            cq_.get(), /*CQ for finished call*/
            pushNodeSubscribeInfoAsyncCallInstance /*The unique tag for the call*/
    );
  }

  for (int i = 0; i < thread_pool_size; i++) {
    ACA_LOG_DEBUG("Pushing the %ldth async worker into the pool", i);
    thread_pool_.push(std::bind(&SubscribeInfoProvisionerAsyncServer::AsyncWorkder, this));
  }
}
