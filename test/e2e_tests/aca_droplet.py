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
        """
        (aca version) call base droplet init, and initialize aca specific property
        """
        super().__init__(self, id)

        self.aca_command = f'''./aliothcontroller/AlcorControlAgent {aca_options}'''

    def _create_docker_container(self):
        """
        (aca version) Create and initialize a docker container.
        Assumes "buildbox:v2" image exist and setup on host
        """
        cwd = os.getcwd()

        # get a docker client
        docker_client = docker.from_env()
        docker_image = "buildbox:v2"
        mount_pnt = docker.types.Mount("/mnt/Transit",
                                       cwd,
                                       type='bind')

        mount_modules = docker.types.Mount("/lib/modules",
                                           "/lib/modules",
                                           type='bind')

        # Create the container in previlaged mode
        container = docker_client.containers.create(
            docker_image, '/bin/bash', tty=True,
            stdin_open=True, auto_remove=False, mounts=[mount_pnt, mount_modules],
            privileged=True, cap_add=["SYS_PTRACE"],
            ports={str(droplet.port_internal) + "/tcp": ('0.0.0.0', droplet.port_external)},
            security_opt=["seccomp=unconfined"])
        container.start()
        container.reload()

        # Increment the static external port number
        droplet.port_external = droplet.port_external + 1

        # Restart dependancy services
        container.exec_run("/etc/init.d/rpcbind restart")
        container.exec_run("/etc/init.d/rsyslog restart")
        container.exec_run("ip link set dev eth0 up mtu 9000")

        # We may need ovs for compatability tests
        # container.exec_run("/etc/init.d/openvswitch-switch restart")

        # Create simlinks
        container.exec_run("ln -s /mnt/Transit/build/bin /trn_bin")
        container.exec_run("ln -s /mnt/Transit/build/xdp /trn_xdp")
        container.exec_run("ln -s /sys/fs/bpf /bpffs")
        container.exec_run(
            "ln - s /mnt/Transit/test/trn_func_tests/output /trn_test_out")

        container.exec_run(
            "ln -s /mnt/Transit/aliothcontroller/ /aliothcontroller")

        # Run the transitd in the background
        container.exec_run("/trn_bin/transitd ",
                           detach=True)

        # Enable debug and tracing for the kernel
        container.exec_run(
            "mount -t debugfs debugfs /sys/kernel/debug")
        container.exec_run(
            "echo 1 > /sys/kernel/debug/tracing/tracing_on")

        # Enable core dumps (just in case!!)
        container.exec_run("ulimit -u")
        cmd = "echo '/mnt/Transit/core/core_{}_%e.%p' |\
 tee /proc/sys/kernel/core_pattern ".format(self.ip)
        container.exec_run(cmd)

        self.container = container
        self.ip = self.container.attrs['NetworkSettings']['IPAddress']
        self.mac = self.container.attrs['NetworkSettings']['MacAddress']

    def get_net_state(self):
        """
        (aca version) Get the configuration in json format.
        Need to be fixed to generate correct formatting.
        """
        json_ns_state = {}
        if (len(self.veth_peers) != 0):
            json_ns_state = {
                "veth": "veth0",
                "namespace": self.veth_peers[0],
                "peer": self.veth_peers[1],
                "ip": self.veth_peers[2],
                "mac": self.veth_peers[3]
            }

        json_host_state = {
            "id": self.id,
            "ip": self.ip,
            "mac": self.mac,
            "veth": json_ns_state
        }
        return json.dumps(json_host_state)
