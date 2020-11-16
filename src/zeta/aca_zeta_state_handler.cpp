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

#include "aca_log.h"
#include "aca_util.h"
#include "aca_zeta_state_handler.h"
#include "aca_zeta_programming.h"
#include "aca_ovs_control.h"
#include <future>

using namespace alcor::schema;
using namespace aca_zeta_programming;

namespace aca_zeta_state_handler
{
Aca_Zeta_State_Handler::Aca_Zeta_State_Handler()
{
  ACA_LOG_INFO("%s", "Zeta State Handler: using Zeta server\n");
  this->aca_zeta_programming =
          &(aca_zeta_programming::ACA_Zeta_Programming::get_instance());
}

Aca_Zeta_State_Handler::~Aca_Zeta_State_Handler()
{
  // allocated Zeta_programming_if is destroyed when program exits.
}

Aca_Zeta_State_Handler &Aca_Zeta_State_Handler::get_instance()
{
  // It is instantiated on first use.
  // allocated instance is destroyed when program exits.
  static Aca_Zeta_State_Handler instance;
  return instance;
}

int Aca_Zeta_State_Handler::update_zeta_state_workitem(const alcor::schema::VpcState current_VpcState)
{
  zeta_config stZetaCfg;
  int overall_rc = EXIT_SUCCESS;

  AuxGateway current_ZetaState = current_VpcState.configuration().auxiliary_gateway();

  if (current_ZetaState.auxgateway_type() == 0) {
    ACA_LOG_INFO("%s", "AuxGateway_type is NONE!\n");
  }
  stZetaCfg.AuxGateway_type = current_ZetaState.auxgateway_type();
  stZetaCfg.id = current_ZetaState.id();
  stZetaCfg.extra_info.port_inband_operation =
          current_ZetaState.zeta_info().port_inband_operation();

  for (auto destination : current_ZetaState.destinations()) {
    struct destination dest;
    dest.ip_address = destination.ip_address();
    dest.mac_address = destination.mac_address();
    stZetaCfg.destinations.push_back(dest);
  }

  aca_ovs_control::ACA_OVS_Control::get_instance().set_oam_server_port(
          stZetaCfg.extra_info.port_inband_operation);
  overall_rc = aca_zeta_programming->update_zeta_group_entry(&stZetaCfg);

  return overall_rc;
}
} // namespace aca_zeta_state_handler
