FROM ubuntu:18.04
RUN echo "--- installing mizar dependencies ---" && \
    apt-get update -y && apt-get install -y \
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
    lcov
RUN pip3 install httpserver netaddr

RUN echo "--- installing openvswitch compoments and libaries ---" && \
    apt-get install -y \
    graphviz \
    autoconf \
    automake \
    bzip2 \
    debhelper \
    dh-autoreconf \
    dh-python \
    libssl-dev \
    libtool \
    openssl \
    procps \
    python-all \
    python-twisted-conch \
    python-zopeinterface \
    python-six \
    libunbound-dev \
    kmod \
    libkmod2 \
    uuid-runtime \
    fakeroot \
    git

ENV OVS_RELEASE_TAG branch-2.12
RUN echo "--- installing openvswitch dependancies ---" && \
    git clone -b ${OVS_RELEASE_TAG} https://github.com/openvswitch/ovs.git /var/local/git/openvswitch/src && \
    cd /var/local/git/openvswitch/src && \
    echo "--- compiling openvswitch dependancies ---" && \
    dpkg-checkbuilddeps && \
    DEB_BUILD_OPTIONS='parallel=8 nocheck' fakeroot debian/rules binary && \
    echo "--- installing openvswitch deb components and libraries ---" && \
    cd .. && \
    dpkg -i libopenvswitch*.deb \
    #openvswitch-datapath-dkms*.deb \
    openvswitch-common*.deb \
    openvswitch-switch*.deb && \
    rm -rf \var\local\git\openvswitch  

RUN echo "--- installing grpc dependencies ---" && \
    apt-get install -y \
    cmake libssl-dev \
    autoconf git pkg-config \
    automake libtool make g++ unzip 

# installing grpc and its dependencies
ENV GRPC_RELEASE_TAG v1.24.x
RUN echo "--- cloning grpc repo ---" && \
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

# this layer is consuming about 406MB, can try to optimize 
RUN echo "--- installing librdkafka ---" && \
    apt-get install -y --no-install-recommends\
    librdkafka-dev \
    doxygen \
    libssl-dev \
    zlib1g-dev \
    libboost-program-options-dev \
    libboost-all-dev \
    && apt-get clean

RUN echo "--- installing cppkafka ---" && \
    git clone https://github.com/mfontanini/cppkafka.git /var/local/git/cppkafka && \
    cd /var/local/git/cppkafka && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make && \
    make install && \
    ldconfig && \
    rm -rf /var/local/git/cppkafka