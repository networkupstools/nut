/* upsdrvquery.c - a single query shot over a driver socket,
                   tracked until a response arrives, returning
                   that line and closing a connection

   Copyright (C) 2023-2025  Jim Klimov <jimklimov+nut@gmail.com>

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

#include "config.h"  /* must be the first header */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef WIN32
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#else
#include "wincompat.h"
#endif

#include "common.h"
#include "upsdrvquery.h"
#include "nut_stdint.h"

/* Normally the upsdrvquery*() methods call upslogx() to report issues
 * such as failed fopen() of Unix socket file, or a dialog timeout or
 * different error.
 * In a few cases we call these methods opportunistically, and so if
 * they fail - we do not care enough to raise a lot of "scary noise";
 * the caller can take care of logging as/if needed.
 * This variable and its values are a bit of internal detail between
 * certain NUT programs to hush the low-level reports when they are
 * not being otherwise debugged (e.g. nut_debug_level < 1).
 * Default value allows all those messages to appear.
 */
int nut_upsdrvquery_debug_level = NUT_UPSDRVQUERY_DEBUG_LEVEL_DEFAULT;

udq_pipe_conn_t *upsdrvquery_connect(const char *sockfn) {
	udq_pipe_conn_t	*conn = (udq_pipe_conn_t*)xcalloc(1, sizeof(udq_pipe_conn_t));

	/* Code borrowed from our numerous sock_connect() implems */
#ifndef WIN32
	struct	sockaddr_un sa;
	ssize_t	ret;

	memset(&sa, '\0', sizeof(sa));
	sa.sun_family = AF_UNIX;
	snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", sockfn);

	conn->sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (conn->sockfd < 0) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_CONNECT)
			upslog_with_errno(LOG_ERR, "open socket");
		free(conn);
		return NULL;
	}

	if (connect(conn->sockfd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_CONNECT)
			upslog_with_errno(LOG_ERR, "connect to driver socket at %s", sockfn);
		close(conn->sockfd);
		free(conn);
		return NULL;
	}

	ret = fcntl(conn->sockfd, F_GETFL, 0);
	if (ret < 0) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_CONNECT)
			upslog_with_errno(LOG_ERR, "fcntl get on driver socket %s failed", sockfn);
		close(conn->sockfd);
		free(conn);
		return NULL;
	}

	if (fcntl(conn->sockfd, F_SETFL, ret | O_NDELAY) < 0) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_CONNECT)
			upslog_with_errno(LOG_ERR, "fcntl set O_NDELAY on driver socket %s failed", sockfn);
		close(conn->sockfd);
		free(conn);
		return NULL;
	}
#else
	BOOL	result = WaitNamedPipe(sockfn, NMPWAIT_USE_DEFAULT_WAIT);

	if (result == FALSE) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_CONNECT)
			upslog_with_errno(LOG_ERR, "WaitNamedPipe : %d\n", GetLastError());
		return NULL;
	}

	conn->sockfd = CreateFile(
			sockfn,         /* pipe name */
			GENERIC_READ |  /* read and write access */
			GENERIC_WRITE,
			0,              /* no sharing */
			NULL,           /* default security attributes FIXME */
			OPEN_EXISTING,  /* opens existing pipe */
			FILE_FLAG_OVERLAPPED, /*  enable async IO */
			NULL);          /* no template file */

	if (conn->sockfd == INVALID_HANDLE_VALUE) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_CONNECT)
			upslog_with_errno(LOG_ERR, "CreateFile : %d\n", GetLastError());
		free(conn);
		return NULL;
	}

	memset(&(conn->overlapped), 0, sizeof(conn->overlapped));

	conn->overlapped.hEvent = CreateEvent(
		NULL,  /* Security */
		FALSE, /* auto-reset */
		FALSE, /* initial state = non signaled */
		NULL   /* no name */
	);

	if (conn->overlapped.hEvent == NULL) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_CONNECT)
			upslogx(LOG_ERR, "Can't create event for reading event log");
		free(conn);
		return NULL;
	}

	memset(conn->buf, 0, sizeof(conn->buf));

	/* Start a read IO so we could wait on the event associated with it
	 * Requires the persistent buffer for the connection
	 */
	upsdebugx(6, "%s: Queue initial async read", __func__);
	ReadFile(conn->sockfd, conn->buf,
		sizeof(conn->buf) - 1, /* -1 to be sure to have a trailing 0 */
		NULL, &conn->overlapped);
	conn->newread = 0;
#endif  /* WIN32 */

	/* Just for fun: stash the name */
	if (snprintf(conn->sockfn, sizeof(conn->sockfn), "%s", sockfn) < 0)
		conn->sockfn[0] = '\0';
	return conn;
}

udq_pipe_conn_t *upsdrvquery_connect_drvname_upsname(const char *drvname, const char *upsname) {
	char	sockname[NUT_PATH_MAX + 1];
#ifndef WIN32
	struct stat     fs;
	snprintf(sockname, sizeof(sockname), "%s/%s-%s",
		dflt_statepath(), drvname, upsname);
	check_unix_socket_filename(sockname);
	if (stat(sockname, &fs)) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_CONNECT)
			upslog_with_errno(LOG_ERR, "Can't open %s", sockname);
		return NULL;
	}
#else
	snprintf(sockname, sizeof(sockname), "\\\\.\\pipe\\%s-%s", drvname, upsname);
#endif  /* WIN32 */

	return upsdrvquery_connect(sockname);
}

void upsdrvquery_close(udq_pipe_conn_t *conn) {
#ifdef WIN32
	int	loggedOut = 0;
#endif  /* WIN32 */

	if (!conn)
		return;

	if (VALID_FD(conn->sockfd)) {
		int	nudl = nut_upsdrvquery_debug_level;
		ssize_t ret;
		upsdebugx(5, "%s: closing driver socket, try to say goodbye", __func__);
		ret = upsdrvquery_write(conn, "LOGOUT\n");
		if (7 <= ret) {
			upsdebugx(5, "%s: okay", __func__);
#ifdef WIN32
			loggedOut = 1;
#endif  /* WIN32 */
			usleep(1000000);
		} else {
			upsdebugx(5, "%s: must have been closed on the other side", __func__);
		}
		nut_upsdrvquery_debug_level = nudl;
	}

#ifndef WIN32
	if (VALID_FD(conn->sockfd))
		close(conn->sockfd);
#else
	if (VALID_FD(conn->overlapped.hEvent)) {
		CloseHandle(conn->overlapped.hEvent);
	}
	memset(&(conn->overlapped), 0, sizeof(conn->overlapped));

	if (VALID_FD(conn->sockfd)) {
		if (DisconnectNamedPipe(conn->sockfd) == 0 && !loggedOut) {
			if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_CONNECT)
				upslogx(LOG_ERR,
					"DisconnectNamedPipe error : %d",
					(int)GetLastError());
		}
		CloseHandle(conn->sockfd);
	}
	conn->newread = 0;
#endif  /* WIN32 */

	conn->sockfd = ERROR_FD;
	memset(conn->buf, 0, sizeof(conn->buf));
	memset(conn->sockfn, 0, sizeof(conn->sockfn));
	/* caller should free the conn */
}

ssize_t upsdrvquery_read_timeout(udq_pipe_conn_t *conn, struct timeval tv) {
	ssize_t	ret;
#ifndef WIN32
	fd_set	rfds;
#else
	DWORD	bytesRead = 0;
	BOOL	res = FALSE;
	struct timeval	start, now, presleep;
#endif

	upsdebugx(5, "%s: tv={sec=%" PRIiMAX ", usec=%06" PRIiMAX "}%s",
		__func__, (intmax_t)tv.tv_sec, (intmax_t)tv.tv_usec,
		tv.tv_sec < 0 || tv.tv_usec < 0 ? " (unlimited timeout)" : ""
		);

	if (!conn || INVALID_FD(conn->sockfd)) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_CONNECT)
			upslog_with_errno(LOG_ERR, "socket not initialized");
		return -1;
	}

#ifndef WIN32
	FD_ZERO(&rfds);
	FD_SET(conn->sockfd, &rfds);

	if (select(conn->sockfd + 1, &rfds, NULL, NULL, tv.tv_sec < 0 || tv.tv_usec < 0 ? NULL : &tv) < 0) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_DIALOG)
			upslog_with_errno(LOG_ERR, "select with socket");
		/* upsdrvquery_close(conn); */
		return -1;
	}

	if (!FD_ISSET(conn->sockfd, &rfds)) {
		upsdebugx(5, "%s: received nothing from driver socket", __func__);
		return -2;	/* timed out, no info */
	}

	memset(conn->buf, 0, sizeof(conn->buf));
	ret = read(conn->sockfd, conn->buf, sizeof(conn->buf));
#else
/*
	if (nut_debug_level > 0 || nut_upsdrvquery_debug_level > 0)
		upslog_with_errno(LOG_ERR, "Support for this platform is not currently implemented");
	return -1;
*/

	/* Is GetLastError() required to move on if pipe has more data?
	 *   if (GetLastError() == ERROR_IO_PENDING) {
	 */
	if (conn->newread) {
		upsdebugx(6, "%s: Restart async read", __func__);
		memset(conn->buf, 0, sizeof(conn->buf));
		ReadFile(conn->sockfd, conn->buf, sizeof(conn->buf) - 1, NULL, &(conn->overlapped));
		conn->newread = 0;
	}

	gettimeofday(&start, NULL);
	while (res == FALSE /*&& bytesRead == 0*/) {
		res = GetOverlappedResult(conn->sockfd, &conn->overlapped, &bytesRead, FALSE);
		if (res != FALSE /*|| bytesRead != 0*/)
			break;

		if (tv.tv_sec < 1 && tv.tv_usec < 1) {
			upsdebugx(5, "%s: pipe read error (no incoming data), proceeding now", __func__);
			break;
		}
		upsdebugx(6, "%s: pipe read error, still waiting for data", __func__);

		/* Throttle down a bit, 0.1 sec (10^5 * 10^-6) should do it conveniently */
		gettimeofday(&presleep, NULL);
		usleep(100000); /* obsoleted in win32, so follow up below */

		gettimeofday(&now, NULL);
		/* NOTE: This code may report a "diff=1.-894714 (0.105286)"
		 * which looks bogus, but for troubleshooting we don't care
		 */
		upsdebugx(7, "%s: presleep=%ld.%06ld now=%ld.%06ld diff=%4.0f.%06ld (%f)",
			__func__, presleep.tv_sec, presleep.tv_usec,
			now.tv_sec, now.tv_usec,
			difftime(now.tv_sec, presleep.tv_sec),
			(long)(now.tv_usec - presleep.tv_usec),
			difftimeval(now, presleep)
			);

		/* accept shorter delays, Windows does not guarantee a minimum sleep it seems */
		if (difftimeval(now, presleep) < 0.05) {
			/* https://stackoverflow.com/a/17283549 */
			HANDLE timer;
			LARGE_INTEGER ft;

			/* SetWaitableTimer() uses 100 nanosecond intervals,
			 * and a negative value indicates relative time: */
			ft.QuadPart = -(10*100000); /* 100 msec */

			timer = CreateWaitableTimer(NULL, TRUE, NULL);
			SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
			WaitForSingleObject(timer, INFINITE);
			CloseHandle(timer);

			gettimeofday(&now, NULL);
			upsdebugx(7, "%s: presleep=%ld.%06ld now=%ld.%06ld diff=%4.0f.%06ld (%f)",
				__func__, presleep.tv_sec, presleep.tv_usec,
				now.tv_sec, now.tv_usec,
				difftime(now.tv_sec, presleep.tv_sec),
				(long)(now.tv_usec - presleep.tv_usec),
				difftimeval(now, presleep)
				);
		}

		/* If nothing was honored, doze off for a whole second */
		if (difftimeval(now, presleep) < 0.05) {
			sleep(1);

			gettimeofday(&now, NULL);
			upsdebugx(7, "%s: presleep=%ld.%06ld now=%ld.%06ld diff=%4.0f.%06ld (%f)",
				__func__, presleep.tv_sec, presleep.tv_usec,
				now.tv_sec, now.tv_usec,
				difftime(now.tv_sec, presleep.tv_sec),
				(long)(now.tv_usec - presleep.tv_usec),
				difftimeval(now, presleep)
				);
		}

		if (difftimeval(now, start) > (tv.tv_sec + 0.000001 * tv.tv_usec)) {
			upsdebugx(5, "%s: pipe read error, timeout exceeded", __func__);
			break;
		}
	}

	if (res != FALSE)
		ret = (ssize_t)bytesRead;
	else
		ret = -1;
#endif  /* WIN32 */

	upsdebugx(ret > 0 ? 5 : 6,
		"%s: received %" PRIiMAX " bytes from driver socket: %s",
		__func__, (intmax_t)ret, (ret > 0 ? conn->buf : "<null>"));
	if (ret > 0 && conn->buf[0] == '\0')
		upsdebug_hex(5, "payload starts with zero byte: ",
			conn->buf, ((size_t)ret > sizeof(conn->buf) ? sizeof(conn->buf) : (size_t)ret));
	return ret;
}

ssize_t upsdrvquery_write(udq_pipe_conn_t *conn, const char *buf) {
	size_t	buflen = strlen(buf);
#ifndef WIN32
	ssize_t	ret;
#else
	DWORD	bytesWritten = 0;
	BOOL	result = FALSE;
#endif  /* WIN32 */

	upsdebugx(5, "%s: write to driver socket: %s", __func__, buf);

	if (!conn || INVALID_FD(conn->sockfd)) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_CONNECT)
			upslog_with_errno(LOG_ERR, "socket not initialized");
		return -1;
	}

#ifndef WIN32
	ret = write(conn->sockfd, buf, buflen);

	if (ret < 0 || ret != (int)buflen) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_DIALOG)
			upslog_with_errno(LOG_ERR, "Write to socket %d failed", conn->sockfd);
		goto socket_error;
	}

	return ret;
#else
	result = WriteFile(conn->sockfd, buf, buflen, &bytesWritten, NULL);
	if (result == 0 || bytesWritten != (DWORD)buflen) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_DIALOG)
			upslog_with_errno(LOG_ERR, "Write to handle %p failed", conn->sockfd);
		goto socket_error;
	}

	return (ssize_t)bytesWritten;
#endif  /* WIN32 */

socket_error:
	/*upsdrvquery_close(conn);*/
	return -1;
}

ssize_t upsdrvquery_prepare(udq_pipe_conn_t *conn, struct timeval tv) {
	struct timeval	start, now;

	/* Avoid noise */
	if (upsdrvquery_write(conn, "NOBROADCAST\n") < 0)
		goto socket_error;

	if (tv.tv_sec < 1 && tv.tv_usec < 1) {
		upsdebugx(5, "%s: proclaiming readiness for tracked commands without flush of server messages", __func__);
		return 1;
	}

	/* flush incoming, if any */
	gettimeofday(&start, NULL);

	if (upsdrvquery_write(conn, "PING\n") < 0)
		goto socket_error;

	upsdebugx(5, "%s: waiting for a while to flush server messages", __func__);
	while (1) {
		char *buf;
		upsdrvquery_read_timeout(conn, tv);
		gettimeofday(&now, NULL);
		if (difftimeval(now, start) > ((double)(tv.tv_sec) + 0.000001 * (double)(tv.tv_usec))) {
			upsdebugx(5, "%s: requested timeout expired", __func__);
			break;
		}

		/* Await a PONG for quick confirmation of achieved quietness
		 * (should only happen after the driver handled NOBROADCAST)
		 */
#ifdef WIN32
		/* Allow a new read to happen later */
		conn->newread = 1;
#endif

		buf = conn->buf;
		while (buf && *buf) {
			if (!strncmp(buf, "PONG\n", 5)) {
				upsdebugx(5, "%s: got expected PONG", __func__);
				goto finish;
			}
			buf = strchr(buf, '\n');
			if (buf) {
/*
				upsdebugx(5, "%s: trying next line of multi-line response: %s",
					__func__, buf);
*/
				buf++;	/* skip EOL char */
			}
		}

		/* Diminishing timeouts for read() */
		tv.tv_usec -= (suseconds_t)(difftimeval(now, start));
		while (tv.tv_usec < 0) {
			tv.tv_sec--;
			tv.tv_usec = 1000000 + tv.tv_usec;	/* Note it is negative */
		}
		if (tv.tv_sec <= 0 && tv.tv_usec <= 0) {
			upsdebugx(5, "%s: requested timeout expired", __func__);
			break;
		}
	}

	/* Check that we can have a civilized dialog --
	 * nope, this one is for network protocol */
/*
	if (upsdrvquery_write(conn, "GET TRACKING\n") < 0)
		goto socket_error;
	if (upsdrvquery_read_timeout(conn, tv) < 1)
		goto socket_error;
	if (strcmp(conn->buf, "ON")) {
		if (nut_debug_level > 0 || nut_upsdrvquery_debug_level >= NUT_UPSDRVQUERY_DEBUG_LEVEL_DIALOG)
			upslog_with_errno(LOG_ERR, "Driver does not have TRACKING support enabled");
		goto socket_error;
	}
*/

finish:
	upsdebugx(5, "%s: ready for tracked commands", __func__);
	return 1;

socket_error:
	upsdrvquery_close(conn);
	return -1;
}

/* UUID v4 basic implementation
 * Note: 'dest' must be at least `UUID4_LEN` long */
#define UUID4_BYTESIZE 16
static int upsdrvquery_nut_uuid_v4(char *uuid_str)
{
	size_t	i;
	uint8_t	nut_uuid[UUID4_BYTESIZE];

	if (!uuid_str)
		return 0;

	for (i = 0; i < UUID4_BYTESIZE; i++)
		nut_uuid[i] = (uint8_t)rand() + (uint8_t)rand();

	/* set variant and version */
	nut_uuid[6] = (nut_uuid[6] & 0x0F) | 0x40;
	nut_uuid[8] = (nut_uuid[8] & 0x3F) | 0x80;

	return snprintf(uuid_str, UUID4_LEN,
		"%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		nut_uuid[0], nut_uuid[1], nut_uuid[2], nut_uuid[3],
		nut_uuid[4], nut_uuid[5], nut_uuid[6], nut_uuid[7],
		nut_uuid[8], nut_uuid[9], nut_uuid[10], nut_uuid[11],
		nut_uuid[12], nut_uuid[13], nut_uuid[14], nut_uuid[15]);
}

ssize_t upsdrvquery_request(
	udq_pipe_conn_t *conn, struct timeval tv,
	const char *query
) {
	/* Assume TRACKING works; post a socket-protocol
	 * query to driver and return whatever it says */
	char	qbuf[LARGEBUF];
	size_t	qlen;
	char	tracking_id[UUID4_LEN];
	struct timeval	start, now;

	if (snprintf(qbuf, sizeof(qbuf), "%s", query) < 0)
		goto socket_error;

	qlen = strlen(qbuf);
	while (qlen > 0 && qbuf[qlen - 1] == '\n') {
		qbuf[qlen - 1] = '\0';
		qlen--;
	}

	upsdrvquery_nut_uuid_v4(tracking_id);

	if (snprintf(qbuf + qlen, sizeof(qbuf) - qlen, " TRACKING %s\n", tracking_id) < 0)
		goto socket_error;

	/* Post the query and wait for reply */
	if (upsdrvquery_write(conn, qbuf) < 0)
		goto socket_error;

	if (tv.tv_sec < 1 && tv.tv_usec < 1) {
		upsdebugx(1, "%s: will wait indefinitely for response to %s",
			__func__, query);
	} else {
		while (tv.tv_usec >= 1000000) {
			tv.tv_usec -= 1000000;
			tv.tv_sec++;
		}
		upsdebugx(5, "%s: will wait up to %" PRIiMAX
			".%06" PRIiMAX " sec for response to %s",
			__func__, (intmax_t)tv.tv_sec,
			(intmax_t)tv.tv_usec, query);
	}

	gettimeofday(&start, NULL);
	while (1) {
		char *buf;
		if (upsdrvquery_read_timeout(conn, tv) < 1)
			goto socket_error;

#ifdef WIN32
		/* Allow a new read to happen later */
		conn->newread = 1;
#endif

		buf = conn->buf;
		while (buf && *buf) {
			if (!strncmp(buf, "TRACKING ", 9)
			&&  !strncmp(buf + 9, tracking_id, UUID4_LEN - 1)
			) {
				int	ret;
				size_t	offset = 9 + UUID4_LEN;
				if (sscanf(buf + offset, " %d", &ret) < 1) {
					upsdebugx(5, "%s: sscanf failed at offset %" PRIuSIZE " (char '%c')",
						__func__, offset, buf[offset]);
					goto socket_error;
				}
				upsdebugx(5, "%s: parsed out command status: %d",
					__func__, ret);
				return ret;
			} else {
				upsdebugx(5, "%s: response did not have expected format",
					__func__);
				/* Maybe a rogue send-to-all? */
			}
			buf = strchr(buf, '\n');
			if (buf) {
				upsdebugx(5, "%s: trying next line of multi-line response: %s",
					__func__, buf);
				buf++;	/* skip EOL char */
			}
		}

		gettimeofday(&now, NULL);
		if (tv.tv_sec < 1 && tv.tv_usec < 1) {
			if ( ((long)(difftimeval(now, start))) % 60 == 0 )
				upsdebugx(5, "%s: waiting indefinitely for response to %s", __func__, query);
			sleep(1);
			continue;
		}

		if (difftimeval(now, start) > ((double)(tv.tv_sec) + 0.000001 * (double)(tv.tv_usec))) {
			upsdebugx(5, "%s: timed out waiting for expected response",
				__func__);
			return -1;
		}
	}

socket_error:
	upsdrvquery_close(conn);
	return -1;
}

ssize_t upsdrvquery_oneshot(
	const char *drvname, const char *upsname,
	const char *query,
	char *buf, const size_t bufsz,
	struct timeval *ptv
) {
	struct timeval	tv;
	ssize_t	ret;
	udq_pipe_conn_t	*conn = upsdrvquery_connect_drvname_upsname(drvname, upsname);

	if (!conn || INVALID_FD(conn->sockfd))
		return -1;

	/* This depends on driver looping delay, polling frequency,
	 * being blocked on other commands, etc. Number so far is
	 * arbitrary and optimistic. A non-zero setting causes a
	 * long initial silence to flush incoming buffers after
	 * the NOBROADCAST. In practice, we do not expect messages
	 * from dstate::send_to_all() to be a nuisance, since we
	 * have just connected and posted the NOBROADCAST so there
	 * is little chance that something appears in that short
	 * time. Also now we know to ignore replies that are not
	 *   TRACKING <id of our query>
	 */
	tv.tv_sec = 3;
	tv.tv_usec = 0;

	/* Here we have a fragile simplistic parser that
	 * expects one line replies to a specific command,
	 * so want to rule out the noise.
	 */
	if (upsdrvquery_prepare(conn, tv) < 0) {
		ret = -1;
		goto finish;
	}

	if (ptv) {
		tv.tv_sec = ptv->tv_sec;
		tv.tv_usec = ptv->tv_usec;
	} else {
		tv.tv_sec = 5;
		tv.tv_usec = 0;
	}
	if ((ret = upsdrvquery_request(conn, tv, query)) < 0) {
		ret = -1;
		goto finish;
	}

	if (buf) {
		snprintf(buf, bufsz, "%s", conn->buf);
	}
finish:
	upsdrvquery_close(conn);
	free(conn);
	return ret;
}
