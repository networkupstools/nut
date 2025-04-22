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
#endif	/* !WIN32 */
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
#endif	/* !WIN32 */

	printf("\n");

	printf("Unable to open %s: %s\n\n", port, strerror(errno));

	/* check for bad port definitions first */
	if (stat(port, &fs)) {
		printf("Things to try:\n\n");
		printf(" - Check 'port=' in ups.conf\n\n");
		printf(" - Check owner/permissions of all parts of path\n\n");
		fatalx(EXIT_FAILURE, "Fatal error: unusable configuration");
	}

/* TODO NUT_WIN32_INCOMPLETE? */
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
#endif	/* !WIN32 */

	printf("     Mode of port: %04o\n\n", (unsigned int) fs.st_mode & 07777);

	printf("Things to try:\n\n");
	printf(" - Use another port (with the right permissions)\n\n");
	printf(" - Fix the port owner/group or permissions on this port\n\n");
	printf(" - Run this driver as another user (upsdrvctl -u or 'user=...' in ups.conf).\n");
	printf("   See upsdrvctl(8) and ups.conf(5).\n\n");

	fatalx(EXIT_FAILURE, "Fatal error: unusable configuration");
}

static void lock_set(TYPE_FD_SER fd, const char *port)
{
	int	ret;

	if (INVALID_FD_SER(fd)) {
#ifndef WIN32
		fatal_with_errno(EXIT_FAILURE, "lock_set: programming error: fd = %d", fd);
#else	/* WIN32 */
		fatal_with_errno(EXIT_FAILURE, "lock_set: programming error: struct = %p", fd);
#endif	/* WIN32 */
	}

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

	NUT_UNUSED_VARIABLE(port);
	ret = 0; /* Make compiler happy */
	ret = ret;
	upslog_with_errno(LOG_WARNING, "Warning: no locking method is available");

#endif
}

/* Non fatal version of ser_open */
TYPE_FD_SER ser_open_nf(const char *port)
{
	TYPE_FD_SER	fd;

	fd = open(port, O_RDWR | O_NOCTTY | O_EXCL | O_NONBLOCK);

	if (INVALID_FD_SER(fd)) {
		return ERROR_FD_SER;
	}

	lock_set(fd, port);

	return fd;
}

TYPE_FD_SER ser_open(const char *port)
{
	TYPE_FD_SER res;

	res = ser_open_nf(port);
	if (INVALID_FD_SER(res)) {
		ser_open_error(port);
	}

	return res;
}

int ser_set_speed_nf(TYPE_FD_SER fd, const char *port, speed_t speed)
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

int ser_set_speed(TYPE_FD_SER fd, const char *port, speed_t speed)
{
	int res;

	res = ser_set_speed_nf(fd,port,speed);
	if(res == -1) {
		fatal_with_errno(EXIT_FAILURE, "tcgetattr(%s)", port);
	}

	return 0;
}

#ifndef WIN32
static int ser_set_control(TYPE_FD_SER fd, int line, int state)
{
	if (state) {
		return ioctl(fd, TIOCMBIS, &line);
	} else {
		return ioctl(fd, TIOCMBIC, &line);
	}
}
#endif	/* !WIN32 */

int ser_set_dtr(TYPE_FD_SER fd, int state)
{
#ifndef WIN32
	return ser_set_control(fd, TIOCM_DTR, state);
#else	/* WIN32 */
	DWORD action;

	if (state == 0) {
		action = CLRDTR;
	}
	else {
		action = SETDTR;
	}

	/* Success */
	if (EscapeCommFunction(fd->handle,action) != 0) {
		return 0;
	}

	return -1;
#endif	/* WIN32 */
}

int ser_set_rts(TYPE_FD_SER fd, int state)
{
#ifndef WIN32
	return ser_set_control(fd, TIOCM_RTS, state);
#else	/* WIN32 */
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
#endif	/* WIN32 */
}

#ifndef WIN32
static int ser_get_control(TYPE_FD_SER fd, int line)
{
	int	flags;

	ioctl(fd, TIOCMGET, &flags);

	return (flags & line);
}
#endif	/* !WIN32 */

int ser_get_dsr(TYPE_FD_SER fd)
{
#ifndef WIN32
	return ser_get_control(fd, TIOCM_DSR);
#else	/* WIN32 */
	int flags;

	w32_getcomm(fd->handle, &flags);
	return (flags & TIOCM_DSR);
#endif	/* WIN32 */
}

int ser_get_cts(TYPE_FD_SER fd)
{
#ifndef WIN32
	return ser_get_control(fd, TIOCM_CTS);
#else	/* WIN32 */
	int flags;

	w32_getcomm(fd->handle, &flags);
	return (flags & TIOCM_CTS);
#endif	/* WIN32 */
}

int ser_get_dcd(TYPE_FD_SER fd)
{
#ifndef WIN32
	return ser_get_control(fd, TIOCM_CD);
#else	/* WIN32 */
	int flags;

	w32_getcomm(fd->handle, &flags);
	return (flags & TIOCM_CD);
#endif	/* WIN32 */
}

int ser_close(TYPE_FD_SER fd, const char *port)
{
	if (INVALID_FD_SER(fd)) {
#ifndef WIN32
		fatal_with_errno(EXIT_FAILURE, "ser_close: programming error: fd=%d port=%s", fd, port);
#else	/* WIN32 */
		fatal_with_errno(EXIT_FAILURE, "ser_close: programming error: struct=%p port=%s", fd, port);
#endif	/* WIN32 */
	}

	if (close(fd) != 0)
		return -1;

#ifdef HAVE_UU_LOCK
	if (do_lock_port)
		uu_unlock(xbasename(port));
#endif

	return 0;
}

ssize_t ser_send_char(TYPE_FD_SER fd, unsigned char ch)
{
	return ser_send_buf_pace(fd, 0, &ch, 1);
}

static ssize_t send_formatted(TYPE_FD_SER fd, const char *fmt, va_list va, useconds_t d_usec)
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
ssize_t ser_send_pace(TYPE_FD_SER fd, useconds_t d_usec, const char *fmt, ...)
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
ssize_t ser_send(TYPE_FD_SER fd, const char *fmt, ...)
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
ssize_t ser_send_buf(TYPE_FD_SER fd, const void *buf, size_t buflen)
{
	return ser_send_buf_pace(fd, 0, buf, buflen);
}

/* send buflen bytes from buf with d_usec delay after each char */
ssize_t ser_send_buf_pace(TYPE_FD_SER fd, useconds_t d_usec, const void *buf,
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

ssize_t ser_get_char(TYPE_FD_SER fd, void *ch, time_t d_sec, useconds_t d_usec)
{
	/* Per standard below, we can cast here, because required ranges are
	 * effectively the same (and signed -1 for suseconds_t), and at most long:
	 * https://pubs.opengroup.org/onlinepubs/009604599/basedefs/sys/types.h.html
	 */
	return select_read(fd, ch, 1, d_sec, (suseconds_t)d_usec);
}

ssize_t ser_get_buf(TYPE_FD_SER fd, void *buf, size_t buflen, time_t d_sec, useconds_t d_usec)
{
	memset(buf, '\0', buflen);

	return select_read(fd, buf, buflen, d_sec, (suseconds_t)d_usec);
}

/* keep reading until buflen bytes are received or a timeout occurs */
ssize_t ser_get_buf_len(TYPE_FD_SER fd, void *buf, size_t buflen, time_t d_sec, useconds_t d_usec)
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
ssize_t ser_get_line_alert(TYPE_FD_SER fd, void *buf, size_t buflen, char endchar,
	const char *ignset, const char *alertset, void handler(char ch),
	time_t d_sec, useconds_t d_usec)
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
ssize_t ser_get_line(TYPE_FD_SER fd, void *buf, size_t buflen, char endchar,
	const char *ignset, time_t d_sec, useconds_t d_usec)
{
	return ser_get_line_alert(fd, buf, buflen, endchar, ignset, "", NULL,
		d_sec, d_usec);
}

ssize_t ser_flush_in(TYPE_FD_SER fd, const char *ignset, int verbose)
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

int ser_flush_io(TYPE_FD_SER fd)
{
	return tcflush(fd, TCIOFLUSH);
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
