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

#include <iostream>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include "goalstateprovisioner.grpc.pb.h"

using namespace alcor::schema;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

class GoalStateProvisionerImpl final : public GoalStateProvisioner::Service {
  public:
  std::unique_ptr<GoalStateProvisioner::Stub> stub_;
  HostRequestReply RequestGoalStates(HostRequest *request);

  // ~GoalStateProvisionerImpl();
  explicit GoalStateProvisionerImpl(){};
  Status PushNetworkResourceStates(ServerContext *context, const GoalState *goalState,
                                   GoalStateOperationReply *goalStateOperationReply) override;

  Status
  PushGoalStatesStream(ServerContext *context,
                       ServerReaderWriter<GoalStateOperationReply, GoalStateV2> *stream) override;

  Status ShutDownServer();

  void RunServer();

  private:
  std::unique_ptr<Server> server;
};
