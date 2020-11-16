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

#include "aca_zeta_programming.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_util.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_log.h"

namespace aca_zeta_programming
{
ACA_Zeta_Programming &ACA_Zeta_Programming::get_instance()
{
  static ACA_Zeta_Programming instance;
  return instance;
}

int ACA_Zeta_Programming::update_zeta_group_entry(zeta_config *zeta_config_in)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  list<struct destination>::iterator it;

  string cmd = "-O OpenFlow13 add-group br-tun group_id=" + zeta_config_in->id + ",type=select";

  for (it = zeta_config_in->destinations.begin();
       it != zeta_config_in->destinations.end(); it++) {
    string outport_name =
            aca_get_outport_name(alcor::schema::NetworkType::VXLAN, (*it).ip_address);
    cmd += ",bucket=output:" + outport_name;
  }

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_ERROR("Command failed!!! overrall_rc: %d\n", overall_rc);
  }

  return overall_rc;
}

int ACA_Zeta_Programming::delete_zeta_group_entry(zeta_config *zeta_config_in)
{
  unsigned long not_care_culminative_time;

  int overall_rc = EXIT_SUCCESS;

  string cmd = "-O OpenFlow13 del-groups br-tun group_id=" + zeta_config_in->id;
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
          cmd, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_ERROR("Command failed!!! overrall_rc: %d\n", overall_rc);
  }

  return overall_rc;
}
} // namespace aca_zeta_programming