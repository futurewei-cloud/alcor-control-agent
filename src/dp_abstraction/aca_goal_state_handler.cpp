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

#include "aca_log.h"
#include "aca_dataplane_ovs.h"
#include "aca_goal_state_handler.h"
#include "goalstateprovisioner.grpc.pb.h"
#include <future>

#include "marl/defer.h"
#include "marl/event.h"
#include "marl/scheduler.h"
#include "marl/waitgroup.h"

using namespace alcor::schema;

std::mutex gs_reply_mutex; // mutex for writing gs reply object
const int resource_state_processing_batch_size = 10000; // batch size of concurrently processing a kind of resource states.
namespace aca_goal_state_handler
{
Aca_Goal_State_Handler::Aca_Goal_State_Handler()
{
  ACA_LOG_INFO("%s", "Goal State Handler: initialize\n");

  // default to dataplane_ovs
  ACA_LOG_INFO("%s", "Network State Handler: using ovs dataplane\n");
  this->core_net_programming_if = new aca_dataplane_ovs::ACA_Dataplane_OVS;

  int rc = this->core_net_programming_if->initialize();

  if (rc == EXIT_SUCCESS) {
    ACA_LOG_INFO("%s", "Core Network Programming initialization succeed\n");
  } else {
    ACA_LOG_ERROR("%s", "Core Network Programming initialization failed\n");
    throw std::system_error(ENXIO, std::generic_category(),
                            "Core Network Programming initialization failed\n");
  }
}

Aca_Goal_State_Handler::~Aca_Goal_State_Handler()
{
  // allocated core_net_programming_if is destroyed when program exits.
}

Aca_Goal_State_Handler &Aca_Goal_State_Handler::get_instance()
{
  // It is instantiated on first use.
  // allocated instance is destroyed when program exits.
  static Aca_Goal_State_Handler instance;
  return instance;
}

// not being used now and the forseeable future
// int Aca_Goal_State_Handler::update_vpc_state_workitem(const VpcState current_VpcState,
//                                                       GoalStateOperationReply &gsOperationReply)
// {
//   return this->core_net_programming_if->update_vpc_state_workitem(
//           current_VpcState, std::ref(gsOperationReply));
// }

// int Aca_Goal_State_Handler::update_vpc_states(GoalState &parsed_struct,
//                                               GoalStateOperationReply &gsOperationReply)
// {
//   std::vector<std::future<int> > workitem_future;
//   int rc;
//   int overall_rc = EXIT_SUCCESS;

//   for (int i = 0; i < parsed_struct.vpc_states_size(); i++) {
//     ACA_LOG_DEBUG("=====>parsing vpc states #%d\n", i);

//     VpcState current_VPCState = parsed_struct.vpc_states(i);

//     workitem_future.push_back(std::async(
//             std::launch::async, &Aca_Goal_State_Handler::update_vpc_state_workitem,
//             this, current_VPCState, std::ref(gsOperationReply)));

//   } // for (int i = 0; i < parsed_struct.vpc_states_size(); i++)

//   for (int i = 0; i < parsed_struct.vpc_states_size(); i++) {
//     rc = workitem_future[i].get();
//     if (rc != EXIT_SUCCESS)
//       overall_rc = rc;
//   } // for (int i = 0; i < parsed_struct.vpc_states_size(); i++)

//   return overall_rc;
// }

// int Aca_Goal_State_Handler::update_subnet_state_workitem(const SubnetState current_SubnetState,
//                                                          GoalStateOperationReply &gsOperationReply)
// {
//   return this->core_net_programming_if->update_subnet_state_workitem(
//           current_SubnetState, std::ref(gsOperationReply));
// }

// int Aca_Goal_State_Handler::update_subnet_states(GoalState &parsed_struct,
//                                                  GoalStateOperationReply &gsOperationReply)
// {
//   std::vector<std::future<int> > workitem_future;
//   int rc;
//   int overall_rc = EXIT_SUCCESS;

//   for (int i = 0; i < parsed_struct.subnet_states_size(); i++) {
//     ACA_LOG_DEBUG("=====>parsing subnet states #%d\n", i);

//     SubnetState current_SubnetState = parsed_struct.subnet_states(i);

//     workitem_future.push_back(std::async(
//             std::launch::async, &Aca_Goal_State_Handler::update_subnet_state_workitem,
//             this, current_SubnetState, std::ref(gsOperationReply)));

//   } // for (int i = 0; i < parsed_struct.subnet_states_size(); i++)

//   for (int i = 0; i < parsed_struct.subnet_states_size(); i++) {
//     rc = workitem_future[i].get();
//     if (rc != EXIT_SUCCESS)
//       overall_rc = rc;
//   } // for (int i = 0; i < parsed_struct.subnet_states_size(); i++)

//   return overall_rc;
// }

int Aca_Goal_State_Handler::update_port_state_workitem(const PortState current_PortState,
                                                       GoalState &parsed_struct,
                                                       GoalStateOperationReply &gsOperationReply)
{
  return this->core_net_programming_if->update_port_state_workitem(
          current_PortState, std::ref(parsed_struct), std::ref(gsOperationReply));
}

int Aca_Goal_State_Handler::update_port_states(GoalState &parsed_struct,
                                               GoalStateOperationReply &gsOperationReply)
{
  std::vector<std::future<int> > workitem_future;
  int rc;
  int overall_rc = EXIT_SUCCESS;
  int count = 1;

  for (int i = 0; i < parsed_struct.port_states_size(); i++) {
    ACA_LOG_DEBUG("=====>parsing port states #%d\n", i);

    PortState current_PortState = parsed_struct.port_states(i);

    workitem_future.push_back(std::async(
            std::launch::async, &Aca_Goal_State_Handler::update_port_state_workitem, this,
            current_PortState, std::ref(parsed_struct), std::ref(gsOperationReply)));
    if (count % resource_state_processing_batch_size == 0){
      for (int i = 0; i < workitem_future.size(); i++) {
        rc = workitem_future[i].get();
        if (rc != EXIT_SUCCESS)
          overall_rc = rc;
      }
      workitem_future.clear();
      count = 1;
    } else {
      count ++;
    }
    // keeping below just in case if we want to call it serially
    // rc = update_port_state_workitem(current_PortState, parsed_struct, gsOperationReply);
    // if (rc != EXIT_SUCCESS)
    //   overall_rc = rc;
  } // for (int i = 0; i < parsed_struct.port_states_size(); i++)

  for (int i = 0; i < workitem_future.size(); i++) {
    rc = workitem_future[i].get();
    if (rc != EXIT_SUCCESS)
      overall_rc = rc;
  } // for (int i = 0; i < parsed_struct.port_states_size(); i++)

  return overall_rc;
}

// function overloading doesn't work when called by function pointer in update_port_states
// therefore, naming this function as "v2"
int Aca_Goal_State_Handler::update_port_state_workitem_v2(const PortState current_PortState,
                                                          GoalStateV2 &parsed_struct,
                                                          GoalStateOperationReply &gsOperationReply)
{
  return this->core_net_programming_if->update_port_state_workitem(
          current_PortState, std::ref(parsed_struct), std::ref(gsOperationReply));
}

int Aca_Goal_State_Handler::update_port_states(GoalStateV2 &parsed_struct,
                                               GoalStateOperationReply &gsOperationReply)
{
  std::vector<std::future<int> > workitem_future;
  int rc;
  int overall_rc = EXIT_SUCCESS;
  int count = 1;
  // below is a c++ 17 feature
  for (auto &[port_id, current_PortState] : parsed_struct.port_states()) {
    ACA_LOG_DEBUG("=====>parsing port state: %s\n", port_id.c_str());

    workitem_future.push_back(std::async(
            std::launch::async, &Aca_Goal_State_Handler::update_port_state_workitem_v2, this,
            current_PortState, std::ref(parsed_struct), std::ref(gsOperationReply)));
    if (count % resource_state_processing_batch_size == 0) {
      for (int i = 0; i < workitem_future.size(); i++) {
        rc = workitem_future[i].get();
        if (rc != EXIT_SUCCESS)
          overall_rc = rc;
      }
      workitem_future.clear();
      count = 1;
    } else {
      count ++;
    }
    // keeping below just in case if we want to call it serially
    // rc = update_port_state_workitem(current_PortState, parsed_struct, gsOperationReply);
    // if (rc != EXIT_SUCCESS)
    //   overall_rc = rc;
  } // for (int i = 0; i < parsed_struct.port_states_size(); i++)

  for (int i = 0; i < workitem_future.size(); i++) {
    rc = workitem_future[i].get();
    if (rc != EXIT_SUCCESS)
      overall_rc = rc;
  } // for (int i = 0; i < parsed_struct.port_states_size(); i++)

  return overall_rc;
}

int Aca_Goal_State_Handler::update_neighbor_state_workitem(const NeighborState current_NeighborState,
                                                           GoalState &parsed_struct,
                                                           GoalStateOperationReply &gsOperationReply)
{
  return this->core_net_programming_if->update_neighbor_state_workitem(
          current_NeighborState, std::ref(parsed_struct), std::ref(gsOperationReply));
}

int Aca_Goal_State_Handler::update_neighbor_states(GoalState &parsed_struct,
                                                   GoalStateOperationReply &gsOperationReply)
{
  std::vector<std::future<int> > workitem_future;
  int rc;
  int overall_rc = EXIT_SUCCESS;
  int count = 1;

  for (int i = 0; i < parsed_struct.neighbor_states_size(); i++) {
    ACA_LOG_DEBUG("=====>parsing neighbor states #%d\n", i);

    NeighborState current_NeighborState = parsed_struct.neighbor_states(i);

    workitem_future.push_back(std::async(
            std::launch::async, &Aca_Goal_State_Handler::update_neighbor_state_workitem,
            this, current_NeighborState, std::ref(parsed_struct),
            std::ref(gsOperationReply)));
    if (count % resource_state_processing_batch_size == 0) {
      for (int i = 0; i < workitem_future.size(); i++) {
        rc = workitem_future[i].get();
        if (rc != EXIT_SUCCESS)
          overall_rc = rc;
      }
      workitem_future.clear();
      count = 1;
    } else {
      count ++;
    }
  }

  for (int i = 0; i < workitem_future.size(); i++) {
    rc = workitem_future[i].get();
    if (rc != EXIT_SUCCESS)
      overall_rc = rc;
  }

  return overall_rc;
}

int Aca_Goal_State_Handler::update_router_state_workitem(const RouterState current_RouterState,
                                                         GoalState &parsed_struct,
                                                         GoalStateOperationReply &gsOperationReply)
{
  return this->core_net_programming_if->update_router_state_workitem(
          current_RouterState, std::ref(parsed_struct), std::ref(gsOperationReply));
}

int Aca_Goal_State_Handler::update_router_states(GoalState &parsed_struct,
                                                 GoalStateOperationReply &gsOperationReply)
{
  std::vector<std::future<int> > workitem_future;
  int rc;
  int overall_rc = EXIT_SUCCESS;
  int count = 1;

  for (int i = 0; i < parsed_struct.router_states_size(); i++) {
    ACA_LOG_DEBUG("=====>parsing router states #%d\n", i);

    RouterState current_RouterState = parsed_struct.router_states(i);

    workitem_future.push_back(std::async(
            std::launch::async, &Aca_Goal_State_Handler::update_router_state_workitem, this,
            current_RouterState, std::ref(parsed_struct), std::ref(gsOperationReply)));
    if (count % resource_state_processing_batch_size == 0) {
      for (int i = 0; i < workitem_future.size(); i++) {
        rc = workitem_future[i].get();
        if (rc != EXIT_SUCCESS)
          overall_rc = rc;
      }
      workitem_future.clear();
      count = 1;
    } else {
      count ++;
    }
  }

  for (int i = 0; i < workitem_future.size(); i++) {
    rc = workitem_future[i].get();
    if (rc != EXIT_SUCCESS)
      overall_rc = rc;
  }

  return overall_rc;
}

int Aca_Goal_State_Handler::update_neighbor_state_workitem_v2(
        const NeighborState current_NeighborState, GoalStateV2 &parsed_struct,
        GoalStateOperationReply &gsOperationReply)
{
  return this->core_net_programming_if->update_neighbor_state_workitem(
          current_NeighborState, std::ref(parsed_struct), std::ref(gsOperationReply));
}

int Aca_Goal_State_Handler::update_neighbor_states(GoalStateV2 &parsed_struct,
                                                   GoalStateOperationReply &gsOperationReply)
{
  std::vector<std::future<int> > workitem_future;
  int rc;
  int overall_rc = EXIT_SUCCESS;
  int count = 1;
  GoalStateV2* gsv2_ptr = &parsed_struct;
  GoalStateOperationReply* reply_ptr = &gsOperationReply;
  int neighbor_size = parsed_struct.neighbor_states().size();
  marl::WaitGroup neighbor_wait_group(neighbor_size);

  // std::cout<<"Set WaitGroup size to "<<neighbor_size<<std::endl;
  std::atomic<int> neighbor_count;
  // for (auto i = parsed_struct.neighbor_states().begin() ; i != parsed_struct.neighbor_states().end(); i ++){
  //   neighbor_count.fetch_add(1);
  //   neighbor_wait_group.add();
  //   marl::schedule([=] {
  //     defer(neighbor_wait_group.done());
  //     update_neighbor_state_workitem_v2(i->second, *gsv2_ptr, *reply_ptr);
  //   });
  // }
  
  for (auto &[neighbor_id, current_NeighborState] : parsed_struct.neighbor_states()) {
    //ACA_LOG_DEBUG("=====>parsing neighbor state: %s\n", neighbor_id.c_str());
    neighbor_count.fetch_add(1);
    // neighbor_wait_group.add();
    marl::schedule([=] {
      defer(neighbor_wait_group.done());
      update_neighbor_state_workitem_v2(current_NeighborState, *gsv2_ptr, *reply_ptr);
    });
  }
/*
    workitem_future.push_back(std::async(
            std::launch::async, &Aca_Goal_State_Handler::update_neighbor_state_workitem_v2,
            this, current_NeighborState, std::ref(parsed_struct),
            std::ref(gsOperationReply)));
    if (count % resource_state_processing_batch_size == 0) {
      for (int i = 0 ; i < workitem_future.size(); i++){
        rc = workitem_future[i].get();
        if (rc != EXIT_SUCCESS){
          overall_rc = rc;
        }
      }
      workitem_future.clear();
      count = 1;
    } else {
      count++;
    }
  */
  std::cout<<"Count "<<neighbor_count.load()<<" neighbors in this GSV2"<<std::endl;
/*
  for (int i = 0; i < workitem_future.size(); i++) {
    rc = workitem_future[i].get();
    if (rc != EXIT_SUCCESS)
      overall_rc = rc;
  }
*/
  neighbor_wait_group.wait();
  return overall_rc;
}

int Aca_Goal_State_Handler::update_router_state_workitem_v2(const RouterState current_RouterState,
                                                            GoalStateV2 &parsed_struct,
                                                            GoalStateOperationReply &gsOperationReply)
{
  return this->core_net_programming_if->update_router_state_workitem(
          current_RouterState, std::ref(parsed_struct), std::ref(gsOperationReply));
}

int Aca_Goal_State_Handler::update_router_states(GoalStateV2 &parsed_struct,
                                                 GoalStateOperationReply &gsOperationReply)
{
  std::vector<std::future<int> > workitem_future;
  int rc;
  int overall_rc = EXIT_SUCCESS;
  int count = 1;

  for (auto &[router_id, current_RouterState] : parsed_struct.router_states()) {
    ACA_LOG_DEBUG("=====>parsing router state: %s\n", router_id.c_str());

    workitem_future.push_back(std::async(
            std::launch::async, &Aca_Goal_State_Handler::update_router_state_workitem_v2, this,
            current_RouterState, std::ref(parsed_struct), std::ref(gsOperationReply)));
    if (count % resource_state_processing_batch_size == 0){
      for (int i = 0; i < workitem_future.size(); i++) {
        rc = workitem_future[i].get();
        if (rc != EXIT_SUCCESS)
          overall_rc = rc;
      }
      workitem_future.clear();
      count = 1;
    } else {
      count ++;
    }
  }

  for (int i = 0; i < workitem_future.size(); i++) {
    rc = workitem_future[i].get();
    if (rc != EXIT_SUCCESS)
      overall_rc = rc;
  }

  return overall_rc;
}

void Aca_Goal_State_Handler::add_goal_state_operation_status(
        GoalStateOperationReply &gsOperationReply, std::string id,
        ResourceType resource_type, OperationType operation_type,
        int operation_rc, ulong culminative_dataplane_programming_time,
        ulong culminative_network_configuration_time, ulong state_elapse_time)
{
  OperationStatus overall_operation_status;

  if (operation_rc == EXIT_SUCCESS)
    overall_operation_status = OperationStatus::SUCCESS;
  else if (operation_rc == EINPROGRESS && resource_type == PORT &&
           operation_type == OperationType::CREATE)
    overall_operation_status = OperationStatus::PENDING;
  else if (operation_rc == -EINVAL)
    overall_operation_status = OperationStatus::INVALID_ARG;
  else
    overall_operation_status = OperationStatus::FAILURE;

  ACA_LOG_DEBUG("gsOperationReply - resource_id: %s\n", id.c_str());
  ACA_LOG_DEBUG("gsOperationReply - resource_type: %d\n", resource_type);
  ACA_LOG_DEBUG("gsOperationReply - operation_type: %d\n", operation_type);
  ACA_LOG_DEBUG("gsOperationReply - operation_status: %d\n", overall_operation_status);
  ACA_LOG_DEBUG("gsOperationReply - dataplane_programming_time: %lu\n",
                culminative_dataplane_programming_time);
  ACA_LOG_DEBUG("gsOperationReply - network_configuration_time: %lu\n",
                culminative_network_configuration_time);
  ACA_LOG_DEBUG("gsOperationReply - total_operation_time: %lu\n", state_elapse_time);

  // -----critical section starts-----
  // (exclusive write access to gsOperationReply signaled by locking gs_reply_mutex):
  gs_reply_mutex.lock();
  GoalStateOperationReply_GoalStateOperationStatus *new_operation_statuses =
          gsOperationReply.add_operation_statuses();
  new_operation_statuses->set_resource_id(id);
  new_operation_statuses->set_resource_type(resource_type);
  new_operation_statuses->set_operation_type(operation_type);
  new_operation_statuses->set_operation_status(overall_operation_status);
  new_operation_statuses->set_dataplane_programming_time(culminative_dataplane_programming_time);
  new_operation_statuses->set_network_configuration_time(culminative_network_configuration_time);
  new_operation_statuses->set_state_elapse_time(state_elapse_time);
  gs_reply_mutex.unlock();
  // -----critical section ends-----
}

} // namespace aca_goal_state_handler
