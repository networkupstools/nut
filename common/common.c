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
#if !HAVE_REALPATH
# include <sys/stat.h>
#endif

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
#if UINTPTR_MAX == 0xffffffffffffffffULL
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

	close(0);
	close(1);
	close(2);

	if (pid != 0)
		_exit(EXIT_SUCCESS);		/* parent */

	/* child */

	/* make fds 0-2 point somewhere defined */
	if (open("/dev/null", O_RDWR) != 0)
		fatal_with_errno(EXIT_FAILURE, "open /dev/null");

	if (dup(0) == -1)
		fatal_with_errno(EXIT_FAILURE, "dup");

	if (dup(0) == -1)
		fatal_with_errno(EXIT_FAILURE, "dup");

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
		fatalx(EXIT_FAILURE, "user %s not found", name);
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
	if ((geteuid() != 0) && (getuid() != 0)) {
		upsdebugx(1, "Can not become_user(%s): not root initially, "
			"remaining UID=%jd GID=%jd",
			pw->pw_name, (intmax_t)getuid(), (intmax_t)getgid());
		return;
	}

	if (getuid() == 0)
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
	NUT_UNUSED_VARIABLE(pw);

	upsdebugx(1, "Can not become_user(%s): not implemented on this platform", pw->pw_name);
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

	/* assuming 10 digits for a long */
	intmax_t _pid = strtol(buf, (char **)NULL, 10);
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

	if (nut_debug_level > 0) {
		static struct timeval	start = { 0, 0 };
		struct timeval		now;

		gettimeofday(&now, NULL);

		if (start.tv_sec == 0) {
			start = now;
		}

		if (start.tv_usec > now.tv_usec) {
			now.tv_usec += 1000000;
			now.tv_sec -= 1;
		}

		fprintf(stderr, "%4.0f.%06ld\t", difftime(now.tv_sec, start.tv_sec), (long)(now.tv_usec - start.tv_usec));
	}

	if (xbit_test(upslog_flags, UPSLOG_STDERR))
		fprintf(stderr, "%s\n", buf);
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
	char *p = strdup(string);

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
#if !HAVE_REALPATH
		struct stat	st;
#endif

		upsdebugx(5,"Comparing lib %s with dirpath entry %s", base_libname, dirp->d_name);
		int compres = strncmp(dirp->d_name, base_libname, base_libname_length);
		if (compres == 0
		&&  dirp->d_name[base_libname_length] == '\0' /* avoid "*.dll.a" etc. */
		) {
			snprintf(current_test_path, LARGEBUF, "%s/%s", dirname, dirp->d_name);
#if HAVE_REALPATH
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
#endif  /* HAVE_REALPATH */

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
# ifdef PATH_LIB
	if (!libname_path) {
		libname_path = get_libname_in_dir(base_libname, base_libname_length, getfullpath(PATH_LIB), counter++);
	}
# endif

	if (!libname_path) {
		/* Resolve "lib" dir near the one with current executable ("bin" or "sbin") */
		libname_path = get_libname_in_dir(base_libname, base_libname_length, getfullpath("../lib"), counter++);
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
