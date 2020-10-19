/*
Copyright 2019 The Alcor Authors.

Licensed under the Apache License, Version 2.0 (the "License");
        you may not use this file except in compliance with the License.
        You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

        Unless required by applicable law or agreed to in writing, software
        distributed under the License is distributed on an "AS IS" BASIS,
        WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
        See the License for the specific language governing permissions and
        limitations under the License.
*/
package com.futurewei.aca.securitygroup;

import com.futurewei.alcor.schema.Common;
import com.futurewei.alcor.schema.Common.EtherType;
import com.futurewei.alcor.schema.Common.Protocol;
import com.futurewei.alcor.schema.Common.OperationType;
import com.futurewei.alcor.schema.Vpc.VpcState;
import com.futurewei.alcor.schema.Vpc.VpcConfiguration;
import com.futurewei.alcor.schema.Subnet.SubnetState;
import com.futurewei.alcor.schema.Subnet.SubnetConfiguration;
import com.futurewei.alcor.schema.Port.PortState;
import com.futurewei.alcor.schema.Port.PortConfiguration;
import com.futurewei.alcor.schema.SecurityGroup.SecurityGroupState;
import com.futurewei.alcor.schema.SecurityGroup.SecurityGroupConfiguration;
import com.futurewei.alcor.schema.SecurityGroup.SecurityGroupConfiguration.SecurityGroupRule;
import com.futurewei.alcor.schema.SecurityGroup.SecurityGroupConfiguration.Direction;
import com.futurewei.alcor.schema.GoalStateProvisionerGrpc.GoalStateProvisionerBlockingStub;
import com.futurewei.alcor.schema.GoalStateProvisionerGrpc;
import com.futurewei.alcor.schema.Goalstate.GoalState;
import com.futurewei.alcor.schema.Goalstateprovisioner.GoalStateOperationReply;
import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;

public class SecurityGroupTest {
    private ManagedChannel managedChannel;

    public SecurityGroupTest() {
        this.managedChannel = ManagedChannelBuilder.forAddress("192.168.131.131",50001)
                .usePlaintext().build();
    }

    private VpcState buildVpcState() {
        VpcConfiguration.Builder vpcConfigBuilder = VpcConfiguration.newBuilder();
        vpcConfigBuilder.setName("vpc1");
        vpcConfigBuilder.setCidr("10.10.10.0/24");
        vpcConfigBuilder.setId("vpc_id1");

        VpcState.Builder vpcStateBuilder = VpcState.newBuilder();
        vpcStateBuilder.setOperationType(OperationType.CREATE);
        vpcStateBuilder.setConfiguration(vpcConfigBuilder.build());

        return vpcStateBuilder.build();
    }

    private SubnetState buildSubnetState() {
        SubnetConfiguration.Builder subnetConfigBuilder = SubnetConfiguration.newBuilder();
        subnetConfigBuilder.setName("subnet1");
        subnetConfigBuilder.setCidr("10.10.10.0/24");
        subnetConfigBuilder.setId("subnet_id1");

        SubnetState.Builder subnetStateBuilder = SubnetState.newBuilder();
        subnetStateBuilder.setOperationType(OperationType.CREATE);
        subnetStateBuilder.setConfiguration(subnetConfigBuilder.build());

        return subnetStateBuilder.build();
    }

    private PortState buildPortState() {
        PortConfiguration.Builder portConfigBuilder = PortConfiguration.newBuilder();
        portConfigBuilder.setName("port1");
        portConfigBuilder.setVpcId("vpc_id1");
        portConfigBuilder.setId("port_id1");
        portConfigBuilder.setFormatVersion(1);
        portConfigBuilder.setRevisionNumber(1);
        portConfigBuilder.setAdminStateUp(true);
        portConfigBuilder.setMessageType(Common.MessageType.FULL);
        portConfigBuilder.setMacAddress("7E:04:D0:C9:12:6C");
        portConfigBuilder.addFixedIps(PortConfiguration.FixedIp.newBuilder()
                .setIpAddress("10.10.10.2").setSubnetId("subnet_id1").build());
        PortConfiguration.SecurityGroupId.Builder securityGroupIdBuilder = PortConfiguration.SecurityGroupId.newBuilder();
        securityGroupIdBuilder.setId("security_group_id1");
        portConfigBuilder.addSecurityGroupIds(securityGroupIdBuilder.build());

        PortState.Builder portStateBuilder = PortState.newBuilder();
        portStateBuilder.setOperationType(OperationType.CREATE);
        portStateBuilder.setConfiguration(portConfigBuilder.build());

        return portStateBuilder.build();
    }

    private SecurityGroupState buildSecurityGroupState() {
        SecurityGroupConfiguration.Builder securityGroupConfigBuilder = SecurityGroupConfiguration.newBuilder();
        securityGroupConfigBuilder.setName("security_group1");
        securityGroupConfigBuilder.setVpcId("vpc_id1");
        securityGroupConfigBuilder.setId("security_group_id1");
        securityGroupConfigBuilder.setFormatVersion(1);
        securityGroupConfigBuilder.setRevisionNumber(1);
        SecurityGroupRule.Builder securityGroupRuleBuilder = SecurityGroupRule.newBuilder();
        securityGroupRuleBuilder.setOperationType(OperationType.CREATE);
        securityGroupRuleBuilder.setId("security_group_rule_id1");
        securityGroupRuleBuilder.setSecurityGroupId("security_group_id1");
        securityGroupRuleBuilder.setDirection(Direction.EGRESS);
        securityGroupRuleBuilder.setEthertype(EtherType.IPV4);
        securityGroupRuleBuilder.setProtocol(Protocol.TCP);
        securityGroupRuleBuilder.setPortRangeMin(100);
        securityGroupRuleBuilder.setPortRangeMax(101);
        securityGroupRuleBuilder.setRemoteIpPrefix("12.12.12.0/24");
        securityGroupConfigBuilder.addSecurityGroupRules(securityGroupRuleBuilder.build());

        SecurityGroupState.Builder securityGroupStateBuilder = SecurityGroupState.newBuilder();
        securityGroupStateBuilder.setOperationType(OperationType.CREATE);
        securityGroupStateBuilder.setConfiguration(securityGroupConfigBuilder.build());

        return securityGroupStateBuilder.build();
    }

    public void pushNetworkResourceStatesTest() {
        GoalStateProvisionerBlockingStub blockingStub =
                GoalStateProvisionerGrpc.newBlockingStub(managedChannel);

        GoalState.Builder builder = GoalState.newBuilder();
        builder.setFormatVersion(1);
        builder.addVpcStates(buildVpcState());
        builder.addSubnetStates(buildSubnetState());
        builder.addPortStates(buildPortState());
        builder.addSecurityGroupStates(buildSecurityGroupState());

        GoalStateOperationReply goalStateOperationReply
                = blockingStub.pushNetworkResourceStates(builder.build());

        System.out.println(goalStateOperationReply.toString());

        managedChannel.shutdown();
    }

    public static void main(String[] args) {
        SecurityGroupTest securityGroupTest = new SecurityGroupTest();
        securityGroupTest.pushNetworkResourceStatesTest();
    }
}
