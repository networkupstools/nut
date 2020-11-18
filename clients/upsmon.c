/* upsmon - monitor power status over the 'net (talks to upsd via TCP)

   Copyright (C)
     1998  Russell Kroll <rkroll@exploits.org>
     2012  Arnaud Quette <arnaud.quette.free.fr>

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

#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include "upsclient.h"
#include "upsmon.h"
#include "parseconf.h"
#include "timehead.h"

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

static	char	*shutdowncmd = NULL, *notifycmd = NULL;
static	char	*powerdownflag = NULL, *configfile = NULL;

static	int	minsupplies = 1, sleepval = 5, deadtime = 15;

	/* default polling interval = 5 sec */
static	int	pollfreq = 5, pollfreqalert = 5;

	/* slave hosts are given 15 sec by default to logout from upsd */
static	int	hostsync = 15;

	/* sum of all power values from config file */
static	int	totalpv = 0;

	/* default replace battery warning interval (seconds) */
static	int	rbwarntime = 43200;

	/* default "all communications down" warning interval (seconds) */
static	int	nocommwarntime = 300;

	/* default interval between the shutdown warning and the shutdown */
static	int	finaldelay = 5;

	/* set by SIGHUP handler, cleared after reload finishes */
static	int	reload_flag = 0;

	/* set after SIGINT, SIGQUIT, or SIGTERM */
static	int	exit_flag = 0;

	/* userid for unprivileged process when using fork mode */
static	char	*run_as_user = NULL;

	/* SSL details - where to find certs, whether to use them */
static	char	*certpath = NULL;
static	char	*certname = NULL;
static	char	*certpasswd = NULL;
static	int	certverify = 0;		/* don't verify by default */
static	int	forcessl = 0;		/* don't require ssl by default */

static	int	userfsd = 0, use_pipe = 1, pipefd[2];

static	utype_t	*firstups = NULL;

static int 	opt_af = AF_UNSPEC;

	/* signal handling things */
static	struct sigaction sa;
static	sigset_t nut_upsmon_sigmask;

static void setflag(int *val, int flag)
{
	*val |= flag;
}

static void clearflag(int *val, int flag)
{
	*val ^= (*val & flag);
}

static int flag_isset(int num, int flag)
{
	return ((num & flag) == flag);
}

static void wall(const char *text)
{
	FILE	*wf;

	wf = popen("wall", "w");

	if (!wf) {
		upslog_with_errno(LOG_NOTICE, "Can't invoke wall");
		return;
	}

	fprintf(wf, "%s\n", text);
	pclose(wf);
}

static void notify(const char *notice, int flags, const char *ntype,
			const char *upsname)
{
	char	exec[LARGEBUF];
	int	ret;

	if (flag_isset(flags, NOTIFY_IGNORE))
		return;

	if (flag_isset(flags, NOTIFY_SYSLOG))
		upslogx(LOG_NOTICE, "%s", notice);

	/* fork here so upsmon doesn't get wedged if the notifier is slow */
	ret = fork();

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "Can't fork to notify");
		return;
	}

	if (ret != 0)	/* parent */
		return;

	/* child continues and does all the work */

	if (flag_isset(flags, NOTIFY_WALL))
		wall(notice);

	if (flag_isset(flags, NOTIFY_EXEC)) {
		if (notifycmd != NULL) {
			snprintf(exec, sizeof(exec), "%s \"%s\"", notifycmd, notice);

			if (upsname)
				setenv("UPSNAME", upsname, 1);
			else
				setenv("UPSNAME", "", 1);

			setenv("NOTIFYTYPE", ntype, 1);
			if (system(exec) == -1) {
				upslog_with_errno(LOG_ERR, "%s", __func__);
			}
		}
	}

	exit(EXIT_SUCCESS);
}

static void do_notify(const utype_t *ups, int ntype)
{
	int	i;
	char	msg[SMALLBUF], *upsname = NULL;

	/* grab this for later */
	if (ups)
		upsname = ups->sys;

	for (i = 0; notifylist[i].name != NULL; i++) {
		if (notifylist[i].type == ntype) {
			upsdebugx(2, "%s: ntype 0x%04x (%s)", __func__, ntype,
				notifylist[i].name);
			snprintf(msg, sizeof(msg), notifylist[i].msg ? notifylist[i].msg : notifylist[i].stockmsg,
				ups ? ups->sys : "");
			notify(msg, notifylist[i].flags, notifylist[i].name,
				upsname);
			return;
		}
	}

	/* not found ?! */
}

/* check for master permissions on the server for this ups */
static int checkmaster(utype_t *ups)
{
	char	buf[SMALLBUF];

	/* don't bother if we're not configured as a master for this ups */
	if (!flag_isset(ups->status, ST_MASTER))
		return 1;

	/* this shouldn't happen (LOGIN checks it earlier) */
	if ((ups->upsname == NULL) || (strlen(ups->upsname) == 0)) {
		upslogx(LOG_ERR, "Set master on UPS [%s] failed: empty upsname",
			ups->sys);
		return 0;
	}

	snprintf(buf, sizeof(buf), "MASTER %s\n", ups->upsname);

	if (upscli_sendline(&ups->conn, buf, strlen(buf)) < 0) {
		upslogx(LOG_ALERT, "Can't set master mode on UPS [%s] - %s",
			ups->sys, upscli_strerror(&ups->conn));
		return 0;
	}

	if (upscli_readline(&ups->conn, buf, sizeof(buf)) == 0) {
		if (!strncmp(buf, "OK", 2))
			return 1;

		/* not ERR, but not caught by readline either? */

		upslogx(LOG_ALERT, "Master privileges unavailable on UPS [%s]",
			ups->sys);
		upslogx(LOG_ALERT, "Response: [%s]", buf);
	}
	else {	/* something caught by readraw's parsing call */
		upslogx(LOG_ALERT, "Master privileges unavailable on UPS [%s]",
			ups->sys);
		upslogx(LOG_ALERT, "Reason: %s", upscli_strerror(&ups->conn));
	}

	return 0;
}

/* authenticate to upsd, plus do LOGIN and MASTER if applicable */
static int do_upsd_auth(utype_t *ups)
{
	char	buf[SMALLBUF];

	if (!ups->un) {
		upslogx(LOG_ERR, "UPS [%s]: no username defined!", ups->sys);
		return 0;
	}

	snprintf(buf, sizeof(buf), "USERNAME %s\n", ups->un);
	if (upscli_sendline(&ups->conn, buf, strlen(buf)) < 0) {
		upslogx(LOG_ERR, "Can't set username on [%s]: %s",
			ups->sys, upscli_strerror(&ups->conn));
			return 0;
	}

	if (upscli_readline(&ups->conn, buf, sizeof(buf)) < 0) {
		upslogx(LOG_ERR, "Set username on [%s] failed: %s",
			ups->sys, upscli_strerror(&ups->conn));
		return 0;
	}

	/* authenticate first */
	snprintf(buf, sizeof(buf), "PASSWORD %s\n", ups->pw);

	if (upscli_sendline(&ups->conn, buf, strlen(buf)) < 0) {
		upslogx(LOG_ERR, "Can't set password on [%s]: %s",
			ups->sys, upscli_strerror(&ups->conn));
			return 0;
	}

	if (upscli_readline(&ups->conn, buf, sizeof(buf)) < 0) {
		upslogx(LOG_ERR, "Set password on [%s] failed: %s",
			ups->sys, upscli_strerror(&ups->conn));
		return 0;
	}

	/* catch insanity from the server - not ERR and not OK either */
	if (strncmp(buf, "OK", 2) != 0) {
		upslogx(LOG_ERR, "Set password on [%s] failed - got [%s]",
			ups->sys, buf);
		return 0;
	}

	/* we require a upsname now */
	if ((ups->upsname == NULL) || (strlen(ups->upsname) == 0)) {
		upslogx(LOG_ERR, "Login to UPS [%s] failed: empty upsname",
			ups->sys);
		return 0;
	}

	/* password is set, let's login */
	snprintf(buf, sizeof(buf), "LOGIN %s\n", ups->upsname);

	if (upscli_sendline(&ups->conn, buf, strlen(buf)) < 0) {
		upslogx(LOG_ERR, "Login to UPS [%s] failed: %s",
			ups->sys, upscli_strerror(&ups->conn));
		return 0;
	}

	if (upscli_readline(&ups->conn, buf, sizeof(buf)) < 0) {
		upslogx(LOG_ERR, "Can't login to UPS [%s]: %s",
			ups->sys, upscli_strerror(&ups->conn));
		return 0;
	}

	/* catch insanity from the server - not ERR and not OK either */
	if (strncmp(buf, "OK", 2) != 0) {
		upslogx(LOG_ERR, "Login on UPS [%s] failed - got [%s]",
			ups->sys, buf);
		return 0;
	}

	/* finally - everything is OK */
	upsdebugx(1, "Logged into UPS %s", ups->sys);
	setflag(&ups->status, ST_LOGIN);

	/* now see if we also need to test master permissions */
	return checkmaster(ups);
}

/* set flags and make announcements when a UPS has been checked successfully */
static void ups_is_alive(utype_t *ups)
{
	time_t	now;

	time(&now);
	ups->lastpoll = now;

	if (ups->commstate == 1)		/* already known */
		return;

	/* only notify for 0->1 transitions (to ignore the first connect) */
	if (ups->commstate == 0)
		do_notify(ups, NOTIFY_COMMOK);

	ups->commstate = 1;
}

/* handle all the notifications for a missing UPS in one place */
static void ups_is_gone(utype_t *ups)
{
	time_t	now;

	/* first time: clear the flag and throw the first notifier */
	if (ups->commstate != 0) {
		ups->commstate = 0;

		/* COMMBAD is the initial loss of communications */
		do_notify(ups, NOTIFY_COMMBAD);
		return;
	}

	time(&now);

	/* first only act if we're <nocommtime> seconds past the last poll */
	if ((now - ups->lastpoll) < nocommwarntime)
		return;

	/* now only complain if we haven't lately */
	if ((now - ups->lastncwarn) > nocommwarntime) {

		/* NOCOMM indicates a persistent condition */
		do_notify(ups, NOTIFY_NOCOMM);
		ups->lastncwarn = now;
	}
}

static void ups_on_batt(utype_t *ups)
{
	if (flag_isset(ups->status, ST_ONBATT)) { 	/* no change */
		upsdebugx(4, "%s: %s (no change)", __func__, ups->sys);
		return;
	}

	sleepval = pollfreqalert;	/* bump up polling frequency */

	ups->linestate = 0;

	upsdebugx(3, "%s: %s (first time)", __func__, ups->sys);

	/* must have changed from OL to OB, so notify */

	do_notify(ups, NOTIFY_ONBATT);
	setflag(&ups->status, ST_ONBATT);
	clearflag(&ups->status, ST_ONLINE);
}

static void ups_on_line(utype_t *ups)
{
	if (flag_isset(ups->status, ST_ONLINE)) { 	/* no change */
		upsdebugx(4, "%s: %s (no change)", __func__, ups->sys);
		return;
	}

	sleepval = pollfreq;

	upsdebugx(3, "%s: %s (first time)", __func__, ups->sys);

	/* ignore the first OL at startup, otherwise send the notifier */
	if (ups->linestate != -1)
		do_notify(ups, NOTIFY_ONLINE);

	ups->linestate = 1;

	setflag(&ups->status, ST_ONLINE);
	clearflag(&ups->status, ST_ONBATT);
}

/* create the flag file if necessary */
static void set_pdflag(void)
{
	FILE	*pdf;

	if (!powerdownflag)
		return;

	pdf = fopen(powerdownflag, "w");
	if (!pdf) {
		upslogx(LOG_ERR, "Failed to create power down flag!");
		return;
	}

	fprintf(pdf, "%s", SDMAGIC);
	fclose(pdf);
}

/* the actual shutdown procedure */
static void doshutdown(void)
{
	int	ret;

	/* this should probably go away at some point */
	upslogx(LOG_CRIT, "Executing automatic power-fail shutdown");
	wall("Executing automatic power-fail shutdown\n");

	do_notify(NULL, NOTIFY_SHUTDOWN);

	sleep(finaldelay);

	/* in the pipe model, we let the parent do this for us */
	if (use_pipe) {
		char	ch;

		ch = 1;
		ret = write(pipefd[1], &ch, 1);
	} else {
		/* one process model = we do all the work here */

		if (geteuid() != 0)
			upslogx(LOG_WARNING, "Not root, shutdown may fail");

		set_pdflag();

		ret = system(shutdowncmd);

		if (ret != 0)
			upslogx(LOG_ERR, "Unable to call shutdown command: %s",
				shutdowncmd);
	}

	exit(EXIT_SUCCESS);
}

/* set forced shutdown flag so other upsmons know what's going on here */
static void setfsd(utype_t *ups)
{
	char	buf[SMALLBUF];
	int	ret;

	/* this shouldn't happen */
	if (!ups->upsname) {
		upslogx(LOG_ERR, "setfsd: programming error: no UPS name set [%s]",
			ups->sys);
		return;
	}

	upsdebugx(2, "Setting FSD on UPS %s", ups->sys);

	snprintf(buf, sizeof(buf), "FSD %s\n", ups->upsname);

	ret = upscli_sendline(&ups->conn, buf, strlen(buf));

	if (ret < 0) {
		upslogx(LOG_ERR, "FSD set on UPS %s failed: %s", ups->sys,
			upscli_strerror(&ups->conn));
		return;
	}

	ret = upscli_readline(&ups->conn, buf, sizeof(buf));

	if (ret < 0) {
		upslogx(LOG_ERR, "FSD set on UPS %s failed: %s", ups->sys,
			upscli_strerror(&ups->conn));
		return;
	}

	if (!strncmp(buf, "OK", 2))
		return;

	/* protocol error: upsd said something other than "OK" */
	upslogx(LOG_ERR, "FSD set on UPS %s failed: %s", ups->sys, buf);
}

static void set_alarm(void)
{
	alarm(NET_TIMEOUT);
}

static void clear_alarm(void)
{
	signal(SIGALRM, SIG_IGN);
	alarm(0);
}

static int get_var(utype_t *ups, const char *var, char *buf, size_t bufsize)
{
	int	ret;
	unsigned int	numq, numa;
	const	char	*query[4];
	char	**answer;

	/* this shouldn't happen */
	if (!ups->upsname) {
		upslogx(LOG_ERR, "get_var: programming error: no UPS name set [%s]",
			ups->sys);
		return -1;
	}

	numq = 0;

	if (!strcmp(var, "numlogins")) {
		query[0] = "NUMLOGINS";
		query[1] = ups->upsname;
		numq = 2;
	}

	if (!strcmp(var, "status")) {
		query[0] = "VAR";
		query[1] = ups->upsname;
		query[2] = "ups.status";
		numq = 3;
	}

	if (numq == 0) {
		upslogx(LOG_ERR, "get_var: programming error: var=%s", var);
		return -1;
	}

	upsdebugx(3, "%s: %s / %s", __func__, ups->sys, var);

	ret = upscli_get(&ups->conn, numq, query, &numa, &answer);

	if (ret < 0) {

		/* detect old upsd */
		if (upscli_upserror(&ups->conn) == UPSCLI_ERR_UNKCOMMAND) {

			upslogx(LOG_ERR, "UPS [%s]: Too old to monitor",
				ups->sys);
			return -1;
		}

		/* some other error */
		return -1;
	}

	if (numa < numq) {
		upslogx(LOG_ERR, "%s: Error: insufficient data "
			"(got %d args, need at least %d)",
			var, numa, numq);
		return -1;
	}

	snprintf(buf, bufsize, "%s", answer[numq]);
	return 0;
}

static void slavesync(void)
{
	utype_t	*ups;
	char	temp[SMALLBUF];
	time_t	start, now;
	int	maxlogins, logins;

	time(&start);

	for (;;) {
		maxlogins = 0;

		for (ups = firstups; ups != NULL; ups = ups->next) {

			/* only check login count on our master(s) */
			if (!flag_isset(ups->status, ST_MASTER))
				continue;

			set_alarm();

			if (get_var(ups, "numlogins", temp, sizeof(temp)) >= 0) {
				logins = strtol(temp, (char **)NULL, 10);

				if (logins > maxlogins)
					maxlogins = logins;
			}

			clear_alarm();
		}

		/* if no UPS has more than 1 login (us), then slaves are gone */
		if (maxlogins <= 1)
			return;

		/* after HOSTSYNC seconds, assume slaves are stuck and bail */
		time(&now);

		if ((now - start) > hostsync) {
			upslogx(LOG_INFO, "Host sync timer expired, forcing shutdown");
			return;
		}

		usleep(250000);
	}
}

static void forceshutdown(void)
{
	utype_t	*ups;
	int	isamaster = 0;

	upsdebugx(1, "Shutting down any UPSes in MASTER mode...");

	/* set FSD on any "master" UPS entries (forced shutdown in progress) */
	for (ups = firstups; ups != NULL; ups = ups->next)
		if (flag_isset(ups->status, ST_MASTER)) {
			isamaster = 1;
			setfsd(ups);
		}

	/* if we're not a master on anything, we should shut down now */
	if (!isamaster)
		doshutdown();

	/* must be the master now */
	upsdebugx(1, "This system is a master... waiting for slave logout...");

	/* wait up to HOSTSYNC seconds for slaves to logout */
	slavesync();

	/* time expired or all the slaves are gone, so shutdown */
	doshutdown();
}

static int is_ups_critical(utype_t *ups)
{
	time_t	now;

	/* FSD = the master is forcing a shutdown */
	if (flag_isset(ups->status, ST_FSD))
		return 1;

	/* not OB or not LB = not critical yet */
	if ((!flag_isset(ups->status, ST_ONBATT)) ||
		(!flag_isset(ups->status, ST_LOWBATT)))
		return 0;

	/* must be OB+LB now */

	/* if UPS is calibrating, don't declare it critical */
	/* FIXME: Consider UPSes where we can know if they have other power
	 * circuits (bypass, etc.) and whether those do currently provide
	 * wall power to the host - and that we do not have both calibration
	 * and a real outage, when we still should shut down right now.
	 */
	if (flag_isset(ups->status, ST_CAL)) {
		upslogx(LOG_WARNING, "%s: seems that UPS [%s] is OB+LB now, but "
			"it is also calibrating - not declaring a critical state",
			  __func__, ups->upsname);
		return 0;
	}

	/* if we're a master, declare it critical so we set FSD on it */
	if (flag_isset(ups->status, ST_MASTER))
		return 1;

	/* must be a slave now */

	/* FSD isn't set, so the master hasn't seen it yet */

	time(&now);

	/* give the master up to HOSTSYNC seconds before shutting down */
	if ((now - ups->lastnoncrit) > hostsync) {
		upslogx(LOG_WARNING, "Giving up on the master for UPS [%s]",
			ups->sys);
		return 1;
	}

	/* there's still time left */
	return 0;
}

/* recalculate the online power value and see if things are still OK */
static void recalc(void)
{
	utype_t	*ups;
	int	val_ol = 0;
	time_t	now;

	time(&now);
	ups = firstups;
	while (ups != NULL) {
		/* promote dead UPSes that were last known OB to OB+LB */
		if ((now - ups->lastpoll) > deadtime)
			if (flag_isset(ups->status, ST_ONBATT)) {
				upsdebugx(1, "Promoting dead UPS: %s", ups->sys);
				setflag(&ups->status, ST_LOWBATT);
			}

		/* note: we assume that a UPS that isn't critical must be OK *
		 *                                                           *
		 * this means a UPS we've never heard from is assumed OL     *
		 * whether this is really the best thing to do is undecided  */

		/* crit = (FSD) || (OB & LB) > HOSTSYNC seconds */
		if (is_ups_critical(ups))
			upsdebugx(1, "Critical UPS: %s", ups->sys);
		else
			val_ol += ups->pv;

		ups = ups->next;
	}

	upsdebugx(3, "Current power value: %d", val_ol);
	upsdebugx(3, "Minimum power value: %d", minsupplies);

	if (val_ol < minsupplies)
		forceshutdown();
}

static void ups_low_batt(utype_t *ups)
{
	if (flag_isset(ups->status, ST_LOWBATT)) { 	/* no change */
		upsdebugx(4, "%s: %s (no change)", __func__, ups->sys);
		return;
	}

	upsdebugx(3, "%s: %s (first time)", __func__, ups->sys);

	/* must have changed from !LB to LB, so notify */

	do_notify(ups, NOTIFY_LOWBATT);
	setflag(&ups->status, ST_LOWBATT);
}

static void upsreplbatt(utype_t *ups)
{
	time_t	now;

	time(&now);

	if ((now - ups->lastrbwarn) > rbwarntime) {
		do_notify(ups, NOTIFY_REPLBATT);
		ups->lastrbwarn = now;
	}
}

static void ups_cal(utype_t *ups)
{
	if (flag_isset(ups->status, ST_CAL)) { 	/* no change */
		upsdebugx(4, "%s: %s (no change)", __func__, ups->sys);
		return;
	}

	upsdebugx(3, "%s: %s (first time)", __func__, ups->sys);

	/* must have changed from !CAL to CAL, so notify */

	do_notify(ups, NOTIFY_CAL);
	setflag(&ups->status, ST_CAL);
}

static void ups_fsd(utype_t *ups)
{
	if (flag_isset(ups->status, ST_FSD)) {		/* no change */
		upsdebugx(4, "%s: %s (no change)", __func__, ups->sys);
		return;
	}

	upsdebugx(3, "%s: %s (first time)", __func__, ups->sys);

	/* must have changed from !FSD to FSD, so notify */

	do_notify(ups, NOTIFY_FSD);
	setflag(&ups->status, ST_FSD);
}

/* cleanly close the connection to a given UPS */
static void drop_connection(utype_t *ups)
{
	upsdebugx(2, "Dropping connection to UPS [%s]", ups->sys);

	ups->commstate = 0;
	ups->linestate = 0;
	clearflag(&ups->status, ST_LOGIN);
	clearflag(&ups->status, ST_CONNECTED);

	upscli_disconnect(&ups->conn);
}

/* change some UPS parameters during reloading */
static void redefine_ups(utype_t *ups, int pv, const char *un,
		const char *pw, const char *master)
{
	ups->retain = 1;

	if (ups->pv != pv) {
		upslogx(LOG_INFO, "UPS [%s]: redefined power value to %d",
			ups->sys, pv);
		ups->pv = pv;
	}

	totalpv += ups->pv;

	if (ups->un) {
		if (strcmp(ups->un, un) != 0) {
			upslogx(LOG_INFO, "UPS [%s]: redefined username",
				ups->sys);

			free(ups->un);

			ups->un = xstrdup(un);

			/*
			 * if not logged in force a reconnection since this
			 * may have been redefined to make a login work
			 */

			if (!flag_isset(ups->status, ST_LOGIN)) {
				upslogx(LOG_INFO, "UPS [%s]: retrying connection",
					ups->sys);

				drop_connection(ups);
			}

		}	/* if (strcmp(ups->un, un) != 0) { */

	} else {

		/* adding a username? (going to new style MONITOR line) */

		if (un) {
			upslogx(LOG_INFO, "UPS [%s]: defined username",
				ups->sys);

			ups->un = xstrdup(un);

			/* possibly force reconnection - see above */

			if (!flag_isset(ups->status, ST_LOGIN)) {
				upslogx(LOG_INFO, "UPS [%s]: retrying connection",
					ups->sys);

				drop_connection(ups);
			}

		}	/* if (un) */
	}

	/* paranoia */
	if (!ups->pw)
		ups->pw = xstrdup("");	/* give it a bogus, but non-NULL one */

	/* obviously don't put the new password in the syslog... */
	if (strcmp(ups->pw, pw) != 0) {
		upslogx(LOG_INFO, "UPS [%s]: redefined password", ups->sys);

		free(ups->pw);
		ups->pw = xstrdup(pw);

		/* possibly force reconnection - see above */

		if (!flag_isset(ups->status, ST_LOGIN)) {
			upslogx(LOG_INFO, "UPS [%s]: retrying connection",
				ups->sys);

			drop_connection(ups);
		}
	}

	/* slave -> master */
	if ((!strcasecmp(master, "master")) && (!flag_isset(ups->status, ST_MASTER))) {
		upslogx(LOG_INFO, "UPS [%s]: redefined as master", ups->sys);
		setflag(&ups->status, ST_MASTER);

		/* reset connection to ensure master mode gets checked */
		drop_connection(ups);
		return;
	}

	/* master -> slave */
	if ((!strcasecmp(master, "slave")) && (flag_isset(ups->status, ST_MASTER))) {
		upslogx(LOG_INFO, "UPS [%s]: redefined as slave", ups->sys);
		clearflag(&ups->status, ST_MASTER);
		return;
	}
}

static void addups(int reloading, const char *sys, const char *pvs,
		const char *un, const char *pw, const char *master)
{
	int	pv;
	utype_t	*tmp, *last;

	/* the username is now required - no more host-based auth */

	if ((!sys) || (!pvs) || (!pw) || (!master) || (!un)) {
		upslogx(LOG_WARNING, "Ignoring invalid MONITOR line in %s!", configfile);
		upslogx(LOG_WARNING, "MONITOR configuration directives require five arguments.");
		return;
	}

	pv = strtol(pvs, (char **) NULL, 10);

	if (pv < 0) {
		upslogx(LOG_WARNING, "UPS [%s]: ignoring invalid power value [%s]",
			sys, pvs);
		return;
	}

	last = tmp = firstups;

	while (tmp) {
		last = tmp;

		/* check for duplicates */
		if (!strcmp(tmp->sys, sys)) {
			if (reloading)
				redefine_ups(tmp, pv, un, pw, master);
			else
				upslogx(LOG_WARNING, "Warning: ignoring duplicate"
					" UPS [%s]", sys);
			return;
		}

		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(utype_t));
	tmp->sys = xstrdup(sys);
	tmp->pv = pv;

	/* build this up so the user doesn't run with bad settings */
	totalpv += tmp->pv;

	tmp->un = xstrdup(un);

	tmp->pw = xstrdup(pw);
	tmp->status = 0;
	tmp->retain = 1;

	/* ignore initial COMMOK and ONLINE by default */
	tmp->commstate = -1;
	tmp->linestate = -1;

	tmp->lastpoll = 0;
	tmp->lastnoncrit = 0;
	tmp->lastrbwarn = 0;
	tmp->lastncwarn = 0;

	if (!strcasecmp(master, "master"))
		setflag(&tmp->status, ST_MASTER);

	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		firstups = tmp;

	if (tmp->pv)
		upslogx(LOG_INFO, "UPS: %s (%s) (power value %d)", tmp->sys,
			flag_isset(tmp->status, ST_MASTER) ? "master" : "slave",
			tmp->pv);
	else
		upslogx(LOG_INFO, "UPS: %s (monitoring only)", tmp->sys);

	tmp->upsname = tmp->hostname = NULL;

	if (upscli_splitname(tmp->sys, &tmp->upsname, &tmp->hostname,
		&tmp->port) != 0) {
		upslogx(LOG_ERR, "Error: unable to split UPS name [%s]",
			tmp->sys);
	}

	if (!tmp->upsname)
		upslogx(LOG_WARNING, "Warning: UPS [%s]: no upsname set!",
			tmp->sys);
}

static void set_notifymsg(const char *name, const char *msg)
{
	int	i;

	for (i = 0; notifylist[i].name != NULL; i++) {
		if (!strcasecmp(notifylist[i].name, name)) {
			free(notifylist[i].msg);
			notifylist[i].msg = xstrdup(msg);
			return;
		}
	}

	upslogx(LOG_WARNING, "'%s' is not a valid notify event name", name);
}

static void set_notifyflag(const char *ntype, char *flags)
{
	int	i, pos;
	char	*ptr, *tmp;

	/* find ntype */

	pos = -1;
	for (i = 0; notifylist[i].name != NULL; i++) {
		if (!strcasecmp(notifylist[i].name, ntype)) {
			pos = i;
			break;
		}
	}

	if (pos == -1) {
		upslogx(LOG_WARNING, "Warning: invalid notify type [%s]", ntype);
		return;
	}

	ptr = flags;

	/* zero existing flags */
	notifylist[pos].flags = 0;

	while (ptr) {
		int	newflag;

		tmp = strchr(ptr, '+');
		if (tmp)
			*tmp++ = '\0';

		newflag = 0;

		if (!strcmp(ptr, "SYSLOG"))
			newflag = NOTIFY_SYSLOG;
		if (!strcmp(ptr, "WALL"))
			newflag = NOTIFY_WALL;
		if (!strcmp(ptr, "EXEC"))
			newflag = NOTIFY_EXEC;
		if (!strcmp(ptr, "IGNORE"))
			newflag = NOTIFY_IGNORE;

		if (newflag)
			notifylist[pos].flags |= newflag;
		else
			upslogx(LOG_WARNING, "Invalid notify flag: [%s]", ptr);

		ptr = tmp;
	}
}

/* in split mode, the parent doesn't hear about reloads */
static void checkmode(char *cfgentry, char *oldvalue, char *newvalue,
			int reloading)
{
	/* nothing to do if in "all as root" mode */
	if (use_pipe == 0)
		return;

	/* it's ok if we're not reloading yet */
	if (reloading == 0)
		return;

	/* also nothing to do if it didn't change */
	if ((oldvalue) && (newvalue)) {
		if (!strcmp(oldvalue, newvalue))
			return;
	}

	/* otherwise, yell at them */
	upslogx(LOG_WARNING, "Warning: %s redefined in split-process mode!",
		cfgentry);
	upslogx(LOG_WARNING, "You must restart upsmon for this change to work");
}

/* returns 1 if used, 0 if not, so we can complain about bogus configs */
static int parse_conf_arg(int numargs, char **arg)
{
	/* using up to arg[1] below */
	if (numargs < 2)
		return 0;

	/* SHUTDOWNCMD <cmd> */
	if (!strcmp(arg[0], "SHUTDOWNCMD")) {
		checkmode(arg[0], shutdowncmd, arg[1], reload_flag);

		free(shutdowncmd);
		shutdowncmd = xstrdup(arg[1]);
		return 1;
	}

	/* POWERDOWNFLAG <fn> */
	if (!strcmp(arg[0], "POWERDOWNFLAG")) {
		checkmode(arg[0], powerdownflag, arg[1], reload_flag);

		free(powerdownflag);
		powerdownflag = xstrdup(arg[1]);

		if (!reload_flag)
			upslogx(LOG_INFO, "Using power down flag file %s",
				arg[1]);

		return 1;
	}

	/* NOTIFYCMD <cmd> */
	if (!strcmp(arg[0], "NOTIFYCMD")) {
		free(notifycmd);
		notifycmd = xstrdup(arg[1]);
		return 1;
	}

	/* POLLFREQ <num> */
	if (!strcmp(arg[0], "POLLFREQ")) {
		pollfreq = atoi(arg[1]);
		return 1;
	}

	/* POLLFREQALERT <num> */
	if (!strcmp(arg[0], "POLLFREQALERT")) {
		pollfreqalert = atoi(arg[1]);
		return 1;
	}

	/* HOSTSYNC <num> */
	if (!strcmp(arg[0], "HOSTSYNC")) {
		hostsync = atoi(arg[1]);
		return 1;
	}

	/* DEADTIME <num> */
	if (!strcmp(arg[0], "DEADTIME")) {
		deadtime = atoi(arg[1]);
		return 1;
	}

	/* MINSUPPLIES <num> */
	if (!strcmp(arg[0], "MINSUPPLIES")) {
		minsupplies = atoi(arg[1]);
		return 1;
	}

	/* RBWARNTIME <num> */
	if (!strcmp(arg[0], "RBWARNTIME")) {
		rbwarntime = atoi(arg[1]);
		return 1;
	}

	/* NOCOMMWARNTIME <num> */
	if (!strcmp(arg[0], "NOCOMMWARNTIME")) {
		nocommwarntime = atoi(arg[1]);
		return 1;
	}

	/* FINALDELAY <num> */
	if (!strcmp(arg[0], "FINALDELAY")) {
		finaldelay = atoi(arg[1]);
		return 1;
	}

	/* RUN_AS_USER <userid> */
 	if (!strcmp(arg[0], "RUN_AS_USER")) {
		free(run_as_user);
		run_as_user = xstrdup(arg[1]);
		return 1;
	}

	/* CERTPATH <path> */
	if (!strcmp(arg[0], "CERTPATH")) {
		free(certpath);
		certpath = xstrdup(arg[1]);
		return 1;
	}

	/* CERTVERIFY (0|1) */
	if (!strcmp(arg[0], "CERTVERIFY")) {
		certverify = atoi(arg[1]);
		return 1;
	}

	/* FORCESSL (0|1) */
	if (!strcmp(arg[0], "FORCESSL")) {
		forcessl = atoi(arg[1]);
		return 1;
	}

	/* using up to arg[2] below */
	if (numargs < 3)
		return 0;

	/* NOTIFYMSG <notify type> <replacement message> */
	if (!strcmp(arg[0], "NOTIFYMSG")) {
		set_notifymsg(arg[1], arg[2]);
		return 1;
	}

	/* NOTIFYFLAG <notify type> <flags> */
	if (!strcmp(arg[0], "NOTIFYFLAG")) {
		set_notifyflag(arg[1], arg[2]);
		return 1;
	}

	/* CERTIDENT <name> <passwd> */
	if (!strcmp(arg[0], "CERTIDENT")) {
		free(certname);
		certname = xstrdup(arg[1]);
		free(certpasswd);
		certpasswd = xstrdup(arg[2]);
		return 1;
	}

	/* using up to arg[4] below */
	if (numargs < 5)
		return 0;

	/* CERTHOST <hostname> <certname> (0|1) (0|1) */
	if (!strcmp(arg[0], "CERTHOST")) {
		upscli_add_host_cert(arg[1], arg[2], atoi(arg[3]), atoi(arg[4]));
		return 1;
	}

	if (!strcmp(arg[0], "MONITOR")) {

		/* original style: no username (only 5 args) */
		if (numargs == 5) {
			upslogx(LOG_ERR, "Unable to use old-style MONITOR line without a username");
			upslogx(LOG_ERR, "Convert it and add a username to upsd.users - see the documentation");

			fatalx(EXIT_FAILURE, "Fatal error: unusable configuration");
		}

		/* <sys> <pwrval> <user> <pw> ("master" | "slave") */
		addups(reload_flag, arg[1], arg[2], arg[3], arg[4], arg[5]);
		return 1;
	}

	/* didn't parse it at all */
	return 0;
}

/* called for fatal errors in parseconf like malloc failures */
static void upsmon_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(%s): %s", configfile, errmsg);
}

static void loadconfig(void)
{
	PCONF_CTX_t	ctx;

	pconf_init(&ctx, upsmon_err);

	if (!pconf_file_begin(&ctx, configfile)) {
		pconf_finish(&ctx);

		if (reload_flag == 1) {
			upslog_with_errno(LOG_ERR, "Reload failed: %s", ctx.errmsg);
			return;
		}

		fatalx(EXIT_FAILURE, "%s", ctx.errmsg);
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				configfile, ctx.linenum, ctx.errmsg);
			continue;
		}

		if (ctx.numargs < 1)
			continue;

		if (!parse_conf_arg(ctx.numargs, ctx.arglist)) {
			unsigned int	i;
			char	errmsg[SMALLBUF];

			snprintf(errmsg, sizeof(errmsg),
				"%s line %d: invalid directive",
				configfile, ctx.linenum);

			for (i = 0; i < ctx.numargs; i++)
				snprintfcat(errmsg, sizeof(errmsg), " %s",
					ctx.arglist[i]);

			upslogx(LOG_WARNING, "%s", errmsg);
		}
	}

	pconf_finish(&ctx);
}

/* SIGPIPE handler */
static void sigpipe(int sig)
{
	upsdebugx(1, "SIGPIPE: dazed and confused, but continuing after signal %i...", sig);
}

/* SIGQUIT, SIGTERM handler */
static void set_exit_flag(int sig)
{
	exit_flag = sig;
}

static void ups_free(utype_t *ups)
{
	free(ups->sys);
	free(ups->upsname);
	free(ups->hostname);
	free(ups->un);
	free(ups->pw);
	free(ups);
}

static void upsmon_cleanup(void)
{
	int	i;
	utype_t	*utmp, *unext;

	/* close all fds */
	utmp = firstups;

	while (utmp) {
		unext = utmp->next;

		drop_connection(utmp);
		ups_free(utmp);

		utmp = unext;
	}

	free(run_as_user);
	free(shutdowncmd);
	free(notifycmd);
	free(powerdownflag);

	for (i = 0; notifylist[i].name != NULL; i++) {
		free(notifylist[i].msg);
	}

	upscli_cleanup();
}

static void user_fsd(int sig)
{
	upslogx(LOG_INFO, "Signal %d: User requested FSD", sig);
	userfsd = 1;
}

static void set_reload_flag(int sig)
{
	NUT_UNUSED_VARIABLE(sig);

	reload_flag = 1;
}

/* handler for alarm when getupsvarfd times out */
static void read_timeout(int sig)
{
	NUT_UNUSED_VARIABLE(sig);

	/* don't do anything here, just return */
}

/* install handlers for a few signals */
static void setup_signals(void)
{
	sigemptyset(&nut_upsmon_sigmask);
	sa.sa_mask = nut_upsmon_sigmask;
	sa.sa_flags = 0;

	sa.sa_handler = sigpipe;
	sigaction(SIGPIPE, &sa, NULL);

	sa.sa_handler = set_exit_flag;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	/* handle timeouts */

	sa.sa_handler = read_timeout;
	sigaction(SIGALRM, &sa, NULL);

	/* deal with the ones from userspace as well */

	sa.sa_handler = user_fsd;
	sigaction(SIGCMD_FSD, &sa, NULL);

	sa.sa_handler = set_reload_flag;
	sigaction(SIGCMD_RELOAD, &sa, NULL);
}

/* remember the last time the ups was not critical (OB + LB) */
static void update_crittimer(utype_t *ups)
{
	/* if !OB, !LB, or CAL, then it's not critical, so log the time */
	if ((!flag_isset(ups->status, ST_ONBATT))  ||
		(!flag_isset(ups->status, ST_LOWBATT)) ||
		(flag_isset(ups->status, ST_CAL))) {

		time(&ups->lastnoncrit);
		return;
	}

	/* fallthrough: let the timer age */
}

/* handle connecting to upsd, plus get SSL going too if possible */
static int try_connect(utype_t *ups)
{
	int	flags = 0, ret;

	upsdebugx(1, "Trying to connect to UPS [%s]", ups->sys);

	clearflag(&ups->status, ST_CONNECTED);

	/* force it if configured that way, just try it otherwise */
	if (forcessl == 1)
		flags |= UPSCLI_CONN_REQSSL;
	else
		flags |= UPSCLI_CONN_TRYSSL;

	if (opt_af == AF_INET)
		flags |= UPSCLI_CONN_INET;

	if (opt_af == AF_INET6)
		flags |= UPSCLI_CONN_INET6;

	if (!certpath) {
		if (certverify == 1) {
			upslogx(LOG_ERR, "Configuration error: "
				"CERTVERIFY is set, but CERTPATH isn't");
			upslogx(LOG_ERR, "UPS [%s]: Connection impossible, "
				"dropping link", ups->sys);

			ups_is_gone(ups);
			drop_connection(ups);

			return 0;	/* failed */
		}
	}

	if (certverify == 1) {
		flags |= UPSCLI_CONN_CERTVERIF;
	}

	ret = upscli_connect(&ups->conn, ups->hostname, ups->port, flags);

	if (ret < 0) {
		upslogx(LOG_ERR, "UPS [%s]: connect failed: %s",
			ups->sys, upscli_strerror(&ups->conn));
		ups_is_gone(ups);
		return 0;
	}

	/* we're definitely connected now */
	setflag(&ups->status, ST_CONNECTED);

	/* prevent connection leaking to NOTIFYCMD */
	fcntl(upscli_fd(&ups->conn), F_SETFD, FD_CLOEXEC);

	/* now try to authenticate to upsd */

	ret = do_upsd_auth(ups);

	if (ret == 1)
		return 1;		/* everything is happy */

	/* something failed in the auth so we may not be completely logged in */

	/* FUTURE: do something beyond the error msgs from do_upsd_auth? */

	return 0;
}

/* deal with the contents of STATUS or ups.status for this ups */
static void parse_status(utype_t *ups, char *status)
{
	char	*statword, *ptr;

	clear_alarm();

	upsdebugx(2, "%s: [%s]", __func__, status);

	/* empty response is the same as a dead ups */
	if (!strcmp(status, "")) {
		ups_is_gone(ups);
		return;
	}

	ups_is_alive(ups);

	/* clear these out early if they disappear */
	if (!strstr(status, "LB"))
		clearflag(&ups->status, ST_LOWBATT);
	if (!strstr(status, "FSD"))
		clearflag(&ups->status, ST_FSD);

	statword = status;

	/* split up the status words and parse each one separately */
	while (statword != NULL) {
		ptr = strchr(statword, ' ');
		if (ptr)
			*ptr++ = '\0';

		upsdebugx(3, "parsing: [%s]", statword);

		if (!strcasecmp(statword, "OL"))
			ups_on_line(ups);
		if (!strcasecmp(statword, "OB"))
			ups_on_batt(ups);
		if (!strcasecmp(statword, "LB"))
			ups_low_batt(ups);
		if (!strcasecmp(statword, "RB"))
			upsreplbatt(ups);
		if (!strcasecmp(statword, "CAL"))
			ups_cal(ups);

		/* do it last to override any possible OL */
		if (!strcasecmp(statword, "FSD"))
			ups_fsd(ups);

		update_crittimer(ups);

		statword = ptr;
	}
}

/* see what the status of the UPS is and handle any changes */
static void pollups(utype_t *ups)
{
	char	status[SMALLBUF];

	/* try a reconnect here */
	if (!flag_isset(ups->status, ST_CONNECTED))
		if (try_connect(ups) != 1)
			return;

	if (upscli_ssl(&ups->conn) == 1)
		upsdebugx(2, "%s: %s [SSL]", __func__, ups->sys);
	else
		upsdebugx(2, "%s: %s", __func__, ups->sys);

	set_alarm();

	if (get_var(ups, "status", status, sizeof(status)) == 0) {
		clear_alarm();
		parse_status(ups, status);
		return;
	}

	/* fallthrough: no communications */
	clear_alarm();

	/* try to make some of these a little friendlier */

	switch (upscli_upserror(&ups->conn)) {

		case UPSCLI_ERR_UNKNOWNUPS:
			upslogx(LOG_ERR, "Poll UPS [%s] failed - [%s] "
			"does not exist on server %s",
			ups->sys, ups->upsname,	ups->hostname);

			break;
		default:
			upslogx(LOG_ERR, "Poll UPS [%s] failed - %s",
				ups->sys, upscli_strerror(&ups->conn));
			break;
	}

	/* throw COMMBAD or NOCOMM as conditions may warrant */
	ups_is_gone(ups);

	/* if upsclient lost the connection, clean up things on our side */
	if (upscli_fd(&ups->conn) == -1) {
		drop_connection(ups);
		return;
	}
}

/* see if the powerdownflag file is there and proper */
static int pdflag_status(void)
{
	FILE	*pdf;
	char	buf[SMALLBUF];

	if (!powerdownflag)
		return 0;	/* unusable */

	pdf = fopen(powerdownflag, "r");

	if (pdf == NULL)
		return 0;	/* not there */

	/* if it exists, see if it has the right text in it */

	if (fgets(buf, sizeof(buf), pdf) == NULL) {
		upslog_with_errno(LOG_ERR, "'%s' exists, but we can't read from it", powerdownflag);
	}
	fclose(pdf);

	/* reasoning: say upsmon.conf is world-writable (!) and some nasty
	 * user puts something "important" as the power flag file.  This
	 * keeps upsmon from utterly trashing it when starting up or powering
	 * down at the expense of not shutting down the UPS.
	 *
	 * solution: don't let mere mortals edit that configuration file.
	 */

	if (!strncmp(buf, SDMAGIC, strlen(SDMAGIC)))
		return 1;	/* exists and looks good */

	return -1;	/* error: something else is in there */
}

/* only remove the flag file if it's actually from us */
static void clear_pdflag(void)
{
	int	ret;

	ret = pdflag_status();

	if (ret == -1)  {
		upslogx(LOG_ERR, "POWERDOWNFLAG (%s) does not contain"
			"the upsmon magic string - disabling!", powerdownflag);
		powerdownflag = NULL;
		return;
	}

	/* it's from us, so we can remove it */
	if (ret == 1)
		unlink(powerdownflag);
}

/* exit with success only if it exists and is proper */
static int check_pdflag(void)
{
	int	ret;

	ret = pdflag_status();

	if (ret == -1) {
		upslogx(LOG_ERR, "POWERDOWNFLAG (%s) does not contain "
			"the upsmon magic string", powerdownflag);
		return EXIT_FAILURE;
	}

	if (ret == 0) {
		/* not there - this is not a shutdown event */
		upslogx(LOG_ERR, "Power down flag is not set");
		return EXIT_FAILURE;
	}

	if (ret != 1) {
		upslogx(LOG_ERR, "Programming error: pdflag_status returned %d",
			ret);
		return EXIT_FAILURE;
	}

	/* only thing left - must be time for a shutdown */
	upslogx(LOG_INFO, "Power down flag is set");
	return EXIT_SUCCESS;
}

static void help(const char *progname)
{
	printf("Monitors UPS servers and may initiate shutdown if necessary.\n\n");

	printf("usage: %s [OPTIONS]\n\n", progname);
	printf("  -c <cmd>	send command to running process\n");
	printf("		commands:\n");
	printf("		 - fsd: shutdown all master UPSes (use with caution)\n");
	printf("		 - reload: reread configuration\n");
	printf("		 - stop: stop monitoring and exit\n");
	printf("  -D		raise debugging level\n");
	printf("  -h		display this help\n");
	printf("  -K		checks POWERDOWNFLAG, sets exit code to 0 if set\n");
	printf("  -p		always run privileged (disable privileged parent)\n");
	printf("  -u <user>	run child as user <user> (ignored when using -p)\n");
	printf("  -4		IPv4 only\n");
	printf("  -6		IPv6 only\n");

	exit(EXIT_SUCCESS);
}

static void runparent(int fd)
{
	int	ret;
	char	ch;

	/* handling signals is the child's job */
	signal(SIGHUP, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);

	ret = read(fd, &ch, 1);

	if (ret < 1) {
		if (errno == ENOENT)
			fatalx(EXIT_FAILURE, "upsmon parent: exiting (child exited)");

		fatal_with_errno(EXIT_FAILURE, "upsmon parent: read");
	}

	if (ch != 1)
		fatalx(EXIT_FAILURE, "upsmon parent: got bogus pipe command %c", ch);

	/* have to do this here - child is unprivileged */
	set_pdflag();

	ret = system(shutdowncmd);

	if (ret != 0)
		upslogx(LOG_ERR, "parent: Unable to call shutdown command: %s",
			shutdowncmd);

	close(fd);
	exit(EXIT_SUCCESS);
}

/* fire up the split parent/child scheme */
static void start_pipe(void)
{
	int	ret;

	ret = pipe(pipefd);

	if (ret)
		fatal_with_errno(EXIT_FAILURE, "pipe creation failed");

	ret = fork();

	if (ret < 0)
		fatal_with_errno(EXIT_FAILURE, "fork failed");

	/* start the privileged parent */
	if (ret != 0) {
		close(pipefd[1]);
		runparent(pipefd[0]);

		exit(EXIT_FAILURE);	/* NOTREACHED */
	}

	close(pipefd[0]);

	/* prevent pipe leaking to NOTIFYCMD */
	fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
}

static void delete_ups(utype_t *target)
{
	utype_t	*ptr, *last;

	if (!target)
		return;

	ptr = last = firstups;

	while (ptr) {
		if (ptr == target) {
			upslogx(LOG_NOTICE, "No longer monitoring UPS [%s]",
				target->sys);

			/* disconnect cleanly */
			drop_connection(ptr);

			/* about to delete the first ups? */
			if (ptr == last)
				firstups = ptr->next;
			else
				last->next = ptr->next;

			/* release memory */

			ups_free(ptr);

			return;
		}

		last = ptr;
		ptr = ptr->next;
	}

	/* shouldn't happen */
	upslogx(LOG_ERR, "delete_ups: UPS not found");
}

/* see if we can open a file */
static int check_file(const char *fn)
{
	FILE	*f;

	f = fopen(fn, "r");

	if (!f) {
		upslog_with_errno(LOG_ERR, "Reload failed: can't open %s", fn);
		return 0;	/* failed */
	}

	fclose(f);
	return 1;	/* OK */
}

static void reload_conf(void)
{
	utype_t	*tmp, *next;

	upslogx(LOG_INFO, "Reloading configuration");

	/* sanity check */
	if (!check_file(configfile)) {
		reload_flag = 0;
		return;
	}

	/* flip through ups list, clear retain value */
	tmp = firstups;

	while (tmp) {
		tmp->retain = 0;
		tmp = tmp->next;
	}

	/* reset paranoia checker */
	totalpv = 0;

	/* reread upsmon.conf */
	loadconfig();

	/* go through the utype_t struct again */
	tmp = firstups;

	while (tmp) {
		next = tmp->next;

		/* !retain means it wasn't in the .conf this time around */
		if (tmp->retain == 0)
			delete_ups(tmp);

		tmp = next;
	}

	/* see if the user just blew off a foot */
	if (totalpv < minsupplies) {
		upslogx(LOG_CRIT, "Fatal error: total power value (%d) less "
			"than MINSUPPLIES (%d)", totalpv, minsupplies);

		fatalx(EXIT_FAILURE, "Impossible power configuration, unable to continue");
	}

	/* finally clear the flag */
	reload_flag = 0;
}

/* make sure the parent is still alive */
static void check_parent(void)
{
	int	ret;
	fd_set	rfds;
	struct	timeval	tv;
	time_t	now;
	static	time_t	lastwarn = 0;

	FD_ZERO(&rfds);
	FD_SET(pipefd[1], &rfds);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	ret = select(pipefd[1] + 1, &rfds, NULL, NULL, &tv);

	if (ret == 0)
		return;

	/* this should never happen, but we MUST KNOW if it ever does */

	time(&now);

	/* complain every 2 minutes */
	if ((now - lastwarn) < 120)
		return;

	lastwarn = now;
	do_notify(NULL, NOTIFY_NOPARENT);

	/* also do this in case the notifier isn't being effective */
	upslogx(LOG_ALERT, "Parent died - shutdown impossible");
}

int main(int argc, char *argv[])
{
	const char	*prog = xbasename(argv[0]);
	int	i, cmd = 0, checking_flag = 0;

	printf("Network UPS Tools %s %s\n", prog, UPS_VERSION);

	/* if no configuration file is specified on the command line, use default */
	configfile = xmalloc(SMALLBUF);
	snprintf(configfile, SMALLBUF, "%s/upsmon.conf", confpath());
	configfile = xrealloc(configfile, strlen(configfile) + 1);

	run_as_user = xstrdup(RUN_AS_USER);

	while ((i = getopt(argc, argv, "+Dhic:f:pu:VK46")) != -1) {
		switch (i) {
			case 'c':
				if (!strncmp(optarg, "fsd", strlen(optarg)))
					cmd = SIGCMD_FSD;
				if (!strncmp(optarg, "stop", strlen(optarg)))
					cmd = SIGCMD_STOP;
				if (!strncmp(optarg, "reload", strlen(optarg)))
					cmd = SIGCMD_RELOAD;

				/* bad command name given */
				if (cmd == 0)
					help(argv[0]);
				break;
			case 'D':
				nut_debug_level++;
				break;
			case 'f':
				free(configfile);
				configfile = xstrdup(optarg);
				break;
			case 'h':
				help(argv[0]);
				break;
			case 'K':
				checking_flag = 1;
				break;
			case 'p':
				use_pipe = 0;
				break;
			case 'u':
				free(run_as_user);
				run_as_user = xstrdup(optarg);
				break;
			case 'V':
				/* just show the banner */
				exit(EXIT_SUCCESS);
			case '4':
				opt_af = AF_INET;
				break;
			case '6':
				opt_af = AF_INET6;
				break;
			default:
				help(argv[0]);
				break;
		}
	}

	if (cmd) {
		sendsignal(prog, cmd);
		exit(EXIT_SUCCESS);
	}

	/* otherwise, we are being asked to start.
	 * so check if a previous instance is running by sending signal '0'
	 * (Ie 'kill <pid> 0') */
	if (sendsignal(prog, 0) == 0) {
		printf("Fatal error: A previous upsmon instance is already running!\n");
		printf("Either stop the previous instance first, or use the 'reload' command.\n");
		exit(EXIT_FAILURE);
	}

	argc -= optind;
	argv += optind;

	open_syslog(prog);

	loadconfig();

	if (checking_flag)
		exit(check_pdflag());

	if (shutdowncmd == NULL)
		printf("Warning: no shutdown command defined!\n");

	/* we may need to get rid of a flag from a previous shutdown */
	if (powerdownflag != NULL)
		clear_pdflag();
	/* FIXME (else): POWERDOWNFLAG is not defined!!
	 * => fallback to a default value */

	if (totalpv < minsupplies) {
		printf("\nFatal error: insufficient power configured!\n\n");

		printf("Sum of power values........: %d\n", totalpv);
		printf("Minimum value (MINSUPPLIES): %d\n", minsupplies);

		printf("\nEdit your upsmon.conf and change the values.\n");
		exit(EXIT_FAILURE);
	}

	if (nut_debug_level < 1) {
		background();
	} else {
		upsdebugx(1, "debug level is '%d'", nut_debug_level);
	}

	/* only do the pipe stuff if the user hasn't disabled it */
	if (use_pipe) {
		struct passwd	*new_uid = get_user_pwent(run_as_user);

		/* === root parent and unprivileged child split here === */
		start_pipe();

		/* write the pid file now, as we will soon lose root */
		writepid(prog);

		become_user(new_uid);
	} else {
		upslogx(LOG_INFO, "Warning: running as one big root process by request (upsmon -p)");

		writepid(prog);
	}

	if (upscli_init(certverify, certpath, certname, certpasswd) < 0) {
		exit(EXIT_FAILURE);
	}

	/* prep our signal handlers */
	setup_signals();

	/* reopen the log for the child process */
	closelog();
	open_syslog(prog);

	while (exit_flag == 0) {
		utype_t	*ups;

		/* check flags from signal handlers */
		if (userfsd)
			forceshutdown();

		if (reload_flag)
			reload_conf();

		for (ups = firstups; ups != NULL; ups = ups->next)
			pollups(ups);

		recalc();

		/* make sure the parent hasn't died */
		if (use_pipe)
			check_parent();

		/* reap children that have exited */
		waitpid(-1, NULL, WNOHANG);

		sleep(sleepval);
	}

	upslogx(LOG_INFO, "Signal %d: exiting", exit_flag);
	upsmon_cleanup();

	exit(EXIT_SUCCESS);
}
