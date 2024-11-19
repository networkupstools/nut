/*
* clone.c: clone an UPS, treating its outlet as if it were an UPS
*          (with shutdown INSTCMD support)
*
* Copyright (C) 2009 - Arjen de Korte <adkorte-guest@alioth.debian.org>
* Copyright (C) 2024 - Jim Klimov <jimklimov+nut@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "config.h"
#include "main.h"
#include "parseconf.h"
#include "attribute.h"
#include "nut_stdint.h"

#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#include <sys/un.h>
#endif

#define DRIVER_NAME	"Clone UPS driver"
#define DRIVER_VERSION	"0.07"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arjen de Korte <adkorte-guest@alioth.debian.org>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

static struct {
	struct {
		long	start;
		long	shutdown;
	} timer;
	char	status[ST_MAX_VALUE_LEN];
} ups = { { -1, -1 }, "WAIT" };

static struct {
	struct {
		double	act;
		double	low;
	} charge;
	struct {
		double	act;
		double	low;
	} runtime;
} battery = { { 0, 0 }, { 0, 0 } };

static int	dumpdone = 0, online = 1, outlet = 1;
static long	offdelay = 120, ondelay = 30;

static PCONF_CTX_t	sock_ctx;
static time_t	last_poll = 0, last_heard = 0, last_ping = 0;

#ifndef WIN32
/* TODO: Why not built in WIN32? */
static time_t	last_connfail = 0;
#else
static char     	read_buf[SMALLBUF];
static OVERLAPPED	read_overlapped;
#endif

static int instcmd(const char *cmdname, const char *extra);


static int parse_args(size_t numargs, char **arg)
{
	if (numargs < 1) {
		goto skip_out;
	}

	if (!strcasecmp(arg[0], "PONG")) {
		upsdebugx(3, "Got PONG from UPS");
		return 1;
	}

	if (!strcasecmp(arg[0], "DUMPDONE")) {
		upsdebugx(3, "UPS: dump is done");
		dumpdone = 1;
		return 1;
	}

	if (!strcasecmp(arg[0], "DATASTALE")) {
		dstate_datastale();
		return 1;
	}

	if (!strcasecmp(arg[0], "DATAOK")) {
		dstate_dataok();
		return 1;
	}

	if (numargs < 2) {
		goto skip_out;
	}

	/* DELINFO <var> */
	if (!strcasecmp(arg[0], "DELINFO")) {
		dstate_delinfo(arg[1]);
		return 1;
	}

	if (numargs < 3) {
		goto skip_out;
	}

	/* SETINFO <varname> <value> */
	if (!strcasecmp(arg[0], "SETINFO")) {

		if (!strncasecmp(arg[1], "driver.", 7) ||
				!strcasecmp(arg[1], "battery.charge.low") ||
				!strcasecmp(arg[1], "battery.runtime.low") ||
				!strncasecmp(arg[1], "ups.delay.", 10) ||
				!strncasecmp(arg[1], "ups.timer.", 10)) {
			/* don't pass on upstream driver settings */
			return 1;
		}

		if (!strcasecmp(arg[1], "ups.status")) {
			snprintf(ups.status, sizeof(ups.status), "%s", arg[2]);

			online = strstr(ups.status, "OL") ? 1 : 0;

			if (ups.timer.shutdown > 0) {
				dstate_setinfo("ups.status", "FSD %s", ups.status);
				return 1;
			}
		}

		if (!strcasecmp(arg[1], "battery.charge")) {
			battery.charge.act = strtod(arg[2], NULL);

			dstate_setinfo("battery.charge.low", "%g", battery.charge.low);
			dstate_setflags("battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING);
			dstate_setaux("battery.charge.low", 3);
		}

		if (!strcasecmp(arg[1], "battery.runtime")) {
			battery.runtime.act = strtod(arg[2], NULL);

			dstate_setinfo("battery.runtime.low", "%g", battery.runtime.low);
			dstate_setflags("battery.runtime.low", ST_FLAG_RW | ST_FLAG_STRING);
			dstate_setaux("battery.runtime.low", 4);
		}

		dstate_setinfo(arg[1], "%s", arg[2]);
		return 1;
	}

skip_out:
	if (nut_debug_level > 0) {
		char	buf[LARGEBUF];
		size_t	i;
		int	len = -1;

		memset(buf, 0, sizeof(buf));
		for (i = 0; i < numargs; i++) {
			len = snprintfcat(buf, sizeof(buf), "[%s] ", arg[i]);
		}
		if (len > 0) {
			buf[len - 1] = '\0';
		}

		upsdebugx(3, "%s: ignored protocol line with %" PRIuSIZE " keyword(s): %s",
			__func__, numargs, numargs < 1 ? "<empty>" : buf);
	}

	return 0;
}


static TYPE_FD sstate_connect(void)
{
	TYPE_FD	fd;
	const char	*dumpcmd = "DUMPALL\n";

#ifndef WIN32
	ssize_t	ret;
	int	len;
	struct sockaddr_un	sa;

	memset(&sa, '\0', sizeof(sa));
	sa.sun_family = AF_UNIX;
	len = snprintf(sa.sun_path, sizeof(sa.sun_path), "%s/%s", dflt_statepath(), device_path);

	if (len < 0) {
		fatalx(EXIT_FAILURE, "Can't create a unix domain socket: "
			"failed to prepare the pathname");
	}
	if ((uintmax_t)len >= (uintmax_t)sizeof(sa.sun_path)) {
		fatalx(EXIT_FAILURE,
			"Can't create a unix domain socket: pathname '%s/%s' "
			"is too long (%" PRIuSIZE ") for 'struct sockaddr_un->sun_path' "
			"on this system (%" PRIuSIZE ")",
			dflt_statepath(), device_path,
			strlen(dflt_statepath()) + 1 + strlen(device_path),
			sizeof(sa.sun_path));
	}

	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (INVALID_FD(fd)) {
		upslog_with_errno(LOG_ERR, "Can't create socket for UPS [%s]", device_path);
		return ERROR_FD;
	}

	ret = connect(fd, (struct sockaddr *) &sa, sizeof(sa));

	if (ret < 0) {
		time_t	now;

		close(fd);

		/* rate-limit complaints - don't spam the syslog */
		time(&now);

		if (difftime(now, last_connfail) < 60) {
			return ERROR_FD;
		}

		last_connfail = now;

		upslog_with_errno(LOG_ERR, "Can't connect to UPS [%s]", device_path);
		return ERROR_FD;
	}

	ret = fcntl(fd, F_GETFL, 0);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl get on UPS [%s] failed", device_path);
		close(fd);
		return ERROR_FD;
	}

	ret = fcntl(fd, F_SETFL, ret | O_NDELAY);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl set O_NDELAY on UPS [%s] failed", device_path);
		close(fd);
		return ERROR_FD;
	}

	/* get a dump started so we have a fresh set of data */
	ret = write(fd, dumpcmd, strlen(dumpcmd));

	if (ret != (int)strlen(dumpcmd)) {
		upslog_with_errno(LOG_ERR, "Initial write to UPS [%s] failed", device_path);
		close(fd);
		return ERROR_FD;
	}

	/* continued below... */
#else /* WIN32 */
	char		pipename[SMALLBUF];
	BOOL		result = FALSE;

	snprintf(pipename, sizeof(pipename), "\\\\.\\pipe\\%s/%s", dflt_statepath(), device_path);

	result = WaitNamedPipe(pipename,NMPWAIT_USE_DEFAULT_WAIT);

	if (result == FALSE) {
		return ERROR_FD;
	}

	fd = CreateFile(
			pipename,	/* pipe name */
			GENERIC_READ |	/* read and write access */
			GENERIC_WRITE,
			0,		/* no sharing */
			NULL,		/* default security attributes FIXME */
			OPEN_EXISTING,	/* opens existing pipe */
			FILE_FLAG_OVERLAPPED,	/*  enable async IO */
			NULL);		/* no template file */

	if (fd == INVALID_HANDLE_VALUE) {
		upslog_with_errno(LOG_ERR, "Can't connect to UPS [%s]", device_path);
		return ERROR_FD;
	}

	/* get a dump started so we have a fresh set of data */
	DWORD bytesWritten = 0;

	result = WriteFile (fd,dumpcmd,strlen(dumpcmd),&bytesWritten,NULL);
	if (result == 0 || bytesWritten != strlen(dumpcmd)) {
		upslog_with_errno(LOG_ERR, "Initial write to UPS [%s] failed", device_path);
		CloseHandle(fd);
		return ERROR_FD;
	}

	/* Start a read IO so we could wait on the event associated with it */
	ReadFile(fd, read_buf,
		sizeof(read_buf) - 1, /*-1 to be sure to have a trailling 0 */
		NULL, &(read_overlapped));
#endif

	/* sstate_connect() continued for both platforms: */
	pconf_init(&sock_ctx, NULL);

	time(&last_heard);

	dumpdone = 0;

	/* set ups.status to "WAIT" while waiting for the driver response to dumpcmd */
	dstate_setinfo("ups.status", "WAIT");

	upslogx(LOG_INFO, "Connected to UPS [%s]", device_path);
	return fd;
}


static void sstate_disconnect(void)
{
	if (INVALID_FD(upsfd)) {
		/* Already disconnected... or not yet? ;) */
		return;
	}

	pconf_finish(&sock_ctx);

#ifndef WIN32
	close(upsfd);
#else
	CloseHandle(upsfd);
#endif

	upsfd = ERROR_FD;
}


static int sstate_sendline(const char *buf)
{
	ssize_t	ret;

	if (INVALID_FD(upsfd)) {
		return -1;	/* failed */
	}

#ifndef WIN32
	ret = write(upsfd, buf, strlen(buf));
#else
	DWORD bytesWritten = 0;
	BOOL  result = FALSE;

	result = WriteFile (upsfd, buf, strlen(buf), &bytesWritten, NULL);

	if (result == 0) {
		ret = 0;
	}
	else {
		ret = (int)bytesWritten;
	}
#endif

	if (ret == (int)strlen(buf)) {
		return 0;
	}

	upslog_with_errno(LOG_NOTICE, "Send to UPS [%s] failed", device_path);
	return -1;	/* failed */
}


static int sstate_readline(void)
{
	int	i;
	ssize_t	ret;
#ifndef WIN32
	char	buf[SMALLBUF];

	if (INVALID_FD(upsfd)) {
		return -1;	/* failed */
	}

	ret = read(upsfd, buf, sizeof(buf));

	if (ret < 0) {
		switch(errno)
		{
			case EINTR:
			case EAGAIN:
				return 0;

			default:
				upslog_with_errno(LOG_WARNING, "Read from UPS [%s] failed", device_path);
				return -1;
		}
	}
#else
	if (INVALID_FD(upsfd)) {
		return -1;	/* failed */
	}

	/* FIXME? I do not see this buf or read_buf filled below */
	char *buf = read_buf;
	DWORD bytesRead;
	GetOverlappedResult(upsfd, &read_overlapped, &bytesRead, FALSE);
	ret = bytesRead;
#endif

	for (i = 0; i < ret; i++) {

		switch (pconf_char(&sock_ctx, buf[i]))
		{
			case 1:
				if (parse_args(sock_ctx.numargs, sock_ctx.arglist)) {
					time(&last_heard);
				}
				continue;

			case 0:
				continue;	/* haven't gotten a line yet */

			default:
				/* parse error */
				upslogx(LOG_NOTICE, "Parse error on sock: %s", sock_ctx.errmsg);
				return -1;
		}
	}

	return 0;
}


static int sstate_dead(int maxage)
{
	time_t	now;
	double	elapsed;

	/* an unconnected ups is always dead */
	if (INVALID_FD(upsfd)) {
		upsdebugx(3, "sstate_dead: connection to driver socket for UPS [%s] lost", device_path);
		return -1;	/* dead */
	}

	time(&now);

	/* ignore DATAOK/DATASTALE unless the dump is done */
	if (dumpdone && dstate_is_stale()) {
		upsdebugx(3, "sstate_dead: driver for UPS [%s] says data is stale", device_path);
		return -1;	/* dead */
	}

	elapsed = difftime(now, last_heard);

	/* somewhere beyond a third of the maximum time - prod it to make it talk */
	if ((elapsed > (maxage / 3)) && (difftime(now, last_ping) > (maxage / 3))) {
		upsdebugx(3, "Send PING to UPS");
		sstate_sendline("PING\n");
		last_ping = now;
	}

	if (elapsed > maxage) {
		upsdebugx(3, "sstate_dead: didn't hear from driver for UPS [%s] for %g seconds (max %d)",
			   device_path, elapsed, maxage);
		return -1;	/* dead */
	}

	return 0;
}


static int instcmd(const char *cmdname, const char *extra)
{
	const char	*val;

	val = dstate_getinfo(getval("load.status"));
	if (val) {
		if (!strcasecmp(val, "off") || !strcasecmp(val, "no")) {
			outlet = 0;
		}

		if (!strcasecmp(val, "on") || !strcasecmp(val, "yes")) {
			outlet = 1;
		}
	}

	if (!strcasecmp(cmdname, "shutdown.return")) {
		if (outlet && (ups.timer.shutdown < 0)) {
			ups.timer.shutdown = offdelay;
			dstate_setinfo("ups.status", "FSD %s", ups.status);
		}
		ups.timer.start = ondelay;
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		if (outlet && (ups.timer.shutdown < 0)) {
			ups.timer.shutdown = offdelay;
			dstate_setinfo("ups.status", "FSD %s", ups.status);
		}
		ups.timer.start = -1;
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}


static int setvar(const char *varname, const char *val)
{
	if (!strcasecmp(varname, "battery.charge.low")) {
		battery.charge.low = strtod(val, NULL);
		dstate_setinfo("battery.charge.low", "%g", battery.charge.low);
		return STAT_SET_HANDLED;
	}

	if (!strcasecmp(varname, "battery.runtime.low")) {
		battery.runtime.low = strtod(val, NULL);
		dstate_setinfo("battery.runtime.low", "%g", battery.runtime.low);
		return STAT_SET_HANDLED;
	}

	upslogx(LOG_NOTICE, "setvar: unknown variable [%s]", varname);
	return STAT_SET_UNKNOWN;
}


void upsdrv_initinfo(void)
{
	const char	*val;

	time(&last_poll);

	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");

	val = getval("offdelay");
	if (val) {
		offdelay = strtol(val, NULL, 10);
	}

	val = getval("ondelay");
	if (val) {
		ondelay = strtol(val, NULL, 10);
	}

	val = getval("mincharge");
	if (val) {
		battery.charge.low = strtod(val, NULL);
	}

	val = getval("minruntime");
	if (val) {
		battery.runtime.low = strtod(val, NULL);
	}

	dstate_setinfo("ups.delay.shutdown", "%ld", offdelay);
	dstate_setinfo("ups.delay.start", "%ld", ondelay);

	dstate_setinfo("ups.timer.shutdown", "%ld", ups.timer.shutdown);
	dstate_setinfo("ups.timer.start", "%ld", ups.timer.start);

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;
}


void upsdrv_updateinfo(void)
{
	time_t	now = time(NULL);
	double	d;

	/* Throttle tight loops to avoid CPU burn, e.g. when the socket to driver
	 * is not in fact connected, so a select() somewhere is not waiting much */
	if (last_poll > 0 && (d = difftime(now, last_poll)) < 1.0) {
		upsdebugx(5, "%s: too little time (%g sec) has passed since last cycle, throttling",
			__func__, d);
		usleep(500000);
		now = time(NULL);
	}

	if (sstate_dead(15)) {
		sstate_disconnect();
		extrafd = upsfd = sstate_connect();
		return;
	}

	if (sstate_readline()) {
		sstate_disconnect();
		return;
	}

	if (ups.timer.shutdown >= 0) {

		ups.timer.shutdown -= (suseconds_t)(difftime(now, last_poll));

		if (ups.timer.shutdown < 0) {
			const char	*val;

			ups.timer.shutdown = -1;
			outlet = 0;

			val = getval("load.off");
			if (val) {
				char	buf[SMALLBUF];
				snprintf(buf, sizeof(buf), "INSTCMD %s\n", val);
				sstate_sendline(buf);
			}
		}

	} else if (ups.timer.start >= 0) {

		if (online) {
			ups.timer.start -= (suseconds_t)(difftime(now, last_poll));
		} else {
			ups.timer.start = ondelay;
		}

		if (ups.timer.start < 0) {
			const char	*val;

			ups.timer.start = -1;
			outlet = 1;

			val = getval("load.on");
			if (val) {
				char	buf[SMALLBUF];
				snprintf(buf, sizeof(buf), "INSTCMD %s\n", val);
				sstate_sendline(buf);
			}

			dstate_setinfo("ups.status", "%s", ups.status);
		}

	} else if (!online && outlet) {

		if (battery.charge.act < battery.charge.low) {
			upslogx(LOG_INFO, "Battery charge low");
			instcmd("shutdown.return", NULL);
		} else if (battery.runtime.act < battery.runtime.low) {
			upslogx(LOG_INFO, "Battery runtime low");
			instcmd("shutdown.return", NULL);
		}
	}

	dstate_setinfo("ups.timer.shutdown", "%ld", ups.timer.shutdown);
	dstate_setinfo("ups.timer.start", "%ld", ups.timer.start);

	last_poll = now;
}


void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	/* replace with a proper shutdown function */
	upslogx(LOG_ERR, "shutdown not supported");
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(EF_EXIT_FAILURE);
}


void upsdrv_help(void)
{
}


void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "offdelay", "Delay before outlet shutdown (seconds)");
	addvar(VAR_VALUE, "ondelay", "Delay before outlet startup (seconds)");

	addvar(VAR_VALUE, "mincharge", "Remaining battery level when UPS switches to LB (percent)");
	addvar(VAR_VALUE, "minruntime", "Remaining battery runtime when UPS switches to LB (seconds)");

	addvar(VAR_VALUE, "load.off", "Command to switch off outlet");
	addvar(VAR_VALUE, "load.on", "Command to switch on outlet");
	addvar(VAR_VALUE, "load.status", "Variable that indicates outlet is on/off");
}


void upsdrv_initups(void)
{
	extrafd = upsfd = sstate_connect();
}


void upsdrv_cleanup(void)
{
	sstate_disconnect();
}
