#ifndef ACA_COMM_MGR_H
#define ACA_COMM_MGR_H

#include "goalstate.pb.h"

using std::string;

namespace aca_comm_manager
{
  class Aca_Comm_Manager
  {

    public:
      // constructor and destructor purposely omitted to use the default one
      // provided by the compiler
      // Aca_Comm_Manager();
      // ~Aca_Comm_Manager();

      int process_messages();

      int deserialize(string kafka_message,
                      aliothcontroller::GoalState &parsed_struct);

      int update_goal_state(aliothcontroller::GoalState &parsed_struct);

      int execute_command(int command, void *input_struct);

      void print_goal_state(aliothcontroller::GoalState parsed_struct);
  };
} // namespace aca_comm_manager
#endif
