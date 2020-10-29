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

#include "aca_oam_server.h"
#include "aca_log.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <errno.h>
#include "aca_ovs_control.h"
#include "aca_ovs_l2_programmer.h"

using namespace std;

namespace aca_zeta_server
{
ACA_Oam_Server &ACA_Oam_Server::get_instance(){
  static ACA_Oam_Server instance;
  return instance;
};

void ACA_Oam_Server::oams_recv(uint32_t in_port, void *message)
{

};

uint8_t _get_message_type(OAM_message *oammsg)
{

};

void _parse_oam_msg_ops(uint32_t in_port, OAM_message *oammsg)
{

};

int ACA_Oam_Server::add_direct_path(const char *bridge, op_data *opdata)
{

};

int del_direct_path(const char *bridge, op_data *opdata)
{

};

}