/*
 *  Copyright (C) 2011 - EATON
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*! \file nutscan-ip.c
    \brief iterator for IPv4 or IPv6 addresses
    \author Frederic Bohe <fredericbohe@eaton.com>
*/

#include "config.h" /* must be first */

#include "nut_stdint.h"
#include "common.h"
#include "nutscan-ip.h"
#include <stdio.h>
#include <sys/types.h>
#ifndef WIN32
# include <sys/socket.h>
# include <netdb.h>
#else
/* Those 2 files for support of getaddrinfo, getnameinfo and freeaddrinfo
   on Windows 2000 and older versions */
# include <ws2tcpip.h>
# include <wspiapi.h>
# ifndef AI_NUMERICSERV
#  define AI_NUMERICSERV NI_NUMERICSERV
# endif
# include "wincompat.h"
#endif

static void increment_IPv6(struct in6_addr * addr)
{
	int i;

	for (i = 15 ; i >= 0 ; i--) {
		addr->s6_addr[i]++;
		if (addr->s6_addr[i] != 0) {
			break;
		}
	}
}

static void invert_IPv6(struct in6_addr * addr1, struct in6_addr * addr2)
{
	struct in6_addr addr;

	memcpy(addr.s6_addr,   addr1->s6_addr, sizeof(addr.s6_addr));
	memcpy(addr1->s6_addr, addr2->s6_addr, sizeof(addr.s6_addr));
	memcpy(addr2->s6_addr, addr.s6_addr,   sizeof(addr.s6_addr));
}

static int ntop(struct in_addr * ip, char * host, GETNAMEINFO_TYPE_ARG46 host_size)
{
	struct sockaddr_in in;
	memset(&in, 0, sizeof(struct sockaddr_in));
	in.sin_addr = *ip;
	in.sin_family = AF_INET;
	return getnameinfo(
		(struct sockaddr *)&in,
		sizeof(struct sockaddr_in),
		host, host_size, NULL, 0, NI_NUMERICHOST);
}

static int ntop6(struct in6_addr * ip, char * host, GETNAMEINFO_TYPE_ARG46 host_size)
{
	struct sockaddr_in6 in6;
	memset(&in6, 0, sizeof(struct sockaddr_in6));
	memcpy(&in6.sin6_addr, ip, sizeof(struct in6_addr));
	in6.sin6_family = AF_INET6;
	return getnameinfo(
		(struct sockaddr *)&in6,
		sizeof(struct sockaddr_in6),
		host, host_size, NULL, 0, NI_NUMERICHOST);
}

/* Return the first ip or NULL if error */
char * nutscan_ip_iter_init(nutscan_ip_iter_t * ip, const char * startIP, const char * stopIP)
{
	uint32_t addr; /* 32-bit IPv4 address */
	int i;
	struct addrinfo hints;
	struct addrinfo *res;
	struct sockaddr_in * s_in;
	struct sockaddr_in6 * s_in6;
	char host[SMALLBUF];

	if (startIP == NULL) {
		return NULL;
	}

	if (stopIP == NULL) {
		stopIP = startIP;
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;

	ip->type = IPv4;
	/* Detecting IPv4 vs IPv6 */
	if (getaddrinfo(startIP, NULL, &hints, &res) != 0) {
		/*Try IPv6 detection */
		ip->type = IPv6;
		hints.ai_family = AF_INET6;
		if (getaddrinfo(startIP, NULL, &hints, &res) != 0) {
			fprintf(stderr, "Invalid address : %s\n", startIP);
			return NULL;
		}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-align"
#endif
		/* Note: we receive a pointer to res above, so have
		 * no control about alignment of its further data */
		s_in6 = (struct sockaddr_in6 *)res->ai_addr;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic pop
#endif
		memcpy(&ip->start6, &s_in6->sin6_addr, sizeof(struct in6_addr));
		freeaddrinfo(res);
	}
	else {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-align"
#endif
		/* Note: we receive a pointer to res above, so have
		 * no control about alignment of its further data */
		s_in = (struct sockaddr_in *)res->ai_addr;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic pop
#endif
		ip->start = s_in->sin_addr;
		freeaddrinfo(res);
	}

	/* Compute stop IP */
	if (ip->type == IPv4) {
		hints.ai_family = AF_INET;
		if (getaddrinfo(stopIP, NULL, &hints, &res) != 0) {
			fprintf(stderr, "Invalid address : %s\n", stopIP);
			return NULL;
		}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-align"
#endif
		/* Note: we receive a pointer to res above, so have
		 * no control about alignment of its further data */
		s_in = (struct sockaddr_in *)res->ai_addr;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic pop
#endif
		ip->stop = s_in->sin_addr;
		freeaddrinfo(res);
	}
	else {
		hints.ai_family = AF_INET6;
		if (getaddrinfo(stopIP, NULL, &hints, &res) != 0) {
			fprintf(stderr, "Invalid address : %s\n", stopIP);
			return NULL;
		}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-align"
#endif
		/* Note: we receive a pointer to res above, so have
		 * no control about alignment of its further data */
		s_in6 = (struct sockaddr_in6 *)res->ai_addr;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic pop
#endif
		memcpy(&ip->stop6, &s_in6->sin6_addr, sizeof(struct in6_addr));
		freeaddrinfo(res);
	}

	/* Make sure start IP is lesser than stop IP */
	if (ip->type == IPv4) {
		if (ntohl(ip->start.s_addr) > ntohl(ip->stop.s_addr)) {
			addr = ip->start.s_addr;
			ip->start.s_addr = ip->stop.s_addr;
			ip->stop.s_addr = addr;
		}

		if (ntop(&ip->start, host, sizeof(host)) != 0) {
			return NULL;
		}

		return strdup(host);
	}
	else { /* IPv6 */
		for (i = 0; i < 16; i++) {
			if (ip->start6.s6_addr[i] !=ip->stop6.s6_addr[i]) {
				if (ip->start6.s6_addr[i] > ip->stop6.s6_addr[i]) {
					invert_IPv6(&ip->start6, &ip->stop6);
				}
				break;
			}
		}

		if (ntop6(&ip->start6, host, sizeof(host)) != 0) {
			return NULL;
		}

		return strdup(host);
	}

}

/* return the next IP
return NULL if there is no more IP
*/
char * nutscan_ip_iter_inc(nutscan_ip_iter_t * ip)
{
	char host[SMALLBUF];

	if (ip->type == IPv4) {
		/* Check if this is the last address to scan */
		if (ip->start.s_addr == ip->stop.s_addr) {
			return NULL;
		}
		/* increment the address (need to pass address in host
		   byte order, then pass back in network byte order */
		ip->start.s_addr = htonl((ntohl(ip->start.s_addr) + 1));

		if (ntop(&ip->start, host, sizeof(host)) != 0) {
			return NULL;
		}

		return strdup(host);
	}
	else {
		/* Check if this is the last address to scan */
		if (memcmp(&ip->start6.s6_addr, &ip->stop6.s6_addr,
				sizeof(ip->start6.s6_addr)) == 0) {
			return NULL;
		}

		increment_IPv6(&ip->start6);
		if (ntop6(&ip->start6, host, sizeof(host)) != 0) {
			return NULL;
		}

		return strdup(host);
	}
}

int nutscan_cidr_to_ip(const char * cidr, char ** start_ip, char ** stop_ip)
{
	char * cidr_tok;
	char * first_ip;
	char * mask;
	char * saveptr = NULL;
	nutscan_ip_iter_t ip;
	int mask_val;
	int mask_byte;
	int ret;
	uint32_t mask_bit; /* 32-bit IPv4 address bitmask */
	char host[SMALLBUF];
	struct addrinfo hints;
	struct addrinfo *res;
	struct sockaddr_in * s_in;
	struct sockaddr_in6 * s_in6;

	*start_ip = NULL;
	*stop_ip = NULL;
#ifdef WIN32
        WSADATA WSAdata;
#endif

	cidr_tok = strdup(cidr);
	first_ip = strdup(strtok_r(cidr_tok, "/", &saveptr));
	if (first_ip == NULL) {
		upsdebugx(0, "WARNING: %s failed to parse first_ip from cidr=%s",
			__func__, cidr);
		free(cidr_tok);
		return 0;
	}
	mask = strtok_r(NULL, "/", &saveptr);
	if (mask == NULL) {
		upsdebugx(0, "WARNING: %s failed to parse mask from cidr=%s (first_ip=%s)",
			__func__, cidr, first_ip);
		free (first_ip);
		free(cidr_tok);
		return 0;
	}
	upsdebugx(5, "%s: parsed cidr=%s into first_ip=%s and mask=%s",
		__func__, cidr, first_ip, mask);

	mask_val = atoi(mask);
	upsdebugx(5, "%s: parsed mask value %d",
		__func__, mask_val);

	/* NOTE: Sanity-wise, some larger number also makes sense
	 * as the maximum subnet size we would scan. But at least,
	 * this helps avoid scanning the whole Internet just due
	 * to string-parsing errors.
	 */
	if (mask_val < 1) {
		fatalx(EXIT_FAILURE, "Bad netmask: %s", mask);
	}

	/* Note: this freeing invalidates "mask" and "saveptr" pointer targets */
	free(cidr_tok);

	/* Detecting IPv4 vs IPv6 */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;

	ip.type = IPv4;

#ifdef WIN32
        WSAStartup(2,&WSAdata);
        atexit((void(*)(void))WSACleanup);
#endif

	if ((ret = getaddrinfo(first_ip, NULL, &hints, &res)) != 0) {
		/* EAI_ADDRFAMILY? */
		upsdebugx(5, "%s: getaddrinfo() failed for AF_INET (IPv4): %d",
			__func__, ret);

		/* Try IPv6 detection */
		ip.type = IPv6;
		hints.ai_family = AF_INET6;
		if ((ret = getaddrinfo(first_ip, NULL, &hints, &res)) != 0) {
			upsdebugx(5, "%s: getaddrinfo() failed for AF_INET6 (IPv6): %d",
				__func__, ret);
			free(first_ip);
			return 0;
		}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-align"
#endif
		/* Note: we receive a pointer to res above, so have
		 * no control about alignment of its further data */
		s_in6 = (struct sockaddr_in6 *)res->ai_addr;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic pop
#endif
		memcpy(&ip.start6, &s_in6->sin6_addr, sizeof(struct in6_addr));
		freeaddrinfo(res);
	}
	else {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-align"
#endif
		/* Note: we receive a pointer to res above, so have
		 * no control about alignment of its further data */
		s_in = (struct sockaddr_in *)res->ai_addr;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic pop
#endif
		ip.start = s_in->sin_addr;
		freeaddrinfo(res);
	}

	if (ip.type == IPv4) {

		if (mask_val > 0) {
			mask_val --;
			mask_bit = 0x80000000;
			mask_bit >>= mask_val;
			mask_bit--;
		}
		else {
			mask_bit = 0xffffffff;
		}
		ip.stop.s_addr = htonl(ntohl(ip.start.s_addr)|mask_bit);
		ip.start.s_addr = htonl(ntohl(ip.start.s_addr)&(~mask_bit));

		if (ntop(&ip.start, host, sizeof(host)) != 0) {
			*start_ip = NULL;
			*stop_ip = NULL;
			return 0;
		}
		*start_ip = strdup(host);

		if (ntop(&ip.stop, host, sizeof(host)) != 0) {
			free(*start_ip);
			*start_ip = NULL;
			*stop_ip = NULL;
			return 0;
		}
		*stop_ip = strdup(host);

		free(first_ip);
		return 1;
	}
	else {
		if (getaddrinfo(first_ip, NULL, &hints, &res) != 0) {
			return 0;
		}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-align"
#endif
		/* Note: we receive a pointer to res above, so have
		 * no control about alignment of its further data */
		s_in6 = (struct sockaddr_in6 *)res->ai_addr;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic pop
#endif
		memcpy(&ip.stop6, &s_in6->sin6_addr, sizeof(struct in6_addr));
		freeaddrinfo(res);

		mask_byte = mask_val / 8;
		if (mask_byte < 16 && mask_byte >= 0) {
			memset(&(ip.stop6.s6_addr[mask_byte + 1]), 0xFF, 15 - (uint8_t)mask_byte);
			memset(&(ip.start6.s6_addr[mask_byte + 1]), 0x00, 15 - (uint8_t)mask_byte);

			mask_bit = (0x100 >> mask_val%8) - 1;
			ip.stop6.s6_addr[mask_byte] |= mask_bit;
			ip.start6.s6_addr[mask_byte] &= (~mask_bit);
		}

		if (ntop6(&ip.start6, host, sizeof(host)) != 0) {
			*start_ip = NULL;
			*stop_ip = NULL;
			return 0;
		}
		*start_ip = strdup(host);

		if (ntop6(&ip.stop6, host, sizeof(host)) != 0) {
			free(*start_ip);
			*start_ip = NULL;
			*stop_ip = NULL;
			return 0;
		}
		*stop_ip = strdup(host);
	}

	free(first_ip);
	return 1;
}
