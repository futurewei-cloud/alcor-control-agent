
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../mizar)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mizar/src/rpcgen/trn_rpc_protocol_clnt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mizar/src/rpcgen/trn_rpc_protocol_xdr.c
)

ADD_LIBRARY(MizarRPCLib STATIC ${SOURCES})
