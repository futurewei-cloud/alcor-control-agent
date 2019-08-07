#ifndef ACA_UTIL_H
#define ACA_UTIL_H

#include <stdlib.h>

// Defines
static char BROKER_LIST[] = "10.213.43.158:9092";
static char KAFKA_TOPIC[] = "Host-ts-1";
static char KAFKA_GROUP_ID[] = "test-group-id";
static char LOCALHOST[] = "localhost";
static char UDP[] = "udp";

static char PHYSICAL_IF[] = "eth0";
static char EMPTY_STRING[] = "";
static char PEER_POSTFIX[] = "_peer";

// copying the defines until it is defined in transit RPC interface
#define TRAN_SUBSTRT_VNI 0
#define TRAN_SUBSTRT_EP 0
#define TRAN_SIMPLE_EP 1


#endif
