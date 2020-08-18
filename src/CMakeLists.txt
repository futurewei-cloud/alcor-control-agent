set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../build/bin)

set(SOURCES
    ./comm/aca_message_producer.cpp
    ./comm/aca_message_consumer.cpp
    ./comm/aca_comm_mgr.cpp
    ./comm/aca_async_grpc_server.cpp
    ./dp_abstraction/aca_goal_state_handler.cpp
    ./dp_abstraction/aca_dataplane_ovs.cpp
    ./net_config/aca_net_config.cpp
    ./ovs/aca_ovs_l2_programmer.cpp
    ./ovs/aca_ovs_l3_programmer.cpp
    ./ovs/aca_vlan_manager.cpp
    ./ovs/ovs_control.cpp
    ./ovs/aca_ovs_control.cpp
    ./dhcp/aca_dhcp_state_handler.cpp
    ./dhcp/aca_dhcp_server.cpp    
)
FIND_LIBRARY(RDKAFKA rdkafka /usr/lib/x86_64-linux-gnu NO_DEFAULT_PATH)
FIND_LIBRARY(CPPKAFKA cppkafka /usr/local/lib NO_DEFAULT_PATH)
FIND_LIBRARY(OPENVSWITCH openvswitch /usr/local/lib NO_DEFAULT_PATH)
FIND_LIBRARY(MESSAGEMANAGER messagemanager ${CMAKE_CURRENT_SOURCE_DIR}/../include NO_DEFAULT_PATH)
link_libraries(${RDKAFKA} ${CPPKAFKA} ${OPENVSWITCH})
include_directories(${RDKAFKA_INCLUDE_DIR} ${CPPKAFKA_INCLUDE_DIR} ${OPENVSWITCH_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/proto3)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/grpc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../mizar/src/include)
include_directories(/usr/local/include/openvswitch)
include_directories(/usr/local/include/openflow)

# Find Protobuf installation
# Looks for protobuf-config.cmake file installed by Protobuf's cmake installation.
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${protobuf_VERSION}")

# Find gRPC installation
# Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

set(_GRPC_GRPCPP_UNSECURE gRPC::grpc++_unsecure)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

add_library(AlcorControlAgentLib STATIC ${SOURCES})

add_executable(AlcorControlAgent aca_main.cpp)
target_link_libraries(AlcorControlAgent cppkafka)
target_link_libraries(AlcorControlAgent rdkafka)
target_link_libraries(AlcorControlAgent openvswitch)
target_link_libraries(AlcorControlAgent AlcorControlAgentLib)
target_link_libraries(AlcorControlAgent proto)
target_link_libraries(AlcorControlAgent grpc)
target_link_libraries(AlcorControlAgent ${PROTOBUF_LIBRARY})
target_link_libraries(AlcorControlAgent ${_GRPC_GRPCPP_UNSECURE})

add_dependencies(AlcorControlAgentLib proto grpc)
add_subdirectory(proto3)
add_subdirectory(grpc)
