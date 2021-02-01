# SPDX-License-Identifier: MIT
# Copyright (c) 2020 The Authors.

# Authors: Phu Tran          <@phudtran>

import unittest
import os
import json
from time import sleep
from .common.node import Node
from .common.common import logger, CONSTANTS


class test_aca_2_ports(unittest.TestCase):

    def setUp(self):
        self.node_1 = Node("aca_node_PARENT", "tenant_network", CONSTANTS.ACA_NODE)
        self.node_2 = Node("aca_node_CHILD", "tenant_network", CONSTANTS.ACA_NODE)
        testcases_to_run = ['DISABLED_2_ports_CREATE_test_traffic_CHILD',
                            'DISABLED_2_ports_CREATE_test_traffic_PARENT']
        cmd = (f'''bash -c '\
cd /mnt/alcor-control-agent && ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*{testcases_to_run[0]}' ''')
        self.node_1.run(cmd)
        cmd = (f'''bash -c '\
cd /mnt/alcor-control-agent && ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*{testcases_to_run[1]}' ''')
        self.node_2.run(cmd)

if __name__=='__main__':
    unittest.main()