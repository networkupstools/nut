/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: t; -*-
 *
 * megatec.c: support for Megatec protocol based UPSes
 *
 * Copyright (C) Carlos Rodrigues <carlos.efr at mail.telepac.pt>
 *
 * megatec.c created on 4/10/2003
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


/*
 * A document describing the protocol implemented by this driver can be
 * found online at "http://www.networkupstools.org/protocols/megatec.html".
 */


#include "main.h"
#ifndef WIN32 /* FIXME removing serial reference just during the USB compilation phase, need to be reactivated when serial will be ported to WIN32. there is several occurence of this FIXME in this file*/
#include "serial.h"
#endif
#include "megatec.h"

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#define DRIVER_NAME	"Megatec protocol driver"
#define DRIVER_VERSION	"1.6" /* FIXME: doesn't conform to "X.YZ" */

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Carlos Rodrigues <carlos.efr@mail.telepac.pt>",
	DRV_STABLE,
#ifdef MEGATEC_SUBDRV
	{ &megatec_subdrv_info, NULL }
#else
	{ NULL }
#endif
};

#define ENDCHAR  '\r'
#define IGNCHARS ""

#define RECV_BUFFER_LEN 128
#define FIELD_BUFFER_LEN 16

/* The expected reply lengths */
#define F_CMD_REPLY_LEN  21
#define Q1_CMD_REPLY_LEN 46
#define I_CMD_REPLY_LEN  38

#define IDENT_MAXTRIES   3

#define READ_TIMEOUT 2       /* timeout on read (seconds) */
#define READ_PACE    300000  /* interval to wait between sending a command and reading the response (usec) */

#define MAX_START_DELAY    9999
#define MAX_SHUTDOWN_DELAY 99

/* Maximum length of a string representing these values */
#define MAX_START_DELAY_LEN    4
#define MAX_SHUTDOWN_DELAY_LEN 2

#define MAX_POLL_FAILURES 3

#define N_FLAGS 8

/* The UPS status flags */
#define FL_ON_BATT    0
#define FL_LOW_BATT   1
#define FL_BOOST_TRIM 2
#define FL_FAILED     3
#define FL_UPS_TYPE   4
#define FL_BATT_TEST  5
#define FL_LOAD_OFF   6
#define FL_BEEPER_ON  7  /* bogus on some models */

/* Maximum lengths for the "I" command reply fields */
#define UPS_MFR_CHARS     15
#define UPS_MODEL_CHARS   10
#define UPS_VERSION_CHARS 10

/* Below this value we can safely consider a voltage to be zero */
#define RESIDUAL_VOLTAGE 10.0


/* The values returned by the UPS for an "I" query */
typedef struct {
	char mfr[UPS_MFR_CHARS + 1];
	char model[UPS_MODEL_CHARS + 1];
	char version[UPS_VERSION_CHARS + 1];
} UPSInfo_t;


/* The values returned by the UPS for an "F" query */
typedef struct {
	float volt;
	float current;
	float battvolt;
	float freq;
} FirmwareValues_t;


/* The values returned by the UPS for an "Q1" query */
typedef struct {
	float ivolt;
	float fvolt;
	float ovolt;
	float load;
	float freq;
	float battvolt;
	float temp;
	char flags[N_FLAGS + 1];
} QueryValues_t;


/* Parameters for known battery types */
typedef struct {
	float nominal;  /* battery voltage (nominal) */
	float min;      /* lower bound for a single battery of "nominal" voltage (see "set_battery_params" below) */
	float max;      /* upper bound for a single battery of "nominal" voltage (see "set_battery_params" below) */
	float empty;    /* fully discharged battery */
	float full;     /* fully charged battery */
	float low;      /* low battery (unused) */
} BatteryVolts_t;


/* Known battery types must be in ascending order by "nominal" first, and then by "max". */
static BatteryVolts_t batteries[] = {{ 12.0,  9.0, 16.0,  9.7, 13.7,  0.0 },   /* Mustek PowerMust 600VA Plus (LB unknown) */
                                     { 12.0, 18.0, 30.0, 18.8, 26.8, 22.3 },   /* PowerWalker Line-Interactive VI 1000 */
                                     { 23.5, 18.0, 30.0, 21.3, 27.1, 22.2 },   /* UNITEK ALPHA2600 */
                                     { 24.0, 18.0, 30.0, 19.4, 27.4, 22.2 },   /* Mustek PowerMust 1000VA Plus */
                                     { 36.0,  1.5,  3.0, 1.64, 2.31, 1.88 },   /* Mustek PowerMust 1000VA On-Line */
                                     { 36.0, 30.0, 42.0, 32.5, 41.0,  0.0 },   /* Mecer ME-2000 (LB unknown) */
                                     { 48.0, 38.0, 58.0, 40.0, 54.6, 44.0 },   /* Sven Smart RM2000 */
                                     { 72.0,  1.5,  3.0, 1.74, 2.37, 1.82 },   /* Effekta RM2000MH */
                                     { 96.0,  1.5,  3.0, 1.63, 2.29,  1.8 },   /* Ablerex MS3000RT (LB at 25% charge) */
                                     {  0.0,  0.0,  0.0,  0.0,  0.0,  0.0 }};  /* END OF DATA */

/* Interval between chars on send (usec) */
static int send_pace = 100000;

/* Some models need this */
static char state_dtr = 1;
static char state_rts = 0;

/* Workaround for buggy models */
static char ignore_off = 0;  /* ignore FL_LOAD_OFF if it behaves strangely */

/* Defined in upsdrv_initinfo */
static float battvolt_empty = -1;  /* unknown */
static float battvolt_full = -1;   /* unknown */

/* Battery voltage multiplier (to match the nominal voltage on some models) */
static float battvolt_mult = 1;

/* Minimum and maximum voltage seen on input */
static float ivolt_min = INT_MAX;  /* unknown */
static float ivolt_max = -1;       /* unknown */

/* In minutes: */
static short start_delay = 2;     /* wait at least this amount of time before coming back online */
static short shutdown_delay = 0;  /* wait until going offline */

/* In percentage: */
static float lowbatt = -1;  /* disabled */

static char watchdog_enabled = 0;  /* disabled by default, of course */
static char watchdog_timeout = 1;  /* in minutes */


static char *copy_field(char* dest, char *src, int field_len);
static float get_battery_charge(float battvolt);
static int set_battery_params(float volt_nominal, float volt_now);
static int check_ups(QueryValues_t *status);
static int get_ups_info(UPSInfo_t *info);
static int get_firmware_values(FirmwareValues_t *values);
static int run_query(QueryValues_t *values);

int instcmd(const char *cmdname, const char *extra);
int setvar(const char *varname, const char *val);


/* I know, macros should evaluate their arguments only once */
#define CLAMP(x, min, max) (((x) < (min)) ? (min) : (((x) > (max)) ? (max) : (x)))


static char *copy_field(char* dest, char *src, int field_len)
{
	int i, j;

	/* First we skip the leading spaces... */
	for (i = 0; i < field_len; i++) {
		if (src[i] != ' ') {
			break;
		}
	}

	/* ... then we copy the rest of the field... */
	j = 0;
	while (i < field_len) {
		dest[j] = src[i];

		i++; j++;
	}

	dest[j] = '\0';

	/* ...and finally, remove the trailing spaces. */
	rtrim(dest, ' ');

	return &src[field_len];  /* return the rest of the source buffer */
}


static float get_battery_charge(float battvolt)
{
	float value;

	if (battvolt_empty < 0 || battvolt_full < 0) {
		return -1;
	}

	battvolt = CLAMP(battvolt, battvolt_empty, battvolt_full);
	value = (battvolt - battvolt_empty) / (battvolt_full - battvolt_empty);

	return value * 100;  /* percentage */
}


/*
 * Set the proper limits, depending on the battery voltage,
 * so that the "charge" calculations return meaningful values.
 *
 * This has to be done by looking at the present battery voltage and
 * the nominal voltage because, for example, some 24V models will
 * show a nominal voltage of 24, while others will show a nominal
 * voltage of 12. The present voltage helps telling them apart.
 */
static int set_battery_params(float volt_nominal, float volt_now)
{
	int i = 0;

	while (batteries[i].nominal > 0) {
		if (volt_nominal == batteries[i].nominal) {         /* battery voltage matches... */
			while (volt_nominal == batteries[i].nominal) {  /* ...find the most adequate parameters */
				if (volt_now > batteries[i].min && volt_now < batteries[i].max) {
					battvolt_empty = batteries[i].empty;
					battvolt_full = batteries[i].full;

					upsdebugx(1, "%.1fV battery, interval [%.1fV, %.1fV].", volt_nominal, battvolt_empty, battvolt_full);

					return i;
				}

				i++;

			}

			upsdebugx(1, "%.1fV battery, present voltage (%.1fV) outside of supported intervals.", volt_nominal, volt_now);

			return -1;
		}

		i++;
	}

	upsdebugx(1, "Unsupported battery voltage (%.1fV).", volt_nominal);

	return -1;
}


/*
 * The "status" parameter is left unchanged on failure.
 */
static int check_ups(QueryValues_t *status)
{
	QueryValues_t values;

	if (run_query(&values) < 0) {
		return -1;
	}

	memcpy(status, &values, sizeof(values));

	return 0;
}


static int get_ups_info(UPSInfo_t *info)
{
	char buffer[RECV_BUFFER_LEN];
	char *anchor;
	int ret;

	upsdebugx(2, "Asking for UPS information [I]...");
#ifndef WIN32 /*FIXME*/
	ser_flush_io(upsfd);
	ser_send_pace(upsfd, send_pace, "I%c", ENDCHAR);
	usleep(READ_PACE);

	/*
	 * Expected reply: "#UPS_MFR........ UPS_MODEL. UPS_VER...<cr>"
	 */
	ret = ser_get_line(upsfd, buffer, RECV_BUFFER_LEN, ENDCHAR, IGNCHARS, READ_TIMEOUT, 0);

#endif
	if (ret < 0) {
		upsdebug_with_errno(2, "I => FAILED");

		return -1;
	}

	if (ret == 0) {
		upsdebugx(2, "I => FAILED [timeout]");

		return -1;
	}

	if (ret < I_CMD_REPLY_LEN) {
		upsdebugx(2, "I => FAILED [short read]");
		upsdebug_hex(3, "I detail", (unsigned char *)buffer, ret);

		return -1;
	}

	if (buffer[0] != '#') {
		upsdebugx(2, "I => FAILED [invalid start character]");
		upsdebug_hex(3, "I detail", (unsigned char *)buffer, ret);

		return -1;
	}

	upsdebugx(2, "I => OK [%s]", buffer);

	memset(info, 0, sizeof(UPSInfo_t));

	/*
	 * Get the manufacturer, model and version fields, skipping
	 * the separator character that sits between them, as well as
	 * the first character (the control character, always a '#').
	 */
	anchor = copy_field(info->mfr, &buffer[1], UPS_MFR_CHARS);
	anchor = copy_field(info->model, anchor + 1, UPS_MODEL_CHARS);
	copy_field(info->version, anchor + 1, UPS_VERSION_CHARS);

	upsdebugx(2, "I VALUES => [%s %s %s]", info->mfr, info->model, info->version);

	return 0;
}


static int get_firmware_values(FirmwareValues_t *values)
{
	char buffer[RECV_BUFFER_LEN];
	char field[FIELD_BUFFER_LEN];
	char *anchor;
	int ret;

	upsdebugx(2, "Asking for UPS power ratings [F]...");
#ifndef WIN32 /*FIXME*/
	ser_flush_io(upsfd);
	ser_send_pace(upsfd, send_pace, "F%c", ENDCHAR);
	usleep(READ_PACE);

	/*
	 * Expected reply: "#MMM.M QQQ SS.SS RR.R<cr>"
	 */
	ret = ser_get_line(upsfd, buffer, RECV_BUFFER_LEN, ENDCHAR, IGNCHARS, READ_TIMEOUT, 0);
#endif

	if (ret < 0) {
		upsdebug_with_errno(2, "F => FAILED");

		return -1;
	}

	if (ret == 0) {
		upsdebugx(2, "F => FAILED [timeout]");

		return -1;
	}

	if (ret < F_CMD_REPLY_LEN) {
		upsdebugx(2, "F => FAILED [short read]");
		upsdebug_hex(3, "F detail", (unsigned char *)buffer, ret);

		return -1;
	}


	if (buffer[0] != '#') {
		upsdebugx(2, "F => FAILED [invalid start character]");
		upsdebug_hex(3, "F detail", (unsigned char *)buffer, ret);

		return -1;
	}

	upsdebugx(2, "F => OK [%s]", buffer);

	anchor = copy_field(field, &buffer[1], 5);
	values->volt = atof(field);

	anchor = copy_field(field, anchor + 1, 3);
	values->current = atof(field);

	anchor = copy_field(field, anchor + 1, 5);
	values->battvolt = atof(field);

	anchor = copy_field(field, anchor + 1, 4);
	values->freq = atof(field);

	upsdebugx(2, "F VALUES => [%.1f %.1f %.1f %.1f]", values->volt,
	          values->current, values->battvolt, values->freq);

	return 0;
}


static int run_query(QueryValues_t *values)
{
	char buffer[RECV_BUFFER_LEN];
	char field[FIELD_BUFFER_LEN];
	char *anchor;
	int ret;

	upsdebugx(2, "Asking for UPS status [Q1]...");
#ifndef WIN32 /*FIXME*/
	ser_flush_io(upsfd);
	ser_send_pace(upsfd, send_pace, "Q1%c", ENDCHAR);
	usleep(READ_PACE);

	/*
	 * Expected reply: "(MMM.M NNN.N PPP.P QQQ RR.R S.SS TT.T b7b6b5b4b3b2b1b0<cr>"
	 */
	ret = ser_get_line(upsfd, buffer, RECV_BUFFER_LEN, ENDCHAR, IGNCHARS, READ_TIMEOUT, 0);
#endif

	if (ret < 0) {
		upsdebug_with_errno(2, "Q1 => FAILED");

		return -1;
	}

	if (ret == 0) {
		upsdebugx(2, "Q1 => FAILED [timeout]");

		return -1;
	}

	if (ret < Q1_CMD_REPLY_LEN) {
		upsdebugx(2, "Q1 => FAILED [short read]");
		upsdebug_hex(3, "Q1 detail", (unsigned char *)buffer, ret);

		return -1;
	}

	if (buffer[0] != '(') {
		upsdebugx(2, "Q1 => FAILED [invalid start character]");
		upsdebug_hex(3, "Q1 detail", (unsigned char *)buffer, ret);

		return -1;
	}

	upsdebugx(2, "Q1 => OK [%s]", buffer);

	anchor = copy_field(field, &buffer[1], 5);
	values->ivolt = atof(field);

	anchor = copy_field(field, anchor + 1, 5);
	values->fvolt = atof(field);

	anchor = copy_field(field, anchor + 1, 5);
	values->ovolt = atof(field);

	anchor = copy_field(field, anchor + 1, 3);
	values->load = atof(field);

	anchor = copy_field(field, anchor + 1, 4);
	values->freq = atof(field);

	anchor = copy_field(field, anchor + 1, 4);
	values->battvolt = atof(field);

	anchor = copy_field(field, anchor + 1, 4);
	values->temp = atof(field);

	anchor = copy_field(values->flags, anchor + 1, N_FLAGS);

	if (strlen(values->flags) < N_FLAGS) {
		upsdebugx(2, "Q1 => FAILED [flags error]");
		return -1;
	}

	upsdebugx(2, "Q1 VALUES => [%.1f %.1f %.1f %.1f %.1f %.1f %.1f %s]",
	          values->ivolt, values->fvolt, values->ovolt, values->load,
	          values->freq, values->battvolt, values->temp, values->flags);

	return 0;
}


void upsdrv_initinfo(void)
{
	int i;
	FirmwareValues_t values;
	QueryValues_t status;
	UPSInfo_t info;

	/*
	 * Some models apparently time-out with the default send pace, so we must
	 * allow the user to override it if needed be. The configuration parameter
	 * is specified in milliseconds for the user's benefit.
	 */
	if (getval("sendpace")) {
		upsdebugx(2, "Default command send pace is %d usec.", send_pace);
		upsdebugx(2, "Parameter [sendpace]: [%s]", getval("sendpace"));

		/* Having 1 second as the upper-bound is an arbitrary choice... */
		send_pace = CLAMP(atoi(getval("sendpace")), 1, 1000) * 1000;

		upslogx(LOG_NOTICE, "Command send pace changed to %d usec.", send_pace);
	}

	/*
	 * UPS detection sequence.
	 */
	upsdebugx(1, "Starting UPS detection process...");

	/* Some models seem to need this. We'll just discard the ouput for now... */
	get_ups_info(&info);

	/* Check for a compatible UPS... */
	for (i = 0; i < IDENT_MAXTRIES; i++) {
		if (check_ups(&status) == 0) {
			break;
		}
	}

	if (i == IDENT_MAXTRIES) {
		fatalx(EXIT_FAILURE, "Megatec protocol UPS not detected.");
	}

	dstate_setinfo("ups.type", status.flags[FL_UPS_TYPE] == '1' ? "standby" : "online");

	upsdebugx(1, "Cancelling any pending shutdown or battery test.");
#ifndef WIN32 /*FIXME*/
	ser_send_pace(upsfd, send_pace, "C%c", ENDCHAR);
#endif

	/*
	 * Try to identify the UPS.
	 */
	if (get_ups_info(&info) >= 0) {
		char model[UPS_MODEL_CHARS + UPS_VERSION_CHARS + 2];
		snprintf(model, sizeof(model), "%s %s", info.model, info.version);

		dstate_setinfo("ups.mfr", "%s", getval("mfr") ? getval("mfr") : info.mfr);
		dstate_setinfo("ups.model", "%s", getval("model") ? getval("model") : model);

		upslogx(LOG_INFO, "Megatec protocol UPS detected [%s %s %s].", info.mfr, info.model, info.version);
	} else {
		dstate_setinfo("ups.mfr", "%s", getval("mfr") ? getval("mfr") : "unknown");
		dstate_setinfo("ups.model", "%s", getval("model") ? getval("model") : "unknown");

		upslogx(LOG_INFO, "Megatec protocol UPS detected.");
	}

	dstate_setinfo("ups.serial", "%s", getval("serial") ? getval("serial") : "unknown");

	/*
	 * Workaround for buggy models.
	 */
	ignore_off = testvar("ignoreoff");

	if (status.flags[FL_LOAD_OFF] == '1' && status.load > 0.01 && !ignore_off) {
		ignore_off = 1;
		upslogx(LOG_INFO, "The UPS reports OFF status but appears to be ON. Parameter \"ignoreoff\" set automatically.");
	}
	upsdebugx(2, "Parameter [ignoreoff]: [%s]", (ignore_off ? "true" : "false"));

	/*
	 * Set battery-related values.
	 */
	if (get_firmware_values(&values) >= 0) {
		dstate_setinfo("battery.voltage.nominal", "%.1f", values.battvolt);
		dstate_setinfo("input.voltage.nominal", "%.1f", values.volt);
		dstate_setinfo("input.frequency.nominal", "%.1f", values.freq);

		if (set_battery_params(values.battvolt, status.battvolt) < 0) {
			upslogx(LOG_NOTICE, "This UPS has an unsupported combination of battery voltage/number of batteries.");
		}
	}

	if (getval("battvoltmult")) {
		upsdebugx(2, "Parameter [battvoltmult]: [%s]", getval("battvoltmult"));

		/* Having SHRT_MAX as the upper-bound is an arbitrary choice... */
		battvolt_mult = CLAMP(atof(getval("battvoltmult")), 1, SHRT_MAX);

		upslogx(LOG_NOTICE, "The battery voltage reported by the UPS will be multiplied by %.1f.", battvolt_mult);
	}

	if (getval("battvolts")) {
		upsdebugx(2, "Parameter [battvolts]: [%s]", getval("battvolts"));

		if (sscanf(getval("battvolts"), "%f:%f", &battvolt_empty, &battvolt_full) != 2) {
			fatalx(EXIT_FAILURE, "Error in \"battvolts\" parameter.");
		}

		upslogx(LOG_NOTICE, "Overriding battery voltage interval [%.1fV, %.1fV].", battvolt_empty, battvolt_full);
	}

	if (battvolt_empty < 0 || battvolt_full < 0) {
		upslogx(LOG_NOTICE, "Cannot calculate charge percentage for this UPS.");
	}

	if (getval("lowbatt")) {
		if (battvolt_empty < 0 || battvolt_full < 0) {
			upslogx(LOG_NOTICE, "Ignoring \"lowbatt\" parameter.");
		} else {
			lowbatt = CLAMP(atof(getval("lowbatt")), 0, 100);
		}
	}

	/*
	 * Set the restart and shutdown delays.
	 */
	if (getval("ondelay")) {
		start_delay = CLAMP(atoi(getval("ondelay")), 0, MAX_START_DELAY);
	}

	if (getval("offdelay")) {
		shutdown_delay = CLAMP(atoi(getval("offdelay")), 0, MAX_SHUTDOWN_DELAY);
	}

	dstate_setinfo("ups.delay.start", "%d", start_delay);
	dstate_setinfo("ups.delay.shutdown", "%d", shutdown_delay);

	/*
	 * Register the available instant commands.
	 */
	dstate_addcmd("test.battery.start.deep");
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("load.on");
	dstate_addcmd("load.off");
	dstate_addcmd("reset.input.minmax");
	dstate_addcmd("reset.watchdog");
	dstate_addcmd("beeper.toggle");

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;

	upsdebugx(1, "Done setting up the UPS.");
}


void upsdrv_updateinfo(void)
{
	QueryValues_t query;
	float charge;
	static int poll_fail = 0;

	if (run_query(&query) < 0) {
		/*
		 * Query wasn't successful (we got some weird
		 * response), however we won't fatalx(EXIT_FAILURE, ) as this
		 * happens sometimes when the ups is offline.
		 *
		 * Some fault tolerance is good, we just assume
		 * that the UPS is just taking a nap. ;)
		 */
		poll_fail++;
		upsdebugx(2, "Poll failure [%d].", poll_fail);
#ifndef WIN32 /*FIXME */
		ser_comm_fail("No status from UPS.");
#endif

		if (poll_fail >= MAX_POLL_FAILURES) {
			upsdebugx(2, "Too many poll failures, data is stale.");
			dstate_datastale();
		}

		return;
	}

	poll_fail = 0;
#ifndef WIN32 /*FIXME*/
	ser_comm_good();
#endif

	dstate_setinfo("input.voltage", "%.1f", query.ivolt);
	dstate_setinfo("input.voltage.fault", "%.1f", query.fvolt);
	dstate_setinfo("output.voltage", "%.1f", query.ovolt);
	dstate_setinfo("ups.load", "%.1f", query.load);
	dstate_setinfo("input.frequency", "%.1f", query.freq);

	/*
	 * The battery voltage multiplier should only be applied to battery.voltage
	 * in order not to break the charge calculation (that uses the 'raw' value
	 * that is reported by the UPS).
	 */
	dstate_setinfo("battery.voltage", "%.2f", battvolt_mult * query.battvolt);

	if (query.temp > 0.01) {
		dstate_setinfo("ups.temperature", "%.1f", query.temp);
	}

	charge = get_battery_charge(query.battvolt);
	if (charge >= 0) {
		dstate_setinfo("battery.charge", "%.1f", charge);

		upsdebugx(2, "Calculated battery charge: %.1f%%", charge);
	}

	dstate_setinfo("ups.beeper.status", query.flags[FL_BEEPER_ON] == '1' ? "enabled" : "disabled");

	status_init();

	/*
	 * Some models, when OFF, never change to on-battery status when
	 * line power is unavailable. To get around this, we also look at
	 * the input voltage level here.
	 */
	if (query.flags[FL_ON_BATT] == '1' || query.ivolt < RESIDUAL_VOLTAGE) {
		status_set("OB");
	} else {
		status_set("OL");

		if (query.flags[FL_BOOST_TRIM] == '1') {
			if (query.ivolt < query.ovolt) {
				status_set("BOOST");
			} else if (query.ivolt > query.ovolt) {
				status_set("TRIM");
			} else {
				status_set("BYPASS");
			}
		}

		/* Update minimum and maximum input voltage levels too */
		if (query.ivolt < ivolt_min) {
			ivolt_min = query.ivolt;
		}

		if (query.ivolt > ivolt_max) {
			ivolt_max = query.ivolt;
		}

		dstate_setinfo("input.voltage.minimum", "%.1f", ivolt_min);
		dstate_setinfo("input.voltage.maximum", "%.1f", ivolt_max);
	}

	/*
	 * If "lowbatt > 0", it becomes a "soft" low battery level
	 * and the hardware flag "FL_LOW_BATT" is always ignored.
	 */
	if ((lowbatt <= 0 && query.flags[FL_LOW_BATT] == '1') ||
	    (lowbatt > 0 && charge < lowbatt)) {
		status_set("LB");
	}

	if (query.flags[FL_BATT_TEST] == '1') {
		status_set("CAL");
	}

	if (query.flags[FL_LOAD_OFF] == '1' && !ignore_off) {
		status_set("OFF");
	}

	alarm_init();

	if (query.flags[FL_FAILED] == '1') {
		alarm_set("Internal UPS fault!");
	}

	alarm_commit();

	status_commit();

	dstate_dataok();
}


void upsdrv_shutdown(void)
{
	int s_wait = getval("offdelay") ? CLAMP(atoi(getval("offdelay")), 0, MAX_SHUTDOWN_DELAY) : shutdown_delay;
	int r_wait = getval("ondelay") ? CLAMP(atoi(getval("ondelay")), 0, MAX_START_DELAY) : start_delay;

	upslogx(LOG_INFO, "Shutting down UPS.");
#ifndef WIN32 /*FIXME*/
	ser_send_pace(upsfd, send_pace, "C%c", ENDCHAR);
	ser_send_pace(upsfd, send_pace, "S%02dR%04d%c", s_wait, r_wait, ENDCHAR);
#endif
}


int instcmd(const char *cmdname, const char *extra)
{
#ifndef WIN32
	char buffer[RECV_BUFFER_LEN];

	/*
	 * Some commands are always supported by every UPS implementing
	 * the megatec protocol, but others may or may not be supported.
	 * Unsupported commands are echoed back without ENDCHAR.
	 */

	if (strcasecmp(cmdname, "test.battery.start.deep") == 0) {
		ser_flush_io(upsfd);
		ser_send_pace(upsfd, send_pace, "TL%c", ENDCHAR);
		usleep(READ_PACE);

		if (ser_get_line(upsfd, buffer, 2 + 1, '\0', IGNCHARS, READ_TIMEOUT, 0) > 0) {
			upslogx(LOG_NOTICE, "test.battery.start.deep not supported by UPS hardware.");
		} else {
			upslogx(LOG_INFO, "Deep battery test started.");
		}

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "test.battery.start") == 0) {
		ser_flush_io(upsfd);
		ser_send_pace(upsfd, send_pace, "T%c", ENDCHAR);
		usleep(READ_PACE);

		if (ser_get_line(upsfd, buffer, 1 + 1, '\0', IGNCHARS, READ_TIMEOUT, 0) > 0) {
			upslogx(LOG_NOTICE, "test.battery.start not supported by UPS hardware.");
		} else {
			upslogx(LOG_INFO, "Battery test started.");
		}

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "test.battery.stop") == 0) {
		ser_flush_io(upsfd);
		ser_send_pace(upsfd, send_pace, "CT%c", ENDCHAR);
		usleep(READ_PACE);

		if (ser_get_line(upsfd, buffer, 2 + 1, '\0', IGNCHARS, READ_TIMEOUT, 0) > 0) {
			upslogx(LOG_NOTICE, "test.battery.stop not supported by UPS hardware.");
		} else {
			upslogx(LOG_INFO, "Battery test stopped.");
		}

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "shutdown.return") == 0) {
		ser_send_pace(upsfd, send_pace, "C%c", ENDCHAR);
		watchdog_enabled = 0;

		ser_send_pace(upsfd, send_pace, "S%02dR%04d%c", shutdown_delay, start_delay, ENDCHAR);

		upslogx(LOG_INFO, "Shutdown (return) initiated.");

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "shutdown.stayoff") == 0) {
		ser_send_pace(upsfd, send_pace, "C%c", ENDCHAR);
		watchdog_enabled = 0;

		ser_flush_io(upsfd);
		ser_send_pace(upsfd, send_pace, "S%02d%c", shutdown_delay, ENDCHAR);
		usleep(READ_PACE);

		if (ser_get_line(upsfd, buffer, 3 + 1, '\0', IGNCHARS, READ_TIMEOUT, 0) > 0) {
			ser_send_pace(upsfd, send_pace, "S%02dR9999%c", shutdown_delay, ENDCHAR);
			upslogx(LOG_NOTICE, "UPS refuses to turn the load off indefinitely. Will turn off for 9999 minutes instead.");
		}

		upslogx(LOG_INFO, "Shutdown (stayoff) initiated.");

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "shutdown.stop") == 0) {
		ser_send_pace(upsfd, send_pace, "C%c", ENDCHAR);
		watchdog_enabled = 0;

		upslogx(LOG_INFO, "Shutdown canceled.");

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "load.on") == 0) {
		ser_send_pace(upsfd, send_pace, "C%c", ENDCHAR);
		watchdog_enabled = 0;

		upslogx(LOG_INFO, "Turning the load on.");

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "load.off") == 0) {
		ser_send_pace(upsfd, send_pace, "C%c", ENDCHAR);
		watchdog_enabled = 0;

		ser_flush_io(upsfd);
		ser_send_pace(upsfd, send_pace, "S00%c", ENDCHAR);
		usleep(READ_PACE);

		if (ser_get_line(upsfd, buffer, 3 + 1, '\0', IGNCHARS, READ_TIMEOUT, 0) > 0) {
			ser_send_pace(upsfd, send_pace, "S00R9999%c", ENDCHAR);
			upslogx(LOG_NOTICE, "UPS refuses to turn the load off indefinitely. Will turn off for 9999 minutes instead.");
		}

		upslogx(LOG_INFO, "Turning the load off.");

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "reset.input.minmax") == 0) {
		ivolt_min = INT_MAX;
		ivolt_max = -1;

		dstate_setinfo("input.voltage.minimum", "%.1f", ivolt_min);
		dstate_setinfo("input.voltage.maximum", "%.1f", ivolt_max);

		upslogx(LOG_INFO, "Resetting minimum and maximum input voltage values.");

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "reset.watchdog") == 0) {
		ser_send_pace(upsfd, send_pace, "C%c", ENDCHAR);
		ser_send_pace(upsfd, send_pace, "S%02dR0001%c", watchdog_timeout, ENDCHAR);

		if (watchdog_enabled) {
			upsdebugx(2, "Resetting the UPS watchdog.");
		} else {
			watchdog_enabled = 1;
			upslogx(LOG_INFO, "UPS watchdog started.");
		}

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "beeper.toggle") == 0) {
		ser_flush_io(upsfd);
		ser_send_pace(upsfd, send_pace, "Q%c", ENDCHAR);
		usleep(READ_PACE);

		if (ser_get_line(upsfd, buffer, 1 + 1, '\0', IGNCHARS, READ_TIMEOUT, 0) > 0) {
			upslogx(LOG_NOTICE, "beeper.toggle not supported by UPS hardware.");
		} else {
			upslogx(LOG_INFO, "Toggling UPS beeper.");
		}

		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
#endif
	return STAT_INSTCMD_UNKNOWN;
}


int setvar(const char *varname, const char *val)
{
	return STAT_SET_UNKNOWN;
}


void upsdrv_help(void)
{
}


void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "mfr", "Manufacturer name");
	addvar(VAR_VALUE, "model", "Model name");
	addvar(VAR_VALUE, "serial", "UPS serial number");
	addvar(VAR_VALUE, "lowbatt", "Low battery level (%)");
	addvar(VAR_VALUE, "ondelay", "Min. delay before UPS startup (minutes)");
	addvar(VAR_VALUE, "offdelay", "Delay before UPS shutdown (minutes)");
	addvar(VAR_VALUE, "battvolts", "Battery voltages (empty:full)");
	addvar(VAR_VALUE, "battvoltmult", "Battery voltage multiplier");
	addvar(VAR_FLAG , "ignoreoff", "Ignore the OFF status from the UPS");
	addvar(VAR_VALUE, "sendpace", "Interval between command chars (msec)");
	addvar(VAR_VALUE, "dtr", "Serial DTR line state (0/1)");
	addvar(VAR_VALUE, "rts", "Serial RTS line state (0/1)");

	megatec_subdrv_makevartable();
}


void upsdrv_initups(void)
{
#ifndef WIN32 /*FIXME*/
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);

	if (getval("dtr")) {
		upsdebugx(2, "Parameter [dtr]: [%s]", getval("dtr"));

		if (strcmp(getval("dtr"), "0") != 0 && strcmp(getval("dtr"), "1") != 0) {
			fatalx(EXIT_FAILURE, "Error in \"dtr\" parameter.");
		}

		state_dtr = atoi(getval("dtr"));
	}

	if (getval("rts")) {
		upsdebugx(2, "Parameter [rts]: [%s]", getval("rts"));

		if (strcmp(getval("rts"), "0") != 0 && strcmp(getval("rts"), "1") != 0) {
			fatalx(EXIT_FAILURE, "Error in \"rts\" parameter.");
		}

		state_rts = atoi(getval("rts"));
	}

	upsdebugx(2, "DTR=%d, RTS=%d", state_dtr, state_rts);

	ser_set_dtr(upsfd, state_dtr);
	ser_set_rts(upsfd, state_rts);
#endif
}


void upsdrv_cleanup(void)
{
#ifndef WIN32
	ser_set_dtr(upsfd, 0);
	ser_close(upsfd, device_path);
#endif
}


/* EOF - megatec.c */
