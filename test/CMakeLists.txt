cmake_minimum_required(VERSION 3.1)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../build/tests)


FIND_LIBRARY(RDKAFKA rdkafka /usr/lib/x86_64-linux-gnu NO_DEFAULT_PATH)
FIND_LIBRARY(CPPKAFKA cppkafka /usr/local/lib NO_DEFAULT_PATH)
FIND_LIBRARY(PULSAR pulsar /usr/lib NO_DEFAULT_PATH)
#FIND_LIBRARY(OPENVSWITCH openvswitch /usr/local/lib NO_DEFAULT_PATH)
FIND_LIBRARY(MESSAGEMANAGER messagemanager ${CMAKE_CURRENT_SOURCE_DIR}/../include NO_DEFAULT_PATH)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src/proto3)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src/grpc)
link_libraries(${PULSAR})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

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

# Setup gtest
enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIR})

# Add test cpp file
add_executable(
    aca_tests
    gtest/aca_test_main.cpp
    gtest/aca_test_dhcp.cpp
    gtest/aca_test_net_config.cpp
    gtest/aca_test_openflow.cpp
    gtest/aca_test_ovs_l2.cpp
    gtest/aca_test_ovs_l3.cpp
    gtest/aca_test_ovs_util.cpp
    gtest/aca_test_oam.cpp
    gtest/aca_test_zeta_programming.cpp
    gtest/aca_test_arp.cpp
    gtest/aca_test_on_demand.cpp
)

# Link test executable against gtest & gtest_main
target_link_libraries(aca_tests gtest gtest_main)
target_link_libraries(aca_tests pulsar)
#target_link_libraries(aca_tests openvswitch)
target_link_libraries(aca_tests AlcorControlAgentLib)
target_link_libraries(aca_tests proto)
target_link_libraries(aca_tests grpc)
target_link_libraries(aca_tests ${_GRPC_GRPCPP_UNSECURE})
target_link_libraries(aca_tests ${PROTOBUF_LIBRARY})
target_link_libraries(aca_tests Threads::Threads)

add_test(
    NAME aca_tests
    COMMAND aca_tests
)

# goal state test to process goal state message and send to local client
# add_executable(gs_tests func_tests/gs_tests.cpp)

# target_link_libraries(gs_tests AlcorControlAgentLib)
# target_link_libraries(gs_tests proto)
# target_link_libraries(gs_tests ${PROTOBUF_LIBRARY})
# target_link_libraries(gs_tests grpc)
# target_link_libraries(gs_tests ${_GRPC_GRPCPP_UNSECURE})