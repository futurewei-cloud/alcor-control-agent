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

from test.trn_controller.droplet import droplet
import os
import docker
import time
import json

class aca_droplet(droplet):
    # "static" variables for all droplets
    port_internal = 50001
    port_external = 50001

    def __init__(self, id, aca_options):
        """
        Call the base droplet class init, and initialize aca specific property
        """
        super().__init__(id)
        self.aca_command = f'''./aca_build/bin/AlcorControlAgent {aca_options}'''

    def _create_docker_container(self):
        """
        Create and initialize a docker container.
        Assumes "aca_build0:latest" image exists and setup on host
        """
        cwd = os.getcwd()

        # get a docker client
        docker_client = docker.from_env()
        docker_image = "aca_build0:latest"
        mount_pnt = docker.types.Mount("/mnt/alcor-control-agent",
                                       f'''{cwd}/../..''',
                                       type='bind')

        mount_modules = docker.types.Mount("/lib/modules",
                                           "/lib/modules",
                                           type='bind')

        # Create the container in privileged mode
        container = docker_client.containers.create(
            docker_image, '/bin/bash', tty=True,
            stdin_open=True, auto_remove=False, mounts=[mount_pnt, mount_modules],
            privileged=True, cap_add=["SYS_PTRACE"],
            ports={str(aca_droplet.port_internal) + "/tcp": ('0.0.0.0', aca_droplet.port_external)},
            security_opt=["seccomp=unconfined"], name=self.id)
        container.start()
        container.reload()

        # Increment the static external port number counter
        aca_droplet.port_external = aca_droplet.port_external + 1

        # Restart dependancy services
        container.exec_run("/etc/init.d/rpcbind restart")
        container.exec_run("/etc/init.d/rsyslog restart")
        container.exec_run("ip link set dev eth0 up mtu 9000")

        # We may need to restart ovs
        # container.exec_run("/etc/init.d/openvswitch-switch restart")

        # Create simlinks
        container.exec_run("ln -s /mnt/alcor-control-agent/mizar/build/bin /trn_bin")
        container.exec_run("ln -s /mnt/alcor-control-agent/mizar/build/xdp /trn_xdp")
        container.exec_run("ln -s /sys/fs/bpf /bpffs")

        container.exec_run(
            "ln -s /mnt/alcor-control-agent/build/ /aca_build")

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
        cmd = "echo '/mnt/alcor-control-agent/mizar/core/core_{}_%e.%p' |\
 tee /proc/sys/kernel/core_pattern ".format(self.ip)
        container.exec_run(cmd)

        self.container = container
        self.ip = self.container.attrs['NetworkSettings']['IPAddress']
        self.mac = self.container.attrs['NetworkSettings']['MacAddress']

    def get_net_state(self):
        """
        Get the configuration in json format.
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
