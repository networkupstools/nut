/* upsd.h - support structures and other minor details

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/*
 * Much of the content from here was also useful to the
 * drivers, so has been moved into include/shared-tables.h
 * instead of being within the daemon specific include file
 *
 */

#ifndef UPSD_H_SEEN
#define UPSD_H_SEEN

#include "attribute.h"

#include "common.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "timehead.h"

#include <sys/file.h>

#include "parseconf.h"
#include "ctype.h"
#include "upstype.h"

#define NUT_NET_ANSWER_MAX SMALLBUF

/* prototypes from upsd.c */

upstype_t *get_ups_ptr(const char *upsname);
int ups_available(const upstype_t *ups, ctype_t *client);

void listen_add(const char *addr, const char *port);

void kick_login_clients(const char *upsname);
int sendback(ctype_t *client, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));
int send_err(ctype_t *client, const char *errtype);

void server_load(void);
void server_free(void);

void check_perms(const char *fn);

/* declarations from upsd.c */

extern int		maxage, maxconn;
extern char		*statepath, *datapath;
extern upstype_t	*firstups;

/* map commands onto signals */

#define SIGCMD_STOP	SIGTERM
#define SIGCMD_RELOAD	SIGHUP

/* awkward way to make a string out of a numeric constant */

#define string_const_aux(x)	#x
#define string_const(x)		string_const_aux(x)

#ifdef SHUT_RDWR
#define shutdown_how SHUT_RDWR
#else
#define shutdown_how 2
#endif

#endif	/* UPSD_H_SEEN */
