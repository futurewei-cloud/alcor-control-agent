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

#include "of_message.h"
#include "aca_log.h"
#include "aca_util.h"

#undef OFP_ASSERT
#undef CONTAINER_OF
#undef ARRAY_SIZE
#undef ROUND_UP

#include <iostream>
#include <memory>
#include <openvswitch/ofp-msgs.h>
#include <openvswitch/ofp-util.h>
#include <openvswitch/ofp-parse.h>
#include <openvswitch/ofp-print.h>

enum {
    ADD_FLOW = 0,
    MODIFY_FLOW = 1,
    MODIFY_FLOW_STRICT = 2,
    DELETE_FLOW = 3,
    DELETE_FLOW_STRICT = 4,
};

template <typename T>
struct FreeDeleter {
    void operator()(T* p) const {
        free(p);
    }
};
typedef std::unique_ptr<char, FreeDeleter<char>> OFString;
typedef std::unique_ptr<ofpact, FreeDeleter<ofpact>> OFPact;

const ofputil_protocol DEFAULT_OF_VERSION = OFPUTIL_P_OF13_OXM;
const ofputil_protocol BUNDLE_OF_VERSION  = OFPUTIL_P_OF14_OXM;

class OFPBuf : public OFRawBuf {
public:
    OFPBuf(ofpbuf* b) : _buf(b) {}
    ~OFPBuf() override {
        ofpbuf_delete(_buf);
        _buf = nullptr;
    }
    void* data() override {
        return _buf->data;
    }
    size_t len() override {
        return _buf->size;
    }

private:
    struct ofpbuf* _buf;
};

class OFBaseMessage : public OFMessage {
public:
    OFBaseMessage() : _xid(0) {}
    uint32_t xid() override {
        return _xid;
    }
    void set_xid(uint32_t id) override {
        _xid = id;
    }

    ofbuf_ptr_t pack_ofpbuf(struct ofpbuf* buf) {
        auto header = static_cast<struct ofp_header*>(buf->data);
        header->xid = htonl(_xid);

        ofpmsg_update_length(buf);

        return std::make_shared<OFPBuf>(buf);
    }
    virtual ~OFBaseMessage() = default;

private:
    uint32_t _xid;
};

class FlowModMessage : public OFBaseMessage {
public:
    FlowModMessage(int op_type, const std::string& flow, bool bundle = false) :
            _op_type(op_type),
            _flow(flow)
            {
                if (bundle) {
                    _of_ver = BUNDLE_OF_VERSION;
                } else {
                    _of_ver = DEFAULT_OF_VERSION;
                }
            }

    ~FlowModMessage() override = default;

    ofbuf_ptr_t pack() override {
        int command = OFPFC_ADD;
        std::string cmd_str = "ADD";

        switch (_op_type) {
            case MODIFY_FLOW:
                command = OFPFC_MODIFY;
                cmd_str = "MOD";
                break;

            case MODIFY_FLOW_STRICT:
                command = OFPFC_MODIFY_STRICT;
                cmd_str = "MOD STRICT";
                break;

            case DELETE_FLOW:
                command = OFPFC_DELETE;
                cmd_str = "DELETE";
                break;

            case DELETE_FLOW_STRICT:
                command = OFPFC_DELETE_STRICT;
                cmd_str = "DELETE STRICT";
                break;

            case ADD_FLOW:
            default:
                /* the description is from ovs implementation
                 * If 'command' is given as -2, 'string' may begin with a command name ("add", "modify", "delete", "modify_strict", or "delete_strict").
                 * A missing command is treated as "add". */
                // command = OFPFC_ADD;
                command = -2;
                cmd_str = "ADD";
                break;
        }

        struct ofputil_flow_mod fm;
        enum ofputil_protocol usable_protocols;

        OFString error(parse_ofp_flow_mod_str(&fm, _flow.c_str(), NULL,
                                              command, &usable_protocols));
        if (error.get()) {
            ACA_LOG_ERROR("OFMessage - failed to parse flow: %s, error: %s\n",
                          _flow.c_str(), error.get());
            return {};
        }

        OFString req_s(ofputil_protocols_to_string(_of_ver));
        OFString usable_s(ofputil_protocols_to_string(usable_protocols));
        if (!(_of_ver & usable_protocols)) {
            ACA_LOG_ERROR("OFMessage - flow not supported by requested OF version %s, flow: %s, usable_protocols: %s\n",
                          req_s.get(), _flow.c_str(), usable_s.get());
            return {};
        }

        auto buf = ofputil_encode_flow_mod((const ofputil_flow_mod *)&fm, _of_ver);
        if (buf == nullptr) {
            ACA_LOG_ERROR("OFMessage - failed to encode flow_str: %s, OF version: %s, usable_protocols: %s\n",
                          _flow.c_str(), req_s.get(), usable_s.get());
            return {};
        }

        //ACA_LOG_INFO("encode flow_str: %s, command: %s, type: %d, usable_protocols: %s\n",
        //             _flow.c_str(), cmd_str.c_str(), (int)(static_cast<struct ofp_header*>(buf->data)->type), usable_s.get());

        // free fm.ofpacts
        //OFPact ofpacts(fm.ofpacts);
        free(CONST_CAST(struct ofpact *, fm.ofpacts));

        return std::make_shared<OFPBuf>(buf);
    }

private:
    int _op_type;
    std::string _flow;
    enum ofputil_protocol _of_ver;
};

ofbuf_ptr_t BundleFlowModMessage::pack_open_req() {
    struct ofputil_bundle_ctrl_msg bundle_ctrl;
    // needs to handshake OFPBCT_OPEN_REQUEST first for ovs to get ready for the following bundle
    bundle_ctrl.type = OFPBCT_OPEN_REQUEST;

    // OFPBF_ORDERED ensures flows get programmed in order
    // OFPBF_ATOMIC means packet atomic -
    //     a given packet from an input port or packet-out request should either be processed with none or
    //     with all of the modifications having been applied
    bundle_ctrl.flags = OFPBF_ORDERED | OFPBF_ATOMIC;
    auto buf = ofputil_encode_bundle_ctrl_request(ofputil_protocol_to_ofp_version(BUNDLE_OF_VERSION),
                                                  &bundle_ctrl);
    ofpmsg_update_length(buf);

    // save bundle_id for later use
    _bundle_id = bundle_ctrl.bundle_id;

    return std::make_shared<OFPBuf>(buf);
}

ofbuf_ptr_t BundleFlowModMessage::pack_commit_req() {
    struct ofputil_bundle_ctrl_msg bundle_ctrl;
    // bundle_id has to be consistent with open request
    bundle_ctrl.bundle_id = _bundle_id;
    // OFPBCT_OPEN_REQUEST or bundle flow-mod needs to be followed with an OFPBCT_COMMIT_REQUEST message,
    // otherwise error OFPBFC_TIMEOUT will occur
    bundle_ctrl.type = OFPBCT_COMMIT_REQUEST;
    // flags need to be consistent too
    bundle_ctrl.flags = OFPBF_ORDERED | OFPBF_ATOMIC;
    auto buf = ofputil_encode_bundle_ctrl_request(ofputil_protocol_to_ofp_version(BUNDLE_OF_VERSION),
                                                  &bundle_ctrl);
    ofpmsg_update_length(buf);

    return std::make_shared<OFPBuf>(buf);
}

std::vector<ofbuf_ptr_t> BundleFlowModMessage::pack_flow_mods() {
    std::vector<ofbuf_ptr_t> ret_buf;

    for (auto of_msg : _flow_mods) {
        struct ofputil_bundle_add_msg bundle_flow_mod;
        // all flow_mod messages in this bundle share the same bundle id which is generated by ofputil_bundle_ctrl_msg
        bundle_flow_mod.bundle_id = _bundle_id;
        // by default keep flags consistent with BundleCtrlMessage
        bundle_flow_mod.flags = OFPBF_ORDERED | OFPBF_ATOMIC;

        // input is std::shared_ptr<OFMessage> of_msg, but need to retrieve data from casting it to std::shared_ptr<FlowModMessage>
        auto fm_msg = std::static_pointer_cast<FlowModMessage>(of_msg);
        // each flow-mod has a unique xid
        fm_msg->set_xid(_fm_xid->fetch_add(1));

        auto fm_buf = fm_msg->pack();
        // ofputil_bundle_add_msg->msg is (ofpheader*)
        bundle_flow_mod.msg = static_cast<struct ofp_header*>(fm_buf->data());

        auto buf = ofputil_encode_bundle_add(ofputil_protocol_to_ofp_version(BUNDLE_OF_VERSION),
                                             &bundle_flow_mod);

        ret_buf.emplace_back(std::make_shared<OFPBuf>(buf));
    }

    return ret_buf;
}

void BundleReplyMessage::unpack(void* data) {
    struct ofputil_bundle_ctrl_msg bundle_ctrl_reply;
    ofputil_decode_bundle_ctrl((ofp_header *)data, &bundle_ctrl_reply);

    _type = bundle_ctrl_reply.type;
    _bundle_id = bundle_ctrl_reply.bundle_id;
}

ofmsg_ptr_t create_add_flow(const std::string& flow, bool bundle) {
    return std::make_shared<FlowModMessage>(ADD_FLOW, flow, bundle);
}

ofmsg_ptr_t create_add_flow(const std::string& flow) {
    return std::make_shared<FlowModMessage>(ADD_FLOW, flow);
}

ofmsg_ptr_t create_mod_flow(const std::string& flow, bool strict) {
    int op_type = strict ? MODIFY_FLOW_STRICT : MODIFY_FLOW;
    return std::make_shared<FlowModMessage>(op_type, flow);
}

ofmsg_ptr_t create_del_flow(const std::string& flow, bool strict) {
    int op_type = strict ? DELETE_FLOW_STRICT : DELETE_FLOW;
    return std::make_shared<FlowModMessage>(op_type, flow);
}

std::vector<ofmsg_ptr_t> create_add_flows(const std::vector<std::string>& flows) {
    std::vector<ofmsg_ptr_t> ret;
    for (const auto &flow : flows) {
        ret.emplace_back(std::make_shared<FlowModMessage>(ADD_FLOW, flow));
    }

    return ret;
}

ofbuf_ptr_t create_packet_out(const char* option) {
    enum ofputil_protocol usable_protocols;
    struct ofputil_packet_out po;
    char *error;

    error = parse_ofp_packet_out_str(&po, option, NULL, &usable_protocols);
    if (error) {
        ACA_LOG_ERROR("OFMessage - create_packet_out had error %s\n", error);
    }

    auto buf = ofputil_encode_packet_out(&po, DEFAULT_OF_VERSION);

    return std::make_shared<OFPBuf>(buf);
}