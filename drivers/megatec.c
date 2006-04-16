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


#include "main.h"
#include "serial.h"
#include "megatec.h"

#include <stdio.h>
#include <limits.h>
#include <string.h>


#define ENDCHAR  '\r'
#define IGNCHARS "(#"

#define RECV_BUFFER_LEN 128

/* The expected reply lengths (without IGNCHARS) */
#define F_CMD_REPLY_LEN  20
#define Q1_CMD_REPLY_LEN 45
#define I_CMD_REPLY_LEN 37

#define IDENT_MAXTRIES   5
#define IDENT_MINSUCCESS 3

#define SEND_PACE    100000  /* 100ms interval between chars */
#define READ_TIMEOUT 2       /* 2 seconds timeout on read */

#define MAX_START_DELAY    9999
#define MAX_SHUTDOWN_DELAY 99

/* Maximum length of a string representing these values */
#define MAX_START_DELAY_LEN 4
#define MAX_SHUTDOWN_DELAY_LEN 2

#define N_FLAGS 8

/* The UPS status flags */
#define FL_ON_BATT    0
#define FL_LOW_BATT   1
#define FL_BOOST_TRIM 2
#define FL_FAILED     3
#define FL_UPS_TYPE   4
#define FL_BATT_TEST  5
#define FL_LOAD_OFF   6
#define FL_BEEPER_ON  7  /* seemingly not used */

/*
 * Battery voltage limits
 */
 
/* Values obtained from a "Mustek PowerMust 600VA Plus" (12V). */
#define BATT_MIN_12V 9.7   /* Estimate by looking at Commander Pro */
#define BATT_MAX_12V 13.7

/* Values obtained from a "Mustek PowerMust 1000VA Plus" (24V). */
#define BATT_MIN_24V 19.4  /* Estimate from LB at 22.2V (using same factor as 12V models) */
#define BATT_MAX_24V 27.4

/* Values obtained from a "PowerWalker Line-Interactive VI1000" (24V, 2x12V battery). */
#define BATT_MIN_2x12V 18.8  /* Estimate from LB at 22.3V (using same factor as 12V models) */
#define BATT_MAX_2x12V 26.8

/*
 * For each UPS type, we define an upper bound (battery)
 * voltage that we know can _never_ be seen.
 */
#define UPPER_BOUND_12V 16
#define UPPER_BOUND_24V 30

/* Maximum lengths for the "I" command reply fields */
#define UPS_MFR_CHARS     15
#define UPS_MODEL_CHARS   10
#define UPS_VERSION_CHARS 10


/* The values returned by the UPS for an "I" query */
typedef struct {
	char mfr[UPS_MFR_CHARS + 1];
	char model[UPS_MODEL_CHARS + 1];
	char version[UPS_VERSION_CHARS + 1];
} UPSInfo;


/* The values returned by the UPS for an "F" query */
typedef struct {
	float volt;
	float current;
	float battvolt;
	float freq;
} FirmwareValues;


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
} QueryValues;


/* Defined in upsdrv_initinfo */
static float battvolt_min;
static float battvolt_max;

/* Minimum and maximum voltage seen on input */
static float ivolt_min = INT_MAX;
static float ivolt_max = -1;

/* In minutes: */
static short start_delay = 3;     /* wait this amount of time to come back online */
static short shutdown_delay = 2;  /* wait until going offline */

/* In percentage: */
static float lowbatt = 0;  /* disabled */


static float batt_charge_pct(float battvolt);
static int check_ups(void);
static char *copy_field(char* dest, char *src, int field_len);
static int get_ups_info(UPSInfo *info);
static int get_firmware_values(FirmwareValues *values);
static int run_query(QueryValues *values);
int instcmd(const char *cmdname, const char *extra);
int setvar(const char *varname, const char *val);


/* I know, macros should evaluate their arguments only once */
#define CLAMP(x, min, max) (((x) < (min)) ? (min) : (((x) > (max)) ? (max) : (x)))


static float batt_charge_pct(float battvolt)
{
	float value;

	battvolt = CLAMP(battvolt, battvolt_min, battvolt_max);
	value = (battvolt - battvolt_min) / (battvolt_max - battvolt_min);

	return value * 100;
}


static int check_ups(void)
{
	char buffer[RECV_BUFFER_LEN];
	int ret;

	upsdebugx(2, "Sending \"F\" command...");
	ser_send_pace(upsfd, SEND_PACE, "F%c", ENDCHAR);
	ret = ser_get_line(upsfd, buffer, RECV_BUFFER_LEN, ENDCHAR, IGNCHARS, READ_TIMEOUT, 0);
	if (ret < F_CMD_REPLY_LEN) {
		upsdebugx(2, "Wrong answer to \"F\" command.");

		return -1;
	}
	upsdebugx(2, "\"F\" command successful.");

	upsdebugx(2, "Sending \"Q1\" command...");
	ser_send_pace(upsfd, SEND_PACE, "Q1%c", ENDCHAR);
	ret = ser_get_line(upsfd, buffer, RECV_BUFFER_LEN, ENDCHAR, IGNCHARS, READ_TIMEOUT, 0);
	if (ret < Q1_CMD_REPLY_LEN) {
		upsdebugx(2, "Wrong answer to \"Q1\" command.");

		return -1;
	}
	upsdebugx(2, "\"Q1\" command successful.");

	return 0;
}


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


static int get_ups_info(UPSInfo *info)
{
	char buffer[RECV_BUFFER_LEN];
	char *anchor;
	int ret;

	upsdebugx(1, "Asking for UPS information (\"I\" command)...");
	ser_send_pace(upsfd, SEND_PACE, "I%c", ENDCHAR);
	ret = ser_get_line(upsfd, buffer, RECV_BUFFER_LEN, ENDCHAR, IGNCHARS, READ_TIMEOUT, 0);
	if (ret < I_CMD_REPLY_LEN) {
		upsdebugx(1, "UPS doesn't return any information about itself.");
		
		return -1;
	}

	upsdebugx(3, "UPS information: %s", buffer);

	memset(info, 0, sizeof(UPSInfo));

	/*
	 * Get the manufacturer, model and version fields, skipping
	 * the separator character that sits between them.
	 */
	anchor = copy_field(info->mfr, buffer, UPS_MFR_CHARS);
	anchor = copy_field(info->model, anchor + 1, UPS_MODEL_CHARS);
	copy_field(info->version, anchor + 1, UPS_VERSION_CHARS);

	return 0;
}


static int get_firmware_values(FirmwareValues *values)
{
	char buffer[RECV_BUFFER_LEN];
	int ret;

	upsdebugx(1, "Asking for UPS power ratings (\"F\" command)...");
	ser_send_pace(upsfd, SEND_PACE, "F%c", ENDCHAR);
	ret = ser_get_line(upsfd, buffer, RECV_BUFFER_LEN, ENDCHAR, IGNCHARS, READ_TIMEOUT, 0);
	if (ret < F_CMD_REPLY_LEN) {
		upsdebugx(1, "UPS doesn't return any information about its power ratings.");
		
		return -1;
	}

	upsdebugx(3, "UPS power ratings: %s", buffer);

	sscanf(buffer, "%f %f %f %f", &values->volt, &values->current,
	       &values->battvolt, &values->freq);

	return 0;
}


static int run_query(QueryValues *values)
{
	char buffer[RECV_BUFFER_LEN];
	int ret;

	upsdebugx(1, "Asking for UPS status (\"Q1\" command)...");
	ser_send_pace(upsfd, SEND_PACE, "Q1%c", ENDCHAR);
	ret = ser_get_line(upsfd, buffer, RECV_BUFFER_LEN, ENDCHAR, IGNCHARS, READ_TIMEOUT, 0);
	if (ret < Q1_CMD_REPLY_LEN) {
		upsdebugx(1, "UPS doesn't return any information about its status.");
		
		return -1;
	}

	upsdebugx(3, "UPS status: %s", buffer);

	sscanf(buffer, "%f %f %f %f %f %f %f %s", &values->ivolt, &values->fvolt, &values->ovolt,
	       &values->load, &values->freq, &values->battvolt, &values->temp, values->flags);

	return 0;
}


void upsdrv_initinfo(void)
{
	int i;
	int success = 0;
	FirmwareValues values;
	QueryValues query;
	UPSInfo info;

	upsdebugx(1, "Starting UPS detection process...");	
	for (i = 0; i < IDENT_MAXTRIES; i++) {
		upsdebugx(2, "Attempting to detect the UPS...");
		if (check_ups() == 0) {
			success++;
		}
	}

	upsdebugx(1, "%d out of %d detection attempts failed (minimum failures: %d).",
	          IDENT_MAXTRIES - success, IDENT_MAXTRIES, IDENT_MAXTRIES - IDENT_MINSUCCESS);

	if (success < IDENT_MINSUCCESS) {
		if (success > 0) {
			fatalx("The UPS is supported, but the connection is too unreliable. Try checking the cable for defects.");
		} else {
			fatalx("Megatec protocol UPS not detected.");
		}
	}
	
	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);

	/*
	 * Try to identify the UPS.
	 */
	if (get_ups_info(&info) >= 0) {
		char model[UPS_MODEL_CHARS + UPS_VERSION_CHARS + 2];
		sprintf(model, "%s %s", info.model, info.version);

		dstate_setinfo("ups.mfr", "%s", getval("mfr") ? getval("mfr") : info.mfr);
		dstate_setinfo("ups.model", "%s", getval("model") ? getval("model") : model);

		upslogx(LOG_INFO, "Megatec protocol UPS detected [%s %s %s].", info.mfr, info.model, info.version);
	} else {
		dstate_setinfo("ups.mfr", "%s", getval("mfr") ? getval("mfr") : "unknown");
		dstate_setinfo("ups.model", "%s", getval("model") ? getval("model") : "unknown");

		upslogx(LOG_INFO, "Megatec protocol UPS detected.");
	}

	dstate_setinfo("ups.serial", "%s", getval("serial") ? getval("serial") : "unknown");

	if (get_firmware_values(&values) < 0) {
		fatalx("Error reading firmware values from UPS!");
	}

	dstate_setinfo("output.voltage.nominal", "%.1f", values.volt);
	dstate_setinfo("battery.voltage.nominal", "%.1f", values.battvolt);

	if (run_query(&query) < 0) {
		fatalx("Error reading status from UPS!");
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
	if (query.battvolt <= UPPER_BOUND_12V) {
		upsdebugx(1, "This looks like a 12V UPS.");
		
		battvolt_min = BATT_MIN_12V;
		battvolt_max = BATT_MAX_12V;
	} else if (query.battvolt <= UPPER_BOUND_24V) {
		if (values.battvolt == 12) {
			upsdebugx(1, "This looks like a 2x12V UPS.");

			battvolt_min = BATT_MIN_2x12V;
			battvolt_max = BATT_MAX_2x12V;
		} else {
			upsdebugx(1, "This looks like a 24V UPS.");

			battvolt_min = BATT_MIN_24V;
			battvolt_max = BATT_MAX_24V;
		}
	} else {
		battvolt_min = 0;
		battvolt_max = INT_MAX;

		upslogx(LOG_WARNING, "This UPS has an unsupported battery voltage range."
		                     " The \"battery.charge\" value will be bogus.");
	}

	if (getval("lowbatt")) {
		lowbatt = CLAMP(atof(getval("lowbatt")), 0, 100);
	}

	/*
	 * Register the available variables.
	 */
	dstate_setinfo("ups.delay.start", "%d", start_delay);
	dstate_setflags("ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.start", MAX_START_DELAY_LEN);

	dstate_setinfo("ups.delay.shutdown", "%d", shutdown_delay);
	dstate_setflags("ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.shutdown", MAX_SHUTDOWN_DELAY_LEN);

	/*
	 * Register the available instant commands.
	 */
	dstate_addcmd("test.battery.start");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("load.on");
	dstate_addcmd("load.off");
	dstate_addcmd("reset.input.minmax");

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;

	/* clean up a possible shutdown in progress */
	ser_send_pace(upsfd, SEND_PACE, "C%c", ENDCHAR);

	upsdebugx(1, "Done setting up the UPS.");
}


void upsdrv_updateinfo(void)
{
	QueryValues query;
	float charge;
	
	if (run_query(&query) < 0) {
		/*
		 * Query wasn't successful (we got some weird
		 * response), however we won't fatalx() as this
		 * happens sometimes when the ups is offline.
		 *
		 * Some fault tolerance is good, we just assume
		 * that the UPS is just taking a nap. ;)
		 */
		dstate_datastale();

		return;
	}

	dstate_setinfo("input.voltage", "%.1f", query.ivolt);
	dstate_setinfo("input.voltage.fault", "%.1f", query.fvolt);
	dstate_setinfo("output.voltage", "%.1f", query.ovolt);
	dstate_setinfo("ups.load", "%.1f", query.load);
	dstate_setinfo("input.frequency", "%.1f", query.freq);
	dstate_setinfo("battery.voltage", "%.1f", query.battvolt);
	dstate_setinfo("ups.temperature", "%.1f", query.temp);

	charge = batt_charge_pct(query.battvolt);
	dstate_setinfo("battery.charge", "%.1f", charge);

	status_init();

	if (query.flags[FL_LOAD_OFF] == '1') {
		status_set("OFF");
	} else if (query.flags[FL_ON_BATT] == '1' || query.flags[FL_BATT_TEST] == '1') {
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
	}

	/*
	 * If "lowbatt > 0", it becomes a "soft" low battery level
	 * and the hardware flag "FL_LOW_BATT" is always ignored.
	 */
	if ((lowbatt <= 0 && query.flags[FL_LOW_BATT] == '1') ||
	    (lowbatt > 0 && charge < lowbatt)) {
		status_set("LB");
	}

	if (query.flags[FL_FAILED] == '1') {
		status_set("FAILED");
	}

	status_commit();

	/* Update minimum and maximum input voltage levels only when on line */
	if (query.flags[FL_ON_BATT] == '0') {
		if (query.ivolt < ivolt_min) {
			ivolt_min = query.ivolt;
		}

		if (query.ivolt > ivolt_max) {
			ivolt_max = query.ivolt;
		}

		dstate_setinfo("input.voltage.minimum", "%.1f", ivolt_min);
		dstate_setinfo("input.voltage.maximum", "%.1f", ivolt_max);
	}

	dstate_dataok();
}


void upsdrv_shutdown(void)
{
	upslogx(LOG_INFO, "Shutting down UPS immediately.");

	ser_send_pace(upsfd, SEND_PACE, "C%c", ENDCHAR);
	ser_send_pace(upsfd, SEND_PACE, "S00R%04d%c", start_delay, ENDCHAR);
}


int instcmd(const char *cmdname, const char *extra)
{
	if (strcasecmp(cmdname, "test.battery.start") == 0) {
		ser_send_pace(upsfd, SEND_PACE, "C%c", ENDCHAR);
		ser_send_pace(upsfd, SEND_PACE, "T%c", ENDCHAR);

		upslogx(LOG_INFO, "Start battery test for 10 seconds.");

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "shutdown.return") == 0) {
		ser_send_pace(upsfd, SEND_PACE, "C%c", ENDCHAR);
		ser_send_pace(upsfd, SEND_PACE, "S%02dR%04d%c", shutdown_delay, start_delay, ENDCHAR);

		upslogx(LOG_INFO, "Shutdown (return) initiated.");

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "shutdown.stayoff") == 0) {
		ser_send_pace(upsfd, SEND_PACE, "C%c", ENDCHAR);
		ser_send_pace(upsfd, SEND_PACE, "S%02dR0000%c", shutdown_delay, ENDCHAR);

		upslogx(LOG_INFO, "Shutdown (stayoff) initiated.");

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "shutdown.stop") == 0) {
		ser_send_pace(upsfd, SEND_PACE, "C%c", ENDCHAR);

		upslogx(LOG_INFO, "Shutdown canceled.");

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "load.on") == 0) {
		ser_send_pace(upsfd, SEND_PACE, "C%c", ENDCHAR);

		upslogx(LOG_INFO, "Turning load on.");

		return STAT_INSTCMD_HANDLED;
	}

	if (strcasecmp(cmdname, "load.off") == 0) {
		ser_send_pace(upsfd, SEND_PACE, "C%c", ENDCHAR);
		ser_send_pace(upsfd, SEND_PACE, "S00R0000%c", ENDCHAR);

		upslogx(LOG_INFO, "Turning load off.");

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

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);

	return STAT_INSTCMD_UNKNOWN;
}


int setvar(const char *varname, const char *val)
{
	int delay;

	if (sscanf(val, "%d", &delay) != 1) {
		return STAT_SET_UNKNOWN;
	}

	if (strcasecmp(varname, "ups.delay.start") == 0) {    
		delay = CLAMP(delay, 0, MAX_START_DELAY);
		start_delay = delay;
		dstate_setinfo("ups.delay.start", "%d", delay);

		dstate_dataok();

		return STAT_SET_HANDLED;
	}

	if (strcasecmp(varname, "ups.delay.shutdown") == 0) {
		delay = CLAMP(delay, 0, MAX_SHUTDOWN_DELAY);
		shutdown_delay = delay;
		dstate_setinfo("ups.delay.shutdown", "%d", delay);

		dstate_dataok();

		return STAT_SET_HANDLED;
	}

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
}


void upsdrv_banner(void)
{
	printf("Network UPS Tools - Megatec protocol driver %s (%s)\n", DRV_VERSION, UPS_VERSION);
	printf("Carlos Rodrigues (c) 2003-2006\n\n");
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


/* EOF - megatec.c */
