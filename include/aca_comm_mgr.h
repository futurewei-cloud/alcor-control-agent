#ifndef ACA_COMM_MGR_H
#define ACA_COMM_MGR_H

// #include "cppkafka/consumer.h"
// #include "cppkafka/configuration.h"

using std::string;

namespace aca_comm_manager{ 

class Aca_Comm_Manager {

public: 

    // constructor and destructor purposely omitted to use the default one
    // provided by the compiler
    // Aca_Comm_Manager(); 
    // ~Aca_Comm_Manager();

    int process_messages();

    int deserialize(string binary_message, void *parsed_struct);

    int program_transitD(void *parsed_struct);

};

} // Aca_Comm_Manager 

#endif
