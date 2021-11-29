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
#include "aca_net_config.h"
#include "aca_comm_mgr.h"

using namespace std;
using namespace aca_message_pulsar;
using aca_net_config::Aca_Net_Config;
using aca_comm_manager::Aca_Comm_Manager;
#define ACALOGNAME "AlcorControlAgentTest"


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
extern string subnet1_gw_ip;
extern string subnet2_gw_ip;
extern string subnet1_gw_mac;
extern string subnet2_gw_mac;
static string subnet1_cidr = "10.10.0.0/24";
static string subnet2_cidr = "10.10.1.0/24";

extern string remote_ip_1; // for docker network
extern string remote_ip_2; // for docker network

extern void aca_test_reset_environment();
GoalState buildGoalState();

static string mq_broker_ip = "pulsar://localhost:6650"; //for the broker running in localhost
static string mq_test_topic = "Host-ts-1";

//
// Test suite: pulsar_test_cases
//
// Testing the pulsar implementation where AlcorControlAgent is the consumer
// and aca_test is acting as producer
// Note: it will require a pulsar setup on localhost therefore this test is DISABLED by default
//   it can be executed by:
//
//     ./aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_pulsar_consumer_test
//

TEST(pulsar_test_cases, DISABLED_pulsar_consumer_test)
{
    int retcode = 0;
    int overall_rc=0;
    string cmd_string;

    int seralizedLength=10000;
    unsigned char serializedGoalState[seralizedLength];
    string GoalStateString = "Test Message";
    GoalState mGoalState=buildGoalState();

    aca_test_reset_environment();

    overall_rc = Aca_Net_Config::get_instance().execute_system_command(
            "docker run -itd --cap-add=NET_ADMIN --name con3 --net=none alpine sh");
    EXPECT_EQ(overall_rc, EXIT_SUCCESS);

    cmd_string = "ovs-docker add-port br-int eth1 con3 --macaddress=" + vmac_address_1 + " --ipaddress="+ vip_address_1 + "/24";
    overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
    EXPECT_EQ(overall_rc, EXIT_SUCCESS);
    overall_rc = EXIT_SUCCESS;

    if(mGoalState.SerializePartialToArray(serializedGoalState,seralizedLength)){
        ACA_LOG_INFO("%s","Successfully covert protobuf struct to message\n");
    }
    GoalStateString.append(reinterpret_cast<const char*> (serializedGoalState));
    ACA_Message_Pulsar_Producer producer(mq_broker_ip, mq_test_topic);
    retcode = producer.publish(GoalStateString);
    EXPECT_EQ(retcode, EXIT_SUCCESS);

    // test valid traffic from parent to child
    cmd_string = "docker exec con3 ping -c1 " + vip_address_3;
    overall_rc = Aca_Net_Config::get_instance().execute_system_command(cmd_string);
    EXPECT_EQ(overall_rc, EXIT_SUCCESS);
    overall_rc = EXIT_SUCCESS;

    //clean up
    overall_rc = Aca_Net_Config::get_instance().execute_system_command(
            "ovs-docker del-ports br-int con3");
    EXPECT_EQ(overall_rc, EXIT_SUCCESS);

    overall_rc = Aca_Net_Config::get_instance().execute_system_command("docker rm con3 -f");
    EXPECT_EQ(overall_rc, EXIT_SUCCESS);
}


GoalState buildGoalState(){
    GoalState GoalState_builder;

    PortState *new_port_states = GoalState_builder.add_port_states();
    new_port_states->set_operation_type(OperationType::CREATE);

    // fill in port state structs for port 1
    PortConfiguration *PortConfiguration_builder = new_port_states->mutable_configuration();
    PortConfiguration_builder->set_revision_number(1);
    PortConfiguration_builder->set_update_type(UpdateType::FULL);
    PortConfiguration_builder->set_id(port_id_1);

    PortConfiguration_builder->set_vpc_id(vpc_id_1);
    PortConfiguration_builder->set_name(port_name_1);
    PortConfiguration_builder->set_mac_address(vmac_address_1);
    PortConfiguration_builder->set_admin_state_up(true);

    PortConfiguration_FixedIp *FixedIp_builder = PortConfiguration_builder->add_fixed_ips();
    FixedIp_builder->set_subnet_id(subnet_id_1);
    FixedIp_builder->set_ip_address(vip_address_1);

    PortConfiguration_SecurityGroupId *SecurityGroup_builder =
            PortConfiguration_builder->add_security_group_ids();
    SecurityGroup_builder->set_id("1");


    SubnetState *new_subnet_states = GoalState_builder.add_subnet_states();
    new_subnet_states->set_operation_type(OperationType::INFO);

    // fill in subnet state structs
    SubnetConfiguration *SubnetConiguration_builder =
            new_subnet_states->mutable_configuration();
    SubnetConiguration_builder->set_revision_number(1);
    SubnetConiguration_builder->set_vpc_id(vpc_id_1);
    SubnetConiguration_builder->set_id(subnet_id_1);
    SubnetConiguration_builder->set_cidr("10.0.0.0/24");
    SubnetConiguration_builder->set_tunnel_id(20);

    NeighborState *new_neighbor_states = GoalState_builder.add_neighbor_states();
    new_neighbor_states->set_operation_type(OperationType::CREATE);

    // fill in neighbor state structs
    NeighborConfiguration *NeighborConfiguration_builder =
            new_neighbor_states->mutable_configuration();
    NeighborConfiguration_builder->set_revision_number(1);

    NeighborConfiguration_builder->set_vpc_id(vpc_id_1);
    NeighborConfiguration_builder->set_id(port_id_3);
    NeighborConfiguration_builder->set_mac_address(vmac_address_3);
    NeighborConfiguration_builder->set_host_ip_address(remote_ip_2);

    NeighborConfiguration_FixedIp *FixedIp_builder2 =
            NeighborConfiguration_builder->add_fixed_ips();
    FixedIp_builder2->set_neighbor_type(NeighborType::L2);
    FixedIp_builder2->set_subnet_id(subnet_id_1);
    FixedIp_builder2->set_ip_address(vip_address_3);

    return GoalState_builder;
}