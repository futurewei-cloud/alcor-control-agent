set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../build/bin)

set(SOURCES
    ./comm/aca_message_producer.cpp
    ./comm/aca_message_consumer.cpp
    ./comm/aca_message_pulsar_consumer.cpp
    ./comm/aca_message_pulsar_producer.cpp
    ./comm/aca_comm_mgr.cpp
    ./comm/aca_grpc.cpp
    ./comm/aca_grpc_subscribe.cpp
    ./comm/aca_grpc_client.cpp
    ./dp_abstraction/aca_goal_state_handler.cpp
    ./dp_abstraction/aca_dataplane_ovs.cpp
    ./net_config/aca_net_config.cpp
    ./ovs/aca_ovs_l2_programmer.cpp
    ./ovs/aca_ovs_l3_programmer.cpp
    ./ovs/aca_vlan_manager.cpp
    ./ovs/ovs_control.cpp
    ./ovs/aca_ovs_control.cpp
    ./ovs/aca_arp_responder.cpp
    ./ovs/libfluid-base/base/BaseOFClient.cc
    ./ovs/libfluid-base/base/BaseOFConnection.cc
    ./ovs/libfluid-base/base/BaseOFServer.cc
    ./ovs/libfluid-base/base/EventLoop.cc
    ./ovs/libfluid-base/OFClient.cc
    ./ovs/libfluid-base/OFConnection.cc
    ./ovs/libfluid-base/OFServer.cc
    ./ovs/libfluid-base/OFServerSettings.cc
    ./ovs/libfluid-base/TLS.cc
    ./ovs/libfluid-msg/of10/of10action.cc
    ./ovs/libfluid-msg/of10/of10common.cc
    ./ovs/libfluid-msg/of10/of10match.cc
    ./ovs/libfluid-msg/of13/of13action.cc
    ./ovs/libfluid-msg/of13/of13common.cc
    ./ovs/libfluid-msg/of13/of13instruction.cc
    ./ovs/libfluid-msg/of13/of13match.cc
    ./ovs/libfluid-msg/of13/of13meter.cc
    ./ovs/libfluid-msg/ofcommon/action.cc
    ./ovs/libfluid-msg/ofcommon/common.cc
    ./ovs/libfluid-msg/ofcommon/msg.cc
    ./ovs/libfluid-msg/util/ethaddr.cc
    ./ovs/libfluid-msg/util/ipaddr.cc
    ./ovs/libfluid-msg/of10msg.cc
    ./ovs/libfluid-msg/of13msg.cc
    ./ovs/of_message.cpp
    ./ovs/of_controller.cpp
    ./on_demand/aca_on_demand_engine.cpp
    ./dhcp/aca_dhcp_state_handler.cpp
    ./dhcp/aca_dhcp_server.cpp
    ./zeta/aca_zeta_oam_server.cpp
    ./zeta/aca_zeta_programming.cpp
)

#Find libevent installation
find_path(LIBEVENT_INCLUDE_DIR
        NAMES event2/thread.h
        HINTS /usr/include
        REQUIRED)

FIND_LIBRARY(LIBUUID_LIBRARIES uuid)
FIND_LIBRARY(RDKAFKA rdkafka /usr/lib/x86_64-linux-gnu NO_DEFAULT_PATH)
FIND_LIBRARY(CPPKAFKA cppkafka /usr/local/lib NO_DEFAULT_PATH)
FIND_LIBRARY(PULSAR pulsar /usr/local/lib NO_DEFAULT_PATH)
FIND_LIBRARY(MESSAGEMANAGER messagemanager ${CMAKE_CURRENT_SOURCE_DIR}/../include NO_DEFAULT_PATH)
link_libraries(${RDKAFKA} ${CPPKAFKA} ${PULSAR})
link_libraries(/usr/lib/x86_64-linux-gnu/libuuid.so)
link_libraries(/usr/lib/x86_64-linux-gnu/libevent_pthreads.so)
link_libraries(/usr/lib/x86_64-linux-gnu/libpthread.so)
link_libraries(/usr/local/lib/libopenvswitch.a) #this was installed by aca-machine-init.sh
include_directories(${RDKAFKA_INCLUDE_DIR} ${CPPKAFKA_INCLUDE_DIR} ${PULSAR_INCLUDE_DIR} ${LIBEVENT_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/proto3)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/grpc)
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
target_link_libraries(AlcorControlAgentLib event) #Libevent linking is '-levent', thus linked lib is 'event' in the cmd
target_link_libraries(AlcorControlAgentLib ssl)
target_link_libraries(AlcorControlAgentLib crypto)
target_link_libraries(AlcorControlAgentLib rt)

add_executable(AlcorControlAgent aca_main.cpp)
target_link_libraries(AlcorControlAgent cppkafka)
target_link_libraries(AlcorControlAgent rdkafka)
target_link_libraries(AlcorControlAgent pulsar)
target_link_libraries(AlcorControlAgent AlcorControlAgentLib)
target_link_libraries(AlcorControlAgent proto)
target_link_libraries(AlcorControlAgent grpc)
target_link_libraries(AlcorControlAgent ${PROTOBUF_LIBRARY})
target_link_libraries(AlcorControlAgent ${_GRPC_GRPCPP_UNSECURE})


add_dependencies(AlcorControlAgentLib proto grpc)
add_subdirectory(proto3)
add_subdirectory(grpc)