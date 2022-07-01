//
// Created by Rio Zhu on 6/22/22.
//
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/ethernet.h> /* the L2 protocols */
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/types.h>
#include <errno.h>
#include <bpf.h>
#include <libbpf.h>
#include "bpf_endian.h"
//#include "bpf_helpers.h"
#include <time.h>

#define TRAN_MAX_PATH_SIZE 256

/* XDP programs keys in transit XDP tailcall jump table */
enum trn_xdp_prog_id_t {
    TRAN_TRANSIT_PROG = 0,
    TRAN_TX_PROG,
    TRAN_PASS_PROG,
    TRAN_REDIRECT_PROG,
    TRAN_DROP_PROG,
    TRAN_MAX_PROG
};

/* XDP interface_map keys for packet redirect */
enum trn_itf_ma_key_t {
    TRAN_ITF_MAP_TENANT = 0,     // id map to ifindex connected to tenant network
    TRAN_ITF_MAP_ZGC,            // id map to ifindex connected to zgc network
    TRAN_ITF_MAP_MAX
};

enum trn_xdp_role_t {
    XDP_FWD = 0,
    XDP_FTN,
    XDP_ROLE_MAX
};


typedef struct {
    int prog_id;          // definition in trn_xdp_prog_id_t
    char *prog_path;      // full path of xdp program
} trn_xdp_prog_t;

typedef struct {
    __u32 ip;
    __u32 iface_index;
    __u16 ibo_port;
    __u8 protocol;     // value from trn_xdp_tunnel_protocol_t
    __u8 role;         // value from trn_xdp_role_t
    __u8 mac[6];       // MAC of physical interface
} trn_iface_t;

typedef struct {
    int prog_fd;
    __u32 prog_id;
    struct bpf_object *obj;
    char pcapfile[TRAN_MAX_PATH_SIZE];
} trn_prog_t;

typedef struct {
    trn_iface_t eth;
    trn_prog_t xdp;
} trn_xdp_object_t;

typedef struct {
    bool ready;
    __u32 xdp_flags;
    trn_xdp_object_t objs[TRAN_ITF_MAP_MAX];

    trn_xdp_prog_t *prog_tbl;

    /*
	 * Array of sidecar programs transit XDP main program can jump to
	 * to perform additional functionality.
	 */
    trn_prog_t ebpf_progs[TRAN_MAX_PROG];
} user_metadata_t;

static user_metadata_t *md = NULL;

typedef struct {
    __u32 saddr;
    __u32 daddr;
    __u16 sport;
    __u16 dport;
    __u8 protocol;
    __u8 vni[3];
} __attribute__((packed, aligned(4))) ipv4_flow_t;

/* Direct Path oam op data */
typedef struct {
    /* Destination Encap */
    __u32 dip;
    __u32 dhip;
    __u8 dmac[6];
    __u8 dhmac[6];
    __u16 timeout;      /* in seconds */
    __u16 rsvd;
} dp_encap_opdata_t;

typedef struct {
    __u16 len;
    struct ethhdr eth;
    struct iphdr ip;
    struct udphdr udp;
    __u32 opcode;       // trn_xdp_flow_op_t

    /* OAM OpData */
    ipv4_flow_t flow;	// flow matcher

    union {
        dp_encap_opdata_t encap;
    } opdata;
} __attribute__((packed, aligned(8))) flow_ctx_t;

typedef struct {
    char *name;
    bool shared;
    int fd;
    struct bpf_map *map;
} trn_xdp_map_t;

/* Make sure to keep in-sync with XDP programs, order doesn't matter */
static trn_xdp_map_t trn_xdp_bpfmaps[] = {
    {"jmp_table", true, -1, NULL},
    {"endpoints_map", true, -1, NULL},
#if turnOn
    {"hosted_eps_if", true, -1, NULL},
#endif
    {"if_config_map", true, -1, NULL},
    {"interfaces_map", true, -1, NULL},
#if turnOn
    {"oam_queue_map", true, -1, NULL},
    {"fwd_flow_cache", true, -1, NULL},
    {"rev_flow_cache", true, -1, NULL},
    {"host_flow_cache", true, -1, NULL},
    {"ep_host_cache", true, -1, NULL},
#endif
    {"xdpcap_hook", false, -1, NULL},
};

static trn_xdp_map_t * trn_transit_map_get(char *map_name)
{
    int num_maps = sizeof(trn_xdp_bpfmaps) / sizeof(trn_xdp_bpfmaps[0]);
    for (int i = 0; i < num_maps; i++) {
        if (strcmp(trn_xdp_bpfmaps[i].name, map_name) == 0) {
            return &trn_xdp_bpfmaps[i];
        }
    }
    return NULL;
}

static int trn_transit_map_get_fd(char *map_name)
{
    trn_xdp_map_t *xdpmap = trn_transit_map_get(map_name);
    if (!xdpmap) {
        printf("Failed to find bpfmap %s.\n", map_name);
        return -1;
    }

    return xdpmap->fd;
}

static inline void trn_set_mac(void *dst, unsigned char *mac)
{
    unsigned short *d = dst;
    unsigned short *s = (unsigned short *)mac;

    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

int trn_transit_dp_assistant(void)
{
    flow_ctx_t sendbuf;
    int sockfd, oam_fd, rc;
    rc = 0;
    struct sockaddr_ll socket_address;
    __u32 if_idx;

    struct sockaddr_in socket_receiver_address;
    memset(&socket_receiver_address, 0x00, sizeof(socket_receiver_address));
    socket_receiver_address.sin_family = AF_INET;
    socket_receiver_address.sin_port = htons(8300);
    socket_receiver_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    /* Wait for Transit XDP provisioned */
//    while (!md || !md->ready) {
//        //usleep(500*1000);
//        nanosleep((const struct timespec[]){{0, 500*1000*1000L}}, NULL);
//    }
    printf("DPA ready!\n");

    /* Grab the tx interface index */
//    if_idx = md->objs[XDP_FWD].eth.iface_index;

    /* Grab the oam_queue_map handle */
//    oam_fd = trn_transit_map_get_fd("oam_queue_map");

    /* Open RAW socket to send on */
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) < 0) {
        printf("DPA failed to open raw socket! Err: %s\n", strerror(errno));
        return 1;
    }
    printf("Successfully created a socket.\n");
    for(;;){
        printf("One round in the forever for loop...\n");
        dp_encap_opdata_t opdata;
        unsigned char src_mac[6] = {00,00,01,00,00,03}; //"00:00:01:00:00:03"
        unsigned char dst_mac[6] = {00,00,01,00,00,02}; //"00:00:01:00:00:02

        strcpy(sendbuf.eth.h_dest, "00:00:01:0:0:3");
        strcpy(sendbuf.eth.h_source, "00:00:01:0:0:2");
        sendbuf.eth.h_proto = bpf_htons(ETH_P_IP);
        sendbuf.opcode = 0; // insert flow

        opdata.dhip = inet_addr("10.0.0.3");
        trn_set_mac(opdata.dhmac, src_mac);

        opdata.dip = inet_addr("10.0.0.2");
        trn_set_mac(opdata.dmac, dst_mac);

        sendbuf.len = sizeof(sendbuf.opcode) + sizeof(sendbuf.flow)+ sizeof(sendbuf.opdata);
        sendbuf.opdata.encap = opdata;
        ipv4_flow_t flow;
        flow.sport = 0;
        flow.dport = 0;
        flow.daddr = inet_addr("10.0.0.3");
        flow.saddr = inet_addr("10.0.0.2");
        flow.protocol = IPPROTO_UDP;
        uint vni = 21;
        flow.vni[0] = (__u8)vni >> 16;
        flow.vni[1] = (__u8)vni >> 8;
        flow.vni[2] = (__u8)vni;

//        sprintf(flow.vni, "%ld", 21);

        sendbuf.flow = flow;

        printf("Assigned values to sendbuf...\n");

        // change following line to read from some kinds of file/array
//        rc = bpf_map_lookup_and_delete_elem(oam_fd, NULL, (void *)&sendbuf);
        if (rc) {
            /* No more to send */
            //usleep(500);
            printf("No need to send, sleep a little bit...\n");
            nanosleep((const struct timespec[]){{0, 500*1000L}}, NULL);
        } else {
            printf("DO need to send message.\n");

            /* OAM packet in buffer, ship it */
//            socket_address.sll_ifindex = if_idx;
//            socket_address.sll_halen = ETH_ALEN;
            /* Destination MAC */
//            memcpy(socket_address.sll_addr, sendbuf.eth.h_dest,
//                   sizeof(sendbuf.eth.h_dest));
            int len;
            if (sendto(sockfd, &sendbuf.opcode, sendbuf.len, 0,
                       (struct sockaddr*)&socket_receiver_address/*&socket_address*/, sizeof(socket_receiver_address/*sockaddr_ll*/)) < 0) {
                printf("DPA failed to send oam packet to 0x%08x, err: %s\n",
                              socket_receiver_address.sin_addr.s_addr, strerror(errno));
            }
            printf("Message sent!\n");
        }
    }

    return 0;
}


int main(){
    printf("Hello World!\n");
    trn_transit_dp_assistant();
    exit(1);
}