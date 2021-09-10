// Copyright (c) 2014 Open Networking Foundation
// Copyright 2021 The Alcor Authors
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

#ifndef __MACADDRESS_H__
#define __MACADDRESS_H__

#include <stdint.h>
#include <string.h>
#include <net/if.h>

#include <sstream>
#include <iomanip>
#include <string>

namespace fluid_msg{

class EthAddress {
    public:
        EthAddress();
        EthAddress(const char* address);
        EthAddress(const std::string &address);
        EthAddress(const EthAddress &other);
        EthAddress(const uint8_t* data);

        EthAddress& operator=(const EthAddress &other);
        bool operator==(const EthAddress &other) const;
        std::string to_string() const;
        void set_data(uint8_t* array);
        uint8_t* get_data(){return this->data;}
        static uint8_t* data_from_string(const std::string &address);

    private:
        uint8_t data[6];
        // void data_from_string(const std::string &address);
};
}
#endif /* __MACADDRESS_H__ */



