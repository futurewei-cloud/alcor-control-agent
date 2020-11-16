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

#ifndef ACA_ZETA_STATE_HANDLER_H
#define ACA_ZETA_STATE_HANDLER_H

#include "aca_zeta_programming.h"
#include "goalstateprovisioner.grpc.pb.h"

namespace aca_zeta_state_handler
{
class Aca_Zeta_State_Handler {
  public:
  static Aca_Zeta_State_Handler &get_instance();

  //process ONE Zeta state int
  int update_zeta_state_workitem(const alcor::schema::VpcState current_VpcState);

  private:
  // constructor and destructor marked as private so that noone can call it
  // for the singleton implementation
  Aca_Zeta_State_Handler();
  ~Aca_Zeta_State_Handler();

  aca_zeta_programming::ACA_Zeta_Programming *aca_zeta_programming;
};
} // namespace aca_zeta_state_handler
#endif // ifndef ACA_ZETA_STATE_HANDLER_H
