/* sstate.h - Network UPS Tools server-side state management

   Copyright (C) 2003  Russell Kroll <rkroll@exploits.org>

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

#include "upstype.h"

#define SS_CONNFAIL_INT 300	/* complain about a dead driver every 5 mins */
#define SS_MAX_READ 256		/* don't let drivers tie us up in read()     */

int sstate_connect(upstype *ups);
void sstate_sock_read(upstype *ups);
const char *sstate_getinfo(const upstype *ups, const char *var);
int sstate_getflags(const upstype *ups, const char *var);
int sstate_getaux(const upstype *ups, const char *var);
const struct enum_t *sstate_getenumlist(const upstype *ups, const char *var);
const struct cmdlist_t *sstate_getcmdlist(const upstype *ups);
void sstate_makeinfolist(const upstype *ups, char *buf, size_t bufsize);
void sstate_makerwlist(const upstype *ups, char *buf, size_t bufsize);
void sstate_makeinstcmdlist(const upstype *ups, char *buf, size_t bufsize);
int sstate_dead(upstype *ups, int maxage);
void sstate_infofree(upstype *ups);
void sstate_cmdfree(upstype *ups);
int sstate_sendline(upstype *ups, const char *buf);
const struct st_tree_t *sstate_getnode(const upstype *ups, const char *varname);

