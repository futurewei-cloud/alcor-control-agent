#!/bin/bash

BUILD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
code_dir=$(dirname $BUILD)
code_dir=$(dirname $code_dir)
echo "build path is $BUILD"
echo "code path is $code_dir"

# Initialize the needed mizar submodule
git submodule update --init --recursive

# Create and Start the build contrainer
docker build -f $code_dir/build/Dockerfile -t aca_build0 .
docker rm -f aca_PARENT || true
docker create -v $code_dir:/mnt/host/code -it --privileged --cap-add=NET_ADMIN --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name aca_PARENT aca_build0:latest /bin/bash
docker start aca_PARENT
docker rm -f aca_CHILD || true
docker create -v $code_dir:/mnt/host/code -it --privileged --cap-add=NET_ADMIN --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name aca_CHILD aca_build0:latest /bin/bash
docker start aca_CHILD

# Build mizar first
# docker exec a1 bash -c "cd /mnt/host/code/mizar && make"

if [ "$1" != "test" ]; then
  # Build alcor control agent
  echo "--- building alcor-control-agent ---"
  docker exec aca_PARENT bash -c "cd /mnt/host/code && cmake . && make"
  docker exec aca_CHILD bash -c "cd /mnt/host/code && cmake . && make"
else if [ "$1" == "2_port_test" ]; then
  echo "--- building alcor-control-agent ---"
  docker exec aca_PARENT bash -c "cd /mnt/host/code && cmake . && make"
  docker exec aca_CHILD bash -c "cd /mnt/host/code && cmake . && make"
  echo "    --- running alcor-control-agent ---"
  # sends output to null device, but stderr to console 
  docker exec aca_PARENT bash -c "nohup /mnt/host/code/build/bin/AlcorControlAgent -d > /dev/null 2>&1 &"
  docker exec aca_CHILD bash -c "nohup /mnt/host/code/build/bin/AlcorControlAgent -d > /dev/null 2>&1 &"
  echo "    --- running 2_ports_CREATE_test ---"
  # run DISABLED_2_ports_CREATE_test_traffic_PARENT and DISABLED_2_ports_CREATE_test_traffic_CHILD
  docker exec aca_PARENT bash -c "cd /mnt/host/code && ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=DISABLED_2_ports_CREATE_test_traffic_PARENT"
  docker exec aca_CHILD bash -c "cd /mnt/host/code && ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=DISABLED_2_ports_CREATE_test_traffic_CHILD"
else
  sed -i.bak -E 's/("add-br )([a-z]+-[a-z]+)(")/\1\2 -- set bridge \2 datapath_type=netdev\3/g' $code_dir/src/ovs/aca_ovs_l2_programmer.cpp
  # Build alcor control agent
  echo "--- building alcor-control-agent pre test ---"
  docker exec aca_PARENT bash -c "cd /mnt/host/code && cmake . && make"
  docker exec aca_CHILD bash -c "cd /mnt/host/code && cmake . && make"

  echo "--- Start ACA Unit test ---"
  echo "    --- rebuilding br-tun and br-int"
  docker exec aca_PARENT bash -c ' \
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

    docker exec aca_CHILD bash -c ' \
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
  docker exec aca_PARENT bash -c "nohup /mnt/host/code/build/bin/AlcorControlAgent -d > /dev/null 2>&1 &"
  docker exec aca_CHILD bash -c "nohup /mnt/host/code/build/bin/AlcorControlAgent -d > /dev/null 2>&1 &"

  echo "    --- Running unit test cases ---"
  docker exec aca_PARENT bash -c "cd /mnt/host/code/build/tests && ./aca_tests && ./gs_tests"
  docker exec aca_CHILD bash -c "cd /mnt/host/code/build/tests && ./aca_tests && ./gs_tests"

  mv -f $code_dir/src/ovs/aca_ovs_l2_programmer.cpp.bak $code_dir/src/ovs/aca_ovs_l2_programmer.cpp
  # Build alcor control agent
  echo "--- building alcor-control-agent post test ---"
  docker exec aca_PARENT bash -c "cd /mnt/host/code && make"
  docker exec aca_CHILD bash -c "cd /mnt/host/code && make"

fi