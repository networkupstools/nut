/* serial.c - common serial port functions for Network UPS Tools drivers

   Copyright (C) 2003  Russell Kroll <rkroll@exploits.org>

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
#include "serial.h"
#include "main.h"
#include "attribute.h"

#ifndef WIN32
#include <grp.h>
#include <pwd.h>
#include <sys/ioctl.h>
#endif
#include <ctype.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_UU_LOCK
#include <libutil.h>
#endif

#include "nut_stdint.h"

	static unsigned int	comm_failures = 0;

static void ser_open_error(const char *port)
	__attribute__((noreturn));

static void ser_open_error(const char *port)
{
	struct	stat	fs;
#ifndef WIN32
	struct	passwd	*user;
	struct	group	*group;
#endif

	printf("\n");

	printf("Unable to open %s: %s\n\n", port, strerror(errno));

	/* check for bad port definitions first */
	if (stat(port, &fs)) {
		printf("Things to try:\n\n");
		printf(" - Check 'port=' in ups.conf\n\n");
		printf(" - Check owner/permissions of all parts of path\n\n");
		fatalx(EXIT_FAILURE, "Fatal error: unusable configuration");
	}

/* TODO */
#ifndef WIN32
	user = getpwuid(getuid());

	if (user)
		printf("  Current user id: %s (%d)\n",
			user->pw_name, (int) user->pw_uid);

	user = getpwuid(fs.st_uid);

	if (user)
		printf("Serial port owner: %s (%d)\n",
			user->pw_name, (int) fs.st_uid);

	group = getgrgid(fs.st_gid);

	if (group)
		printf("Serial port group: %s (%d)\n",
			group->gr_name, (int) fs.st_gid);

#endif
	printf("     Mode of port: %04o\n\n", (int) fs.st_mode & 07777);

	printf("Things to try:\n\n");
	printf(" - Use another port (with the right permissions)\n\n");
	printf(" - Fix the port owner/group or permissions on this port\n\n");
	printf(" - Run this driver as another user (upsdrvctl -u or 'user=...' in ups.conf).\n");
	printf("   See upsdrvctl(8) and ups.conf(5).\n\n");

	fatalx(EXIT_FAILURE, "Fatal error: unusable configuration");
}

static void lock_set(TYPE_FD fd, const char *port)
{
	int	ret;

	if (fd == ERROR_FD)
		fatal_with_errno(EXIT_FAILURE, "lock_set: programming error: fd = %d", PRINT_FD(fd));

	if (do_lock_port == 0)
		return;

#ifdef HAVE_UU_LOCK
	ret = uu_lock(xbasename(port));

	if (ret != 0)
		fatalx(EXIT_FAILURE, "Can't uu_lock %s: %s", xbasename(port),
			uu_lockerr(ret));

#elif defined(HAVE_FLOCK)

	ret = flock(fd, LOCK_EX | LOCK_NB);

	if (ret != 0)
		fatalx(EXIT_FAILURE, "%s is locked by another process", port);

#elif defined(HAVE_LOCKF)

	lseek(fd, 0L, SEEK_SET);

	ret = lockf(fd, F_TLOCK, 0L);

	if (ret != 0)
		fatalx(EXIT_FAILURE, "%s is locked by another process", port);

#else

	upslog_with_errno(LOG_WARNING, "Warning: no locking method is available");

#endif
}

/* Non fatal version of ser_open */
TYPE_FD ser_open_nf(const char *port)
{
	TYPE_FD	fd;

	fd = open(port, O_RDWR | O_NOCTTY | O_EXCL | O_NONBLOCK);

	if (fd == ERROR_FD)
		return ERROR_FD;

	lock_set(fd, port);

	return fd;
}

TYPE_FD ser_open(const char *port)
{
	TYPE_FD res;

	res = ser_open_nf(port);
	if(res == ERROR_FD) {
		ser_open_error(port);
	}

	return res;
}

int ser_set_speed_nf(TYPE_FD fd, const char *port, speed_t speed)
{
	struct	termios	tio;
	NUT_UNUSED_VARIABLE(port);

	if (tcgetattr(fd, &tio) != 0) {
		return -1;
	}

	tio.c_cflag = CS8 | CLOCAL | CREAD;
	tio.c_iflag = IGNPAR;
	tio.c_oflag = 0;
	tio.c_lflag = 0;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;

#ifdef HAVE_CFSETISPEED
	cfsetispeed(&tio, speed);
	cfsetospeed(&tio, speed);
#else
#error This system lacks cfsetispeed() and has no other means to set the speed
#endif

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &tio);

	return 0;
}
int ser_set_speed(TYPE_FD fd, const char *port, speed_t speed)
{
	int res;

	res = ser_set_speed_nf(fd,port,speed);
	if(res == -1) {
		fatal_with_errno(EXIT_FAILURE, "tcgetattr(%s)", port);
	}

	return 0;
}

#ifndef WIN32
static int ser_set_control(TYPE_FD fd, int line, int state)
{
	if (state) {
		return ioctl(fd, TIOCMBIS, &line);
	} else {
		return ioctl(fd, TIOCMBIC, &line);
	}
}

int ser_set_dtr(TYPE_FD fd, int state)
{
	return ser_set_control(fd, TIOCM_DTR, state);
}

int ser_set_rts(int fd, int state)
{
	return ser_set_control(fd, TIOCM_RTS, state);
}
#else
int ser_set_dtr(TYPE_FD fd, int state)
{
	DWORD action;

	if(state == 0) {
		action = CLRDTR;
	}
	else {
		action = SETDTR;
	}

	/* Success */
	if( EscapeCommFunction(fd->handle,action) != 0) {
		return 0;
	}
	return -1;
}

int ser_set_rts(TYPE_FD fd, int state)
{
	DWORD action;

	if(state == 0) {
		action = CLRRTS;
	}
	else {
		action = SETRTS;
	}
	/* Success */
	if( EscapeCommFunction(fd->handle,action) != 0) {
		return 0;
	}
	return -1;
}
#endif

#ifndef WIN32
static int ser_get_control(TYPE_FD fd, int line)
{
	int	flags;

	ioctl(fd, TIOCMGET, &flags);

	return (flags & line);
}

int ser_get_dsr(TYPE_FD fd)
{
	return ser_get_control(fd, TIOCM_DSR);
}

int ser_get_cts(TYPE_FD fd)
{
	return ser_get_control(fd, TIOCM_CTS);
}

int ser_get_dcd(TYPE_FD fd)
{
	return ser_get_control(fd, TIOCM_CD);
}
#else
int ser_get_dsr(TYPE_FD fd)
{
	int flags;

	w32_getcomm(fd->handle, &flags);
	return (flags & TIOCM_DSR);
}

int ser_get_cts(TYPE_FD fd)
{
	int flags;

	w32_getcomm(fd->handle, &flags);
	return (flags & TIOCM_CTS);
}

int ser_get_dcd(TYPE_FD fd)
{
	int flags;

	w32_getcomm(fd->handle, &flags);
	return (flags & TIOCM_CD);
}

#endif

int ser_flush_io(TYPE_FD fd)
{
	return tcflush(fd, TCIOFLUSH);
}

int ser_close(TYPE_FD fd, const char *port)
{
	if (fd == ERROR_FD)
		fatal_with_errno(EXIT_FAILURE, "ser_close: programming error: fd=%d port=%s", PRINT_FD(fd), port);

	if (close(fd) != 0)
		return -1;

#ifdef HAVE_UU_LOCK
	if (do_lock_port)
		uu_unlock(xbasename(port));
#endif

	return 0;
}

<<<<<<< HEAD
#ifndef WIN32
ssize_t ser_send_char(int fd, unsigned char ch)
#else
ssize_t ser_send_char(HANDLE fd, unsigned char ch)
#endif
=======
int ser_send_char(TYPE_FD fd, unsigned char ch)
>>>>>>> First implementation of termios functions
{
	return ser_send_buf_pace(fd, 0, &ch, 1);
}

<<<<<<< HEAD
#ifndef WIN32
static ssize_t send_formatted(int fd, const char *fmt, va_list va, useconds_t d_usec)
#else
static ssize_t send_formatted(HANDLE fd, const char *fmt, va_list va, useconds_t d_usec)
#endif
=======
static int send_formatted(TYPE_FD fd, const char *fmt, va_list va, unsigned long d_usec)
>>>>>>> First implementation of termios functions
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
	ret = vsnprintf(buf, sizeof(buf), fmt, va);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	if (ret >= (int)sizeof(buf)) {
		upslogx(LOG_WARNING, "vsnprintf needed more than %d bytes", (int)sizeof(buf));
	}

	return ser_send_buf_pace(fd, d_usec, buf, strlen(buf));
}

/* send the results of the format string with d_usec delay after each char */
<<<<<<< HEAD
#ifndef WIN32
ssize_t ser_send_pace(int fd, useconds_t d_usec, const char *fmt, ...)
#else
ssize_t ser_send_pace(HANDLE fd, useconds_t d_usec, const char *fmt, ...)
#endif
=======
int ser_send_pace(TYPE_FD fd, unsigned long d_usec, const char *fmt, ...)
>>>>>>> First implementation of termios functions
{
	ssize_t	ret;
	va_list	ap;

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
	ret = send_formatted(fd, fmt, ap, d_usec);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	va_end(ap);

	return ret;
}

/* send the results of the format string with no delay */
<<<<<<< HEAD
#ifndef WIN32
ssize_t ser_send(int fd, const char *fmt, ...)
#else
ssize_t ser_send(HANDLE fd, const char *fmt, ...)
#endif
=======
int ser_send(TYPE_FD fd, const char *fmt, ...)
>>>>>>> First implementation of termios functions
{
	ssize_t	ret;
	va_list	ap;

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
	ret = send_formatted(fd, fmt, ap, 0);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	va_end(ap);

	return ret;
}

/* send buflen bytes from buf with no delay */
<<<<<<< HEAD
#ifndef WIN32
ssize_t ser_send_buf(int fd, const void *buf, size_t buflen)
#else
ssize_t ser_send_buf(HANDLE fd, const void *buf, size_t buflen)
#endif
=======
int ser_send_buf(TYPE_FD fd, const void *buf, size_t buflen)
>>>>>>> First implementation of termios functions
{
	return ser_send_buf_pace(fd, 0, buf, buflen);
}

/* send buflen bytes from buf with d_usec delay after each char */
<<<<<<< HEAD
ssize_t ser_send_buf_pace(int fd, useconds_t d_usec, const void *buf,
=======
int ser_send_buf_pace(TYPE_FD fd, unsigned long d_usec, const void *buf,
>>>>>>> First implementation of termios functions
	size_t buflen)
{
	ssize_t	ret = 0;
	ssize_t	sent;
	const char	*data = buf;

	assert(buflen < SSIZE_MAX);
	for (sent = 0; sent < (ssize_t)buflen; sent += ret) {
		/* Conditions above ensure that (buflen - sent) > 0 below */
		ret = write(fd, &data[sent], (d_usec == 0) ? (size_t)((ssize_t)buflen - sent) : 1);

		if (ret < 1) {
			return ret;
		}

		usleep(d_usec);
	}

	return sent;
}

<<<<<<< HEAD
	return sent;
}
#endif

#ifndef WIN32
ssize_t ser_get_char(int fd, void *ch, time_t d_sec, useconds_t d_usec)
#else
ssize_t ser_get_char(HANDLE fd, void *ch, time_t d_sec, useconds_t d_usec)
#endif
=======
int ser_get_char(TYPE_FD fd, void *ch, long d_sec, long d_usec)
>>>>>>> First implementation of termios functions
{
	/* Per standard below, we can cast here, because required ranges are
	 * effectively the same (and signed -1 for suseconds_t), and at most long:
	 * https://pubs.opengroup.org/onlinepubs/009604599/basedefs/sys/types.h.html
	 */
	return select_read(fd, ch, 1, d_sec, (suseconds_t)d_usec);
}

<<<<<<< HEAD
#ifndef WIN32
ssize_t ser_get_buf(int fd, void *buf, size_t buflen, time_t d_sec, useconds_t d_usec)
#else
ssize_t ser_get_buf(HANDLE fd, void *buf, size_t buflen, time_t d_sec, useconds_t d_usec)
#endif
=======
int ser_get_buf(TYPE_FD fd, void *buf, size_t buflen, long d_sec, long d_usec)
>>>>>>> First implementation of termios functions
{
	memset(buf, '\0', buflen);

	return select_read(fd, buf, buflen, d_sec, (suseconds_t)d_usec);
}

/* keep reading until buflen bytes are received or a timeout occurs */
<<<<<<< HEAD
#ifndef WIN32
ssize_t ser_get_buf_len(int fd, void *buf, size_t buflen, time_t d_sec, useconds_t d_usec)
#else
ssize_t ser_get_buf_len(HANDLE fd, void *buf, size_t buflen, time_t d_sec, useconds_t d_usec)
#endif
=======
int ser_get_buf_len(TYPE_FD fd, void *buf, size_t buflen, long d_sec, long d_usec)
>>>>>>> First implementation of termios functions
{
	ssize_t	ret;
	ssize_t	recv;
	char	*data = buf;

	assert(buflen < SSIZE_MAX);
	memset(buf, '\0', buflen);

	for (recv = 0; recv < (ssize_t)buflen; recv += ret) {

		ret = select_read(fd, &data[recv],
			(size_t)((ssize_t)buflen - recv),
			d_sec, (suseconds_t)d_usec);

		if (ret < 1) {
			return ret;
		}
	}

	return recv;
}

/* reads a line up to <endchar>, discarding anything else that may follow,
   with callouts to the handler if anything matches the alertset */
<<<<<<< HEAD
#ifndef WIN32
ssize_t ser_get_line_alert(int fd, void *buf, size_t buflen, char endchar,
	const char *ignset, const char *alertset, void handler(char ch),
	time_t d_sec, useconds_t d_usec)
#else
ssize_t ser_get_line_alert(HANDLE fd, void *buf, size_t buflen, char endchar,
	const char *ignset, const char *alertset, void handler(char ch),
	time_t d_sec, useconds_t d_usec)
#endif
=======
int ser_get_line_alert(TYPE_FD fd, void *buf, size_t buflen, char endchar,
	const char *ignset, const char *alertset, void handler(char ch), 
	long d_sec, long d_usec)
>>>>>>> First implementation of termios functions
{
	ssize_t	i, ret;
	char	tmp[64];
	char	*data = buf;
	ssize_t	count = 0, maxcount;

	assert(buflen < SSIZE_MAX && buflen > 0);
	memset(buf, '\0', buflen);

	maxcount = (ssize_t)buflen - 1;		/* for trailing \0 */

	while (count < maxcount) {
		ret = select_read(fd, tmp, sizeof(tmp), d_sec, (suseconds_t)d_usec);

		if (ret < 1) {
			return ret;
		}

		for (i = 0; i < ret; i++) {

			if ((count == maxcount) || (tmp[i] == endchar)) {
				return count;
			}

			if (strchr(ignset, tmp[i]))
				continue;

			if (strchr(alertset, tmp[i])) {
				if (handler)
					handler(tmp[i]);

				continue;
			}

			data[count++] = tmp[i];
		}
	}

	return count;
}

/* as above, only with no alertset handling (just a wrapper) */
<<<<<<< HEAD
#ifndef WIN32
ssize_t ser_get_line(int fd, void *buf, size_t buflen, char endchar,
	const char *ignset, time_t d_sec, useconds_t d_usec)
#else
ssize_t ser_get_line(HANDLE fd, void *buf, size_t buflen, char endchar,
	const char *ignset, time_t d_sec, useconds_t d_usec)
#endif
=======
int ser_get_line(TYPE_FD fd, void *buf, size_t buflen, char endchar,
	const char *ignset, long d_sec, long d_usec)
>>>>>>> First implementation of termios functions
{
	return ser_get_line_alert(fd, buf, buflen, endchar, ignset, "", NULL,
		d_sec, d_usec);
}

<<<<<<< HEAD
#ifndef WIN32
ssize_t ser_flush_in(int fd, const char *ignset, int verbose)
#else
ssize_t ser_flush_in(HANDLE fd, const char *ignset, int verbose)
#endif
=======
int ser_flush_in(TYPE_FD fd, const char *ignset, int verbose)
>>>>>>> First implementation of termios functions
{
	ssize_t	ret, extra = 0;
	char	ch;

	while ((ret = ser_get_char(fd, &ch, 0, 0)) > 0) {

		if (strchr(ignset, ch))
			continue;

		extra++;

		if (verbose == 0)
			continue;

		if (isprint((unsigned char)ch & 0xFF))
			upslogx(LOG_INFO, "ser_flush_in: read char %c",	ch);
		else
			upslogx(LOG_INFO, "ser_flush_in: read char 0x%02x", ch);
	}

	return extra;
}

void ser_comm_fail(const char *fmt, ...)
{
	int	ret;
	char	why[SMALLBUF];
	va_list	ap;

	/* this means we're probably here because select was interrupted */
	if (exit_flag != 0)
		return;		/* ignored, since we're about to exit anyway */

	comm_failures++;

	if ((comm_failures == SER_ERR_LIMIT) ||
		((comm_failures % SER_ERR_RATE) == 0)) {
		upslogx(LOG_WARNING, "Warning: excessive comm failures, "
			"limiting error reporting");
	}

	/* once it's past the limit, only log once every SER_ERR_LIMIT calls */
	if ((comm_failures > SER_ERR_LIMIT) &&
		((comm_failures % SER_ERR_LIMIT) != 0))
		return;

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
	ret = vsnprintf(why, sizeof(why), fmt, ap);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	if ((ret < 1) || (ret >= (int) sizeof(why)))
		upslogx(LOG_WARNING, "ser_comm_fail: vsnprintf needed "
			"more than %d bytes", (int)sizeof(why));

	upslogx(LOG_WARNING, "Communications with UPS lost: %s", why);
}

void ser_comm_good(void)
{
	if (comm_failures == 0)
		return;

	upslogx(LOG_NOTICE, "Communications with UPS re-established");
	comm_failures = 0;
}
