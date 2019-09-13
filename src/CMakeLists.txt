set(SOURCES
    ./comm/messageproducer.cpp
    ./comm/messageconsumer.cpp
    ./comm/aca_comm_mgr.cpp
    ./comm/aca_async_grpc_server.cpp
)
FIND_LIBRARY(RDKAFKA rdkafka /usr/lib/x86_64-linux-gnu NO_DEFAULT_PATH)
FIND_LIBRARY(CPPKAFKA cppkafka /usr/local/lib NO_DEFAULT_PATH)
FIND_LIBRARY(MESSAGEMANAGER messagemanager ${CMAKE_CURRENT_SOURCE_DIR}/../include NO_DEFAULT_PATH)
link_libraries(${RDKAFKA} ${CPPKAFKA})
include_directories(${RDKAFKA_INCLUDE_DIR} ${CPPKAFKA_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/proto3)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/grpc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/transit_rpc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Transit/src/rpcgen)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Transit/src/include)

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

add_library(networkControlAgentLib STATIC ${SOURCES})

add_executable(networkControlAgent netwagent.cpp)
target_link_libraries(networkControlAgent cppkafka)
target_link_libraries(networkControlAgent rdkafka)
target_link_libraries(networkControlAgent networkControlAgentLib)
target_link_libraries(networkControlAgent proto)
target_link_libraries(networkControlAgent grpc)
target_link_libraries(networkControlAgent transitRPCLib)
target_link_libraries(networkControlAgent ${PROTOBUF_LIBRARY})
target_link_libraries(networkControlAgent ${_GRPC_GRPCPP_UNSECURE})

add_dependencies(networkControlAgentLib proto grpc transitRPCLib)
add_subdirectory(proto3)
add_subdirectory(grpc)
add_subdirectory(transit_rpc)
