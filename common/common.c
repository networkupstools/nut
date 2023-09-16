/* common.c - common useful functions

   Copyright (C) 2000  Russell Kroll <rkroll@exploits.org>
   Copyright (C) 2021-2022  Jim Klimov <jimklimov+nut@gmail.com>

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
#include "timehead.h"

#include <ctype.h>
#ifndef WIN32
#include <syslog.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/un.h>
#else
#include <wincompat.h>
#endif

#include <dirent.h>
#if !HAVE_DECL_REALPATH
# include <sys/stat.h>
#endif

#ifdef WITH_LIBSYSTEMD
# include <systemd/sd-daemon.h>
/* upsnotify() debug-logs its reports; a watchdog ping is something we
 * try to send often so report it just once (whether enabled or not) */
static int upsnotify_reported_watchdog_systemd = 0;
/* Similarly for only reporting once if the notification subsystem is disabled */
static int upsnotify_reported_disabled_systemd = 0;
# ifndef DEBUG_SYSTEMD_WATCHDOG
/* Define this to 1 for lots of spam at debug level 6, and ignoring WATCHDOG_PID
 * so trying to post reports anyway if WATCHDOG_USEC is valid */
#  define DEBUG_SYSTEMD_WATCHDOG 0
# endif
#endif
/* Similarly for only reporting once if the notification subsystem is not built-in */
static int upsnotify_reported_disabled_notech = 0;

/* the reason we define UPS_VERSION as a static string, rather than a
	macro, is to make dependency tracking easier (only common.o depends
	on nut_version_macro.h), and also to prevent all sources from
	having to be recompiled each time the version changes (they only
	need to be re-linked). */
#include "nut_version.h"
const char *UPS_VERSION = NUT_VERSION_MACRO;

#include <stdio.h>

/* Know which bitness we were built for,
 * to adjust the search paths for get_libname() */
#include "nut_stdint.h"
#if defined(UINTPTR_MAX) && (UINTPTR_MAX + 0) == 0xffffffffffffffffULL
# define BUILD_64   1
#else
# ifdef BUILD_64
#  undef BUILD_64
# endif
#endif

/* https://stackoverflow.com/a/12844426/4715872 */
#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
pid_t get_max_pid_t()
{
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif
	if (sizeof(pid_t) == sizeof(short)) return (pid_t)SHRT_MAX;
	if (sizeof(pid_t) == sizeof(int)) return (pid_t)INT_MAX;
	if (sizeof(pid_t) == sizeof(long)) return (pid_t)LONG_MAX;
#if defined(__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || (defined _STDC_C99) || (defined __C99FEATURES__) /* C99+ build mode */
# if defined(LLONG_MAX)  /* since C99 */
	if (sizeof(pid_t) == sizeof(long long)) return (pid_t)LLONG_MAX;
# endif
#endif
	abort();
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic pop
#endif
}

	int	nut_debug_level = 0;
	int	nut_log_level = 0;
	static	int	upslog_flags = UPSLOG_STDERR;

	static struct timeval	upslog_start = { 0, 0 };

static void xbit_set(int *val, int flag)
{
	*val |= flag;
}

static void xbit_clear(int *val, int flag)
{
	*val ^= (*val & flag);
}

static int xbit_test(int val, int flag)
{
	return ((val & flag) == flag);
}

/* enable writing upslog_with_errno() and upslogx() type messages to
   the syslog */
void syslogbit_set(void)
{
	xbit_set(&upslog_flags, UPSLOG_SYSLOG);
}

/* get the syslog ready for us */
void open_syslog(const char *progname)
{
#ifndef WIN32
	int	opt;

	opt = LOG_PID;

	/* we need this to grab /dev/log before chroot */
#ifdef LOG_NDELAY
	opt |= LOG_NDELAY;
#endif

	openlog(progname, opt, LOG_FACILITY);

	switch (nut_log_level)
	{
#if HAVE_SETLOGMASK && HAVE_DECL_LOG_UPTO
	case 7:
		setlogmask(LOG_UPTO(LOG_EMERG));	/* system is unusable */
		break;
	case 6:
		setlogmask(LOG_UPTO(LOG_ALERT));	/* action must be taken immediately */
		break;
	case 5:
		setlogmask(LOG_UPTO(LOG_CRIT));		/* critical conditions */
		break;
	case 4:
		setlogmask(LOG_UPTO(LOG_ERR));		/* error conditions */
		break;
	case 3:
		setlogmask(LOG_UPTO(LOG_WARNING));	/* warning conditions */
		break;
	case 2:
		setlogmask(LOG_UPTO(LOG_NOTICE));	/* normal but significant condition */
		break;
	case 1:
		setlogmask(LOG_UPTO(LOG_INFO));		/* informational */
		break;
	case 0:
		setlogmask(LOG_UPTO(LOG_DEBUG));	/* debug-level messages */
		break;
	default:
		fatalx(EXIT_FAILURE, "Invalid log level threshold");
#else
	case 0:
		break;
	default:
		upslogx(LOG_INFO, "Changing log level threshold not possible");
		break;
#endif
	}
#else
	EventLogName = progname;
#endif
}

/* close ttys and become a daemon */
void background(void)
{
#ifndef WIN32
	int	pid;

	if ((pid = fork()) < 0)
		fatal_with_errno(EXIT_FAILURE, "Unable to enter background");

	xbit_set(&upslog_flags, UPSLOG_SYSLOG);
	xbit_clear(&upslog_flags, UPSLOG_STDERR);

	if (pid != 0) {
		/* parent */
		/* these are typically fds 0-2: */
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		_exit(EXIT_SUCCESS);
	}

	/* child */

	/* make fds 0-2 (typically) point somewhere defined */
#ifdef HAVE_DUP2
	/* system can close (if needed) and (re-)open a specific FD number */
	if (1) { /* scoping */
		TYPE_FD devnull = open("/dev/null", O_RDWR);
		if (devnull < 0)
			fatal_with_errno(EXIT_FAILURE, "open /dev/null");

		if (dup2(devnull, STDIN_FILENO) != STDIN_FILENO)
			fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDIN");
		if (dup2(devnull, STDOUT_FILENO) != STDOUT_FILENO)
			fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDOUT");
		if (dup2(devnull, STDERR_FILENO) != STDERR_FILENO)
			fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDERR");

		close(devnull);
	}
#else
# ifdef HAVE_DUP
	/* opportunistically duplicate to the "lowest-available" FD number */
	close(STDIN_FILENO);
	if (open("/dev/null", O_RDWR) != STDIN_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDIN");

	close(STDOUT_FILENO);
	if (dup(STDIN_FILENO) != STDOUT_FILENO)
		fatal_with_errno(EXIT_FAILURE, "dup /dev/null as STDOUT");

	close(STDERR_FILENO);
	if (dup(STDIN_FILENO) != STDERR_FILENO)
		fatal_with_errno(EXIT_FAILURE, "dup /dev/null as STDERR");
# else
	close(STDIN_FILENO);
	if (open("/dev/null", O_RDWR) != STDIN_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDIN");

	close(STDOUT_FILENO);
	if (open("/dev/null", O_RDWR) != STDOUT_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDOUT");

	close(STDERR_FILENO);
	if (open("/dev/null", O_RDWR) != STDERR_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDERR");
# endif
#endif

#ifdef HAVE_SETSID
	setsid();		/* make a new session to dodge signals */
#endif

#else /* WIN32 */
	xbit_set(&upslog_flags, UPSLOG_SYSLOG);
	xbit_clear(&upslog_flags, UPSLOG_STDERR);
#endif

	upslogx(LOG_INFO, "Startup successful");
}

/* do this here to keep pwd/grp stuff out of the main files */
struct passwd *get_user_pwent(const char *name)
{
#ifndef WIN32
	struct passwd *r;
	errno = 0;
	if ((r = getpwnam(name)))
		return r;

	/* POSIX does not specify that "user not found" is an error, so
	   some implementations of getpwnam() do not set errno when this
	   happens. */
	if (errno == 0)
		fatalx(EXIT_FAILURE, "OS user %s not found", name);
	else
		fatal_with_errno(EXIT_FAILURE, "getpwnam(%s)", name);
#else
	NUT_UNUSED_VARIABLE(name);
#endif /* WIN32 */

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) || (defined HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE_RETURN) )
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE_RETURN
#pragma GCC diagnostic ignored "-Wunreachable-code-return"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
# ifdef HAVE_PRAGMA_CLANG_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE_RETURN
#  pragma clang diagnostic ignored "-Wunreachable-code-return"
# endif
#endif
	/* Oh joy, adding unreachable "return" to make one compiler happy,
	 * and pragmas around to make other compilers happy, all at once! */
	return NULL;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) || (defined HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE_RETURN) )
#pragma GCC diagnostic pop
#endif
}

/* change to the user defined in the struct */
void become_user(struct passwd *pw)
{
#ifndef WIN32
	/* if we can't switch users, then don't even try */
	intmax_t initial_uid = getuid();
	intmax_t initial_euid = geteuid();

	if (!pw) {
		upsdebugx(1, "Can not become_user(<null>), skipped");
		return;
	}

	if ((initial_euid != 0) && (initial_uid != 0)) {
		intmax_t initial_gid = getgid();
		if (initial_euid == (intmax_t)pw->pw_uid
		||   initial_uid == (intmax_t)pw->pw_uid
		) {
			upsdebugx(1, "No need to become_user(%s): "
				"already UID=%jd GID=%jd",
				pw->pw_name, initial_uid, initial_gid);
		} else {
			upsdebugx(1, "Can not become_user(%s): "
				"not root initially, "
				"remaining UID=%jd GID=%jd",
				pw->pw_name, initial_uid, initial_gid);
		}
		return;
	}

	if (initial_uid == 0)
		if (seteuid(0))
			fatal_with_errno(EXIT_FAILURE, "getuid gave 0, but seteuid(0) failed");

	if (initgroups(pw->pw_name, pw->pw_gid) == -1)
		fatal_with_errno(EXIT_FAILURE, "initgroups");

	if (setgid(pw->pw_gid) == -1)
		fatal_with_errno(EXIT_FAILURE, "setgid");

	if (setuid(pw->pw_uid) == -1)
		fatal_with_errno(EXIT_FAILURE, "setuid");

	upsdebugx(1, "Succeeded to become_user(%s): now UID=%jd GID=%jd",
		pw->pw_name, (intmax_t)getuid(), (intmax_t)getgid());
#else
	upsdebugx(1, "Can not become_user(%s): not implemented on this platform",
		pw ? pw->pw_name : "<null>");
#endif
}

/* drop down into a directory and throw away pointers to the old path */
void chroot_start(const char *path)
{
	if (chdir(path))
		fatal_with_errno(EXIT_FAILURE, "chdir(%s)", path);

#ifndef WIN32
	if (chroot(path))
		fatal_with_errno(EXIT_FAILURE, "chroot(%s)", path);

#else
	upsdebugx(1, "Can not chroot into %s: not implemented on this platform", path);
#endif

	if (chdir("/"))
		fatal_with_errno(EXIT_FAILURE, "chdir(/)");

#ifndef WIN32
	upsdebugx(1, "chrooted into %s", path);
#endif
}

#ifdef WIN32
/* In WIN32 all non binaries files (namely configuration and PID files)
   are retrieved relative to the path of the binary itself.
   So this function fill "dest" with the full path to "relative_path"
   depending on the .exe path */
char * getfullpath(char * relative_path)
{
	char buf[MAX_PATH];
	if ( GetModuleFileName(NULL, buf, sizeof(buf)) == 0 ) {
		return NULL;
	}

	/* remove trailing executable name and its preceeding slash */
	char * last_slash = strrchr(buf, '\\');
	*last_slash = '\0';

	if( relative_path ) {
		strncat(buf, relative_path, sizeof(buf) - 1);
	}

	return(xstrdup(buf));
}
#endif

/* drop off a pidfile for this process */
void writepid(const char *name)
{
#ifndef WIN32
	char	fn[SMALLBUF];
	FILE	*pidf;
	mode_t	mask;

	/* use full path if present, else build filename in PIDPATH */
	if (*name == '/')
		snprintf(fn, sizeof(fn), "%s", name);
	else
		snprintf(fn, sizeof(fn), "%s/%s.pid", PIDPATH, name);

	mask = umask(022);
	pidf = fopen(fn, "w");

	if (pidf) {
		intmax_t pid = (intmax_t)getpid();
		upsdebugx(1, "Saving PID %" PRIdMAX " into %s", pid, fn);
		fprintf(pidf, "%" PRIdMAX "\n", pid);
		fclose(pidf);
	} else {
		upslog_with_errno(LOG_NOTICE, "writepid: fopen %s", fn);
	}

	umask(mask);
#else
	NUT_UNUSED_VARIABLE(name);
#endif
}

/* send sig to pid, returns -1 for error, or
 * zero for a successfully sent signal
 */
int sendsignalpid(pid_t pid, int sig)
{
#ifndef WIN32
	int	ret;

	if (pid < 2 || pid > get_max_pid_t()) {
		upslogx(LOG_NOTICE,
			"Ignoring invalid pid number %" PRIdMAX,
			(intmax_t) pid);
		return -1;
	}

	/* see if this is going to work first - does the process exist? */
	ret = kill(pid, 0);

	if (ret < 0) {
		perror("kill");
		return -1;
	}

	if (sig != 0) {
		/* now actually send it */
		ret = kill(pid, sig);

		if (ret < 0) {
			perror("kill");
			return -1;
		}
	}

	return 0;
#else
	NUT_UNUSED_VARIABLE(pid);
	NUT_UNUSED_VARIABLE(sig);
	upslogx(LOG_ERR,
		"%s: not implemented for Win32 and "
		"should not have been called directly!",
		__func__);
	return -1;
#endif
}

/* parses string buffer into a pid_t if it passes
 * a few sanity checks; returns -1 on error
 */
pid_t parsepid(const char *buf)
{
	pid_t	pid = -1;
	intmax_t	_pid;

	if (!buf) {
		upsdebugx(6, "%s: called with NULL input", __func__);
		return pid;
	}

	/* assuming 10 digits for a long */
	_pid = strtol(buf, (char **)NULL, 10);
	if (_pid <= get_max_pid_t()) {
		pid = (pid_t)_pid;
	} else {
		upslogx(LOG_NOTICE, "Received a pid number too big for a pid_t: %" PRIdMAX, _pid);
	}

	return pid;
}

/* open pidfn, get the pid, then send it sig
 * returns negative codes for errors, or
 * zero for a successfully sent signal
 */
#ifndef WIN32
int sendsignalfn(const char *pidfn, int sig)
{
	char	buf[SMALLBUF];
	FILE	*pidf;
	pid_t	pid = -1;
	int	ret = -1;

	pidf = fopen(pidfn, "r");
	if (!pidf) {
		upslog_with_errno(LOG_NOTICE, "fopen %s", pidfn);
		return -3;
	}

	if (fgets(buf, sizeof(buf), pidf) == NULL) {
		upslogx(LOG_NOTICE, "Failed to read pid from %s", pidfn);
		fclose(pidf);
		return -2;
	}
	/* TOTHINK: Original code only closed pidf before
	 * exiting the method, on error or "normally".
	 * Why not here? Do we want an (exclusive?) hold
	 * on it while being active in the method?
	 */

	/* this method actively reports errors, if any */
	pid = parsepid(buf);

	if (pid >= 0) {
		/* this method actively reports errors, if any */
		ret = sendsignalpid(pid, sig);
	}

	fclose(pidf);
	return ret;
}

#else	/* => WIN32 */

int sendsignalfn(const char *pidfn, const char * sig)
{
	BOOL	ret;

	ret = send_to_named_pipe(pidfn, sig);

	if (ret != 0) {
		return -1;
	}

	return 0;
}
#endif	/* WIN32 */

int snprintfcat(char *dst, size_t size, const char *fmt, ...)
{
	va_list ap;
	size_t len = strlen(dst);
	int ret;

	size--;
	if (len > size) {
		/* Do not truncate existing string */
		errno = ERANGE;
		return -1;
	}

	va_start(ap, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	/* Note: this code intentionally uses a caller-provided format string */
	ret = vsnprintf(dst + len, size - len, fmt, ap);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	dst[size] = '\0';

	/* Note: there is a standards loophole here: strlen() must return size_t
	 * and printf() family returns a signed int with negatives for errors.
	 * In theory it can overflow a 64-vs-32 bit range, or signed-vs-unsigned.
	 * In practice we hope to not have gigabytes-long config strings.
	 */
	if (ret < 0) {
		return ret;
	}
#ifdef INT_MAX
	if ( ( (unsigned long long)len + (unsigned long long)ret ) >= (unsigned long long)INT_MAX ) {
		errno = ERANGE;
		return -1;
	}
#endif
	return (int)len + ret;
}

/* lazy way to send a signal if the program uses the PIDPATH */
#ifndef WIN32
int sendsignal(const char *progname, int sig)
{
	char	fn[SMALLBUF];

	snprintf(fn, sizeof(fn), "%s/%s.pid", PIDPATH, progname);

	return sendsignalfn(fn, sig);
}
#else
int sendsignal(const char *progname, const char * sig)
{
	return sendsignalfn(progname, sig);
}
#endif

const char *xbasename(const char *file)
{
#ifndef WIN32
	const char *p = strrchr(file, '/');
#else
	const char *p = strrchr(file, '\\');
	const char *r = strrchr(file, '/');
	/* if not found, try '/' */
	if( r > p ) {
		p = r;
	}
#endif

	if (p == NULL)
		return file;
	return p + 1;
}

/* Based on https://www.gnu.org/software/libc/manual/html_node/Calculating-Elapsed-Time.html
 * modified for a syntax similar to difftime()
 */
double difftimeval(struct timeval x, struct timeval y)
{
	struct timeval	result;
	double	d;

	/* Code below assumes that tv_sec is signed (time_t),
	 * but tv_usec is not necessarily */
	/* Perform the carry for the later subtraction by updating y. */
	if (x.tv_usec < y.tv_usec) {
		intmax_t numsec = (y.tv_usec - x.tv_usec) / 1000000 + 1;
		y.tv_usec -= 1000000 * numsec;
		y.tv_sec += numsec;
	}

	if (x.tv_usec - y.tv_usec > 1000000) {
		intmax_t numsec = (x.tv_usec - y.tv_usec) / 1000000;
		y.tv_usec += 1000000 * numsec;
		y.tv_sec -= numsec;
	}

	/* Compute the time remaining to wait.
	 * tv_usec is certainly positive. */
	result.tv_sec = x.tv_sec - y.tv_sec;
	result.tv_usec = x.tv_usec - y.tv_usec;

	d = 0.000001 * result.tv_usec + result.tv_sec;
	return d;
}

#if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_MONOTONIC) && HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC
/* From https://github.com/systemd/systemd/blob/main/src/basic/time-util.c
 * and  https://github.com/systemd/systemd/blob/main/src/basic/time-util.h
 */
typedef uint64_t usec_t;
typedef uint64_t nsec_t;
#define PRI_NSEC PRIu64
#define PRI_USEC PRIu64

#define USEC_INFINITY ((usec_t) UINT64_MAX)
#define NSEC_INFINITY ((nsec_t) UINT64_MAX)

#define MSEC_PER_SEC  1000ULL
#define USEC_PER_SEC  ((usec_t) 1000000ULL)
#define USEC_PER_MSEC ((usec_t) 1000ULL)
#define NSEC_PER_SEC  ((nsec_t) 1000000000ULL)
#define NSEC_PER_MSEC ((nsec_t) 1000000ULL)
#define NSEC_PER_USEC ((nsec_t) 1000ULL)

# if defined(WITH_LIBSYSTEMD) && (WITH_LIBSYSTEMD) && !(defined(WITHOUT_LIBSYSTEMD) && (WITHOUT_LIBSYSTEMD)) && defined(HAVE_SD_NOTIFY) && (HAVE_SD_NOTIFY)
/* Limited to upsnotify() use-cases below, currently */
static usec_t timespec_load(const struct timespec *ts) {
	assert(ts);

	if (ts->tv_sec < 0 || ts->tv_nsec < 0)
		return USEC_INFINITY;

	if ((usec_t) ts->tv_sec > (UINT64_MAX - (ts->tv_nsec / NSEC_PER_USEC)) / USEC_PER_SEC)
		return USEC_INFINITY;

	return
		(usec_t) ts->tv_sec * USEC_PER_SEC +
		(usec_t) ts->tv_nsec / NSEC_PER_USEC;
}

/* Not used, currently -- maybe later */
/*
static nsec_t timespec_load_nsec(const struct timespec *ts) {
	assert(ts);

	if (ts->tv_sec < 0 || ts->tv_nsec < 0)
		return NSEC_INFINITY;

	if ((nsec_t) ts->tv_sec >= (UINT64_MAX - ts->tv_nsec) / NSEC_PER_SEC)
		return NSEC_INFINITY;

	return (nsec_t) ts->tv_sec * NSEC_PER_SEC + (nsec_t) ts->tv_nsec;
}
*/
# endif	/* WITH_LIBSYSTEMD && HAVE_SD_NOTIFY && !WITHOUT_LIBSYSTEMD */

double difftimespec(struct timespec x, struct timespec y)
{
	struct timespec	result;
	double	d;

	/* Code below assumes that tv_sec is signed (time_t),
	 * but tv_nsec is not necessarily */
	/* Perform the carry for the later subtraction by updating y. */
	if (x.tv_nsec < y.tv_nsec) {
		intmax_t numsec = (y.tv_nsec - x.tv_nsec) / 1000000000L + 1;
		y.tv_nsec -= 1000000000L * numsec;
		y.tv_sec += numsec;
	}

	if (x.tv_nsec - y.tv_nsec > 1000000) {
		intmax_t numsec = (x.tv_nsec - y.tv_nsec) / 1000000000L;
		y.tv_nsec += 1000000000L * numsec;
		y.tv_sec -= numsec;
	}

	/* Compute the time remaining to wait.
	 * tv_nsec is certainly positive. */
	result.tv_sec = x.tv_sec - y.tv_sec;
	result.tv_nsec = x.tv_nsec - y.tv_nsec;

	d = 0.000000001 * result.tv_nsec + result.tv_sec;
	return d;
}
#endif	/* HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC */

/* Send (daemon) state-change notifications to an
 * external service management framework such as systemd
 */
int upsnotify(upsnotify_state_t state, const char *fmt, ...)
{
	int ret = -127;
	va_list va;
	char	buf[LARGEBUF];
	char	msgbuf[LARGEBUF];
	size_t	msglen = 0;

#if defined(WITH_LIBSYSTEMD) && (WITH_LIBSYSTEMD) && !(defined(WITHOUT_LIBSYSTEMD) && (WITHOUT_LIBSYSTEMD))
# ifdef HAVE_SD_NOTIFY
#  if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_MONOTONIC) && HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC
	/* In current systemd, this is only used for RELOADING/READY after
	 * a reload action for Type=notify-reload; for more details see
	 * https://github.com/systemd/systemd/blob/main/src/core/service.c#L2618
	 */
	struct timespec monoclock_ts;
	int got_monoclock = clock_gettime(CLOCK_MONOTONIC, &monoclock_ts);
#  endif	/* HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC */
# endif	/* HAVE_SD_NOTIFY */
#endif	/* WITH_LIBSYSTEMD */

	/* Prepare the message (if any) as a string */
	msgbuf[0] = '\0';
	if (fmt) {
		va_start(va, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
		/* generic message... */
		ret = vsnprintf(msgbuf, sizeof(msgbuf), fmt, va);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
		va_end(va);

		if ((ret < 0) || (ret >= (int) sizeof(msgbuf))) {
			syslog(LOG_WARNING,
				"%s (%s:%d): vsnprintf needed more than %" PRIuSIZE " bytes: %d",
				__func__, __FILE__, __LINE__, sizeof(msgbuf), ret);
		} else {
			msglen = strlen(msgbuf);
		}
		/* Reset for actual notification processing below */
		ret = -127;
	}

#if defined(WITH_LIBSYSTEMD) && (WITH_LIBSYSTEMD)
# if defined(WITHOUT_LIBSYSTEMD) && (WITHOUT_LIBSYSTEMD)
	NUT_UNUSED_VARIABLE(buf);
	NUT_UNUSED_VARIABLE(msglen);
	if (!upsnotify_reported_disabled_systemd)
		upsdebugx(6, "%s: notify about state %i with libsystemd: "
		"skipped for libcommonclient build, "
		"will not spam more about it", __func__, state);
	upsnotify_reported_disabled_systemd = 1;
# else
	if (!getenv("NOTIFY_SOCKET")) {
		if (!upsnotify_reported_disabled_systemd)
			upsdebugx(6, "%s: notify about state %i with libsystemd: "
				"was requested, but not running as a service unit now, "
				"will not spam more about it",
				__func__, state);
		upsnotify_reported_disabled_systemd = 1;
	} else {
#  ifdef HAVE_SD_NOTIFY
		char monoclock_str[SMALLBUF];
		monoclock_str[0] = '\0';
#   if defined(HAVE_CLOCK_GETTIME) && defined(HAVE_CLOCK_MONOTONIC) && HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC
		if (got_monoclock == 0) {
			usec_t monots = timespec_load(&monoclock_ts);
			ret = snprintf(monoclock_str + 1, sizeof(monoclock_str) - 1, "MONOTONIC_USEC=%" PRI_USEC, monots);
			if ((ret < 0) || (ret >= (int) sizeof(monoclock_str) - 1)) {
				syslog(LOG_WARNING,
					"%s (%s:%d): snprintf needed more than %" PRIuSIZE " bytes: %d",
					__func__, __FILE__, __LINE__, sizeof(monoclock_str), ret);
				msglen = 0;
			} else {
				monoclock_str[0] = '\n';
			}
		}
#   endif	/* HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC */

#   if ! DEBUG_SYSTEMD_WATCHDOG
		if (state != NOTIFY_STATE_WATCHDOG || !upsnotify_reported_watchdog_systemd)
#   endif
			upsdebugx(6, "%s: notify about state %i with libsystemd: use sd_notify()", __func__, state);

		/* https://www.freedesktop.org/software/systemd/man/sd_notify.html */
		if (msglen) {
			ret = snprintf(buf, sizeof(buf), "STATUS=%s", msgbuf);
			if ((ret < 0) || (ret >= (int) sizeof(buf))) {
				syslog(LOG_WARNING,
					"%s (%s:%d): snprintf needed more than %" PRIuSIZE " bytes: %d",
					__func__, __FILE__, __LINE__, sizeof(buf), ret);
				msglen = 0;
			} else {
				msglen = (size_t)ret;
			}
		}

		switch (state) {
			case NOTIFY_STATE_READY:
				ret = snprintf(buf + msglen, sizeof(buf) - msglen,
					"%sREADY=1%s",
					msglen ? "\n" : "",
					monoclock_str);
				break;

			case NOTIFY_STATE_READY_WITH_PID:
				if (1) { /* scoping */
					char pidbuf[SMALLBUF];
					if (snprintf(pidbuf, sizeof(pidbuf), "%lu", (unsigned long) getpid())) {
						ret = snprintf(buf + msglen, sizeof(buf) - msglen,
							"%sREADY=1\n"
							"MAINPID=%s%s",
							msglen ? "\n" : "",
							pidbuf,
							monoclock_str);
						upsdebugx(6, "%s: notifying systemd about MAINPID=%s",
							__func__, pidbuf);
						/* https://github.com/systemd/systemd/issues/25961
						 * Reset the WATCHDOG_PID so we know this is the
						 * process we want to post pings from!
						 */
						unsetenv("WATCHDOG_PID");
						setenv("WATCHDOG_PID", pidbuf, 1);
					} else {
						upsdebugx(6, "%s: NOT notifying systemd about MAINPID, "
							"got an error stringifying it; processing as "
							"plain NOTIFY_STATE_READY",
							__func__);
						ret = snprintf(buf + msglen, sizeof(buf) - msglen,
							"%sREADY=1%s",
							msglen ? "\n" : "",
							monoclock_str);
						/* TODO: Maybe revise/drop this tweak if
						 * loggers other than systemd are used: */
						state = NOTIFY_STATE_READY;
					}
				}
				break;

			case NOTIFY_STATE_RELOADING:
				ret = snprintf(buf + msglen, sizeof(buf) - msglen, "%s%s%s",
					msglen ? "\n" : "",
					"RELOADING=1",
					monoclock_str);
				break;

			case NOTIFY_STATE_STOPPING:
				ret = snprintf(buf + msglen, sizeof(buf) - msglen, "%s%s",
					msglen ? "\n" : "",
					"STOPPING=1");
				break;

			case NOTIFY_STATE_STATUS:
				/* Only send a text message per "fmt" */
				if (!msglen) {
					upsdebugx(6, "%s: failed to notify about status: none provided", __func__);
					ret = -1;
				} else {
					ret = (int)msglen;
				}
				break;

			case NOTIFY_STATE_WATCHDOG:
				/* Ping the framework that we are still alive */
				if (1) {	/* scoping */
					int	postit = 0;

#   ifdef HAVE_SD_WATCHDOG_ENABLED
					uint64_t	to = 0;
					postit = sd_watchdog_enabled(0, &to);

					if (postit < 0) {
#    if ! DEBUG_SYSTEMD_WATCHDOG
						if (!upsnotify_reported_watchdog_systemd)
#    endif
							upsdebugx(6, "%s: sd_enabled_watchdog query failed: %s",
								__func__, strerror(postit));
					} else {
#    if ! DEBUG_SYSTEMD_WATCHDOG
						if (!upsnotify_reported_watchdog_systemd || postit > 0)
#    else
						if (postit > 0)
#    endif
							upsdebugx(6, "%s: sd_enabled_watchdog query returned: %d "
								"(%" PRIu64 "msec remain)",
								__func__, postit, to);
					}
#   endif

					if (postit < 1) {
						char *s = getenv("WATCHDOG_USEC");
#    if ! DEBUG_SYSTEMD_WATCHDOG
						if (!upsnotify_reported_watchdog_systemd)
#    endif
							upsdebugx(6, "%s: WATCHDOG_USEC=%s", __func__, s);
						if (s && *s) {
							long l = strtol(s, (char **)NULL, 10);
							if (l > 0) {
								pid_t wdpid = parsepid(getenv("WATCHDOG_PID"));
								if (wdpid == (pid_t)-1 || wdpid == getpid()) {
#    if ! DEBUG_SYSTEMD_WATCHDOG
									if (!upsnotify_reported_watchdog_systemd)
#    endif
										upsdebugx(6, "%s: can post: WATCHDOG_PID=%li",
											__func__, (long)wdpid);
									postit = 1;
								} else {
#    if ! DEBUG_SYSTEMD_WATCHDOG
									if (!upsnotify_reported_watchdog_systemd)
#    endif
										upsdebugx(6, "%s: watchdog is configured, "
											"but not for this process: "
											"WATCHDOG_PID=%li",
											__func__, (long)wdpid);
#    if DEBUG_SYSTEMD_WATCHDOG
									/* Just try to post - at worst, systemd
									 * NotifyAccess will prohibit the message.
									 * The envvar simply helps child processes
									 * know they should not spam the watchdog
									 * handler (usually only MAINPID should):
									 *   https://github.com/systemd/systemd/issues/25961#issuecomment-1373947907
									 */
									postit = 1;
#    else
									postit = 0;
#    endif
								}
							}
						}
					}

					if (postit > 0) {
						ret = snprintf(buf + msglen, sizeof(buf) - msglen, "%s%s",
							msglen ? "\n" : "",
							"WATCHDOG=1");
					} else if (postit == 0) {
#    if ! DEBUG_SYSTEMD_WATCHDOG
						if (!upsnotify_reported_watchdog_systemd)
#    endif
							upsdebugx(6, "%s: failed to tickle the watchdog: not enabled for this unit", __func__);
						ret = -126;
					}
				}
				break;

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
				if (!msglen) {
					upsdebugx(6, "%s: unknown state and no status message provided", __func__);
					ret = -1;
				} else {
					upsdebugx(6, "%s: unknown state but have a status message provided", __func__);
					ret = (int)msglen;
				}
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
		}

		if ((ret < 0) || (ret >= (int) sizeof(buf))) {
			/* Refusal to send the watchdog ping is not an error to report */
			if ( !(ret == -126 && (state == NOTIFY_STATE_WATCHDOG)) ) {
				syslog(LOG_WARNING,
					"%s (%s:%d): snprintf needed more than %" PRIuSIZE " bytes: %d",
					__func__, __FILE__, __LINE__, sizeof(buf), ret);
			}
			ret = -1;
		} else {
			upsdebugx(6, "%s: posting sd_notify: %s", __func__, buf);
			msglen = (size_t)ret;
			ret = sd_notify(0, buf);
			if (ret > 0 && state == NOTIFY_STATE_READY_WITH_PID) {
				/* Usually we begin the main loop just after this
				 * and post a watchdog message but systemd did not
				 * yet prepare to handle us */
				upsdebugx(6, "%s: wait for NOTIFY_STATE_READY_WITH_PID to be handled by systemd", __func__);
#   ifdef HAVE_SD_NOTIFY_BARRIER
				sd_notify_barrier(0, UINT64_MAX);
#   else
				usleep(3 * 1000000);
#   endif
			}
		}

#  else	/* not HAVE_SD_NOTIFY: */
		/* FIXME: Try to fork and call systemd-notify helper program */
		upsdebugx(6, "%s: notify about state %i with libsystemd: lacking sd_notify()", __func__, state);
		ret = -127;
#  endif	/* HAVE_SD_NOTIFY */
	}
# endif	/* if not WITHOUT_LIBSYSTEMD (explicit avoid) */
#else	/* not WITH_LIBSYSTEMD */
	NUT_UNUSED_VARIABLE(buf);
	NUT_UNUSED_VARIABLE(msglen);
#endif	/* WITH_LIBSYSTEMD */

	if (ret < 0
#if defined(WITH_LIBSYSTEMD) && (WITH_LIBSYSTEMD) && !(defined(WITHOUT_LIBSYSTEMD) && (WITHOUT_LIBSYSTEMD)) && (defined(HAVE_SD_NOTIFY) && HAVE_SD_NOTIFY)
# if ! DEBUG_SYSTEMD_WATCHDOG
	&& (!upsnotify_reported_watchdog_systemd || (state != NOTIFY_STATE_WATCHDOG))
# endif
#endif
	) {
		if (ret == -127) {
			if (!upsnotify_reported_disabled_notech)
				upsdebugx(6, "%s: failed to notify about state %i: no notification tech defined, will not spam more about it", __func__, state);
			upsnotify_reported_disabled_notech = 1;
		} else {
			upsdebugx(6, "%s: failed to notify about state %i", __func__, state);
		}
	}

#if defined(WITH_LIBSYSTEMD) && (WITH_LIBSYSTEMD)
# if ! DEBUG_SYSTEMD_WATCHDOG
	if (state == NOTIFY_STATE_WATCHDOG && !upsnotify_reported_watchdog_systemd) {
		upsdebugx(6, "%s: logged the systemd watchdog situation once, will not spam more about it", __func__);
		upsnotify_reported_watchdog_systemd = 1;
	}
# endif
#endif

	return ret;
}

void nut_report_config_flags(void)
{
	/* Roughly similar to upslogx() but without the buffer-size limits and
	 * timestamp/debug-level prefixes. Only printed if debug (any) is on.
	 * Depending on amount of configuration tunables involved by a particular
	 * build of NUT, the string can be quite long (over 1KB).
	 */
	const char *acinit_ver = NULL;
	/* Pass these as variables to avoid warning about never reaching one
	 * of compiled codepaths: */
	const char *compiler_ver = CC_VERSION;
	const char *config_flags = CONFIG_FLAGS;
	struct timeval		now;

	if (nut_debug_level < 1)
		return;

	/* Only report git revision if NUT_VERSION_MACRO in nut_version.h aka
	 * UPS_VERSION here is remarkably different from PACKAGE_VERSION from
	 * configure.ac AC_INIT() -- which may be e.g. "2.8.0.1" although some
	 * distros, especially embedders, tend to place their product IDs here).
	 * The macro may be that fixed version or refer to git source revision,
	 * as decided when generating nut_version.h (and if it was re-generated
	 * in case of rebuilds while developers are locally iterating -- this
	 * may be disabled for faster local iterations at a cost of a little lie).
	 */
	if (PACKAGE_VERSION && UPS_VERSION &&
		(strlen(UPS_VERSION) < 12 || !strstr(UPS_VERSION, PACKAGE_VERSION))
	) {
		/* If UPS_VERSION is too short (so likely a static string
		 * from configure.ac AC_INIT() -- although some distros,
		 * especially embedders, tend to place their product IDs here),
		 * or if PACKAGE_VERSION *is NOT* a substring of it: */
		acinit_ver = PACKAGE_VERSION;
	}

	/* NOTE: If changing wording here, keep in sync with configure.ac logic
	 * looking for CONFIG_FLAGS_DEPLOYED via "configured with flags:" string!
	 */

	gettimeofday(&now, NULL);

	if (upslog_start.tv_sec == 0) {
		upslog_start = now;
	}

	if (upslog_start.tv_usec > now.tv_usec) {
		now.tv_usec += 1000000;
		now.tv_sec -= 1;
	}

	if (xbit_test(upslog_flags, UPSLOG_STDERR)) {
		fprintf(stderr, "%4.0f.%06ld\t[D1] Network UPS Tools version %s%s%s%s%s%s%s %s%s\n",
			difftime(now.tv_sec, upslog_start.tv_sec),
			(long)(now.tv_usec - upslog_start.tv_usec),
			UPS_VERSION,
			(acinit_ver ? " (release/snapshot of " : ""),
			(acinit_ver ? acinit_ver : ""),
			(acinit_ver ? ")" : ""),
			(compiler_ver && *compiler_ver != '\0' ? " built with " : ""),
			(compiler_ver && *compiler_ver != '\0' ? compiler_ver : ""),
			(compiler_ver && *compiler_ver != '\0' ? " and" : ""),
			(config_flags && *config_flags != '\0' ? "configured with flags: " : "configured all by default guesswork"),
			(config_flags && *config_flags != '\0' ? config_flags : "")
		);
#ifdef WIN32
		fflush(stderr);
#endif
	}

	/* NOTE: May be ignored or truncated by receiver if that syslog server
	 * (and/or OS sender) does not accept messages of such length */
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG))
		syslog(LOG_DEBUG, "Network UPS Tools version %s%s%s%s%s%s%s %s%s",
			UPS_VERSION,
			(acinit_ver ? " (release/snapshot of " : ""),
			(acinit_ver ? acinit_ver : ""),
			(acinit_ver ? ")" : ""),
			(compiler_ver && *compiler_ver != '\0' ? " built with " : ""),
			(compiler_ver && *compiler_ver != '\0' ? compiler_ver : ""),
			(compiler_ver && *compiler_ver != '\0' ? " and" : ""),
			(config_flags && *config_flags != '\0' ? "configured with flags: " : "configured all by default guesswork"),
			(config_flags && *config_flags != '\0' ? config_flags : "")
		);
}

static void vupslog(int priority, const char *fmt, va_list va, int use_strerror)
{
	int	ret;
	char	buf[LARGEBUF];

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#pragma clang diagnostic ignored "-Wformat-security"
#endif
	ret = vsnprintf(buf, sizeof(buf), fmt, va);
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	if ((ret < 0) || (ret >= (int) sizeof(buf)))
		syslog(LOG_WARNING, "vupslog: vsnprintf needed more than %d bytes",
			LARGEBUF);

	if (use_strerror) {
		snprintfcat(buf, sizeof(buf), ": %s", strerror(errno));

#ifdef WIN32
		LPVOID WinBuf;
		DWORD WinErr = GetLastError();
		FormatMessage(
				FORMAT_MESSAGE_MAX_WIDTH_MASK |
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				WinErr,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPTSTR) &WinBuf,
				0, NULL );

		snprintfcat(buf, sizeof(buf), " [%s]", (char *)WinBuf);
		LocalFree(WinBuf);
#endif
	}

	/* Note: nowadays debug level can be changed during run-time,
	 * so mark the starting point whenever we first try to log */
	if (upslog_start.tv_sec == 0) {
		struct timeval		now;
		gettimeofday(&now, NULL);
		upslog_start = now;
	}

	if (xbit_test(upslog_flags, UPSLOG_STDERR)) {
		if (nut_debug_level > 0) {
			struct timeval		now;

			gettimeofday(&now, NULL);

			if (upslog_start.tv_usec > now.tv_usec) {
				now.tv_usec += 1000000;
				now.tv_sec -= 1;
			}

			fprintf(stderr, "%4.0f.%06ld\t",
				difftime(now.tv_sec, upslog_start.tv_sec),
				(long)(now.tv_usec - upslog_start.tv_usec));
		}
		fprintf(stderr, "%s\n", buf);
#ifdef WIN32
		fflush(stderr);
#endif
	}
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG))
		syslog(priority, "%s", buf);
}


/* Return the default path for the directory containing configuration files */
const char * confpath(void)
{
	const char *path = getenv("NUT_CONFPATH");

#ifdef WIN32
	if (path == NULL) {
		/* fall back to built-in pathname relative to binary/workdir */
		path = getfullpath(PATH_ETC);
	}
#endif

	/* We assume, here and elsewhere, that
	 * at least CONFPATH is always defined */
	return (path != NULL && *path != '\0') ? path : CONFPATH;
}

/* Return the default path for the directory containing state files */
const char * dflt_statepath(void)
{
	const char *path = getenv("NUT_STATEPATH");

#ifdef WIN32
	if (path == NULL) {
		/* fall back to built-in pathname relative to binary/workdir */
		path = getfullpath(PATH_VAR_RUN);
	}
#endif

	/* We assume, here and elsewhere, that
	 * at least STATEPATH is always defined */
	return (path != NULL && *path != '\0') ? path : STATEPATH;
}

/* Return the alternate path for pid files, for processes running as non-root
 * Per documentation and configure script, the fallback value is the
 * state-file path as the daemon and drivers can write there too.
 * Note that this differs from PIDPATH that higher-privileged daemons, such
 * as upsmon, tend to use.
 */
const char * altpidpath(void)
{
	const char * path;

	path = getenv("NUT_ALTPIDPATH");
	if ( (path == NULL) || (*path == '\0') ) {
		path = getenv("NUT_STATEPATH");

#ifdef WIN32
		if (path == NULL) {
			/* fall back to built-in pathname relative to binary/workdir */
			path = getfullpath(PATH_VAR_RUN);
		}
#endif
	}

	if ( (path != NULL) && (*path != '\0') )
		return path;

#ifdef ALTPIDPATH
	return ALTPIDPATH;
#else
	/* With WIN32 in the loop, this may be more than a fallback to STATEPATH: */
	return dflt_statepath();
#endif
}

/* Die with a standard message if socket filename is too long */
void check_unix_socket_filename(const char *fn) {
	size_t len = strlen(fn);
#ifdef UNIX_PATH_MAX
	size_t max = UNIX_PATH_MAX;
#else
	size_t max = PATH_MAX;
#endif
#ifndef WIN32
	struct sockaddr_un	ssaddr;
	max = sizeof(ssaddr.sun_path);
#endif

	if (len < max)
		return;

	/* Avoid useless truncated pathnames that
	 * other driver instances would conflict
	 * with, and upsd can not discover.
	 * Note this is quite short on many OSes
	 * varying 104-108 bytes (UNIX_PATH_MAX)
	 * as opposed to PATH_MAX or MAXPATHLEN
	 * typically of a kilobyte range.
	 */
	fatalx(EXIT_FAILURE,
		"Can't create a unix domain socket: pathname '%s' "
		"is too long (%" PRIuSIZE ") for 'struct sockaddr_un->sun_path' "
		"on this system (%" PRIuSIZE ")",
		fn, len, max);
}

/* logs the formatted string to any configured logging devices + the output of strerror(errno) */
void upslog_with_errno(int priority, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	vupslog(priority, fmt, va, 1);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(va);
}

/* logs the formatted string to any configured logging devices */
void upslogx(int priority, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	vupslog(priority, fmt, va, 0);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(va);
}

void s_upsdebug_with_errno(int level, const char *fmt, ...)
{
	va_list va;
	char fmt2[LARGEBUF];

	/* Note: Thanks to macro wrapping, we do not quite need this
	 * test now, but we still need the "level" value to report
	 * below - when it is not zero.
	 */
	if (nut_debug_level < level)
		return;

/* For debugging output, we want to prepend the debug level so the user can
 * e.g. lower the level (less -D's on command line) to retain just the amount
 * of logging info he needs to see at the moment. Using '-DDDDD' all the time
 * is too brutal and needed high-level overview can be lost. This [D#] prefix
 * can help limit this debug stream quicker, than experimentally picking ;) */
	if (level > 0) {
		int ret;
		ret = snprintf(fmt2, sizeof(fmt2), "[D%d] %s", level, fmt);
		if ((ret < 0) || (ret >= (int) sizeof(fmt2))) {
			syslog(LOG_WARNING, "upsdebug_with_errno: snprintf needed more than %d bytes",
				LARGEBUF);
		} else {
			fmt = (const char *)fmt2;
		}
	}

	va_start(va, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	vupslog(LOG_DEBUG, fmt, va, 1);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(va);
}

void s_upsdebugx(int level, const char *fmt, ...)
{
	va_list va;
	char fmt2[LARGEBUF];

	if (nut_debug_level < level)
		return;

/* See comments above in upsdebug_with_errno() - they apply here too. */
	if (level > 0) {
		int ret;
		ret = snprintf(fmt2, sizeof(fmt2), "[D%d] %s", level, fmt);
		if ((ret < 0) || (ret >= (int) sizeof(fmt2))) {
			syslog(LOG_WARNING, "upsdebugx: snprintf needed more than %d bytes",
				LARGEBUF);
		} else {
			fmt = (const char *)fmt2;
		}
	}

	va_start(va, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	vupslog(LOG_DEBUG, fmt, va, 0);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(va);
}

/* dump message msg and len bytes from buf to upsdebugx(level) in
   hexadecimal. (This function replaces Philippe Marzouk's original
   dump_hex() function) */
void s_upsdebug_hex(int level, const char *msg, const void *buf, size_t len)
{
	char line[100];
	int n;	/* number of characters currently in line */
	size_t i;	/* number of bytes output from buffer */

	n = snprintf(line, sizeof(line), "%s: (%" PRIuSIZE " bytes) =>", msg, len);
	if (n < 0) goto failed;

	for (i = 0; i < len; i++) {

		if (n > 72) {
			upsdebugx(level, "%s", line);
			line[0] = 0;
		}

		n = snprintfcat(line, sizeof(line), n ? " %02x" : "%02x",
			((const unsigned char *)buf)[i]);

		if (n < 0) goto failed;
	}

	s_upsdebugx(level, "%s", line);
	return;

failed:
	s_upsdebugx(level, "%s", "Failed to print a hex dump for debug");
}

/* taken from www.asciitable.com */
static const char* ascii_symb[] = {
	"NUL",  /*  0x00    */
	"SOH",  /*  0x01    */
	"STX",  /*  0x02    */
	"ETX",  /*  0x03    */
	"EOT",  /*  0x04    */
	"ENQ",  /*  0x05    */
	"ACK",  /*  0x06    */
	"BEL",  /*  0x07    */
	"BS",   /*  0x08    */
	"TAB",  /*  0x09    */
	"LF",   /*  0x0A    */
	"VT",   /*  0x0B    */
	"FF",   /*  0x0C    */
	"CR",   /*  0x0D    */
	"SO",   /*  0x0E    */
	"SI",   /*  0x0F    */
	"DLE",  /*  0x10    */
	"DC1",  /*  0x11    */
	"DC2",  /*  0x12    */
	"DC3",  /*  0x13    */
	"DC4",  /*  0x14    */
	"NAK",  /*  0x15    */
	"SYN",  /*  0x16    */
	"ETB",  /*  0x17    */
	"CAN",  /*  0x18    */
	"EM",   /*  0x19    */
	"SUB",  /*  0x1A    */
	"ESC",  /*  0x1B    */
	"FS",   /*  0x1C    */
	"GS",   /*  0x1D    */
	"RS",   /*  0x1E    */
	"US"    /*  0x1F    */
};

/* dump message msg and len bytes from buf to upsdebugx(level) in ascii. */
void s_upsdebug_ascii(int level, const char *msg, const void *buf, size_t len)
{
	char line[256];
	int n;	/* number of characters currently in line */
	size_t i;	/* number of bytes output from buffer */
	unsigned char ch;

	if (nut_debug_level < level)
		return;	/* save cpu cycles */

	n = snprintf(line, sizeof(line), "%s", msg);
	if (n < 0) goto failed;

	for (i=0; i<len; ++i) {
		ch = ((const unsigned char *)buf)[i];

		if (ch < 0x20)
			n = snprintfcat(line, sizeof(line), "%3s ", ascii_symb[ch]);
		else if (ch >= 0x80)
			n = snprintfcat(line, sizeof(line), "%02Xh ", ch);
		else
			n = snprintfcat(line, sizeof(line), "'%c' ", ch);

		if (n < 0) goto failed;
	}

	s_upsdebugx(level, "%s", line);
	return;

failed:
	s_upsdebugx(level, "%s", "Failed to print an ASCII data dump for debug");
}

static void vfatal(const char *fmt, va_list va, int use_strerror)
{
	if (xbit_test(upslog_flags, UPSLOG_STDERR_ON_FATAL))
		xbit_set(&upslog_flags, UPSLOG_STDERR);
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG_ON_FATAL))
		xbit_set(&upslog_flags, UPSLOG_SYSLOG);

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	vupslog(LOG_ERR, fmt, va, use_strerror);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
}

void fatal_with_errno(int status, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	vfatal(fmt, va, (errno > 0) ? 1 : 0);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(va);

	exit(status);
}

void fatalx(int status, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	vfatal(fmt, va, 0);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(va);

	exit(status);
}

static const char *oom_msg = "Out of memory";

void *xmalloc(size_t size)
{
	void *p = malloc(size);

	if (p == NULL)
		fatal_with_errno(EXIT_FAILURE, "%s", oom_msg);

#ifdef WIN32
	/* FIXME: This is what (x)calloc() is for! */
	memset(p, 0, size);
#endif

	return p;
}

void *xcalloc(size_t number, size_t size)
{
	void *p = calloc(number, size);

	if (p == NULL)
		fatal_with_errno(EXIT_FAILURE, "%s", oom_msg);

#ifdef WIN32
	/* FIXME: calloc() above should have initialized this already! */
	memset(p, 0, size * number);
#endif

	return p;
}

void *xrealloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);

	if (p == NULL)
		fatal_with_errno(EXIT_FAILURE, "%s", oom_msg);
	return p;
}

char *xstrdup(const char *string)
{
	char *p;

	if (string == NULL) {
		upsdebugx(1, "%s: got null input", __func__);
		return NULL;
	}

	p = strdup(string);

	if (p == NULL)
		fatal_with_errno(EXIT_FAILURE, "%s", oom_msg);
	return p;
}

/* Read up to buflen bytes from fd and return the number of bytes
   read. If no data is available within d_sec + d_usec, return 0.
   On error, a value < 0 is returned (errno indicates error). */
#ifndef WIN32
ssize_t select_read(const int fd, void *buf, const size_t buflen, const time_t d_sec, const suseconds_t d_usec)
{
	int		ret;
	fd_set		fds;
	struct timeval	tv;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tv.tv_sec = d_sec;
	tv.tv_usec = d_usec;

	ret = select(fd + 1, &fds, NULL, NULL, &tv);

	if (ret < 1) {
		return ret;
	}

	return read(fd, buf, buflen);
}
#else
ssize_t select_read(serial_handler_t *fd, void *buf, const size_t buflen, const time_t d_sec, const suseconds_t d_usec)
{
	/* This function is only called by serial drivers right now */
	/* TODO: Assert below that resulting values fit in ssize_t range */
	/* DWORD bytes_read; */
	int res;
	DWORD timeout;
	COMMTIMEOUTS TOut;

	timeout = (d_sec*1000) + ((d_usec+999)/1000);

	GetCommTimeouts(fd->handle,&TOut);
	TOut.ReadIntervalTimeout = MAXDWORD;
	TOut.ReadTotalTimeoutMultiplier = 0;
	TOut.ReadTotalTimeoutConstant = timeout;
	SetCommTimeouts(fd->handle,&TOut);

	res = w32_serial_read(fd,buf,buflen,timeout);

	return res;
}
#endif

/* Write up to buflen bytes to fd and return the number of bytes
   written. If no data is available within d_sec + d_usec, return 0.
   On error, a value < 0 is returned (errno indicates error). */
#ifndef WIN32
ssize_t select_write(const int fd, const void *buf, const size_t buflen, const time_t d_sec, const suseconds_t d_usec)
{
	int		ret;
	fd_set		fds;
	struct timeval	tv;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	tv.tv_sec = d_sec;
	tv.tv_usec = d_usec;

	ret = select(fd + 1, NULL, &fds, NULL, &tv);

	if (ret < 1) {
		return ret;
	}

	return write(fd, buf, buflen);
}
#else
/* Note: currently not implemented de-facto for Win32 */
ssize_t select_write(serial_handler_t *fd, const void *buf, const size_t buflen, const time_t d_sec, const suseconds_t d_usec)
{
	NUT_UNUSED_VARIABLE(fd);
	NUT_UNUSED_VARIABLE(buf);
	NUT_UNUSED_VARIABLE(buflen);
	NUT_UNUSED_VARIABLE(d_sec);
	NUT_UNUSED_VARIABLE(d_usec);
	upsdebugx(1, "WARNING: method %s() is not implemented yet for WIN32", __func__);
	return 0;
}
#endif

/* FIXME: would be good to get more from /etc/ld.so.conf[.d] and/or
 * LD_LIBRARY_PATH and a smarter dependency on build bitness; also
 * note that different OSes can have their pathnames set up differently
 * with regard to default/preferred bitness (maybe a "32" in the name
 * should also be searched explicitly - again, IFF our build is 32-bit).
 *
 * General premise for this solution is that some parts of NUT (e.g. the
 * nut-scanner tool, or DMF feature code) must be pre-built and distributed
 * in binary packages, but only at run-time it gets to know which third-party
 * libraries it should use for particular operations. This differs from e.g.
 * distribution packages which group NUT driver binaries explicitly dynamically
 * linked against certain OS-provided libraries for accessing this or that
 * communications media and/or vendor protocol.
 */
static const char * search_paths[] = {
	/* Use the library path (and bitness) provided during ./configure first */
	LIBDIR,
	"/usr"LIBDIR,
	"/usr/local"LIBDIR,
#ifdef BUILD_64
	/* Fall back to explicit preference of 64-bit paths as named on some OSes */
	"/usr/lib/64",
	"/usr/lib64",
#endif
	"/usr/lib",
#ifdef BUILD_64
	"/lib/64",
	"/lib64",
#endif
	"/lib",
#ifdef BUILD_64
	"/usr/local/lib/64",
	"/usr/local/lib64",
#endif
	"/usr/local/lib",
#ifdef AUTOTOOLS_TARGET_SHORT_ALIAS
	"/usr/lib/" AUTOTOOLS_TARGET_SHORT_ALIAS,
	"/usr/lib/gcc/" AUTOTOOLS_TARGET_SHORT_ALIAS,
#else
# ifdef AUTOTOOLS_HOST_SHORT_ALIAS
	"/usr/lib/" AUTOTOOLS_HOST_SHORT_ALIAS,
	"/usr/lib/gcc/" AUTOTOOLS_HOST_SHORT_ALIAS,
# else
#  ifdef AUTOTOOLS_BUILD_SHORT_ALIAS
	"/usr/lib/" AUTOTOOLS_BUILD_SHORT_ALIAS,
	"/usr/lib/gcc/" AUTOTOOLS_BUILD_SHORT_ALIAS,
#  endif
# endif
#endif
#ifdef AUTOTOOLS_TARGET_ALIAS
	"/usr/lib/" AUTOTOOLS_TARGET_ALIAS,
	"/usr/lib/gcc/" AUTOTOOLS_TARGET_ALIAS,
#else
# ifdef AUTOTOOLS_HOST_ALIAS
	"/usr/lib/" AUTOTOOLS_HOST_ALIAS,
	"/usr/lib/gcc/" AUTOTOOLS_HOST_ALIAS,
# else
#  ifdef AUTOTOOLS_BUILD_ALIAS
	"/usr/lib/" AUTOTOOLS_BUILD_ALIAS,
	"/usr/lib/gcc/" AUTOTOOLS_BUILD_ALIAS,
#  endif
# endif
#endif
#ifdef WIN32
	/* TODO: Track the binary program name (many platform-specific solutions,
	 * or custom one to stash argv[0] in select programs, and derive its
	 * dirname (with realpath and apparent path) as well as "../lib".
	 * Perhaps a decent fallback idea for all platforms, not just WIN32.
	 */
	".",
#endif
	NULL
};

static char * get_libname_in_dir(const char* base_libname, size_t base_libname_length, const char* dirname, int index) {
	/* Implementation detail for get_libname() below.
	 * Returns pointer to allocated copy of the buffer
	 * (caller must free later) if dir has lib,
	 * or NULL otherwise.
	 * base_libname_length is optimization to not recalculate length in a loop.
	 * index is for search_paths[] table looping; use negative to not log dir number
	 */
	DIR *dp;
	struct dirent *dirp;
	char *libname_path = NULL;
	char current_test_path[LARGEBUF];

	memset(current_test_path, 0, LARGEBUF);

	if ((dp = opendir(dirname)) == NULL) {
		if (index >= 0) {
			upsdebugx(5,"NOT looking for lib %s in unreachable directory #%d : %s",
				base_libname, index, dirname);
		} else {
			upsdebugx(5,"NOT looking for lib %s in unreachable directory : %s",
				base_libname, dirname);
		}
		return NULL;
	}

	if (index >= 0) {
		upsdebugx(2,"Looking for lib %s in directory #%d : %s", base_libname, index, dirname);
	} else {
		upsdebugx(2,"Looking for lib %s in directory : %s", base_libname, dirname);
	}
	while ((dirp = readdir(dp)) != NULL)
	{
#if !HAVE_DECL_REALPATH
		struct stat	st;
#endif

		upsdebugx(5,"Comparing lib %s with dirpath entry %s", base_libname, dirp->d_name);
		int compres = strncmp(dirp->d_name, base_libname, base_libname_length);
		if (compres == 0
		&&  dirp->d_name[base_libname_length] == '\0' /* avoid "*.dll.a" etc. */
		) {
			snprintf(current_test_path, LARGEBUF, "%s/%s", dirname, dirp->d_name);
#if HAVE_DECL_REALPATH
			libname_path = realpath(current_test_path, NULL);
#else
			/* Just check if candidate name is (points to?) valid file */
			libname_path = NULL;
			if (stat(current_test_path, &st) == 0) {
				if (st.st_size > 0) {
					libname_path = xstrdup(current_test_path);
				}
			}

# ifdef WIN32
			if (!libname_path) {
				for (char *p = current_test_path; *p != '\0' && (p - current_test_path) < LARGEBUF; p++) {
					if (*p == '/') *p = '\\';
				}
				upsdebugx(3, "%s: WIN32: re-checking with %s", __func__, current_test_path);
				if (stat(current_test_path, &st) == 0) {
					if (st.st_size > 0) {
						libname_path = xstrdup(current_test_path);
					}
				}
			}
			if (!libname_path && strcmp(dirname, ".") == 0 && current_test_path[0] == '.' && current_test_path[1] == '\\' && current_test_path[2] != '\0') {
				/* Seems mingw stat() only works for files in current dir,
				 * so for others a chdir() is needed (and memorizing the
				 * original dir, and no threading at this moment, to be safe!)
				 * https://stackoverflow.com/a/66096983/4715872
				 */
				upsdebugx(3, "%s: WIN32: re-checking with %s", __func__, current_test_path + 2);
				if (stat(current_test_path + 2, &st) == 0) {
					if (st.st_size > 0) {
						libname_path = xstrdup(current_test_path + 2);
					}
				}
			}
# endif /* WIN32 */
#endif  /* HAVE_DECL_REALPATH */

			upsdebugx(2,"Candidate path for lib %s is %s (realpath %s)",
				base_libname, current_test_path,
				(libname_path!=NULL)?libname_path:"NULL");
			if (libname_path != NULL)
				break;
		}
	} /* while iterating dir */

	closedir(dp);

	return libname_path;
}

static char * get_libname_in_pathset(const char* base_libname, size_t base_libname_length, char* pathset, int *counter)
{
	/* Note: this method iterates specified pathset,
	 * so increments the counter by reference */
	char *libname_path = NULL;
	char *onedir = NULL;

	if (!pathset || *pathset == '\0')
		return NULL;

	/* First call to tokenization passes the string, others pass NULL */
	while (NULL != (onedir = strtok( (onedir ? NULL : pathset), ":" ))) {
		libname_path = get_libname_in_dir(base_libname, base_libname_length, onedir, *counter++);
		if (libname_path != NULL)
			break;
	}

#ifdef WIN32
	/* Note: with mingw, the ":" separator above might have been resolvable */
	if (!libname_path) {
		onedir = NULL; /* probably is NULL already, but better ensure this */
		while (NULL != (onedir = strtok( (onedir ? NULL : pathset), ";" ))) {
			libname_path = get_libname_in_dir(base_libname, base_libname_length, onedir, *counter++);
			if (libname_path != NULL)
				break;
		}
	}
#endif  /* WIN32 */

	return libname_path;
}

char * get_libname(const char* base_libname)
{
	int index = 0, counter = 0;
	char *libname_path = NULL;
	size_t base_libname_length = strlen(base_libname);

	/* Normally these envvars should not be set, but if the user insists,
	 * we should prefer the override... */
#ifdef BUILD_64
	libname_path = get_libname_in_pathset(base_libname, base_libname_length, getenv("LD_LIBRARY_PATH_64"), &counter);
	if (libname_path != NULL) {
		upsdebugx(2, "Looking for lib %s, found in LD_LIBRARY_PATH_64", base_libname);
		goto found;
	}
#else
	libname_path = get_libname_in_pathset(base_libname, base_libname_length, getenv("LD_LIBRARY_PATH_32"), &counter);
	if (libname_path != NULL) {
		upsdebugx(2, "Looking for lib %s, found in LD_LIBRARY_PATH_32", base_libname);
		goto found;
	}
#endif

	libname_path = get_libname_in_pathset(base_libname, base_libname_length, getenv("LD_LIBRARY_PATH"), &counter);
	if (libname_path != NULL) {
		upsdebugx(2, "Looking for lib %s, found in LD_LIBRARY_PATH", base_libname);
		goto found;
	}

	for (index = 0 ; (search_paths[index] != NULL) && (libname_path == NULL) ; index++)
	{
		libname_path = get_libname_in_dir(base_libname, base_libname_length, search_paths[index], counter++);
		if (libname_path != NULL)
			break;
	}

#ifdef WIN32
	/* TODO: Need a reliable cross-platform way to get the full path
	 * of current executable -- possibly stash it when starting NUT
	 * programs... consider some way for `nut-scanner` too */
	if (!libname_path) {
		/* First check near the EXE (if executing it from another
		 * working directory) */
		libname_path = get_libname_in_dir(base_libname, base_libname_length, getfullpath(NULL), counter++);
	}

# ifdef PATH_LIB
	if (!libname_path) {
		libname_path = get_libname_in_dir(base_libname, base_libname_length, getfullpath(PATH_LIB), counter++);
	}
# endif

	if (!libname_path) {
		/* Resolve "lib" dir near the one with current executable ("bin" or "sbin") */
		libname_path = get_libname_in_dir(base_libname, base_libname_length, getfullpath("/../lib"), counter++);
	}
#endif  /* WIN32 so far */

#ifdef WIN32
	/* Windows-specific: DLLs can be provided by common "PATH" envvar,
	 * at lowest search priority though (after EXE dir, system, etc.) */
	if (!libname_path) {
		upsdebugx(2, "Looking for lib %s in PATH", base_libname);
		libname_path = get_libname_in_pathset(base_libname, base_libname_length, getenv("PATH"), &counter);
	}
#endif  /* WIN32 */

found:
	upsdebugx(1,"Looking for lib %s, found %s", base_libname, (libname_path!=NULL)?libname_path:"NULL");
	return libname_path;
}

/* TODO: Extend for TYPE_FD and WIN32 eventually? */
void set_close_on_exec(int fd) {
	/* prevent fd leaking to child processes */
#ifndef FD_CLOEXEC
	/* Find a way, if possible at all old platforms */
	NUT_UNUSED_VARIABLE(fd);
#else
# ifdef WIN32
	/* Find a way, if possible at all (WIN32: get INT fd from the HANDLE?) */
	NUT_UNUSED_VARIABLE(fd);
# else
	fcntl(fd, F_SETFD, FD_CLOEXEC);
# endif
#endif
}
