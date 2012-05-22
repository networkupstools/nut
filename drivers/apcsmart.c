/*
 * apcsmart.c - driver for APC smart protocol units (originally "newapc")
 *
 * Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
 *           (C) 2000  Nigel Metheringham <Nigel.Metheringham@Intechnology.co.uk>
 *           (C) 2011  Michal Soltys <soltys@ziu.info>
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

#include <sys/types.h>
#include <sys/file.h>
#include <regex.h>
#include <ctype.h>

#include "main.h"
#include "serial.h"
#include "timehead.h"

#include "apcsmart.h"
#include "apcsmart_tabs.h"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Russell Kroll <rkroll@exploits.org>\n"
	"Nigel Metheringham <Nigel.Metheringham@Intechnology.co.uk>\n"
	"Michal Soltys <soltys@ziu.info>",
	DRV_STABLE,
	{ &apc_tab_info, NULL }
};

static int ups_status = 0;

/*
 * Aix compatible names
 */
#if defined(VWERSE) && !defined(VWERASE)
#define VWERASE VWERSE
#endif /* VWERSE && !VWERASE */

#if defined(VDISCRD) && !defined(VDISCARD)
#define VDISCARD VDISCRD
#endif /* VDISCRD && !VDISCARD */


#ifndef CTRL
#define CONTROL(x)	(x&037)
#else
#define CONTROL		CTRL
#endif

/*
 * Allow use of system default characters if defined and reasonable.
 * These are based on the BSD ttydefaults.h
 */
#ifndef CDISCARD
#define CDISCARD CONTROL('O')
#endif
#ifndef CDSUSP
#define CDSUSP   CONTROL('Y')
#endif
#ifndef CEOF
#define CEOF     CONTROL('D')
#endif
#ifndef CEOL
#define CEOL	 0xff		/* was 0 */
#endif
#ifndef CERASE
#define CERASE   0177
#endif
#ifndef CINTR
#define CINTR    CONTROL('C')
#endif
#ifndef CKILL
#define CKILL	 CONTROL('U')	/* was '@' */
#endif
#ifndef CLNEXT
#define CLNEXT   CONTROL('V')
#endif
#ifndef CMIN
#define CMIN		CEOF
#endif
#ifndef CQUIT
#define CQUIT    CONTROL('\\')
#endif
#ifndef CRPRNT
#define CRPRNT   CONTROL('R')
#endif
#ifndef CREPRINT
#define CREPRINT CRPRNT
#endif
#ifndef CSTART
#define CSTART   CONTROL('Q')
#endif
#ifndef CSTOP
#define CSTOP    CONTROL('S')
#endif
#ifndef CSUSP
#define CSUSP    CONTROL('Z')
#endif
#ifndef CTIME
#define CTIME		CEOL
#endif
#ifndef CWERASE
#define CWERASE  CONTROL('W')
#endif


/* some forwards */

static int sdcmd_S(const void *);
static int sdcmd_AT(const void *);
static int sdcmd_K(const void *);
static int sdcmd_Z(const void *);
static int sdcmd_CS(const void *);

static int (*sdlist[])(const void *) = {
	sdcmd_S,
	sdcmd_AT,
	sdcmd_K,
	sdcmd_Z,
	sdcmd_CS,
};

#define SDIDX_S		0
#define SDIDX_AT	1
#define SDIDX_K		2
#define SDIDX_Z		3
#define SDIDX_CS	4
#define SDCNT 		((int)(sizeof(sdlist)/sizeof(sdlist[0])))

static apc_vartab_t *vartab_lookup_char(char cmdchar)
{
	int	i;

	for (i = 0; apc_vartab[i].name != NULL; i++)
		if (apc_vartab[i].cmd == cmdchar)
			return &apc_vartab[i];

	return NULL;
}

static apc_vartab_t *vartab_lookup_name(const char *var)
{
	int	i;

	for (i = 0; apc_vartab[i].name != NULL; i++)
		if (!(apc_vartab[i].flags & APC_DEPR) &&
		    !strcasecmp(apc_vartab[i].name, var))
			return &apc_vartab[i];

	return NULL;
}

/* FUTURE: change to use function pointers */

static int rexhlp(const char *rex, const char *val)
{
	int ret;
	regex_t mbuf;

	regcomp(&mbuf, rex, REG_EXTENDED|REG_NOSUB);
	ret = regexec(&mbuf, val, 0,0,0);
	regfree(&mbuf);
	return ret;
}

/* convert APC formatting to NUT formatting */
/* TODO: handle errors better */
static const char *convert_data(apc_vartab_t *cmd_entry, const char *upsval)
{
	static char temp[APC_LBUF];
	int tval;

	/* this should never happen */
	if (strlen(upsval) >= sizeof(temp)) {
		upslogx(LOG_CRIT, "length of [%s] too long", cmd_entry->name);
		strncpy(temp, upsval, sizeof(temp) - 1);
		temp[sizeof(temp) - 1] = '\0';
		return temp;
	}

	switch(cmd_entry->flags & APC_F_MASK) {
		case APC_F_PERCENT:
		case APC_F_VOLT:
		case APC_F_AMP:
		case APC_F_CELSIUS:
		case APC_F_HEX:
		case APC_F_DEC:
		case APC_F_SECONDS:
		case APC_F_LEAVE:
			/* no conversion for any of these */
			strcpy(temp, upsval);
			return temp;

		case APC_F_HOURS:
			/* convert to seconds */

			tval = 60 * 60 * strtol(upsval, NULL, 10);

			snprintf(temp, sizeof(temp), "%d", tval);
			return temp;

		case APC_F_MINUTES:
			/* Convert to seconds - NUT standard time measurement */
			tval = 60 * strtol(upsval, NULL, 10);
			/* Ignore errors - there's not much we can do */
			snprintf(temp, sizeof(temp), "%d", tval);
			return temp;

		case APC_F_REASON:
			switch (upsval[0]) {
				case 'R': return "unacceptable utility voltage rate of change";
				case 'H': return "high utility voltage";
				case 'L': return "low utility voltage";
				case 'T': return "line voltage notch or spike";
				case 'O': return "no transfers yet since turnon";
				case 'S': return "simulated power failure or UPS test";
				default:
					  strcpy(temp, upsval);
					  return temp;
			}
	}

	upslogx(LOG_NOTICE, "Unable to handle conversion of [%s]", cmd_entry->name);
	strcpy(temp, upsval);
	return temp;
}

static void apc_ser_set(void)
{
	struct termios tio, tio_chk;
	char *cable;

	/*
	 * this must be called before the rest, as ser_set_speed() performs
	 * early initialization of the port, apart from changing speed
	 */
	ser_set_speed(upsfd, device_path, B2400);

	memset(&tio, 0, sizeof(tio));
	errno = 0;

	if (tcgetattr(upsfd, &tio))
		fatal_with_errno(EXIT_FAILURE, "tcgetattr(%s)", device_path);

	/* set port mode: common stuff, canonical processing */

	tio.c_cflag |= (CS8 | CLOCAL | CREAD);

	tio.c_lflag |= ICANON;
#ifdef NOKERNINFO
	tio.c_lflag |= NOKERNINFO;
#endif
	tio.c_lflag &= ~(ISIG | IEXTEN);

	tio.c_iflag |= (IGNCR | IGNPAR);
	tio.c_iflag &= ~(IXON | IXOFF);

	tio.c_cc[VEOL] = '*';	/* specially handled in apc_read() */

#ifdef _POSIX_VDISABLE
	tio.c_cc[VERASE] = _POSIX_VDISABLE;
	tio.c_cc[VKILL]  = _POSIX_VDISABLE;
	tio.c_cc[VEOF] = _POSIX_VDISABLE;
	tio.c_cc[VEOL2] = _POSIX_VDISABLE;
#endif

#if 0
	/* unused in canonical mode: */
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
#endif

	if (tcflush(upsfd, TCIOFLUSH))
		fatal_with_errno(EXIT_FAILURE, "tcflush(%s)", device_path);

	/*
	 * warn:
	 * Note, that tcsetattr() returns success if /any/ of the requested
	 * changes could be successfully carried out. Thus the more complicated
	 * test.
	 */
	if (tcsetattr(upsfd, TCSANOW, &tio))
		fatal_with_errno(EXIT_FAILURE, "tcsetattr(%s)", device_path);

	/* clear status flags so that they don't affect our binary compare */
#ifdef PENDIN
	tio.c_lflag &= ~PENDIN;
#endif
#ifdef FLUSHO
	tio.c_lflag &= ~FLUSHO;
#endif

	memset(&tio_chk, 0, sizeof(tio_chk));
	if (tcgetattr(upsfd, &tio_chk))
		fatal_with_errno(EXIT_FAILURE, "tcgetattr(%s)", device_path);

	/* clear status flags so that they don't affect our binary compare */
#ifdef PENDIN
	tio_chk.c_lflag &= ~PENDIN;
#endif
#ifdef FLUSHO
	tio_chk.c_lflag &= ~FLUSHO;
#endif

	if (memcmp(&tio_chk, &tio, sizeof(tio))) {
		struct cchar {
			const char *name;
			int sub;
			u_char def;
		};
		const struct cchar cchars1[] = {
#ifdef VDISCARD
			{ "discard",    VDISCARD,       CDISCARD },
#endif
#ifdef VDSUSP
			{ "dsusp",      VDSUSP,         CDSUSP },
#endif
			{ "eof",        VEOF,           CEOF },
			{ "eol",        VEOL,           CEOL },
			{ "eol2",       VEOL2,          CEOL },
			{ "erase",      VERASE,         CERASE },
#ifdef VINTR
			{ "intr",       VINTR,          CINTR },
#endif
			{ "kill",       VKILL,          CKILL },
			{ "lnext",      VLNEXT,         CLNEXT },
			{ "min",        VMIN,           CMIN },
			{ "quit",       VQUIT,          CQUIT },
#ifdef VREPRINT
			{ "reprint",    VREPRINT,       CREPRINT },
#endif
			{ "start",      VSTART,         CSTART },
#ifdef VSTATUS
			{ "status",     VSTATUS,        CSTATUS },
#endif
			{ "stop",       VSTOP,          CSTOP },
			{ "susp",       VSUSP,          CSUSP },
			{ "time",       VTIME,          CTIME },
			{ "werase",     VWERASE,        CWERASE },
			{ .name = NULL },
		};
		const struct cchar *cp;
		struct termios *tp;

		upslogx(LOG_NOTICE, "%s: device reports different attributes than what were set", device_path);

		/*
		 * According to the manual the most common problem is
		 * mis-matched combinations of input and output baud rates.  If
		 * the combination is not supported then neither are changed.
		 * This should not be a problem here since we set them both to
		 * the same extremely common rate of 2400.
		 */

		tp = &tio;
		upsdebugx(1, "tcsetattr(): gfmt1:cflag=%x:iflag=%x:lflag=%x:oflag=%x:",
					(unsigned int) tp->c_cflag, (unsigned int) tp->c_iflag,
					(unsigned int) tp->c_lflag, (unsigned int) tp->c_oflag);
		for (cp = cchars1; cp->name; ++cp)
			upsdebugx(1, "\t%s=%x:", cp->name, tp->c_cc[cp->sub]);
		upsdebugx(1, "\tispeed=%d:ospeed=%d", (int) cfgetispeed(tp), (int) cfgetospeed(tp));

		tp = &tio_chk;
		upsdebugx(1, "tcgetattr(): gfmt1:cflag=%x:iflag=%x:lflag=%x:oflag=%x:",
					(unsigned int) tp->c_cflag, (unsigned int) tp->c_iflag,
					(unsigned int) tp->c_lflag, (unsigned int) tp->c_oflag);
		for (cp = cchars1; cp->name; ++cp)
			upsdebugx(1, "\t%s=%x:", cp->name, tp->c_cc[cp->sub]);
		upsdebugx(1, "\tispeed=%d:ospeed=%d", (int) cfgetispeed(tp), (int) cfgetospeed(tp));
	}

	cable = getval("cable");
	if (cable && !strcasecmp(cable, ALT_CABLE_1)) {
		if (ser_set_dtr(upsfd, 1) == -1)
			fatalx(EXIT_FAILURE, "ser_set_dtr() failed (%s)", device_path);
		if (ser_set_rts(upsfd, 0) == -1)
			fatalx(EXIT_FAILURE, "ser_set_rts() failed (%s)", device_path);
	}
}

static void ups_status_set(void)
{
	status_init();
	if (ups_status & APC_STAT_CAL)
		status_set("CAL");		/* calibration */
	if (ups_status & APC_STAT_TRIM)
		status_set("TRIM");		/* SmartTrim */
	if (ups_status & APC_STAT_BOOST)
		status_set("BOOST");		/* SmartBoost */
	if (ups_status & APC_STAT_OL)
		status_set("OL");		/* on line */
	if (ups_status & APC_STAT_OB)
		status_set("OB");		/* on battery */
	if (ups_status & APC_STAT_OVER)
		status_set("OVER");		/* overload */
	if (ups_status & APC_STAT_LB)
		status_set("LB");		/* low battery */
	if (ups_status & APC_STAT_RB)
		status_set("RB");		/* replace batt */

	if (ups_status == 0)
		status_set("OFF");

	status_commit();
}

static void alert_handler(char ch)
{
	switch (ch) {
		case '!':		/* clear OL, set OB */
			upsdebugx(4, "alert_handler: OB");
			ups_status &= ~APC_STAT_OL;
			ups_status |= APC_STAT_OB;
			break;

		case '$':		/* clear OB, set OL */
			upsdebugx(4, "alert_handler: OL");
			ups_status &= ~APC_STAT_OB;
			ups_status |= APC_STAT_OL;
			break;

		case '%':		/* set LB */
			upsdebugx(4, "alert_handler: LB");
			ups_status |= APC_STAT_LB;
			break;

		case '+':		/* clear LB */
			upsdebugx(4, "alert_handler: not LB");
			ups_status &= ~APC_STAT_LB;
			break;

		case '#':		/* set RB */
			upsdebugx(4, "alert_handler: RB");
			ups_status |= APC_STAT_RB;
			break;

		case '?':		/* set OVER */
			upsdebugx(4, "alert_handler: OVER");
			ups_status |= APC_STAT_OVER;
			break;

		case '=':		/* clear OVER */
			upsdebugx(4, "alert_handler: not OVER");
			ups_status &= ~APC_STAT_OVER;
			break;

		default:
			upsdebugx(4, "alert_handler got 0x%02x (unhandled)", ch);
			break;
	}

	ups_status_set();
}

/*
 * we need a tiny bit different processing due to '*' and canonical mode; the
 * function is subtly different from generic ser_get_line_alert()
 */
static int apc_read(char *buf, size_t buflen, int flags)
{
	const char *iset = IGN_CHARS, *aset = "";
	size_t	count = 0;
	int	i, ret, sec = 3, usec = 0;
	char	temp[APC_LBUF];

	if (upsfd == -1)
		return 0;
	if (flags & SER_D0) {
		sec = 0; usec = 0;
	}
	if (flags & SER_DX) {
		sec = 0; usec = 200000;
	}
	if (flags & SER_D1) {
		sec = 1; usec = 500000;
	}
	if (flags & SER_D3) {
		sec = 3; usec = 0;
	}
	if (flags & SER_AA) {
		iset = IGN_AACHARS;
		aset = ALERT_CHARS;
	}
	if (flags & SER_CC) {
		iset = IGN_CCCHARS;
		aset = "";
		sec = 6; usec = 0;
	}
	if (flags & SER_CS) {
		iset = IGN_CSCHARS;
		aset = "";
		sec = 6; usec = 0;
	}

	memset(buf, '\0', buflen);

	while (count < buflen - 1) {
		errno = 0;
		ret = select_read(upsfd, temp, sizeof(temp), sec, usec);

		/* error or no timeout allowed */
		if (ret < 0 || (ret == 0 && !(flags & SER_TO))) {
			ser_comm_fail("%s", ret ? strerror(errno) : "timeout");
			return ret;
		}
		/* ok, timeout is acceptable */
		if (ret == 0 && (flags & SER_TO)) {
			/*
			 * but it doesn't imply ser_comm_good
			 *
			 * to be more precise - we might be in comm_fail
			 * condition, trying to "nudge" the UPS with some
			 * command obviously expecting timeout if the comm is
			 * still lost. This would result with filling logs with
			 * confusing comm lost/comm re-established pairs. Thus
			 * - just return here.
			 */
			return count;
		}

		/* parse input */
		for (i = 0; i < ret; i++) {
			/* standard "line received" condition */
			if ((count == buflen - 1) || (temp[i] == ENDCHAR)) {
				ser_comm_good();
				return count;
			}
			/*
			 * '*' is set as a secondary EOL; convert to 'OK' only as a
			 * reply to shutdown command in sdok(); otherwise next
			 * select_read() will continue normally
			 */
			if ((flags & SER_HA) && temp[i] == '*') {
				/*
				 * a bit paranoid, but remember '*' is not real EOL;
				 * there could be some firmware in existence, that
				 * would send both string: 'OK\n' and alert: '*'.
				 * Just in case, try to flush the input with small 1 sec.
				 * timeout
				 */
				memset(buf, '\0', buflen);
				errno = 0;
				ret = select_read(upsfd, temp, sizeof(temp), 1, 0);
				if (ret < 0) {
					ser_comm_fail("%s", strerror(errno));
					return ret;
				}
				buf[0] = 'O';
				buf[1] = 'K';
				ser_comm_good();
				return 2;
			}
			/* ignore set */
			if (strchr(iset, temp[i]) || temp[i] == '*') {
				continue;
			}
			/* alert set */
			if (strchr(aset, temp[i])) {
				alert_handler(temp[i]);
				continue;
			}

			buf[count++] = temp[i];
		}
	}

	ser_comm_good();
	return count;
}
static int apc_write(unsigned char code)
{
	errno = 0;
	if (upsfd == -1)
		return 0;
	return ser_send_char(upsfd, code);
}

/*
 * We have to watch out for NA, here;
 * This is generally safe, as otherwise we will just timeout. The reason we do
 * it, is that under certain conditions an ups might respond with NA for
 * something it would normally handle (e.g. calling @ while being in powered
 * off or hibernated state. If we keep sending the "arguments" after getting
 * NA, they will be interpreted as commands, which is quite a bug :)
 * Furthermore later flushes might not work properly, if the reply to those
 * commands are generated with some delay.
 *
 * We also set errno to something usable, so outside upslog calls don't output
 * confusing "success".
 */
static int apc_write_long(const char *code)
{
	char temp[APC_LBUF];
	int ret;
	errno = 0;

	if (upsfd == -1)
		return 0;

	ret = ser_send_char(upsfd, *code);
	if (ret != 1)
		return ret;
	/* peek for the answer - anything at this point is failure */
	ret = apc_read(temp, sizeof(temp), SER_DX|SER_TO);
	if (ret) {
		errno = ECANCELED;
		return -1;
	}

	ret = ser_send_pace(upsfd, 50000, "%s", code + 1);
	return ret < 0 ? ret : ret + 1;
}

static int apc_write_rep(unsigned char code)
{
	char temp[APC_LBUF];
	int ret;
	errno = 0;

	if (upsfd == -1)
		return 0;

	ret = ser_send_char(upsfd, code);
	if (ret != 1)
		return ret;
	/* peek for the answer - anything at this point is failure */
	ret = apc_read(temp, sizeof(temp), SER_DX|SER_TO);
	if (ret) {
		errno = ECANCELED;
		return -1;
	}

	usleep(1300000);
	ret = ser_send_char(upsfd, code);
	if (ret != 1)
		return ret;
	return 2;
}

/* all flags other than SER_AA are ignored */
static void apc_flush(int flags)
{
	char temp[APC_LBUF];

	if (flags & SER_AA) {
		tcflush(upsfd, TCOFLUSH);
		while(apc_read(temp, sizeof(temp), SER_D0|SER_TO|SER_AA));
	} else {
		tcflush(upsfd, TCIOFLUSH);
		/* tcflush(upsfd, TCIFLUSH); */
		/* while(apc_read(temp, sizeof(temp), SER_D0|SER_TO)); */
	}
}

static const char *preread_data(apc_vartab_t *vt)
{
	int ret;
	char temp[APC_LBUF];

	upsdebugx(4, "preread_data: %s", vt->name);

	apc_flush(0);
	ret = apc_write(vt->cmd);

	if (ret != 1) {
		upslogx(LOG_ERR, "preread_data: apc_write failed");
		return 0;
	}

	ret = apc_read(temp, sizeof(temp), SER_TO);

	if (ret < 0) {
		upslogx(LOG_ERR, "preread_data: apc_read failed");
		return 0;
	}

	if (!ret || !strcmp(temp, "NA")) {
		upslogx(LOG_ERR, "preread_data: %s timed out or not supported", vt->name);
		return 0;
	}

	return convert_data(vt, temp);
}

static void remove_var(const char *cal, apc_vartab_t *vt)
{
	const char *fmt;
	char info[256];

	if (isprint(vt->cmd))
		fmt = "[%c]";
	else
		fmt = "[0x%02x]";

	snprintf(info, sizeof(info), "%s%s%s",
		"%s: verified variable [%s] (APC: ",
		fmt,
		") returned NA"
	);
	upsdebugx(1, info, cal, vt->name, vt->cmd);

	snprintf(info, sizeof(info), "%s%s%s",
		"%s: removing [%s] (APC: ",
		fmt,
		")"
	);
	upsdebugx(1, info, cal, vt->name, vt->cmd);

	vt->flags &= ~APC_PRESENT;
	dstate_delinfo(vt->name);
}

static int poll_data(apc_vartab_t *vt)
{
	int	ret;
	char	temp[APC_LBUF];

	if (!(vt->flags & APC_PRESENT))
		return 1;

	upsdebugx(4, "poll_data: %s", vt->name);

	apc_flush(SER_AA);
	ret = apc_write(vt->cmd);

	if (ret != 1) {
		upslogx(LOG_ERR, "poll_data: apc_write failed");
		dstate_datastale();
		return 0;
	}

	if (apc_read(temp, sizeof(temp), SER_AA) < 1) {
		dstate_datastale();
		return 0;
	}

	/* automagically no longer supported by the hardware somehow */
	if (!strcmp(temp, "NA"))
		remove_var("poll_data", vt);

	dstate_setinfo(vt->name, "%s", convert_data(vt, temp));
	dstate_dataok();

	return 1;
}

static int dfa_fwnew(const char *val)
{
	int ret;
	regex_t mbuf;
	/* must be xx.yy.zz */
	const char rex[] = "^[[:alnum:]]+\\.[[:alnum:]]+\\.[[:alnum:]]+$";

	regcomp(&mbuf, rex, REG_EXTENDED|REG_NOSUB);
	ret = regexec(&mbuf, val, 0,0,0);
	regfree(&mbuf);
	return ret;
}

static int dfa_cmdset(const char *val)
{
	int ret;
	regex_t mbuf;
	/*
	 * must be #.alerts.commands ; we'll be a bit lax here
	 */
	const char rex[] = "^[0-9]\\.[^.]*\\.[^.]+$";

	regcomp(&mbuf, rex, REG_EXTENDED|REG_NOSUB);
	ret = regexec(&mbuf, val, 0,0,0);
	regfree(&mbuf);
	return ret;
}

static int valid_cmd(char cmd, const char *val)
{
	char info[256], *fmt;
	int ret;

	switch (cmd) {
		case APC_FW_NEW:
			ret = dfa_fwnew(val);
			break;
		case APC_CMDSET:
			ret = dfa_cmdset(val);
			break;
		default:
			return 1;
	}

	if (ret) {
		if (isprint(cmd))
			fmt = "[%c]";
		else
			fmt = "[0x%02x]";

		snprintf(info, sizeof(info), "%s%s%s",
			"valid_cmd: cmd ",
			fmt,
			" failed regex match"
		);
		upslogx(LOG_WARNING, info, cmd);
	}

	return !ret;
}

/*
 * query_ups() is called before any APC_PRESENT flags are determined;
 * only for the variable provided
 */
static int query_ups(const char *var)
{
	int i, j;
	const char *temp;
	apc_vartab_t *vt, *vtn;

	/*
	 * at first run we know nothing about variable; we have to handle
	 * APC_MULTI gracefully as well
	 */
	for (i = 0; apc_vartab[i].name != NULL; i++) {
		vt = &apc_vartab[i];
		if (strcmp(vt->name, var) || vt->flags & APC_DEPR)
			continue;

		/* found, [try to] get it */

		temp = preread_data(vt);
		if (!temp || !valid_cmd(vt->cmd, temp)) {
			if (vt->flags & APC_MULTI) {
				vt->flags |= APC_DEPR;
				continue;
			}
			upsdebugx(1, "query_ups: unknown variable %s", var);
			break;
		}

		vt->flags |= APC_PRESENT;
		dstate_setinfo(vt->name, "%s", temp);
		dstate_dataok();

		/* supported, deprecate all the remaining ones */
		if (vt->flags & APC_MULTI)
			for (j = i + 1; apc_vartab[j].name != NULL; j++) {
				vtn = &apc_vartab[j];
				if (strcmp(vtn->name, vt->name))
					continue;
				vtn->flags |= APC_DEPR;
				vtn->flags &= ~APC_PRESENT;
			}

		return 1; /* success */
	}

	return 0;
}

/*
 * This function iterates over vartab, deprecating nut:apc 1:n variables. We
 * prefer earliest present variable. All the other ones must be marked as
 * deprecated and as not present.
 * This is intended to call after verifying the presence of variables.
 * Otherwise it would take a while to execute due to preread_data()
 */
static void deprecate_vars(void)
{
	int i, j;
	const char *temp;
	apc_vartab_t *vt, *vtn;

	for (i = 0; apc_vartab[i].name != NULL; i++) {
		vt = &apc_vartab[i];
		if (vt->flags & APC_DEPR)
			/* already handled */
			continue;

		if (!(vt->flags & APC_MULTI))
			continue;
		if (!(vt->flags & APC_PRESENT)) {
			vt->flags |= APC_DEPR;
			continue;
		}
		/* pre-read data, we have to verify it */
		temp = preread_data(vt);
		if (!temp || !valid_cmd(vt->cmd, temp)) {
			upslogx(LOG_ERR, "deprecate_vars: [%s] is unreadable or invalid, deprecating", vt->name);
			vt->flags |= APC_DEPR;
			vt->flags &= ~APC_PRESENT;
			continue;
		}

		/* multi & present, deprecate all the remaining ones */
		for (j = i + 1; apc_vartab[j].name != NULL; j++) {
			vtn = &apc_vartab[j];
			if (strcmp(vtn->name, vt->name))
				continue;
			vtn->flags |= APC_DEPR;
			vtn->flags &= ~APC_PRESENT;
		}

		dstate_setinfo(vt->name, "%s", temp);
		dstate_dataok();
	}
}

static void do_capabilities(int qco)
{
	const	char	*ptr, *entptr;
	char	upsloc, temp[APC_LBUF], cmd, loc, etmp[APC_SBUF], *endtemp;
	int	nument, entlen, i, matrix, ret, valid;
	apc_vartab_t *vt;

	upsdebugx(1, "APC - About to get capabilities string");
	/* If we can do caps, then we need the Firmware revision which has
	   the locale descriptor as the last character (ugh)
	*/
	ptr = dstate_getinfo("ups.firmware");
	if (ptr)
		upsloc = ptr[strlen(ptr) - 1];
	else
		upsloc = 0;

	/* get capability string */
	apc_flush(0);
	ret = apc_write(APC_CAPS);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "do_capabilities: apc_write failed");
		return;
	}
	/*
	 * note - apc_read() needs larger timeout grace and different
	 * ignore set due to certain characters like '#' being received
	 */
	ret = apc_read(temp, sizeof(temp), SER_CC|SER_TO);

	if ((ret < 1) || (!strcmp(temp, "NA"))) {

		/* Early Smart-UPS, not as smart as later ones */
		/* This should never happen since we only call
		   this if the REQ_CAPABILITIES command is supported
		*/
		upslogx(LOG_ERR, "ERROR: APC cannot do capabilities but said it could !");
		return;
	}

	/* recv always puts a \0 at the end, so this is safe */
	/* however it assumes a zero byte cannot be embedded */
	endtemp = &temp[0] + strlen(temp);

	if (temp[0] != '#') {
		upsdebugx(1, "unrecognized capability start char %c", temp[0]);
		upsdebugx(1, "please report this error [%s]", temp);
		upslogx(LOG_ERR, "ERROR: unknown capability start char %c!",
			temp[0]);

		return;
	}

	if (temp[1] == '#') {		/* Matrix-UPS */
		matrix = 1;
		ptr = &temp[0];
	}
	else {
		ptr = &temp[1];
		matrix = 0;
	}

	/* command char, location, # of entries, entry length */

	while (ptr[0] != '\0') {
		if (matrix)
			ptr += 2;	/* jump over repeating ## */

		/* check for idiocy */
		if (ptr >= endtemp) {

			/* if we expected this, just ignore it */
			if (qco)
				return;

			fatalx(EXIT_FAILURE,
				"capability string has overflowed\n"
				"please report this error\n"
				"ERROR: capability overflow!"
				);
		}

		cmd = ptr[0];
		loc = ptr[1];
		nument = ptr[2] - 48;
		entlen = ptr[3] - 48;
		entptr = &ptr[4];

		vt = vartab_lookup_char(cmd);
		valid = vt && ((loc == upsloc) || (loc == '4'));

		/* mark this as writable */
		if (valid) {
			upsdebugx(1, "supported capability: %02x (%c) - %s",
				cmd, loc, vt->name);

			dstate_setflags(vt->name, ST_FLAG_RW);

			/* make sure setvar knows what this is */
			vt->flags |= APC_RW | APC_ENUM;
		}

		for (i = 0; i < nument; i++) {
			if (valid) {
				snprintf(etmp, entlen + 1, "%s", entptr);
				dstate_addenum(vt->name, "%s", convert_data(vt, etmp));
			}

			entptr += entlen;
		}

		ptr = entptr;
	}
}

static int update_status(void)
{
	int	ret;
	char	buf[APC_LBUF];

	upsdebugx(4, "update_status");

	apc_flush(SER_AA);
	ret = apc_write(APC_STATUS);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "update_status: apc_write failed");
		dstate_datastale();
		return 0;
	}

	ret = apc_read(buf, sizeof(buf), SER_AA);

	if ((ret < 1) || (!strcmp(buf, "NA"))) {
		dstate_datastale();
		return 0;
	}

	ups_status = strtol(buf, 0, 16) & 0xff;
	ups_status_set();

	dstate_dataok();

	return 1;
}

static void oldapcsetup(void)
{
	/* really old models ignore REQ_MODEL, so find them first */
	if (!query_ups("ups.model")) {
		/* force the model name */
		dstate_setinfo("ups.model", "Smart-UPS");
	}

	/* see if this might be an old Matrix-UPS instead */
	if (query_ups("output.current"))
		dstate_setinfo("ups.model", "Matrix-UPS");

	query_ups("ups.firmware");
	query_ups("ups.serial");
	query_ups("input.voltage");
	query_ups("battery.charge");
	query_ups("battery.voltage");
	query_ups("input.voltage");
	query_ups("output.voltage");
	query_ups("ups.temperature");
	query_ups("ups.load");

	update_status();

	/*
	 * If we have come down this path then we dont do capabilities and
	 * other shiny features.
	 */
}

static void protocol_verify(unsigned char cmd)
{
	int i, found;
	const char *fmt, *temp;
	char info[256];

	/* don't bother with cmd/var we don't care about */
	if (strchr(APC_UNR_CMDS, cmd))
		return;

	if (isprint(cmd))
		fmt = "[%c]";
	else
		fmt = "[0x%02x]";

	/*
	 * see if it's a variable
	 * note: some nut variables map onto multiple APC ones (firmware)
	 */
	for (i = 0; apc_vartab[i].name != NULL; i++) {
		if (apc_vartab[i].cmd == cmd) {
			if (apc_vartab[i].flags & APC_MULTI) {
				/* APC_MULTI are handled by deprecate_vars() */
				apc_vartab[i].flags |= APC_PRESENT;
				return;
			}

			temp = preread_data(&apc_vartab[i]);
			if (!temp || !valid_cmd(cmd, temp)) {
				snprintf(info, sizeof(info), "%s%s%s",
					"UPS variable [%s] - APC: ",
					fmt,
					" invalid or unreadable"
				);
				upsdebugx(3, info, apc_vartab[i].name, cmd);
				return;
			}

			apc_vartab[i].flags |= APC_PRESENT;

			snprintf(info, sizeof(info), "%s%s",
				"UPS supports variable [%s] - APC: ",
				fmt
			);
			upsdebugx(3, info, apc_vartab[i].name, cmd);

			dstate_setinfo(apc_vartab[i].name, "%s", temp);
			dstate_dataok();

			/* handle special data for our two strings */
			if (apc_vartab[i].flags & APC_STRING) {
				dstate_setflags(apc_vartab[i].name,
					ST_FLAG_RW | ST_FLAG_STRING);
				dstate_setaux(apc_vartab[i].name, APC_STRLEN);

				apc_vartab[i].flags |= APC_RW;
			}
			return;
		}
	}

	/*
	 * check the command list
	 * some APC commands map onto multiple nut ones (start and stop)
	 */
	found = 0;
	for (i = 0; apc_cmdtab[i].name != NULL; i++) {
		if (apc_cmdtab[i].cmd == cmd) {

			snprintf(info, sizeof(info), "%s%s",
				"UPS supports command [%s] - APC: ",
				fmt
			);
			upsdebugx(3, info, apc_cmdtab[i].name, cmd);

			dstate_addcmd(apc_cmdtab[i].name);

			apc_cmdtab[i].flags |= APC_PRESENT;
			found = 1;
		}
	}

	if (found)
		return;

	snprintf(info, sizeof(info), "%s%s%s",
		"protocol_verify - APC: ",
		fmt,
		" unrecognized"
	);
	upsdebugx(1, info, cmd);
}

/* some hardware is a special case - hotwire the list of cmdchars */
static int firmware_table_lookup(void)
{
	int	ret;
	unsigned int	i, j;
	char	buf[APC_LBUF];

	upsdebugx(1, "attempting firmware lookup using command 'V'");

	apc_flush(0);
	ret = apc_write(APC_FW_OLD);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "firmware_table_lookup: apc_write failed");
		return 0;
	}

	ret = apc_read(buf, sizeof(buf), SER_TO);

        /*
	 * Some UPSes support both 'V' and 'b'. As 'b' doesn't always return
	 * firmware version, we attempt that only if 'V' doesn't work.
	 */
	if ((ret < 1) || (!strcmp(buf, "NA"))) {
		upsdebugx(1, "attempting firmware lookup using command 'b'");
		ret = apc_write(APC_FW_NEW);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "firmware_table_lookup: apc_write failed");
			return 0;
		}

		ret = apc_read(buf, sizeof(buf), SER_TO);

		if (ret < 1) {
			upslog_with_errno(LOG_ERR, "firmware_table_lookup: apc_read failed");
			return 0;
		}
	}

	upsdebugx(2, "firmware: [%s]", buf);

	/* this will be reworked if we get a lot of these things */
	if (!strcmp(buf, "451.2.I"))
		/* quirk_capability_overflow */
		return 2;

	for (i = 0; apc_compattab[i].firmware != NULL; i++) {
		if (!strcmp(apc_compattab[i].firmware, buf)) {

			upsdebugx(2, "matched - cmdchars: %s",
					apc_compattab[i].cmdchars);

			if (strspn(apc_compattab[i].firmware, "05")) {
				dstate_setinfo("ups.model", "Matrix-UPS");
			} else {
				dstate_setinfo("ups.model", "Smart-UPS");
			}

			/* matched - run the cmdchars from the table */
			for (j = 0; j < strlen(apc_compattab[i].cmdchars); j++)
				protocol_verify(apc_compattab[i].cmdchars[j]);
			deprecate_vars();

			return 1;	/* matched */
		}
	}

	return 0;
}

static void getbaseinfo(void)
{
	unsigned int	i;
	int	ret, qco;
	char 	*cmds, temp[APC_LBUF];

	/*
	 *  try firmware lookup first; we could start with 'a', but older models
	 *  sometimes return other things than a command set
	 */
	qco = firmware_table_lookup();
	if (qco == 1)
		/* found compat */
		return;

	upsdebugx(2, "firmware not found in compatibility table - trying normal method");
	upsdebugx(1, "APC - attempting to find command set");
	/*
	 * Initially we ask the UPS what commands it takes If this fails we are
	 * going to need an alternate strategy - we can deal with that if it
	 * happens
	 */

	apc_flush(0);
	ret = apc_write(APC_CMDSET);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "getbaseinfo: apc_write failed");
		return;
	}

	ret = apc_read(temp, sizeof(temp), SER_CS|SER_TO);

	if ((ret < 1) || (!strcmp(temp, "NA")) || !valid_cmd(APC_CMDSET, temp)) {
		/* We have an old dumb UPS - go to specific code for old stuff */
		upsdebugx(1, "APC - trying to handle unknown model");
		oldapcsetup();
		return;
	}

	upsdebugx(1, "APC - Parsing out supported cmds and vars");
	/*
	 * returned set is verified for validity above, so just extract
	 * what's interesting for us
	 */
	cmds = strrchr(temp, '.');
	for (i = 1; i < strlen(cmds); i++)
		protocol_verify(cmds[i]);
	deprecate_vars();

	/* if capabilities are supported, add them here */
	if (strchr(cmds, APC_CAPS)) {
		do_capabilities(qco);
		upsdebugx(1, "APC - UPS capabilities determined");
	}
}

/* check for calibration status and either start or stop */
static int do_cal(int start)
{
	char	temp[APC_LBUF];
	int	tval, ret;

	apc_flush(SER_AA);
	ret = apc_write(APC_STATUS);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "do_cal: apc_write failed");
		return STAT_INSTCMD_HANDLED;		/* FUTURE: failure */
	}

	ret = apc_read(temp, sizeof(temp), SER_AA);

	/* if we can't check the current calibration status, bail out */
	if ((ret < 1) || (!strcmp(temp, "NA")))
		return STAT_INSTCMD_HANDLED;		/* FUTURE: failure */

	tval = strtol(temp, 0, 16);

	if (tval & APC_STAT_CAL) {	/* calibration currently happening */
		if (start == 1) {
			/* requested start while calibration still running */
			upslogx(LOG_INFO, "runtime calibration already in progress");
			return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
		}

		/* stop requested */

		upslogx(LOG_INFO, "stopping runtime calibration");

		ret = apc_write(APC_CMD_CALTOGGLE);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "do_cal: apc_write failed");
			return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
		}

		ret = apc_read(temp, sizeof(temp), SER_AA);

		if ((ret < 1) || (!strcmp(temp, "NA")) || (!strcmp(temp, "NO"))) {
			upslogx(LOG_WARNING, "stop calibration failed: %s",
				temp);
			return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
		}

		return STAT_INSTCMD_HANDLED;	/* FUTURE: success */
	}

	/* calibration not happening */

	if (start == 0) {		/* stop requested */
		upslogx(LOG_INFO, "runtime calibration not occurring");
		return STAT_INSTCMD_HANDLED;		/* FUTURE: failure */
	}

	upslogx(LOG_INFO, "starting runtime calibration");

	ret = apc_write(APC_CMD_CALTOGGLE);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "do_cal: apc_write failed");
		return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
	}

	ret = apc_read(temp, sizeof(temp), SER_AA);

	if ((ret < 1) || (!strcmp(temp, "NA")) || (!strcmp(temp, "NO"))) {
		upslogx(LOG_WARNING, "start calibration failed: %s", temp);
		return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
	}

	return STAT_INSTCMD_HANDLED;			/* FUTURE: success */
}

#if 0
/* get the UPS talking to us in smart mode */
static int smartmode(void)
{
	int	ret;
	char	temp[APC_LBUF];

	apc_flush(0);
	ret = apc_write(APC_GOSMART);
	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "smartmode: apc_write failed");
		return 0;
	}
	ret = apc_read(temp, sizeof(temp), 0);

	if ((ret < 1) || (!strcmp(temp, "NA")) || (!strcmp(temp, "NO"))) {
		upslogx(LOG_CRIT, "enabling smartmode failed !");
		return 0;
	}

	return 1;
}
#endif

/*
 * get the UPS talking to us in smart mode
 * note: this is weird overkill, but possibly excused due to some obscure
 * hardware/firmware combinations; simpler version commmented out above, for
 * now let's keep minimally adjusted old one
 */
static int smartmode(int cnt)
{
	int ret, tries;
	char temp[APC_LBUF];

	for (tries = 0; tries < cnt; tries++) {

		apc_flush(0);
		ret = apc_write(APC_GOSMART);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "smartmode: issuing 'Y' failed");
			return 0;
		}
		ret = apc_read(temp, sizeof(temp), SER_D1);
		if (ret > 0 && !strcmp(temp, "SM"))
			return 1;	/* success */
		if (ret < 0) {
			/* error, so we didn't timeout - wait a bit before retry */
			sleep(1);
		}

		apc_flush(0);
		ret = apc_write(27); /* ESC */

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "smartmode: issuing ESC failed");
			return 0;
		}

		/* eat the response (might be NA, might be something else) */
		apc_read(temp, sizeof(temp), SER_TO|SER_D1);
	}

	return 0;	/* failure */
}

/*
 * all shutdown commands should respond with 'OK' or '*'
 * apc_read() handles conversion to 'OK' so we care only about that one
 * ign allows for timeout without assuming an error
 */
static int sdok(int ign)
{
	int ret;
	char temp[APC_SBUF];

	/*
	 * older upses on failed commands might just timeout, we cut down
	 * timeout grace though
	 * furthermore, command 'Z' will not reply with anything
	 */
	ret = apc_read(temp, sizeof(temp), SER_HA|SER_D1|SER_TO);
	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "sdok: apc_read failed");
		return STAT_INSTCMD_FAILED;
	}

	upsdebugx(4, "sdok: got \"%s\"", temp);

	if ((!ret && ign) || !strcmp(temp, "OK")) {
		upsdebugx(4, "sdok: last issued shutdown cmd succeeded");
		return STAT_INSTCMD_HANDLED;
	}

	upsdebugx(1, "sdok: last issued shutdown cmd failed");
	return STAT_INSTCMD_FAILED;
}

/* soft hibernate: S - working only when OB, otherwise ignored */
static int sdcmd_S(const void *foo)
{
	int ret;

	apc_flush(0);
	upsdebugx(1, "issuing soft hibernate");
	ret = apc_write(APC_CMD_SOFTDOWN);
	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "sdcmd_S: issuing 'S' failed");
		return STAT_INSTCMD_FAILED;
	}

	return sdok(0);
}

/* soft hibernate, hack version for CS 350 & co. */
static int sdcmd_CS(const void *foo)
{
	int ret;
	char temp[APC_SBUF];

	upsdebugx(1, "using CS 350 'force OB' shutdown method");
	if (ups_status & APC_STAT_OL) {
		apc_flush(0);
		upsdebugx(1, "status OL - forcing OB temporarily");
		ret = apc_write(APC_CMD_SIMPWF);
		if (ret < 0) {
			upslog_with_errno(LOG_ERR, "sdcmd_CS: issuing 'U' failed");
			return STAT_INSTCMD_FAILED;
		}
		/* eat response */
		ret = apc_read(temp, sizeof(temp), SER_D1);
		if (ret < 0) {
			upslog_with_errno(LOG_ERR, "sdcmd_CS: 'U' returned nothing ?");
			return STAT_INSTCMD_FAILED;
		}
	}
	return sdcmd_S(0);
}

/*
 * hard hibernate: @nnn / @nn
 * note: works differently for older and new models, see manual page for
 * thorough explanation
 */
static int sdcmd_AT(const void *str)
{
	int ret, cnt, padto, i;
	const char *awd = str;
	char temp[APC_SBUF], *ptr;

	if (!awd)
		awd = "000";

	cnt = strlen(awd);
	padto = cnt == 2 ? 2 : 3;

	temp[0] = APC_CMD_GRACEDOWN;
	ptr = temp + 1;
	for (i = cnt; i < padto ; i++) {
		*ptr++ = '0';
	}
	strcpy(ptr, awd);

	upsdebugx(1, "issuing '@' with %d minutes of additional wakeup delay", (int)strtol(awd, NULL, 10)*6);

	apc_flush(0);
	ret = apc_write_long(temp);
	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "sdcmd_AT: issuing '@' with %d digits failed", padto);
		return STAT_INSTCMD_FAILED;
	}

	ret = sdok(0);
	if (ret == STAT_INSTCMD_HANDLED || padto == 3)
		return ret;

	upslog_with_errno(LOG_ERR, "sdcmd_AT: command '@' with 2 digits doesn't work - try 3 digits");
	/*
	 * "tricky" part - we tried @nn variation and it (unsurprisingly)
	 * failed; we have to abort the sequence with something bogus to have
	 * the clean state; newer upses will respond with 'NO', older will be
	 * silent (YMMV);
	 */
	apc_write(APC_CMD_GRACEDOWN);
	/* eat response, allow it to timeout */
	apc_read(temp, sizeof(temp), SER_D1|SER_TO);

	return STAT_INSTCMD_FAILED;
}

/* shutdown: K - delayed poweroff */
static int sdcmd_K(const void *foo)
{
	int ret;

	upsdebugx(1, "issuing 'K'");

	apc_flush(0);
	ret = apc_write_rep(APC_CMD_SHUTDOWN);
	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "sdcmd_K: issuing 'K' failed");
		return STAT_INSTCMD_FAILED;
	}

	return sdok(0);
}

/* shutdown: Z - immediate poweroff */
static int sdcmd_Z(const void *foo)
{
	int ret;

	upsdebugx(1, "issuing 'Z'");

	apc_flush(0);
	ret = apc_write_rep(APC_CMD_OFF);
	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "sdcmd_Z: issuing 'Z' failed");
		return STAT_INSTCMD_FAILED;
	}

	/* note: ups will not reply anything after this command */
	return sdok(1);
}

static void upsdrv_shutdown_simple(void)
{
	unsigned int sdtype = 0;
	const char *val;

	if ((val = getval("sdtype")))
		sdtype = strtol(val, NULL, 10);

	switch (sdtype) {

	case 5:		/* hard hibernate */
		sdcmd_AT(getval("awd"));
		break;
	case 4:		/* special hack for CS 350 and similar models */
		sdcmd_CS(0);
		break;

	case 3:		/* delayed poweroff */
		sdcmd_K(0);
		break;

	case 2:		/* instant poweroff */
		sdcmd_Z(0);
		break;
	case 1:
		/*
		 * Send a combined set of shutdown commands which can work
		 * better if the UPS gets power during shutdown process
		 * Specifically it sends both the soft shutdown 'S' and the
		 * hard hibernate '@nnn' commands
		 */
		upsdebugx(1, "UPS - currently %s - sending soft/hard hibernate commands",
			(ups_status & APC_STAT_OL) ? "on-line" : "on battery");

		/* S works only when OB */
		if ((ups_status & APC_STAT_OB) && sdcmd_S(0) == STAT_INSTCMD_HANDLED)
			break;
		sdcmd_AT(getval("awd"));
		break;

	default:
		/*
		 * Send @nnn or S, depending on OB / OL status
		 */
		if (ups_status & APC_STAT_OL)		/* on line */
			sdcmd_AT(getval("awd"));
		else
			sdcmd_S(0);
	}
}

static void upsdrv_shutdown_advanced(void)
{
	const void *arg;
	const char *val;
	size_t i, len;

	val = getval("advorder");
	len = strlen(val);

	/*
	 * try each method in the list with a little bit of handling in certain
	 * cases
	 */
	for (i = 0; i < len; i++) {
		switch (val[i] - '0') {
			case SDIDX_AT:
				arg = getval("awd");
				break;
			default:
				arg = NULL;
		}

		if (sdlist[val[i] - '0'](arg) == STAT_INSTCMD_HANDLED)
			break;	/* finish if command succeeded */
	}
}

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{
	char	temp[APC_LBUF];
	int	ret;

	if (!smartmode(1))
		upsdebugx(1, "SM detection failed. Trying a shutdown command anyway");

	/* check the line status */

	ret = apc_write(APC_STATUS);

	if (ret == 1) {
		ret = apc_read(temp, sizeof(temp), SER_D1);

		if (ret < 1) {
			upsdebugx(1, "status read failed ! assuming on battery state");
			ups_status = APC_STAT_LB | APC_STAT_OB;
		} else {
			ups_status = strtol(temp, 0, 16);
		}

	} else {
		upsdebugx(1, "status request failed; assuming on battery state");
		ups_status = APC_STAT_LB | APC_STAT_OB;
	}

	if (testvar("advorder") && strcasecmp(getval("advorder"), "no"))
		upsdrv_shutdown_advanced();
	else
		upsdrv_shutdown_simple();
}

static void update_info_normal(void)
{
	int	i;

	upsdebugx(3, "update_info_normal: starting");

	for (i = 0; apc_vartab[i].name != NULL; i++) {
		if ((apc_vartab[i].flags & APC_POLL) == 0)
			continue;

		if (!poll_data(&apc_vartab[i])) {
			upsdebugx(3, "update_info_normal: poll_data (%s) failed - "
				"aborting scan", apc_vartab[i].name);
			return;
		}
	}

	upsdebugx(3, "update_info_normal: done");
}

static void update_info_all(void)
{
	int	i;

	upsdebugx(3, "update_info_all: starting");

	for (i = 0; apc_vartab[i].name != NULL; i++) {
		if (!poll_data(&apc_vartab[i])) {
			upsdebugx(3, "update_info_all: poll_data (%s) failed - "
				"aborting scan", apc_vartab[i].name);
			return;
		}
	}

	upsdebugx(3, "update_info_all: done");
}

static int setvar_enum(apc_vartab_t *vt, const char *val)
{
	int	i, ret;
	char	orig[APC_LBUF], temp[APC_LBUF];
	const char	*ptr;

	apc_flush(SER_AA);
	ret = apc_write(vt->cmd);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "setvar_enum: apc_write failed");
		return STAT_SET_FAILED;
	}

	ret = apc_read(orig, sizeof(orig), SER_AA);

	if ((ret < 1) || (!strcmp(orig, "NA")))
		return STAT_SET_FAILED;

	ptr = convert_data(vt, orig);

	/* suppress redundant changes - easier on the eeprom */
	if (!strcmp(ptr, val)) {
		upslogx(LOG_INFO, "ignoring enum SET %s='%s' (unchanged value)",
			vt->name, val);

		return STAT_SET_HANDLED;	/* FUTURE: no change */
	}

	for (i = 0; i < 6; i++) {
		ret = apc_write(APC_NEXTVAL);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "setvar_enum: apc_write failed");
			return STAT_SET_FAILED;
		}

		/* this should return either OK (if rotated) or NO (if not) */
		ret = apc_read(temp, sizeof(temp), SER_AA);

		if ((ret < 1) || (!strcmp(temp, "NA")))
			return STAT_SET_FAILED;

		/* sanity checks */
		if (!strcmp(temp, "NO"))
			return STAT_SET_FAILED;
		if (strcmp(temp, "OK"))
			return STAT_SET_FAILED;

		/* see what it rotated onto */
		ret = apc_write(vt->cmd);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "setvar_enum: apc_write failed");
			return STAT_SET_FAILED;
		}

		ret = apc_read(temp, sizeof(temp), SER_AA);

		if ((ret < 1) || (!strcmp(temp, "NA")))
			return STAT_SET_FAILED;

		ptr = convert_data(vt, temp);

		upsdebugx(1, "rotate value: got [%s], want [%s]",
			ptr, val);

		if (!strcmp(ptr, val)) {	/* got it */
			upslogx(LOG_INFO, "SET %s='%s'", vt->name, val);

			/* refresh data from the hardware */
			poll_data(vt);
			/* query_ups(vt->name, 0); */

			return STAT_SET_HANDLED;	/* FUTURE: success */
		}

		/* check for wraparound */
		if (!strcmp(ptr, orig)) {
			upslogx(LOG_ERR, "setvar: variable %s wrapped",
				vt->name);

			return STAT_SET_FAILED;
		}
	}

	upslogx(LOG_ERR, "setvar: gave up after 6 tries for %s",
		vt->name);

	/* refresh data from the hardware */
	poll_data(vt);
	/* query_ups(vt->name, 0); */

	return STAT_SET_HANDLED;
}

static int setvar_string(apc_vartab_t *vt, const char *val)
{
	unsigned int	i;
	int	ret;
	char	temp[APC_LBUF], *ptr;

	/* sanitize length */
	if (strlen(val) > APC_STRLEN) {
		upslogx(LOG_ERR, "setvar_string: value (%s) too long", val);
		return STAT_SET_FAILED;
	}

	apc_flush(SER_AA);
	ret = apc_write(vt->cmd);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "setvar_string: apc_write failed");
		return STAT_SET_FAILED;
	}

	ret = apc_read(temp, sizeof(temp), SER_AA);

	if ((ret < 1) || (!strcmp(temp, "NA")))
		return STAT_SET_FAILED;

	/* suppress redundant changes - easier on the eeprom */
	if (!strcmp(temp, val)) {
		upslogx(LOG_INFO, "ignoring string SET %s='%s' (unchanged value)",
			vt->name, val);

		return STAT_SET_HANDLED;	/* FUTURE: no change */
	}

	/* length sanitized above */
	temp[0] = APC_NEXTVAL;
	strcpy(temp + 1, val);
	ptr = temp + strlen(temp);
	for (i = strlen(val); i < APC_STRLEN; i++)
		*ptr++ = '\015'; /* pad with CRs */
	*ptr = 0;

	ret = apc_write_long(temp);

	if (ret != APC_STRLEN + 1) {
		upslog_with_errno(LOG_ERR, "setvar_string: apc_write_long failed");
		return STAT_SET_FAILED;
	}

	ret = apc_read(temp, sizeof(temp), SER_AA);

	if (ret < 1) {
		upslogx(LOG_ERR, "setvar_string: short final read");
		return STAT_SET_FAILED;
	}

	if (!strcmp(temp, "NO")) {
		upslogx(LOG_ERR, "setvar_string: got NO at final read");
		return STAT_SET_FAILED;
	}

	/* refresh data from the hardware */
	poll_data(vt);
	/* query_ups(vt->name, 0); */

	upslogx(LOG_INFO, "SET %s='%s'", vt->name, val);

	return STAT_SET_HANDLED;	/* FUTURE: success */
}

static int setvar(const char *varname, const char *val)
{
	apc_vartab_t	*vt;

	vt = vartab_lookup_name(varname);

	if (!vt)
		return STAT_SET_UNKNOWN;

	if ((vt->flags & APC_RW) == 0) {
		upslogx(LOG_WARNING, "setvar: [%s] is not writable", varname);
		return STAT_SET_UNKNOWN;
	}

	if (vt->flags & APC_ENUM)
		return setvar_enum(vt, val);

	if (vt->flags & APC_STRING)
		return setvar_string(vt, val);

	upslogx(LOG_WARNING, "setvar: Unknown type for [%s]", varname);
	return STAT_SET_UNKNOWN;
}

/* load on */
static int do_loadon(void)
{
	int ret;
	apc_flush(0);
	upsdebugx(1, "issuing load-on command");

	ret = apc_write_rep(APC_CMD_ON);
	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "do_loadon: apc_write_rep failed");
		return STAT_INSTCMD_FAILED;
	}

	/*
	 * ups will not reply anything after this command, but might
	 * generate brief OVER condition (which will be corrected on
	 * the next status update)
	 */

	upsdebugx(1, "load-on command (apc:^N) executed");
	return STAT_INSTCMD_HANDLED;
}

/* actually send the instcmd's char to the ups */
static int do_cmd(const apc_cmdtab_t *ct)
{
	int ret;
	char temp[APC_LBUF];
	const char *strerr;

	apc_flush(SER_AA);

	if (ct->flags & APC_REPEAT) {
		ret = apc_write_rep(ct->cmd);
		strerr = "apc_write_rep";
	} else {
		ret = apc_write(ct->cmd);
		strerr = "apc_write";
	}

	if (ret < 1) {
		upslog_with_errno(LOG_ERR, "do_cmd: %s failed", strerr);
		return STAT_INSTCMD_FAILED;
	}

	ret = apc_read(temp, sizeof(temp), SER_AA);

	if (ret < 1)
		return STAT_INSTCMD_FAILED;

	if (strcmp(temp, "OK")) {
		upslogx(LOG_WARNING, "got [%s] after command [%s]",
			temp, ct->name);

		return STAT_INSTCMD_FAILED;
	}

	upslogx(LOG_INFO, "command: %s", ct->name);
	return STAT_INSTCMD_HANDLED;
}

/* some commands must be repeated in a window to execute */
static int instcmd_chktime(apc_cmdtab_t *ct, const char *ext)
{
	double	elapsed;
	time_t	now;
	static	time_t	last = 0;

	time(&now);

	elapsed = difftime(now, last);
	last = now;

	/* you have to hit this in a small window or it fails */
	if ((elapsed < MINCMDTIME) || (elapsed > MAXCMDTIME)) {
		upsdebugx(1, "instcmd_chktime: outside window for [%s %s] (%2.0f)",
				ct->name, ext ? ext : "\b", elapsed);
		return 0;
	}

	return 1;
}

static int instcmd(const char *cmd, const char *ext)
{
	int i;
	apc_cmdtab_t *ct = NULL;

	for (i = 0; apc_cmdtab[i].name != NULL; i++) {
		/* main command must match */
		if (strcasecmp(apc_cmdtab[i].name, cmd))
			continue;
		/* extra was provided - check it */
		if (ext && *ext) {
			if (!apc_cmdtab[i].ext)
				continue;
			if (strlen(apc_cmdtab[i].ext) > 2) {
				if (rexhlp(apc_cmdtab[i].ext, ext))
					continue;
			} else {
				if (strcasecmp(apc_cmdtab[i].ext, ext))
					continue;
			}
		} else if (apc_cmdtab[i].ext)
			continue;
		ct = &apc_cmdtab[i];
		break;
	}

	if (!ct) {
		upslogx(LOG_WARNING, "instcmd: unknown command [%s %s]", cmd,
				ext ? ext : "\b");
		return STAT_INSTCMD_INVALID;
	}

	if (!(ct->flags & APC_PRESENT)) {
		upslogx(LOG_WARNING, "instcmd: command [%s %s] recognized, but"
		       " not supported by your UPS model", cmd,
				ext ? ext : "\b");
		return STAT_INSTCMD_INVALID;
	}

	/* first verify if the command is "nasty" */
	if ((ct->flags & APC_NASTY) && !instcmd_chktime(ct, ext))
		return STAT_INSTCMD_HANDLED;	/* future: again */

	/* we're good to go, handle special stuff first, then generic cmd */

	if (!strcasecmp(cmd, "calibrate.start"))
		return do_cal(1);

	if (!strcasecmp(cmd, "calibrate.stop"))
		return do_cal(0);

	if (!strcasecmp(cmd, "load.on"))
		return do_loadon();

	if (!strcasecmp(cmd, "load.off"))
		return sdcmd_Z(0);

	if (!strcasecmp(cmd, "shutdown.stayoff"))
		return sdcmd_K(0);

	if (!strcasecmp(cmd, "shutdown.return")) {
		if (!ext || !*ext)
			return sdcmd_S(0);

		/* ext length is guaranteed by regex match above */
		if (!strncasecmp(ext, "at", 2))
			return sdcmd_AT(ext + 3);

		if (!strncasecmp(ext, "cs", 2))
			return sdcmd_CS(0);
	}

	/* nothing special here */
	return do_cmd(ct);
}

/* install pointers to functions for msg handlers called from msgparse */
static void setuphandlers(void)
{
	upsh.setvar = setvar;
	upsh.instcmd = instcmd;
}

/* functions that interface with main.c */

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "cable", "specify alternate cable (940-0095B)");
	addvar(VAR_VALUE, "awd", "hard hibernate's additional wakeup delay");
	addvar(VAR_VALUE, "sdtype", "specify simple shutdown method (0 - " APC_SDMAX ")");
	addvar(VAR_VALUE, "advorder", "enable advanced shutdown control");
}

void upsdrv_initups(void)
{
	size_t i, len;
	char *val;

	upsfd = extrafd = ser_open(device_path);
	apc_ser_set();

	/* sanitize awd (additional waekup delay of '@' command) */
	if ((val = getval("awd")) && rexhlp(APC_AWDFMT, val)) {
			fatalx(EXIT_FAILURE, "invalid value (%s) for option 'awd'", val);
	}

	/* sanitize sdtype */
	if ((val = getval("sdtype")) && rexhlp(APC_SDFMT, val)) {
			fatalx(EXIT_FAILURE, "invalid value (%s) for option 'sdtype'", val);
	}

	/* sanitize advorder */
	if ((val = getval("advorder")) && strcasecmp(val, "no")) {
		len = strlen(val);

		if (!len || len > SDCNT)
			fatalx(EXIT_FAILURE, "invalid length of 'advorder' option (%s)", val);
		for (i = 0; i < len; i++) {
			if (val[i] < '0' || val[i] >= '0' + SDCNT) {
				fatalx(EXIT_FAILURE, "invalid characters in 'advorder' option (%s)", val);
			}
		}
	}
}

void upsdrv_cleanup(void)
{
	char temp[APC_LBUF];

	apc_flush(0);
	/* try to bring the UPS out of smart mode */
	apc_write(APC_GODUMB);
	apc_read(temp, sizeof(temp), SER_TO);
	ser_close(upsfd, device_path);
}

void upsdrv_help(void)
{
}

void upsdrv_initinfo(void)
{
	const char *pmod, *pser;

	if (!smartmode(5)) {
		fatalx(EXIT_FAILURE,
			"unable to detect an APC Smart protocol UPS on port %s\n"
			"check the cabling, port name or model name and try again", device_path
			);
	}

	/* manufacturer ID - hardcoded in this particular module */
	dstate_setinfo("ups.mfr", "APC");

	getbaseinfo();

	if (!(pmod = dstate_getinfo("ups.model")))
		pmod = "\"unknown model\"";
	if (!(pser = dstate_getinfo("ups.serial")))
		pser = "unknown serial";

	upsdebugx(1, "detected %s [%s] on %s", pmod, pser, device_path);

	setuphandlers();
}

void upsdrv_updateinfo(void)
{
	static int last_worked = 0;
	static time_t last_full = 0;
	time_t now;

	/* try to wake up a dead ups once in awhile */
	if (dstate_is_stale()) {
		if (!last_worked)
			upsdebugx(LOG_DEBUG, "upsdrv_updateinfo: comm lost");

		/* reset this so a full update runs when the UPS returns */
		last_full = 0;

		if (++last_worked < 10)
			return;

		/* become aggressive after a few tries */
		upsdebugx(LOG_DEBUG, "upsdrv_updateinfo: nudging ups with 'Y', iteration #%d ...", last_worked);
		if (!smartmode(1))
			return;

		last_worked = 0;
	}

	if (!update_status())
		return;

	time(&now);

	/* refresh all variables hourly */
	/* does not catch measure-ups II insertion/removal */
	if (difftime(now, last_full) > 3600) {
		last_full = now;
		update_info_all();
		return;
	}

	update_info_normal();
}
