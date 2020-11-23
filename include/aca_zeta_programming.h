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

#ifndef ACA_ZETA_PROGRAMMING_H
#define ACA_ZETA_PROGRAMMING_H

#include <string>
#include <list>
#include "goalstateprovisioner.grpc.pb.h"

using namespace std;

namespace aca_zeta_programming
{
struct zeta_config {
  string group_id;
  //
  list<string> zeta_buckets;
  uint32_t port_inband_operation;
};

class ACA_Zeta_Programming {
  public:
  static ACA_Zeta_Programming &get_instance();

  int create_or_update_zeta_config(const alcor::schema::AuxGateway current_AuxGateway, const string vpc_id);

  int delete_zeta_config(const alcor::schema::AuxGateway current_AuxGateway, const string vpc_id);

  private:
  int _create_or_update_zeta_group_entry(zeta_config *zeta_config_in);

  int _delete_zeta_group_entry(zeta_config *zeta_config_in);
};
} // namespace aca_zeta_programming
#endif // #ifndef ACA_ZETA_PROGRAMMING_H