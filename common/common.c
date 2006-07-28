/* common.c - common useful functions

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

#include "common.h"

#include <ctype.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>

	int	nut_debug_level = 0;
	static	int	upslog_flags = UPSLOG_STDERR;

static void xbit_set(int *val, int flag)
{
	*val = (*val |= flag);
}

static void xbit_clear(int *val, int flag)
{
	*val = (*val ^= (*val & flag));
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
	int	opt;

	opt = LOG_PID;

	/* we need this to grab /dev/log before chroot */
#ifdef LOG_NDELAY
	opt |= LOG_NDELAY;
#endif

	openlog(progname, opt, LOG_FACILITY);
}

/* close ttys and become a daemon */
void background(void)
{
	int	pid;

	if ((pid = fork()) < 0)
		fatal_with_errno("Unable to enter background");

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
		fatal_with_errno("open /dev/null");
	dup(0);
	dup(0);

#ifdef HAVE_SETSID
	setsid();		/* make a new session to dodge signals */
#endif

	upslogx(LOG_INFO, "Startup successful");
}

/* do this here to keep pwd/grp stuff out of the main files */
struct passwd *get_user_pwent(const char *name)
{
	struct passwd *r;
	errno = 0;
	if ((r = getpwnam(name)))
		return r;

	/* POSIX does not specify that "user not found" is an error, so
	   some implementations of getpwnam() do not set errno when this
	   happens. */
	if (errno == 0)
		fatalx("user %s not found", name);
	else
		fatal_with_errno("getpwnam(%s)", name);
		
	return NULL;  /* to make the compiler happy */
}

/* change to the user defined in the struct */
void become_user(struct passwd *pw)
{
	/* if we can't switch users, then don't even try */
	if ((geteuid() != 0) && (getuid() != 0))
		return;

	if (getuid() == 0)
		if (seteuid(0))
			fatal_with_errno("getuid gave 0, but seteuid(0) failed");

	if (initgroups(pw->pw_name, pw->pw_gid) == -1)
		fatal_with_errno("initgroups");

	if (setgid(pw->pw_gid) == -1)
		fatal_with_errno("setgid");

	if (setuid(pw->pw_uid) == -1)
		fatal_with_errno("setuid");
}

/* drop down into a directory and throw away pointers to the old path */
void chroot_start(const char *path)
{
	if (chdir(path))
		fatal_with_errno("chdir(%s)", path);

	if (chroot(path))
		fatal_with_errno("chroot(%s)", path);

	if (chdir("/"))
		fatal_with_errno("chdir(/)");

	upsdebugx(1, "chrooted into %s", path);
}

/* drop off a pidfile for this process */
void writepid(const char *name)
{
	char	fn[SMALLBUF];
	FILE	*pidf;
	int	mask;

	/* use full path if present, else build filename in PIDPATH */
	if (*name == '/')
		snprintf(fn, sizeof(fn), "%s", name);
	else
		snprintf(fn, sizeof(fn), "%s/%s.pid", PIDPATH, name);

	mask = umask(022);
	pidf = fopen(fn, "w");

	if (pidf) {
		fprintf(pidf, "%d\n", (int) getpid());
		fclose(pidf);
	} else {
		upslog_with_errno(LOG_NOTICE, "writepid: fopen %s", fn);
	}

	umask(mask);
}

/* open pidfn, get the pid, then send it sig */
int sendsignalfn(const char *pidfn, int sig)
{
	char	buf[SMALLBUF];
	FILE	*pidf;
	int	pid, ret;

	pidf = fopen(pidfn, "r");
	if (!pidf) {
		upslog_with_errno(LOG_NOTICE, "fopen %s", pidfn);
		return -1;
	}

	fgets(buf, sizeof(buf), pidf);
	buf[strlen(buf)-1] = '\0';

	pid = strtol(buf, (char **)NULL, 10);

	if (pid < 2) {
		upslogx(LOG_NOTICE, "Ignoring invalid pid number %d", pid);
		return -1;
	}

	/* see if this is going to work first */
	ret = kill(pid, 0);

	if (ret < 0) {
		perror("kill");
		return -1;
	}

	/* now actually send it */
	ret = kill(pid, sig);

	if (ret < 0) {
		perror("kill");
		return -1;
	}

	return 0;
}

int snprintfcat(char *dst, size_t size, const char *fmt, ...)
{
	va_list ap;
	size_t len = strlen(dst);
	int ret;

	size--;
	assert(len <= size);

	va_start(ap, fmt);
	ret = vsnprintf(dst + len, size - len, fmt, ap);
	va_end(ap);

	dst[size] = '\0';
	return len + ret;
}

/* lazy way to send a signal if the program uses the PIDPATH */
int sendsignal(const char *progname, int sig)
{
	char	fn[SMALLBUF];

	snprintf(fn, sizeof(fn), "%s/%s.pid", PIDPATH, progname);

	return sendsignalfn(fn, sig);
}

const char *xbasename(const char *file)
{
	const char *p = strrchr(file, '/');

	if (p == NULL)
		return file;
	return p + 1;
}

static void vupslog(int priority, const char *fmt, va_list va, int use_strerror)
{
	int	ret;
	char	buf[LARGEBUF];

	ret = vsnprintf(buf, sizeof(buf), fmt, va);

	if ((ret < 0) || (ret >= (int) sizeof(buf)))
		syslog(LOG_WARNING, "vupslog: vsnprintf needed more than %d bytes",
			LARGEBUF);

	if (use_strerror)
		snprintfcat(buf, sizeof(buf), ": %s", strerror(errno));

	if (xbit_test(upslog_flags, UPSLOG_STDERR))
		fprintf(stderr, "%s\n", buf);
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG))
		syslog(priority, "%s", buf);
}

/* Return the default path for the directory containing configuration files */
const char * confpath(void) 
{
	const char * path;

	if ((path = getenv("NUT_CONFPATH")) == NULL)
		path = CONFPATH;

	return path;
}

/* Return the default path for the directory containing state files */
const char * dflt_statepath(void) 
{
	const char * path;

	if ((path = getenv("NUT_STATEPATH")) == NULL)
		path = STATEPATH;

	return path;
}

/* Return the alternate path for pid files */
const char * altpidpath(void) 
{
#ifdef ALTPIDPATH
	return ALTPIDPATH;
#else
	return dflt_statepath();
#endif
}

/* logs the formatted string to any configured logging devices + the output of strerror(errno) */
void upslog_with_errno(int priority, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vupslog(priority, fmt, va, 1);
	va_end(va);
}

/* logs the formatted string to any configured logging devices */
void upslogx(int priority, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vupslog(priority, fmt, va, 0);
	va_end(va);
}

void upsdebug_with_errno(int level, const char *fmt, ...)
{
	va_list va;
	
	if (nut_debug_level < level)
		return;

	va_start(va, fmt);
	vupslog(LOG_DEBUG, fmt, va, 1);
	va_end(va);
}

void upsdebugx(int level, const char *fmt, ...)
{
	va_list va;
	
	if (nut_debug_level < level)
		return;

	va_start(va, fmt);
	vupslog(LOG_DEBUG, fmt, va, 0);
	va_end(va);
}


/* Philippe Marzouk <philm@users.sourceforge.net> (dump_hex()) */
/* FIXME: to be reworked */
#define NIBBLE(_i)    (((_i) < 10) ? '0' + (_i) : 'A' + (_i) - 10)
void upsdebug_hex(int level, const char *msg, const char *buf, int len)
{
	int i;
	int nlocal;
	const char *pc;
	char *out;
	const char *start;
	char c;
	char line[100];
 
	start = buf;
	out = line;
	
	for (i = 0, pc = buf, nlocal = len; i < 16; i++, pc++)
	{
		if (nlocal > 0)
		{
			c = *pc;

			*out++ = NIBBLE ((c >> 4) & 0xF);
			*out++ = NIBBLE (c & 0xF);

			nlocal--;
		}
		else
		{
			*out++ = ' ';
			*out++ = ' ';
		}
		*out++ = ' ';
	}
	*out++ = 0;

	upsdebugx(level, "%s: (%d bytes) => %s", msg, len, line);

	buf += 16;
	len -= 16;
}

static void vfatal(const char *fmt, va_list va, int use_strerror)
{
	if (xbit_test(upslog_flags, UPSLOG_STDERR_ON_FATAL))
		xbit_set(&upslog_flags, UPSLOG_STDERR);
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG_ON_FATAL))
		xbit_set(&upslog_flags, UPSLOG_SYSLOG);

	vupslog(LOG_ERR, fmt, va, use_strerror);
}

void fatal_with_errno(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfatal(fmt, va, (errno > 0) ? 1 : 0);
	va_end(va);

	exit(EXIT_FAILURE);
}

void fatalx(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfatal(fmt, va, 0);
	va_end(va);

	exit(EXIT_FAILURE);
}

static const char *oom_msg = "Out of memory";

void *xmalloc(size_t size)
{
	void *p = malloc(size);

	if (p == NULL)
		fatal_with_errno("%s", oom_msg);
	return p;
}

void *xcalloc(size_t number, size_t size)
{
	void *p = calloc(number, size);

	if (p == NULL)
		fatal_with_errno("%s", oom_msg);
	return p;
}

void *xrealloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);

	if (p == NULL)
		fatal_with_errno("%s", oom_msg);
	return p;
}

char *xstrdup(const char *string)
{
	char *p = strdup(string);

	if (p == NULL)
		fatal_with_errno("%s", oom_msg);
	return p;
}

/* modify in - strip all trailing instances of <sep> */
void rtrim(char *in, char sep)
{
	char	*p = NULL;

	p = &in[strlen(in) - 1];

	while (p >= in) {
		if (*p != sep)
			return;

		*p-- = '\0';
	}
}
