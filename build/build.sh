# MIT License
# Copyright(c) 2020 Futurewei Cloud
#
#     Permission is hereby granted,
#     free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction,
#     including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons
#     to whom the Software is furnished to do so, subject to the following conditions:
#
#     The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
#
#     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
#     WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#!/bin/bash

BUILD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
echo "build path is $BUILD"

# Initialize the needed mizar submodule
git submodule update --init --recursive

# Print out the git branch and log
echo "--- try to print branch name and git log ---"

git branch
git log

# Create and Start the build contrainer
docker build -f $BUILD/Dockerfile -t aca_build0 .
docker rm -f a1 || true
docker create -v $BUILD/..:/mnt/host/code -it --privileged --cap-add=NET_ADMIN --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name a1 aca_build0:latest /bin/bash
docker start a1

# Build mizar first
# docker exec a1 bash -c "cd /mnt/host/code/mizar && make"

if [ "$1" != "test" ]; then
  # Build alcor control agent
  echo "--- check what is in src/ovs/aca_ovs_l3_programmer.cpp ---"

  docker exec a1 bash -c "cat /mnt/host/code/src/ovs/aca_ovs_l3_programmer.cpp"

  echo "--- building alcor-control-agent ---"
  docker exec a1 bash -c "cd /mnt/host/code && cmake . && make && \
    /etc/init.d/openvswitch-switch restart && \
    ovs-vswitchd --pidfile --detach"
else
  sed -i.bak -E 's/("add-br )([a-z]+-[a-z]+)(")/\1\2 -- set bridge \2 datapath_type=netdev\3/g' $BUILD/../src/ovs/aca_ovs_l2_programmer.cpp
  # Build alcor control agent
  echo "--- building alcor-control-agent pre test ---"
  docker exec a1 bash -c "cd /mnt/host/code && cmake . && make"

  echo "--- Start ACA Unit test ---"
  echo "    --- rebuilding br-tun and br-int"
  docker exec a1 bash -c ' \
    ovs-ctl --delete-bridges stop && \
    rm -rf /etc/openvswitch /usr/local/etc/openvswitch && \
    mkdir -p /var/log/openvswitch && \
    mkdir -p /etc/openvswitch && \
    ovsdb-tool create /etc/openvswitch/conf.db /usr/local/share/openvswitch/vswitch.ovsschema && \
    mkdir -p /var/run/openvswitch && \
    ovsdb-server --remote=punix:/var/run/openvswitch/db.sock --remote=db:Open_vSwitch,Open_vSwitch,manager_options --pidfile --detach --log-file && \
    ovs-vsctl --no-wait init && \
    ovs-vswitchd --pidfile --detach --log-file && \
    ovs-vsctl add-br br-int -- set bridge br-int datapath_type=netdev && \
    ovs-vsctl add-br br-tun -- set bridge br-tun datapath_type=netdev && \
    ovs-vsctl \
    -- add-port br-int patch-tun \
    -- set interface patch-tun type=patch options:peer=patch-int \
    -- add-port br-tun patch-int \
    -- set interface patch-int type=patch options:peer=patch-tun && \
    ovs-ofctl add-flow br-tun "table=0, priority=1,in_port="patch-int" actions=resubmit(,2)" && \
    ovs-ofctl add-flow br-tun "table=2, priority=0 actions=resubmit(,22)"'

  echo "    --- running alcor-control-agent ---"
  # sends output to null device, but stderr to console 
  docker exec a1 bash -c "nohup /mnt/host/code/build/bin/AlcorControlAgent -d > /dev/null 2>&1 &"

  echo "    --- Running unit test cases ---"
  docker exec a1 bash -c "cd /mnt/host/code/build/tests && ./aca_tests && ./gs_tests"

  mv -f $BUILD/../src/ovs/aca_ovs_l2_programmer.cpp.bak $BUILD/../src/ovs/aca_ovs_l2_programmer.cpp
  # Build alcor control agent
  echo "--- building alcor-control-agent post test ---"
  docker exec a1 bash -c "cd /mnt/host/code && make"

fi

