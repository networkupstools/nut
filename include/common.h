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
#include <signal.h>
#include <stdlib.h>
#include <string.h>
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

/* send a signal to another running process */
int sendsignal(const char *progname, int sig);

int snprintfcat(char *dst, size_t size, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 3, 4)));

/* open <pidfn>, get the pid, then send it <sig> */
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

void upslog_with_errno(int priority, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));
void upslogx(int priority, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));
void upsdebug_with_errno(int level, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));
void upsdebugx(int level, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));
void upsdebug_hex(int level, const char *msg, const void *buf, int len);
void upsdebug_ascii(int level, const char *msg, const void *buf, int len);

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

ssize_t select_read(const int fd, void *buf, const size_t buflen, const long d_sec, const long d_usec);
ssize_t select_write(const int fd, const void *buf, const size_t buflen, const long d_sec, const long d_usec);

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
