/* powerp-txt.c	Model specific routines for CyberPower text
			protocol UPSes 

   Copyright (C)
	2007		Doug Reynolds <mav@wastegate.net>
	2007-2008	Arjen de Korte <adkorte-guest@alioth.debian.org>

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

/*
   Throughout this driver, READ and WRITE comments are shown. These are
   the typical commands to and replies from the UPS that was used for
   decoding the protocol (with a serial logger).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "main.h"
#include "serial.h"

#include "powerpanel.h"
#include "powerp-txt.h"

typedef struct {
	float		i_volt;
	float		o_volt;
	int		o_load;
	int		b_chrg;
	int		u_temp;
	float		i_freq;
	unsigned char	flags[2];
} status_t;

static char	powpan_answer[SMALLBUF];

static const struct {
	char	*var;
	char	*get;
	char	*set;
} vartab[] = {
	{ "input.transfer.high", "P6\r", "C2:%03d\r" },
	{ "input.transfer.low", "P7\r", "C3:%03d\r" },
	{ "battery.charge.low", "P8\r", "C4:%02d\r" },
	{ NULL, NULL, NULL }
};

static const struct {
	char	*cmd;
	char	*command;
} cmdtab[] = {
	{ "test.failure.start", "T\r" },
	{ "test.failure.stop", "CT\r" },
	{ "beeper.enable", "C7:1\r" },
	{ "beeper.disable", "C7:0\r" },
	{ "beeper.on", NULL },
	{ "beeper.off", NULL },
	{ "shutdown.reboot", "S01R0001\r" },
	{ "shutdown.return", "Z02\r" },
	{ "shutdown.stop", "C\r" },
	{ "shutdown.stayoff", "S01\r" },
	{ NULL, NULL }
};

static int powpan_command(const char *command)
{
	int	ret;

	ser_flush_io(upsfd);

	upsdebug_hex(3, "send", (unsigned char *)command, strlen(command));

	ret = ser_send_pace(upsfd, UPSDELAY, command);

	if (ret < (int)strlen(command)) {
		return -1;
	}

	usleep(100000);

	ret = ser_get_line(upsfd, powpan_answer, sizeof(powpan_answer),
		ENDCHAR, IGNCHAR, SER_WAIT_SEC, SER_WAIT_USEC);

	if (ret < 1) {
		upsdebugx(3, "read: timed out");
		return ret;
	}

	upsdebug_hex(3, "read", (unsigned char *)powpan_answer, ret);
	return ret;
}

static int powpan_instcmd(const char *cmdname, const char *extra)
{
	int	i;

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
	
		if (powpan_command(cmdtab[i].command) > 0) {
			return STAT_INSTCMD_HANDLED;
		}

		upslogx(LOG_ERR, "instcmd: command [%s] failed", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}

	upslogx(LOG_NOTICE, "instcmd: command [%s] unknown", cmdname);
	return STAT_INSTCMD_UNKNOWN;
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
			upslogx(LOG_INFO, "setvar: [%s] no change for variable [%s]", val, varname);
			return STAT_SET_HANDLED;
		}

		snprintf(command, sizeof(command), vartab[i].set, atoi(val));

		if ((powpan_command(command) == 2) && (!strcasecmp(powpan_answer, "#0"))) {
			dstate_setinfo(varname, val);
			return STAT_SET_HANDLED;
		}

		upslogx(LOG_ERR, "setvar: setting variable [%s] to [%s] failed", varname, val);
		return STAT_SET_UNKNOWN;
	}

	upslogx(LOG_ERR, "setvar: variable [%s] not found", varname);
	return STAT_SET_UNKNOWN;
}

static void powpan_initinfo()
{
	int	i;
	char	*s;

	/*
	 * NOTE: The reply is already in the buffer, since the P4\r command
	 * was used for autodetection of the UPS. No need to do it again.
	 */
	if ((s = strtok(&powpan_answer[1], ",")) != NULL) {
		dstate_setinfo("ups.model", "%s", rtrim(s, ' '));
	}
	if ((s = strtok(NULL, ",")) != NULL) {
		dstate_setinfo("ups.firmware", s);
	}
	if ((s = strtok(NULL, ",")) != NULL) {
		dstate_setinfo("ups.serial", s);
	}
	if ((s = strtok(NULL, ",")) != NULL) {
		dstate_setinfo("ups.mfr", "%s", rtrim(s, ' '));
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

	upsh.instcmd = powpan_instcmd;
	upsh.setvar = powpan_setvar;
}

static int powpan_status(status_t *status)
{
	int	ret;

	ser_flush_io(upsfd);

	/*
	 * WRITE D\r
	 * READ #I119.0O119.0L000B100T027F060.0S..\r
	 *      01234567890123456789012345678901234
	 */
	upsdebug_hex(3, "send", (unsigned char *)"D\r", 2);

	ret = ser_send_pace(upsfd, UPSDELAY, "D\r");
	if (ret < 2) {
		return -1;
	}

	usleep(200000);

	ret = ser_get_buf_len(upsfd, (unsigned char *)powpan_answer, 35, SER_WAIT_SEC, SER_WAIT_USEC);
	if (ret < 1) {
		upsdebugx(3, "read: timed out");
		return -1;
	}

	upsdebug_hex(3, "read", (unsigned char *)powpan_answer, ret);

	ret = sscanf(powpan_answer, "#I%fO%fL%dB%dT%dF%fS%2c\r",
		&status->i_volt, &status->o_volt, &status->o_load,
		&status->b_chrg, &status->u_temp, &status->i_freq,
		status->flags);

	if (ret < 7) {
		upsdebugx(4, "Parsing status string failed");
		return -1;
	}

	return 0;
}

static void powpan_updateinfo()
{
	status_t	status;

	if (powpan_status(&status)) {
		ser_comm_fail("Status read failed!");
		dstate_datastale();
		return;
	}

	dstate_setinfo("input.voltage", "%.1f", status.i_volt);
	dstate_setinfo("output.voltage", "%.1f", status.o_volt);
	dstate_setinfo("ups.load", "%d", status.o_load);
	dstate_setinfo("input.frequency", "%.1f", status.i_freq);
	dstate_setinfo("ups.temperature", "%d", status.u_temp);
	dstate_setinfo("battery.charge", "%d", status.b_chrg);

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
		if ((status.i_volt > 0.50 * status.o_volt) && (status.i_volt < 0.98 * status.o_volt)) {
			status_set("BOOST");
		}

		if ((status.o_volt > 0.50 * status.i_volt) && (status.o_volt < 0.98 * status.i_volt)) {
			status_set("TRIM");
		}
	}

	if (status.flags[0] & 0x08) {
		status_set("TEST");
	}

	if (status.flags[0] == 0) {
		status_set("OFF");
	}

	status_commit();

	ser_comm_good();
	dstate_dataok();

	return;
}

static void powpan_shutdown()
{
	int	i;

	for (i = 0; i < MAXTRIES; i++) {

		if (powpan_instcmd("shutdown.return", NULL) == STAT_INSTCMD_HANDLED) {
			upslogx(LOG_INFO, "Waiting for power to return...");
			return;
		}
	}

	upslogx(LOG_ERR, "Shutdown command failed!");
}

static int powpan_initups()
{
	int	ret, i;

	upsdebugx(1, "Trying text protocol...");

	ser_set_speed(upsfd, device_path, B2400);

	powpan_command("\r\r");

	for (i = 0; i < MAXTRIES; i++) {

		/*
		 * WRITE P4\r
		 * READ #BC1200     ,1.600,000000000000,CYBER POWER    
		 *      01234567890123456789012345678901234567890123456
		 */
		ret = powpan_command("P4\r");
		if (ret < 1) {
			upsdebugx(3, "read: timed out");
			continue;
		}
		
		upsdebug_hex(3, "read", (unsigned char *)powpan_answer, ret);

		if (ret < 46) {
			upsdebugx(2, "Expected 46 bytes, but only got %d", ret);
			continue;
		}

		if (powpan_answer[0] != '#') {
			upsdebugx(2, "Expected start character '#', but got '%c'", powpan_answer[0]);
			continue;
		}

		return ret;
	}

	return -1;
}

subdriver_t powpan_text = {
	"text",
	powpan_initups,
	powpan_initinfo,
	powpan_updateinfo,
	powpan_shutdown
};
