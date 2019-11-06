# Copyright (c) 2019 The Authors.
#
# Authors: Eric Li           <@er1cthe0ne>
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.

#from test.trn_controller.common import cidr, logger, run_cmd
from Transit.e2e_test.aca_droplet import aca_droplet
import os
import docker
import time
import json

class aca_droplet(droplet):
    # "static" variable for all droplets
    port_internal = 50001
    port_external = 50001

    def __init__(self, id, aca_options, droplet_type='docker', phy_itf='eth0'):
        super().__init__(self, id)

        self.aca_command = f'''./aliothcontroller/AlcorControlAgent {aca_options}'''
