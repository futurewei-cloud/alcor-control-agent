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

#include "aca_log.h"
#include "aca_net_config.h"
#include <errno.h>
#include <stdlib.h>
#include <string>
#include <chrono>
#include <atomic>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>

using namespace std;

static char DEFAULT_MTU[] = "9000";

// Default execution timeout and output buffer size
// should the output be bigger than 512 it is truncated
// to 512 - 1 with 0 added, the rest will be read in the
// next loop
#define ACA_SYSTEM_TIMEOUT 10
#define ACA_BUFSIZ 512
static int debug;

extern std::atomic_ulong g_total_network_configuration_time;
extern bool g_demo_mode;

namespace aca_net_config
{
  // Execution routines static in this namespace
  static int aca_system(const char *);

Aca_Net_Config &Aca_Net_Config::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static Aca_Net_Config instance;
  return instance;
}

int Aca_Net_Config::create_namespace(string ns_name, ulong &culminative_time)
{
  int rc;

  if (ns_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty namespace, rc: %d\n", rc);
    return rc;
  }

  string cmd_string = IP_NETNS_PREFIX + "add " + ns_name;

  return execute_system_command(cmd_string, culminative_time);
}

// caller needs to ensure the device name is 15 characters or less
// due to linux limit
int Aca_Net_Config::create_veth_pair(string veth_name, string peer_name, ulong &culminative_time)
{
  int rc;

  if (veth_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty veth_name, rc: %d\n", rc);
    return rc;
  }

  if (peer_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty peer_name, rc: %d\n", rc);
    return rc;
  }

  string cmd_string = "ip link add " + veth_name + " type veth peer name " + peer_name;

  return execute_system_command(cmd_string, culminative_time);
}

int Aca_Net_Config::delete_veth_pair(string peer_name, ulong &culminative_time)
{
  int rc;

  if (peer_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty peer_name, rc: %d\n", rc);
    return rc;
  }

  string cmd_string = "ip link delete " + peer_name;

  return execute_system_command(cmd_string, culminative_time);
}

int Aca_Net_Config::setup_peer_device(string peer_name, ulong &culminative_time)
{
  int rc;

  if (peer_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty peer_name, rc: %d\n", rc);
    return rc;
  }

  string cmd_string = "ip link set dev " + peer_name + " up mtu " + DEFAULT_MTU;

  return execute_system_command(cmd_string, culminative_time);
}

int Aca_Net_Config::move_to_namespace(string veth_name, string ns_name, ulong &culminative_time)
{
  int rc;

  if (veth_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty veth_name, rc: %d\n", rc);
    return rc;
  }

  if (ns_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty ns_name, rc: %d\n", rc);
    return rc;
  }

  string cmd_string = "ip link set " + veth_name + " netns " + ns_name;

  return execute_system_command(cmd_string, culminative_time);
}

int Aca_Net_Config::setup_veth_device(string ns_name, veth_config new_veth_config,
                                      ulong &culminative_time)
{
  int overall_rc = EXIT_SUCCESS;
  int command_rc;
  string cmd_string;

  if (ns_name.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty ns_name, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (new_veth_config.veth_name.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty new_veth_config.veth_name, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (new_veth_config.ip.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty new_veth_config.ip, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (new_veth_config.prefix_len.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty new_veth_config.prefix_len, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (new_veth_config.mac.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty new_veth_config.mac, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (new_veth_config.gateway_ip.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty new_veth_config.gateway_ip, rc: %d\n", overall_rc);
    return overall_rc;
  }

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip addr add " +
               new_veth_config.ip + "/" + new_veth_config.prefix_len + " dev " +
               new_veth_config.veth_name;
  command_rc = execute_system_command(cmd_string, culminative_time);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip link set dev " +
               new_veth_config.veth_name + " up";
  command_rc = execute_system_command(cmd_string, culminative_time);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " route add default gw " +
               new_veth_config.gateway_ip;
  command_rc = execute_system_command(cmd_string, culminative_time);
  // it is okay if the gateway is already setup
  //   if (command_rc != EXIT_SUCCESS)
  //     overall_rc = command_rc;

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ifconfig " +
               new_veth_config.veth_name + " hw ether " + new_veth_config.mac;
  command_rc = execute_system_command(cmd_string, culminative_time);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  if (g_demo_mode) {
    cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " sysctl -w net.ipv4.tcp_mtu_probing=2";
    command_rc = execute_system_command(cmd_string, culminative_time);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;

    cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ethtool -K " +
                 new_veth_config.veth_name + " tso off gso off ufo off";
    command_rc = execute_system_command(cmd_string, culminative_time);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;

    cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ethtool --offload " +
                 new_veth_config.veth_name + " rx off tx off";
    command_rc = execute_system_command(cmd_string, culminative_time);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;

    cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ifconfig lo up";
    command_rc = execute_system_command(cmd_string, culminative_time);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;
  }

  return overall_rc;
}

// this functions bring the linux device down for the rename,
// and then bring it back up
int Aca_Net_Config::rename_veth_device(string ns_name, string org_veth_name,
                                       string new_veth_name, ulong &culminative_time)
{
  int overall_rc = EXIT_SUCCESS;
  int command_rc;
  string cmd_string;

  if (ns_name.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty ns_name, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (org_veth_name.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty org_veth_name, rc: %d\n", overall_rc);
    return overall_rc;
  }

  if (new_veth_name.empty()) {
    overall_rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty new_veth_name, rc: %d\n", overall_rc);
    return overall_rc;
  }

  // bring the link down
  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip link set dev " +
               org_veth_name + " down";
  command_rc = execute_system_command(cmd_string, culminative_time);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip link set " +
               org_veth_name + " name " + new_veth_name;
  command_rc = execute_system_command(cmd_string, culminative_time);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  // bring the device back up
  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip link set dev " +
               new_veth_name + " up";
  command_rc = execute_system_command(cmd_string, culminative_time);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  return overall_rc;
}

// workaround to add the gateway information based on the current CNI contract
// to be removed with the final contract/design
int Aca_Net_Config::add_gw(string ns_name, string gateway_ip, ulong &culminative_time)
{
  int rc;
  string cmd_string;

  if (ns_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty ns_name, rc: %d\n", rc);
    return rc;
  }

  if (gateway_ip.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty gateway_ip, rc: %d\n", rc);
    return rc;
  }

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " route add default gw " + gateway_ip;
  rc = execute_system_command(cmd_string, culminative_time);

  return rc;
}

int Aca_Net_Config::execute_system_command(string cmd_string)
{
  ulong not_care;

  return execute_system_command(cmd_string, not_care);
}

int Aca_Net_Config::execute_system_command(string cmd_string, ulong &culminative_time)
{
  int rc;

  if (cmd_string.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty cmd_string, rc: %d\n", rc);
    return rc;
  }

  auto network_configuration_time_start = chrono::steady_clock::now();

  if (0)
    rc = system(cmd_string.c_str());
  if (1)
    rc = aca_system(cmd_string.c_str());

  auto network_configuration_time_end = chrono::steady_clock::now();

  auto network_configuration_elapse_time =
          chrono::duration_cast<chrono::nanoseconds>(
                  network_configuration_time_end - network_configuration_time_start)
                  .count();

  culminative_time += network_configuration_elapse_time;

  g_total_network_configuration_time += network_configuration_elapse_time;

  ACA_LOG_DEBUG("Elapsed time for system command took: %ld nanoseconds or %ld milliseconds.\n",
                network_configuration_elapse_time,
                network_configuration_elapse_time / 1000000);

  if (rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("Command succeeded: %s\n", cmd_string.c_str());
  } else {
    ACA_LOG_DEBUG("Command failed!!!: %s\n", cmd_string.c_str());
  }

  return rc;
}

static int aca_system(const char *cmd)
{
  int s[2];
  FILE *fp;
  char *p;
  char  buf[ACA_BUFSIZ];
  int timeout;

  debug = 0;
  if ((getenv("ACA_SYSTEM_DEBUG")))
      debug = 1;

  timeout = ACA_SYSTEM_TIMEOUT;
  if ((p = getenv("ACA_SYSTEM_TIMEOUT"))) {
    timeout = atoi(p);
    if (timeout <= 0)
      timeout = ACA_SYSTEM_TIMEOUT;
  }

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, s) < 0) {
    perror("socketpair()");
    return -1;
  }

  pid_t pid;
  pid = fork();
  if (pid < 0) {
    perror("fork()");
    close(s[0]);
    close(s[1]);
    return -1;
  }

  if (pid == 0) {
    if (debug)
      fprintf(stderr, "\
%s: child %d to exec() cmd %s\n", __func__, getpid(), cmd);
    /* child run command
     */
    close(s[0]);
    dup2(s[1], 1);
    execl("/bin/sh", "sh", "-c", cmd, NULL);
    exit(-1);
  }

  fd_set rmask;
  close(s[1]);
  FD_ZERO(&rmask);
  FD_SET(s[0], &rmask);

  if (timeout < 0)
    timeout = 10;

  struct timeval tv;
  tv.tv_sec = timeout;
  tv.tv_usec = 0;

  if (debug)
    fprintf(stderr, "\
%s: parent %d to select() timeout %d\n", __func__, getpid(), timeout);

  int cc;
  cc = select(s[0] + 1, &rmask, NULL, NULL, &tv);
  if (cc < 0) {
    perror("select()");
    close(s[0]);
    kill(pid, SIGTERM);
    return -1;
  }

  if (cc == 0) {
    if (debug)
      fprintf(stderr, "%s: select() timed out kill %d\n", __func__, pid);
    kill(pid, SIGTERM);
    close(s[0]);
    errno = ETIME;
    return -1;
  }

  fp = fdopen(s[0], "r");
  /* read and reap
   */
  while ((fgets(buf, sizeof(buf), fp)))
    printf("%s", buf);

  fclose(fp);
  int wstatus;
  if (waitpid(pid, &wstatus, 0) < 0) {
    return -1;
  }

  return wstatus;
}

} // namespace aca_net_config
