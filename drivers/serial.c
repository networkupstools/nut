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

#include <grp.h>
#include <pwd.h>
#include <ctype.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef HAVE_UU_LOCK
#include <libutil.h>
#endif

	static unsigned int	comm_failures = 0;

static void ser_open_error(const char *port)
{
	struct	stat	fs;
	struct	passwd	*user;
	struct	group	*group;

	printf("\n");

	printf("Unable to open %s: %s\n\n", port, strerror(errno));

	/* check for bad port definitions first */
	if (stat(port, &fs)) {
		printf("Things to try:\n\n");
		printf(" - Check 'port=' in ups.conf\n\n");
		printf(" - Check owner/permissions of all parts of path\n\n");
		fatalx(EXIT_FAILURE, "Fatal error: unusable configuration");
	}

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

	printf("     Mode of port: %04o\n\n", (int) fs.st_mode & 07777);

	printf("Things to try:\n\n");
	printf(" - Use another port (with the right permissions)\n\n");
	printf(" - Fix the port owner/group or permissions on this port\n\n");
	printf(" - Run this driver as another user (upsdrvctl -u or 'user=...' in ups.conf).\n");
	printf("   See upsdrvctl(8) and ups.conf(5).\n\n");

	fatalx(EXIT_FAILURE, "Fatal error: unusable configuration");
}

static void lock_set(int fd, const char *port)
{
	int	ret;

	if (fd < 0)
		fatal_with_errno(EXIT_FAILURE, "lock_set: programming error: fd = %d", fd);

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
int ser_open_nf(const char *port)

{
	int	fd;

	fd = open(port, O_RDWR | O_NOCTTY | O_EXCL | O_NONBLOCK);

	if (fd < 0) {
		return -1;
	}

	lock_set(fd, port);

	return fd;
}

int ser_open(const char *port)
{
	int res;

	res = ser_open_nf(port);
	if(res == -1) {
		ser_open_error(port);
	}

	return res;
}

int ser_set_speed_nf(int fd, const char *port, speed_t speed)
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

int ser_set_speed(int fd, const char *port, speed_t speed)
{
	int res;

	res = ser_set_speed_nf(fd,port,speed);
	if(res == -1) {
		fatal_with_errno(EXIT_FAILURE, "tcgetattr(%s)", port);
	}

	return 0;
}

static int ser_set_control(int fd, int line, int state)
{
	if (state) {
		return ioctl(fd, TIOCMBIS, &line);
	} else {
		return ioctl(fd, TIOCMBIC, &line);
	}
}

int ser_set_dtr(int fd, int state)
{
	return ser_set_control(fd, TIOCM_DTR, state);
}

int ser_set_rts(int fd, int state)
{
	return ser_set_control(fd, TIOCM_RTS, state);
}

static int ser_get_control(int fd, int line)
{
	int	flags;

	ioctl(fd, TIOCMGET, &flags);

	return (flags & line);
}

int ser_get_dsr(int fd)
{
	return ser_get_control(fd, TIOCM_DSR);
}

int ser_get_cts(int fd)
{
	return ser_get_control(fd, TIOCM_CTS);
}

int ser_get_dcd(int fd)
{
	return ser_get_control(fd, TIOCM_CD);
}

int ser_flush_io(int fd)
{
	return tcflush(fd, TCIOFLUSH);
}

int ser_close(int fd, const char *port)
{
	if (fd < 0)
		fatal_with_errno(EXIT_FAILURE, "ser_close: programming error: fd=%d port=%s", fd, port);

	if (close(fd) != 0)
		return -1;

#ifdef HAVE_UU_LOCK
	if (do_lock_port)
		uu_unlock(xbasename(port));
#endif

	return 0;
}

int ser_send_char(int fd, unsigned char ch)
{
	return ser_send_buf_pace(fd, 0, &ch, 1);
}

static int send_formatted(int fd, const char *fmt, va_list va, unsigned long d_usec)
{
	int	ret;
	char	buf[LARGEBUF];

	ret = vsnprintf(buf, sizeof(buf), fmt, va);

	if (ret >= (int)sizeof(buf)) {
		upslogx(LOG_WARNING, "vsnprintf needed more than %d bytes", (int)sizeof(buf));
	}

	return ser_send_buf_pace(fd, d_usec, buf, strlen(buf));
}

/* send the results of the format string with d_usec delay after each char */
int ser_send_pace(int fd, unsigned long d_usec, const char *fmt, ...)
{
	int	ret;
	va_list	ap;

	va_start(ap, fmt);

	ret = send_formatted(fd, fmt, ap, d_usec);

	va_end(ap);

	return ret;
}

/* send the results of the format string with no delay */
int ser_send(int fd, const char *fmt, ...)
{
	int	ret;
	va_list	ap;

	va_start(ap, fmt);

	ret = send_formatted(fd, fmt, ap, 0);

	va_end(ap);

	return ret;
}

/* send buflen bytes from buf with no delay */
int ser_send_buf(int fd, const void *buf, size_t buflen)
{
	return ser_send_buf_pace(fd, 0, buf, buflen);
}

/* send buflen bytes from buf with d_usec delay after each char */
int ser_send_buf_pace(int fd, unsigned long d_usec, const void *buf,
	size_t buflen)
{
	int	ret;
	size_t	sent;
	const char	*data = buf;

	for (sent = 0; sent < buflen; sent += ret) {

		ret = write(fd, &data[sent], (d_usec == 0) ? (buflen - sent) : 1);

		if (ret < 1) {
			return ret;
		}

		usleep(d_usec);
	}

	return sent;
}

int ser_get_char(int fd, void *ch, long d_sec, long d_usec)
{
	return select_read(fd, ch, 1, d_sec, d_usec);
}

int ser_get_buf(int fd, void *buf, size_t buflen, long d_sec, long d_usec)
{
	memset(buf, '\0', buflen);

	return select_read(fd, buf, buflen, d_sec, d_usec);
}

/* keep reading until buflen bytes are received or a timeout occurs */
int ser_get_buf_len(int fd, void *buf, size_t buflen, long d_sec, long d_usec)
{
	int	ret;
	size_t	recv;
	char	*data = buf;

	memset(buf, '\0', buflen);

	for (recv = 0; recv < buflen; recv += ret) {

		ret = select_read(fd, &data[recv], buflen - recv, d_sec, d_usec);

		if (ret < 1) {
			return ret;
		}
	}

	return recv;
}

/* reads a line up to <endchar>, discarding anything else that may follow,
   with callouts to the handler if anything matches the alertset */
int ser_get_line_alert(int fd, void *buf, size_t buflen, char endchar,
	const char *ignset, const char *alertset, void handler(char ch),
	long d_sec, long d_usec)
{
	int	i, ret;
	char	tmp[64];
	char	*data = buf;
	size_t	count = 0, maxcount;

	memset(buf, '\0', buflen);

	maxcount = buflen - 1;		/* for trailing \0 */

	while (count < maxcount) {
		ret = select_read(fd, tmp, sizeof(tmp), d_sec, d_usec);

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
int ser_get_line(int fd, void *buf, size_t buflen, char endchar,
	const char *ignset, long d_sec, long d_usec)
{
	return ser_get_line_alert(fd, buf, buflen, endchar, ignset, "", NULL,
		d_sec, d_usec);
}

int ser_flush_in(int fd, const char *ignset, int verbose)
{
	int	ret, extra = 0;
	char	ch;

	while ((ret = ser_get_char(fd, &ch, 0, 0)) > 0) {

		if (strchr(ignset, ch))
			continue;

		extra++;

		if (verbose == 0)
			continue;

		if (isprint(ch & 0xFF))
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
#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	ret = vsnprintf(why, sizeof(why), fmt, ap);
#if defined (__GNUC__) || defined (__clang__)
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
