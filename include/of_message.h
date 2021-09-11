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

#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class OFRawBuf {
public:
    virtual ~OFRawBuf() = default;
    virtual void* data() = 0;
    virtual size_t len() = 0;
};

class OFMessage {
public:
    virtual ~OFMessage() = default;
    // xid
    virtual uint32_t xid() = 0;
    virtual void set_xid(uint32_t id) = 0;
    // pack
    virtual std::shared_ptr<OFRawBuf> pack() = 0;
};

typedef uint32_t ofmsg_xid_t;
typedef std::shared_ptr<OFMessage> ofmsg_ptr_t;
typedef std::shared_ptr<OFRawBuf> ofbuf_ptr_t;

class BundleFlowModMessage {
public:
    BundleFlowModMessage(const std::vector<ofmsg_ptr_t> flow_mods, std::atomic<uint32_t>* fm_xid) :
            _flow_mods(flow_mods),
            _fm_xid(fm_xid) { }

    ~BundleFlowModMessage() = default;

    uint32_t get_bundle_id() {
        return _bundle_id;
    }

    std::shared_ptr<OFRawBuf> pack_open_req();
    std::shared_ptr<OFRawBuf> pack_commit_req();
    std::vector<std::shared_ptr<OFRawBuf> > pack_flow_mods();

private:
    // bundle_id is generated from BundleCtrlMessage->pack()
    uint32_t _bundle_id;
    // starting x_id (auto increased for each message in this bundle, so all unique)
    // need to sync auto increased value with caller for overall OF msg xid management
    std::atomic<uint32_t>* _fm_xid;
    // each flow mod message may have different op_type, like bundling add/mod/delete flow operations together
    std::vector<ofmsg_ptr_t> _flow_mods;
};

class BundleReplyMessage {
public:
    BundleReplyMessage() { }

    ~BundleReplyMessage() = default;

    uint32_t get_bundle_id() {
        return _bundle_id;
    }

    uint16_t get_type() {
        return _type;
    }

    void unpack(void* data);

private:
    uint32_t _bundle_id;

    uint16_t _type;
};

ofmsg_ptr_t create_add_flow(const std::string& flow, bool bundle = false);
ofmsg_ptr_t create_mod_flow(const std::string& flow, bool strict);
ofmsg_ptr_t create_del_flow(const std::string& match, bool strict);
std::vector<ofmsg_ptr_t> create_add_flows(const std::vector<std::string>& flows, bool bundle = false);
ofbuf_ptr_t create_packet_out(const char* option);