#include "aca_log.h"
#include "aca_util.h"
#include "aca_config.h"
#include "aca_vlan_manager.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_comm_mgr.h"
#include "gtest/gtest.h"
#include "goalstate.pb.h"
#include "aca_ovs_control.h"
#include <unistd.h> /* for getopt */
#include <iostream>
#include <string>
#include <thread>
#include "aca_grpc.h"
#include "aca_on_demand_engine.h"

extern GoalStateProvisionerImpl *g_grpc_server;

TEST(aca_on_demand_testcases, DISABLED_grpc_client_connectivity_test)
{
  sleep(10);
  ACA_LOG_INFO("%s", "Start of DISABLED_grpc_client_connectivity_test\n");
  ASSERT_NE(g_grpc_server, nullptr);
  ACA_LOG_INFO("%s", "Made sure that g_grpc_server is not null\n");

  // test sending a request with an expected request_id, which should return
  // a reply with the same request_id, and a SUCCESS operation status.

  HostRequest example_request_with_expected_id;
  example_request_with_expected_id.add_state_requests();
  string expected_request_id = "12345";
  string unexpected_request_id = "54321";

  example_request_with_expected_id.mutable_state_requests(0)->set_request_id(expected_request_id);
  ACA_LOG_INFO("Channel state: %d\n", g_grpc_server->chan_->GetState(false));
  ACA_LOG_INFO("Request one's request ID: %s\n",
               example_request_with_expected_id.state_requests(0).request_id().c_str());
  alcor::schema::HostRequestReply reply_one =
          g_grpc_server->RequestGoalStates(&example_request_with_expected_id);
  ACA_LOG_INFO("Reply one's operation status size: %d\n",
               reply_one.operation_statuses_size());
  ASSERT_EQ(reply_one.operation_statuses(0).operation_status(), OperationStatus::SUCCESS);
  ASSERT_EQ(reply_one.operation_statuses(0).request_id(), expected_request_id);

  // test sending a request with an expected request_id, which should return
  // a reply with the same request_id, and a SUCCESS operation status.

  HostRequest example_request_with_unexpected_id;
  example_request_with_unexpected_id.add_state_requests();
  example_request_with_unexpected_id.mutable_state_requests(0)->set_request_id(unexpected_request_id);
  ACA_LOG_INFO("Request two's request ID: %s\n",
               example_request_with_unexpected_id.state_requests(0).request_id().c_str());
  alcor::schema::HostRequestReply reply_two =
          g_grpc_server->RequestGoalStates(&example_request_with_unexpected_id);
  ACA_LOG_INFO("Reply two's operation status size: %d\n",
               reply_two.operation_statuses_size());
  ASSERT_EQ(reply_two.operation_statuses(0).operation_status(), OperationStatus::FAILURE);
  ASSERT_EQ(reply_two.operation_statuses(0).request_id(), unexpected_request_id);
}
