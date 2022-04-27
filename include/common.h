/* common.h - prototypes for the common useful functions

   Copyright (C) 2000  Russell Kroll <rkroll@exploits.org>

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

#ifndef NUT_COMMON_H_SEEN
#define NUT_COMMON_H_SEEN 1

#include "config.h"		/* must be the first header */

/* Need this on AIX when using xlc to get alloca */
#ifdef _AIX
#pragma alloca
#endif /* _AIX */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <stdlib.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>	/* for strncasecmp() and strcasecmp() */
#endif
#ifdef HAVE_STRING_H
#include <string.h>	/* for strdup() and many others */
#endif

#include <syslog.h>
#include <unistd.h>
#include <assert.h>

#include "timehead.h"
#include "attribute.h"
#include "proto.h"
#include "str.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

extern const char *UPS_VERSION;

/* Use in code to notify the developers and quiesce the compiler that
 * (for this codepath) the argument or variable is unused intentionally.
 * void f(int x) {
 *   NUT_UNUSED_VARIABLE(x);
 *   ...
 * }
 *
 * Note that solutions which mark up function arguments or employ this or
 * that __attribute__ proved not portable enough for wherever NUT builds.
 */
#define NUT_UNUSED_VARIABLE(x) (void)(x)

/** @brief Default timeout (in seconds) for network operations, as used by `upsclient` and `nut-scanner`. */
#define DEFAULT_NETWORK_TIMEOUT		5

/** @brief Default timeout (in seconds) for retrieving the result of a `TRACKING`-enabled operation (e.g. `INSTCMD`, `SET VAR`). */
#define DEFAULT_TRACKING_TIMEOUT	10

/* get the syslog ready for us */
void open_syslog(const char *progname);

/* close ttys and become a daemon */
void background(void);

/* do this here to keep pwd/grp stuff out of the main files */
struct passwd *get_user_pwent(const char *name);

/* change to the user defined in the struct */
void become_user(struct passwd *pw);

/* drop down into a directory and throw away pointers to the old path */
void chroot_start(const char *path);

/* write a pid file - <name> is a full pathname *or* just the program name */
void writepid(const char *name);

/* parses string buffer into a pid_t if it passes
 * a few sanity checks; returns -1 on error */
pid_t parsepid(const char *buf);

/* send a signal to another running process */
int sendsignal(const char *progname, int sig);

int snprintfcat(char *dst, size_t size, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 3, 4)));

/* Report maximum platform value for the pid_t */
pid_t get_max_pid_t(void);

/* send sig to pid after some sanity checks, returns
 * -1 for error, or zero for a successfully sent signal */
int sendsignalpid(pid_t pid, int sig);

/* open <pidfn>, get the pid, then send it <sig>
 * returns zero for successfully sent signal,
 * negative for errors:
 * -3   PID file not found
 * -2   PID file not parsable
 * -1   Error sending signal
 */
int sendsignalfn(const char *pidfn, int sig);

const char *xbasename(const char *file);

/* enable writing upslog_with_errno() and upslogx() type messages to
   the syslog */
void syslogbit_set(void);

/* Return the default path for the directory containing configuration files */
const char * confpath(void);

/* Return the default path for the directory containing state files */
const char * dflt_statepath(void);

/* Return the alternate path for pid files */
const char * altpidpath(void);

/* Die with a standard message if socket filename is too long */
void check_unix_socket_filename(const char *fn);

/* upslog*() messages are sent to syslog always;
 * their life after that is out of NUT's control */
void upslog_with_errno(int priority, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));
void upslogx(int priority, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));

/* upsdebug*() messages are only logged if debugging
 * level is high enough. To speed up a bit (minimize
 * passing of ultimately ignored data trough the stack)
 * these are "hidden" implementations wrapped into
 * macros for earlier routine names spread around the
 * codebase, they would check debug level first and
 * only if logging should happen - call the routine
 * and pass around pointers and other data.
 */
void s_upsdebug_with_errno(int level, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));
void s_upsdebugx(int level, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));
void s_upsdebug_hex(int level, const char *msg, const void *buf, size_t len);
void s_upsdebug_ascii(int level, const char *msg, const void *buf, size_t len);
/* These macros should help avoid run-time overheads
 * passing data for messages nobody would ever see.
 *
 * Also NOTE: the "level" may be specified by callers in various ways,
 * e.g. as a "X ? Y : Z" style expression; to catch those expansions
 * transparently we hide them into parentheses as "(label)".
 *
 * For stricter C99 compatibility, all parameters specified to a macro
 * must be populated by caller (so we do not handle "fmt, args..." where
 * the args part may be skipped by caller because fmt is a fixed string).
 * Note it is then up to the caller (and compiler checks) that at least
 * one argument is provided, the format string (maybe fixed) -- as would
 * be required by the actual s_upsdebugx*() method after macro evaluation.
 */
#define upsdebug_with_errno(level, ...) \
	do { if (nut_debug_level >= (level)) { s_upsdebug_with_errno((level), __VA_ARGS__); } } while(0)
#define upsdebugx(level, ...) \
	do { if (nut_debug_level >= (level)) { s_upsdebugx((level), __VA_ARGS__); } } while(0)
#define upsdebug_hex(level, msg, buf, len) \
	do { if (nut_debug_level >= (level)) { s_upsdebug_hex((level), msg, buf, len); } } while(0)
#define upsdebug_ascii(level, msg, buf, len) \
	do { if (nut_debug_level >= (level)) { s_upsdebug_ascii((level), msg, buf, len); } } while(0)

void fatal_with_errno(int status, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3))) __attribute__((noreturn));
void fatalx(int status, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3))) __attribute__((noreturn));

extern int nut_debug_level;
extern int nut_log_level;

void *xmalloc(size_t size);
void *xcalloc(size_t number, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *string);

ssize_t select_read(const int fd, void *buf, const size_t buflen, const time_t d_sec, const suseconds_t d_usec);
ssize_t select_write(const int fd, const void *buf, const size_t buflen, const time_t d_sec, const suseconds_t d_usec);

char * get_libname(const char* base_libname);

/* Buffer sizes used for various functions */
#define SMALLBUF	512
#define LARGEBUF	1024

/** @brief (Minimum) Size that a string must have to hold a UUID4 (i.e. UUID4 length + the terminating null character). */
#define UUID4_LEN	37

/* Provide declarations for getopt() global variables */

#ifdef NEED_GETOPT_H
#include <getopt.h>
#else
#ifdef NEED_GETOPT_DECLS
extern char *optarg;
extern int optind;
#endif /* NEED_GETOPT_DECLS */
#endif /* HAVE_GETOPT_H */

/* logging flags: bitmask! */

#define UPSLOG_STDERR		0x0001
#define UPSLOG_SYSLOG		0x0002
#define UPSLOG_STDERR_ON_FATAL	0x0004
#define UPSLOG_SYSLOG_ON_FATAL	0x0008

#ifndef HAVE_SETEUID
#	define seteuid(x) setresuid(-1,x,-1)    /* Works for HP-UX 10.20 */
#	define setegid(x) setresgid(-1,x,-1)    /* Works for HP-UX 10.20 */
#endif

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* NUT_COMMON_H_SEEN */
