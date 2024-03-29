/* state.h - Network UPS Tools common state management functions

   Copyright (C)
	2003	Russell Kroll <rkroll@exploits.org>
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

#ifndef NUT_STATE_H_SEEN
#define NUT_STATE_H_SEEN 1

#include "extstate.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define ST_SOCK_BUF_LEN 512

#include "timehead.h"

#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_MONOTONIC) && HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC
typedef struct timespec	st_tree_timespec_t;
#else
typedef struct timeval	st_tree_timespec_t;
#endif

typedef struct st_tree_s {
	char	*var;
	char	*val;			/* points to raw or safe */

	char	*raw;			/* raw data from caller */
	size_t	rawsize;

	char	*safe;			/* safe data from pconf_encode */
	size_t	safesize;

	int	flags;
	long	aux;

	/* When was this entry last written (meaning that
	 * val/raw/safe, flags, aux, enum or range value
	 * was added, changed or deleted)?
	 */
	st_tree_timespec_t	lastset;

	struct enum_s		*enum_list;
	struct range_s		*range_list;

	struct st_tree_s	*left;
	struct st_tree_s	*right;
} st_tree_t;

int state_get_timestamp(st_tree_timespec_t *now);
int st_tree_node_compare_timestamp(const st_tree_t *node, const st_tree_timespec_t *cutoff);
int state_setinfo(st_tree_t **nptr, const char *var, const char *val);
int state_addenum(st_tree_t *root, const char *var, const char *val);
int state_addrange(st_tree_t *root, const char *var, const int min, const int max);
int state_setaux(st_tree_t *root, const char *var, const char *auxs);
const char *state_getinfo(st_tree_t *root, const char *var);
int state_getflags(st_tree_t *root, const char *var);
long state_getaux(st_tree_t *root, const char *var);
const enum_t *state_getenumlist(st_tree_t *root, const char *var);
const range_t *state_getrangelist(st_tree_t *root, const char *var);
void state_setflags(st_tree_t *root, const char *var, size_t numflags, char **flags);
int state_addcmd(cmdlist_t **list, const char *cmd);
void state_infofree(st_tree_t *node);
void state_cmdfree(cmdlist_t *list);
int state_delcmd(cmdlist_t **list, const char *cmd);
int state_delinfo(st_tree_t **root, const char *var);
int state_delinfo_olderthan(st_tree_t **root, const char *var, const st_tree_timespec_t *cutoff);
int state_delenum(st_tree_t *root, const char *var, const char *val);
int state_delrange(st_tree_t *root, const char *var, const int min, const int max);
st_tree_t *state_tree_find(st_tree_t *node, const char *var);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* NUT_STATE_H_SEEN */
