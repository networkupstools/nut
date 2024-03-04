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
#else
#include "wincompat.h"
#endif

#include "proto.h"
#include "common.h"
#include "upsconf.h"
#include "attribute.h"
#include "nut_stdint.h"
#include "main.h"
#include "upsdrvquery.h"

typedef struct {
	char	*upsname;
	char	*driver;
	char	*port;
	int	sdorder;
	int	maxstartdelay;
	int	exceeded_timeout;
#ifndef WIN32
	pid_t	pid;
#else
	int	pid;	/* for WIN32 used just as a flag that this UPS was started by this tool in this run */
#endif
	void	*next;
}	ups_t;

static ups_t	*upstable = NULL;
static int	upscount = 0;

static int	maxsdorder = 0, testmode = 0, exec_error = 0, exec_timeout = 0;

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

	/* passthrough to the drivers: chroot path and new user name; signal/command */
static char	*pt_root = NULL, *pt_user = NULL, *pt_cmd = NULL;

	/* flag to pass nut_debug_level to launched drivers (as their -D... args) */
static int	nut_debug_level_passthrough = 0;
static int	nut_foreground_passthrough = -1;

/* Keep track of requested operation (function pointer) */
static void	(*command)(const ups_t *) = NULL;

/* signal handling */
int	exit_flag = 0;
#ifndef WIN32
static int	reload_flag = 0;
static time_t	last_dangerous_reload = 0;
#endif
#ifndef WIN32
static int	signal_flag = 0;
#else
static char	*signal_flag = NULL;
#endif

void do_upsconf_args(char *arg_upsname, char *var, char *val)
{
	ups_t	*tmp, *last;

	/* handle global declarations */
	if (!arg_upsname) {
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

		if (!strcmp(var, "nowait")) {
			char * s = getenv("NUT_IGNORE_NOWAIT");
			if (s && !strcmp(s, "true")) {
				upsdebugx(0, "NOTE: 'nowait' setting ignored due to NUT_IGNORE_NOWAIT envvar");
			} else {
				waitfordrivers = 0;
			}
		}

		/* ignore anything else - it's probably for main */

		return;
	}

	last = tmp = upstable;

	while (tmp) {
		last = tmp;

		if (!strcmp(tmp->upsname, arg_upsname)) {
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
	tmp->upsname = xstrdup(arg_upsname);
	tmp->driver = NULL;
	tmp->port = NULL;
	tmp->pid = -1;
	tmp->next = NULL;
	tmp->sdorder = 0;
	tmp->maxstartdelay = -1;	/* use global value by default */
	tmp->exceeded_timeout = 0;

	if (!strcmp(var, "driver"))
		tmp->driver = xstrdup(val);

	if (!strcmp(var, "port"))
		tmp->port = xstrdup(val);

	if (last)
		last->next = tmp;
	else
		upstable = tmp;
}

static void signal_driver_cmd(const ups_t *ups,
#ifndef WIN32
	int cmd
#else
	const char *cmd
#endif
)
{
#ifndef WIN32
	char	pidfn[SMALLBUF];
#endif
	int	ret;

#ifndef WIN32
	if (cmd == SIGCMD_RELOAD_OR_ERROR)
#else
	if (cmd && !strcmp(cmd, SIGCMD_RELOAD_OR_ERROR))
#endif
	{
		/* not a signal, use socket protocol */
		char buf[LARGEBUF];
		struct timeval	tv;

		upsdebugx(1, "Signalling UPS [%s]: %s",
			ups->upsname, "driver.reload-or-error");

		if (testmode)
			return;

		/* Post the query and wait for reply */
		/* FIXME: coordinate with pollfreq? */
		tv.tv_sec = 15;
		tv.tv_usec = 0;
		ret = upsdrvquery_oneshot(ups->driver, ups->upsname,
			"INSTCMD driver.reload-or-error\n",
			buf, sizeof(buf), &tv);
		if (ret < 0) {
			goto socket_error;
		} else {
			upslogx(LOG_INFO, "Request to reload-or-error returned code %d", ret);
			if (ret != STAT_INSTCMD_HANDLED)
				exec_error++;
			/* TODO: Propagate "ret" to caller, eventually CLI exit-code? */
		}

		return;

socket_error:
		upslog_with_errno(LOG_ERR, "Socket dialog with the other driver instance");
		exec_error++;
		return;
	}

#ifndef WIN32
/* TODO: implement WIN32 */
/* handle generally signalling the UPS */
	/* Real signals */
#ifndef WIN32
	upsdebugx(1, "Signalling UPS [%s]: %d (%s)",
		ups->upsname, cmd, strsignal(cmd));
#else
	upsdebugx(1, "Signalling UPS [%s]: %s",
		ups->upsname, cmd);
#endif

#ifndef WIN32
	if (ups->pid == -1) {
		struct stat	fs;
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
	} else {
		/* We started the driver in this run of upsdrvctl
		 * tool, stayed foregrounded, and now are singnalling
		 * the driver(s) tracked by this process.
		 * NOTE: Not a filename here, but using same variable
		 * name makes the code below simpler to maintain.
		 */
		snprintf(pidfn, sizeof(pidfn), "PID %" PRIdMAX, (intmax_t)ups->pid);
	}
#else
	snprintf(pidfn, sizeof(pidfn), "%s-%s", ups->driver, ups->upsname);
#endif

	upsdebugx(2, "Sending signal to %s", pidfn);

	if (testmode)
		return;

#ifndef WIN32
	if (ups->pid == -1) {
		ret = sendsignalfn(pidfn, cmd);
	} else {
		ret = sendsignalpid(ups->pid, cmd);
		/* reap zombie if this child died */
		if (waitpid(ups->pid, NULL, WNOHANG) == ups->pid) {
			upslog_with_errno(LOG_WARNING,
				"Child process %s exited; signal return code is %d",
				pidfn, ret);
		}
	}
#else
	ret = sendsignal(pidfn, cmd);
#endif

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "Signalling %s failed: %d", pidfn, ret);
		exec_error++;
	}
#endif	/* WIN32 */
}

/* handle generally signalling the UPS with recently raised signal */
static void signal_driver(const ups_t *ups) {
	signal_driver_cmd(ups, signal_flag);
}

/* handle sending the signal */
static void stop_driver(const ups_t *ups)
{
	char	pidfn[SMALLBUF];
	int	ret, i;

	upsdebugx(1, "Stopping UPS: %s", ups->upsname);

#ifndef WIN32
	if (ups->pid == -1) {
		struct stat	fs;
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
	} else {
		/* We started the driver in this run of upsdrvctl
		 * tool, stayed foregrounded, and now are exiting.
		 * NOTE: Not a filename here, but using same variable
		 * name makes the code below simpler to maintain.
		 */
		snprintf(pidfn, sizeof(pidfn), "PID %" PRIdMAX, (intmax_t)ups->pid);
	}
#else
	snprintf(pidfn, sizeof(pidfn), "%s-%s", ups->driver, ups->upsname);
#endif

	upsdebugx(2, "Sending signal to %s", pidfn);

	if (testmode)
		return;

#ifndef WIN32
	if (ups->pid == -1) {
		ret = sendsignalfn(pidfn, SIGTERM);
	} else {
		ret = sendsignalpid(ups->pid, SIGTERM);
		/* reap zombie if this child died */
		if (waitpid(ups->pid, NULL, WNOHANG) == ups->pid) {
			return;
		}
	}
#else
	ret = sendsignal(pidfn, COMMAND_STOP);
#endif

	if (ret < 0) {
#ifndef WIN32
		upsdebugx(2, "SIGTERM to %s failed, retrying with SIGKILL", pidfn);
		if (ups->pid == -1) {
			ret = sendsignalfn(pidfn, SIGKILL);
		} else {
			ret = sendsignalpid(ups->pid, SIGKILL);
			/* reap zombie if this child died */
			if (waitpid(ups->pid, NULL, WNOHANG) == ups->pid) {
				return;
			}
		}
#else
		upsdebugx(2, "Stopping %s failed, retrying again", pidfn);
		ret = sendsignal(pidfn, COMMAND_STOP);
#endif
		if (ret < 0) {
			upslog_with_errno(LOG_ERR, "Stopping %s failed", pidfn);
			exec_error++;
			return;
		}
	}

	for (i = 0; i < 5 ; i++) {
#ifndef WIN32
		if (ups->pid == -1) {
			ret = sendsignalfn(pidfn, 0);
		} else {
			/* reap zombie if this child died */
			if (waitpid(ups->pid, NULL, WNOHANG) == ups->pid) {
					return;
			}
			ret = sendsignalpid(ups->pid, 0);
		}
#else
		ret = sendsignalfn(pidfn, 0);
#endif
		if (ret != 0) {
			upsdebugx(2, "Sending signal to %s failed, driver is finally down or wrongly owned", pidfn);
			return;
		}
		sleep(1);
	}

#ifndef WIN32
	upslog_with_errno(LOG_ERR, "Stopping %s failed, retrying harder", pidfn);
	if (ups->pid == -1) {
		ret = sendsignalfn(pidfn, SIGKILL);
	} else {
		/* reap zombie if this child died */
		if (waitpid(ups->pid, NULL, WNOHANG) == ups->pid) {
			return;
		}
		ret = sendsignalpid(ups->pid, SIGKILL);
	}
#else
	upslog_with_errno(LOG_ERR, "Stopping %s failed, retrying again", pidfn);
	ret = sendsignal(pidfn, COMMAND_STOP);
#endif
	if (ret == 0) {
		for (i = 0; i < 5 ; i++) {
#ifndef WIN32
			if (ups->pid == -1) {
				ret = sendsignalfn(pidfn, 0);
			} else {
				/* reap zombie if this child died */
				if (waitpid(ups->pid, NULL, WNOHANG) == ups->pid) {
					return;
				}
				ret = sendsignalpid(ups->pid, 0);
			}
#else
			ret = sendsignalfn(pidfn, 0);
#endif
			if (ret != 0) {
				upsdebugx(2, "Sending signal to %s failed, driver is finally down or wrongly owned", pidfn);
				/* While a TERMinated driver cleans up,
				 * a stuck and KILLed one does not, so:
				 */
				if (ups->pid == -1) {
					unlink(pidfn);
				}
				return;
			}
			sleep(1);
		}
	}

	upslog_with_errno(LOG_ERR, "Stopping %s failed", pidfn);
	exec_error++;
}

void set_exit_flag(const int sig)
{
	exit_flag = sig;
}

static void set_signal_flag(const
#ifndef WIN32
	int
#else
	char *
#endif
	sig
) {
	/* non-const, so some casting trickery */
	signal_flag = (
#ifndef WIN32
		int
#else
		char *
#endif
		)sig;
}

static void reset_signal_flag(void)
{
#ifndef WIN32
	set_signal_flag(0);
#else
	set_signal_flag(NULL);
#endif
}

#ifndef WIN32
/* TODO: Equivalent for WIN32 - see SIGCMD_RELOAD in upd and upsmon */
static void set_reload_flag(const
#ifndef WIN32
	int
#else
	char *
#endif
	sig
) {
	set_signal_flag(sig);
	switch (sig) {
		case SIGCMD_RELOAD_OR_EXIT:     /* SIGUSR1 */
			/* reload-or-exit (this driver instance may die) */
			reload_flag = 2;
			break;

# ifdef SIGCMD_RELOAD_OR_RESTART
		case SIGCMD_RELOAD_OR_RESTART:  /* SIGUSR2 */
			/* reload-or-restart (this driver instance may recycle itself) */
			/* FIXME: Not implemented yet */
			reload_flag = 3;
			break;
# endif

		case SIGCMD_RELOAD:     /* SIGHUP */
		case SIGCMD_RELOAD_OR_ERROR:	/* Not even a signal, but a socket protocol action */
		default:
			/* reload what we can, log what needs a restart so skipped */
			reload_flag = 1;
	}

	upsdebugx(1, "%s: raising reload flag due to signal %d (%s) => reload_flag=%d",
		__func__, sig, strsignal(sig), reload_flag);
}
#endif

static void setup_signals(void)
{
#ifndef WIN32
	struct sigaction	sa;
#endif

	set_exit_flag(0);
	reset_signal_flag();

#ifndef WIN32
	/* Keep in sync with signal handling in drivers/main.c */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = set_exit_flag;

	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

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
	sigaction(SIGCMD_RELOAD, &sa, NULL);    /* SIGHUP */
	sigaction(SIGCMD_RELOAD_OR_EXIT, &sa, NULL);    /* SIGUSR1 */
# ifdef SIGCMD_RELOAD_OR_RESTART
/* FIXME: Want SIGCMD_RELOAD_OR_RESTART implemented */
	sigaction(SIGCMD_RELOAD_OR_RESTART, &sa, NULL); /* SIGUSR2 */
# endif

# ifdef SIGCMD_DATA_DUMP
	/* handle run-time data dump (may be limited to non-backgrounding lifetimes) */
	sa.sa_handler = set_signal_flag;
	sigaction(SIGCMD_DATA_DUMP, &sa, NULL); /* SIGURG or SIGWINCH something else on obscure systems */
# endif
#endif	/* WIN32 */
}

#ifndef WIN32
static void waitpid_timeout(const int sig)
{
	NUT_UNUSED_VARIABLE(sig);

	/* do nothing */
	return;
}
#endif

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

	if (nut_foreground_passthrough > 0 && upscount == 1) {
		upsdebugx(1, "Starting the only driver with explicitly "
			"requested foregrounding mode, not forking");
	} else {
		pid_t	pid, waitret;

		pid = fork();

		if (pid < 0)
			fatal_with_errno(EXIT_FAILURE, "fork");

		if (pid != 0) {			/* parent */
			int	wstat;
			struct sigaction	sa;

			/* work around const for this one... */
			int *pupid = (int *)&(ups->pid);
			int *puexectimeout = (int *)&(ups->exceeded_timeout);
			*pupid = pid;
			*puexectimeout = 0;

			/* Handle "parallel" drivers startup */
			if (waitfordrivers == 0) {
				upsdebugx(2, "'nowait' set, continuing...");
				return;
			}

			if (nut_foreground_passthrough > 0 && upscount > 1) {
				/* Let upsdrvctl fork to run its numerous children
				 * but without further forking on their side - so
				 * not waiting for them to complete start-ups.
				 */
				upsdebugx(1, "Starting driver with explicitly "
					"requested foregrounding mode: will not "
					"wait for it to fork and detach, continuing...");
				return;
			}

			if (nut_foreground_passthrough != 0
			 && nut_debug_level > 0
			 && nut_debug_level_passthrough > 0
			) {
				upsdebugx(2, "Starting driver with debug but "
					"without explicit backgrounding: "
					"will not wait for it to fork and "
					"detach, continuing...");
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

			waitret = waitpid(pid, &wstat, 0);

			alarm(0);

			if (waitret == -1) {
				upslogx(LOG_WARNING, "Startup timer elapsed, continuing...");
				exec_timeout++;
				*puexectimeout = 1;
				return;
			}

			if (WIFEXITED(wstat) == 0) {
				upslogx(LOG_WARNING, "Driver exited abnormally");
				exec_error++;
				return;
			}

			/* the rest only work when WIFEXITED is nonzero */

			if (WEXITSTATUS(wstat) != 0) {
				upslogx(LOG_WARNING, "Driver failed to start"
				" (exit status=%d)", WEXITSTATUS(wstat));
				exec_error++;
				return;
			}

			if (WIFSIGNALED(wstat)) {
				upslog_with_errno(LOG_WARNING, "Driver died after signal %d",
					WTERMSIG(wstat));
				exec_error++;
			}

			return;
		}
	}

	/* child or foreground mode (no fork) */

	ret = execv(argv[0], argv);

	/* shouldn't get here normally */
	upsdebugx(1, "%s: execv returned %d", __func__, ret);
	fatal_with_errno(EXIT_FAILURE, "execv");
#else
	BOOL	ret;
	DWORD	res;
	DWORD	exit_code = 0;
	char	commandline[SMALLBUF];
	STARTUPINFO	StartupInfo;
	PROCESS_INFORMATION	ProcessInformation;
	int	i = 1;

	memset(&StartupInfo, 0, sizeof(STARTUPINFO));

	/* the command line is made of the driver name followed by args */
	snprintf(commandline, sizeof(commandline), "%s", ups->driver);
	while (argv[i] != NULL) {
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

	if (ret == 0) {
		fatal_with_errno(EXIT_FAILURE, "execv");
	}

	/* Wait a bit then look at driver process.
	 * Unlike under Linux, Windows spawn drivers directly. If the driver is alive, all is OK.
	 * An optimization can probably be implemented to prevent waiting so much time when all is OK.
	 */
	res = WaitForSingleObject(ProcessInformation.hProcess,
			(ups->maxstartdelay!=-1?ups->maxstartdelay:maxstartdelay)*1000);

	if (res != WAIT_TIMEOUT) {
		GetExitCodeProcess( ProcessInformation.hProcess, &exit_code );
		upslogx(LOG_WARNING, "Driver failed to start (exit status=%d)", ret);
		exec_error++;
		return;
	} else {
		/* work around const for this one... */
		int *pupid = (int *)&(ups->pid);
		int *puexectimeout = (int *)&(ups->exceeded_timeout);
		*pupid = 0;	/* For WIN32, just a flag (not "-1" has a meaning) */
		*puexectimeout = 1;
	}

	return;
#endif
}

static void start_driver(const ups_t *ups)
{
	char	*argv[10];
	char	dfn[SMALLBUF], dbg[SMALLBUF];
	int	ret, arg = 0;
	int	initial_exec_error = exec_error, initial_exec_timeout = exec_timeout, drv_maxretry = maxretry;
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
				"high for pass-through args, truncated to %" PRIuSIZE,
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

	/* Default: -1, FG/BG depends on debugging level */
	/* send_all_drivers() also warns if got many drivers to handle
	 * and foreground mode - it won't loop really */
	switch (nut_foreground_passthrough) {
		case 0:
			argv[arg++] = (char *)"-B";		/* FIXME: cast away const */
			break;
		case 1:
			argv[arg++] = (char *)"-F";		/* FIXME: cast away const */
			break;
		case 2:
			argv[arg++] = (char *)"-FF";		/* FIXME: cast away const */
			break;
		default:
			if (nut_debug_level_passthrough > 0
			&&  nut_debug_level > 0
			) {
				upsdebugx(1, "WARNING: Requested a debugging level "
					"but not explicitly a backgrounding mode - "
					"driver may never try to fork away; however "
					"the upsdrvctl tool will fork it and not wait.");
			}
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
		int cur_exec_timeout = exec_timeout;

		upsdebugx(2, "%i remaining attempts", drv_maxretry);
		debugcmdline(2, "exec: ", argv);
		drv_maxretry--;

		if (!testmode) {
			forkexec(argv, ups);
		}

		/* driver command succeeded */
		if (cur_exec_error == exec_error && cur_exec_timeout == exec_timeout) {
			drv_maxretry = 0;
			exec_error = initial_exec_error;
			exec_timeout = initial_exec_timeout;
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

static void help(const char *arg_progname)
{
	printf("Starts and stops UPS drivers via ups.conf.\n\n");
	printf("usage: %s [OPTIONS] (start | stop | shutdown) [<ups>]\n\n", arg_progname);
	printf("usage: %s [OPTIONS] -c <command> [<ups>]\n\n", arg_progname);

	printf("Common options:\n");
	printf("  -h			display this help\n");
	printf("  -r <path>		drivers will chroot to <path>\n");
	printf("  -t			testing mode - prints actions without doing them\n");
	printf("  -u <user>		drivers started will switch from root to <user>\n");
	printf("  -D			raise debugging level\n");
	printf("  -d			pass debugging level from upsdrvctl to driver\n");
	printf("  -F			driver stays foregrounded even if no debugging is enabled\n");
	printf("  -FF			driver stays foregrounded and still saves the PID file\n");
	printf("  -B			driver(s) stay backgrounded even if debugging is bumped\n");

	printf("Signalling a running driver:\n");
	printf("  -c <command>		send <command> via signal to running driver(s)\n");
	printf("              		supported commands:\n");
#ifndef WIN32
/* FIXME: port event loop from upsd/upsmon to allow messaging fellow drivers in WIN32 builds */
	printf("              		- data-dump: if the driver still has STDOUT attached (maybe\n");
	printf("              		  to log), dump its currently collected information there\n");
	printf("              		- reload: re-read configuration files, ignoring changed\n");
	printf("              		  values which require a driver restart (can not be changed\n");
	printf("              		  on the fly)\n");
#endif /* WIN32 */
	printf("              		- reload-or-error: re-read configuration files, ignoring but\n");
	printf("              		  counting changed values which require a driver restart (can\n");
	printf("              		  not be changed on the fly), and return a success/fail code\n");
	printf("              		  based on that count, so the caller can decide the fate of\n");
	printf("              		  the currently running driver instance\n");
#ifndef WIN32
/* FIXME: port event loop from upsd/upsmon to allow messaging fellow drivers in WIN32 builds */
# ifdef SIGCMD_RELOAD_OR_RESTART
	printf("              		- reload-or-restart: re-read configuration files (close the\n");
	printf("              		  old driver instance device connection if needed, and have\n");
	printf("              		  it effectively restart)\n");
# endif
	printf("              		- reload-or-exit: re-read configuration files (exit the old\n");
	printf("              		  driver instance if needed, so an external caller like the\n");
	printf("              		  systemd or SMF frameworks would start another copy)\n");
#endif /* WIN32 */

	printf("Driver life cycle options:\n");
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

#ifndef WIN32
	snprintf(dfn, sizeof(dfn), "%s/%s", driverpath, ups->driver);
#else
	snprintf(dfn, sizeof(dfn), "%s/%s.exe", driverpath, ups->driver);
#endif

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

static void send_one_driver(void (*command_func)(const ups_t *), const char *arg_upsname)
{
	ups_t	*ups = upstable;

	if (!ups)
		fatalx(EXIT_FAILURE, "Error: no UPS definitions found in ups.conf!\n");

	exec_error = 0;
	exec_timeout = 0;
	while (ups) {
		if (!strcmp(ups->upsname, arg_upsname)) {
			command_func(ups);
			return;
		}

		ups = ups->next;
	}

	fatalx(EXIT_FAILURE, "UPS %s not found in ups.conf", arg_upsname);
}

/* walk UPS table and send command to all UPSes according to sdorder */
static void send_all_drivers(void (*command_func)(const ups_t *))
{
	ups_t	*ups;
	int	i;

	if (!upstable)
		fatalx(EXIT_FAILURE, "Error: no UPS definitions found in ups.conf");

	exec_error = 0;
	exec_timeout = 0;
	if (command_func != &shutdown_driver) {
		ups = upstable;

		/* Only warn when relevant - got more than one device to start */
		if (command_func == &start_driver
		&&  ups->next
		&&  ( (nut_foreground_passthrough > 0)
		      || (nut_foreground_passthrough != 0
		          && nut_debug_level > 0
		          && nut_debug_level_passthrough > 0)
		    )
		) {
			upslogx(LOG_WARNING,
				"Starting \"all\" drivers but requested the %s!"
				"This request will not wait for driver(s) to complete "
				"their initialization%s.",
				(nut_foreground_passthrough > 0
					? "foreground mode"
					: "debug mode without backgrounding"),
				(nut_foreground_passthrough > 0
					? ", but upsdrvctl tool will stay foregrounded" : "")
			);
		}

		while (ups) {
			command_func(ups);

			ups = ups->next;
		}

		return;
	}

	/* Orderly processing of shutdowns */
	for (i = 0; i <= maxsdorder; i++) {
		ups = upstable;

		while (ups) {
			if (ups->sdorder == i)
				command_func(ups);

			ups = ups->next;
		}
	}
}

static void exit_cleanup(void)
{
	ups_t	*tmp, *next;

	upsdebugx(1, "Completed the job of upsdrvctl tool, cleaning up and exiting now");

	tmp = upstable;

	if (command == &start_driver
	&&  upscount > 0
	&&  nut_foreground_passthrough > 0
	) {
		/* First stop the drivers, if any are running */
		while (tmp) {
			next = tmp->next;
			if (tmp->pid != -1) {
				stop_driver(tmp);
			}
			tmp = next;
		}
	}

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

	upsdebugx(1, "Completed the job of upsdrvctl tool, clean-up finished, exiting now");
}

int main(int argc, char **argv)
{
	int	i, lastarg = 0;
	char	*prog;

	printf("Network UPS Tools - UPS driver controller %s\n",
		UPS_VERSION);

	prog = argv[0];
	while ((i = getopt(argc, argv, "+htu:r:DdFBVc:")) != -1) {
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

			case 'F':
				if (nut_foreground_passthrough > 0) {
					/* specified twice to save PID file anyway */
					nut_foreground_passthrough = 2;
				} else {
					nut_foreground_passthrough = 1;
				}
				break;

			case 'B':
				nut_foreground_passthrough = 0;
				break;

			case 'c':
				if (command || pt_cmd) {
					fatalx(EXIT_FAILURE,
						"Error: only one command per run can be "
						"sent with option -%c. Try -h for help.", i);
				}
				command = &signal_driver;

				if (!strncmp(optarg, "reload-or-error", strlen(optarg))) {
					signal_flag = SIGCMD_RELOAD_OR_ERROR;
				}
#ifndef WIN32
/* FIXME: port event loop from upsd/upsmon to allow messaging fellow drivers in WIN32 builds */
				else
				if (!strncmp(optarg, "dump", strlen(optarg))) {
					signal_flag = SIGCMD_DATA_DUMP;
				} else
				if (!strncmp(optarg, "data-dump", strlen(optarg))) {
					signal_flag = SIGCMD_DATA_DUMP;
				} else
				if (!strncmp(optarg, "reload", strlen(optarg))) {
					signal_flag = SIGCMD_RELOAD;
				} else
# ifdef SIGCMD_RELOAD_OR_RESTART
				if (!strncmp(optarg, "reload-or-restart", strlen(optarg))) {
					signal_flag = SIGCMD_RELOAD_OR_RESTART;
				} else
# endif
				if (!strncmp(optarg, "reload-or-exit", strlen(optarg))) {
					signal_flag = SIGCMD_RELOAD_OR_EXIT;
				}
#endif	/* WIN32 */

				/* bad command given */
				if (!signal_flag) {
					fatalx(EXIT_FAILURE,
						"Error: unknown argument to option -%c. Try -h for help.", i);
				}

				pt_cmd = optarg;
#ifndef WIN32
				upsdebugx(1, "Will send signal %d (%s) for command '%s' "
					"to already-running driver (if any) and exit",
					signal_flag, strsignal(signal_flag), optarg);
#else
				upsdebugx(1, "Will send request '%s' for command '%s' "
					"to already-running driver (if any) and exit",
					signal_flag, optarg);
#endif	/* WIN32 */
				break;
			case 'h':
			default:
				help(prog);
		}
	}

	{ /* scoping */
		char *s = getenv("NUT_DEBUG_LEVEL");
		int l;
		if (s && str_to_int(s, &l, 10)) {
			if (l > 0 && nut_debug_level < 1) {
				upslogx(LOG_INFO, "Defaulting debug verbosity to NUT_DEBUG_LEVEL=%d "
					"since none was requested by command-line options", l);
				nut_debug_level = l;
			}	/* else follow -D settings */
		}	/* else nothing to bother about */
	}

	argc -= optind;
	argv += optind;

	/* We expect maybe command (if not signalling above) and maybe UPS name */
	if (!command && argc < 1)
		help(prog);

	if (testmode) {
		printf("*** Testing mode: not calling exec/kill/signal\n");

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
			"pass its current debug level to the launched driver, and '-B' keeps it backgrounded.\n");
	}

	if (!command) {
		if (!strcmp(argv[0], "start")) {
			command = &start_driver;
		} else
		if (!strcmp(argv[0], "stop")) {
			command = &stop_driver;
		} else
		if (!strcmp(argv[0], "shutdown")) {
			command = &shutdown_driver;
		}
		lastarg = 1;
	}

	if (!command)
		fatalx(EXIT_FAILURE, "Error: unrecognized command [%s]", argv[0]);

#ifndef WIN32
	driverpath = xstrdup(DRVPATH);	/* set default */
#else
	driverpath = getfullpath(NULL); /* Relative path in WIN32 */
#endif

	atexit(exit_cleanup);

	read_upsconf(1);

	if (argc == lastarg) {
		ups_t	*tmp = upstable;
		upscount = 0;

		while (tmp) {
			tmp = tmp->next;
			upscount++;
		}

		upsdebugx(1, "upsdrvctl commanding all drivers (%d found): %s",
			upscount, (pt_cmd ? pt_cmd : NUT_STRARG(argv[lastarg])));
		send_all_drivers(command);
	} else
	if (argc == (lastarg + 1)) {
		upscount = 1;
		upsdebugx(1, "upsdrvctl commanding one driver (%s): %s",
			argv[lastarg], (pt_cmd ? pt_cmd : NUT_STRARG(argv[lastarg - 1])));
		send_one_driver(command, argv[lastarg]);
	} else {
		fatalx(EXIT_FAILURE, "Error: extra arguments left on command line\n"
			"(common options should be before a command and UPS name)");
	}

	/* Note that the numeric value here is not precise (it reflects
	 * the number of "timeouts" which grows with amount of drivers
	 * and retries. Below we re-check each driver to convert the
	 * value into some amount of known failures (or succeses). */
	if (exec_timeout) {
#ifndef WIN32
		ups_t	*tmp = upstable;
#endif
		upsdebugx(1, "upsdrvctl: got some timeouts with preceding operations, revising them now");
#ifndef WIN32
		while (tmp) {
			if (tmp->exceeded_timeout && tmp->pid) {
				/* reap zombie if this child died, and
				 * get info if we know how it went (or
				 * still goes) */
				int wstat;
				pid_t waitret = waitpid(tmp->pid, &wstat, WNOHANG);

				upsdebugx(1,
					"Driver [%s] PID %" PRIdMAX " initially exceeded "
					"maxstartdelay but now waitpid() returns %" PRIdMAX
					" and status bits 0x%.*X",
					tmp->upsname, (intmax_t)tmp->pid,
					(intmax_t)waitret, (int)(2*sizeof(wstat)), wstat);

				if (waitret == tmp->pid) {
					upsdebugx(1,
						"Driver [%s] PID %" PRIdMAX " initially exceeded "
						"maxstartdelay but has finished by now",
						tmp->upsname, (intmax_t)tmp->pid);
					tmp->exceeded_timeout = 0;
				} else
				if (waitret == 0) {
					/* Special behavior for WNOHANG */
					upslogx(LOG_WARNING,
						"Driver [%s] PID %" PRIdMAX " initially exceeded "
						"maxstartdelay and is still starting",
						tmp->upsname, (intmax_t)tmp->pid);
					/* TOTHINK: Should this "timeout" cause an error
					 * exit code, if this is the only problem?
					 * Maybe as a special case - if this is the only
					 * driver (dedicated starter) vs. start-all?
					 *     if (argc != (lastarg + 1)) ...
					 * or  if (upscount == 1) ...
					 */
					exec_error++;
				} else
				if (waitret == -1) {
					upslog_with_errno(LOG_WARNING,
						"Driver [%s] PID %" PRIdMAX " initially exceeded "
						"maxstartdelay and we got an error asking it again",
						tmp->upsname, (intmax_t)tmp->pid);
					exec_error++;
				} else
				if (WIFEXITED(wstat) == 0) {
					upslogx(LOG_WARNING,
						"Driver [%s] PID %" PRIdMAX " initially exceeded "
						"maxstartdelay and has exited abnormally by now",
						tmp->upsname, (intmax_t)tmp->pid);
					exec_error++;
				} else
				/* the rest only work when WIFEXITED is nonzero */
				if (WEXITSTATUS(wstat) != 0) {
					upslogx(LOG_WARNING,
						"Driver [%s] PID %" PRIdMAX " initially exceeded "
						"maxstartdelay and has failed to start by now "
						"(exit status=%d)",
						tmp->upsname, (intmax_t)tmp->pid, WEXITSTATUS(wstat));
					exec_error++;
				} else
				if (WIFSIGNALED(wstat)) {
					upslog_with_errno(LOG_WARNING,
						"Driver [%s] PID %" PRIdMAX " initially exceeded "
						"maxstartdelay and has died after signal %d by now",
						tmp->upsname, (intmax_t)tmp->pid, WTERMSIG(wstat));
					exec_error++;
				}
			}

			tmp = tmp->next;
		}
#else	/* WIN32 */
		/* TOTHINK: Is there something we can do on the platform? */
		exec_error++;
#endif	/* WIN32 */
	}

	if (exec_error) {
		upsdebugx(1, "upsdrvctl: got some errors with preceding operations, exiting with failure now");
		exit(EXIT_FAILURE);
	}

	if (command == &start_driver
	&&  upscount > 0
	&&  nut_foreground_passthrough > 0
	) {
		/* Note: for a single started driver, we just
		 * exec() it and should not even get here
		 */
		upsdebugx(1, "upsdrvctl: was asked for explicit foregrounding - "
			"not exiting now (driver startup was completed)");

		/* raise exit_flag upon SIGTERM, Ctrl+C, etc. */
		setup_signals();
		while (!exit_flag) {
#ifndef WIN32
			ups_t	*tmp = upstable, *next;
			/* Track if any child process has stopped (due to
			 * an error, normal exit, signal...) to kill others
			 * and exit the tool - with error if applicable.
			 */
			while (tmp) {
				next = tmp->next;
				if (tmp->pid != -1) {
					int	status;
					if (waitpid(tmp->pid, &status, WNOHANG) == tmp->pid) {
						if (WIFEXITED(status)) {
							int	es = WEXITSTATUS(status);
							time_t	now;
							double	elapsed;

							time(&now);
							elapsed = difftime(now, last_dangerous_reload);
							if (elapsed < 60
							 || (es - 128) == SIGCMD_RELOAD_OR_EXIT
# ifdef SIGCMD_RELOAD_OR_RESTART
							 || (es - 128) == SIGCMD_RELOAD_OR_RESTART
# endif
							) {
								/* Arbitrary but generous time to handle
								 * a reload including driver loop lag */
								upsdebugx(1, "Driver [%s] for [%s] exited "
									"soon after reload-or-exit or "
									"similar signal, restarting it",
									tmp->driver, tmp->upsname);
								tmp->pid = -1;
								start_driver(tmp);
							} else {
								/* Quit without excuses, recycle myself */
								upsdebugx(1, "Driver [%s] for [%s] exited "
									"inexplicably with code %d, aborting",
									tmp->driver, tmp->upsname, es);
								if (last_dangerous_reload)
									upsdebugx(1, "Last 'dangerous' signal "
										"was processed %f sec ago",
										elapsed);
								exit_flag = -1;
								tmp->pid = -1;
							}
						}
					}
				}
				tmp = next;
			}

			if (exit_flag == -1) {
				fatalx(EXIT_FAILURE, "At least one tracked driver running "
					"in foreground mode has exited, stopping upsdrvctl "
					"(and other tracked drivers) so the bundle can be "
					"restarted by system properly");
				/* NOTE: Users really should run one driver per instance,
				 * wrapped in services where available */
			}

			if (signal_flag) {
				upsdebugx(1, "upsdrvctl: handling signal: starting");
				if (signal_flag == SIGCMD_RELOAD_OR_EXIT
# ifdef SIGCMD_RELOAD_OR_RESTART
				 || signal_flag == SIGCMD_RELOAD_OR_RESTART
# endif
				) time(&last_dangerous_reload);

				tmp = upstable;
				while (tmp) {
					next = tmp->next;
					signal_driver(tmp);
					tmp = next;
				}
				reset_signal_flag();
				upsdebugx(1, "upsdrvctl: handling signal: finished");
			}
#endif	/* WIN32 */

			sleep(1);
		}
	}

	upsdebugx(1, "upsdrvctl: successfully finished");
	exit(EXIT_SUCCESS);
}
