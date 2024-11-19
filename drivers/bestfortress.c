/*
   bestfortress.c - model specific routines for (very) old Best Power Fortress

   Copyright (C) 2002  Russell Kroll <rkroll@exploits.org> (skeleton)
             (C) 2002  Holger Dietze <holger.dietze@advis.de>
             (C) 2009  Stuart D. Gathman <stuart@bmsi.com>

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "main.h"
#include "serial.h"
#include "nut_stdint.h"

#define UPSDELAY 50000	/* 50 ms delay required for reliable operation */
#define SER_WAIT_SEC	2	/* allow 2.0 sec for ser_get calls */
#define SER_WAIT_USEC	0
#define ENDCHAR		'\r'
#define IGNCHARS	" \n"

#if defined(__sgi) && ! defined(__GNUC__)
#define        inline  __inline
#endif

#define DRIVER_NAME     "Best Fortress UPS driver"
#define DRIVER_VERSION  "0.10"

/* driver description structure */
upsdrv_info_t   upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Holger Dietze <holger.dietze@advis.de>\n"
	"Stuart D. Gathman <stuart@bmsi.com>\n",
	DRV_EXPERIMENTAL,
	{ NULL }
};

/*
 * Choose 20s for off delay, reading the tea leaves from other drivers.
 */
static const char *shutdown_delay = "20";

/*
 * Logging plan:
 * CRIT: executing poweroff
 * ERR: indicates a bug in nut
 * WARNING: indicates a serious misconfiguration that will cause nut
 *           to fail to function
 * NOTICE: rare situations without a planned reporting path (none so
 *         far)
 * INFO: One message each for startup, first establishment of comms
 * DEBUG: Loss/restore of comms, and perhaps things that are normally
 *        reported via other daemons (on battery, low battery)
 * D1: Things that happen rarely (startup/shutdown, param set, instant
 *     commands)
 * D2: ?
 * D3: trace of communications
 * D4: more io than necessary to see working communications
 * D5: log even succesful acks
 */

static int instcmd (const char *cmdname, const char *extra);
static int upsdrv_setvar (const char *varname, const char *val);

/* Rated maximum VA output as configured by the user. */
static int maxload = 0;

/*
 * NB: Not called for shutdown.
 */
void upsdrv_initinfo(void)
{
	dstate_setinfo("ups.mfr", "Best Power");
	dstate_setinfo("ups.model", "Fortress");
	dstate_setinfo("battery.voltage.nominal", "24");

	/*dstate_setinfo ("alarm.overload", "0");*/ /* Flag */
	/*dstate_setinfo ("alarm.temp", "0");*/ /* Flag */
	if (maxload)
		dstate_setinfo("ups.load", "0");
	dstate_setinfo("output.voltamps", "0");

	/* tunable via front panel: (european voltage level)
	   parameter        factory default  range
	   INFO_LOWXFER     196 V   p7=nnn   160-210
	   INFO_HIGHXFER    254 V   p8=nnn   215-274
	   INFO_LOBATTIME   2 min   p2=n     1-5

	   comm mode    p6=0 dumb DONT USE (will lose access to parameter setting!)
	        p6=1 B1200
	        p6=2 B2400
	        P6=3 B4800
	        p6=4 B9600
	   maybe cycle through speeds to autodetect?

	   echo off     e0
	   echo on      e1
	*/
	dstate_setinfo("input.transfer.low", "%s", "");
	dstate_setflags("input.transfer.low", ST_FLAG_STRING | ST_FLAG_RW);
	dstate_setaux("input.transfer.low", 3);

	dstate_setinfo("input.transfer.high", "%s", "");
	dstate_setflags("input.transfer.high", ST_FLAG_STRING | ST_FLAG_RW);
	dstate_setaux("input.transfer.high", 3);

	dstate_setinfo("battery.runtime.low", "%s", "");
	dstate_setflags("battery.runtime.low", ST_FLAG_STRING | ST_FLAG_RW);
	dstate_setaux("battery.runtime.low", 3);

	upsh.instcmd = instcmd;
	upsh.setvar = upsdrv_setvar;

	dstate_addcmd("shutdown.return");
	dstate_addcmd("load.off");
}

/* convert hex digit to int */
static inline int fromhex (char c)
{
	return (c >= '0' && c <= '9') ? c - '0'
		: (c >= 'A' && c <= 'F') ? c - 'A' + 10
		: (c >= 'a' && c <= 'f') ? c - 'a' + 10
		: 0;
}

/* do checksumming on UPS response */
static int checksum (char * s)
{
	int i;
	int sum;
	for (i = 40, sum = 0; s[0] && s[1] && i > 0; i--, s += 2) {
		sum += (fromhex (s[0]) << 4) + fromhex (s[1]);
	}
	return sum;
}

/* set info to integer value */
static inline int setinfo_int (const char *key, const char * s, size_t len)
{
	char buf[10];
	int val;

	if (len > sizeof(buf))
		len = sizeof(buf)-1;
	strncpy (buf, s, len);
	buf[len] = 0;
	val = atoi(buf);
	dstate_setinfo (key, "%d", val);
	return val;
}

/* set info to integer value (for runtime remaining)
   value is expressed in minutes, but desired in seconds
 */
static inline void setinfo_int_minutes (const char *key, const char * s, size_t len)
{
	char buf[10];

	if (len > sizeof(buf))
		len = sizeof(buf)-1;
	strncpy (buf, s, len);
	buf[len] = 0;
	dstate_setinfo (key, "%d", 60*atoi (buf));
}

/* set info to float value */
static inline void setinfo_float (const char *key, const char * fmt, const char * s, size_t len, double factor)
{
	char buf[10];
	if (len > sizeof(buf))
		len = sizeof(buf)-1;
	strncpy (buf, s, len);
	buf[len] = 0;

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	dstate_setinfo (key, fmt, factor * (double)(atoi (buf)));
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
}

static int upssend(const char *fmt,...) {
	int ret;
	char buf[1024], *p;
	va_list ap;
	unsigned int	sent = 0;
	useconds_t d_usec = UPSDELAY;

	va_start(ap, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	ret = vsnprintf(buf, sizeof(buf), fmt, ap);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	/* Why do we not upsflushin here? */

	upsdebugx(3, "%s: sending %d bytes <%s>", __func__, ret, buf);

	if ((ret < 1) || (ret >= (int) sizeof(buf)))
		upslogx(LOG_ERR, "%s: vsnprintf needed more than %d bytes",
			__func__, (int) sizeof(buf));

	for (p = buf; *p && sent < INT_MAX - 1; p++) {
#ifndef WIN32
		if (write(upsfd, p, 1) != 1)
#else
		DWORD bytes_written;
		BOOL res;
		res = WriteFile(upsfd, p, 1, &bytes_written,NULL);
		if (res == 0 || bytes_written == 0)
#endif
			return -1;

		/* Note: LGTM.com analysis warns that here
		 * "Comparison is always true because d_usec >= 2"
		 * since we initialize with UPSDELAY above.
		 * Do not remove this check just in case that
		 * initialization changes, or run-time value
		 * becomes modified, in later iterations.
		 */
		if (d_usec > 0)
			usleep(d_usec);

		sent++;
		if (sent >= INT_MAX) {
			upslogx(LOG_ERR, "%s: sent >= INT_MAX, aborting",
				__func__);
		}
	}

	if (ret != (int) sent) {
		upsdebugx(1, "%s: ret %d != sent %u", __func__, ret, sent);
	}

	return (int)sent;
}

static ssize_t upsrecv(char *buf,size_t bufsize,char ec,const char *ic)
{
	ssize_t nread;

	nread = ser_get_line(upsfd, buf, bufsize - 1, ec, ic,
			     SER_WAIT_SEC, SER_WAIT_USEC);

	/* \todo is buf null terminated? */
	upsdebugx(4, "%s: read %" PRIiSIZE " <%s>", __func__, nread, buf);

	return nread;
}

static ssize_t upsflushin(int f, int verbose, const char *ignset)
{
	NUT_UNUSED_VARIABLE(f);
	upsdebugx(4, "%s: begin", __func__);
	return ser_flush_in(upsfd, ignset, verbose);
}

/* read out UPS and store info */
void upsdrv_updateinfo(void)
{
	char temp[256];
	char *p = NULL;
	int loadva;
	size_t len = 0;
	ssize_t recv;
	int retry;
	char ch;
	int checksum_ok = -1, is_online = 1, is_off, low_batt, trimming, boosting;

	upsdebugx(2, "%s: begin", __func__);

	for (retry = 0; retry < 5; ++retry) {
		upsflushin (0, 0, "\r ");
		upssend ("f\r");
		while (ser_get_char(upsfd, &ch, 0, UPSDELAY) > 0 && ch != '\n'); /* response starts with \r\n */
		temp[2] = 0;
		do {
			if ((recv = upsrecv (temp+2, sizeof temp - 2, ENDCHAR, IGNCHARS)) <= 0) {
				upsdebugx(1, "%s: upsrecv failed, "
					  "retrying without counting", __func__);
				upsflushin (0, 0, "\r ");
				upssend ("f\r");
				while (ser_get_char(upsfd, &ch, 0, UPSDELAY) > 0 && ch != '\n'); /* response starts with \r\n */
			}
		} while (temp[2] == 0);

		upsdebugx(3, "%s: received %" PRIiSIZE " bytes (try %i)",
			  __func__, recv, retry);

		/* syslog (LOG_DAEMON | LOG_NOTICE,"ups: got %d chars '%s'\n", recv, temp + 2); */
		/* status example:
		   000000000001000000000000012201210000001200014500000280600000990025000000000301BE
		   000000000001000000000000012401230000001200014800000280600000990025000000000301B7
		   |Vi||Vo|    |Io||Psou|    |Vb||f| |tr||Ti|            CS
		   000000000001000000000000023802370000000200004700000267500000990030000000000301BD
		   1    1    2    2    3    3    4    4    5    5    6    6    7    7   78
		   0    5    0    5    0    5    0    5    0    5    0    5    0    5    0    5   90
		 */

		/* last bytes are a checksum:
		   interpret response as hex string, sum of all bytes must be zero
		 */
		checksum_ok = ( (checksum (temp+2) & 0xff) == 0 );
		/* setinfo (INFO_, ""); */

		if (!checksum_ok) {
			upsdebug_hex(5, "upsdrv_updateinfo: "
				     "checksum failure buffer hex",
				     temp, (size_t)recv);
			upsdebug_ascii(5, "upsdrv_updateinfo: "
				       "checksum failure buffer ascii",
				       temp, (size_t)recv);
		}


		/* I can't figure out why this is missing the first two chars.
		   But the first two chars are not used, so just set them to zero
		   when missing. */
		len = strlen(temp+2);
		temp[0] = '0';
		temp[1] = '0';
		p = temp+2;
		if (len == 78)
			p = temp;
		else if (len != 80)
			checksum_ok = 0;
		if (checksum_ok) break;

		upsdebugx(1, "%s: failed to read status try %d",
			  __func__, retry);
		sleep(SER_WAIT_SEC);
	}

	if (!p || len < 1 || checksum_ok < 0) {
		/* \todo: Analyze/fix code and rewrite message. */
		upsdebugx(2, "%s: pointer to data not initialized after processing",
			__func__);
		dstate_datastale();
		return;
	}

	if (!checksum_ok) {
		upsdebugx(2, "%s: checksum corruption", __func__);
		upsdebug_hex(3, "buffer", temp, (size_t)len);
		dstate_datastale();
		return;
	}

	/* Log assuming ASCII which it 99% is.  \todo Improve. */
	upsdebugx(3, "%s: %s", __func__, p);

	setinfo_int ("input.voltage", p+24,4);
	setinfo_int ("output.voltage", p+28,4);
	setinfo_float ("battery.voltage", "%.1f", p+50,4, 0.1);
	setinfo_float ("output.current", "%.1f", p+36,4, 0.1);
	loadva = setinfo_int ("output.voltamps", p+40,6);
	if (maxload)
		dstate_setinfo ("ups.load", "%d", loadva * 100 / maxload);
	setinfo_float ("input.frequency", "%.1f", p+54,3, 0.1);
	setinfo_int_minutes ("battery.runtime", p+58,4);
	setinfo_int ("ups.temperature", p+62,4);

	is_online = p[17] == '0';
	low_batt = fromhex(p[21]) & 8 || fromhex(p[20]) & 1;
	is_off = p[11] == '0';
	trimming = p[33] == '1';
	boosting = 0; /* FIXME, don't know which bit gets set
			 (brownouts are very rare here and I can't
			 simulate one) */

	status_init();
	if (low_batt)
		status_set("LB ");
	else if (trimming)
		status_set("TRIM");
	else if (boosting)
		status_set("BOOST");
	else
		status_set(is_online ? (is_off ? "OFF " : "OL ") : "OB ");

	/* setinfo(INFO_STATUS, "%s%s",
	 *	(util < lownorm) ? "BOOST ", "",
	 *	(util > highnorm) ? "TRIM ", "",
	 *	((flags & TIOCM_CD) == 0) ? "" : "LB ",
	 *	((flags & TIOCM_CTS) == TIOCM_CTS) ? "OB" : "OL");
	 */

	status_commit();
	dstate_dataok();
}


/* Parameter setting */

/* all UPS tunable parameters are set with command
   'p%d=%s'
*/
static int setparam (int parameter, int dlen, const char * data)
{
	char reply[80];

	upsdebugx(2, "%s: begin", __func__);

	/* Note the use of "%*s" - parameter (int)dlen specifies
	 * the string width reserved for data */
	upssend ("p%d=%*s\r", parameter, dlen, data);
	if (upsrecv (reply, sizeof(reply), ENDCHAR, "") < 0) {
		upsdebugx(1, "%s: did not get reply", __func__);
		return 0;
	}
	if (strncmp (reply, "OK", 2) == 0) {
		upsdebugx(5, "%s: reply OK", __func__);
		return 1;
	} else {
		upsdebugx(1, "%s: reply NOT ok", __func__);
		return 0;
	}
}

/* ups_setsuper: set super-user access
   (allows setting variables)
*/
static void ups_setsuper (int super)
{
	setparam (999, super ? 4 : 0, super ? "2639" : "");
}

/* sets whether UPS will reapply power after it has shut down and line
 * power returns.
 */
static void autorestart (int restart)
{
	ups_setsuper (1);
	setparam (1, 1, restart ? "1" : "0");
	ups_setsuper (0);
}

/* set UPS parameters */
static int upsdrv_setvar (const char *var, const char * data) {
	int parameter;
	size_t len = strlen(data);
	upsdebugx(1, "%s: %s %s (%" PRIuSIZE " bytes)", __func__, var, data, len);
	if (strcmp("input.transfer.low", var) == 0) {
		parameter = 7;
	}
	else if (strcmp("input.transfer.high", var) == 0) {
		parameter = 8;
	}
	else if (strcmp("battery.runtime.low", var) == 0) {
		parameter = 2;
	}
	else {
		/*
		 * \todo Figure out if we get here by a code error or
		 * by the user asking for a variable that does not
		 * exist.  If the former, change to LOG_ERR and if the
		 * latter change to LOG_DEBUG.
		 */
		upslogx(LOG_INFO, "%s: unsettable variable %s", __func__, var);
		return STAT_SET_UNKNOWN;
	}
	ups_setsuper (1);
	assert (len < INT_MAX);
	if (setparam (parameter, (int)len, data)) {
		dstate_setinfo (var, "%*s", (int)len, data);
	}
	ups_setsuper (0);
	return STAT_SET_HANDLED;
}

/*
 * The "power down and maybe return command" is "OFF %d\r", with a
 * delay in seconds before poweroff.  As a special case, "OFF 0" does
 * not shut down.  The UPS will power on the load when power returns
 * (or after a delay if power is not out), according to the front panel
 * parameter, or the value set via `autorestart()`.
 */

/*
 * This is equivalent to the `shutdown.return` instant command, but
 * invoked with `-k`.
 * \todo Reduce duplication.
 */
void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	int	ret = -1;

	upsdebugx(2, "%s: begin", __func__);

	ret = do_loop_shutdown_commands("shutdown.return", NULL);
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(ret == STAT_INSTCMD_HANDLED ? EF_EXIT_SUCCESS : EF_EXIT_FAILURE);

	upsdebugx(2, "%s: end", __func__);
}

static int instcmd (const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "load.off")) {
		upslogx(LOG_CRIT, "%s: %s: OFF/stayoff in 1s",
			__func__, cmdname);
		autorestart (0);
		upssend ("OFF1\r");
		return STAT_INSTCMD_HANDLED;
	}
	else if (!strcasecmp(cmdname, "shutdown.return")) {
		const	char	*grace;

		upsdebugx(2, "%s: %s: start", __func__, cmdname);

		grace = dstate_getinfo("ups.delay.shutdown");
		if (!grace) {
			upsdebugx(1, "%s: ups.delay.shutdown is NULL!", __func__);
			/* Pick a different value than 20 so we can see it in the logs. */
			grace = "30";
		}

		upslogx(LOG_CRIT, "%s: OFF/restart in %s seconds", __func__, grace);

		/* Start again, overriding front panel setting. */
		autorestart (1);

		upssend ("OFF%s\r", grace);
		/* I'm nearly dead, Jim */

		upsdebugx(2, "%s: %s: end", __func__, cmdname);
		return STAT_INSTCMD_HANDLED;
	}
	/* \todo Software error or user error? */
	upslogx(LOG_ERR, "%s: unknown command [%s] [%s]",
		__func__, cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar (VAR_VALUE, "baudrate", "serial line speed");
	addvar (VAR_VALUE, "max_load", "rated VA load VA");
}

static struct {
	const char * val;
	speed_t speed;
} speed_table[] = {
	{"1200", B1200},
	{"2400", B2400},
	{"4800", B4800},
	{"9600", B9600},
	{NULL, B1200},
};

/*
 * Called first, for normal operation and for shutdown.
 */
void upsdrv_initups(void)
{
	speed_t speed = B1200;

	char * speed_val = getval("baudrate");
	char * max_load = getval("max_load");

	upsdebugx(1, "%s: begin", __func__);

	if (max_load) maxload = atoi(max_load);

	if (speed_val) {
		int i;
		for (i=0; speed_table[i].val; i++) {
			if (strcmp (speed_val, speed_table[i].val) == 0)
				break;
		}
		speed = speed_table[i].speed;
	}

	upsfd = ser_open(device_path);
	if (INVALID_FD(upsfd)) {
		upslogx(LOG_WARNING, "%s: failed to open %s",
			__func__, device_path);
		/* \todo: Deal with the failure */
	}

	/* ser_set_speed returns int 0 always; fatal if ioctl fails */
	ser_set_speed(upsfd, device_path, speed);

	upsdebugx(1, "%s: opened %s speed %s upsfd %d",
		  __func__, device_path, speed_val ? speed_val : "DEFAULT", upsfd);

	/* Set early so that it is in place for shutdown. */
	dstate_setinfo("ups.delay.shutdown", "%s", shutdown_delay);

	upsdebugx(1, "%s: end", __func__);
}

void upsdrv_cleanup(void)
{
	upsdebugx(1, "%s: begin/end", __func__);
}
