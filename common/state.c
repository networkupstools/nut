/* state.c - Network UPS Tools common state management functions

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

#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"
#include "state.h"
#include "parseconf.h"

static void val_escape(struct st_tree_t *node)
{
	char	etmp[ST_MAX_VALUE_LEN];

	/* escape any tricky stuff like \ and " */
	pconf_encode(node->val, etmp, sizeof(etmp));

	/* if nothing was escaped, we don't need to do anything else */
	if (!strcmp(node->raw, etmp)) {
		node->val = node->raw;
		return;
	}

	/* first time: set a good starting place */
	if (node->safesize == 0) {
		node->safesize = strlen(etmp) + 1;
		node->safe = xmalloc(node->safesize);
	}

	/* if the escaped value grew, deal with it */
	if (strlen(etmp) > (node->safesize - 1)) {
		node->safesize = strlen(etmp) + 1;
		node->safe = xrealloc(node->safe, node->safesize);
	}

	snprintf(node->safe, node->safesize, "%s", etmp);
	node->val = node->safe;
}

/* free all memory associated with a node */
static void st_tree_node_free(struct st_tree_t *node)
{
	struct	enum_t	*tmp, *next;

	if (node->var)
		free(node->var);
	if (node->raw)
		free(node->raw);
	if (node->safe)
		free(node->safe);

	/* never free node->val, since it's just a pointer to raw or safe */

	/* blow away the list of enums */
	tmp = node->enum_list;
	while (tmp) {
		next = tmp->next;

		free(tmp->val);
		free(tmp);

		tmp = next;
	}

	/* now finally kill the node itself */
	free(node);
}

/* add a subtree to another subtree */
static void st_tree_node_add(struct st_tree_t **nptr, struct st_tree_t *sptr)
{
	struct	st_tree_t 	*node = *nptr;

	if (!sptr)
		return;

	if (!node) {
		*nptr = sptr;
		return;
	}

	if (strcmp(sptr->var, node->var) < 0)
		st_tree_node_add(&node->left, sptr);
	else
		st_tree_node_add(&node->right, sptr);
}

int state_delinfo(struct st_tree_t **nptr, const char *var)
{
	struct	st_tree_t	*node = *nptr;

	if (!node)
		return 0;	/* variable not found! */

	if (strcasecmp(var, node->var) < 0)
		return state_delinfo(&node->left, var);

	if (strcasecmp(var, node->var) > 0)
		return state_delinfo(&node->right, var);

	/* apparently, we've found it! */

	/* whatever is on the left, hang it off current right */
	st_tree_node_add(&node->right, node->left);

	/* now point the parent at the old right child */
	*nptr = node->right;

	st_tree_node_free(node);

	return 1;
}	

/* interface */

int state_setinfo(struct st_tree_t **nptr, const char *var, const char *val)
{
	struct	st_tree_t	*node = *nptr;

	if (!node) {
		*nptr = xmalloc(sizeof(struct st_tree_t));

		node = *nptr;
		node->var = xstrdup(var);

		node->rawsize = strlen(val) + 1;
		node->raw = xmalloc(node->rawsize);
		snprintf(node->raw, node->rawsize, "%s", val);

		/* this is usually sufficient if nothing gets escaped */
		node->val = node->raw;
		node->safesize = 0;
		node->safe = NULL;

		/* but see if it needs escaping anyway */
		val_escape(node);

		/* these are updated by other functions */
		node->flags = 0;
		node->aux = 0;
		node->enum_list = NULL;

		node->left = NULL;
		node->right = NULL;

		return 1;	/* added */
	}

	if (strcasecmp(var, node->var) < 0)
		return state_setinfo(&node->left, var, val);

	if (strcasecmp(var, node->var) > 0)
		return state_setinfo(&node->right, var, val);

	/* var must equal node->var - updating an existing entry */

	if (!strcasecmp(node->raw, val))
		return 0;		/* no change */

	/* expand the buffer if the value grows */
	if (strlen(val) > (node->rawsize - 1)) {
		node->rawsize = strlen(val) + 1;
		node->raw = xrealloc(node->raw, node->rawsize);
		node->val = node->raw;
	}

	/* store the literal value for later comparisons */
	snprintf(node->raw, node->rawsize, "%s", val);

	val_escape(node);

	return 1;	/* added */
}

int state_addenum(struct st_tree_t *root, const char *var, const char *value)
{
	struct	st_tree_t	*sttmp;
	struct	enum_t	*etmp, *elast;
	char	enc[ST_MAX_VALUE_LEN];

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "dstate_addenum: base variable (%s) "
			"does not exist", var);
		return 0;	/* failed */
	}

	/* smooth over any oddities in the enum value */
	pconf_encode(value, enc, sizeof(enc));

	etmp = sttmp->enum_list;
	elast = NULL;

	while (etmp) {
		elast = etmp;

		/* don't add duplicates - silently ignore them */
		if (!strcmp(etmp->val, enc))
			return 1;

		etmp = etmp->next;
	}

	etmp = xmalloc(sizeof(struct enum_t));
	etmp->val = xstrdup(enc);
	etmp->next = NULL;

	if (!elast)
		sttmp->enum_list = etmp;
	else
		elast->next = etmp;

	return 1;
}

int state_setaux(struct st_tree_t *root, const char *var, const char *auxs)
{
	struct	st_tree_t	*sttmp;
	int	aux;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "dstate_addenum: base variable (%s) "
			"does not exist", var);
		return -1;	/* failed */
	}

	aux = strtol(auxs, (char **) NULL, 10);

	/* silently ignore matches */
	if (sttmp->aux == aux)
		return 0;

	sttmp->aux = aux;

	return 1;
}

const char *state_getinfo(struct st_tree_t *root, const char *var)
{
	struct	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp)
		return NULL;

	return sttmp->val;
}

int state_getflags(struct st_tree_t *root, const char *var)
{
	struct	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp)
		return -1;

	return sttmp->flags;
}

int state_getaux(struct st_tree_t *root, const char *var)
{
	struct	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp)
		return -1;

	return sttmp->aux;
}

const struct enum_t *state_getenumlist(struct st_tree_t *root, const char *var)
{
	struct	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp)
		return NULL;

	return sttmp->enum_list;
}

void state_setflags(struct st_tree_t *root, const char *var, int numflags,
	char **flag)
{	
	int	i;
	struct	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "dstate_setflags: base variable (%s) "
			"does not exist", var);
		return;
	}

	sttmp->flags = 0;

	for (i = 0; i < numflags; i++) {

		if (!strcasecmp(flag[i], "RW")) {
			sttmp->flags |= ST_FLAG_RW;
			continue;
		}

		if (!strcasecmp(flag[i], "STRING")) {
			sttmp->flags |= ST_FLAG_STRING;
			continue;
		}

		upsdebugx(2, "Unrecognized flag [%s]", flag[i]);
	}
}
		
void state_addcmd(struct cmdlist_t **list, const char *cmdname)
{
	struct	cmdlist_t	*tmp, *last;

	tmp = last = *list;

	while (tmp) {
		last = tmp;

		/* ignore duplicates */
		if (!strcasecmp(tmp->name, cmdname))
			return;

		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(struct cmdlist_t));
	tmp->name = xstrdup(cmdname);
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		*list = tmp;
}

void state_infofree(struct st_tree_t *node)
{
	if (!node)
		return;

	state_infofree(node->left);

	state_infofree(node->right);

	st_tree_node_free(node);
}

int state_delcmd(struct cmdlist_t **list, const char *cmd)
{
	struct	cmdlist_t	*tmp, *last;

	tmp = *list;
	last = NULL;

	while (tmp) {
		if (!strcmp(tmp->name, cmd)) {

			if (last)
				last->next = tmp->next;
			else
				*list = tmp->next;

			free(tmp->name);
			free(tmp);

			return 1;	/* deleted */
		}

		tmp = tmp->next;
	}

	return 0;	/* not found */
}

int state_delenum(struct st_tree_t *root, const char *var, const char *val)
{
	struct	st_tree_t	*sttmp;
	struct	enum_t *etmp, *elast;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp)
		return 0;

	/* look for val in enum_list */
	etmp = sttmp->enum_list;
	elast = NULL;	

	while (etmp) {
		if (!strcmp(etmp->val, val)) {

			if (elast)
				elast->next = etmp->next;
			else
				sttmp->enum_list = etmp->next;

			free(etmp->val);
			free(etmp);

			return 1;	/* deleted */
		}

		elast = etmp;
		etmp = etmp->next;
	}

	return 0;	/* not found */
}

struct st_tree_t *state_tree_find(struct st_tree_t *node, const char *var)
{
	if (!node)
		return NULL;

	if (strcasecmp(var, node->var) < 0)
		return state_tree_find(node->left, var);

	if (strcasecmp(var, node->var) > 0)
		return state_tree_find(node->right, var);

	return node;
}
