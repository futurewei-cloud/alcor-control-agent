// Copyright 2019 The Alcor Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ACA_CONFIG_H
#define ACA_CONFIG_H

#define MAX_PORT_SCAN_RETRY 300
#define PORT_SCAN_SLEEP_INTERVAL 1000 // 1000ms = 1s

// prefix to indicate it is an Alcor Distributed Router host mac
// (aka host DVR mac)
#define HOST_DVR_MAC_PREFIX "fe:16:11:"
#define HOST_DVR_MAC_MATCH HOST_DVR_MAC_PREFIX + "00:00:00/ff:ff:ff:00:00:00"

#define DHCP_MSG_OPTS_DNS_LENGTH (5) //max 5 dns address

#endif // #ifndef ACA_CONFIG_H