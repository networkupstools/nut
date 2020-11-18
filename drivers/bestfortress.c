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

#define UPSDELAY 50000	/* 50 ms delay required for reliable operation */
#define SER_WAIT_SEC	2	/* allow 2.0 sec for ser_get calls */
#define SER_WAIT_USEC	0
#define ENDCHAR		'\r'
#define IGNCHARS	" \n"

#if defined(__sgi) && ! defined(__GNUC__)
#define        inline  __inline
#endif

#define DRIVER_NAME     "Best Fortress UPS driver"
#define DRIVER_VERSION  "0.05"

/* driver description structure */
upsdrv_info_t   upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Holger Dietze <holger.dietze@advis.de>\n"
	"Stuart D. Gathman <stuart@bmsi.com>\n",
	DRV_EXPERIMENTAL,
	{ NULL }
};

static int instcmd (const char *cmdname, const char *extra);
static int upsdrv_setvar (const char *varname, const char *val);

/* rated VA load if known */
static int maxload = 0;

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
	dstate_setinfo("ups.delay.shutdown", "10");	/* write only */

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

#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	/* FIXME (bitness-dependent?):
	 *   error: cast from function call of type 'int' to non-matching type 'double' [-Werror,-Wbad-function-cast]
	 */
	dstate_setinfo (key, fmt, factor * (double)atoi (buf));
#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic pop
#endif
}

static int upssend(const char *fmt,...) {
	int ret;
	char buf[1024], *p;
	va_list ap;
	unsigned int	sent = 0;
	int d_usec = UPSDELAY;

	va_start(ap, fmt);
#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	ret = vsnprintf(buf, sizeof(buf), fmt, ap);
#if defined (__GNUC__) || defined (__clang__)
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	if ((ret < 1) || (ret >= (int) sizeof(buf)))
		upslogx(LOG_WARNING, "ser_send_pace: vsnprintf needed more "
				"than %d bytes", (int)sizeof(buf));
	for (p = buf; *p; p++) {
		if (write(upsfd, p, 1) != 1)
			return -1;

		if (d_usec)
			usleep(d_usec);

		sent++;
	}

	return sent;
}

static int upsrecv(char *buf,size_t bufsize,char ec,const char *ic)
{
	return ser_get_line(upsfd, buf, bufsize - 1, ec, ic,
	                    SER_WAIT_SEC, SER_WAIT_USEC);
}

static int upsflushin(int f, int verbose, const char *ignset)
{
	NUT_UNUSED_VARIABLE(f);
	return ser_flush_in(upsfd, ignset, verbose);
}

/* read out UPS and store info */
void upsdrv_updateinfo(void)
{
	char temp[256];
	char *p;
	int loadva;
	int len, recv;
	int retry;
	char ch;
	int checksum_ok, is_online=1, is_off, low_batt, trimming, boosting;

	upsdebugx(1, "upsdrv_updateinfo");

	for (retry = 0; retry < 5; ++retry) {
		upsflushin (0, 0, "\r ");
		upssend ("f\r");
		while (ser_get_char(upsfd, &ch, 0, UPSDELAY) > 0 && ch != '\n'); /* response starts with \r\n */
		temp[2] = 0;
		do {
			if ((recv = upsrecv (temp+2, sizeof temp - 2, ENDCHAR, IGNCHARS)) <= 0) {
				upsflushin (0, 0, "\r ");
				upssend ("f\r");
				while (ser_get_char(upsfd, &ch, 0, UPSDELAY) > 0 && ch != '\n'); /* response starts with \r\n */
			}
		} while (temp[2] == 0);

		upsdebugx(1, "upsdrv_updateinfo: received %i bytes (try %i)", recv, retry);
		upsdebug_hex(5, "buffer", temp, recv);

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
		checksum_ok = (checksum (temp+2) & 0xff) == 0;
		/* setinfo (INFO_, ""); */

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
		sleep(SER_WAIT_SEC);
	}

	if (!checksum_ok) {
		upsdebugx(2, "checksum corruption");
		upsdebug_hex(3, "buffer", temp, len);
		dstate_datastale();
		return;
	}
	/* upslogx(LOG_INFO, "updateinfo: %s", p); */

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
int setparam (int parameter, int dlen, const char * data)
{
	char reply[80];
	upssend ("p%d=%*s\r", parameter, dlen, data);
	if (upsrecv (reply, sizeof(reply), ENDCHAR, "") < 0) return 0;
	return strncmp (reply, "OK", 2) == 0;
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
	int len = strlen(data);
	upsdebugx(1, "Setvar: %s %s", var, data);
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
		upslogx(LOG_INFO, "Setvar: unsettable variable %s", var);
		return STAT_SET_UNKNOWN;
	}
	ups_setsuper (1);
	if (setparam (parameter, len, data)) {
		dstate_setinfo (var, "%*s", len, data);
	}
	ups_setsuper (0);
	return STAT_SET_HANDLED;
}

void upsdrv_shutdown(void)
{
	const	char	*grace;

	grace = dstate_getinfo("ups.delay.shutdown");

	if (!grace)
		grace = "1"; /* apparently, OFF0 does not work */

	printf ("shutdown in %s seconds\n", grace);
	/* make power return when utility power returns */
	autorestart (1);
	upssend ("OFF%s\r", grace);
	/* I'm nearly dead, Jim */
	/* OFF will powercycle when line power is available again */
}

static int instcmd (const char *cmdname, const char *extra)
{
	const char *p;

	if (!strcasecmp(cmdname, "load.off")) {
		printf ("powering off\n");
		autorestart (0);
		upssend ("OFF1\r");
		return STAT_INSTCMD_HANDLED;
	}
	else if (!strcasecmp(cmdname, "shutdown.return")) {
		p = dstate_getinfo ("ups.delay.shutdown");
		if (!p) p = "1";
		printf ("shutdown in %s seconds\n", p);
		autorestart (1);
		upssend ("OFF%s\r", p);
		return STAT_INSTCMD_HANDLED;
	}
	upslogx(LOG_INFO, "instcmd: unknown command [%s] [%s]", cmdname, extra);
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

struct {
	const char * val;
	speed_t speed;
} speed_table[] = {
	{"1200", B1200},
	{"2400", B2400},
	{"4800", B4800},
	{"9600", B9600},
	{NULL, B1200},
};

void upsdrv_initups(void)
{
	speed_t speed = B1200;

	char * speed_val = getval("baudrate");
	char * max_load = getval("max_load");

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
	ser_set_speed(upsfd, device_path, speed);
	/* TODO: probe ups type */

	/* the upsh handlers can't be done here, as they get initialized
	 * shortly after upsdrv_initups returns to main.
	 */
}

void upsdrv_cleanup(void)
{
}
