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
  HostRequest example_request;
  example_request.add_state_requests();
  ACA_LOG_INFO("Channel state: %d\n", g_grpc_server->chan_->GetState(false));
  alcor::schema::HostRequestReply reply = g_grpc_server->RequestGoalStates(&example_request);
  ASSERT_EQ(reply.operation_statuses(0).operation_status(), OperationStatus::SUCCESS);
}
