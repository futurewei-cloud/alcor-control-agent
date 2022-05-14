#!/bin/bash

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


function install_distrobuilder {
    apt update &&\
    apt install -y debootstrap rsync gpg squashfs-tools git

    # install golang
    wget https://go.dev/dl/go1.18.2.linux-amd64.tar.gz
    rm -rf /usr/local/go &&\
    tar -C /usr/local -xzf go1.18.2.linux-amd64.tar.gz
    export PATH=$PATH:/usr/local/go/bin
    rm go1.18.2.linux-amd64.tar.gz

    # install distobuilder
    git clone https://github.com/lxc/distrobuilder
    cd ./distrobuilder
    make
    cd ..
    rm -rf ./distrobuilder
}

function build_aca_lxc {
    BUILD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
    echo "build path is $BUILD"
    DEP_PATH="/var/local/git"

    $HOME/go/bin/distrobuilder build-lxd lxd.yaml && \
    lxc image import lxd.tar.xz rootfs.squashfs --alias aca && \
    rm -rf lxd.tar.xz \
        rootfs.squashfs \
        *.service \
        ignite && \
    lxc launch aca aca
    lxc exec aca -- bash -c "chmod +x /root/aca/build/aca-machine-init.sh"
    lxc exec aca -- bash -c "/root/aca/build/aca-machine-init.sh"
    chown -R lxd /etc/run/openvswitch
    lxc config device add aca modules disk source=/lib/modules path=/lib/modules && \ 
        lxc config device add aca log disk source=/var/log/openvswitch path=/var/log/openvswitch && \ 
        lxc config device add aca lib disk source=/var/lib/openvswitch path=/var/lib/openvswitch && \
        lxc config device add aca run disk source=/var/run/openvswitch path=/var/run/openvswitch && \
        lxc config device add aca etc disk source=/etc/openvswitch path=/etc/openvswitch
}


while getopts "ib" opt; do
case $opt in
  i)
    echo "Install distrobuilder"
    install_distrobuilder
    ;;
  b)
    echo "Build aca lxc container"
    build_aca_lxc
    ;;
  \?)
    echo "Invalid arguements"
esac
done