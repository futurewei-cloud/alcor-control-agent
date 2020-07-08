// Copyright (c) 2008-2017, 2019 Nicira, Inc.
// Copyright 2019 The Alcor Authors - file modified.
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

#ifndef OVS_CONTROL_H
#define OVS_CONTROL_H

#define IPTOS_PREC_INTERNETCONTROL 0xc0
#define DSCP_DEFAULT (IPTOS_PREC_INTERNETCONTROL >> 2)
#define STDOUT_FILENO   1   /* Standard output.  */

#include <openvswitch/vconn-provider.h> /* add to /usr/local/include/openvswitch */
#include <openvswitch/ofpbuf.h>
#include <openvswitch/ofp-errors.h>
#include <openvswitch/ofp-packet.h>
#include <openvswitch/ofp-port.h>
#include <openvswitch/ofp-table.h>
#include <openvswitch/ofp-protocol.h>
#include <openvswitch/ofp-switch.h>
// #include <linux/in.h>
// #include <string>

extern "C" { 
    struct unixctl_conn;
}
// OVS implementation class
namespace ovs_control
{
class OVS_Control {
  public:
  static OVS_Control &get_instance();

/* --names, --no-names: Show port and table names in output and accept them in
 * input.  (When neither is specified, the default is to accept port names but,
 * for backward compatibility, not to show them unless this is an interactive
 * console session.)  */
  static int use_names;
  static int verbosity;
  /* -F, --flow-format: Allowed protocols.  By default, any protocol is allowed. */
  static enum ofputil_protocol allowed_protocols;
  /* --unixctl-path: Path to use for unixctl server, for "monitor" and "snoop"
     commands. */
  char *unixctl_path;
  
  /* 
   * structs and funcntions borrow from ovs-ofctl.c 
   */
  void monitor(const char *bridge, const char *opt);
  void packet_out(const char *bridge, const char *opt);
  void dump_flows(const char *bridge, const char *opt);
  void dump_flows__(int argc, const char *argv, bool aggregate);
  vconn *prepare_dump_flows(int argc, const char *argv, bool aggregate,
                    ofputil_flow_stats_request *fsr,
                    ofputil_protocol *protocolp);
  enum ofputil_protocol set_protocol_for_flow_dump(vconn *vconn,
                           ofputil_protocol cur_protocol,
                           ofputil_protocol usable_protocols);
  enum ofputil_protocol open_vconn_for_flow_mod(const char *remote, vconn **vconnp,
                        ofputil_protocol usable_protocols);
  bool try_set_protocol(struct vconn *vconn, enum ofputil_protocol want,
                 enum ofputil_protocol *cur);
  void fetch_switch_config(vconn *vconn, ofputil_switch_config *config);
  void set_switch_config(vconn *vconn, const ofputil_switch_config *config);              
  int open_vconn_socket(const char *name, vconn **vconnp);
  void run(int retval, const char *message, ...);
  enum ofputil_protocol open_vconn(const char *name, vconn **vconnp);
  void transact_noreply(vconn *vconn, ofpbuf *request);
  void transact_multiple_noreply(vconn *vconn, ovs_list *requests);
  int monitor_set_invalid_ttl_to_controller(vconn *vconn);
  bool set_packet_in_format(vconn *vconn,
                     enum ofputil_packet_in_format packet_in_format,
                     bool must_succeed);
  void monitor_vconn(vconn *vconn, bool reply_to_echo_requests,
               bool resume_continuations, const char *bridge);
  void send_openflow_buffer(vconn *vconn, ofpbuf *buffer);
  void dump_transaction(vconn *vconn, ofpbuf *request, const char *bridge);
  
  struct barrier_aux {
    struct vconn *vconn;        /* OpenFlow connection for sending barrier. */
    struct unixctl_conn *conn;  /* Connection waiting for barrier response. */
  };

  enum PI { PI_FEATURES, PI_PORT_DESC }; 
  struct port_iterator {
      struct vconn *vconn;
      PI variant;
      struct ofpbuf *reply;
      ovs_be32 send_xid;
      bool more;
  };
  enum TI { TI_STATS, TI_FEATURES };
  struct table_iterator {
    struct vconn *vconn;
    TI variant;
    struct ofpbuf *reply;
    ovs_be32 send_xid;
    bool more;

    struct ofputil_table_features features;
    struct ofpbuf raw_properties;
  };

  ofp_port_t str_to_port_no(const char *vconn_name, const char *port_name);
  bool str_to_ofp(const char *s, ofp_port_t *ofp_port);
  void port_iterator_fetch_port_desc(port_iterator *pi);
  void port_iterator_fetch_features(port_iterator *pi);
  void port_iterator_init(port_iterator *pi, vconn *vconn);
  bool port_iterator_next(port_iterator *pi, ofputil_phy_port *pp);
  void port_iterator_destroy(port_iterator *pi);
  void fetch_ofputil_phy_port(const char *vconn_name, const char *port_name, ofputil_phy_port *pp);
  const ofputil_port_map *get_port_map(const char *vconn_name);
  const ofputil_port_map *ports_to_accept(const char *vconn_name);
  const ofputil_port_map *ports_to_show(const char *vconn_name);
  void table_iterator_init(table_iterator *ti, struct vconn *vconn);
  const ofputil_table_features * table_iterator_next(table_iterator *ti);
  void table_iterator_destroy(table_iterator *ti);
  const ofputil_table_map *get_table_map(const char *vconn_name);
  const ofputil_table_map *tables_to_accept(const char *vconn_name);
  const ofputil_table_map *tables_to_show(const char *vconn_name);
  bool should_accept_names(void);
  bool should_show_names(void);
  const char * openflow_from_hex(const char *hex, ofpbuf **msgp);

  // compiler will flag the error when below is called.
  OVS_Control(OVS_Control const &) = delete;
  void operator=(OVS_Control const &) = delete;

  private:
  OVS_Control(){};
  ~OVS_Control(){};
};
} // namespace ovs_control
#endif // #ifndef OVS_CONTROL_H