/*
 *  Copyright (C) 2011 - EATON
 *  Copyright (C) 2020-2024 - Jim Klimov <jimklimov+nut@gmail.com>
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

/*! \file nutscan-ip.h
    \brief iterator for IPv4 or IPv6 addresses
    \author Frederic Bohe <fredericbohe@eaton.com>
*/

#ifndef SCAN_IP
#define SCAN_IP

#ifndef WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#else	/* WIN32 */
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#endif	/* WIN32 */

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

enum network_type {
	IPv4,
	IPv6
};

typedef struct nutscan_ip_iter {
	enum network_type	type;
	struct in_addr		start;
	struct in_addr		stop;
	struct in6_addr		start6;
	struct in6_addr		stop6;
} nutscan_ip_iter_t;

char * nutscan_ip_iter_init(nutscan_ip_iter_t *, const char * startIP, const char * stopIP);
char * nutscan_ip_iter_inc(nutscan_ip_iter_t *);
int nutscan_cidr_to_ip(const char * cidr, char ** start_ip, char ** stop_ip);

/* Track requested IP ranges (from CLI or auto-discovery) */
/* One IP address range: */
typedef struct nutscan_ip_range_s {
	char * start_ip;
	char * end_ip;
	struct nutscan_ip_range_s * next;
} nutscan_ip_range_t;

/* List of IP address ranges and helper data: */
typedef struct nutscan_ip_range_list_s {
	nutscan_ip_range_t * ip_ranges;		/* Actual linked list of entries, first entry */
	nutscan_ip_range_t * ip_ranges_last;	/* Pointer to end of list for quicker additions */
	size_t ip_ranges_count;			/* Counter of added entries */
} nutscan_ip_range_list_t;

/* Initialize fields of caller-provided list
 * (can allocate one if arg is NULL - caller
 * must free it later). Does not assume that
 * caller's list values are valid and should
 * be freed (can be some garbage from stack).
 *
 * Returns pointer to the original or allocated list.
 */
nutscan_ip_range_list_t *nutscan_init_ip_ranges(nutscan_ip_range_list_t *irl);

/* Free information from the list (does not
 * free the list object itself, can be static)
 * so it can be further re-used or freed.
 */
void nutscan_free_ip_ranges(nutscan_ip_range_list_t *irl);

/* Prints contents of irl into a groovy-like string,
 * using a static buffer (rewritten by each call) */
const char * nutscan_stringify_ip_ranges(nutscan_ip_range_list_t *irl);

size_t nutscan_add_ip_range(nutscan_ip_range_list_t *irl, char * start_ip, char * end_ip);

/* Iterator over given nutscan_ip_range_list_t structure
 * and the currently pointed-to range in its list.
 * Several iterators may use the same range.
 */
typedef struct nutscan_ip_range_list_iter_s {
	const nutscan_ip_range_list_t * irl;	/* Structure with actual linked list of address-range entries */
	nutscan_ip_range_t * ip_ranges_iter;	/* Helper for iteration: across the list of IP ranges */
	nutscan_ip_iter_t    curr_ip_iter;	/* Helper for iteration: across one currently iterated IP range */
} nutscan_ip_range_list_iter_t;

char * nutscan_ip_ranges_iter_init(nutscan_ip_range_list_iter_t *irliter, const nutscan_ip_range_list_t *irl);
char * nutscan_ip_ranges_iter_inc(nutscan_ip_range_list_iter_t *irliter);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
