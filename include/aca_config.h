// MIT License
// Copyright(c) 2020 Futurewei Cloud
//
//     Permission is hereby granted,
//     free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction,
//     including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons
//     to whom the Software is furnished to do so, subject to the following conditions:
//
//     The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
//     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//     WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef ACA_CONFIG_H
#define ACA_CONFIG_H

#define MAX_PORT_SCAN_RETRY 300
#define PORT_SCAN_SLEEP_INTERVAL 1000 // 1000ms = 1s

// prefix to indicate it is an Alcor Distributed Router host mac
// (aka host DVR mac)
#define HOST_DVR_MAC_PREFIX "fe:16:11:"
#define HOST_DVR_MAC_MATCH HOST_DVR_MAC_PREFIX + "00:00:00/ff:ff:ff:00:00:00"

#define DHCP_MSG_OPTS_DNS_LENGTH (5) //max 5 dns address

#define DEFAULT_MTU 9000

#define USLEEPTIME_IN_MICROSECONDS 30000

#endif // #ifndef ACA_CONFIG_H