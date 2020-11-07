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

/*! \file nutscan-ip.h
    \brief iterator for IPv4 or IPv6 addresses
    \author Frederic Bohe <fredericbohe@eaton.com>
*/

#ifndef SCAN_IP
#define SCAN_IP

#include <arpa/inet.h>
#include <netinet/in.h>

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

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
