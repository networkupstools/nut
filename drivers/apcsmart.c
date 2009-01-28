/*
   apcsmart.c - driver for APC smart protocol units (originally "newapc")

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
             (C) 2000  Nigel Metheringham <Nigel.Metheringham@Intechnology.co.uk>

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

#include "main.h"
#include "serial.h"
#include "apcsmart.h"

#define DRIVER_NAME	"APC Smart protocol driver"
#define DRIVER_VERSION	"2.00"

static upsdrv_info_t table_info = {
	"APC command table",
	APC_TABLE_VERSION,
	NULL,
	0,
	{ NULL }
};

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Russell Kroll <rkroll@exploits.org>\n" \
	"Nigel Metheringham <Nigel.Metheringham@Intechnology.co.uk>",
	DRV_STABLE,
	{ &table_info, NULL }
};

#define ALT_CABLE_1 "940-0095B"

	static	int	ups_status = 0, quirk_capability_overflow = 0;

static struct apc_vartab_t *vartab_lookup_char(char cmdchar)
{
	int	i;

	for (i = 0; apc_vartab[i].name != NULL; i++)
		if (apc_vartab[i].cmd == cmdchar)
			return &apc_vartab[i];

	return NULL;
}

static struct apc_vartab_t *vartab_lookup_name(const char *var)
{
	int	i;

	for (i = 0; apc_vartab[i].name != NULL; i++)
		if (!strcasecmp(apc_vartab[i].name, var))
			return &apc_vartab[i];

	return NULL;
}

/* FUTURE: change to use function pointers */

/* convert APC formatting to NUT formatting */
static char *convert_data(struct apc_vartab_t *cmd_entry, char *upsval)
{
	static	char tmp[128];
	int	tval;

	switch(cmd_entry->flags & APC_FORMATMASK) {
		case APC_F_PERCENT:
		case APC_F_VOLT:
		case APC_F_AMP:
		case APC_F_CELSIUS:
		case APC_F_HEX:
		case APC_F_DEC:
		case APC_F_SECONDS:
		case APC_F_LEAVE:

			/* no conversion for any of these */
			return upsval;

		case APC_F_HOURS:
			/* convert to seconds */

			tval = 60 * 60 * strtol(upsval, NULL, 10);

			snprintf(tmp, sizeof(tmp), "%d", tval);
			return tmp;

		case APC_F_MINUTES:
			/* Convert to seconds - NUT standard time measurement */
			tval = 60 * strtol(upsval, NULL, 10);
			/* Ignore errors - Theres not much we can do */
			snprintf(tmp, sizeof(tmp), "%d", tval);
			return tmp;

		default:
			upslogx(LOG_NOTICE, "Unable to handle conversion of %s",
				cmd_entry->name);
			return upsval;
	}

	/* NOTREACHED */
	return upsval;
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

		default:
			upsdebugx(4, "alert_handler got 0x%02x (unhandled)", ch);
			break;
	}

	ups_status_set();
}

static int read_buf(char *buf, size_t buflen)
{
	int	ret;

	ret = ser_get_line_alert(upsfd, buf, buflen, ENDCHAR, POLL_IGNORE,
		POLL_ALERT, alert_handler, SER_WAIT_SEC, SER_WAIT_USEC);

	if (ret < 1) {
		ser_comm_fail("%s", ret ? strerror(errno) : "timeout");
		return ret;
	}

	ser_comm_good();
	return ret;
}

static int poll_data(struct apc_vartab_t *vt)
{
	int	ret;
	char	tmp[SMALLBUF];

	if ((vt->flags & APC_PRESENT) == 0)
		return 1;

	upsdebugx(4, "poll_data: %s", vt->name);

	ret = ser_send_char(upsfd, vt->cmd);

	if (ret != 1) {
		upslogx(LOG_ERR, "poll_data: ser_send_char failed");
		dstate_datastale();
		return 0;
	}

	if (read_buf(tmp, sizeof(tmp)) < 1) {
		dstate_datastale();
		return 0;
	}

	/* no longer supported by the hardware somehow */
	if (!strcmp(tmp, "NA")) {
		dstate_delinfo(vt->name);
		return 1;
	}

	dstate_setinfo(vt->name, "%s", convert_data(vt, tmp));
	dstate_dataok();

	return 1;
}

/* check for support or just update a named variable */
static int query_ups(const char *var, int first)
{
	int	ret;
	char	temp[256], *ptr;
	struct	apc_vartab_t *vt;

	vt = vartab_lookup_name(var);

	if (!vt) {
		upsdebugx(1, "query_ups: unknown variable %s", var);
		return 0;
	}

	/* already known to not be supported? */
	if (vt->flags & APC_IGNORE)
		return 0;

	/* empty the input buffer (while allowing the alert handler to run) */
	ret = ser_get_line_alert(upsfd, temp, sizeof(temp), ENDCHAR, 
		POLL_IGNORE, POLL_ALERT, alert_handler, 0, 0);

	ret = ser_send_char(upsfd, vt->cmd);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "query_ups: ser_send_char failed");
		return 0;
	}

	ret = ser_get_line_alert(upsfd, temp, sizeof(temp), ENDCHAR, 
		POLL_IGNORE, POLL_ALERT, alert_handler, SER_WAIT_SEC,
		SER_WAIT_USEC);

	if ((ret < 1) && (first == 0)) {
		ser_comm_fail("%s", ret ? strerror(errno) : "timeout");
		return 0;
	}

	ser_comm_good();

	if ((ret < 1) || (!strcmp(temp, "NA"))) {	/* not supported */
		vt->flags |= APC_IGNORE;
		return 0;
	}

	ptr = convert_data(vt, temp);
	dstate_setinfo(vt->name, "%s", ptr);

	return 1;	/* success */
}

static void do_capabilities(void)
{
	const	char	*ptr, *entptr;
	char	upsloc, temp[512], cmd, loc, etmp[16], *endtemp;
	int	nument, entlen, i, matrix, ret;
	struct	apc_vartab_t *vt;

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
	ret = ser_send_char(upsfd, APC_CAPABILITY);		/* ^Z */

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "do_capabilities: ser_send_char failed");
		return;
	}

	/* note different IGN set since ^Z returns things like # */
	ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, 
		MINIGNCHARS, SER_WAIT_SEC, SER_WAIT_USEC);

	if ((ret < 1) || (!strcmp(temp, "NA"))) {

		/* Early Smart-UPS, not as smart as later ones */
		/* This should never happen since we only call
		   this if the REQ_CAPABILITIES command is supported
		*/
		upslogx(LOG_ERR, "ERROR: APC cannot do capabilites but said it could!");
		return;
	}

	/* recv always puts a \0 at the end, so this is safe */
	/* however it assumes a zero byte cannot be embedded */
	endtemp = &temp[0] + strlen(temp);

	if (temp[0] != '#') {
		printf("Unrecognized capability start char %c\n", temp[0]);
		printf("Please report this error [%s]\n", temp);
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
			if (quirk_capability_overflow)
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

		/* mark this as writable */
		if (vt && ((loc == upsloc) || (loc == '4'))) {
			upsdebugx(1, "Supported capability: %02x (%c) - %s", 
				cmd, loc, vt->name);

			dstate_setflags(vt->name, ST_FLAG_RW);

			/* make sure setvar knows what this is */
			vt->flags |= APC_RW | APC_ENUM;
		}

		for (i = 0; i < nument; i++) {
			snprintf(etmp, entlen + 1, "%s", entptr);

			if (vt && ((loc == upsloc) || (loc == '4')))
				dstate_addenum(vt->name, "%s",
					convert_data(vt, etmp));

			entptr += entlen;
		}

		ptr = entptr;
	}
}

static int update_status(void)
{
	int	ret;
	char	buf[SMALLBUF];

	upsdebugx(4, "update_status");

	ser_flush_in(upsfd, IGNCHARS, nut_debug_level);

	ret = ser_send_char(upsfd, APC_STATUS);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "update_status: ser_send_char failed");
		dstate_datastale();
		return 0;
	}

	ret = read_buf(buf, sizeof(buf));

	if ((ret < 1) || (!strcmp(buf, "NA"))) {
		dstate_datastale();
		return 0;
	}

	ups_status = strtol(buf, 0, 16) & 0xff;
	ups_status_set();

	status_commit();
	dstate_dataok();

	return 1;
}

static void oldapcsetup(void)
{
	int	ret = 0;

	/* really old models ignore REQ_MODEL, so find them first */
	ret = query_ups("ups.model", 1);

	if (ret != 1) {
		/* force the model name */
		dstate_setinfo("ups.model", "Smart-UPS");
	}

	/* see if this might be an old Matrix-UPS instead */
	if (query_ups("output.current", 1))
		dstate_setinfo("ups.model", "Matrix-UPS");

	query_ups("ups.serial", 1);
	query_ups("input.voltage", 1); /* This one may fail... no problem */

	update_status();

	/* If we have come down this path then we dont do capabilities and
	   other shiny features
	*/
}

static void protocol_verify(unsigned char cmd)
{
	int	i, found;

	/* we might not care about this one */
	if (strchr(CMD_IGN_CHARS, cmd))
		return;

	/* see if it's a variable */
	for (i = 0; apc_vartab[i].name != NULL; i++) {

		/* 1:1 here, so the first match is the only match */

		if (apc_vartab[i].cmd == cmd) {
			upsdebugx(3, "UPS supports variable [%s]",
				apc_vartab[i].name);

			/* load initial data */
			apc_vartab[i].flags |= APC_PRESENT;
			poll_data(&apc_vartab[i]);

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

	/* check the command list */

	/* some cmdchars map onto multiple commands (start and stop) */

	found = 0;

	for (i = 0; apc_cmdtab[i].name != NULL; i++) {
		if (apc_cmdtab[i].cmd == cmd) {
			upsdebugx(2, "UPS supports command [%s]",
				apc_cmdtab[i].name);

			dstate_addcmd(apc_cmdtab[i].name);

			apc_cmdtab[i].flags |= APC_PRESENT;
			found = 1;
		}
	}

	if (found)
		return;

	if (isprint(cmd))
		upsdebugx(1, "protocol_verify: 0x%02x [%c] unrecognized", 
			cmd, cmd);
	else
		upsdebugx(1, "protocol_verify: 0x%02x unrecognized", cmd);
}

/* some hardware is a special case - hotwire the list of cmdchars */
static int firmware_table_lookup(void)
{
	int	ret;
	unsigned int	i, j;
	char	buf[SMALLBUF];

	upsdebugx(1, "Attempting firmware lookup");

	ret = ser_send_char(upsfd, 'b');

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "getbaseinfo: ser_send_char failed");
		return 0;
	}

	ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, IGNCHARS, 
		SER_WAIT_SEC, SER_WAIT_USEC);

	/* see if this is an older version like an APC600 which doesn't
	 * response to 'a' or 'b' queries
	 */
	if ((ret < 1) || (!strcmp(buf, "NA"))) {
		upsdebugx(1, "Attempting to contact older Smart-UPS version");
		ret = ser_send_char(upsfd, 'V');

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "getbaseinfo: ser_send_char failed");
			return 0;
		}

		ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, IGNCHARS,
			SER_WAIT_SEC, SER_WAIT_USEC);

		if (ret < 1) {
			upslog_with_errno(LOG_ERR, "firmware_table_lookup: ser_get_line failed");
			return 0;
		}

		upsdebugx(2, "Firmware: [%s]", buf);

		/* found one, force the model information */
		if (!strcmp(buf, "6QD") || /* (APC600.) */
				!strcmp(buf, "8QD") || /* (SmartUPS 1250, vintage 07/94.) */
				!strcmp(buf, "8TI") || /* (SmartUPS 1250, vintage 08/24/95.) */
				!strcmp(buf, "6TI") || /* (APC600.) */
				!strcmp(buf, "6QI") || /* (APC600.) */
				!strcmp(buf, "0ZI") || /* (APC Matrix 3000, vintage 10/99.) */
				!strcmp(buf, "5UI")) { /* (APC Matrix 5000, vintage 10/93.) */
			upsdebugx(1, "Found Smart-UPS");
			dstate_setinfo("ups.model", "Smart-UPS");
		}
		else return 0;
	}

	/* this will be reworked if we get a lot of these things */
	if (!strcmp(buf, "451.2.I")) {
		quirk_capability_overflow = 1;
		return 0;
	}

	for (i = 0; compat_tab[i].firmware != NULL; i++) {
		if (!strcmp(compat_tab[i].firmware, buf)) {

			upsdebugx(2, "Matched - cmdchars: %s",
				compat_tab[i].cmdchars);

			/* matched - run the cmdchars from the table */
			for (j = 0; j < strlen(compat_tab[i].cmdchars); j++)
				protocol_verify(compat_tab[i].cmdchars[j]);

			return 1;	/* matched */
		}
	}

	upsdebugx(2, "Not found in table - trying normal method");
	return 0;			
}

static void getbaseinfo(void)
{
	unsigned int	i;
	int	ret = 0;
	char 	*alrts, *cmds, temp[512];

	if (firmware_table_lookup() == 1)
		return;

	upsdebugx(1, "APC - Attempting to find command set");
	/* Initially we ask the UPS what commands it takes
	   If this fails we are going to need an alternate
	   strategy - we can deal with that if it happens
	*/

	ret = ser_send_char(upsfd, APC_CMDSET);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "getbaseinfo: ser_send_char failed");
		return;
	}

	ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, IGNCHARS, 
		SER_WAIT_SEC, SER_WAIT_USEC);

	if ((ret < 1) || (!strcmp(temp, "NA"))) {

		/* We have an old dumb UPS - go to specific code for old stuff */
		oldapcsetup();
		return;
	}

	upsdebugx(1, "APC - Parsing out command set");
	/* We have the version.alert.cmdchars string
	   NB the alert chars are normally in IGNCHARS
	   so will have been pretty much edited out.
	   You will need to change the ser_get_line above if
	   you want to check those out too....
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

	/* if capabilities are supported, add them here */
	if (strchr(cmds, APC_CAPABILITY))
		do_capabilities();

	upsdebugx(1, "APC - UPS capabilities determined");
}

/* check for calibration status and either start or stop */
static int do_cal(int start)
{
	char	temp[256];
	int	tval, ret;

	ret = ser_send_char(upsfd, APC_STATUS);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "do_cal: ser_send_char failed");
		return STAT_INSTCMD_HANDLED;		/* FUTURE: failure */
	}

	ret = read_buf(temp, sizeof(temp));

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

		ret = ser_send_char(upsfd, APC_CMD_CALTOGGLE);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "do_cal: ser_send_char failed");
			return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
		}

		ret = read_buf(temp, sizeof(temp));

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

	ret = ser_send_char(upsfd, APC_CMD_CALTOGGLE);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "do_cal: ser_send_char failed");
		return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
	}

	ret = read_buf(temp, sizeof(temp));

	if ((ret < 1) || (!strcmp(temp, "NA")) || (!strcmp(temp, "NO"))) {
		upslogx(LOG_WARNING, "Start calibration failed: %s", temp);
		return STAT_INSTCMD_HANDLED;	/* FUTURE: failure */
	}

	return STAT_INSTCMD_HANDLED;			/* FUTURE: success */
}

/* get the UPS talking to us in smart mode */
static int smartmode(void)
{
	int	ret, tries;
	char	temp[256];

	for (tries = 0; tries < 5; tries++) {

		ret = ser_send_char(upsfd, APC_GOSMART);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "smartmode: ser_send_char failed");
			return 0;
		}

		ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, 
			IGNCHARS, SER_WAIT_SEC, SER_WAIT_USEC);

		if (ret > 0)
			if (!strcmp(temp, "SM"))
				return 1;	/* success */

		sleep(1);	/* wait before trying again */

		/* it failed, so try to bail out of menus on newer units */

		ret = ser_send_char(upsfd, 27);	/* ESC */

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "smartmode: ser_send_char failed");
			return 0;
		}

		/* eat the response (might be NA, might be something else) */
		ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, 
			IGNCHARS, SER_WAIT_SEC, SER_WAIT_USEC);
	}

	return 0;	/* failure */
}

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{
	char	temp[32];
	int	ret, tval, sdtype = 0;

	if (!smartmode())
		printf("Detection failed.  Trying a shutdown command anyway.\n");

	/* check the line status */

	ret = ser_send_char(upsfd, APC_STATUS);

	if (ret == 1) {
		ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, 
			IGNCHARS, SER_WAIT_SEC, SER_WAIT_USEC);

		if (ret < 1) {
			printf("Status read failed!  Assuming on battery state\n");
			tval = APC_STAT_LB | APC_STAT_OB;
		} else {
			tval = strtol(temp, 0, 16);
		}

	} else {
		printf("Status request failed; assuming on battery state\n");
		tval = APC_STAT_LB | APC_STAT_OB;
	}

	if (testvar("sdtype"))
		sdtype = atoi(getval("sdtype"));

	switch (sdtype) {

	case 4:		/* special hack for CS 350 and similar models */
		printf("Using CS 350 'force OB' shutdown method\n");

		if (tval & APC_STAT_OL) {
			printf("On line - forcing OB temporarily\n");
			ser_send_char(upsfd, 'U');
		}

		ser_send_char(upsfd, 'S');
		break;

	case 3:		/* shutdown with grace period */
		printf("Sending delayed power off command to UPS\n");

		ser_send_char(upsfd, APC_CMD_SHUTDOWN);
		usleep(CMDLONGDELAY);
		ser_send_char(upsfd, APC_CMD_SHUTDOWN);

		break;

	case 2:		/* instant shutdown */
		printf("Sending power off command to UPS\n");

		ser_send_char(upsfd, APC_CMD_OFF);
		usleep(CMDLONGDELAY);
		ser_send_char(upsfd, APC_CMD_OFF);

		break;

	case 1:

		/* Send a combined set of shutdown commands which can work better */
		/* if the UPS gets power during shutdown process */
		/* Specifically it sends both the soft shutdown 'S' */
		/* and the powerdown after grace period - '@000' commands */
		printf("UPS - currently %s - sending shutdown/powerdown\n",
			(tval & APC_STAT_OL) ? "on-line" : "on battery");

		ser_flush_in(upsfd, IGNCHARS, nut_debug_level);
		ser_send_pace(upsfd, UPSDELAY, "S@000");
		break;

	default:

		/* @000 - shutdown after 'p' grace period             */
		/*      - returns after 000 minutes (i.e. right away) */

		/* S    - shutdown after 'p' grace period, only on battery */
		/*        returns after 'e' charge % plus 'r' seconds      */

		ser_flush_in(upsfd, IGNCHARS, nut_debug_level);

		if (tval & APC_STAT_OL) {		/* on line */
			printf("On line, sending shutdown+return command...\n");
			ser_send_pace(upsfd, UPSDELAY, "@000");
		}
		else {
			printf("On battery, sending normal shutdown command...\n");
			ser_send_char(upsfd, APC_CMD_SOFTDOWN);
		}
	}
}

/* 940-0095B support: set DTR, lower RTS */
static void init_serial_0095B(void)
{
	ser_set_dtr(upsfd, 1);
	ser_set_rts(upsfd, 0);
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

static int setvar_enum(struct apc_vartab_t *vt, const char *val)
{
	int	i, ret;
	char	orig[256], temp[256], *ptr;

	ret = ser_send_char(upsfd, vt->cmd);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "setvar_enum: ser_send_char failed");
		return STAT_SET_HANDLED;	/* FUTURE: failed */
	}

	ret = read_buf(orig, sizeof(orig));

	if ((ret < 1) || (!strcmp(orig, "NA")))
		return STAT_SET_HANDLED;	/* FUTURE: failed */

	ptr = convert_data(vt, orig);

	/* suppress redundant changes - easier on the eeprom */
	if (!strcmp(ptr, val)) {
		upslogx(LOG_INFO, "Ignoring enum SET %s='%s' (unchanged value)",
			vt->name, val);

		return STAT_SET_HANDLED;	/* FUTURE: no change */
	}

	for (i = 0; i < 6; i++) {
		ret = ser_send_char(upsfd, APC_NEXTVAL);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "setvar_enum: ser_send_char failed");
			return STAT_SET_HANDLED;	/* FUTURE: failed */
		}

		/* this should return either OK (if rotated) or NO (if not) */
		ret = read_buf(temp, sizeof(temp));

		if ((ret < 1) || (!strcmp(temp, "NA")))
			return STAT_SET_HANDLED;	/* FUTURE: failed */

		/* sanity checks */
		if (!strcmp(temp, "NO"))
			return STAT_SET_HANDLED;	/* FUTURE: failed */
		if (strcmp(temp, "OK") != 0)
			return STAT_SET_HANDLED;	/* FUTURE: failed */

		/* see what it rotated onto */
		ret = ser_send_char(upsfd, vt->cmd);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "setvar_enum: ser_send_char failed");
			return STAT_SET_HANDLED;	/* FUTURE: failed */
		}

		ret = read_buf(temp, sizeof(temp));

		if ((ret < 1) || (!strcmp(temp, "NA")))
			return STAT_SET_HANDLED;	/* FUTURE: failed */

		ptr = convert_data(vt, temp);

		upsdebugx(1, "Rotate value: got [%s], want [%s]",
			ptr, val);

		if (!strcmp(ptr, val)) {	/* got it */
			upslogx(LOG_INFO, "SET %s='%s'", vt->name, val);

			/* refresh data from the hardware */
			query_ups(vt->name, 0);

			return STAT_SET_HANDLED;	/* FUTURE: success */
		}

		/* check for wraparound */
		if (!strcmp(ptr, orig)) {
			upslogx(LOG_ERR, "setvar: variable %s wrapped",
				vt->name);

			return STAT_SET_HANDLED;	/* FUTURE: failed */
		}			
	}

	upslogx(LOG_ERR, "setvar: gave up after 6 tries for %s",
		vt->name);

	/* refresh data from the hardware */
	query_ups(vt->name, 0);

	return STAT_SET_HANDLED;
}

static int setvar_string(struct apc_vartab_t *vt, const char *val)
{
	unsigned int	i;
	int	ret;
	char	temp[256];

	ser_flush_in(upsfd, IGNCHARS, nut_debug_level);

	ret = ser_send_char(upsfd, vt->cmd);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "setvar_string: ser_send_char failed");
		return STAT_SET_HANDLED;	/* FUTURE: failed */
	}

	ret = read_buf(temp, sizeof(temp));

	if ((ret < 1) || (!strcmp(temp, "NA")))
		return STAT_SET_HANDLED;	/* FUTURE: failed */

	/* suppress redundant changes - easier on the eeprom */
	if (!strcmp(temp, val)) {
		upslogx(LOG_INFO, "Ignoring string SET %s='%s' (unchanged value)",
			vt->name, val);

		return STAT_SET_HANDLED;	/* FUTURE: no change */
	}

	ret = ser_send_char(upsfd, APC_NEXTVAL);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "setvar_string: ser_send_char failed");
		return STAT_SET_HANDLED;	/* FUTURE: failed */
	}

	usleep(UPSDELAY);

	for (i = 0; i < strlen(val); i++) {
		ret = ser_send_char(upsfd, val[i]);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "setvar_string: ser_send_char failed");
			return STAT_SET_HANDLED;	/* FUTURE: failed */
		}

		usleep(UPSDELAY);
	}

	/* pad to 8 chars with CRs */
	for (i = strlen(val); i < APC_STRLEN; i++) {
		ret = ser_send_char(upsfd, 13);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "setvar_string: ser_send_char failed");
			return STAT_SET_HANDLED;	/* FUTURE: failed */
		}

		usleep(UPSDELAY);
	}

	ret = read_buf(temp, sizeof(temp));

	if (ret < 1) {
		upslogx(LOG_ERR, "setvar_string: short final read");
		return STAT_SET_HANDLED;	/* FUTURE: failed */
	}

	if (!strcmp(temp, "NO")) {
		upslogx(LOG_ERR, "setvar_string: got NO at final read");
		return STAT_SET_HANDLED;	/* FUTURE: failed */
	}

	/* refresh data from the hardware */
	query_ups(vt->name, 0);

	upslogx(LOG_INFO, "SET %s='%s'", vt->name, val);

	return STAT_SET_HANDLED;	/* FUTURE: failed */
}

static int setvar(const char *varname, const char *val)
{
	struct	apc_vartab_t	*vt;

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

/* actually send the instcmd's char to the ups */
static int do_cmd(struct apc_cmdtab_t *ct)
{
	int	ret;
	char	buf[SMALLBUF];

	ret = ser_send_char(upsfd, ct->cmd);

	if (ret != 1) {
		upslog_with_errno(LOG_ERR, "do_cmd: ser_send_char failed");
		return STAT_INSTCMD_HANDLED;		/* FUTURE: failed */
	}

	/* some commands have to be sent twice with a 1.5s gap */
	if (ct->flags & APC_REPEAT) {
		usleep(CMDLONGDELAY);

		ret = ser_send_char(upsfd, ct->cmd);

		if (ret != 1) {
			upslog_with_errno(LOG_ERR, "do_cmd: ser_send_char failed");
			return STAT_INSTCMD_HANDLED;	/* FUTURE: failed */
		}
	}

	ret = read_buf(buf, sizeof(buf));

	if (ret < 1)
		return STAT_INSTCMD_HANDLED;		/* FUTURE: failed */

	if (strcmp(buf, "OK") != 0) {
		upslogx(LOG_WARNING, "Got [%s] after command [%s]",
			buf, ct->name);

		return STAT_INSTCMD_HANDLED;		/* FUTURE: failed */
	}

	upslogx(LOG_INFO, "Command: %s", ct->name);
	return STAT_INSTCMD_HANDLED;			/* FUTURE: success */
}

/* some commands must be repeated in a window to execute */
static int instcmd_chktime(struct apc_cmdtab_t *ct)
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
		return STAT_INSTCMD_HANDLED;		/* FUTURE: again */
	}

	return do_cmd(ct);
}

static int instcmd(const char *cmdname, const char *extra)
{
	int	i;
	struct	apc_cmdtab_t	*ct;

	ct = NULL;

	for (i = 0; apc_cmdtab[i].name != NULL; i++)
		if (!strcasecmp(apc_cmdtab[i].name, cmdname))
			ct = &apc_cmdtab[i];

	if (!ct) {
		upslogx(LOG_WARNING, "instcmd: unknown command [%s]", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}

	if ((ct->flags & APC_PRESENT) == 0) {
		upslogx(LOG_WARNING, "instcmd: command [%s] is not supported",
			cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}

	if (!strcasecmp(cmdname, "calibrate.start"))
		return do_cal(1);

	if (!strcasecmp(cmdname, "calibrate.stop"))
		return do_cal(0);

	if (ct->flags & APC_NASTY)
		return instcmd_chktime(ct);

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
	addvar(VAR_VALUE, "sdtype", "Specify shutdown type (1-3)");
}

void upsdrv_initups(void)
{
	char	*cable;

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);

	cable = getval("cable");

	if (cable)
		if (!strcasecmp(cable, ALT_CABLE_1))
			init_serial_0095B();

	/* make sure we wake up if the UPS sends alert chars to us */
	extrafd = upsfd;
}

void upsdrv_help(void)
{
	printf("\nShutdown types:\n");
	printf("  0: soft shutdown or powerdown, depending on battery status\n");
	printf("  1: soft shutdown followed by powerdown\n");
	printf("  2: instant power off\n");
	printf("  3: power off with grace period\n");
	printf("  4: 'force OB' hack method for CS 350\n");
	printf("Modes 0-1 will make the UPS come back when power returns\n");
	printf("Modes 2-3 will make the UPS stay turned off when power returns\n");
}

void upsdrv_initinfo(void)
{
	if (!smartmode()) {
		fatalx(EXIT_FAILURE, 
			"Unable to detect an APC Smart protocol UPS on port %s\n"
			"Check the cabling, port name or model name and try again", device_path
			);
	}

	/* manufacturer ID - hardcoded in this particular module */
	dstate_setinfo("ups.mfr", "APC");

	getbaseinfo();

	printf("Detected %s [%s] on %s\n", dstate_getinfo("ups.model"),
		dstate_getinfo("ups.serial"), device_path);

	setuphandlers();
}

void upsdrv_updateinfo(void)
{
	static	time_t	last_full = 0;
	time_t	now;

	/* try to wake up a dead ups once in awhile */
	if ((dstate_is_stale()) && (!smartmode())) {
		ser_comm_fail("Communications with UPS lost - check cabling");

		/* reset this so a full update runs when the UPS returns */
		last_full = 0;
		return;
	}

	ser_comm_good();

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

void upsdrv_cleanup(void)
{
	/* try to bring the UPS out of smart mode */
	ser_send_char(upsfd, APC_GODUMB);

	ser_close(upsfd, device_path);
}
