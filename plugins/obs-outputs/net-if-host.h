/* obs-outputs' network interface host interface (obs-rust/obs-studio#851):
 * address parsing and printing and the interface list, per host. */

#pragma once

#include "plugins/obs-outputs/net-if.h"

void netif_host_addr_to_string(char *dest, struct sockaddr_storage *byte_address);
bool netif_host_str_to_addr(struct sockaddr_storage *out, int *addr_len, const char *addr, bool ipv6);
void netif_host_get_addrs(struct netif_saddr_data *ifaddrs);

/* Shared (net-if.c): add one interface address to the list. */
void netif_push(struct sockaddr *copy_source, struct netif_saddr_data *saddr_d, const char *adapter);
