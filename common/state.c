/* state.c - Network UPS Tools common state management functions

   Copyright (C)
	2003	Russell Kroll <rkroll@exploits.org>
	2008	Arjen de Korte <adkorte-guest@alioth.debian.org>
	2012	Arnaud Quette <arnaud.quette@free.fr>
	2020-2024	Jim Klimov <jimklimov+nut@gmail.com>

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

#include "config.h"	/* must be first */

#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#include <sys/un.h>
#endif	/* !WIN32 */

#include "common.h"
#include "state.h"
#include "parseconf.h"

/* internal helpers */

static void val_escape(st_tree_t *node)
{
	char	etmp[ST_MAX_VALUE_LEN];

	/* escape any tricky stuff like \ and " */
	pconf_encode(node->raw, etmp, sizeof(etmp));

	/* if nothing was escaped, we don't need to do anything else */
	if (!strcmp(node->raw, etmp)) {
		node->val = node->raw;
		return;
	}

	/* if the escaped value grew, deal with it */
	if (node->safesize < (strlen(etmp) + 1)) {
		node->safesize = strlen(etmp) + 1;
		node->safe = xrealloc(node->safe, node->safesize);
	}

	snprintf(node->safe, node->safesize, "%s", etmp);
	node->val = node->safe;
}

static void st_tree_enum_free(enum_t *list)
{
	if (!list) {
		return;
	}

	st_tree_enum_free(list->next);

	free(list->val);
	free(list);
}

static void st_tree_range_free(range_t *list)
{
	if (!list) {
		return;
	}

	st_tree_range_free(list->next);

	free(list);
}

/* free all memory associated with a node */
static void st_tree_node_free(st_tree_t *node)
{
	free(node->var);
	free(node->raw);
	free(node->safe);

	/* never free node->val, since it's just a pointer to raw or safe */

	/* blow away the list of enums */
	st_tree_enum_free(node->enum_list);

	/* and the list of ranges */
	st_tree_range_free(node->range_list);

	/* now finally kill the node itself */
	free(node);
}

/* add a subtree to another subtree */
static void st_tree_node_add(st_tree_t **nptr, st_tree_t *sptr)
{
	if (!sptr) {
		return;
	}

	while (*nptr) {

		st_tree_t	*node = *nptr;

		if (strcasecmp(node->var, sptr->var) > 0) {
			nptr = &node->left;
			continue;
		}

		if (strcasecmp(node->var, sptr->var) < 0) {
			nptr = &node->right;
			continue;
		}

		upsdebugx(1, "%s: duplicate value (shouldn't happen)", __func__);
		return;
	}

	*nptr = sptr;
}

static int st_tree_node_refresh_timestamp(const st_tree_t *node)
{
	if (!node)
		return -1;

	return state_get_timestamp((st_tree_timespec_t *)&node->lastset);
}

/* interface */

/* As underlying system methods:
 * return 0 on success, -1 and errno on error
 */
int state_get_timestamp(st_tree_timespec_t *now)
{
	if (!now)
		return -1;

#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_MONOTONIC) && HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC
	return clock_gettime(CLOCK_MONOTONIC, now);
#else
	return gettimeofday(now, NULL);
#endif
}

/* Returns -1 if the node->lastset is "older" than cutoff,
 * 0 if it is equal, or +1 if it is newer.
 * Returns -2 or -3 if node or cutoff are null.
 */
int st_tree_node_compare_timestamp(
	const st_tree_t *node,
	const st_tree_timespec_t *cutoff
) {
	double d;

	if (!node)
		return -2;

	if (!cutoff)
		return -3;

	/* Like in difftime(), the first arg is "finish" and
	 * the second arg is "start" of a time range (below),
	 * so if the diff is negative, then "lastset" happened
	 * before "cutoff":
	 */
#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_MONOTONIC) && HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC
	d = difftimespec(node->lastset, *cutoff);
#else
	d = difftimeval(node->lastset, *cutoff);
#endif

	if (d < 0)
		return -1;
	if (d > 0)
		return 1;
	return 0;
}

/* remove a variable from a tree
 * except for variables with ST_FLAG_IMMUTABLE
 * (for override.* to survive) per issue #737
 */
int state_delinfo(st_tree_t **nptr, const char *var)
{
	while (*nptr) {

		st_tree_t	*node = *nptr;

		if (strcasecmp(node->var, var) > 0) {
			nptr = &node->left;
			continue;
		}

		if (strcasecmp(node->var, var) < 0) {
			nptr = &node->right;
			continue;
		}

		if (node->flags & ST_FLAG_IMMUTABLE) {
			upsdebugx(6, "%s: not deleting immutable variable [%s]", __func__, var);
			return 0;
		}

		/* whatever is on the left, hang it off current right */
		st_tree_node_add(&node->right, node->left);

		/* now point the parent at the old right child */
		*nptr = node->right;

		st_tree_node_free(node);

		return 1;
	}

	return 0;	/* not found */
}

int state_delinfo_olderthan(st_tree_t **nptr, const char *var, const st_tree_timespec_t *cutoff)
{
	while (*nptr) {

		st_tree_t	*node = *nptr;

		if (strcasecmp(node->var, var) > 0) {
			nptr = &node->left;
			continue;
		}

		if (strcasecmp(node->var, var) < 0) {
			nptr = &node->right;
			continue;
		}

		if (node->flags & ST_FLAG_IMMUTABLE) {
			upsdebugx(6, "%s: not deleting immutable variable [%s]", __func__, var);
			return 0;
		}

		if (st_tree_node_compare_timestamp(node, cutoff) >= 0) {
			upsdebugx(6, "%s: not deleting recently updated variable [%s]", __func__, var);
			return 0;
		}
		upsdebugx(6, "%s: deleting variable [%s] last updated too long ago", __func__, var);

		/* whatever is on the left, hang it off current right */
		st_tree_node_add(&node->right, node->left);

		/* now point the parent at the old right child */
		*nptr = node->right;

		st_tree_node_free(node);

		return 1;
	}

	return 0;	/* not found */
}

int state_setinfo(st_tree_t **nptr, const char *var, const char *val)
{
	while (*nptr) {

		st_tree_t	*node = *nptr;

		if (strcasecmp(node->var, var) > 0) {
			nptr = &node->left;
			continue;
		}

		if (strcasecmp(node->var, var) < 0) {
			nptr = &node->right;
			continue;
		}

		/* refresh even if "skip-writing" same info value */
		st_tree_node_refresh_timestamp(node);

		/* updating an existing entry */
		if (!strcasecmp(node->raw, val)) {
			return 0;	/* no change */
		}

		/* changes should be ignored */
		if (node->flags & ST_FLAG_IMMUTABLE) {
			upsdebugx(6, "%s: not changing immutable variable [%s]", __func__, var);
			return 0;	/* no change */
		}

		/* expand the buffer if the value grows */
		if (node->rawsize < (strlen(val) + 1)) {
			node->rawsize = strlen(val) + 1;
			node->raw = xrealloc(node->raw, node->rawsize);
		}

		/* store the literal value for later comparisons */
		snprintf(node->raw, node->rawsize, "%s", val);

		val_escape(node);

		return 1;	/* changed */
	}

	*nptr = xcalloc(1, sizeof(**nptr));

	(*nptr)->var = xstrdup(var);
	(*nptr)->raw = xstrdup(val);
	(*nptr)->rawsize = strlen(val) + 1;
	st_tree_node_refresh_timestamp(*nptr);

	val_escape(*nptr);

	return 1;	/* added */
}

static int st_tree_enum_add(enum_t **list, const char *enc)
{
	enum_t	*item;

	while (*list) {

		if (strcmp((*list)->val, enc)) {
			list = &(*list)->next;
			continue;
		}

		return 0;	/* duplicate */
	}

	item = xcalloc(1, sizeof(*item));
	item->val = xstrdup(enc);
	item->next = *list;

	/* now we're done creating it, add it to the list */
	*list = item;

	return 1;	/* added */
}

int state_addenum(st_tree_t *root, const char *var, const char *val)
{
	st_tree_t	*sttmp;
	char	enc[ST_MAX_VALUE_LEN];

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "%s: base variable (%s) "
			"does not exist", __func__, var);
		return 0;	/* failed */
	}

	/* smooth over any oddities in the enum value */
	pconf_encode(val, enc, sizeof(enc));

	st_tree_node_refresh_timestamp(sttmp);
	return st_tree_enum_add(&sttmp->enum_list, enc);
}

static int st_tree_range_add(range_t **list, const int min, const int max)
{
	range_t	*item;

	while (*list) {

		if (((*list)->min != min) && ((*list)->max != max)) {
			list = &(*list)->next;
			continue;
		}

		return 0;	/* duplicate */
	}

	item = xcalloc(1, sizeof(*item));
	item->min = min;
	item->max = max;
	item->next = *list;

	/* now we're done creating it, add it to the list */
	*list = item;

	return 1;	/* added */
}

int state_addrange(st_tree_t *root, const char *var, const int min, const int max)
{
	st_tree_t	*sttmp;

	/* sanity check */
	if (min > max) {
		upslogx(LOG_ERR, "%s: min is superior to max! (%i, %i)",
			__func__, min, max);
		return 0;
	}

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "%s: base variable (%s) "
			"does not exist", __func__, var);
		return 0;	/* failed */
	}

	st_tree_node_refresh_timestamp(sttmp);
	return st_tree_range_add(&sttmp->range_list, min, max);
}

int state_setaux(st_tree_t *root, const char *var, const char *auxs)
{
	st_tree_t	*sttmp;
	long	aux;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "%s: base variable (%s) "
			"does not exist", __func__, var);
		return -1;	/* failed */
	}

	st_tree_node_refresh_timestamp(sttmp);
	aux = strtol(auxs, (char **) NULL, 10);

	/* silently ignore matches */
	if (sttmp->aux == aux) {
		return 0;
	}

	sttmp->aux = aux;

	return 1;
}

const char *state_getinfo(st_tree_t *root, const char *var)
{
	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		return NULL;
	}

	return sttmp->val;
}

int state_getflags(st_tree_t *root, const char *var)
{
	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		return -1;
	}

	return sttmp->flags;
}

long state_getaux(st_tree_t *root, const char *var)
{
	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		return -1;
	}

	return sttmp->aux;
}

const enum_t *state_getenumlist(st_tree_t *root, const char *var)
{
	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		return NULL;
	}

	return sttmp->enum_list;
}

const range_t *state_getrangelist(st_tree_t *root, const char *var)
{
	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		return NULL;
	}

	return sttmp->range_list;
}

void state_setflags(st_tree_t *root, const char *var, size_t numflags, char **flag)
{
	size_t	i;
	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		upslogx(LOG_ERR, "%s: base variable (%s) "
			"does not exist", __func__, var);
		return;
	}

	st_tree_node_refresh_timestamp(sttmp);
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

		if (!strcasecmp(flag[i], "NUMBER")) {
			sttmp->flags |= ST_FLAG_NUMBER;
			continue;
		}

		upsdebugx(2, "%s: Unrecognized flag [%s]", __func__, flag[i]);
	}
}

int state_addcmd(cmdlist_t **list, const char *cmd)
{
	cmdlist_t	*item;

	while (*list) {

		if (strcasecmp((*list)->name, cmd) > 0) {
			/* insertion point reached */
			break;
		}

		if (strcasecmp((*list)->name, cmd) < 0) {
			list = &(*list)->next;
			continue;
		}

		return 0;	/* duplicate */
	}

	item = xcalloc(1, sizeof(*item));
	item->name = xstrdup(cmd);
	item->next = *list;

	/* now we're done creating it, insert it in the list */
	*list = item;

	return 1;	/* added */
}

void state_infofree(st_tree_t *node)
{
	if (!node) {
		return;
	}

	state_infofree(node->left);
	state_infofree(node->right);

	st_tree_node_free(node);
}

void state_cmdfree(cmdlist_t *list)
{
	if (!list) {
		return;
	}

	state_cmdfree(list->next);

	free(list->name);
	free(list);
}

int state_delcmd(cmdlist_t **list, const char *cmd)
{
	while (*list) {

		cmdlist_t	*item = *list;

		if (strcasecmp(item->name, cmd) > 0) {
			/* not found */
			break;
		}

		if (strcasecmp(item->name, cmd) < 0) {
			list = &item->next;
			continue;
		}

		/* we found it! */

		*list = item->next;

		free(item->name);
		free(item);

		return 1;	/* deleted */
	}

	return 0;	/* not found */
}

static int st_tree_del_enum(enum_t **list, const char *val)
{
	while (*list) {

		enum_t	*item = *list;

		/* if this is not the right value, go on to the next */
		if (strcasecmp(item->val, val)) {
			list = &item->next;
			continue;
		}

		/* we found it! */
		*list = item->next;

		free(item->val);
		free(item);

		return 1;	/* deleted */
	}

	return 0;	/* not found */
}

int state_delenum(st_tree_t *root, const char *var, const char *val)
{
	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		return 0;
	}

	st_tree_node_refresh_timestamp(sttmp);
	return st_tree_del_enum(&sttmp->enum_list, val);
}

static int st_tree_del_range(range_t **list, const int min, const int max)
{
	while (*list) {

		range_t	*item = *list;

		/* if this is not the right value, go on to the next */
		if (((*list)->min != min) && ((*list)->max != max)) {
			list = &item->next;
			continue;
		}

		/* we found it! */
		*list = item->next;

		free(item);

		return 1;	/* deleted */
	}

	return 0;	/* not found */
}

int state_delrange(st_tree_t *root, const char *var, const int min, const int max)
{
	st_tree_t	*sttmp;

	/* find the tree node for var */
	sttmp = state_tree_find(root, var);

	if (!sttmp) {
		return 0;
	}

	st_tree_node_refresh_timestamp(sttmp);
	return st_tree_del_range(&sttmp->range_list, min, max);
}

st_tree_t *state_tree_find(st_tree_t *node, const char *var)
{
	while (node) {

		if (strcasecmp(node->var, var) > 0) {
			node = node->left;
			continue;
		}

		if (strcasecmp(node->var, var) < 0) {
			node = node->right;
			continue;
		}

		break;	/* found */
	}

	return node;
}
