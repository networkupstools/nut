/* sstate.c - Network UPS Tools server-side state management

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

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> 

#include "common.h"

#include "timehead.h"

#include "upstype.h"
#include "sstate.h"
#include "state.h"

static int parse_args(upstype_t *ups, int numargs, char **arg)
{
	if (numargs < 1)
		return 0;

	if (!strcasecmp(arg[0], "PONG")) {
		upsdebugx(3, "Got PONG from UPS [%s]", ups->name);
		return 1;
	}

	if (!strcasecmp(arg[0], "DUMPDONE")) {
		upsdebugx(3, "UPS [%s]: dump is done", ups->name);
		ups->dumpdone = 1;
		return 1;
	}

	if (!strcasecmp(arg[0], "DATASTALE")) {
		ups->data_ok = 0;
		return 1;
	}

	if (!strcasecmp(arg[0], "DATAOK")) {
		ups->data_ok = 1;
		return 1;
	}

	if (numargs < 2)
		return 0;

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

	return 0;
}

/* nothing fancy - just make the driver say something back to us */
static void sendping(upstype_t *ups)
{
	int	ret;
	const	char	*cmd = "PING\n";

	upsdebugx(3, "Pinging UPS [%s]", ups->name);

	ret = write(ups->sock_fd, cmd, strlen(cmd));

	if ((ret < 1) || (ret != (int) strlen(cmd))) {
		upslog_with_errno(LOG_NOTICE, "Send ping to UPS [%s] failed", ups->name);

		sstate_infofree(ups);
		sstate_cmdfree(ups);
		pconf_finish(&ups->sock_ctx);

		close(ups->sock_fd);
		ups->sock_fd = -1;
		ups->dumpdone = 0;
		return;
	}

	time(&ups->last_ping);
}

/* interface */

int sstate_connect(upstype_t *ups)
{
	int	ret, fd;
	const	char	*dumpcmd = "DUMPALL\n";
	struct	sockaddr_un sa;

	memset(&sa, '\0', sizeof(sa));
	sa.sun_family = AF_UNIX;
	snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", ups->fn);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0) {
		upslog_with_errno(LOG_ERR, "Can't create socket for UPS [%s]", ups->name);
		return -1;
	}

	ret = connect(fd, (struct sockaddr *) &sa, sizeof(sa));

	if (ret < 0) {
		time_t	now;

		close(fd);

		/* rate-limit complaints - don't spam the syslog */
		time(&now);
		if (difftime(now, ups->last_connfail) < SS_CONNFAIL_INT)
			return -1;

		ups->last_connfail = now;
		upslog_with_errno(LOG_ERR, "Can't connect to UPS [%s] (%s)", 
			ups->name, ups->fn);

		return -1;
	}

	ret = fcntl(fd, F_GETFL, 0);

	if (ret < 0) {
		close(fd);
		upslog_with_errno(LOG_ERR, "fcntl get on UPS [%s] failed", ups->name);
		return -1;
	}

	ret = fcntl(fd, F_SETFL, ret | O_NDELAY);

	if (ret < 0) {
		close(fd);
		upslog_with_errno(LOG_ERR, "fcntl set O_NDELAY on UPS [%s] failed", ups->name);
		return -1;
	}

	/* get a dump started so we have a fresh set of data */
	ret = write(fd, dumpcmd, strlen(dumpcmd));

	if ((ret < 1) || (ret != (int) strlen(dumpcmd))) {
		close(fd);
		upslog_with_errno(LOG_ERR, "Initial write to UPS [%s] failed", ups->name);
		return -1;
	}

	/* clear out any old junk from before */
	sstate_infofree(ups);
	sstate_cmdfree(ups);
	pconf_init(&ups->sock_ctx, NULL);
	ups->stale = 0;

	/* set ups.status to "WAIT" while waiting for the driver response to dumpcmd */
	state_setinfo(&ups->inforoot, "ups.status", "WAIT");

	upslogx(LOG_INFO, "Connected to UPS [%s]: %s", ups->name, ups->fn);

	return fd;
}

void sstate_sock_read(upstype_t *ups)
{
	int	i, ret;
	char	ch;

	for (i = 0; i < SS_MAX_READ; i++) {

		ret = read(ups->sock_fd, &ch, 1);

		if (ret < 1) {

			/* ran out of data pre-parse */
			if ((ret == -1) && (errno == EAGAIN))
				return;

			if (ret == 0)
				upslogx(LOG_WARNING, "UPS [%s] disconnected - "
					"check driver", ups->name);
			else
				upslog_with_errno(LOG_WARNING, "Read from UPS [%s] failed",
					ups->name);

			sstate_infofree(ups);
			sstate_cmdfree(ups);
			pconf_finish(&ups->sock_ctx);

			close(ups->sock_fd);
			ups->sock_fd = -1;
			ups->dumpdone = 0;

			return;
		}

		ret = pconf_char(&ups->sock_ctx, ch);

		if (ret == 0)
			continue;	/* haven't gotten a line yet */

		if (ret == 1) {		/* got one - parse it */
			/* set the 'last heard' time to now for later staleness checks */
			if (parse_args(ups, ups->sock_ctx.numargs,
				ups->sock_ctx.arglist))
			        time(&ups->last_heard);
				
			/* only one command per pass */
			return;
		}

		/* parse error */
		upslogx(LOG_NOTICE, "Parse error on sock: %s",
			ups->sock_ctx.errmsg);
	}
}

const char *sstate_getinfo(const upstype_t *ups, const char *var)
{
	/* requesting an old variable name? */
	if (!strchr(var, '.'))
		return NULL;

	return state_getinfo(ups->inforoot, var);
}

int sstate_getflags(const upstype_t *ups, const char *var)
{
	return state_getflags(ups->inforoot, var);
}	

int sstate_getaux(const upstype_t *ups, const char *var)
{
	return state_getaux(ups->inforoot, var);
}	

const struct enum_t *sstate_getenumlist(const upstype_t *ups, const char *var)
{
	return state_getenumlist(ups->inforoot, var);
}

const struct cmdlist_t *sstate_getcmdlist(const upstype_t *ups)
{
	return ups->cmdlist;
}

int sstate_dead(upstype_t *ups, int maxage)
{
	time_t	now;
	double	elapsed;

	/* an unconnected ups is always dead */
	if (ups->sock_fd == -1) {
		upsdebugx(3, "sstate_dead: connection to driver socket for UPS [%s] lost", ups->name);
		return 1;	/* dead */
	}

	time(&now);

	/* ignore DATAOK/DATASTALE unless the dump is done */
	if ((ups->dumpdone) && (!ups->data_ok)) {
		upsdebugx(3, "sstate_dead: driver for UPS [%s] says data is stale", ups->name);
		return 1;	/* dead */
	}

	elapsed = difftime(now, ups->last_heard);

	/* somewhere beyond a third of the maximum time - prod it to make it talk */
	if ((elapsed > (maxage / 3)) && (difftime(now, ups->last_ping) > (maxage / 3)))
		sendping(ups);

	if (elapsed > maxage) {
		upsdebugx(3, "sstate_dead: didn't hear from driver for UPS [%s] for %g seconds (max %d)",
					ups->name, elapsed, maxage);
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
	int	ret;

	if (ups->sock_fd == -1)
		return 0;	/* failed */

	ret = write(ups->sock_fd, buf, strlen(buf));

	if ((ret < 1) || (ret != (int) strlen(buf))) {
		upslog_with_errno(LOG_NOTICE, "Send to UPS [%s] failed", ups->name);

		sstate_infofree(ups);
		sstate_cmdfree(ups);
		pconf_finish(&ups->sock_ctx);

		close(ups->sock_fd);
		ups->sock_fd = -1;

		return 0;	/* failed */
	}

	return 1;	
}

const struct st_tree_t *sstate_getnode(const upstype_t *ups, const char *varname)
{
	return state_tree_find(ups->inforoot, varname);
}
