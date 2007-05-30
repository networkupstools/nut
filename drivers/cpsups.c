/* cpsups.c - model specific routines for CyberPower text protocol UPSes 

   Copyright (C) 2003  Walt Holman <waltabbyh@comcast.net>
   with thanks to Russell Kroll <rkroll@exploits.org>

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

/* This driver started out as the bestups.c driver from 1.5.11 - 
 * I've hacked it up every which way to get it to function 
 * with a CPS1100AVR. Thanks go to the guys at 
 * http://networkupstools.org for creating a very nice toolset.
*/

/* hack version for 1200VA
* by DJR
* 
* Ported to 2.0.4 and generialized
* by RHR
*/

#include "cpsups.h"

#define DRV_VERSION ".05"

static int rmchar = 0;

static void model_set(const char *abbr, const char *rating)
{

        /* 
         * Added: Brad Sawatzky <brad+nut@lamorak.phys.virginia.edu> 02Jun04
         * NOTE: I have no idea how to set the runtime parameter... I basically
         * scaled up linearly from the 1100 and 500 entries based on the
         * 'voltage'.  The realtime runtime calculated under load looks
         * reasonable.
         */
	if (!strcmp(abbr, "#1500VA    ")) {
	        dstate_setinfo("ups.mfr", "%s", "CyberPower");
	        dstate_setinfo("ups.model", "CPS1500AVR %s", rating);
		dstate_setinfo("ups.runtime", "%s", "90");
		dstate_setinfo("ups.power.nominal", "%s", "1500");
	        return;
	}

	/* Added: Doug Reynolds */
	if (!strcmp(abbr, "#BC1200    ")) {
		dstate_setinfo("ups.mfr", "%s", "CyberPower");
		dstate_setinfo("ups.model", "CPS1200VA %s", rating);
		dstate_setinfo("ups.runtime", "%s", "70");
		dstate_setinfo("ups.power.nominal", "%s", "1200");
		rmchar = 1;
		return;
	}

	if (!strcmp(abbr, "#1100VA    ")) {
	        dstate_setinfo("ups.mfr", "%s", "CyberPower");
	        dstate_setinfo("ups.model", "CPS1100VA %s", rating);
		dstate_setinfo("ups.runtime", "%s", "60");
		dstate_setinfo("ups.power.nominal", "%s", "1100");
	        return;
	}

	/* Added: Armin Diehl <diehl@...> 14Dec04 */
	if (!strcmp(abbr, "#1000VA    ")) {
		dstate_setinfo("ups.mfr", "%s", "MicroDowell");
		dstate_setinfo("ups.model", "B.Box BP 1000 %s", rating);
		dstate_setinfo("ups.runtime", "%s", "50");
		dstate_setinfo("ups.voltage", "%s", "1000");
		return;
	}
	        
	if (!strcmp(abbr, "#825VA     ")) {
		dstate_setinfo("ups.mfr", "%s", "CyberPower");
		dstate_setinfo("ups.model", "CPS825VA %s", rating);
		dstate_setinfo("ups.runtime", "%s", "29");
		dstate_setinfo("ups.power.nominal", "%s", "825");
		return;
	}

	/* Added: Armin Diehl <diehl@...> 14Dec04 */
	if (!strcmp(abbr, "#750VA     ")) {
		dstate_setinfo("ups.mfr", "%s", "MicroDowell");
		dstate_setinfo("ups.model", "B.Box BP 750 %s", rating);
		dstate_setinfo("ups.runtime", "%s", "29");
		dstate_setinfo("ups.voltage", "%s", "825");
		return;
	}

	if (!strcmp(abbr, "#500VA     ")) {
                dstate_setinfo("ups.mfr", "%s", "CyberPower");
                dstate_setinfo("ups.model", "OP500TE %s", rating);
                dstate_setinfo("ups.runtime", "%s", "16.5");
                dstate_setinfo("ups.power.nominal", "%s", "500");
		return;
	}

	dstate_setinfo("ups.mfr", "%s", "Unknown");
	dstate_setinfo("ups.model", "Unknown %s (%s)", abbr, rating);
	dstate_setinfo("ups.runtime", "%s", "1");
	dstate_setinfo("ups.power.nominal", "%s", "1");

	printf("Unknown model detected - please report this ID: '%s'\n", abbr);
}

static int instcmd(const char *cmdname, const char *extra)
{
	/* The following commands also appear to be valid on the CPS1100 */

	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send_pace(upsfd, UPSDELAY, "CT\r");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start")) {
		ser_send_pace(upsfd, UPSDELAY, "T\r");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}


static int get_ident(char *buf, size_t bufsize)
{
	int	i, ret;

	for (i = 0; i < MAXTRIES; i++) {
		ser_send_pace(upsfd, UPSDELAY, "\rP4\r");

		ret = ser_get_line(upsfd, buf, bufsize, ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		if (ret > 0)
			upsdebugx(2, "get_ident: got [%s]", buf);

		/* buf must start with # and be in the range [25-27] */
		if ((ret > 0) && (buf[0] == '#') && (strlen(buf) >= 25) &&
			(strlen(buf) <= 50))
			return 1;
		else {
			/* Try without leading \r */
			ser_send_pace(upsfd, UPSDELAY, "P4\r");
			ret = ser_get_line(upsfd, buf, bufsize, ENDCHAR, "",
				SER_WAIT_SEC, SER_WAIT_USEC);

			if (ret > 0)
				upsdebugx(2, "get_ident: got [%s]", buf);

			/* buf must start with # and be in the range [25-27] */
			if ((ret > 0) && (buf[0] == '#') &&
				(strlen(buf) >= 25) && (strlen(buf) <= 50))
				return 1;
		}

		sleep(1);
	}

	upslogx(LOG_INFO, "Giving up on hardware detection after %d tries",
		MAXTRIES);

	return 0;
}

static int scan_poll_values(char *buf)
{
	char values[20][200], *pos;
	int i = 0, battremain, length, rseconds;
	double rminutes;

/*	These are used to hold status of UPS.
 *	tmp1 = online/onbattery status
 */
	char *tmp1 = &buf[pollstatusmap[POLL_UPSSTATUS].begin];

	while ((pollstatusmap[i].end != 0))
	{
		pos = &buf[pollstatusmap[i].begin];
		length = pollstatusmap[i].end + 1 - pollstatusmap[i].begin;
		strncpy(values[i],pos,length);
		i++;
	}

	if ((*tmp1 & CPS_STAT_OL) && !(*tmp1 & CPS_STAT_OB)) 
		status_set("OL");

	if (*tmp1 & CPS_STAT_OB) 
		status_set("OB");

	if (*tmp1 & CPS_STAT_CAL)
		status_set("CAL");

	if (*tmp1 & CPS_STAT_LB) 
		status_set("LB");

	if (*tmp1 == 0)
		status_set("OFF");


	pos = values[3];
        battremain = strtol(dstate_getinfo("ups.power.nominal"),NULL,10) * (strtod(pos,NULL)/100);

	/* Figure out runtime minutes */
	rminutes = strtod(dstate_getinfo("ups.runtime"),NULL) * 
		((battremain * ( 1 - (strtod (values[2],NULL) / 100 ))) / 
		 strtol(dstate_getinfo("ups.power.nominal"),NULL,10));
	rseconds = ((int)(rminutes*100) - ((int)rminutes)*100) * 0.6 ;

        dstate_setinfo("input.voltage", "%g", strtod(values[0],NULL));
        dstate_setinfo("output.voltage", "%g", strtod(values[1],NULL));
        dstate_setinfo("ups.load", "%li", strtol(values[2],NULL,10));
        dstate_setinfo("input.frequency", "%g", strtod(values[5],NULL));
        dstate_setinfo("ups.temperature", "%li", strtol(values[4],NULL,10));
	dstate_setinfo("battery.charge", "%02.1f", strtod(values[3],NULL));
	dstate_setinfo("battery.runtime", "%2.0f:%02d", rminutes, rseconds);

	status_commit();
	dstate_dataok();
	return 0;
}

static void ups_ident(void)
{
	char	buf[256], *ptr, *com, *model, *rating;
	int	i;

	if (!get_ident(buf, sizeof(buf)))
		fatalx(EXIT_FAILURE, "Unable to detect a CyberPower text protocol UPS");

	model = rating = NULL;

	ptr = buf;

	/* Leaving this in place for future */
	for (i = 0; i < 2; i++) {
		com = strchr(ptr, ',');

		if (com)
			*com = '\0';

		switch (i) {
			case 0: model = ptr;
				break;
			case 1: rating = ptr;
				break;
			default:
				break;
		}

		if (com)
			ptr = com + 1;
	}

	if (!model)
		fatalx(EXIT_FAILURE, "Didn't get a valid ident string");

	model_set(model, rating);
}

static void ups_sync(void)
{
	char	buf[256];
	int	i, ret;

	for (i = 0; i < MAXTRIES; i++) {
		if (rmchar) {
			ser_send_pace(upsfd, UPSDELAY, "P4\r");
			upsdebugx(3, "ups_sync: send [%s]", "P4\\r");
		}
		else {
			ser_send_pace(upsfd, UPSDELAY, "\rP4\r");
			upsdebugx(3, "ups_sync: send [%s]", "\\rP4\\r");
		}

		ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);
		upsdebugx(3, "ups_sync: got ret %d [%s]", ret, buf);

		/* return once we get something that looks usable */
		if ((ret > 0) && (buf[0] == '#')) {
			upsdebugx(3, "ups_sync: got line beginning with #, looks usable, returning");
			return;
		}

		usleep(250000);
	}

	fatalx(EXIT_FAILURE, "Unable to detect a CyberPower text protocol UPS");
}

void upsdrv_initinfo(void)
{
	int ret;
	char temp[256];

	ups_sync();
	ups_ident();

	printf("Detected %s %s on %s\n", dstate_getinfo("ups.mfr"),
		dstate_getinfo("ups.model"), device_path);

	/* paranoia - cancel any shutdown that might already be running */
	ser_send_pace(upsfd, UPSDELAY, "C\r"); /* Need a readback so the first poll doesn't fail */
	ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "", SER_WAIT_SEC, SER_WAIT_USEC);

	upsh.instcmd = instcmd;

	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
}

static int ups_on_line(void)
{
	int	i, ret;
	char	temp[256];

	for (i = 0; i < MAXTRIES; i++) {
		if (rmchar) {
			ser_send_pace(upsfd, UPSDELAY, "D\r");
		}
		else {
			ser_send_pace(upsfd, UPSDELAY, "\rD\r");
		}

		ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		/* D must return 34 bytes starting with a # */
		if ((ret > 0) && (temp[0] == '#') && (strlen(temp) == 34)) {

			char * pos = &temp[pollstatusmap[POLL_UPSSTATUS].begin];

			if ((*pos & CPS_STAT_OL) && !(*pos & CPS_STAT_OB))
				return(1);    /* on line */

			return 0;	/* on battery */
		}

		sleep(1);
	}

	upslogx(LOG_ERR, "Status read failed: assuming on battery");

	return 0;	/* on battery */
}

void upsdrv_shutdown(void)
{
	int ret;
	char    buf[256];

	ups_sync();

	printf("The UPS will shut down in approximately one minute.\n");

	if (ups_on_line())
		printf("The UPS will restart in about one minute.\n");
	else
		printf("The UPS will restart when power returns.\n");

	/* Although this is straight from the bestups.c driver, the UPS
	 * does indeed shutdown correctly. */

	ser_send_pace(upsfd, UPSDELAY, "S01R0001\r");

	ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
					   SER_WAIT_SEC, SER_WAIT_USEC);

	if ((ret < 1) || (buf[0] != '#'))
		printf ("Warning: got unexpected reply to shutdown command, shutdown may fail\n");
}

void upsdrv_updateinfo(void)
{
	char	buf[256];
	int	ret;

	if (rmchar) {
		ret = ser_send_pace(upsfd, UPSDELAY, "D\r");
	}
	else {
		ret = ser_send_pace(upsfd, UPSDELAY, "\rD\r");
	}

	if (ret < 1) {
		ser_comm_fail("ser_send_pace failed");
		dstate_datastale();
		return;
	}

	/* these things need a long time to respond completely */
	usleep(200000);

	ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
		SER_WAIT_SEC, SER_WAIT_USEC);

	if (ret < 1) {
		ser_comm_fail(NULL);
		dstate_datastale();
		return;
	}

	if (ret < 34) {
		if (ret == 2) 		/* We need to retry this read right away    */
		{
			ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "", SER_WAIT_SEC, SER_WAIT_USEC);
			if (ret < 34) {
				ser_comm_fail("Poll failed: short read (got %d bytes)", ret);
				dstate_datastale();
				return;
			}
		} else {
			ser_comm_fail("Poll failed: short read (got %d bytes)", ret);
			dstate_datastale();
			return;
		}
	}

	if (ret > 34) {
		upslogx(LOG_INFO, "String too long...");
		ser_comm_fail("Poll failed: response too long (got %d bytes)",
			ret);
		dstate_datastale();
		return;
	}

	if (buf[0] != '#') {
		ser_comm_fail("Poll failed: invalid start character (got %02x)",
			buf[0]);
		dstate_datastale();
		return;
	}

	ser_comm_good();

	scan_poll_values(buf);

	status_init();

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
	experimental_driver = 1;	/* Causes a warning message to be printed */
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


