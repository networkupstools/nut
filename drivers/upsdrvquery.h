/* upsdrvquery.h - a single query shot over a driver socket,
                   tracked until a response arrives, returning
                   that line and closing a connection

   Copyright (C) 2023  Jim Klimov <jimklimov+nut@gmail.com>

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

#ifndef NUT_UPSDRVQUERY_H_SEEN
#define NUT_UPSDRVQUERY_H_SEEN

#include "common.h"	/* TYPE_FD etc. */
#include "timehead.h"

typedef struct udq_pipe_conn_s {
	TYPE_FD		sockfd;
#ifdef WIN32
	OVERLAPPED	overlapped;
	int		newread;	/* Set to 1 to start a new ReadFile, forget old buf */
#endif	/* WIN32 */
	char		buf[LARGEBUF];
	char		sockfn[LARGEBUF];
} udq_pipe_conn_t;

udq_pipe_conn_t *upsdrvquery_connect(const char *sockfn);
udq_pipe_conn_t *upsdrvquery_connect_drvname_upsname(const char *drvname, const char *upsname);
void upsdrvquery_close(udq_pipe_conn_t *conn);

ssize_t upsdrvquery_read_timeout(udq_pipe_conn_t *conn, struct timeval tv);
ssize_t upsdrvquery_write(udq_pipe_conn_t *conn, const char *buf);

ssize_t upsdrvquery_prepare(udq_pipe_conn_t *conn, struct timeval tv);
ssize_t upsdrvquery_request(udq_pipe_conn_t *conn, struct timeval tv, const char *query);

/* if buf != NULL, last reply is copied there */
ssize_t upsdrvquery_oneshot(const char *drvname, const char *upsname, const char *query, char *buf, const size_t bufsz, struct timeval *tv);

#endif	/* NUT_UPSDRVQUERY_H_SEEN */
