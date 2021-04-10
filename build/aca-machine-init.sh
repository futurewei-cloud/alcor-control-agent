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

# TODO: remove the unneeded dependencies
echo "1--- installing mizar dependencies ---" && \
    apt-get update -y && apt-get install -y \
    rpcbind \
    rsyslog \
    build-essential \
    clang-7 \
    llvm-7 \
    libelf-dev \
    iproute2  \
    net-tools \
    iputils-ping \
    ethtool \
    curl \
    python3 \
    python3-pip \
    netcat \
    libcmocka-dev \
    lcov
pip3 install httpserver netaddr

echo "2--- installing librdkafka ---" && \
    apt-get update -y && apt-get install -y --no-install-recommends\
    librdkafka-dev \
    doxygen \
    libssl-dev \
    zlib1g-dev \
    libboost-program-options-dev \
    libboost-all-dev \
    && apt-get clean

echo "3--- installing cppkafka ---" && \
    apt-get update -y && apt-get install -y cmake 
    git clone https://github.com/mfontanini/cppkafka.git /var/local/git/cppkafka && \
    cd /var/local/git/cppkafka && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make && \
    make install && \
    ldconfig && \
    rm -rf /var/local/git/cppkafka
    cd ~

echo "4--- installing grpc dependencies ---" && \
    apt-get update -y && apt-get install -y \
    cmake libssl-dev \
    autoconf git pkg-config \
    automake libtool make g++ unzip 

# installing grpc and its dependencies
GRPC_RELEASE_TAG="v1.24.x"
echo "5--- cloning grpc repo ---" && \
    git clone -b $GRPC_RELEASE_TAG https://github.com/grpc/grpc /var/local/git/grpc && \
    cd /var/local/git/grpc && \
    git submodule update --init && \
    echo "--- installing c-ares ---" && \
    cd /var/local/git/grpc/third_party/cares/cares && \
    git fetch origin && \
    git checkout cares-1_15_0 && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake -DCMAKE_BUILD_TYPE=Release ../.. && \
    make -j4 install && \
    cd ../../../../.. && \
    rm -rf third_party/cares/cares && \
    echo "--- installing protobuf ---" && \
    cd /var/local/git/grpc/third_party/protobuf && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release .. && \
    make -j4 install && \
    cd ../../../.. && \
    rm -rf third_party/protobuf && \
    echo "--- installing grpc ---" && \
    cd /var/local/git/grpc && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DgRPC_PROTOBUF_PROVIDER=package -DgRPC_ZLIB_PROVIDER=package -DgRPC_CARES_PROVIDER=package -DgRPC_SSL_PROVIDER=package -DCMAKE_BUILD_TYPE=Release ../.. && \
    make -j4 install && \
    echo "--- installing google test ---" && \
    cd /var/local/git/grpc/third_party/googletest && \
    cmake -Dgtest_build_samples=ON -DBUILD_SHARED_LIBS=ON . && \
    make && \
    make install && \
    rm -rf /var/local/git/grpc && \
    cd ~

OVS_RELEASE_TAG="branch-2.12"
echo "6--- installing openvswitch dependancies ---" && \
    git clone -b $OVS_RELEASE_TAG https://github.com/openvswitch/ovs.git /var/local/git/openvswitch && \
    cd /var/local/git/openvswitch && \
    ./boot.sh && \
    ./configure --prefix=/usr/local --localstatedir=/var --sysconfdir=/etc --enable-shared --enable-ndebug && \
    make && \
    make install && \
    cp /var/local/git/openvswitch/lib/vconn-provider.h /usr/local/include/openvswitch/vconn-provider.h && \
    rm -rf /var/local/git/openvswitch && \
    test -f /usr/bin/ovs-vsctl && rm -rf /usr/local/sbin/ov* /usr/local/bin/ov* /usr/local/bin/vtep* && \
    cd ~

PULSAR_RELEASE_TAG='pulsar-2.6.1'
echo "7--- installing pulsar dependacies ---" && \
    mkdir -p /var/local/git/pulsar && \
    wget https://archive.apache.org/dist/pulsar/${PULSAR_RELEASE_TAG}/DEB/apache-pulsar-client.deb -O /var/local/git/pulsar/apache-pulsar-client.deb && \
    wget https://archive.apache.org/dist/pulsar/${PULSAR_RELEASE_TAG}/DEB/apache-pulsar-client-dev.deb -O /var/local/git/pulsar/apache-pulsar-client-dev.deb && \
    cd /var/local/git/pulsar && \
    apt install -y ./apache-pulsar-client*.deb && \
    rm -rf /var/local/git/pulsar 
    cd ~

echo "8--- building alcor-control-agent"
cd $BUILD/.. && cmake . && make

if [ "$1" == "delete-bridges" ]; then
  echo "9--- deleting br-tun and br-int if requested"
  PATH=$PATH:/usr/local/share/openvswitch/scripts \
      LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
  ovs-ctl --system-id=random --delete-bridges restart
fi

echo "10--- running alcor-control-agent"
# sends output to null device, but stderr to console 
nohup $BUILD/bin/AlcorControlAgent -d > /dev/null 2>&1 &
