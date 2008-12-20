/* dummy-ups.c - NUT testing driver and repeater

   Copyright (C)
       2005 - 2008  Arnaud Quette <http://arnaud.quette.free.fr/contact.html>

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

/* TODO list:
 * - separate the code between dummy and repeater/meta
 * - for repeater/meta: add support for instant commands and setvar
 * - variable/value enforcement using cmdvartab for testing
 *   the variable existance
 * - allow variable creation on the fly (using upsrw)
 * - poll the "port" file for change
 */

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>

#include "main.h"
#include "parseconf.h"
#include "upsclient.h"
#include "dummy-ups.h"

#define DRIVER_NAME	"Dummy UPS driver"
#define DRIVER_VERSION	"0.05"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <arnaud.quette@gmail.com>",
	DRV_STABLE,
	{ NULL }
};

#define MODE_UNKNOWN	0
#define MODE_DUMMY		1 /* use the embedded defintion or a definition file */
#define MODE_REPEATER	2 /* use libupsclient to repeat an UPS */
#define MODE_META		3 /* consolidate data from several UPSs (TBS) */

int mode=MODE_UNKNOWN;

#define MAX_STRING_SIZE	128

static int setvar(const char *varname, const char *val);
static int parse_data_file(int upsfd);
static dummy_info_t *find_info(const char *varname);
static int is_valid_data(const char* varname);
static int is_valid_value(const char* varname, const char *value);
/* libupsclient update */
static int upsclient_update_vars(void);

static char		*client_upsname = NULL, *hostname = NULL;
static UPSCONN_t	*ups = NULL;

/* Driver functions */

void upsdrv_initinfo(void)
{
	int	port;
	dummy_info_t *item;

	switch (mode) {
		case MODE_DUMMY:
			/* Initialise basic essential variables */
			for ( item = nut_data ; item->info_type != NULL ; item++ ) {
				if (item->drv_flags & DU_FLAG_INIT) {
					dstate_setinfo(item->info_type, "%s", item->default_value);
					dstate_setflags(item->info_type, item->info_flags);

					/* Set max length for strings, if needed */
					if (item->info_flags & ST_FLAG_STRING)
						dstate_setaux(item->info_type, item->info_len);
				}
			}

			/* Now get user's defined variables */
			if (parse_data_file(upsfd) < 0)
				upslogx(LOG_NOTICE, "Unable to parse the definition file %s", device_path);

			/* Initialize handler */
			upsh.setvar = setvar;

			dstate_dataok();
			break;
		case MODE_META:
		case MODE_REPEATER:
			/* Obtain the target name */
			if (upscli_splitname(device_path, &client_upsname, &hostname, &port) != 0) {
				fatalx(EXIT_FAILURE, "Error: invalid UPS definition.\nRequired format: upsname[@hostname[:port]]");
			}
			/* Connect to the target */
			ups = xmalloc(sizeof(*ups));
			if (upscli_connect(ups, hostname, port, UPSCLI_CONN_TRYSSL) < 0) {
				fatalx(EXIT_FAILURE, "Error: %s", upscli_strerror(ups));
			}
			else {
				upsdebugx(1, "Connected to %s@%s", client_upsname, hostname);
			}
			if (upsclient_update_vars() < 0) {
				/* check for an old upsd */
				if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND) {
					fatalx(EXIT_FAILURE, "Error: upsd is too old to support this query");
				}
				fatalx(EXIT_FAILURE, "Error: %s", upscli_strerror(ups));
			}
			/* FIXME: commands and settables! */
			break;
		default:
		case MODE_UNKNOWN:
			fatalx(EXIT_FAILURE, "no suitable definition found!");
			break;
	}
}

void upsdrv_updateinfo(void)
{
	upsdebugx(1, "upsdrv_updateinfo...");

	sleep(1);

	switch (mode) {
		case MODE_DUMMY:
			/* simply avoid driver staleness */
			status_init();
			status_set(dstate_getinfo("ups.status"));
			status_commit();
			dstate_dataok();
			break;
		case MODE_META:
		case MODE_REPEATER:
			if (upsclient_update_vars())
				dstate_dataok();
			break;
	}
}

void upsdrv_shutdown(void)
{
	fatalx(EXIT_FAILURE, "shutdown not supported");
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

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	/* allow '-x xyzzy' */
	/* addvar(VAR_FLAG, "xyzzy", "Enable xyzzy mode"); */

	/* allow '-x foo=<some value>' */
	/* addvar(VAR_VALUE, "foo", "Override foo setting"); */
}

void upsdrv_initups(void)
{
	/* check the running mode... */
	if (strchr(device_path, '@')) {
		upsdebugx(1, "Repeater mode");
		mode = MODE_REPEATER;
		dstate_setinfo("driver.parameter.mode", "repeater");
		/* if there is at least one more => MODE_META... */
	}
	else {
		upsdebugx(1, "Dummy mode");
		mode = MODE_DUMMY;
		dstate_setinfo("driver.parameter.mode", "dummy");
	}
}

void upsdrv_cleanup(void)
{
	if ( (mode == MODE_META) || (mode == MODE_REPEATER) ) {
		if (ups) {
			upscli_disconnect(ups);
		}

		free(client_upsname);
		free(hostname);
		free(ups);
	}
}

static int setvar(const char *varname, const char *val)
{
	dummy_info_t *item;

	upsdebugx(2, "entering setvar(%s, %s)", varname, val);

	if (!strncmp(varname, "ups.status", 10)) {
		status_init();
		 /* FIXME: split and check values (support multiple values), à la usbhid-ups */
		status_set(val);
		status_commit();

		return STAT_SET_HANDLED;
	}

	/* Check variable validity */
	if (!is_valid_data(varname)) {
		upsdebugx(2, "setvar: invalid variable name (%s)", varname);

		return STAT_SET_UNKNOWN;
	}

	/* Check value validity */
	if (!is_valid_value(varname, val)) {
		upsdebugx(2, "setvar: invalid value (%s) for variable (%s)", val, varname);

		return STAT_SET_UNKNOWN;
	}

	dstate_setinfo(varname, "%s", val);

	if ( (item = find_info(varname)) != NULL) {
		dstate_setflags(item->info_type, item->info_flags);

		/* Set max length for strings, if needed */
		if (item->info_flags & ST_FLAG_STRING)
			dstate_setaux(item->info_type, item->info_len);
	}

	return STAT_SET_HANDLED;
}

/*************************************************/
/*               Support functions               */
/*************************************************/

static int upsclient_update_vars(void)
{
	int		ret;
	unsigned int	numq, numa;
	const char	*query[4];
	char		**answer;

	query[0] = "VAR";
	query[1] = client_upsname;
	numq = 2;

	ret = upscli_list_start(ups, numq, query);

	if (ret < 0) {

		return ret;
	}

	while (upscli_list_next(ups, numq, query, &numa, &answer) == 1) {

		/* VAR <upsname> <varname> <val> */
		if (numa < 4) {
			fatalx(EXIT_FAILURE, "Error: insufficient data (got %d args, need at least 4)", numa);
		}
		/* do not override the driver collection */
		if (strncmp(answer[2], "driver.", 7))
			dstate_setinfo(answer[2], "%s", answer[3]);
	}
	return 1;
}

/* find info element definition in info array */
static dummy_info_t *find_info(const char *varname)
{
	dummy_info_t *item;

	for ( item = nut_data ; item->info_type != NULL ; item++ )	{
		if (!strcasecmp(item->info_type, varname))
			return item;
	}

	upsdebugx(2, "find_info: unknown variable: %s\n", varname);

	return NULL;
}

/* check if data exists in our data table */
static int is_valid_data(const char* varname)
{
	dummy_info_t *item;

	if ( (item = find_info(varname)) != NULL) {
			return 1;
	}

	/* FIXME: we need to have the full data set before
	 * enforcing controls! */
	
	upsdebugx(1, "Unknown data. Commiting anyway...");
	return 1;
	/* return 0;*/
}

/* check if data's value validity */
static int is_valid_value(const char* varname, const char *value)
{
	dummy_info_t *item;

	if ( (item = find_info(varname)) != NULL) {
		/* FIXME: test enum or bound against value */
		return 1;
	}

	/* FIXME: we need to have the full data set before
	 * enforcing controls! */
	
	upsdebugx(1, "Unknown data. Commiting value anyway...");
	return 1;
	/* return 0;*/
}

/* called for fatal errors in parseconf like malloc failures */
static void upsconf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(ups.conf): %s", errmsg);
}

static int parse_data_file(int upsfd)
{
	PCONF_CTX_t	ctx;
	char	fn[SMALLBUF];
	char	*ptr, *var_value;
	int		value_args = 0, counter;

	if (device_path[0] == '/')
		snprintf(fn, sizeof(fn), "%s", device_path);
	else
		snprintf(fn, sizeof(fn), "%s/%s", confpath(), device_path);

	pconf_init(&ctx, upsconf_err);

	if (!pconf_file_begin(&ctx, fn))
		fatalx(EXIT_FAILURE, "Can't open dummy-ups definition file %s: %s",
			fn, ctx.errmsg);

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upsdebugx(2, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		/* Check if we have something to process */
		if (ctx.numargs < 1)
			continue;

		/* Remove the ":" after the variable name */
		if ((ptr = strchr(ctx.arglist[0], ':')) != NULL)
			*ptr = '\0';

		upsdebugx(2, "parse_data_file: variable \"%s\" with %d args", ctx.arglist[0], ctx.numargs);

		/* skip the driver.* collection data */
		if (!strncmp(ctx.arglist[0], "driver.", 7)) {
			upsdebugx(2, "parse_data_file: skipping %s", ctx.arglist[0]);
			continue;
		}

		/* From there, we get varname in arg[0], and values in other arg[1...x] */
		/* FIXME: iteration on arg[2, 3, ...]
			if ST_FLAG_STRING => all args are the value
			if ups.status, each arg is a value to be set (ie OB LB) + check against enum
			else int/float values need to be check against bound/enum
		*/
		var_value = (char*) xmalloc(MAX_STRING_SIZE);
		for (counter = 1, value_args = ctx.numargs ; counter < value_args ; counter++) {
			if (counter != 1) /* don't append the first space separator */
				strncat(var_value, " ", MAX_STRING_SIZE);
			strncat(var_value, ctx.arglist[counter], MAX_STRING_SIZE);
		}

		if (setvar(ctx.arglist[0], var_value) == STAT_SET_UNKNOWN)
			upsdebugx(2, "parse_data_file: can't add \"%s\" with value \"%s\"\nError: %s",
				ctx.arglist[0], var_value, ctx.errmsg);
		else
			upsdebugx(2, "parse_data_file: added \"%s\" with value \"%s\"",
				ctx.arglist[0], var_value);
	}

	/* cleanup now, since parseconf is not useful anymore! */
	pconf_finish(&ctx);

	return 1;
}
