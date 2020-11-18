/*
 * powerp-txt.c - Model specific routines for CyberPower text
 *                protocol UPSes
 *
 * Copyright (C)
 *	2007        Doug Reynolds <mav@wastegate.net>
 *	2007-2008   Arjen de Korte <adkorte-guest@alioth.debian.org>
 *	2012-2016   Timothy Pearson <kb9vqf@pearsoncomputing.net>
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

/*
 * Throughout this driver, READ and WRITE comments are shown. These are
 * the typical commands to and replies from the UPS that was used for
 * decoding the protocol (with a serial logger).
 */

#include "main.h"
#include "serial.h"

#include "powerp-txt.h"

typedef struct {
	float          i_volt;
	float          o_volt;
	int            o_load;
	int            b_chrg;
	int            u_temp;
	float          i_freq;
	unsigned char  flags[2];
	unsigned char  has_b_volt;
	float          b_volt;
	unsigned char  has_o_freq;
	float          o_freq;
	unsigned char  has_runtime;
	int            runtime;
	int            c_unknwn;
	float          q_unknwn;
} status_t;

static int	ondelay = 1;	/* minutes */
static int	offdelay = 60;	/* seconds */

static char	powpan_answer[SMALLBUF];

static struct {
	const char	*var;
	const char	*get;
	const char	*set;
} vartab[] = {
	{ "input.transfer.high", "P6\r", "C2:%03d\r" },
	{ "input.transfer.low", "P7\r", "C3:%03d\r" },
	{ "battery.charge.low", "P8\r", "C4:%02d\r" },
	{ NULL }
};

static struct {
	const char	*cmd;
	const char	*command;
} cmdtab[] = {
	{ "test.battery.start.quick", "T\r" },
	{ "test.battery.start.deep", "TL\r" },
	{ "test.battery.stop", "CT\r" },
	{ "beeper.enable", "C7:1\r" },
	{ "beeper.disable", "C7:0\r" },
	{ "beeper.on", NULL },
	{ "beeper.off", NULL },
	{ "shutdown.stop", "C\r" },
	{ NULL }
};

static int powpan_command(const char *command)
{
	int	ret;

	ser_flush_io(upsfd);

	ret = ser_send_pace(upsfd, UPSDELAY, "%s", command);

	if (ret < 0) {
		upsdebug_with_errno(3, "send");
		return -1;
	}

	if (ret == 0) {
		upsdebug_with_errno(3, "send: timeout");
		return -1;
	}

	upsdebug_hex(3, "send", command, strlen(command));

	usleep(100000);

	ret = ser_get_line(upsfd, powpan_answer, sizeof(powpan_answer),
		ENDCHAR, IGNCHAR, SER_WAIT_SEC, SER_WAIT_USEC);

	if (ret < 0) {
		upsdebug_with_errno(3, "read");
		upsdebug_hex(4, "  \\_", powpan_answer, strlen(powpan_answer));
		return -1;
	}

	if (ret == 0) {
		upsdebugx(3, "read: timeout");
		upsdebug_hex(4, "  \\_", powpan_answer, strlen(powpan_answer));
		return -1;
	}

	upsdebug_hex(3, "read", powpan_answer, ret);
	return ret;
}

static int powpan_instcmd(const char *cmdname, const char *extra)
{
	int	i;
	char	command[SMALLBUF];

	if (!strcasecmp(cmdname, "beeper.off")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.off' command has been renamed to 'beeper.disable'");
		return powpan_instcmd("beeper.disable", NULL);
	}

	if (!strcasecmp(cmdname, "beeper.on")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.on' command has been renamed to 'beeper.enable'");
		return powpan_instcmd("beeper.enable", NULL);
	}

	for (i = 0; cmdtab[i].cmd != NULL; i++) {

		if (strcasecmp(cmdname, cmdtab[i].cmd)) {
			continue;
		}

		if ((powpan_command(cmdtab[i].command) == 2) && (!strcasecmp(powpan_answer, "#0"))) {
			return STAT_INSTCMD_HANDLED;
		}

		upslogx(LOG_ERR, "%s: command [%s] [%s] failed", __func__, cmdname, extra);
		return STAT_INSTCMD_FAILED;
	}

	if (!strcasecmp(cmdname, "shutdown.return")) {
		if (offdelay < 60) {
			snprintf(command, sizeof(command), "Z.%d\r", offdelay / 6);
		} else {
			snprintf(command, sizeof(command), "Z%02d\r", offdelay / 60);
		}
	} else if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		if (offdelay < 60) {
			snprintf(command, sizeof(command), "S.%d\r", offdelay / 6);
		} else {
			snprintf(command, sizeof(command), "S%02d\r", offdelay / 60);
		}
	} else if (!strcasecmp(cmdname, "shutdown.reboot")) {
		if (offdelay < 60) {
			snprintf(command, sizeof(command), "S.%dR%04d\r", offdelay / 6, ondelay);
		} else {
			snprintf(command, sizeof(command), "S%02dR%04d\r", offdelay / 60, ondelay);
		}
	} else {
		upslogx(LOG_NOTICE, "%s: command [%s] [%s] unknown", __func__, cmdname, extra);
		return STAT_INSTCMD_UNKNOWN;
	}

	if ((powpan_command(command) == 2) && (!strcasecmp(powpan_answer, "#0"))) {
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_ERR, "%s: command [%s] [%s] failed", __func__, cmdname, extra);
	return STAT_INSTCMD_FAILED;
}

static int powpan_setvar(const char *varname, const char *val)
{
	char	command[SMALLBUF];
	int 	i;

	for (i = 0;  vartab[i].var != NULL; i++) {

		if (strcasecmp(varname, vartab[i].var)) {
			continue;
		}

		if (!strcasecmp(val, dstate_getinfo(varname))) {
			upslogx(LOG_INFO, "%s: [%s] no change for variable [%s]", __func__, val, varname);
			return STAT_SET_HANDLED;
		}

		snprintf(command, sizeof(command), vartab[i].set, atoi(val));

		if ((powpan_command(command) == 2) && (!strcasecmp(powpan_answer, "#0"))) {
			dstate_setinfo(varname, "%s", val);
			return STAT_SET_HANDLED;
		}

		upslogx(LOG_ERR, "%s: setting variable [%s] to [%s] failed", __func__, varname, val);
		return STAT_SET_UNKNOWN;
	}

	upslogx(LOG_ERR, "%s: variable [%s] not found", __func__, varname);
	return STAT_SET_UNKNOWN;
}

static void powpan_initinfo(void)
{
	int	i;
	char	*s;

	dstate_setinfo("ups.delay.start", "%d", 60 * ondelay);
	dstate_setinfo("ups.delay.shutdown", "%d", offdelay);

	/*
	 * NOTE: The reply is already in the buffer, since the P4\r command
	 * was used for autodetection of the UPS. No need to do it again.
	 */
	if ((s = strtok(&powpan_answer[1], ",")) != NULL) {
		dstate_setinfo("ups.model", "%s", str_rtrim(s, ' '));
	}
	if ((s = strtok(NULL, ",")) != NULL) {
		dstate_setinfo("ups.firmware", "%s", s);
	}
	if ((s = strtok(NULL, ",")) != NULL) {
		dstate_setinfo("ups.serial", "%s", s);
	}
	if ((s = strtok(NULL, ",")) != NULL) {
		dstate_setinfo("ups.mfr", "%s", str_rtrim(s, ' '));
	}

	/*
	 * WRITE P3\r
	 * READ #12.0,002,008.0,00\r
	 */
	if (powpan_command("P3\r") > 0) {

		if ((s = strtok(&powpan_answer[1], ",")) != NULL) {
			dstate_setinfo("battery.voltage.nominal", "%g", strtod(s, NULL));
		}
		if ((s = strtok(NULL, ",")) != NULL) {
			dstate_setinfo("battery.packs", "%li", strtol(s, NULL, 10));
		}
		if ((s = strtok(NULL, ",")) != NULL) {
			dstate_setinfo("battery.capacity", "%g", strtod(s, NULL));
		}
	}

	/*
	 * WRITE P2\r
	 * READ #1200,0720,120,47,63\r
	 */
	if (powpan_command("P2\r") > 0) {

		if ((s = strtok(&powpan_answer[1], ",")) != NULL) {
			dstate_setinfo("ups.power.nominal", "%li", strtol(s, NULL, 10));
		}
		if ((s = strtok(NULL, ",")) != NULL) {
			dstate_setinfo("ups.realpower.nominal", "%li", strtol(s, NULL, 10));
		}
		if ((s = strtok(NULL, ",")) != NULL) {
			dstate_setinfo("input.voltage.nominal", "%li", strtol(s, NULL, 10));
		}
		if ((s = strtok(NULL, ",")) != NULL) {
			dstate_setinfo("input.frequency.low", "%li", strtol(s, NULL, 10));
		}
		if ((s = strtok(NULL, ",")) != NULL) {
			dstate_setinfo("input.frequency.high", "%li", strtol(s, NULL, 10));
		}
	}

	/*
	 * WRITE P1\r
	 * READ #120,138,088,20\r
	 */
	if (powpan_command("P1\r") > 0) {

		if ((s = strtok(&powpan_answer[1], ",")) != NULL) {
			dstate_setinfo("input.voltage.nominal", "%li", strtol(s, NULL, 10));
		}
		if ((s = strtok(NULL, ",")) != NULL) {
			dstate_setinfo("input.transfer.high", "%li", strtol(s, NULL, 10));
		}
		if ((s = strtok(NULL, ",")) != NULL) {
			dstate_setinfo("input.transfer.low", "%li", strtol(s, NULL, 10));
		}
		if ((s = strtok(NULL, ",")) != NULL) {
			dstate_setinfo("battery.charge.low", "%li", strtol(s, NULL, 10));
		}
	}

	for (i = 0; cmdtab[i].cmd != NULL; i++) {
		dstate_addcmd(cmdtab[i].cmd);
	}

	for (i = 0; vartab[i].var != NULL; i++) {

		if (!dstate_getinfo(vartab[i].var)) {
			continue;
		}

		if (powpan_command(vartab[i].get) < 1) {
			continue;
		}

		if ((s = strtok(&powpan_answer[1], ",")) != NULL) {
			dstate_setflags(vartab[i].var, ST_FLAG_RW);
			dstate_addenum(vartab[i].var, "%li", strtol(s, NULL, 10));
		}

		while ((s = strtok(NULL, ",")) != NULL) {
			dstate_addenum(vartab[i].var, "%li", strtol(s, NULL, 10));
		}
	}

	/*
	 * WRITE P5\r
	 * READ #<unknown>\r
	 */
	if (powpan_command("P5\r") > 0) {
		/*
		 * Looking at the format of the commands "P<n>\r" it seems likely
		 * that this command exists also. Let's see if someone cares to
		 * tell us if it does (should be visible when running with -DDDDD).
		 */
	}

	/*
	 * WRITE P9\r
	 * READ #<unknown>\r
	 */
	if (powpan_command("P9\r") > 0) {
		/*
		 * Looking at the format of the commands "P<n>\r" it seems likely
		 * that this command exists also. Let's see if someone cares to
		 * tell us if it does (should be visible when running with -DDDDD).
		 */
	}

	/*
	 * Cancel pending shutdown.
	 * WRITE C\r
	 * READ #0\r
	*/
	powpan_command("C\r");

	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("shutdown.reboot");
}

static int powpan_status(status_t *status)
{
	int	ret;

	ser_flush_io(upsfd);

	/*
	 * WRITE D\r
	 * READ #I119.0O119.0L000B100T027F060.0S..\r
	 *      01234567890123456789012345678901234
	 *      0         1         2         3
	 */
	ret = ser_send_pace(upsfd, UPSDELAY, "D\r");

	if (ret < 0) {
		upsdebug_with_errno(3, "send");
		return -1;
	}

	if (ret == 0) {
		upsdebug_with_errno(3, "send: timeout");
		return -1;
	}

	upsdebug_hex(3, "send", "D\r", 2);

	usleep(200000);

	ret = ser_get_buf_len(upsfd, powpan_answer, 35, SER_WAIT_SEC, SER_WAIT_USEC);

	if (ret < 0) {
		upsdebug_with_errno(3, "read");
		upsdebug_hex(4, "  \\_", powpan_answer, 35);
		return -1;
	}

	if (ret == 0) {
		upsdebugx(3, "read: timeout");
		upsdebug_hex(4, "  \\_", powpan_answer, 35);
		return -1;
	}

	upsdebug_hex(3, "read", powpan_answer, ret);

	ret = sscanf(powpan_answer, "#I%fO%fL%dB%dT%dF%fS%2c\r",
		&status->i_volt, &status->o_volt, &status->o_load,
		&status->b_chrg, &status->u_temp, &status->i_freq,
		status->flags);

	if (ret >= 7) {
		status->has_b_volt = 0;
		status->has_o_freq = 0;
		status->has_runtime = 0;
	}
	else {
		ret = ser_get_buf_len(upsfd, powpan_answer+35, 23, SER_WAIT_SEC, SER_WAIT_USEC);

		if (ret < 0) {
			upsdebug_with_errno(3, "read");
			upsdebug_hex(4, "  \\_", powpan_answer+35, 23);
			return -1;
		}

		if (ret == 0) {
			upsdebugx(3, "read: timeout");
			upsdebug_hex(4, "  \\_", powpan_answer+35, 23);
			return -1;
		}

		upsdebug_hex(3, "read", powpan_answer, ret);

		ret = sscanf(powpan_answer, "#I%fO%fL%dB%dV%fT%dF%fH%fR%dC%dQ%fS%2c\r",
		&status->i_volt, &status->o_volt, &status->o_load,
		&status->b_chrg, &status->b_volt, &status->u_temp,
		&status->i_freq, &status->o_freq, &status->runtime,
		&status->c_unknwn, &status->q_unknwn, status->flags);
		status->has_b_volt = 1;
		status->has_o_freq = 1;
		status->has_runtime = 1;
		dstate_setinfo("battery.voltage.nominal", "%g", 72.0);
		dstate_setinfo("output.voltage.nominal", "%g", 120.0);
	}

	if (ret < 7) {
		upsdebugx(4, "Parsing status string failed");
		return -1;
	}

	return 0;
}

static int powpan_updateinfo(void)
{
	status_t	status;

	if (powpan_status(&status)) {
		return -1;
	}

	dstate_setinfo("input.voltage", "%.1f", status.i_volt);
	dstate_setinfo("output.voltage", "%.1f", status.o_volt);
	dstate_setinfo("ups.load", "%d", status.o_load);
	dstate_setinfo("input.frequency", "%.1f", status.i_freq);
	dstate_setinfo("ups.temperature", "%d", status.u_temp);
	dstate_setinfo("battery.charge", "%d", status.b_chrg);
	if (status.has_b_volt) {
		dstate_setinfo("battery.voltage", "%.1f", status.b_volt);
	}
	if (status.has_o_freq) {
		dstate_setinfo("output.frequency", "%.1f", status.o_freq);
	}
	if (status.has_runtime) {
		dstate_setinfo("battery.runtime", "%d", status.runtime*60);
	}

	status_init();

	if (status.flags[0] & 0x40) {
		status_set("OB");
	} else {
		status_set("OL");
	}

	if (status.flags[0] & 0x20) {
		status_set("LB");
	}

	/* !OB && !TEST */
	if (!(status.flags[0] & 0x48)) {

		if (status.o_volt < 0.5 * status.i_volt) {
			upsdebugx(2, "%s: output voltage too low", __func__);
		} else if (status.o_volt < 0.95 * status.i_volt) {
			status_set("TRIM");
		} else if (status.o_volt < 1.05 * status.i_volt) {
			/* ignore */
		} else if (status.o_volt < 1.5 * status.i_volt) {
			status_set("BOOST");
		} else {
			upsdebugx(2, "%s: output voltage too high", __func__);
		}
	}

	if (status.flags[0] & 0x08) {
		status_set("TEST");
	}

	if (status.flags[0] == 0) {
		status_set("OFF");
	}

	status_commit();

	return (status.flags[0] & 0x40) ? 1 : 0;
}

static int powpan_initups(void)
{
	int	ret, i;

	upsdebugx(1, "Trying text protocol...");

	ser_set_speed(upsfd, device_path, B2400);

	/* This fails for many devices, so don't bother to complain */
	powpan_command("\r\r");

	for (i = 0; i < MAXTRIES; i++) {

		const char	*val;

		/*
		 * WRITE P4\r
		 * READ #BC1200     ,1.600,000000000000,CYBER POWER
		 *      01234567890123456789012345678901234567890123456
		 *      0         1         2         3         4
		 */
		ret = powpan_command("P4\r");

		if (ret < 1) {
			continue;
		}

		if (ret < 46) {
			upsdebugx(2, "Expected 46 bytes, but only got %d", ret);
			continue;
		}

		if (powpan_answer[0] != '#') {
			upsdebugx(2, "Expected start character '#', but got '%c'", powpan_answer[0]);
			continue;
		}

		val = getval("ondelay");
		if (val) {
			ondelay = strtol(val, NULL, 10);
		}

		if ((ondelay < 0) || (ondelay > 9999)) {
			fatalx(EXIT_FAILURE, "Start delay '%d' out of range [0..9999]", ondelay);
		}

		val = getval("offdelay");
		if (val) {
			offdelay = strtol(val, NULL, 10);
		}

		if ((offdelay < 6) || (offdelay > 600)) {
			fatalx(EXIT_FAILURE, "Shutdown delay '%d' out of range [6..600]", offdelay);
		}

		/* Truncate to nearest setable value */
		if (offdelay < 60) {
			offdelay -= (offdelay % 6);
		} else {
			offdelay -= (offdelay % 60);
		}

		return ret;
	}

	return -1;
}

subdriver_t powpan_text = {
	"text",
	powpan_instcmd,
	powpan_setvar,
	powpan_initups,
	powpan_initinfo,
	powpan_updateinfo
};
