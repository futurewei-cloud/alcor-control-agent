# AlcorControlAgent
Next Generation Cloud Network Control Agent

# Summary
Source code folder

Comm: Lib for communication with network controllers and transit daemon
grpc: Generate source and library for gRPC interface with Alcor Controllers
proto3: Generate source and library for proto3 scheme for communication with Alcor Controllers
transit_rpc: Generate library for RPC interface with transit daemon
test: Unit and functional test code

# Installation

## Installing prebuilt packages

On Ubuntu, install librdkafka from the Confluent APT repositories,
see instructions [here](https://docs.confluent.io/current/installation/installing_cp/deb-ubuntu.html#get-the-software) and then install librdkafka:

 ```bash
 $ apt install librdkafka-dev
 $ apt install doxygen
 $ apt-get install libssl-dev
 $ apt-get install zlib1g-dev
 $ apt-get install libboost-program-options-dev
 $ apt-get install libboost-all-dev
 
 # for Transit:
 $ apt install libcmocka-dev
 ```

Download cppkafka from GitHub [here](https://github.com/mfontanini/cppkafka/blob/master/README.md) and install cppkafka using the following commands: 

```Shell
 $ cd <cppkafka_folder>
 $ mkdir build
 $ cd build
 $ cmake <OPTIONS> ..
 $ make
 $ make install
 $ ldconfig
```

## Cloning AlcorControlAgent Repo

The Alcor Control Agent includes the network controller and transit submodules to consume the needed proto3 schema and RPC definitions. Therefore, the below command is needed when cloning:

```Shell
git clone --recurse-submodules https://github.com/futurewei-cloud/AlcorControlAgent.git
```

## Compiling AlcorControlAgent

Note that the _AlcorControlAgent_ depends on the transit daemon interface to program XDP (through RPC). Therefore, it expects to have the transit submodule code under the root "AlcorControlAgent" directory. This is needed in order to compile and generate the needed "trn_rpc_protocol.h" header file.

You will also need to install the protobuf compiler and development environment:
```Shell
sudo apt-get install libprotobuf-dev protobuf-compiler
```

Compile and install gtest:
```Shell
https://github.com/google/googletest/blob/master/googletest/README.md
cmake -Dgtest_build_samples=ON -DBUILD_SHARED_LIBS=ON .
make
make install
```

If compiling using linux containers, the below line in /src/CmakeLists.txt needs to change:
```Shell
From - FIND_LIBRARY(CPPKAFKA cppkafka /usr/local/lib64 NO_DEFAULT_PATH)
To   - FIND_LIBRARY(CPPKAFKA cppkafka /usr/local/lib NO_DEFAULT_PATH)
```

In order to compile _networkcontrolagent_ you need to run,

```Shell
 $ cd ..  (go up to project folder)
 $ cmake .
 $ make
```

You should be ready to run the executable

```Shell
 $ ./src/networkControlAgent
```
