/* blazer.c - model specific routines for Blazer UPS models

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
                 2002  Phil Hutton <blazer-driver@hutton.sh>
                 2003  Arnaud Quette <arnaud.quette@free.fr>

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

#define DRV_VERSION	"0.07"

#define ENDCHAR		13	/* replies end with CR */
#define UPSDELAY 	5
#define MAXTRIES	10
#define SENDDELAY	100000	/* 100 ms delay between chars on transmit */
#define SER_WAIT_SEC	3	/* allow 3.0 sec for responses */
#define SER_WAIT_USEC	0

static	float	lowvolt = 0, highvolt = 0, voltrange = 0;
static	int	poll_failures = 0;
static	int	inverted_bypass_bit = 0;


static int instcmd(const char *cmdname, const char *extra)
{
	/* Stop battery test */
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send_pace(upsfd, SENDDELAY, "CT\r");
		return STAT_INSTCMD_HANDLED;
	}

	/* Start battery test */
	if (!strcasecmp(cmdname, "test.battery.start")) {
		ser_send_pace(upsfd, SENDDELAY, "T\r");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
	dstate_setinfo("ups.mfr", "Centralion");
	dstate_setinfo("ups.model", "Blazer");
  
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");

	printf("Detected UPS on %s\n", device_path);

	upsh.instcmd = instcmd;

	/* paranoia - cancel any shutdown that might already be running */
	ser_send_pace(upsfd, SENDDELAY, "C\r");
}

/* is this really necessary to make this hardware work? */
static void setup_serial(void)
{
	struct	termios	tio;

	if (tcgetattr(upsfd, &tio) == -1)
		fatal("tcgetattr");

	tio.c_iflag = IXON | IXOFF;
	tio.c_oflag = 0;
	tio.c_cflag = (CS8 | CREAD | HUPCL | CLOCAL);
	tio.c_lflag = 0;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0; 

#ifdef HAVE_CFSETISPEED
	cfsetispeed(&tio, B2400);
	cfsetospeed(&tio, B2400);
#else
#error This system lacks cfsetispeed() and has no other means to set the speed
#endif

	if (tcsetattr(upsfd, TCSANOW, &tio) == -1)
		fatal("tcsetattr");
}

static void ups_sync(void)
{
	char	buf[256];
	int	tries = 0, ret;

	printf("Syncing with UPS: ");
	fflush(stdout);

	for (;;) {
		tries++;
		if (tries > MAXTRIES)
			fatalx("\nFailed - giving up...");

		printf(".");
		fflush(stdout);

		ret = ser_send_pace(upsfd, SENDDELAY, "\rQ1\r");

		if (ret < 1)
			continue;
		
		printf(".");
		fflush(stdout);
		sleep(UPSDELAY);
		printf(".");
		fflush(stdout);

		ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		if ((ret > 0) && (buf[0] == '('))
			break;
	}

	printf(" done\n");
}

void upsdrv_shutdown(void)
{
	ser_send_pace(upsfd, SENDDELAY, "S01R0001\r");
}

static void ups_ident(void)
{
	char	buf[256];
	int	tries = 0, ret;
	int	ratecurrent;
	float	ratevolt, ratefreq;

	printf("Identifying UPS: ");
	fflush (stdout);

	for (;;) {
		tries++;
		if (tries > MAXTRIES) {
			upsdebugx(2, "Failed - giving up...");
			exit (1);
		}

		printf(".");
		fflush(stdout);
		ret = ser_send_pace(upsfd, SENDDELAY, "\rF\r");

		if (ret < 1)
			continue;

		printf(".");
		fflush(stdout);
		sleep(UPSDELAY);
		printf(".");
		fflush(stdout);

		ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		if ((ret > 0) && (buf[0] == '#') && (strlen(buf) == 21))
			break;
	}

	printf(" done\n");

	sscanf(buf, "%*c %f %d %f %f", &ratevolt, &ratecurrent, 
		&lowvolt, &ratefreq);
	upsdebugx(2, "UPS is rated at %.2fV, %dA, %.2fHz.\n",
		ratevolt, ratecurrent, ratefreq);

	/* Not sure exactly how the battery voltage from the "F" command
	 * should be handled.  The lowest voltage I've seen before the
	 * UPS warns is 22V, so we'll try subtracting 2 from lowvolt.*/
	lowvolt -= 2;

	highvolt = 27.4;

	voltrange = highvolt - lowvolt;
}

static void pollfail(const char *why)
{
	poll_failures++;

	/* ignore the first few since these UPSes tend to drop characters */
	if (poll_failures == 3)
		upslogx(LOG_ERR, "%s", why);

	return;
}

void upsdrv_updateinfo(void)
{
	char	utility[16], outvolt[16], loadpct[16], acfreq[16], 
		battvolt[16], upstemp[16], pstat[16], buf[256];
	float	bvoltp;
	int	ret;

	ret = ser_send_pace(upsfd, SENDDELAY, "\rQ1\r");

	if (ret < 1) {
		pollfail("Poll failed: send status request failed");
		dstate_datastale();
		return;
	}

	sleep(UPSDELAY); 

	ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
		SER_WAIT_SEC, SER_WAIT_USEC);

	if (ret < 1) {
		pollfail("Poll failed: read failed");
		dstate_datastale();
		return;
	}

	if (strlen(buf) < 46) {
		pollfail("Poll failed: short read from UPS");
		dstate_datastale();
		return;
	}

	if (strlen(buf) > 46) {
		pollfail("Poll failed: oversized read from UPS");
		dstate_datastale();
		return;
	}

	if (buf[0] != '(') {
		pollfail("Poll failed: invalid start character");
		return;
	}

	/* only say this if it got high enough to log a failure note */
	if (poll_failures >= 3)
		upslogx(LOG_NOTICE, "UPS poll succeeded");

	poll_failures = 0;

	sscanf(buf, "%*c%s %*s %s %s %s %s %s %s", utility, outvolt, 
		loadpct, acfreq, battvolt, upstemp, pstat);
	
	bvoltp = ((atof (battvolt) - lowvolt) / voltrange) * 100.0;

	if (bvoltp > 100.0)
		bvoltp = 100.0;

	dstate_setinfo("input.voltage", "%s", utility);
	dstate_setinfo("input.frequency", "%s", acfreq);
	dstate_setinfo("output.voltage", "%s", outvolt);
	dstate_setinfo("battery.charge", "%02.1f", bvoltp);
	dstate_setinfo("battery.voltage", "%s", battvolt);
	dstate_setinfo("ups.load", "%s", loadpct);

	status_init();

	if (pstat[0] == '0') {
		status_set("OL");		/* on line */

		/* only allow these when OL since they're bogus when OB */

		if (pstat[2] == (inverted_bypass_bit ? '0' : '1')) {
			/* boost or trim in effect */
			if (atof(utility) < atof(outvolt))
				status_set("BOOST");

			if (atof(utility) > atof(outvolt))
				status_set("TRIM");
		}

	} else {
		status_set("OB");		/* on battery */
	}

	if (pstat[1] == '1')
		status_set("LB");		/* low battery */

	status_commit();
	dstate_dataok();
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - Blazer UPS driver %s (%s)\n", 
		DRV_VERSION, UPS_VERSION);
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);

	setup_serial();

	ups_sync();
	ups_ident();
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
