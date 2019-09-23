#ifndef ACA_NET_CONFIG_H
#define ACA_NET_CONFIG_H

#include <string>

using std::string;

namespace aca_net_config
{
class Aca_Net_Config {
  public:
  // constructor and destructor purposely omitted to use the default one
  // provided by the compiler
  // Aca_Net_Config();
  // ~Aca_Net_Config();

  static Aca_Net_Config &get_instance();

  int create_namespace(string ns_name);

  int create_veth_pair(string veth_name, string peer_name);

  int setup_peer_device(string peer_name);

  int move_to_namespace(string veth_name, string ns_name);

  int setup_veth_device(string ns_name, string veth_name, string ip,
                        string prefixlen, string mac, string gw_ip);

  int rename_veth_device(string ns_name, string org_veth_name, string new_veth_name);

  int execute_system_command(string cmd_string);

  // compiler will flag the error when below is called.
  Aca_Net_Config(Aca_Net_Config const &) = delete;
  void operator=(Aca_Net_Config const &) = delete;

  private:
  Aca_Net_Config(){};
  ~Aca_Net_Config(){};
};

} // namespace aca_net_config
#endif