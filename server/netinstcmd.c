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

static void send_instcmd(ctype_t *client, const char *upsname, 
	const char *cmdname)
{
	int	found;
	upstype_t	*ups;
	const	struct  cmdlist_t  *ctmp;
	char	sockcmd[SMALLBUF];

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

	upslogx(LOG_INFO, "Instant command: %s@%s did %s on %s",
		client->username, client->addr, cmdname, 
		ups->name);

	snprintf(sockcmd, sizeof(sockcmd), "INSTCMD %s\n", cmdname);

	if (!sstate_sendline(ups, sockcmd)) {
		upslogx(LOG_INFO, "Set command send failed");
		send_err(client, NUT_ERR_INSTCMD_FAILED);
		return;
	}

	sendback(client, "OK\n");
}

void net_instcmd(ctype_t *client, int numarg, const char **arg)
{
	if (numarg < 2) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}
	
	/* INSTCMD <ups> <cmdname> */
	send_instcmd(client, arg[0], arg[1]);
	return;
}
