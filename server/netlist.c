/* netlist.c - LIST handlers for upsd

   Copyright (C)
    2003  Russell Kroll <rkroll@exploits.org>
	2012  Arnaud Quette <arnaud.quette@free.fr>

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

#include "common.h"

#include "upsd.h"
#include "sstate.h"
#include "state.h"
#include "neterr.h"

#include "netlist.h"

extern	upstype_t	*firstups;	/* for list_ups */
extern	nut_ctype_t *firstclient;	/* for list_clients */

static int tree_dump(st_tree_t *node, nut_ctype_t *client, const char *ups,
	int rw, int fsd)
{
	int	ret;

	if (!node)
		return 1;	/* not an error */

	if (node->left) {
		ret = tree_dump(node->left, client, ups, rw, fsd);

		if (!ret)
			return 0;		/* write failed in child */
	}

	if (rw) {

		/* only send this back if it's been flagged RW */
		if (node->flags & ST_FLAG_RW) {
			ret = sendback(client, "RW %s %s \"%s\"\n",
				ups, node->var, node->val);

		} else {
			ret = 1;	/* dummy */
		}

	} else {

		/* normal variable list only */

		/* status is always a special case */
		if ((fsd == 1) && (!strcasecmp(node->var, "ups.status"))) {
			ret = sendback(client, "VAR %s %s \"FSD %s\"\n",
				ups, node->var, node->val);

		} else {
			ret = sendback(client, "VAR %s %s \"%s\"\n",
				ups, node->var, node->val);
		}
	}

	if (ret != 1)
		return 0;

	if (node->right)
		return tree_dump(node->right, client, ups, rw, fsd);

	return 1;
}

static void list_rw(nut_ctype_t *client, const char *upsname)
{
	const   upstype_t *ups;

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (!ups_available(ups, client))
		return;

	if (!sendback(client, "BEGIN LIST RW %s\n", upsname))
		return;

	if (!tree_dump(ups->inforoot, client, upsname, 1, ups->fsd))
		return;

	sendback(client, "END LIST RW %s\n", upsname);
}

static void list_var(nut_ctype_t *client, const char *upsname)
{
	const   upstype_t *ups;

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (!ups_available(ups, client))
		return;

	if (!sendback(client, "BEGIN LIST VAR %s\n", upsname))
		return;

	if (!tree_dump(ups->inforoot, client, upsname, 0, ups->fsd))
		return;

	sendback(client, "END LIST VAR %s\n", upsname);
}

static void list_cmd(nut_ctype_t *client, const char *upsname)
{
	const   upstype_t *ups;
	cmdlist_t	*ctmp;

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (!ups_available(ups, client))
		return;

	if (!sendback(client, "BEGIN LIST CMD %s\n", upsname))
		return;

	for (ctmp = ups->cmdlist; ctmp != NULL; ctmp = ctmp->next) {
		if (!sendback(client, "CMD %s %s\n", upsname, ctmp->name))
			return;
	}

	sendback(client, "END LIST CMD %s\n", upsname);
}

static void list_enum(nut_ctype_t *client, const char *upsname, const char *var)
{
	const   upstype_t *ups;
	const	st_tree_t	*node;
	const	enum_t	*etmp;

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (!ups_available(ups, client))
		return;

	node = sstate_getnode(ups, var);

	if (!node) {
		send_err(client, NUT_ERR_VAR_NOT_SUPPORTED);
		return;
	}

	if (!sendback(client, "BEGIN LIST ENUM %s %s\n", upsname, var))
		return;

	for (etmp = node->enum_list; etmp != NULL; etmp = etmp->next) {
		if (!sendback(client, "ENUM %s %s \"%s\"\n",
			upsname, var, etmp->val))
			return;
	}

	sendback(client, "END LIST ENUM %s %s\n", upsname, var);
}

static void list_range(nut_ctype_t *client, const char *upsname, const char *var)
{
	const   upstype_t *ups;
	const	st_tree_t	*node;
	const	range_t	*rtmp;

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (!ups_available(ups, client))
		return;

	node = sstate_getnode(ups, var);

	if (!node) {
		send_err(client, NUT_ERR_VAR_NOT_SUPPORTED);
		return;
	}

	if (!sendback(client, "BEGIN LIST RANGE %s %s\n", upsname, var))
		return;

	for (rtmp = node->range_list; rtmp != NULL; rtmp = rtmp->next) {
		if (!sendback(client, "RANGE %s %s \"%i\" \"%i\"\n",
			upsname, var, rtmp->min, rtmp->max))
			return;
	}

	sendback(client, "END LIST RANGE %s %s\n", upsname, var);
}

static void list_ups(nut_ctype_t *client)
{
	upstype_t	*utmp;
	char	esc[SMALLBUF];

	if (!sendback(client, "BEGIN LIST UPS\n"))
		return;

	utmp = firstups;

	while (utmp) {
		int	ret;

		if (utmp->desc) {
			pconf_encode(utmp->desc, esc, sizeof(esc));
			ret = sendback(client, "UPS %s \"%s\"\n",
				utmp->name, esc);

		} else {
			ret = sendback(client, "UPS %s \"Description unavailable\"\n",
				 utmp->name);
		}

		if (!ret)
			return;

		utmp = utmp->next;
	}

	sendback(client, "END LIST UPS\n");
}

static void list_clients(nut_ctype_t *client, const char *upsname)
{
	const upstype_t *ups;
	nut_ctype_t		*c, *cnext;

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (!sendback(client, "BEGIN LIST CLIENT %s\n", upsname))
		return;

	if (firstclient) {
		int	ret;
		/* show connected clients */
		for (c = firstclient; c; c = cnext) {
			if (c->loginups && (!ups || !strcasecmp(c->loginups, ups->name))) {
				ret = sendback(client, "CLIENT %s %s\n", c->loginups, c->addr);
				if (!ret)
					return;
			}
			cnext = c->next;
		}
	}
	sendback(client, "END LIST CLIENT %s\n", upsname);
}

void net_list(nut_ctype_t *client, int numarg, const char **arg)
{
	if (numarg < 1) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	/* LIST UPS */
	if (!strcasecmp(arg[0], "UPS")) {
		list_ups(client);
		return;
	}

	if (numarg < 2) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	/* LIST VAR UPS */
	if (!strcasecmp(arg[0], "VAR")) {
		list_var(client, arg[1]);
		return;
	}

	/* LIST RW UPS */
	if (!strcasecmp(arg[0], "RW")) {
		list_rw(client, arg[1]);
		return;
	}

	/* LIST CMD UPS */
	if (!strcasecmp(arg[0], "CMD")) {
		list_cmd(client, arg[1]);
		return;
	}

	/* LIST CLIENT UPS */
	if (!strcasecmp(arg[0], "CLIENT")) {
		list_clients(client, arg[1]);
		return;
	}

	if (numarg < 3) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	/* LIST ENUM UPS VARNAME */
	if (!strcasecmp(arg[0], "ENUM")) {
		list_enum(client, arg[1], arg[2]);
		return;
	}

	/* LIST RANGE UPS VARNAME */
	if (!strcasecmp(arg[0], "RANGE")) {
		list_range(client, arg[1], arg[2]);
		return;
	}

	send_err(client, NUT_ERR_INVALID_ARGUMENT);
}
