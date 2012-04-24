#ifndef NUT_COMMON_H
#define NUT_COMMON_H

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
#ifndef WIN32
#include <syslog.h>
#else
/* We need windows XP or above (for getaddrinfo and freeaddrinfo) */
#ifndef WINVER
#define  WINVER 0x0501
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <unistd.h>
#include <assert.h>

#include "timehead.h"
#include "attribute.h"
#include "proto.h"

#ifdef WIN32
typedef struct serial_handler_s {
	HANDLE handle;
	OVERLAPPED io_status;
	int     overlapped_armed;

	unsigned int vmin_;
	unsigned int vtime_;
	unsigned int r_binary;
	unsigned int w_binary;
} serial_handler_t;
#endif

extern const char *UPS_VERSION;

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

char *rtrim(char *in, const char sep);
char* ltrim(char *in, const char sep);

#ifndef WIN32
int select_read(const int fd, void *buf, const size_t buflen, const long d_sec, const long d_usec);
#else
int select_read(serial_handler_t * fd, void *buf, const size_t buflen, const long d_sec, const long d_usec);
#endif
int select_write(const int fd, const void *buf, const size_t buflen, const long d_sec, const long d_usec);

/* Buffer sizes used for various functions */
#define SMALLBUF	512
#define LARGEBUF	1024

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

#ifdef WIN32
/* FIXME : this might not be the optimal mapping between syslog and ReportEvent*/
#define LOG_ERR 	EVENTLOG_ERROR_TYPE
#define LOG_INFO 	EVENTLOG_INFORMATION_TYPE
#define LOG_DEBUG	EVENTLOG_WARNING_TYPE
#define LOG_NOTICE	EVENTLOG_INFORMATION_TYPE
#define LOG_ALERT	EVENTLOG_ERROR_TYPE
#define LOG_WARNING	EVENTLOG_WARNING_TYPE
#define LOG_CRIT	EVENTLOG_ERROR_TYPE
#define LOG_EMERG	EVENTLOG_ERROR_TYPE

#define closelog()

#define SVCNAME TEXT("Network UPS Tools")
#define EVENTLOG_PIPE_NAME TEXT("\\\\.\\pipe\\nut")
#define UPSMON_PIPE_NAME TEXT("\\\\.\\pipe\\upsmon")
#define UPSD_PIPE_NAME TEXT("\\\\.\\pipe\\upsd")

char * getfullpath(char * relative_path);
#define PATH_ETC	"\\..\\etc"
#define PATH_VAR_RUN "\\..\\var\\run"
#define PATH_SHARE "\\..\\share"
#define PATH_BIN "\\..\\bin"
#define PATH_SBIN "\\..\\sbin"
#endif /* WIN32*/

#ifndef HAVE_USLEEP
int __cdecl usleep(unsigned int useconds);
#endif /* HAVE_USLEEP */

#endif /* NUT_COMMON_H */
