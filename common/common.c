/* common.c - common useful functions

   Copyright (C) 2000  Russell Kroll <rkroll@exploits.org>
   Copyright (C) 2021-2025  Jim Klimov <jimklimov+nut@gmail.com>

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
# include <syslog.h>
# include <errno.h>
# include <pwd.h>
# include <grp.h>
# include <sys/un.h>
#else
# include <wincompat.h>
# include <processthreadsapi.h>
# include <psapi.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>	/* readlink */
#endif

#include <dirent.h>
#if !HAVE_DECL_REALPATH
# include <sys/stat.h>
#endif

#if (defined WITH_LIBSYSTEMD_INHIBITOR) && (defined WITH_LIBSYSTEMD && WITH_LIBSYSTEMD) && (defined WITH_LIBSYSTEMD_INHIBITOR && WITH_LIBSYSTEMD_INHIBITOR) && !(defined(WITHOUT_LIBSYSTEMD) && (WITHOUT_LIBSYSTEMD))
#  ifdef HAVE_SYSTEMD_SD_BUS_H
#   include <systemd/sd-bus.h>
#  endif
/* Code below is inspired by https://systemd.io/INHIBITOR_LOCKS/ docs, and
 * https://github.com/systemd/systemd/issues/34004 discussion which pointed
 * to https://github.com/systemd/systemd/blob/main/src/login/inhibit.c tool
 * and https://github.com/systemd/systemd/blob/main/src/basic/errno-util.h etc.
 * and https://www.freedesktop.org/software/systemd/man/latest/sd_bus_call_method.html
 */
static int RET_NERRNO(int ret) {
	if (ret < 0) {
		if (errno > 0)
			return -EINVAL;
		return -errno;
	}

	return ret;
}

/* FIXME: Pedantically speaking, the attribute is assumed supported by GCC
 *  and CLANG; practically - not sure if we have platforms with sufficiently
 *  new libsystemd (its headers and example code also use this) and older or
 *  different compilers. This can be addressed a bit more clumsily directly,
 *  but we only want to do so if needed in real life. */
#define _cleanup_(f)	__attribute__((cleanup(f)))

/* The "bus_login_mgr" definition per
 * https://github.com/systemd/systemd/blob/4cf7a676af9a79ff418227d8ff488dfca6f243ab/src/shared/bus-locator.c#L24 */
#define SDBUS_DEST	"org.freedesktop.login1"
#define SDBUS_PATH	"/org/freedesktop/login1"
#define SDBUS_IFACE	"org.freedesktop.login1.Manager"

static /*_cleanup_(sd_bus_flush_close_unrefp)*/ sd_bus	*systemd_bus = NULL;
static int	isSupported_Inhibit = -1, isSupported_Inhibit_errno = 0;
static int	isSupported_PreparingForSleep = -1, isSupported_PreparingForSleep_errno = 0;

static void close_sdbus_once(void) {
	/* Per https://manpages.debian.org/testing/libsystemd-dev/sd_bus_flush_close_unrefp.3.en.html
	 * these end-of-life methods do not tell us if we succeeded or failed
	 * closing the bus connection in any manner, so we here also do not.
	 */

	if (!systemd_bus) {
		errno = 0;
		return;
	}

	upsdebugx(1, "%s: trying", __func__);
	errno = 0;
	sd_bus_flush_close_unrefp(&systemd_bus);
	systemd_bus = NULL;
}

static int open_sdbus_once(const char *caller) {
	static int	openedOnce = 0, faultReported = 0;
	int	r = 1;

	errno = 0;
	if (systemd_bus)
		return r;

# if defined HAVE_SD_BUS_OPEN_SYSTEM_WITH_DESCRIPTION && HAVE_SD_BUS_OPEN_SYSTEM_WITH_DESCRIPTION
	r = sd_bus_open_system_with_description(&systemd_bus, "Bus connection for Network UPS Tools sleep/suspend/hibernate handling");
# else
	r = sd_bus_open_system(&systemd_bus);
# endif
	if (r < 0 || !systemd_bus) {
		if (r >= 0) {
			if (!faultReported)
				upsdebugx(1, "%s: Failed to acquire bus for %s(): "
					"got null pointer and %d exit-code; setting EINVAL",
					__func__, NUT_STRARG(caller), r);
			r = -EINVAL;
		} else {
			if (!faultReported)
				upsdebugx(1, "%s: Failed to acquire bus for %s() (%d): %s",
					__func__, NUT_STRARG(caller), r, strerror(-r));
		}
		faultReported = 1;
	} else {
		upsdebugx(1, "%s: succeeded for %s", __func__, NUT_STRARG(caller));
		faultReported = 0;
	}

	if (systemd_bus && !openedOnce) {
		openedOnce = 1;
		atexit(close_sdbus_once);
	}

	if (systemd_bus) {
# if !(defined HAVE_SD_BUS_OPEN_SYSTEM_WITH_DESCRIPTION && HAVE_SD_BUS_OPEN_SYSTEM_WITH_DESCRIPTION)
#  if defined HAVE_SD_BUS_SET_DESCRIPTION && HAVE_SD_BUS_SET_DESCRIPTION
		if (sd_bus_set_description(systemd_bus, "Bus connection for Network UPS Tools sleep/suspend/hibernate handling") < 0)
			upsdebugx(1, "%s: failed to sd_bus_set_description(), oh well", __func__);
#  endif
# endif

		/* second arg for (bool)arg_ask_password - 0 for the non-interactive daemon */
		sd_bus_set_allow_interactive_authorization(systemd_bus, 0);
	}

	return r;
}

static int would_reopen_sdbus(int r) {
	if (r >= 0)
		return 0;

	switch (-r) {
		/* Rule out issues that would not clear themselves (e.g. not stale connections) */
		case ENOENT:
		case EPERM:
		case EACCES:
			return 0;

		default:
			break;
	}

	return 1;
}

static int reopen_sdbus_once(int r, const char *caller, const char *purpose)
{
	if (r >= 0)
		return r;

	switch (-r) {
		/* Rule out issues that would not clear themselves (e.g. not stale connections) */
		case ENOENT:
		case EPERM:
		case EACCES:
			break;

		/* An "Invalid request descriptor" might fit this bill */
		default:
			upsdebugx(1, "%s for %s() for %s failed (%d) once, will retry D-Bus connection: %s",
				__func__, caller, purpose, r, strerror(-r));

			close_sdbus_once();
			r = open_sdbus_once(caller);
			if (r < 0) {
				/* Errors, if any, reported above */
				return r;
			}
			break;
	}

	return r;
}

int isInhibitSupported(void)
{
	return isSupported_Inhibit;
}

int isPreparingForSleepSupported(void)
{
	return isSupported_PreparingForSleep;
}

TYPE_FD Inhibit(const char *arg_what, const char *arg_who, const char *arg_why, const char *arg_mode)
{
	_cleanup_(sd_bus_error_free) sd_bus_error	error = SD_BUS_ERROR_NULL;
	_cleanup_(sd_bus_message_unrefp) sd_bus_message	*reply = NULL;
	int	r;
	TYPE_FD	fd = ERROR_FD;

	if (isSupported_Inhibit == 0) {
		/* Already determined that we can not use it, e.g. due to perms */
		errno = isSupported_Inhibit_errno;
		return -errno;
	}

	/* Not found in public headers:
	bool	arg_ask_password = true;
	(void) polkit_agent_open_if_enabled(BUS_TRANSPORT_LOCAL, arg_ask_password);
	 */

	r = open_sdbus_once(__func__);
	if (r < 0) {
		/* Errors, if any, reported above */
		return r;
	}

	r = sd_bus_call_method(systemd_bus, SDBUS_DEST, SDBUS_PATH, SDBUS_IFACE, "Inhibit", &error, &reply, "ssss", arg_what, arg_who, arg_why, arg_mode);
	if (r < 0) {
		if (would_reopen_sdbus(r)) {
			if ((r = reopen_sdbus_once(r, __func__, "sd_bus_call_method()")) < 0)
				return r;

			r = sd_bus_call_method(systemd_bus, SDBUS_DEST, SDBUS_PATH, SDBUS_IFACE, "Inhibit", &error, &reply, "ssss", arg_what, arg_who, arg_why, arg_mode);
		} else {
			/* Permissions for the privileged operation... did it ever succeed? */
			if (isSupported_Inhibit < 0) {
				upsdebugx(1, "%s: %s() failed seemingly due to permissions, marking %s as not supported",
					__func__, "sd_bus_call_method", "Inhibit");
				isSupported_Inhibit = 0;
				isSupported_Inhibit_errno = r;
			}
		}

		if (r < 0) {
			upsdebugx(1, "%s: %s() failed (%d): %s",
				__func__, "sd_bus_call_method", r, strerror(-r));
			if (error.message && *(error.message))
				upsdebugx(2, "%s: details from libsystemd: %s",
					__func__, error.message);
			return r;
		} else {
			upsdebugx(1, "%s: reconnection to D-Bus helped with %s()",
				__func__, "sd_bus_call_method");
		}
	}

	r = sd_bus_message_read_basic(reply, SD_BUS_TYPE_UNIX_FD, &fd);
	if (r < 0) {
		upsdebugx(1, "%s: %s() failed (%d): %s",
			__func__, "sd_bus_message_read_basic", r, strerror(-r));
		if (isSupported_Inhibit < 0 && !would_reopen_sdbus(r)) {
			upsdebugx(1, "%s: %s() failed seemingly due to permissions, marking %s as not supported",
				__func__, "sd_bus_message_read_basic", "Inhibit");
			isSupported_Inhibit = 0;
			isSupported_Inhibit_errno = r;
		}
		return r;
	}

	/* Data query succeeded, so it is supported */
	isSupported_Inhibit = 1;

	/* NOTE: F_DUPFD_CLOEXEC is in POSIX.1-2008 (Linux 2.6.24); seek out
	 * an alternative sequence of options if needed on older systems */
	r = RET_NERRNO(fcntl(fd, F_DUPFD_CLOEXEC, 3));
	if (r < 0) {
		upsdebugx(1, "%s: fcntl() failed (%d): %s",
			__func__, r, strerror(-r));
		return fd;
	}

	return r;
}

void Uninhibit(TYPE_FD *fd_ptr)
{
	if (!fd_ptr)
		return;
	if (INVALID_FD(*fd_ptr))
		return;

	/* Closing the socket allows systemd to proceed (we un-inhibit our lock on system
	 * life-cycle handling). After waking up, we should Inhibit() anew, if needed.
	 */
	close(*fd_ptr);
	*fd_ptr = ERROR_FD;
}

int isPreparingForSleep(void)
{
	static int32_t	prev = -1;
	int32_t	val = 0;	/* 4-byte int expected for SD_BUS_TYPE_BOOLEAN aka 'b' */
	int	r;
	_cleanup_(sd_bus_error_free) sd_bus_error	error = SD_BUS_ERROR_NULL;

	if (isSupported_PreparingForSleep == 0) {
		/* Already determined that we can not use it, e.g. due to perms */
		errno = isSupported_PreparingForSleep_errno;
		upsdebug_with_errno(8, "%s: isSupported_PreparingForSleep=%d",
			__func__, isSupported_PreparingForSleep);
		return -errno;
	}

	r = open_sdbus_once(__func__);
	if (r < 0) {
		/* Errors, if any, reported above */
		return r;
	}

	/* @org.freedesktop.DBus.Property.EmitsChangedSignal("false")
	 *     readonly b PreparingForSleep = ...;
	 * https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.login1.html
	 * https://www.freedesktop.org/software/systemd/man/latest/sd_bus_set_property.html
	 * https://www.freedesktop.org/software/systemd/man/latest/sd_bus_message_append.html (data types)
	 */
	r = sd_bus_get_property_trivial(systemd_bus, SDBUS_DEST, SDBUS_PATH, SDBUS_IFACE, "PreparingForSleep", &error, SD_BUS_TYPE_BOOLEAN, &val);
	if (r < 0) {
		if (would_reopen_sdbus(r)) {
			if ((r = reopen_sdbus_once(r, __func__, "sd_bus_get_property_trivial()")) < 0)
				return r;

			r = sd_bus_get_property_trivial(systemd_bus, SDBUS_DEST, SDBUS_PATH, SDBUS_IFACE, "PreparingForSleep", &error, 'b', &val);
		} else {
			if (isSupported_PreparingForSleep < 0) {
				upsdebugx(1, "%s: %s() failed seemingly due to permissions, marking %s as not supported",
					__func__, "sd_bus_get_property_trivial", "PreparingForSleep");
				isSupported_PreparingForSleep = 0;
				isSupported_PreparingForSleep_errno = r;
			}
		}

		if (r < 0) {
			upsdebugx(1, "%s: %s() failed (%d): %s",
				__func__, "sd_bus_get_property_trivial", r, strerror(-r));
			if (error.message && *(error.message))
				upsdebugx(2, "%s: details from libsystemd: %s",
					__func__, error.message);
			return r;
		} else {
			upsdebugx(1, "%s: reconnection to D-Bus helped with %s()",
				__func__, "sd_bus_get_property_trivial");
		}
	}

	/* Data query succeeded, so it is supported */
	isSupported_PreparingForSleep = 1;

	if (val == prev) {
		/* Unchanged */
		upsdebugx(8, "%s: state unchanged", __func__);
		return -1;
	}

	/* First run and not immediately going to sleep, assume unchanged (no-op for upsmon et al) */
	if (prev < 0 && !val) {
		prev = val;
		upsdebugx(8, "%s: state unchanged (assumed): first run and not immediately going to sleep", __func__);
		return -1;
	}

	/* 0 or 1 */
	upsdebugx(8, "%s: state changed): %" PRIi32 " -> %" PRIi32, __func__, prev, val);
	prev = val;
	return val;
}

#else	/* not WITH_LIBSYSTEMD_INHIBITOR */

int isInhibitSupported(void)
{
	return 0;
}

int isPreparingForSleepSupported(void)
{
	return 0;
}

TYPE_FD Inhibit(const char *arg_what, const char *arg_who, const char *arg_why, const char *arg_mode)
{
	static int	reported = 0;
	NUT_UNUSED_VARIABLE(arg_what);
	NUT_UNUSED_VARIABLE(arg_who);
	NUT_UNUSED_VARIABLE(arg_why);
	NUT_UNUSED_VARIABLE(arg_mode);

	if (!reported) {
		upsdebugx(6, "%s: Not implemented on this platform", __func__);
		reported = 1;
	}
	return ERROR_FD;
}

int isPreparingForSleep(void)
{
	static int	reported = 0;

	if (!reported) {
		upsdebugx(6, "%s: Not implemented on this platform", __func__);
		reported = 1;
	}
	return -1;
}

void Uninhibit(TYPE_FD *fd_ptr)
{
	static int	reported = 0;
	NUT_UNUSED_VARIABLE(fd_ptr);

	if (!reported) {
		upsdebugx(6, "%s: Not implemented on this platform", __func__);
		reported = 1;
	}
}

#endif	/* not WITH_LIBSYSTEMD_INHIBITOR */

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
# endif	/* DEBUG_SYSTEMD_WATCHDOG */
#endif	/* WITH_LIBSYSTEMD */
/* Similarly for only reporting once if the notification subsystem is not built-in */
static int upsnotify_reported_disabled_notech = 0;
static int upsnotify_report_verbosity = -1;

/* the reason we define UPS_VERSION as a static string, rather than a
	macro, is to make dependency tracking easier (only common.o depends
	on nut_version.h), and also to prevent all sources from
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

#if defined(HAVE_LIB_BSD_KVM_PROC) && HAVE_LIB_BSD_KVM_PROC
# include <kvm.h>
# include <sys/param.h>
# include <sys/sysctl.h>
#endif

#if defined(HAVE_LIB_ILLUMOS_PROC) && HAVE_LIB_ILLUMOS_PROC
# include <procfs.h>
#endif

pid_t get_max_pid_t(void)
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

	/* Normally sendsignalfn(), sendsignalpid() and related methods call
	 * upslogx() to report issues such as failed fopen() of PID file,
	 * failed parse of its contents, inability to send a signal (absent
	 * process or some other issue like permissions).
	 * Some of these low-level reports look noisy and scary to users,
	 * others are a bit confusing ("PID file not found... is it bad or
	 * good, what do I do with that knowledge?") so several consuming
	 * programs actually parse the returned codes to formulate their
	 * own messages like "No earlier instance of this daemon was found
	 * running" and users benefit even less from low-level reports.
	 * This variable and its values are a bit of internal detail between
	 * certain NUT programs to hush the low-level reports when they are
	 * not being otherwise debugged (e.g. nut_debug_level < 1).
	 * Default value allows all those messages to appear.
	 */
	int	nut_sendsignal_debug_level = NUT_SENDSIGNAL_DEBUG_LEVEL_DEFAULT;

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

int syslog_is_disabled(void)
{
	static int value = -1;

	if (value < 0) {
		char *s = getenv("NUT_DEBUG_SYSLOG");
		/* Not set or not disabled by the setting: default is enabled (inversed per method name) */
		value = 0;
		if (s) {
			if (!strcmp(s, "stderr")) {
				value = 1;
			} else if (!strcmp(s, "none") || !strcmp(s, "false")) {
				value = 2;
			} else if (!strcmp(s, "syslog") || !strcmp(s, "true") || !strcmp(s, "default")) {
				/* Just reserve a value to quietly do the default */
				value = 0;
			} else {
				upsdebugx(0, "%s: unknown NUT_DEBUG_SYSLOG='%s' value, ignored (assuming enabled)",
					__func__, s);
			}
		}
	}

	return value;
}

int banner_is_disabled(void)
{
	static int value = -1;

	if (value < 0) {
		char *s = getenv("NUT_QUIET_INIT_BANNER");
		/* Envvar present and empty or true-ish means NUT tool name+version
		 * banners disabled by the setting: default is enabled (inversed per
		 * method name) */
		value = 0;
		if (s) {
			if (*s == '\0' || !strcasecmp(s, "true") || strcmp(s, "1")) {
				value = 1;
			}
		}
	}

	return value;
}

const char *describe_NUT_VERSION_once(void)
{
	static char	buf[LARGEBUF];
	static const char	*printed = NULL;

	if (printed)
		return printed;

	memset(buf, 0, sizeof(buf));

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
	/* NOTE: Some compilers deduce that macro-based decisions about
	 * NUT_VERSION_IS_RELEASE make one of codepaths unreachable in
	 * a particular build. So we pragmatically handwave this away.
	 */
	if (1 < snprintf(buf, sizeof(buf),
		"%s %s%s%s",
		NUT_VERSION_MACRO,
		NUT_VERSION_IS_RELEASE ? "release" : "(development iteration after ",
		NUT_VERSION_IS_RELEASE ? "" : NUT_VERSION_SEMVER_MACRO,
		NUT_VERSION_IS_RELEASE ? "" : ")"
	)) {
		printed = buf;
	} else {
		upslogx(LOG_WARNING, "%s: failed to report detailed NUT version", __func__);
		printed = UPS_VERSION;
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic pop
#endif

	return printed;
}

int print_banner_once(const char *prog, int even_if_disabled)
{
	static int	printed = 0;
	static int	ret = -1;

	if (printed)
		return ret;

	if (!banner_is_disabled() || even_if_disabled) {
		ret = printf("Network UPS Tools %s %s%s\n",
			prog, describe_NUT_VERSION_once(),
			even_if_disabled == 2 ? "\n" : "");
		fflush(stdout);
		if (ret > 0)
			printed = 1;
	}

	return ret;
}

const char *suggest_doc_links(const char *progname, const char *progconf) {
	static char	buf[LARGEBUF];

	buf[0] = '\0';

	if (progname) {
		char	*s = NULL, *buf2 = xstrdup(xbasename(progname));
		size_t	i;

		for (i = 0; buf2[i]; i++) {
			buf2[i] = tolower(buf2[i]);
		}

		if ((s = strstr(buf2, ".exe")) && strcmp(buf2, "nut.exe"))
			*s = '\0';

		snprintf(buf, sizeof(buf),
			"For more information please ");
#if defined(WITH_DOCS) && WITH_DOCS
		/* FIXME: Currently all NUT tools and drivers are in same
		 *  man page section for "System Management Programs".
		 *  If this ever changes (e.g. clients like `upsc` can be
		 *  a "User Program" just as well), we may need an extra
		    method argument here.
		 */
		snprintfcat(buf, sizeof(buf),
			"Read The Fine Manual ('man %s %s') and/or ",
			MAN_SECTION_CMD_SYS, buf2);
#endif
		snprintfcat(buf, sizeof(buf),
			"see\n\t%s/docs/man/%s.html\n",
			NUT_WEBSITE_BASE, buf2);

		free(buf2);
	}

	if (progconf)
		snprintfcat(buf, sizeof(buf),
			"%s check documentation and samples of %s\n",
			progname ? "Also" : "Please",
			progconf);

	return buf;
}

/* enable writing upslog_with_errno() and upslogx() type messages to
   the syslog */
void syslogbit_set(void)
{
	if (!syslog_is_disabled())
		xbit_set(&upslog_flags, UPSLOG_SYSLOG);
}

/* get the syslog ready for us */
void open_syslog(const char *progname)
{
#ifndef WIN32
	int	opt;

	if (syslog_is_disabled())
		return;

	opt = LOG_PID;

	/* we need this to grab /dev/log before chroot */
# ifdef LOG_NDELAY
	opt |= LOG_NDELAY;
# endif	/* LOG_NDELAY */

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
# else
	case 0:
		break;
	default:
		upslogx(LOG_INFO, "Changing log level threshold not possible");
		break;
# endif	/* HAVE_SETLOGMASK && HAVE_DECL_LOG_UPTO */
	}
#else
	EventLogName = progname;
#endif	/* WIND32 */
}

/* close ttys and become a daemon */
void background(void)
{
	/* Normally we enable SYSLOG and disable STDERR,
	 * unless NUT_DEBUG_SYSLOG envvar interferes as
	 * interpreted in syslog_is_disabled() method: */
	int	syslog_disabled = syslog_is_disabled(),
		stderr_disabled = (syslog_disabled == 0 || syslog_disabled == 2);

#ifndef WIN32
	int	pid;

	if ((pid = fork()) < 0)
		fatal_with_errno(EXIT_FAILURE, "Unable to enter background");
#endif

	if (!syslog_disabled)
		/* not disabled: NUT_DEBUG_SYSLOG is unset or invalid */
		xbit_set(&upslog_flags, UPSLOG_SYSLOG);
	if (stderr_disabled)
		/* NUT_DEBUG_SYSLOG="none" or unset/invalid */
		xbit_clear(&upslog_flags, UPSLOG_STDERR);

#ifndef WIN32
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
# ifdef HAVE_DUP2
	/* system can close (if needed) and (re-)open a specific FD number */
	if (1) { /* scoping */
		TYPE_FD devnull = open("/dev/null", O_RDWR);
		if (devnull < 0)
			fatal_with_errno(EXIT_FAILURE, "open /dev/null");

		if (dup2(devnull, STDIN_FILENO) != STDIN_FILENO)
			fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDIN");
		if (dup2(devnull, STDOUT_FILENO) != STDOUT_FILENO)
			fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDOUT");
		if (stderr_disabled) {
			if (dup2(devnull, STDERR_FILENO) != STDERR_FILENO)
				fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDERR");
		}

		close(devnull);
	}
# else
#  ifdef HAVE_DUP
	/* opportunistically duplicate to the "lowest-available" FD number */
	close(STDIN_FILENO);
	if (open("/dev/null", O_RDWR) != STDIN_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDIN");

	close(STDOUT_FILENO);
	if (dup(STDIN_FILENO) != STDOUT_FILENO)
		fatal_with_errno(EXIT_FAILURE, "dup /dev/null as STDOUT");

	if (stderr_disabled) {
		close(STDERR_FILENO);
		if (dup(STDIN_FILENO) != STDERR_FILENO)
			fatal_with_errno(EXIT_FAILURE, "dup /dev/null as STDERR");
	}
#  else
	close(STDIN_FILENO);
	if (open("/dev/null", O_RDWR) != STDIN_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDIN");

	close(STDOUT_FILENO);
	if (open("/dev/null", O_RDWR) != STDOUT_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDOUT");

	if (stderr_disabled) {
		close(STDERR_FILENO);
		if (open("/dev/null", O_RDWR) != STDERR_FILENO)
			fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDERR");
	}
#  endif
# endif

# ifdef HAVE_SETSID
	setsid();		/* make a new session to dodge signals */
# endif
#endif	/* not WIN32 */

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

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
# pragma GCC diagnostic push
#endif
#if (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC)
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#if (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC)
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
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

char * getprocname(pid_t pid)
{
	/* Try to identify process (program) name for the given PID,
	 * return NULL if we can not for any reason (does not run,
	 * no rights, do not know how to get it on current OS, etc.)
	 * If the returned value is not NULL, caller should free() it.
	 * Some implementation pieces borrowed from
	 * https://man7.org/linux/man-pages/man2/readlink.2.html and
	 * https://github.com/openbsd/src/blob/master/bin/ps/ps.c
	 * NOTE: Very much platform-dependent!
	 */
	char	*procname = NULL;
	size_t	procnamelen = 0;
	char	pathname[NUT_PATH_MAX];
	struct stat	st;

#ifdef WIN32
	/* Try Windows API calls, then fall through to /proc emulation in MinGW/MSYS2
	 * https://stackoverflow.com/questions/1591342/c-how-to-determine-if-a-windows-process-is-running
	 * http://cppip.blogspot.com/2013/01/check-if-process-is-running.html
	 */
	upsdebugx(5, "%s: begin to query WIN32 process info", __func__);
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
	if (process) {
		DWORD	ret = GetModuleFileNameExA(
				process,	/* hProcess */
				NULL,		/* hModule */
				(LPSTR)pathname,
				(DWORD)(sizeof(pathname))
			);
		CloseHandle(process);
		pathname[sizeof(pathname) - 1] = '\0';

		if (ret) {
			/* length of the string copied to the buffer */
			procnamelen = strlen(pathname);

			upsdebugx(3, "%s: try to parse the name from WIN32 process info",
				__func__);
			if (ret != procnamelen) {
				upsdebugx(3, "%s: length mismatch getting WIN32 process info: %"
					PRIuMAX " vs. " PRIuSIZE,
					__func__, (uintmax_t)ret, procnamelen);
			}

			if ((procname = (char*)calloc(procnamelen + 1, sizeof(char)))) {
				if (snprintf(procname, procnamelen + 1, "%s", pathname) < 1) {
					upsdebug_with_errno(3, "%s: failed to snprintf procname: WIN32-like", __func__);
				} else {
					goto finish;
				}
			} else {
				upsdebug_with_errno(3, "%s: failed to allocate the procname "
					"string to store token from WIN32 size %" PRIuSIZE,
					__func__, procnamelen);
			}

			/* Fall through to try /proc etc. if available */
		} else {
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

			upsdebugx(3, "%s: failed to get WIN32 process info: %s",
				__func__, (char *)WinBuf);
			LocalFree(WinBuf);
		}
	}
#endif

	if (stat("/proc", &st) == 0 && ((st.st_mode & S_IFMT) == S_IFDIR)) {
		upsdebugx(3, "%s: /proc is an accessible directory, investigating", __func__);

#if (defined HAVE_READLINK) && HAVE_READLINK
		/* Linux-like */
		if (snprintf(pathname, sizeof(pathname), "/proc/%" PRIuMAX "/exe", (uintmax_t)pid) < 10) {
			upsdebug_with_errno(3, "%s: failed to snprintf pathname: Linux-like", __func__);
			goto finish;
		}

		if (lstat(pathname, &st) == 0) {
			goto process_stat_symlink;
		}

		/* FreeBSD-like */
		if (snprintf(pathname, sizeof(pathname), "/proc/%" PRIuMAX "/file", (uintmax_t)pid) < 10) {
			upsdebug_with_errno(3, "%s: failed to snprintf pathname: FreeBSD-like", __func__);
			goto finish;
		}

		if (lstat(pathname, &st) == 0) {
			goto process_stat_symlink;
		}

		goto process_parse_file;

process_stat_symlink:
		upsdebugx(3, "%s: located symlink for PID %" PRIuMAX " at: %s",
			__func__, (uintmax_t)pid, pathname);
		/* Some magic symlinks under (for example) /proc and /sys
		 * report 'st_size' as zero. In that case, take PATH_MAX
		 * or equivalent as a "good enough" estimate. */
		if (st.st_size) {
			/* Add one for ending '\0' */
			procnamelen = st.st_size + 1;
		} else {
			procnamelen = sizeof(pathname);
		}

		/* Not xcalloc() here, not too fatal if we fail */
		procname = (char*)calloc(procnamelen, sizeof(char));
		if (procname) {
			int nbytes = readlink(pathname, procname, procnamelen);
			if (nbytes < 0) {
				upsdebug_with_errno(1, "%s: failed to readlink() from %s",
					__func__, pathname);
				free(procname);
				procname = NULL;
				goto process_parse_file;
			}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
			if ((unsigned int)nbytes > SIZE_MAX || procnamelen <= (size_t)nbytes) {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
# pragma GCC diagnostic pop
#endif
				upsdebugx(1, "%s: failed to readlink() from %s: may have been truncated",
					__func__, pathname);
				free(procname);
				procname = NULL;
				goto process_parse_file;
			}

			/* Got a useful reply */
			procname[nbytes] = '\0';
			goto finish;
		} else {
			upsdebug_with_errno(3, "%s: failed to allocate the procname string "
				"to readlink() size %" PRIuSIZE, __func__, procnamelen);
			goto finish;
		}
#else
		upsdebugx(3, "%s: this platform does not have readlink(), skipping this method", __func__);
		goto process_parse_file;
#endif	/* HAVE_READLINK */

process_parse_file:
		upsdebugx(5, "%s: try to parse some files under /proc", __func__);

		/* Check /proc/NNN/cmdline (may start with a '-' to ignore, for
		 * a title string like "-bash" where programs edit their argv[0]
		 * (Linux-like OSes at least). Inspired by
		 * https://gist.github.com/evanslai/30c6d588a80222f665f10b4577dadd61
		 */
		if (snprintf(pathname, sizeof(pathname), "/proc/%" PRIuMAX "/cmdline", (uintmax_t)pid) < 10) {
			upsdebug_with_errno(3, "%s: failed to snprintf pathname: Linux-like", __func__);
			goto finish;
		}

		if (stat(pathname, &st) == 0) {
			FILE* fp = fopen(pathname, "r");
			if (fp) {
				char	buf[sizeof(pathname)];
				if (fgets(buf, sizeof(buf), fp) != NULL) {
					/* check the first token in the file, the program name */
					char* first = strtok(buf, " ");

					fclose(fp);
					if (first) {
						if (*first == '-')
							first++;

						/* Not xcalloc() here, not too fatal if we fail */
						if ((procnamelen = strlen(first))) {
							upsdebugx(3, "%s: try to parse some files under /proc: processing %s",
								__func__, pathname);
							if ((procname = (char*)calloc(procnamelen + 1, sizeof(char)))) {
								if (snprintf(procname, procnamelen + 1, "%s", first) < 1) {
									upsdebug_with_errno(3, "%s: failed to snprintf procname: Linux-like", __func__);
								}
							} else {
								upsdebug_with_errno(3, "%s: failed to allocate the procname "
									"string to store token from 'cmdline' size %" PRIuSIZE,
									__func__, procnamelen);
							}

							goto finish;
						}
					}
				} else {
					fclose(fp);
				}
			}
		}

		/* Check /proc/NNN/stat (second token, in parentheses, may be truncated)
		 * see e.g. https://stackoverflow.com/a/12675103/4715872 */
		if (snprintf(pathname, sizeof(pathname), "/proc/%" PRIuMAX "/stat", (uintmax_t)pid) < 10) {
			upsdebug_with_errno(3, "%s: failed to snprintf pathname: Linux-like", __func__);
			goto finish;
		}

		if (stat(pathname, &st) == 0) {
			FILE* fp = fopen(pathname, "r");
			if (fp) {
				long	spid;
				char	sstate;
				char	buf[sizeof(pathname)];

				memset (buf, 0, sizeof(buf));
				if ( (fscanf(fp, "%ld (%[^)]) %c", &spid, buf, &sstate)) == 3 ) {
					/* Some names can be pretty titles like "init(Ubuntu)"
					 * or "Relay(223)". Or truncated like "docker-desktop-".
					 * Tokenize by "(" " " and extract the first token to
					 * address the former "problem", not too much we can
					 * do about the latter except for keeping NUT program
					 * names concise.
					 */
					char* first = strtok(buf, "( ");

					fclose(fp);
					if (first) {
						/* Not xcalloc() here, not too fatal if we fail */
						if ((procnamelen = strlen(first))) {
							upsdebugx(3, "%s: try to parse some files under /proc: processing %s "
								"(WARNING: may be truncated)",
								__func__, pathname);
							if ((procname = (char*)calloc(procnamelen + 1, sizeof(char)))) {
								if (snprintf(procname, procnamelen + 1, "%s", first) < 1) {
									upsdebug_with_errno(3, "%s: failed to snprintf procname: Linux-like", __func__);
								}
							} else {
								upsdebug_with_errno(3, "%s: failed to allocate the procname "
									"string to store token from 'stat' size %" PRIuSIZE,
									__func__, procnamelen);
							}

							goto finish;
						}
					}
				} else {
					fclose(fp);
				}
			}
		}

#if defined(HAVE_LIB_ILLUMOS_PROC) && HAVE_LIB_ILLUMOS_PROC
		/* Solaris/illumos: parse binary structure at /proc/NNN/psinfo */
		if (snprintf(pathname, sizeof(pathname), "/proc/%" PRIuMAX "/psinfo", (uintmax_t)pid) < 10) {
			upsdebug_with_errno(3, "%s: failed to snprintf pathname: Solaris/illumos-like", __func__);
			goto finish;
		}

		if (stat(pathname, &st) == 0) {
			FILE* fp = fopen(pathname, "r");
			if (!fp) {
				upsdebug_with_errno(3, "%s: try to parse '%s':"
					"fopen() returned NULL", __func__, pathname);
			} else {
				psinfo_t	info;	/* process information from /proc */
				size_t	r;

				memset (&info, 0, sizeof(info));
				r = fread((char *)&info, sizeof (info), 1, fp);
				if (r != 1) {
					upsdebug_with_errno(3, "%s: try to parse '%s': "
						"unexpected read size: got %" PRIuSIZE
						" record(s) from file of size %" PRIuMAX
						" vs. 1 piece of %" PRIuSIZE " struct size",
						__func__, pathname, r,
						(uintmax_t)st.st_size, sizeof (info));
					fclose(fp);
				} else {
					fclose(fp);

					/* Not xcalloc() here, not too fatal if we fail */
					if ((procnamelen = strlen(info.pr_fname))) {
						upsdebugx(3, "%s: try to parse some files under /proc: processing %s",
							__func__, pathname);
						if ((procname = (char*)calloc(procnamelen + 1, sizeof(char)))) {
							if (snprintf(procname, procnamelen + 1, "%s", info.pr_fname) < 1) {
								upsdebug_with_errno(3, "%s: failed to snprintf pathname: Solaris/illumos-like", __func__);
							}
						} else {
							upsdebug_with_errno(3, "%s: failed to allocate the procname "
								"string to store token from 'psinfo' size %" PRIuSIZE,
								__func__, procnamelen);
						}

						goto finish;
					}
				}
			}
		}
#endif
	} else {
		upsdebug_with_errno(3, "%s: /proc is not a directory or not accessible", __func__);
	}

#if defined(HAVE_LIB_BSD_KVM_PROC) && HAVE_LIB_BSD_KVM_PROC
	/* OpenBSD, maybe other BSD: no /proc; use API call, see ps.c link above and
	 * https://kaashif.co.uk/2015/06/18/how-to-get-a-list-of-processes-on-openbsd-in-c/
	 */
	if (!procname) {
		char	errbuf[_POSIX2_LINE_MAX];
		kvm_t	*kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);

		upsdebugx(3, "%s: try to parse BSD KVM process info snapsnot", __func__);
		if (!kd) {
			upsdebugx(3, "%s: try to parse BSD KVM process info snapsnot: "
				"kvm_openfiles() returned NULL", __func__);
		} else {
			int	nentries = 0;
			struct	kinfo_proc *kp = kvm_getprocs(kd, KERN_PROC_PID, pid, sizeof(*kp), &nentries);

			if (!kp) {
				upsdebugx(3, "%s: try to parse BSD KVM process info snapsnot: "
					"kvm_getprocs() returned NULL", __func__);
			} else {
				int	i;
				if (nentries != 1)
					upsdebugx(3, "%s: expected to get 1 reply from BSD kvm_getprocs but got %d",
						__func__, nentries);
				for (i = 0; i < nentries; i++) {
					upsdebugx(5, "%s: processing reply #%d from BSD"
						" kvm_getprocs: pid=%" PRIuMAX " name='%s'",
						__func__, i, (uintmax_t)kp[i].p_pid, kp[i].p_comm);
					if ((uintmax_t)(kp[i].p_pid) == (uintmax_t)pid) {
						/* Not xcalloc() here, not too fatal if we fail */
						if ((procnamelen = strlen(kp[i].p_comm))) {
							if ((procname = (char*)calloc(procnamelen + 1, sizeof(char)))) {
								if (snprintf(procname, procnamelen + 1, "%s", kp[i].p_comm) < 1) {
									upsdebug_with_errno(3, "%s: failed to snprintf procname: BSD-like", __func__);
								}
							} else {
								upsdebug_with_errno(3, "%s: failed to allocate the procname "
									"string to store token from BSD KVM process info "
									"snapsnot size %" PRIuSIZE,
									__func__, procnamelen);
							}

							goto finish;
						}
					}
				}
			}
		}
	}
#endif	/* HAVE_LIB_BSD_KVM_PROC */

	goto finish;

finish:
	if (procname) {
		procnamelen = strlen(procname);
		if (procnamelen == 0) {
			free(procname);
			procname = NULL;
		} else {
			upsdebugx(1, "%s: determined process name for PID %" PRIuMAX ": %s",
				__func__, (uintmax_t)pid, procname);
		}
	}

	if (!procname) {
		upsdebugx(1, "%s: failed to determine process name for PID %" PRIuMAX,
			__func__, (uintmax_t)pid);
	}

	return procname;
}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
# pragma GCC diagnostic pop
#endif

size_t parseprogbasename(char *buf, size_t buflen, const char *progname, size_t *pprogbasenamelen, size_t *pprogbasenamedot)
{
	size_t	i,
		progbasenamelen = 0,
		progbasenamedot = 0;

	if (pprogbasenamelen)
		*pprogbasenamelen = 0;

	if (pprogbasenamedot)
		*pprogbasenamedot = 0;

	if (!buf || !progname || !buflen || progname[0] == '\0')
		return 0;

	for (i = 0; i < buflen && progname[i] != '\0'; i++) {
		if (progname[i] == '/'
#ifdef WIN32
		||  progname[i] == '\\'
#endif
		) {
			progbasenamelen = 0;
			progbasenamedot = 0;
			continue;
		}

		if (progname[i] == '.')
			progbasenamedot = progbasenamelen;

		buf[progbasenamelen++] = progname[i];
	}
	buf[progbasenamelen] = '\0';
	buf[buflen - 1] = '\0';

	if (pprogbasenamelen)
		*pprogbasenamelen = progbasenamelen;

	if (pprogbasenamedot)
		*pprogbasenamedot = progbasenamedot;

	return progbasenamelen;
}

int checkprocname_ignored(const char *caller)
{
	char	*s = NULL;

	if ((s = getenv("NUT_IGNORE_CHECKPROCNAME"))) {
		/* FIXME: Make server/conf.c::parse_boolean() reusable */
		if ( (!strcasecmp(s, "true")) || (!strcasecmp(s, "on")) || (!strcasecmp(s, "yes")) || (!strcasecmp(s, "1"))) {
			upsdebugx(1, "%s for %s: skipping because caller set NUT_IGNORE_CHECKPROCNAME", __func__, NUT_STRARG(caller));
			return 1;
		}
	}

	return 0;
}

int compareprocname(pid_t pid, const char *procname, const char *progname)
{
	/* Given the binary path name of (presumably) a running process,
	 * check if it matches the assumed name of the current program.
	 * The "pid" value is used in log reporting.
	 * Returns:
	 *	-3	Skipped because NUT_IGNORE_CHECKPROCNAME is set
	 *	-2	Could not parse a program name (ok to proceed,
	 *		risky - but matches legacy behavior)
	 *	-1	Could not identify a program name (ok to proceed,
	 *		risky - but matches legacy behavior)
	 *	0	Process name identified, does not seem to match
	 *	1+	Process name identified, and seems to match with
	 *		varying precision
	 * Generally speaking, if (compareprocname(...)) then ok to proceed
	 */

	int	ret = -127;
	size_t	procbasenamelen = 0, progbasenamelen = 0;
	/* Track where the last dot is in the basename; 0 means none */
	size_t	procbasenamedot = 0, progbasenamedot = 0;
	char	procbasename[NUT_PATH_MAX], progbasename[NUT_PATH_MAX];

	if (checkprocname_ignored(__func__)) {
		ret = -3;
		goto finish;
	}

	if (!procname || !progname) {
		ret = -1;
		goto finish;
	}

	/* First quickly try for an exact hit (possible dir names included) */
	if (!strcmp(procname, progname)) {
		ret = 1;
		goto finish;
	}

	/* Parse the basenames apart */
	if (!parseprogbasename(progbasename, sizeof(progbasename), progname, &progbasenamelen, &progbasenamedot)
	||  !parseprogbasename(procbasename, sizeof(procbasename), procname, &procbasenamelen, &procbasenamedot)
	) {
		ret = -2;
		goto finish;
	}

	/* First quickly try for an exact hit of base names */
	if (progbasenamelen == procbasenamelen && progbasenamedot == procbasenamedot && !strcmp(procbasename, progbasename)) {
		ret = 2;
		goto finish;
	}

	/* Check for executable program filename extensions and/or case-insensitive
	 * matching on some platforms */
#ifdef WIN32
	if (!strcasecmp(procname, progname)) {
		ret = 3;
		goto finish;
	}

	if (!strcasecmp(procbasename, progbasename)) {
		ret = 4;
		goto finish;
	}

	if (progbasenamedot == procbasenamedot || !progbasenamedot || !procbasenamedot) {
		/* Same base name before ext, maybe different casing or absence of ext in one of them */
		size_t	dot = progbasenamedot ? progbasenamedot : procbasenamedot;

		if (!strncasecmp(progbasename, procbasename, dot - 1) &&
		     (  (progbasenamedot && !strcasecmp(progbasename + progbasenamedot, ".exe"))
		     || (procbasenamedot && !strcasecmp(procbasename + procbasenamedot, ".exe")) )
		) {
			ret = 5;
			goto finish;
		}
	}
#endif

	/* TOTHINK: Developer builds wrapped with libtool may be prefixed
	 * by "lt-" in the filename. Should we re-enter (or wrap around)
	 * this search with a set of variants with/without the prefix on
	 * both sides?..
	 */

	/* Nothing above has matched */
	ret = 0;

finish:
	switch (ret) {
		case 5:
			upsdebugx(1,
				"%s: case-insensitive base name hit with "
				"an executable program extension involved for "
				"PID %" PRIuMAX " of '%s'=>'%s' and checked "
				"'%s'=>'%s'",
				__func__, (uintmax_t)pid,
				procname, procbasename,
				progname, progbasename);
			break;

		case 4:
			upsdebugx(1,
				"%s: case-insensitive base name hit for PID %"
				PRIuMAX " of '%s'=>'%s' and checked '%s'=>'%s'",
				__func__, (uintmax_t)pid,
				procname, procbasename,
				progname, progbasename);
			break;

		case 3:
			upsdebugx(1,
				"%s: case-insensitive full name hit for PID %"
				PRIuMAX " of '%s' and checked '%s'",
				__func__, (uintmax_t)pid, procname, progname);
			break;

		case 2:
			upsdebugx(1,
				"%s: case-sensitive base name hit for PID %"
				PRIuMAX " of '%s'=>'%s' and checked '%s'=>'%s'",
				__func__, (uintmax_t)pid,
				procname, procbasename,
				progname, progbasename);
			break;

		case 1:
			upsdebugx(1,
				"%s: exact case-sensitive full name hit for PID %"
				PRIuMAX " of '%s' and checked '%s'",
				__func__, (uintmax_t)pid, procname, progname);
			break;

		case 0:
			upsdebugx(1,
				"%s: did not find any match of program names "
				"for PID %" PRIuMAX " of '%s'=>'%s' and checked "
				"'%s'=>'%s'",
				__func__, (uintmax_t)pid,
				procname, procbasename,
				progname, progbasename);
			break;

		case -1:
			/* failed to getprocname(), logged above in it */
			break;

		case -2:
			upsdebugx(1,
				"%s: failed to parse base names of the programs",
				__func__);
			break;

		case -3:
			/* skipped due to envvar, logged above */
			break;

		default:
			upsdebugx(1,
				"%s: unexpected result looking for process name "
				"of PID %" PRIuMAX ": %d",
				__func__, (uintmax_t)pid, ret);
			ret = -127;
			break;
	}

	return ret;
}

int checkprocname(pid_t pid, const char *progname)
{
	/* If we can determine the binary path name of the specified "pid",
	 * check if it matches the assumed name of the current program.
	 * Returns: same as compareprocname()
	 * Generally speaking, if (checkprocname(...)) then ok to proceed
	 */
	char	*procname = NULL;
	int	ret = 0;

	/* Quick skip before drilling into getprocname() */
	if (checkprocname_ignored(__func__)) {
		ret = -3;
		goto finish;
	}

	if (!progname) {
		ret = -1;
		goto finish;
	}

	procname = getprocname(pid);
	if (!procname) {
		ret = -1;
		goto finish;
	}

	ret = compareprocname(pid, procname, progname);

finish:
	if (procname)
		free(procname);

	return ret;
}

#ifdef WIN32
/* In WIN32 all non binaries files (namely configuration and PID files)
   are retrieved relative to the path of the binary itself.
   So this function fill "dest" with the full path to "relative_path"
   depending on the .exe path */
char * getfullpath(char * relative_path)
{
	char buf[NUT_PATH_MAX];
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
	char	fn[NUT_PATH_MAX];
	FILE	*pidf;
	mode_t	mask;

	/* use full path if present, else build filename in PIDPATH */
	if (*name == '/')
		snprintf(fn, sizeof(fn), "%s", name);
	else
		snprintf(fn, sizeof(fn), "%s/%s.pid", rootpidpath(), name);

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
int sendsignalpid(pid_t pid, int sig, const char *progname, int check_current_progname)
{
#ifndef WIN32
	int	ret, cpn1 = -10, cpn2 = -10;
	char	*current_progname = NULL, *procname = NULL;

	/* TOTHINK: What about containers where a NUT daemon *is* the only process
	 * and is the PID=1 of the container (recycle if dead)? */
	if (pid < 2 || pid > get_max_pid_t()) {
		if (nut_debug_level > 0 || nut_sendsignal_debug_level > 0)
			upslogx(LOG_NOTICE,
				"Ignoring invalid pid number %" PRIdMAX,
				(intmax_t) pid);
		return -1;
	}

	ret = 0;
	if (!checkprocname_ignored(__func__))
		procname = getprocname(pid);

	if (procname && progname) {
		/* Check against some expected (often built-in) name */
		if (!(cpn1 = compareprocname(pid, procname, progname))) {
			/* Did not match expected (often built-in) name */
			ret = -1;
		} else {
			if (cpn1 > 0) {
				/* Matched expected name, ok to proceed */
				ret = 1;
			}
			/* ...else could not determine name of PID; think later */
		}
	}
	/* if (cpn1 == -3) => NUT_IGNORE_CHECKPROCNAME=true */
	/* if (cpn1 == -1) => could not determine name of PID... retry just in case? */
	if (procname && ret <= 0 && check_current_progname && cpn1 != -3) {
		/* NOTE: This could be optimized a bit by pre-finding the procname
		 * of "pid" and re-using it, but this is not a hot enough code path
		 * to bother much.
		 */
		current_progname = getprocname(getpid());
		if (current_progname && (cpn2 = compareprocname(pid, procname, current_progname))) {
			if (cpn2 > 0) {
				/* Matched current process as asked, ok to proceed */
				ret = 2;
			}
			/* ...else could not determine name of PID; think later */
		} else {
			if (current_progname) {
				/* Did not match current process name */
				ret = -2;
			} /* else just did not determine current process
			   * name, so did not actually check either
			   * // ret = -3;
			   */
		}
	}

	/* if ret == 0, ok to proceed - not asked for any sanity checks;
	 * if ret > 0, ok to proceed - we had some definitive match above;
	 * if ret < 0, NOT OK to proceed - we had some definitive fault above
	 */
	if (ret < 0) {
		upsdebugx(1,
			"%s: ran at least one check, and all such checks "
			"found a process name for PID %" PRIuMAX " and "
			"failed to match: "
			"found procname='%s', "
			"expected progname='%s' (res=%d%s), "
			"current progname='%s' (res=%d%s)",
			__func__, (uintmax_t)pid,
			NUT_STRARG(procname),
			NUT_STRARG(progname), cpn1,
			(cpn1 == -10 ? ": did not check" : ""),
			NUT_STRARG(current_progname), cpn2,
			(cpn2 == -10 ? ": did not check" : ""));

		if (nut_debug_level > 0 || nut_sendsignal_debug_level > 1) {
			switch (ret) {
				case -1:
					upslogx(LOG_ERR, "Tried to signal PID %" PRIuMAX
						" which exists but is not of"
						" expected program '%s'; not asked"
						" to cross-check current PID's name",
						(uintmax_t)pid, progname);
					break;

				/* Maybe we tried both data sources, maybe just current_progname */
				case -2:
				/*case -3:*/
					if (progname && current_progname) {
						/* Tried both, downgraded verdict further */
						upslogx(LOG_ERR, "Tried to signal PID %" PRIuMAX
							" which exists but is not of expected"
							" program '%s' nor current '%s'",
							(uintmax_t)pid, progname, current_progname);
					} else if (current_progname) {
						/* Not asked for progname==NULL */
						upslogx(LOG_ERR, "Tried to signal PID %" PRIuMAX
							" which exists but is not of"
							" current program '%s'",
							(uintmax_t)pid, current_progname);
					} else if (progname) {
						upslogx(LOG_ERR, "Tried to signal PID %" PRIuMAX
							" which exists but is not of"
							" expected program '%s'; could not"
							" cross-check current PID's name",
							(uintmax_t)pid, progname);
					} else {
						/* Both NULL; one not asked, another not detected;
						 * should not actually get here (wannabe `ret==-3`)
						 */
						upslogx(LOG_ERR, "Tried to signal PID %" PRIuMAX
							" but could not cross-check current PID's"
							" name: did not expect to get here",
							(uintmax_t)pid);
					}
					break;

				default:
					break;
			}
		}

		if (current_progname) {
			free(current_progname);
			current_progname = NULL;
		}

		if (procname) {
			free(procname);
			procname = NULL;
		}

		/* Logged or not, sanity-check was requested and failed */
		return -1;
	}

	if (current_progname) {
		free(current_progname);
		current_progname = NULL;
	}

	if (procname) {
		free(procname);
		procname = NULL;
	}

	/* see if this is going to work first - does the process exist,
	 * and do we have permissions to signal it? */
	ret = kill(pid, 0);

	if (ret < 0) {
		if (nut_debug_level > 0 || nut_sendsignal_debug_level >= NUT_SENDSIGNAL_DEBUG_LEVEL_KILL_SIG0PING)
			perror("kill");
		return -1;
	}

	if (sig != 0) {
		/* now actually send it */
		ret = kill(pid, sig);

		if (ret < 0) {
			if (nut_debug_level > 0 || nut_sendsignal_debug_level > 1)
				perror("kill");
			return -1;
		}
	}

	return 0;
#else
	NUT_UNUSED_VARIABLE(pid);
	NUT_UNUSED_VARIABLE(sig);
	NUT_UNUSED_VARIABLE(progname);
	NUT_UNUSED_VARIABLE(check_current_progname);
	/* Windows builds use named pipes, not signals per se */
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

	errno = 0;
	if (!buf) {
		upsdebugx(6, "%s: called with NULL input", __func__);
		errno = EINVAL;
		return pid;
	}

	/* assuming 10 digits for a long */
	_pid = strtol(buf, (char **)NULL, 10);
	if (_pid <= get_max_pid_t()) {
		pid = (pid_t)_pid;
	} else {
		errno = ERANGE;

		if (nut_debug_level > 0 || nut_sendsignal_debug_level > 0)
			upslogx(LOG_NOTICE,
				"Received a pid number too big for a pid_t: %"
				PRIdMAX, _pid);
	}

	return pid;
}

/* open pidfn, get the pid;
 * returns negative codes for errors, or
 * zero for a successfully discovered value
 */
pid_t parsepidfile(const char *pidfn)
{
	char	buf[SMALLBUF];
	FILE	*pidf;
	pid_t	pid = -1;

	pidf = fopen(pidfn, "r");
	if (!pidf) {
		/* This one happens quite often when a daemon starts
		 * for the first time and no opponent PID file exists,
		 * so the cut-off verbosity is higher.
		 */
		if (nut_debug_level > 0 ||
		    nut_sendsignal_debug_level >= NUT_SENDSIGNAL_DEBUG_LEVEL_FOPEN_PIDFILE)
			upslog_with_errno(LOG_NOTICE, "fopen %s", pidfn);
		return -3;
	}

	if (fgets(buf, sizeof(buf), pidf) == NULL) {
		if (nut_debug_level > 0 || nut_sendsignal_debug_level > 2)
			upslogx(LOG_NOTICE, "Failed to read pid from %s", pidfn);
		fclose(pidf);
		return -2;
	}

	/* TOTHINK: Original sendsignalfn code (which this
	 * was extracted from) only closed pidf before
	 * exiting the method, on error or "normally".
	 * Why not here? Do we want an (exclusive?) hold
	 * on it while being active in the method?
	 */

	/* this method actively reports errors, if any */
	pid = parsepid(buf);

	fclose(pidf);

	return pid;
}

/* open pidfn, get the pid, then send it sig
 * returns negative codes for errors, or
 * zero for a successfully sent signal
 */
#ifndef WIN32
int sendsignalfn(const char *pidfn, int sig, const char *progname, int check_current_progname)
{
	int	ret = -1;
	pid_t	pid = parsepidfile(pidfn);

	if (pid >= 0) {
		/* this method actively reports errors, if any */
		ret = sendsignalpid(pid, sig, progname, check_current_progname);
	}

	return ret;
}

#else	/* => WIN32 */

int sendsignalfn(const char *pidfn, const char * sig, const char *progname_ignored, int check_current_progname_ignored)
{
	BOOL	ret;
	NUT_UNUSED_VARIABLE(progname_ignored);
	NUT_UNUSED_VARIABLE(check_current_progname_ignored);

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

/*****************************************************************************
 * String methods for space-separated token lists, used originally in dstate *
 *****************************************************************************/

/* Return non-zero if "string" contains "token" (case-sensitive),
 * either surrounded by space character(s) or start/end of "string",
 * or 0 if that token is not there, or if either string is NULL or empty.
 */
int	str_contains_token(const char *string, const char *token)
{
	char	*s = NULL;
	size_t	offset = 0, toklen = 0;

	if (!token || !*token || !string || !*string)
		return 0;

	s = strstr(string, token);
	toklen = strlen(token);

repeat:
	/* not found or hit end of line */
	if (!s || !*s)
		return 0;

	offset = s - string;
#ifdef DEBUG
	upsdebugx(3, "%s: '%s' in '%s': offset=%" PRIuSIZE" toklen=%" PRIuSIZE" s[toklen]='0x%2X'\n",
		__func__, token, string, offset, toklen, s[toklen]);
#endif
	if (offset == 0 || string[offset - 1] == ' ') {
		/* We have hit the start of token */
		if (s[toklen] == '\0' || s[toklen] == ' ') {
			/* And we have hit the end of token */
			return 1;
		}
	}

	/* token was a substring of some other token */
	s = strstr(s + 1, token);
	goto repeat;
}

/* Add "token" to end of string "tgt", if it is not yet there
 * (prefix it with a space character if "tgt" is not empty).
 * Return 0 if already there, 1 if token was added successfully,
 * -1 if we needed to add it but it did not fit under the tgtsize limit,
 * -2 if either string was NULL or "token" was empty.
 * NOTE: If token contains space(s) inside, recurse to treat it
 * as several tokens to add independently.
 * Optionally calls "callback_always" (if not NULL) after checking
 * for spaces (and maybe recursing) and before checking if the token
 * is already there, and/or "callback_unique" (if not NULL) after
 * checking for uniqueness and going to add a newly seen token.
 * If such callback returns 0, abort the addition of token.
 */
int	str_add_unique_token(char *tgt, size_t tgtsize, const char *token,
			    int (*callback_always)(char *, size_t, const char *),
			    int (*callback_unique)(char *, size_t, const char *)
)
{
	size_t	toklen = 0, tgtlen = 0;

#ifdef DEBUG
	upsdebugx(3, "%s: '%s'\n", __func__, token);
#endif

	if (!tgt || !token || !*token)
		return -2;

	if (strstr(token, " ")) {
		/* Recurse adding each sub-token one by one (avoid duplicates)
		 * We frown upon adding "A FEW TOKENS" at once, but in e.g.
		 * code with mapping tables this is not easily avoidable...
		 */
		char	*tmp = xstrdup(token), *p = tmp, *s = tmp;
		int	retval = -2, ret = 0;

		while (*p) {
			if (*p == ' ') {
				*p = '\0';
				if (s != p) {
					/* Only recurse to set non-trivial tokens */
					ret = str_add_unique_token(tgt, tgtsize, s, callback_always, callback_unique);

					/* Only remember this ret if we are just
					 * starting, or it is a failure, or
					 * if we never failed and keep up the
					 * successful streak */
					if ( (retval == -2)
					||   (ret < 0)
					||   (retval >= 0 && ret >= retval) )
						retval = ret;
				}
				p++;
				s = p;	/* Start of new word... or a consecutive space to ignore on next cycle */
			} else {
				p++;
			}
		}

		if (s != p) {
			/* Last valid token did end with (*p=='\0') */
			ret = str_add_unique_token(tgt, tgtsize, s, callback_always, callback_unique);
			if ( (retval == -2)
			||   (ret < 0)
			||   (retval >= 0 && ret >= retval) )
				retval = ret;
		}

		free(tmp);

		/* Return 0 if all tokens were already there,
		 * or 1 if all tokens were successfully added
		 * (and there was at least one non-trivial token) */
		return retval;
	}

	if (callback_always) {
		int	cbret = callback_always(tgt, tgtsize, token);
		if (!cbret) {
			upsdebugx(2, "%s: skip token '%s': due to callback_always()", __func__, token);
			return -3;
		}
	}

	if (str_contains_token(tgt, token)) {
		upsdebugx(2, "%s: skip token '%s': was already set", __func__, token);
		return 0;
	}

	if (callback_unique) {
		int	cbret = callback_unique(tgt, tgtsize, token);
		if (!cbret) {
			upsdebugx(2, "%s: skip token '%s': due to callback_unique()", __func__, token);
			return -3;
		}
	}

	/* separate with a space if multiple elements are present */
	toklen = strlen(token);
	tgtlen = strlen(tgt);

	if (tgtsize < (tgtlen + (tgtlen > 0 ? 1 : 0) + toklen + 1)) {
		upsdebugx(1, "%s: skip token '%s': too long for target string", __func__, token);
		return -1;
	}

	if (snprintfcat(tgt, tgtsize, "%s%s", (tgtlen > 0) ? " " : "", token) < 0) {
		upsdebugx(1, "%s: error adding token '%s': snprintfcat() failed", __func__, token);
		return -1;
	}

	/* Added successfully */
	return 1;
}

/* lazy way to send a signal if the program uses the PIDPATH */
#ifndef WIN32
int sendsignal(const char *progname, int sig, int check_current_progname)
{
	char	fn[NUT_PATH_MAX];

	snprintf(fn, sizeof(fn), "%s/%s.pid", rootpidpath(), progname);

	return sendsignalfn(fn, sig, progname, check_current_progname);
}
#else
int sendsignal(const char *progname, const char * sig, int check_current_progname)
{
	/* progname is used as the pipe name for WIN32
	 * check_current_progname is de-facto ignored
	 */
	return sendsignalfn(progname, sig, NULL, check_current_progname);
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

	d = 0.000001 * (double)(result.tv_usec) + (double)(result.tv_sec);
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

	if ((usec_t) ts->tv_sec > (UINT64_MAX - ((uint64_t)(ts->tv_nsec) / NSEC_PER_USEC)) / USEC_PER_SEC)
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

	d = 0.000000001 * (double)(result.tv_nsec) + (double)(result.tv_sec);
	return d;
}
#endif	/* HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC */

/* Help avoid cryptic "upsnotify: notify about state 4 with libsystemd:"
 * (with only numeric codes) below */
const char *str_upsnotify_state(upsnotify_state_t state) {
	switch (state) {
		case NOTIFY_STATE_READY:
			return "NOTIFY_STATE_READY";
		case NOTIFY_STATE_READY_WITH_PID:
			return "NOTIFY_STATE_READY_WITH_PID";
		case NOTIFY_STATE_RELOADING:
			return "NOTIFY_STATE_RELOADING";
		case NOTIFY_STATE_STOPPING:
			return "NOTIFY_STATE_STOPPING";
		case NOTIFY_STATE_STATUS:
			/* Send a text message per "fmt" below */
			return "NOTIFY_STATE_STATUS";
		case NOTIFY_STATE_WATCHDOG:
			/* Ping the framework that we are still alive */
			return "NOTIFY_STATE_WATCHDOG";
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
		default:
			/* Must not occur. */
			return "NOTIFY_STATE_UNDEFINED";
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}
}

static void upsnotify_suggest_NUT_QUIET_INIT_UPSNOTIFY_once(void) {
	static	int reported = 0;

	if (reported)
		return;

	reported = 1;

	if (getenv("NUT_QUIET_INIT_UPSNOTIFY"))
		return;

	upsdebugx(1, "On systems without service units, "
		"consider `export NUT_QUIET_INIT_UPSNOTIFY=true`");
}

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

	/* Were we asked to be quiet on the console? */
	if (upsnotify_report_verbosity < 0) {
		char *quiet_init = getenv("NUT_QUIET_INIT_UPSNOTIFY");
		if (quiet_init == NULL) {
			/* No envvar, default is to inform once on the console */
			upsnotify_report_verbosity = 0;
		} else {
			/* Envvar is set, does it tell us to be quiet?
			 * NOTE: Empty also means "yes" */
			if (*quiet_init == '\0'
				|| (strcasecmp(quiet_init, "true")
				&&  strcasecmp(quiet_init, "yes")
				&&  strcasecmp(quiet_init, "on")
				&&  strcasecmp(quiet_init, "1") )
			) {
				upsdebugx(1,
					"NUT_QUIET_INIT_UPSNOTIFY='%s' value "
					"was not recognized, ignored",
					quiet_init);
				upsnotify_report_verbosity = 0;
			} else {
				/* Avoid the verbose message below
				 * (only seen with non-zero debug) */
				upsnotify_report_verbosity = 1;
			}
		}
	}

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
			if (syslog_is_disabled()) {
				fprintf(stderr,
					"%s (%s:%d): vsnprintf needed more than %" PRIuSIZE " bytes: %d",
					__func__, __FILE__, __LINE__, sizeof(msgbuf), ret);
			} else {
				syslog(LOG_WARNING,
					"%s (%s:%d): vsnprintf needed more than %" PRIuSIZE " bytes: %d",
					__func__, __FILE__, __LINE__, sizeof(msgbuf), ret);
			}
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
	if (!upsnotify_reported_disabled_systemd) {
		upsdebugx(upsnotify_report_verbosity,
			"%s: notify about state %s with libsystemd: "
			"skipped for libcommonclient build, "
			"will not spam more about it",
			__func__, str_upsnotify_state(state));
		upsnotify_suggest_NUT_QUIET_INIT_UPSNOTIFY_once();
	}

	upsnotify_reported_disabled_systemd = 1;
# else	/* not WITHOUT_LIBSYSTEMD */
	if (!getenv("NOTIFY_SOCKET")) {
		if (!upsnotify_reported_disabled_systemd) {
			upsdebugx(upsnotify_report_verbosity,
				"%s: notify about state %s with libsystemd: "
				"was requested, but not running as a service "
				"unit now, will not spam more about it",
				__func__, str_upsnotify_state(state));
			upsnotify_suggest_NUT_QUIET_INIT_UPSNOTIFY_once();
		}
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
				if (syslog_is_disabled()) {
					fprintf(stderr,
						"%s (%s:%d): snprintf needed more than %" PRIuSIZE " bytes: %d",
						__func__, __FILE__, __LINE__, sizeof(monoclock_str), ret);
				} else {
					syslog(LOG_WARNING,
						"%s (%s:%d): snprintf needed more than %" PRIuSIZE " bytes: %d",
						__func__, __FILE__, __LINE__, sizeof(monoclock_str), ret);
				}
				msglen = 0;
			} else {
				monoclock_str[0] = '\n';
			}
		}
#   endif	/* HAVE_CLOCK_GETTIME && HAVE_CLOCK_MONOTONIC */

#   if ! DEBUG_SYSTEMD_WATCHDOG
		if (state != NOTIFY_STATE_WATCHDOG || !upsnotify_reported_watchdog_systemd)
#   endif
			upsdebugx(6, "%s: notify about state %s with "
				"libsystemd: use sd_notify()",
				__func__, str_upsnotify_state(state));

		/* https://www.freedesktop.org/software/systemd/man/sd_notify.html */
		if (msglen) {
			ret = snprintf(buf, sizeof(buf), "STATUS=%s", msgbuf);
			if ((ret < 0) || (ret >= (int) sizeof(buf))) {
				if (syslog_is_disabled()) {
					fprintf(stderr,
						"%s (%s:%d): snprintf needed more than %" PRIuSIZE " bytes: %d",
						__func__, __FILE__, __LINE__, sizeof(buf), ret);
				} else {
					syslog(LOG_WARNING,
						"%s (%s:%d): snprintf needed more than %" PRIuSIZE " bytes: %d",
						__func__, __FILE__, __LINE__, sizeof(buf), ret);
				}
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
#   endif	/* HAVE_SD_WATCHDOG_ENABLED */

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
				if (syslog_is_disabled()) {
					fprintf(stderr,
						"%s (%s:%d): snprintf needed more than %" PRIuSIZE " bytes: %d",
						__func__, __FILE__, __LINE__, sizeof(buf), ret);
				} else {
					syslog(LOG_WARNING,
						"%s (%s:%d): snprintf needed more than %" PRIuSIZE " bytes: %d",
						__func__, __FILE__, __LINE__, sizeof(buf), ret);
				}
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
		upsdebugx(6, "%s: notify about state %s with "
			"libsystemd: lacking sd_notify()",
			__func__, str_upsnotify_state(state));
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
				upsdebugx(upsnotify_report_verbosity,
					"%s: failed to notify about state %s: "
					"no notification tech defined, "
					"will not spam more about it",
					__func__, str_upsnotify_state(state));
			upsnotify_reported_disabled_notech = 1;
			upsnotify_suggest_NUT_QUIET_INIT_UPSNOTIFY_once();
		} else {
			upsdebugx(6,
				"%s: failed to notify about state %s",
				__func__, str_upsnotify_state(state));
		}
	}

#if defined(WITH_LIBSYSTEMD) && (WITH_LIBSYSTEMD)
# if ! DEBUG_SYSTEMD_WATCHDOG
	if (state == NOTIFY_STATE_WATCHDOG && !upsnotify_reported_watchdog_systemd) {
		upsdebugx(upsnotify_report_verbosity,
			"%s: logged the systemd watchdog situation once, "
			"will not spam more about it", __func__);
		upsnotify_reported_watchdog_systemd = 1;
		upsnotify_suggest_NUT_QUIET_INIT_UPSNOTIFY_once();
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
#if 0
	const char *acinit_ver = NULL;
#endif
	/* Pass these as variables to avoid warning about never reaching one
	 * of compiled codepaths: */
	const char *compiler_ver = CC_VERSION;
	const char *config_flags = CONFIG_FLAGS;
	struct timeval		now;

	if (nut_debug_level < 1)
		return;

#if 0
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
/*
		// Triplet that was printed below:
		(acinit_ver ? " (release/snapshot of " : ""),
		(acinit_ver ? acinit_ver : ""),
		(acinit_ver ? ")" : ""),
*/
	}
#endif

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
		fprintf(stderr, "%4.0f.%06ld\t[D1] Network UPS Tools version %s%s%s%s %s%s\n",
			difftime(now.tv_sec, upslog_start.tv_sec),
			(long)(now.tv_usec - upslog_start.tv_usec),
			describe_NUT_VERSION_once(),
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
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG)) {
		syslog(LOG_DEBUG, "Network UPS Tools version %s%s%s%s %s%s",
			describe_NUT_VERSION_once(),
			(compiler_ver && *compiler_ver != '\0' ? " built with " : ""),
			(compiler_ver && *compiler_ver != '\0' ? compiler_ver : ""),
			(compiler_ver && *compiler_ver != '\0' ? " and" : ""),
			(config_flags && *config_flags != '\0' ? "configured with flags: " : "configured all by default guesswork"),
			(config_flags && *config_flags != '\0' ? config_flags : "")
		);
	}
}

static void vupslog(int priority, const char *fmt, va_list va, int use_strerror)
{
	int	ret, errno_orig = errno;
	size_t	bufsize = LARGEBUF;
	char	*buf = xcalloc(bufsize, sizeof(char));

	/* Be pedantic about our limitations */
	bufsize *= sizeof(char);

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
	/* Note: errors here can reset errno,
	 * so errno_orig is stashed beforehand */
	do {
		ret = vsnprintf(buf, bufsize, fmt, va);

		if ((ret < 0) || ((uintmax_t)ret >= (uintmax_t)bufsize)) {
			/* Try to adjust bufsize until we can print the
			 * whole message. Note that standards only require
			 * up to 4095 bytes to be manageable in printf-like
			 * methods:
			 *   The number of characters that can be produced
			 *   by any single conversion shall be at least 4095.
			 *   C17dr § 7.21.6.1 15
			 * In general, vsnprintf() is not specified to set
			 * errno on any condition (or to not implement a
			 * larger limit). Select implementations may do so
			 * though.
			 * Based on https://stackoverflow.com/a/72981237/4715872
			 */
			if (bufsize < SIZE_MAX/2) {
				size_t	newbufsize = bufsize*2;
				if (ret > 0) {
					/* Be generous, we snprintfcat() some
					 * suffixes, prefix a timestamp, etc. */
					if (((uintmax_t)ret) > (SIZE_MAX - LARGEBUF)) {
						goto vupslog_too_long;
					}
					newbufsize = (size_t)ret + LARGEBUF;
				} /* else: errno, e.g. ERANGE printing:
				   *  "...(34 => Result too large)" */
				if (nut_debug_level > 0) {
					fprintf(stderr, "WARNING: vupslog: "
						"vsnprintf needed more than %"
						PRIuSIZE " bytes: %d (%d => %s),"
						" extending to %" PRIuSIZE "\n",
						bufsize, ret,
						errno, strerror(errno),
						newbufsize);
				}
				bufsize = newbufsize;
				buf = xrealloc(buf, bufsize);
				continue;
			}
		} else {
			/* All fits well now; majority of use-cases should
			 * have nailed this on first try (envvar prints of
			 * longer fully-qualified PATHs, compilation settings
			 * reports etc. may need more). Even a LARGEBUF may
			 * still overflow some older syslog buffers and would
			 * be truncated there. At least stderr would see as
			 * complete a picture as we can give it.
			 */
			break;
		}

		/* Arbitrary limit, gotta stop somewhere */
		if (bufsize > LARGEBUF * 64) {
vupslog_too_long:
			if (syslog_is_disabled()) {
				fprintf(stderr, "vupslog: vsnprintf needed "
					"more than %" PRIuSIZE " bytes; logged "
					"output can be truncated",
					bufsize);
			} else {
				syslog(LOG_WARNING, "vupslog: vsnprintf needed "
					"more than %" PRIuSIZE " bytes; logged "
					"output can be truncated",
					bufsize);
			}
			break;
		}
	} while(1);
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	if (use_strerror) {
#ifdef WIN32
		LPVOID WinBuf;
		DWORD WinErr = GetLastError();
#endif

		snprintfcat(buf, bufsize, ": %s", strerror(errno_orig));

#ifdef WIN32
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

		snprintfcat(buf, bufsize, " [%s]", (char *)WinBuf);
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

			/* Print all in one shot, to better avoid
			 * mixed lines in parallel threads */
			fprintf(stderr, "%4.0f.%06ld\t%s\n",
				difftime(now.tv_sec, upslog_start.tv_sec),
				(long)(now.tv_usec - upslog_start.tv_usec),
				buf);
		} else {
			fprintf(stderr, "%s\n", buf);
		}
#ifdef WIN32
		fflush(stderr);
#endif
	}
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG))
		syslog(priority, "%s", buf);
	free(buf);
}


/* Return the default path for the directory containing configuration files */
const char * confpath(void)
{
	static const char *path = NULL;

	/* Cached by earlier calls? */
	if (path)
		return path;

	path = getenv("NUT_CONFPATH");

#ifdef WIN32
	if (path == NULL) {
		/* fall back to built-in pathname relative to binary/workdir */
		path = getfullpath(PATH_ETC);
	}
#endif

	/* We assume, here and elsewhere, that
	 * at least CONFPATH is always defined */
	if (path == NULL || *path == '\0')
		path = CONFPATH;

	return path;
}

/* Return the default path for the directory containing state files */
const char * dflt_statepath(void)
{
	static const char *path = NULL;

	/* Cached by earlier calls? */
	if (path)
		return path;

	path = getenv("NUT_STATEPATH");

#ifdef WIN32
	if (path == NULL) {
		/* fall back to built-in pathname relative to binary/workdir */
		path = getfullpath(PATH_VAR_RUN);
	}
#endif

	/* We assume, here and elsewhere, that
	 * at least STATEPATH is always defined */
	if (path == NULL || *path == '\0')
		path = STATEPATH;

	return path;
}

/* Return the alternate path for pid files, for processes running as non-root
 * Per documentation and configure script, the fallback value is the
 * state-file path as the daemon and drivers can write there too.
 * Note that this differs from PIDPATH that higher-privileged daemons, such
 * as upsmon, tend to use.
 */
const char * altpidpath(void)
{
	static const char *path = NULL;

	/* Cached by earlier calls? */
	if (path)
		return path;

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
	path = ALTPIDPATH;
#else
	/* With WIN32 in the loop, this may be more than a fallback to STATEPATH: */
	path = dflt_statepath();
#endif

	return path;
}

/* Return the main path for pid files, for processes running as root, such
 * as upsmon. Typically this is the built-in PIDPATH (from configure script)
 * but certain use-cases such as the test suite can override it with the
 * NUT_PIDPATH environment variable.
 */
const char * rootpidpath(void)
{
	static const char *path = NULL;

	/* Cached by earlier calls? */
	if (path)
		return path;

	path = getenv("NUT_PIDPATH");

#ifdef WIN32
	if (path == NULL) {
		/* fall back to built-in pathname relative to binary/workdir */
		path = getfullpath(PATH_ETC);
	}
#endif

	/* We assume, here and elsewhere, that
	 * at least PIDPATH is always defined */
	if (path == NULL || *path == '\0')
		path = PIDPATH;

	return path;
}

/* Die with a standard message if socket filename is too long */
void check_unix_socket_filename(const char *fn) {
	size_t len = strlen(fn);
	size_t max = NUT_PATH_MAX;
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
	 * We define NUT_PATH_MAX as the greatest
	 * value of them all.
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
	static int NUT_DEBUG_PID = -1;

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

		if (NUT_DEBUG_PID < 0) {
			NUT_DEBUG_PID = (getenv("NUT_DEBUG_PID") != NULL);
		}

		if (NUT_DEBUG_PID) {
			/* Note that we re-request PID every time as it can
			 * change during the run-time (forking etc.) */
			ret = snprintf(fmt2, sizeof(fmt2), "[D%d:%" PRIiMAX "] %s", level, (intmax_t)getpid(), fmt);
		} else {
			ret = snprintf(fmt2, sizeof(fmt2), "[D%d] %s", level, fmt);
		}
		if ((ret < 0) || (ret >= (int) sizeof(fmt2))) {
			if (syslog_is_disabled()) {
				fprintf(stderr, "upsdebug_with_errno: snprintf needed more than %d bytes",
					LARGEBUF);
			} else {
				syslog(LOG_WARNING, "upsdebug_with_errno: snprintf needed more than %d bytes",
					LARGEBUF);
			}
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
	static int NUT_DEBUG_PID = -1;

	if (nut_debug_level < level)
		return;

/* See comments above in upsdebug_with_errno() - they apply here too. */
	if (level > 0) {
		int ret;

		if (NUT_DEBUG_PID < 0) {
			NUT_DEBUG_PID = (getenv("NUT_DEBUG_PID") != NULL);
		}

		if (NUT_DEBUG_PID) {
			/* Note that we re-request PID every time as it can
			 * change during the run-time (forking etc.) */
			ret = snprintf(fmt2, sizeof(fmt2), "[D%d:%" PRIiMAX "] %s", level, (intmax_t)getpid(), fmt);
		} else {
			ret = snprintf(fmt2, sizeof(fmt2), "[D%d] %s", level, fmt);
		}

		if ((ret < 0) || (ret >= (int) sizeof(fmt2))) {
			if (syslog_is_disabled()) {
				fprintf(stderr, "upsdebugx: snprintf needed more than %d bytes",
					LARGEBUF);
			} else {
				syslog(LOG_WARNING, "upsdebugx: snprintf needed more than %d bytes",
					LARGEBUF);
			}
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
	/* Normally we enable SYSLOG and disable STDERR,
	 * unless NUT_DEBUG_SYSLOG envvar interferes as
	 * interpreted in syslog_is_disabled() method: */
	int	syslog_disabled = syslog_is_disabled(),
		stderr_disabled = (syslog_disabled == 0 || syslog_disabled == 2);

	if (xbit_test(upslog_flags, UPSLOG_STDERR_ON_FATAL))
		xbit_set(&upslog_flags, UPSLOG_STDERR);
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG_ON_FATAL)) {
		if (syslog_disabled) {
			/* FIXME: Corner case... env asked for stderr
			 * instead of syslog - should we care about
			 * UPSLOG_STDERR_ON_FATAL being not set? */
			if (!stderr_disabled)
				xbit_set(&upslog_flags, UPSLOG_STDERR);
		} else {
			xbit_set(&upslog_flags, UPSLOG_SYSLOG);
		}
	}

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
static const char * search_paths_builtin[] = {
	/* Use the library path (and bitness) provided during ./configure first */
	LIBDIR,
	"/usr"LIBDIR,		/* Note: this can lead to bogus strings like */
	"/usr/local"LIBDIR,	/* "/usr/usr/lib" which would be ignored quickly */
/* TOTHINK: Should AUTOTOOLS_* specs also be highly preferred?
 * Currently they are listed after the "legacy" hard-coded paths...
 */
#ifdef MULTIARCH_TARGET_ALIAS
# ifdef BUILD_64
	"/usr/lib/64/" MULTIARCH_TARGET_ALIAS,
	"/usr/lib64/" MULTIARCH_TARGET_ALIAS,
	"/lib/64/" MULTIARCH_TARGET_ALIAS,
	"/lib64/" MULTIARCH_TARGET_ALIAS,
# endif	/* MULTIARCH_TARGET_ALIAS && BUILD_64 */
	"/usr/lib/" MULTIARCH_TARGET_ALIAS,
	"/lib/" MULTIARCH_TARGET_ALIAS,
#endif	/* MULTIARCH_TARGET_ALIAS */
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

static const char ** search_paths = search_paths_builtin;

/* free this when a NUT program ends (common library is unloaded)
 * IFF it is not the built-in version. */
static void nut_free_search_paths(void) {
	if (search_paths == NULL) {
		search_paths = search_paths_builtin;
		return;
	}

	if (search_paths != search_paths_builtin) {
		size_t i;
		for (i = 0; search_paths[i] != NULL; i++) {
			free((char *)search_paths[i]);
		}
		free(search_paths);
		search_paths = search_paths_builtin;
	}
}

void nut_prepare_search_paths(void) {
	/* Produce the search_paths[] with minimal confusion allowing
	 * for faster walks and fewer logs in NUT applications:
	 * * only existing paths
	 * * discard lower-priority duplicates if a path is already listed
	 *
	 * NOTE: Currently this only trims info from search_paths_builtin[]
	 * but might later supplant iterations in the get_libname(),
	 * get_libname_in_pathset() and upsdebugx_report_search_paths()
	 * methods. Surely would make their code easier, but at a cost of
	 * probably losing detailed logging of where something came from...
	 */
	static int atexit_hooked = 0;
	size_t	count_builtin = 0, count_filtered = 0, i, j, index = 0;
	const char ** filtered_search_paths;
	DIR *dp;

	/* As a starting point, allow at least as many items as before */
	/* TODO: somehow extend (xrealloc?) if we mix other paths later */
	for (i = 0; search_paths_builtin[i] != NULL; i++) {}
	count_builtin = i + 1;	/* +1 for the NULL */

	/* Bytes inside should all be zeroed... */
	filtered_search_paths = xcalloc(count_builtin, sizeof(const char *));

	/* FIXME: here "count_builtin" means size of filtered_search_paths[]
	 * and may later be more, if we would consider other data sources */
	for (i = 0; search_paths_builtin[i] != NULL && count_filtered < count_builtin; i++) {
		int dupe = 0;
		const char *dirname = search_paths_builtin[i];

		if ((dp = opendir(dirname)) == NULL) {
			upsdebugx(5, "%s: SKIP "
				"unreachable directory #%" PRIuSIZE " : %s",
				__func__, index++, dirname);
			continue;
		}
		index++;

#if HAVE_DECL_REALPATH
		/* allocates the buffer we free() later */
		dirname = (const char *)realpath(dirname, NULL);
#endif

		/* Revise for duplicates */
		/* Note: (count_filtered == 0) means first existing dir seen, no hassle */
		for (j = 0; j < count_filtered; j++) {
			if (!strcmp(filtered_search_paths[j], dirname)) {
#if HAVE_DECL_REALPATH
				if (strcmp(search_paths_builtin[i], dirname)) {
					/* They differ, highlight it */
					upsdebugx(5, "%s: SKIP "
						"duplicate directory #%" PRIuSIZE " : %s (%s)",
						__func__, index, dirname,
						search_paths_builtin[i]);
				} else
#endif
				upsdebugx(5, "%s: SKIP "
					"duplicate directory #%" PRIuSIZE " : %s",
					__func__, index, dirname);

				dupe = 1;
#if HAVE_DECL_REALPATH
				free((char *)dirname);
				/* Have some valid value, for kicks (likely
				 * to be ignored in the code path below) */
				dirname = search_paths_builtin[i];
#endif
				break;
			}
		}

		if (!dupe) {
			upsdebugx(5, "%s: ADD[#%" PRIuSIZE "] "
				"existing unique directory: %s",
				__func__, count_filtered, dirname);
#if !HAVE_DECL_REALPATH
			/* Make a copy of table entry, else we have
			 * a dynamic result of realpath() made above.
			 */
			dirname = (const char *)xstrdup(dirname);
#endif
			filtered_search_paths[count_filtered++] = dirname;
		}	/* else: dirname was freed above (for realpath)
			 * or is a reference to the table entry; no need
			 * to free() it either way */

		closedir(dp);
	}

	/* If we mangled this before, forget the old result: */
	nut_free_search_paths();

	/* Better safe than sorry: */
	filtered_search_paths[count_filtered] = NULL;
	search_paths = filtered_search_paths;

	if (!atexit_hooked) {
		atexit(nut_free_search_paths);
		atexit_hooked = 1;
	}
}

void upsdebugx_report_search_paths(int level, int report_search_paths_builtin) {
	size_t	index;
	char	*s, *varname;
	const char ** reported_search_paths = (
		report_search_paths_builtin
		? search_paths_builtin
		: search_paths);

	if (nut_debug_level < level)
		return;

	upsdebugx(level, "Run-time loadable library search paths used by this build of NUT:");

	/* NOTE: Reporting order follows get_libname(), and
	 * while some values are individual paths, others can
	 * be "pathsets" (e.g. coming envvars) with certain
	 * platform-dependent separator characters. */
#ifdef BUILD_64
	varname = "LD_LIBRARY_PATH_64";
#else
	varname = "LD_LIBRARY_PATH_32";
#endif

	if (((s = getenv(varname)) != NULL) && strlen(s) > 0) {
		upsdebugx(level, "\tVia %s:\t%s", varname, s);
	}

	varname = "LD_LIBRARY_PATH";
	if (((s = getenv(varname)) != NULL) && strlen(s) > 0) {
		upsdebugx(level, "\tVia %s:\t%s", varname, s);
	}

	for (index = 0; reported_search_paths[index] != NULL; index++)
	{
		if (index == 0) {
			upsdebugx(level, "\tNOTE: Reporting %s built-in paths:",
				(report_search_paths_builtin ? "raw"
				 : "filtered (existing unique)"));
		}
		upsdebugx(level, "\tBuilt-in:\t%s", reported_search_paths[index]);
	}

#ifdef WIN32
	if (((s = getfullpath(NULL)) != NULL) && strlen(s) > 0) {
		upsdebugx(level, "\tWindows near EXE:\t%s", s);
	}

# ifdef PATH_LIB
	if (((s = getfullpath(PATH_LIB)) != NULL) && strlen(s) > 0) {
		upsdebugx(level, "\tWindows PATH_LIB (%s):\t%s", PATH_LIB, s);
	}
# endif

	if (((s = getfullpath("/../lib")) != NULL) && strlen(s) > 0) {
		upsdebugx(level, "\tWindows \"lib\" dir near EXE:\t%s", s);
	}

	varname = "PATH";
	if (((s = getenv(varname)) != NULL) && strlen(s) > 0) {
		upsdebugx(level, "\tWindows via %s:\t%s", varname, s);
	}
#endif
}

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
	char *libname_path = NULL, *libname_alias = NULL;
	char current_test_path[NUT_PATH_MAX];

	upsdebugx(3, "%s('%s', %" PRIuSIZE ", '%s', %i): Entering method...",
		__func__, base_libname, base_libname_length, dirname, index);

	memset(current_test_path, 0, sizeof(current_test_path));

	if ((dp = opendir(dirname)) == NULL) {
		if (index >= 0) {
			upsdebugx(5, "%s: NOT looking for lib %s in "
				"unreachable directory #%d : %s",
				__func__, base_libname, index, dirname);
		} else {
			upsdebugx(5, "%s: NOT looking for lib %s in "
				"unreachable directory : %s",
				__func__, base_libname, dirname);
		}
		return NULL;
	}

	if (index >= 0) {
		upsdebugx(4, "%s: Looking for lib %s in directory #%d : %s",
			__func__, base_libname, index, dirname);
	} else {
		upsdebugx(4, "%s: Looking for lib %s in directory : %s",
			__func__, base_libname, dirname);
	}

	/* TODO: Try a quick stat() first? */
	while ((dirp = readdir(dp)) != NULL)
	{
#if !HAVE_DECL_REALPATH
		struct stat	st;
#endif
		int compres;

		upsdebugx(5, "%s: Comparing lib %s with dirpath entry %s",
			__func__, base_libname, dirp->d_name);
		compres = strncmp(dirp->d_name, base_libname, base_libname_length);
		if (compres == 0) {
			/* avoid "*.dll.a", ".so.1.2.3" etc. */
			if (dirp->d_name[base_libname_length] != '\0') {
				if (!libname_alias) {
					libname_alias = xstrdup(dirp->d_name);
				}
				continue;
			}

			snprintf(current_test_path, sizeof(current_test_path), "%s/%s", dirname, dirp->d_name);
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
				char *p;
				for (p = current_test_path; *p != '\0' && (p - current_test_path) < LARGEBUF; p++) {
					if (*p == '/') *p = '\\';
				}
				upsdebugx(4, "%s: WIN32: re-checking with %s",
					__func__, current_test_path);
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
				upsdebugx(4, "%s: WIN32: re-checking with %s",
					__func__, current_test_path + 2);
				if (stat(current_test_path + 2, &st) == 0) {
					if (st.st_size > 0) {
						libname_path = xstrdup(current_test_path + 2);
					}
				}
			}
# endif /* WIN32 */
#endif  /* HAVE_DECL_REALPATH */

			upsdebugx(2, "Candidate path for lib %s is %s (realpath %s)",
				base_libname, current_test_path,
				NUT_STRARG(libname_path));
			if (libname_path != NULL)
				break;
		}
	} /* while iterating dir */

	closedir(dp);

	if (libname_alias) {
		if (!libname_path) {
			upsdebugx(1, "Got no strong candidate path for lib %s in %s"
				", but saw seemingly related names (are you missing"
				" a symbolic link, perhaps?) e.g.: %s",
				base_libname, dirname, libname_alias);
		}

		free(libname_alias);
	}

	return libname_path;
}

static char * get_libname_in_pathset(const char* base_libname, size_t base_libname_length, char* pathset, int *counter)
{
	/* Note: this method iterates specified pathset,
	 * so it increments the counter by reference.
	 * A copy of original pathset is used, because
	 * strtok() tends to modify its input! */
	char *libname_path = NULL;
	char *onedir = NULL;
	char* pathset_tmp;

	upsdebugx(3, "%s('%s', %" PRIuSIZE ", '%s', %i): Entering method...",
		__func__, base_libname, base_libname_length,
		NUT_STRARG(pathset),
		counter ? *counter : -1);

	if (!pathset || *pathset == '\0')
		return NULL;

	/* First call to tokenization passes the string, others pass NULL */
	pathset_tmp = xstrdup(pathset);
	upsdebugx(4, "%s: Looking for lib %s in a colon-separated path set",
		__func__, base_libname);
	while (NULL != (onedir = strtok( (onedir ? NULL : pathset_tmp), ":" ))) {
		libname_path = get_libname_in_dir(base_libname, base_libname_length, onedir, (*counter)++);
		if (libname_path != NULL)
			break;
	}
	free(pathset_tmp);

#ifdef WIN32
	/* Note: with mingw, the ":" separator above might have been resolvable */
	pathset_tmp = xstrdup(pathset);
	if (!libname_path) {
		onedir = NULL; /* probably is NULL already, but better ensure this */
		upsdebugx(4, "%s: WIN32: Looking for lib %s in a semicolon-separated path set",
			__func__, base_libname);
		while (NULL != (onedir = strtok( (onedir ? NULL : pathset_tmp), ";" ))) {
			libname_path = get_libname_in_dir(base_libname, base_libname_length, onedir, (*counter)++);
			if (libname_path != NULL)
				break;
		}
	}
	free(pathset_tmp);
#endif  /* WIN32 */

	return libname_path;
}

char * get_libname(const char* base_libname)
{
	/* NOTE: Keep changes to practical search order
	 * synced to upsdebugx_report_search_paths() */
	int index = 0, counter = 0;
	char *libname_path = NULL;
	size_t base_libname_length = strlen(base_libname);
	struct stat	st;

	upsdebugx(3, "%s('%s'): Entering method...", __func__, base_libname);

	/* First, check for an exact hit by absolute/relative path
	 * if `base_libname` includes path separator character(s) */
	if (xbasename(base_libname) != base_libname) {
		upsdebugx(4, "%s: Looking for lib %s by exact hit...",
			__func__, base_libname);
#if HAVE_DECL_REALPATH
		/* allocates the buffer we free() later */
		libname_path = realpath(base_libname, NULL);
		if (libname_path != NULL) {
			if (stat(libname_path, &st) == 0) {
				if (st.st_size > 0) {
					upsdebugx(2, "Looking for lib %s, found by exact hit",
						base_libname);
					goto found;
				}
			}

			/* else: does not actually exist */
			free(libname_path);
			libname_path = NULL;
		}
#endif	/* HAVE_DECL_REALPATH */

		/* Just check if candidate name is (points to?) valid file */
		if (stat(base_libname, &st) == 0) {
			if (st.st_size > 0) {
				libname_path = xstrdup(base_libname);
				upsdebugx(2, "Looking for lib %s, found by exact hit",
					base_libname);
				goto found;
			}
		}
	}

	/* Normally these envvars should not be set, but if the user insists,
	 * we should prefer the override... */
#ifdef BUILD_64
	upsdebugx(4, "%s: Looking for lib %s by path-set LD_LIBRARY_PATH_64...",
		__func__, base_libname);
	libname_path = get_libname_in_pathset(base_libname, base_libname_length, getenv("LD_LIBRARY_PATH_64"), &counter);
	if (libname_path != NULL) {
		upsdebugx(2, "Looking for lib %s, found in LD_LIBRARY_PATH_64",
			base_libname);
		goto found;
	}
#else
	upsdebugx(4, "%s: Looking for lib %s by path-set LD_LIBRARY_PATH_32...",
		__func__, base_libname);
	libname_path = get_libname_in_pathset(base_libname, base_libname_length, getenv("LD_LIBRARY_PATH_32"), &counter);
	if (libname_path != NULL) {
		upsdebugx(2, "Looking for lib %s, found in LD_LIBRARY_PATH_32",
			base_libname);
		goto found;
	}
#endif

	upsdebugx(4, "%s: Looking for lib %s by path-set LD_LIBRARY_PATH...",
		__func__, base_libname);
	libname_path = get_libname_in_pathset(base_libname, base_libname_length, getenv("LD_LIBRARY_PATH"), &counter);
	if (libname_path != NULL) {
		upsdebugx(2, "Looking for lib %s, found in LD_LIBRARY_PATH",
			base_libname);
		goto found;
	}

	upsdebugx(4, "%s: Looking for lib %s by search_paths[]...",
		__func__, base_libname);
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
		upsdebugx(4, "%s: WIN32: Looking for lib %s near EXE...",
			__func__, base_libname);
		libname_path = get_libname_in_dir(base_libname, base_libname_length, getfullpath(NULL), counter++);
	}

# ifdef PATH_LIB
	if (!libname_path) {
		upsdebugx(4, "%s: WIN32: Looking for lib %s via PATH_LIB...",
			__func__, base_libname);
		libname_path = get_libname_in_dir(base_libname, base_libname_length, getfullpath(PATH_LIB), counter++);
	}
# endif

	if (!libname_path) {
		/* Resolve "lib" dir near the one with current executable ("bin" or "sbin") */
		upsdebugx(4, "%s: WIN32: Looking for lib %s in a 'lib' dir near EXE...",
			__func__, base_libname);
		libname_path = get_libname_in_dir(base_libname, base_libname_length, getfullpath("/../lib"), counter++);
	}
#endif  /* WIN32 so far */

#ifdef WIN32
	/* Windows-specific: DLLs can be provided by common "PATH" envvar,
	 * at lowest search priority though (after EXE dir, system, etc.) */
	if (!libname_path) {
		upsdebugx(4, "%s: WIN32: Looking for lib %s in PATH",
			__func__, base_libname);
		libname_path = get_libname_in_pathset(base_libname, base_libname_length, getenv("PATH"), &counter);
	}
#endif  /* WIN32 */

found:
	upsdebugx(1, "Looking for lib %s, found %s",
		base_libname, NUT_STRARG(libname_path));
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

/**** REGEX helper methods ****/

int strcmp_null(const char *s1, const char *s2)
{
	if (s1 == NULL && s2 == NULL) {
		return 0;
	}

	if (s1 == NULL) {
		return -1;
	}

	if (s2 == NULL) {
		return 1;
	}

	return strcmp(s1, s2);
}

#if (defined HAVE_LIBREGEX && HAVE_LIBREGEX)
int compile_regex(regex_t **compiled, const char *regex, const int cflags)
{
	int	r;
	regex_t	*preg;

	if (regex == NULL) {
		*compiled = NULL;
		return 0;
	}

	preg = malloc(sizeof(*preg));
	if (!preg) {
		return -1;
	}

	r = regcomp(preg, regex, cflags);
	if (r) {
		free(preg);
		return -2;
	}

	*compiled = preg;

	return 0;
}

int match_regex(const regex_t *preg, const char *str)
{
	int	r;
	size_t	len = 0;
	char	*string;
	regmatch_t	match;

	if (!preg) {
		return 1;
	}

	if (!str) {
		string = xstrdup("");
	} else {
		/* skip leading whitespace */
		for (len = 0; len < strlen(str); len++) {

			if (!strchr(" \t\n", str[len])) {
				break;
			}
		}

		string = xstrdup(str+len);

		/* skip trailing whitespace */
		for (len = strlen(string); len > 0; len--) {

			if (!strchr(" \t\n", string[len-1])) {
				break;
			}
		}

		string[len] = '\0';
	}

	/* test the regular expression */
	r = regexec(preg, string, 1, &match, 0);
	free(string);
	if (r) {
		return 0;
	}

	/* check that the match is the entire string */
	if ((match.rm_so != 0) || (match.rm_eo != (int)len)) {
		return 0;
	}

	return 1;
}

int match_regex_hex(const regex_t *preg, const int n)
{
	char	buf[10];

	snprintf(buf, sizeof(buf), "%04x", (unsigned int)n);

	return match_regex(preg, buf);
}
#endif	/* HAVE_LIBREGEX */
