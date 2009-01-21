/* ssl.h - ssl support prototypes for upsd

   Copyright (C) 2002  Russell Kroll <rkroll@exploits.org>

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

#ifndef SSL_H_SEEN
#define SSL_H_SEEN 1

#ifdef HAVE_SSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#include "ctype.h"

extern char	*certfile;

void ssl_init(void);
void ssl_finish(ctype_t *client);

int ssl_read(ctype_t *client, char *buf, size_t buflen);
int ssl_write(ctype_t *client, const char *buf, size_t buflen);

void net_starttls(ctype_t *client, int numarg, const char **arg);

#endif	/* SSL_H_SEEN */
