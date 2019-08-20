FIND_LIBRARY(RDKAFKA rdkafka /usr/lib/x86_64-linux-gnu NO_DEFAULT_PATH)
FIND_LIBRARY(CPPKAFKA cppkafka /usr/local/lib64 NO_DEFAULT_PATH)
FIND_LIBRARY(MESSAGEMANAGER messagemanager ${CMAKE_CURRENT_SOURCE_DIR}/../include NO_DEFAULT_PATH)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src/proto3)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Transit/src/include)
link_libraries(${RDKAFKA} ${CPPKAFKA})

# Find Protobuf installation
# Looks for protobuf-config.cmake file installed by Protobuf's cmake installation.
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${protobuf_VERSION}")

add_executable(aca_func_tests func_tests/aca_func_tests.cpp)

target_link_libraries(aca_func_tests networkControlAgentLib)
target_link_libraries(aca_func_tests proto)
target_link_libraries(aca_func_tests transitRPCLib)
target_link_libraries(aca_func_tests ${PROTOBUF_LIBRARY})