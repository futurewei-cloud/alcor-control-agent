set(SOURCES
    ./comm/messageproducer.cpp
    ./comm/messageconsumer.cpp
    ./comm/aca_comm_mgr.cpp
)
FIND_LIBRARY(RDKAFKA rdkafka /usr/lib/x86_64-linux-gnu NO_DEFAULT_PATH)
FIND_LIBRARY(CPPKAFKA cppkafka /usr/local/lib64 NO_DEFAULT_PATH)
FIND_LIBRARY(MESSAGEMANAGER messagemanager ${CMAKE_CURRENT_SOURCE_DIR}/../include NO_DEFAULT_PATH)
link_libraries(${RDKAFKA} ${CPPKAFKA})
include_directories(${RDKAFKA_INCLUDE_DIR} ${CPPKAFKA_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/proto3)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Transit/src/rpcgen)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Transit/src/include)

FIND_PACKAGE(Protobuf REQUIRED)

add_library(networkControlAgentLib STATIC ${SOURCES})

SET(OBJS
    ${CMAKE_CURRENT_SOURCE_DIR}/../Transit/src/rpcgen/trn_rpc_protocol_clnt.o
    ${CMAKE_CURRENT_SOURCE_DIR}/../Transit/src/rpcgen/trn_rpc_protocol_xdr.o
)

SET_SOURCE_FILES_PROPERTIES(
    ${OBJS}
    PROPERTIES
    EXTERNAL_OBJECT true
    GENERATED true
)

add_executable(networkControlAgent netwagent.cpp ${OBJS})
target_link_libraries(networkControlAgent cppkafka)
target_link_libraries(networkControlAgent rdkafka)
target_link_libraries(networkControlAgent networkControlAgentLib)
target_link_libraries(networkControlAgent proto)
target_link_libraries(networkControlAgent ${PROTOBUF_LIBRARY})

add_dependencies(networkControlAgentLib proto)
add_subdirectory(proto3)