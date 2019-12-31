#!/bin/bash

nsenter -t 1 -m -u -n -i rm -rf ~/alcor-control-agent
nsenter -t 1 -m -u -n -i echo "--- installing mizar dependencies ---" && \
    apt-get update -y && nsenter -t 1 -m -u -n -i apt-get install -y \
    sudo \
    rpcbind \
    rsyslog \
    build-essential \
    clang-7 \
    llvm-7 \
    libelf-dev \
    openvswitch-switch \
    iproute2  \
    net-tools \
    iputils-ping \
    ethtool \
    curl \
    python3 \
    python3-pip \
    netcat \
    libcmocka-dev \
    lcov \
    git && \
nsenter -t 1 -m -u -n -i pip3 install httpserver netaddr && \
nsenter -t 1 -m -u -n -i git clone --recurse-submodules -j8 https://github.com/futurewei-cloud/alcor-control-agent.git ~/alcor-control-agent && \
nsenter -t 1 -m -u -n -i make -C /root/alcor-control-agent/mizar

nsenter -t 1 -m -u -n -i echo "--- installing grpc dependencies ---" && \
    apt-get install -y \
    cmake libssl-dev \
    autoconf git pkg-config \
    automake libtool make g++ unzip 

# installing grpc and its dependencies
ENV GRPC_RELEASE_TAG v1.24.x
nsenter -t 1 -m -u -n -i echo "--- cloning grpc repo ---" && \
    git clone -b ${GRPC_RELEASE_TAG} https://github.com/grpc/grpc /var/local/git/grpc && \
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
    rm -rf /var/local/git/grpc

nsenter -t 1 -m -u -n -i echo "--- installing librdkafka ---" && \
    apt-get install -y --no-install-recommends\
    librdkafka-dev \
    doxygen \
    libssl-dev \
    zlib1g-dev \
    libboost-program-options-dev \
    libboost-all-dev \
    && apt-get clean

nsenter -t 1 -m -u -n -i echo "--- installing cppkafka ---" && \
    git clone https://github.com/mfontanini/cppkafka.git /var/local/git/cppkafka && \
    cd /var/local/git/cppkafka && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make && \
    make install && \
    ldconfig && \
    rm -rf /var/local/git/cppkafka

nsenter -t 1 -m -u -n -i cd ~/alcor-control-agent && cmake . && make
nsenter -t 1 -m -u -n -i ln -snf ~/alcor-control-agent/build/ /aca_build && \

echo "done" && sleep infinity