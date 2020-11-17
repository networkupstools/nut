/* main.c - Network UPS Tools driver core

   Copyright (C)
   1999			Russell Kroll <rkroll@exploits.org>
   2005 - 2017	Arnaud Quette <arnaud.quette@free.fr>
   2017 		Eaton (author: Emilien Kia <EmilienKia@Eaton.com>)

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
#include "dstate.h"
#include "attribute.h"

	/* data which may be useful to the drivers */
	int		upsfd = -1;
	char		*device_path = NULL;
	const char	*progname = NULL, *upsname = NULL, *device_name = NULL;

	/* may be set by the driver to wake up while in dstate_poll_fds */
	int	extrafd = -1;

	/* for ser_open */
	int	do_lock_port = 1;

	/* for dstate->sock_connect, default to asynchronous */
	int	do_synchronous = 0;

	/* for detecting -a values that don't match anything */
	static	int	upsname_found = 0;

	static vartab_t	*vartab_h = NULL;

	/* variables possibly set by the global part of ups.conf */
	unsigned int	poll_interval = 2;
	static char	*chroot_path = NULL, *user = NULL;

	/* signal handling */
	int	exit_flag = 0;

	/* everything else */
	static char	*pidfn = NULL;
	int	dump_data = 0; /* Store the update_count requested */

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
	printf("  -D             - raise debugging level\n");
	printf("  -d <count>     - dump data to stdout after 'count' updates loop and exit\n");
	printf("  -q             - raise log level threshold\n");
	printf("  -h             - display this help\n");
	printf("  -k             - force shutdown\n");
	printf("  -i <int>       - poll interval\n");
	printf("  -r <dir>       - chroot to <dir>\n");
	printf("  -u <user>      - switch to <user> (if started as root)\n");
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

/* store these in dstate as driver.(parameter|flag) */
static void dparam_setinfo(const char *var, const char *val)
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
static void storeval(const char *var, char *val)
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

/* callback from driver - create the table for -x/conf entries */
void addvar(int vartype, const char *name, const char *desc)
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
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		vartab_h = tmp;
}

/* handle -x / ups.conf config details that are for this part of the code */
static int main_arg(char *var, char *val)
{
	/* flags for main */

	if (!strcmp(var, "nolock")) {
		do_lock_port = 0;
		dstate_setinfo("driver.flag.nolock", "enabled");
		return 1;	/* handled */
	}

	if (!strcmp(var, "ignorelb")) {
		dstate_setinfo("driver.flag.ignorelb", "enabled");
		return 1;	/* handled */
	}

	/* any other flags are for the driver code */
	if (!val)
		return 0;

	/* variables for main: port */

	if (!strcmp(var, "port")) {
		device_path = xstrdup(val);
		device_name = xbasename(device_path);
		dstate_setinfo("driver.parameter.port", "%s", val);
		return 1;	/* handled */
	}

	if (!strcmp(var, "sddelay")) {
		upslogx(LOG_INFO, "Obsolete value sddelay found in ups.conf");
		return 1;	/* handled */
	}

	/* allow per-driver overrides of the global setting */
	if (!strcmp(var, "synchronous")) {
		if (!strcmp(val, "yes"))
			do_synchronous=1;
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

	return 0;	/* unhandled, pass it through to the driver */
}

static void do_global_args(const char *var, const char *val)
{
	if (!strcmp(var, "pollinterval")) {
		poll_interval = atoi(val);
		return;
	}

	if (!strcmp(var, "chroot")) {
		free(chroot_path);
		chroot_path = xstrdup(val);
	}

	if (!strcmp(var, "user")) {
		free(user);
		user = xstrdup(val);
	}

	if (!strcmp(var, "synchronous")) {
		if (!strcmp(val, "yes"))
			do_synchronous=1;
		else
			do_synchronous=0;
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
		dstate_setinfo(tmp, "enabled");

		storeval(var, NULL);
		return;
	}

	/* don't let the user shoot themselves in the foot */
	if (!strcmp(var, "driver")) {
		if (strcmp(val, progname) != 0)
			fatalx(EXIT_FAILURE, "Error: UPS [%s] is for driver %s, but I'm %s!\n",
				confupsname, val, progname);
		return;
	}

	/* allow per-driver overrides of the global setting */
	if (!strcmp(var, "pollinterval")) {
		poll_interval = atoi(val);
		return;
	}

	/* everything else must be for the driver */
	storeval(var, val);
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

static void vartab_free(void)
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

static void exit_cleanup(void)
{
	free(chroot_path);
	free(device_path);
	free(user);

	if (pidfn) {
		unlink(pidfn);
		free(pidfn);
	}

	dstate_free();
	vartab_free();
}

static void set_exit_flag(int sig)
{
	exit_flag = sig;
}

static void setup_signals(void)
{
	struct sigaction	sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sa.sa_handler = set_exit_flag;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
}

int main(int argc, char **argv)
{
	struct	passwd	*new_uid = NULL;
	int	i, do_forceshutdown = 0;
	int	update_count = 0;

	atexit(exit_cleanup);

	/* pick up a default from configure --with-user */
	user = xstrdup(RUN_AS_USER);	/* xstrdup: this gets freed at exit */

	progname = xbasename(argv[0]);
	open_syslog(progname);

	upsdrv_banner();

	if (upsdrv_info.status == DRV_EXPERIMENTAL) {
		printf("Warning: This is an experimental driver.\n");
		printf("Some features may not function correctly.\n\n");
	}

	/* build the driver's extra (-x) variable table */
	upsdrv_makevartable();

	while ((i = getopt(argc, argv, "+a:s:kDd:hx:Lqr:u:Vi:")) != -1) {
		switch (i) {
			case 'a':
				upsname = optarg;

				read_upsconf();

				if (!upsname_found)
					fatalx(EXIT_FAILURE, "Error: Section %s not found in ups.conf",
						optarg);
				break;
			case 's':
				upsname = optarg;
				upsname_found = 1;
				break;
			case 'D':
				nut_debug_level++;
				break;
			case 'd':
				dump_data = atoi(optarg);
				break;
			case 'i':
				poll_interval = atoi(optarg);
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
				free(user);
				user = xstrdup(optarg);
				break;
			case 'V':
				/* already printed the banner, so exit */
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

	upsdebugx(1, "debug level is '%d'", nut_debug_level);

	new_uid = get_user_pwent(user);

	if (chroot_path)
		chroot_start(chroot_path);

	become_user(new_uid);

	/* Only switch to statepath if we're not powering off or just dumping data, for discovery */
	/* This avoid case where ie /var is umounted */
	if ((!do_forceshutdown) && (!dump_data) && (chdir(dflt_statepath())))
		fatal_with_errno(EXIT_FAILURE, "Can't chdir to %s", dflt_statepath());

	/* Setup signals to communicate with driver once backgrounded. */
	if ((nut_debug_level == 0) && (!do_forceshutdown)) {
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

			if (sendsignalfn(buffer, SIGTERM) != 0) {
				/* Can't send signal to PID, assume invalid file */
				break;
			}

			upslogx(LOG_WARNING, "Duplicate driver instance detected (PID file %s exists)! Terminating other driver!", buffer);

			/* Allow driver some time to quit */
			sleep(5);
		}

		/* Only write pid if we're not just dumping data, for discovery */
		if (!dump_data) {
			pidfn = xstrdup(buffer);
			writepid(pidfn);	/* before backgrounding */
		}
	}

	/* clear out callback handler data */
	memset(&upsh, '\0', sizeof(upsh));

	/* note: device.type is set early to be overridden by the driver
	 * when its a pdu! */
	dstate_setinfo("device.type", "ups");

	upsdrv_initups();

	/* UPS is detected now, cleanup upon exit */
	atexit(upsdrv_cleanup);

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

	/* get the base data established before allowing connections */
	upsdrv_initinfo();
	upsdrv_updateinfo();

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
	if (!dump_data)
		dstate_init(progname, upsname);

	/* The poll_interval may have been changed from the default */
	dstate_setinfo("driver.parameter.pollinterval", "%d", poll_interval);

	/* The synchronous option may have been changed from the default */
	dstate_setinfo("driver.parameter.synchronous", "%s",
		(do_synchronous==1)?"yes":"no");

	/* remap the device.* info from ups.* for the transition period */
	if (dstate_getinfo("ups.mfr") != NULL)
		dstate_setinfo("device.mfr", "%s", dstate_getinfo("ups.mfr"));
	if (dstate_getinfo("ups.model") != NULL)
		dstate_setinfo("device.model", "%s", dstate_getinfo("ups.model"));
	if (dstate_getinfo("ups.serial") != NULL)
		dstate_setinfo("device.serial", "%s", dstate_getinfo("ups.serial"));

	if ( (nut_debug_level == 0) && (!dump_data) ) {
		background();
		writepid(pidfn);	/* PID changes when backgrounding */
	}

	while (!exit_flag) {

		struct timeval	timeout;

		gettimeofday(&timeout, NULL);
		timeout.tv_sec += poll_interval;

		upsdrv_updateinfo();

		/* Dump the data tree (in upsc-like format) to stdout and exit */
		if (dump_data) {
			/* Wait for 'dump_data' update loops to ensure data completion */
			if (update_count == dump_data) {
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
	}

	/* if we get here, the exit flag was set by a signal handler */
	/* however, avoid to "pollute" data dump output! */
	if (!dump_data)
		upslogx(LOG_INFO, "Signal %d: exiting", exit_flag);

	exit(EXIT_SUCCESS);
}
