#!/bin/bash

BUILD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
code_dir=$(dirname $BUILD)
code_dir=$(dirname $code_dir)
echo "build path is $BUILD"
echo "code path is $code_dir"

# Initialize the needed mizar submodule
# git submodule update --init --recursive

# Create and Start the build contrainer
echo "    --- images_1 ---"
docker images
docker build -f $code_dir/build/Dockerfile -t aca_build0 .
echo "    --- images_2 ---"
docker images
echo "    --- container-1 ---"
docker ps -a
docker rm -f aca_PARENT || true
docker create -v $code_dir:/mnt/host/code -it --privileged --cap-add=NET_ADMIN --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name aca_PARENT aca_build0:latest /bin/bash
docker start aca_PARENT

docker rm -f aca_CHILD || true
docker create -v $code_dir:/mnt/host/code -it --privileged --cap-add=NET_ADMIN --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name aca_CHILD aca_build0:latest /bin/bash
docker start aca_CHILD
echo "    --- container-2 ---"
docker ps -a
# Build mizar first
# docker exec a1 bash -c "cd /mnt/host/code/mizar && make"

if [ "$1" == "2_port_test" ]; then
  echo "    --- building alcor-control-agent ---"
  docker exec aca_CHILD bash -c "cd /mnt/host/code && cmake . && make"
  docker exec aca_PARENT bash -c "cd /mnt/host/code && cmake . && make"
  # echo "    --- running alcor-control-agent ---"
  # sends output to null device, but stderr to console 
  # docker exec aca_CHILD bash -c "nohup /mnt/host/code/build/bin/AlcorControlAgent -d > /dev/null 2>&1 &"
  # docker exec aca_PARENT bash -c "nohup /mnt/host/code/build/bin/AlcorControlAgent -d > /dev/null 2>&1 &"
  echo "    --- running 2_ports_CREATE_test ---"
  # run DISABLED_2_ports_CREATE_test_traffic_PARENT and DISABLED_2_ports_CREATE_test_traffic_CHILD
  docker exec aca_CHILD bash -c "cd /mnt/host/code && ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=DISABLED_2_ports_CREATE_test_traffic_CHILD"
  docker exec aca_PARENT bash -c "cd /mnt/host/code && ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=DISABLED_2_ports_CREATE_test_traffic_PARENT"
  
fi