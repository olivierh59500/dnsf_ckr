#include "arp.h"
#include "mem.h"
#include "eth.h"
#include "sk.h"
#include "if.h"
#include "ip.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/bpf.h>
#include <string.h>
#include <ctype.h>

#define getnibble(n) ( isxdigit(n) && isalpha(n) ? toupper(n) - 55 :\
                       isxdigit(n) && isdigit(n) ? n - 48 : n )

struct dnsf_ckr_arp_header *dnsf_ckr_parse_arp_dgram(const unsigned char *buf, const size_t bsize) {
    struct dnsf_ckr_arp_header *arph = NULL;
    if (buf == NULL) {
        return NULL;
    }
    arph = (struct dnsf_ckr_arp_header *) dnsf_ckr_getmem(sizeof(struct dnsf_ckr_arp_header));
    memset(arph, 0, sizeof(struct dnsf_ckr_arp_header));
    if (bsize == 0) {
        return NULL;
    }
    arph->hwtype = (unsigned short)(buf[0] << 8) | (unsigned short)(buf[1]);
    arph->ptype = (unsigned short)(buf[2] << 8) | (unsigned short)(buf[3]);
    arph->hw_addr_len = buf[4];
    arph->pt_addr_len = buf[5];
    arph->opcode = (unsigned short)(buf[6] << 8) | (unsigned short)(buf[7]);
    arph->src_hw_addr = (unsigned char *) dnsf_ckr_getmem(arph->hw_addr_len);
    memcpy(arph->src_hw_addr, &buf[8], arph->hw_addr_len);
    arph->src_pt_addr = (unsigned char *) dnsf_ckr_getmem(arph->pt_addr_len);
    memcpy(arph->src_pt_addr, &buf[8 + arph->hw_addr_len], arph->pt_addr_len);
    arph->dest_hw_addr = (unsigned char *) dnsf_ckr_getmem(arph->hw_addr_len);
    memcpy(arph->dest_hw_addr, &buf[8 + arph->hw_addr_len + arph->pt_addr_len], arph->hw_addr_len);
    arph->dest_pt_addr = (unsigned char *) dnsf_ckr_getmem(arph->pt_addr_len);
    memcpy(arph->dest_pt_addr, &buf[8 + arph->hw_addr_len + arph->hw_addr_len + arph->pt_addr_len], arph->pt_addr_len);
    return arph;
}

unsigned char *dnsf_ckr_mk_arp_dgram(size_t *bsize, const struct dnsf_ckr_arp_header arph) {
    unsigned char *dgram = NULL, *dp;
    size_t a;
    if (bsize == NULL || arph.src_hw_addr == NULL ||
                         arph.src_pt_addr == NULL ||
                        arph.dest_hw_addr == NULL ||
                        arph.dest_pt_addr == NULL) {
        return NULL;
    }
    dgram = (unsigned char *) dnsf_ckr_getmem(8 + (arph.hw_addr_len * 2) +
                                                  (arph.pt_addr_len * 2));
    dp = dgram;
    *dp = (arph.hwtype & 0xff00) >> 8;
    dp++;
    *dp = (arph.hwtype & 0x00ff);
    dp++;
    *dp = (arph.ptype & 0xff00) >> 8;
    dp++;
    *dp = (arph.ptype & 0x00ff);
    dp++;
    *dp = arph.hw_addr_len;
    dp++;
    *dp = arph.pt_addr_len;
    dp++;
    *dp = (arph.opcode & 0xff00) >> 8;
    dp++;
    *dp = (arph.opcode & 0x00ff);
    dp++;
    for (a = 0; a < arph.hw_addr_len; a++, dp++) {
        *dp = arph.src_hw_addr[a];
    }
    for (a = 0; a < arph.pt_addr_len; a++, dp++) {
        *dp = arph.src_pt_addr[a];
    }
    for (a = 0; a < arph.hw_addr_len; a++, dp++) {
        *dp = arph.dest_hw_addr[a];
    }
    for (a = 0; a < arph.pt_addr_len; a++, dp++) {
        *dp = arph.dest_pt_addr[a];
    }
    *bsize = dp - dgram;
    return dgram;
}

unsigned char *dnsf_ckr_mac2byte(const char *mac, size_t len) {
    const char *m;
    unsigned char *retval = (unsigned char *) dnsf_ckr_getmem(len);
    unsigned char *r = retval, *rend = r + len;
    memset(retval, 0, len);
    for (m = mac; *m != 0; m++) {
        if (r == rend) {
            break;
        }
        if (*m == ':') {
            r++;
        }
        *r = ((*r) << 4) | getnibble(*m);
    }
    return retval;
}

char *dnsf_ckr_get_mac_by_addr(in_addr_t addr, const char *loiface, const int max_tries) {
    struct dnsf_ckr_ethernet_frame eth;
    struct dnsf_ckr_arp_header arp;
    char *mac, *ip;
    unsigned char *mac_in_bytes, *rawpkt;
    size_t rawpkt_sz;
    int bytes_total;
    dnsf_ckr_sk sk;
    char *buf, *buf_p;
    int blen, ntry = max_tries;
    struct bpf_hdr *bpf_buf, *bpf_ptr;
    unsigned short ether_type;
    struct dnsf_ckr_arp_header *arp_reply;
    struct in_addr excessive_types_in_bsd_sockets_sucks;

    excessive_types_in_bsd_sockets_sucks.s_addr = addr;

    sk = dnsf_ckr_create_arp_socket(loiface);
    if (sk == -1) return NULL;
    eth.payload = NULL;
    memset(&arp, 0, sizeof(struct dnsf_ckr_arp_header));
    memset(eth.dest_hw_addr, 0xff, sizeof(eth.dest_hw_addr));
    mac = dnsf_ckr_get_iface_mac(loiface);
    mac_in_bytes = dnsf_ckr_mac2byte(mac, 6);
    memcpy(eth.src_hw_addr, mac_in_bytes, 6);
    free(mac);
    mac = NULL;
    free(mac_in_bytes);
    eth.ether_type = ETHER_TYPE_ARP;
    arp.hwtype = ARP_HW_TYPE_ETHERNET;
    arp.ptype = ARP_PROTO_TYPE_IP;
    arp.hw_addr_len = 6;
    arp.pt_addr_len = 4;
    arp.opcode = ARP_OPCODE_REQUEST;
    arp.src_hw_addr = (unsigned char *) dnsf_ckr_getmem(arp.hw_addr_len);
    memcpy(arp.src_hw_addr, eth.src_hw_addr, 6);
    ip = dnsf_ckr_get_iface_ip(loiface);
    if (ip == NULL) {
        free(arp.src_hw_addr);
        return NULL;
    }
    arp.src_pt_addr = dnsf_ckr_addr2byte(ip, 4);
    free(ip);
    arp.dest_hw_addr = (unsigned char *) dnsf_ckr_getmem(arp.hw_addr_len);
    memset(arp.dest_hw_addr, 0, arp.hw_addr_len);
    arp.dest_pt_addr = (unsigned char *)&addr;
    eth.payload = dnsf_ckr_mk_arp_dgram(&eth.payload_size, arp);
    rawpkt = dnsf_ckr_mk_ethernet_frame(&rawpkt_sz, eth);
    while (ntry-- > 0 && mac == NULL) {
        bytes_total = write(sk, rawpkt, rawpkt_sz);
        if (bytes_total > 0) {
            blen = get_arp_socket_blen(sk);
            if (blen > 0) {
                bpf_buf = (struct bpf_hdr *) dnsf_ckr_getmem(blen);
                if ((bytes_total = read(sk, bpf_buf, blen)) > 0) {
                    buf = (char *)bpf_buf;
                    while (buf < ((char *)bpf_buf + bytes_total)) {
                        bpf_ptr = (struct bpf_hdr *)buf;
                        buf_p = ((char *)bpf_ptr + bpf_ptr->bh_hdrlen);
                        ether_type = (unsigned short) buf_p[12] << 8 | buf_p[13];
                        if (ether_type == ETHER_TYPE_ARP) {
                            arp_reply = dnsf_ckr_parse_arp_dgram((unsigned char *)&buf_p[14], bpf_ptr->bh_datalen - 14);
                            if (arp_reply != NULL && arp_reply->opcode == ARP_OPCODE_REPLY) {
                                ip = (char *) dnsf_ckr_getmem(20);
                                sprintf(ip, "%d.%d.%d.%d", arp_reply->src_pt_addr[0],
                                                           arp_reply->src_pt_addr[1],
                                                           arp_reply->src_pt_addr[2],
                                                           arp_reply->src_pt_addr[3]);
                                if (strcmp(inet_ntoa(excessive_types_in_bsd_sockets_sucks), ip) == 0) {
                                    mac = (char *) dnsf_ckr_getmem(20);
                                    sprintf(mac, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", arp_reply->src_hw_addr[0],
                                                                                  arp_reply->src_hw_addr[1],
                                                                                  arp_reply->src_hw_addr[2],
                                                                                  arp_reply->src_hw_addr[3],
                                                                                  arp_reply->src_hw_addr[4],
                                                                                  arp_reply->src_hw_addr[5]);
                                }
                                free(ip);
                            }
                            dnsf_ckr_arp_header_free(arp_reply);
                        }
                        buf += BPF_WORDALIGN(bpf_ptr->bh_hdrlen + bpf_ptr->bh_caplen);
                    }
                    free(bpf_buf);
                }
            }
        }
        sleep(1);
    }
    free(rawpkt);
    close(sk);
    free(eth.payload);
    free(arp.src_hw_addr);
    free(arp.src_pt_addr);
    free(arp.dest_hw_addr);
    return mac;
}
