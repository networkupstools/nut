/* upsmon - monitor power status over the 'net (talks to upsd via TCP)

   Copyright (C)
     1998  Russell Kroll <rkroll@exploits.org>
     2012  Arnaud Quette <arnaud.quette.free.fr>
     2020-2023  Jim Klimov <jimklimov+nut@gmail.com>

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
#ifndef WIN32
#include <sys/wait.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#else
#include <wincompat.h>
#endif

#include "nut_stdint.h"
#include "upsclient.h"
#include "upsmon.h"
#include "parseconf.h"
#include "timehead.h"

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

static	char	*shutdowncmd = NULL, *notifycmd = NULL;
static	char	*powerdownflag = NULL, *configfile = NULL;

static	unsigned int	minsupplies = 1, sleepval = 5;

	/* sum of all power values from config file */
static	unsigned int	totalpv = 0;

	/* default TTL of a device gone AWOL, 3 x polling interval = 15 sec */
static	int deadtime = 15;

	/* default polling interval = 5 sec */
static	unsigned int	pollfreq = 5, pollfreqalert = 5;

	/* If pollfail_log_throttle_max > 0, error messages for same
	 * state of an UPS (e.g. "Data stale" or "Driver not connected")
	 * will only be repeated every so many POLLFREQ loops.
	 * If pollfail_log_throttle_max == 0, such error messages will
	 * only be reported once when that situation starts, and ends.
	 * By default (or for negative values) it is logged every pollfreq
	 * loop cycle (which can abuse syslog and its storage), same as
	 * if "max = 1".
	 * To support this, each utype_t (UPS) structure tracks individual
	 * pollfail_log_throttle_count and pollfail_log_throttle_state
	 */
static	int	pollfail_log_throttle_max = -1;

	/* We support "administrative OFF" for power devices which can
	 * be managed to turn off their load while the UPS or ePDU remains
	 * accessible for monitoring and management. This toggle allows
	 * to delay propagation of such state into a known loss of a feed
	 * (possibly triggering FSD on MONITOR'ing clients that are in fact
	 * still alive - e.g. with multiple power sources), because when
	 * some devices begin battery calibration, they report "OFF" for
	 * a few seconds and only then they report "CAL" after switching
	 * all the power relays (causing false-positives for FSD trigger).
	 * A negative value means to disable decreasing the power-source
	 * counter in such cases, and a zero makes the effect immediate.
	 * NOTE: so far we support the device reporting an "OFF" state
	 * which usually means completely un-powering the load; TODO was
	 * logged for adding similar support for just some outlets/groups.
	 */
static	int	offdurationtime = 30;

	/* secondary hosts are given 15 sec by default to logout from upsd */
static	int	hostsync = 15;

	/* default replace battery warning interval (seconds) */
static	int	rbwarntime = 43200;

	/* default "all communications down" warning interval (seconds) */
static	int	nocommwarntime = 300;

	/* default interval between the shutdown warning and the shutdown */
static	unsigned int	finaldelay = 5;

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

static	int	shutdownexitdelay = 0;	/* by default doshutdown() exits immediately */
static	int	userfsd = 0, pipefd[2];
	/* Should we run "all in one" (e.g. as root) or split
	 * into two upsmon processes for some more security? */
#ifndef WIN32
static	int	use_pipe = 1;
#else
	/* Do not fork in WIN32 */
static	int	use_pipe = 0;
static HANDLE   mutex = INVALID_HANDLE_VALUE;
#endif

static	utype_t	*firstups = NULL;

static int 	opt_af = AF_UNSPEC;

#ifndef WIN32
	/* signal handling things */
static	struct sigaction sa;
static	sigset_t nut_upsmon_sigmask;
#endif

#ifdef HAVE_SYSTEMD
# define SERVICE_UNIT_NAME "nut-monitor.service"
#endif

/* Users can pass a -D[...] option to enable debugging.
 * For the service tracing purposes, also the upsmon.conf
 * can define a debug_min value in the global section,
 * to set the minimal debug level (CLI provided value less
 * than that would not have effect, can only have more).
 */
static int nut_debug_level_global = -1;
/* Debug level specified via command line - we revert to
 * it when reloading if there was no DEBUG_MIN in ups.conf
 */
static int nut_debug_level_args = 0;

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

static int try_restore_pollfreq(utype_t *ups) {
	/* Use relaxed pollfreq if we are not in a hardware
	 * power state that is prone to UPS disappearance */
	if (!flag_isset(ups->status, ST_ONBATT | ST_OFF | ST_BYPASS | ST_CAL)) {
		sleepval = pollfreq;
		return 1;
	}
	return 0;
}

static void wall(const char *text)
{
#ifndef WIN32
	FILE	*wf;

	wf = popen("wall", "w");

	if (!wf) {
		upslog_with_errno(LOG_NOTICE, "Can't invoke wall");
		return;
	}

	fprintf(wf, "%s\n", text);
	pclose(wf);
#else
	#define MESSAGE_CMD "message.exe"
	char * command;

	/* first +1 is for the space between message and text
	   second +1 is for trailing 0
	   +2 is for "" */
	command = malloc (strlen(MESSAGE_CMD) + 1 + 2 + strlen(text) + 1);
	if( command == NULL ) {
		upslog_with_errno(LOG_NOTICE, "Not enough memory for wall");
		return;
	}

	sprintf(command,"%s \"%s\"",MESSAGE_CMD,text);
	if ( system(command) != 0 ) {
		upslog_with_errno(LOG_NOTICE, "Can't invoke wall");
	}
	free(command);
#endif
}

#ifdef WIN32
typedef struct async_notify_s {
	char *notice;
	int flags;
	char *ntype;
	char *upsname;
	char *date;
} async_notify_t;

static unsigned __stdcall async_notify(LPVOID param)
{
	char	exec[LARGEBUF];
	char	notice[LARGEBUF];

	/* the following code is a copy of the content of the NOT WIN32 part of
	"notify" function below */

	async_notify_t *data = (async_notify_t *)param;

	if (flag_isset(data->flags, NOTIFY_WALL)) {
		snprintf(notice,LARGEBUF,"%s: %s", data->date, data->notice);
		wall(notice);
	}

	if (flag_isset(data->flags, NOTIFY_EXEC)) {
		if (notifycmd != NULL) {
			snprintf(exec, sizeof(exec), "%s \"%s\"", notifycmd, data->notice);

			if (data->upsname)
				setenv("UPSNAME", data->upsname, 1);
			else
				setenv("UPSNAME", "", 1);

			setenv("NOTIFYTYPE", data->ntype, 1);
			if (system(exec) == -1) {
				upslog_with_errno(LOG_ERR, "%s", __func__);
			}
		}
	}

	free(data->notice);
	free(data->ntype);
	free(data->upsname);
	free(data->date);
	free(data);
	return 1;
}
#endif

static void notify(const char *notice, int flags, const char *ntype,
			const char *upsname)
{
#ifndef WIN32
	char	exec[LARGEBUF];
	int	ret;
#endif

	upsdebugx(6, "%s: sending notification for [%s]: type %s with flags 0x%04x: %s",
		__func__, upsname, ntype, flags, notice);

	if (flag_isset(flags, NOTIFY_IGNORE)) {
		upsdebugx(6, "%s: NOTIFY_IGNORE", __func__);
		return;
	}

	if (flag_isset(flags, NOTIFY_SYSLOG)) {
		upsdebugx(6, "%s: NOTIFY_SYSLOG (as LOG_NOTICE)", __func__);
		upslogx(LOG_NOTICE, "%s", notice);
	}

#ifndef WIN32
	/* fork here so upsmon doesn't get wedged if the notifier is slow */
	ret = fork();

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "Can't fork to notify");
		return;
	}

	if (ret != 0) {	/* parent */
		upsdebugx(6, "%s (parent): forked a child to notify via subprocesses", __func__);
		return;
	}

	/* child continues and does all the work */
	upsdebugx(6, "%s (child): forked to notify via subprocesses", __func__);

	if (flag_isset(flags, NOTIFY_WALL)) {
		upsdebugx(6, "%s (child): NOTIFY_WALL", __func__);
		wall(notice);
	}

	if (flag_isset(flags, NOTIFY_EXEC)) {
		if (notifycmd != NULL) {
			upsdebugx(6, "%s (child): NOTIFY_EXEC: calling NOTIFYCMD as '%s \"%s\"'",
				__func__, notifycmd, notice);

			snprintf(exec, sizeof(exec), "%s \"%s\"", notifycmd, notice);

			if (upsname)
				setenv("UPSNAME", upsname, 1);
			else
				setenv("UPSNAME", "", 1);

			setenv("NOTIFYTYPE", ntype, 1);
			if (system(exec) == -1) {
				upslog_with_errno(LOG_ERR, "%s", __func__);
			}
		} else {
			upsdebugx(6, "%s (child): NOTIFY_EXEC: no NOTIFYCMD was configured", __func__);
		}
	}

	exit(EXIT_SUCCESS);
#else
	async_notify_t * data;
	time_t t;

	data = malloc(sizeof(async_notify_t));
	data->notice = strdup(notice);
	data->flags = flags;
	data->ntype = strdup(ntype);
	data->upsname = strdup(upsname);
	t = time(NULL);
	data->date = strdup(ctime(&t));

	_beginthreadex(
			NULL,	/* security FIXME */
			0,	/* stack size */
			async_notify,
			(void *)data,
			0,	/* Creation flags */
			NULL	/* thread id */
		      );
#endif
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
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
			snprintf(msg, sizeof(msg),
				notifylist[i].msg ? notifylist[i].msg : notifylist[i].stockmsg,
				ups ? ups->sys : "");
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
			notify(msg, notifylist[i].flags, notifylist[i].name,
				upsname);
			return;
		}
	}

	/* not found ?! */
}

/* check if we need "primary" mode (managerial permissions)
 * on the server for this ups, and apply for them then.
 * Returns 0 in case of error, 1 otherwise (including when
 * we do not need to try becoming a primary). This currently
 * propagates further as the return value of do_upsd_auth().
 */
/* TODO: Includes API change in NUT 2.8.0 to replace deprecated
 * keywords "MASTER" with "PRIMARY", and "SLAVE" with "SECONDARY",
 * (and backwards-compatible alias handling)
 */
static int apply_for_primary(utype_t *ups)
{
	char	buf[SMALLBUF];
	char	upscli_readraw_error;

	/* don't bother if we're not configured as a primary for this ups */
	if (!flag_isset(ups->status, ST_PRIMARY))
		return 1;

	/* this shouldn't happen (LOGIN checks it earlier) */
	if ((ups->upsname == NULL) || (strlen(ups->upsname) == 0)) {
		upslogx(LOG_ERR, "Set primary managerial mode on UPS [%s] failed: empty upsname",
			ups->sys);
		return 0;
	}

	/* Use PRIMARY first but if talking to older server, retry with MASTER */
	snprintf(buf, sizeof(buf), "PRIMARY %s\n", ups->upsname);

	if (upscli_sendline(&ups->conn, buf, strlen(buf)) < 0) {
		/* File descriptor not suitable, net_write() errors, etc.
		 * Not connected to issues with PRIMARY vs. MASTER keyword.
		 */
		upslogx(LOG_ALERT, "Can't set primary managerial mode on UPS [%s] - %s",
			ups->sys, upscli_strerror(&ups->conn));
		return 0;
	}

	if (upscli_readline(&ups->conn, buf, sizeof(buf)) == 0) {
		if (!strncmp(buf, "OK", 2))
			return 1;

		/* Try the older keyword */
		upsdebugx(3,
			"%s: Server did not grant PRIMARY mode on UPS [%s], "
			"retry with older MASTER keyword",
			__func__, ups->upsname);
		snprintf(buf, sizeof(buf), "MASTER %s\n", ups->upsname);

		if (upscli_sendline(&ups->conn, buf, strlen(buf)) < 0) {
			upslogx(LOG_ALERT, "Can't set primary managerial mode on UPS [%s] - %s",
				ups->sys, upscli_strerror(&ups->conn));
			return 0;
		}

		if (upscli_readline(&ups->conn, buf, sizeof(buf)) == 0) {
			if (!strncmp(buf, "OK", 2))
				return 1;

			upscli_readraw_error = 0;
		}
		else {
			upscli_readraw_error = 1;
		}
	}
	else {
		upscli_readraw_error = 1;
	}

	if (upscli_readraw_error == 0) {
		/* not ERR, but not caught by readline either? */
		upslogx(LOG_ALERT, "Primary managerial privileges unavailable on UPS [%s]",
			ups->sys);
		upslogx(LOG_ALERT, "Response: [%s]", buf);
	}
	else {	/* something caught by readraw's parsing call */
		upslogx(LOG_ALERT, "Primary managerial privileges unavailable on UPS [%s]",
			ups->sys);
		upslogx(LOG_ALERT, "Reason: %s", upscli_strerror(&ups->conn));
	}

	return 0;
}

/* authenticate to upsd, plus do LOGIN and apply for PRIMARY/MASTER privileges
 * if applicable to this ups device MONITORing configuration */
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

	/* now see if we also need to test primary managerial-mode permissions */
	return apply_for_primary(ups);
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

static void ups_is_off(utype_t *ups)
{
	time_t	now;

	time(&now);

	if (flag_isset(ups->status, ST_OFF)) {	/* no change */
		upsdebugx(4, "%s: %s (no change)", __func__, ups->sys);
		if (ups->offsince < 1) {
			/* Should not happen, but just in case */
			ups->offsince = now;
		} else {
			if (offdurationtime > 0 && (now - ups->offsince) > offdurationtime) {
				/* This should be a rare but urgent situation
				 * that warrants an extra notification? */
				upslogx(LOG_WARNING, "%s: %s is in state OFF for %d sec, "
					"assuming the line is not fed "
					"(if it is calibrating etc., check "
					"the upsmon 'OFFDURATION' option)",
					__func__, ups->sys, (int)(now - ups->offsince));
				ups->offstate = 1;
			}
		}
		return;
	}

	sleepval = pollfreqalert;	/* bump up polling frequency */

	ups->offsince = now;
	if (offdurationtime == 0) {
		/* This should be a rare but urgent situation
		 * that warrants an extra notification? */
		upslogx(LOG_WARNING, "%s: %s is in state OFF, assuming the line is not fed (if it is calibrating etc., check the upsmon 'OFFDURATION' option)", __func__, ups->sys);
		ups->offstate = 1;
	} else
	if (offdurationtime < 0) {
		upsdebugx(1, "%s: %s is in state OFF, but we are not assuming the line is not fed (due to upsmon 'OFFDURATION' option)", __func__, ups->sys);
	}

	upsdebugx(3, "%s: %s (first time)", __func__, ups->sys);

	/* must have changed from !OFF to OFF, so notify */
	do_notify(ups, NOTIFY_OFF);
	setflag(&ups->status, ST_OFF);
}

static void ups_is_notoff(utype_t *ups)
{
	/* Called when OFF is NOT among known states */
	ups->offsince = 0;
	ups->offstate = 0;
	if (flag_isset(ups->status, ST_OFF)) {	/* actual change */
		do_notify(ups, NOTIFY_NOTOFF);
		clearflag(&ups->status, ST_OFF);
		try_restore_pollfreq(ups);
	}
}

static void ups_is_bypass(utype_t *ups)
{
	if (flag_isset(ups->status, ST_BYPASS)) { 	/* no change */
		upsdebugx(4, "%s: %s (no change)", __func__, ups->sys);
		return;
	}

	sleepval = pollfreqalert;	/* bump up polling frequency */

	ups->bypassstate = 1;	/* if we lose comms, consider it AWOL */

	upsdebugx(3, "%s: %s (first time)", __func__, ups->sys);

	/* must have changed from !BYPASS to BYPASS, so notify */

	do_notify(ups, NOTIFY_BYPASS);
	setflag(&ups->status, ST_BYPASS);
}

static void ups_is_notbypass(utype_t *ups)
{
	/* Called when BYPASS is NOT among known states */
	ups->bypassstate = 0;
	if (flag_isset(ups->status, ST_BYPASS)) {	/* actual change */
		do_notify(ups, NOTIFY_NOTBYPASS);
		clearflag(&ups->status, ST_BYPASS);
		try_restore_pollfreq(ups);
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
	try_restore_pollfreq(ups);

	if (flag_isset(ups->status, ST_ONLINE)) { 	/* no change */
		upsdebugx(4, "%s: %s (no change)", __func__, ups->sys);
		return;
	}

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
	__attribute__((noreturn));

static void doshutdown(void)
{
	upsnotify(NOTIFY_STATE_STOPPING, "Executing automatic power-fail shutdown");

	/* this should probably go away at some point */
	upslogx(LOG_CRIT, "Executing automatic power-fail shutdown");
	wall("Executing automatic power-fail shutdown\n");

	do_notify(NULL, NOTIFY_SHUTDOWN);

	sleep(finaldelay);

	/* in the pipe model, we let the parent do this for us */
	if (use_pipe) {
		char	ch;
		ssize_t	wret;

		ch = 1;
		wret = write(pipefd[1], &ch, 1);

		if (wret < 1)
			upslogx(LOG_ERR, "Unable to call parent pipe for shutdown");
	} else {
		/* one process model = we do all the work here */
		int	sret;

#ifndef WIN32
		if (geteuid() != 0)
			upslogx(LOG_WARNING, "Not root, shutdown may fail");
#endif

		set_pdflag();

#ifdef WIN32
		SC_HANDLE SCManager;
		SC_HANDLE Service;
		SERVICE_STATUS Status;

		SCManager = OpenSCManager(
				NULL,	/* local computer */
				NULL,	/* ServiceActive database */
				SC_MANAGER_ALL_ACCESS); /* full access rights */

		if (NULL == SCManager) {
			upslogx(LOG_ERR, "OpenSCManager failed (%d)\n", (int)GetLastError());
		}
		else {
			Service = OpenService(SCManager,SVCNAME,SERVICE_STOP);
			if (Service == NULL) {
				upslogx(LOG_ERR,"OpenService  failed (%d)\n", (int)GetLastError());
			}
			else {
				ControlService(Service,SERVICE_CONTROL_STOP,&Status);
				/* Give time to the service to stop */
				Sleep(2000);
			}
		}
#endif

		sret = system(shutdowncmd);

		if (sret != 0)
			upslogx(LOG_ERR, "Unable to call shutdown command: %s",
				shutdowncmd);
	}

	if (shutdownexitdelay == 0) {
		upsdebugx(1,
			"Exiting upsmon immediately "
			"after initiating shutdown, by default");
	} else
	if (shutdownexitdelay < 0) {
		upslogx(LOG_WARNING,
			"Configured to not exit upsmon "
			"after initiating shutdown");
		/* Technically, here we sleep until SIGTERM or poweroff */
		do {
			sleep(1);
		} while (!exit_flag);
	} else {
		upslogx(LOG_WARNING,
			"Configured to only exit upsmon %d sec "
			"after initiating shutdown", shutdownexitdelay);
		do {
			sleep(1);
			shutdownexitdelay--;
		} while (!exit_flag && shutdownexitdelay);
	}
	exit(EXIT_SUCCESS);
}

/* set forced shutdown flag so other upsmons know what's going on here */
static void setfsd(utype_t *ups)
{
	char	buf[SMALLBUF];
	ssize_t	ret;

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
#ifndef WIN32
	alarm(NET_TIMEOUT);
#endif
}

static void clear_alarm(void)
{
#ifndef WIN32
# if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wstrict-prototypes"
# endif
	signal(SIGALRM, SIG_IGN);
# if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
#  pragma GCC diagnostic pop
# endif
	alarm(0);
#endif
}

static int get_var(utype_t *ups, const char *var, char *buf, size_t bufsize)
{
	int	ret;
	size_t	numq, numa;
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
			"(got %" PRIuSIZE " args, need at least %" PRIuSIZE ")",
			var, numa, numq);
		return -1;
	}

	snprintf(buf, bufsize, "%s", answer[numq]);
	return 0;
}

/* Called by upsmon which is the primary on some UPS(es) to wait
 * until all secondaries log out from it on the shared upsd server
 * or the HOSTSYNC timeout expires
 */
static void sync_secondaries(void)
{
	utype_t	*ups;
	char	temp[SMALLBUF];
	time_t	start, now;
	long	maxlogins, logins;

	time(&start);

	for (;;) {
		maxlogins = 0;

		for (ups = firstups; ups != NULL; ups = ups->next) {

			/* only check login count on devices we are the primary for */
			if (!flag_isset(ups->status, ST_PRIMARY))
				continue;

			set_alarm();

			if (get_var(ups, "numlogins", temp, sizeof(temp)) >= 0) {
				logins = strtol(temp, (char **)NULL, 10);

				if (logins > maxlogins)
					maxlogins = logins;
			}

			clear_alarm();
		}

		/* if no UPS has more than 1 login (that would be us),
		 * then secondaries are all gone */
		/* TO THINK: how about redundant setups with several primary-mode
		 * clients managing an UPS, or possibly differend UPSes, with the
		 * same upsd? */
		if (maxlogins <= 1)
			return;

		/* after HOSTSYNC seconds, assume secondaries are stuck - and bail */
		time(&now);

		if ((now - start) > hostsync) {
			upslogx(LOG_INFO, "Host sync timer expired, forcing shutdown");
			return;
		}

		usleep(250000);
	}
}

static void forceshutdown(void)
	__attribute__((noreturn));

static void forceshutdown(void)
{
	utype_t	*ups;
	int	isaprimary = 0;

	upsdebugx(1, "Shutting down any UPSes in PRIMARY mode...");

	/* set FSD on any "primary" UPS entries (forced shutdown in progress) */
	for (ups = firstups; ups != NULL; ups = ups->next)
		if (flag_isset(ups->status, ST_PRIMARY)) {
			isaprimary = 1;
			setfsd(ups);
		}

	/* if we're not a primary on anything, we should shut down now */
	if (!isaprimary)
		doshutdown();

	/* we must be the primary now */
	upsdebugx(1, "This system is a primary... waiting for secondaries to logout...");

	/* wait up to HOSTSYNC seconds for secondaries to logout */
	sync_secondaries();

	/* time expired or all the secondaries are gone, so shutdown */
	doshutdown();
}

static int is_ups_critical(utype_t *ups)
{
	time_t	now;

	/* FSD = the primary is forcing a shutdown, or a driver forwarded the flag
	 * from a smarter UPS depending on vendor protocol, ability and settings
	 * (e.g. is charging but battery too low to guarantee safety to the load)
	 */
	if (flag_isset(ups->status, ST_FSD))
		return 1;

	if (ups->commstate == 0) {
		if (flag_isset(ups->status, ST_CAL)) {
			upslogx(LOG_WARNING,
				"UPS [%s] was last known to be calibrating "
				"and currently is not communicating, assuming dead",
				ups->sys);
			return 1;
		}

		if (ups->bypassstate == 1
		|| flag_isset(ups->status, ST_BYPASS)) {
			upslogx(LOG_WARNING,
				"UPS [%s] was last known to be on BYPASS "
				"and currently is not communicating, assuming dead",
				ups->sys);
			return 1;
		}

		if (ups->offstate == 1
		|| (offdurationtime >= 0 && flag_isset(ups->status, ST_OFF))) {
			upslogx(LOG_WARNING,
				"UPS [%s] was last known to be (administratively) OFF "
				"and currently is not communicating, assuming dead",
				ups->sys);
			return 1;
		}

		if (ups->linestate == 0) {
			upslogx(LOG_WARNING,
				"UPS [%s] was last known to be not fully online "
				"and currently is not communicating, assuming dead",
				ups->sys);
			return 1;
		}
	}

	/* administratively OFF (long enough, see OFFDURATION) */
	if (flag_isset(ups->status, ST_OFF) && offdurationtime >= 0
	&& ups->offstate == 1) {
		upslogx(LOG_WARNING,
			"UPS [%s] is reported as (administratively) OFF",
			ups->sys);
		upsdebugx(1, "UPS [%s] is now critical being OFF for too long. In case of persisting unwanted shutdowns, consider disabling the upsmon 'OFFDURATION' option.", ups->sys);
		return 1;
	}

	/* not OB or not LB = not critical yet */
	if ((!flag_isset(ups->status, ST_ONBATT))
	|| (!flag_isset(ups->status, ST_LOWBATT))
	)
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

	/* if we're a primary, declare it critical so we set FSD on it */
	if (flag_isset(ups->status, ST_PRIMARY))
		return 1;

	/* must be a secondary now */

	/* FSD isn't set, so the primary hasn't seen it yet */

	time(&now);

	/* give the primary up to HOSTSYNC seconds before shutting down */
	if ((now - ups->lastnoncrit) > hostsync) {
		upslogx(LOG_WARNING, "Giving up on the primary for UPS [%s] "
			"after %d sec since last comms",
			ups->sys, (int)(now - ups->lastnoncrit));
		return 1;
	}

	/* there's still time left, maybe OB+LB will go away next time we look? */
	return 0;
}

/* recalculate the online power value and see if things are still OK */
static void recalc(void)
{
	utype_t	*ups;
	unsigned int	val_ol = 0;
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

		/* crit = (FSD) || (OB & LB) > HOSTSYNC seconds || (OFF || BYPASS) && nocomms */
		if (is_ups_critical(ups))
			upsdebugx(1, "Critical UPS: %s", ups->sys);
		else
			val_ol += ups->pv;

		ups = ups->next;
	}

	upsdebugx(3, "Current power value: %u", val_ol);
	upsdebugx(3, "Minimum power value: %u", minsupplies);

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

static void ups_is_cal(utype_t *ups)
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

static void ups_is_notcal(utype_t *ups)
{
	/* Called when CAL is NOT among known states */
	if (flag_isset(ups->status, ST_CAL)) {	/* actual change */
		do_notify(ups, NOTIFY_NOTCAL);
		clearflag(&ups->status, ST_CAL);
		try_restore_pollfreq(ups);
	}
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
	if (ups->linestate == 1 && flag_isset(ups->status, ST_ONLINE))
		upsdebugx(2, "Dropping connection to UPS [%s], last seen as fully online.", ups->sys);
	else
		upsdebugx(2, "Dropping connection to UPS [%s], last seen as not fully online (might be considered critical later).", ups->sys);

	if(ups->offstate == 1 || flag_isset(ups->status, ST_OFF))
		upsdebugx(2, "Disconnected UPS [%s] was last seen in status OFF, this UPS might be considered critical later.", ups->sys);

	if(ups->bypassstate == 1 || flag_isset(ups->status, ST_BYPASS))
		upsdebugx(2, "Disconnected UPS [%s] was last seen in status BYPASS, this UPS might be considered critical later.", ups->sys);

	if(flag_isset(ups->status, ST_CAL))
		upsdebugx(2, "Disconnected UPS [%s] was last seen in status CAL, this UPS might be considered critical later.", ups->sys);

	ups->commstate = 0;

	/* forget poll-failure logging throttling */
	ups->pollfail_log_throttle_count = -1;
	ups->pollfail_log_throttle_state = UPSCLI_ERR_NONE;

	clearflag(&ups->status, ST_LOGIN);
	clearflag(&ups->status, ST_CLICONNECTED);

	upscli_disconnect(&ups->conn);
}

/* change some UPS parameters during reloading */
static void redefine_ups(utype_t *ups, unsigned int pv, const char *un,
		const char *pw, const char *managerialOption)
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

	/* secondary|slave -> primary|master */
	if ( (   (!strcasecmp(managerialOption, "primary"))
	      || (!strcasecmp(managerialOption, "master"))  )
	     && (!flag_isset(ups->status, ST_PRIMARY)) ) {
		upslogx(LOG_INFO, "UPS [%s]: redefined as a primary", ups->sys);
		setflag(&ups->status, ST_PRIMARY);

		/* reset connection to ensure primary mode gets checked */
		drop_connection(ups);
		return;
	}

	/* primary|master -> secondary|slave */
	if ( (   (!strcasecmp(managerialOption, "secondary"))
	      || (!strcasecmp(managerialOption, "slave"))  )
	     && (flag_isset(ups->status, ST_PRIMARY)) ) {
		upslogx(LOG_INFO, "UPS [%s]: redefined as a secondary", ups->sys);
		clearflag(&ups->status, ST_PRIMARY);
		return;
	}
}

static void addups(int reloading, const char *sys, const char *pvs,
		const char *un, const char *pw, const char *managerialOption)
{
	unsigned int	pv;
	utype_t	*tmp, *last;
	long	lpv;

	/* the username is now required - no more host-based auth */

	if ((!sys) || (!pvs) || (!pw) || (!managerialOption) || (!un)) {
		upslogx(LOG_WARNING, "Ignoring invalid MONITOR line in %s!", configfile);
		upslogx(LOG_WARNING, "MONITOR configuration directives require five arguments.");
		return;
	}

	lpv = strtol(pvs, (char **) NULL, 10);

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wtautological-compare"
# pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
	/* Different platforms, different sizes, none fits all... */
	if (lpv < 0 || (sizeof(long) > sizeof(unsigned int) && lpv > (long)UINT_MAX)) {
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
		upslogx(LOG_WARNING, "UPS [%s]: ignoring invalid power value [%s]",
			sys, pvs);
		return;
	}
	pv = (unsigned int)lpv;

	last = tmp = firstups;

	while (tmp) {
		last = tmp;

		/* check for duplicates */
		if (!strcmp(tmp->sys, sys)) {
			if (reloading)
				redefine_ups(tmp, pv, un, pw, managerialOption);
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

	/* forget poll-failure logging throttling */
	tmp->pollfail_log_throttle_count = -1;
	tmp->pollfail_log_throttle_state = UPSCLI_ERR_NONE;

	tmp->lastpoll = 0;
	tmp->lastnoncrit = 0;
	tmp->lastrbwarn = 0;
	tmp->lastncwarn = 0;

	if (   (!strcasecmp(managerialOption, "primary"))
	    || (!strcasecmp(managerialOption, "master"))  ) {
		setflag(&tmp->status, ST_PRIMARY);
	}

	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		firstups = tmp;

	if (tmp->pv)
		upslogx(LOG_INFO, "UPS: %s (%s) (power value %d)", tmp->sys,
			flag_isset(tmp->status, ST_PRIMARY) ? "primary" : "secondary",
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
static int parse_conf_arg(size_t numargs, char **arg)
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

	/* SHUTDOWNEXIT <boolean|number> */
	if (!strcmp(arg[0], "SHUTDOWNEXIT")) {
		if (!strcasecmp(arg[1], "on")
		||  !strcasecmp(arg[1], "yes")
		||  !strcasecmp(arg[1], "true")) {
			shutdownexitdelay = 0;
		} else
		if (!strcasecmp(arg[1], "off")
		||  !strcasecmp(arg[1], "no")
		||  !strcasecmp(arg[1], "false")) {
			shutdownexitdelay = -1;
		} else {
			if (!str_to_int(arg[1], &shutdownexitdelay, 10)) {
				upslogx(LOG_WARNING,
					"SHUTDOWNEXIT value not recognized, "
					"defaulting to 'yes'");
				shutdownexitdelay = 0;
			}
		}
		return 1;
	}

	/* POWERDOWNFLAG <fn> */
	if (!strcmp(arg[0], "POWERDOWNFLAG")) {
		checkmode(arg[0], powerdownflag, arg[1], reload_flag);

		free(powerdownflag);
#ifndef WIN32
		powerdownflag = xstrdup(arg[1]);
#else
		powerdownflag = filter_path(arg[1]);
#endif

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
		int ipollfreq = atoi(arg[1]);
		if (ipollfreq < 0) {
			upsdebugx(0, "Ignoring invalid POLLFREQ value: %d", ipollfreq);
		} else {
			pollfreq = (unsigned int)ipollfreq;
		}
		return 1;
	}

	/* POLLFREQALERT <num> */
	if (!strcmp(arg[0], "POLLFREQALERT")) {
		int ipollfreqalert = atoi(arg[1]);
		if (ipollfreqalert < 0) {
			upsdebugx(0, "Ignoring invalid POLLFREQALERT value: %d", ipollfreqalert);
		} else {
			pollfreqalert = (unsigned int)ipollfreqalert;
		}
		return 1;
	}

	/* POLLFAIL_LOG_THROTTLE_MAX <num> */
	if (!strcmp(arg[0], "POLLFAIL_LOG_THROTTLE_MAX")) {
		int ipollfail_log_throttle_max = atoi(arg[1]);
		if (ipollfail_log_throttle_max < 0 || ipollfail_log_throttle_max == INT_MAX) {
			upsdebugx(0, "Ignoring invalid POLLFAIL_LOG_THROTTLE_MAX value: %d", ipollfail_log_throttle_max);
		} else {
			pollfail_log_throttle_max = ipollfail_log_throttle_max;
		}
		return 1;
	}

	/* OFFDURATION <num> */
	if (!strcmp(arg[0], "OFFDURATION")) {
		offdurationtime = atoi(arg[1]);
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
		int iminsupplies = atoi(arg[1]);
		if (iminsupplies < 0) {
			upsdebugx(0, "Ignoring invalid MINSUPPLIES value: %d", iminsupplies);
		} else {
			minsupplies = (unsigned int)iminsupplies;
		}
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
		int ifinaldelay = atoi(arg[1]);
		if (ifinaldelay < 0) {
			upsdebugx(0, "Ignoring invalid FINALDELAY value: %d", ifinaldelay);
		} else {
			finaldelay = (unsigned int)ifinaldelay;
		}
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

	/* DEBUG_MIN (NUM) */
	/* debug_min (NUM) also acceptable, to be on par with ups.conf */
	if (!strcasecmp(arg[0], "DEBUG_MIN")) {
		int lvl = -1; /* typeof common/common.c: int nut_debug_level */
		if ( str_to_int (arg[1], &lvl, 10) && lvl >= 0 ) {
			nut_debug_level_global = lvl;
		} else {
			upslogx(LOG_INFO, "WARNING : Invalid DEBUG_MIN value found in upsmon.conf global settings");
		}
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

		/* <sys> <pwrval> <user> <pw> ("primary"|"master" | "secondary"|"slave") */
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
	int	numerrors = 0;

	pconf_init(&ctx, upsmon_err);

	if (!pconf_file_begin(&ctx, configfile)) {
		pconf_finish(&ctx);

		if (reload_flag == 1) {
			upslog_with_errno(LOG_ERR, "Reload failed: %s", ctx.errmsg);
			return;
		}

		fatalx(EXIT_FAILURE, "%s", ctx.errmsg);
	}

	if (reload_flag == 1) {
		/* if upsmon.conf added or changed
		 * (or commented away) the debug_min
		 * setting, detect that */
		nut_debug_level_global = -1;

		if (pollfail_log_throttle_max >= 0) {
			utype_t	*ups;

			upslogx(LOG_INFO,
				"Forgetting POLLFAIL_LOG_THROTTLE_MAX=%d and "
				"resetting UPS error-state counters before "
				"a configuration reload",
				pollfail_log_throttle_max);
			pollfail_log_throttle_max = -1;

			/* forget poll-failure logging throttling, so that we
			 * rediscover the error-states and the counts involved
			 */
			ups = firstups;
			while (ups) {
				ups->pollfail_log_throttle_count = -1;
				ups->pollfail_log_throttle_state = UPSCLI_ERR_NONE;
				ups = ups->next;
			}
		}
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				configfile, ctx.linenum, ctx.errmsg);
			numerrors++;
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

			numerrors++;
			upslogx(LOG_WARNING, "%s", errmsg);
		}
	}

	if (reload_flag == 1) {
		if (nut_debug_level_global > -1) {
			upslogx(LOG_INFO,
				"Applying DEBUG_MIN %d from upsmon.conf",
				nut_debug_level_global);
			nut_debug_level = nut_debug_level_global;
		} else {
			/* DEBUG_MIN is absent or commented-away in ups.conf */
			upslogx(LOG_INFO,
				"Applying debug level %d from "
				"original command line arguments",
				nut_debug_level_args);
			nut_debug_level = nut_debug_level_args;
		}

		if (pollfail_log_throttle_max >= 0) {
			upslogx(LOG_INFO,
				"Applying POLLFAIL_LOG_THROTTLE_MAX %d from upsmon.conf",
				pollfail_log_throttle_max);
		}
	}

	/* FIXME: Per legacy behavior, we silently went on.
	 * Maybe should abort on unusable configs?
	 */
	if (numerrors) {
		upslogx(LOG_ERR, "Encountered %d config errors, those entries were ignored", numerrors);
	}

	pconf_finish(&ctx);
}

#ifndef WIN32
/* SIGPIPE handler */
static void sigpipe(int sig)
{
	upsdebugx(1, "SIGPIPE: dazed and confused, but continuing after signal %i...", sig);
}
#endif

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

#ifdef WIN32
	if(mutex != INVALID_HANDLE_VALUE) {
		ReleaseMutex(mutex);
		CloseHandle(mutex);
	}
#endif
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

#ifndef WIN32
/* handler for alarm when getupsvarfd times out */
static void read_timeout(int sig)
{
	NUT_UNUSED_VARIABLE(sig);

	/* don't do anything here, just return */
}
#endif

/* install handlers for a few signals */
static void setup_signals(void)
{
#ifndef WIN32
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
#else
	pipe_create(UPSMON_PIPE_NAME);
#endif
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

	clearflag(&ups->status, ST_CLICONNECTED);

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
	setflag(&ups->status, ST_CLICONNECTED);

	/* prevent connection leaking to NOTIFYCMD */
	set_close_on_exec(upscli_fd(&ups->conn));

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
	if (status == NULL || status[0] == '\0') {
		ups_is_gone(ups);
		return;
	}

	ups_is_alive(ups);

	/* clear these out early if they disappear */
	if (!strstr(status, "LB"))
		clearflag(&ups->status, ST_LOWBATT);
	if (!strstr(status, "FSD"))
		clearflag(&ups->status, ST_FSD);

	/* similar to above - clear these flags and send notifications */
	if (!strstr(status, "CAL"))
		ups_is_notcal(ups);
	if (!strstr(status, "OFF"))
		ups_is_notoff(ups);
	if (!strstr(status, "BYPASS"))
		ups_is_notbypass(ups);

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
			ups_is_cal(ups);
		if (!strcasecmp(statword, "OFF"))
			ups_is_off(ups);
		if (!strcasecmp(statword, "BYPASS"))
			ups_is_bypass(ups);
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
	int	pollfail_log = 0;	/* if we throttle, only upsdebugx() but not upslogx() the failures */
	int	upserror;

	/* try a reconnect here */
	if (!flag_isset(ups->status, ST_CLICONNECTED)) {
		if (try_connect(ups) != 1) {
			return;
		}
	}

	if (upscli_ssl(&ups->conn) == 1)
		upsdebugx(2, "%s: %s [SSL]", __func__, ups->sys);
	else
		upsdebugx(2, "%s: %s", __func__, ups->sys);

	set_alarm();

	if (get_var(ups, "status", status, sizeof(status)) == 0) {
		clear_alarm();

		/* reset pollfail log throttling */
#if 0
		/* Note: last error is never cleared, so we reset it below */
		upserror = upscli_upserror(&ups->conn);
		upsdebugx(3, "%s: Poll UPS [%s] after getvar(status) okay: upserror=%d: %s",
			__func__, ups->sys, upserror, upscli_strerror(&ups->conn));
#endif
		upserror = UPSCLI_ERR_NONE;
		if (pollfail_log_throttle_max >= 0
		&&  ups->pollfail_log_throttle_state != upserror
		) {
			/* Notify throttled log that we are okay now */
			upslogx(LOG_ERR, "Poll UPS [%s] recovered from "
				"failure state code %d - now %d",
				ups->sys, ups->pollfail_log_throttle_state,
				upserror);
		}
		ups->pollfail_log_throttle_state = upserror;
		ups->pollfail_log_throttle_count = -1;

		parse_status(ups, status);
		return;
	}

	/* fallthrough: no communications */
	clear_alarm();

	/* try to make some of these a little friendlier */
	upserror = upscli_upserror(&ups->conn);
	upsdebugx(3, "%s: Poll UPS [%s] after getvar(status) failed: upserror=%d",
		__func__, ups->sys, upserror);
	if (pollfail_log_throttle_max < 0) {
		/* Log properly on each loop */
		pollfail_log = 1;
	} else {
		if (ups->pollfail_log_throttle_state == upserror) {
			/* known issue, no syslog spam now... maybe */
			if (pollfail_log_throttle_max == 0) {
				/* Only log once for start or end of the same
				 * failure state */
				pollfail_log = 0;
			} else {
				/* here (pollfail_log_throttle_max > 0) :
				 * only log once for start, every MAX iterations,
				 * and end of the same failure state
				 */
				if (ups->pollfail_log_throttle_count++ >= (pollfail_log_throttle_max - 1)) {
					/* ping... */
					pollfail_log = 1;
					ups->pollfail_log_throttle_count = 0;
				} else {
					pollfail_log = 0;
				}
			}
		} else {
			/* new error => reset pollfail log throttling and log it
			 * now (numeric states here, string for new state below) */
			if (pollfail_log_throttle_max == 0) {
				upslogx(LOG_ERR, "Poll UPS [%s] failure state code "
					"changed from %d to %d; "
					"report below will not be repeated to syslog:",
					ups->sys, ups->pollfail_log_throttle_state,
					upserror);
			} else {
				upslogx(LOG_ERR, "Poll UPS [%s] failure state code "
					"changed from %d to %d; "
					"report below will only be repeated to syslog "
					"every %d polling loop cycles (%d sec):",
					ups->sys, ups->pollfail_log_throttle_state,
					upserror, pollfail_log_throttle_max,
					pollfail_log_throttle_max * pollfreq);
			}

			ups->pollfail_log_throttle_state = upserror;
			ups->pollfail_log_throttle_count = 0;
			pollfail_log = 1;
		}
	}

	switch (upserror) {
		case UPSCLI_ERR_UNKNOWNUPS:
			if (pollfail_log) {
				upslogx(LOG_ERR, "Poll UPS [%s] failed - [%s] "
					"does not exist on server %s",
					ups->sys, ups->upsname,	ups->hostname);
			} else {
				upsdebugx(1, "Poll UPS [%s] failed - [%s] "
					"does not exist on server %s",
					ups->sys, ups->upsname,	ups->hostname);
			}
			break;

		default:
			if (pollfail_log) {
				upslogx(LOG_ERR, "Poll UPS [%s] failed - %s",
					ups->sys, upscli_strerror(&ups->conn));
			} else {
				upsdebugx(1, "Poll UPS [%s] failed - [%s]",
					ups->sys, upscli_strerror(&ups->conn));
			}
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

static void help(const char *arg_progname)
	__attribute__((noreturn));

static void help(const char *arg_progname)
{
	printf("Monitors UPS servers and may initiate shutdown if necessary.\n\n");

	printf("usage: %s [OPTIONS]\n\n", arg_progname);
	printf("  -c <cmd>	send command to running process\n");
	printf("		commands:\n");
	printf("		 - fsd: shutdown all primary-mode UPSes (use with caution)\n");
	printf("		 - reload: reread configuration\n");
	printf("		 - stop: stop monitoring and exit\n");
#ifndef WIN32
	printf("  -P <pid>	send the signal above to specified PID (bypassing PID file)\n");
#endif
	printf("  -D		raise debugging level (and stay foreground by default)\n");
	printf("  -F		stay foregrounded even if no debugging is enabled\n");
	printf("  -B		stay backgrounded even if debugging is bumped\n");
	printf("  -V		display the version of this software\n");
	printf("  -h		display this help\n");
	printf("  -K		checks POWERDOWNFLAG, sets exit code to 0 if set\n");
	printf("  -p		always run privileged (disable privileged parent)\n");
	printf("  -u <user>	run child as user <user> (ignored when using -p)\n");
	printf("  -4		IPv4 only\n");
	printf("  -6		IPv6 only\n");

	nut_report_config_flags();

	exit(EXIT_SUCCESS);
}

#ifndef WIN32
static void runparent(int fd)
	__attribute__((noreturn));

static void runparent(int fd)
{
	ssize_t	ret;
	int	sret;
	char	ch;

	/* handling signals is the child's job */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
	signal(SIGHUP, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRICT_PROTOTYPES)
# pragma GCC diagnostic pop
#endif

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

	sret = system(shutdowncmd);

	if (sret != 0)
		upslogx(LOG_ERR, "parent: Unable to call shutdown command: %s",
			shutdowncmd);

	close(fd);
	exit(EXIT_SUCCESS);
}
#endif

/* fire up the split parent/child scheme */
static void start_pipe(void)
{
#ifndef WIN32
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

#ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);	/* NOTREACHED */
#endif
	}

	close(pipefd[0]);

	/* prevent pipe leaking to NOTIFYCMD */
	set_close_on_exec(pipefd[1]);
#endif	/* WIN32 */
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
	int	i, cmdret = -1, checking_flag = 0, foreground = -1;

#ifndef WIN32
	pid_t	oldpid = -1;
	int	cmd = 0;
#else
	const char * cmd = NULL;
	DWORD ret;

	HANDLE		handles[MAXIMUM_WAIT_OBJECTS];
	int		maxhandle = 0;
	pipe_conn_t	*conn;

	/* remove trailing .exe */
	char * drv_name;
	drv_name = (char *)xbasename(argv[0]);
	char * name = strrchr(drv_name,'.');
	if( name != NULL ) {
		if(strcasecmp(name, ".exe") == 0 ) {
			prog = strdup(drv_name);
			char * t = strrchr(prog,'.');
			*t = 0;
		}
	}
	else {
		prog = drv_name;
	}
#endif

	printf("Network UPS Tools %s %s\n", prog, UPS_VERSION);

	/* if no configuration file is specified on the command line, use default */
	configfile = xmalloc(SMALLBUF);
	snprintf(configfile, SMALLBUF, "%s/upsmon.conf", confpath());
	configfile = xrealloc(configfile, strlen(configfile) + 1);

	run_as_user = xstrdup(RUN_AS_USER);

	while ((i = getopt(argc, argv, "+DFBhic:P:f:pu:VK46")) != -1) {
		switch (i) {
			case 'c':
				if (!strncmp(optarg, "fsd", strlen(optarg))) {
					cmd = SIGCMD_FSD;
				} else
				if (!strncmp(optarg, "stop", strlen(optarg))) {
					cmd = SIGCMD_STOP;
				} else
				if (!strncmp(optarg, "reload", strlen(optarg))) {
					cmd = SIGCMD_RELOAD;
				}

				/* bad command name given */
				if (cmd == 0)
					help(argv[0]);
				break;
#ifndef WIN32
			case 'P':
				if ((oldpid = parsepid(optarg)) < 0)
					help(argv[0]);
				break;
#endif
			case 'D':
				nut_debug_level++;
				nut_debug_level_args++;
				break;
			case 'F':
				foreground = 1;
				break;
			case 'B':
				foreground = 0;
				break;
			case 'f':
				free(configfile);
				configfile = xstrdup(optarg);
				break;
			case 'h':
				help(argv[0]);
#ifndef HAVE___ATTRIBUTE__NORETURN
				break;
#endif
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
				/* just show the optional CONFIG_FLAGS banner */
				nut_report_config_flags();
				exit(EXIT_SUCCESS);
			case '4':
				opt_af = AF_INET;
				break;
			case '6':
				opt_af = AF_INET6;
				break;
			default:
				help(argv[0]);
#ifndef HAVE___ATTRIBUTE__NORETURN
				break;
#endif
		}
	}

	if (foreground < 0) {
		if (nut_debug_level > 0) {
			foreground = 1;
		} else {
			foreground = 0;
		}
	}

	{ /* scoping */
		char *s = getenv("NUT_DEBUG_LEVEL");
		int l;
		if (s && str_to_int(s, &l, 10)) {
			if (l > 0 && nut_debug_level_args < 1) {
				upslogx(LOG_INFO, "Defaulting debug verbosity to NUT_DEBUG_LEVEL=%d "
					"since none was requested by command-line options", l);
				nut_debug_level = l;
				nut_debug_level_args = l;
			}	/* else follow -D settings */
		}	/* else nothing to bother about */
	}

	/* Note: "cmd" may be non-trivial to command that instance by
	 * explicit PID number or lookup in PID file (error if absent).
	 * Otherwise, we are being asked to start and "cmd" is 0/NULL -
	 * for probing whether a competing older instance of this program
	 * is running (error if it is).
	 */
#ifndef WIN32
	/* If cmd == 0 we are starting and check if a previous instance
	 * is running by sending signal '0' (i.e. 'kill <pid> 0' equivalent)
	 */
	if (oldpid < 0) {
		cmdret = sendsignal(prog, cmd);
	} else {
		cmdret = sendsignalpid(oldpid, cmd);
	}

#else	/* WIN32 */
	if (cmd) {
		/* Command the running daemon, it should be there */
		cmdret = sendsignal(UPSMON_PIPE_NAME, cmd);
	} else {
		/* Starting new daemon, check for competition */
		mutex = CreateMutex(NULL, TRUE, UPSMON_PIPE_NAME);
		if (mutex == NULL) {
			if (GetLastError() != ERROR_ACCESS_DENIED) {
				fatalx(EXIT_FAILURE,
					"Can not create mutex %s : %d.\n",
					UPSMON_PIPE_NAME, (int)GetLastError());
			}
		}

		cmdret = -1; /* unknown, maybe ok */
		if (GetLastError() == ERROR_ALREADY_EXISTS
		||  GetLastError() == ERROR_ACCESS_DENIED
		) {
			cmdret = 0; /* known conflict */
		}
	}
#endif	/* WIN32 */

	switch (cmdret) {
	case 0:
		if (cmd) {
			upsdebugx(1, "Signaled old daemon OK");
		} else {
			if (checking_flag) {
				printf("Note: A previous upsmon instance is already running!\n");
				printf("Usually it should not be running during OS shutdown,\n");
				printf("which is when checking POWERDOWNFLAG makes most sense.\n");
			} else {
				printf("Fatal error: A previous upsmon instance is already running!\n");
				printf("Either stop the previous instance first, or use the 'reload' command.\n");
				exit(EXIT_FAILURE);
			}
		}
		break;

	case -3:
	case -2:
		/* if starting new daemon, no competition running -
		 *    maybe OK (or failed to detect it => problem)
		 * if signaling old daemon - certainly have a problem
		 */
		upslogx(LOG_WARNING, "Could not %s PID file "
			"to see if previous upsmon instance is "
			"already running!",
			(cmdret == -3 ? "find" : "parse"));
		break;

	case -1:
	case 1:	/* WIN32 */
	default:
		/* if cmd was nontrivial - speak up below, else be quiet */
		upsdebugx(1, "Just failed to send signal, no daemon was running");
		break;
	}

	if (cmd) {
		/* We were signalling a daemon, successfully or not - exit now... */
		if (cmdret != 0) {
			/* sendsignal*() above might have logged more details
			 * for troubleshooting, e.g. about lack of PID file
			 */
			upslogx(LOG_NOTICE, "Failed to signal the currently running daemon (if any)");
#ifndef WIN32
# ifdef HAVE_SYSTEMD
			switch (cmd) {
			case SIGCMD_RELOAD:
				upslogx(LOG_NOTICE, "Try 'systemctl reload %s'%s",
					SERVICE_UNIT_NAME,
					(oldpid < 0 ? " or add '-P $PID' argument" : ""));
				break;
			case SIGCMD_STOP:
				upslogx(LOG_NOTICE, "Try 'systemctl stop %s'%s",
					SERVICE_UNIT_NAME,
					(oldpid < 0 ? " or add '-P $PID' argument" : ""));
				break;
			case SIGCMD_FSD:
				if (oldpid < 0) {
					upslogx(LOG_NOTICE, "Try to add '-P $PID' argument");
				}
				break;
			default:
				upslogx(LOG_NOTICE, "Try 'systemctl <command> %s'%s",
					SERVICE_UNIT_NAME,
					(oldpid < 0 ? " or add '-P $PID' argument" : ""));
				break;
			}
# else
			if (oldpid < 0) {
				upslogx(LOG_NOTICE, "Try to add '-P $PID' argument");
			}
# endif
#endif	/* not WIN32 */
		}

		exit((cmdret == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	argc -= optind;
	argv += optind;

	open_syslog(prog);

	loadconfig();

	/* CLI debug level can not be smaller than debug_min specified
	 * in upsmon.conf. Note that non-zero debug_min does not impact
	 * foreground running mode.
	 */
	if (nut_debug_level_global > nut_debug_level)
		nut_debug_level = nut_debug_level_global;
	upsdebugx(1, "debug level is '%d'", nut_debug_level);

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

	if (!foreground) {
		background();
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
#ifndef WIN32
		/* Note: upsmon does not fork in WIN32 */
		upslogx(LOG_INFO, "Warning: running as one big root process by request (upsmon -p)");
#endif

		writepid(prog);
	}

	if (upscli_init(certverify, certpath, certname, certpasswd) < 0) {
		upsnotify(NOTIFY_STATE_STOPPING, "Failed upscli_init()");
		exit(EXIT_FAILURE);
	}

	/* prep our signal handlers */
	setup_signals();

	/* reopen the log for the child process */
	closelog();
	open_syslog(prog);

	upsnotify(NOTIFY_STATE_READY_WITH_PID, NULL);

	while (exit_flag == 0) {
		utype_t	*ups;

		upsnotify(NOTIFY_STATE_WATCHDOG, NULL);

		/* check flags from signal handlers */
		if (userfsd)
			forceshutdown();

		if (reload_flag) {
			upsnotify(NOTIFY_STATE_RELOADING, NULL);
			reload_conf();
			upsnotify(NOTIFY_STATE_READY, NULL);
		}

		for (ups = firstups; ups != NULL; ups = ups->next)
			pollups(ups);

		recalc();

		/* make sure the parent hasn't died */
		if (use_pipe)
			check_parent();

#ifndef WIN32
		/* reap children that have exited */
		waitpid(-1, NULL, WNOHANG);

		sleep(sleepval);
#else
		maxhandle = 0;
		memset(&handles,0,sizeof(handles));

		/* Wait on the read IO of each connections */
		for (conn = pipe_connhead; conn; conn = conn->next) {
			handles[maxhandle] = conn->overlapped.hEvent;
			maxhandle++;
		}
		/* Add the new pipe connected event */
		handles[maxhandle] = pipe_connection_overlapped.hEvent;
		maxhandle++;

		ret = WaitForMultipleObjects(maxhandle,handles,FALSE,sleepval*1000);

		if (ret == WAIT_FAILED) {
			upslogx(LOG_ERR, "Wait failed");
			exit(EXIT_FAILURE);
		}

		if (ret == WAIT_TIMEOUT) {
			continue;
		}

		/* Retrieve the signaled connection */
		for(conn = pipe_connhead; conn != NULL; conn = conn->next) {
			if( conn->overlapped.hEvent == handles[ret-WAIT_OBJECT_0]) {
				break;
			}
		}
		/* a new pipe connection has been signaled */
		if (handles[ret] == pipe_connection_overlapped.hEvent) {
			pipe_connect();
		}
		/* one of the read event handle has been signaled */
		else {
			if( conn != NULL) {
				if ( pipe_ready(conn) ) {
					if (!strncmp(conn->buf, SIGCMD_FSD, sizeof(SIGCMD_FSD))) {
						user_fsd(1);
					}

					else if (!strncmp(conn->buf, SIGCMD_RELOAD, sizeof(SIGCMD_RELOAD))) {
						set_reload_flag(1);
					}

					else if (!strncmp(conn->buf, SIGCMD_STOP, sizeof(SIGCMD_STOP))) {
						set_exit_flag(1);
					}

					else {
						upslogx(LOG_ERR,"Unknown signal");
					}

					pipe_disconnect(conn);
				}
			}
		}
#endif
	}

	upslogx(LOG_INFO, "Signal %d: exiting", exit_flag);
	upsnotify(NOTIFY_STATE_STOPPING, "Signal %d: exiting", exit_flag);
	upsmon_cleanup();

	exit(EXIT_SUCCESS);
}
