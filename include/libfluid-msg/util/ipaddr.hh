// Copyright (c) 2014 Open Networking Foundation
// Copyright 2020 Futurewei Cloud
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

#ifndef __IPADDRESS_H__
#define __IPADDRESS_H__

#include <stdint.h>
#include <string.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sstream>
#include <string>

namespace fluid_msg{

enum {NONE = 0, IPV4 = 4, IPV6 = 6 };

class IPAddress {
    public:
        IPAddress();
        IPAddress(const char* address);
        IPAddress(const std::string &address);
        IPAddress(const IPAddress &other);
        IPAddress(const uint32_t ip_addr);
        IPAddress(const uint8_t ip_addr[16]);
        IPAddress(const struct in_addr& ip_addr);
        IPAddress(const struct in6_addr& ip_addr);
        ~IPAddress(){};

        IPAddress& operator=(const IPAddress& other);
        bool operator==(const IPAddress& other) const;
        int get_version() const;
        void setIPv4(uint32_t address);
        void setIPv6(uint8_t address[16]);
        uint32_t getIPv4();
        uint8_t * getIPv6();
        static uint32_t IPv4from_string(const std::string &address);
        static struct in6_addr IPv6from_string(const std::string &address);

    private:
        int version;
        union {
            uint32_t ipv4;
            uint8_t ipv6[16];
        };
};
}
#endif /* __IPADDRESS_H__ */
