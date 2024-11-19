/* dummy-ups.c - NUT simulation and device repeater driver

   Copyright (C)
       2005 - 2015  Arnaud Quette <http://arnaud.quette.free.fr/contact.html>
       2014 - 2024  Jim Klimov <jimklimov+nut@gmail.com>

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
 * - for repeater/meta:
 *   * add support for instant commands and setvar
 * - for dummy:
 *   * variable/value enforcement using cmdvartab for testing
 *     the variable existance, and possible values
 *   * allow variable creation on the fly (using upsrw)
 *   * poll the "port" file for change
 */

#include "config.h" /* must be the first header */

#ifndef WIN32
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <sys/stat.h>
#include <string.h>

#include "main.h"
#include "parseconf.h"
#include "nut_stdint.h"
#include "upsclient.h"
#include "dummy-ups.h"

#define DRIVER_NAME	"Device simulation and repeater driver"
#define DRIVER_VERSION	"0.20"

/* driver description structure */
upsdrv_info_t upsdrv_info =
{
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <arnaud.quette@gmail.com>",
	DRV_STABLE,
	{ NULL }
};

enum drivermode {
	MODE_NONE = 0,

	/* use the embedded defintion or a definition file, parsed in a
	 * loop again and again (often with TIMER lines to delay changes)
	 * Default mode for files with *.seq naming pattern
	 * (legacy-compatibility note: and other patterns except *.dev)
	 */
	MODE_DUMMY_LOOP,

	/* use the embedded defintion or a definition file, parsed once
	 *
	 * This allows to spin up a dummy device with initial readings
	 * and retain in memory whatever SET VAR was sent by clients later.
	 * This is also less stressful on system resources to run the dummy.
	 *
	 * Default mode for files with *.dev naming pattern
	 */
	MODE_DUMMY_ONCE,

	/* use libupsclient to repeat another UPS */
	MODE_REPEATER,

	/* consolidate data from several UPSs (TBS) */
	MODE_META
};
typedef enum drivermode drivermode_t;

static drivermode_t mode = MODE_NONE;

/* parseconf context, for dummy mode using a file */
static PCONF_CTX_t	*ctx = NULL;
static time_t		next_update = -1;
static struct stat	datafile_stat;

#define MAX_STRING_SIZE	128

static int setvar(const char *varname, const char *val);
static int instcmd(const char *cmdname, const char *extra);
static int parse_data_file(TYPE_FD arg_upsfd);
static dummy_info_t *find_info(const char *varname);
static int is_valid_data(const char* varname);
static int is_valid_value(const char* varname, const char *value);
/* libupsclient update */
static int upsclient_update_vars(void);

/* connection information */
static char		*client_upsname = NULL, *hostname = NULL;
static UPSCONN_t	*ups = NULL;
static uint16_t	port;

/* repeater mode parameters */
static int repeater_disable_strict_start = 0;

/* Driver functions */

void upsdrv_initinfo(void)
{
	dummy_info_t *item;

	switch (mode)
	{
		case MODE_DUMMY_ONCE:
		case MODE_DUMMY_LOOP:
			/* Initialise basic essential variables */
			for ( item = nut_data ; item->info_type != NULL ; item++ )
			{
				if (item->drv_flags & DU_FLAG_INIT)
				{
					dstate_setinfo(item->info_type, "%s", item->default_value);
					dstate_setflags(item->info_type, item->info_flags);

					/* Set max length for strings, if needed */
					if (item->info_flags & ST_FLAG_STRING)
						dstate_setaux(item->info_type, (long)item->info_len);
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
			if (upscli_splitname(device_path, &client_upsname, &hostname, &port) != 0)
			{
				fatalx(EXIT_FAILURE, "Error: invalid UPS definition.\nRequired format: upsname[@hostname[:port]]");
			}
			/* Connect to the target */
			ups = xmalloc(sizeof(*ups));
			if (upscli_connect(ups, hostname, port, UPSCLI_CONN_TRYSSL) < 0)
			{
				if(repeater_disable_strict_start == 1)
				{
					upslogx(LOG_WARNING, "Warning: %s", upscli_strerror(ups));
				}
				else
				{
					fatalx(EXIT_FAILURE, "Error: %s. "
					"Any errors encountered starting the repeater mode result in driver termination, "
					"perhaps you want to set the 'repeater_disable_strict_start' option?"
					, upscli_strerror(ups));
				}
			}
			else
			{
				upsdebugx(1, "Connected to %s@%s", client_upsname, hostname);
			}
			if (upsclient_update_vars() < 0)
			{
				/* check for an old upsd */
				if (upscli_upserror(ups) == UPSCLI_ERR_UNKCOMMAND)
				{
					fatalx(EXIT_FAILURE, "Error: upsd is too old to support this query");
				}

				if(repeater_disable_strict_start == 1)
				{
					upslogx(LOG_WARNING, "Warning: %s", upscli_strerror(ups));
				}
				else
				{
					fatalx(EXIT_FAILURE, "Error: %s. "
					"Any errors encountered starting the repeater mode result in driver termination, "
					"perhaps you want to set the 'repeater_disable_strict_start' option?"
					, upscli_strerror(ups));
				}
			}
			/* FIXME: commands and settable variable! */
			break;

		case MODE_NONE:
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
		default:
			fatalx(EXIT_FAILURE, "no suitable definition found!");
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}
	upsh.instcmd = instcmd;

	dstate_addcmd("load.off");
}

static int prepare_filepath(char *fn, size_t buflen)
{
	/* Note: device_path is a global variable,
	 * the "port=..." value parsed in main.c */
	if (device_path[0] == '/'
#ifdef WIN32
	||  device_path[1] == ':'	/* "C:\..." */
#endif
	) {
		/* absolute path */
		return snprintf(fn, buflen, "%s", device_path);
	} else if (device_path[0] == '.') {
		/* "./" or "../" e.g. via CLI, relative to current working
		 * directory of the driver process... at this moment */
		if (getcwd(fn, buflen)) {
			return snprintf(fn + strlen(fn), buflen - strlen(fn), "/%s", device_path);
		} else {
			return snprintf(fn, buflen, "%s", device_path);
		}
	} else {
		/* assumed to be a filename in NUT config file path
		 * (possibly under, with direct use of dirname without dots)
		 * Note that we do not fiddle with file-path separator,
		 * modern Windows (at least via MinGW/MSYS2) supports
		 * the POSIX slash.
		 */
		return snprintf(fn, buflen, "%s/%s", confpath(), device_path);
	}
}

void upsdrv_updateinfo(void)
{
	upsdebugx(1, "upsdrv_updateinfo...");

	sleep(1);

	switch (mode)
	{
		case MODE_DUMMY_LOOP:
			/* Now get user's defined variables */
			if (parse_data_file(upsfd) >= 0)
				dstate_dataok();
			break;

		case MODE_DUMMY_ONCE:
			/* less stress on the sys */
			if (ctx == NULL && next_update == -1) {
				struct stat	fs;
				char fn[SMALLBUF];

				prepare_filepath(fn, sizeof(fn));

				/* Determine if file modification timestamp has changed
				 * since last use (so we would want to re-read it) */
#ifndef WIN32
				/* Either successful stat (zero return) is OK to
				 * fill the "fs" struct. Note that currently
				 * "upsfd" is a no-op for files, they are re-opened
				 * and re-parsed every time so callers can modify
				 * the data without complications.
				 */
				if ( (INVALID_FD(upsfd) || 0 != fstat (upsfd, &fs)) && 0 != stat (fn, &fs))
#else
				/* Consider GetFileAttributesEx() for WIN32_FILE_ATTRIBUTE_DATA?
				 *   https://stackoverflow.com/questions/8991192/check-the-file-size-without-opening-file-in-c/8991228#8991228
				 */
				if (0 != stat (fn, &fs))
#endif
				{
					upsdebugx(2, "%s: MODE_DUMMY_ONCE: Can't stat %s currently", __func__, fn);
					/* retry ASAP until we get a file */
					memset(&datafile_stat, 0, sizeof(struct stat));
					next_update = 1;
				} else {
					if (datafile_stat.st_mtime != fs.st_mtime) {
						upsdebugx(2,
							"%s: MODE_DUMMY_ONCE: input file was already read once "
							"to the end, but changed later - re-reading: %s",
							__func__, fn);
						/* updated file => retry ASAP */
						next_update = 1;
						datafile_stat = fs;
					}
				}
			}

			if (ctx == NULL && next_update == -1) {
				upsdebugx(2, "%s: MODE_DUMMY_ONCE: NO-OP: input file was already read once to the end", __func__);
				dstate_dataok();
			} else {
				/* initial parsing interrupted by e.g. TIMER line */
				if (parse_data_file(upsfd) >= 0)
					dstate_dataok();
			}
			break;

		case MODE_META:
		case MODE_REPEATER:
			if (upsclient_update_vars() > 0)
			{
				dstate_dataok();
			}
			else
			{
				/* try to reconnect */
				upscli_disconnect(ups);
				if (upscli_connect(ups, hostname, port, UPSCLI_CONN_TRYSSL) < 0)
				{
					upsdebugx(1, "Error reconnecting: %s", upscli_strerror(ups));
				}
				else
				{
					upsdebugx(1, "Reconnected");
				}
			}
			break;

		case MODE_NONE:
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
		default:
			break;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}
}

void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	/* replace with a proper shutdown function */
	upslogx(LOG_ERR, "shutdown not supported");
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(EF_EXIT_FAILURE);
}

static int instcmd(const char *cmdname, const char *extra)
{
/*
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send_buf(upsfd, ...);
		return STAT_INSTCMD_HANDLED;
	}
*/
	/* FIXME: the below is only valid if (mode == MODE_DUMMY)
	 * if (mode == MODE_REPEATER) => forward
	 * if (mode == MODE_META) => ?
	 */

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE,	"mode",	"Specify mode instead of guessing it from port value (dummy = dummy-loop, dummy-once, repeater)"); /* meta */
	addvar(VAR_FLAG,    "repeater_disable_strict_start", "Do not terminate the driver encountering errors when starting the repeater mode");
}

void upsdrv_initups(void)
{
	const char *val;

	val = dstate_getinfo("driver.parameter.mode");
	if (val) {
		if (!strcmp(val, "dummy-loop")
		&&  !strcmp(val, "dummy-once")
		&&  !strcmp(val, "dummy")
		&&  !strcmp(val, "repeater")
		/* &&  !strcmp(val, "meta") */
		) {
			fatalx(EXIT_FAILURE, "Unsupported mode was specified: %s", val);
		}
	}

	/* check the running mode... */
	if ( (!val && strchr(device_path, '@'))
	||   (val && !strcmp(val, "repeater"))
	/*||   (val && !strcmp(val, "meta")) */
	) {
		upsdebugx(1, "Repeater mode");
		mode = MODE_REPEATER;
		dstate_setinfo("driver.parameter.mode", "repeater");
		/* FIXME: if there is at least one more => MODE_META... */
	}
	else
	{
		char fn[SMALLBUF];
		mode = MODE_NONE;

		if (val) {
			if (!strcmp(val, "dummy-loop")) {
				upsdebugx(2, "Dummy (simulation) mode looping infinitely was explicitly requested");
				mode = MODE_DUMMY_LOOP;
			} else
			if (!strcmp(val, "dummy-once")) {
				upsdebugx(2, "Dummy (simulation) mode with data read once was explicitly requested");
				mode = MODE_DUMMY_ONCE;
			} else
			if (!strcmp(val, "dummy")) {
				upsdebugx(2, "Dummy (simulation) mode default (looping infinitely) was explicitly requested");
				mode = MODE_DUMMY_LOOP;
			}
		}

		if (mode == MODE_NONE) {
			if (str_ends_with(device_path, ".seq")) {
				upsdebugx(2, "Dummy (simulation) mode with a sequence file name pattern (looping infinitely)");
				mode = MODE_DUMMY_LOOP;
			} else if (str_ends_with(device_path, ".dev")) {
				upsdebugx(2, "Dummy (simulation) mode with a device data dump file name pattern (read once)");
				mode = MODE_DUMMY_ONCE;
			}
		}

		/* Report decisions similar to those above,
		 * just a bit shorter and at another level */
		switch (mode) {
			case MODE_DUMMY_ONCE:
				upsdebugx(1, "Dummy (simulation) mode using data read once");
				dstate_setinfo("driver.parameter.mode", "dummy-once");
				break;

			case MODE_DUMMY_LOOP:
				upsdebugx(1, "Dummy (simulation) mode looping infinitely");
				dstate_setinfo("driver.parameter.mode", "dummy-loop");
				break;

			case MODE_NONE:
			case MODE_REPEATER:
			case MODE_META:
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
			default:
				/* This was the only mode until MODE_DUMMY_LOOP
				 * got split from MODE_DUMMY_ONCE in NUT v2.8.0
				 * so we keep the previously known mode string
				 * and it remains default when we are not sure
				 */
				upsdebugx(1, "Dummy (simulation) mode default (looping infinitely)");
				mode = MODE_DUMMY_LOOP;
				dstate_setinfo("driver.parameter.mode", "dummy");
				break;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
		}

		prepare_filepath(fn, sizeof(fn));

		/* Update file modification timestamp (and other data) */
#ifndef WIN32
		/* Either successful stat (zero return) is OK to fill the
		 * "datafile_stat" struct. Note that currently "upsfd" is
		 * a no-op for files, they are re-opened and re-parsed
		 * every time so callers can modify the data without
		 * complications.
		 */
		if ( (INVALID_FD(upsfd) || 0 != fstat (upsfd, &datafile_stat)) && 0 != stat (fn, &datafile_stat))
#else
		/* Consider GetFileAttributesEx() for WIN32_FILE_ATTRIBUTE_DATA?
		 *   https://stackoverflow.com/questions/8991192/check-the-file-size-without-opening-file-in-c/8991228#8991228
		 */
		if (0 != stat (fn, &datafile_stat))
#endif
		{
			upsdebugx(2, "%s: Can't stat %s (%s) currently", __func__, device_path, fn);
		} else {
			upsdebugx(2, "Located %s for device simulation data: %s", device_path, fn);
		}
	}
	if (testvar("repeater_disable_strict_start"))
	{
		repeater_disable_strict_start = 1;
	}
}

void upsdrv_cleanup(void)
{
	if ( (mode == MODE_META) || (mode == MODE_REPEATER) )
	{
		if (ups)
		{
			upscli_disconnect(ups);
		}

		if (ctx)
		{
			pconf_finish(ctx);
			free(ctx);
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

	/* FIXME: the below is only valid if (mode == MODE_DUMMY)
	 * if (mode == MODE_REPEATER) => forward
	 * if (mode == MODE_META) => ?
	 */
	if (!strncmp(varname, "ups.status", 10))
	{
		status_init();
		 /* FIXME: split and check values (support multiple values), à la usbhid-ups */
		status_set(val);
		status_commit();

		return STAT_SET_HANDLED;
	}

	/* Check variable validity */
	if (!is_valid_data(varname))
	{
		upsdebugx(2, "setvar: invalid variable name (%s)", varname);
		return STAT_SET_UNKNOWN;
	}

	/* Check value validity */
	if (!is_valid_value(varname, val))
	{
		upsdebugx(2, "setvar: invalid value (%s) for variable (%s)", val, varname);
		return STAT_SET_UNKNOWN;
	}

	/* If value is empty, remove the variable (FIXME: do we need
	 * a magic word?) */
	if (strlen(val) == 0)
	{
		dstate_delinfo(varname);
	}
	else
	{
		dstate_setinfo(varname, "%s", val);

		if ( (item = find_info(varname)) != NULL)
		{
			dstate_setflags(item->info_type, item->info_flags);

			/* Set max length for strings, if needed */
			if (item->info_flags & ST_FLAG_STRING)
				dstate_setaux(item->info_type, (long)item->info_len);
		}
		else
		{
			upsdebugx(2, "setvar: unknown variable (%s), using default flags", varname);
			dstate_setflags(varname, ST_FLAG_STRING | ST_FLAG_RW);
			dstate_setaux(varname, 32);
		}
	}
	return STAT_SET_HANDLED;
}

/*************************************************/
/*               Support functions               */
/*************************************************/

static int upsclient_update_vars(void)
{
	int		ret;
	size_t	numq, numa;
	const char	*query[4];
	char		**answer;

	query[0] = "VAR";
	query[1] = client_upsname;
	numq = 2;

	ret = upscli_list_start(ups, numq, query);

	if (ret < 0)
	{
		upsdebugx(1, "Error: %s (%i)", upscli_strerror(ups), upscli_upserror(ups));
		return ret;
	}

	while (upscli_list_next(ups, numq, query, &numa, &answer) == 1)
	{
		/* VAR <upsname> <varname> <val> */
		if (numa < 4)
		{
			upsdebugx(1, "Error: insufficient data (got %" PRIuSIZE " args, need at least 4)", numa);
		}

		upsdebugx(5, "Received: %s %s %s %s",
				answer[0], answer[1], answer[2], answer[3]);

		/* do not override the driver collection */
		if (strncmp(answer[2], "driver.", 7))
			setvar(answer[2], answer[3]);
	}
	return 1;
}

/* find info element definition in info array */
static dummy_info_t *find_info(const char *varname)
{
	dummy_info_t *item;

	for ( item = nut_data ; item->info_type != NULL ; item++ )
	{
		if (!strcasecmp(item->info_type, varname))
			return item;
	}

	upsdebugx(2, "find_info: unknown variable: %s", varname);

	return NULL;
}

/* check if data exists in our data table */
static int is_valid_data(const char* varname)
{
	dummy_info_t *item;

	if ( (item = find_info(varname)) != NULL)
	{
		return 1;
	}

	/* FIXME: we need to have the full data set before
	 * enforcing controls! We also need a way to automate
	 * the update / sync process (with cmdvartab?!) */

	upsdebugx(1, "Unknown data. Committing anyway...");
	return 1;
	/* return 0;*/
}

/* check if data's value validity */
static int is_valid_value(const char* varname, const char *value)
{
	dummy_info_t *item;
	NUT_UNUSED_VARIABLE(value);

	if ( (item = find_info(varname)) != NULL)
	{
		/* FIXME: test enum or bound against value */
		return 1;
	}

	/* FIXME: we need to have the full data set before
	 * enforcing controls! We also need a way to automate
	 * the update / sync process (with cmdvartab?) */

	upsdebugx(1, "Unknown data. Committing value anyway...");
	return 1;
	/* return 0;*/
}

/* called for fatal errors in parseconf like malloc failures */
static void upsconf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(ups.conf): %s", errmsg);
}

/* for dummy mode
 * parse the definition file and process its content
 */
static int parse_data_file(TYPE_FD arg_upsfd)
{
	char	fn[SMALLBUF];
	char	*ptr, var_value[MAX_STRING_SIZE];
	size_t	value_args = 0, counter;
	time_t	now;
	NUT_UNUSED_VARIABLE(arg_upsfd);

	time(&now);

	upsdebugx(1, "entering parse_data_file()");

	if (now < next_update)
	{
		upsdebugx(1, "leaving (paused)...");
		return 1;
	}

	/* initialise everything, to loop back at the beginning of the file */
	if (ctx == NULL)
	{
		ctx = (PCONF_CTX_t *)xmalloc(sizeof(PCONF_CTX_t));

		prepare_filepath(fn, sizeof(fn));
		pconf_init(ctx, upsconf_err);

		if (!pconf_file_begin(ctx, fn))
			fatalx(EXIT_FAILURE, "Can't open dummy-ups definition file %s: %s",
				fn, ctx->errmsg);
	}

	/* Reset the next call time, so that we can loop back on the file
	 * if there is no blocking action (ie TIMER) until the end of the file */
	next_update = -1;

	/* Now start or continue parsing... */
	while (pconf_file_next(ctx))
	{
		if (pconf_parse_error(ctx))
		{
			upsdebugx(2, "Parse error: %s:%d: %s",
				fn, ctx->linenum, ctx->errmsg);
			continue;
		}

		/* Check if we have something to process */
		if (ctx->numargs < 1)
			continue;

		/* Process actions (only "TIMER" ATM) */
		if (!strncmp(ctx->arglist[0], "TIMER", 5))
		{
			/* TIMER <seconds> will wait "seconds" before
			 * continuing the parsing */
			int delay = atoi (ctx->arglist[1]);
			time(&next_update);
			next_update += delay;
			upsdebugx(1, "suspending execution for %i seconds...", delay);
			break;
		}

		/* Remove ":" suffix, after the variable name */
		if ((ptr = strchr(ctx->arglist[0], ':')) != NULL)
			*ptr = '\0';

		upsdebugx(3, "parse_data_file: variable \"%s\" with %d args",
			ctx->arglist[0], (int)ctx->numargs);

		/* Skip the driver.* collection data */
		if (!strncmp(ctx->arglist[0], "driver.", 7))
		{
			upsdebugx(2, "parse_data_file: skipping %s", ctx->arglist[0]);
			continue;
		}

		/* From there, we get varname in arg[0], and values in other arg[1...x] */
		/* special handler for status */
		if (!strncmp( ctx->arglist[0], "ups.status", 10))
		{
			status_init();
			for (counter = 1, value_args = ctx->numargs ;
				counter < value_args ; counter++)
			{
				status_set(ctx->arglist[counter]);
			}
			status_commit();
		}
		else
		{
			for (counter = 1, value_args = ctx->numargs ;
				counter < value_args ; counter++)
			{
				if (counter == 1) /* don't append the first space separator */
					snprintf(var_value, sizeof(var_value), "%s", ctx->arglist[counter]);
				else
					snprintfcat(var_value, sizeof(var_value), " %s", ctx->arglist[counter]);
			}

			if (setvar(ctx->arglist[0], var_value) == STAT_SET_UNKNOWN)
			{
				upsdebugx(2, "parse_data_file: can't add \"%s\" with value \"%s\"\nError: %s",
					ctx->arglist[0], var_value, ctx->errmsg);
			}
			else
			{
				upsdebugx(3, "parse_data_file: added \"%s\" with value \"%s\"",
					ctx->arglist[0], var_value);
			}
		}
	}

	/* Cleanup parseconf if there is no pending action */
	if (next_update == -1)
	{
		pconf_finish(ctx);
		free(ctx);
		ctx=NULL;
	}
	return 1;
}
