/*
 * blazer.c: driver core for Megatec/Q1 protocol based UPSes
 *
 * OBSOLETION WARNING: Please to not base new development on this
 * codebase, instead create a new subdriver for nutdrv_qx which
 * generally covers all Megatec/Qx protocol family and aggregates
 * device support from such legacy drivers over time.
 *
 * A document describing the protocol implemented by this driver can be
 * found online at http://www.networkupstools.org/ups-protocols/megatec.html
 *
 * Copyright (C)
 *   2008,2009 - Arjen de Korte <adkorte-guest@alioth.debian.org>
 *   2012 - Arnaud Quette <ArnaudQuette@Eaton.com>
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

#include "main.h"
#include "blazer.h"
#include "nut_float.h"

static int	ondelay = 3;	/* minutes */
static int	offdelay = 30;	/* seconds */

static int	proto;
static int	online = 1;

static struct {
	double	packs;	/* battery voltage multiplier */
	struct {
		double	nom;	/* nominal runtime on battery (full load) */
		double	est;	/* estimated runtime remaining (full load) */
		double	exp;	/* load exponent */
	} runt;
	struct {
		double	act;	/* actual battery voltage */
		double	high;	/* battery float voltage */
		double	nom;	/* nominal battery voltage */
		double	low;	/* battery low voltage */
	} volt;
	struct {
		double	act;	/* actual battery charge */
		long	time;	/* recharge time from empty to full */
	} chrg;
} batt = { 1, { -1, 0, 0 }, { -1, -1, -1, -1 }, { -1, 43200 } };

static struct {
	double	act;	/* actual load (reported by UPS) */
	double	low;	/* idle load */
	double	eff;	/* effective load */
} load = { 0, 0.1, 1 };

static time_t	lastpoll = 0;

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
	{ "zinto", "Q1\r", "F\r", "FW?\r" },
	{ NULL, NULL, NULL, NULL }
};


/*
 * Do whatever we think is needed when we read a battery voltage from the UPS.
 * Basically all it does now, is guestimating the battery charge, but this
 * could be extended.
 */
static double blazer_battery(const char *ptr, char **endptr)
{
	batt.volt.act = batt.packs * strtod(ptr, endptr);

	if ((!getval("runtimecal") || !dstate_getinfo("battery.charge")) &&
			(batt.volt.low > 0) && (batt.volt.high > batt.volt.low)) {
		batt.chrg.act = 100 * (batt.volt.act - batt.volt.low) / (batt.volt.high - batt.volt.low);

		if (batt.chrg.act < 0) {
			batt.chrg.act = 0;
		}

		if (batt.chrg.act > 100) {
			batt.chrg.act = 100;
		}

		dstate_setinfo("battery.charge", "%.0f", batt.chrg.act);
	}

	return batt.volt.act;
}


/*
 * Do whatever we think is needed when we read the load from the UPS.
 */
static double blazer_load(const char *ptr, char **endptr)
{
	load.act = strtod(ptr, endptr);

	load.eff = pow(load.act / 100, batt.runt.exp);

	if (load.eff < load.low) {
		load.eff = load.low;
	}

	return load.act;
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

	const char	*val;
	int		i;

	val = dstate_getinfo("battery.voltage.nominal");

	batt.volt.nom = strtod(val ? val : ptr, endptr);

	for (i = 0; packs[i] > 0; i++) {

		if (packs[i] * batt.volt.act > 1.2 * batt.volt.nom) {
			continue;
		}

		if (packs[i] * batt.volt.act < 0.8 * batt.volt.nom) {
			upslogx(LOG_INFO, "Can't autodetect number of battery packs [%.0f/%.2f]", batt.volt.nom, batt.volt.act);
			break;
		}

		batt.packs = packs[i];
		break;
	}

	return batt.volt.nom;
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
		{ "ups.load", "%.0f", blazer_load },
		{ "input.frequency", "%.1f", strtod },
		{ "battery.voltage", "%.2f", blazer_battery },
		{ "ups.temperature", "%.1f", strtod },
		{ NULL, NULL, NULL }
	};

	char	buf[SMALLBUF], *val, *last = NULL;
	int	i;

	/*
	 * > [Q1\r]
	 * < [(226.0 195.0 226.0 014 49.0 27.5 30.0 00001000\r]
	 *    01234567890123456789012345678901234567890123456
	 *    0         1         2         3         4
	 */
	if (blazer_command(cmd, buf, sizeof(buf)) < 46) {
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

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
		dstate_setinfo(status[i].var, status[i].fmt, status[i].conv(val, NULL));
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	}

	if (!val) {
		upsdebugx(2, "%s: parsing failed", __func__);
		return -1;
	}

	if (strspn(val, "01") != 8) {
		upsdebugx(2, "Invalid status [%s]", val);
		return -1;
	}

	if (val[7] == '1') {	/* Beeper On */
		dstate_setinfo("ups.beeper.status", "enabled");
	} else {
		dstate_setinfo("ups.beeper.status", "disabled");
	}

	if (val[4] == '1') {	/* UPS Type is Standby (0 is On_line) */
		dstate_setinfo("ups.type", "offline / line interactive");
	} else {
		dstate_setinfo("ups.type", "online");
	}

	status_init();

	if (val[0] == '1') {	/* Utility Fail (Immediate) */
		status_set("OB");
		online = 0;
	} else {
		status_set("OL");
		online = 1;
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
		status_set("FSD");
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
		{ NULL, NULL, NULL }
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

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
		dstate_setinfo(rating[i].var, rating[i].fmt, rating[i].conv(val, NULL));
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	}

	return 0;
}


static int blazer_vendor(const char *cmd)
{
	const struct {
		const char	*var;
		const int	len;
	} information[] = {
		{ "ups.mfr",      15 },
		{ "ups.model",    10 },
		{ "ups.firmware", 10 },
		{ NULL, 0 }
	};

	char	buf[SMALLBUF];
	int	i, index;

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

	for (i = 0, index = 1; information[i].var; index += information[i++].len+1) {
		char	val[SMALLBUF];

		snprintf(val, sizeof(val), "%.*s", information[i].len, &buf[index]);

		dstate_setinfo(information[i].var, "%s", str_rtrim(val, ' '));
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
		{ NULL, NULL }
	};

	char	buf[SMALLBUF] = "";
	int	i;

	upslogx(LOG_INFO, "instcmd(%s, %s)", cmdname, extra ? extra : "[NULL]");

	for (i = 0; instcmd[i].cmd; i++) {

		if (strcasecmp(cmdname, instcmd[i].cmd)) {
			continue;
		}

		snprintf(buf, sizeof(buf), "%s", instcmd[i].ups);

		/*
		 * If a command is invalid, it will be echoed back
		 * As an exception, Best UPS units will report "ACK" in case of success!
		 * Other UPSes will reply "(ACK" in case of success.
		 */
		if (blazer_command(buf, buf, sizeof(buf)) > 0) {
			if (strncmp(buf, "ACK", 3) && strncmp(buf, "(ACK", 4)) {
				upslogx(LOG_ERR, "instcmd: command [%s] failed", cmdname);
				return STAT_INSTCMD_FAILED;
			}
		}

		upslogx(LOG_INFO, "instcmd: command [%s] handled", cmdname);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.return")) {

		/*
		 * Sn: Shutdown after n minutes and then turn on when mains is back
		 * SnRm: Shutdown after n minutes and then turn on after m minutes
		 * Accepted values for n: .2 -> .9 , 01 -> 10
		 * Accepted values for m: 0001 -> 9999
		 * Note: "S01R0001" and "S01R0002" may not work on early (GE)
		 * firmware versions.  The failure mode is that the UPS turns
		 * off and never returns.  The fix is to push the return value
		 * up by 2, i.e. S01R0003, and it will return online properly.
		 * (thus the default of ondelay=3 mins)
		 */

		if (ondelay == 0) {

			if (offdelay < 60) {
				snprintf(buf, sizeof(buf), "S.%d\r", offdelay / 6);
			} else {
				snprintf(buf, sizeof(buf), "S%02d\r", offdelay / 60);
			}

		} else if (offdelay < 60) {

			snprintf(buf, sizeof(buf), "S.%dR%04d\r", offdelay / 6, ondelay);

		} else {

			snprintf(buf, sizeof(buf), "S%02dR%04d\r", offdelay / 60, ondelay);

		}

	} else if (!strcasecmp(cmdname, "shutdown.stayoff")) {

		/*
		 * SnR0000
		 * Shutdown after n minutes and stay off
		 * Accepted values for n: .2 -> .9 , 01 -> 10
		 */

		if (offdelay < 60) {
			snprintf(buf, sizeof(buf), "S.%dR0000\r", offdelay / 6);
		} else {
			snprintf(buf, sizeof(buf), "S%02dR0000\r", offdelay / 60);
		}

	} else if (!strcasecmp(cmdname, "test.battery.start")) {
		long	delay = extra ? strtol(extra, NULL, 10) : 10;

		if ((delay < 1) || (delay > 99)) {
			upslogx(LOG_ERR,
				"instcmd: command [%s] failed, delay [%s] out of range",
				cmdname, extra);
			return STAT_INSTCMD_FAILED;
		}

		snprintf(buf, sizeof(buf), "T%02ld\r", delay);
	} else {
		upslogx(LOG_ERR, "instcmd: command [%s] not found", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}

	/*
	 * If a command is invalid, it will be echoed back.
	 * As an exception, Best UPS units will report "ACK" in case of success!
	 * Other UPSes will reply "(ACK" in case of success.
	 */
	if (blazer_command(buf, buf, sizeof(buf)) > 0) {
		if (strncmp(buf, "ACK", 3) && strncmp(buf, "(ACK", 4)) {
			upslogx(LOG_ERR, "instcmd: command [%s] failed", cmdname);
			return STAT_INSTCMD_FAILED;
		}
	}

	upslogx(LOG_INFO, "instcmd: command [%s] handled", cmdname);
	return STAT_INSTCMD_HANDLED;
}


void blazer_makevartable(void)
{
	addvar(VAR_VALUE, "ondelay", "Delay before UPS startup (minutes)");
	addvar(VAR_VALUE, "offdelay", "Delay before UPS shutdown (seconds)");

	addvar(VAR_VALUE, "runtimecal", "Parameters used for runtime calculation");
	addvar(VAR_VALUE, "chargetime", "Nominal charge time for UPS battery");
	addvar(VAR_VALUE, "idleload", "Minimum load to be used for runtime calculation");

	addvar(VAR_FLAG, "norating", "Skip reading rating information from UPS");
	addvar(VAR_FLAG, "novendor", "Skip reading vendor information from UPS");

	addvar(VAR_FLAG, "protocol", "Preselect communication protocol (skip autodetection)");
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

	if ((offdelay < 12) || (offdelay > 600)) {
		fatalx(EXIT_FAILURE, "Shutdown delay '%d' out of range [12..600]", offdelay);
	}

	/* Truncate to nearest setable value */
	if (offdelay < 60) {
		offdelay -= (offdelay % 6);
	} else {
		offdelay -= (offdelay % 60);
	}

	val = dstate_getinfo("battery.voltage.high");
	if (val) {
		batt.volt.high = strtod(val, NULL);
	}

	val = dstate_getinfo("battery.voltage.low");
	if (val) {
		batt.volt.low = strtod(val, NULL);
	}
}


static void blazer_initbattery(void)
{
	const char	*val;

	/* If no values were provided by the user in ups.conf, try to guesstimate
	 * battery.charge, but announce it! */
	if ( (!d_equal(batt.volt.nom, 1)) && ((d_equal(batt.volt.high, -1)) || (d_equal(batt.volt.low, -1)))) {
		upslogx(LOG_INFO, "No values provided for battery high/low voltages in ups.conf\n");

		/* Basic formula, which should cover most cases */
		batt.volt.low = 104 * batt.volt.nom / 120;
		batt.volt.high = 130 * batt.volt.nom / 120;

		/* Publish these data too */
		dstate_setinfo("battery.voltage.low", "%.2f", batt.volt.low);
		dstate_setinfo("battery.voltage.high", "%.2f", batt.volt.high);

		upslogx(LOG_INFO, "Using 'guestimation' (low: %f, high: %f)!", batt.volt.low, batt.volt.high);
	}

	val = getval("runtimecal");
	if (val) {
		double	rh, lh, rl, ll;

		time(&lastpoll);

		if (sscanf(val, "%lf,%lf,%lf,%lf", &rh, &lh, &rl, &ll) < 4) {
			fatalx(EXIT_FAILURE, "Insufficient parameters for runtimecal");
		}

		if ((rl < rh) || (rh <= 0)) {
			fatalx(EXIT_FAILURE, "Parameter out of range (runtime)");
		}

		if ((lh > 100) || (ll > lh) || (ll <= 0)) {
			fatalx(EXIT_FAILURE, "Parameter out of range (load)");
		}

		batt.runt.exp = log(rl / rh) / log(lh / ll);
		upsdebugx(2, "battery runtime exponent : %.3f", batt.runt.exp);

		batt.runt.nom = rh * pow(lh / 100, batt.runt.exp);
		upsdebugx(2, "battery runtime nominal  : %.1f", batt.runt.nom);

	} else {
		upslogx(LOG_INFO, "Battery runtime will not be calculated (runtimecal not set)");
		return;
	}

	if (batt.chrg.act < 0) {
		batt.volt.low = batt.volt.nom;
		batt.volt.high = 1.15 * batt.volt.nom;

		blazer_battery(dstate_getinfo("battery.voltage"), NULL);
	}

	val = dstate_getinfo("battery.charge");
	if (val) {
		batt.runt.est = batt.runt.nom * strtod(val, NULL) / 100;
		upsdebugx(2, "battery runtime estimate : %.1f", batt.runt.est);
	} else {
		fatalx(EXIT_FAILURE, "Initial battery charge undetermined");
	}

	val = getval("chargetime");
	if (val) {
		batt.chrg.time = strtol(val, NULL, 10);

		if (batt.chrg.time <= 0) {
			fatalx(EXIT_FAILURE, "Charge time out of range [1..s]");
		}

		upsdebugx(2, "battery charge time      : %ld", batt.chrg.time);
	} else {
		upslogx(LOG_INFO, "No charge time specified, using built in default [%ld seconds]", batt.chrg.time);
	}

	val = getval("idleload");
	if (val) {
		load.low = strtod(val, NULL) / 100;

		if ((load.low <= 0) || (load.low > 1)) {
			fatalx(EXIT_FAILURE, "Idle load out of range [0..100]");
		}

		upsdebugx(2, "minimum load used (idle) : %.3f", load.low);
	} else {
		upslogx(LOG_INFO, "No idle load specified, using built in default [%.1f %%]", 100 * load.low);
	}
}


void blazer_initinfo(void)
{
	const char	*protocol = getval("protocol");
	int	retry;

	for (proto = 0; command[proto].status; proto++) {

		int	ret = -1;

		if (protocol && strcasecmp(protocol, command[proto].name)) {
			upsdebugx(2, "Skipping %s protocol...", command[proto].name);
			continue;
		}

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
		int	ret = -1;

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
		int	ret = -1;

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

	blazer_initbattery();

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
			upsdebugx(1, "Communications with UPS lost: status read failed!");
			retry++;
		} else if (retry == MAXTRIES) {
			upslogx(LOG_WARNING, "Communications with UPS lost: status read failed!");
			retry++;
		} else {
			dstate_datastale();
		}

		return;
	}

	if (getval("runtimecal")) {
		time_t	now;

		time(&now);

		if (online) {	/* OL */
			batt.runt.est += batt.runt.nom * difftime(now, lastpoll) / batt.chrg.time;
			if (batt.runt.est > batt.runt.nom) {
				batt.runt.est = batt.runt.nom;
			}
		} else {	/* OB */
			batt.runt.est -= load.eff * difftime(now, lastpoll);
			if (batt.runt.est < 0) {
				batt.runt.est = 0;
			}
		}

		dstate_setinfo("battery.charge", "%.0f", 100 * batt.runt.est / batt.runt.nom);
		dstate_setinfo("battery.runtime", "%.0f", batt.runt.est / load.eff);

		lastpoll = now;
	}

	if (retry > MAXTRIES) {
		upslogx(LOG_NOTICE, "Communications with UPS re-established");
	}

	retry = 0;

	dstate_dataok();
}

void upsdrv_shutdown(void)
	__attribute__((noreturn));

void upsdrv_shutdown(void)
{
	int	retry;

	/* Stop pending shutdowns */
	for (retry = 1; retry <= MAXTRIES; retry++) {

		if (blazer_instcmd("shutdown.stop", NULL) != STAT_INSTCMD_HANDLED) {
			continue;
		}

		break;

	}

	if (retry > MAXTRIES) {
		upslogx(LOG_NOTICE, "No shutdown pending");
	}

	/* Shutdown */
	for (retry = 1; retry <= MAXTRIES; retry++) {

		if (blazer_instcmd("shutdown.return", NULL) != STAT_INSTCMD_HANDLED) {
			continue;
		}

		fatalx(EXIT_SUCCESS, "Shutting down in %d seconds", offdelay);

	}

	fatalx(EXIT_FAILURE, "Shutdown failed!");
}
