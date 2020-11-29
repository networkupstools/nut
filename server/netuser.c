/* netuser.c - LOGIN/LOGOUT/USERNAME/PASSWORD/MASTER handlers for upsd

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
#include "neterr.h"
#include "user.h"		/* for user_checkaction */

#include "netuser.h"

/* LOGIN <ups> */
void net_login(nut_ctype_t *client, int numarg, const char **arg)
{
	upstype_t	*ups;

	if (numarg != 1) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	if (client->loginups != NULL) {
		upslogx(LOG_INFO, "Client %s@%s tried to login twice", client->username, client->addr);
		send_err(client, NUT_ERR_ALREADY_LOGGED_IN);
		return;
	}

	/* make sure we got a valid UPS name */
	ups = get_ups_ptr(arg[0]);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	/* make sure this is a valid user */
	if (!user_checkaction(client->username, client->password, "LOGIN")) {
		send_err(client, NUT_ERR_ACCESS_DENIED);
		return;
	}

	ups->numlogins++;
	client->loginups = xstrdup(ups->name);

	upslogx(LOG_INFO, "User %s@%s logged into UPS [%s]%s", client->username, client->addr,
		client->loginups, client->ssl ? " (SSL)" : "");
	sendback(client, "OK\n");
}

void net_logout(nut_ctype_t *client, int numarg, const char **arg)
{
	NUT_UNUSED_VARIABLE(arg);
	if (numarg != 0) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	if (client->loginups != NULL) {
		upslogx(LOG_INFO, "User %s@%s logged out from UPS [%s]%s", client->username, client->addr,
			client->loginups, client->ssl ? " (SSL)" : "");
	}

	sendback(client, "OK Goodbye\n");

	client->last_heard = 0;
}

/* MASTER <upsname> */
void net_master(nut_ctype_t *client, int numarg, const char **arg)
{
	upstype_t	*ups;

	if (numarg != 1) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	ups = get_ups_ptr(arg[0]);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	/* make sure this user is allowed to do MASTER */
	if (!user_checkaction(client->username, client->password, "MASTER")) {
		send_err(client, NUT_ERR_ACCESS_DENIED);
		return;
	}

	/* this is just an access level check */
	sendback(client, "OK MASTER-GRANTED\n");
}

/* USERNAME <username> */
void net_username(nut_ctype_t *client, int numarg, const char **arg)
{
	if (numarg != 1) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	if (client->username != NULL) {
		upslogx(LOG_INFO, "Client %s@%s tried to set a username twice",
			client->username, client->addr);

		send_err(client, NUT_ERR_ALREADY_SET_USERNAME);
		return;
	}

	client->username = xstrdup(arg[0]);
	sendback(client, "OK\n");
}

/* PASSWORD <password> */
void net_password(nut_ctype_t *client, int numarg, const char **arg)
{
	if (numarg != 1) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	if (client->password != NULL) {
		if (client->username)
			upslogx(LOG_INFO, "Client %s@%s tried to set a password twice",
				client->username, client->addr);
		else
			upslogx(LOG_INFO, "Client on %s tried to set a password twice",
				client->addr);

		send_err(client, NUT_ERR_ALREADY_SET_PASSWORD);
		return;
	}

	client->password = xstrdup(arg[0]);
	sendback(client, "OK\n");
}
