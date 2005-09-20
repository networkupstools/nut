/*  belkin-hid.h - data to monitor Belkin UPS Systems USB/HID devices with NUT
 *
 *  Copyright (C)  
 *	2003 - 2005	Arnaud Quette <arnaud.quette@free.fr>
 *      2005            Peter Selinger <selinger@users.sourceforge.net>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://www.mgeups.com>
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

#include "belkin-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */

/* --------------------------------------------------------------- */
/*      Model Name formating entries                               */
/* --------------------------------------------------------------- */

models_name_t belkin_model_names [] =
{
  /* not used for anything. */

  /* end of structure. */
  { NULL, NULL, -1, "Generic Belkin HID model" }
};

/* some conversion functions specific to Belkin */

/* returns statically allocated string - must not use it again before
   done with result! */
static char *belkin_firmware_conversion_fun(long value) {
	static char buf[20];
	sprintf(buf, "%ld", value >> 4);
	
	return buf;
}

static info_lkp_t belkin_firmware_conversion[] = {
	{ 0, NULL, belkin_firmware_conversion_fun }
};

static char *belkin_upstype_conversion_fun(long value) {
	switch (value & 0x0f) {
	case 0: default:
		return "online";
	case 1:
		return "offline";
	case 2:
		return "line-interactive";
	case 3:
		return "simple online";
	case 4: 
		return "simple offline";
	case 5:
		return "simple line-interactive";
	}
}

static info_lkp_t belkin_upstype_conversion[] = {
	{ 0, NULL, belkin_upstype_conversion_fun }
};

static char *belkin_sensitivity_conversion_fun(long value) {
	switch (value) {
	case 0: default:
		return "normal";
	case 1:
		return "reduced";
	case 2:
		return "low";
	}
}

static info_lkp_t belkin_sensitivity_conversion[] = {
	{ 0, NULL, belkin_sensitivity_conversion_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *belkin_10_conversion_fun(long value) {
	static char buf[20];
	
	sprintf(buf, "%0.1f", value * 0.1);
	return buf;
}

static info_lkp_t belkin_10_conversion[] = {
	{ 0, NULL, belkin_10_conversion_fun }
};

static info_lkp_t belkin_test_info[] = {
  { 0, "No test initiated", NULL },
  { 1, "Done and passed", NULL },
  { 2, "Done and warning", NULL },
  { 3, "Done and error", NULL },
  { 4, "Aborted", NULL },
  { 5, "In progress", NULL },
  { 0, "NULL", NULL }
};

static char *belkin_overload_conversion_fun(long value) {
	if (value & 0x0010) {
		return "OVER";
	} else {
		return "";
	}		
}

static info_lkp_t belkin_overload_conversion[] = {
	{ 0, NULL, belkin_overload_conversion_fun }
};

static char *belkin_overheat_conversion_fun(long value) {
	if (value & 0x0040) {
		return "OVERHEAT";
	} else {
		return "";
	}
}

static info_lkp_t belkin_overheat_conversion[] = {
	{ 0, NULL, belkin_overheat_conversion_fun }
};

static char *belkin_commfault_conversion_fun(long value) {
	if (value & 0x0080) {
		return "COMMFAULT";
	} else {
		return "";
	}
}

static info_lkp_t belkin_commfault_conversion[] = {
	{ 0, NULL, belkin_commfault_conversion_fun }
};

static char *belkin_depleted_conversion_fun(long value) {
	if (value & 0x40) {
		return "DEPLETED";
	} else {
		return "";
	}
}

static info_lkp_t belkin_depleted_conversion[] = {
	{ 0, NULL, belkin_depleted_conversion_fun }
};

static char *belkin_replacebatt_conversion_fun(long value) {
	if (value & 0x80) {
		return "RB";
	} else {
		return "";
	}		
}

static info_lkp_t belkin_replacebatt_conversion[] = {
	{ 0, NULL, belkin_replacebatt_conversion_fun }
};



/* HID2NUT lookup table */
hid_info_t hid_belkin[] = {

  /* FIXME: the variable ups.serial.internal should not really exist;
     it should be ups.serial. The problem is that the Belkin UPS does
     not announce its serial number in the device descriptor (like a
     good HID device should), but in a report. So this info is
     unavailable at init-time. */

  /* interpreted Belkin variables */
  { "battery.charge", 0, 0, "UPS.BELKINBatterySystem.BELKINCharge", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.charge.low", ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.charge.warning", ST_FLAG_STRING, 5, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL }, /* Read only */
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_OK, stringid_conversion },
  { "battery.voltage", 0, 0, "UPS.BELKINBatterySystem.BELKINVoltage", NULL, "%s", HU_FLAG_OK, belkin_10_conversion },
  { "battery.voltage.nominal", 0, 0, "UPS.BELKINConfig.BELKINConfigBatteryVoltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.frequency", 0, 0, "UPS.BELKINPowerState.BELKINInput.BELKINFrequency", NULL, "%s", HU_FLAG_OK, belkin_10_conversion },
  { "input.frequency.nominal", 0, 0, "UPS.BELKINConfig.BELKINConfigFrequency", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.sensitivity", ST_FLAG_RW, 0, "UPS.BELKINDevice.BELKINVoltageSensitivity", NULL, "%s", HU_FLAG_OK, belkin_sensitivity_conversion },
  { "input.transfer.high", ST_FLAG_RW, 0, "UPS.BELKINConfig.BELKINHighVoltageTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.transfer.high.max", 0, 0, "UPS.BELKINConfig.BELKINHighVoltageTransferMax", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.transfer.high.min", 0, 0, "UPS.BELKINConfig.BELKINHighVoltageTransferMin", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.transfer.low", ST_FLAG_RW, 0, "UPS.BELKINConfig.BELKINLowVoltageTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.transfer.low.max", 0, 0, "UPS.BELKINConfig.BELKINLowVoltageTransferMax", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.transfer.low.min", 0, 0, "UPS.BELKINConfig.BELKINLowVoltageTransferMin", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.voltage", 0, 0, "UPS.BELKINPowerState.BELKINInput.BELKINVoltage", NULL, "%s", HU_FLAG_OK, belkin_10_conversion },
  { "input.voltage.nominal", 0, 0, "UPS.BELKINConfig.BELKINConfigVoltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "output.frequency", 0, 0, "UPS.BELKINPowerState.BELKINOutput.BELKINFrequency", NULL, "%s", HU_FLAG_OK, belkin_10_conversion },
  { "output.voltage", 0, 0, "UPS.BELKINPowerState.BELKINOutput.BELKINVoltage", NULL, "%s", HU_FLAG_OK, belkin_10_conversion },
  { "ups.beeper.status", 0, 0, "UPS.BELKINControls.BELKINAudibleAlarmControl", NULL, "%s", HU_FLAG_OK, beeper_info },
  { "ups.delay.restart", ST_FLAG_RW, 0, "UPS.BELKINControls.BELKINDelayBeforeStartup", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "ups.delay.shutdown", ST_FLAG_RW, 0, "UPS.BELKINControls.BELKINDelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "ups.firmware", 0, 0, "UPS.BELKINDevice.BELKINUPSType", NULL, "%s", HU_FLAG_OK, belkin_firmware_conversion },
  { "ups.load", 0, 0, "UPS.BELKINPowerState.BELKINOutput.BELKINPercentLoad", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "ups.load.high", 0, 0, "UPS.BELKINConfig.BELKINConfigOverloadTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "ups.mfr.date", 0, 0, "UPS.PowerSummary.ManufacturerDate", NULL, "%s", HU_FLAG_OK, date_conversion },
  { "ups.power.nominal", 0, 0, "UPS.BELKINConfig.BELKINConfigApparentPower", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "ups.serial", 0, 0, "UPS.PowerSummary.iSerialNumber", NULL, "%s", HU_FLAG_OK, stringid_conversion },
  { "ups.test.result", ST_FLAG_RW, 0, "UPS.BELKINControls.BELKINTest", NULL, "%s", HU_FLAG_OK, belkin_test_info },
  { "ups.type", 0, 0, "UPS.BELKINDevice.BELKINUPSType", NULL, "%s", HU_FLAG_OK, belkin_upstype_conversion },

  /* status */
  { "ups.status", 0, 1, "UPS.PowerSummary.Discharging",NULL, "%s", HU_FLAG_OK, discharging_info },
  { "ups.status", 0, 1, "UPS.PowerSummary.Charging", NULL, "%s", HU_FLAG_OK, charging_info },
  { "ups.status", 0, 1, "UPS.PowerSummary.ShutdownImminent", NULL, "%s", HU_FLAG_OK, shutdownimm_info },
  { "ups.status", 0, 1, "UPS.PowerSummary.ACPresent", NULL, "%s", HU_FLAG_OK, onbatt_info },
  { "ups.status", 0, 1, "UPS.PowerSummary.BelowRemainingCapacityLimit", NULL, "%s", HU_FLAG_OK, lowbatt_info },
  { "ups.status", 0, 1, "UPS.BELKINStatus.BELKINPowerStatus", NULL, "%s", HU_FLAG_OK, belkin_overload_conversion },
  { "ups.status", 0, 1, "UPS.BELKINStatus.BELKINPowerStatus", NULL, "%s", HU_FLAG_OK, belkin_overheat_conversion },
  { "ups.status", 0, 1, "UPS.BELKINStatus.BELKINPowerStatus", NULL, "%s", HU_FLAG_OK, belkin_commfault_conversion },
  { "ups.status", 0, 1, "UPS.BELKINStatus.BELKINBatteryStatus", NULL, "%s", HU_FLAG_OK, belkin_depleted_conversion },
  { "ups.status", 0, 1, "UPS.BELKINStatus.BELKINBatteryStatus", NULL, "%s", HU_FLAG_OK, belkin_replacebatt_conversion },

  /* Server side variables */
  { "driver.version.internal", ST_FLAG_STRING, sizeof(DRIVER_VERSION), NULL, NULL, DRIVER_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  { "driver.version.data", ST_FLAG_STRING, sizeof(BELKIN_HID_VERSION), NULL, NULL, BELKIN_HID_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  
  /* instant commands. */
  /* split into subsets while waiting for extradata support
   * ie: test.battery.start quick
   */
  { "test.battery.start.quick", 0, 0, "UPS.BELKINControls.BELKINTest", NULL, "1", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "test.battery.start.deep", 0, 0, "UPS.BELKINControls.BELKINTest", NULL, "2", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "test.battery.stop", 0, 0, "UPS.BELKINControls.BELKINTest", NULL, "3", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "beeper.on", 0, 0, "UPS.BELKINControls.BELKINAudibleAlarmControl", NULL, "2", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "beeper.off", 0, 0, "UPS.BELKINControls.BELKINAudibleAlarmControl", NULL, "3", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "load.off", 0, 0, "UPS.BELKINControls.BELKINDelayBeforeShutdown", NULL, "1", HU_TYPE_CMD | HU_FLAG_OK, NULL },

  /* Note: on my Belkin UPS, there is no way to implement
     shutdown.return or load.on, even though they should exist in
     principle. Hopefully other Belkin models will be better
     designed. Fixme: fill in the appropriate instant commands.  For a
     more detailed description of the problem and a possible (but not
     yet implemented) workaround, see the belkinunv(8) man page. 
     -PS 2005/08/28 */

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};
