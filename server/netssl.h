/* netssl.h - ssl support prototypes for upsd

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

#ifndef NUT_NETSSL_H_SEEN
#define NUT_NETSSL_H_SEEN 1

#include "nut_ctype.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

extern char	*certfile;
extern char	*certname;
extern char	*certpasswd;
#ifdef WITH_CLIENT_CERTIFICATE_VALIDATION
extern int certrequest;
#endif /* WITH_CLIENT_CERTIFICATE_VALIDATION */

/* List possible values for certrequested */
/* No request */
#define NETSSL_CERTREQ_NO		0
/* Requested (cnx failed if no certificate) */
#define NETSSL_CERTREQ_REQUEST	1
/* Required (cnx failed if no certificate or invalid CA chain) */
#define NETSSL_CERTREQ_REQUIRE	2


void ssl_init(void);
void ssl_finish(nut_ctype_t *client);
void ssl_cleanup(void);

int ssl_read(nut_ctype_t *client, char *buf, size_t buflen);
int ssl_write(nut_ctype_t *client, const char *buf, size_t buflen);

void net_starttls(nut_ctype_t *client, int numarg, const char **arg);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* NUT_NETSSL_H_SEEN */
