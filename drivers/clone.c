/*
* clone.c: UPS driver clone
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

#include "config.h"
#include "main.h"
#include "parseconf.h"
#include "attribute.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DRIVER_NAME	"Clone UPS driver"
#define DRIVER_VERSION	"0.02"

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
static time_t	last_poll = 0, last_heard = 0,
		last_ping = 0, last_connfail = 0;

static int instcmd(const char *cmdname, const char *extra);


static int parse_args(size_t numargs, char **arg)
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

	/* DELINFO <var> */
	if (!strcasecmp(arg[0], "DELINFO")) {
		dstate_delinfo(arg[1]);
		return 1;
	}

	if (numargs < 3) {
		return 0;
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

	return 0;
}


static int sstate_connect(void)
{
	ssize_t	ret;
	int	fd;
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

	time(&last_heard);

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
	ssize_t	ret;

	if (upsfd < 0) {
		return -1;	/* failed */
	}

	ret = write(upsfd, buf, strlen(buf));

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
	if (upsfd < 0) {
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
		if (!strncasecmp(val, "off", 3) || !strncasecmp(val, "no", 2)) {
			outlet = 0;
		}

		if (!strncasecmp(val, "on", 2) || !strncasecmp(val, "yes", 3)) {
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

		ups.timer.shutdown -= difftime(now, last_poll);

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
			ups.timer.start -= difftime(now, last_poll);
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
	__attribute__((noreturn));

void upsdrv_shutdown(void)
{
	fatalx(EXIT_FAILURE, "shutdown not supported");
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
