/* dummy-ups.c - new testing driver

   Copyright (C) 2005  Arnaud Quette <http://arnaud.quette.free.fr/contact.html>

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
#include "parseconf.h"
#include "dummy-ups.h"

static int setvar(const char *varname, const char *val);
static int parse_data_file(int upsfd);
static dummy_info_t *find_info(const char *varname);
static int is_valid_data(const char* varname);
static int is_valid_value(const char* varname, const char *value);

void upsdrv_initinfo(void)
{
	dummy_info_t *item;

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
}

void upsdrv_updateinfo(void)
{
	upsdebugx(1, "upsdrv_updateinfo...");

	sleep(1);

	/* simply avoid driver staleness */
	status_init();
	status_set(dstate_getinfo("ups.status"));
	status_commit();

	dstate_dataok();
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

void upsdrv_banner(void)
{
	printf("Network UPS Tools - Dummy UPS driver %s (%s)\n\n", 
		DRV_VERSION, UPS_VERSION);
}

void upsdrv_initups(void)
{
	/* Nothing to do here... */
}

void upsdrv_cleanup(void)
{
	/* Nothing to do here... */
}


static int setvar(const char *varname, const char *val)
{
	dummy_info_t *item;

	upsdebugx(2, "entering setvar(%s, %s)", varname, val);

	if (!strncmp(varname, "ups.status", 10)) {
		status_init();
		 /* FIXME: split and check values (support multiple values) */
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

/* check if data exists */
static int is_valid_data(const char* varname)
{
	dummy_info_t *item;

	if ( (item = find_info(varname)) != NULL) {
			return 1;
	}

	return 0;
}

/* check if data's value validity */
static int is_valid_value(const char* varname, const char *value)
{
	dummy_info_t *item;

	if ( (item = find_info(varname)) != NULL) {
		/* FIXME: test enum or bound against value */
		return 1;
	}

	return 0;
}

/* called for fatal errors in parseconf like malloc failures */
static void upsconf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(ups.conf): %s", errmsg);
}

static int parse_data_file(int upsfd)
{
	char	fn[SMALLBUF];
	char	*ptr;
	PCONF_CTX_t	ctx;

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
		else
			upsdebugx(2, "parse_data_file: %s is not a driver. var", ctx.arglist[0]);

		/* From there, we get varname in arg[0], and values in other arg[1...x] */
		/* FIXME: iteration on arg[2, 3, ...]
			if ST_FLAG_STRING => all args are the value
			if ups.status, each arg is a value to be set (ie OB LB) + check against enum
			else int/float values need to be check against bound/enum
		*/
		if (setvar(ctx.arglist[0], ctx.arglist[1]) == STAT_SET_UNKNOWN)
			upsdebugx(2, "parse_data_file: can't add \"%s\" with value \"%s\"",
				ctx.arglist[0], ctx.arglist[1]);
		else
			upsdebugx(2, "parse_data_file: added \"%s\" with value \"%s\"",
				ctx.arglist[0], ctx.arglist[1]);
	}

	return 1;
}
