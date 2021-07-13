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

#include "aca_log.h"
#include "aca_util.h"
#include "ovs_control.h"
#include "aca_on_demand_engine.h"
#include <sstream> // std::(istringstream)
#include <string> // std::(string)
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <chrono>
#include <errno.h>
#include <unistd.h>
#include <openflow/openflow-common.h>
#include <openvswitch/types.h>
#include <openvswitch/vconn.h>
#include <openvswitch/list.h>
#include <openvswitch/shash.h>
#include <openvswitch/ofp-print.h>
#include <openvswitch/ofp-monitor.h>
//#include <openvswitch/ofp-flow.h>
#include <openvswitch/ofp-msgs.h>
#include <openvswitch/ofp-util.h>
#include <openvswitch/poll-loop.h>
#include <openvswitch/vlog.h>

using namespace std;
using namespace aca_on_demand_engine;

extern std::atomic_ulong g_total_execute_openflow_time;

extern "C" {
void mask_allowed_ofp_versions(uint32_t);
const char *ovs_rundir(void);
void dp_parse_name(const char *datapath_name, char **name, char **type);
uint32_t get_allowed_ofp_versions(void);
void ovs_fatal_valist(int err_no, const char *format, va_list);
void ovs_fatal(int err_no, const char *format, ...);
const char *ovs_strerror(int);
bool str_to_uint(const char *, int base, unsigned int *);
inline ofp_port_t u16_to_ofp(uint16_t port);
void daemon_save_fd(int fd);
void daemonize_start(bool access_datapath);
int unixctl_server_create(const char *path, struct unixctl_server **);
void set_allowed_ofp_versions(const char *string);
long long int time_wall_msec(void);
typedef void
unixctl_cb_func(struct unixctl_conn *, int argc, const char *argv[], void *aux);
void unixctl_command_register(const char *name, const char *usage, int min_args,
                              int max_args, unixctl_cb_func *cb, void *aux);
void unixctl_command_reply(struct unixctl_conn *, const char *body);
void unixctl_command_reply_error(struct unixctl_conn *, const char *error);
void daemonize_complete(void);
void unixctl_server_run(struct unixctl_server *);
void unixctl_server_wait(struct unixctl_server *);
void unixctl_server_destroy(struct unixctl_server *);
void ofp_print_packet(FILE *stream, const void *data, size_t len, ovs_be32 packet_type);
const char *eth_from_hex(const char *hex, struct dp_packet **packetp);
void ds_put_hex_dump(struct ds *ds, const void *buf_, size_t size, uintptr_t ofs, bool ascii);
char *ds_steal_cstr(struct ds *);
}

VLOG_DEFINE_THIS_MODULE(ovs_control)

namespace ovs_control
{
OVS_Control &OVS_Control::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static OVS_Control instance;

  /* -F, --flow-format: Allowed protocols.  By default, any protocol is allowed. */
  allowed_protocols = static_cast<ofputil_protocol>(OFPUTIL_P_ANY);
  bundle = true;

  return instance;
}

int OVS_Control::use_names;
int OVS_Control::verbosity;
enum ofputil_protocol OVS_Control::allowed_protocols;
bool OVS_Control::bundle;

void OVS_Control::monitor(const char *bridge, const char *opt)
{
  verbosity = 2;
  use_names = -1;

  /* -P, --packet-in-format: Packet IN format to use in monitor and snoop
    * commands.  Either one of NXPIF_* to force a particular packet_in format, or
    * -1 to let ovs-ofctl choose the default. */
  int preferred_packet_in_format = -1;

  vconn *vconn;
  enum ofputil_protocol usable_protocols = static_cast<ofputil_protocol>(OFPUTIL_P_ANY);
  bool resume_continuations = false;

  set_allowed_ofp_versions("OpenFlow13");

  open_vconn(bridge, &vconn);

  string option;
  stringstream iss(opt);

  /* If the user wants the invalid_ttl_to_controller feature, limit the
     * OpenFlow versions to those that support that feature.  (Support in
     * OpenFlow 1.0 is an Open vSwitch extension.) */
  while (iss >> option) {
    if (option.compare("invalid_ttl") == 0) {
      uint32_t usable_versions =
              ((1u << OFP10_VERSION) | (1u << OFP11_VERSION) | (1u << OFP12_VERSION));
      uint32_t allowed_versions = get_allowed_ofp_versions();
      if (!(allowed_versions & usable_versions)) {
        struct ds versions = DS_EMPTY_INITIALIZER;
        ofputil_format_version_bitmap_names(&versions, usable_versions);
        // ovs_fatal(0, "invalid_ttl requires one of the OpenFlow "
        //           "versions %s but none is enabled (use -O)",
        //           ds_cstr(&versions));
        ACA_LOG_ERROR("invalid_ttl requires one of the OpenFlow "
                      "versions %s but none is enabled (use -O)\n",
                      ds_cstr(&versions));
      }
      mask_allowed_ofp_versions(usable_versions);
      break;
    }
  }

  iss.str(opt);
  iss.clear();

  while (iss >> option) {
    if (isdigit(option[0])) {
      struct ofputil_switch_config config;

      fetch_switch_config(vconn, &config);
      config.miss_send_len = atoi(option.c_str());
      set_switch_config(vconn, &config);
    } else if (option.compare("invalid_ttl") == 0) {
      monitor_set_invalid_ttl_to_controller(vconn);
    } else if (option.compare(0, 6, "watch:") == 0) {
      ofputil_flow_monitor_request fmr;
      ofpbuf *msg;
      char *error;

      error = parse_flow_monitor_request(&fmr, option.substr(6).c_str(),
                                         ports_to_accept(bridge),
                                         tables_to_accept(bridge), &usable_protocols);
      if (error) {
        // ovs_fatal(0, "%s", error);
        ACA_LOG_ERROR("failed to parse_flow_monitor_request, error: %s", error);
      }

      if (!(usable_protocols & allowed_protocols)) {
        char *allowed_s = ofputil_protocols_to_string(allowed_protocols);
        char *usable_s = ofputil_protocols_to_string(usable_protocols);
        //    ovs_fatal(0, "none of the usable flow formats (%s) is among "
        //              "the allowed flow formats (%s)", usable_s, allowed_s);
        ACA_LOG_ERROR("none of the usable flow formats (%s) is among "
                      "the allowed flow formats (%s)",
                      usable_s, allowed_s);
      }

      msg = ofpbuf_new(0);
      ofputil_append_flow_monitor_request(&fmr, msg);
      dump_transaction(vconn, msg, bridge);
      fflush(stdout);
    } else if (option.compare("resume") == 0) {
      /* This option is intentionally undocumented because it is meant
             * only for testing. */
      resume_continuations = true;
      /* Set miss_send_len to ensure that we get packet-ins. */
      struct ofputil_switch_config config;
      fetch_switch_config(vconn, &config);
      config.miss_send_len = UINT16_MAX;
      set_switch_config(vconn, &config);
    } else {
      //ovs_fatal(0, "%s: unsupported \"monitor\" argument", option);
      ACA_LOG_ERROR("%s: unsupported monitor argument", option.c_str());
    }
  }

  if (preferred_packet_in_format >= 0) {
    /* A particular packet-in format was requested, so we must set it. */
    set_packet_in_format(
            vconn, static_cast<ofputil_packet_in_format>(preferred_packet_in_format), true);
  } else {
    /* Otherwise, we always prefer NXT_PACKET_IN2. */
    if (!set_packet_in_format(vconn, OFPUTIL_PACKET_IN_NXT2, false)) {
      /* We can't get NXT_PACKET_IN2.  For OpenFlow 1.0 only, request
             * NXT_PACKET_IN.  (Before 2.6, Open vSwitch will accept a request
             * for NXT_PACKET_IN with OF1.1+, but even after that it still
             * sends packet-ins in the OpenFlow native format.) */
      if (vconn_get_version(vconn) == OFP10_VERSION) {
        set_packet_in_format(vconn, OFPUTIL_PACKET_IN_NXT, false);
      }
    }
  }

  monitor_vconn(vconn, true, resume_continuations, bridge);
}

void OVS_Control::packet_out(const char *bridge, const char *options)
{
  enum ofputil_protocol usable_protocols;
  enum ofputil_protocol protocol;
  struct ofputil_packet_out po;
  struct vconn *vconn;
  struct ofpbuf *opo;
  char *error;

  error = parse_ofp_packet_out_str(&po, options, ports_to_accept(bridge),
                                   tables_to_accept(bridge), &usable_protocols);
  if (error) {
    //ovs_fatal(0, "%s", error);
    ACA_LOG_ERROR("%s", error);
  }
  protocol = open_vconn_for_flow_mod(bridge, &vconn, usable_protocols);
  opo = ofputil_encode_packet_out(&po, protocol);
  transact_noreply(vconn, opo);
  vconn_close(vconn);
  free(CONST_CAST(void *, po.packet));
  free(po.ofpacts);
}

int OVS_Control::dump_flows(const char *bridge, const char *flow, bool show_stats)
{
  ACA_LOG_DEBUG("%s", "OVS_Control::dump_flows ---> Entering\n");

  int rc = EXIT_FAILURE;
  int n_criteria = 0;

  ACA_LOG_INFO("Executing dump_flows on bridge: %s, flow: %s, show_stats: %d\n",
               bridge, flow, show_stats);

  auto openflow_client_start = chrono::steady_clock::now();

  if (!n_criteria && !should_show_names() && show_stats) {
    dump_flows__(bridge, flow, false);
    rc = EXIT_SUCCESS;
  } else {
    ofputil_flow_stats_request fsr;
    enum ofputil_protocol protocol;
    struct vconn *vconn;

    vconn = prepare_dump_flows(bridge, flow, false, &fsr, &protocol);
    struct ofputil_flow_stats *fses;
    size_t n_fses;
    run(vconn_dump_flows(vconn, &fsr, protocol, &fses, &n_fses), "dump flows");

    struct ds s = DS_EMPTY_INITIALIZER;
    for (size_t i = 0; i < n_fses; i++) {
      ds_clear(&s);
      ofputil_flow_stats_format(&s, &fses[i], ports_to_show(bridge), tables_to_show(bridge),
                                //ports_to_show(ctx->argv[1]),
                                //tables_to_show(ctx->argv[1]),
                                show_stats);
      ACA_LOG_DEBUG(" %s\n", ds_cstr(&s));
    }
    if (n_fses > 0)
      rc = EXIT_SUCCESS;

    ds_destroy(&s);

    for (size_t i = 0; i < n_fses; i++) {
      free(CONST_CAST(struct ofpact *, fses[i].ofpacts));
    }
    free(fses);

    vconn_close(vconn);
  }

  auto openflow_client_end = chrono::steady_clock::now();

  auto openflow_client_time_total_time =
          cast_to_microseconds(openflow_client_end - openflow_client_start).count();

  g_total_execute_openflow_time += openflow_client_time_total_time;

  ACA_LOG_INFO("Elapsed time for dump_flows call took: %ld microseconds or %ld milliseconds. rc: %d\n",
               openflow_client_time_total_time,
               us_to_ms(openflow_client_time_total_time), rc);

  ACA_LOG_DEBUG("OVS_Control::dump_flows <--- Exiting, rc = %d\n", rc);

  return rc;
}

void OVS_Control::dump_flows__(const char *bridge, const char *flow, bool aggregate)
{
  struct ofputil_flow_stats_request fsr;
  enum ofputil_protocol protocol;
  struct vconn *vconn;

  vconn = prepare_dump_flows(bridge, flow, aggregate, &fsr, &protocol);
  dump_transaction(vconn, ofputil_encode_flow_stats_request(&fsr, protocol), bridge);
  vconn_close(vconn);
}

vconn *OVS_Control::prepare_dump_flows(const char *bridge, const char *flow,
                                       bool aggregate, ofputil_flow_stats_request *fsr,
                                       ofputil_protocol *protocolp)
{
  const char *vconn_name = bridge;
  enum ofputil_protocol usable_protocols, protocol;
  struct vconn *vconn;
  char *error;

  // const char *match = argc > 2 ? argv[2] : "";
  const char *match = flow;
  const struct ofputil_port_map *port_map = *match ? ports_to_accept(vconn_name) : NULL;
  const struct ofputil_table_map *table_map = *match ? tables_to_accept(vconn_name) : NULL;
  error = parse_ofp_flow_stats_request_str(fsr, aggregate, match, port_map,
                                           table_map, &usable_protocols);
  if (error) {
    //ovs_fatal(0, "%s", error);
    ACA_LOG_ERROR("%s", error);
  }

  protocol = open_vconn(vconn_name, &vconn);
  *protocolp = set_protocol_for_flow_dump(vconn, protocol, usable_protocols);
  return vconn;
}

enum ofputil_protocol
OVS_Control::set_protocol_for_flow_dump(vconn *vconn, ofputil_protocol cur_protocol,
                                        ofputil_protocol usable_protocols)
{
  char *usable_s;
  int i;

  for (i = 0; i < (int)ofputil_n_flow_dump_protocols; i++) {
    enum ofputil_protocol f = ofputil_flow_dump_protocols[i];
    if (f & usable_protocols & allowed_protocols && try_set_protocol(vconn, f, &cur_protocol)) {
      return f;
    }
  }

  usable_s = ofputil_protocols_to_string(usable_protocols);
  if (usable_protocols & allowed_protocols) {
    // ovs_fatal(0, "switch does not support any of the usable flow "
    //           "formats (%s)", usable_s);
    ACA_LOG_ERROR("switch does not support any of the usable flow "
                  "formats (%s)",
                  usable_s);
  } else {
    char *allowed_s = ofputil_protocols_to_string(allowed_protocols);
    // ovs_fatal(0, "none of the usable flow formats (%s) is among the "
    //           "allowed flow formats (%s)", usable_s, allowed_s);
    ACA_LOG_ERROR("none of the usable flow formats (%s) is among the "
                  "allowed flow formats (%s)",
                  usable_s, allowed_s);
  }
  return (ofputil_protocol)0;
}

int OVS_Control::add_flow(const char *bridge, const char *flow)
{
  return flow_mod(bridge, flow, OFPFC_ADD);
}

int OVS_Control::mod_flows(const char *bridge, const char *flow, bool strict)
{
  return flow_mod(bridge, flow, strict ? OFPFC_MODIFY_STRICT : OFPFC_MODIFY);
}

int OVS_Control::del_flows(const char *bridge, const char *flow, bool strict)
{
  return flow_mod(bridge, flow, strict ? OFPFC_DELETE_STRICT : OFPFC_DELETE);
}

int OVS_Control::flow_mod(const char *bridge, const char *flow, unsigned short int command)
{
  ACA_LOG_DEBUG("%s", "OVS_Control::flow_mod ---> Entering\n");

  struct ofputil_flow_mod fm;
  char *error;
  enum ofputil_protocol usable_protocols;
  int rc;

  ACA_LOG_INFO("Executing flow_mod on bridge: %s, flow: %s, command: %d\n",
               bridge, flow, command);

  auto openflow_client_start = chrono::steady_clock::now();
  error = parse_ofp_flow_mod_str(&fm, flow, ports_to_accept(bridge),
                                 tables_to_accept(bridge), command, &usable_protocols);
  if (error) {
    // ovs_fatal(0, "%s", error);
    ACA_LOG_ERROR("%s", error);
    rc = EXIT_FAILURE;
  } else {
    // flow_mod__ returns void
    std::chrono::_V2::steady_clock::time_point start = std::chrono::steady_clock::now();

    flow_mod__(bridge, &fm, 1, usable_protocols);
    std::chrono::_V2::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto message_total_operation_time =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    ACA_LOG_INFO("[flow_mod] Start flow_mod__ at: [%ld], finished at: [%ld]\nElapsed time for flow_mod__ took: %ld microseconds or %ld milliseconds\n",
                 start, end, message_total_operation_time,
                 (message_total_operation_time / 1000));
    rc = EXIT_SUCCESS;
  }

  auto openflow_client_end = chrono::steady_clock::now();

  auto openflow_client_time_total_time =
          cast_to_microseconds(openflow_client_end - openflow_client_start).count();

  g_total_execute_openflow_time += openflow_client_time_total_time;

  ACA_LOG_INFO("Elapsed time for flow_mod call took: %ld microseconds or %ld milliseconds. rc: %d\n",
               openflow_client_time_total_time,
               us_to_ms(openflow_client_time_total_time), rc);

  ACA_LOG_DEBUG("OVS_Control::flow_mod <--- Exiting, rc = %d\n", rc);

  return rc;
}

void OVS_Control::flow_mod__(const char *remote, struct ofputil_flow_mod *fms,
                             size_t n_fms, enum ofputil_protocol usable_protocols)
{
  enum ofputil_protocol protocol;
  struct vconn *vconn;
  size_t i;

  if (bundle) {
    bundle_flow_mod__(remote, fms, n_fms, usable_protocols);
    return;
  }

  protocol = open_vconn_for_flow_mod(remote, &vconn, usable_protocols);

  for (i = 0; i < n_fms; i++) {
    struct ofputil_flow_mod *fm = &fms[i];

    transact_noreply(vconn, ofputil_encode_flow_mod(fm, protocol));
    free(CONST_CAST(struct ofpact *, fm->ofpacts));
    minimatch_destroy(&fm->match);
  }
  vconn_close(vconn);
}

void OVS_Control::bundle_flow_mod__(const char *remote, struct ofputil_flow_mod *fms,
                                    size_t n_fms, enum ofputil_protocol usable_protocols)
{
  enum ofputil_protocol protocol;
  struct vconn *vconn;
  struct ovs_list requests;
  size_t i;

  ovs_list_init(&requests);

  /* Bundles need OpenFlow 1.3+. */
  // usable_protocols &= OFPUTIL_P_OF13_UP;
  protocol = open_vconn_for_flow_mod(remote, &vconn, usable_protocols);

  for (i = 0; i < n_fms; i++) {
    struct ofputil_flow_mod *fm = &fms[i];
    struct ofpbuf *request = ofputil_encode_flow_mod(fm, protocol);

    ovs_list_push_back(&requests, &request->list_node);
    free(CONST_CAST(struct ofpact *, fm->ofpacts));
    minimatch_destroy(&fm->match);
  }

  bundle_transact(vconn, &requests, OFPBF_ORDERED | OFPBF_ATOMIC);
  ofpbuf_list_delete(&requests);
  vconn_close(vconn);
}

enum ofputil_protocol
OVS_Control::open_vconn_for_flow_mod(const char *remote, vconn **vconnp,
                                     enum ofputil_protocol usable_protocols)
{
  enum ofputil_protocol cur_protocol;
  char *usable_s;
  int i;

  if (!(usable_protocols & allowed_protocols)) {
    char *allowed_s = ofputil_protocols_to_string(allowed_protocols);
    usable_s = ofputil_protocols_to_string(usable_protocols);
    // ovs_fatal(0, "none of the usable flow formats (%s) is among the "
    //           "allowed flow formats (%s)", usable_s, allowed_s);
    ACA_LOG_ERROR("none of the usable flow formats (%s) is among the "
                  "allowed flow formats (%s)",
                  usable_s, allowed_s);
  }

  /* If the initial flow format is allowed and usable, keep it. */
  cur_protocol = open_vconn(remote, vconnp);
  if (usable_protocols & allowed_protocols & cur_protocol) {
    return cur_protocol;
  }

  /* Otherwise try each flow format in turn. */
  for (i = 0; i < (int)sizeof(enum ofputil_protocol) * CHAR_BIT; i++) {
    enum ofputil_protocol f = (ofputil_protocol)(1 << i);

    if (f != cur_protocol && f & usable_protocols & allowed_protocols &&
        try_set_protocol(*vconnp, f, &cur_protocol)) {
      return f;
    }
  }

  usable_s = ofputil_protocols_to_string(usable_protocols);
  // ovs_fatal(0, "switch does not support any of the usable flow "
  //           "formats (%s)", usable_s);
  ACA_LOG_ERROR("switch does not support any of the usable flow "
                "formats (%s)",
                usable_s);
  return (ofputil_protocol)0;
}

/* Returns the port number corresponding to 'port_name' (which may be a port
 * name or number) within the switch 'vconn_name'. */
ofp_port_t OVS_Control::str_to_port_no(const char *vconn_name, const char *port_name)
{
  ofp_port_t port_no;
  if (ofputil_port_from_string(port_name, NULL, &port_no) ||
      ofputil_port_from_string(port_name, ports_to_accept(vconn_name), &port_no)) {
    return port_no;
  }
  // ovs_fatal(0, "%s: unknown port `%s'", vconn_name, port_name);
  ACA_LOG_ERROR("%s: unknown port `%s'", vconn_name, port_name);
  return (ofputil_protocol)0;
}

bool OVS_Control::try_set_protocol(struct vconn *vconn, enum ofputil_protocol want,
                                   enum ofputil_protocol *cur)
{
  for (;;) {
    struct ofpbuf *request, *reply;
    enum ofputil_protocol next;

    request = ofputil_encode_set_protocol(*cur, want, &next);
    if (!request) {
      return *cur == want;
    }

    run(vconn_transact_noreply(vconn, request, &reply), "talking to %s",
        vconn_get_name(vconn));
    if (reply) {
      char *s = ofp_to_string(reply->data, reply->size, NULL, NULL, 2);
      VLOG_DBG("%s: failed to set protocol, switch replied: %s", vconn_get_name(vconn), s);
      free(s);
      ofpbuf_delete(reply);
      return false;
    }

    *cur = next;
  }
}

void OVS_Control::fetch_switch_config(vconn *vconn, ofputil_switch_config *config)
{
  struct ofpbuf *request;
  struct ofpbuf *reply;
  enum ofptype type;

  request = ofpraw_alloc(OFPRAW_OFPT_GET_CONFIG_REQUEST, vconn_get_version(vconn), 0);
  run(vconn_transact(vconn, request, &reply), "talking to %s", vconn_get_name(vconn));

  if (ofptype_decode(&type, (ofp_header *)reply->data) || type != OFPTYPE_GET_CONFIG_REPLY) {
    // ovs_fatal(0, "%s: bad reply to config request", vconn_get_name(vconn));
    ACA_LOG_ERROR("%s: bad reply to config request", vconn_get_name(vconn));
  }
  ofputil_decode_get_config_reply((ofp_header *)reply->data, config);
  ofpbuf_delete(reply);
}

void OVS_Control::set_switch_config(vconn *vconn, const ofputil_switch_config *config)
{
  ofp_version version = static_cast<ofp_version>(vconn_get_version(vconn));
  transact_noreply(vconn, ofputil_encode_set_config(config, version));
}

int OVS_Control::open_vconn_socket(const char *name, vconn **vconnp)
{
  char vconn_name[50];
  int error;

  sprintf(vconn_name, "unix:%s", name);
  error = vconn_open(vconn_name, get_allowed_ofp_versions(), DSCP_DEFAULT, vconnp);
  if (error && error != ENOENT) {
    // ovs_fatal(0, "%s: failed to open socket (%s)", name,
    //           ovs_strerror(error));
    ACA_LOG_ERROR("%s: failed to open socket (%s)", name, ovs_strerror(error));
  }
  // free(vconn_name);
  return error;
}

enum ofputil_protocol OVS_Control::open_vconn(const char *name, vconn **vconnp)
{
  const char *suffix = "mgmt";
  char *datapath_name, *datapath_type;
  enum ofputil_protocol protocol;
  char bridge_path[50] = "", socket_name[50] = "";
  int version;
  int error;

  sprintf(bridge_path, "%s/%s.%s", ovs_rundir(), name, suffix);
  dp_parse_name(name, &datapath_name, &datapath_type);
  sprintf(socket_name, "%s/%s.%s", ovs_rundir(), datapath_name, suffix);
  free(datapath_name);
  free(datapath_type);
  if (strchr(name, ':')) {
    run(vconn_open(name, get_allowed_ofp_versions(), DSCP_DEFAULT, vconnp),
        "connecting to %s", name);
  } else if (!open_vconn_socket(name, vconnp)) {
    /* Fall Through. */
  } else if (!open_vconn_socket(bridge_path, vconnp)) {
    /* Fall Through. */
  } else if (!open_vconn_socket(socket_name, vconnp)) {
    /* Fall Through. */
  } else {
    // free(bridge_path);
    // free(socket_name);
    // ovs_fatal(0, "%s is not a bridge or a socket", name);
    ACA_LOG_ERROR("%s is not a bridge or a socket", name);
  }

  // if (target == SNOOP) {
  //     vconn_set_recv_any_version(*vconnp);
  // }

  // free(bridge_path);
  // free(socket_name);

  VLOG_DBG("connecting to %s", vconn_get_name(*vconnp));
  error = vconn_connect_block(*vconnp, -1);
  if (error) {
    // ovs_fatal(0, "%s: failed to connect to socket (%s)", name,
    //            ovs_strerror(error));
    ACA_LOG_ERROR("%s: failed to connect to socket (%s)", name, ovs_strerror(error));
  }

  version = vconn_get_version(*vconnp);
  protocol = ofputil_protocol_from_ofp_version(static_cast<ofp_version>(version));
  if (!protocol) {
    // ovs_fatal(0, "%s: unsupported OpenFlow version 0x%02x",
    //            name, version);
    ACA_LOG_ERROR("%s: unsupported OpenFlow version 0x%02x", name, version);
  }
  return protocol;
}

int OVS_Control::monitor_set_invalid_ttl_to_controller(vconn *vconn)
{
  struct ofputil_switch_config config;

  fetch_switch_config(vconn, &config);
  if (!config.invalid_ttl_to_controller) {
    config.invalid_ttl_to_controller = 1;
    set_switch_config(vconn, &config);

    /* Then retrieve the configuration to see if it really took.  OpenFlow
         * has ill-defined error reporting for bad flags, so this is about the
         * best we can do. */
    fetch_switch_config(vconn, &config);
    if (!config.invalid_ttl_to_controller) {
      // ovs_fatal(0, "setting invalid_ttl_to_controller failed (this "
      //           "switch probably doesn't support this flag)");
      ACA_LOG_ERROR("%s", "setting invalid_ttl_to_controller failed (this "
                          "switch probably doesn't support this flag)");
    }
  }
  return 0;
}

/* Converts hex digits in 'hex' to an OpenFlow message in '*msgp'.  The
 * caller must free '*msgp'.  On success, returns NULL.  On failure, returns
 * an error message and stores NULL in '*msgp'. */
const char *OVS_Control::openflow_from_hex(const char *hex, ofpbuf **msgp)
{
  struct ofp_header *oh;
  struct ofpbuf *msg;

  msg = ofpbuf_new(strlen(hex) / 2);
  *msgp = NULL;

  if (ofpbuf_put_hex(msg, hex, NULL)[0] != '\0') {
    ofpbuf_delete(msg);
    return "Trailing garbage in hex data";
  }

  if (msg->size < sizeof(struct ofp_header)) {
    ofpbuf_delete(msg);
    return "Message too short for OpenFlow";
  }

  oh = (ofp_header *)msg->data;
  if (msg->size != ntohs(oh->length)) {
    ofpbuf_delete(msg);
    return "Message size does not match length in OpenFlow header";
  }

  *msgp = msg;
  return NULL;
}

/* Prints to stderr all of the messages received on 'vconn'.
 *
 * Iff 'reply_to_echo_requests' is true, sends a reply to any echo request
 * received on 'vconn'.
 *
 * If 'resume_continuations' is true, sends an NXT_RESUME in reply to any
 * NXT_PACKET_IN2 that includes a continuation. */
void OVS_Control::monitor_vconn(vconn *vconn, bool reply_to_echo_requests,
                                bool resume_continuations, const char *bridge_)
{
  static const char *bridge = bridge_;
  bool timestamp = true;
  struct barrier_aux barrier_aux = { vconn, NULL };
  struct unixctl_server *server;
  bool exiting = false;
  bool blocked = false;
  int error;

  // Put all functions used by daemon in a local struct.
  struct X {
    static void ofctl_exit(unixctl_conn *conn, int argc OVS_UNUSED,
                           const char *argv[] OVS_UNUSED, void *exiting_)
    {
      bool *exiting = (bool *)exiting_;
      *exiting = true;
      unixctl_command_reply(conn, NULL);
    }

    static void ofctl_send(unixctl_conn *conn, int argc, const char *argv[], void *vconn_)
    {
      struct vconn *vconn = (struct vconn *)vconn_;
      struct ds reply;
      bool ok;
      int i;

      ok = true;
      ds_init(&reply);
      for (i = 1; i < argc; i++) {
        const char *error_msg;
        struct ofpbuf *msg;
        int error;

        error_msg = OVS_Control().openflow_from_hex(argv[i], &msg);
        if (error_msg) {
          ds_put_format(&reply, "%s\n", error_msg);
          ok = false;
          continue;
        }

        fprintf(stderr, "send: ");
        ofp_print(stderr, msg->data, msg->size,
                  // ports_to_show(vconn_get_name(vconn)),
                  // tables_to_show(vconn_get_name(vconn)), verbosity);
                  OVS_Control().ports_to_show(bridge),
                  OVS_Control().tables_to_show(bridge), verbosity);
        error = vconn_send_block(vconn, msg);
        if (error) {
          ofpbuf_delete(msg);
          ds_put_format(&reply, "%s\n", ovs_strerror(error));
          ok = false;
        } else {
          ds_put_cstr(&reply, "sent\n");
        }
      }

      if (ok) {
        unixctl_command_reply(conn, ds_cstr(&reply));
      } else {
        unixctl_command_reply_error(conn, ds_cstr(&reply));
      }
      ds_destroy(&reply);
    }

    static void unixctl_packet_out(struct unixctl_conn *conn, int OVS_UNUSED argc,
                                   const char *argv[], void *vconn_)
    {
      struct vconn *vconn = (struct vconn *)vconn_;
      enum ofputil_protocol protocol = ofputil_protocol_from_ofp_version(
              static_cast<ofp_version>(vconn_get_version(vconn)));
      struct ds reply = DS_EMPTY_INITIALIZER;
      bool ok = true;

      enum ofputil_protocol usable_protocols;
      struct ofputil_packet_out po;
      char *error_msg;

      error_msg = parse_ofp_packet_out_str(&po, argv[1],
                                           // ports_to_accept(vconn_get_name(vconn)),
                                           // tables_to_accept(vconn_get_name(vconn)),
                                           OVS_Control().ports_to_accept(bridge),
                                           OVS_Control().tables_to_accept(bridge),
                                           &usable_protocols);
      if (error_msg) {
        ds_put_format(&reply, "%s\n", error_msg);
        free(error_msg);
        ok = false;
      }

      if (ok && !(usable_protocols & protocol)) {
        ds_put_format(&reply, "PACKET_OUT actions are incompatible with the OpenFlow connection.\n");
        ok = false;
      }

      if (ok) {
        struct ofpbuf *msg = ofputil_encode_packet_out(&po, protocol);

        ofp_print(stderr, msg->data, msg->size,
                  // ports_to_show(vconn_get_name(vconn)),
                  // tables_to_show(vconn_get_name(vconn)), verbosity);
                  OVS_Control().ports_to_show(bridge),
                  OVS_Control().tables_to_show(bridge), verbosity);
        int error = vconn_send_block(vconn, msg);
        if (error) {
          ofpbuf_delete(msg);
          ds_put_format(&reply, "%s\n", ovs_strerror(error));
          ok = false;
        }
      }

      if (ok) {
        unixctl_command_reply(conn, ds_cstr(&reply));
      } else {
        unixctl_command_reply_error(conn, ds_cstr(&reply));
      }
      ds_destroy(&reply);

      if (!error_msg) {
        free(CONST_CAST(void *, po.packet));
        free(po.ofpacts);
      }
    }

    static void ofctl_barrier(struct unixctl_conn *conn, int argc OVS_UNUSED,
                              const char *argv[] OVS_UNUSED, void *aux_)
    {
      struct barrier_aux *aux = (struct barrier_aux *)aux_;
      struct ofpbuf *msg;
      int error;

      if (aux->conn) {
        unixctl_command_reply_error(conn, "already waiting for barrier reply");
        return;
      }

      msg = ofputil_encode_barrier_request(
              static_cast<ofp_version>(vconn_get_version(aux->vconn)));
      error = vconn_send_block(aux->vconn, msg);
      if (error) {
        ofpbuf_delete(msg);
        unixctl_command_reply_error(conn, ovs_strerror(error));
      } else {
        aux->conn = conn;
      }
    }

    static void ofctl_set_output_file(struct unixctl_conn *conn, int argc OVS_UNUSED,
                                      const char *argv[], void *aux OVS_UNUSED)
    {
      int fd;

      fd = open(argv[1], O_CREAT | O_TRUNC | O_WRONLY, 0666);
      if (fd < 0) {
        unixctl_command_reply_error(conn, ovs_strerror(errno));
        return;
      }

      fflush(stderr);
      dup2(fd, STDERR_FILENO);
      close(fd);
      unixctl_command_reply(conn, NULL);
    }

    static void ofctl_block(struct unixctl_conn *conn, int argc OVS_UNUSED,
                            const char *argv[] OVS_UNUSED, void *blocked_)
    {
      bool *blocked = (bool *)blocked_;

      if (!*blocked) {
        *blocked = true;
        unixctl_command_reply(conn, NULL);
      } else {
        unixctl_command_reply(conn, "already blocking");
      }
    }

    static void ofctl_unblock(struct unixctl_conn *conn, int argc OVS_UNUSED,
                              const char *argv[] OVS_UNUSED, void *blocked_)
    {
      bool *blocked = (bool *)blocked_;

      if (*blocked) {
        *blocked = false;
        unixctl_command_reply(conn, NULL);
      } else {
        unixctl_command_reply(conn, "already unblocked");
      }
    }
  };

  daemon_save_fd(STDERR_FILENO);
  daemonize_start(false);
  error = unixctl_server_create(unixctl_path, &server);
  if (error) {
    // ovs_fatal(error, "failed to create unixctl server");
    ACA_LOG_ERROR("%s", "failed to create unixctl server");
  }
  unixctl_command_register("exit", "", 0, 0, X::ofctl_exit, &exiting);
  unixctl_command_register("ofctl/send", "OFMSG...", 1, INT_MAX, X::ofctl_send, vconn);
  unixctl_command_register("ofctl/packet-out", "\"in_port=<port> packet=<hex data> actions=...\"",
                           1, 1, X::unixctl_packet_out, vconn);
  unixctl_command_register("ofctl/barrier", "", 0, 0, X::ofctl_barrier, &barrier_aux);
  unixctl_command_register("ofctl/set-output-file", "FILE", 1, 1,
                           X::ofctl_set_output_file, NULL);

  unixctl_command_register("ofctl/block", "", 0, 0, X::ofctl_block, &blocked);
  unixctl_command_register("ofctl/unblock", "", 0, 0, X::ofctl_unblock, &blocked);

  daemonize_complete();

  enum ofp_version version = static_cast<ofp_version>(vconn_get_version(vconn));
  enum ofputil_protocol protocol = ofputil_protocol_from_ofp_version(version);

  for (;;) {
    struct ofpbuf *b;
    int retval;

    unixctl_server_run(server);

    while (!blocked) {
      enum ofptype type;

      retval = vconn_recv(vconn, &b);
      if (retval == EAGAIN) {
        break;
      }
      run(retval, "vconn_recv");

      if (timestamp) {
        char *s = xastrftime_msec("%Y-%m-%d %H:%M:%S.###: ", time_wall_msec(), true);
        fputs(s, stderr);
        free(s);
      }
      ofptype_decode(&type, (ofp_header *)b->data);

      ofp_print(stderr, b->data, b->size,
                // ports_to_show(vconn_get_name(vconn)),
                // tables_to_show(vconn_get_name(vconn)), verbosity + 2);
                ports_to_show(bridge), tables_to_show(bridge), verbosity + 2);
      fflush(stderr);

      switch ((int)type) {
      case OFPTYPE_BARRIER_REPLY:
        if (barrier_aux.conn) {
          unixctl_command_reply(barrier_aux.conn, NULL);
          barrier_aux.conn = NULL;
        }
        break;

      case OFPTYPE_ECHO_REQUEST:
        if (reply_to_echo_requests) {
          struct ofpbuf *reply;

          reply = ofputil_encode_echo_reply((ofp_header *)b->data);
          retval = vconn_send_block(vconn, reply);
          if (retval) {
            // ovs_fatal(retval, "failed to send echo reply");
            ACA_LOG_ERROR("%s", "failed to send echo reply");
          }
        }
        break;

      case OFPTYPE_PACKET_IN:
        if (resume_continuations) {
          struct ofputil_packet_in pin;
          struct ofpbuf continuation;
          size_t total_lenp;
          uint32_t buffer_idp;

          error = ofputil_decode_packet_in((ofp_header *)b->data, true, NULL, NULL,
                                           &pin, &total_lenp, &buffer_idp, &continuation);
          uint32_t in_port = pin.flow_metadata.flow.in_port.ofp_port;
          /*
            The pin.packet here has the same memory address, even after multiple calls.
            If you intent to store it somewhere, it is advised to make a copy of it.
          */
          ACA_On_Demand_Engine::get_instance().parse_packet(in_port, pin.packet);

          if (error) {
            fprintf(stderr, "decoding packet-in failed: %s",
                    ofperr_to_string((ofperr)error));
          } else if (continuation.size) {
            struct ofpbuf *reply;

            reply = ofputil_encode_resume(&pin, &continuation, protocol);

            fprintf(stderr, "send: ");
            ofp_print(stderr, reply->data, reply->size, ports_to_show(bridge),
                      tables_to_show(bridge),
                      // ports_to_show(vconn_get_name(vconn)),
                      // tables_to_show(vconn_get_name(vconn)),
                      verbosity + 2);
            fflush(stderr);

            retval = vconn_send_block(vconn, reply);
            if (retval) {
              // ovs_fatal(retval, "failed to send NXT_RESUME");
              ACA_LOG_ERROR("%s", "failed to send NXT_RESUME");
            }
          }
        }
        break;
      }
      ofpbuf_delete(b);
    }

    if (exiting) {
      break;
    }
    vconn_run(vconn);
    vconn_run_wait(vconn);
    if (!blocked) {
      vconn_recv_wait(vconn);
    }
    unixctl_server_wait(server);
    poll_block();
  }
  vconn_close(vconn);
  unixctl_server_destroy(server);
}

void OVS_Control::run(int retval, const char *message, ...)
{
  if (retval) {
    va_list args;

    va_start(args, message);
    ovs_fatal_valist(retval, message, args);
  }
}

bool OVS_Control::set_packet_in_format(vconn *vconn, enum ofputil_packet_in_format packet_in_format,
                                       bool must_succeed)
{
  struct ofpbuf *spif;

  spif = ofputil_encode_set_packet_in_format(
          static_cast<ofp_version>(vconn_get_version(vconn)), packet_in_format);
  if (must_succeed) {
    transact_noreply(vconn, spif);
  } else {
    struct ofpbuf *reply;

    run(vconn_transact_noreply(vconn, spif, &reply), "talking to %s", vconn_get_name(vconn));
    if (reply) {
      char *s = ofp_to_string(reply->data, reply->size, NULL, NULL, 2);
      VLOG_DBG("%s: failed to set packet in format to nx_packet_in, "
               "controller replied: %s.",
               vconn_get_name(vconn), s);
      free(s);
      ofpbuf_delete(reply);

      return false;
    } else {
      VLOG_DBG("%s: using user-specified packet in format %s", vconn_get_name(vconn),
               ofputil_packet_in_format_to_string(packet_in_format));
    }
  }
  return true;
}

void OVS_Control::bundle_transact(struct vconn *vconn, struct ovs_list *requests, uint16_t flags)
{
  struct ovs_list errors;
  int retval = vconn_bundle_transact(vconn, requests, flags, &errors);

  bundle_print_errors(&errors, requests, vconn_get_name(vconn));

  if (retval) {
    // ovs_fatal(retval, "talking to %s", vconn_get_name(vconn));
    ACA_LOG_ERROR("talking to %s", vconn_get_name(vconn));
  }
}

/* Frees the error messages as they are printed. */
void OVS_Control::bundle_print_errors(struct ovs_list *errors,
                                      struct ovs_list *requests, const char *vconn_name)
{
  struct ofpbuf *error, *next;
  struct ofpbuf *bmsg;

  INIT_CONTAINER(bmsg, requests, list_node);

  LIST_FOR_EACH_SAFE(error, next, list_node, errors)
  {
    const struct ofp_header *error_oh = (ofp_header *)error->data;
    ovs_be32 error_xid = error_oh->xid;
    enum ofperr ofperr;
    struct ofpbuf payload;

    ofperr = ofperr_decode_msg(error_oh, &payload);
    if (!ofperr) {
      fprintf(stderr, "***decode error***");
    } else {
      /* Default to the likely truncated message. */
      const struct ofp_header *ofp_msg = (ofp_header *)payload.data;
      size_t msg_len = payload.size;

      /* Find the failing message from the requests list to be able to
             * dump the whole message.  We assume the errors are returned in
             * the same order as in which the messages are sent to get O(n)
             * rather than O(n^2) processing here.  If this heuristics fails we
             * may print the truncated hexdumps instead. */
      LIST_FOR_EACH_CONTINUE(bmsg, list_node, requests)
      {
        const struct ofp_header *oh = (ofp_header *)bmsg->data;

        if (oh->xid == error_xid) {
          ofp_msg = oh;
          msg_len = bmsg->size;
          break;
        }
      }
      fprintf(stderr, "Error %s for: ", ofperr_get_name(ofperr));
      ofp_print(stderr, ofp_msg, msg_len, ports_to_show(vconn_name),
                tables_to_show(vconn_name), verbosity + 1);
    }
    ofpbuf_uninit(&payload);
    ofpbuf_delete(error);
  }
  fflush(stderr);
}

/* Sends 'request', which should be a request that only has a reply if an error
 * occurs, and waits for it to succeed or fail.  If an error does occur, prints
 * it and exits with an error.
 *
 * Destroys 'request'. */
void OVS_Control::transact_noreply(vconn *vconn, ofpbuf *request)
{
  struct ovs_list requests;

  ovs_list_init(&requests);
  ovs_list_push_back(&requests, &request->list_node);
  transact_multiple_noreply(vconn, &requests);
}

/* Sends all of the 'requests', which should be requests that only have replies
 * if an error occurs, and waits for them to succeed or fail.  If an error does
 * occur, prints it and exits with an error.
 *
 * Destroys all of the 'requests'. */
void OVS_Control::transact_multiple_noreply(vconn *vconn, ovs_list *requests)
{
  struct ofpbuf *reply;

  run(vconn_transact_multiple_noreply(vconn, requests, &reply), "talking to %s",
      vconn_get_name(vconn));
  if (reply) {
    ofp_print(stderr, reply->data, reply->size, ports_to_show(vconn_get_name(vconn)),
              tables_to_show(vconn_get_name(vconn)), verbosity + 2);
    exit(1);
  }
  ofpbuf_delete(reply);
}

void OVS_Control::send_openflow_buffer(vconn *vconn, ofpbuf *buffer)
{
  run(vconn_send_block(vconn, buffer), "failed to send packet to switch");
}

void OVS_Control::dump_transaction(vconn *vconn, ofpbuf *request, const char *bridge)
{
  const ofp_header *oh = (ofp_header *)request->data;
  if (ofpmsg_is_stat_request(oh)) {
    ovs_be32 send_xid = oh->xid;
    enum ofpraw request_raw;
    enum ofpraw reply_raw;
    bool done = false;

    ofpraw_decode_partial(&request_raw, (ofp_header *)request->data, request->size);
    reply_raw = ofpraw_stats_request_to_reply(request_raw, oh->version);

    send_openflow_buffer(vconn, request);
    while (!done) {
      ovs_be32 recv_xid;
      struct ofpbuf *reply;

      run(vconn_recv_block(vconn, &reply), "OpenFlow packet receive failed");
      recv_xid = ((struct ofp_header *)reply->data)->xid;
      if (send_xid == recv_xid) {
        enum ofpraw ofpraw;
        ofp_print(stdout, reply->data, reply->size, ports_to_show(bridge),
                  tables_to_show(bridge),
                  //   ports_to_show(vconn_get_name(vconn)),
                  //   tables_to_show(vconn_get_name(vconn)),
                  verbosity + 1);

        ofpraw_decode(&ofpraw, (struct ofp_header *)reply->data);
        if (ofptype_from_ofpraw(ofpraw) == OFPTYPE_ERROR) {
          done = true;
        } else if (ofpraw == reply_raw) {
          done = !ofpmp_more((struct ofp_header *)reply->data);
        } else {
          // ovs_fatal(0, "received bad reply: %s",
          //           ofp_to_string(
          //               reply->data, reply->size,
          //               ports_to_show(vconn_get_name(vconn)),
          //               tables_to_show(vconn_get_name(vconn)),
          //               OVS_Control::verbosity + 1));
          ACA_LOG_ERROR("received bad reply: %s",
                        ofp_to_string(reply->data, reply->size,
                                      ports_to_show(vconn_get_name(vconn)),
                                      tables_to_show(vconn_get_name(vconn)),
                                      OVS_Control::verbosity + 1));
        }
      } else {
        VLOG_DBG("received reply with xid %08" PRIx32 " "
                 "!= expected %08" PRIx32,
                 recv_xid, send_xid);
      }
      ofpbuf_delete(reply);
    }
  } else {
    struct ofpbuf *reply;
    run(vconn_transact(vconn, request, &reply), "talking to %s", vconn_get_name(vconn));
    ofp_print(stdout, reply->data, reply->size, ports_to_show(vconn_get_name(vconn)),
              tables_to_show(vconn_get_name(vconn)), verbosity + 1);
    ofpbuf_delete(reply);
  }
}

bool OVS_Control::str_to_ofp(const char *s, ofp_port_t *ofp_port)
{
  bool ret;
  uint32_t port_;

  ret = str_to_uint(s, 10, &port_);
  *ofp_port = OFP_PORT_C(port_);

  return ret;
}

void OVS_Control::port_iterator_fetch_port_desc(port_iterator *pi)
{
  pi->variant = PI_PORT_DESC;
  pi->more = true;

  struct ofpbuf *rq = ofputil_encode_port_desc_stats_request(
          static_cast<ofp_version>(vconn_get_version(pi->vconn)), OFPP_ANY);
  pi->send_xid = ((struct ofp_header *)rq->data)->xid;
  send_openflow_buffer(pi->vconn, rq);
}

void OVS_Control::port_iterator_fetch_features(port_iterator *pi)
{
  pi->variant = PI_FEATURES;

  /* Fetch the switch's ofp_switch_features. */
  enum ofp_version version = static_cast<ofp_version>(vconn_get_version(pi->vconn));
  struct ofpbuf *rq = ofpraw_alloc(OFPRAW_OFPT_FEATURES_REQUEST, version, 0);
  run(vconn_transact(pi->vconn, rq, &pi->reply), "talking to %s", vconn_get_name(pi->vconn));

  enum ofptype type;
  if (ofptype_decode(&type, (struct ofp_header *)pi->reply->data) ||
      type != OFPTYPE_FEATURES_REPLY) {
    // ovs_fatal(0, "%s: received bad features reply",
    //           vconn_get_name(pi->vconn));
    ACA_LOG_ERROR("%s: received bad features reply", vconn_get_name(pi->vconn));
  }
  if (!ofputil_switch_features_has_ports(pi->reply)) {
    /* The switch features reply does not contain a complete list of ports.
         * Probably, there are more ports than will fit into a single 64 kB
         * OpenFlow message.  Use OFPST_PORT_DESC to get a complete list of
         * ports. */
    ofpbuf_delete(pi->reply);
    pi->reply = NULL;
    port_iterator_fetch_port_desc(pi);
    return;
  }

  struct ofputil_switch_features features;
  enum ofperr error = ofputil_pull_switch_features(pi->reply, &features);
  if (error) {
    // ovs_fatal(0, "%s: failed to decode features reply (%s)",
    //           vconn_get_name(pi->vconn), ofperr_to_string(error));
    ACA_LOG_ERROR("%s: failed to decode features reply (%s)",
                  vconn_get_name(pi->vconn), ofperr_to_string(error));
  }
}

/* Initializes 'pi' to prepare for iterating through all of the ports on the
 * OpenFlow switch to which 'vconn' is connected.
 *
 * During iteration, the client should not make other use of 'vconn', because
 * that can cause other messages to be interleaved with the replies used by the
 * iterator and thus some ports may be missed or a hang can occur. */
void OVS_Control::port_iterator_init(port_iterator *pi, vconn *vconn)
{
  memset(pi, 0, sizeof *pi);
  pi->vconn = vconn;
  if (vconn_get_version(vconn) < OFP13_VERSION) {
    port_iterator_fetch_features(pi);
  } else {
    port_iterator_fetch_port_desc(pi);
  }
}

/* Obtains the next port from 'pi'.  On success, initializes '*pp' with the
 * port's details and returns true, otherwise (if all the ports have already
 * been seen), returns false.  */
bool OVS_Control::port_iterator_next(port_iterator *pi, ofputil_phy_port *pp)
{
  for (;;) {
    if (pi->reply) {
      int retval = ofputil_pull_phy_port(
              static_cast<ofp_version>(vconn_get_version(pi->vconn)), pi->reply, pp);
      if (!retval) {
        return true;
      } else if (retval != EOF) {
        // ovs_fatal(0, "received bad reply: %s",
        //           ofp_to_string(pi->reply->data, pi->reply->size,
        //                         NULL, NULL, verbosity + 1));
        ACA_LOG_ERROR("received bad reply: %s",
                      ofp_to_string(pi->reply->data, pi->reply->size, NULL,
                                    NULL, verbosity + 1));
      }
    }

    if (pi->variant == PI_FEATURES || !pi->more) {
      return false;
    }

    ovs_be32 recv_xid;
    do {
      ofpbuf_delete(pi->reply);
      run(vconn_recv_block(pi->vconn, &pi->reply), "OpenFlow receive failed");
      recv_xid = ((struct ofp_header *)pi->reply->data)->xid;
    } while (pi->send_xid != recv_xid);

    struct ofp_header *oh = (ofp_header *)pi->reply->data;
    enum ofptype type;
    if (ofptype_pull(&type, pi->reply) || type != OFPTYPE_PORT_DESC_STATS_REPLY) {
      // ovs_fatal(0, "received bad reply: %s",
      //           ofp_to_string(pi->reply->data, pi->reply->size, NULL,
      //                         NULL, verbosity + 1));
      ACA_LOG_ERROR("received bad reply: %s",
                    ofp_to_string(pi->reply->data, pi->reply->size, NULL, NULL,
                                  verbosity + 1));
    }

    pi->more = (ofpmp_flags(oh) & OFPSF_REPLY_MORE) != 0;
  }
}

/* Destroys iterator 'pi'. */
void OVS_Control::port_iterator_destroy(port_iterator *pi)
{
  if (pi) {
    while (pi->variant == PI_PORT_DESC && pi->more) {
      /* Drain vconn's queue of any other replies for this request. */
      struct ofputil_phy_port pp;
      port_iterator_next(pi, &pp);
    }

    ofpbuf_delete(pi->reply);
  }
}

/* Opens a connection to 'vconn_name', fetches the port structure for
 * 'port_name' (which may be a port name or number), and copies it into
 * '*pp'. */
void OVS_Control::fetch_ofputil_phy_port(const char *vconn_name,
                                         const char *port_name, ofputil_phy_port *pp)
{
  struct vconn *vconn;
  ofp_port_t port_no;
  bool found = false;

  /* Try to interpret the argument as a port number. */
  if (!str_to_ofp(port_name, &port_no)) {
    port_no = OFPP_NONE;
  }

  /* OpenFlow 1.0, 1.1, and 1.2 put the list of ports in the
     * OFPT_FEATURES_REPLY message.  OpenFlow 1.3 and later versions put it
     * into the OFPST_PORT_DESC reply.  Try it the correct way. */
  open_vconn(vconn_name, &vconn);
  struct port_iterator pi;
  for (port_iterator_init(&pi, vconn); port_iterator_next(&pi, pp);) {
    if (port_no != OFPP_NONE ? port_no == pp->port_no : !strcmp(pp->name, port_name)) {
      found = true;
      break;
    }
  }
  port_iterator_destroy(&pi);
  vconn_close(vconn);

  if (!found) {
    // ovs_fatal(0, "%s: couldn't find port `%s'", vconn_name, port_name);
    ACA_LOG_ERROR("%s: couldn't find port `%s'", vconn_name, port_name);
  }
}

/* Initializes 'ti' to prepare for iterating through all of the tables on the
 * OpenFlow switch to which 'vconn' is connected.
 *
 * During iteration, the client should not make other use of 'vconn', because
 * that can cause other messages to be interleaved with the replies used by the
 * iterator and thus some tables may be missed or a hang can occur. */
void OVS_Control::table_iterator_init(table_iterator *ti, vconn *vconn)
{
  memset(ti, 0, sizeof *ti);
  ti->vconn = vconn;
  ti->variant = (vconn_get_version(vconn) < OFP13_VERSION ? TI_STATS : TI_FEATURES);
  ti->more = true;

  enum ofpraw ofpraw = (ti->variant == TI_STATS ? OFPRAW_OFPST_TABLE_REQUEST :
                                                  OFPRAW_OFPST13_TABLE_FEATURES_REQUEST);
  struct ofpbuf *rq = ofpraw_alloc(ofpraw, vconn_get_version(vconn), 0);
  ti->send_xid = ((struct ofp_header *)rq->data)->xid;
  send_openflow_buffer(ti->vconn, rq);
}

/* Obtains the next table from 'ti'.  On success, returns the next table's
 * features; on failure, returns NULL.  */
const ofputil_table_features *OVS_Control::table_iterator_next(table_iterator *ti)
{
  for (;;) {
    if (ti->reply) {
      int retval;
      if (ti->variant == TI_STATS) {
        struct ofputil_table_stats ts;
        retval = ofputil_decode_table_stats_reply(ti->reply, &ts, &ti->features);
      } else {
        ovs_assert(ti->variant == TI_FEATURES);
        retval = ofputil_decode_table_features(ti->reply, &ti->features, &ti->raw_properties);
      }
      if (!retval) {
        return &ti->features;
      } else if (retval != EOF) {
        // ovs_fatal(0, "received bad reply: %s",
        //           ofp_to_string(ti->reply->data, ti->reply->size,
        //                         NULL, NULL, verbosity + 1));
        ACA_LOG_ERROR("received bad reply: %s",
                      ofp_to_string(ti->reply->data, ti->reply->size, NULL,
                                    NULL, verbosity + 1));
      }
    }

    if (!ti->more) {
      return NULL;
    }

    ovs_be32 recv_xid;
    do {
      ofpbuf_delete(ti->reply);
      run(vconn_recv_block(ti->vconn, &ti->reply), "OpenFlow receive failed");
      recv_xid = ((struct ofp_header *)ti->reply->data)->xid;
    } while (ti->send_xid != recv_xid);

    struct ofp_header *oh = (ofp_header *)ti->reply->data;
    enum ofptype type;
    if (ofptype_pull(&type, ti->reply) ||
        type != (ti->variant == TI_STATS ? OFPTYPE_TABLE_STATS_REPLY :
                                           OFPTYPE_TABLE_FEATURES_STATS_REPLY)) {
      // ovs_fatal(0, "received bad reply: %s",
      //           ofp_to_string(ti->reply->data, ti->reply->size, NULL,
      //                         NULL, verbosity + 1));
      ACA_LOG_ERROR("received bad reply: %s",
                    ofp_to_string(ti->reply->data, ti->reply->size, NULL, NULL,
                                  verbosity + 1));
    }

    ti->more = (ofpmp_flags(oh) & OFPSF_REPLY_MORE) != 0;
  }
}

/* Destroys iterator 'ti'. */
void OVS_Control::table_iterator_destroy(table_iterator *ti)
{
  if (ti) {
    while (ti->more) {
      /* Drain vconn's queue of any other replies for this request. */
      table_iterator_next(ti);
    }

    ofpbuf_delete(ti->reply);
  }
}

const ofputil_port_map *OVS_Control::get_port_map(const char *vconn_name)
{
  static shash port_maps = SHASH_INITIALIZER(&port_maps);
  struct ofputil_port_map *map = (ofputil_port_map *)shash_find_data(&port_maps, vconn_name);
  if (!map) {
    map = (ofputil_port_map *)malloc(sizeof *map);
    ofputil_port_map_init(map);
    shash_add(&port_maps, vconn_name, map);
    if (!strchr(vconn_name, ':') || !vconn_verify_name(vconn_name)) {
      /* For an active vconn (which includes a vconn constructed from a
             * bridge name), connect to it and pull down the port name-number
             * mapping. */
      struct vconn *vconn;
      open_vconn(vconn_name, &vconn);

      struct port_iterator pi;
      struct ofputil_phy_port pp;
      for (port_iterator_init(&pi, vconn); port_iterator_next(&pi, &pp);) {
        ofputil_port_map_put(map, pp.port_no, pp.name);
      }
      port_iterator_destroy(&pi);

      vconn_close(vconn);
    } else {
      /* Don't bother with passive vconns, since it could take a long
             * time for the remote to try to connect to us.  Don't bother with
             * invalid vconn names either. */
    }
  }
  return map;
}

const ofputil_port_map *OVS_Control::ports_to_accept(const char *vconn_name)
{
  return should_accept_names() ? get_port_map(vconn_name) : NULL;
}

const ofputil_port_map *OVS_Control::ports_to_show(const char *vconn_name)
{
  return should_show_names() ? get_port_map(vconn_name) : NULL;
}

const ofputil_table_map *OVS_Control::get_table_map(const char *vconn_name)
{
  static shash table_maps = SHASH_INITIALIZER(&table_maps);
  ofputil_table_map *map = (ofputil_table_map *)shash_find_data(&table_maps, vconn_name);
  if (!map) {
    map = (ofputil_table_map *)malloc(sizeof *map);
    ofputil_table_map_init(map);
    shash_add(&table_maps, vconn_name, map);

    if (!strchr(vconn_name, ':') || !vconn_verify_name(vconn_name)) {
      /* For an active vconn (which includes a vconn constructed from a
             * bridge name), connect to it and pull down the port name-number
             * mapping. */
      struct vconn *vconn;
      open_vconn(vconn_name, &vconn);

      struct table_iterator ti;
      table_iterator_init(&ti, vconn);
      for (;;) {
        const struct ofputil_table_features *tf = table_iterator_next(&ti);
        if (!tf) {
          break;
        }
        if (tf->name[0]) {
          ofputil_table_map_put(map, tf->table_id, tf->name);
        }
      }
      table_iterator_destroy(&ti);

      vconn_close(vconn);
    } else {
      /* Don't bother with passive vconns, since it could take a long
             * time for the remote to try to connect to us.  Don't bother with
             * invalid vconn names either. */
    }
  }
  return map;
}

const ofputil_table_map *OVS_Control::tables_to_accept(const char *vconn_name)
{
  return should_accept_names() ? get_table_map(vconn_name) : NULL;
}

const ofputil_table_map *OVS_Control::tables_to_show(const char *vconn_name)
{
  return should_show_names() ? get_table_map(vconn_name) : NULL;
}

/* We accept port and table names unless the feature is turned off explicitly. */
bool OVS_Control::should_accept_names(void)
{
  return use_names != 0;
}

/* We show port and table names only if the feature is turned on explicitly, or
 * if we're interacting with a user on the console. */
bool OVS_Control::should_show_names(void)
{
  static int interactive = -1;
  if (interactive == -1) {
    interactive = isatty(STDOUT_FILENO);
  }

  return use_names > 0 || (use_names == -1 && interactive);
}

} // namespace ovs_control
