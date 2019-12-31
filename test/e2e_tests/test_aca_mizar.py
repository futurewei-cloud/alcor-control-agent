# Copyright 2019 The Alcor Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from aca_droplet import aca_droplet
import unittest

class test_aca_mizar(unittest.TestCase):

    def setUp(self):
        groupID = "test5"
        prefix = "-d -h es7"
        self.aca_droplets = {}
        self.aca_droplets["d50001"] = aca_droplet("routerhost_0", prefix + "-vpc1-transit-router1 -g " + groupID)
        self.aca_droplets["d50002"] = aca_droplet("switchhost_0", prefix + "-subnet1-transit-switch1 -g " + groupID)
        self.aca_droplets["d50003"] = aca_droplet("switchhost_1", prefix + "-subnet1-transit-switch2 -g " + groupID)
        self.aca_droplets["d50004"] = aca_droplet("switchhost_2", prefix + "-subnet1-transit-switch3 -g " + groupID)
        n_aca_droplets = 10
        for i in range(n_aca_droplets):
            id = "d" + str(i)
            name = "ephost_" + str(i)
            aca_options = prefix + "-subnet1-ep" + str(i) + " -g " + groupID
            self.aca_droplets[id] = aca_droplet(name, aca_options) 

    def test_aca_mizar(self):
        open('machine.json', 'w').close()
        output = open("machine.json", "a")
        output.write("{ ")
        output.write("\"Hosts\": [ ")
        for d in self.aca_droplets.values():
            if (d.id != "routerhost_0"):
                output.write(", ")
            output.write(d.get_net_state())
            # Run alcor control agent in the background
            d.run(d.aca_command, True)        
        output.write("] ")
        output.write("}")
        output.close()
        print("Alcor Control Agent now loaded!")
        input("Press Enter to cleanup and destroy all the created containers.")
