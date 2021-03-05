#!/bin/bash

BUILD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
code_dir=$(dirname $BUILD)
code_dir=$(dirname $code_dir)
echo "build path is $BUILD"
echo "code path is $code_dir"

# Initialize the needed mizar submodule
# git submodule update --init --recursive

# Create and Start the build contrainer
if [ "$1" == "compile_and_run_unit_test" ]; then
  echo "    --- images list ---"
  docker images
  echo "    --- build image ---"
  docker build -f $code_dir/build/Dockerfile -t aca_build0 .
  echo "    --- images list ---"
  docker images
  echo "    --- container list ---"
  docker ps -a
  echo "    --- create container ---"
  docker rm -f aca_PARENT || true
  docker create -v $code_dir:/mnt/host/code -it --privileged --cap-add=NET_ADMIN --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name aca_PARENT aca_build0:latest /bin/bash
  docker start aca_PARENT
  docker exec aca_PARENT bash -c "/etc/init.d/openvswitch-switch restart && \
  ovs-vswitchd --pidfile --detach"
  docker rm -f aca_CHILD || true
  docker create -v $code_dir:/mnt/host/code -it --privileged --cap-add=NET_ADMIN --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name aca_CHILD aca_build0:latest /bin/bash
  docker start aca_CHILD
  docker exec aca_CHILD bash -c "/etc/init.d/openvswitch-switch restart && \
  ovs-vswitchd --pidfile --detach"
  echo "    --- container list ---"
  docker ps -a
  echo "    --- building alcor-control-agent ---"
  docker exec aca_CHILD bash -c "cd /mnt/host/code && cmake . && make"
  docker exec aca_PARENT bash -c "cd /mnt/host/code && cmake . && make"
  echo "    --- running test case---"
  echo "    --- basic test case---"
  # run basic test case, ./build/tests/aca_tests
  docker exec aca_CHILD bash -c "cd /mnt/host/code && ./build/tests/aca_tests"

elif [ "$1" == "2_port_test_traffic" ]; then
  echo "    --- 2_ports_CREATE test case---"
  parent_container_ip=$(docker inspect -f '{{range.NetworkSettings.Networks}}{{.IPAddress}}{{end}}' aca_PARENT)
  child_container_ip=$(docker inspect -f '{{range.NetworkSettings.Networks}}{{.IPAddress}}{{end}}' aca_CHILD)
  echo "aca_PARENT ip: $parent_container_ip"
  echo "aca_CHILD ip: $child_container_ip"
  # run DISABLED_2_ports_CREATE_test_traffic_PARENT and DISABLED_2_ports_CREATE_test_traffic_CHILD
  docker exec -d aca_CHILD /bin/bash -c "cd /mnt/host/code && ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_2_ports_CREATE_test_traffic_CHILD -p $parent_container_ip" &
  sleep 5
  docker exec aca_PARENT /bin/bash -c "cd /mnt/host/code && ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_2_ports_CREATE_test_traffic_PARENT -c $child_container_ip"
fi
