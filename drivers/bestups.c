/* bestups.c - model specific routines for Best-UPS Fortress models

   OBSOLETION WARNING: Please to not base new development on this
   codebase, instead create a new subdriver for nutdrv_qx which
   generally covers all Megatec/Qx protocol family and aggregates
   device support from such legacy drivers over time.

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

   ID config option by Jason White <jdwhite@jdwhite.org>

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

#define DRIVER_NAME	"Best UPS driver"
#define DRIVER_VERSION	"1.06"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Russell Kroll <rkroll@exploits.org>\n" \
	"Jason White <jdwhite@jdwhite.org>",
	DRV_STABLE,
	{ NULL }
};

#define ENDCHAR  13	/* replies end with CR */
#define MAXTRIES 5
#define UPSDELAY 50000	/* 50 ms delay required for reliable operation */

#define SER_WAIT_SEC	3	/* allow 3.0 sec for ser_get calls */
#define SER_WAIT_USEC	0

static	float	lowvolt = 0, highvolt = 0;
static	int	battvoltmult = 1;
static	int	inverted_bypass_bit = 0;

static void model_set(const char *abbr, const char *rating)
{
	if (!strcmp(abbr, "FOR")) {
		dstate_setinfo("ups.mfr", "%s", "Best Power");
		dstate_setinfo("ups.model", "Fortress %s", rating);
		return;
	}

	if (!strcmp(abbr, "FTC")) {
		dstate_setinfo("ups.mfr", "%s", "Best Power");
		dstate_setinfo("ups.model", "Fortress Telecom %s", rating);
		return;
	}

	if (!strcmp(abbr, "PRO")) {
		dstate_setinfo("ups.mfr", "%s", "Best Power");
		dstate_setinfo("ups.model", "Patriot Pro %s", rating);
		inverted_bypass_bit = 1;
		return;
	}

	if (!strcmp(abbr, "PR2")) {
		dstate_setinfo("ups.mfr", "%s", "Best Power");
		dstate_setinfo("ups.model", "Patriot Pro II %s", rating);
		inverted_bypass_bit = 1;
		return;
	}

	if (!strcmp(abbr, "325")) {
		dstate_setinfo("ups.mfr", "%s", "Sola Australia");
		dstate_setinfo("ups.model", "Sola 325 %s", rating);
		return;
	}

	if (!strcmp(abbr, "520")) {
		dstate_setinfo("ups.mfr", "%s", "Sola Australia");
		dstate_setinfo("ups.model", "Sola 520 %s", rating);
		return;
	}

	if (!strcmp(abbr, "610")) {
		dstate_setinfo("ups.mfr", "%s", "Best Power");
		dstate_setinfo("ups.model", "610 %s", rating);
		return;
	}

	if (!strcmp(abbr, "620")) {
		dstate_setinfo("ups.mfr", "%s", "Sola Australia");
		dstate_setinfo("ups.model", "Sola 620 %s", rating);
		return;
	}

	if (!strcmp(abbr, "AX1")) {
		dstate_setinfo("ups.mfr", "%s", "Best Power");
		dstate_setinfo("ups.model", "Axxium Rackmount %s", rating);
		return;
	}

	dstate_setinfo("ups.mfr", "%s", "Unknown");
	dstate_setinfo("ups.model", "Unknown %s (%s)", abbr, rating);

	printf("Unknown model detected - please report this ID: '%s'\n", abbr);
}

static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send_pace(upsfd, UPSDELAY, "CT\r");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start")) {
		ser_send_pace(upsfd, UPSDELAY, "T\r");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

static int get_ident(char *buf, size_t bufsize)
{
	int	i;
	ssize_t	ret;
	char	*ID;

	ID = getval("ID");	/* user-supplied override from ups.conf */

	if (ID) {
		upsdebugx(2, "NOTE: using user-supplied ID response");
		snprintf(buf, bufsize, "%s", ID);
		return 1;
	}

	for (i = 0; i < MAXTRIES; i++) {
		ser_send_pace(upsfd, UPSDELAY, "\rID\r");

		ret = ser_get_line(upsfd, buf, bufsize, ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		if (ret > 0)
			upsdebugx(2, "get_ident: got [%s]", buf);

		/* buf must start with ( and be in the range [25-27] */
		if ((ret > 0) && (buf[0] != '(') && (strlen(buf) >= 25) &&
			(strlen(buf) <= 27))
			return 1;

		sleep(1);
	}

	upslogx(LOG_INFO, "Giving up on hardware detection after %d tries",
		MAXTRIES);

	return 0;
}

static void ups_ident(void)
{
	int	i;
	char	buf[256], *ptr;
	char	*model = NULL, *rating = NULL;

	if (!get_ident(buf, sizeof(buf))) {
		fatalx(EXIT_FAILURE, "Unable to detect a Best/SOLA or Phoenix protocol UPS");
	}

	/* FOR,750,120,120,20.0,27.6 */
	ptr = strtok(buf, ",");

	for (i = 0; ptr; i++) {

		switch (i)
		{
		case 0:
			model = ptr;
			break;

		case 1:
			rating = ptr;
			break;

		case 2:
			dstate_setinfo("input.voltage.nominal", "%d", atoi(ptr));
			break;

		case 3:
			dstate_setinfo("output.voltage.nominal", "%d", atoi(ptr));
			break;

		case 4:
			lowvolt = atof(ptr);
			break;

		case 5:
			highvolt = atof(ptr);
			break;
		}

		ptr = strtok(NULL, ",");
	}

	if ((!model) || (!rating)) {
		fatalx(EXIT_FAILURE, "Didn't get a valid ident string");
	}

	model_set(model, rating);

	/* Battery voltage multiplier */
	ptr = getval("battvoltmult");

	if (ptr) {
		battvoltmult = atoi(ptr);
	}

	/* Lookup the nominal battery voltage (should be between lowvolt and highvolt */
	for (i = 0; i < 8; i++) {
		const int	nominal[] = { 2, 6, 12, 24, 36, 48, 72, 96 };

		if ((lowvolt < nominal[i]) && (highvolt > nominal[i])) {
			dstate_setinfo("battery.voltage.nominal", "%d", battvoltmult * nominal[i]);
			break;
	 	}
	}

	ptr = getval("nombattvolt");

	if (ptr) {
		highvolt = atof(ptr);
	}
}

static void ups_sync(void)
{
	char	buf[256];
	int	i;
	ssize_t	ret;

	for (i = 0; i < MAXTRIES; i++) {
		ser_send_pace(upsfd, UPSDELAY, "\rQ1\r");

		ret = ser_get_line(upsfd, buf, sizeof(buf), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		/* return once we get something that looks usable */
		if ((ret > 0) && (buf[0] == '('))
			return;

		usleep(250000);
	}

	fatalx(EXIT_FAILURE, "Unable to detect a Best/SOLA or Phoenix protocol UPS");
}

void upsdrv_initinfo(void)
{
	ups_sync();
	ups_ident();

	printf("Detected %s %s on %s\n", dstate_getinfo("ups.mfr"),
		dstate_getinfo("ups.model"), device_path);

	/* paranoia - cancel any shutdown that might already be running */
	ser_send_pace(upsfd, UPSDELAY, "C\r");

	upsh.instcmd = instcmd;

	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
}

static int ups_on_line(void)
{
	int	i;
	ssize_t	ret;
	char	temp[256], pstat[32];

	for (i = 0; i < MAXTRIES; i++) {
		ser_send_pace(upsfd, UPSDELAY, "\rQ1\r");

		ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, "",
			SER_WAIT_SEC, SER_WAIT_USEC);

		/* Q1 must return 46 bytes starting with a ( */
		if ((ret > 0) && (temp[0] == '(') && (strlen(temp) == 46)) {

			sscanf(temp, "%*s %*s %*s %*s %*s %*s %*s %s", pstat);

			if (pstat[0] == '0')
				return 1;	/* on line */

			return 0;	/* on battery */
		}

		sleep(1);
	}

	upslogx(LOG_ERR, "Status read failed: assuming on battery");

	return 0;	/* on battery */
}

void upsdrv_shutdown(void)
{
	printf("The UPS will shut down in approximately one minute.\n");

	if (ups_on_line())
		printf("The UPS will restart in about one minute.\n");
	else
		printf("The UPS will restart when power returns.\n");

	ser_send_pace(upsfd, UPSDELAY, "S01R0001\r");
}

void upsdrv_updateinfo(void)
{
	char	involt[16], outvolt[16], loadpct[16], acfreq[16],
		battvolt[16], upstemp[16], pstat[16], buf[256];
	float	bvoltp;
	ssize_t	ret;

	ret = ser_send_pace(upsfd, UPSDELAY, "\rQ1\r");

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
		ser_comm_fail("Poll failed: %s", ret ? strerror(errno) : "timeout");
		dstate_datastale();
		return;
	}

	if (ret < 46) {
		ser_comm_fail("Poll failed: short read (got %zd bytes)", ret);
		dstate_datastale();
		return;
	}

	if (ret > 46) {
		ser_comm_fail("Poll failed: response too long (got %zd bytes)",
			ret);
		dstate_datastale();
		return;
	}

	if (buf[0] != '(') {
		ser_comm_fail("Poll failed: invalid start character (got %02x)",
			buf[0]);
		dstate_datastale();
		return;
	}

	ser_comm_good();

	sscanf(buf, "%*c%s %*s %s %s %s %s %s %s", involt, outvolt,
		loadpct, acfreq, battvolt, upstemp, pstat);

	/* Guesstimation of battery charge left (inaccurate) */
	bvoltp = 100 * (atof(battvolt) - lowvolt) / (highvolt - lowvolt);

	if (bvoltp > 100) {
		bvoltp = 100;
	}

	dstate_setinfo("battery.voltage", "%.1f", battvoltmult * atof(battvolt));
	dstate_setinfo("input.voltage", "%s", involt);
	dstate_setinfo("output.voltage", "%s", outvolt);
	dstate_setinfo("ups.load", "%s", loadpct);
	dstate_setinfo("input.frequency", "%s", acfreq);

	if(upstemp[0] != 'X') {
		dstate_setinfo("ups.temperature", "%s", upstemp);
	}

	dstate_setinfo("battery.charge", "%02.1f", bvoltp);

	status_init();

	if (pstat[0] == '0') {
		status_set("OL");		/* on line */

		/* only allow these when OL since they're bogus when OB */

		if (pstat[2] == (inverted_bypass_bit ? '0' : '1')) {
			/* boost or trim in effect */
			if (atof(involt) < atof(outvolt))
				status_set("BOOST");

			if (atof(involt) > atof(outvolt))
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
	addvar(VAR_VALUE, "nombattvolt", "Override nominal battery voltage");
	addvar(VAR_VALUE, "battvoltmult", "Battery voltage multiplier");
	addvar(VAR_VALUE, "ID", "Force UPS ID response string");
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
