/* netinstcmd.c - INSTCMD handler for upsd

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

#include "user.h"			/* for user_checkinstcmd */
#include "neterr.h"

#include "netinstcmd.h"

static void send_instcmd(nut_ctype_t *client, const char *upsname,
	const char *cmdname, const char *value, const char *tracking_id)
{
	int	found, have_tracking_id = 0;
	upstype_t	*ups;
	const	cmdlist_t  *ctmp;
	char	sockcmd[SMALLBUF], esc[SMALLBUF];

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (!ups_available(ups, client))
		return;

	ctmp = sstate_getcmdlist(ups);

	found = 0;

	while (ctmp) {
		if (!strcasecmp(ctmp->name, cmdname)) {
			found = 1;
			break;
		}

		ctmp = ctmp->next;
	}

	if (!found) {
		send_err(client, NUT_ERR_CMD_NOT_SUPPORTED);
		return;
	}

	/* see if this user is allowed to do this command */
	if (!user_checkinstcmd(client->username, client->password, cmdname)) {
		send_err(client, NUT_ERR_ACCESS_DENIED);
		return;
	}

	/* Format the base command */
	snprintf(sockcmd, sizeof(sockcmd), "INSTCMD %s", cmdname);

	/* see if the user has also passed a value for this command */
	if (value != NULL)
		snprintfcat(sockcmd, sizeof(sockcmd), " %s", pconf_encode(value, esc, sizeof(esc)));

	/* see if the user want execution tracking for this command */
	if (tracking_id && *tracking_id) {
		snprintfcat(sockcmd, sizeof(sockcmd), " TRACKING %s", tracking_id);
		/* Add an entry in the tracking structure */
		tracking_add(tracking_id);
		have_tracking_id = 1;
	}

	/* add EOL */
	snprintfcat(sockcmd, sizeof(sockcmd), "\n");

	upslogx(LOG_INFO, "Instant command: %s@%s did %s%s%s on %s (tracking ID: %s)",
		client->username, client->addr, cmdname,
		(value != NULL)?" with value ":"",
		(value != NULL)?value:"",
		ups->name,
		(have_tracking_id) ? tracking_id : "disabled");

	if (!sstate_sendline(ups, sockcmd)) {
		upslogx(LOG_INFO, "Set command send failed");
		send_err(client, NUT_ERR_INSTCMD_FAILED);
		return;
	}

	/* return the result, possibly including tracking_id */
	if (have_tracking_id)
		sendback(client, "OK TRACKING %s\n", tracking_id);
	else
		sendback(client, "OK\n");
}

void net_instcmd(nut_ctype_t *client, int numarg, const char **arg)
{
	const char *devname = NULL;
	const char *cmdname = NULL;
	const char *cmdparam = NULL;
	char	tracking_id[UUID4_LEN] = "";

	if (numarg < 2) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	/* INSTCMD <ups> <cmdname> [cmdparam] */
	/* Check arguments */
	devname = arg[0];
	cmdname = arg[1];
	if (numarg == 3)
		cmdparam = arg[2];

	if (client->tracking) {
		/* Generate a tracking ID, if client requested status tracking */
		nut_uuid_v4(tracking_id);
	}

	send_instcmd(client, devname, cmdname, cmdparam, tracking_id);

	return;
}
