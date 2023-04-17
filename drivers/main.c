/* main.c - Network UPS Tools driver core

   Copyright (C)
   1999			Russell Kroll <rkroll@exploits.org>
   2005 - 2017	Arnaud Quette <arnaud.quette@free.fr>
   2017 		Eaton (author: Emilien Kia <EmilienKia@Eaton.com>)
   2017 - 2022	Jim Klimov <jimklimov+nut@gmail.com>

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
#include "main.h"
#include "nut_stdint.h"
#include "dstate.h"
#include "attribute.h"

#ifndef WIN32
# include <grp.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/* data which may be useful to the drivers */
TYPE_FD	upsfd = ERROR_FD;

char		*device_path = NULL;
const char	*progname = NULL, *upsname = NULL, *device_name = NULL;

/* may be set by the driver to wake up while in dstate_poll_fds */
TYPE_FD	extrafd = ERROR_FD;
#ifdef WIN32
static HANDLE	mutex = INVALID_HANDLE_VALUE;
#endif

/* for ser_open */
int	do_lock_port = 1;

/* for dstate->sock_connect, default to effectively
 * asynchronous (0) with fallback to synchronous (1) */
int	do_synchronous = -1;

/* for detecting -a values that don't match anything */
static	int	upsname_found = 0;

# ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
# endif /* DRIVERS_MAIN_WITHOUT_MAIN */
vartab_t	*vartab_h = NULL;

/* variables possibly set by the global part of ups.conf
 * user and group may be set globally or per-driver
 */
time_t	poll_interval = 2;
static char	*chroot_path = NULL, *user = NULL, *group = NULL;
static int	user_from_cmdline = 0, group_from_cmdline = 0;

/* signal handling */
int	exit_flag = 0;
/* reload_flag is 0 most of the time (including initial config reading),
 * and is briefly 1 when a reload signal is received and is being handled
 */
static int	reload_flag = 0;

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
/* should this driver instance go to background (default)
 * or stay foregrounded (default if -D/-d options are set on
 * command line)?
 * Value is tri-state:
 * -1 (default) Background the driver process
 *  0 User required to not background explicitly,
 *    or passed -D (or -d) and current value was -1
 *  1 User required to background even if with -D or dump_mode
 */
static int background_flag = -1;
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */

/* Users can pass a -D[...] option to enable debugging.
 * For the service tracing purposes, also the ups.conf
 * can define a debug_min value in the global or device
 * section, to set the minimal debug level (CLI provided
 * value less than that would not have effect, can only
 * have more).
 */
static int nut_debug_level_global = -1;
static int nut_debug_level_driver = -1;

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
/* everything else */
static char	*pidfn = NULL;
static int	dump_data = 0; /* Store the update_count requested */
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */

/* print the driver banner */
void upsdrv_banner (void)
{
	int i;

	printf("Network UPS Tools - %s %s (%s)\n", upsdrv_info.name, upsdrv_info.version, UPS_VERSION);

	/* process sub driver(s) information */
	for (i = 0; upsdrv_info.subdrv_info[i]; i++) {

		if (!upsdrv_info.subdrv_info[i]->name) {
			continue;
		}

		if (!upsdrv_info.subdrv_info[i]->version) {
			continue;
		}

		printf("%s %s\n", upsdrv_info.subdrv_info[i]->name,
			upsdrv_info.subdrv_info[i]->version);
	}
}

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
/* power down the attached load immediately */
static void forceshutdown(void)
	__attribute__((noreturn));

static void forceshutdown(void)
{
	upslogx(LOG_NOTICE, "Initiating UPS shutdown");

	/* the driver must not block in this function */
	upsdrv_shutdown();
	exit(EXIT_SUCCESS);
}

/* this function only prints the usage message; it does not call exit() */
static void help_msg(void)
{
	vartab_t	*tmp;

	nut_report_config_flags();

	printf("\nusage: %s (-a <id>|-s <id>) [OPTIONS]\n", progname);

	printf("  -a <id>        - autoconfig using ups.conf section <id>\n");
	printf("                 - note: -x after -a overrides ups.conf settings\n\n");

	printf("  -s <id>        - configure directly from cmd line arguments\n");
	printf("                 - note: must specify all driver parameters with successive -x\n");
	printf("                 - note: at least 'port' variable should be set\n");
	printf("                 - note: to explore the current values on a device from an\n");
	printf("                   unprivileged user account (with sufficient media access in\n");
	printf("                   the OS - e.g. to query networked devices), you can specify\n");
	printf("                   '-d 1' argument and `export NUT_STATEPATH=/tmp` beforehand\n\n");

	printf("  -V             - print version, then exit\n");
	printf("  -L             - print parseable list of driver variables\n");
	printf("  -D             - raise debugging level (and stay foreground by default)\n");
	printf("  -d <count>     - dump data to stdout after 'count' updates loop and exit\n");
	printf("  -F             - stay foregrounded even if no debugging is enabled\n");
	printf("  -B             - stay backgrounded even if debugging is bumped\n");
	printf("  -q             - raise log level threshold\n");
	printf("  -h             - display this help\n");
	printf("  -k             - force shutdown\n");
	printf("  -i <int>       - poll interval\n");
	printf("  -r <dir>       - chroot to <dir>\n");
	printf("  -u <user>      - switch to <user> (if started as root)\n");
	printf("  -g <group>     - set pipe access to <group> (if started as root)\n");
	printf("  -x <var>=<val> - set driver variable <var> to <val>\n");
	printf("                 - example: -x cable=940-0095B\n\n");

	if (vartab_h) {
		tmp = vartab_h;

		printf("Acceptable values for -x or ups.conf in this driver:\n\n");

		while (tmp) {
			if (tmp->vartype == VAR_VALUE)
				printf("%40s : -x %s=<value>\n",
					tmp->desc, tmp->var);
			else
				printf("%40s : -x %s\n", tmp->desc, tmp->var);
			tmp = tmp->next;
		}
	}

	upsdrv_help();
}
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */

/* store these in dstate as driver.(parameter|flag) */
# ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
# endif /* DRIVERS_MAIN_WITHOUT_MAIN */
void dparam_setinfo(const char *var, const char *val)
{
	char	vtmp[SMALLBUF];

	/* store these in dstate for debugging and other help */
	if (val) {
		snprintf(vtmp, sizeof(vtmp), "driver.parameter.%s", var);
		dstate_setinfo(vtmp, "%s", val);
		return;
	}

	/* no value = flag */

	snprintf(vtmp, sizeof(vtmp), "driver.flag.%s", var);
	dstate_setinfo(vtmp, "enabled");
}

/* cram var [= <val>] data into storage */
# ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
# endif /* DRIVERS_MAIN_WITHOUT_MAIN */
void storeval(const char *var, char *val)
{
	vartab_t	*tmp, *last;

	if (!strncasecmp(var, "override.", 9)) {
		dstate_setinfo(var+9, "%s", val);
		dstate_setflags(var+9, ST_FLAG_IMMUTABLE);
		return;
	}

	if (!strncasecmp(var, "default.", 8)) {
		dstate_setinfo(var+8, "%s", val);
		return;
	}

	tmp = last = vartab_h;

	while (tmp) {
		last = tmp;

		/* sanity check */
		if (!tmp->var) {
			tmp = tmp->next;
			continue;
		}

		/* later definitions overwrite earlier ones */
		if (!strcasecmp(tmp->var, var)) {
			free(tmp->val);

			if (val)
				tmp->val = xstrdup(val);

			/* don't keep things like SNMP community strings */
			if ((tmp->vartype & VAR_SENSITIVE) == 0)
				dparam_setinfo(var, val);

			tmp->found = 1;
			return;
		}

		tmp = tmp->next;
	}

	/* try to help them out */
	printf("\nFatal error: '%s' is not a valid %s for this driver.\n", var,
		val ? "variable name" : "flag");
	printf("\n");
	printf("Look in the man page or call this driver with -h for a list of\n");
	printf("valid variable names and flags.\n");

	exit(EXIT_SUCCESS);
}

/* retrieve the value of variable <var> if possible */
char *getval(const char *var)
{
	vartab_t	*tmp = vartab_h;

	while (tmp) {
		if (!strcasecmp(tmp->var, var))
			return(tmp->val);
		tmp = tmp->next;
	}

	return NULL;
}

/* see if <var> has been defined, even if no value has been given to it */
int testvar(const char *var)
{
	vartab_t	*tmp = vartab_h;

	while (tmp) {
		if (!strcasecmp(tmp->var, var))
			return tmp->found;
		tmp = tmp->next;
	}

	return 0;	/* not found */
}

/* See if <var> can be (re-)loaded now: either is reloadable by definition,
 * or no value has been given to it yet. Returns "-1" if nothing needs to
 * be done and that is not a failure (e.g. value not modified so we do not
 * care if we may change it or not).
 */
int testvar_reloadable(const char *var, const char *val, int vartype)
{
	vartab_t	*tmp = vartab_h;

	while (tmp) {
		if (!strcasecmp(tmp->var, var)) {
			/* variable name is known */
			if (val && tmp->val) {
				/* a value is already known */
				if (vartype == tmp->vartype && !strcasecmp(tmp->val, val)) {
					if (reload_flag) {
						upsdebugx(1, "%s: setting '%s' "
							"exists and is unmodified",
							__func__, var);
					}
					return -1;	/* no-op for caller */
				} else {
					/* warn loudly if we are reloading and
					 * can not change this modified value */
					upsdebugx((reload_flag ? (tmp->reloadable ? 1 : 0) : 1),
						"%s: setting '%s' exists and differs: "
						"new type bitmask %d vs. %d, "
						"new value '%s' vs. '%s'%s",
						__func__, var,
						vartype, tmp->vartype,
						val, tmp->val,
						((!reload_flag || tmp->reloadable) ? "" :
							" (driver restart is needed to apply)")
						);
					return (
						(!reload_flag)	/* For initial config reads, legacy code trusted what it saw */
						|| tmp->reloadable	/* set in addvar*() */
					);
				}
			}

			/* okay to redefine if not yet defined, or if reload is allowed,
			 * or if initially loading the configs
			 */
			return (
				(!reload_flag)
				|| ((!tmp->found) || tmp->reloadable)
			);
		}
		tmp = tmp->next;
	}

	return 1;	/* not found, may (re)load the definition */
}

/* Similar to testvar_reloadable() above which is for addvar*() defined
 * entries, but for less streamlined stuff defined right here in main.c.
 * See if value (probably saved in dstate) can be (re-)loaded now: either
 * it is reloadable by parameter definition, or no value has been saved
 * into it yet (<oldval> is NULL).
 * Returns "-1" if nothing needs to be done and that is not a failure
 * (e.g. value not modified so we do not care if we may change it or not).
 */
int testval_reloadable(const char *var, const char *oldval, const char *newval, int reloadable)
{
	/* Nothing saved yet? Okay to store new value! */
	if (!oldval)
		return 1;

	/* Should not happen? Or... (commented-away etc.) */
	if (!newval) {
		upslogx(LOG_WARNING, "%s: new setting for '%s' is NULL", __func__, var);
		return ((!reload_flag) || reloadable);
	}

	/* a value is already known, another is desired */
	if (!strcasecmp(oldval, newval)) {
		if (reload_flag) {
			upsdebugx(1, "%s: setting '%s' "
				"exists and is unmodified",
				__func__, var);
		}
		return -1;	/* no-op for caller */
	} else {
		/* warn loudly if we are reloading and
		 * can not change this modified value */
		upsdebugx((reload_flag ? (reloadable ? 1 : 0) : 1),
			"%s: setting '%s' exists and differs: "
			"new value '%s' vs. '%s'%s",
			__func__, var,
			newval, oldval,
			((!reload_flag || reloadable) ? "" :
				" (driver restart is needed to apply)")
			);
		/* For initial config reads, legacy code trusted what it saw */
		return ((!reload_flag) || reloadable);
	}
}

/* Similar to testvar_reloadable() above which is for addvar*() defined
 * entries, but for less streamlined stuff defined right here in main.c.
 * See if <var> (by <arg> name saved in dstate) can be (re-)loaded now:
 * either it is reloadable by parameter definition, or no value has been
 * saved into it yet (<oldval> is NULL).
 * Returns "-1" if nothing needs to be done and that is not a failure
 * (e.g. value not modified so we do not care if we may change it or not).
 */
int testinfo_reloadable(const char *var, const char *infoname, const char *newval, int reloadable)
{
	/* Keep legacy behavior: not reloading, trust the initial config */
	if (!reload_flag || !infoname)
		return 1;

	/* Only if reloading, suffer the overhead of lookups: */
	return testval_reloadable(var, dstate_getinfo(infoname), newval, reloadable);
}

/* implement callback from driver - create the table for -x/conf entries */
static void do_addvar(int vartype, const char *name, const char *desc, int reloadable)
{
	vartab_t	*tmp, *last;

	tmp = last = vartab_h;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(vartab_t));

	tmp->vartype = vartype;
	tmp->var = xstrdup(name);
	tmp->val = NULL;
	tmp->desc = xstrdup(desc);
	tmp->found = 0;
	tmp->reloadable = reloadable;
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		vartab_h = tmp;
}

/* public callback from driver - create the table for -x/conf entries for reloadable values */
void addvar_reloadable(int vartype, const char *name, const char *desc)
{
	do_addvar(vartype, name, desc, 1);
}

/* public callback from driver - create the table for -x/conf entries for set-once values */
void addvar(int vartype, const char *name, const char *desc)
{
	do_addvar(vartype, name, desc, 0);
}

/* handle -x / ups.conf config details that are for this part of the code */
static int main_arg(char *var, char *val)
{
	/* flags for main */

	upsdebugx(3, "%s: var='%s' val='%s'",
		__func__,
		var ? var : "<null>", /* null should not happen... but... */
		val ? val : "<null>");

	/* !reload_flag quietly forbids changing this flag on the fly, as
	 * it would have no effect anyway without a (serial) reconnection
	 */
	if (!strcmp(var, "nolock") && !reload_flag) {
		do_lock_port = 0;
		dstate_setinfo("driver.flag.nolock", "enabled");
		return 1;	/* handled */
	}

	/* FIXME: this one we could potentially reload, but need to figure
	 * out that the flag line was commented away or deleted -- there is
	 * no setting value to flip in configs here
	 */
	if (!strcmp(var, "ignorelb") && !reload_flag) {
		dstate_setinfo("driver.flag.ignorelb", "enabled");
		return 1;	/* handled */
	}

	/* any other flags are for the driver code */
	if (!val)
		return 0;	/* unhandled, pass it through to the driver */

	/* In checks below, testinfo_reloadable(..., 0) should forbid
	 * re-population of the setting with a new value, but emit a
	 * warning if it did change (so driver restart is needed to apply)
	 */

	/* variables for main: port */

	if (!strcmp(var, "port") && testinfo_reloadable(var, "driver.parameter.port", val, 0) > 0) {
		device_path = xstrdup(val);
		device_name = xbasename(device_path);
		dstate_setinfo("driver.parameter.port", "%s", val);
		return 1;	/* handled */
	}

	/* user specified at the driver level overrides that on global level
	 * or the built-in default
	 */
	if (!strcmp(var, "user") && testval_reloadable(var, user, val, 0) > 0) {
		if (user_from_cmdline) {
			upsdebugx(0, "User '%s' specified in driver section "
				"was ignored due to '%s' specified on command line",
				val, user);
		} else {
			upsdebugx(1, "Overriding previously specified user '%s' "
				"with '%s' specified for driver section",
				user, val);
			free(user);
			user = xstrdup(val);
		}
		return 1;	/* handled */
	}

	if (!strcmp(var, "group") && testval_reloadable(var, group, val, 0) > 0) {
		if (group_from_cmdline) {
			upsdebugx(0, "Group '%s' specified in driver section "
				"was ignored due to '%s' specified on command line",
				val, group);
		} else {
			upsdebugx(1, "Overriding previously specified group '%s' "
				"with '%s' specified for driver section",
				group, val);
			free(group);
			group = xstrdup(val);
		}
		return 1;	/* handled */
	}

	if (!strcmp(var, "sddelay")) {
		upslogx(LOG_INFO, "Obsolete value sddelay found in ups.conf");
		return 1;	/* handled */
	}

	/* Allow per-driver overrides of the global setting
	 * and allow to reload this, why not.
	 * Note: having both global+driver section definitions may
	 * cause noise, but it allows either to be commented away
	 * and the other to take hold. Both disappearing would not
	 * be noticed by the reload operation currently, however.
	 */
	if (!strcmp(var, "pollinterval")) {
		int do_handle = 1;
		char buf[SMALLBUF];

		/* log a message if value changed; skip if no good buf */
		if (snprintf(buf, sizeof(buf), "%" PRIdMAX, (intmax_t)poll_interval)) {
			if ((do_handle = testval_reloadable(var, buf, val, 1)) == 0) {
				/* Should not happen, but... */
				fatalx(EXIT_FAILURE, "Error: failed to check "
					"testval_reloadable() for pollinterval: "
					"old %s vs. new %s", buf, NUT_STRARG(val));
			}
		}

		if (do_handle > 0) {
			int ipv = atoi(val);
			if (ipv > 0) {
				poll_interval = (time_t)ipv;
			} else {
				fatalx(EXIT_FAILURE, "Error: UPS [%s]: invalid pollinterval: %d",
					NUT_STRARG(upsname), ipv);
			}
		}	/* else: no-op */

		return 1;	/* handled */
	}

	/* Allow per-driver overrides of the global setting
	 * and allow to reload this, why not.
	 * Note: this may cause "spurious" redefinitions of the
	 * "no" setting which is the fallback for random values.
	 * Also note that global+driver section definitions may
	 * cause noise, but it allows either to be commented away
	 * and the other to take hold. Both disappearing would not
	 * be noticed by the reload operation currently, however.
	 */
	if (!strcmp(var, "synchronous")
	&& testval_reloadable(var, ((do_synchronous==1)?"yes":((do_synchronous==0)?"no":"auto")), val, 1) > 0
	) {
		if (!strcmp(val, "yes"))
			do_synchronous=1;
		else
		if (!strcmp(val, "auto"))
			do_synchronous=-1;
		else
			do_synchronous=0;

		return 1;	/* handled */
	}

	/* only for upsdrvctl - ignored here */
	if (!strcmp(var, "sdorder"))
		return 1;	/* handled */

	/* only for upsd (at the moment) - ignored here */
	if (!strcmp(var, "desc"))
		return 1;	/* handled */

	/* Allow each driver to specify its minimal debugging level -
	 * admins can set more with command-line args, but can't set
	 * less without changing config. Should help debug of services. */
	if (!strcmp(var, "debug_min")) {
		int lvl = -1; // typeof common/common.c: int nut_debug_level
		if ( str_to_int (val, &lvl, 10) && lvl >= 0 ) {
			nut_debug_level_driver = lvl;
		} else {
			upslogx(LOG_INFO, "WARNING : Invalid debug_min value found in ups.conf for the driver");
		}
		return 1;	/* handled */
	}

	return 0;	/* unhandled, pass it through to the driver */
}

static void do_global_args(const char *var, const char *val)
{
	char buf[SMALLBUF];

	upsdebugx(3, "%s: var='%s' val='%s'",
		__func__,
		var ? var : "<null>", /* null should not happen... but... */
		val ? val : "<null>");

	/* Allow to reload this, why not */
	if (!strcmp(var, "pollinterval")) {
		int do_handle = 1;

		/* log a message if value changed; skip if no good buf */
		if (snprintf(buf, sizeof(buf), "%" PRIdMAX, (intmax_t)poll_interval)) {
			if ((do_handle = testval_reloadable(var, buf, val, 1)) == 0) {
				/* Should not happen, but... */
				fatalx(EXIT_FAILURE, "Error: failed to check "
					"testval_reloadable() for pollinterval: "
					"old %s vs. new %s", buf, val);
			}
		}

		if (do_handle > 0) {
			int ipv = atoi(val);
			if (ipv > 0) {
				poll_interval = (time_t)ipv;
			} else {
				fatalx(EXIT_FAILURE, "Error: invalid pollinterval: %d", ipv);
			}
		}	/* else: no-op */

		return;
	}

	/* In checks below, testinfo_reloadable(..., 0) should forbid
	 * re-population of the setting with a new value, but emit a
	 * warning if it did change (so driver restart is needed to apply)
	 */

	if (!strcmp(var, "chroot") && testval_reloadable(var, chroot_path, val, 0) > 0) {
		free(chroot_path);
		chroot_path = xstrdup(val);
	}

	if (!strcmp(var, "user") && testval_reloadable(var, user, val, 0) > 0) {
		if (user_from_cmdline) {
			upsdebugx(0, "User specified in global section '%s' "
				"was ignored due to '%s' specified on command line",
				val, user);
		} else {
			upsdebugx(1, "Overriding previously specified user '%s' "
				"with '%s' specified in global section",
				user, val);
			free(user);
			user = xstrdup(val);
		}
	}

	if (!strcmp(var, "group") && testval_reloadable(var, group, val, 0) > 0) {
		if (group_from_cmdline) {
			upsdebugx(0, "Group specified in global section '%s' "
				"was ignored due to '%s' specified on command line",
				val, group);
		} else {
			upsdebugx(1, "Overriding previously specified group '%s' "
				"with '%s' specified in global section",
				group, val);
			free(group);
			group = xstrdup(val);
		}
	}

	/* Allow to reload this, why not
	 * Note: this may cause "spurious" redefinitions of the
	 * "no" setting which is the fallback for random values.
	 * Also note that global+driver section definitions may
	 * cause noise, but it allows either to be commented away
	 * and the other to take hold. Both disappearing would not
	 * be noticed by the reload operation currently, however.
	 */
	if (!strcmp(var, "synchronous")
	&& testval_reloadable(var, ((do_synchronous==1)?"yes":((do_synchronous==0)?"no":"auto")), val, 1) > 0
	) {
		if (!strcmp(val, "yes"))
			do_synchronous=1;
		else
		if (!strcmp(val, "auto"))
			do_synchronous=-1;
		else
			do_synchronous=0;
	}

	/* Allow to specify its minimal debugging level for all drivers -
	 * admins can set more with command-line args, but can't set
	 * less without changing config. Should help debug of services. */
	if (!strcmp(var, "debug_min")) {
		int lvl = -1; // typeof common/common.c: int nut_debug_level
		if ( str_to_int (val, &lvl, 10) && lvl >= 0 ) {
			nut_debug_level_global = lvl;
		} else {
			upslogx(LOG_INFO, "WARNING : Invalid debug_min value found in ups.conf global settings");
		}
	}

	/* unrecognized */
}

void do_upsconf_args(char *confupsname, char *var, char *val)
{
	char	tmp[SMALLBUF];

	/* handle global declarations */
	if (!confupsname) {
		do_global_args(var, val);
		return;
	}

	/* no match = not for us */
	if (strcmp(confupsname, upsname) != 0)
		return;

	upsname_found = 1;

	if (main_arg(var, val))
		return;

	/* flags (no =) now get passed to the driver-level stuff */
	if (!val) {
		/* also store this, but it's a bit different */
		snprintf(tmp, sizeof(tmp), "driver.flag.%s", var);

		/* allow reloading if defined and permitted via addvar()
		 * or not defined there (FIXME?)
		 */
		if (testvar_reloadable(var, NULL, VAR_FLAG) > 0) {
			dstate_setinfo(tmp, "enabled");
			storeval(var, NULL);
		}

		return;
	}

	/* In checks below, testval_reloadable(..., 0) should forbid
	 * re-population of the setting with a new value, but emit a
	 * warning if it did change (so driver restart is needed to apply)
	 */

	/* don't let the user shoot themselves in the foot
	 * reload should not allow changes here, but would report
	 */
	if (!strcmp(var, "driver") && testval_reloadable(var, progname, val, 0) > 0) {
		/* Accomodate for libtool wrapped developer iterations
		 * running e.g. `drivers/.libs/lt-dummy-ups` filenames
		 */
		size_t tmplen = strlen("lt-");
		if (strncmp("lt-", progname, tmplen) == 0
		&&  strcmp(val, progname + tmplen) == 0) {
			/* debug level may be not initialized yet, and situation
			 * should not happen in end-user builds, so ok to yell: */
			upsdebugx(0, "Seems this driver binary %s is a libtool "
				"wrapped build for driver %s", progname, val);
			/* progname points to xbasename(argv[0]) in-place;
			 * roll the pointer forward a bit, we know we can:
			 */
			progname = progname + tmplen;
		}

		if (strcmp(val, progname) != 0) {
			fatalx(EXIT_FAILURE, "Error: UPS [%s] is for driver %s, but I'm %s!\n",
				confupsname, val, progname);
		}
		return;
	}

	/* everything else must be for the driver */

	/* allow reloading if defined and permitted via addvar()
	 * or not defined there (FIXME?)
	 */
	if (testvar_reloadable(var, val, VAR_VALUE) > 0) {
		storeval(var, val);
	}
}

static void assign_debug_level(void) {
	/* CLI debug level can not be smaller than debug_min specified
	 * in ups.conf, and value specified for a driver config section
	 * overrides the global one. Note that non-zero debug_min does
	 * not impact foreground running mode.
	 */
	int nut_debug_level_upsconf = -1;

	if (nut_debug_level_global >= 0 && nut_debug_level_driver >= 0) {
		nut_debug_level_upsconf = nut_debug_level_driver;
	} else {
		if (nut_debug_level_global >= 0) nut_debug_level_upsconf = nut_debug_level_global;
		if (nut_debug_level_driver >= 0) nut_debug_level_upsconf = nut_debug_level_driver;
	}

	if (nut_debug_level_upsconf > nut_debug_level)
		nut_debug_level = nut_debug_level_upsconf;

	upsdebugx(1, "debug level is '%d'", nut_debug_level);
}

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
/* split -x foo=bar into 'foo' and 'bar' */
static void splitxarg(char *inbuf)
{
	char	*eqptr, *val, *buf;

	/* make our own copy - avoid changing argv */
	buf = xstrdup(inbuf);

	eqptr = strchr(buf, '=');

	if (!eqptr)
		val = NULL;
	else {
		*eqptr++ = '\0';
		val = eqptr;
	}

	/* see if main handles this first */
	if (main_arg(buf, val))
		return;

	/* otherwise store it for later */
	storeval(buf, val);
}

/* dump the list from the vartable for external parsers */
static void listxarg(void)
{
	vartab_t	*tmp;

	tmp = vartab_h;

	if (!tmp)
		return;

	while (tmp) {

		switch (tmp->vartype) {
			case VAR_VALUE: printf("VALUE"); break;
			case VAR_FLAG: printf("FLAG"); break;
			default: printf("UNKNOWN"); break;
		}

		printf(" %s \"%s\"\n", tmp->var, tmp->desc);

		tmp = tmp->next;
	}
}
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */

# ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
# endif /* DRIVERS_MAIN_WITHOUT_MAIN */
void vartab_free(void)
{
	vartab_t	*tmp, *next;

	tmp = vartab_h;

	while (tmp) {
		next = tmp->next;

		free(tmp->var);
		free(tmp->val);
		free(tmp->desc);
		free(tmp);

		tmp = next;
	}
}

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static void exit_upsdrv_cleanup(void)
{
	dstate_setinfo("driver.state", "cleanup.upsdrv");
	upsdrv_cleanup();
}

static void exit_cleanup(void)
{
	dstate_setinfo("driver.state", "cleanup.exit");

	if (!dump_data) {
		upsnotify(NOTIFY_STATE_STOPPING, "exit_cleanup()");
	}

	free(chroot_path);
	free(device_path);
	free(user);
	free(group);

	if (pidfn) {
		unlink(pidfn);
		free(pidfn);
	}

	dstate_free();
	vartab_free();

#ifdef WIN32
	if(mutex != INVALID_HANDLE_VALUE) {
		ReleaseMutex(mutex);
		CloseHandle(mutex);
	}
#endif
}
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */

void set_exit_flag(int sig)
{
	exit_flag = sig;
}

#ifndef WIN32
/* TODO: Equivalent for WIN32 - see SIGCMD_RELOAD in upd and upsmon */
static void set_reload_flag(int sig)
{
	NUT_UNUSED_VARIABLE(sig);
	reload_flag = 1;
}

# ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
# endif /* DRIVERS_MAIN_WITHOUT_MAIN */
void setup_signals(void)
{
	struct sigaction	sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	/* handle shutdown signals */
	sa.sa_handler = set_exit_flag;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

	/* basic signal setup to ignore SIGPIPE */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
	sa.sa_handler = SIG_IGN;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic pop
#endif
	sigaction(SIGPIPE, &sa, NULL);

	/* handle reloading */
	sa.sa_handler = set_reload_flag;
	sigaction(SIGHUP, &sa, NULL);
}
#endif /* WIN32*/

/* This source file is used in some unit tests to mock realistic driver
 * behavior - using a production driver skeleton, but their own main().
 */
#ifndef DRIVERS_MAIN_WITHOUT_MAIN
int main(int argc, char **argv)
{
	struct	passwd	*new_uid = NULL;
	int	i, do_forceshutdown = 0;
	int	update_count = 0;

	dstate_setinfo("driver.state", "init.starting");

	atexit(exit_cleanup);

	/* pick up a default from configure --with-user */
	user = xstrdup(RUN_AS_USER);	/* xstrdup: this gets freed at exit */

	/* pick up a default from configure --with-group */
	group = xstrdup(RUN_AS_GROUP);	/* xstrdup: this gets freed at exit */

	progname = xbasename(argv[0]);

#ifdef WIN32
	const char * drv_name;
	drv_name = xbasename(argv[0]);
	/* remove trailing .exe */
	char * dot = strrchr(drv_name,'.');
	if( dot != NULL ) {
		if(strcasecmp(dot, ".exe") == 0 ) {
			progname = strdup(drv_name);
			char * t = strrchr(progname,'.');
			*t = 0;
		}
	}
	else {
		progname = strdup(drv_name);
	}
#endif

	open_syslog(progname);

	upsdrv_banner();

	if (upsdrv_info.status == DRV_EXPERIMENTAL) {
		printf("Warning: This is an experimental driver.\n");
		printf("Some features may not function correctly.\n\n");
	}

	/* build the driver's extra (-x) variable table */
	upsdrv_makevartable();

	while ((i = getopt(argc, argv, "+a:s:kFBDd:hx:Lqr:u:g:Vi:")) != -1) {
		switch (i) {
			case 'a':
				if (upsname)
					fatalx(EXIT_FAILURE, "Error: options '-a id' and '-s id' "
						"are mutually exclusive and single-use only.");

				upsname = optarg;

				read_upsconf();

				if (!upsname_found)
					fatalx(EXIT_FAILURE, "Error: Section %s not found in ups.conf",
						optarg);
				break;
			case 's':
				if (upsname)
					fatalx(EXIT_FAILURE, "Error: options '-a id' and '-s id' "
						"are mutually exclusive and single-use only.");

				upsname = optarg;
				upsname_found = 1;
				break;
			case 'F':
				background_flag = 0;
				break;
			case 'B':
				background_flag = 1;
				break;
			case 'D':
				nut_debug_level++;
				break;
			case 'd':
				dump_data = atoi(optarg);
				break;
			case 'i': { /* scope */
					int ipv = atoi(optarg);
					if (ipv > 0) {
						poll_interval = (time_t)ipv;
					} else {
						fatalx(EXIT_FAILURE, "Error: command-line: invalid pollinterval: %d",
							ipv);
					}
				}
				break;
			case 'k':
				do_lock_port = 0;
				do_forceshutdown = 1;
				break;
			case 'L':
				listxarg();
				exit(EXIT_SUCCESS);
			case 'q':
				nut_log_level++;
				break;
			case 'r':
				chroot_path = xstrdup(optarg);
				break;
			case 'u':
				if (user_from_cmdline) {
					upsdebugx(1, "Previously specified user for drivers '%s' "
						"was ignored due to '%s' specified on command line"
						" (again?)",
						user, optarg);
				} else {
					upsdebugx(1, "Built-in default or configured user "
						"for drivers '%s' was ignored due to '%s' "
						"specified on command line",
						user, optarg);
				}
				free(user);
				user = xstrdup(optarg);
				user_from_cmdline = 1;
				break;
			case 'g':
				if (group_from_cmdline) {
					upsdebugx(1, "Previously specified group for drivers '%s' "
						"was ignored due to '%s' specified on command line"
						" (again?)",
						group, optarg);
				} else {
					upsdebugx(1, "Built-in default or configured group "
						"for drivers '%s' was ignored due to '%s' "
						"specified on command line",
						group, optarg);
				}
				free(group);
				group = xstrdup(optarg);
				group_from_cmdline = 1;
				break;
			case 'V':
				/* already printed the banner for program name */
				nut_report_config_flags();
				exit(EXIT_SUCCESS);
			case 'x':
				splitxarg(optarg);
				break;
			case 'h':
				help_msg();
				exit(EXIT_SUCCESS);
			default:
				fatalx(EXIT_FAILURE,
					"Error: unknown option -%c. Try -h for help.", i);
		}
	}

	if (nut_debug_level > 0 || dump_data) {
		if ( background_flag < 0 ) {
			/* Only flop from default - stay foreground with debug on */
			background_flag = 0;
		} else {
			upsdebugx (0,
				"Debug level is %d, dump data count is %s, "
				"but backgrounding mode requested as %s",
				nut_debug_level,
				dump_data ? "on" : "off",
				background_flag ? "on" : "off"
				);
		}
	} /* else: default remains `background_flag==-1` where nonzero is true */

	/* Since debug mode dumps from drivers are often posted to mailing list
	 * or issue tracker, as well as viewed locally, it can help to know the
	 * build options involved when troubleshooting (especially when needed
	 * to walk through building a PR branch with candidate fix for an issue).
	 * Reference code for such message is in common/common.c prints to log
	 * and/or syslog when any debug level is enabled.
	 */
	nut_report_config_flags();

	argc -= optind;
	argv += optind;

	if (argc > 0) {
		fatalx(EXIT_FAILURE,
			"Error: too many non-option arguments. Try -h for help.");
	}

	if (!upsname_found) {
		fatalx(EXIT_FAILURE,
			"Error: specifying '-a id' or '-s id' is now mandatory. Try -h for help.");
	}

	/* we need to get the port from somewhere */
	if (!device_path) {
		fatalx(EXIT_FAILURE,
			"Error: you must specify a port name in ups.conf or in '-x port=...' argument.\n"
			"Try -h for help.");
	}

	assign_debug_level();

	new_uid = get_user_pwent(user);

	if (chroot_path)
		chroot_start(chroot_path);

	become_user(new_uid);

	/* Only switch to statepath if we're not powering off
	 * or not just dumping data (for discovery) */
	/* This avoids case where ie /var is unmounted already */
#ifndef WIN32
	if ((!do_forceshutdown) && (!dump_data) && (chdir(dflt_statepath())))
		fatal_with_errno(EXIT_FAILURE, "Can't chdir to %s", dflt_statepath());

	/* Setup signals to communicate with driver once backgrounded. */
	if ((background_flag != 0) && (!do_forceshutdown)) {
		char	buffer[SMALLBUF];

		setup_signals();

		snprintf(buffer, sizeof(buffer), "%s/%s-%s.pid", altpidpath(), progname, upsname);

		/* Try to prevent that driver is started multiple times. If a PID file */
		/* already exists, send a TERM signal to the process and try if it goes */
		/* away. If not, retry a couple of times. */
		for (i = 0; i < 3; i++) {
			struct stat	st;

			if (stat(buffer, &st) != 0) {
				/* PID file not found */
				break;
			}

			upslogx(LOG_WARNING, "Duplicate driver instance detected (PID file %s exists)! Terminating other driver!", buffer);

			if (sendsignalfn(buffer, SIGTERM) != 0) {
				/* Can't send signal to PID, assume invalid file */
				break;
			}

			/* Allow driver some time to quit */
			sleep(5);
		}

		if (i > 0) {
			struct stat	st;
			if (stat(buffer, &st) == 0) {
				upslogx(LOG_WARNING, "Duplicate driver instance is still alive (PID file %s exists) after several termination attempts! Killing other driver!", buffer);
				if (sendsignalfn(buffer, SIGKILL) == 0) {
					sleep(5);
					if (sendsignalfn(buffer, 0) == 0) {
						upslogx(LOG_WARNING, "Duplicate driver instance is still alive (could signal the process)");
						/* TODO: Should we writepid() below in this case?
						 * Or if driver init fails, restore the old content
						 * for that running sibling? */
					} else {
						upslogx(LOG_WARNING, "Could not signal the other driver after kill, either its process is finally dead or owned by another user!");
					}
				} else {
					upslogx(LOG_WARNING, "Could not signal the other driver, either its process is dead or owned by another user!");
				}
				/* Note: PID file would remain here, but invalid
				 * as far as further killers would be concerned */
			}
		}

		/* Only write pid if we're not just dumping data, for discovery */
		if (!dump_data) {
			pidfn = xstrdup(buffer);
			writepid(pidfn);	/* before backgrounding */
		}
	}
#else
	char	name[SMALLBUF];

	snprintf(name,sizeof(name), "%s-%s",progname,upsname);

	mutex = CreateMutex(NULL,TRUE,name);
	if(mutex == NULL ) {
		if( GetLastError() != ERROR_ACCESS_DENIED ) {
			fatalx(EXIT_FAILURE, "Can not create mutex %s : %d.\n",name,(int)GetLastError());
		}
	}

	if (GetLastError() == ERROR_ALREADY_EXISTS || GetLastError() == ERROR_ACCESS_DENIED) {
		upslogx(LOG_WARNING, "Duplicate driver instance detected! Terminating other driver!");
		for(i=0;i<10;i++) {
			DWORD res;
			sendsignal(name, COMMAND_STOP);
			if(mutex != NULL ) {
				res = WaitForSingleObject(mutex,1000);
				if(res==WAIT_OBJECT_0) {
					break;
				}
			}
			else {
				sleep(1);
				mutex = CreateMutex(NULL,TRUE,name);
				if(mutex != NULL ) {
					break;
				}
			}
		}
		if(i >= 10 ) {
			fatalx(EXIT_FAILURE, "Can not terminate the previous driver.\n");
		}
	}
#endif

	/* clear out callback handler data */
	memset(&upsh, '\0', sizeof(upsh));

	/* note: device.type is set early to be overridden by the driver
	 * when its a pdu! */
	dstate_setinfo("device.type", "ups");

	dstate_setinfo("driver.state", "init.device");
	upsdrv_initups();
	dstate_setinfo("driver.state", "init.quiet");

	/* UPS is detected now, cleanup upon exit */
	atexit(exit_upsdrv_cleanup);

	/* now see if things are very wrong out there */
	if (upsdrv_info.status == DRV_BROKEN) {
		fatalx(EXIT_FAILURE, "Fatal error: broken driver. It probably needs to be converted.\n");
	}

	if (do_forceshutdown)
		forceshutdown();

	/* publish the top-level data: version numbers, driver name */
	dstate_setinfo("driver.version", "%s", UPS_VERSION);
	dstate_setinfo("driver.version.internal", "%s", upsdrv_info.version);
	dstate_setinfo("driver.name", "%s", progname);

	/*
	 * If we are not debugging, send the early startup logs generated by
	 * upsdrv_initinfo() and upsdrv_updateinfo() to syslog, not just stderr.
	 * Otherwise these logs are lost.
	 */
	if ((nut_debug_level == 0) && (!dump_data))
		syslogbit_set();

	/* get the base data established before allowing connections */
	dstate_setinfo("driver.state", "init.info");
	upsdrv_initinfo();
	/* Note: a few drivers also call their upsdrv_updateinfo() during
	 * their upsdrv_initinfo(), possibly to impact the initialization */
	dstate_setinfo("driver.state", "init.updateinfo");
	upsdrv_updateinfo();
	dstate_setinfo("driver.state", "init.quiet");

	if (dstate_getinfo("driver.flag.ignorelb")) {
		int	have_lb_method = 0;

		if (dstate_getinfo("battery.charge") && dstate_getinfo("battery.charge.low")) {
			upslogx(LOG_INFO, "using 'battery.charge' to set battery low state");
			have_lb_method++;
		}

		if (dstate_getinfo("battery.runtime") && dstate_getinfo("battery.runtime.low")) {
			upslogx(LOG_INFO, "using 'battery.runtime' to set battery low state");
			have_lb_method++;
		}

		if (!have_lb_method) {
			fatalx(EXIT_FAILURE,
				"The 'ignorelb' flag is set, but there is no way to determine the\n"
				"battery state of charge.\n\n"
				"Only set this flag if both 'battery.charge' and 'battery.charge.low'\n"
				"and/or 'battery.runtime' and 'battery.runtime.low' are available.\n");
		}
	}

	/* now we can start servicing requests */
	/* Only write pid if we're not just dumping data, for discovery */
	if (!dump_data) {
		char * sockname = dstate_init(progname, upsname);
		/* Normally we stick to the built-in account info,
		 * so if they were not over-ridden - no-op here:
		 */
		if (strcmp(group, RUN_AS_GROUP)
		||  strcmp(user,  RUN_AS_USER)
		) {
#ifndef WIN32
			int allOk = 1;
			/* Tune group access permission to the pipe,
			 * so that upsd can access it (using the
			 * specified or retained default group):
			 */
			struct group *grp = getgrnam(group);
			upsdebugx(1, "Group and/or user account for this driver "
				"was customized ('%s:%s') compared to built-in "
				"defaults. Fixing socket '%s' ownership/access.",
				user, group, sockname);

			if (grp == NULL) {
				upsdebugx(1, "WARNING: could not resolve "
					"group name '%s': %s",
					group, strerror(errno)
				);
				allOk = 0;
			} else {
				struct stat statbuf;
				mode_t mode;
				if (chown(sockname, -1, grp->gr_gid)) {
					upsdebugx(1, "WARNING: chown failed: %s",
						strerror(errno)
					);
					allOk = 0;
				}

				if (stat(sockname, &statbuf)) {
					/* Logically we'd fail chown above if file
					 * does not exist or is not accessible, but
					 * practically we only need stat for chmod
					 */
					upsdebugx(1, "WARNING: stat failed: %s",
						strerror(errno)
					);
					allOk = 0;
				} else {
					/* chmod g+rw sockname */
					mode = statbuf.st_mode;
					mode |= S_IWGRP;
					mode |= S_IRGRP;
					if (chmod(sockname, mode)) {
						upsdebugx(1, "WARNING: chmod failed: %s",
							strerror(errno)
						);
						allOk = 0;
					}
				}
			}

			if (allOk) {
				upsdebugx(1, "Group access for this driver successfully fixed");
			} else {
				upsdebugx(0, "WARNING: Needed to fix group access "
					"to filesystem socket of this driver, but failed; "
					"run the driver with more debugging to see how exactly.\n"
					"Consumers of the socket, such as upsd data server, "
					"can fail to interact with the driver and represent "
					"the device: %s",
					sockname);
			}
#else	/* not WIN32 */
			upsdebugx(1, "Options for alternate user/group are not implemented on this platform");
#endif	/* WIN32 */
		}
		free(sockname);
	}

	/* The poll_interval may have been changed from the default */
	dstate_setinfo("driver.parameter.pollinterval", "%" PRIdMAX, (intmax_t)poll_interval);

	/* The synchronous option may have been changed from the default */
	dstate_setinfo("driver.parameter.synchronous", "%s",
		(do_synchronous==1)?"yes":((do_synchronous==0)?"no":"auto"));

	/* remap the device.* info from ups.* for the transition period */
	if (dstate_getinfo("ups.mfr") != NULL)
		dstate_setinfo("device.mfr", "%s", dstate_getinfo("ups.mfr"));
	if (dstate_getinfo("ups.model") != NULL)
		dstate_setinfo("device.model", "%s", dstate_getinfo("ups.model"));
	if (dstate_getinfo("ups.serial") != NULL)
		dstate_setinfo("device.serial", "%s", dstate_getinfo("ups.serial"));

	if (background_flag != 0) {
		background();
		writepid(pidfn);	/* PID changes when backgrounding */
	}

	dstate_setinfo("driver.state", "quiet");
	if (dump_data) {
		upsdebugx(1, "Driver initialization completed, beginning data dump (%d loops)", dump_data);
	} else {
		upsdebugx(1, "Driver initialization completed, beginning regular infinite loop");
		upsnotify(NOTIFY_STATE_READY_WITH_PID, NULL);
	}

	while (!exit_flag) {
		struct timeval	timeout;

		if (!dump_data) {
			upsnotify(NOTIFY_STATE_WATCHDOG, NULL);
		}

		gettimeofday(&timeout, NULL);
		timeout.tv_sec += poll_interval;

		dstate_setinfo("driver.state", "updateinfo");
		upsdrv_updateinfo();
		dstate_setinfo("driver.state", "quiet");

		/* Dump the data tree (in upsc-like format) to stdout and exit */
		if (dump_data) {
			/* Wait for 'dump_data' update loops to ensure data completion */
			if (update_count == dump_data) {
				dstate_setinfo("driver.state", "dumping");
				dstate_dump();
				exit_flag = 1;
			}
			else
				update_count++;
		}
		else {
			while (!dstate_poll_fds(timeout, extrafd) && !exit_flag) {
				/* repeat until time is up or extrafd has data */
			}
		}

		if (reload_flag && !exit_flag) {
			dstate_setinfo("driver.state", "reloading");
			upsnotify(NOTIFY_STATE_RELOADING, NULL);
			/* TODO: Call actual config reloading activity */
			reload_flag = 0;
			dstate_setinfo("driver.state", "quiet");
			upsnotify(NOTIFY_STATE_READY, NULL);
		}
	}

	/* if we get here, the exit flag was set by a signal handler */
	/* however, avoid to "pollute" data dump output! */
	if (!dump_data) {
		upslogx(LOG_INFO, "Signal %d: exiting", exit_flag);
		upsnotify(NOTIFY_STATE_STOPPING, "Signal %d: exiting", exit_flag);
	}

	exit(EXIT_SUCCESS);
}
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
