/* nut_ctype.h - client data definitions for upsd

   Copyright (C)
	2002	Russell Kroll <rkroll@exploits.org>
	2008	Arjen de Korte <adkorte-guest@alioth.debian.org>
	2011	Arnaud Quette <arnaud.quette@free.fr>

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

#ifndef NUT_CTYPE_H_SEEN
#define NUT_CTYPE_H_SEEN 1

/* Mozilla NSS */
#ifdef WITH_NSS
	#include <nss.h>
	#include <ssl.h>
#endif

/* OpenSSL */
#ifdef WITH_OPENSSL
	#include <openssl/err.h>
	#include <openssl/ssl.h>
#endif

#include "parseconf.h"

/* client structure */
typedef struct nut_ctype_s {
	char	*addr;
	int	sock_fd;
	time_t	last_heard;
	char	*loginups;
	char	*password;
	char	*username;
	/* per client status info for commands and settings
	 * (disabled by default) */
	int	tracking;

#ifdef	WITH_OPENSSL
	SSL	*ssl;
#elif defined(WITH_NSS)
	PRFileDesc	*ssl;
#else
	void *ssl;
#endif
	int	ssl_connected;

	PCONF_CTX_t	ctx;

	/* doubly linked list */
	struct nut_ctype_s	*prev;
	struct nut_ctype_s	*next;
} nut_ctype_t;

#endif	/* NUT_CTYPE_H_SEEN */
