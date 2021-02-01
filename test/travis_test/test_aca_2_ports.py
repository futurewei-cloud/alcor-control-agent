# SPDX-License-Identifier: MIT
# Copyright (c) 2020 The Authors.

# Authors: Phu Tran          <@phudtran>


import os
import json
from time import sleep
import common.node as node
import common.common as common

def setUp():
    node_1 = node.Node("aca_node_PARENT", "tenant_network", common.CONSTANTS.ACA_NODE)
    node_2 = node.Node("aca_node_CHILD", "tenant_network", common.CONSTANTS.ACA_NODE)
    testcases_to_run = ['DISABLED_2_ports_CREATE_test_traffic_CHILD',
                        'DISABLED_2_ports_CREATE_test_traffic_PARENT']
    cmd = (f'''bash -c '\
        cd /mnt/alcor-control-agent && ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*{testcases_to_run[0]}' ''')
    node_1.run(cmd)
    cmd = (f'''bash -c '\
        cd /mnt/alcor-control-agent && ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*{testcases_to_run[1]}' ''')
    node_2.run(cmd)

if __name__=='__main__':
    setUp()