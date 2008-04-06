/* netxml-ups.c	Driver routines for network XML UPS units 

   Copyright (C)
	2008		Arjen de Korte <adkorte-guest@alioth.debian.org>

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ne_request.h>
#include <ne_basic.h>
#include <ne_props.h>
#include <ne_uri.h>
#include <ne_xmlreq.h>
#include <ne_ssl.h>
#include <ne_auth.h>

#include "main.h"

#include "netxml-ups.h"
#include "mge-xml.h"

#define DRV_VERSION	"0.12"
#define MAXRETRIES	5

#ifdef DEBUG
#define difftimeval(x,y)	((double)y.tv_sec-x.tv_sec+((double)(y.tv_usec-x.tv_usec)/1000000))
#endif

/* Global vars */
uint32_t		ups_status = 0;
static int		timeout = 5;
static subdriver_t	*subdriver = &mge_xml_subdriver;
static ne_session	*session = NULL;
static ne_uri		uri;

/* Support functions */
static void ups_alarm_set(void);
static void ups_status_set(void);
static int authenticate(void *userdata, const char *realm, int attempt, char *username, char *password);
static int dispatch_request(ne_request *request, ne_xml_parser *parser);

void upsdrv_initinfo(void)
{
	int		ret;
	ne_request	*request;
	ne_xml_parser	*parser;

	upsdebugx(3, "ne_request_create(session, \"GET\", \"%s\");", subdriver->initinfo);

	if (strlen(subdriver->initinfo) < 1) {
		fatalx(EXIT_FAILURE, "%s: failure to read initinfo element", __func__);
	}

	request = ne_request_create(session, "GET", subdriver->initinfo);

	/* Create an XML parser. */
	parser = ne_xml_create();

	/* Push a new handler on the parser stack */
	ne_xml_push_handler(parser, subdriver->startelm_cb, subdriver->cdata_cb, subdriver->endelm_cb, NULL);

	ret = dispatch_request(request, parser);

	ne_xml_destroy(parser);
	ne_request_destroy(request);

	if (ret != NE_OK) {
		fatalx(EXIT_FAILURE, "%s: communication failure [%s]", __func__, ne_get_error(session));
	}

	if (!subdriver->getobject) {
		fatalx(EXIT_FAILURE, "%s: found no way to get variables", __func__);
	}

	if (!subdriver->setobject) {
		upsdebugx(2, "%s: found no way to set variables", __func__);
	}

	dstate_setinfo("driver.version.internal", "%s", subdriver->version);

	/* upsh.instcmd = instcmd; */
	/* upsh.setvar = setvar; */
}

void upsdrv_updateinfo(void)
{
	static int	retries = 0;
	int		ret;
	ne_request	*request;
	ne_xml_parser	*parser;
#ifdef DEBUG
	struct timeval	start, stop;
#endif

	upsdebugx(3, "ne_request_create(session, \"GET\", \"%s\");", subdriver->getobject);

	if (strlen(subdriver->getobject) < 1) {
		fatalx(EXIT_FAILURE, "%s: failure to read getobject element", __func__);
	}

	request = ne_request_create(session, "GET", subdriver->getobject);

	/* Create an XML parser. */
	parser = ne_xml_create();

	/* Push a new handler on the parser stack */
	ne_xml_push_handler(parser, subdriver->startelm_cb, subdriver->cdata_cb, subdriver->endelm_cb, NULL);

#ifdef DEBUG
	gettimeofday(&start, NULL);
#endif
	ret = dispatch_request(request, parser);
#ifdef DEBUG
	gettimeofday(&stop, NULL);

	upsdebugx(1, "dispatch request (%.3f seconds)", difftimeval(start, stop));
#endif

	ne_xml_destroy(parser);
	ne_request_destroy(request);

	if (ret != NE_OK) {
#ifdef DEBUG
		upslogx(LOG_ERR, "%s (%.3f seconds)", ne_get_error(session), difftimeval(start, stop));
#endif
		if (retries < MAXRETRIES) {
			upsdebugx(1, "%s (%d from %d)", ne_get_error(session), retries, MAXRETRIES);
			retries++;
		} else {
			upslogx(LOG_ERR, "%s", ne_get_error(session));
			dstate_datastale();
		}

		return;
	}

	retries = 0;

	status_init();

	alarm_init();
	ups_alarm_set();
	alarm_commit();

	ups_status_set();
	status_commit();

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */

	/* maybe try to detect the UPS here, but try a shutdown even if
	   it doesn't respond at first if possible */

	/* replace with a proper shutdown function */
	fatalx(EXIT_FAILURE, "shutdown not supported");

	/* you may have to check the line status since the commands
	   for toggling power are frequently different for OL vs. OB */

	/* OL: this must power cycle the load if possible */

	/* OB: the load must remain off until the power returns */
}

/*
static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send_buf(upsfd, ...);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}
*/

/*
static int setvar(const char *varname, const char *val)
{
	if (!strcasecmp(varname, "ups.test.interval")) {
		ser_send_buf(upsfd, ...);
		return STAT_SET_HANDLED;
	}

	upslogx(LOG_NOTICE, "setvar: unknown variable [%s]", varname);
	return STAT_SET_UNKNOWN;
}
*/

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	char	buf[SMALLBUF];

	snprintf(buf, sizeof(buf), "network timeout (default %d seconds)", timeout);
	addvar(VAR_VALUE, "timeout", buf);

	addvar(VAR_VALUE, "login", "login value for authenticated mode");
	addvar(VAR_VALUE, "password", "password value for authenticated mode");
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - network XML UPS driver %s (%s)\n\n",
		DRV_VERSION, UPS_VERSION);

	experimental_driver = 1;
}

void upsdrv_initups(void)
{
	FILE	*fp;
	int	ret;
	char	*val;

	/* allow override of default network timeout value */
	val = getval("timeout");

	if (val) {
		timeout = atoi(val);
	}

	/* Initialize socket libraries */
	if (ne_sock_init()) {
		fatalx(EXIT_FAILURE, "%s: failed to initialize socket libraries", progname);
	}

	/* Parse the URI argument. */
	if (ne_uri_parse(device_path, &uri) || uri.host == NULL) {
		fatalx(EXIT_FAILURE, "%s: invalid hostname '%s'", progname, device_path);
	}
/*
	if (uri.scheme == NULL) {
		uri.scheme = strdup("http");
	}
 
	if (uri.host == NULL) {
		uri.host = strdup(device_path);
	}
 */
	if (uri.port == 0) {
		uri.port = ne_uri_defaultport(uri.scheme);
	}

	upsdebugx(1, "using %s://%s port %d", uri.scheme, uri.host, uri.port);

	/* create the session */
	session = ne_session_create(uri.scheme, uri.host, uri.port);

	/* allow the first connection some extra time, as
	   we may need to resolve the name of the UPS first */
	ne_set_connect_timeout(session, 60);

	/* just wait for a couple of seconds */
	ne_set_read_timeout(session, timeout);

	/* Sets the user-agent string */
	ne_set_useragent(session, subdriver->version);

	if (strcasecmp(uri.scheme, "https") == 0) {
		/* Load default CAs if using SSL. */
		ne_ssl_trust_default_ca(session);
	}

	ne_set_server_auth(session, authenticate, NULL);

	/* if debug level is set, direct output to stderr */
	if (!nut_debug_level) {
		fp = fopen("/dev/null", "w");
	} else {
		fp = stderr;
	}

	if (!fp) {
		upslog_with_errno(LOG_INFO, "Connectivity test skipped");
		return;
	}

	/* see if we have a connection */
	ret = ne_get(session, subdriver->initups, fileno(fp));

	if (!nut_debug_level) {
		fclose(fp);
	} else {
		fprintf(fp, "\n...done\n");
	}

	if (ret != NE_OK) {
		fatalx(LOG_INFO, "Connectivity test: %s", ne_get_error(session));
	}

	upslogx(LOG_INFO, "Connectivity test: %s", ne_get_error(session));

	/* following connects shouldn't require DNS queries anymore */
	ne_set_connect_timeout(session, timeout);
}

void upsdrv_cleanup(void)
{
	free(subdriver->getobject);
	free(subdriver->setobject);

	if (session) {
		ne_session_destroy(session);
	}

	ne_uri_free(&uri);
}

/**********************************************************************
 * Support functions
 *********************************************************************/

/* Starting with neon-0.27.0 the ne_dispatch_request() function will check
   for a valid XML content-type (following RFC 3023 rules) in the header.
   Unfortunately, (at least) the Transverse NMC doesn't follow this RFC, so
   we can't use this anymore and we'll have to roll our own here. */
static int dispatch_request(ne_request *request, ne_xml_parser *parser)
{
	int ret;

	do {
		ret = ne_begin_request(request);

		if (ret != NE_OK) {
			break;
		}

		ret = ne_xml_parse_response(request, parser);

		if (ret == NE_OK) {
			ret = ne_end_request(request);
		}

	} while (ret == NE_RETRY);

	return ret;
}

/* Supply the 'login' and 'password' when authentication is required */
static int authenticate(void *userdata, const char *realm, int attempt, char *username, char *password)
{
	char	*val;

	upsdebugx(2, "%s: realm = [%s], attempt = %d", __func__, realm, attempt);

	val = getval("login");
	snprintf(username, NE_ABUFSIZ, "%s", val ? val : "");

	val = getval("password");
	snprintf(password, NE_ABUFSIZ, "%s", val ? val : "");

	return attempt;
}

/* Convert the local status information to NUT format and set NUT
   alarms. */
static void ups_alarm_set(void)
{
	if (STATUS_BIT(REPLACEBATT)) {
		alarm_set("Replace battery!");
	}
	if (STATUS_BIT(SHUTDOWNIMM)) {
		alarm_set("Shutdown imminent!");
	}
	if (STATUS_BIT(FANFAIL)) {
		alarm_set("Fan failure!");
	}
	if (STATUS_BIT(NOBATTERY)) {
		alarm_set("No battery installed!");
	}
	if (STATUS_BIT(BATTVOLTLO)) {
		alarm_set("Battery voltage too low!");
	}
	if (STATUS_BIT(BATTVOLTHI)) {
		alarm_set("Battery voltage too high!");
	}
	if (STATUS_BIT(CHARGERFAIL)) {
		alarm_set("Battery charger fail!");
	}
	if (STATUS_BIT(OVERHEAT)) {
		alarm_set("Temperature too high!");
	}
	if (STATUS_BIT(COMMFAULT)) {
		alarm_set("Internal UPS fault!");
	}
	if (STATUS_BIT(AWAITINGPOWER)) {
		alarm_set("Awaiting power!");
	}
	if (STATUS_BIT(FUSEFAULT)) {
		alarm_set("Fuse fault!");
	}
	if (STATUS_BIT(BYPASSAUTO)) {
		alarm_set("Automatic bypass mode!");
	}
	if (STATUS_BIT(BYPASSMAN)) {
		alarm_set("Manual bypass mode!");
	}
}

/* Convert the local status information to NUT format and set NUT
   status. */
static void ups_status_set(void)
{
	if (STATUS_BIT(VRANGE)) {
		dstate_setinfo("input.transfer.reason", "input voltage out of range");
	} else if (STATUS_BIT(FRANGE)) {
		dstate_setinfo("input.transfer.reason", "input frequency out of range");
	} else {
		dstate_delinfo("input.transfer.reason");
	}

	if (STATUS_BIT(ONLINE)) {
		status_set("OL");		/* on line */
	} else {
		status_set("OB");		/* on battery */
	}
	if (STATUS_BIT(DISCHRG) && !STATUS_BIT(DEPLETED)) {
		status_set("DISCHRG");		/* discharging */
	}
	if (STATUS_BIT(CHRG) && !STATUS_BIT(FULLYCHARGED)) {
		status_set("CHRG");		/* charging */
	}
	if (STATUS_BIT(LOWBATT) || STATUS_BIT(TIMELIMITEXP) || STATUS_BIT(SHUTDOWNIMM)) {
		status_set("LB");		/* low battery */
	}
	if (STATUS_BIT(OVERLOAD)) {
		status_set("OVER");		/* overload */
	}
	if (STATUS_BIT(REPLACEBATT)) {
		status_set("RB");		/* replace batt */
	}
	if (STATUS_BIT(TRIM)) {
		status_set("TRIM");		/* SmartTrim */
	}
	if (STATUS_BIT(BOOST)) {
		status_set("BOOST");		/* SmartBoost */
	}
	if (STATUS_BIT(BYPASSAUTO) || STATUS_BIT(BYPASSMAN)) {
		status_set("BYPASS");		/* on bypass */
	}
	if (STATUS_BIT(OFF)) {
		status_set("OFF");		/* ups is off */
	}
	if (STATUS_BIT(CAL)) {
		status_set("CAL");		/* calibration */
	}
}
