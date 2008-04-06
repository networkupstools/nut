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

#include "extstate.h"

#define ST_SOCK_BUF_LEN 512

struct st_tree_t {
	char	*var;
	char	*val;			/* points to raw or safe */

	char	*raw;			/* raw data from caller */
	size_t	rawsize;

	char	*safe;			/* safe data from pconf_encode */
	size_t	safesize;

	int	flags;
	int	aux;

	struct	enum_t		*enum_list;

	struct	st_tree_t	*left;
	struct	st_tree_t	*right;
};

int state_setinfo(struct st_tree_t **nptr, const char *var, const char *val);
int state_addenum(struct st_tree_t *root, const char *var, const char *val);
int state_setaux(struct st_tree_t *root, const char *var, const char *auxs);
const char *state_getinfo(struct st_tree_t *root, const char *var);
int state_getflags(struct st_tree_t *root, const char *var);
int state_getaux(struct st_tree_t *root, const char *var);
const struct enum_t *state_getenumlist(struct st_tree_t *root, const char *var);
void state_setflags(struct st_tree_t *root, const char *var, int numflags, char **flags);
int state_addcmd(struct cmdlist_t **list, const char *cmd);
void state_infofree(struct st_tree_t *node);
void state_cmdfree(struct cmdlist_t *list);
int state_delcmd(struct cmdlist_t **list, const char *cmd);
int state_delinfo(struct st_tree_t **root, const char *var);
int state_delenum(struct st_tree_t *root, const char *var, const char *val);
struct st_tree_t *state_tree_find(struct st_tree_t *node, const char *var);
