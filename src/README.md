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
