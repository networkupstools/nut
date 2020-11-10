/* conf.h - supporting elements of conf parsing functions for upsd

   Copyright (C)
	2001	Russell Kroll <rkroll@exploits.org>
	2008	Arjen de Korte <adkorte-guest@alioth.debian.org>

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

#ifndef NUT_CONF_H_SEEN
#define NUT_CONF_H_SEEN 1

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* read upsd.conf */
void load_upsdconf(int reloading);

/* add valid UPSes from ups.conf to the internal structures */
void upsconf_add(int reloading);

/* flush existing config, then reread everything */
void conf_reload(void);

typedef struct ups_s {
	char	*upsname;
	char	*driver;
	char	*port;
	char	*desc;
	struct ups_s	*next;
} ups_t;

/* used for really clean shutdowns */
void delete_acls(void);
void delete_access(void);

extern	int	num_ups;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* NUT_CONF_H_SEEN */
