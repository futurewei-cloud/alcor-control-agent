# AlcorControlAgent
Next Generation Cloud Network Control Agent

# Summary
Source code folder:

- Comm: Library for communication with network controllers and transit daemon
- grpc: Auto generated source codes and library for gRPC interface with Alcor Controllers
- net_config: Library for configurating host networking components
- proto3: Auto generated source codes and library for proto3 scheme for communication with Alcor Controllers
- transit_rpc: Library for RPC interface with transit daemon
- test: Unit and intergration test code

# Build and Execution Instructions using Dockerfile
Since the Alcor Control Agent relies on a few external dependencies, Dockerfile was used for fast build and test environment setup.

## Cloning alcor-control-agent Repo
The Alcor Control Agent includes the Alcor controller and Transit submodules to consume the needed proto3 schemas and RPC definitions. Therefore, the below commands are needed when cloning:

```Shell
cd ~/dev
git clone --recurse-submodules https://github.com/futurewei-cloud/alcor-control-agent.git
git submodule update --init --recursive
```

## Run the build script to set up the build container and compile the alcor-control-agent
Assuming alcor-control-agent was cloned into ~/dev directory:
```Shell
cd build
./build.sh
```
## Running alcor-control-agent and tests
You can run the test (optional):
```Shell
root@ca62b6feec63:/mnt/host/code/alcor-control-agent# ./build/tests/aca_tests
```

You should be ready to run the executable:
```Shell
root@ca62b6feec63:/mnt/host/code/alcor-control-agent# ./build/bin/AlcorControlAgent
```
