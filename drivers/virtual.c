/*
* virtual.c: UPS driver clone
*
* Copyright (C) 2009 - Arjen de Korte <adkorte-guest@alioth.debian.org>
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

#include "main.h"
#include "parseconf.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DRIVER_NAME	"Virtual UPS driver"
#define DRIVER_VERSION	"0.01"

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
		int	start;
		int	shutdown;
	} timer;
	char	status[ST_MAX_VALUE_LEN];
} ups = { { -1, -1 }, "" };

static int	dumpdone = 0, online = 1;
static int	offdelay = 120, ondelay = 30;

static PCONF_CTX_t	sock_ctx;
static time_t	lastpoll;

static int parse_args(int numargs, char **arg)
{
	if (numargs < 1) {
		return 0;
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
		return 0;
	}

	/* ADDCMD <cmdname> */
	if (!strcasecmp(arg[0], "ADDCMD")) {
		/* dstate_addcmd(arg[1]); */
		return 1;
	}

	/* DELCMD <cmdname> */
	if (!strcasecmp(arg[0], "DELCMD")) {
		/* dstate_delcmd(arg[1]); */
		return 1;
	}

	/* DELINFO <var> */
	if (!strcasecmp(arg[0], "DELINFO")) {
		dstate_delinfo(arg[1]);
		return 1;
	}

	if (numargs < 3) {
		return 0;
	}

	/* SETFLAGS <varname> <flags>... */
	if (!strcasecmp(arg[0], "SETFLAGS")) {
		/* dstate_setflags(arg[1], arg[2]); */
		return 1;
	}

	/* SETINFO <varname> <value> */
	if (!strcasecmp(arg[0], "SETINFO")) {
		if (!strncasecmp(arg[1], "driver.", 7) ||
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

		dstate_setinfo(arg[1], arg[2]);
		return 1;
	}

	/* ADDENUM <varname> <enumval> */
	if (!strcasecmp(arg[0], "ADDENUM")) {
		/* dstate_addenum(arg[1], arg[2]); */
		return 1;
	}

	/* DELENUM <varname> <enumval> */
	if (!strcasecmp(arg[0], "DELENUM")) {
		/* dstate_delenum(arg[1], arg[2]); */
		return 1;
	}

	/* SETAUX <varname> <auxval> */
	if (!strcasecmp(arg[0], "SETAUX")) {
		/* dstate_setaux(arg[1], arg[2]); */
		return 1;
	}

	return 0;
}


static int sstate_connect(void)
{
	int	ret, fd;
	const char	*dumpcmd = "DUMPALL\n";
	struct sockaddr_un	sa;

	memset(&sa, '\0', sizeof(sa));
	sa.sun_family = AF_UNIX;
	snprintf(sa.sun_path, sizeof(sa.sun_path), "%s/%s", dflt_statepath(), device_path);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0) {
		upslog_with_errno(LOG_ERR, "Can't create socket for UPS [%s]", device_path);
		return -1;
	}

	ret = connect(fd, (struct sockaddr *) &sa, sizeof(sa));

	if (ret < 0) {
		static time_t	last_connfail = 0;
		time_t	now;

		close(fd);

		/* rate-limit complaints - don't spam the syslog */
		time(&now);

		if (difftime(now, last_connfail) < 60) {
			return -1;
		}

		last_connfail = now;

		upslog_with_errno(LOG_ERR, "Can't connect to UPS [%s]", device_path);
		return -1;
	}

	ret = fcntl(fd, F_GETFL, 0);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl get on UPS [%s] failed", device_path);
		close(fd);
		return -1;
	}

	ret = fcntl(fd, F_SETFL, ret | O_NDELAY);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl set O_NDELAY on UPS [%s] failed", device_path);
		close(fd);
		return -1;
	}

	/* get a dump started so we have a fresh set of data */
	ret = write(fd, dumpcmd, strlen(dumpcmd));

	if (ret != (int)strlen(dumpcmd)) {
		upslog_with_errno(LOG_ERR, "Initial write to UPS [%s] failed", device_path);
		close(fd);
		return -1;
	}

	pconf_init(&sock_ctx, NULL);

	dumpdone = 0;

	/* set ups.status to "WAIT" while waiting for the driver response to dumpcmd */
	dstate_setinfo("ups.status", "WAIT");

	upslogx(LOG_INFO, "Connected to UPS [%s]", device_path);
	return fd;
}


static void sstate_disconnect(void)
{
	if (upsfd < 0) {
		return;
	}

	pconf_finish(&sock_ctx);

	close(upsfd);
	upsfd = -1;
}

static int sstate_sendline(const char *buf)
{
	int	ret;

	if (upsfd < 0) {
		return 0;	/* failed */
	}

	ret = write(upsfd, buf, strlen(buf));

	if (ret == (int)strlen(buf)) {
		return 1;
	}

	upslog_with_errno(LOG_NOTICE, "Send to UPS [%s] failed", device_path);
	sstate_disconnect();

	return 0;	/* failed */
}


static int sstate_readline(void)
{
	int	i, ret;
	char	buf[SMALLBUF];

	if (upsfd < 0) {
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

	for (i = 0; i < ret; i++) {

		switch (pconf_char(&sock_ctx, buf[i]))
		{
		case 1:
			parse_args(sock_ctx.numargs, sock_ctx.arglist);
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

static int instcmd(const char *cmdname, const char *extra)
{
	const char	*val;
	int	do_shutdown = 1;

	val = dstate_getinfo(getval("load.status"));
	if (val) {
		if (!strcasecmp(val, "off") || !strcasecmp(val, "no")) {
			do_shutdown = 0;
		}
	}

	if (!strcasecmp(cmdname, "shutdown.return")) {
		if (do_shutdown && (ups.timer.shutdown < 0)) {
			ups.timer.shutdown = offdelay;
			dstate_setinfo("ups.status", "FSD %s", ups.status);
		}
		ups.timer.start = ondelay;
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		if (do_shutdown && (ups.timer.shutdown < 0)) {
			ups.timer.shutdown = offdelay;
			dstate_setinfo("ups.status", "FSD %s", ups.status);
		}
		ups.timer.start = -1;
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	const char	*val;

	time(&lastpoll);

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

	dstate_setinfo("ups.delay.shutdown", "%d", offdelay);
	dstate_setinfo("ups.delay.start", "%d", ondelay);

	dstate_setinfo("ups.timer.shutdown", "%d", ups.timer.shutdown);
	dstate_setinfo("ups.timer.start", "%d", ups.timer.start);

	upsh.instcmd = instcmd;
}

void upsdrv_updateinfo(void)
{
	time_t	now = time(NULL);

	if (sstate_readline() < 0) {
		sstate_disconnect();
		extrafd = upsfd = sstate_connect();
		return;
	}

	if (ups.timer.shutdown >= 0) {

		ups.timer.shutdown -= difftime(now, lastpoll);

		if (ups.timer.shutdown < 0) {
			const char	*val;

			ups.timer.shutdown = -1;

			val = getval("load.off");
			if (val) {
				char	buf[SMALLBUF];
				snprintf(buf, sizeof(buf), "INSTCMD %s\n", val);
				sstate_sendline(buf);
			}
		}

	} else if (ups.timer.start >= 0) {

		if (online) {
			ups.timer.start -= difftime(now, lastpoll);
		} else {
			ups.timer.start = ondelay;
		}

		if (ups.timer.start < 0) {
			const char	*val;

			ups.timer.start = -1;
			dstate_setinfo("ups.status", "%s", ups.status);

			val = getval("load.on");
			if (val) {
				char	buf[SMALLBUF];
				snprintf(buf, sizeof(buf), "INSTCMD %s\n", val);
				sstate_sendline(buf);
			}
		}
	}

	dstate_setinfo("ups.timer.shutdown", "%d", ups.timer.shutdown);
	dstate_setinfo("ups.timer.start", "%d", ups.timer.start);

	lastpoll = now;
}

void upsdrv_shutdown(void)
{
	fatalx(EXIT_FAILURE, "shutdown not supported");
}

/*
static int setvar(const char *varname, const char *val)
{
	if (!strcasecmp(varname, "ups.test.interval")) {
		ser_send_buf(upsfd, ...);
		return STAT_SET_HANDLED;
	}

	upslogx(LOG_NOTICE, "setvar: unknown variable [%s]", varname);
	return STAT_SET_UNKNOWN;
}
*/

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "offdelay", "Delay before outlet shutdown (seconds)");
	addvar(VAR_VALUE, "ondelay", "Delay before outlet startup (seconds)");

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
