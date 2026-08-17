/* voltronic-mib.c - SNMP support for Voltronic enterprise 43943 UPSes
 *
 * Copyright (C) 2026 Network UPS Tools contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The private MIB is used in preference to RFC1628.  Tested firmware uses
 * index 0 for private line tables and index 1 for RFC1628 line tables.
 */

#include "voltronic-mib.h"

#define VOLTRONIC_MIB_VERSION "0.10"

#define VOLTRONIC_SYSOID ".1.3.6.1.4.1.43943"
#define VOLTRONIC_UPS_OID VOLTRONIC_SYSOID ".1.1."
#define VOLTRONIC_RFC1628_OID ".1.3.6.1.2.1.33.1."

#define VOLTRONIC_DEFAULT_OFFDELAY "30"
#define VOLTRONIC_DEFAULT_ONDELAY "60"

static info_lkp_t voltronic_battery_status_info[] = {
	info_lkp_default(1, ""),       /* unknown */
	info_lkp_default(2, ""),       /* normal */
	info_lkp_default(3, "LB"),     /* low */
	info_lkp_default(4, "LB"),     /* depleted */
	info_lkp_default(5, ""),       /* discharging; source reports OB */
	info_lkp_default(6, "RB"),     /* failure */
	info_lkp_default(7, "RB"),     /* replace */
	info_lkp_sentinel
};

static info_lkp_t voltronic_power_source_info[] = {
	info_lkp_default(1, ""),
	info_lkp_default(2, "OFF"),
	info_lkp_default(3, "OL"),
	info_lkp_default(4, "OL BYPASS"),
	info_lkp_default(5, "OB"),
	info_lkp_default(6, "OL BOOST"),
	info_lkp_default(7, "OL TRIM"),
	info_lkp_default(8, "OB"),
	info_lkp_default(9, "OFF"),
	info_lkp_default(10, "OL"),    /* high-efficiency/ECO mode */
	info_lkp_default(11, "OL"),    /* converter mode */
	info_lkp_sentinel
};

static info_lkp_t voltronic_test_status_info[] = {
	info_lkp_default(8, "TEST"),
	info_lkp_sentinel
};

static info_lkp_t voltronic_battery_test_result_info[] = {
	info_lkp_default(1, "done and passed"),
	info_lkp_default(2, "done and warning"),
	info_lkp_default(3, "done and error"),
	info_lkp_default(4, "aborted"),
	info_lkp_default(5, "in progress"),
	info_lkp_default(6, "no test initiated"),
	info_lkp_sentinel
};

static info_lkp_t voltronic_ups_type_info[] = {
	info_lkp_default(0, "offline"),
	info_lkp_default(1, "line-interactive"),
	info_lkp_default(2, "online"),
	info_lkp_sentinel
};

static info_lkp_t voltronic_yes_no_info[] = {
	info_lkp_default(0, "no"),
	info_lkp_default(1, "yes"),
	info_lkp_default(2, ""),       /* unsupported */
	info_lkp_sentinel
};

static info_lkp_t voltronic_beeper_status_info[] = {
	info_lkp_default(1, "enabled"),
	info_lkp_default(2, "disabled"),
	info_lkp_sentinel
};

#if WITH_SNMP_LKP_FUN
static const char *voltronic_eco_mode(void *raw_value)
{
	long value = *((long *)raw_value);

	if (value != 10L) {
		errno = EINVAL;
		return NULL;
	}

	return "vendor:voltronic:ECO";
}

static info_lkp_t voltronic_eco_mode_info[] = {
	info_lkp_fun_vp2s(0, "dummy", voltronic_eco_mode),
	info_lkp_sentinel
};
#endif /* WITH_SNMP_LKP_FUN */

static snmp_info_t voltronic_mib[] = {
	/* Agent and UPS identity.  The private manufacturer object is empty on
	 * tested OEM firmware, so identify the protocol family, not its reseller. */
	snmp_info_default("ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "Voltronic", SU_FLAG_ABSENT, NULL),
	snmp_info_default("ups.model", ST_FLAG_STRING, SU_INFOSIZE, VOLTRONIC_UPS_OID "1.3.0", NULL, SU_FLAG_STATIC, NULL),
	snmp_info_default("ups.serial", ST_FLAG_STRING, SU_INFOSIZE, VOLTRONIC_UPS_OID "1.4.0", NULL, SU_FLAG_STATIC, NULL),
	snmp_info_default("ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, VOLTRONIC_UPS_OID "1.6.0", NULL, SU_FLAG_STATIC, NULL),
	snmp_info_default("ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, VOLTRONIC_RFC1628_OID "1.4.0", NULL, SU_FLAG_STATIC, NULL),
	snmp_info_default("ups.type", ST_FLAG_STRING, SU_INFOSIZE, VOLTRONIC_UPS_OID "1.7.0", NULL, SU_FLAG_STATIC, voltronic_ups_type_info),

	/* Ratings.  Private objects use tenths and retain the rated 1000 VA / 900 W
	 * distinction.  Canonical aggregate names precede compatibility aliases. */
	snmp_info_default("input.voltage.nominal", 0, 0.1, VOLTRONIC_UPS_OID "2.1.0", NULL, SU_FLAG_STATIC | SU_FLAG_ZEROINVALID, NULL),
	snmp_info_default("input.frequency.nominal", 0, 0.1, VOLTRONIC_RFC1628_OID "9.2.0", NULL, SU_FLAG_STATIC | SU_FLAG_ZEROINVALID, NULL),
	snmp_info_default("output.voltage.nominal", 0, 0.1, VOLTRONIC_UPS_OID "2.2.0", NULL, SU_FLAG_STATIC | SU_FLAG_ZEROINVALID, NULL),
	snmp_info_default("output.frequency.nominal", 0, 0.1, VOLTRONIC_UPS_OID "2.3.0", NULL, SU_FLAG_STATIC | SU_FLAG_ZEROINVALID, NULL),
	snmp_info_default("output.current.nominal", 0, 0.1, VOLTRONIC_UPS_OID "2.4.0", NULL, SU_FLAG_STATIC | SU_FLAG_ZEROINVALID, NULL),
	snmp_info_default("ups.power.nominal", 0, 0.1, VOLTRONIC_UPS_OID "2.5.0", NULL, SU_FLAG_STATIC | SU_FLAG_ZEROINVALID, NULL),
	snmp_info_default("output.power.nominal", 0, 0.1, VOLTRONIC_UPS_OID "2.5.0", NULL, SU_FLAG_STATIC | SU_FLAG_ZEROINVALID, NULL),
	snmp_info_default("ups.realpower.nominal", 0, 0.1, VOLTRONIC_UPS_OID "2.6.0", NULL, SU_FLAG_STATIC | SU_FLAG_ZEROINVALID, NULL),
	snmp_info_default("output.realpower.nominal", 0, 0.1, VOLTRONIC_UPS_OID "2.6.0", NULL, SU_FLAG_STATIC | SU_FLAG_ZEROINVALID, NULL),
	snmp_info_default("battery.voltage.nominal", 0, 0.1, VOLTRONIC_UPS_OID "2.7.0", NULL, SU_FLAG_STATIC | SU_FLAG_ZEROINVALID, NULL),

	/* Topology and status. */
	snmp_info_default("input.phases", 0, 1.0, VOLTRONIC_UPS_OID "4.4.0", NULL, 0, NULL),
	snmp_info_default("output.phases", 0, 1.0, VOLTRONIC_UPS_OID "5.6.0", NULL, 0, NULL),
	snmp_info_default("input.bypass.phases", 0, 1.0, VOLTRONIC_UPS_OID "6.2.0", NULL, 0, NULL),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, VOLTRONIC_UPS_OID "5.1.0", NULL, SU_STATUS_PWR, voltronic_power_source_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, VOLTRONIC_UPS_OID "5.1.0", NULL, SU_STATUS_CAL, voltronic_test_status_info),
	snmp_info_default("ups.status", ST_FLAG_STRING, SU_INFOSIZE, VOLTRONIC_UPS_OID "3.1.0", NULL, SU_STATUS_BATT, voltronic_battery_status_info),
#if WITH_SNMP_LKP_FUN
	snmp_info_default("experimental.ups.mode.buzzwords", ST_FLAG_STRING, SU_INFOSIZE, VOLTRONIC_UPS_OID "5.1.0", NULL, 0, voltronic_eco_mode_info),
#endif

	/* Battery measurements.  RFC1628 entries are lower-precision fallbacks and
	 * SU_FLAG_UNIQUE prevents them from replacing a working private object. */
	snmp_info_default("battery.runtime", 0, 60.0, VOLTRONIC_UPS_OID "3.3.0", NULL, SU_FLAG_UNIQUE | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("battery.runtime", 0, 60.0, VOLTRONIC_RFC1628_OID "2.3.0", NULL, SU_FLAG_UNIQUE | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("battery.charge", 0, 1.0, VOLTRONIC_UPS_OID "3.4.0", NULL, SU_FLAG_UNIQUE | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("battery.charge", 0, 1.0, VOLTRONIC_RFC1628_OID "2.4.0", NULL, SU_FLAG_UNIQUE | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("battery.voltage", 0, 0.1, VOLTRONIC_UPS_OID "3.5.0", NULL, SU_FLAG_UNIQUE | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("battery.voltage", 0, 0.1, VOLTRONIC_RFC1628_OID "2.5.0", NULL, SU_FLAG_UNIQUE | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("battery.temperature", 0, 0.1, VOLTRONIC_UPS_OID "3.13.0", NULL, SU_FLAG_UNIQUE | SU_FLAG_NEGINVALID, NULL),
	snmp_info_default("battery.temperature", 0, 1.0, VOLTRONIC_RFC1628_OID "2.7.0", NULL, SU_FLAG_UNIQUE | SU_FLAG_NEGINVALID, NULL),

	/* Private line tables are zero-based on tested firmware. */
	snmp_info_default("input.frequency", 0, 0.1, VOLTRONIC_UPS_OID "4.5.1.2.0", NULL, SU_FLAG_UNIQUE | SU_INPUT_1, NULL),
	snmp_info_default("input.frequency", 0, 0.1, VOLTRONIC_RFC1628_OID "3.3.1.2.1", NULL, SU_FLAG_UNIQUE | SU_INPUT_1, NULL),
	snmp_info_default("input.voltage", 0, 0.1, VOLTRONIC_UPS_OID "4.5.1.3.0", NULL, SU_FLAG_UNIQUE | SU_INPUT_1, NULL),
	snmp_info_default("input.voltage", 0, 1.0, VOLTRONIC_RFC1628_OID "3.3.1.3.1", NULL, SU_FLAG_UNIQUE | SU_INPUT_1, NULL),

	snmp_info_default("output.frequency", 0, 0.1, VOLTRONIC_UPS_OID "5.2.0", NULL, SU_FLAG_UNIQUE | SU_OUTPUT_1, NULL),
	snmp_info_default("output.frequency", 0, 0.1, VOLTRONIC_RFC1628_OID "4.2.0", NULL, SU_FLAG_UNIQUE | SU_OUTPUT_1, NULL),
	snmp_info_default("output.voltage", 0, 0.1, VOLTRONIC_UPS_OID "5.7.1.2.0", NULL, SU_FLAG_UNIQUE | SU_OUTPUT_1, NULL),
	snmp_info_default("output.voltage", 0, 1.0, VOLTRONIC_RFC1628_OID "4.4.1.2.1", NULL, SU_FLAG_UNIQUE | SU_OUTPUT_1, NULL),
	snmp_info_default("output.current", 0, 0.1, VOLTRONIC_UPS_OID "5.7.1.3.0", NULL, SU_OUTPUT_1, NULL),
	snmp_info_default("ups.realpower", 0, 0.1, VOLTRONIC_UPS_OID "5.7.1.5.0", NULL, SU_OUTPUT_1, NULL),
	snmp_info_default("output.realpower", 0, 0.1, VOLTRONIC_UPS_OID "5.7.1.5.0", NULL, SU_OUTPUT_1, NULL),
	snmp_info_default("ups.load", 0, 1.0, VOLTRONIC_UPS_OID "5.7.1.7.0", NULL, SU_OUTPUT_1 | SU_FLAG_NEGINVALID, NULL),

	snmp_info_default("input.bypass.frequency", 0, 0.1, VOLTRONIC_UPS_OID "6.1.0", NULL, SU_FLAG_UNIQUE | SU_BYPASS_1, NULL),
	snmp_info_default("input.bypass.frequency", 0, 0.1, VOLTRONIC_RFC1628_OID "5.1.0", NULL, SU_FLAG_UNIQUE | SU_BYPASS_1, NULL),
	snmp_info_default("input.bypass.voltage", 0, 0.1, VOLTRONIC_UPS_OID "6.3.1.2.0", NULL, SU_FLAG_UNIQUE | SU_BYPASS_1, NULL),
	snmp_info_default("input.bypass.voltage", 0, 1.0, VOLTRONIC_RFC1628_OID "5.3.1.2.1", NULL, SU_FLAG_UNIQUE | SU_BYPASS_1, NULL),

	/* These are mode-specific limits, unlike the zero-valued generic RFC1628
	 * transfer points exposed by this firmware. */
	snmp_info_default("input.transfer.bypass.high", 0, 0.1, VOLTRONIC_UPS_OID "11.21.0", NULL, SU_FLAG_SEMI_STATIC | SU_FLAG_ZEROINVALID, NULL),
	snmp_info_default("input.transfer.bypass.low", 0, 0.1, VOLTRONIC_UPS_OID "11.22.0", NULL, SU_FLAG_SEMI_STATIC | SU_FLAG_ZEROINVALID, NULL),
	snmp_info_default("input.transfer.eco.high", 0, 0.1, VOLTRONIC_UPS_OID "11.25.0", NULL, SU_FLAG_SEMI_STATIC | SU_FLAG_ZEROINVALID, NULL),
	snmp_info_default("input.transfer.eco.low", 0, 0.1, VOLTRONIC_UPS_OID "11.26.0", NULL, SU_FLAG_SEMI_STATIC | SU_FLAG_ZEROINVALID, NULL),

	/* Readable state plus delayed controls.  None of these commands were
	 * exercised during development against a protected live load. */
	snmp_info_default("ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, VOLTRONIC_UPS_OID "7.2.0", NULL, 0, voltronic_battery_test_result_info),
	snmp_info_default("ups.beeper.status", ST_FLAG_STRING, SU_INFOSIZE, VOLTRONIC_UPS_OID "8.4.0", NULL, 0, voltronic_beeper_status_info),
	snmp_info_default("ups.start.auto", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, VOLTRONIC_UPS_OID "11.3.0", NULL, SU_FLAG_SEMI_STATIC, voltronic_yes_no_info),
	snmp_info_default("ups.start.battery", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, VOLTRONIC_UPS_OID "11.12.0", NULL, SU_FLAG_SEMI_STATIC, voltronic_yes_no_info),
	snmp_info_default("ups.timer.shutdown", ST_FLAG_RW, 1.0, VOLTRONIC_UPS_OID "8.1.0", NULL, SU_TYPE_TIME, NULL),
	snmp_info_default("ups.timer.start", ST_FLAG_RW, 1.0, VOLTRONIC_UPS_OID "8.3.0", NULL, SU_TYPE_TIME, NULL),
	snmp_info_default("load.off.delay", 0, 1.0, VOLTRONIC_UPS_OID "8.1.0", VOLTRONIC_DEFAULT_OFFDELAY, SU_TYPE_CMD, NULL),
	snmp_info_default("load.on.delay", 0, 1.0, VOLTRONIC_UPS_OID "8.3.0", VOLTRONIC_DEFAULT_ONDELAY, SU_TYPE_CMD, NULL),
	snmp_info_default("shutdown.stop", 0, 1.0, VOLTRONIC_UPS_OID "8.1.0", "-2", SU_TYPE_CMD, NULL),
	snmp_info_default("beeper.enable", 0, 1.0, VOLTRONIC_UPS_OID "8.4.0", "1", SU_TYPE_CMD, NULL),
	snmp_info_default("beeper.disable", 0, 1.0, VOLTRONIC_UPS_OID "8.4.0", "2", SU_TYPE_CMD, NULL),

	snmp_info_sentinel
};

mib2nut_info_t voltronic = {
	"voltronic",
	VOLTRONIC_MIB_VERSION,
	VOLTRONIC_UPS_OID "5.1.0",
	VOLTRONIC_UPS_OID "1.3.0",
	voltronic_mib,
	VOLTRONIC_SYSOID,
	NULL
};
