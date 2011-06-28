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

#include <sys/file.h>
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

/* some forwards */

static int sdcmd_AT(const void *);
static int sdcmd_S(const void *);
static int sdcmd_CS(const void *);
static int sdcmd_K(const void *);
static int sdcmd_Z(const void *);

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

#define SDCNT 		5

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

/* convert APC formatting to NUT formatting */
/* TODO: handle errors better */
static const char *convert_data(apc_vartab_t *cmd_entry, const char *upsval)
{
	static char temp[512];
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

static int apc_ser_set(void)
{
	struct termios tio;
	char *cable;

#if 0
	if (upsfd == -1) {
		upslog_with_errno(LOG_EMERG, "apc_ser_set: programming error ! port (%s) should be opened !", device_path);
		return 0;
	}
#endif
	if (upsfd == -1)
		return 0;

	memset(&tio, 0, sizeof(tio));
	errno = 0;
	if (tcgetattr(upsfd, &tio)) {
		upslog_with_errno(LOG_ERR, "apc_ser_set: tcgetattr(%s)", device_path);
		return 0;
	}

	/* set port mode: common stuff, canonical processing, speed */

	tio.c_cflag = CS8 | CLOCAL | CREAD;

	tio.c_lflag = ICANON & ~ISIG;

	tio.c_iflag = (IGNCR | IGNPAR) & ~(IXON | IXOFF);

	tio.c_oflag = 0;

	tio.c_cc[VERASE] = _POSIX_VDISABLE;
	tio.c_cc[VKILL]  = _POSIX_VDISABLE;
	tio.c_cc[VEOF] = _POSIX_VDISABLE;
	tio.c_cc[VEOL2] = _POSIX_VDISABLE;

	tio.c_cc[VEOL] = '*';	/* specially handled in apc_read() */

	/*
	 * unused in canonical mode:
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	*/

	cfsetispeed(&tio, B2400);
	cfsetospeed(&tio, B2400);

	tcflush(upsfd, TCIOFLUSH);

	errno = 0;
	if (tcsetattr(upsfd, TCSANOW, &tio)) {
		upslog_with_errno(LOG_ERR, "apc_ser_set: tcsetattr(%s)", device_path);
		return 0;
	}

	cable = getval("cable");
	if (cable && !strcasecmp(cable, ALT_CABLE_1)) {
		if (ser_set_dtr(upsfd, 1) == -1) {
			upslog_with_errno(LOG_ERR, "apc_ser_set: ser_set_dtr() failed (%s)", device_path);
			return 0;
		}
		if (ser_set_rts(upsfd, 0) == -1) {
			upslog_with_errno(LOG_ERR, "apc_ser_set: ser_set_rts() failed (%s)", device_path);
			return 0;
		}
	}

	return 1;
}

/*
 * try to [re]open serial port
 */
static int apc_ser_try(void)
{
	int fd, ret;

#if 0
	if (upsfd >= 0)
		return 1;
#endif
	if (upsfd >= 0) {
		upslog_with_errno(LOG_EMERG, "apc_ser_try: programming error ! port (%s) should be closed !", device_path);
		return 0;
	}

	errno = 0;
	fd = open(device_path, O_RDWR | O_NOCTTY | O_EXCL | O_NONBLOCK);

	if (fd < 0) {
		upslog_with_errno(LOG_CRIT, "apc_ser_try: couldn't [re]open serial port (%s)", device_path);
		return 0;
	}

	if (do_lock_port) {
		errno = 0;
		ret = 0;
#ifdef HAVE_UU_LOCK
		ret = uu_lock(xbasename(device_path));
#elif defined(HAVE_FLOCK)
		ret = flock(fd, LOCK_EX | LOCK_NB);
#elif defined(HAVE_LOCKF)
		lseek(fd, 0L, SEEK_SET);
		ret = lockf(fd, F_TLOCK, 0L);
#endif
		if (ret < 0)
			upslog_with_errno(LOG_ERR, "apc_ser_try: couldn't lock the port (%s)", device_path);
	}

	upsfd = fd;
	extrafd = fd;

	return 1;
}

/*
 * forcefully tear down serial connection
 */
static int apc_ser_tear(void)
{
	int ret;

#if 0
	if (upsfd == -1) {
		upslog_with_errno(LOG_EMERG, "apc_ser_tear: programming error ! port (%s) should be opened !", device_path);
		return 0;
	}
#endif
	if (upsfd == -1)
		return 1;

	tcflush(upsfd, TCIOFLUSH);
#ifdef HAVE_UU_LOCK
	if (do_lock_port)
		uu_unlock(xbasename(device_path));
#endif
	errno = 0;
	ret = close(upsfd);
	if (ret < 0)
		upslog_with_errno(LOG_ERR, "apc_ser_tear: couldn't close the port (%s)", device_path);
	upsfd = -1;
	extrafd = -1;

	return 1;
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

static void apc_flush(int flags)
{
	char temp[512];

	if (flags & SER_AL) {
		tcflush(upsfd, TCOFLUSH);
		while(ser_get_line_alert(upsfd, temp, sizeof(temp), ENDCHAR, IGN_AACHARS, ALERT_CHARS, alert_handler, 0, 0) > 0);
	} else {
		tcflush(upsfd, TCIOFLUSH);
		/* while(ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, IGN_CHARS, 0, 0) > 0); */
	}
}

static int apc_write(unsigned char code)
{
	errno = 0;
	if (upsfd == -1)
		return 0;
	return ser_send_char(upsfd, code);
}

static int apc_write_rep(unsigned char code)
{
	int ret;
	errno = 0;
	if (upsfd == -1)
		return 0;

	ret = ser_send_char(upsfd, code);
	if (ret != 1)
		return ret;
	usleep(CMDLONGDELAY);
	ret = ser_send_char(upsfd, code);
	if (ret != 1)
		return ret;
	return 2;
}

static int apc_write_long(const char *code)
{
	errno = 0;
	if (upsfd == -1)
		return 0;

	return ser_send_pace(upsfd, UPSDELAY, "%s", code);
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
	char	temp[512];

	if (upsfd == -1)
		return 0;
	/*
	 * 3 sec is near the edge of how much this command can take until
	 * ENDCHAR shows up in the input; bump to 6 seconds to be on the safe
	 * side + update ignore set
	 */
	if (flags & SER_CC) {
		iset = IGN_CCCHARS;
		sec = 6;
	}
	/* alert aware read */
	if (flags & SER_AL) {
		iset = IGN_AACHARS;
		aset = ALERT_CHARS;
	}
	/* watch out for '*' during shutdown command */
	if (flags & SER_AX) {
		flags |= SER_SD;
	}
	/* "prep for shutdown" read */
	if (flags & SER_SD) {
		/* cut down timeout to 1.5 sec */
		sec = 1;
		usec = 500000;
		flags |= SER_TO;
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
			/* but it doesn't imply ser_comm_good */
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
			 * '*' is set as a secondary EOL; return '*' only as a
			 * reply to shutdown command in sdok(); otherwise next
			 * select_read() will continue normally
			 */
			if ((flags & SER_AX) && temp[i] == '*') {
				/*
				 * almost crazy, but suppose we could get
				 * something else besides '*'; just in case eat
				 * it - it's not real EOL after all
				 * there's only need to eat it, if count > 0
				 * timeout is not allowed either
				 */
				if (count) {
					errno = 0;
					ret = select_read(upsfd, temp, sizeof(temp), sec, usec);
					if (ret <= 0) {
						ser_comm_fail("%s", ret ? strerror(errno) : "timeout");
						return ret;
					}
				}
				buf[0] = 'O';
				buf[1] = 'K';
				buf[2] = '\0';
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

static void remove_var(const char *cal, apc_vartab_t *vt)
{
	const char *fmt;
	char info[256];

	if (isprint(vt->cmd))
		fmt = "[%c]";
	else
		fmt = "[0x%02x]";

	strcpy(info, "%s: Verified variable [%s] (APC: ");
	strcat(info, fmt);
	strcat(info, ") returned NA.");
	upsdebugx(1, info, cal, vt->name, vt->cmd);

	strcpy(info, "%s: Removing [%s] (APC: ");
	strcat(info, fmt);
	strcat(info, ").");
	upsdebugx(1, info, cal, vt->name, vt->cmd);

	vt->flags &= ~APC_PRESENT;
	dstate_delinfo(vt->name);
}

static int poll_data(apc_vartab_t *vt)
{
	int	ret;
	char	temp[512];

	if ((vt->flags & APC_PRESENT) == 0)
		return 1;

	upsdebugx(4, "poll_data: %s", vt->name);

	ret = apc_write(vt->cmd);

	if (ret != 1) {
		upslogx(LOG_ERR, "poll_data: apc_write failed");
		dstate_datastale();
		return 0;
	}

	if (apc_read(temp, sizeof(temp), SER_AL) < 1) {
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

/*
 * blindly check if variable is actually supported, update vartab accordingly,
 * also get the value
 */
static int query_ups(const char *var)
{
	int ret, i, j;
	char temp[512];
	const char *ptr;
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

		apc_flush(SER_AL);
		ret = apc_write(vt->cmd);
		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "query_ups: apc_write failed");
			break;
		}
		ret = apc_read(temp, sizeof(temp), SER_AL|SER_TO);

		if (ret < 1 || !strcmp(temp, "NA")) {
			if (vt->flags & APC_MULTI) {
				vt->flags |= APC_DEPR;
				continue;
			}
			upsdebugx(1, "query_ups: unknown variable %s", var);
			break;
		}

		vt->flags |= APC_PRESENT;
		ptr = convert_data(vt, temp);
		dstate_setinfo(vt->name, "%s", ptr);

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

static void do_capabilities(int qco)
{
	const	char	*ptr, *entptr;
	char	upsloc, temp[512], cmd, loc, etmp[32], *endtemp;
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
	ret = apc_write(APC_CAPS);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "do_capabilities: apc_write failed");
		return;
	}

	/*
	 * note SER_CC - apc_read() needs larger timeout grace and different
	 * ignore set due to certain characters like '#' being received
	 */
	ret = apc_read(temp, sizeof(temp), SER_CC);

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
		upsdebugx(1, "Unrecognized capability start char %c", temp[0]);
		upsdebugx(1, "Please report this error [%s]", temp);
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
				"Capability string has overflowed\n"
				"Please report this error\n"
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
			upsdebugx(1, "Supported capability: %02x (%c) - %s", 
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
	char	buf[512];

	upsdebugx(4, "update_status");

	apc_flush(SER_AL);

	ret = apc_write(APC_STATUS);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "update_status: apc_write failed");
		dstate_datastale();
		return 0;
	}

	ret = apc_read(buf, sizeof(buf), SER_AL);

	if ((ret < 1) || (!strcmp(buf, "NA"))) {
		dstate_datastale();
		return 0;
	}

	ups_status = strtol(buf, 0, 16) & 0xff;
	ups_status_set();

	dstate_dataok();

	return 1;
}

/*
 * This function iterates over vartab, deprecating nut:apc 1:n variables.  We
 * prefer earliest present variable. All the other ones must be marked as
 * deprecated and as not present.
 */
static void deprecate_vars(void)
{
	int i, j;
	apc_vartab_t *vt, *vtn;

	for (i = 0; apc_vartab[i].name != NULL; i++) {
		vt = &apc_vartab[i];
		if (!(vt->flags & APC_MULTI))
			continue;
		if (!(vt->flags & APC_PRESENT)) {
			vt->flags |= APC_DEPR;
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
		/* read preferred data */
		poll_data(vt);
	}
}

static void oldapcsetup(void)
{
	int	ret = 0;

	/* really old models ignore REQ_MODEL, so find them first */
	ret = query_ups("ups.model");

	if (ret != 1) {
		/* force the model name */
		dstate_setinfo("ups.model", "Smart-UPS");
	}

	/* see if this might be an old Matrix-UPS instead */
	if (query_ups("output.current"))
		dstate_setinfo("ups.model", "Matrix-UPS");

	query_ups("ups.firmware");
	query_ups("ups.serial");
	query_ups("input.voltage"); /* This one may fail... no problem */

	update_status();

	/*
	 * If we have come down this path then we dont do capabilities and
	 * other shiny features.
	 */
}

static void protocol_verify(unsigned char cmd)
{
	int i, found;
	const char *fmt;
	char info[64];

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

			strcpy(info, "UPS supports variable [%s] - APC: ");
			strcat(info, fmt);
			upsdebugx(3, info, apc_vartab[i].name, cmd);

			/* mark as present */
			apc_vartab[i].flags |= APC_PRESENT;

			/* APC_MULTI are handled by deprecate_vars() */
			if (!(apc_vartab[i].flags & APC_MULTI)) {
				/* load initial data */
				poll_data(&apc_vartab[i]);
			}

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

			strcpy(info, "UPS supports command [%s] - APC: ");
			strcat(info, fmt);
			upsdebugx(3, info, apc_cmdtab[i].name, cmd);

			dstate_addcmd(apc_cmdtab[i].name);

			apc_cmdtab[i].flags |= APC_PRESENT;
			found = 1;
		}
	}

	if (found || strchr(APC_UNR_CMDS, cmd))
		return;

	strcpy(info, "protocol_verify - APC: ");
	strcat(info, fmt);
	strcat(info, " unrecognized");
	upsdebugx(1, info, cmd);
}

/* some hardware is a special case - hotwire the list of cmdchars */
static int firmware_table_lookup(int *qco)
{
	int	ret;
	unsigned int	i, j;
	char	buf[512];

	upsdebugx(1, "Attempting firmware lookup using command 'V'");

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
		upsdebugx(1, "Attempting firmware lookup using command 'b'");
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

	upsdebugx(2, "Firmware: [%s]", buf);

	/* this will be reworked if we get a lot of these things */
	if (!strcmp(buf, "451.2.I"))
		/* quirk_capability_overflow */
		*qco = 1;

	if (!*qco)
		for (i = 0; apc_compattab[i].firmware != NULL; i++) {
			if (!strcmp(apc_compattab[i].firmware, buf)) {

				upsdebugx(2, "Matched - cmdchars: %s",
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

	upsdebugx(2, "Not found in table - trying normal method");
	return 0;			
}

static void getbaseinfo(void)
{
	unsigned int	i;
	int	ret = 0, qco = 0;
	char 	*alrts, *cmds, temp[512];

	/*
	 *  try firmware lookup first; we could start with 'a', but older models
	 *  sometimes return other things than a command set
	 */
	if (firmware_table_lookup(&qco) == 1)
		return;

	upsdebugx(1, "APC - Attempting to find command set");
	/* Initially we ask the UPS what commands it takes
	   If this fails we are going to need an alternate
	   strategy - we can deal with that if it happens
	*/

	ret = apc_write(APC_CMDSET);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "getbaseinfo: apc_write failed");
		return;
	}

	ret = apc_read(temp, sizeof(temp), SER_TO);

	if ((ret < 1) || (!strcmp(temp, "NA"))) {
		/* We have an old dumb UPS - go to specific code for old stuff */
		oldapcsetup();
		return;
	}

	upsdebugx(1, "APC - Parsing out command set");
	/*
	 * note that apc_read() above will filter commands overlapping with
	 * alerts; we don't really care about those for the needed
	 * functionality - but keep in mind 'a' reports them, should you ever
	 * need them; in such case, it would be best to e.g. add SER_CS and
	 * IGN_CSCHARS and adjust apc_read() accordingly
	 */
 	alrts = strchr(temp, '.');
	if (alrts == NULL) {
		fatalx(EXIT_FAILURE, "Unable to split APC version string");
	}
	*alrts++ = 0;

	cmds = strchr(alrts, '.');
	if (cmds == NULL) {
		fatalx(EXIT_FAILURE, "Unable to find APC command string");
	}
	*cmds++ = 0;

	for (i = 0; i < strlen(cmds); i++)
		protocol_verify(cmds[i]);
	deprecate_vars();

	/* if capabilities are supported, add them here */
	if (strchr(cmds, APC_CAPS))
		do_capabilities(qco);

	upsdebugx(1, "APC - UPS capabilities determined");
}

/* check for calibration status and either start or stop */
static int do_cal(int start)
{
	char	temp[512];
	int	tval, ret;

	ret = apc_write(APC_STATUS);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "do_cal: apc_write failed");
		return STAT_INSTCMD_HANDLED;		/* FUTURE: failure */
	}

	ret = apc_read(temp, sizeof(temp), SER_AL);

	/* if we can't check the current calibration status, bail out */
	if ((ret < 1) || (!strcmp(temp, "NA")))
		return STAT_INSTCMD_HANDLED;		/* FUTURE: failure */

	tval = strtol(temp, 0, 16);

	if (tval & APC_STAT_CAL) {	/* calibration currently happening */
		if (start == 1) {
			/* requested start while calibration still running */
			upslogx(LOG_INFO, "Runtime calibration already in progress");
			return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
		}

		/* stop requested */

		upslogx(LOG_INFO, "Stopping runtime calibration");

		ret = apc_write(APC_CMD_CALTOGGLE);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "do_cal: apc_write failed");
			return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
		}

		ret = apc_read(temp, sizeof(temp), SER_AL);

		if ((ret < 1) || (!strcmp(temp, "NA")) || (!strcmp(temp, "NO"))) {
			upslogx(LOG_WARNING, "Stop calibration failed: %s", 
				temp);
			return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
		}

		return STAT_INSTCMD_HANDLED;	/* FUTURE: success */
	}

	/* calibration not happening */

	if (start == 0) {		/* stop requested */
		upslogx(LOG_INFO, "Runtime calibration not occurring");
		return STAT_INSTCMD_HANDLED;		/* FUTURE: failure */
	}

	upslogx(LOG_INFO, "Starting runtime calibration");

	ret = apc_write(APC_CMD_CALTOGGLE);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "do_cal: apc_write failed");
		return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
	}

	ret = apc_read(temp, sizeof(temp), SER_AL);

	if ((ret < 1) || (!strcmp(temp, "NA")) || (!strcmp(temp, "NO"))) {
		upslogx(LOG_WARNING, "Start calibration failed: %s", temp);
		return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
	}

	return STAT_INSTCMD_HANDLED;			/* FUTURE: success */
}

/* get the UPS talking to us in smart mode */
static int smartmode(void)
{
	int	ret;
	char	temp[512];

	apc_flush(0);
	ret = apc_write(APC_GOSMART);
	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "smartmode: apc_write failed");
		return 0;
	}
	ret = apc_read(temp, sizeof(temp), 0);

	if ((ret < 1) || (!strcmp(temp, "NA")) || (!strcmp(temp, "NO"))) {
		upslogx(LOG_CRIT, "Enabling smartmode failed !");
		return 0;
	}

	return 1;
#if 0
	for (tries = 0; tries < 3; tries++) {

		if (ret > 0 && !strcmp(temp, "SM"))
			return 1;	/* success */

		sleep(1);	/* wait before trying again */

		/* it failed, so try to bail out of menus on newer units */

		ret = apc_write(27); /* ESC */

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "smartmode: apc_write failed");
			return 0;
		}

		/* eat the response (might be NA, might be something else) */
		apc_read(temp, sizeof(temp), 0);
	}

	return 0;	/* failure */
#endif
}

/* verify validity of ATn argument */
static int validate_ATn_arg(const char *str)
{
	int i = 0;
	if (!str || !*str)
		return 0;
	while (str[i] && i < 4) {
		if (str[i] < '0' || str[i] > '9')
			return -1;
		i++;
	}
	return i < 4 ? i : -1;
}

/*
 * all shutdown commands should respond with 'OK' or '*'
 * apc_read() handles conversion to 'OK' so we care only about that one
 */
static int sdok(int ign)
{
	int ret;
	char temp[32];

	/*
	 * older upses on failed commands might just timeout, we cut down
	 * timeout grace in apc_read though
	 * furthermore, Z will not reply with anything
	 */
	ret = apc_read(temp, sizeof(temp), SER_AX);
	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "sdok: apc_read failed");
		return STAT_INSTCMD_FAILED;
	}

	upsdebugx(4, "sdok: got \"%s\"", temp);

	if ((!ret && ign) || !strcmp(temp, "OK")) {
		upsdebugx(4, "sdok: last issued shutdown command succeeded");
		return STAT_INSTCMD_HANDLED;
	}

	upsdebugx(1, "sdok: last issued shutdown command failed");
	return STAT_INSTCMD_FAILED;
}

/* soft hibernate: S - working only when OB, otherwise ignored */
static int sdcmd_S(const void *foo)
{
	int ret;

	apc_flush(0);
	upsdebugx(1, "Issuing soft hibernate");
	ret = apc_write(APC_CMD_SOFTDOWN);
	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "sdcmd_S: apc_write failed");
		return STAT_INSTCMD_FAILED;
	}

	return sdok(0);
}

/* soft hibernate, hack version for CS 350 & co. */
static int sdcmd_CS(const void *foo)
{
	int ret;
	char temp[32];

	upsdebugx(1, "Using CS 350 'force OB' shutdown method");
	if (ups_status & APC_STAT_OL) {
		apc_flush(0);
		upsdebugx(1, "On-line - forcing OB temporarily");
		ret = apc_write(APC_CMD_SIMPWF);
		if (ret < 0) {
			upslog_with_errno(LOG_ERR, "sdcmd_CS: apc_write failed");
			return STAT_INSTCMD_FAILED;
		}
		/* eat response */
		ret = apc_read(temp, sizeof(temp), SER_SD);
		if (ret < 0) {
			upslog_with_errno(LOG_ERR, "sdcmd_CS: apc_read failed");
			return STAT_INSTCMD_FAILED;
		}
	}
	return sdcmd_S(0);
}

/*
 * hard hibernate: @nnn / @nn
 * note: works differently for older and new models, see help function for
 * detailed info
 */
static int sdcmd_AT(const void *str)
{
	int ret, cnt, padto, i;
	const char *awd = str;
	char temp[32], *ptr;

	cnt = validate_ATn_arg(awd);
	if (cnt < 0) {
		upslogx(LOG_ERR, "sdcmd_ATn: invalid argument (%s)", awd);
	}

	if (!awd) {
		awd = "000";
		cnt = 3;
	}

	padto = cnt == 2 ? 2 : 3;
	temp[0] = APC_CMD_GRACEDOWN;
	ptr = temp + 1;
	for (i = cnt; i < padto ; i++) {
		*ptr++ = '0';
	}
	strcpy(ptr, awd);

#if 0
	mmax = cnt == 2 ? 99 : 999;

	if ((strval = getval("awd"))) {
		errno = 0;
		n = strtol(strval, NULL, 10);
		if (errno || n < 0 || n > mmax)
			n = 0;
	}

	snprintf(temp, sizeof(temp), "%c%.*d", APC_CMD_GRACEDOWN, cnt, n);
#endif


	apc_flush(0);
	upsdebugx(1, "Issuing hard hibernate with %d minutes additional wakeup delay", (int)strtol(awd, NULL, 10)*6);

	ret = apc_write_long(temp);
	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "sdcmd_AT: apc_write_long failed");
		return STAT_INSTCMD_FAILED;
	}

	ret = sdok(0);
	if (ret == STAT_INSTCMD_HANDLED || cnt == 3)
		return ret;

	/*
	 * "tricky" part - we tried @nn variation and it (unsurprisingly)
	 * failed; we have to abort the sequence with something bogus to have
	 * the clean state; newer upses will respond with 'NO', older will be
	 * silent (YMMV);
	 */
	apc_write(APC_CMD_GRACEDOWN);
	/* eat response */
	apc_read(temp, sizeof(temp), SER_SD);

	return STAT_INSTCMD_FAILED;
}

/* shutdown: K - delayed poweroff */
static int sdcmd_K(const void *foo)
{
	int ret;

	apc_flush(0);
	upsdebugx(1, "Issuing delayed poweroff");

	ret = apc_write_rep(APC_CMD_SHUTDOWN);
	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "sdcmd_K: apc_write_rep failed");
		return STAT_INSTCMD_FAILED;
	}

	return sdok(0);
}

/* shutdown: Z - immediate poweroff */
static int sdcmd_Z(const void *foo)
{
	int ret;

	apc_flush(0);
	upsdebugx(1, "Issuing immediate poweroff");

	ret = apc_write_rep(APC_CMD_OFF);
	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "sdcmd_Z: apc_write_rep failed");
		return STAT_INSTCMD_FAILED;
	}

	/* note: ups will not reply anything after this command */
	return sdok(1);
}

static void upsdrv_shutdown_simple(void)
{
	unsigned int sdtype = 0;
	const char *val;

	if ((val = getval("sdtype"))) {
		errno = 0;
		sdtype = strtol(val, NULL, 10);
		if (errno || sdtype < 0 || sdtype > 5)
			sdtype = 0;
	}

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
	char	temp[32];
	int	ret;

	if (!smartmode())
		upsdebugx(1, "SM detection failed. Trying a shutdown command anyway");

	/* check the line status */

	ret = apc_write(APC_STATUS);

	if (ret == 1) {
		ret = apc_read(temp, sizeof(temp), SER_SD);

		if (ret < 1) {
			upsdebugx(1, "Status read failed ! Assuming on battery state");
			ups_status = APC_STAT_LB | APC_STAT_OB;
		} else {
			ups_status = strtol(temp, 0, 16);
		}

	} else {
		upsdebugx(1, "Status request failed; assuming on battery state");
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
	char	orig[512], temp[512];
	const char	*ptr;

	apc_flush(SER_AL);
	ret = apc_write(vt->cmd);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "setvar_enum: apc_write failed");
		return STAT_SET_FAILED;
	}

	ret = apc_read(orig, sizeof(orig), SER_AL);

	if ((ret < 1) || (!strcmp(orig, "NA")))
		return STAT_SET_FAILED;

	ptr = convert_data(vt, orig);

	/* suppress redundant changes - easier on the eeprom */
	if (!strcmp(ptr, val)) {
		upslogx(LOG_INFO, "Ignoring enum SET %s='%s' (unchanged value)",
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
		ret = apc_read(temp, sizeof(temp), SER_AL);

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

		ret = apc_read(temp, sizeof(temp), SER_AL);

		if ((ret < 1) || (!strcmp(temp, "NA")))
			return STAT_SET_FAILED;

		ptr = convert_data(vt, temp);

		upsdebugx(1, "Rotate value: got [%s], want [%s]",
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
	char	temp[512], *ptr;

	if (strlen(val) > APC_STRLEN) {
		upslogx(LOG_ERR, "setvar_string: value (%s) too long", val);
		return STAT_SET_FAILED;
	}

	apc_flush(SER_AL);
	ret = apc_write(vt->cmd);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "setvar_string: apc_write failed");
		return STAT_SET_FAILED;
	}

	ret = apc_read(temp, sizeof(temp), SER_AL);

	if ((ret < 1) || (!strcmp(temp, "NA")))
		return STAT_SET_FAILED;

	/* suppress redundant changes - easier on the eeprom */
	if (!strcmp(temp, val)) {
		upslogx(LOG_INFO, "Ignoring string SET %s='%s' (unchanged value)",
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

	ret = apc_write_long(ptr);

	if ((size_t)ret != strlen(ptr)) {
		upslog_with_errno(LOG_ERR, "setvar_string: apc_write_long failed");
		return STAT_SET_FAILED;
	}

	ret = apc_read(temp, sizeof(temp), SER_AL);

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
	upsdebugx(1, "Issuing load-on command.");

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

	upsdebugx(1, "Load N command (apc:^N) executed.");
	return STAT_INSTCMD_HANDLED;
}

/* actually send the instcmd's char to the ups */
static int do_cmd(const apc_cmdtab_t *ct)
{
	int ret;
	char temp[512];
	const char *strerr;

	apc_flush(SER_AL);

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

	ret = apc_read(temp, sizeof(temp), SER_AL);

	if (ret < 1)
		return STAT_INSTCMD_FAILED;

	if (strcmp(temp, "OK")) {
		upslogx(LOG_WARNING, "Got [%s] after command [%s]",
			temp, ct->name);

		return STAT_INSTCMD_FAILED;
	}

	upslogx(LOG_INFO, "Command: %s", ct->name);
	return STAT_INSTCMD_HANDLED;
}

/* some commands must be repeated in a window to execute */
static int instcmd_chktime(apc_cmdtab_t *ct)
{
	double	elapsed;
	time_t	now;
	static	time_t	last = 0;

	time(&now);

	elapsed = difftime(now, last);
	last = now;

	/* you have to hit this in a small window or it fails */
	if ((elapsed < MINCMDTIME) || (elapsed > MAXCMDTIME)) {
		upsdebugx(1, "instcmd_chktime: outside window for %s (%2.0f)",
				ct->name, elapsed);
		return 0;
	}

	return 1;
}

static int instcmd(const char *cmd, const char *ext)
{
	int i;
	apc_cmdtab_t *ct = NULL;

	for (i = 0; apc_cmdtab[i].name != NULL; i++) {
		if (strcasecmp(apc_cmdtab[i].name, cmd))
			continue;
		/* extra were provided and we care - check them */
		if (ext && *ext && apc_cmdtab[i].ext) {
			if (!strcmp(apc_cmdtab[i].ext, "!for")) {
				/*
				 * can't have that using && with the above,
				 * if you're thinking about "fixing" it :)
				 */
				if (validate_ATn_arg(ext) < 0)
					continue;
			} else if (strcasecmp(apc_cmdtab[i].ext, ext))
				continue;
		}
		ct = &apc_cmdtab[i];
		break;
	}

	if (!ct) {
		upslogx(LOG_WARNING, "instcmd: unknown command [%s]", cmd);
		return STAT_INSTCMD_INVALID;
	}

	if (!(ct->flags & APC_PRESENT)) {
		upslogx(LOG_WARNING, "instcmd: command [%s] recognized, but"
		       " not supported by your UPS model", cmd);
		return STAT_INSTCMD_INVALID;
	}

	/* first verify if the command is "nasty" */
	if ((ct->flags & APC_NASTY) && !instcmd_chktime(ct))
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
		if (!ct->ext)
			return sdcmd_S(0);

		if (!strcmp(ct->ext, "!for"))
			return sdcmd_AT(ext);

		if (!strcmp(ct->ext, "cs"))
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
	addvar(VAR_VALUE, "cable", "Specify alternate cable (940-0095B)");
	addvar(VAR_VALUE, "awd", "Hard hibernate's additional wakeup delay");
	addvar(VAR_VALUE, "sdtype", "Specify simple shutdown method (0-5)");
	addvar(VAR_VALUE, "advorder", "Enable advanced shutdown control");
}

void upsdrv_initups(void)
{
	size_t i, len;
	char *val;

	if (!apc_ser_try())
		fatalx(EXIT_FAILURE, "couldn't open port (%s)", device_path);
	if (!apc_ser_set())
		fatalx(EXIT_FAILURE, "couldn't set options on port (%s)", device_path);

	if (validate_ATn_arg((val = getval("awd"))) < 0)
		fatalx(EXIT_FAILURE, "Invalid value (%s) for option 'awd'.", val);

	/* sanitize advorder */
	if (!(val = getval("advorder")) || !strcasecmp(val, "no"))
		return;

	len = strlen(val);

	if (!len || len > SDCNT)
		fatalx(EXIT_FAILURE, "Invalid length of 'advorder' option (%s).", val);
	for (i = 0; i < len; i++) {
		if (val[i] < '0' || val[i] >= '0' + SDCNT) {
			fatalx(EXIT_FAILURE, "Invalid characters in 'advorder' option (%s).", val);
		}
	}
}

void upsdrv_cleanup(void)
{
	char temp[512];

	apc_flush(0);
	/* try to bring the UPS out of smart mode */
	apc_write(APC_GODUMB);
	apc_read(temp, sizeof(temp), SER_TO);
	apc_ser_tear();
}

void upsdrv_help(void)
{
}

void upsdrv_initinfo(void)
{
	const char *pmod, *pser;

	if (!smartmode()) {
		fatalx(EXIT_FAILURE, 
			"Unable to detect an APC Smart protocol UPS on port %s\n"
			"Check the cabling, port name or model name and try again", device_path
			);
	}

	/* manufacturer ID - hardcoded in this particular module */
	dstate_setinfo("ups.mfr", "APC");

	getbaseinfo();

	if (!(pmod = dstate_getinfo("ups.model")))
		pmod = "\"unknown model\"";
	if (!(pser = dstate_getinfo("ups.serial")))
		pser = "unknown serial";

	upsdebugx(1, "Detected %s [%s] on %s", pmod, pser, device_path);

	setuphandlers();
}

void upsdrv_updateinfo(void)
{
	static int last_worked = 0;
	static time_t last_full = 0;
	time_t now;

	/* try to wake up a dead ups once in awhile */
	while ((dstate_is_stale())) {
		upslogx(LOG_ERR, "Communications with UPS lost - check cabling.");

		/* reset this so a full update runs when the UPS returns */
		last_full = 0;

		/* become aggressive */
		if (++last_worked > 10) {
			upslogx(LOG_ERR, "Attempting to reset serial port (%s).", device_path);
			if (!(apc_ser_tear() && apc_ser_try() && apc_ser_set()))
				return;
		}
		if (!smartmode())
			return;

		last_worked = 0;
		break;
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

#if 0

/* old version for reference, if we wanted to get back to it */
/*
 * blindly check if variable is actually supported, update vartab accordingly,
 * also get the value; could use some simplifying ...
 */
static int query_ups(const char *var, int first)
{
	int ret, i, j;
	char temp[512];
	const char *ptr;
	apc_vartab_t *vt, *vtn;

	/*
	 * at first run we know nothing about variable; we have to handle
	 * APC_MULTI gracefully as well
	 */
	for (i = 0; apc_vartab[i].name != NULL; i++) {
		vt = &apc_vartab[i];
		if (strcmp(vt->name, var) || vt->flags & APC_DEPR)
			continue;
		/*
		 * ok, found in table & not deprecated - if not the first run
		 * and not present, bail out
		 */
		if (!first && !(vt->flags & APC_PRESENT))
			break;

		/* found, [try to] get it */

		/* empty the input buffer (while allowing the alert handler to run) */
		apc_flush(SER_AL);
		ret = apc_write(vt->cmd);
		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "query_ups: apc_write failed");
			break;
		}
		ret = apc_read(temp, sizeof(temp), SER_AL | (first ? SER_TO : 0));

		if (ret < 1 || !strcmp(temp, "NA")) {
			if (first) {
				if (vt->flags & APC_MULTI) {
					vt->flags |= APC_DEPR;
					continue;
				}
				upsdebugx(1, "query_ups: unknown variable %s", var);
				break;
			}
			/* automagically no longer supported by the hardware somehow */
			if (ret > 0)
				remove_var("query_ups", vt);
			break;
		}

		vt->flags |= APC_PRESENT;
		ptr = convert_data(vt, temp);
		dstate_setinfo(vt->name, "%s", ptr);

		/* supported, deprecate all the remaining ones */
		if (first && vt->flags & APC_MULTI)
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
#endif
