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
#include "upsdrvquery.h"

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
 * and is briefly 1 when a reload signal is received and is being handled,
 * or 2 if the reload attempt is allowed to exit the current driver (e.g.
 * changed some ups.conf settings that can not be re-applied on the fly)
 * assuming it gets restarted by external framework (systemd) or caller
 * (like NUT driver CLI `-c reload-or-restart` handling), if needed.
 */
static int	reload_flag = 0;

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
/* Should this driver instance go to background (default)
 * or stay foregrounded (default if -D/-d options are set on
 * command line)? Note that debug_min in ups.conf allows for
 * verbosity while backgrounded by default.
 * Value is multi-state (FIXME: enum?):
 *  -1 (default) Decide based on debug verbosity or dump_mode
 *  0 User required to background even if with -D or dump_mode,
 *    or did not require foregrounding/dumping/debug on CLI
 *  1 User required to not background explicitly,
 *    or passed -D (or -d) and current value was -1
 *  2 User required to not background explicitly,
 *    and yet to write the PID file, with -FF option
 */
static int foreground = -1;
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */

/* Users can pass a -D[...] option to enable debugging.
 * For the service tracing purposes, also the ups.conf
 * can define a debug_min value in the global or device
 * section, to set the minimal debug level (CLI provided
 * value less than that would not have effect, can only
 * have more). Finally, it can also be set over socket
 * protocol, taking precedence over other inputs.
 */
static int nut_debug_level_args = -1;
static int nut_debug_level_global = -1;
static int nut_debug_level_driver = -1;
static int nut_debug_level_protocol = -1;

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
/* everything else */
static char	*pidfn = NULL;
static int	dump_data = 0; /* Store the update_count requested */
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */

/* pre-declare some private methods used */
static void assign_debug_level(void);
/* TODO: Equivalent for WIN32 - see SIGCMD_RELOAD in upd and upsmon */
static void set_reload_flag(
#ifndef WIN32
	int
#else
	char *
#endif
	sig);
#ifndef DRIVERS_MAIN_WITHOUT_MAIN
/* Returns a result code from INSTCMD enum values */
static int handle_reload_flag(void);
#endif

/* Set in do_ups_confargs() for consumers like handle_reload_flag() */
static int reload_requires_restart = -1;

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

	/* the driver always exits here, to not block probable ongoing shutdown */
	exit(exit_flag == -1 ? EXIT_FAILURE : EXIT_SUCCESS);
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
	printf("  -FF            - stay foregrounded and still save the PID file\n");
	printf("  -B             - stay backgrounded even if debugging is bumped\n");
	printf("  -q             - raise log level threshold\n");
	printf("  -h             - display this help\n");
	printf("  -k             - force shutdown\n");
	printf("  -c <command>   - send <command> via signal to background process\n");
	printf("                   Supported commands:\n");
# ifndef WIN32
/* FIXME: port event loop from upsd/upsmon to allow messaging fellow drivers in WIN32 builds */
	printf("                   - reload: re-read configuration files, ignoring changed\n");
	printf("                     values which require a driver restart (can not be changed\n");
	printf("                     on the fly)\n");
# endif	/* WIN32 */
/* Note: this one is beside signal-sending (goes via socket protocol): */
	printf("                   - reload-or-error: re-read configuration files, ignoring but\n");
	printf("                     counting changed values which require a driver restart (can\n");
	printf("                     not be changed on the fly), and return a success/fail code\n");
	printf("                     based on that count, so the caller can decide the fate of\n");
	printf("                     the currently running driver instance\n");
# ifndef WIN32
/* FIXME: port event loop from upsd/upsmon to allow messaging fellow drivers in WIN32 builds */
#  ifdef SIGCMD_RELOAD_OR_RESTART
	printf("                   - reload-or-restart: re-read configuration files (close the\n");
	printf("                     old driver instance device connection if needed, and have\n");
	printf("                     it effectively restart)\n");
#  endif
	printf("                   - reload-or-exit: re-read configuration files (exit the old\n");
	printf("                     driver instance if needed, so an external caller like the\n");
	printf("                     systemd or SMF frameworks would start another copy)\n");
	/* NOTE for FIXME above: PID-signalling is non-WIN32-only for us */
	printf("  -P <pid>       - send the signal above to specified PID (bypassing PID file)\n");
# endif	/* WIN32 */
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

	/* NOTE: (FIXME?) The override and default mechanisms here
	 * effectively bypass both VAR_SENSITIVE protections and
	 * the constraint of having previously defined the name by
	 * addvar() in a driver codebase, or of having a dot in it.
	 * See https://github.com/networkupstools/nut/issues/1891
	 * if this would need solving eventually. At the moment the
	 * sensitivity impacts certain auth values for netxml-ups
	 * and snmp-ups reading from vartab directly, and overrides
	 * are ignored - so no practical problem to solve right now.
	 */
	if (!strncasecmp(var, "override.", 9)) {
		/* NOTE: No regard for VAR_SENSITIVE here */
		dstate_setinfo(var+9, "%s", val);
		dstate_setflags(var+9, ST_FLAG_IMMUTABLE);
		dparam_setinfo(var, val);
		return;
	}

	if (!strncasecmp(var, "default.", 8)) {
		/* NOTE: No regard for VAR_SENSITIVE here */
		dstate_setinfo(var+8, "%s", val);
		dparam_setinfo(var, val);
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
			if ((tmp->vartype & VAR_SENSITIVE) == 0) {
				dparam_setinfo(var, val);
			} else {
				upsdebugx(4, "%s: skip dparam_setinfo() "
					"for sensitive variable '%s'",
					__func__, var);
			}

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
	int	verdict = -2;

	/* FIXME: handle VAR_FLAG typed (bitmask) values specially somehow?
	 * Either we set the flag at some point (because its name is mentioned)
	 * or we do not (initially set - no way so far to know it got commented
	 * away before a reload on the fly). Might load new config info into a
	 * separate list and then compare missing points?..
	 */
	upsdebugx(6, "%s: searching for var=%s, vartype=%d, reload_flag=%d",
		__func__, NUT_STRARG(var), vartype, reload_flag);

	while (tmp) {
		if (!strcasecmp(tmp->var, var)) {
			/* variable name is known */
			upsdebugx(6, "%s: found var=%s, val='%s' => '%s', vartype=%d => %d, found=%d, reloadable=%d, reload_flag=%d",
				__func__, NUT_STRARG(var),
				NUT_STRARG(tmp->val), NUT_STRARG(val),
				tmp->vartype, vartype,
				tmp->found, tmp->reloadable, reload_flag);

			if (val && tmp->val) {
				/* a value is already known by name
				 * and bitmask for VAR_FLAG/VAR_VALUE matches
				 */
				if ((vartype & tmp->vartype) && !strcasecmp(tmp->val, val)) {
					if ((tmp->vartype & VAR_FLAG) && val == NULL) {
						if (reload_flag) {
							upsdebugx(1, "%s: setting '%s' "
								"exists and is a flag; "
								"new value was not specified",
								__func__, var);
						}

						/* by default: apply flags initially, ignore later */
						verdict = (
							(!reload_flag)	/* For initial config reads, legacy code trusted what it saw */
							|| tmp->reloadable	/* set in addvar*() */
						);
						goto finish;
					}

					if (reload_flag) {
						upsdebugx(1, "%s: setting '%s' "
							"exists and is unmodified",
							__func__, var);
					}

					verdict = -1;	/* no-op for caller */
					goto finish;
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
					/* FIXME: Define a special EXIT_RELOAD or something,
					 * for "not quite a failure"? Or close connections
					 * and re-exec() this driver from scratch (and so to
					 * keep MAINPID for systemd et al)?
					 */
					if (reload_flag == 2 && !tmp->reloadable)
						fatalx(
#ifndef WIN32
							(128 + SIGCMD_RELOAD_OR_EXIT)
#else
							EXIT_SUCCESS
#endif
							, "NUT driver reload-or-exit: setting %s was changed and requires a driver restart", var);

					verdict = (
						(!reload_flag)	/* For initial config reads, legacy code trusted what it saw */
						|| tmp->reloadable	/* set in addvar*() */
					);

					/* handle reload-or-error reports */
					if (verdict == 0) {
						if (reload_requires_restart < 1)
							reload_requires_restart = 1;
						else
							reload_requires_restart++;
					}

					goto finish;
				}
			}

			/* okay to redefine if not yet defined, or if reload is allowed,
			 * or if initially loading the configs
			 */
			verdict = (
				(!reload_flag)
				|| ((!tmp->found) || tmp->reloadable)
			);
			goto finish;
		}
		tmp = tmp->next;
	}

	verdict = 1;	/* not found, may (re)load the definition */

finish:
	switch (verdict) {
		case -1:	/* no-op for caller, same value remains */
		case  1:	/* value may be (re-)applied */
			if (reload_requires_restart < 0)
				reload_requires_restart = 0;
			break;

		case  0:	/* value may not be (re-)applied, but it may not have been required */
			break;
	}

	upsdebugx(6, "%s: verdict for (re)loading var=%s value: %d",
		__func__, NUT_STRARG(var), verdict);
	return verdict;
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
	int	verdict = -2;

	upsdebugx(6, "%s: var=%s, oldval=%s, newval=%s, reloadable=%d, reload_flag=%d",
		__func__, NUT_STRARG(var), NUT_STRARG(oldval), NUT_STRARG(newval),
		reloadable, reload_flag);

	/* Nothing saved yet? Okay to store new value! */
	if (!oldval) {
		verdict = 1;
		goto finish;
	}

	/* Should not happen? Or... (commented-away etc.) */
	if (!newval) {
		upslogx(LOG_WARNING, "%s: new setting for '%s' is NULL", __func__, var);
		verdict = ((!reload_flag) || reloadable);
		goto finish;
	}

	/* a value is already known, another is desired */
	if (!strcasecmp(oldval, newval)) {
		if (reload_flag) {
			upsdebugx(1, "%s: setting '%s' "
				"exists and is unmodified",
				__func__, var);
		}
		verdict = -1;	/* no-op for caller */
		goto finish;
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
		/* FIXME: Define a special EXIT_RELOAD or something,
		 * for "not quite a failure"? Or close connections
		 * and re-exec() this driver from scratch (and so to
		 * keep MAINPID for systemd et al)?
		 */
		if (reload_flag == 2 && !reloadable)
			fatalx(
#ifndef WIN32
				(128 + SIGCMD_RELOAD_OR_EXIT)
#else
				EXIT_SUCCESS
#endif
				, "NUT driver reload-or-exit: setting %s was changed and requires a driver restart", var);
		/* For initial config reads, legacy code trusted what it saw */
		verdict = ((!reload_flag) || reloadable);

		/* handle reload-or-error reports */
		if (verdict == 0) {
			if (reload_requires_restart < 1)
				reload_requires_restart = 1;
			else
				reload_requires_restart++;
		}

		goto finish;
	}

finish:
	switch (verdict) {
		case -1:	/* no-op for caller, same value remains */
		case  1:	/* value may be (re-)applied */
			if (reload_requires_restart < 0)
				reload_requires_restart = 0;
			break;

		case  0:	/* value may not be (re-)applied, but it may not have been required */
			break;
	}

	upsdebugx(6, "%s: verdict for (re)loading var=%s value: %d",
		__func__, NUT_STRARG(var), verdict);
	return verdict;
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
	int	verdict = -2;

	upsdebugx(6, "%s: var=%s, infoname=%s, newval=%s, reloadable=%d, reload_flag=%d",
		__func__, NUT_STRARG(var), NUT_STRARG(infoname), NUT_STRARG(newval),
		reloadable, reload_flag);

	/* Keep legacy behavior: not reloading, trust the initial config */
	if (!reload_flag || !infoname) {
		verdict = 1;
		goto finish;
	}

	/* Suffer the overhead of lookups only if reloading */

	/* FIXME: handle "driver.flag.*" prefixed values specially somehow?
	 * Either we set the flag at some point (because its name is mentioned)
	 * or we do not (initially set - no way so far to know it got commented
	 * away before a reload on the fly). Might load new config info into a
	 * separate list and then compare missing points?..
	 */
	verdict = testval_reloadable(var, dstate_getinfo(infoname), newval, reloadable);

finish:
	upsdebugx(6, "%s: verdict for (re)loading var=%s value: %d",
		__func__, NUT_STRARG(var), verdict);
	return verdict;
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

/* handle instant commands common for all drivers */
int main_instcmd(const char *cmdname, const char *extra, conn_t *conn) {
	char buf[SMALLBUF];
	if (conn)
#ifndef WIN32
		snprintf(buf, sizeof(buf), "socket %d", conn->fd);
#else
		snprintf(buf, sizeof(buf), "handle %p", conn->fd);
#endif
	else
		snprintf(buf, sizeof(buf), "(null)");

	upsdebugx(2, "entering main_instcmd(%s, %s) for [%s] on %s",
		cmdname, extra, NUT_STRARG(upsname), buf);

	if (!strcmp(cmdname, "driver.killpower")) {
		if (!strcmp("1", dstate_getinfo("driver.flag.allow_killpower"))) {
			upslogx(LOG_WARNING, "Requesting UPS [%s] to power off, "
				"as/if handled by its driver by default (may exit), "
				"due to socket protocol request", NUT_STRARG(upsname));
			upsdrv_shutdown();
			return STAT_INSTCMD_HANDLED;
		} else {
			upslogx(LOG_WARNING, "Got socket protocol request for UPS [%s] "
				"to power off, but driver.flag.allow_killpower does not"
				"permit this - request was currently ignored!",
				NUT_STRARG(upsname));
			return STAT_INSTCMD_INVALID;
		}
	}

#ifndef WIN32
/* TODO: Equivalent for WIN32 - see SIGCMD_RELOAD in upd and upsmon */
	if (!strcmp(cmdname, "driver.reload")) {
		set_reload_flag(SIGCMD_RELOAD);
		/* TODO: sync mode to track that reload finished, and how?
		 * Especially to know if there were values we can not change
		 * on the fly, so caller may want to restart the driver itself.
		 */
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcmp(cmdname, "driver.reload-or-exit")) {
		set_reload_flag(SIGCMD_RELOAD_OR_EXIT);
		return STAT_INSTCMD_HANDLED;
	}

# ifdef SIGCMD_RELOAD_OR_RESTART
	if (!strcmp(cmdname, "driver.reload-or-restart")) {
		set_reload_flag(SIGCMD_RELOAD_OR_RESTART);
		return STAT_INSTCMD_HANDLED;
	}
# endif
#endif  /* WIN32 */

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
	if (!strcmp(cmdname, "driver.reload-or-error")) {
		/* sync-capable handling */
		set_reload_flag(SIGCMD_RELOAD_OR_ERROR);
		/* Returns a result code from INSTCMD enum values */
		return handle_reload_flag();
	}
#endif

	/* By default, the driver-specific values are
	 * unknown to shared standard handler */
	upsdebugx(2, "shared %s() does not handle command %s, "
		"proceeding to driver-specific handler",
		__func__, cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

/* handle setting variables common for all drivers */
int main_setvar(const char *varname, const char *val, conn_t *conn) {
	char buf[SMALLBUF];
	if (conn)
#ifndef WIN32
		snprintf(buf, sizeof(buf), "socket %d", conn->fd);
#else
		snprintf(buf, sizeof(buf), "handle %p", conn->fd);
#endif
	else
		snprintf(buf, sizeof(buf), "(null)");

	upsdebugx(2, "entering main_setvar(%s, %s) for [%s] on %s",
		varname, val, NUT_STRARG(upsname), buf);

	if (!strcmp(varname, "driver.debug")) {
		int num;
		if (str_to_int(val, &num, 10)) {
			if (num < 0) {
				upsdebugx(nut_debug_level > 0 ? 1 : 0,
					"NOTE: Will fall back to CLI/DriverConfig/GlobalConfig debug verbosity preference now");
				num = -1;
			}
			if (nut_debug_level > 0 && num == 0)
				upsdebugx(1, "NOTE: Will disable verbose debug now, due to socket protocol request");
			nut_debug_level_protocol = num;
			assign_debug_level();
			return STAT_SET_HANDLED;
		} else {
			goto invalid;
		}
	}

	if (!strcmp(varname, "driver.flag.allow_killpower")) {
		int num = 0;
		if (str_to_int(val, &num, 10)) {
			if (num <= 0) {
				num = 0;
			} else	num = 1;
		} else {
			/* support certain strings */
			if (!strncmp(val, "enable", 6)	/* "enabled" matches too */
			 || !strcmp(val, "true")
			 || !strcmp(val, "yes")
			 || !strcmp(val, "on")
			) num = 1;
		}

		upsdebugx(1, "%s: Setting %s=%d", __func__, varname, num);
		dstate_setinfo("driver.flag.allow_killpower", "%d", num);
		return STAT_SET_HANDLED;
	}

	/* By default, the driver-specific values are
	 * unknown to shared standard handler */
	upsdebugx(2, "shared %s() does not handle variable %s, "
		"proceeding to driver-specific handler",
		__func__, varname);
	return STAT_SET_UNKNOWN;

invalid:
	upsdebugx(1, "Error: UPS [%s]: invalid %s value: %s",
		NUT_STRARG(upsname), varname, val);
	return STAT_SET_INVALID;
}

/* handle -x / ups.conf config details that are for this part of the code */
static int main_arg(char *var, char *val)
{
	int do_handle = -2;

	/* flags for main */

	upsdebugx(3, "%s: var='%s' val='%s'",
		__func__,
		var ? var : "<null>", /* null should not happen... but... */
		val ? val : "<null>");

	/* !reload_flag simply forbids changing this flag on the fly, as
	 * it would have no effect anyway without a (serial) reconnection
	 */
	if (!strcmp(var, "nolock")) {
		if (reload_flag) {
			upsdebugx(6, "%s: SKIP: flag var='%s' can not be reloaded", __func__, var);
		} else {
			do_lock_port = 0;
			dstate_setinfo("driver.flag.nolock", "enabled");
		}
		return 1;	/* handled */
	}

	/* FIXME: this one we could potentially reload, but need to figure
	 * out that the flag line was commented away or deleted -- there is
	 * no setting value to flip in configs here
	 */
	if (!strcmp(var, "ignorelb")) {
		if (reload_flag) {
			upsdebugx(6, "%s: SKIP: flag var='%s' currently can not be reloaded", __func__, var);
		} else {
			dstate_setinfo("driver.flag.ignorelb", "enabled");
		}
		return 1;	/* handled */
	}

	if (!strcmp(var, "allow_killpower")) {
		if (reload_flag) {
			upsdebugx(6, "%s: SKIP: flag var='%s' currently can not be reloaded "
				"(but may be changed by protocol SETVAR)", __func__, var);
		} else {
			dstate_setinfo("driver.flag.allow_killpower", "1");
		}
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

	if (!strcmp(var, "port")) {
		if (testinfo_reloadable(var, "driver.parameter.port", val, 0) > 0) {
			device_path = xstrdup(val);
			device_name = xbasename(device_path);
			dstate_setinfo("driver.parameter.port", "%s", val);
		}
		return 1;	/* handled */
	}

	/* user specified at the driver level overrides that on global level
	 * or the built-in default
	 */
	if (!strcmp(var, "user")) {
		if (testval_reloadable(var, user, val, 0) > 0) {
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
		}
		return 1;	/* handled */
	}

	if (!strcmp(var, "group")) {
		if (testval_reloadable(var, group, val, 0) > 0) {
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
	if (!strcmp(var, "synchronous")) {
		if (testval_reloadable(var, ((do_synchronous==1)?"yes":((do_synchronous==0)?"no":"auto")), val, 1) > 0) {
			if (!strcmp(val, "yes"))
				do_synchronous=1;
			else
			if (!strcmp(val, "auto"))
				do_synchronous=-1;
			else
				do_synchronous=0;
		}

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
	 * less without changing config. Should help debug of services.
	 * Note: during reload_flag!=0 handling this is reset to -1, to
	 * catch commented-away settings, so not checking previous value.
	 */
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
	int do_handle = 1;

	upsdebugx(3, "%s: var='%s' val='%s'",
		__func__,
		var ? var : "<null>", /* null should not happen... but... */
		val ? val : "<null>");

	/* Allow to reload this, why not */
	if (!strcmp(var, "pollinterval")) {
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

	if (!strcmp(var, "chroot")) {
		if (testval_reloadable(var, chroot_path, val, 0) > 0) {
			free(chroot_path);
			chroot_path = xstrdup(val);
		}

		return;
	}

	if (!strcmp(var, "user")) {
		if (testval_reloadable(var, user, val, 0) > 0) {
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

		return;
	}

	if (!strcmp(var, "group")) {
		if (testval_reloadable(var, group, val, 0) > 0) {
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

		return;
	}

	/* Allow to reload this, why not
	 * Note: this may cause "spurious" redefinitions of the
	 * "no" setting which is the fallback for random values.
	 * Also note that global+driver section definitions may
	 * cause noise, but it allows either to be commented away
	 * and the other to take hold. Both disappearing would not
	 * be noticed by the reload operation currently, however.
	 */
	if (!strcmp(var, "synchronous")) {
		if (testval_reloadable(var, ((do_synchronous==1)?"yes":((do_synchronous==0)?"no":"auto")), val, 1) > 0) {
			if (!strcmp(val, "yes"))
				do_synchronous=1;
			else
			if (!strcmp(val, "auto"))
				do_synchronous=-1;
			else
				do_synchronous=0;
		}

		return;
	}

	/* Allow to specify its minimal debugging level for all drivers -
	 * admins can set more with command-line args, but can't set
	 * less without changing config. Should help debug of services.
	 * Note: during reload_flag!=0 handling this is reset to -1, to
	 * catch commented-away settings, so not checking previous value.
	 */
	if (!strcmp(var, "debug_min")) {
		int lvl = -1; // typeof common/common.c: int nut_debug_level
		if ( str_to_int (val, &lvl, 10) && lvl >= 0 ) {
			nut_debug_level_global = lvl;
		} else {
			upslogx(LOG_INFO, "WARNING : Invalid debug_min value found in ups.conf global settings");
		}

		return;
	}

	/* unrecognized */
}

void do_upsconf_args(char *confupsname, char *var, char *val)
{
	char	tmp[SMALLBUF];

	upsdebugx(5, "%s: confupsname=%s, var=%s, val=%s",
		__func__, NUT_STRARG(confupsname), NUT_STRARG(var), NUT_STRARG(val));

	/* handle global declarations */
	if (!confupsname) {
		upsdebugx(5, "%s: call do_global_args()", __func__);
		do_global_args(var, val);
		return;
	}

	/* no match = not for us */
	if (strcmp(confupsname, upsname) != 0)
		return;

	upsname_found = 1;

	upsdebugx(5, "%s: call main_arg()", __func__);
	if (main_arg(var, val))
		return;
	upsdebugx(5, "%s: not a main_arg()", __func__);

	/* flags (no =) now get passed to the driver-level stuff */
	if (!val) {
		upsdebugx(5, "%s: process as flag", __func__);

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
	if (!strcmp(var, "driver")) {
		int do_handle;

		upsdebugx(5, "%s: this is a 'driver' setting, may we proceed?", __func__);
		do_handle = testval_reloadable(var, progname, val, 0);

		if (do_handle == -1) {
			upsdebugx(5, "%s: 'driver' setting already applied with this value", __func__);
			return;
		}

		/* Acceptable progname is only set once during start-up
		 * val is from ups.conf
		 */
		if (!reload_flag || do_handle > 0) {
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
	upsdebugx(5, "%s: process as value", __func__);
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

	if (nut_debug_level_protocol >= 0) {
		upslogx(LOG_INFO,
			"Applying debug level %d received during run-time "
			"via socket protocol, ignoring other settings",
			nut_debug_level_protocol);
		nut_debug_level = nut_debug_level_protocol;
		goto finish;
	}

	if (nut_debug_level_global >= 0 && nut_debug_level_driver >= 0) {
		/* Use nearest-defined fit */
		nut_debug_level_upsconf = nut_debug_level_driver;
		if (reload_flag) {
			upslogx(LOG_INFO,
				"Applying debug_min=%d from ups.conf"
				" driver section (overriding global %d)",
				nut_debug_level_upsconf, nut_debug_level_global);
		}
	} else {
		if (nut_debug_level_global >= 0) {
			nut_debug_level_upsconf = nut_debug_level_global;
			if (reload_flag) upslogx(LOG_INFO,
				"Applying debug_min=%d from ups.conf"
				" global section",
				nut_debug_level_upsconf);
		}
		if (nut_debug_level_driver >= 0) {
			nut_debug_level_upsconf = nut_debug_level_driver;
			if (reload_flag) upslogx(LOG_INFO,
				"Applying debug_min=%d from ups.conf"
				" driver section",
				nut_debug_level_upsconf);
		}
	}

	if (reload_flag && nut_debug_level_upsconf <= nut_debug_level_args) {
		/* DEBUG_MIN is absent or commented-away in ups.conf,
		 * or is smaller than te CLI arg '-D' count */
		upslogx(LOG_INFO,
			"Applying debug level %d from "
			"original command line arguments",
			nut_debug_level_args);
	}

	/* at minimum, the verbosity we started with - via CLI arguments;
	 * maybe a greater debug_min is set in current config file
	 */
	nut_debug_level = nut_debug_level_args;
	if (nut_debug_level_upsconf > nut_debug_level)
		nut_debug_level = nut_debug_level_upsconf;

finish:
	upsdebugx(1, "debug level is '%d'", nut_debug_level);
	dstate_setinfo("driver.debug", "%d", nut_debug_level);
	dstate_setflags("driver.debug", ST_FLAG_RW | ST_FLAG_NUMBER);
}

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
/* Returns a result code from INSTCMD enum values */
static int handle_reload_flag(void) {
	int ret;

	if (!reload_flag || exit_flag)
		return STAT_INSTCMD_INVALID;

	upslogx(LOG_INFO, "Handling requested live reload of NUT driver configuration for [%s]", upsname);
	dstate_setinfo("driver.state", "reloading");
	upsnotify(NOTIFY_STATE_RELOADING, NULL);

	/* If commented away or deleted in config, debug_min
	 * should "disappear" for us (a CLI argument, if any,
	 * would still be honoured); if it is (re-)defined in
	 * config, then it gets considered.
	 */
	nut_debug_level_global = -1;
	nut_debug_level_driver = -1;

	/* Call actual config reloading activity, which
	 * eventually calls back do_upsconf_args() from
	 * this program.
	 */
	reload_requires_restart = -1;
	/* 0 - Do not abort drivers started with '-s TMP_UPS_NAME' */
	if (read_upsconf(0) < 0) {
		upsdebugx(1, "%s: read_upsconf() failed fundamentally; "
			"is this driver running via ups.conf at all?",
			__func__);
	}

	upsdebugx(1, "%s: read_upsconf() for [%s] completed, restart-required verdict was: %d",
		__func__, upsname, reload_requires_restart);

	/* handle reload-or-error reports */
	if (reload_requires_restart < 1) {
		/* -1 unchanged, 0 nobody complained and everyone confirmed */
		ret = STAT_INSTCMD_HANDLED;
	} else {
		/* 1+ entries required a restart */
		ret = STAT_INSTCMD_INVALID;
	}

	/* TODO: Callbacks in drivers to re-parse configs?
	 * Currently this reloadability relies on either
	 * explicit reload_flag aware code called from the
	 * read_upsconf() method, or on drivers continuously
	 * reading dstate_getinfo() and not caching once
	 * their C variables.
	 */

	/* Re-mix currently known debug verbosity desires */
	assign_debug_level();

	/* Wrap it up */
	reload_flag = 0;
	dstate_setinfo("driver.state", "quiet");
	upsnotify(NOTIFY_STATE_READY, NULL);
	upslogx(LOG_INFO, "Completed requested live reload of NUT driver configuration for [%s]: %d", upsname, ret);

	return ret;
}

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
	switch (exit_flag) {
		case -2:
			upsdebugx(1, "%s: raising exit flag due to programmatic abort: EXIT_SUCCESS", __func__);
			break;
		case -1:
			upsdebugx(1, "%s: raising exit flag due to programmatic abort: EXIT_FAILURE", __func__);
			break;
		default:
			upsdebugx(1, "%s: raising exit flag due to signal %d", __func__, sig);
	}
	exit_flag = sig;
}

static void set_reload_flag(
#ifndef WIN32
	int
#else
	char *
#endif
	sig)
{
#ifndef WIN32
/* TODO: Equivalent for WIN32 - see SIGCMD_RELOAD in upd and upsmon */
	switch (sig) {
		case SIGCMD_RELOAD_OR_EXIT:	/* SIGUSR1 */
			/* reload-or-exit (this driver instance may die) */
			reload_flag = 2;
			break;

#ifdef SIGCMD_RELOAD_OR_RESTART
		case SIGCMD_RELOAD_OR_RESTART:	/* SIGUSR2 */
			/* reload-or-restart (this driver instance may recycle itself) */
			/* FIXME: Not implemented yet */
			reload_flag = 3;
			break;
#endif

		case SIGCMD_RELOAD:	/* SIGHUP */
		case SIGCMD_RELOAD_OR_ERROR:	/* Not even a signal, but a socket protocol action */
		default:
			/* reload what we can, log what needs a restart so skipped */
			reload_flag = 1;
	}

	upsdebugx(1, "%s: raising reload flag due to signal %d (%s) => reload_flag=%d",
		__func__, sig, strsignal(sig), reload_flag);
#else
	if (sig && !strcmp(sig, SIGCMD_RELOAD_OR_ERROR)) {
		/* reload what we can, log what needs a restart so skipped */
		reload_flag = 1;
	} else {
		/* non-fatal reload as a fallback */
		reload_flag = 1;
        }

	upsdebugx(1, "%s: raising reload flag due to command %s => reload_flag=%d",
		__func__, sig, reload_flag);
#endif  /* WIN32 */
}

#ifndef WIN32
/* TODO: Equivalent for WIN32 - see SIGCMD_RELOAD in upd and upsmon */
static void handle_dstate_dump(int sig) {
	/* no set_dump_flag() here, make it instant */
	upsdebugx(1, "%s: starting driver state dump for [%s] due to signal %d",
		__func__, upsname, sig);
	/* FIXME: upslogx() instead of printf() when backgrounded, if STDOUT got closed? */
	dstate_dump();
	upsdebugx(1, "%s: finished driver state dump for [%s]",
		__func__, upsname);
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
	sigaction(SIGCMD_RELOAD, &sa, NULL);	/* SIGHUP */
	sigaction(SIGCMD_RELOAD_OR_EXIT, &sa, NULL);	/* SIGUSR1 */
# ifdef SIGCMD_RELOAD_OR_RESTART
/* FIXME: Want SIGCMD_RELOAD_OR_RESTART implemented */
	sigaction(SIGCMD_RELOAD_OR_RESTART, &sa, NULL);	/* SIGUSR2 */
# endif

# ifdef SIGCMD_DATA_DUMP
	/* handle run-time data dump (may be limited to non-backgrounding lifetimes) */
	sa.sa_handler = handle_dstate_dump;
	sigaction(SIGCMD_DATA_DUMP, &sa, NULL);	/* SIGURG or SIGWINCH something else on obscure systems */
# endif
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

#ifndef WIN32
	int	cmd = 0;
	pid_t	oldpid = -1;
#else
/* FIXME: *actually* handle WIN32 builds too */
	const char * cmd = NULL;
#endif

	/* init verbosity from default in common.c (0 probably) */
	nut_debug_level_args = nut_debug_level;

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

	while ((i = getopt(argc, argv, "+a:s:kFBDd:hx:Lqr:u:g:Vi:c:"
#ifndef WIN32
		"P:"
#endif
	)) != -1) {
		switch (i) {
			case 'a':
				if (upsname)
					fatalx(EXIT_FAILURE, "Error: options '-a id' and '-s id' "
						"are mutually exclusive and single-use only.");

				upsname = optarg;

				read_upsconf(1);

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
				if (foreground > 0) {
					/* specified twice to save PID file anyway */
					foreground = 2;
				} else {
					foreground = 1;
				}
				break;
			case 'B':
				foreground = 0;
				break;
			case 'D':
				/* bump right here, may impact reporting of other CLI args */
				nut_debug_level++;
				nut_debug_level_args++;
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
/* FIXME: port event loop from upsd/upsmon to allow messaging fellow drivers in WIN32 builds */
			case 'c':
				if (cmd) {
					help_msg();
					fatalx(EXIT_FAILURE,
						"Error: only one command per run can be "
						"sent with option -%c. Try -h for help.", i);
				}

				if (!strncmp(optarg, "reload-or-error", strlen(optarg))) {
					cmd = SIGCMD_RELOAD_OR_ERROR;
				}
#ifndef WIN32
				else
				if (!strncmp(optarg, "reload", strlen(optarg))) {
					cmd = SIGCMD_RELOAD;
				} else
# ifdef SIGCMD_RELOAD_OR_RESTART
				if (!strncmp(optarg, "reload-or-restart", strlen(optarg))) {
					cmd = SIGCMD_RELOAD_OR_RESTART;
				} else
# endif
				if (!strncmp(optarg, "reload-or-exit", strlen(optarg))) {
					cmd = SIGCMD_RELOAD_OR_EXIT;
				}
#endif	/* WIN32 */

				/* bad command given */
				if (!cmd) {
					help_msg();
					fatalx(EXIT_FAILURE,
						"Error: unknown argument to option -%c. Try -h for help.", i);
				}
#ifndef WIN32
				upsdebugx(1, "Will send signal %d (%s) for command '%s' "
					"to already-running driver %s-%s (if any) and exit",
					cmd, strsignal(cmd), optarg, progname, upsname);
#else
				upsdebugx(1, "Will send request '%s' for command '%s' "
					"to already-running driver %s-%s (if any) and exit",
					cmd, optarg, progname, upsname);
#endif	/* WIN32 */
				break;

#ifndef WIN32
			/* NOTE for FIXME above: PID-signalling is non-WIN32-only for us */
			case 'P':
				if ((oldpid = parsepid(optarg)) < 0)
					help_msg();
				break;
#endif	/* WIN32 */
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

	if (foreground < 0) {
		/* Guess a default */
		/* Note: only care about CLI-requested debug verbosity here */
		if (nut_debug_level > 0 || dump_data) {
			/* Only flop from default - stay foreground with debug on */
			foreground = 1;
		} else {
			/* Legacy default - stay background and quiet */
			foreground = 0;
		}
	} else {
		/* Follow explicit user -F/-B request */
		upsdebugx (0,
			"Debug level is %d, dump data count is %s, "
			"but backgrounding mode requested as %s",
			nut_debug_level,
			dump_data ? "on" : "off",
			foreground ? "off" : "on"
			);
	}

	{ /* scoping */
		char *s = getenv("NUT_DEBUG_LEVEL");
		int l;
		if (s && str_to_int(s, &l, 10)) {
			if (l > 0 && nut_debug_level_args < 1) {
				upslogx(LOG_INFO, "Defaulting debug verbosity to NUT_DEBUG_LEVEL=%d "
					"since none was requested by command-line options", l);
				nut_debug_level = l;
				nut_debug_level_args = l;
			}	/* else follow -D settings */
		}	/* else nothing to bother about */
	}

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

	/* we need to get the port from somewhere, unless we are just sending a signal and exiting */
	if (!device_path && !cmd) {
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
	if ((!do_forceshutdown) && (!dump_data)) {
		if (chdir(dflt_statepath()))
			fatal_with_errno(EXIT_FAILURE, "Can't chdir to %s", dflt_statepath());

		/* Setup signals to communicate with driver which is destined for a long run. */
		setup_signals();
	}
#endif  /* WIN32 */

	if (do_forceshutdown) {
		/* First try to handle this over socket protocol
		 * with the running older driver instance (if any);
		 * if this does not succeed, fall through to legacy
		 * approach (kill sibling if needed, recapture device,
		 * command it...)
		 */
		ssize_t	cmdret = -1;
		struct timeval	tv;

		/* Post the query and wait for reply */
		/* FIXME: coordinate with pollfreq? */
		tv.tv_sec = 15;
		tv.tv_usec = 0;
		cmdret = upsdrvquery_oneshot(progname, upsname,
			"SET driver.flag.allow_killpower 1\n",
			NULL, 0, &tv);

		if (cmdret >= 0) {
			/* FIXME: somehow mark drivers expected to loop infinitely? */
			tv.tv_sec = -1;
			tv.tv_usec = -1;
			cmdret = upsdrvquery_oneshot(progname, upsname,
				"INSTCMD driver.killpower\n",
				NULL, 0, &tv);

			if (cmdret < 0) {
				upsdebugx(1, "Socket dialog with the other driver instance: %s", strerror(errno));
			} else {
				upslogx(LOG_INFO, "Request to killpower via running driver returned code %" PRIiSIZE, cmdret);
				if (cmdret == 0)
					/* Note: many drivers would abort with
					 * "shutdown not supported" at this
					 * point... we would too, but later
					 * and at a higher time/processing cost.
					 */
					exit (EXIT_SUCCESS);
				/* else fall through to legacy handling */
			}
		} else {
			upsdebugx(1, "Socket dialog with the other driver instance: %s",
				strerror(errno));
		}
	}

	/* Handle reload-or-error over socket protocol with
	 * the running older driver instance */
#ifndef WIN32
	if (cmd == SIGCMD_RELOAD_OR_ERROR)
#else
	if (cmd && !strcmp(cmd, SIGCMD_RELOAD_OR_ERROR))
#endif  /* WIN32 */
	{	/* Not a signal, but a socket protocol action */
		ssize_t	cmdret = -1;
		char	buf[LARGEBUF];
		struct timeval	tv;

		/* Post the query and wait for reply */
		/* FIXME: coordinate with pollfreq? */
		tv.tv_sec = 15;
		tv.tv_usec = 0;
		cmdret = upsdrvquery_oneshot(progname, upsname,
			"INSTCMD driver.reload-or-error\n",
			buf, sizeof(buf), &tv);

		if (cmdret < 0) {
			upslog_with_errno(LOG_ERR, "Socket dialog with the other driver instance");
		} else {
			/* TODO: handle buf reply contents */
			upslogx(LOG_INFO, "Request to reload-or-error returned code %" PRIiSIZE, cmdret);
		}

		/* exit((cmdret == 0) ? EXIT_SUCCESS : EXIT_FAILURE); */
		exit(((cmdret < 0) || (((uintmax_t)cmdret) > ((uintmax_t)INT_MAX))) ? 255 : (int)cmdret);
	}

#ifndef WIN32
	/* Setup PID file to receive signals to communicate with this driver
	 * instance once backgrounded, and to stop a competing older instance.
	 * Or to send it a signal deliberately.
	 */
	if (cmd || ((foreground == 0) && (!do_forceshutdown))) {
		char	pidfnbuf[SMALLBUF];

		snprintf(pidfnbuf, sizeof(pidfnbuf), "%s/%s-%s.pid", altpidpath(), progname, upsname);

		if (cmd) {	/* Signals */
			int cmdret = -1;
			/* Send a signal to older copy of the driver, if any */
			if (oldpid < 0) {
				cmdret = sendsignalfn(pidfnbuf, cmd);
			} else {
				cmdret = sendsignalpid(oldpid, cmd);
			}

			switch (cmdret) {
			case 0:
				upsdebugx(1, "Signaled old daemon OK");
				break;

			case -3:
			case -2:
				/* if starting new daemon, no competition running -
				 *    maybe OK (or failed to detect it => problem)
				 * if signaling old daemon - certainly have a problem
				 */
				upslogx(LOG_WARNING, "Could not %s PID file '%s' "
					"to see if previous driver instance is "
					"already running!",
					(cmdret == -3 ? "find" : "parse"),
					pidfnbuf);
				break;

			case -1:
			case 1: /* WIN32 */
			default:
				/* if cmd was nontrivial - speak up below, else be quiet */
				upsdebugx(1, "Just failed to send signal, no daemon was running");
				break;
			}

		/* We were signalling a daemon, successfully or not - exit now...
		 * Modulo the possibility of a "reload-or-something" where we
		 * effectively terminate the old driver and start a new one due
		 * to configuration changes that were not reloadable. Such mode
		 * is not implemented currently.
		 */
		if (cmdret != 0) {
			/* sendsignal*() above might have logged more details
			 * for troubleshooting, e.g. about lack of PID file
			 */
			upslogx(LOG_NOTICE, "Failed to signal the currently running daemon (if any)");
# ifdef HAVE_SYSTEMD
			switch (cmd) {
				case SIGCMD_RELOAD:
					upslogx(LOG_NOTICE, "Try something like "
						"'systemctl reload nut-driver@%s.service'%s",
						upsname,
						(oldpid < 0 ? " or add '-P $PID' argument" : ""));
					break;

				case SIGCMD_RELOAD_OR_EXIT:
#  ifdef SIGCMD_RELOAD_OR_RESTART
				case SIGCMD_RELOAD_OR_RESTART:
#  endif
					upslogx(LOG_NOTICE, "Try something like "
						"'systemctl reload-or-restart "
						"nut-driver@%s.service'%s",
						upsname,
						(oldpid < 0 ? " or add '-P $PID' argument" : ""));
					break;

				default:
					upslogx(LOG_NOTICE, "Try something like "
						"'systemctl <command> nut-driver@%s.service'%s",
						upsname,
						(oldpid < 0 ? " or add '-P $PID' argument" : ""));
					break;
				}
				/* ... or edit nut-server.service locally to start `upsd -FF`
				 * and so save the PID file for ability to manage the daemon
				 * beside the service framework, possibly confusing things...
				 */
# else	/* not HAVE_SYSTEMD */
				if (oldpid < 0) {
					upslogx(LOG_NOTICE, "Try to add '-P $PID' argument");
				}
# endif	/* HAVE_SYSTEMD */
			}

			exit((cmdret == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
		}

		/* Try to prevent that driver is started multiple times. If a PID file */
		/* already exists, send a TERM signal to the process and try if it goes */
		/* away. If not, retry a couple of times. */
		for (i = 0; i < 3; i++) {
			struct stat	st;

			if (stat(pidfnbuf, &st) != 0) {
				/* PID file not found */
				break;
			}

			upslogx(LOG_WARNING, "Duplicate driver instance detected (PID file %s exists)! Terminating other driver!", pidfnbuf);

			if (sendsignalfn(pidfnbuf, SIGTERM) != 0) {
				/* Can't send signal to PID, assume invalid file */
				break;
			}

			/* Allow driver some time to quit */
			sleep(5);
		}

		if (i > 0) {
			struct stat	st;
			if (stat(pidfnbuf, &st) == 0) {
				upslogx(LOG_WARNING, "Duplicate driver instance is still alive (PID file %s exists) after several termination attempts! Killing other driver!", pidfnbuf);
				if (sendsignalfn(pidfnbuf, SIGKILL) == 0) {
					sleep(5);
					if (sendsignalfn(pidfnbuf, 0) == 0) {
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
			pidfn = xstrdup(pidfnbuf);
			writepid(pidfn);	/* before backgrounding */
		}
	}
#else	/* WIN32 */
	char	name[SMALLBUF];

	snprintf(name,sizeof(name), "%s-%s",progname,upsname);

	if (cmd) {
/* FIXME: port event loop from upsd/upsmon to allow messaging fellow drivers in WIN32 builds */
/* Should not really get here since cmd would remain 0 until WIN32 support is implemented */
		fatalx(EXIT_FAILURE, "Signal support not implemented for this platform");
	}

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
#endif	/* WIN32 */

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
			/* Use file descriptor, not name, to first check and then manipulate permissions:
			 *   https://cwe.mitre.org/data/definitions/367.html
			 *   https://wiki.sei.cmu.edu/confluence/display/c/FIO01-C.+Be+careful+using+functions+that+use+file+names+for+identification
			 */
			TYPE_FD fd = ERROR_FD;

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
				goto sockname_ownership_finished;
			} else {
				struct stat statbuf;
				mode_t mode;

				if (INVALID_FD((fd = open(sockname, O_RDWR | O_APPEND)))) {
					upsdebugx(1, "WARNING: opening socket file for stat/chown failed: %s",
						strerror(errno)
					);
					allOk = 0;
					/* Can not proceed with ops below */
					goto sockname_ownership_finished;
				}

				if (fstat(fd, &statbuf)) {
					upsdebugx(1, "WARNING: stat for chown failed: %s",
						strerror(errno)
					);
					allOk = 0;
				} else {
					/* Here we do a portable chgrp() essentially: */
					if (fchown(fd, statbuf.st_uid, grp->gr_gid)) {
						upsdebugx(1, "WARNING: chown failed: %s",
							strerror(errno)
						);
						allOk = 0;
					}
				}

				/* Refresh file info */
				if (fstat(fd, &statbuf)) {
					/* Logically we'd fail chown above if file
					 * does not exist or is not accessible */
					upsdebugx(1, "WARNING: stat for chmod failed: %s",
						strerror(errno)
					);
					allOk = 0;
				} else {
					/* chmod g+rw sockname */
					mode = statbuf.st_mode;
					mode |= S_IWGRP;
					mode |= S_IRGRP;
					if (fchmod(fd, mode)) {
						upsdebugx(1, "WARNING: chmod failed: %s",
							strerror(errno)
						);
						allOk = 0;
					}
				}
			}

sockname_ownership_finished:
			if (VALID_FD(fd)) {
				close(fd);
				fd = ERROR_FD;
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

	switch (foreground) {
		case 0:
			background();
			/* We had saved a PID before backgrounding, but
			 * it changes when backgrounding - so save again
			 */
			writepid(pidfn);
			break;

		/* >0: Keep the initial PID; don't care about "!dump_data" here
		 * currently: let users figure out their mess (or neat hacks)
		 */
		case 2:
			if (!pidfn) {
				char	pidfnbuf[SMALLBUF];
				snprintf(pidfnbuf, sizeof(pidfnbuf), "%s/%s-%s.pid", altpidpath(), progname, upsname);
				pidfn = xstrdup(pidfnbuf);
			}
			upslogx(LOG_WARNING, "Running as foreground process, but saving a PID file anyway");
			writepid(pidfn);
			break;

		default:
			upslogx(LOG_WARNING, "Running as foreground process, not saving a PID file");
	}

	dstate_setinfo("driver.flag.allow_killpower", "0");
	dstate_setflags("driver.flag.allow_killpower", ST_FLAG_RW | ST_FLAG_NUMBER);
	dstate_addcmd("driver.killpower");

#ifndef WIN32
/* TODO: Equivalent for WIN32 - see SIGCMD_RELOAD in upd and upsmon */
	dstate_addcmd("driver.reload");
	dstate_addcmd("driver.reload-or-exit");
# ifndef DRIVERS_MAIN_WITHOUT_MAIN
	dstate_addcmd("driver.reload-or-error");
# endif
# ifdef SIGCMD_RELOAD_OR_RESTART
	dstate_addcmd("driver.reload-or-restart");
# endif
#endif

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
				handle_reload_flag();
			}
		}

		handle_reload_flag();
	}

	/* if we get here, the exit flag was set by a signal handler */
	/* however, avoid to "pollute" data dump output! */
	if (!dump_data) {
		upslogx(LOG_INFO, "Signal %d: exiting", exit_flag);
		upsnotify(NOTIFY_STATE_STOPPING, "Signal %d: exiting", exit_flag);
	}

	exit(exit_flag == -1 ? EXIT_FAILURE : EXIT_SUCCESS);
}
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
