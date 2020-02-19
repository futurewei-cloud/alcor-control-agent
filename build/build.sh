#!/bin/bash

# Initialize the needed mizar submodule
git submodule update --init --recursive

# Create and Start the build contrainer
docker build -f AlcorControlAgent/build/Dockerfile -t aca_build0 .
docker create -v $(pwd):/mnt/host/code -it --privileged --cap-add=NET_ADMIN --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name a1 aca_build0:latest /bin/bash
docker start a1

# Build mizar first
docker exec a1 bash -c "cd /mnt/host/code/AlcorControlAgent/mizar && make"

# Build alcor control agent
docker exec a1 bash -c "cd /mnt/host/code/AlcorControlAgent && cmake . && make"
