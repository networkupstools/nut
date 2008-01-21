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

static void set_var(ctype_t *client, const char *upsname, const char *var,
	const char *newval)
{
	upstype_t	*ups;
	const	char	*val;
	const	struct  enum_t  *etmp;
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

	/* must be OK now */

	upslogx(LOG_INFO, "Set variable: %s@%s set %s on %s to %s",
		client->username, client->addr, var, ups->name, newval);

	snprintf(cmd, sizeof(cmd), "SET %s \"%s\"\n",
		var, pconf_encode(newval, esc, sizeof(esc)));

	if (!sstate_sendline(ups, cmd)) {
		upslogx(LOG_INFO, "Set command send failed");
		send_err(client, NUT_ERR_SET_FAILED);
		return;
	}

	sendback(client, "OK\n");
}

void net_set(ctype_t *client, int numarg, const char **arg)
{
	if (numarg < 4) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	/* SET VAR UPS VARNAME VALUE */
	if (!strcasecmp(arg[0], "VAR")) {
		set_var(client, arg[1], arg[2], arg[3]);
		return;
	}

	send_err(client, NUT_ERR_INVALID_ARGUMENT);
	return;
}
