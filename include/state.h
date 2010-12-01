/* state.h - Network UPS Tools common state management functions

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

#ifndef STATE_H_SEEN
#define STATE_H_SEEN

#include "extstate.h"

#define ST_SOCK_BUF_LEN 512

typedef struct st_tree_s {
	char	*var;
	char	*val;			/* points to raw or safe */

	char	*raw;			/* raw data from caller */
	size_t	rawsize;

	char	*safe;			/* safe data from pconf_encode */
	size_t	safesize;

	int	flags;
	int	aux;

	struct enum_s		*enum_list;

	struct st_tree_s	*left;
	struct st_tree_s	*right;
} st_tree_t;

int state_setinfo(st_tree_t **nptr, const char *var, const char *val);
int state_addenum(st_tree_t *root, const char *var, const char *val);
int state_setaux(st_tree_t *root, const char *var, const char *auxs);
const char *state_getinfo(st_tree_t *root, const char *var);
int state_getflags(st_tree_t *root, const char *var);
int state_getaux(st_tree_t *root, const char *var);
const enum_t *state_getenumlist(st_tree_t *root, const char *var);
void state_setflags(st_tree_t *root, const char *var, int numflags, char **flags);
int state_addcmd(cmdlist_t **list, const char *cmd);
void state_infofree(st_tree_t *node);
void state_cmdfree(cmdlist_t *list);
int state_delcmd(cmdlist_t **list, const char *cmd);
int state_delinfo(st_tree_t **root, const char *var);
int state_delenum(st_tree_t *root, const char *var, const char *val);
st_tree_t *state_tree_find(st_tree_t *node, const char *var);

#endif /* STATE_H_SEEN */
