# NetworkControlAgent
Next Generation Cloud Networking

# Summary
Source code folder

Comm: Lib for communication with network controllers and transit daemon

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

## Compiling NetworkAgent

Note that _networkcontrolagent_ depends on transit daemon interface to invoke it (through RPC) to program XDP. Therefore, it expect to have a transit code in parallel to the root "AliothControlAgent" directory and it can compile to generate the needed "trn_rpc_protocol.h" header file.

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
