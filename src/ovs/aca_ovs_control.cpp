// Copyright 2019 The Alcor Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "aca_ovs_control.h"
#include "ovs_control.h"
#include <iostream>
#include <vector> 
#include <unordered_map> 
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <chrono>
#include <errno.h>
#include <stdexcept>
#include <inttypes.h>
// #include <unistd.h>
#include <pcap.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

using namespace std;
using namespace ovs_control;

extern string g_ofctl_command;
extern string g_ofctl_target;
extern string g_ofctl_options;

namespace aca_ovs_control
{
ACA_OVS_Control &ACA_OVS_Control::get_instance()
{
  // Instance is destroyed when program exits.
  // It is instantiated on first use.
  static ACA_OVS_Control instance;  

  return instance;
}

int ACA_OVS_Control::control()
{
    int overall_rc = EXIT_SUCCESS;

    char target[g_ofctl_target.size() + 1];
    g_ofctl_target.copy(target, g_ofctl_target.size() + 1);
    target[g_ofctl_target.size()] = '\0';

    char options[g_ofctl_options.size() + 1];
    g_ofctl_options.copy(options, g_ofctl_options.size() + 1);
    options[g_ofctl_options.size()] = '\0';

    if (g_ofctl_command.compare("monitor") == 0) {
        monitor(target, options);
    } else if (g_ofctl_command.compare("dump-flows") == 0) {
        dump_flows(target, options);
    } else if (g_ofctl_command.compare("packet-out") == 0) {
        packet_out(target, options);
    } else {
        cout << "Usage: -c <command> -t <target> -o <options>" << endl;
        cout << "   commands: monitor, dump-flows, packet-out..." << endl;
        cout << "   target: swtich name, such as br-int, br-tun, ..." << endl;
        cout << "   options: " << endl;
        cout << "      moinor: \"[miss-len] [invalid-ttl] [resume] [watch:format]\"" << endl;
        cout << "      packet-out: \"in_port=<in_port> packet=<hex string> [actions=<actions>]\"" << endl;
    }

    return overall_rc;
}

int ACA_OVS_Control::dump_flows(const char *bridge, const char *opt) 
{
    int overall_rc = EXIT_SUCCESS;
    OVS_Control::get_instance().dump_flows(bridge, opt);
    return overall_rc;
}

int ACA_OVS_Control::monitor(const char *bridge, const char *opt) 
{
    int overall_rc = EXIT_SUCCESS;
    OVS_Control::get_instance().monitor(bridge, opt);
    return overall_rc;
}

int ACA_OVS_Control::packet_out(const char *bridge, const char *opt) 
{
    int overall_rc = EXIT_SUCCESS;
    OVS_Control::get_instance().packet_out(bridge, opt);
    return overall_rc;
}

void ACA_OVS_Control::parse_packet(void *packet)
{
    const struct ether_header *eth_header;
    /* The packet is larger than the ether_header struct,
    but we just want to look at the first part of the packet
    that contains the header. We force the compiler
    to treat the pointer to the packet as just a pointer
    to the ether_header. The data payload of the packet comes
    after the headers. Different packet types have different header
    lengths though, but the ethernet header is always the same (14 bytes) */
    eth_header = (struct ether_header *) packet;
    
    printf("Destination Mac: %s\n", ether_ntoa((ether_addr *) &eth_header->ether_dhost));
    printf("Source Mac: %s\n", ether_ntoa((ether_addr *) &eth_header->ether_shost));

    int vlan_len = 0;
    char *base = (char *) packet;
    uint16_t ether_type = ntohs(*(uint16_t *) (base + 12));
    if (ether_type == ETHERTYPE_VLAN) {
        printf("Ethernet Type: 802.1Q VLAN tagging (0x8100) \n");
        ether_type = ntohs(*(uint16_t *) (base + 16));
        vlan_len = 4;
    } 
    
    if (ether_type == ETHERTYPE_ARP) {
        printf("Ethernet Type: ARP (0x0806) \n");
        printf("   From: %s\n", inet_ntoa(*(in_addr *) (base + 14 + vlan_len + 14)));
        printf("     to: %s\n", inet_ntoa(*(in_addr *) (base + 14 + vlan_len + 14 + 10)));
    } else if (ether_type == ETHERTYPE_IP) {
        printf("Ethernet Type: IP (0x0800) \n");
    } else if (ether_type == ETHERTYPE_REVARP) {
        printf("Ethernet Type: REVARP (0x8035) \n");
    } else {
        printf("Ethernet Type: Cannot Tell!\n");
        return;
    }
    
    /* define/compute ip header offset */
    const struct sniff_ip *ip = (struct sniff_ip*)(base + SIZE_ETHERNET + vlan_len);
    int size_ip = IP_HL(ip)*4;
    // const struct ip *ip = (struct ip*)(base + SIZE_ETHERNET + vlan_len);
    // int size_ip = ip->ip_hl;
    
    if (size_ip < 20) {
        // printf("   *** Invalid IP header length: %u bytes\n", size_ip);
        return;
    } else {
        /* print source and destination IP addresses */
        printf("       From: %s\n", inet_ntoa(ip->ip_src));
        printf("         To: %s\n", inet_ntoa(ip->ip_dst));
        
        /* determine protocol */ 
        switch(ip->ip_p) {
            case IPPROTO_TCP:
                printf("   Protocol: TCP\n");
                break;
            case IPPROTO_UDP:
                printf("   Protocol: UDP\n");
                break;
            case IPPROTO_ICMP:
                printf("   Protocol: ICMP\n");
                break;
            case IPPROTO_IP:
                printf("   Protocol: IP\n");
                break;
            default:
                printf("   Protocol: unknown\n");
        }
    }
    /* define/compute tcp header offset */
    const struct sniff_tcp *tcp = (struct sniff_tcp*)(base + SIZE_ETHERNET + vlan_len + size_ip);
    const unsigned char *payload; 
    int size_payload;
    int size_tcp = TH_OFF(tcp)*4;

    if (size_tcp < 20) {
        // printf("   *** Invalid TCP header length: %u bytes\n", size_tcp);
        return;
    } else {    
        printf("   Src port: %d\n", ntohs(tcp->th_sport));
        printf("   Dst port: %d\n", ntohs(tcp->th_dport));
        
        /* define/compute tcp payload (segment) offset */
        payload = (u_char *)(base + SIZE_ETHERNET + vlan_len + size_ip + size_tcp);
        
        /* compute tcp payload (segment) size */
        size_payload = ntohs(ip->ip_len) - (size_ip + size_tcp);
        
        /*
        * Print payload data; it might be binary, so don't just
        * treat it as a string.
        */
        if (size_payload > 0) {
            printf("   Payload (%d bytes):\n", size_payload);
            print_payload(payload, size_payload);
        }
    }

    /*
     * To Do: parse UPD packet and identity DHCP
     */
     /* define/compute udp header offset */
    const struct sniff_udp *udp = (struct sniff_udp*)(base + SIZE_ETHERNET + vlan_len + size_ip);
    int size_udp = ntohs(udp->uh_ulen);
    
    if (size_udp < 20) {
        return;
    } else {
        int udp_sport = ntohs(udp->uh_sport);
        int udp_dport = ntohs(udp->uh_dport);
        printf("   Src port: %d\n", udp_sport);
        printf("   Dst port: %d\n", udp_dport);
        
        if (udp_sport == 68 && udp_dport == 67) {
            printf ("   Message Type: DHCP\n");
        }

        /* define/compute udp payload (daragram) offset */
        payload = (u_char *)(base + SIZE_ETHERNET + vlan_len + size_ip + 8);

        /* compute udp payload (datagram) size */
        size_payload = ntohs(ip->ip_len) - (size_ip + 8);

        /*
        * Print payload data; it might be binary, so don't just
        * treat it as a string.
        */
        if (size_payload > 0) {
            printf("   Payload (%d bytes):\n", size_payload);
            print_payload(payload, size_payload);
        }
    }
}

/*
 * print packet payload data (avoid printing binary data)
 */
void
ACA_OVS_Control::print_payload(const u_char *payload, int len)
{

    int len_rem = len;
    int line_width = 16;   /* number of bytes per line */
    int line_len;
    int offset = 0;     /* zero-based offset counter */
    const u_char *ch = payload;

    if (len <= 0)
    return;

    /* data fits on one line */
    if (len <= line_width) {
        print_hex_ascii_line(ch, len, offset);
    } else {
        /* data spans multiple lines */
        for ( ;; ) {
            /* compute current line length */
            line_len = line_width % len_rem;
            /* print line */
            print_hex_ascii_line(ch, line_len, offset);
            /* compute total remaining */
            len_rem = len_rem - line_len;
            /* shift pointer to remaining bytes to print */
            ch = ch + line_len;
            /* add offset */
            offset = offset + line_width;
            /* check if we have line width chars or less */
            if (len_rem <= line_width) {
                /* print last line and get out */
                print_hex_ascii_line(ch, len_rem, offset);
                break;
            }
        }
    }
}

/*
 * print data in rows of 16 bytes: offset   hex   ascii
 * 00000   47 45 54 20 2f 20 48 54  54 50 2f 31 2e 31 0d 0a   GET / HTTP/1.1..
 */
void
ACA_OVS_Control::print_hex_ascii_line(const u_char *payload, int len, int offset)
{
    int i;
    int gap;
    const u_char *ch;

    /* offset */
    printf("%05d   ", offset);
    
    /* hex */
    ch = payload;
    for(i = 0; i < len; i++) {
        printf("%02x ", *ch);
        ch++;
        /* print extra space after 8th byte for visual aid */
        if (i == 7) printf(" ");
    }
    /* print space to handle line less than 8 bytes */
    if (len < 8) printf(" ");
    
    /* fill hex gap with spaces if not full line */
    if (len < 16) {
        gap = 16 - len;
        for (i = 0; i < gap; i++) {
            printf("   ");
        }
    }
    printf("   ");
    
    /* ascii (if printable) */
    ch = payload;
    for(i = 0; i < len; i++) {
        if (isprint(*ch)) printf("%c", *ch);
        else printf(".");
        ch++;
    }
    printf("\n");
    return;
}

} // namespace aca_ovs_control
