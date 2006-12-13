/* fentonups.c - model specific routines for Fenton Technologies units

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
                 2005  Michel Bouissou <michel@bouissou.net>

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

#include "fentonups.h"

#define ENDCHAR 	13	/* replies end with CR */
#define SER_WAIT_SEC	3	/* allow 3.0 sec for ser_get calls */
#define SER_WAIT_USEC	0

#define	DRV_VERSION "1.22"

static	int	cap_upstemp = 0;
static	float	lowvolt = 0, voltrange, chrglow = 0, chrgrange;
static	int	lownorm, highnorm;

/* handle devices which don't give a properly formatted I string */
static int check_mtab2(const char *raw)
{
	int	i;
	char	*cooked;

	/* trim off the leading # and any trailing spaces */
	cooked = xstrdup(&raw[1]);
	rtrim(cooked, ' ');

	for (i = 0; mtab2[i].id != NULL; i++) {
		if (!strcmp(cooked, mtab2[i].id)) {

			/* found it - load the data */
			dstate_setinfo("ups.mfr", "%s", mtab2[i].mfr);
			dstate_setinfo("ups.model", "%s", mtab2[i].model);
			lowvolt = mtab2[i].lowvolt;
			voltrange = mtab2[i].voltrange;
			chrglow = mtab2[i].chrglow;
			chrgrange = mtab2[i].chrgrange;
			cap_upstemp = mtab2[i].has_temp;

			dstate_setinfo("input.transfer.low", "%d",
				mtab2[i].lowxfer);
			dstate_setinfo("input.transfer.high", "%d",
				mtab2[i].highxfer);

			lownorm = mtab2[i].lownorm;
			highnorm = mtab2[i].highnorm;

			free(cooked);
			return 1;	/* found */
		}
	}

	free(cooked);
	return 0;	/* not found */
}

static void guessmodel(const char *raw)
{
	char	mch, *mstr;

	/* first see if it's in the mtab2 */
	if (check_mtab2(raw))
		return;

	mch = raw[17];

	mstr = xstrdup(&raw[18]);
	mstr[10] = '\0';	/* 10 chars max, per the protocol */

	/* trim whitespace */
	rtrim(mstr, ' ');

	/* use Fenton model-chars to attempt UPS detection */

	switch (mch) {
		case 'L':
			dstate_setinfo("ups.model", "PowerPal %s", mstr);
			cap_upstemp = 0;
			break;

		case 'H':
			dstate_setinfo("ups.model", "PowerOn %s", mstr);
			cap_upstemp = 1;
			break;

		case 'M':
			dstate_setinfo("ups.model", "PowerPure %s", mstr);
			cap_upstemp = 1;
			break;

		default: 
			dstate_setinfo("ups.model", "Unknown %s", mstr);
			upslogx(LOG_ERR, "Unknown ups - "
				"please report this ID string: %s", raw);
			break;
	}

	free(mstr);
}

static int instcmd(const char *cmdname, const char *extra)
{
	int	ret;

	if (!strcasecmp(cmdname, "test.battery.start")) {
		ret = ser_send(upsfd, "T\r");

		if (ret != 2)
			upslog_with_errno(LOG_ERR, "instcmd: ser_send failed");

		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ret = ser_send(upsfd, "CT\r");

		if (ret != 2)
			upslog_with_errno(LOG_ERR, "instcmd: ser_send failed");

		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static char *get_id(void)
{
	int	i, ret;
	char	temp[SMALLBUF];

	/* try to get the initial UPS data */
	for (i = 0; i < 5; i++) {
		ret = ser_send(upsfd, "I\r");

		if (ret != 2)
			upslog_with_errno(LOG_ERR, "get_id: ser_send failed");

		sleep(1);
		ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "", 
			SER_WAIT_SEC, SER_WAIT_USEC);

		if (ret < 1) {
			upslogx(LOG_ERR, "Short read during UPS id sequence");
			continue;
		}

		if (temp[0] != '#') {
			upslogx(LOG_ERR, "Bad UPS info start character [%c]",
				temp[0]);
			continue;
		}

		/* got what we need.. */
		return xstrdup(temp);
	}

	return NULL;
}

void upsdrv_initinfo(void)
{
	int	modelnum, i, ret;
	char	temp[256], model[32], *raw;

	raw = get_id();

	if (!raw)
		fatalx("Unable to detect a Fenton or Megatec protocol UPS");

	snprintf(temp, sizeof(temp), "%s", raw);

	temp[11] = 0;
	temp[27] = 0;

	/* manufacturer */
	rtrim(&temp[1], ' ');
	dstate_setinfo("ups.mfr", "%s", &temp[1]);

	/* L660A = PowerPal (L) @ 660 VA, American (A) version (115V) */

	/* grab full model string */
	rtrim(&temp[17], ' ');
	snprintf(model, sizeof(model), "%s", &temp[17]);

	modelnum = -1;

	/* figure out official model name and voltage info from table */
	for (i = 0; modeltab[i].mtext != NULL; i++) {
		if (!strcmp(modeltab[i].mtext, model)) {
			modelnum = i;
			lowvolt = modeltab[i].lowvolt;
			voltrange = modeltab[i].voltrange;
			chrglow = modeltab[i].chrglow;
			chrgrange = modeltab[i].chrgrange;
			cap_upstemp = modeltab[i].has_temp;
			break;
		}
	}

	/* table lookup fails -> guess */
	if (modelnum == -1)
		guessmodel(raw);
	else {
		dstate_setinfo("ups.model", "%s", modeltab[modelnum].desc);

		dstate_setinfo("input.transfer.low", "%d", 
			modeltab[modelnum].lowxfer);

		dstate_setinfo("input.transfer.high", "%d",
			modeltab[modelnum].highxfer);

		lownorm = modeltab[modelnum].lownorm;
		highnorm = modeltab[modelnum].highnorm;
	}

	/* now add instant command support info */
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");

	printf("Detected %s on %s\n", dstate_getinfo("ups.model"), device_path);
	free(raw);

	/* paranoia - cancel any shutdown that might already be running */
	ret = ser_send(upsfd, "C\r");

	if (ret != 2)
		upslog_with_errno(LOG_ERR, "upsdrv_initinfo: ser_send failed");

	upsh.instcmd = instcmd;

	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
}

void upsdrv_updateinfo(void)
{
	char	temp[256], involt[16], loadpct[16], acfreq[16], battvolt[16],
		upstemp[16], pstat[16], outvolt[16];
	int	util, ret;
	double	bvoltp;
	float   lowbattvolt = 0;

	ret = ser_send(upsfd, "Q1\r");

	if (ret != 3)
		upslog_with_errno(LOG_ERR, "upsdrv_updateinfo: ser_send failed");

	/* give it a chance to return the full line */
	usleep(300000);

	ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "", 
		SER_WAIT_SEC, SER_WAIT_USEC);

	if (ret < 1) {
		ser_comm_fail(NULL);
		dstate_datastale();
		return;
	}

	/* sanity checks for poll data */
	if (ret < 46) {
		ser_comm_fail("Poll failed: short read (got %d bytes)", ret);
		dstate_datastale();
		return;
	}

	if (ret > 46) {
		ser_comm_fail("Poll failed: response too long (got %d bytes)", 
			ret);
		dstate_datastale();
		return;
	}

	if (temp[0] != '(') {
		ser_comm_fail("Poll failed: invalid start character (got %02x)",
			temp[0]);
		dstate_datastale();
		return;
	}

	ser_comm_good();

	/* (MMM.M NNN.N PPP.P QQQ RR.R S.SS TT.T  b7b6b5b4b3b2b1b0<cr>
	 *
	 * MMM.M : input voltage (involt)
	 * NNN.N : fault voltage (ignored)
	 * PPP.P : output voltage
	 */

	sscanf(temp, "%*c%s %*s %s %s %s %s %s %s", involt, outvolt, loadpct,
		acfreq, battvolt, upstemp, pstat);

	dstate_setinfo("input.voltage", "%s", involt);
	dstate_setinfo("output.voltage", "%s", outvolt);
	dstate_setinfo("battery.voltage", "%s", battvolt);

	status_init();

	util = atoi(involt);

	if (pstat[0] == '0') {
		status_set("OL");		/* on line */

		/* only allow these when OL since they're bogus when OB */
		if (pstat[2] == '1') {		/* boost or trim in effect */
			if (util < lownorm)
				status_set("BOOST");

			if (util > highnorm)
				status_set("TRIM");
		}
		if (atof(battvolt) > chrglow) {
			bvoltp = ((atof(battvolt) - chrglow) / chrgrange) * 100.0;
		} else {
			bvoltp = ((atof(battvolt) - lowvolt) / voltrange) * 100.0;
		}


	} else {
		status_set("OB");		/* on battery */
		bvoltp = ((atof(battvolt) - lowvolt) / voltrange) * 100.0;
	}

	if (bvoltp > 100.0)
		bvoltp = 100.0;
	dstate_setinfo("battery.charge", "%02.1f", bvoltp);

	if (pstat[1] == '1') {
		status_set("LB");		/* low battery */
	} else {
		if (pstat[0] == '1' && getval("lowbattvolt")) {
			lowbattvolt = atof(getval("lowbattvolt"));
			if ((lowbattvolt > 0) && (lowbattvolt >= (atof(battvolt))))
				status_set("LB");               /* low battery */
		}
	}


	status_commit();

	if (cap_upstemp == 1)
		dstate_setinfo("ups.temperature", "%s", upstemp);

	dstate_setinfo("input.frequency", "%s", acfreq);
	dstate_setinfo("ups.load", "%s", loadpct);

	dstate_dataok();
}

static int ups_on_line(void)
{
	int	ret;
	char	temp[256], pstat[32];

	ret = ser_send(upsfd, "Q1\r");

	if (ret != 3) {
		printf("Status request send failed, assuming on battery state");
		return 0;
	}

	ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "", 
		SER_WAIT_SEC, SER_WAIT_USEC);

	if (ret < 1) {
		printf("Status request read failed, assuming on battery state\n");
		return 0;
	}

	if (strlen(temp) != 46) {
		printf("Status request read returned garbage (%lu bytes), "
			"assuming on battery state\n", (unsigned long)strlen(temp));
		return 0;
	}

	sscanf(temp, "%*s %*s %*s %*s %*s %*s %*s %s", pstat);	

	if (pstat[0] == '1')
		return 0;	/* on battery */

	return 1;	/* on line */
}

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{
	/* line status determines which command is used */

	/* on battery: send S01<cr>, ups will return by itself on utility */
	/* on line: send S01R0003<cr>, ups will cycle and return soon */

	if (ups_on_line()) {
		printf("On line, sending shutdown+return command...\n");

		/* return values below 3 misbehave on some hardware revs */
		ser_send(upsfd, "S01R0003\r");
		return;
	}

	printf("On battery, sending normal shutdown command...\n");
	ser_send(upsfd, "S01\r");
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "lowbattvolt", "Set low battery level, in volts");
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - Fenton UPS driver %s (%s)\n", 
		DRV_VERSION, UPS_VERSION);

	upslogx(LOG_WARNING, "This driver is obsolete and has been replaced by the \"megatec\""
	                     " driver. It will be removed somewhere in the near future.");
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
