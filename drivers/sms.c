/* sms.c - driver for SMS UPS hardware

   Copyright (C) 2001  Marcio Gomes  <tecnica@microlink.com.br>

   based on fentonups.c:

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

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

   2001/05/17 - Version 0.10 - Initial release
   2001/06/01 - Version 0.20 - Add Battery Informations in driver
   2001/06/04 - Version 0.30 - Updated Battery Volts range, to reflect a correct
                               percent ( % )
   2002/12/02 - Version 0.40 - Update driver to new-model, based on Fentonups 
                               driver version 0.90  
   2002/12/18 - Version 0.50 - Add Sinus Single 2 KVA in database, test with
                               new Manager III sinusoidal versions.
                               Change Detect Name, SMS do not pass real Models
                               in Megatec Info command, only the VA/KVA version
   2002/12/18 - Version 0.51 - Updated Battery Volts range, to reflect a correct
                               percent ( % )
   2002/12/27 - Version 0.60 - Add new UPS Commands SDRET, SIMPWF change BTEST1
   2002/12/28 - Version 0.70 - Add new UPS Commands SHUTDOWN,STOPSHUTD,WATCHDOG
   2003/06/11 - Version 0.71 - Converted to dstate calls and names (rkroll)


   Microlink ISP/Pop-Rio contributed with MANAGER III 1300, MANAGER III 650 UPS
   and Sinus Single 2 KVA for my tests.

   http://www.microlink.com.br and http://www.pop-rio.com.br



*/

#define DRV_VERSION "0.72"

#include "main.h"
#include "serial.h"
#include "sms.h"

#define ENDCHAR 13	/* replies end with CR */

static	int	cap_upstemp = 0;
static	float	lowvolt = 0, voltrange;
static	int	lownorm, highnorm, poll_failures = 0;

static void guessmodel(const char *raw)
{
	char	mch, *mstr;

	mch = raw[17];

        printf ("0         1         2        3         \n");
        printf ("012345678901234567890123567890123456789\n");
        printf ("%s\n", raw);
	mstr = xstrdup(&raw[18]);
	mstr[10] = '\0';	/* 10 chars max, per the protocol */
       
	/* trim whitespace */
	rtrim(mstr, ' ');
     
	dstate_setinfo("ups.model", "SMS %s", mstr);   
	cap_upstemp = 1;

	free(mstr);
}

static void getbaseinfo(void)
{
	char	temp[256], model[32], *raw;
	int	modelnum, i;

	/* dummy read attempt to sync - throw it out */
	ser_send(upsfd, "I\r");
	ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "", 3, 0);

	/* now retrieve information and parse */
	ser_send(upsfd, "I\r");
	ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "", 3, 0);
	raw = xstrdup(temp);

	if (temp[0] != '#')
		fatalx("Bad UPS info start character [%s]", temp);

	temp[11] = 0;
	temp[27] = 0;

	/* manufacturer */
	rtrim(&temp[1], ' ');

	dstate_setinfo("ups.mfr", "%s", &temp[1]);

/*
        0         1         2        3         
        012345678901234567890123567890123456789
        #SMS LTDA         1300 VA   VER 1.0 
        #SMS LTDA        1300VA SEN VER 5.0 
        #SMS LTDA             2 KVA   VER 1.0 
*/
	/* grab full model string */

	rtrim(&temp[17], ' ');

	snprintf(model, sizeof(model), "%s", &temp[17]);
/*        printf("->%s<-\n",model);  */
	modelnum = -1;

	/* figure out official model name and voltage info from table */
	for (i = 0; modeltab[i].mtext != NULL; i++) {
		if (!strcmp(modeltab[i].mtext, model)) {
			modelnum = i;
			lowvolt = modeltab[i].lowvolt;
			voltrange = modeltab[i].voltrange;
			cap_upstemp = modeltab[i].has_temp;
			break;
		}
	}

	/* table lookup fails -> guess */
	if (modelnum == -1)
		guessmodel (raw);
	else {
		dstate_setinfo("ups.model", "%s", modeltab[modelnum].desc);

		dstate_setinfo("input.transfer.low", "%i", 
			modeltab[modelnum].lowxfer);
		dstate_setinfo("input.transfer.high", "%i", 
			modeltab[modelnum].highxfer);

		lownorm = modeltab[modelnum].lownorm;
		highnorm = modeltab[modelnum].highnorm;
	}

	/* now add instant command support info */
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("shutdown.return");	/* was CMD_SDRET */
	dstate_addcmd("test.failure.start");
	dstate_addcmd("shutdown.stayoff");	/* was CMD_SHUTDOWN */
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("reset.watchdog");

	printf("Detected %s on %s\n", dstate_getinfo("ups.model"), device_path);
	free(raw);

	/* paranoia - cancel any shutdown that might already be running */
	ser_send(upsfd, "C\r");
}

static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send(upsfd, "CT\r");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start")) {
		ser_send(upsfd, "TL\r");/* start battery test until bat low */
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.failure.start")) {
		ser_send(upsfd, "T\r");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.return")) {
		/* shutdown and restart */
		ser_send(upsfd, "C\r");
		ser_send(upsfd, "S.3R0003\r");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		/* shutdown now (one way) */
		ser_send(upsfd, "C\r");
		ser_send(upsfd, "S.3\r");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stop")) {
		/* Cancel Shutdown  */
		ser_send(upsfd, "C\r");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "reset.watchdog")) {
		/* WATCHDOG Crontab Function */
		ser_send(upsfd, "C\r");
		ser_send(upsfd, "S05R0003\r");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	getbaseinfo();

	upsh.instcmd = instcmd;
	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
}

static void pollfail(const char *why)
{
	poll_failures++;

	/* don't spew into the syslog forever */
	if (poll_failures < 3)
		upslogx(LOG_ERR, why);

	return;
}

void upsdrv_updateinfo(void)
{
	char	temp[256], utility[16], loadpct[16], acfreq[16], battvolt[16],
		upstemp[16], pstat[16], outvolt[16];
	int	util, ret;
	double	bvoltp;

	ser_send(upsfd, "Q1\r");

	ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "", 3, 0);

	/* sanity checks for poll data */
	if (strlen(temp) < 46) {
		pollfail("Poll failed: short read from UPS");
		dstate_datastale();
		return;
	}

	if (strlen(temp) > 46) {
		pollfail("Poll failed: oversized read from UPS");
		dstate_datastale();
		return;
	}

	if (temp[0] != '(') {
		pollfail("Poll failed: invalid start character");
		dstate_datastale();
		return;
	}

	if (poll_failures > 0)
		upslogx(LOG_NOTICE, "UPS poll succeeded");

	poll_failures = 0;		

	/* (MMM.M NNN.N PPP.P QQQ RR.R S.SS TT.T  b7b6b5b4b3b2b1b0<cr>
	 *
	 * MMM.M : input voltage (utility)
	 * NNN.N : fault voltage (ignored)
	 * PPP.P : output voltage
	 */

	sscanf(temp, "%*c%s %*s %s %s %s %s %s %s", utility, outvolt, loadpct,
		acfreq, battvolt, upstemp, pstat);

	dstate_setinfo("output.voltage", "%s", outvolt);
	dstate_setinfo("input.voltage", "%s", utility);
	dstate_setinfo("battery.voltage", "%s", battvolt);

	bvoltp = ((atof(battvolt) - lowvolt) / voltrange) * 100.0;

	if (bvoltp > 100.0)
		bvoltp = 100.0;

	dstate_setinfo("battery.charge", "%02.1f", bvoltp);

	status_init();

	util = atoi(utility);

	if (pstat[0] == '0') {
		status_set("OL");		/* on line */

		/* only allow these when OL since they're bogus when OB */
		if (pstat[2] == '1') {		/* boost or trim in effect */
			if (util < lownorm)
				status_set("BOOST");

			if (util > highnorm)
				status_set("TRIM");
		}

	} else {
		status_set("OB");		/* on battery */
	}

	if (pstat[1] == '1')
		status_set("LB");		/* low battery */

	status_commit();

	if (cap_upstemp == 1)
		dstate_setinfo("ups.temperature", "%s", upstemp);

	dstate_setinfo("input.frequency", "%s", acfreq);
	dstate_setinfo("ups.load", "%s", loadpct);

	dstate_dataok();
}

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{
	char	temp[256], pstat[32];

	/* basic idea: find out line status and send appropriate command */

	ser_send(upsfd, "Q1\r");
	ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "", 3, 0);
	sscanf (temp, "%*s %*s %*s %*s %*s %*s %*s %s", pstat);

	/* on battery: send S01<cr>, ups will return by itself on utility */
	/* on line: send S01R0003<cr>, ups will cycle and return soon */

	ser_send(upsfd, "S01");

	if (pstat[0] == '0') {			/* on line */
		printf("On line, sending shutdown+return command...\n");
		ser_send(upsfd, "R0003");
	} else
		printf("On battery, sending normal shutdown command...\n");

	ser_send_char(upsfd, 13);	/* end sequence */
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - SMS UPS driver %s (%s)\n", 
		DRV_VERSION, UPS_VERSION);
        printf("by Marcio Gomes at Microlink - tecnica@microlink.com.br\n\n");
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
