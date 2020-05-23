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

#ifndef ACA_UTIL_H
#define ACA_UTIL_H

#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"

#define cast_to_nanoseconds(x) chrono::duration_cast<chrono::nanoseconds>(x)

static inline const char *aca_get_operation_name(alcor::schema::OperationType operation)
{
  switch (operation) {
  case alcor::schema::OperationType::CREATE:
    return "CREATE";
  case alcor::schema::OperationType::UPDATE:
    return "UPDATE";
  case alcor::schema::OperationType::GET:
    return "GET";
  case alcor::schema::OperationType::DELETE:
    return "DELETE";
  case alcor::schema::OperationType::INFO:
    return "INFO";
  case alcor::schema::OperationType::FINALIZE:
    return "FINALIZE";
  case alcor::schema::OperationType::CREATE_UPDATE_SWITCH:
    return "CREATE_UPDATE_SWITCH";
  case alcor::schema::OperationType::CREATE_UPDATE_ROUTER:
    return "CREATE_UPDATE_ROUTER";
  case alcor::schema::OperationType::CREATE_UPDATE_GATEWAY:
    return "CREATE_UPDATE_GATEWAY";
  default:
    return "ERROR: unknown operation type!";
  }
}

#endif
