/* obs-outputs' POSIX socket headers for net-if.h (#851). */

#pragma once

#ifdef __linux__
#include <linux/if_link.h>
#elif defined(__FreeBSD__)
#include <netinet/in.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#define __NET_IF_GNU_SOURCE__
#endif //_GNU_SOURCE
#endif //__FreeBSD__

#include <ifaddrs.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#ifdef __FreeBSD__
#ifdef ___NET_IF_GNU_SOURCE__
#undef ___NET_IF_GNU_SOURCE__
#undef _GNU_SOURCE
#endif
#endif
