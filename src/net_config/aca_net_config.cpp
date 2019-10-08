#include "aca_log.h"
#include "aca_net_config.h"
#include <errno.h>
#include <stdlib.h>
#include <string>
#include <chrono>

using namespace std;

static char DEFAULT_MTU[] = "9000";

extern bool g_demo_mode;

namespace aca_net_config
{
Aca_Net_Config &Aca_Net_Config::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static Aca_Net_Config instance;
  return instance;
}

int Aca_Net_Config::create_namespace(string ns_name)
{
  int rc;

  if (ns_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty namespace, rc: %d\n", rc);
    return rc;
  }

  string cmd_string = IP_NETNS_PREFIX + "add " + ns_name;

  return execute_system_command(cmd_string);
}

// caller needs to ensure the device name is 15 characters or less
// due to linux limit
int Aca_Net_Config::create_veth_pair(string veth_name, string peer_name)
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

  return execute_system_command(cmd_string);
}

int Aca_Net_Config::setup_peer_device(string peer_name)
{
  int rc;

  if (peer_name.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty peer_name, rc: %d\n", rc);
    return rc;
  }

  string cmd_string = "ip link set dev " + peer_name + " up mtu " + DEFAULT_MTU;

  return execute_system_command(cmd_string);
}

int Aca_Net_Config::move_to_namespace(string veth_name, string ns_name)
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

  return execute_system_command(cmd_string);
}

int Aca_Net_Config::setup_veth_device(string ns_name, veth_config new_veth_config)
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
  command_rc = execute_system_command(cmd_string);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip link set dev " +
               new_veth_config.veth_name + " up";
  command_rc = execute_system_command(cmd_string);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " route add default gw " +
               new_veth_config.gateway_ip;
  command_rc = execute_system_command(cmd_string);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ifconfig " +
               new_veth_config.veth_name + " hw ether " + new_veth_config.mac;
  command_rc = execute_system_command(cmd_string);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  if (g_demo_mode) {
    cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " sysctl -w net.ipv4.tcp_mtu_probing=2";
    command_rc = execute_system_command(cmd_string);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;

    cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ethtool -K " +
                 new_veth_config.veth_name + " tso off gso off ufo off";
    command_rc = execute_system_command(cmd_string);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;

    cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ethtool --offload " +
                 new_veth_config.veth_name + " rx off tx off";
    command_rc = execute_system_command(cmd_string);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;

    cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ifconfig lo up";
    command_rc = execute_system_command(cmd_string);
    if (command_rc != EXIT_SUCCESS)
      overall_rc = command_rc;
  }

  return overall_rc;
}

// this functions bring the linux device down for the rename,
// and then bring it back up
int Aca_Net_Config::rename_veth_device(string ns_name, string org_veth_name, string new_veth_name)
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
  command_rc = execute_system_command(cmd_string);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip link set " +
               org_veth_name + " name " + new_veth_name;
  command_rc = execute_system_command(cmd_string);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  // bring the device back up
  cmd_string = IP_NETNS_PREFIX + "exec " + ns_name + " ip link set dev " +
               new_veth_name + " up";
  command_rc = execute_system_command(cmd_string);
  if (command_rc != EXIT_SUCCESS)
    overall_rc = command_rc;

  return overall_rc;
}

// workaround to add the gateway information based on the current CNI contract
// to be removed with the final contract/design
int Aca_Net_Config::add_gw(string ns_name, string gateway_ip)
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
  rc = execute_system_command(cmd_string);

  return rc;
}

int Aca_Net_Config::execute_system_command(string cmd_string)
{
  int rc;

  if (cmd_string.empty()) {
    rc = -EINVAL;
    ACA_LOG_ERROR("Invalid argument: Empty cmd_string, rc: %d\n", rc);
    return rc;
  }

  auto start = chrono::steady_clock::now();

  rc = system(cmd_string.c_str());

  auto end = chrono::steady_clock::now();

  ACA_LOG_DEBUG("Elapsed time for system command took: %ld nanoseconds or %ld milliseconds.\n",
                chrono::duration_cast<chrono::nanoseconds>(end - start).count(),
                chrono::duration_cast<chrono::milliseconds>(end - start).count());

  if (rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("Command succeeded: %s\n", cmd_string.c_str());
  } else {
    ACA_LOG_DEBUG("Command failed!!!: %s\n", cmd_string.c_str());
  }

  return rc;
}

} // namespace aca_net_config