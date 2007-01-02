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

#include <sys/ioctl.h>

#include "cpsups.h"

#define DRV_VERSION ".06"

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

	/* Added: Doug Reynolds 29Dec06 */
        if (!strcmp(abbr, "#BC1200    ")) {
		dstate_setinfo("ups.mfr", "%s", "CyberPower");
		dstate_setinfo("ups.model", "CPS1200VA %s", rating);
		dstate_setinfo("ups.runtime", "%s", "60");
		dstate_setinfo("ups.power.nominal", "%s", "1200");
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
	dstate_setinfo("ups.model", "%s (%s)", abbr, rating);
	dstate_setinfo("ups.runtime", "%s", "1");
	dstate_setinfo("ups.power.nominal", "%s", "1");

	printf("Unknown model detected - please report this ID: '%s'\n", abbr);
}

static void clr_cps_serial(void)
{
	int dtr_bit = TIOCM_DTR;

	ioctl(upsfd, TIOCMBIC, &dtr_bit);
}

static void set_cps_serial(void)
{
        int dtr_bit = TIOCM_DTR;

        ioctl(upsfd, TIOCMBIS, &dtr_bit);
        tcflush(upsfd, TCIOFLUSH);
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
	int	a, rdy, ret;
	char	dr[256];
	
/*
 *              The cyberpower monitoring program sends the first \r seperate
 *              from the identity request.  It sends a \r, waits for a #2,
 *              then sends a P4\r
 *
 *              Hopefully, I will be able to duplicate that function here.
 *		This seems to be a better way for the driver to accurately
 *		detect unknown models.  In former state, it would fail to 
 *		detect anything.
 *
 */
	
	for (a = 0; a < MAXTRIES; a++) {
		
		set_cps_serial();

		ser_send_pace(upsfd, UPSDELAY, "\r");
 		
		ret = ser_get_line(upsfd, dr, sizeof(dr), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		clr_cps_serial();

		if ((dr[0] == '#') && (dr[1] == '2')) {
			upsdebugx(2, "get_ident: got [%s], ready to poll for ups model", dr);
			rdy = 1;
			break;
			}
		else {
			upsdebugx(2, "get_ident: got [%s], instead of '#2', retrying", dr);
			rdy = 0;
			}
		}


	if (rdy) {
		for (a = 0; a < MAXTRIES; a++) {

			set_cps_serial();
			ser_send_pace(upsfd, UPSDELAY, "P4\r");
	
			ret = ser_get_line(upsfd, buf, bufsize, ENDCHAR, "",
				SER_WAIT_SEC, SER_WAIT_USEC);
				
			clr_cps_serial();

			set_cps_serial();
			ser_send_pace(upsfd, UPSDELAY, "P4\r");

			ret = ser_get_line(upsfd, buf, bufsize, ENDCHAR, "",
				SER_WAIT_SEC, SER_WAIT_USEC);

			clr_cps_serial();

			if (ret > 0)
				upsdebugx(2, "get_ident: got [%s]", buf);

			/* buf must start with # and be in the range [25-27] */

			if ((ret > 0) && (buf[0] == '#') && (strlen(buf) >= 25) && (strlen(buf) <= 50)) {
				rdy = 1;
				break;
				}
			else {
				rdy = 0;
				upsdebugx(2, "get_ident: got [%s], instead of '#2', retrying", buf);
				}
			}
		}
	
	if (rdy) {
		for (a = 0; a < MAXTRIES; a++) {

			set_cps_serial();
			ser_send_pace(upsfd, UPSDELAY, "P3\r");

			ret = ser_get_line(upsfd, dr, sizeof(dr), ENDCHAR, "",
				SER_WAIT_SEC, SER_WAIT_USEC);

			clr_cps_serial();

			if ((ret > 0) && (dr[0] == '#') && (strlen(dr) > 2)) {

				rdy = 1;
				upsdebugx(2, "get_ident: P3 smart mode ok");
				break;
				}
			else {
				rdy = 0;
				upsdebugx(2, "get_ident: got [%s], not smart mode ok", dr);
				}
			}
		}

	if (rdy) {
		for (a = 0; a < MAXTRIES; a++) {
		
		set_cps_serial();
		ser_send_pace(upsfd, UPSDELAY, "P2\r");

		ret = ser_get_line(upsfd, dr, sizeof(dr), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		clr_cps_serial();

		if ((ret > 0) && (dr[0] == '#') && (strlen(dr) > 2)) {

			rdy = 1;
			upsdebugx(2, "get_ident: P2 smart mode ok");
			break;
			}
		else {
			rdy = 0;
			upsdebugx(2, "get_ident: got [%s], not smart mode ok", dr);
			}
		}	
	}

	if (rdy) {
		for (a = 0; a < MAXTRIES; a++) {


		set_cps_serial();
		ser_send_pace(upsfd, UPSDELAY, "P1\r");

		ret = ser_get_line(upsfd, dr, sizeof(dr), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		clr_cps_serial();

		if ((ret > 0) && (dr[0] == '#') && (strlen(dr) > 2)) {

			rdy = 1;
			upsdebugx(2, "get_ident: P1 smart mode ok");
			break;
			}
		else {
			rdy = 0;
			upsdebugx(2, "get_ident: got [%s], not smart mode ok", dr);                        
			}
		}
	}

	if (rdy) {
		for (a = 0; a < MAXTRIES; a++) {
		
		set_cps_serial();
		ser_send_pace(upsfd, UPSDELAY, "P7\r");

		ret = ser_get_line(upsfd, dr, sizeof(dr), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		clr_cps_serial();

		if ((ret > 0) && (dr[0] == '#') && (strlen(dr) > 2)) {

			rdy = 1;
			upsdebugx(2, "get_ident: P7 smart mode ok");
			break;
			}
		else {
			rdy = 0;
			upsdebugx(2, "get_ident: got [%s], not smart mode ok", dr);
			}
		}
	}

	if (rdy) {
		for (a = 0; a < MAXTRIES; a++) {

		set_cps_serial();
		ser_send_pace(upsfd, UPSDELAY, "P6\r");

		ret = ser_get_line(upsfd, dr, sizeof(dr), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		clr_cps_serial();

		if ((ret > 0) && (dr[0] == '#') && (strlen(dr) > 2)) {

			rdy = 1;
			upsdebugx(2, "get_ident: P6 smart mode ok");
			break;
			}
		else {
			rdy = 0;
			upsdebugx(2, "get_ident: got [%s], not smart mode ok", dr);
			}
		}
	}

	if (rdy) {
		for (a = 0; a < MAXTRIES; a++) {
		
		set_cps_serial();
		ser_send_pace(upsfd, UPSDELAY, "P8\r");

		ret = ser_get_line(upsfd, dr, sizeof(dr), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		clr_cps_serial();

		if ((ret > 0) && (dr[0] == '#') && (strlen(dr) > 2)) {

			rdy = 1;
			upsdebugx(2, "get_ident: P8 smart mode ok");
			break;
			}
		else {
			rdy = 0;
			upsdebugx(2, "get_ident: got [%s], not smart mode ok", dr);
			}
		}
	}


	if (rdy) {
		for (a = 0; a < MAXTRIES; a++) {
			
			set_cps_serial();
			ser_send_pace(upsfd, UPSDELAY, "P9\r");
			
			ret = ser_get_line(upsfd, dr, sizeof(dr), ENDCHAR, "",
				SER_WAIT_SEC, SER_WAIT_USEC);

			clr_cps_serial();
			
			if ((ret > 0) && (dr[0] == '#') && (strlen(dr) > 2) && (strlen(dr) < 8)) {
				
				rdy = 1;
				upsdebugx(2, "get_ident: P9 smart mode ok");
				break;
				}
			else {
				rdy = 0;
				upsdebugx(2, "get_ident: got [%s], not smart mode ok", dr);
				} 

			}
		}	

	
	
	if (!rdy) {
		upslogx(LOG_INFO, "Giving up on hardware detection after %d tries", MAXTRIES);
		return 0;
		}
	else	{
		upsdebugx(2, "get_ident: ups initialized and ready");
		return 1;
		}
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
		fatalx("Unable to detect a CyberPower text protocol UPS");

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
		fatalx("Didn't get a valid ident string");

	model_set(model, rating);
}


void upsdrv_initinfo(void)
{
	int ret;
	char temp[256];

/*	ups_sync(); 
 *
 *	DJR 12-29-2006
 *	Looking at the ups_sync routine makes me think that ups_sync and ups_ident
 *	does the same thing.  Currently, I am remming it out, seems to work dandy
 *	without it.
 */

	ups_ident();

	printf("Detected %s %s on %s\n", dstate_getinfo("ups.mfr"),
		dstate_getinfo("ups.model"), device_path);

	/* paranoia - cancel any shutdown that might already be running */
	
	set_cps_serial();
	ser_send_pace(upsfd, UPSDELAY, "D\r"); /* Need a readback so the first poll doesn't fail */
	ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "", SER_WAIT_SEC, SER_WAIT_USEC);
	
	clr_cps_serial();
	
	set_cps_serial();
	ser_send_pace(upsfd, UPSDELAY, "C\r"); /* Need a readback so the first poll doesn't fail */
	ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "", SER_WAIT_SEC, SER_WAIT_USEC);
	
	clr_cps_serial();

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
		
		set_cps_serial();
		ser_send_pace(upsfd, UPSDELAY, "D\r");

		ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);
		
		clr_cps_serial();		

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

/*	Don't need ups_sync() here, initialization has been done a long
 *	time ago!
 *	ups_sync();
 */

	printf("The UPS will shut down in approximately one minute.\n");

	if (ups_on_line())
		printf("The UPS will restart in about one minute.\n");
	else
		printf("The UPS will restart when power returns.\n");

	/* Although this is straight from the bestups.c driver, the UPS
	 * does indeed shutdown correctly. */

	/* DJR - after hexedited the powerpanel, I believe it is
		S00R0000 instead of S01R0001	*/
	/* DJR - after more testing, S00R0000 seems to kinda work,
		S01R0001 does work, and Z02 is what powerpanel uses...so... */

	set_cps_serial();
	ser_send_pace(upsfd, UPSDELAY, "Z02\r");

	ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
					   SER_WAIT_SEC, SER_WAIT_USEC);

	clr_cps_serial();

	if ((ret >= 2) && (buf[0] == '#') && (buf[1] == '0'))
		upsdebugx(2,"upsdrv_shutdown: got [%s], good to shutdown", buf);
	else {		
		upsdebugx(2,"upsdrv_shutdown: got [%s], shutdown failed", buf);
		printf ("Warning: got unexpected reply to shutdown command, shutdown may fail\n");
	}
}

void upsdrv_updateinfo(void)
{
	char	buf[256];
	int	ret;

/*
 *	Only sending D\r here.  The leading \r is not needed
 *
 */

	set_cps_serial();
	ret = ser_send_pace(upsfd, UPSDELAY, "D\r");

	if (ret < 1) {
		ser_comm_fail("ser_send_pace failed");
		dstate_datastale();
		return;
	}

	/* these things need a long time to respond completely */

/*	
 *	usleep(200000);
 *	DJR 12-29-2006
 *	the cps 1200 doesn't seem to, the ups_on_line works without the delay
 */

	ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
		SER_WAIT_SEC, SER_WAIT_USEC);
	
	clr_cps_serial();
	
	if (ret < 1) {
		ser_comm_fail(NULL);
		dstate_datastale();
		return;
	}

	if ((ret >= 34) && (ret <= 36))	 {	/* removing the if to see if we got a #2    */
		
		upsdebugx(2,"upsdrv_updateinfo: got [%s]", buf);

	} 
	else {
		upsdebugx(2,"upsdrv_updateinfo: got [%s]", buf);
		ser_comm_fail("Poll failed: short read (got %d bytes)", ret);
		dstate_datastale();
		return;
	}
	

	if (ret > 36) {
		upslogx(LOG_INFO, "String too long...");
		upsdebugx(2,"upsdrv_updateinfo: got [%s]", buf);
		ser_comm_fail("Poll failed: response too long (got %d bytes)",
			ret);
		dstate_datastale();
		return;
	}

	if (buf[0] != '#') {
		upsdebugx(2,"upsdrv_updateinfo: got [%s]", buf);
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
