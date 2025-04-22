/*
 *  Copyright (C) 2011 - EATON
 *  Copyright (C) 2022 - 2024 Jim Klimov <jimklimov+nut@gmail.com>
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
#else	/* WIN32 */
/* Those 2 files for support of getaddrinfo, getnameinfo and freeaddrinfo
   on Windows 2000 and older versions */
# include <ws2tcpip.h>
# include <wspiapi.h>
# ifndef AI_NUMERICSERV
#  define AI_NUMERICSERV NI_NUMERICSERV
# endif
# include "wincompat.h"
#endif	/* WIN32 */

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
	struct sockaddr_in in4;
	memset(&in4, 0, sizeof(struct sockaddr_in));
	in4.sin_addr = *ip;
	in4.sin_family = AF_INET;
	return getnameinfo(
		(struct sockaddr *)&in4,
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

/* Track requested IP ranges (from CLI or auto-discovery) */
nutscan_ip_range_list_t *nutscan_init_ip_ranges(nutscan_ip_range_list_t *irl)
{
	if (!irl) {
		irl = (nutscan_ip_range_list_t *)xcalloc(1, sizeof(nutscan_ip_range_list_t));
	}

	irl->ip_ranges = NULL;
	irl->ip_ranges_last = NULL;
	irl->ip_ranges_count = 0;

	return irl;
}

void nutscan_free_ip_ranges(nutscan_ip_range_list_t *irl)
{
	nutscan_ip_range_t *p;

	if (!irl) {
		upsdebugx(5, "%s: skip, no nutscan_ip_range_list_t was specified", __func__);
		return;
	}

	p = irl->ip_ranges;
	while (p) {
		irl->ip_ranges = p->next;

		/* Only free the strings once, if they pointed to same */
		if (p->start_ip == p->end_ip && p->start_ip) {
			free(p->start_ip);
		} else {
			if (p->start_ip)
				free(p->start_ip);
			if (p->end_ip)
				free(p->end_ip);
		}

		free(p);
		p = irl->ip_ranges;
	}

	irl->ip_ranges_last = NULL;
	irl->ip_ranges_count = 0;
}

size_t nutscan_add_ip_range(nutscan_ip_range_list_t *irl, char * start_ip, char * end_ip)
{
	nutscan_ip_range_t *p;

	if (!irl) {
		upsdebugx(5, "%s: skip, no nutscan_ip_range_list_t was specified", __func__);
		return 0;
	}

	if (!start_ip && !end_ip) {
		upsdebugx(5, "%s: skip, no addresses were provided", __func__);
		return irl->ip_ranges_count;
	}

	if (start_ip == NULL) {
		upsdebugx(5, "%s: only end address was provided, setting start to same: %s",
			 __func__, end_ip);
		start_ip = end_ip;
	}
	if (end_ip == NULL) {
		upsdebugx(5, "%s: only start address was provided, setting end to same: %s",
			 __func__, start_ip);
		end_ip = start_ip;
	}

	p = xcalloc(1, sizeof(nutscan_ip_range_t));

	if (start_ip == end_ip || strcmp(start_ip, end_ip) <= 0) {
		p->start_ip = start_ip;
		p->end_ip = end_ip;
	} else {
		p->start_ip = end_ip;
		p->end_ip = start_ip;
	}

	p->next = NULL;

	if (!irl->ip_ranges) {
		/* First entry */
		irl->ip_ranges = p;
	}

	if (irl->ip_ranges_last) {
		/* Got earlier entries, promote the tail */
		irl->ip_ranges_last->next = p;
	}

	irl->ip_ranges_last = p;
	irl->ip_ranges_count++;

	upsdebugx(1, "Recorded IP address range #%" PRIuSIZE ": [%s .. %s]",
		irl->ip_ranges_count, start_ip, end_ip);

	return irl->ip_ranges_count;
}

const char * nutscan_stringify_ip_ranges(nutscan_ip_range_list_t *irl)
{
	static char buf[LARGEBUF - 64];	/* Leave some space for upsdebugx() prefixes */
	size_t	len = 0;

	memset(buf, 0, sizeof(buf));
	len += snprintf(buf + len, sizeof(buf) - len,
		"(%" PRIuSIZE ")[",
		(irl ? irl->ip_ranges_count : 0));

	if (irl && irl->ip_ranges && irl->ip_ranges_count) {
		nutscan_ip_range_t	*p;
		size_t	j;

		for (
			j = 0, p = irl->ip_ranges;
			p && len < sizeof(buf) - 6;
			p = p->next, j++
		 ) {
			if (j) {
				buf[len++] = ',';
				buf[len++] = ' ';
			}

			if (len > sizeof(buf) - 6) {
				/* Too little left, but enough for this */
				buf[len++] = '.';
				buf[len++] = '.';
				buf[len++] = '.';
				break;
			}

			if (p->start_ip == p->end_ip || !strcmp(p->start_ip, p->end_ip)) {
				len += snprintf(buf + len, sizeof(buf) - len,
					"%s", p->start_ip);
			} else {
				len += snprintf(buf + len, sizeof(buf) - len,
					"%s .. %s", p->start_ip, p->end_ip);
			}
		}
	}

	if (len < sizeof(buf) - 1)
		buf[len++] = ']';

	return buf;
}

/* Return the first ip or NULL if error */
char * nutscan_ip_ranges_iter_init(nutscan_ip_range_list_iter_t *irliter, const nutscan_ip_range_list_t *irl)
{
	char	*ip_str;

	if (!irliter) {
		upsdebugx(5, "%s: skip, no nutscan_ip_range_list_iter_t was specified", __func__);
		return NULL;
	}

	if (!irl) {
		upsdebugx(5, "%s: skip, no nutscan_ip_range_list_t was specified", __func__);
		return NULL;
	}

	if (!irl->ip_ranges) {
		upsdebugx(5, "%s: skip, empty nutscan_ip_range_list_t was specified", __func__);
		return NULL;
	}

	memset(irliter, 0, sizeof(nutscan_ip_range_list_iter_t));
	irliter->irl = irl;
	irliter->ip_ranges_iter = irl->ip_ranges;
	memset(&(irliter->curr_ip_iter), 0, sizeof(nutscan_ip_iter_t));

	upsdebugx(4, "%s: beginning iteration with first IP range [%s .. %s]",
		__func__, irliter->ip_ranges_iter->start_ip,
		irliter->ip_ranges_iter->end_ip);

	ip_str = nutscan_ip_iter_init(
		&(irliter->curr_ip_iter),
		irliter->ip_ranges_iter->start_ip,
		irliter->ip_ranges_iter->end_ip);

	upsdebugx(5, "%s: got IP from range: %s", __func__, NUT_STRARG(ip_str));
	return ip_str;
}

/* return the next IP
 * return NULL if there is no more IP
 */
char * nutscan_ip_ranges_iter_inc(nutscan_ip_range_list_iter_t *irliter)
{
	char	*ip_str;

	if (!irliter) {
		upsdebugx(5, "%s: skip, no nutscan_ip_range_list_iter_t was specified", __func__);
		return NULL;
	}

	if (!irliter->irl) {
		upsdebugx(5, "%s: skip, no nutscan_ip_range_list_t was specified", __func__);
		return NULL;
	}

	if (!irliter->irl->ip_ranges) {
		upsdebugx(5, "%s: skip, empty nutscan_ip_range_list_t was specified", __func__);
		return NULL;
	}

	if (!irliter->ip_ranges_iter) {
		upsdebugx(5, "%s: skip, finished nutscan_ip_range_list_t was specified", __func__);
		return NULL;
	}

	ip_str = nutscan_ip_iter_inc(&(irliter->curr_ip_iter));

	if (ip_str) {
		upsdebugx(5, "%s: got IP from range: %s", __func__, NUT_STRARG(ip_str));
		return ip_str;
	}

	upsdebugx(5, "%s: end of IP range [%s .. %s]",
		__func__, irliter->ip_ranges_iter->start_ip,
		irliter->ip_ranges_iter->end_ip);

	/* else: end of one range, try to switch to next */
	irliter->ip_ranges_iter = irliter->ip_ranges_iter->next;
	if (!(irliter->ip_ranges_iter)) {
		upsdebugx(5, "%s: end of whole IP range list", __func__);
		return NULL;
	}

	memset(&(irliter->curr_ip_iter), 0, sizeof(nutscan_ip_iter_t));
	upsdebugx(4, "%s: beginning iteration with next IP range [%s .. %s]",
		__func__, irliter->ip_ranges_iter->start_ip,
		irliter->ip_ranges_iter->end_ip);

	ip_str = nutscan_ip_iter_init(
		&(irliter->curr_ip_iter),
		irliter->ip_ranges_iter->start_ip,
		irliter->ip_ranges_iter->end_ip);

	upsdebugx(5, "%s: got IP from range: %s", __func__, NUT_STRARG(ip_str));
	return ip_str;
}

/* Return the first ip or NULL if error */
char * nutscan_ip_iter_init(nutscan_ip_iter_t * ip, const char * startIP, const char * stopIP)
{
	uint32_t addr; /* 32-bit IPv4 address */
	int i;
	struct addrinfo hints;
	struct addrinfo *res;
	char host[SMALLBUF];

	/* Ensure proper alignment of IPvN structure fields:
	 * we receive a pointer to res from getaddrinfo() et al,
	 * so have no control about alignment of its further data.
	 * Make a copy of the bytes into an object allocated
	 * whichever way the system likes it.
	 */
	struct sockaddr_in	s_in4buf, *s_in4 = &s_in4buf;
	struct sockaddr_in6	s_in6buf, *s_in6 = &s_in6buf;

	if (startIP == NULL) {
		return NULL;
	}

	if (stopIP == NULL) {
		stopIP = startIP;
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;

	ip->type = IPv4;
	/* Detecting IPv4 vs. IPv6 */
	if (getaddrinfo(startIP, NULL, &hints, &res) != 0) {
		/*Try IPv6 detection */
		ip->type = IPv6;
		hints.ai_family = AF_INET6;
		if (getaddrinfo(startIP, NULL, &hints, &res) != 0) {
			upsdebugx(0, "WARNING: %s: Invalid address : %s",
				__func__, startIP);
			return NULL;
		}

		memcpy(s_in6, res->ai_addr, sizeof(struct sockaddr_in6));
		memcpy(&ip->start6, &s_in6->sin6_addr, sizeof(struct in6_addr));
		freeaddrinfo(res);
	}
	else {
		memcpy(s_in4, res->ai_addr, sizeof(struct sockaddr_in));
		ip->start = s_in4->sin_addr;
		freeaddrinfo(res);
	}

	/* Compute stop IP */
	if (ip->type == IPv4) {
		hints.ai_family = AF_INET;
		if (getaddrinfo(stopIP, NULL, &hints, &res) != 0) {
			upsdebugx(0, "WARNING: %s: Invalid address : %s",
				__func__, stopIP);
			return NULL;
		}

		memcpy(s_in4, res->ai_addr, sizeof(struct sockaddr_in));
		ip->stop = s_in4->sin_addr;
		freeaddrinfo(res);
	}
	else {
		hints.ai_family = AF_INET6;
		if (getaddrinfo(stopIP, NULL, &hints, &res) != 0) {
			upsdebugx(0, "WARNING: %s: Invalid address : %s",
				__func__, stopIP);
			return NULL;
		}
		memcpy(s_in6, res->ai_addr, sizeof(struct sockaddr_in6));
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
		size_t	hlen;
		for (i = 0; i < 16; i++) {
			if (ip->start6.s6_addr[i] !=ip->stop6.s6_addr[i]) {
				if (ip->start6.s6_addr[i] > ip->stop6.s6_addr[i]) {
					invert_IPv6(&ip->start6, &ip->stop6);
				}
				break;
			}
		}

		/* IPv6 addresses must be in square brackets,
		 * for upsclient et al to differentiate them
		 * from the port number.
		 */
		host[0] = '[';
		if (ntop6(&ip->start6, host + 1, sizeof(host) - 2) != 0) {
			return NULL;
		}
		hlen = strlen(host);
		host[hlen] = ']';
		host[hlen + 1] = '\0';

		return strdup(host);
	}

}

/* return the next IP
 * return NULL if there is no more IP
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
		size_t	hlen;

		/* Check if this is the last address to scan */
		if (memcmp(&ip->start6.s6_addr, &ip->stop6.s6_addr,
				sizeof(ip->start6.s6_addr)) == 0) {
			return NULL;
		}

		increment_IPv6(&ip->start6);

		/* IPv6 addresses must be in square brackets,
		 * for upsclient et al to differentiate them
		 * from the port number.
		 */
		host[0] = '[';
		if (ntop6(&ip->start6, host + 1, sizeof(host) - 2) != 0) {
			return NULL;
		}
		hlen = strlen(host);
		host[hlen] = ']';
		host[hlen + 1] = '\0';

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
	int ret;
	int mask_val;	/* mask length in bits */
	int mask_byte;	/* number of byte in 128-bit IPv6 address array (uint8_t[16]) for netmask */
	uint32_t mask_bit; /* 32-bit IPv4 address bitmask value */
	char host[SMALLBUF];
	struct addrinfo hints;
	struct addrinfo *res;

	/* Ensure proper alignment of IPvN structure fields:
	 * we receive a pointer to res from getaddrinfo() et al,
	 * so have no control about alignment of its further data.
	 * Make a copy of the bytes into an object allocated
	 * whichever way the system likes it.
	 */
	struct sockaddr_in	s_in4buf, *s_in4 = &s_in4buf;
	struct sockaddr_in6	s_in6buf, *s_in6 = &s_in6buf;

	*start_ip = NULL;
	*stop_ip = NULL;

	if (!cidr) {
		upsdebugx(0, "WARNING: %s: null cidr pointer was provided",
			__func__);
		return 0;
	}

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
		free(first_ip);
		free(cidr_tok);
		return 0;
	}

	if (first_ip[0] == '[' && first_ip[strlen(first_ip) - 1] == ']') {
		char *s = strdup(first_ip + 1);
		s[strlen(s) - 1] = '\0';
		free(first_ip);
		first_ip = s;
	}

	upsdebugx(5, "%s: parsed cidr=%s into first_ip=%s and mask=%s",
		__func__, cidr, first_ip, mask);

	/* TODO: check if mask is also an IP address or a bit count */
	mask_val = atoi(mask);
	upsdebugx(5, "%s: parsed mask into numeric value %d",
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

	/* Detecting IPv4 vs. IPv6 */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;

	ip.type = IPv4;

	if ((ret = getaddrinfo(first_ip, NULL, &hints, &res)) != 0) {
		/* EAI_ADDRFAMILY? */
		upsdebugx(5, "%s: getaddrinfo() failed for AF_INET (IPv4, will retry with IPv6): %d: %s",
			__func__, ret, gai_strerror(ret));

		/* Try IPv6 detection */
		ip.type = IPv6;
		hints.ai_family = AF_INET6;
		if ((ret = getaddrinfo(first_ip, NULL, &hints, &res)) != 0) {
			upsdebugx(5, "%s: getaddrinfo() failed for AF_INET6 (IPv6): %d: %s",
				__func__, ret, gai_strerror(ret));
			free(first_ip);
			return 0;
		}

		memcpy(s_in6, res->ai_addr, sizeof(struct sockaddr_in6));
		memcpy(&ip.start6, &s_in6->sin6_addr, sizeof(struct in6_addr));
		freeaddrinfo(res);
	}
	else {
		memcpy(s_in4, res->ai_addr, sizeof(struct sockaddr_in));
		ip.start = s_in4->sin_addr;
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
	else {	/* ip.type == IPv6 */
		if (getaddrinfo(first_ip, NULL, &hints, &res) != 0) {
			return 0;
		}

		memcpy(s_in6, res->ai_addr, sizeof(struct sockaddr_in6));
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
