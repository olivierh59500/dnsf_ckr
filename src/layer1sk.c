#include "layer1sk.h"

dnsf_ckr_sk dnsf_ckr_create_layer1_socket(const char *iface) {
#if DNSF_CKR_TGT_OS == DNSF_CKR_PLATFORM_FREEBSD
    return dnsf_ckr_create_fbsdl1sk(iface);
#endif
    return -1;
}

void dnsf_ckr_close_layer1_socket(const dnsf_ckr_sk socket, const char *iface) {
#if DNSF_CKR_TGT_OS == DNSF_CKR_PLATFORM_FREEBSD
    dnsf_ckr_close_fbsdl1sk(socket, iface);
#endif
}