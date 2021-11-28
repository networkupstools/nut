/* netxml-ups.c	Driver routines for network XML UPS units

   Copyright (C)
	2008-2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
	2013		Vaclav Krpec <VaclavKrpec@Eaton.com>

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
#include "dstate.h"

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

#include "nut_stdint.h"

#define DRIVER_NAME	"network XML UPS"
#define DRIVER_VERSION	"0.43"

/** *_OBJECT query multi-part body boundary */
#define FORM_POST_BOUNDARY "NUT-NETXML-UPS-OBJECTS"

/* driver description structure */
upsdrv_info_t	upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arjen de Korte <adkorte-guest@alioth.debian.org>" \
	"Vaclav Krpec <VaclavKrpec@Eaton.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};


/** *_OBJECT query status */
typedef enum {
	OBJECT_OK = 0,       /**< OK            */
	OBJECT_PARSE_ERROR,  /**< Parse error   */
	OBJECT_ERROR,        /**< Generic error */
} object_query_status_t;  /* end of typedef enum */


/** *_OBJECT entry type */
typedef enum {
	SET_OBJECT_REQUEST,   /**< SET_OBJECT request  */
	SET_OBJECT_RESPONSE,  /**< SET_OBJECT response */
} object_query_type_t;  /* end of typedef enum */


/** *_OBJECT POST request mode */
typedef enum {
	RAW_POST,   /**< RAW POST mode  */
	FORM_POST,  /**< FORM POST mode */
} object_post_mode_t;  /* end of typedef enum */


typedef struct set_object_req   set_object_req_t;    /**< SET_OBJECT request  carrier */
typedef struct set_object_resp  set_object_resp_t;   /**< SET_OBJECT response carrier */
typedef struct object_entry     object_entry_t;      /**< *_OBJECT entry      carrier */
typedef struct object_query     object_query_t;      /**< *_OBJECT query      handle  */


/** SET_OBJECT request carrier */
struct set_object_req {
	char *name;   /**< OBJECT name  */
	char *value;  /**< OBJECT value */
};  /* end of struct set_object_req */


/** SET_OBJECT response carrier */
struct set_object_resp {
	char *name;    /**< OBJECT name   */
	char *unit;    /**< OBJECT unit   */
	char *access;  /**< OBJECT access */
	char *value;   /**< OBJECT value  */
};  /* end of struct set_object_resp */


/** *_OBJECT query entry */
struct object_entry {
	/** Payload */
	union {
		set_object_req_t  req;   /**< Request  entry */
		set_object_resp_t resp;  /**< Response entry */
	} payld;

	/* Metadata */
	object_entry_t *next;  /**< Next     entry */
	object_entry_t *prev;  /**< Previous entry */
};  /* end of struct object_entry */


/** *_OBJECT query handle */
struct object_query {
	object_query_status_t  status;  /**< Query status      */
	object_query_type_t    type;    /**< List entries type */
	object_post_mode_t     mode;    /**< POST request mode */
	size_t                 cnt;     /**< Count of entries  */
	object_entry_t        *head;    /**< List head         */
	object_entry_t        *tail;    /**< List tail         */
};  /* end of struct object_query */


/**
 *  \brief  *_OBJECT query constructor
 *
 *  \param  type  Query type
 *  \param  mode  Query mode
 *
 *  \return *_OBJECT query handle or \c NULL in case of memory error
 */
static object_query_t *object_query_create(
	object_query_type_t type,
	object_post_mode_t  mode);


/**
 *  \brief  Number of *_OBJECT query entries
 *
 *  \param  handle  Query handle
 *
 *  \return NUmber of entries
 */
static size_t object_query_size(object_query_t *handle);


/**
 *  \brief  *_OBJECT query destructor
 *
 *  \param  handle  Query handle
 */
static void object_query_destroy(object_query_t *handle);


/**
 *  \brief  SET_OBJECT: add request query entry
 *
 *  \param  handle  Request query handle
 *  \param  name    OBJECT name
 *  \param  value   OBJECT value
 *
 *  \return Query entry or \c NULL in case of memory error
 */
static object_entry_t *set_object_add(
	object_query_t *handle,
	const char     *name,
	const char     *value);


/**
 *  \brief  SET_OBJECT: RAW POST mode implementation
 *
 *  \param  req  SET_OBJECT request
 *
 *  \return Response to the request
 */
static object_query_t *set_object_raw(object_query_t *req);


/**
 *  \brief  SET_OBJECT: FORM POST mode implementation
 *
 *  \param  req  SET_OBJECT request
 *
 *  \return \c NULL (FORM POST mode resp. is ignored by specification)
 */
static object_query_t *set_object_form(object_query_t *req);


/**
 *  \brief  SET_OBJECT: implementation
 *
 *  \param  req  SET_OBJECT request
 *
 *  \return Response to the request
 */
static object_query_t *set_object(object_query_t *req);


/**
 *  \brief  SET_OBJECT: RAW POST mode request serialisation
 *
 *  \param  handle  Request query handle
 *
 *  \return POST request body
 */
static ne_buffer *set_object_serialise_raw(object_query_t *handle);


/**
 *  \brief  SET_OBJECT: FORM POST mode request serialisation
 *
 *  \param  handle  Request query handle
 *
 *  \return POST request body
 */
static ne_buffer *set_object_serialise_form(object_query_t *handle);


/* FIXME:
 * "built with neon library %s" LIBNEON_VERSION
 * subdrivers (limited to MGE only ATM) */

/* Global vars */
uint32_t		ups_status = 0;
static int		timeout = 5;
int		shutdown_duration = 120;
static int		shutdown_timer = 0;
static time_t		lastheard = 0;
static subdriver_t	*subdriver = &mge_xml_subdriver;
static ne_session	*session = NULL;
static ne_socket	*sock = NULL;
static ne_uri		uri;
static char	*product_page = NULL;

/* Support functions */
static void netxml_alarm_set(void);
static void netxml_status_set(void);
static int netxml_authenticate(void *userdata, const char *realm, int attempt, char *username, char *password);
static int netxml_dispatch_request(ne_request *request, ne_xml_parser *parser);
static int netxml_get_page(const char *page);

static int instcmd(const char *cmdname, const char *extra);
static int setvar(const char *varname, const char *val);

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
		/* store product page, for later use */
		product_page = xstrdup(page);

		dstate_setinfo("driver.version.data", "%s", subdriver->version);

		if (testvar("subscribe") && (netxml_alarm_subscribe(subdriver->subscribe) == NE_OK)) {
			extrafd = ne_sock_fd(sock);
			time(&lastheard);
		}

		/* Register r/w variables */
		vname_register_rw();

		/* Set UPS driver handler callbacks */
		upsh.setvar  = &setvar;
		upsh.instcmd = &instcmd;

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

	/* also refresh the product information, at least for firmware information */
	ret = netxml_get_page(product_page);
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

void upsdrv_shutdown(void) {
	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */

	/* maybe try to detect the UPS here, but try a shutdown even if
	   it doesn't respond at first if possible */

	/* replace with a proper shutdown function */
	/* fatalx(EXIT_FAILURE, "shutdown not supported"); */

	/* you may have to check the line status since the commands
	   for toggling power are frequently different for OL vs. OB */

	/* OL: this must power cycle the load if possible */

	/* OB: the load must remain off until the power returns */

	int status = STAT_SET_FAILED;  /* pessimistic assumption */

	object_query_t *resp = NULL;
	object_query_t *req  = NULL;

	/* Pragmatic do { ... } while (0) loop allowing break to cleanup */
	do {
		/* Create SET_OBJECT request */
		req = object_query_create(SET_OBJECT_REQUEST, FORM_POST);

		if (NULL == req)
			break;

		if (NULL == set_object_add(req, "battery.runtime.low", "999999999"))
			break;

		/* Send SET_OBJECT request */
		resp = set_object(req);

#if (0)  /* FORM_POST method response is ignored, we can only hope it worked... */
		if (NULL == resp)
			break;

		/* Check if setting was done */
		if (1 > object_query_size(resp)) {
			status = STAT_SET_UNKNOWN;

			break;
		}
#endif  /* end of code removal */

		status = STAT_SET_HANDLED;  /* success */

	} while (0);  /* end of pragmatic loop, break target */

	/* Cleanup */
	if (NULL != req)
		object_query_destroy(req);

	if (NULL != resp)
		object_query_destroy(resp);

	if (STAT_SET_HANDLED != status)
		fatalx(EXIT_FAILURE, "Shutdown failed: %d", status);
}

static int instcmd(const char *cmdname, const char *extra)
{
/*
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send_buf(upsfd, ...);
		return STAT_INSTCMD_HANDLED;
	}
*/

	upslogx(LOG_NOTICE, "%s: unknown command [%s] [%s]", __func__, cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

static int setvar(const char *varname, const char *val) {
	int status = STAT_SET_FAILED;  /* pessimistic assumption */

	object_query_t *resp = NULL;
	object_query_t *req  = NULL;

	/* Pragmatic do { ... } while (0) loop allowing break to cleanup */
	do {
		/* Create SET_OBJECT request */
		req = object_query_create(SET_OBJECT_REQUEST, FORM_POST);

		if (NULL == req)
			break;

		if (NULL == set_object_add(req, varname, val))
			break;

		/* Send SET_OBJECT request */
		resp = set_object(req);

		if (NULL == resp)
			break;

		/* Check if setting was done */
		if (1 > object_query_size(resp)) {
			status = STAT_SET_UNKNOWN;

			break;
		}

		status = STAT_SET_HANDLED;  /* success */

	} while (0);  /* end of pragmatic loop, break target */

	/* Cleanup */
	if (NULL != req)
		object_query_destroy(req);

	if (NULL != resp)
		object_query_destroy(resp);

	return status;
}

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	char	buf[SMALLBUF];

	snprintf(buf, sizeof(buf), "network timeout (default: %d seconds)", timeout);
	addvar(VAR_VALUE, "timeout", buf);

	addvar(VAR_FLAG, "subscribe", "authenticated subscription on NMC");

	addvar(VAR_VALUE | VAR_SENSITIVE, "login", "login value for authenticated mode");
	addvar(VAR_VALUE | VAR_SENSITIVE, "password", "password value for authenticated mode");

	snprintf(buf, sizeof(buf), "shutdown duration in second (default: %d seconds)", shutdown_duration);
	addvar(VAR_VALUE, "shutdown_duration", buf);

	if( shutdown_timer > 0 ) {
		snprintf(buf, sizeof(buf), "shutdown timer in second (default: %d seconds)", shutdown_timer);
	}
	else {
		snprintf(buf, sizeof(buf), "shutdown timer in second (default: none)");
	}
	addvar(VAR_VALUE, "shutdown_timer", buf);

	/* Legacy MGE-XML conversion from 2000's, not needed in modern firmwares */
	addvar(VAR_FLAG, "do_convert_deci", "enable legacy convert_deci() for certain measurements 10x too large");
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

	val = getval("shutdown_duration");
	if (val) {
		shutdown_duration = atoi(val);

		if (shutdown_duration < 0) {
			fatalx(EXIT_FAILURE, "shutdown duration must be greater than or equal to 0");
		}
	}

	val = getval("shutdown_timer");
	if (val) {
		shutdown_timer = atoi(val);

		if (shutdown_timer < 0) {
			fatalx(EXIT_FAILURE, "shutdown timer must be greater than or equal to 0");
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
		fp = fopen("/dev/null", "w");
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
	free(product_page);

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
	int		ret = NE_ERROR;
	ne_request	*request;
	ne_xml_parser	*parser;

	upsdebugx(2, "%s: %s", __func__, (page != NULL)?page:"(null)");

	if (page != NULL) {
		request = ne_request_create(session, "GET", page);

		parser = ne_xml_create();

		ne_xml_push_handler(parser, subdriver->startelm_cb, subdriver->cdata_cb, subdriver->endelm_cb, NULL);

		ret = netxml_dispatch_request(request, parser);

		if (ret) {
			upsdebugx(2, "%s: %s", __func__, ne_get_error(session));
		}

		ne_xml_destroy(parser);
		ne_request_destroy(request);
	}
	return ret;
}

static int netxml_alarm_subscribe(const char *page)
{
	int	ret, secret = -1;
	unsigned int	port = 0;
	char	buf[LARGEBUF], *s;
	ne_request	*request;
	ne_sock_addr	*addr;
	const ne_inet_addr	*ai;
	char	resp_buf[LARGEBUF];

	/* Clear response buffer */
	memset(resp_buf, 0, sizeof(resp_buf));

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

	snprintf(buf, sizeof(buf),	"<?xml version=\"1.0\"?>\n");
	snprintfcat(buf, sizeof(buf),	"<Subscribe>\n");
	snprintfcat(buf, sizeof(buf),		"<Class>%s v%s</Class>\n", progname, DRIVER_VERSION);
	snprintfcat(buf, sizeof(buf),		"<Type>connected socket</Type>\n");
	snprintfcat(buf, sizeof(buf),		"<HostName>%s</HostName>\n", dstate_getinfo("driver.hostname"));
	snprintfcat(buf, sizeof(buf),		"<XMLClientParameters>\n");
	snprintfcat(buf, sizeof(buf),		"<ShutdownDuration>%d</ShutdownDuration>\n", shutdown_duration);
	if( shutdown_timer > 0 ) {
		snprintfcat(buf, sizeof(buf),	"<ShutdownTimer>%d</ShutdownTimer>\r\n", shutdown_timer);
	}
	else {
		snprintfcat(buf, sizeof(buf),	"<ShutdownTimer>NONE</ShutdownTimer>\n");
	}
	snprintfcat(buf, sizeof(buf),			"<AutoConfig>LOCAL</AutoConfig>\n");
	snprintfcat(buf, sizeof(buf),			"<OutletGroup>1</OutletGroup>\n");
	snprintfcat(buf, sizeof(buf),		"</XMLClientParameters>\n");
	snprintfcat(buf, sizeof(buf),		"<Warning></Warning>\n");
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

		ret = ne_read_response_block(request, resp_buf, sizeof(resp_buf));

		if (ret == NE_OK) {
			ret = ne_end_request(request);
		}

	} while (ret == NE_RETRY);

	ne_request_destroy(request);

	/* due to different formats used by the various NMCs, we need to\
	   break up the reply in lines and parse each one separately */
	for (s = strtok(resp_buf, "\r\n"); s != NULL; s = strtok(NULL, "\r\n")) {
		long long int	tmp_port = -1, tmp_secret = -1;
		upsdebugx(2, "%s: parsing %s", __func__, s);

		if (!strncasecmp(s, "<Port>", 6) && (sscanf(s+6, "%lli", &tmp_port) != 1)) {
			return NE_RETRY;
		}

		/* FIXME? Does a port==0 make sense here? Or should the test below be for port<1?
		 * Legacy code until a fix here used sscanf() above to get a '%u' value...
		 */
		if (tmp_port < 0 || tmp_port > UINT_MAX) {
			upsdebugx(2, "%s: parsing initial subcription failed, bad port value", __func__);
			return NE_RETRY;
		}

		if (!strncasecmp(s, "<Secret>", 8) && (sscanf(s+8, "%lli", &tmp_secret) != 1)) {
			return NE_RETRY;
		}

		if (tmp_secret < 0 || tmp_secret > UINT_MAX) {
			upsdebugx(2, "%s: parsing initial subcription failed, bad secret value", __func__);
			return NE_RETRY;
		}

		/* Range of valid values constrained above */
		port = (unsigned int)tmp_port;
		secret = (int)tmp_secret;

	}

	if ((port < 1) || (secret == -1)) {
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

	snprintf(buf, sizeof(buf), "<Subscription Identification=\"%u\"></Subscription>", secret);
	ret = ne_sock_fullwrite(sock, buf, strlen(buf) + 1);

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
	NUT_UNUSED_VARIABLE(userdata);

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
	if (STATUS_BIT(CAL)) {
		status_set("CAL");		/* calibrating */
	}
}


/*
 * *_OBJECT interface implementation
 */

static object_query_t *object_query_create(
	object_query_type_t type,
	object_post_mode_t  mode)
{
	object_query_t *handle = (object_query_t *)calloc(1,
		sizeof(object_query_t));

	if (NULL == handle)
		return NULL;

	handle->type = type;
	handle->mode = mode;

	return handle;
}


static size_t object_query_size(object_query_t *handle) {
	assert(NULL != handle);

	return handle->cnt;
}


/**
 *  \brief  SET_OBJECT request list entry destructor
 *
 *  \param  req  SET_OBJECT request list entry
 */
static void set_object_req_destroy(set_object_req_t *req) {
	assert(NULL != req);

	if (NULL != req->name)
		free(req->name);

	if (NULL != req->value)
		free(req->value);
}


/**
 *  \brief  SET_OBJECT response list entry destructor
 *
 *  \param  resp  SET_OBJECT response list entry
 */
static void set_object_resp_destroy(set_object_resp_t *resp) {
	assert(NULL != resp);

	if (NULL != resp->name)
		free(resp->name);

	if (NULL != resp->unit)
		free(resp->unit);

	if (NULL != resp->access)
		free(resp->access);

	if (NULL != resp->value)
		free(resp->value);
}


/**
 *  \brief  *_OBJECT query entry destructor
 *
 *  \param  handle  SET_OBJECT query handle
 *  \param  entry   SET_OBJECT query entry
 */
static void object_entry_destroy(object_query_t *handle, object_entry_t *entry) {
	assert(NULL != handle);
	assert(NULL != entry);

	/* Sanity checks */
	assert(0 < handle->cnt);

	/* Relink list */
	if (entry == handle->head) {
		handle->head = entry->next;
	}
	else {
		assert(NULL != entry->prev);

		entry->prev->next = entry->next;
	}

	if (entry == handle->tail) {
		handle->tail = entry->prev;
	}
	else {
		assert(NULL != entry->next);

		entry->next->prev = entry->prev;
	}

	--handle->cnt;

	/* Destroy payload */
	switch (handle->type) {
		case SET_OBJECT_REQUEST:
			set_object_req_destroy(&entry->payld.req);

			break;

		case SET_OBJECT_RESPONSE:
			set_object_resp_destroy(&entry->payld.resp);

			break;
	}

	/* Destroy entry */
	free(entry);
}


static void object_query_destroy(object_query_t *handle) {
	assert(NULL != handle);

	/* Destroy entries */
	while (handle->cnt)
		object_entry_destroy(handle, handle->head);

	/* Destroy handle */
	free(handle);
}


/**
 *  \brief  Add *_OBJECT list entry (at list end)
 *
 *  \param  handle  Entry list handle
 *  \param  entry   Entry
 */
static void object_add_entry(object_query_t *handle, object_entry_t *entry) {
	assert(NULL != handle);
	assert(NULL != entry);

	/* Sanity checks */
	assert(SET_OBJECT_REQUEST == handle->type);

	/* Add entry at end of bi-directional list */
	if (handle->cnt) {
		assert(NULL != handle->tail);
		assert(NULL == handle->tail->next);

		handle->tail->next = entry;
		entry->prev = handle->tail;
	}

	/* Add the very first entry */
	else {
		handle->head = entry;
		entry->prev  = NULL;
	}

	handle->tail = entry;
	entry->next  = NULL;

	++handle->cnt;
}


static object_entry_t *set_object_add(
	object_query_t *handle,
	const char     *name,
	const char     *value)
{
	char *name_cpy;
	char *value_cpy;

	assert(NULL != name);
	assert(NULL != value);

	object_entry_t *entry = (object_entry_t *)calloc(1,
		sizeof(object_entry_t));

	if (NULL == entry)
		return NULL;

	/* Copy payload data */
	name_cpy  = strdup(name);
	value_cpy = strdup(value);

	/* Cleanup in case of memory error */
	if (NULL == name_cpy || NULL == value_cpy) {
		if (NULL != name_cpy)
			free(name_cpy);

		if (NULL != value_cpy)
			free(value_cpy);

		free(entry);

		return NULL;
	}

	/* Set payload */
	entry->payld.req.name  = name_cpy;
	entry->payld.req.value = value_cpy;

	/* Enlist */
	object_add_entry(handle, entry);

	return entry;
}


/**
 *  \brief  Common SET_OBJECT entries serialiser
 *
 *  \param  buff   Buffer
 *  \param  entry  SET_OBJECT request entry
 *
 *  \return OBJECT_OK    on success
 *  \return OBJECT_ERROR otherwise
 */
static object_query_status_t set_object_serialise_entries(ne_buffer *buff, object_entry_t *entry) {
	object_query_status_t status = OBJECT_OK;

	assert(NULL != buff);

	ne_buffer_zappend(buff, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
	ne_buffer_zappend(buff, "<SET_OBJECT>\r\n");

	for (; NULL != entry; entry = entry->next) {
		const char *vname = vname_nut2mge_xml(entry->payld.req.name);

		/* Serialise one object */
		if (NULL != vname) {
			ne_buffer_zappend(buff, "  <OBJECT name=\"");
			ne_buffer_zappend(buff, vname);
			ne_buffer_zappend(buff, "\">");
			ne_buffer_zappend(buff, entry->payld.req.value);
			ne_buffer_zappend(buff, "</OBJECT>\r\n");
		}

		/* Var. name resolution error */
		else
			status = OBJECT_ERROR;
	}

	ne_buffer_zappend(buff, "</SET_OBJECT>\r\n");

	return status;
}


static ne_buffer *set_object_serialise_raw(object_query_t *handle) {
	assert(NULL != handle);

	/* Sanity checks */
	assert(SET_OBJECT_REQUEST == handle->type);

	/* Create buffer */
	ne_buffer *buff = ne_buffer_create();

	/* neon API ref. states that the function always succeeds */
	assert(NULL != buff);

	/* Serialise all entries */
	set_object_serialise_entries(buff, handle->head);

	return buff;
}


static ne_buffer *set_object_serialise_form(object_query_t *handle) {
	const char *vname = NULL;

	assert(NULL != handle);

	/* Sanity checks */
	assert(SET_OBJECT_REQUEST == handle->type);

	/* Create buffer */
	ne_buffer *buff = ne_buffer_create();

	/* neon API ref. states that the function always succeeds */
	assert(NULL != buff);

	/* Simple request */
	if (1 == object_query_size(handle)) {
		assert(NULL != handle->head);

		/* TODO: Simple req. doesn't seem to work
		vname = vname_nut2mge_xml(handle->head->payld.req.name);
		*/
	}
	if (NULL != vname) {
		assert(NULL != handle->head);

		ne_buffer_zappend(buff, "objectName=");
		ne_buffer_zappend(buff, vname);
		ne_buffer_zappend(buff, "&objectValue=");
		ne_buffer_zappend(buff, handle->head->payld.req.value);
	}

	/* Multi set request (or empty request) */
	else {
		/* Add request prologue */
		ne_buffer_zappend(buff, "--" FORM_POST_BOUNDARY "\r\n");
		ne_buffer_zappend(buff, "Content-Disposition: form-data; name=\"file\"; "
					"filename=\"Configuration.xml\"\r\n");
		ne_buffer_zappend(buff, "Content-Type: application/octet-stream\r\n");
		ne_buffer_zappend(buff, "\r\n");

		/* Serialise all entries */
		set_object_serialise_entries(buff, handle->head);

		/* Add request epilogue */
		ne_buffer_zappend(buff, "--" FORM_POST_BOUNDARY "--\r\n");
	}

	return buff;
}


/**
 *  \brief  neon callback for SET_OBJECT RAW POST mode response element start
 *
 *  \param  userdata  Obfuscated SET_OBJECT_RESPONSE query handle
 *  \param  parent    Element parent
 *  \param  nspace    Element namespace (empty)
 *  \param  name      Element name
 *  \param  attrs     Element attributes
 *
 *  \return \c NE_XML_STATEROOT + distance of the element from root
 */
static int set_object_raw_resp_start_element(
	void        *userdata,
	int          parent,
	const char  *nspace,
	const char  *name,
	const char **attrs)
{
	object_query_t *handle = (object_query_t *)userdata;

	assert(NULL != handle);

	/* Sanity checks */
	assert(SET_OBJECT_RESPONSE == handle->type);

	/* Check that namespace is empty */
	if (NULL != nspace && '\0' != *nspace) {
		handle->status = OBJECT_PARSE_ERROR;

		return NE_XML_STATEROOT;
	}

	/* OBJECT (as a SET_OBJECT child) */
	if (NE_XML_STATEROOT + 1 == parent && 0 == strcasecmp(name, "OBJECT")) {
		size_t i;

		object_entry_t *entry = (object_entry_t *)calloc(1,
			sizeof(object_entry_t));

		/* Memory error */
		if (NULL == entry) {
			handle->status = OBJECT_ERROR;

			return NE_XML_STATEROOT;
		}

		/* Set attributes */
		for (i = 0; NULL != attrs[i] && NULL != attrs[i + 1]; i += 2) {
			char       **attr = NULL;
			const char  *aval = NULL;

			/* Skip unset attribute name and/or value (useless) */
			if (NULL == attrs[i] || NULL == attrs[i + 1])
				continue;

			/* Obviously, the following holds, now */
			assert(NULL != attrs[i]);
			assert(NULL != attrs[i + 1]);

			/* name */
			if (0 == strcasecmp(attrs[i], "name")) {
				attr = &entry->payld.resp.name;
				aval = vname_mge_xml2nut(attrs[i + 1]);
			}

			/* unit */
			else if (0 == strcasecmp(attrs[i], "unit")) {
				attr = &entry->payld.resp.unit;
				aval = attrs[i + 1];
			}

			/* access */
			else if (0 == strcasecmp(attrs[i], "access")) {
				attr = &entry->payld.resp.access;
				aval = attrs[i + 1];
			}

			/* Set known attribute */
			if (NULL != attr) {
				/* Copy value */
				if (NULL != aval) {
					*attr = strdup(aval);

					if (NULL == *attr)
						handle->status = OBJECT_ERROR;
				}

				/* Value resolution error */
				else
					handle->status = OBJECT_ERROR;
			}
		}

		object_add_entry(handle, entry);

		return NE_XML_STATEROOT + 2;  /* signal to cdata callback */
	}

	/* SET_OBJECT (as the root child) */
	if (NE_XML_STATEROOT == parent && 0 == strcasecmp(name, "SET_OBJECT"))
		return NE_XML_STATEROOT + 1;

	/* Unknown element (as a SET_OBJECT child) */
	if (NE_XML_STATEROOT + 1 == parent)
		return NE_XML_STATEROOT + 1;

	/* Ignore any other root children */
	return NE_XML_STATEROOT;
}


/**
 *  \brief  neon callback for SET_OBJECT RAW POST mode response data start
 *
 *  The callback is used to set OBJECT element value.
 *  This is done for state \c NE_XML_STATEROOT + 2
 *  (see \ref set_object_raw_resp_start_element).
 *
 *  \param  userdata  Obfuscated SET_OBJECT_RESPONSE query handle
 *  \param  state     Element distance from root
 *  \param  cdata     Character data
 *  \param  len       Character data length
 *
 *  \return state
 */
static int set_object_raw_resp_cdata(
	void       *userdata,
	int         state,
	const char *cdata,
	size_t      len)
{
	object_query_t *handle = (object_query_t *)userdata;

	assert(NULL != handle);

	/* Sanity checks */
	assert(SET_OBJECT_RESPONSE == handle->type);

	/* Ignore any element except OBJECT */
	if (NE_XML_STATEROOT + 2 != state)
		return state;

	if (OBJECT_OK == handle->status) {
		char *value;

		/* Set last object value */
		assert(NULL != handle->tail);
		assert(NULL != handle->tail->payld.resp.name);

		value = vvalue_mge_xml2nut(handle->tail->payld.resp.name, cdata, len);

		handle->tail->payld.resp.value = value;

		if (NULL == handle->tail->payld.resp.value)
			handle->status = OBJECT_ERROR;
	}

	return state;
}


/**
 *  \brief  neon callback for SET_OBJECT RAW POST mode response element start
 *
 *  \param  userdata  Obfuscated SET_OBJECT_RESPONSE query handle
 *  \param  state     Element distance from root
 *  \param  nspace    Element namespace (empty)
 *  \param  name      Element name
 *
 *  \return \c NE_XML_STATEROOT + distance of the element from root
 */
static int set_object_raw_resp_end_element(
	void       *userdata,
	int         state,
	const char *nspace,
	const char *name)
{
	NUT_UNUSED_VARIABLE(userdata);
	NUT_UNUSED_VARIABLE(nspace);

	/* OBJECT (as a SET_OBJECT child) */
	if (NE_XML_STATEROOT + 2 == state) {
		assert(0 == strcasecmp(name, "OBJECT"));

		return NE_XML_STATEROOT + 1;
	}

	/*
	 * Otherwise, state is either NE_XML_STATEROOT or NE_XML_STATEROOT + 1
	 * In any case, we return NE_XML_STATEROOT
	 */
	return NE_XML_STATEROOT;
}


static object_query_t *set_object_deserialise_raw(ne_buffer *buff) {
	int ne_status;

	assert(NULL != buff);

	/* Create SET_OBJECT query response */
	object_query_t *handle = object_query_create(SET_OBJECT_RESPONSE, RAW_POST);

	if (NULL == handle)
		return NULL;

	/* Create XML parser */
	ne_xml_parser *parser = ne_xml_create();

	/* neon API ref. states that the function always succeeds */
	assert(NULL != parser);

	/* Set element & data handlers */
	ne_xml_push_handler(
		parser,
		set_object_raw_resp_start_element,
		set_object_raw_resp_cdata,
		set_object_raw_resp_end_element,
		handle);

	/* Parse the response */
	ne_status = ne_xml_parse(parser, buff->data, buff->used);

	if (NE_OK != ne_status)
		handle->status = OBJECT_PARSE_ERROR;

	/* Destroy parser */
	ne_xml_destroy(parser);

	return handle;
}


/**
 *  \brief  Send HTTP request over a session
 *
 *  The function creates HTTP request, sends it and reads-out the response.
 *
 *  \param[in]   argsession HTTP session
 *  \param[in]   method     Request method
 *  \param[in]   arguri     Request URI
 *  \param[in]   ct         Request content type (optional, \c NULL accepted)
 *  \param[in]   req_body   Request body (optional, \c NULL is accepted)
 *  \param[out]  resp_body  Response body (optional, \c NULL is accepted)
 *
 *  \return HTTP status code if response was sent, 0 on send error
 */
static int send_http_request(
	ne_session *argsession,
	const char *method,
	const char *arguri,
	const char *ct,
	ne_buffer  *req_body,
	ne_buffer  *resp_body)
{
	int resp_code = 0;

	ne_request *req = NULL;

	/* Create request */
	req = ne_request_create(argsession, method, arguri);

	/* Neon claims that request creation is always successful */
	assert(NULL != req);

	do {  /* Pragmatic do ... while (0) loop allowing breaks on error */
		const ne_status *req_st;

		/* Set Content-Type */
		if (NULL != ct)
			ne_add_request_header(req, "Content-Type", ct);

		/* Set request body */
		if (NULL != req_body)
			/* BEWARE: The terminating '\0' byte is "used", too */
			ne_set_request_body_buffer(req,
				req_body->data, req_body->used - 1);

		/* Send request */
		int status = ne_begin_request(req);

		if (NE_OK != status) {
			break;
		}

		/* Read response */
		assert(NE_OK == status);

		for (;;) {
			char buff[512];

			ssize_t read;

			read = ne_read_response_block(req, buff, sizeof(buff));

			/* Read failure */
			if (0 > read) {
				status = NE_ERROR;

				break;
			}

			if (0 == read)
				break;

			if (NULL != resp_body)
				ne_buffer_append(resp_body, buff, (size_t)read);
		}

		if (NE_OK != status) {
			break;
		}

		/* Request served */
		ne_end_request(req);

		/* Get response code */
		req_st = ne_get_status(req);

		assert(NULL != req_st);

		resp_code = req_st->code;

	} while (0);  /* end of do ... while (0) pragmatic loop */

	if (NULL != req)
		ne_request_destroy(req);

	return resp_code;
}


static object_query_t *set_object_raw(object_query_t *req) {
	int             resp_code;
	object_query_t *resp      = NULL;
	ne_buffer      *req_body  = NULL;
	ne_buffer      *resp_body = NULL;

	assert(NULL != req);

	/* Sanity check */
	assert(SET_OBJECT_REQUEST == req->type);
	assert(RAW_POST           == req->mode);

	/* Serialise request POST data */
	req_body = set_object_serialise_raw(req);

	/* Send request */
	resp_body = ne_buffer_create();

	assert(NULL != resp_body);

	resp_code = send_http_request(session,
		"POST", "/set_obj.htm", NULL, req_body, resp_body);

	/*
	 * Repeat in case of 401 - Unauthorised
	 *
	 * Note that this is a WA of NMC sending Connection: close
	 * header in the 401 response, in which case neon closes
	 * connection (quite rightfully).
	 */
	if (401 == resp_code)
		resp_code = send_http_request(session,
			"POST", "/set_obj.htm", NULL, req_body, resp_body);

	/* Deserialise response */
	if (200 == resp_code)
		resp = set_object_deserialise_raw(resp_body);

	/* Cleanup */
	if (NULL != req_body)
		ne_buffer_destroy(req_body);

	if (NULL != resp_body)
		ne_buffer_destroy(resp_body);

	return resp;
}


static object_query_t *set_object_form(object_query_t *req) {
	int       resp_code;
	ne_buffer *req_body = NULL;

	const char *ct = "multipart/form-data; boundary=" FORM_POST_BOUNDARY;

	/* TODO: Single request doesn't seem to work
	if (1 == object_query_size(req))
		ct = "application/x-form-urlencoded";
	*/

	assert(NULL != req);

	/* Sanity check */
	assert(SET_OBJECT_REQUEST == req->type);
	assert(FORM_POST          == req->mode);

	/* Serialise request POST data */
	req_body = set_object_serialise_form(req);

	/* Send request (response is ignored by the proto. spec v3) */
	resp_code = send_http_request(session,
		"POST", "/Forms/set_obj_2", ct, req_body, NULL);

	/*
	 * Repeat in case of 401 - Unauthorised
	 *
	 * Note that this is a WA of NMC sending Connection: close
	 * header in the 401 response, in which case neon closes
	 * connection (quite rightfully).
	 */
	if (401 == resp_code) {
		resp_code = send_http_request(session,
			"POST", "/Forms/set_obj_2", ct, req_body, NULL);
	}

	/* Cleanup */
	if (NULL != req_body)
		ne_buffer_destroy(req_body);

	return NULL;
}


static object_query_t *set_object(object_query_t *req) {
	object_query_t *resp = NULL;

	assert(NULL != req);

	/* Sanity checks */
	assert(SET_OBJECT_REQUEST == req->type);

	/* Select implementation by POST request mode */
	switch (req->mode) {
		case RAW_POST:
			resp = set_object_raw(req);

			break;

		case FORM_POST:
			resp = set_object_form(req);

			break;
	}

	return resp;
}
