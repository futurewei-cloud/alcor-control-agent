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
chown -R 100000:100000 /etc/run/openvswitch
lxc config device add aca modules disk source=/lib/modules path=/lib/modules && \ 
    lxc config device add aca log disk source=/var/log/openvswitch path=/var/log/openvswitch && \ 
    lxc config device add aca lib disk source=/var/lib/openvswitch path=/var/lib/openvswitch && \
    lxc config device add aca run disk source=/var/run/openvswitch path=/var/run/openvswitch && \
    lxc config device add aca etc disk source=/etc/openvswitch path=/etc/openvswitch
