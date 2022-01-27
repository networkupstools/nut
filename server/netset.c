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
	const char *newval, const char *tracking_id)
{
	upstype_t	*ups;
	const	char	*val;
	const	enum_t  *etmp;
	const	range_t  *rtmp;
	char	cmd[SMALLBUF], esc[SMALLBUF];
	int	have_tracking_id = 0;

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
		long	aux;

		aux = sstate_getaux(ups, var);

		/* check for insanity from the driver */
		if (aux < 1) {
			upslogx(LOG_WARNING, "UPS [%s]: auxdata for %s is invalid",
				ups->name, var);

			send_err(client, NUT_ERR_SET_FAILED);
			return;
		}

		/* FIXME? Should this cast to "long"?
		 * An int-size string is quite a lot already,
		 * even on architectures with a moderate INTMAX
		 */
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

	snprintf(cmd, sizeof(cmd), "SET %s \"%s\"",
		var, pconf_encode(newval, esc, sizeof(esc)));

	/* see if the user want execution tracking for this command */
	if (tracking_id && *tracking_id) {
		snprintfcat(cmd, sizeof(cmd), " TRACKING %s", tracking_id);
		/* Add an entry in the tracking structure */
		tracking_add(tracking_id);
		have_tracking_id = 1;
	}

	/* add EOL */
	snprintfcat(cmd, sizeof(cmd), "\n");

	upslogx(LOG_INFO, "Set variable: %s@%s set %s on %s to %s (tracking ID: %s)",
		client->username, client->addr, var, ups->name, newval,
		(have_tracking_id) ? tracking_id : "disabled");

	if (!sstate_sendline(ups, cmd)) {
		upslogx(LOG_INFO, "Set command send failed");
		send_err(client, NUT_ERR_SET_FAILED);
		return;
	}

	/* return the result, possibly including tracking_id */
	if (have_tracking_id)
		sendback(client, "OK TRACKING %s\n", tracking_id);
	else
		sendback(client, "OK\n");
}

void net_set(nut_ctype_t *client, size_t numarg, const char **arg)
{
	char	tracking_id[UUID4_LEN] = "";

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

		if (client->tracking) {
			/* Generate a tracking ID, if client requested status tracking */
			nut_uuid_v4(tracking_id);
		}

		set_var(client, arg[1], arg[2], arg[3], tracking_id);

		return;
	}

	/* SET TRACKING VALUE */
	if (!strcasecmp(arg[0], "TRACKING")) {
		if (!strcasecmp(arg[1], "ON")) {
			/* general enablement along with for this client */
			client->tracking = tracking_enable();
		}
		else if (!strcasecmp(arg[1], "OFF")) {
			/* disable status tracking for this client first */
			client->tracking = 0;
			/* then only disable the general one if no other clients use it!
			 * Note: don't call tracking_free() since we want info to
			 * persist, and tracking_cleanup() takes care of cleaning */
			if (tracking_disable()) {
				upsdebugx(2, "%s: TRACKING disabled for one client, more remain.", __func__);
			} else {
				upsdebugx(2, "%s: TRACKING disabled for last client.", __func__);
			}
		}
		else {
			send_err(client, NUT_ERR_INVALID_ARGUMENT);
			return;
		}
		upsdebugx(1, "%s: TRACKING general %s, client %s.", __func__,
			tracking_is_enabled() ? "enabled" : "disabled",
			client->tracking ? "enabled" : "disabled");

		sendback(client, "OK\n");

		return;
	}

	send_err(client, NUT_ERR_INVALID_ARGUMENT);
	return;
}
