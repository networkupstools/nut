/* upsdrvctl.c - UPS driver controller

   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>

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

#include "config.h"  /* must be the first header */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#ifndef WIN32
#include <sys/wait.h>
#endif

#include "proto.h"
#include "common.h"
#include "upsconf.h"
#include "attribute.h"
#include "nut_stdint.h"

typedef struct {
	char	*upsname;
	char	*driver;
	char	*port;
	int	sdorder;
	int	maxstartdelay;
	void	*next;
}	ups_t;

static ups_t	*upstable = NULL;

static int	maxsdorder = 0, testmode = 0, exec_error = 0;

	/* Should we wait for driver (1) or "parallelize" drivers start (0) */
static int	waitfordrivers = 1;

	/* timer - keeps us from getting stuck if a driver hangs */
static int	maxstartdelay = 45;

	/* counter - retry that many time(s) to start the driver if it fails to */
static int	maxretry = 1;

	/* timer - delay between each restart attempt of the driver(s) */
static int	retrydelay = 5;

	/* Directory where driver executables live */
static char	*driverpath = NULL;

	/* passthrough to the drivers: chroot path and new user name */
static char	*pt_root = NULL, *pt_user = NULL;

	/* flag to pass nut_debug_level to launched drivers (as their -D... args) */
static int	nut_debug_level_passthrough = 0;

void do_upsconf_args(char *upsname, char *var, char *val)
{
	ups_t	*tmp, *last;

	/* handle global declarations */
	if (!upsname) {
		if (!strcmp(var, "maxstartdelay"))
			maxstartdelay = atoi(val);

		if (!strcmp(var, "driverpath")) {
			free(driverpath);
			driverpath = xstrdup(val);
		}

		if (!strcmp(var, "maxretry"))
			maxretry = atoi(val);

		if (!strcmp(var, "retrydelay"))
			retrydelay = atoi(val);

		if (!strcmp(var, "nowait"))
			waitfordrivers = 0;

		/* ignore anything else - it's probably for main */

		return;
	}

	last = tmp = upstable;

	while (tmp) {
		last = tmp;

		if (!strcmp(tmp->upsname, upsname)) {
			if (!strcmp(var, "driver"))
				tmp->driver = xstrdup(val);

			if (!strcmp(var, "port"))
				tmp->port = xstrdup(val);

			if (!strcmp(var, "maxstartdelay"))
				tmp->maxstartdelay = atoi(val);

			if (!strcmp(var, "sdorder")) {
				tmp->sdorder = atoi(val);

				if (tmp->sdorder > maxsdorder)
					maxsdorder = tmp->sdorder;
			}

			return;
		}

		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(ups_t));
	tmp->upsname = xstrdup(upsname);
	tmp->driver = NULL;
	tmp->port = NULL;
	tmp->next = NULL;
	tmp->sdorder = 0;
	tmp->maxstartdelay = -1;	/* use global value by default */

	if (!strcmp(var, "driver"))
		tmp->driver = xstrdup(val);

	if (!strcmp(var, "port"))
		tmp->port = xstrdup(val);

	if (last)
		last->next = tmp;
	else
		upstable = tmp;
}

/* handle sending the signal */
static void stop_driver(const ups_t *ups)
{
	char	pidfn[SMALLBUF];
	int	ret;
	struct stat	fs;

	upsdebugx(1, "Stopping UPS: %s", ups->upsname);

	snprintf(pidfn, sizeof(pidfn), "%s/%s-%s.pid", altpidpath(),
		ups->driver, ups->upsname);
	ret = stat(pidfn, &fs);

	if ((ret != 0) && (ups->port != NULL)) {
		upslog_with_errno(LOG_ERR, "Can't open %s", pidfn);
		snprintf(pidfn, sizeof(pidfn), "%s/%s-%s.pid", altpidpath(),
			ups->driver, xbasename(ups->port));
		ret = stat(pidfn, &fs);
	}

	if (ret != 0) {
		upslog_with_errno(LOG_ERR, "Can't open %s either", pidfn);
		exec_error++;
		return;
	}

	upsdebugx(2, "Sending signal to %s", pidfn);

	if (testmode)
		return;

	ret = sendsignalfn(pidfn, SIGTERM);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "Stopping %s failed", pidfn);
		exec_error++;
		return;
	}
}

static void waitpid_timeout(const int sig)
{
	NUT_UNUSED_VARIABLE(sig);

	/* do nothing */
	return;
}

/* print out a command line at the given debug level. */
static void debugcmdline(int level, const char *msg, char *const argv[])
{
	char	cmdline[LARGEBUF];

	snprintf(cmdline, sizeof(cmdline), "%s", msg);

	while (*argv) {
		snprintfcat(cmdline, sizeof(cmdline), " %s", *argv++);
	}

	upsdebugx(level, "%s", cmdline);
}

static void forkexec(char *const argv[], const ups_t *ups)
{
#ifndef WIN32
	int	ret;
	pid_t	pid;

	pid = fork();

	if (pid < 0)
		fatal_with_errno(EXIT_FAILURE, "fork");

	if (pid != 0) {			/* parent */
		int	wstat;
		struct sigaction	sa;

		/* Handle "parallel" drivers startup */
		if (waitfordrivers == 0) {
			upsdebugx(2, "'nowait' set, continuing...");
			return;
		}

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = waitpid_timeout;
		sigaction(SIGALRM, &sa, NULL);

		/* Use the local maxstartdelay, if available */
		if (ups->maxstartdelay != -1) {
			if (ups->maxstartdelay >= 0)
				alarm((unsigned int)ups->maxstartdelay);
		} else { /* Otherwise, use the global (or default) value */
			if (maxstartdelay >= 0)
				alarm((unsigned int)maxstartdelay);
		}

		ret = waitpid(pid, &wstat, 0);

		alarm(0);

		if (ret == -1) {
			upslogx(LOG_WARNING, "Startup timer elapsed, continuing...");
			exec_error++;
			return;
		}

		if (WIFEXITED(wstat) == 0) {
			upslogx(LOG_WARNING, "Driver exited abnormally");
			exec_error++;
			return;
		}

		if (WEXITSTATUS(wstat) != 0) {
			upslogx(LOG_WARNING, "Driver failed to start"
			" (exit status=%d)", WEXITSTATUS(wstat));
			exec_error++;
			return;
		}

		/* the rest only work when WIFEXITED is nonzero */

		if (WIFSIGNALED(wstat)) {
			upslog_with_errno(LOG_WARNING, "Driver died after signal %d",
				WTERMSIG(wstat));
			exec_error++;
		}

		return;
	}

	/* child */

	ret = execv(argv[0], argv);

	/* shouldn't get here */
	fatal_with_errno(EXIT_FAILURE, "execv");
#else
	BOOL	ret;
	char	commandline[SMALLBUF];
	STARTUPINFO StartupInfo;
	PROCESS_INFORMATION ProcessInformation;
	int 	i = 1;

	memset(&StartupInfo,0,sizeof(STARTUPINFO));

	/* the command line is made of the driver name followed by args */
	snprintf(commandline,sizeof(commandline),"%s", ups->driver);
	while( argv[i] != NULL ) {	
		snprintfcat(commandline, sizeof(commandline), " %s", argv[i]);
		i++;
	}
	ret = CreateProcess(
			argv[0],
			commandline,
			NULL,
			NULL,
			FALSE,
			CREATE_NEW_PROCESS_GROUP,
			NULL,
			NULL,
			&StartupInfo,
			&ProcessInformation
			);

	if( ret == 0 ) {
		fatal_with_errno(EXIT_FAILURE, "execv");
	}
#endif
}

static void start_driver(const ups_t *ups)
{
	char	*argv[9];
	char	dfn[SMALLBUF], dbg[SMALLBUF];
	int	ret, arg = 0;
	int	initial_exec_error = exec_error, drv_maxretry = maxretry;
	struct stat	fs;

	upsdebugx(1, "Starting UPS: %s", ups->upsname);

#ifndef WIN32
	snprintf(dfn, sizeof(dfn), "%s/%s", driverpath, ups->driver);
#else
	snprintf(dfn, sizeof(dfn), "%s/%s.exe", driverpath, ups->driver);
#endif
	ret = stat(dfn, &fs);

	if (ret < 0)
		fatal_with_errno(EXIT_FAILURE, "Can't start %s", dfn);

	argv[arg++] = dfn;

	if (nut_debug_level_passthrough > 0
	&&  nut_debug_level > 0
	&&  sizeof(dbg) > 3
	) {
		size_t d, m;

		/* cut-off point: buffer size or requested debug level */
		m = sizeof(dbg) - 1;	/* leave a place for '\0' */

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wtautological-compare"
# pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
		/* Different platforms, different sizes, none fits all... */
		/* can we fit this many 'D's? */
		if ((uintmax_t)SIZE_MAX > (uintmax_t)nut_debug_level /* else can't assign, requested debug level is huge */
		&&  (size_t)nut_debug_level + 1 < m
		) {
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
			/* need even fewer (leave a place for '-'): */
			m = (size_t)nut_debug_level + 1;
		} else {
			upsdebugx(1, "Requested debugging level %d is too "
				"high for pass-through args, truncated to %zu",
				nut_debug_level,
				(m - 1)	/* count off '-' (and '\0' already) chars */
				);
		}

		dbg[0] = '-';
		for (d = 1; d < m ; d++) {
			dbg[d] = 'D';
		}
		dbg[d] = '\0';
		argv[arg++] = dbg;
	}

	argv[arg++] = (char *)"-a";		/* FIXME: cast away const */
	argv[arg++] = ups->upsname;

	/* stick on the chroot / user args if given to us */
	if (pt_root) {
		argv[arg++] = (char *)"-r";	/* FIXME: cast away const */
		argv[arg++] = pt_root;
	}

	if (pt_user) {
		argv[arg++] = (char *)"-u";	/* FIXME: cast away const */
		argv[arg++] = pt_user;
	}

	/* tie it off */
	argv[arg++] = NULL;


	while (drv_maxretry > 0) {
		int cur_exec_error = exec_error;

		upsdebugx(2, "%i remaining attempts", drv_maxretry);
		debugcmdline(2, "exec: ", argv);
		drv_maxretry--;

		if (!testmode) {
			forkexec(argv, ups);
		}

		/* driver command succeeded */
		if (cur_exec_error == exec_error) {
			drv_maxretry = 0;
			exec_error = initial_exec_error;
		}
		else {
		/* otherwise, retry if still needed */
			if (drv_maxretry > 0)
				if (retrydelay >= 0)
					sleep ((unsigned int)retrydelay);
		}
	}
}

static void help(const char *progname)
	__attribute__((noreturn));

static void help(const char *progname)
{
	printf("Starts and stops UPS drivers via ups.conf.\n\n");
	printf("usage: %s [OPTIONS] (start | stop | shutdown) [<ups>]\n\n", progname);

	printf("  -h			display this help\n");
	printf("  -r <path>		drivers will chroot to <path>\n");
	printf("  -t			testing mode - prints actions without doing them\n");
	printf("  -u <user>		drivers started will switch from root to <user>\n");
	printf("  -D            	raise debugging level\n");
	printf("  -d            	pass debugging level from upsdrvctl to driver\n");
	printf("  start			start all UPS drivers in ups.conf\n");
	printf("  start	<ups>		only start driver for UPS <ups>\n");
	printf("  stop			stop all UPS drivers in ups.conf\n");
	printf("  stop <ups>		only stop driver for UPS <ups>\n");
	printf("  shutdown		shutdown all UPS drivers in ups.conf\n");
	printf("  shutdown <ups>	only shutdown UPS <ups>\n");

	exit(EXIT_SUCCESS);
}

static void shutdown_driver(const ups_t *ups)
{
	char	*argv[9];
	char	dfn[SMALLBUF];
	int	arg = 0;

	upsdebugx(1, "Shutdown UPS: %s", ups->upsname);

	snprintf(dfn, sizeof(dfn), "%s/%s", driverpath, ups->driver);

	argv[arg++] = dfn;
	argv[arg++] = (char *)"-a";		/* FIXME: cast away const */
	argv[arg++] = ups->upsname;
	argv[arg++] = (char *)"-k";		/* FIXME: cast away const */

	/* stick on the chroot / user args if given to us */
	if (pt_root) {
		argv[arg++] = (char *)"-r";	/* FIXME: cast away const */
		argv[arg++] = pt_root;
	}

	if (pt_user) {
		argv[arg++] = (char *)"-u";	/* FIXME: cast away const */
		argv[arg++] = pt_user;
	}

	argv[arg++] = NULL;

	debugcmdline(2, "exec: ", argv);

	if (!testmode) {
		forkexec(argv, ups);
	}
}

static void send_one_driver(void (*command)(const ups_t *), const char *upsname)
{
	ups_t	*ups = upstable;

	if (!ups)
		fatalx(EXIT_FAILURE, "Error: no UPS definitions found in ups.conf!\n");

	while (ups) {
		if (!strcmp(ups->upsname, upsname)) {
			command(ups);
			return;
		}

		ups = ups->next;
	}

	fatalx(EXIT_FAILURE, "UPS %s not found in ups.conf", upsname);
}

/* walk UPS table and send command to all UPSes according to sdorder */
static void send_all_drivers(void (*command)(const ups_t *))
{
	ups_t	*ups;
	int	i;

	if (!upstable)
		fatalx(EXIT_FAILURE, "Error: no UPS definitions found in ups.conf");

	if (command != &shutdown_driver) {
		ups = upstable;

		while (ups) {
			command(ups);

			ups = ups->next;
		}

		return;
	}

	for (i = 0; i <= maxsdorder; i++) {
		ups = upstable;

		while (ups) {
			if (ups->sdorder == i)
				command(ups);

			ups = ups->next;
		}
	}
}

static void exit_cleanup(void)
{
	ups_t	*tmp, *next;

	tmp = upstable;

	while (tmp) {
		next = tmp->next;

		free(tmp->driver);
		free(tmp->port);
		free(tmp->upsname);
		free(tmp);

		tmp = next;
	}

	free(driverpath);
}

int main(int argc, char **argv)
{
	int	i;
	char	*prog;
	void	(*command)(const ups_t *) = NULL;

	printf("Network UPS Tools - UPS driver controller %s\n",
		UPS_VERSION);

	prog = argv[0];
	while ((i = getopt(argc, argv, "+htu:r:DdV")) != -1) {
		switch(i) {
			case 'r':
				pt_root = optarg;
				break;

			case 't':
				testmode = 1;
				break;

			case 'u':
				pt_user = optarg;
				break;

			case 'V':
				exit(EXIT_SUCCESS);

			case 'D':
				nut_debug_level++;
				break;

			case 'd':
				nut_debug_level_passthrough = 1;
				break;

			case 'h':
			default:
				help(prog);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		help(prog);

	if (testmode) {
		printf("*** Testing mode: not calling exec/kill\n");

		if (nut_debug_level < 2)
			nut_debug_level = 2;
	}

	if (nut_debug_level_passthrough == 0) {
		upsdebugx(2, "\n"
			"If you're not a NUT core developer, chances are that you're told to enable debugging\n"
			"to see why a driver isn't working for you. We're sorry for the confusion, but this is\n"
			"the 'upsdrvctl' wrapper, not the driver you're interested in.\n\n"
			"Below you'll find one or more lines starting with 'exec:' followed by an absolute\n"
			"path to the driver binary and some command line option. This is what the driver\n"
			"starts and you need to copy and paste that line and append the debug flags to that\n"
			"line (less the 'exec:' prefix).\n\n"
			"Alternately, provide an additional '-d' (lower-case) parameter to 'upsdrvctl' to\n"
			"pass its current debug level to the launched driver.\n");
	}

	if (!strcmp(argv[0], "start"))
		command = &start_driver;

	if (!strcmp(argv[0], "stop"))
		command = &stop_driver;

	if (!strcmp(argv[0], "shutdown"))
		command = &shutdown_driver;

	if (!command)
		fatalx(EXIT_FAILURE, "Error: unrecognized command [%s]", argv[0]);

	driverpath = xstrdup(DRVPATH);	/* set default */

	atexit(exit_cleanup);

	read_upsconf();

	if (argc == 1)
		send_all_drivers(command);
	else
		send_one_driver(command, argv[1]);

	if (exec_error)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
