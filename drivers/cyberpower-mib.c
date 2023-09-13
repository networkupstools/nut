/*  cyberpower-mib.c - data to monitor Cyberpower RMCARD
 *
 *  Copyright (C) 2010 - Eric Schultz <paradxum@mentaljynx.com>
 *
 *  derived (i.e. basically copied and modified) of bestpower by:
 *  Copyright (C) 2010 - Arnaud Quette <arnaud.quette@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "cyberpower-mib.h"

#define CYBERPOWER_MIB_VERSION		"0.55"
#define CYBERPOWER_OID_MODEL_NAME	".1.3.6.1.4.1.3808.1.1.1.1.1.1.0"

/* CPS-MIB::ups */
#define CYBERPOWER_SYSOID			".1.3.6.1.4.1.3808.1.1.1"

/* Per https://github.com/networkupstools/nut/issues/1997
 * some CPS devices offer the shorter vendor OID as sysOID
 */
#define CYBERPOWER_SYSOID2			".1.3.6.1.4.1.3808"

/* https://www.cyberpowersystems.com/products/software/mib-files/ */
/* Per CPS MIB 2.9 upsBaseOutputStatus OBJECT-TYPE: */
static info_lkp_t cyberpower_power_status[] = {
	{ 2, "OL", NULL, NULL },	/* onLine */
	{ 3, "OB", NULL, NULL },	/* onBattery */
	{ 4, "OL BOOST", NULL, NULL },	/* onBoost */
	{ 5, "OFF", NULL, NULL },	/* onSleep */
	{ 6, "OFF", NULL, NULL },	/* off */
	{ 7, "OL", NULL, NULL },	/* rebooting */
	{ 8, "OL", NULL, NULL },	/* onECO */
	{ 9, "OL BYPASS", NULL, NULL },	/* onBypass */
	{ 10, "OL TRIM", NULL, NULL },	/* onBuck */
	{ 11, "OL OVER", NULL, NULL },	/* onOverload */

	/* Note: a "NULL" string must be last due to snmp-ups.c parser logic */
	{ 1, "NULL", NULL, NULL },	/* unknown */

	{ 0, NULL, NULL, NULL }
} ;

static info_lkp_t cyberpower_battery_status[] = {
	{ 1, "", NULL, NULL },	/* unknown */
	{ 2, "", NULL, NULL },	/* batteryNormal */
	{ 3, "LB", NULL, NULL },	/* batteryLow */
	{ 0, NULL, NULL, NULL }
} ;

static info_lkp_t cyberpower_cal_status[] = {
	{ 1, "", NULL, NULL },          /* Calibration Successful */
	{ 2, "", NULL, NULL },          /* Calibration Invalid */
	{ 3, "CAL", NULL, NULL },       /* Calibration in progress */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t cyberpower_battrepl_status[] = {
	{ 1, "", NULL, NULL },          /* No battery needs replacing */
	{ 2, "RB", NULL, NULL },        /* Batteries need to be replaced */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t cyberpower_ups_alarm_info[] = {
	{ 1, "", NULL, NULL },                       /* Normal */
	{ 2, "Temperature too high!", NULL, NULL },  /* Overheat */
	{ 3, "Internal UPS fault!", NULL, NULL },    /* Hardware Fault */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t cyberpower_transfer_reasons[] = {
	{ 1, "noTransfer", NULL, NULL },
	{ 2, "highLineVoltage", NULL, NULL },
	{ 3, "brownout", NULL, NULL },
	{ 4, "selfTest", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t cyberpower_testdiag_results[] = {
	{ 1, "Ok", NULL, NULL },
	{ 2, "Failed", NULL, NULL },
	{ 3, "InvalidTest", NULL, NULL },
	{ 4, "TestInProgress", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

/* Snmp2NUT lookup table for CyberPower MIB */
static snmp_info_t cyberpower_mib[] = {

	/* standard MIB items */
	{ "device.description", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.1.0", NULL, SU_FLAG_OK, NULL },
	{ "device.contact", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.4.0", NULL, SU_FLAG_OK, NULL },
	{ "device.location", ST_FLAG_STRING | ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.2.1.1.6.0", NULL, SU_FLAG_OK, NULL },

	/* Device page */
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "ups",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },

	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "CYBERPOWER",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, CYBERPOWER_OID_MODEL_NAME,
		"CyberPower", SU_FLAG_STATIC, NULL },
	{ "ups.id", ST_FLAG_STRING | ST_FLAG_RW, 8, ".1.3.6.1.4.1.3808.1.1.1.1.1.2.0",
                "", SU_FLAG_OK | SU_FLAG_STATIC, NULL },

	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.1.2.3.0",
		"", SU_FLAG_STATIC, NULL },
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.1.2.1.0",
		"", SU_FLAG_STATIC, NULL },
	{ "ups.mfr.date", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.1.2.2.0", "",
		0, NULL },

	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.4.1.1.0", "",
		SU_FLAG_OK | SU_STATUS_PWR, &cyberpower_power_status[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.2.1.1.0", "",
		SU_FLAG_OK | SU_STATUS_BATT, &cyberpower_battery_status[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.7.2.7.0", "",
		SU_FLAG_OK | SU_STATUS_CAL, &cyberpower_cal_status[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.2.2.5.0", "",
		SU_FLAG_OK | SU_STATUS_RB, &cyberpower_battrepl_status[0] },
	{ "ups.load", 0, 1.0, ".1.3.6.1.4.1.3808.1.1.1.4.2.3.0", "",
		0, NULL },

	{ "ups.temperature", 0, 1, ".1.3.6.1.4.1.3808.1.1.1.10.2.0", "", SU_FLAG_OK, NULL },

	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.10.1.0", "",
		SU_FLAG_OK, &cyberpower_ups_alarm_info[0] },

	/* Battery runtime is expressed in seconds */
	{ "battery.runtime", 0, 1.0, ".1.3.6.1.4.1.3808.1.1.1.2.2.4.0", "",
		0, NULL },
	/* The elapsed time in seconds since the
	 * UPS has switched to battery power */
	{ "battery.runtime.elapsed", 0, 1.0, ".1.3.6.1.4.1.3808.1.1.1.2.1.2.0", "",
		0, NULL },
	/* Different generations/models reported "battery.voltage" by different OIDs: */
	{ "battery.voltage", 0, 0.1, ".1.3.6.1.2.1.33.1.2.5.0", "",
		0, NULL },
	{ "battery.voltage", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.2.2.2.0", "",
		0, NULL },
	{ "battery.voltage.nominal", 0, 1.0, ".1.3.6.1.4.1.3808.1.1.1.2.2.8.0", "",
		0, NULL },
	/* Different generations/models reported "battery.current" by different OIDs: */
	{ "battery.current", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.4.2.4.0", "",
		0, NULL },
	{ "battery.current", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.2.2.7.0", "",
		0, NULL },
	{ "battery.charge", 0, 1.0, ".1.3.6.1.4.1.3808.1.1.1.2.2.1.0", "",
		0, NULL },
	{ "battery.temperature", 0, 1.0, ".1.3.6.1.4.1.3808.1.1.1.2.2.3.0", "",
		0, NULL },
	/* upsBaseBatteryLastReplaceDate */
	{ "battery.date", ST_FLAG_STRING, 8, ".1.3.6.1.4.1.3808.1.1.1.2.1.3.0", "",
		SU_FLAG_OK | SU_FLAG_SEMI_STATIC, NULL },

	{ "input.voltage", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.3.2.1.0", "",
		0, NULL },
	{ "input.frequency", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.3.2.4.0", "",
		0, NULL },
	/* upsAdvanceInputLineFailCause */
	{ "input.transfer.reason", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.3808.1.1.1.3.2.5.0", "",
		SU_TYPE_INT | SU_FLAG_OK, &cyberpower_transfer_reasons[0] },

	{ "output.voltage", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.4.2.1.0", "",
		0, NULL },
	{ "output.frequency", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.4.2.2.0", "",
		0, NULL },
	{ "output.current", 0, 0.1, ".1.3.6.1.4.1.3808.1.1.1.4.2.4.0", "",
		0, NULL },

	/* Delays affecting instant commands */

	/* upsAdvanceConfigReturnDelay */
	{ "ups.delay.start", ST_FLAG_RW, 1.0, ".1.3.6.1.4.1.3808.1.1.1.5.2.9.0", "0",
		SU_FLAG_OK | SU_TYPE_TIME, NULL },
	/* Not provided by CPS-MIB */
	{ "ups.delay.reboot", 0, 1.0, NULL, "0",
		SU_FLAG_OK | SU_FLAG_ABSENT, NULL },
	/* upsAdvanceConfigSleepDelay */
	{ "ups.delay.shutdown", ST_FLAG_RW, 1.0, ".1.3.6.1.4.1.3808.1.1.1.5.2.11.0", "60",
		SU_FLAG_OK | SU_TYPE_TIME, NULL },
	/* instant commands. */
	/* upsAdvanceControlUpsOff */
	{ "load.off", 0, 1, ".1.3.6.1.4.1.3808.1.1.1.6.2.1.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* upsAdvanceControlTurnOnUPS */
	{ "load.on", 0, 1, ".1.3.6.1.4.1.3808.1.1.1.6.2.6.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* upsAdvanceControlUpsOff */
	{ "shutdown.stayoff", 0, 1, ".1.3.6.1.4.1.3808.1.1.1.6.2.6.0", "3", SU_TYPE_CMD | SU_FLAG_OK, NULL },
#if 0
	/* upsBaseControlConserveBattery - note that this command
	 * is not suitable here because it puts ups to sleep only
	 * in battery mode. If power is restored during the shutdown
	 * process, the command is not executed by ups hardware. */
	{ "shutdown.return", 0, 1, ".1.3.6.1.4.1.3808.1.1.1.6.1.1.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
#endif
	/* upsAdvanceControlUpsSleep */
	{ "shutdown.return", 0, 1, ".1.3.6.1.4.1.3808.1.1.1.6.2.3.0", "3", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* upsAdvanceControlSimulatePowerFail */
	{ "test.failure.start", 0, 1, ".1.3.6.1.4.1.3808.1.1.1.6.2.4.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* upsAdvanceTestIndicators */
	{ "test.panel.start", 0, 1, ".1.3.6.1.4.1.3808.1.1.1.7.2.5.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* upsAdvanceTestDiagnostics */
	{ "test.battery.start", 0, 1, ".1.3.6.1.4.1.3808.1.1.1.7.2.2.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* upsAdvanceTestRuntimeCalibration */
	{ "calibrate.start", 0, 1, ".1.3.6.1.4.1.3808.1.1.1.7.2.6.0", "2", SU_TYPE_CMD | SU_FLAG_OK, NULL },
	{ "calibrate.stop", 0, 1, ".1.3.6.1.4.1.3808.1.1.1.7.2.6.0", "3", SU_TYPE_CMD | SU_FLAG_OK, NULL },

	/* upsAdvanceTestLastDiagnosticsDate */
	{ "ups.test.date", ST_FLAG_STRING, 8, ".1.3.6.1.4.1.3808.1.1.1.7.2.4.0", "",
		SU_FLAG_OK | SU_FLAG_SEMI_STATIC, NULL },
	/* upsAdvanceTestDiagnosticsResults */
	{ "ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.3808.1.1.1.7.2.3.0", "",
		SU_FLAG_OK, &cyberpower_testdiag_results[0] },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
} ;

mib2nut_info_t	cyberpower = { "cyberpower", CYBERPOWER_MIB_VERSION, NULL,
	CYBERPOWER_OID_MODEL_NAME, cyberpower_mib, CYBERPOWER_SYSOID, NULL };

mib2nut_info_t	cyberpower2 = { "cyberpower", CYBERPOWER_MIB_VERSION, NULL,
	CYBERPOWER_OID_MODEL_NAME, cyberpower_mib, CYBERPOWER_SYSOID2, NULL };
