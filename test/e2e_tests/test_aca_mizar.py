# Copyright (c) 2019 The Authors.
#
# Authors: Sherif Abdelwahab <@zasherif>
#          Phu Tran          <@phudtran>
#          Eric Li           <@er1cthe0ne>   
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.

#from test.trn_controller.controller import controller
from test.e2e_test.aca_droplet import aca_droplet
#from test.trn_controller.common import cidr
#from test.trn_controller.endpoint import endpoint
import unittest
#from time import sleep

class test_aca_mizar(unittest.TestCase):

    def setUp(self):

        groupID = "test5"
        prefix = "-d -h es7"
        self.aca_droplet = {}
        n_droplets = 20
        for i in range(n_droplets):
            id = "d" + str(i)
            name = "ephost_" + str(i)
            aca_options = prefix + "-subnet1-ep" + str(i) + " -g " + groupID
            self.aca_droplet[id] = aca_droplet(name, aca_options) 
        self.aca_droplet["d200"] = aca_droplet("switchhost_0", prefix + "-subnet1-transit-switch1 -g " + groupID)
        self.aca_droplet["d201"] = aca_droplet("switchhost_1", prefix + "-subnet1-transit-switch2 -g " + groupID)
        self.aca_droplet["d202"] = aca_droplet("switchhost_2", prefix + "-subnet1-transit-switch3 -g " + groupID)

    def test_aca_mizar(self):
        open('machine_config.json', 'w').close()
        output = open("machine_config.json", "a")
        print("Network Agent not yet loaded")
        input("Press Enter to continue...")
        for d in self.aca_droplet.values():
            output.write(d.get_net_state())
            # Run network control agent in the background
            # TODO: fix below
            d.run(d.nca_command, True)        

        output.close()
        print("Network Agent now loaded")
        input("Press Enter to cleanup")
