#ifndef ACA_COMM_MGR_H
#define ACA_COMM_MGR_H

#include "cppkafka/buffer.h"
#include "goalstateprovisioner.grpc.pb.h"

using std::string;

namespace aca_comm_manager
{
class Aca_Comm_Manager {
  public:
  // constructor and destructor purposely omitted to use the default one
  // provided by the compiler
  // Aca_Comm_Manager();
  // ~Aca_Comm_Manager();

  static Aca_Comm_Manager &get_instance();

  int deserialize(const cppkafka::Buffer *kafka_buffer,
                  alcorcontroller::GoalState &parsed_struct);

  int update_vpc_states(const alcorcontroller::GoalState &parsed_struct,
                        alcorcontroller::GoalStateOperationReply &gsOperationReply);

  int update_subnet_states(const alcorcontroller::GoalState &parsed_struct,
                           alcorcontroller::GoalStateOperationReply &gsOperationReply);

  int update_port_state_workitem(const alcorcontroller::PortState &current_PortState,
                                 const alcorcontroller::GoalState &parsed_struct,
                                 alcorcontroller::GoalStateOperationReply &gsOperationReply);

  int update_port_states(const alcorcontroller::GoalState &parsed_struct,
                         alcorcontroller::GoalStateOperationReply &gsOperationReply);

  int update_goal_state(const alcorcontroller::GoalState &parsed_struct,
                        alcorcontroller::GoalStateOperationReply &gsOperationReply);

  int execute_command(int command, void *input_struct, ulong &culminative_time);

  void print_goal_state(alcorcontroller::GoalState parsed_struct);

  // compiler will flag the error when below is called.
  Aca_Comm_Manager(Aca_Comm_Manager const &) = delete;
  void operator=(Aca_Comm_Manager const &) = delete;

  private:
  Aca_Comm_Manager(){};
  ~Aca_Comm_Manager(){};

  int load_agent_xdp(string interface, ulong &culminative_time);
};
} // namespace aca_comm_manager
#endif
