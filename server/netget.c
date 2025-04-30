/* netget.c - GET handlers for upsd

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
#include "desc.h"
#include "neterr.h"
#include "nut_stdint.h"

#include "netget.h"

static void get_numlogins(nut_ctype_t *client, const char *upsname)
{
	const	upstype_t	*ups;

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (!ups_available(ups, client))
		return;

	sendback(client, "NUMLOGINS %s %d\n", upsname, ups->numlogins);
}

static void get_upsdesc(nut_ctype_t *client, const char *upsname)
{
	const	upstype_t	*ups;
	char	esc[SMALLBUF];

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (ups->desc) {
		pconf_encode(ups->desc, esc, sizeof(esc));
		sendback(client, "UPSDESC %s \"%s\"\n", upsname, esc);

	} else {

		sendback(client, "UPSDESC %s \"Unavailable\"\n", upsname);
	}
}

static void get_desc(nut_ctype_t *client, const char *upsname, const char *var)
{
	const	upstype_t	*ups;
	const	char	*desc;

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (!ups_available(ups, client))
		return;

	desc = desc_get_var(var);

	if (desc)
		sendback(client, "DESC %s %s \"%s\"\n", upsname, var, desc);
	else
		sendback(client, "DESC %s %s \"Description unavailable\"\n", upsname, var);
}

static void get_cmddesc(nut_ctype_t *client, const char *upsname, const char *cmd)
{
	const	upstype_t	*ups;
	const	char	*desc;

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (!ups_available(ups, client))
		return;

	desc = desc_get_cmd(cmd);

	if (desc)
		sendback(client, "CMDDESC %s %s \"%s\"\n", upsname, cmd, desc);
	else
		sendback(client, "CMDDESC %s %s \"Description unavailable\"\n",
			upsname, cmd);
}

static void get_type(nut_ctype_t *client, const char *upsname, const char *var)
{
	char	buf[SMALLBUF];
	const	upstype_t	*ups;
	const	st_tree_t	*node;

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

	snprintf(buf, sizeof(buf), "TYPE %s %s", upsname, var);

	if (node->flags & ST_FLAG_IMMUTABLE) {
#if defined DEBUG && DEBUG
		/* Properly exposing this needs also an update to
		 * docs/net-protocol.txt (promote the paragraph
		 * provided as a note currently) and to the NUT RFC
		 * https://www.rfc-editor.org/info/rfc9271
		 */
		snprintfcat(buf, sizeof(buf), " IMMUTABLE");
#endif
		upsdebugx(3, "%s: UPS[%s] variable %s is IMMUTABLE",
			__func__, upsname, var);
	}

	if (node->flags & ST_FLAG_RW)
		snprintfcat(buf, sizeof(buf), " RW");

	if (node->enum_list) {
		snprintfcat(buf, sizeof(buf), " ENUM");
	}

	if (node->range_list) {
		snprintfcat(buf, sizeof(buf), " RANGE");
	}

	if (node->flags & ST_FLAG_STRING) {
		sendback(client, "%s STRING:%ld\n", buf, node->aux);
		return;
	}

	/* Any variable that is not string | range | enum is just a simple
	 * numeric value */

	if (!(node->flags & ST_FLAG_NUMBER)) {
		upsdebugx(3, "%s: assuming that UPS[%s] variable %s which has no type flag is a NUMBER",
			__func__, upsname, var);
	}

	/* Sanity-check current contents */
	if (node->val && *(node->val)) {
		double	d;
		long	l;
		int	ok = 1;
		size_t	len = strlen(node->val);

		errno = 0;
		if (!str_to_double_strict(node->val, &d, 10)) {
			upsdebugx(3, "%s: UPS[%s] variable %s is flagged a NUMBER but not (exclusively) a double: %s",
				__func__, upsname, var, node->val);
			upsdebug_with_errno(4, "%s: val=%f len=%" PRIuSIZE,
				__func__, d, len);
			ok = 0;
		}

		if (!ok) {
			/* did not parse as a float... range issues or NaN? */
			errno = 0;
			ok = 1;
			if (!str_to_long_strict(node->val, &l, 10)) {
				upsdebugx(3, "%s: UPS[%s] variable %s is flagged a NUMBER but not (exclusively) a long int: %s",
					__func__, upsname, var, node->val);
				upsdebug_with_errno(4, "%s: val=%ld len=%" PRIuSIZE,
					__func__, l, len);
				ok = 0;
			}
		}

#if defined DEBUG && DEBUG
		/* Need to figure out an "aux" value here (length of current
		 * string at least?) and propagate the flag into where netset
		 * would see it. Maybe this sanity-check should move into the
		 * core state.c logic, so dstate setting would already remember
		 * the defaulted flag (and maybe set another to clarify it is
		 * a guess). Currently that code does not concern itself with
		 * sanity-checks, it seems!
		 */
		if (!ok && !(node->flags & ST_FLAG_NUMBER)) {
			upsdebugx(3, "%s: assuming UPS[%s] variable %s is a STRING after all, by contents; "
				"value='%s' len='%" PRIuSIZE "' aux='%ld'",
				__func__, upsname, var, node->val, len, node->aux);

			sendback(client, "%s STRING:%ld\n", buf, node->aux);
			return;
		}
#endif

		if (!ok) {
			/* FIXME: Should this return an error?
			 * Value was explicitly flagged as a NUMBER but is not by content.
			 * Note that state_addinfo() does not sanity-check; but
			 * netset.c::set_var() does though (for protocol clients).
			 */
			upslogx(LOG_WARNING, "%s: UPS[%s] variable %s is flagged as a NUMBER but is not "
				"by contents (please report as a bug to NUT project)): %s",
				__func__, upsname, var, node->val);
		}
	}

	sendback(client, "%s NUMBER\n", buf);
}

static void get_var_server(nut_ctype_t *client, const char *upsname, const char *var)
{
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
	int	pkgurlHasNutOrg = PACKAGE_URL ? (strstr(PACKAGE_URL, "networkupstools.org") != NULL) : 0;

	if (!strcasecmp(var, "server.info")) {
		/* NOTE: Some compilers deduce that macro-based decisions about
		 * NUT_VERSION_IS_RELEASE make one of codepaths unreachable in
		 * a particular build. So we pragmatically handwave this away.
		 */
		sendback(client, "VAR %s server.info "
			"\"Network UPS Tools upsd %s - "
			"%s%s%s\"\n",
			upsname, UPS_VERSION,
			PACKAGE_URL ? PACKAGE_URL : "",
			(PACKAGE_URL && !pkgurlHasNutOrg) ? " or " : "",
			pkgurlHasNutOrg ? "" : "https://www.networkupstools.org/"
			);
		return;
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic pop
#endif

	if (!strcasecmp(var, "server.version")) {
		sendback(client, "VAR %s server.version \"%s\"\n",
			upsname, UPS_VERSION);
		return;
	}

	send_err(client, NUT_ERR_VAR_NOT_SUPPORTED);
}

static void get_var(nut_ctype_t *client, const char *upsname, const char *var)
{
	const	upstype_t	*ups;
	const	char	*val;

	/* ignore upsname for server.* variables */
	if (!strncasecmp(var, "server.", 7)) {
		get_var_server(client, upsname, var);
		return;
	}

	ups = get_ups_ptr(upsname);

	if (!ups) {
		send_err(client, NUT_ERR_UNKNOWN_UPS);
		return;
	}

	if (!ups_available(ups, client))
		return;

	val = sstate_getinfo(ups, var);

	if (!val) {
		send_err(client, NUT_ERR_VAR_NOT_SUPPORTED);
		return;
	}

	/* handle special case for status */
	if ((!strcasecmp(var, "ups.status")) && (ups->fsd))
		sendback(client, "VAR %s %s \"FSD %s\"\n", upsname, var, val);
	else
		sendback(client, "VAR %s %s \"%s\"\n", upsname, var, val);
}

void net_get(nut_ctype_t *client, size_t numarg, const char **arg)
{
	if (numarg < 1) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	/* GET TRACKING [ID] */
	if (!strcasecmp(arg[0], "TRACKING")) {
		if (numarg < 2) {
			sendback(client, "%s\n", (client->tracking) ? "ON" : "OFF");
		}
		else {
			if (client->tracking)
				sendback(client, "%s\n", tracking_get(arg[1]));
			else
				send_err(client, NUT_ERR_FEATURE_NOT_CONFIGURED);
		}
		return;
	}

	if (numarg < 2) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	/* GET NUMLOGINS UPS */
	if (!strcasecmp(arg[0], "NUMLOGINS")) {
		get_numlogins(client, arg[1]);
		return;
	}

	/* GET UPSDESC UPS */
	if (!strcasecmp(arg[0], "UPSDESC")) {
		get_upsdesc(client, arg[1]);
		return;
	}

	if (numarg < 3) {
		send_err(client, NUT_ERR_INVALID_ARGUMENT);
		return;
	}

	/* GET VAR UPS VARNAME */
	if (!strcasecmp(arg[0], "VAR")) {
		get_var(client, arg[1], arg[2]);
		return;
	}

	/* GET TYPE UPS VARNAME */
	if (!strcasecmp(arg[0], "TYPE")) {
		get_type(client, arg[1], arg[2]);
		return;
	}

	/* GET DESC UPS VARNAME */
	if (!strcasecmp(arg[0], "DESC")) {
		get_desc(client, arg[1], arg[2]);
		return;
	}

	/* GET CMDDESC UPS CMDNAME */
	if (!strcasecmp(arg[0], "CMDDESC")) {
		get_cmddesc(client, arg[1], arg[2]);
		return;
	}

	send_err(client, NUT_ERR_INVALID_ARGUMENT);
	return;
}
