/* upstype.h - internal UPS tracking structure details

   Copyright (C)
	2003	Russell Kroll <rkroll@exploits.org>
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

#ifndef NUT_UPSTYPE_H_SEEN
#define NUT_UPSTYPE_H_SEEN 1

#include "parseconf.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* structure for the linked list of each UPS that we track */
typedef struct upstype_s {
	char			*name;
	char			*fn;
	char			*desc;

	int			sock_fd;
	int			stale;
	int			dumpdone;
	int			data_ok;
	time_t			last_heard;
	time_t			last_ping;
	time_t			last_connfail;
	PCONF_CTX_t		sock_ctx;
	struct st_tree_s	*inforoot;
	struct cmdlist_s	*cmdlist;

	int	numlogins;
	int	fsd;		/* forced shutdown in effect? */

	int	retain;

	struct upstype_s	*next;

} upstype_t;

extern upstype_t	*firstups;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* NUT_UPSTYPE_H_SEEN */
