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

#include "aca_zeta_programmer.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_util.h"
#include "goalstateprovisioner.grpc.pb.h"
#include "aca_log.h"

namespace aca_zeta_programmer{

ACA_Zeta_Programmer &ACA_Zeta_Programmer::get_instance()
{
  static ACA_Zeta_Programmer instance;
  return instance;
}

int ACA_Zeta_Programmer::add_group_entry(zeta_config *zeta_config_in)
{
  unsigned long not_care_culminative_time;
  int overall_rc = EXIT_SUCCESS;

  string cmd = "-O OpenFlow13 add-group br-tun group_id=" + zeta_config_in->group_id + ",type=select"
  vector<string> outport_list = zeta_config_in->buckets;

  for( int i = 0; i < outport_list.size(); i++){
      string outport_name = aca_get_outport_name(alcor::schema::NetworkType::VXLAN, outport_list[i]);
      cmd += ",bucket=output:" + outport_name;
  }

  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
            cmd, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_DEBUG("Command failed!!! overrall_rc: %d\n", overrall_rc);
  }
  
  return overall_rc;
}

int ACA_Zeta_Programmer::delete_group_entry(zeta_config *zeta_config_in)
{
  unsigned long not_care_culminative_time;

  int overall_rc = EXIT_SUCCESS;

  string cmd = "-O OpenFlow13 del-groups br-tun group_id=" + zeta_config_in->group_id;
  aca_ovs_l2_programmer::ACA_OVS_L2_Programmer::get_instance().execute_openflow_command(
        cmd, not_care_culminative_time, overall_rc);

  if (overall_rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Command succeeded!\n");
  } else {
    ACA_LOG_DEBUG("Command failed!!! overrall_rc: %d\n", overrall_rc);
  }
  
  return overall_rc;
}

int ACA_Zeta_Programmer::dump_groups_entry(const char *opt)
{

}

int ACA_Zeta_Programmer::update_group_entry(zeta_config *zeta_config_in){

}
}