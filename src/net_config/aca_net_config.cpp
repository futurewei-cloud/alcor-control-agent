#include "aca_log.h"
#include "aca_net_config.h"
#include <errno.h>
#include <stdlib.h>

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
		ACA_LOG_ERROR("Invalid argument: Empty namespace, rc: %d\n",
			      rc);
		return rc;
	}

	string cmd_string = "ip netns add " + ns_name;

	return execute_system_command(cmd_string);
}

// caller needs to ensure the device name is 15 characters or less
// due to linux limit
int Aca_Net_Config::create_veth_pair(string veth_name, string peer_name)
{
	int rc;

	if (veth_name.empty()) {
		rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty veth_name, rc: %d\n",
			      rc);
		return rc;
	}

	if (peer_name.empty()) {
		rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty peer_name, rc: %d\n",
			      rc);
		return rc;
	}

	string cmd_string = "ip link add " + veth_name +
			    " type veth peer name " + peer_name;

	return execute_system_command(cmd_string);
}

int Aca_Net_Config::setup_peer_device(string peer_name)
{
	int rc;

	if (peer_name.empty()) {
		rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty peer_name, rc: %d\n",
			      rc);
		return rc;
	}

	string cmd_string =
		"ip link set dev " + peer_name + " up mtu " + DEFAULT_MTU;

	return execute_system_command(cmd_string);
}

int Aca_Net_Config::move_to_namespace(string veth_name, string ns_name)
{
	int rc;

	if (veth_name.empty()) {
		rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty veth_name, rc: %d\n",
			      rc);
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

int Aca_Net_Config::setup_veth_device(string ns_name, string veth_name,
				      string ip, string prefixlen, string mac,
				      string gw_ip)
{
	int overall_rc = EXIT_SUCCESS;
	int command_rc;
	string cmd_string;

	if (ns_name.empty()) {
		overall_rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty ns_name, rc: %d\n",
			      overall_rc);
		return overall_rc;
	}

	if (veth_name.empty()) {
		overall_rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty veth_name, rc: %d\n",
			      overall_rc);
		return overall_rc;
	}

	if (ip.empty()) {
		overall_rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty ip, rc: %d\n",
			      overall_rc);
		return overall_rc;
	}

	if (prefixlen.empty()) {
		overall_rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty prefixlen, rc: %d\n",
			      overall_rc);
		return overall_rc;
	}

	if (mac.empty()) {
		overall_rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty mac, rc: %d\n",
			      overall_rc);
		return overall_rc;
	}

	if (gw_ip.empty()) {
		overall_rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty gw_ip, rc: %d\n",
			      overall_rc);
		return overall_rc;
	}

	cmd_string = "ip netns exec " + ns_name + " ip addr add " + ip + "/" +
		     prefixlen + " dev " + veth_name;
	command_rc = execute_system_command(cmd_string);
	if (command_rc != EXIT_SUCCESS)
		overall_rc = command_rc;

	cmd_string = "ip netns exec " + ns_name + " ip link set dev " +
		     veth_name + " up";
	command_rc = execute_system_command(cmd_string);
	if (command_rc != EXIT_SUCCESS)
		overall_rc = command_rc;

	cmd_string =
		"ip netns exec " + ns_name + " route add default gw " + gw_ip;
	command_rc = execute_system_command(cmd_string);
	if (command_rc != EXIT_SUCCESS)
		overall_rc = command_rc;

	cmd_string = "ip netns exec " + ns_name + " ifconfig " + veth_name +
		     " hw ether " + mac;
	command_rc = execute_system_command(cmd_string);
	if (command_rc != EXIT_SUCCESS)
		overall_rc = command_rc;

	if (g_demo_mode) {
		cmd_string = "ip netns exec " + ns_name +
			     " sysctl -w net.ipv4.tcp_mtu_probing=2";
		command_rc = execute_system_command(cmd_string);
		if (command_rc != EXIT_SUCCESS)
			overall_rc = command_rc;

		cmd_string = "ip netns exec " + ns_name + " ethtool -K " +
			     veth_name + " tso off gso off ufo off";
		command_rc = execute_system_command(cmd_string);
		if (command_rc != EXIT_SUCCESS)
			overall_rc = command_rc;

		cmd_string = "ip netns exec " + ns_name +
			     " ethtool --offload " + veth_name +
			     " rx off tx off";
		command_rc = execute_system_command(cmd_string);
		if (command_rc != EXIT_SUCCESS)
			overall_rc = command_rc;

		cmd_string = "ip netns exec " + ns_name + " ifconfig lo up";
		command_rc = execute_system_command(cmd_string);
		if (command_rc != EXIT_SUCCESS)
			overall_rc = command_rc;
	}

	return overall_rc;
}

// this functions bring the linux device down for the rename,
// and then bring it back up
int Aca_Net_Config::rename_veth_device(string ns_name, string org_veth_name,
				       string new_veth_name)
{
	int overall_rc = EXIT_SUCCESS;
	int command_rc;
	string cmd_string;

	if (ns_name.empty()) {
		overall_rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty ns_name, rc: %d\n",
			      overall_rc);
		return overall_rc;
	}

	if (org_veth_name.empty()) {
		overall_rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty org_veth_name, rc: %d\n",
			      overall_rc);
		return overall_rc;
	}

	if (new_veth_name.empty()) {
		overall_rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty new_veth_name, rc: %d\n",
			      overall_rc);
		return overall_rc;
	}

	// bring the link down
	cmd_string = "ip netns exec " + ns_name + " ip link set dev " +
		     org_veth_name + " down";
	command_rc = execute_system_command(cmd_string);
	if (command_rc != EXIT_SUCCESS)
		overall_rc = command_rc;

	cmd_string = "ip netns exec " + ns_name + " ip link set " +
		     org_veth_name + " name " + new_veth_name;
	command_rc = execute_system_command(cmd_string);
	if (command_rc != EXIT_SUCCESS)
		overall_rc = command_rc;

	// bring the device back up
	cmd_string = "ip netns exec " + ns_name + " ip link set dev " +
		     new_veth_name + " up";
	command_rc = execute_system_command(cmd_string);
	if (command_rc != EXIT_SUCCESS)
		overall_rc = command_rc;

	return overall_rc;
}

int Aca_Net_Config::execute_system_command(string cmd_string)
{
	int rc;

	if (cmd_string.empty()) {
		rc = -EINVAL;
		ACA_LOG_ERROR("Invalid argument: Empty cmd_string, rc: %d\n",
			      rc);
		return rc;
	}

	rc = system(cmd_string.c_str());
	if (rc == EXIT_SUCCESS) {
		ACA_LOG_INFO("Command succeeded: %s\n", cmd_string.c_str());
	} else {
		ACA_LOG_DEBUG("Command failed!!!: %s\n", cmd_string.c_str());
	}

	return rc;
}

} // namespace aca_net_config