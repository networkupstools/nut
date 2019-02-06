/* netset.c - SET handler for upsd

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

#include "common.h"

#include "upsd.h"
#include "sstate.h"
#include "state.h"
#include "user.h"		/* for user_checkaction */
#include "neterr.h"

#include "netset.h"

static void set_var(nut_ctype_t *client, const char *upsname, const char *var,
	const char *newval, const char *status_id)
{
	upstype_t	*ups;
	const	char	*val;
	const	enum_t  *etmp;
	const	range_t  *rtmp;
	char	cmd[SMALLBUF], esc[SMALLBUF];

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (!ups_available(ups, client))
		return;

	/* make sure this user is allowed to do SET */
	if (!user_checkaction(client->username, client->password, "SET")) {
		send_err(client, NUT_ERR_ACCESS_DENIED);
		return;
	}

	val = sstate_getinfo(ups, var);

	if (!val) {
		send_err(client, NUT_ERR_VAR_NOT_SUPPORTED);
		return;
	}

	/* make sure this variable is writable (RW) */
	if ((sstate_getflags(ups, var) & ST_FLAG_RW) == 0) {
		send_err(client, NUT_ERR_READONLY);
		return;
	}

	/* see if the new value is allowed for this variable */

	if (sstate_getflags(ups, var) & ST_FLAG_STRING) {
		int	aux;

		aux = sstate_getaux(ups, var);

		/* check for insanity from the driver */
		if (aux < 1) {
			upslogx(LOG_WARNING, "UPS [%s]: auxdata for %s is invalid",
				ups->name, var);

			send_err(client, NUT_ERR_SET_FAILED);
			return;
		}

		if (aux < (int) strlen(newval)) {
			send_err(client, NUT_ERR_TOO_LONG);
			return;
		}
	}

	/* see if it's enumerated */

	etmp = sstate_getenumlist(ups, var);

	if (etmp) {
		int	found = 0;

		while (etmp) {
			if (!strcmp(etmp->val, newval)) {
				found = 1;
				break;
			}

			etmp = etmp->next;
		}

		if (!found) {
			send_err(client, NUT_ERR_INVALID_VALUE);
			return;
		}
	}

	/* or if it's within a range */

	rtmp = sstate_getrangelist(ups, var);

	if (rtmp) {
		int	found = 0;
		int inewval = atoi(newval);

		while (rtmp) {
			if ((inewval >= rtmp->min) && (inewval <= rtmp->max)) {
				found = 1;
				break;
			}

			rtmp = rtmp->next;
		}

		if (!found) {
			send_err(client, NUT_ERR_INVALID_VALUE);
			return;
		}
	}

	/* must be OK now */

	upslogx(LOG_INFO, "Set variable: %s@%s set %s on %s to %s (tracking ID: %s)",
		client->username, client->addr, var, ups->name, newval,
		(status_id != NULL)?status_id:"disabled");

	/* see if the user want execution tracking for this command */
	if (status_id != NULL) {
		snprintf(cmd, sizeof(cmd), "SET %s \"%s\" STATUS_ID %s\n",
			var, pconf_encode(newval, esc, sizeof(esc)), status_id);
		/* Add an entry in the tracking structure */
		cmdset_status_add(status_id);
	}
	else {
		snprintf(cmd, sizeof(cmd), "SET %s \"%s\"\n",
			var, pconf_encode(newval, esc, sizeof(esc)));
	}

	if (!sstate_sendline(ups, cmd)) {
		upslogx(LOG_INFO, "Set command send failed");
		send_err(client, NUT_ERR_SET_FAILED);
		return;
	}

	/* return the result, possibly including status_id */
	if (status_id != NULL)
		sendback(client, "OK %s\n", status_id);
	else
		sendback(client, "OK\n");
}

void net_set(nut_ctype_t *client, int numarg, const char **arg)
{
	char	status_id[UUID4_LEN] = "";

	/* Base verification, to ensure that we have at least the SET parameter */
	if (numarg < 2) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	/* SET VAR UPS VARNAME VALUE */
	if (!strcasecmp(arg[0], "VAR")) {
		if (numarg < 4) {
			send_err(client, NUT_ERR_INVALID_ARGUMENT);
			return;
		}

		if (client->cmdset_status_enabled) {
			/* Generate a tracking ID, if client requested status tracking */
			nut_uuid_v4(status_id);
		}

		set_var(client, arg[1], arg[2], arg[3], status_id);

		return;
	}

	/* SET CMDSET_STATUS VALUE */
	if (!strcasecmp(arg[0], "CMDSET_STATUS")) {
		if (!strcasecmp(arg[1], "ON")) {
			/* general enablement along with for this client */
			cmdset_status_enabled = 1;
			client->cmdset_status_enabled = 1;
		}
		else if (!strcasecmp(arg[1], "OFF")) {
			/* disable status tracking for this client first */
			client->cmdset_status_enabled = 0;
			/* then only disable the general one if no other clients use it!
			 * Note: don't call cmdset_status_free() since we want info to
			 * persist, and cmdset_status_cleanup() takes care of cleaning */
			cmdset_status_enabled = cmdset_status_disable();
		}
		else {
			send_err(client, NUT_ERR_INVALID_ARGUMENT);
			return;
		}
		upsdebugx(1, "%s: CMDSET_STATUS %s", __func__,
			(cmdset_status_enabled == 1)?"enabled":"disabled");

		sendback(client, "OK\n");

		return;
	}

	send_err(client, NUT_ERR_INVALID_ARGUMENT);
	return;
}
