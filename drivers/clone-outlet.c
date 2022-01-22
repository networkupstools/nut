/*
* clone-outlet.c: clone outlet UPS driver
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

#define DRIVER_NAME	"clone outlet UPS Driver"
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
		char	*shutdown;
	} delay;
	struct {
		char	*shutdown;
	} timer;
	char	*status;
} prefix = { { NULL }, { NULL }, NULL };

static struct {
	struct {
		long	shutdown;
	} delay;
	struct {
		long	shutdown;
	} timer;
	int	status;
} outlet = { { -1 }, { -1 }, 1 };

static struct {
	char	status[LARGEBUF];
} ups = { "WAIT" };

static int	dumpdone = 0;

static PCONF_CTX_t	sock_ctx;
static time_t	last_heard = 0, last_ping = 0, last_connfail = 0;

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

		if (!strncasecmp(arg[1], "driver.", 7)) {
			/* don't pass on upstream driver settings */
			return 1;
		}

		if (!strcasecmp(arg[1], prefix.delay.shutdown)) {
			outlet.delay.shutdown = strtol(arg[2], NULL, 10);
		}

		if (!strcasecmp(arg[1], prefix.timer.shutdown)) {
			outlet.timer.shutdown = strtol(arg[2], NULL, 10);
		}

		if (!strcasecmp(arg[1], prefix.status)) {
			outlet.status = strncasecmp(arg[2], "off", 3);
		}

		if (!strcasecmp(arg[1], "ups.status")) {
			snprintf(ups.status, sizeof(ups.status), "%s", arg[2]);
			return 1;
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


void upsdrv_initinfo(void)
{
}


void upsdrv_updateinfo(void)
{
	if (sstate_dead(15)) {
		sstate_disconnect();
		extrafd = upsfd = sstate_connect();
		return;
	}

	if (sstate_readline()) {
		sstate_disconnect();
		return;
	}

	if (outlet.status == 0) {
		upsdebugx(2, "OFF flag set (%s: switched off)", prefix.status);
		dstate_setinfo("ups.status", "%s OFF", ups.status);
		return;
	}

	if ((outlet.timer.shutdown > -1) && (outlet.timer.shutdown <= outlet.delay.shutdown)) {
		upsdebugx(2, "FSD flag set (%s: -1 < [%ld] <= %ld)", prefix.timer.shutdown, outlet.timer.shutdown, outlet.delay.shutdown);
		dstate_setinfo("ups.status", "FSD %s", ups.status);
		return;
	}

	upsdebugx(3, "%s: power state not critical", getval("prefix"));
	dstate_setinfo("ups.status", "%s", ups.status);
}


void upsdrv_shutdown(void)
{
}


void upsdrv_help(void)
{
}


void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "prefix", "Outlet prefix (mandatory)");
}


void upsdrv_initups(void)
{
	char	buf[SMALLBUF];
	const char	*val;

	val = getval("prefix");
	if (!val) {
		fatalx(EXIT_FAILURE, "Outlet prefix is mandatory for this driver!");
	}

	snprintf(buf, sizeof(buf), "%s.delay.shutdown", val);
	prefix.delay.shutdown = xstrdup(buf);

	snprintf(buf, sizeof(buf), "%s.timer.shutdown", val);
	prefix.timer.shutdown = xstrdup(buf);

	snprintf(buf, sizeof(buf), "%s.status", val);
	prefix.status = xstrdup(buf);

	extrafd = upsfd = sstate_connect();
}


void upsdrv_cleanup(void)
{
	free(prefix.delay.shutdown);
	free(prefix.timer.shutdown);
	free(prefix.status);

	sstate_disconnect();
}
