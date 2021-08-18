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
#include "nut_ctype.h"
#include "upstype.h"

#define NUT_NET_ANSWER_MAX SMALLBUF

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* prototypes from upsd.c */

upstype_t *get_ups_ptr(const char *upsname);
int ups_available(const upstype_t *ups, nut_ctype_t *client);

void listen_add(const char *addr, const char *port);

void kick_login_clients(const char *upsname);
int sendback(nut_ctype_t *client, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));
int send_err(nut_ctype_t *client, const char *errtype);

void server_load(void);
void server_free(void);

void check_perms(const char *fn);

/* return values for instcmd / setvar status tracking,
 * mapped on drivers/upshandler.h, apart from STAT_PENDING (initial state) */
enum {
   STAT_PENDING = -1,	/* not yet completed */
   STAT_HANDLED = 0,	/* completed successfully (NUT_SUCCESS or "OK") */
   STAT_UNKNOWN,	/* unspecified error (NUT_ERR_UNKNOWN) */
   STAT_INVALID,	/* invalid command/setvar (NUT_ERR_INVALID_ARGUMENT) */
   STAT_FAILED		/* command/setvar failed (NUT_ERR_INSTCMD_FAILED / NUT_ERR_SET_FAILED) */
};

/* Commands and settings status tracking functions */
int tracking_add(const char *id);
int tracking_set(const char *id, const char *value);
int tracking_del(const char *id);
void tracking_free(void);
void tracking_cleanup(void);
char *tracking_get(const char *id);
int tracking_enable(void);
int tracking_disable(void);
int tracking_is_enabled(void);

/* declarations from upsd.c */

extern int		maxage, maxconn, tracking_delay;
extern char		*statepath, *datapath;
extern upstype_t	*firstups;
extern nut_ctype_t	*firstclient;

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

/* UUID v4 generation function
 * Note: 'dest' must be at least `UUID4_LEN` long */
int nut_uuid_v4(char *uuid_str);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* UPSD_H_SEEN */
