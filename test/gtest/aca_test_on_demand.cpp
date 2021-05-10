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
  grpc::CompletionQueue cq;
  example_request_with_expected_id.add_state_requests();
  string expected_request_id = "12345";
  string unexpected_request_id = "54321";

  example_request_with_expected_id.mutable_state_requests(0)->set_request_id(expected_request_id);
  ACA_LOG_INFO("Channel state: %d\n", g_grpc_server->chan_->GetState(false));
  ACA_LOG_INFO("Request one's request ID: %s\n",
               example_request_with_expected_id.state_requests(0).request_id().c_str());
  g_grpc_server->RequestGoalStates(&example_request_with_expected_id, &cq);

  HostRequest example_request_with_unexpected_id;
  example_request_with_unexpected_id.add_state_requests();
  example_request_with_unexpected_id.mutable_state_requests(0)->set_request_id(unexpected_request_id);
  ACA_LOG_INFO("Request two's request ID: %s\n",
               example_request_with_unexpected_id.state_requests(0).request_id().c_str());

  g_grpc_server->RequestGoalStates(&example_request_with_unexpected_id, &cq);

  void *got_tag;
  bool ok = false;
  HostRequestReply_HostRequestOperationStatus hostOperationStatus;
  OperationStatus replyStatus;

  ACA_LOG_INFO("%s\n", "Beginning of process_async_grpc_replies");
  int counter = 0;
  while (cq.Next(&got_tag, &ok)) {
    counter = counter + 1;
    ACA_LOG_INFO("Inside the while loop, counter: %d\n", counter);
    if (ok) {
      ACA_LOG_INFO("%s\n", "This call is OK");
      AsyncClientCall *call = static_cast<AsyncClientCall *>(got_tag);
      ACA_LOG_INFO("%s\n", "Static cast done successfully");
      if (call->status.ok()) {
        ACA_LOG_INFO("%s\n", "Got an GRPC reply that is OK, need to process it.");
        for (int i = 0; i < call->reply.operation_statuses_size(); i++) {
          hostOperationStatus = call->reply.operation_statuses(i);
          replyStatus = hostOperationStatus.operation_status();
          ACA_LOG_INFO("This reply has request ID: %s\n",
                       hostOperationStatus.request_id().c_str());
          if (hostOperationStatus.request_id() == expected_request_id) {
            ASSERT_EQ(hostOperationStatus.operation_status(), OperationStatus::SUCCESS);
          } else {
            ASSERT_EQ(hostOperationStatus.operation_status(), OperationStatus::FAILURE);
          }
        }
        ACA_LOG_DEBUG("Return from NCM - Reply Status: %s\n",
                      to_string(replyStatus).c_str());
      } else {
        ACA_LOG_INFO("%s, error details: %s\n", "Call->status.ok() is false",
                     call->status.error_message().c_str());
      }
      delete call;
    } else {
      ACA_LOG_INFO("%s\n", "Got an GRPC reply that is NOT OK, don't need to process the data");
    }
    if (counter == 2)
      break;
  }
}
