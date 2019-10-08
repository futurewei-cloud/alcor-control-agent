# AlcorControlAgent
Next Generation Cloud Network Control Agent

# Summary
Source code folder:

- Comm: Library for communication with network controllers and transit daemon
- grpc: Generates source and library for gRPC interface with Alcor Controllers
- net_config: Library for configurating host networking components
- proto3: Generates source and library for proto3 scheme for communication with Alcor Controllers
- transit_rpc: Generates library for RPC interface with transit daemon
- test: Unit and intergration test code

# Build and Execution Instructions using Dockerfile

Since the Alcor Control Agent relies on a few external dependencies, Dockerfile was use for fast build and test environment setup.

## Cloning alcor-control-agent Repo

The Alcor Control Agent includes the Alcor controller and Transit submodules to consume the needed proto3 schemas and RPC definitions. Therefore, the below command is needed when cloning:

```Shell
git clone --recurse-submodules https://github.com/futurewei-cloud/alcor-control-agent.git
git submodule update --init --recursive
```

## Setting up the build container
Assuming alcor-control-agent was cloned into ~/dev directory:
```Shell
cd build
docker build -t aca_build0 .
docker create -v ~/dev:/mnt/host/code -it --privileged --cap-add=NET_ADMIN --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --name a1 aca_build0:latest /bin/bash
docker network connect net0 a1
docker network connect net1 a1
docker start a1
docker exec -it a1 /bin/bash
```

## Compiling alcor-control-agent
In order to compile alcor-control-agent you need to run:
```Shell
docker exec -it a1 /bin/bash
root@ca62b6feec63:# cd /mnt/host/code/alcor-control-agent/
root@ca62b6feec63:/mnt/host/code/alcor-control-agent# cmake .
root@ca62b6feec63:/mnt/host/code/alcor-control-agent# make
```
## Running alcor-control-agent and tests
You can run the test (optional):
```Shell
 $ ./build/tests/aca_tests
```

You should be ready to run the executable:
```Shell
 $ ./build/bin/AlcorControlAgent
```
