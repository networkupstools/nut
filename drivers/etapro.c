/* etapro.c - model specific routines for ETA UPS

   Copyright (C) 2002  Marek Michalkiewicz  <marekm@amelek.gda.pl>

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
   This driver is for the ETA UPS (http://www.eta.com.pl/) with the
   "PRO" option (available at small added cost, highly recommended).
   All units (even without that option) should also work in "dumb"
   mode with the genericups driver (type 7 or 10), but in that mode
   shutdown only works when running on battery.

   Tested with ETA mini+UPS 720 PRO.  Thanks to ETA for help with
   protocol documentation, no free UPS though (but they still can
   send me another one if they like me ;-).

   Shutdown should work even when on line, so this should help avoid
   power races (system remaining in halted or "ATX standby" state,
   requiring manual intervention).  Delay from power off to power on
   can be set in software, currently hardcoded to 15 seconds.

   Instant commands CMD_OFF and CMD_ON should work (not tested yet).
   Be careful with CMD_OFF - it turns off the load after one second.

   Known issues:
    - larger units (>= 1000VA) have a 24V battery, so the battery
      voltage reported should be multiplied by 2 if the model
      string indicates such a larger unit.
    - load percentage is only measured when running on battery, and
      is reported as 0 when on line.  This seems to be a hardware
      limitation of the UPS, so we can't do much about it...
    - UPS does not provide any remaining battery charge (or time at
      current load) information, but we should be able to estimate it
      based on battery voltage, load percentage and UPS model.
    - error handling not tested (we assume that the UPS is always
      correctly connected to the serial port).
 */

#include "main.h"
#include "serial.h"

#define DRIVER_NAME	"ETA PRO driver"
#define DRIVER_VERSION	"0.04"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Marek Michalkiewicz <marekm@amelek.gda.pl>",
	DRV_STABLE,
	{ NULL }
};

static int
etapro_get_response(const char *resp_type)
{
	char tmp[256];
	char *cp;
	unsigned int n, val;

	/* Read until a newline is found or there is no room in the buffer.
	   Unlike ser_get_line(), don't discard the following characters
	   because we have to handle multi-line responses.  */
	n = 0;
	while (ser_get_char(upsfd, (unsigned char *)&tmp[n], 1, 0) == 1) {
		if (n >= sizeof(tmp) - 1 || tmp[n] == '\n')
			break;
		n++;
	}
	tmp[n] = '\0';
	if (n == 0) {
		upslogx(LOG_ERR, "no response from UPS");
		return -1;
	}
	/* Search for start of response (skip any echoed back command).  */
	cp = strstr(tmp, resp_type);
	if (!cp || *cp == '\0' || cp[strlen(cp) - 1] != '\r') {
		upslogx(LOG_ERR, "bad response (%s)", tmp);
		return -1;
	}
	cp[strlen(cp) - 1] = '\0';  /* remove the CR */

	switch (cp[1]) {
		/* Handle ASCII text responses directly here.  */
	case 'R':
		dstate_setinfo("ups.mfr", "%s", cp + 2);
		return 0;
	case 'S':
		dstate_setinfo("ups.model", "%s", cp + 2);
		return 0;
	case 'T':
		dstate_setinfo("ups.mfr.date", "%s", cp + 2);
		return 0;
	}
	/* Handle all other responses as hexadecimal numbers.  */
	val = 0;
	if (sscanf(cp + 2, "%x", &val) != 1) {
		upslogx(LOG_ERR, "bad response format (%s)", tmp);
		return -1;
	}
	return val;
}

static void
etapro_set_on_timer(int seconds)
{
	int x;

	if (seconds == 0) {  /* cancel the running timer */
		ser_send(upsfd, "RS\r");
		x = etapro_get_response("SV");
		if (x == 0x30)
			return;  /* OK */
	} else {
		if (seconds > 0x7fff) {  /* minutes */
			seconds = (seconds + 59) / 60;
			if (seconds > 0x7fff)
				seconds = 0x7fff;
			printf("UPS on in %d minutes\n", seconds);
			seconds |= 0x8000;
		} else {
			printf("UPS on in %d seconds\n", seconds);
		}

		ser_send(upsfd, "RN%04X\r", seconds);
		x = etapro_get_response("SV");
		if (x == 0x20)
			return;  /* OK */
	}
	upslogx(LOG_ERR, "etapro_set_on_timer: error, status=0x%02x", x);
}

static void
etapro_set_off_timer(int seconds)
{
	int x;

	if (seconds == 0) {  /* cancel the running timer */
		ser_send(upsfd, "RR\r");
		x = etapro_get_response("SV");
		if (x == 0x10)
			return;  /* OK */
	} else {
		if (seconds > 0x7fff) {  /* minutes */
			seconds /= 60;
			if (seconds > 0x7fff)
				seconds = 0x7fff;
			printf("UPS off in %d minutes\n", seconds);
			seconds |= 0x8000;
		} else {
			printf("UPS off in %d seconds\n", seconds);
		}

		ser_send(upsfd, "RO%04X\r", seconds);
		x = etapro_get_response("SV");
		if (x == 0)
			return;  /* OK */
	}
	upslogx(LOG_ERR, "etapro_set_off_timer: error, status=0x%02x", x);
}

static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "load.off")) {
		etapro_set_off_timer(1);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "load.on")) {
		etapro_set_on_timer(1);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.return")) {
		upsdrv_shutdown();
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

void
upsdrv_initinfo(void)
{
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	dstate_addcmd("shutdown.return");

	/* First command after power on returns junk - ignore it.  */
	ser_send(upsfd, "RI\r");
	sleep(1);

	upsdrv_updateinfo();

	upsh.instcmd = instcmd;
}

void
upsdrv_updateinfo(void)
{
	int x, flags;
	double utility, outvolt, battvolt, loadpct;

	ser_flush_in(upsfd, "", nut_debug_level);
	ser_send(upsfd, "RI\r");  /* identify */

	x = etapro_get_response("SR");  /* manufacturer */
	if (x < 0) {
		dstate_datastale();
		return;
	}
	x = etapro_get_response("SS");  /* model */
	if (x < 0) {
		dstate_datastale();
		return;
	}
	x = etapro_get_response("ST");  /* mfr date */
	if (x < 0) {
		dstate_datastale();
		return;
	}
	x = etapro_get_response("SU");  /* UPS ident */
	if (x < 0) {
		dstate_datastale();
		return;
	}

	ser_send(upsfd, "RP\r");  /* read measurements */

	x = etapro_get_response("SO");  /* status flags */
	if (x < 0) {
		dstate_datastale();
		return;
	}
	flags = x;

	x = etapro_get_response("SG");  /* input voltage, 0xFF = 270V */
	if (x < 0) {
		dstate_datastale();
		return;
	}
	utility = (270.0 / 255) * x;

	x = etapro_get_response("SH");  /* output voltage, 0xFF = 270V */
	if (x < 0) {
		dstate_datastale();
		return;
	}
	outvolt = (270.0 / 255) * x;

	x = etapro_get_response("SI");  /* battery voltage, 0xFF = 14V */
	if (x < 0) {
		dstate_datastale();
		return;
	}

	/* TODO: >= 1000VA models have a 24V battery (max 28V) - check
	   the model string returned by the RI command.  */
	battvolt = (14.0 / 255) * x;

	x = etapro_get_response("SL");  /* load (on battery), 0xFF = 150% */
	if (x < 0) {
		dstate_datastale();
		return;
	}
	loadpct = (150.0 / 255) * x;

	x = etapro_get_response("SN");  /* time running on battery */
	if (x < 0) {
		dstate_datastale();
		return;
	}
	/* This is the time how long the UPS has been running on battery
	   (in seconds, reset to zero after power returns), but there
	   seems to be no variable defined for this yet...  */

	status_init();

	if (!(flags & 0x02))
		status_set("OFF");
	else if (flags & 0x01)
		status_set("OL");
	else
		status_set("OB");

	if (!(flags & 0x04))
		status_set("LB");

	/* TODO bit 3: 1 = ok, 0 = fault */

	if (flags & 0x10)
		status_set("BOOST");

	if (loadpct > 100.0)
		status_set("OVER");

	/* Battery voltage out of range (lower than LB, or too high).  */
	if (flags & 0x20)
		status_set("RB");

	/* TODO bit 6: 1 = charging, 0 = full */

	status_commit();

	dstate_setinfo("input.voltage", "%03.1f", utility);
	dstate_setinfo("output.voltage", "%03.1f", outvolt);
	dstate_setinfo("battery.voltage", "%02.2f", battvolt);
	dstate_setinfo("ups.load", "%03.1f", loadpct);

	dstate_dataok();
}

/* TODO: delays should be tunable, the UPS supports max 32767 minutes.  */

/* Shutdown command to off delay in seconds.  */
#define SHUTDOWN_GRACE_TIME 10

/* Shutdown to return delay in seconds.  */
#define SHUTDOWN_TO_RETURN_TIME 15

void
upsdrv_shutdown(void)
{
	etapro_set_on_timer(SHUTDOWN_GRACE_TIME + SHUTDOWN_TO_RETURN_TIME);
	etapro_set_off_timer(SHUTDOWN_GRACE_TIME);
}

void
upsdrv_help(void)
{
}

void
upsdrv_makevartable(void)
{
}

void
upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B1200);

	ser_set_dtr(upsfd, 0);
	ser_set_rts(upsfd, 1);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
