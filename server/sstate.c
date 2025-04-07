/* sstate.c - Network UPS Tools server-side state management

   Copyright (C)
	2003	Russell Kroll <rkroll@exploits.org>
	2008	Arjen de Korte <adkorte-guest@alioth.debian.org>
	2012	Arnaud Quette <arnaud.quette@free.fr>

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

#include "sstate.h"
#include "upsd.h"
#include "upstype.h"
#include "nut_stdint.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#include <sys/un.h>
#endif	/* !WIN32 */

static int parse_args(upstype_t *ups, size_t numargs, char **arg)
{
	if (numargs < 1)
		return 0;

	if (!strcasecmp(arg[0], "PONG")) {
		upsdebugx(3, "%s: Got PONG from UPS [%s]", __func__, ups->name);
		return 1;
	}

	if (!strcasecmp(arg[0], "DUMPDONE")) {
		upsdebugx(3, "%s: UPS [%s]: dump is done", __func__, ups->name);
		ups->dumpdone = 1;
		return 1;
	}

	if (!strcasecmp(arg[0], "DATASTALE")) {
		upsdebugx(3, "%s: UPS [%s]: data is STALE now", __func__, ups->name);
		ups->data_ok = 0;
		return 1;
	}

	if (!strcasecmp(arg[0], "DATAOK")) {
		upsdebugx(3, "%s: UPS [%s]: data is NOT STALE now", __func__, ups->name);
		ups->data_ok = 1;
		return 1;
	}

	if (numargs < 2)
		return 0;

	/* FIXME: all these should return their state_...() value! */
	/* ADDCMD <cmdname> */
	if (!strcasecmp(arg[0], "ADDCMD")) {
		state_addcmd(&ups->cmdlist, arg[1]);
		return 1;
	}

	/* DELCMD <cmdname> */
	if (!strcasecmp(arg[0], "DELCMD")) {
		state_delcmd(&ups->cmdlist, arg[1]);
		return 1;
	}

	/* DELINFO <var> */
	if (!strcasecmp(arg[0], "DELINFO")) {
		state_delinfo(&ups->inforoot, arg[1]);
		return 1;
	}

	if (numargs < 3)
		return 0;

	/* SETFLAGS <varname> <flags>... */
	if (!strcasecmp(arg[0], "SETFLAGS")) {
		state_setflags(ups->inforoot, arg[1], numargs - 2, &arg[2]);
		return 1;
	}

	/* SETINFO <varname> <value> */
	if (!strcasecmp(arg[0], "SETINFO")) {
		state_setinfo(&ups->inforoot, arg[1], arg[2]);
		return 1;
	}

	/* ADDENUM <varname> <enumval> */
	if (!strcasecmp(arg[0], "ADDENUM")) {
		state_addenum(ups->inforoot, arg[1], arg[2]);
		return 1;
	}

	/* DELENUM <varname> <enumval> */
	if (!strcasecmp(arg[0], "DELENUM")) {
		state_delenum(ups->inforoot, arg[1], arg[2]);
		return 1;
	}

	/* SETAUX <varname> <auxval> */
	if (!strcasecmp(arg[0], "SETAUX")) {
		state_setaux(ups->inforoot, arg[1], arg[2]);
		return 1;
	}

	/* TRACKING <id> <status> */
	if (!strcasecmp(arg[0], "TRACKING")) {
		tracking_set(arg[1], arg[2]);
		upsdebugx(1, "%s: TRACKING: ID %s status %s", __func__, arg[1], arg[2]);

		/* log actual result of instcmd / setvar */
		if (strncmp(arg[2], "PENDING", 7) != 0) {
			upslogx(LOG_INFO, "tracking ID: %s\tresult: %s", arg[1], tracking_get(arg[1]));
		}
		return 1;
	}

	if (numargs < 4)
		return 0;

	/* ADDRANGE <varname> <minvalue> <maxvalue> */
	if (!strcasecmp(arg[0], "ADDRANGE")) {
		state_addrange(ups->inforoot, arg[1], atoi(arg[2]), atoi(arg[3]));
		return 1;
	}

	/* DELRANGE <varname> <minvalue> <maxvalue> */
	if (!strcasecmp(arg[0], "DELRANGE")) {
		state_delrange(ups->inforoot, arg[1], atoi(arg[2]), atoi(arg[3]));
		return 1;
	}

	return 0;
}

/* nothing fancy - just make the driver say something back to us */
static void sendping(upstype_t *ups)
{
	ssize_t	ret;
	const char	*cmd = "PING\n";
	size_t	cmdlen = strlen(cmd);

	if ((!ups) || INVALID_FD(ups->sock_fd)) {
		return;
	}

	upsdebugx(3, "%s: Pinging UPS [%s]", __func__, ups->name);

#ifndef WIN32
	ret = write(ups->sock_fd, cmd, cmdlen);
#else	/* WIN32 */
	DWORD bytesWritten = 0;
	BOOL  result = FALSE;

	result = WriteFile (ups->sock_fd, cmd, cmdlen, &bytesWritten, NULL);
	if( result == 0 ) {
		/* Write failed */
		ret = 0;
	}
	else  {
		ret = (ssize_t)bytesWritten;
	}
#endif	/* WIN32 */

	if ((ret < 1) || (ret != (ssize_t)cmdlen))  {
		upslog_with_errno(LOG_NOTICE, "Send ping to UPS [%s] failed", ups->name);
		sstate_disconnect(ups);
		return;
	}

	time(&ups->last_ping);
}

/* interface */

TYPE_FD sstate_connect(upstype_t *ups)
{
	TYPE_FD	fd;
#ifndef WIN32
	const char	*dumpcmd = "DUMPALL\n";
	size_t	dumpcmdlen = strlen(dumpcmd);
	ssize_t	ret;
	struct sockaddr_un	sa;

	upsdebugx(2, "%s: preparing UNIX socket %s", __func__, NUT_STRARG(ups->fn));
	check_unix_socket_filename(ups->fn);

	memset(&sa, '\0', sizeof(sa));
	sa.sun_family = AF_UNIX;
	snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", ups->fn);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (INVALID_FD(fd)) {
		upslog_with_errno(LOG_ERR, "Can't create socket for UPS [%s]", ups->name);
		return ERROR_FD;
	}

	ret = connect(fd, (struct sockaddr *) &sa, sizeof(sa));

	if (ret < 0) {
		time_t	now;

		if (strstr(sa.sun_path, "/")) {
			upsdebugx(2, "%s: failed to connect() UNIX socket %s (%s)",
				__func__, NUT_STRARG(ups->fn), sa.sun_path);
		} else {
			char	cwd[NUT_PATH_MAX+1];
			upsdebugx(2, "%s: failed to connect() UNIX socket %s (%s/%s)",
				__func__, NUT_STRARG(ups->fn),
				getcwd(cwd, sizeof(cwd)), sa.sun_path);
		}
		close(fd);

		/* rate-limit complaints - don't spam the syslog */
		time(&now);
		if (difftime(now, ups->last_connfail) < SS_CONNFAIL_INT)
			return ERROR_FD;

		ups->last_connfail = now;
		if (strstr(ups->fn, "/")) {
			upslog_with_errno(LOG_ERR, "Can't connect to UPS [%s] (%s)",
				ups->name, ups->fn);
		} else {
			char	cwd[NUT_PATH_MAX+1];
			upslog_with_errno(LOG_ERR, "Can't connect to UPS [%s] (%s/%s)",
				ups->name, getcwd(cwd, sizeof(cwd)), ups->fn);
		}

		return ERROR_FD;
	}

	ret = fcntl(fd, F_GETFL, 0);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "%s: fcntl get on UPS [%s] failed", __func__, ups->name);
		close(fd);
		return ERROR_FD;
	}

	ret = fcntl(fd, F_SETFL, ret | O_NDELAY);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "%s: fcntl set O_NDELAY on UPS [%s] failed", __func__, ups->name);
		close(fd);
		return ERROR_FD;
	}

	/* get a dump started so we have a fresh set of data */
	ret = write(fd, dumpcmd, dumpcmdlen);

	if ((ret < 1) || (ret != (ssize_t)dumpcmdlen))  {
		upslog_with_errno(LOG_ERR, "Initial write to UPS [%s] failed", ups->name);
		close(fd);
		return ERROR_FD;
	}

#else	/* WIN32 */
	char pipename[NUT_PATH_MAX];
	const char	*dumpcmd = "DUMPALL\n";
	BOOL  result = FALSE;
	DWORD bytesWritten;

	upsdebugx(2, "%s: preparing Windows pipe %s", __func__, NUT_STRARG(ups->fn));
	snprintf(pipename, sizeof(pipename), "\\\\.\\pipe\\%s", ups->fn);

	result = WaitNamedPipe(pipename, NMPWAIT_USE_DEFAULT_WAIT);

	if (result == FALSE) {
		upsdebugx(2, "%s: failed to WaitNamedPipe(%s)",
			__func__, pipename);
		return ERROR_FD;
	}

	fd = CreateFile(
			pipename,       /* pipe name */
			GENERIC_READ |  /* read and write access */
			GENERIC_WRITE,
			0,              /* no sharing */
			NULL,           /* default security attributes FIXME */
			OPEN_EXISTING,  /* opens existing pipe */
			FILE_FLAG_OVERLAPPED, /*  enable async IO */
			NULL);          /* no template file */

	if (fd == INVALID_HANDLE_VALUE) {
		upslog_with_errno(LOG_ERR, "Can't connect to UPS [%s] (%s) named pipe %s",
			ups->name, ups->fn, pipename);
		return ERROR_FD;
	}

	/* get a dump started so we have a fresh set of data */
	bytesWritten = 0;

	result = WriteFile(fd, dumpcmd, strlen(dumpcmd), &bytesWritten, NULL);
	if (result == 0 || bytesWritten != strlen(dumpcmd)) {
		upslog_with_errno(LOG_ERR, "Initial write to UPS [%s] failed", ups->name);
		CloseHandle(fd);
		return ERROR_FD;
	}

	/* Start a read IO so we could wait on the event associated with it */
	ReadFile(fd, ups->buf,
		sizeof(ups->buf) - 1, /*-1 to be sure to have a trailling 0 */
		NULL, &(ups->read_overlapped));
#endif	/* WIN32 */

	/* sstate_connect() continued for both platforms: */

	pconf_init(&ups->sock_ctx, NULL);

	ups->dumpdone = 0;
	ups->stale = 0;

	/* now is the last time we heard something from the driver */
	time(&ups->last_heard);

	/* set ups.status to "WAIT" while waiting for the driver response to dumpcmd */
	state_setinfo(&ups->inforoot, "ups.status", "WAIT");

	upslogx(LOG_INFO, "Connected to UPS [%s]: %s", ups->name, ups->fn);

	return fd;
}

void sstate_disconnect(upstype_t *ups)
{
	if ((!ups) || INVALID_FD(ups->sock_fd)) {
		return;
	}

	sstate_infofree(ups);
	sstate_cmdfree(ups);

	pconf_finish(&ups->sock_ctx);

#ifndef WIN32
	close(ups->sock_fd);
#else	/* WIN32 */
	CloseHandle(ups->sock_fd);
#endif	/* WIN32 */

	ups->sock_fd = ERROR_FD;
}

void sstate_readline(upstype_t *ups)
{
	ssize_t	i, ret;

#ifndef WIN32
	char	buf[SMALLBUF];

	if ((!ups) || INVALID_FD(ups->sock_fd)) {
		return;
	}

	ret = read(ups->sock_fd, buf, sizeof(buf));

	if (ret < 0) {
		switch(errno)
		{
		case EINTR:
		case EAGAIN:
			return;

		default:
			upslog_with_errno(LOG_WARNING, "Read from UPS [%s] failed", ups->name);
			sstate_disconnect(ups);
			return;
		}
	}
#else	/* WIN32 */
	if ((!ups) || INVALID_FD(ups->sock_fd)) {
		return;
	}

	/* FIXME? I do not see either buf filled below */
	char *buf = ups->buf;
	DWORD bytesRead;
	GetOverlappedResult(ups->sock_fd, &ups->read_overlapped, &bytesRead, FALSE);
	ret = bytesRead;
#endif	/* WIN32 */

	for (i = 0; i < ret; i++) {

		switch (pconf_char(&ups->sock_ctx, buf[i]))
		{
		case 1:
			/* set the 'last heard' time to now for later staleness checks */
			if (parse_args(ups, ups->sock_ctx.numargs, ups->sock_ctx.arglist)) {
				time(&ups->last_heard);
			}
			continue;

		case 0:
			continue;	/* haven't gotten a line yet */

		default:
			/* parse error */
			upslogx(LOG_NOTICE, "Parse error on sock: %s", ups->sock_ctx.errmsg);
			return;
		}
	}

#ifdef WIN32
	/* Restart async read */
	memset(ups->buf,0,sizeof(ups->buf));
	ReadFile( ups->sock_fd, ups->buf, sizeof(ups->buf)-1,NULL, &(ups->read_overlapped)); /* -1 to be sure to have a trailing 0 */
#endif	/* WIN32 */
}

const char *sstate_getinfo(const upstype_t *ups, const char *var)
{
	return state_getinfo(ups->inforoot, var);
}

int sstate_getflags(const upstype_t *ups, const char *var)
{
	return state_getflags(ups->inforoot, var);
}

long sstate_getaux(const upstype_t *ups, const char *var)
{
	return state_getaux(ups->inforoot, var);
}

const enum_t *sstate_getenumlist(const upstype_t *ups, const char *var)
{
	return state_getenumlist(ups->inforoot, var);
}

const range_t *sstate_getrangelist(const upstype_t *ups, const char *var)
{
	return state_getrangelist(ups->inforoot, var);
}

const cmdlist_t *sstate_getcmdlist(const upstype_t *ups)
{
	return ups->cmdlist;
}

int sstate_dead(upstype_t *ups, int arg_maxage)
{
	time_t	now;
	double	elapsed;

	/* an unconnected ups is always dead */
	if ((!ups) || INVALID_FD(ups->sock_fd)) {
		upsdebugx(3, "%s: connection to driver socket for UPS [%s] lost", __func__, ups->name);
		return 1;	/* dead */
	}

	time(&now);

	elapsed = difftime(now, ups->last_heard);

	/* Somewhere beyond a third of the maximum time - prod it to make it talk
	 * Note this helps detect drivers that died without closing the connection
	 */
	if ((elapsed > (arg_maxage / 3)) && (difftime(now, ups->last_ping) > (arg_maxage / 3)))
		sendping(ups);

	if (elapsed > arg_maxage) {
		upsdebugx(3, "%s: didn't hear from driver for UPS [%s] for %g seconds (max %d)",
			__func__, ups->name, elapsed, arg_maxage);
		return 1;	/* dead */
	}

	/* ignore DATAOK/DATASTALE unless the dump is done */
	if ((ups->dumpdone) && (!ups->data_ok)) {
		upsdebugx(3, "%s: driver for UPS [%s] says data is stale", __func__, ups->name);
		return 1;	/* dead */
	}

	return 0;
}

/* release all info(tree) data used by <ups> */
void sstate_infofree(upstype_t *ups)
{
	state_infofree(ups->inforoot);

	ups->inforoot = NULL;
}

void sstate_cmdfree(upstype_t *ups)
{
	state_cmdfree(ups->cmdlist);

	ups->cmdlist = NULL;
}

int sstate_sendline(upstype_t *ups, const char *buf)
{
	ssize_t	ret;
	size_t	buflen;

#ifdef WIN32
	DWORD bytesWritten = 0;
	BOOL  result = FALSE;
#endif	/* WIN32 */

	if ((!ups) || INVALID_FD(ups->sock_fd)) {
		return 0;	/* failed */
	}

	buflen = strlen(buf);
	if (buflen >= SSIZE_MAX) {
		/* Can't compare buflen to ret... */
		upslog_with_errno(LOG_NOTICE, "Send ping to UPS [%s] failed: buffered message too large", ups->name);
		return 0;	/* failed */
	}

#ifndef WIN32
	ret = write(ups->sock_fd, buf, buflen);
#else	/* WIN32 */
	result = WriteFile (ups->sock_fd, buf, buflen, &bytesWritten, NULL);

	if (result == 0) {
		ret = 0;
	}
	else {
		ret = (ssize_t)bytesWritten;
	}
#endif	/* WIN32 */

	if (ret == (ssize_t)buflen) {
		return 1;
	}

	upslog_with_errno(LOG_NOTICE, "Send to UPS [%s] failed", ups->name);
	sstate_disconnect(ups);

	return 0;	/* failed */
}

const st_tree_t *sstate_getnode(const upstype_t *ups, const char *varname)
{
	return state_tree_find(ups->inforoot, varname);
}
