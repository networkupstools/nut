/* powerpanel.c - Model specific routines for CyberPower text protocol UPSes 

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
#include <sys/ioctl.h>

#include "main.h"
#include "serial.h"
#include "powerpanel.h"

#define DRV_VERSION "0.11"

static char powpan_reply[SMALLBUF];

/*
 * This function will send a command and readback the reply. It will
 * return the length of the reply (including the start character '#'
 * on success). A negative return value indicates a problem.
 */
static int powpan_command(const char *command)
{
	int 	dtr_bit = TIOCM_DTR;
	int 	ret = -1;

	upsdebugx(3, "command: %s", command);

	ioctl(upsfd, TIOCMBIS, &dtr_bit);
	tcflush(upsfd, TCIOFLUSH);

	if (ser_send_pace(upsfd, UPSDELAY, command) == strlen(command))
	{
		/*
		 * We expect up to 35 characters, which should take
		 * about 150ms to receive (at 2400 Baud).
		 */
		usleep(200000);

		ret = ser_get_line(upsfd, powpan_reply, sizeof(powpan_reply), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		if (ret > 0)
			upsdebug_hex(3, "reply  ", (unsigned char *)powpan_reply, ret);
		else
			upsdebugx(3, "reply  : <none>");
	}

	ioctl(upsfd, TIOCMBIC, &dtr_bit);

	if (powpan_reply[0] != '#')
		return -2;

	return ret;
}

static int instcmd(const char *cmdname, const char *extra)
{
	int 	ret = -1;

	if (!strcasecmp(cmdname, "test.battery.start"))
		ret = powpan_command("T\r");

	if (!strcasecmp(cmdname, "test.battery.stop"))
		ret = powpan_command("CT\r");

	if (!strcasecmp(cmdname, "beeper.on"))
		ret = powpan_command("C7:1\r");

	if (!strcasecmp(cmdname, "beeper.off"))
		ret = powpan_command("C7:0\r");

	if (ret > 0)
		return STAT_INSTCMD_HANDLED;

	upslogx(LOG_NOTICE, "instcmd: command [%s] failed", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static int setvar(const char *varname, const char *val)
{
	char	command[SMALLBUF];

	/*
	 * After setting a variable, the UPS replies with "#0" on
	 * success and "#9" on failure. We only check for success.
	 */

	if (!strcasecmp(varname, "input.transfer.high"))
	{
		snprintf(command, sizeof(command), "C2:%s\r", val);

		if ((powpan_command(command) > 0) && !strcasecmp(powpan_reply, "#0"))
		{
			dstate_setinfo("input.transfer.high", val);
			return STAT_SET_HANDLED;
		}
	}

	if (!strcasecmp(varname, "input.transfer.low"))
	{
		snprintf(command, sizeof(command), "C3:%s\r", val);

		if ((powpan_command(command) > 0) && !strcasecmp(powpan_reply, "#0"))
		{
			dstate_setinfo("input.transfer.low", val);
			return STAT_SET_HANDLED;
		}
	}

	if (!strcasecmp(varname, "battery.charge.low"))
	{
		snprintf(command, sizeof(command), "C4:%s\r", val);

		if ((powpan_command(command) > 0) && !strcasecmp(powpan_reply, "#0"))
		{
			dstate_setinfo("battery.charge.low", val);
			return STAT_SET_HANDLED;
		}
	}

	upslogx(LOG_NOTICE, "setvar: setting variable [%s] to [%s] failed", varname, val);
	return STAT_SET_UNKNOWN;
}

/*
 * Try up to MAXTRIES times to get a useful identity from the UPS.
 * It should reply with "#2" after a "\r" command (probably protocol
 * version) and with an identification string after "P4\r".
 */
static int get_ident(void)
{
	char	*s;
	int	retry = MAXTRIES;

	while (retry--)
	{
		/*
		 * WRITE \r
		 * READ #2\r
		 */
		if ((powpan_command("\r") < 0) || (powpan_reply[1] != '2'))
			upslogx(LOG_INFO, "Sent \"\\r\", expected \"#2\\r\" but got \"%s\"", powpan_reply);

		/*
		 * WRITE P4\r
		 * READ #BC1200     ,1.600,000000000000,CYBER POWER    \r
		 */
		if (powpan_command("P4\r") > 0)
		{
			if ((s = strtok(powpan_reply+1, ",")) != NULL)
				dstate_setinfo("ups.model", s);
			if ((s = strtok(NULL, ",")) != NULL)
				dstate_setinfo("ups.firmware", s);
			if ((s = strtok(NULL, ",")) != NULL)
				dstate_setinfo("ups.serial", s);
			if ((s = strtok(NULL, ",")) != NULL)
				dstate_setinfo("ups.mfr", s);

			return 1;
		}

		upslogx(LOG_INFO, "Sent \"P4\\r\", expected \"#<something>\" but got \"%s\"", powpan_reply);
 	}

	return 0;
}

/*
 * Try if we can autodetect some parameters from the UPS. This may not
 * work on every supported model, so don't complain to heavily if this
 * doesn't succeed for all commands.
 */
static int get_settings(void)
{
	char	*s;
	int	ret = 0;

	/*
	 * WRITE P3\r
	 * READ #12.0,002,008.0,00\r
	 */
	if (powpan_command("P3\r") > 0)
	{
		if ((s = strtok(powpan_reply+1, ",")) != NULL)
			dstate_setinfo("battery.voltage.nominal", s);
		if ((s = strtok(NULL, ",")) != NULL)
			dstate_setinfo("battery.packs", s);
		if ((s = strtok(NULL, ",")) != NULL)
			dstate_setinfo("battery.capacity", s);

		ret++;
	}

	/*
	 * WRITE P2\r
	 * READ #1200,0720,120,47,63\r
	 */
	if (powpan_command("P2\r") > 0)
	{
		if ((s = strtok(powpan_reply+1, ",")) != NULL)
			dstate_setinfo("ups.power.nominal", s);
		if ((s = strtok(NULL, ",")) != NULL)
			dstate_setinfo("ups.realpower.nominal", s);
		if ((s = strtok(NULL, ",")) != NULL)
			dstate_setinfo("input.voltage.nominal", s);
		if ((s = strtok(NULL, ",")) != NULL)
			dstate_setinfo("input.frequency.low", s);
		if ((s = strtok(NULL, ",")) != NULL)
			dstate_setinfo("input.frequency.high", s);

		ret++;
	}

	/*
	 * WRITE P1\r
	 * READ #120,138,088,20\r
	 */
	if (powpan_command("P1\r") > 0)
	{
		if ((s = strtok(powpan_reply+1, ",")) != NULL)
			dstate_setinfo("input.voltage.nominal", s);
		if ((s = strtok(NULL, ",")) != NULL)
			dstate_setinfo("input.transfer.high", s);
		if ((s = strtok(NULL, ",")) != NULL)
			dstate_setinfo("input.transfer.low", s);
		if ((s = strtok(NULL, ",")) != NULL)
			dstate_setinfo("battery.charge.low", s);

		ret++;
	}

	/*
	 * WRITE P6\r
	 * READ #130,131,132,133,134,135,136,137,138,139,140\r
	 */
	if (dstate_getinfo("input.transfer.high") && (powpan_command("P6\r") > 0))
	{
		if ((s = strtok(powpan_reply+1, ",")) != NULL)
		{
			dstate_addenum("input.transfer.high", s);
			dstate_setflags("input.transfer.high", ST_FLAG_STRING | ST_FLAG_RW);
			dstate_setaux("input.transfer.high", 3);
		}
		while ((s = strtok(NULL, ",")) != NULL)
			dstate_addenum("input.transfer.high", s);

		ret++;
	}

	/*
	 * WRITE P7\r
	 * READ #080,081,082,083,084,085,086,087,088,089,090\r
	 */
	if (dstate_getinfo("input.transfer.low") && (powpan_command("P7\r") > 0))
	{
		if ((s = strtok(powpan_reply+1, ",")) != NULL)
		{
			dstate_addenum("input.transfer.low", s);
			dstate_setflags("input.transfer.low", ST_FLAG_STRING | ST_FLAG_RW);
			dstate_setaux("input.transfer.low", 3);
		}
		while ((s = strtok(NULL, ",")) != NULL)
			dstate_addenum("input.transfer.low", s);

		ret++;
	}

	/*
	 * WRITE P8\r\
	 * READ #20,25,30,35,40,45,50,55,60,65\r
	 */
	if (dstate_getinfo("battery.charge.low") && (powpan_command("P8\r") > 0))
	{
		if ((s = strtok(powpan_reply+1, ",")) != NULL)
		{
			dstate_addenum("battery.charge.low", s);
			dstate_setflags("battery.charge.low", ST_FLAG_STRING | ST_FLAG_RW);
			dstate_setaux("battery.charge.low", 2);
		}
		while ((s = strtok(NULL, ",")) != NULL)
			dstate_addenum("battery.charge.low", s);

		ret++;
	}

	return ret;
}

void upsdrv_initinfo(void)
{
	char	*s;

	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);

	if (get_ident() == 0)
		fatalx("Unable to detect a CyberPower text protocol UPS");

	printf("Detected %s %s on %s\n", dstate_getinfo("ups.mfr"),
		dstate_getinfo("ups.model"), device_path);

	if (get_settings() == 0)
		upslogx(LOG_WARNING, "Can't read settings from CyberPower text protocol UPS");

	/*
	 * WRITE D\r
	 * READ #I119.0O119.0L000B100T027F060.0S..\r
	 */
	powpan_command("D\r");

	/*
	 * Cancel pending shutdown.
	 * WRITE C\r
	 * READ #0\r
	*/
	powpan_command("C\r");

	/*
	 * Allow to override the following parameters
	 */
	if ((s = getval("manufacturer")) != NULL)
		dstate_setinfo("ups.mfr", s);
	if ((s = getval("model")) != NULL)
		dstate_setinfo("ups.model", s);
	if ((s = getval("serial")) != NULL)
		dstate_setinfo("ups.serial", s);

	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("beeper.on");
	dstate_addcmd("beeper.off");
	/* dstate_addcmd("shutdown.return"); */
	/* dstate_addcmd("shutdown.reboot"); */

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;
}

void upsdrv_shutdown(void)
{
	int	retry = MAXTRIES;

	if (get_ident() == 0)
		fatalx("Unable to detect a CyberPower text protocol UPS");

	/*
	 * Don't abort on the first try
	 */
	while (retry--)
	{
		if (powpan_command("Z02\r") > 0)
		{
			upslogx(LOG_INFO, "Shutdown in progress");
			return;
		}
	}

	upslogx(LOG_ERR, "Shutdown command returned with an error!");
}

void upsdrv_updateinfo(void)
{
	/*
	 * WRITE D\r
	 * READ #I119.0O119.0L000B100T027F060.0S..\r
	 */
	if (powpan_command("D\r") != 34)
	{
		ser_comm_fail("Status read failed!");
		dstate_datastale();
	}

	status_init();

	if ((powpan_reply[POLL_STATUS] & CPS_STAT_OL) && !(powpan_reply[POLL_STATUS] & CPS_STAT_OB))
		status_set("OL");

	if (powpan_reply[POLL_STATUS] & CPS_STAT_OB) 
		status_set("OB");

	if (powpan_reply[POLL_STATUS] & CPS_STAT_LB) 
		status_set("LB");

	if (powpan_reply[POLL_STATUS] & CPS_STAT_CAL)
		status_set("CAL");

	if (powpan_reply[POLL_STATUS]  == 0)
		status_set("OFF");

	status_commit();

        dstate_setinfo("input.voltage", "%g", strtod(&powpan_reply[POLL_INPUTVOLT], NULL));
        dstate_setinfo("output.voltage", "%g", strtod(&powpan_reply[POLL_OUTPUTVOLT], NULL));
        dstate_setinfo("ups.load", "%li", strtol(&powpan_reply[POLL_LOAD], NULL, 10));
        dstate_setinfo("input.frequency", "%g", strtod(&powpan_reply[POLL_FREQUENCY], NULL));
        dstate_setinfo("ups.temperature", "%li", strtol(&powpan_reply[POLL_TEMP], NULL,10));
	dstate_setinfo("battery.charge", "%02.1f", strtod(&powpan_reply[POLL_BATTCHARGE], NULL));

	ser_comm_good();
	dstate_dataok();

	return;
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools -  CyberPower text protocol UPS driver %s (%s)\n",
		DRV_VERSION, UPS_VERSION);
	experimental_driver = 1;
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
