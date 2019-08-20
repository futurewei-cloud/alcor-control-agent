
# Find Protobuf installation
# Looks for protobuf-config.cmake file installed by Protobuf's cmake installation.
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${protobuf_VERSION}")

set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)

# Find gRPC installation
# Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

set(_GRPC_GRPCPP_UNSECURE gRPC::grpc++_unsecure)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

# Proto file
get_filename_component(aca_proto "../../AliothController/src/schema/proto3/*.proto" ABSOLUTE)
get_filename_component(aca_proto_path "${aca_proto}" PATH)

set(aca_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/goalstateprovisioner.pb.cc")
set(aca_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/goalstateprovisioner.pb.h")
set(aca_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/goalstateprovisioner.grpc.pb.cc")
set(aca_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/goalstateprovisioner.grpc.pb.h")
add_custom_command(
      OUTPUT "${aca_proto_srcs}" "${aca_proto_hdrs}" "${aca_grpc_srcs}" "${aca_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${aca_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${aca_proto}"
      DEPENDS "${aca_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
 
ADD_LIBRARY(grpc ${aca_proto_srcs} ${aca_proto_hdrs} ${aca_grpc_srcs} ${aca_grpc_hdrs})
