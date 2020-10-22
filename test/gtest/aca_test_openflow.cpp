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

#include "gtest/gtest.h"
#include "aca_ovs_control.h"
#include "ovs_control.h"
#include "aca_ovs_l2_programmer.h"

using namespace aca_ovs_control;
using namespace ovs_control;
using aca_ovs_l2_programmer::ACA_OVS_L2_Programmer;

//
// Test suite: ovs_flow_mod_cases
//
// Testing the openflow helper functions for add/mod/delete flows and flow_exists
//
TEST(ovs_flow_mod_cases, add_flows)
{
  int overall_rc;

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;

  // add flow
  ACA_OVS_Control::get_instance().add_flow(
          "br-tun", "ip,nw_dst=192.168.0.1,priority=1,actions=drop");

  overall_rc = ACA_OVS_Control::get_instance().flow_exists("br-tun", "ip,nw_dst=192.168.0.1");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
}

TEST(ovs_flow_mod_cases, mod_flows)
{
  int overall_rc;

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // add flow
  ACA_OVS_Control::get_instance().add_flow(
          "br-tun", "tcp,nw_dst=192.168.0.1,priority=1,actions=drop");

  // modify flow
  ACA_OVS_Control::get_instance().mod_flows(
          "br-tun", "tcp,nw_dst=192.168.0.1,priority=1,actions=resubmit(,2)");

  overall_rc = ACA_OVS_Control::get_instance().flow_exists("br-tun", "tcp,nw_dst=192.168.0.1");
  EXPECT_EQ(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
}

TEST(ovs_flow_mod_cases, del_flows)
{
  int overall_rc;

  // create and setup br-int and br-tun bridges, and their patch ports
  overall_rc = ACA_OVS_L2_Programmer::get_instance().setup_ovs_bridges_if_need();
  ASSERT_EQ(overall_rc, EXIT_SUCCESS);

  // add flow
  ACA_OVS_Control::get_instance().add_flow(
          "br-tun", "tcp,nw_dst=192.168.0.9,priority=1,actions=drop");

  // delete flow
  ACA_OVS_Control::get_instance().del_flows("br-tun", "tcp,nw_dst=192.168.0.9,priority=1");

  overall_rc = ACA_OVS_Control::get_instance().flow_exists("br-tun", "tcp,nw_dst=192.168.0.9");
  EXPECT_NE(overall_rc, EXIT_SUCCESS);
  overall_rc = EXIT_SUCCESS;
}