/* desc.c - variable/command description handling for upsd

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

#include <string.h>

#include "common.h"
#include "parseconf.h"

#include "desc.h"

extern const char *datapath;

struct dlist_t {
	char	*name;
	char	*desc;
	struct	dlist_t	*next;
};

	static	struct	dlist_t	*cmd_list = NULL, *var_list = NULL;

static void list_free(struct dlist_t *ptr)
{
	struct	dlist_t	*next;

	while (ptr) {
		next = ptr->next;

		free(ptr->name);
		free(ptr->desc);
		free(ptr);

		ptr = next;
	}
}

static const char *list_get(const struct dlist_t *list, const char *name)
{
	const	struct	dlist_t	*tmp;

	tmp = list;

	while (tmp) {
		if (!strcasecmp(tmp->name, name))
			return tmp->desc;

		tmp = tmp->next;
	}

	return NULL;
}

static void desc_add(struct dlist_t **list, const char *name, const char *desc)
{
	struct	dlist_t	*tmp, *last;

	tmp = last = *list;

	while (tmp) {
		last = tmp;

		/* replace duplicates */
		if (!strcasecmp(tmp->name, name)) {
			free(tmp->desc);
			tmp->desc = xstrdup(desc);
			return;
		}

		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(struct dlist_t));

	tmp->name = xstrdup(name);
	tmp->desc = xstrdup(desc);
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		*list = tmp;
}

static void desc_file_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf (cmdvartab): %s", errmsg);
}

/* interface */

void desc_load(void)
{
	char	fn[SMALLBUF];
	PCONF_CTX_t	ctx;

	snprintf(fn, sizeof(fn), "%s/cmdvartab", datapath);

	pconf_init(&ctx, desc_file_err);

	/* this file is not required */
	if (!pconf_file_begin(&ctx, fn)) {
		upslogx(LOG_INFO, "%s not found - disabling descriptions", fn);
		pconf_finish(&ctx);
		return;
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);

			continue;
		}
			
		if (ctx.numargs < 3)
			continue;

		if (!strcmp(ctx.arglist[0], "CMDDESC")) {
			desc_add(&cmd_list, ctx.arglist[1], ctx.arglist[2]);
			continue;
		}

		if (!strcmp(ctx.arglist[0], "VARDESC")) {
			desc_add(&var_list, ctx.arglist[1], ctx.arglist[2]);
			continue;
		}

		/* unknown */
	}

	pconf_finish(&ctx);
}

void desc_free(void)
{
	list_free(cmd_list);
	list_free(var_list);

	cmd_list = var_list = NULL;
}

const char *desc_get_cmd(const char *name)
{
	return list_get(cmd_list, name);
}

const char *desc_get_var(const char *name)
{
	return list_get(var_list, name);
}
