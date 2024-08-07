/* netmisc.c - miscellaneous network handlers for upsd (VER, HELP, FSD)

   Copyright (C)
    2003  Russell Kroll <rkroll@exploits.org>
    2012  Arnaud Quette <arnaud.quette.free.fr>

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

#include "netmisc.h"

void net_ver(nut_ctype_t *client, size_t numarg, const char **arg)
{
	int	pkgurlHasNutOrg = 0;
	NUT_UNUSED_VARIABLE(arg);

	if (numarg != 0) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif
	if (PACKAGE_URL && strstr(PACKAGE_URL, "networkupstools.org")) {
		pkgurlHasNutOrg = 1;
	} else {
		upsdebugx(1, "%s: WARNING: PACKAGE_URL of this build does not point to networkupstools.org!", __func__);
	}

	/* NOTE: Some compilers deduce that macro-based decisions about
	 * NUT_VERSION_IS_RELEASE make one of codepaths unreachable in
	 * a particular build. So we pragmatically handwave this away.
	 */
	sendback(client, "Network UPS Tools upsd %s - %s%s%s\n",
		UPS_VERSION,
		PACKAGE_URL ? PACKAGE_URL : "",
		(PACKAGE_URL && !pkgurlHasNutOrg) ? " or " : "",
		pkgurlHasNutOrg ? "" : "https://www.networkupstools.org/"
		);
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic pop
#endif
}

void net_netver(nut_ctype_t *client, size_t numarg, const char **arg)
{
	NUT_UNUSED_VARIABLE(arg);
	if (numarg != 0) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	sendback(client, "%s\n", NUT_NETVERSION);
}

void net_help(nut_ctype_t *client, size_t numarg, const char **arg)
{
	NUT_UNUSED_VARIABLE(arg);
	if (numarg != 0) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	sendback(client, "Commands: HELP VER PROTVER GET LIST SET INSTCMD"
		" LOGIN LOGOUT USERNAME PASSWORD STARTTLS\n");
	/* Not exposed: PRIMARY/MASTER FSD */
}

void net_fsd(nut_ctype_t *client, size_t numarg, const char **arg)
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

	/* make sure this user is allowed to do FSD */
	if (!user_checkaction(client->username, client->password, "FSD")) {
		send_err(client, NUT_ERR_ACCESS_DENIED);
		return;
	}

	upslogx(LOG_INFO, "Client %s@%s set FSD on UPS [%s]",
		client->username, client->addr, ups->name);

	ups->fsd = 1;
	sendback(client, "OK FSD-SET\n");
}

