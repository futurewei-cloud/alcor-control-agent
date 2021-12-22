#!/bin/bash

# sudo sed -i -e 's/\r$//' ./build/build_test_only.sh

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

cd $BUILD/.. && cmake . && \
# after cmake ., modify the generated link.txt s so that the "-lssl" and "-lcrypto" appears after the openvswitch, so that it can compile
sed -i 's/\(-ldl -lrt -lm -lpthread\)/-lssl -lcrypto -lltdl \1/' src/CMakeFiles/AlcorControlAgent.dir/link.txt && \
sed -i 's/\(-ldl -lrt -lm -lpthread\)/-lssl -lcrypto -lltdl \1/' test/CMakeFiles/aca_tests.dir/link.txt && \
sed -i 's/\(-ldl -lrt -lm -lpthread\)/-lssl -lcrypto -lltdl \1/' test/CMakeFiles/gs_tests.dir/link.txt && \
make
