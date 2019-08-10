
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../Transit)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Transit/src/rpcgen/trn_rpc_protocol_clnt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Transit/src/rpcgen/trn_rpc_protocol_xdr.c
)

ADD_LIBRARY(transitRPCLib STATIC ${SOURCES})
