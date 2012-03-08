/* netxml-ups.c	Driver routines for network XML UPS units 

   Copyright (C)
	2008-2009	Arjen de Korte <adkorte-guest@alioth.debian.org>

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

#include "main.h"
#include "netxml-ups.h"
#include "mge-xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ne_request.h>
#include <ne_basic.h>
#include <ne_props.h>
#include <ne_uri.h>
#include <ne_xml.h>
#include <ne_xmlreq.h>
#include <ne_ssl.h>
#include <ne_auth.h>
#include <ne_socket.h>

#define DRIVER_NAME	"network XML UPS"
#define DRIVER_VERSION	"0.30"
#ifdef WIN32 /* FIXME ?? skip alarm handling */
#define HAVE_NE_SET_CONNECT_TIMEOUT  1
#define HAVE_NE_SOCK_CONNECT_TIMEOUT 1
#endif

/* driver description structure */
upsdrv_info_t	upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arjen de Korte <adkorte-guest@alioth.debian.org>",
	DRV_EXPERIMENTAL,
	{ NULL }
};
/* FIXME:
 * "built with neon library %s" LIBNEON_VERSION 
 * subdrivers (limited to MGE only ATM) */

/* Global vars */
uint32_t		ups_status = 0;
static int		timeout = 5;
static time_t		lastheard = 0;
static subdriver_t	*subdriver = &mge_xml_subdriver;
static ne_session	*session = NULL;
static ne_socket	*sock = NULL;
static ne_uri		uri;

/* Support functions */
static void netxml_alarm_set(void);
static void netxml_status_set(void);
static int netxml_authenticate(void *userdata, const char *realm, int attempt, char *username, char *password);
static int netxml_dispatch_request(ne_request *request, ne_xml_parser *parser);
static int netxml_get_page(const char *page);

static int netxml_alarm_subscribe(const char *page);

#if HAVE_NE_SET_CONNECT_TIMEOUT && HAVE_NE_SOCK_CONNECT_TIMEOUT
	/* we don't need to use alarm() */
#else
static void netxml_alarm_handler(int sig)
{
	/* don't do anything here, just return */
}
#endif

void upsdrv_initinfo(void)
{
	char	*page, *last = NULL;
	char	buf[SMALLBUF];
	
	snprintf(buf, sizeof(buf), "%s", subdriver->initinfo);

	for (page = strtok_r(buf, " ", &last); page != NULL; page = strtok_r(NULL, " ", &last)) {

		if (netxml_get_page(page) != NE_OK) {
			continue;
		}

		dstate_setinfo("driver.version.internal", "%s", subdriver->version);

		if (testvar("subscribe") && (netxml_alarm_subscribe(subdriver->subscribe) == NE_OK)) {
			extrafd = ne_sock_fd(sock);
			time(&lastheard);
		}

		return;
	}

	fatalx(EXIT_FAILURE, "%s: communication failure [%s]", __func__, ne_get_error(session));
}

void upsdrv_updateinfo(void)
{
	int	ret, errors = 0;

	/* We really should be dealing with alarms through a separate callback, so that we can keep the
	 * processing of alarms and polling for data separated. Currently, this isn't supported by the
	 * driver main body, so we'll have to revert to polling each time we're called, unless the
	 * socket indicates we're no longer connected.
	 */
	if (testvar("subscribe")) {
		char	buf[LARGEBUF];

		ret = ne_sock_read(sock, buf, sizeof(buf));

		if (ret > 0) {
			/* alarm message received */

			ne_xml_parser	*parser = ne_xml_create();
			upsdebugx(2, "%s: ne_sock_read(%d bytes) => %s", __func__, ret, buf);
			ne_xml_push_handler(parser, subdriver->startelm_cb, subdriver->cdata_cb, subdriver->endelm_cb, NULL);
			ne_xml_parse(parser, buf, strlen(buf));
			ne_xml_destroy(parser);
			time(&lastheard);

		} else if ((ret == NE_SOCK_TIMEOUT) && (difftime(time(NULL), lastheard) < 180)) {
			/* timed out */

			upsdebugx(2, "%s: ne_sock_read(timeout)", __func__);

		} else {
			/* connection closed or unknown error */

			upslogx(LOG_ERR, "NSM connection with '%s' lost", uri.host);

			upsdebugx(2, "%s: ne_sock_read(%d) => %s", __func__, ret, ne_sock_error(sock));
			ne_sock_close(sock);

			if (netxml_alarm_subscribe(subdriver->subscribe) == NE_OK) {
				extrafd = ne_sock_fd(sock);
				time(&lastheard);
				return;
			}

			dstate_datastale();
			extrafd = -1;
			return;
		}
	}

	/* get additional data */
	ret = netxml_get_page(subdriver->getobject);
	if (ret != NE_OK) {
		errors++;
	}

	ret = netxml_get_page(subdriver->summary);
	if (ret != NE_OK) {
		errors++;
	}

	if (errors > 1) {
		dstate_datastale();
		return;
	}

	status_init();

	alarm_init();
	netxml_alarm_set();
	alarm_commit();

	netxml_status_set();
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

	upslogx(LOG_NOTICE, "%s: unknown command [%s]", __func__, cmdname);
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

	upslogx(LOG_NOTICE, "%s: unknown variable [%s]", __func__, varname);
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

	snprintf(buf, sizeof(buf), "network timeout (default: %d seconds)", timeout);
	addvar(VAR_VALUE, "timeout", buf);

	addvar(VAR_FLAG, "subscribe", "NSM subscribe to NMC");

	addvar(VAR_VALUE | VAR_SENSITIVE, "login", "login value for authenticated mode");
	addvar(VAR_VALUE | VAR_SENSITIVE, "password", "password value for authenticated mode");
}

void upsdrv_initups(void)
{
	int	ret;
	char	*val;
	FILE	*fp;

#if HAVE_NE_SET_CONNECT_TIMEOUT && HAVE_NE_SOCK_CONNECT_TIMEOUT
	/* we don't need to use alarm() */
#else
	struct sigaction	sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sa.sa_handler = netxml_alarm_handler;
	sigaction(SIGALRM, &sa, NULL);
#endif
	/* allow override of default network timeout value */
	val = getval("timeout");
	if (val) {
		timeout = atoi(val);

		if (timeout < 1) {
			fatalx(EXIT_FAILURE, "timeout must be greater than 0");
		}
	}

	if (nut_debug_level > 5) {
		ne_debug_init(stderr, NE_DBG_HTTP | NE_DBG_HTTPBODY);
	}

	if (ne_sock_init()) {
		fatalx(EXIT_FAILURE, "%s: failed to initialize socket libraries", progname);
	}

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

	session = ne_session_create(uri.scheme, uri.host, uri.port);
	
	/* timeout if we can't (re)connect to the UPS */
#ifdef HAVE_NE_SET_CONNECT_TIMEOUT
	ne_set_connect_timeout(session, timeout);
#endif

	/* just wait for a couple of seconds */
	ne_set_read_timeout(session, timeout);

	ne_set_useragent(session, subdriver->version);

	if (strcasecmp(uri.scheme, "https") == 0) {
		ne_ssl_trust_default_ca(session);
	}

	ne_set_server_auth(session, netxml_authenticate, NULL);

	/* if debug level is set, direct output to stderr */
	if (!nut_debug_level) {
#ifndef WIN32
		fp = fopen("/dev/null", "w");
#else
		fp = fopen("nul", "w");
#endif
	} else {
		fp = stderr;
	}

	if (!fp) {
		fatal_with_errno(EXIT_FAILURE, "Connectivity test failed");
	}

	/* see if we have a connection */
	ret = ne_get(session, subdriver->initups, fileno(fp));

	if (!nut_debug_level) {
		fclose(fp);
	} else {
		fprintf(fp, "\n");
	}

	if (ret != NE_OK) {
		fatalx(EXIT_FAILURE, "Connectivity test: %s", ne_get_error(session));
	}

	upslogx(LOG_INFO, "Connectivity test: %s", ne_get_error(session));
}

void upsdrv_cleanup(void)
{
	free(subdriver->configure);
	free(subdriver->subscribe);
	free(subdriver->summary);
	free(subdriver->getobject);
	free(subdriver->setobject);

	if (sock) {
		ne_sock_close(sock);
	}

	if (session) {
		ne_session_destroy(session);
	}

	ne_uri_free(&uri);
}

/**********************************************************************
 * Support functions
 *********************************************************************/

static int netxml_get_page(const char *page)
{
	int		ret;
	ne_request	*request;
	ne_xml_parser	*parser;

	upsdebugx(2, "%s: %s", __func__, page);

	request = ne_request_create(session, "GET", page);

	parser = ne_xml_create();

	ne_xml_push_handler(parser, subdriver->startelm_cb, subdriver->cdata_cb, subdriver->endelm_cb, NULL);

	ret = netxml_dispatch_request(request, parser);

	if (ret) {
		upsdebugx(2, "%s: %s", __func__, ne_get_error(session));
	}

	ne_xml_destroy(parser);
	ne_request_destroy(request);

	return ret;
}

static int netxml_alarm_subscribe(const char *page)
{
	int	ret, port = -1, secret = -1;
	char	buf[LARGEBUF], *s;
	ne_request	*request;
	ne_sock_addr	*addr;
	const ne_inet_addr	*ai;

	upsdebugx(2, "%s: %s", __func__, page);

	sock = ne_sock_create();

	if (gethostname(buf, sizeof(buf)) == 0) {
		dstate_setinfo("driver.hostname", "%s", buf);
	} else {
		dstate_setinfo("driver.hostname", "<unknown>");
	}

#ifdef HAVE_NE_SOCK_CONNECT_TIMEOUT
	ne_sock_connect_timeout(sock, timeout);
#endif
	ne_sock_read_timeout(sock, 1);

	netxml_get_page(subdriver->configure);

	snprintf(buf, sizeof(buf),	"<?xml version=\"1.0\">\n");
	snprintfcat(buf, sizeof(buf),	"<Subscribe>\n");
	snprintfcat(buf, sizeof(buf),		"<Class>%s v%s</Class>\n", progname, DRIVER_VERSION);
	snprintfcat(buf, sizeof(buf),		"<Type>connected socket</Type>\n");
	snprintfcat(buf, sizeof(buf),		"<HostName>%s</HostName>\n", dstate_getinfo("driver.hostname"));
	snprintfcat(buf, sizeof(buf),		"<XMLClientParameters>\n");
	snprintfcat(buf, sizeof(buf),			"<ShutdownDuration>%s</ShutdownDuration>\n", dstate_getinfo("driver.delay.shutdown"));
	snprintfcat(buf, sizeof(buf),			"<ShutdownTimer>%s</ShutdownTimer>\n", dstate_getinfo("driver.timer.shutdown"));
	snprintfcat(buf, sizeof(buf),			"<AutoConfig>CENTRALIZED</AutoConfig>\n");
	snprintfcat(buf, sizeof(buf),			"<OutletGroup>1</OutletGroup>\n");
	snprintfcat(buf, sizeof(buf),		"</XMLClientParameters>\n");
/*	snprintfcat(buf, sizeof(buf),		"<Warning>NUT driver</Warning>\n"); */
	snprintfcat(buf, sizeof(buf),	"</Subscribe>\n");

	/* now send subscription message setting all the proper flags */
	request = ne_request_create(session, "POST", page);
	ne_set_request_body_buffer(request, buf, strlen(buf));

	/* as the NMC reply is not xml standard compliant let's parse it this way */
	do {
#ifndef HAVE_NE_SOCK_CONNECT_TIMEOUT
		alarm(timeout+1);
#endif
		ret = ne_begin_request(request);

#ifndef HAVE_NE_SOCK_CONNECT_TIMEOUT
		alarm(0);
#endif
		if (ret != NE_OK) {
			break;
		}

		ret = ne_read_response_block(request, buf, sizeof buf);

		if (ret == NE_OK) {
			ret = ne_end_request(request);
		}

	} while (ret == NE_RETRY);

	ne_request_destroy(request);

	/* due to different formats used by the various NMCs, we need to\
	   break up the reply in lines and parse each one separately */
	for (s = strtok(buf, "\r\n"); s != NULL; s = strtok(NULL, "\r\n")) {
		upsdebugx(2, "%s: parsing %s", __func__, s);

		if (!strncasecmp(s, "<Port>", 6) && (sscanf(s+6, "%u", &port) != 1)) {
			return NE_RETRY;
		}

		if (!strncasecmp(s, "<Secret>", 8) && (sscanf(s+8, "%u", &secret) != 1)) {
			return NE_RETRY;
		}
	}

	if ((port == -1) || (secret == -1)) {
		upsdebugx(2, "%s: parsing initial subcription failed", __func__);
		return NE_RETRY;
	}

	/* Resolve the given hostname.  'flags' must be zero.  Hex
	* string IPv6 addresses (e.g. `::1') may be enclosed in brackets
	* (e.g. `[::1]'). */
	addr = ne_addr_resolve(uri.host, 0);

	/* Returns zero if name resolution was successful, non-zero on
	* error. */
	if (ne_addr_result(addr) != 0) {
		upsdebugx(2, "%s: name resolution failure on %s: %s", __func__, uri.host, ne_addr_error(addr, buf, sizeof(buf)));
		ne_addr_destroy(addr);
		return NE_RETRY;
	}

	for (ai = ne_addr_first(addr); ai != NULL; ai = ne_addr_next(addr)) {

		upsdebugx(2, "%s: connecting to host %s port %d", __func__, ne_iaddr_print(ai, buf, sizeof(buf)), port);

#ifndef HAVE_NE_SOCK_CONNECT_TIMEOUT
		alarm(timeout+1);
#endif
		ret = ne_sock_connect(sock, ai, port);

#ifndef HAVE_NE_SOCK_CONNECT_TIMEOUT
		alarm(0);
#endif
		if (ret == NE_OK) {
			upsdebugx(2, "%s: connection to %s open on fd %d", __func__, uri.host, ne_sock_fd(sock));
			break;
		}
	}

	ne_addr_destroy(addr);

	if (ai == NULL) {
		upsdebugx(2, "%s: failed to create listening socket", __func__);
		return NE_RETRY;
	}

	snprintf(buf, sizeof(buf), "<Subscription Identification=\"%u\"></Subscription>\n", secret);
	ret = ne_sock_fullwrite(sock, buf, strlen(buf));

	if (ret != NE_OK) {
		upsdebugx(2, "%s: send failed: %s", __func__, ne_sock_error(sock));
		return NE_RETRY;
	}

	ret = ne_sock_read(sock, buf, sizeof(buf));

	if (ret < 1) {
		upsdebugx(2, "%s: read failed: %s", __func__, ne_sock_error(sock));
		return NE_RETRY;
	}

	if (strcasecmp(buf, "<Subscription Answer=\"ok\"></Subscription>")) {
		upsdebugx(2, "%s: subscription rejected", __func__);
		return NE_RETRY;
	}

	upslogx(LOG_INFO, "NSM connection to '%s' established", uri.host);
	return NE_OK;
}

static int netxml_dispatch_request(ne_request *request, ne_xml_parser *parser)
{
	int ret;

	/*
	 * Starting with neon-0.27.0 the ne_xml_dispatch_request() function will check
	 * for a valid XML content-type (following RFC 3023 rules) in the header.
	 * Unfortunately, (at least) the Transverse NMC doesn't follow this RFC, so
	 * we can't use this anymore and we'll have to roll our own here.
	 */
	do {
#ifndef HAVE_NE_SET_CONNECT_TIMEOUT
		alarm(timeout+1);
#endif
		ret = ne_begin_request(request);

#ifndef HAVE_NE_SET_CONNECT_TIMEOUT
		alarm(0);
#endif
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
static int netxml_authenticate(void *userdata, const char *realm, int attempt, char *username, char *password)
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
static void netxml_alarm_set(void)
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
		alarm_set("Communication fault!");
	}
	if (STATUS_BIT(INTERNALFAULT)) {
		alarm_set("Internal UPS fault!");
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
static void netxml_status_set(void)
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
	if (STATUS_BIT(DISCHRG)) {
		status_set("DISCHRG");		/* discharging */
	}
	if (STATUS_BIT(CHRG)) {
		status_set("CHRG");		/* charging */
	}
	if (STATUS_BIT(LOWBATT)) {
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

	if (STATUS_BIT(SHUTDOWNIMM)) {
		status_set("FSD");		/* shutdown imminent */
	}
}

