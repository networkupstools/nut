/*
 * blazer.c: driver core for Megatec/Q1 protocol based UPSes
 *
 * A document describing the protocol implemented by this driver can be
 * found online at "http://www.networkupstools.org/protocols/megatec.html".
 *
 * Copyright (C) 2008 - Arjen de Korte <adkorte-guest@alioth.debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <math.h>

#include "main.h"
#include "blazer.h"


static int	ondelay = 3;	/* minutes */
static int	offdelay = 30;	/* seconds */

static int	proto;

/*
 * This little structure defines the various flavors of the Megatec protocol.
 * Only the .name and .status are mandatory, .rating and .vendor elements are
 * optional. If only some models support the last two, fill them in anyway
 * and tell people to use the 'norating' and 'novendor' options to bypass
 * getting them.
 */
static const struct {
	const char	*name;
	const char	*status;
	const char	*rating;
	const char	*vendor;
} command[] = {
	{ "megatec", "Q1\r", "F\r", "I\r" },
	{ "mustek", "QS\r", "F\r", "I\r" },
	{ "megatec/old", "D\r", "F\r", "I\r" },
	{ NULL }
};


/*
 * Do whatever we think is needed when we read a battery voltage from the UPS.
 * Basically all it does now, is guestimating the battery charge, but this
 * could be extended.
 */
static double blazer_battery(const char *ptr, char **endptr)
{
	double		bv, bl = -1, bh = -1;
	const char	*val;

	bv = strtod(ptr, endptr);

	val = dstate_getinfo("battery.packs");
	if (val) {
		bv *= strtod(val, NULL);
	}

	val = dstate_getinfo("battery.voltage.high");
	if (val) {
		bh = strtod(val, NULL);
	}

	val = dstate_getinfo("battery.voltage.low");
	if (val) {
		bl = strtod(val, NULL);
	}

	/*
	 * If "battery.voltage.(low|high)" are both set, guesstimate "battery.charge".
	 */
	if ((bl > 0) && (bh > bl)) {
		dstate_setinfo("battery.charge", "%.0f", 100 * fmin(fmax((bv-bl)/(bh-bl), 0), 1));
	}

	return bv;
}


/*
 * The battery voltage will quickly return to at least the nominal value after
 * discharging them. For overlapping battery.voltage.low/high ranges therefor
 * choose the one with the highest multiplier.
 */
static double blazer_packs(const char *ptr, char **endptr)
{
	const double packs[] = {
		120, 100, 80, 60, 48, 36, 30, 24, 18, 12, 8, 6, 4, 3, 2, 1, 0.5, -1
	};

	double		bn, bv = -1;
	const char	*val;
	int		i;

	val = dstate_getinfo("battery.voltage.nominal");

	bn = strtod(val ? val : ptr, endptr);

	val = dstate_getinfo("battery.voltage");
	if (val) {
		bv = strtod(val, NULL);;
	}

	for (i = 0; packs[i] > 0; i++) {

		if (packs[i] * bv > 1.2 * bn) {
			continue;
		}

		if (packs[i] * bv < 0.8 * bn) {
			upslogx(LOG_INFO, "Can't autodetect number of battery packs [%.0f/%.2f]", bn, bv);
			break;
		}

		dstate_setinfo("battery.packs", "%g", packs[i]);
		break;
	}

#ifdef TESTING
	/*
	 * NOTE: don't do this automatically, leave this up to the user to decide!
	 */
	dstate_setinfo("battery.voltage.high", "%.2f", 1.15 * bn);
	dstate_setinfo("battery.voltage.low",  "%.2f", 0.85 * bn);
#endif

	return bn;
}


static int blazer_status(const char *cmd)
{
	const struct {
		const char	*var;
		const char	*fmt;
		double	(*conv)(const char *, char **);
	} status[] = {
		{ "input.voltage", "%.1f", strtod },
		{ "input.voltage.fault", "%.1f", strtod },
		{ "output.voltage", "%.1f", strtod },
		{ "ups.load", "%.0f", strtod },
		{ "input.frequency", "%.1f", strtod },
		{ "battery.voltage", "%.2f", blazer_battery },
		{ "ups.temperature", "%.1f", strtod },
		{ NULL }
	};

	char	buf[SMALLBUF], *val, *last = NULL;
	int	i;

	/*
	 * > [Q1\r]
	 * < [(226.0 195.0 226.0 014 49.0 27.5 30.0 00001000\r]
	 *    01234567890123456789012345678901234567890123456
	 *    0         1         2         3         4
	 */
	if (blazer_command(cmd, buf, sizeof(buf)) < 47) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	for (i = 0, val = strtok_r(buf+1, " ", &last); status[i].var; i++, val = strtok_r(NULL, " \r\n", &last)) {

		if (!val) {
			upsdebugx(2, "%s: parsing failed", __func__);
			return -1;
		}

		if (strspn(val, "0123456789.") != strlen(val)) {
			upsdebugx(2, "%s: non numerical value [%s]", __func__, val);
			continue;
		}

		dstate_setinfo(status[i].var, status[i].fmt, status[i].conv(val, NULL));
	}

	if (strspn(val, "01") != 8) {
		upsdebugx(2, "Invalid status [%s]", val);
		return -1;
	}

	if (val[7] == '1') {	/* Beeper On */
		dstate_setinfo("beeper.status", "enabled");
	} else {
		dstate_setinfo("beeper.status", "disabled");
	}

	if (val[4] == '1') {	/* UPS Type is Standby (0 is On_line) */
		dstate_setinfo("ups.type", "offline / line interactive");
	} else {
		dstate_setinfo("ups.type", "online");
	}

	status_init();

	if (val[0] == '1') {	/* Utility Fail (Immediate) */
		status_set("OB");
	} else {
		status_set("OL");
	}

	if (val[1] == '1') {	/* Battery Low */
		status_set("LB");
	}

	if (val[2] == '1') {	/* Bypass/Boost or Buck Active */

		double	vi, vo;

		vi = strtod(dstate_getinfo("input.voltage"),  NULL);
		vo = strtod(dstate_getinfo("output.voltage"), NULL);

		if (vo < 0.5 * vi) {
			upsdebugx(2, "%s: output voltage too low", __func__);
		} else if (vo < 0.95 * vi) {
			status_set("TRIM");
		} else if (vo < 1.05 * vi) {
			status_set("BYPASS");
		} else if (vo < 1.5 * vi) {
			status_set("BOOST"); 
		} else {
			upsdebugx(2, "%s: output voltage too high", __func__);
		}
	}

	if (val[5] == '1') {	/* Test in Progress */
		status_set("CAL");
	}

	alarm_init();

	if (val[3] == '1') {	/* UPS Failed */
		alarm_set("UPS selftest failed!");
	}

	if (val[6] == '1') {	/* Shutdown Active */
		alarm_set("Shutdown imminent!");
	}

	alarm_commit();

	status_commit();

	return 0;
}


static int blazer_rating(const char *cmd)
{
	const struct {
		const char	*var;
		const char	*fmt;
		double		(*conv)(const char *, char **);
	} rating[] = {
		{ "input.voltage.nominal", "%.0f", strtod },
		{ "input.current.nominal", "%.1f", strtod },
		{ "battery.voltage.nominal", "%.1f", blazer_packs },
		{ "input.frequency.nominal", "%.0f", strtod },
		{ NULL }
	};

	char	buf[SMALLBUF], *val, *last = NULL;
	int	i;

	/*
	 * > [F\r]
	 * < [#220.0 000 024.0 50.0\r]
	 *    0123456789012345678901
	 *    0         1         2
	 */
	if (blazer_command(cmd, buf, sizeof(buf)) < 22) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (buf[0] != '#') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	for (i = 0, val = strtok_r(buf+1, " ", &last); rating[i].var; i++, val = strtok_r(NULL, " \r\n", &last)) {

		if (!val) {
			upsdebugx(2, "%s: parsing failed", __func__);
			return -1;
		}

		if (strspn(val, "0123456789.") != strlen(val)) {
			upsdebugx(2, "%s: non numerical value [%s]", __func__, val);
			continue;
		}

		dstate_setinfo(rating[i].var, rating[i].fmt, rating[i].conv(val, NULL));
	}

	return 0;
}


static int blazer_vendor(const char *cmd)
{
	const struct {
		const char	*var;
		const char	*def;
	} information[] = {
		{ "ups.mfg",      "[generic]" },
		{ "ups.model",    "[megatec]" },
		{ "ups.firmware", "[unknown]" },
		{ NULL }
	};

	char	buf[SMALLBUF], *val, *last = NULL;
	int	i;

	/*
	 * > [I\r]
	 * < [#-------------   ------     VT12046Q  \r]
	 *    012345678901234567890123456789012345678
	 *    0         1         2         3
	 */
	if (blazer_command(cmd, buf, sizeof(buf)) < 39) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (buf[0] != '#') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	for (i = 0, val = strtok_r(buf+1, " ", &last); information[i].var; i++, val = strtok_r(NULL, " \r\n", &last)) {

		dstate_setinfo(information[i].var, "%s", val ? val : information[i].def);
	}

	return 0;
}


static int blazer_instcmd(const char *cmdname, const char *extra)
{
	const struct {
		const char *cmd;
		const char *ups;
	} instcmd[] = {
		{ "beeper.toggle", "Q\r" },
		{ "load.off", "S00R0000\r" },
		{ "load.on", "C\r" },
		{ "shutdown.stop", "C\r" },
		{ "test.battery.start.deep", "TL\r" },
		{ "test.battery.start.quick", "T\r" },
		{ "test.battery.stop", "CT\r" },
		{ NULL }
	};

	char	buf[SMALLBUF] = "";
	int	i;

	for (i = 0; instcmd[i].cmd; i++) {

		if (strcasecmp(cmdname, instcmd[i].cmd)) {
			continue;
		}

		snprintf(buf, sizeof(buf), "%s", instcmd[i].ups);

		if (blazer_command(buf, buf, sizeof(buf)) == 0) {
			upslogx(LOG_INFO, "instcmd: command [%s] handled", cmdname);
			return STAT_INSTCMD_HANDLED;
		}

		upslogx(LOG_ERR, "instcmd: command [%s] failed", cmdname);
		return STAT_INSTCMD_FAILED;
	}

	if (!strcasecmp(cmdname, "shutdown.return")) {
		if (offdelay < 60) {
			snprintf(buf, sizeof(buf), "S.%dR%04d\r", offdelay / 6, ondelay);
		} else {
			snprintf(buf, sizeof(buf), "S%02dR%04d\r", offdelay / 60, ondelay);
		}
	} else if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		if (offdelay < 60) {
			snprintf(buf, sizeof(buf), "S.%dR0000\r", offdelay / 6);
		} else {
			snprintf(buf, sizeof(buf), "S%02dR0000\r", offdelay / 60);
		}
	} else if (!strcasecmp(cmdname, "test.battery.start")) {
		int	delay = extra ? strtol(extra, NULL, 10) : 10;

		if ((delay < 0) || (delay > 99)) {
			return STAT_INSTCMD_FAILED;
		}

		snprintf(buf, sizeof(buf), "T%02d\r", delay);
	} else {
		upslogx(LOG_ERR, "instcmd: command [%s] not found", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}

	if (blazer_command(buf, buf, sizeof(buf)) == 0) {
		upslogx(LOG_INFO, "instcmd: command [%s] handled", cmdname);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_ERR, "instcmd: command [%s] failed", cmdname);
	return STAT_INSTCMD_FAILED;

}


void blazer_makevartable(void)
{
	addvar(VAR_VALUE, "ondelay", "Delay before UPS startup (minutes)");
	addvar(VAR_VALUE, "offdelay", "Delay before UPS shutdown (seconds)");

	addvar(VAR_FLAG, "norating", "Skip reading rating information from UPS");
	addvar(VAR_FLAG, "novendor", "Skip reading vendor information from UPS");
}


void blazer_initups(void)
{
	const char	*val;

	val = getval("ondelay");
	if (val) {
		ondelay = strtol(val, NULL, 10);
	}

	if ((ondelay < 0) || (ondelay > 9999)) {
		fatalx(EXIT_FAILURE, "Start delay '%d' out of range [0..9999]", ondelay);
	}

	val = getval("offdelay");
	if (val) {
		offdelay = strtol(val, NULL, 10);
	}

	if ((offdelay < 6) || (offdelay > 600)) {
		fatalx(EXIT_FAILURE, "Shutdown delay '%d' out of range [6..600]", offdelay);
	}

	/* Truncate to nearest setable value */
	if (offdelay < 60) {
		offdelay -= (offdelay % 6);
	} else {
		offdelay -= (offdelay % 60);
	}
}


void blazer_initinfo(void)
{
	int	retry;

	for (proto = 0; command[proto].status; proto++) {

		int	ret;

		upsdebugx(2, "Trying %s protocol...", command[proto].name);

		for (retry = 1; retry <= MAXTRIES; retry++) {

			ret = blazer_status(command[proto].status);
			if (ret < 0) {
				upsdebugx(2, "Status read %d failed", retry);
				continue;
			}

			upsdebugx(2, "Status read in %d tries", retry);
			break;
		}

		if (!ret) {
			upslogx(LOG_INFO, "Supported UPS detected with %s protocol", command[proto].name);
			break;
		}
	}

	if (!command[proto].status) {
		fatalx(EXIT_FAILURE, "No supported UPS detected");
	}

	if (command[proto].rating && !testvar("norating")) {
		int	ret;

		for (retry = 1; retry <= MAXTRIES; retry++) {

			ret = blazer_rating(command[proto].rating);
			if (ret < 0) {
				upsdebugx(1, "Rating read %d failed", retry);
				continue;
			}

			upsdebugx(2, "Ratings read in %d tries", retry);
			break;
		}

		if (ret) {
			upslogx(LOG_DEBUG, "Rating information unavailable");
		}
	}

	if (command[proto].vendor && !testvar("novendor")) {
		int	ret;

		for (retry = 1; retry <= MAXTRIES; retry++) {

			ret = blazer_vendor(command[proto].vendor);
			if (ret < 0) {
				upsdebugx(1, "Vendor information read %d failed", retry);
				continue;
			}

			upslogx(LOG_INFO, "Vendor information read in %d tries", retry);
			break;
		}

		if (ret) {
			upslogx(LOG_DEBUG, "Vendor information unavailable");
		}
	}

	dstate_setinfo("ups.delay.start", "%d", 60 * ondelay);
	dstate_setinfo("ups.delay.shutdown", "%d", offdelay);

	dstate_addcmd("beeper.toggle");
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.start.deep");
	dstate_addcmd("test.battery.start.quick");
	dstate_addcmd("test.battery.stop");

	upsh.instcmd = blazer_instcmd;
}


void upsdrv_updateinfo(void)
{
	static int	retry = 0;

	if (blazer_status(command[proto].status)) {

		if (retry < MAXTRIES) {
			upslogx(LOG_WARNING, "Communications with UPS lost: status read failed!");
			retry++;
		} else {
			dstate_datastale();
		}

		return;
	}

	if (retry) {
		upslogx(LOG_NOTICE, "Communications with UPS re-established");
	}

	retry = 0;

	dstate_dataok();
}


void upsdrv_shutdown(void)
{
	int	retry;

	for (retry = 0; retry < MAXTRIES; retry++) {

		if (blazer_instcmd("shutdown.return", NULL) == STAT_INSTCMD_HANDLED) {
			return;
		}
	}

	upsdebugx(2, "Shutdown failed after %d retries!", retry);
}
