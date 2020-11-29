/* sstate.h - Network UPS Tools server-side state management

   Copyright (C)
	2003	Russell Kroll <rkroll@exploits.org>
	2008	Arjen de Korte <adkorte-guest@alioth.debian.org>
	2012	Arnaud Quette <arnaud.quette@free.fr>

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

#ifndef NUT_SSTATE_H_SEEN
#define NUT_SSTATE_H_SEEN 1

#include "state.h"
#include "upstype.h"

#define SS_CONNFAIL_INT 300	/* complain about a dead driver every 5 mins */
#define SS_MAX_READ 256		/* don't let drivers tie us up in read()     */

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

int sstate_connect(upstype_t *ups);
void sstate_disconnect(upstype_t *ups);
void sstate_readline(upstype_t *ups);
const char *sstate_getinfo(const upstype_t *ups, const char *var);
int sstate_getflags(const upstype_t *ups, const char *var);
int sstate_getaux(const upstype_t *ups, const char *var);
const enum_t *sstate_getenumlist(const upstype_t *ups, const char *var);
const range_t *sstate_getrangelist(const upstype_t *ups, const char *var);
const cmdlist_t *sstate_getcmdlist(const upstype_t *ups);
void sstate_makeinfolist(const upstype_t *ups, char *buf, size_t bufsize);
void sstate_makerwlist(const upstype_t *ups, char *buf, size_t bufsize);
void sstate_makeinstcmdlist_t(const upstype_t *ups, char *buf, size_t bufsize);
int sstate_dead(upstype_t *ups, int maxage);
void sstate_infofree(upstype_t *ups);
void sstate_cmdfree(upstype_t *ups);
int sstate_sendline(upstype_t *ups, const char *buf);
const st_tree_t *sstate_getnode(const upstype_t *ups, const char *varname);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* NUT_SSTATE_H_SEEN */
