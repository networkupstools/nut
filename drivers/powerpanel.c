/* powerpanel.c	Model specific routines for CyberPower text/binary
			protocol UPSes 

   Copyright (C) 2007  Arjen de Korte <arjen@de-korte.org>
                       Doug Reynolds <mav@wastegate.net>

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

static unsigned char	powpan_answer[SMALLBUF];
static int		mode = 0;

static int powpan_command_bin(const char *buf, size_t bufsize)
{
	int	ret;

	upsdebug_hex(3, "send", (unsigned char *)buf, bufsize);

	ser_flush_io(upsfd);

	ret = ser_send_buf_pace(upsfd, UPSDELAY, (unsigned char *)buf, bufsize);

	if (ret < (int)bufsize) {
		return -1;
	}

	usleep(100000);

	ret = ser_get_buf_len(upsfd, powpan_answer, bufsize - 1, SER_WAIT_SEC, SER_WAIT_USEC);

	if (ret > 0) {
		upsdebug_hex(3, "read", powpan_answer, ret);
	} else {
		upsdebugx(3, "read: timed out");
	}

	return ret;
}

static int powpan_command_txt(const char *command)
{
	int	ret;

	upsdebug_hex(3, "send", (unsigned char *)command, strlen(command));

	ser_flush_io(upsfd);

	ret = ser_send_pace(upsfd, UPSDELAY, command);

	if (ret < (int)strlen(command)) {
		return -1;
	}

	usleep(100000);

	ret = ser_get_line(upsfd, (char *)powpan_answer, sizeof(powpan_answer),
		ENDCHAR, IGNCHAR, SER_WAIT_SEC, SER_WAIT_USEC);

	if (ret > 0) {
		upsdebug_hex(3, "read", powpan_answer, ret);
	} else {
		upsdebugx(3, "read: timed out");
	}

	return ret;
}

static int instcmd_bin(const char *cmdname, const char *extra)
{
	int	i;

	for (i = 0; powpan_cmdtab_bin[i].cmd != NULL; i++) {

		if (strcasecmp(cmdname, powpan_cmdtab_bin[i].cmd)) {
			continue;
		}

		if ((powpan_command_bin(powpan_cmdtab_bin[i].command, powpan_cmdtab_bin[i].len) ==
				powpan_cmdtab_bin[i].len - 1) &&
				(!memcmp(powpan_answer, powpan_cmdtab_bin[i].command, powpan_cmdtab_bin[i].len - 1))) {
			return STAT_INSTCMD_HANDLED;
		}

		upslogx(LOG_ERR, "instcmd: command [%s] failed", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}

	upslogx(LOG_NOTICE, "instcmd: command [%s] unknown", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static int instcmd_txt(const char *cmdname, const char *extra)
{
	int	i;

	for (i = 0; powpan_cmdtab_txt[i].cmd != NULL; i++) {

		if (strcasecmp(cmdname, powpan_cmdtab_txt[i].cmd)) {
			continue;
		}
	
		if (powpan_command_txt(powpan_cmdtab_txt[i].command) > 0) {
			return STAT_INSTCMD_HANDLED;
		}

		upslogx(LOG_ERR, "instcmd: command [%s] failed", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}

	upslogx(LOG_NOTICE, "instcmd: command [%s] unknown", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static int setvar_bin(const char *varname, const char *val)
{
	char	command[SMALLBUF];
	int 	i, j;

	for (i = 0;  powpan_vartab_bin[i].var != NULL; i++) {

		if (strcasecmp(varname, powpan_vartab_bin[i].var)) {
			continue;
		}

		if (!strcasecmp(val, dstate_getinfo(varname))) {
			upslogx(LOG_INFO, "setvar: [%s] no change for variable [%s]", val, varname);
			return STAT_SET_HANDLED;
		}

		for (j = 0; powpan_vartab_bin[i].map[j].val != NULL; j++) {

			if (strcasecmp(val, powpan_vartab_bin[i].map[j].val)) {
				continue;
			}

			snprintf(command, sizeof(command), powpan_vartab_bin[i].set,
				powpan_vartab_bin[i].map[j].command);

			if ((powpan_command_bin(command, 4) == 3) && (!memcmp(powpan_answer, command, 3))) {
				dstate_setinfo(varname, val);
				return STAT_SET_HANDLED;
			}

			upslogx(LOG_ERR, "setvar: setting variable [%s] to [%s] failed", varname, val);
			return STAT_SET_UNKNOWN;
		}

		upslogx(LOG_ERR, "setvar: [%s] is not valid for variable [%s]", val, varname);
		return STAT_SET_UNKNOWN;
	}

	upslogx(LOG_ERR, "setvar: variable [%s] not found", varname);
	return STAT_SET_UNKNOWN;
}

static int setvar_txt(const char *varname, const char *val)
{
	char	command[SMALLBUF];
	int 	i;

	for (i = 0;  powpan_vartab_txt[i].var != NULL; i++) {

		if (strcasecmp(varname, powpan_vartab_txt[i].var)) {
			continue;
		}

		if (!strcasecmp(val, dstate_getinfo(varname))) {
			upslogx(LOG_INFO, "setvar: [%s] no change for variable [%s]", val, varname);
			return STAT_SET_HANDLED;
		}

		snprintf(command, sizeof(command), powpan_vartab_txt[i].set, atoi(val));

		if ((powpan_command_txt(command) == 2) && (!strcasecmp((char *)powpan_answer, "#0"))) {
			dstate_setinfo(varname, val);
			return STAT_SET_HANDLED;
		}

		upslogx(LOG_ERR, "setvar: setting variable [%s] to [%s] failed", varname, val);
		return STAT_SET_UNKNOWN;
	}

	upslogx(LOG_ERR, "setvar: variable [%s] not found", varname);
	return STAT_SET_UNKNOWN;
}

static void initinfo_bin()
{
	int	i, j;
	char	*s;

	/*
	 * NOTE: The reply is already in the buffer, since the F\r command
	 * was used for autodetection of the UPS. No need to do it again.
	 */
	if ((s = strtok((char *)&powpan_answer[1], ".")) != NULL) {
		dstate_setinfo("ups.model", "%s", rtrim(s, ' '));
	}
	if ((s = strtok(NULL, ".")) != NULL) {
		dstate_setinfo("input.voltage.nominal", "%d", (unsigned char)s[0]);
	}
	if ((s = strtok(NULL, ".")) != NULL) {
		dstate_setinfo("input.frequency.nominal", "%d", (unsigned char)s[0]);
	}
	if ((s = strtok(NULL, ".")) != NULL) {
		dstate_setinfo("ups.firmware", "%c.%c%c%c", s[0], s[1], s[2], s[3]);
	}

	for (i = 0; powpan_cmdtab_bin[i].cmd != NULL; i++) {
		dstate_addcmd(powpan_cmdtab_bin[i].cmd);
	}

	for (i = 0; powpan_vartab_bin[i].var != NULL; i++) {
		
		if (powpan_command_bin(powpan_vartab_bin[i].get, 3) < 2) {
			continue;
		}

		for (j = 0; powpan_vartab_bin[i].map[j].val != NULL; j++) {

			if (powpan_vartab_bin[i].map[j].command != powpan_answer[1]) {
				continue;
			}

			dstate_setinfo(powpan_vartab_bin[i].var, powpan_vartab_bin[i].map[j].val);
			break;
		}
	
		if (dstate_getinfo(powpan_vartab_bin[i].var) == NULL) {
			upslogx(LOG_WARNING, "warning: [%d] unknown value for [%s]!",
				powpan_answer[1], powpan_vartab_bin[i].var);
			continue;
		}

		dstate_setflags(powpan_vartab_bin[i].var, ST_FLAG_RW);

		for (j = 0; powpan_vartab_bin[i].map[j].val != 0; j++) {
			dstate_addenum(powpan_vartab_bin[i].var, powpan_vartab_bin[i].map[j].val);
		}
	}

	upsh.instcmd = instcmd_bin;
	upsh.setvar = setvar_bin;
}

static void initinfo_txt()
{
	int	i;
	char	*s;

	/*
	 * NOTE: The reply is already in the buffer, since the P4\r command
	 * was used for autodetection of the UPS. No need to do it again.
	 */
	if ((s = strtok((char *)&powpan_answer[1], ",")) != NULL) {
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
	if (powpan_command_txt("P3\r") > 0) {

		if ((s = strtok((char *)&powpan_answer[1], ",")) != NULL) {
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
	if (powpan_command_txt("P2\r") > 0) {

		if ((s = strtok((char *)&powpan_answer[1], ",")) != NULL) {
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
	if (powpan_command_txt("P1\r") > 0) {

		if ((s = strtok((char *)&powpan_answer[1], ",")) != NULL) {
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

	for (i = 0; powpan_cmdtab_txt[i].cmd != NULL; i++) {
		dstate_addcmd(powpan_cmdtab_txt[i].cmd);
	}

	for (i = 0; powpan_vartab_txt[i].var != NULL; i++) {

		if (!dstate_getinfo(powpan_vartab_txt[i].var)) {
			continue;
		}

		if (powpan_command_txt(powpan_vartab_txt[i].get) < 1) {
			continue;
		}

		if ((s = strtok((char *)&powpan_answer[1], ",")) != NULL) {
			dstate_setflags(powpan_vartab_txt[i].var, ST_FLAG_RW);
			dstate_addenum(powpan_vartab_txt[i].var, "%li", strtol(s, NULL, 10));
		}

		while ((s = strtok(NULL, ",")) != NULL) {
			dstate_addenum(powpan_vartab_txt[i].var, "%li", strtol(s, NULL, 10));
		}
	}

	/*
	 * WRITE P5\r
	 * READ #<unknown>\r
	 */
	if (powpan_command_txt("P5\r") > 0) {
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
	if (powpan_command_txt("P9\r") > 0) {
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
	powpan_command_txt("C\r");

	upsh.instcmd = instcmd_txt;
	upsh.setvar = setvar_txt;
}

void upsdrv_initinfo(void)
{
	char	*s;

	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);

	dstate_setinfo("ups.mfr", "CyberPower");
	dstate_setinfo("ups.model", "[unknown]");
	dstate_setinfo("ups.serial", "[unknown]");

	powpan_protocol[mode].initinfo();

	/*
	 * Allow to override the following parameters
	 */
	if ((s = getval("manufacturer")) != NULL) {
		dstate_setinfo("ups.mfr", s);
	}
	if ((s = getval("model")) != NULL) {
		dstate_setinfo("ups.model", s);
	}
	if ((s = getval("serial")) != NULL) {
		dstate_setinfo("ups.serial", s);
	}
}

static int powpan_status(int expected)
{
	int	ret;

	upsdebug_hex(3, "send", (unsigned char *)"D\r", 2);

	ser_flush_io(upsfd);

	ret = ser_send_pace(upsfd, UPSDELAY, "D\r");

	if (ret < 2) {
		return -1;
	}

	usleep(200000);

	ret = ser_get_buf_len(upsfd, powpan_answer, expected, SER_WAIT_SEC, SER_WAIT_USEC);

	if (ret > 0) {
		upsdebug_hex(3, "read", powpan_answer, ret);
	} else {
		upsdebugx(3, "read: timed out");
	}

	return ret;
}

static void updateinfo_bin()
{
	unsigned char	*status = &powpan_answer[9];

	/*
	 * WRITE D\r
	 * READ #VVL.CTF.....\r
        *      01234567890123
	 */
	if (powpan_status(14) < 14) {
		ser_comm_fail("Status read failed!");
		dstate_datastale();
		return;
	}

	if ((status[0] + status[1]) != 255) {
		ser_comm_fail("Status checksum (1) failed!");
		dstate_datastale();
		return;
	}

	if ((status[2] + status[3]) != 255) {
		ser_comm_fail("Status checksum (2) failed!");
		dstate_datastale();
		return;
	}

        dstate_setinfo("input.voltage", "%d", powpan_answer[1]);
        dstate_setinfo("output.voltage", "%d", powpan_answer[2]);
        dstate_setinfo("ups.load", "%d", powpan_answer[3]);
	dstate_setinfo("battery.charge", "%d", powpan_answer[5]);
        dstate_setinfo("ups.temperature", "%d", powpan_answer[6]);
	/*
	 * The following is just a wild guess. With a nominal input
	 * frequency of 60 Hz, the PR2200 shows a value of 150 (decimal).
	 * No idea what it means though, since we got only one reading.
	 */
        dstate_setinfo("input.frequency", "%.1f", powpan_answer[7] / 2.5);

	if (status[0] & 0x01) {
		dstate_setinfo("ups.beeper.status", "enabled");
	} else {
		dstate_setinfo("ups.beeper.status", "disabled");
	}

	status_init();

	if (status[0] & 0x80) {
		status_set("OB");
	} else {
		status_set("OL");

		if (powpan_answer[1] < powpan_answer[2] - 2) {
			status_set("BOOST");
		}

		if (powpan_answer[1] > powpan_answer[2] + 2) {
			status_set("TRIM");
		}
	}

	if (status[0] & 0x40) {
		status_set("LB");
	}

	if (status[0] & 0x04) {
		status_set("TEST");
	}

	if (status[0] == 0) {
		status_set("OFF");
	}

	status_commit();

	ser_comm_good();
	dstate_dataok();

	return;
}

static void updateinfo_txt()
{
	unsigned char	*status = &powpan_answer[32];

	/*
	 * WRITE D\r
	 * READ #I119.0O119.0L000B100T027F060.0S..\r
	 *      01234567890123456789012345678901234
	 */
	if (powpan_status(35) < 35) {
		ser_comm_fail("Status read failed!");
		dstate_datastale();
		return;
	}

        dstate_setinfo("input.voltage", "%g", strtod((char *)&powpan_answer[2], NULL));
        dstate_setinfo("output.voltage", "%g", strtod((char *)&powpan_answer[8], NULL));
        dstate_setinfo("ups.load", "%li", strtol((char *)&powpan_answer[14], NULL, 10));
        dstate_setinfo("input.frequency", "%g", strtod((char *)&powpan_answer[26], NULL));
        dstate_setinfo("ups.temperature", "%li", strtol((char *)&powpan_answer[22], NULL,10));
	dstate_setinfo("battery.charge", "%02.1f", strtod((char *)&powpan_answer[18], NULL));

	status_init();

	if (status[0] & 0x40) {
		status_set("OB");
	} else {
		status_set("OL");

		if (strtod((char *)&powpan_answer[2], NULL) < (strtod((char *)&powpan_answer[8], NULL) - 2)) {
			status_set("BOOST");
		}

		if (strtod((char *)&powpan_answer[2], NULL) > (strtod((char *)&powpan_answer[8], NULL) + 2)) {
			status_set("TRIM");
		}
	}

	if (status[0] & 0x20) {
		status_set("LB");
	}

	if (status[0] & 0x08) {
		status_set("TEST");
	}

	if (status[0] == 0) {
		status_set("OFF");
	}

	status_commit();

	ser_comm_good();
	dstate_dataok();

	return;
}

void upsdrv_updateinfo(void)
{
	powpan_protocol[mode].updateinfo();
}

void shutdown_bin()
{
	unsigned char	*status = (unsigned char *)&powpan_answer[9];
	int		i;

	for (i = 0; i < MAXTRIES; i++) {

		if (powpan_status(14) < 14) {
			continue;
		}

		if ((status[0] + status[1]) != 255) {
			continue;
		}

		/*
		 * We're still on battery...
		 */
		if (status[0] & 0x80) {
			break;
		}
 
		/*
		 * Apparently, the power came back already, so just reboot.
		 */
		if (instcmd_bin("shutdown.reboot", NULL) == STAT_INSTCMD_HANDLED) {
			upslogx(LOG_INFO, "Rebooting now...");
			return;
		}
	}

	for (i = 0; i < MAXTRIES; i++) {

		/*
		 * ...send wait for return.
		 */
		if (instcmd_bin("shutdown.return", NULL) == STAT_INSTCMD_HANDLED) {
			upslogx(LOG_INFO, "Waiting for power to return...");
			return;
		}
	}

	upslogx(LOG_ERR, "Shutdown command failed!");
}

void shutdown_txt()
{
	int	i;

	for (i = 0; i < MAXTRIES; i++) {

		if (instcmd_txt("shutdown.return", NULL) == STAT_INSTCMD_HANDLED) {
			upslogx(LOG_INFO, "Waiting for power to return...");
			return;
		}
	}

	upslogx(LOG_ERR, "Shutdown command failed!");
}

void upsdrv_shutdown(void)
{
	powpan_protocol[mode].shutdown();
}

static int initups_bin()
{
	int	ret, i;

	upsdebugx(1, "Trying binary protocol...");

	ser_set_speed(upsfd, device_path, B1200);

	powpan_command_txt("\r\r");

	for (i = 0; i < MAXTRIES; i++) {

		/*
		 * WRITE F\r
		 * READ .PR2200    .x.<.1100
		 *      01234567890123456789
		 */
		ret = powpan_command_txt("F\r");

		if (ret < 20) {
			upsdebugx(2, "Expected 20 bytes but only got %d", ret);
			continue;
		}

		if (powpan_answer[0] != '.') {
			upsdebugx(2, "Expected start character '.' but got '%c'", (char)powpan_answer[0]);
			continue;
		}

		upslogx(LOG_INFO, "CyberPower binary protocol UPS on %s detected", device_path);
		return ret;
	}

	return -1;
}

static int initups_txt()
{
	int	ret, i;

	upsdebugx(1, "Trying text protocol...");

	ser_set_speed(upsfd, device_path, B2400);

	powpan_command_txt("\r\r");

	for (i = 0; i < MAXTRIES; i++) {

		/*
		 * WRITE P4\r
		 * READ #BC1200     ,1.600,000000000000,CYBER POWER    
		 *      01234567890123456789012345678901234567890123456
		 */
		ret = powpan_command_txt("P4\r");

		if (ret < 46) {
			upsdebugx(2, "Expected 46 bytes, but only got %d", ret);
			continue;
		}

		if (powpan_answer[0] != '#') {
			upsdebugx(2, "Expected start character '#', but got '%c'", (char)powpan_answer[0]);
			continue;
		}

		upslogx(LOG_INFO, "CyberPower text protocol UPS on %s detected", device_path);
		return ret;
	}

	return -1;
}

void upsdrv_initups(void)
{
	char	*version;

	version = getval("protocol");
	upsfd = ser_open(device_path);

	ser_set_rts(upsfd, 0);

	/*
	 * Try to autodetect which UPS is connected.
	 */
	for (mode = 0; powpan_protocol[mode].initups != NULL; mode++) {

		if ((version != NULL) && strcasecmp(version, powpan_protocol[mode].version)) {
			continue;
		}

		ser_set_dtr(upsfd, 1);
		usleep(10000);

		if (powpan_protocol[mode].initups() > 0) {
			return;
		}

		ser_set_dtr(upsfd, 0);
		usleep(10000);
	}

	fatalx(EXIT_FAILURE, "CyberPower UPS not found on %s", device_path);
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "manufacturer", "manufacturer");
	addvar(VAR_VALUE, "model", "modelname");
	addvar(VAR_VALUE, "serial", "serialnumber");
	addvar(VAR_VALUE, "protocol", "protocol to use [text|binary] (default: autodetion)");
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools -  CyberPower text/binary protocol UPS driver %s (%s)\n",
		DRV_VERSION, UPS_VERSION);
	experimental_driver = 1;
}

void upsdrv_cleanup(void)
{
	ser_set_dtr(upsfd, 0);
	ser_close(upsfd, device_path);
}
