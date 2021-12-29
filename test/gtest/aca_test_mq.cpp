//
// Created by FangJ on 2021/11/29.
//
// MIT License
// Copyright(c) 2020 Futurewei Cloud
//
//     Permission is hereby granted,
//     free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction,
//     including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons
//     to whom the Software is furnished to do so, subject to the following conditions:
//
//     The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
//     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//     WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "aca_log.h"
#include "gtest/gtest.h"
#include "goalstate.pb.h"
#include "aca_grpc.h"
#include "aca_grpc_client.h"
#include "aca_message_pulsar_producer.h"
#include "aca_message_pulsar_consumer.h"
#include "aca_ovs_l2_programmer.h"
#include "aca_ovs_control.h"
#define ACALOGNAME "AlcorControlAgentTest"

using namespace std;
using namespace aca_message_pulsar;
using namespace aca_ovs_l2_programmer;
using aca_ovs_control::ACA_OVS_Control;


extern string project_id;
extern string vpc_id_1;
extern string vpc_id_2;
extern string subnet_id_1;
extern string subnet_id_2;
extern string port_id_1;
extern string port_id_2;
extern string port_id_3;
extern string port_id_4;
extern string port_name_1;
extern string port_name_2;
extern string port_name_3;
extern string port_name_4;
extern string vmac_address_1;
extern string vmac_address_2;
extern string vmac_address_3;
extern string vmac_address_4;
extern string vip_address_1;
extern string vip_address_2;
extern string vip_address_3;
extern string vip_address_4;
extern string remote_ip_1; // for docker network
extern string remote_ip_2; // for docker network
extern bool g_demo_mode;

extern void aca_test_reset_environment();
extern void aca_test_create_default_port_state(PortState *new_port_states);
extern void aca_test_create_default_subnet_state(SubnetState *new_subnet_states);
extern void aca_test_1_neighbor_CREATE_DELETE(NeighborType input_neighbor_type);
extern void aca_test_1_neighbor_CREATE_DELETE_V2(NeighborType input_neighbor_type);
extern void aca_test_1_port_CREATE_plus_neighbor_CREATE(NeighborType input_neighbor_type);
extern void aca_test_1_port_CREATE_plus_neighbor_CREATE_V2(NeighborType input_neighbor_type);
extern void aca_test_10_neighbor_CREATE(NeighborType input_neighbor_type);
extern void aca_test_10_neighbor_CREATE_V2(NeighborType input_neighbor_type);
extern void aca_test_1_port_CREATE_plus_N_neighbors_CREATE(NeighborType input_neighbor_type,
                                                           uint neighbors_to_create);
extern void
aca_test_1_port_CREATE_plus_N_neighbors_CREATE_V2(NeighborType input_neighbor_type,
                                                  uint neighbors_to_create);

static string mq_broker_ip = "pulsar://localhost:6650"; //for the broker running in localhost
static string mq_test_topic = "Host-ts-1";
static string mq_subscription = "test_subscription";
static string mq_key="9192a4d4-ffff-4ece-b3f0-8d36e3d88001"; // 3dda2801-d675-4688-a63f-dcda8d327f50  9192a4d4-ffff-4ece-b3f0-8d36e3d88001
static int mq_hash=49775; //  21485  49775

//
// Test suite: pulsar_test_cases
//
// Note: it requires a pulsar setup on localhost therefore this test is DISABLED by default.
// You will need three terminals:
//      Terminal(1): run pulsar standalone.
//      Terminal(2): run pulsar consumer test case.
//      Terminal(3): run pulsar producer test cases.


// This case tests the pulsar consumer implementation.
// First run this case by executing:
//    ./aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_pulsar_consumer_test
// Then run the following producer test cases.

TEST(pulsar_test_cases, DISABLED_pulsar_consumer_test)
{
    bool previous_demo_mode = g_demo_mode;
    g_demo_mode = true;

    aca_test_reset_environment();
    ACA_Message_Pulsar_Consumer consumer=ACA_Message_Pulsar_Consumer::get_instance();
    consumer.init(mq_test_topic, mq_broker_ip, mq_subscription);
//    ACA_Message_Pulsar_Consumer consumer(mq_test_topic, mq_broker_ip, mq_subscription);
    consumer.multicastConsumerDispatched();
    pause();

    g_demo_mode = previous_demo_mode;
}

//    sudo ./aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_pulsar_unicast_consumer_test
TEST(pulsar_test_cases, DISABLED_pulsar_unicast_consumer_test)
{
    string cmd_string;


    bool previous_demo_mode = g_demo_mode;
    g_demo_mode = true;

    aca_test_reset_environment();

    ACA_Message_Pulsar_Consumer consumer = ACA_Message_Pulsar_Consumer::get_instance();
    consumer.init(mq_test_topic, mq_broker_ip, mq_subscription);
    consumer.unicastConsumerDispatched(mq_hash);
    pause();

    g_demo_mode = previous_demo_mode;
}

//    sudo ./aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_pulsar_unicast_consumer_recover_test
TEST(pulsar_test_cases, DISABLED_pulsar_unicast_consumer_recover_test)
{
    string cmd_string;
    string mq_update_topic="update topic";
    bool previous_demo_mode = g_demo_mode;
    g_demo_mode = true;

    aca_test_reset_environment();

    auto* pt=
            new ACA_Message_Pulsar_Consumer(mq_test_topic,mq_broker_ip,mq_subscription);
    pt->unicastConsumerDispatched(mq_hash);
    pt->unicastResubscribe(mq_update_topic,mq_hash);
    delete pt;

    pt=
            new ACA_Message_Pulsar_Consumer(ACA_Message_Pulsar_Consumer::recovered_topic,mq_broker_ip,mq_subscription);
    pt->unicastConsumerDispatched(mq_hash);

    pause();

    g_demo_mode = previous_demo_mode;
}

//  sudo ./aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_pulsar_unicast_consumer_resubscribe_test
TEST(pulsar_test_cases, DISABLED_pulsar_unicast_consumer_resubscribe_test)
{
    string cmd_string;
    string mq_update_topic="update topic";
    bool previous_demo_mode = g_demo_mode;
    g_demo_mode = true;

    aca_test_reset_environment();

    ACA_Message_Pulsar_Consumer consumer(mq_update_topic, mq_broker_ip, mq_subscription);
    consumer.unicastConsumerDispatched(mq_hash);
    consumer.unicastResubscribe(true);
    consumer.unicastResubscribe(false,mq_test_topic, to_string(mq_hash));
    pause();

    g_demo_mode = previous_demo_mode;
}

// This case tests the pulsar producer implementation and publishes a GoalState to the subscribed topic.
// First run pulsar_consumer_test then execute
//      sudo ./aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_pulsar_producer_test
TEST(pulsar_test_cases, DISABLED_pulsar_producer_test)
{
    int retcode = 0;
    int overall_rc=0;
    int length=1000;
    ulong not_care_culminative_time;
    string cmd_string;
    string GoalStateString;
    unsigned char serializedGoalState[length];

    GoalStateV2 GoalState_builder;
    PortState new_port_states;
    SubnetState new_subnet_states;

    aca_test_reset_environment();

    aca_test_create_default_port_state(&new_port_states);
    auto &port_states_map = *GoalState_builder.mutable_port_states();
    port_states_map[port_id_1] = new_port_states;

    aca_test_create_default_subnet_state(&new_subnet_states);
    auto &subnet_states_map = *GoalState_builder.mutable_subnet_states();
    subnet_states_map[subnet_id_1] = new_subnet_states;


    if(GoalState_builder.SerializeToString(&GoalStateString)){
        ACA_LOG_INFO("%s","Successfully covert GoalStateV2 to message\n");
    }

    ACA_Message_Pulsar_Producer producer(mq_broker_ip, mq_test_topic);
    retcode = producer.publish(GoalStateString,mq_key);
    EXPECT_EQ(retcode, EXIT_SUCCESS);

    ACA_LOG_INFO("%s","Waiting for GoalStateV2 update.\n");
    sleep(1);

    ACA_OVS_L2_Programmer::get_instance().execute_ovsdb_command(
            "get Interface " + port_name_1 + " ofport", not_care_culminative_time, overall_rc);
    EXPECT_EQ(overall_rc, EXIT_SUCCESS);
    overall_rc = EXIT_SUCCESS;

}