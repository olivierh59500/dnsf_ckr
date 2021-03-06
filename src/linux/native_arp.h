/*
 *                              Copyright (C) 2014 by Rafael Santiago
 *
 * This is free software. You can redistribute it and/or modify under
 * the terms of the GNU General Public License version 2.
 *
 */
#ifndef _DNSF_CKR_NATIVE_ARP_H
#define _DNSF_CKR_NATIVE_ARP_H 1

#include <netinet/in.h>

char *dnsf_ckr_get_mac_by_addr(in_addr_t addr, const char *loiface, const int max_tries);

#endif
