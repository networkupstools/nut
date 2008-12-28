/*  belkin-hid.h - data to monitor Belkin UPS Systems USB/HID devices with NUT
 *
 *  Copyright (C)  
 *	2003 - 2008	Arnaud Quette <arnaud.quette@free.fr>
 *  2005        Peter Selinger <selinger@users.sourceforge.net>
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

#include "usbhid-ups.h"
#include "belkin-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "main.h"     /* for getval() */
#include "common.h"
#include "usb-common.h"

#define BELKIN_HID_VERSION      "Belkin HID 0.12"

/* Belkin */
#define BELKIN_VENDORID 0x050d

/* USB IDs device table */
static usb_device_id_t belkin_usb_device_table[] = {
	/* F6C800-UNV */
	{ USB_DEVICE(BELKIN_VENDORID, 0x0980), NULL },
	/* F6C900-UNV */
	{ USB_DEVICE(BELKIN_VENDORID, 0x0900), NULL },
	/* F6C100-UNV */
	{ USB_DEVICE(BELKIN_VENDORID, 0x0910), NULL },
	/* F6C120-UNV */
	{ USB_DEVICE(BELKIN_VENDORID, 0x0912), NULL },
	/* F6C550-AVR */
	{ USB_DEVICE(BELKIN_VENDORID, 0x0551), NULL },
	/* F6C1500-TW-RK */
	{ USB_DEVICE(BELKIN_VENDORID, 0x0751), NULL },
	/* F6H375-USB */
	{ USB_DEVICE(BELKIN_VENDORID, 0x0375), NULL },
	/* F6C1100-UNV, F6C1200-UNV */
	{ USB_DEVICE(BELKIN_VENDORID, 0x1100), NULL },
	
	/* Terminating entry */
	{ -1, -1, NULL }
};

/* some conversion functions specific to Belkin */

/* returns statically allocated string - must not use it again before
   done with result! */
static char *belkin_firmware_conversion_fun(double value)
{
	static char buf[20];

	snprintf(buf, sizeof(buf), "%ld", (long)value >> 4);
	
	return buf;
}

static info_lkp_t belkin_firmware_conversion[] = {
	{ 0, NULL, belkin_firmware_conversion_fun }
};

static char *belkin_upstype_conversion_fun(double value)
{
	switch ((long)value & 0x0f)
	{
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
	default:
		return "online";
	}
}

static info_lkp_t belkin_upstype_conversion[] = {
	{ 0, NULL, belkin_upstype_conversion_fun }
};

static char *belkin_sensitivity_conversion_fun(double value)
{
	switch ((long)value)
	{
	case 1:
		return "reduced";
	case 2:
		return "low";
	default:
		return "normal";
	}
}

static info_lkp_t belkin_sensitivity_conversion[] = {
	{ 0, NULL, belkin_sensitivity_conversion_fun }
};

static info_lkp_t belkin_test_info[] = {
	{ 0, "No test initiated", NULL },
	{ 1, "Done and passed", NULL },
	{ 2, "Done and warning", NULL },
	{ 3, "Done and error", NULL },
	{ 4, "Aborted", NULL },
	{ 5, "In progress", NULL },
	{ 0, NULL, NULL }
};

static char *belkin_overload_conversion_fun(double value)
{
	if ((long)value & 0x0010) {
		return "overload";
	} else {
		return "!overload";
	}		
}

static info_lkp_t belkin_overload_conversion[] = {
	{ 0, NULL, belkin_overload_conversion_fun }
};

static char *belkin_overheat_conversion_fun(double value)
{
	if ((long)value & 0x0040) {
		return "overheat";
	} else {
		return "!overheat";
	}
}

static info_lkp_t belkin_overheat_conversion[] = {
	{ 0, NULL, belkin_overheat_conversion_fun }
};

static char *belkin_commfault_conversion_fun(double value)
{
	if ((long)value & 0x0080) {
		return "commfault";
	} else {
		return "!commfault";
	}
}

static info_lkp_t belkin_commfault_conversion[] = {
	{ 0, NULL, belkin_commfault_conversion_fun }
};

static char *belkin_awaitingpower_conversion_fun(double value)
{
	if ((long)value & 0x2000) {
		return "awaitingpower";
	} else {
		return "!awaitingpower";
	}
}

static info_lkp_t belkin_awaitingpower_conversion[] = {
	{ 0, NULL, belkin_awaitingpower_conversion_fun }
};

static char *belkin_online_conversion_fun(double value)
{
	if ((long)value & 0x0001) {
		return "!online";
	} else {
		return "online";
	}		
}

static info_lkp_t belkin_online_conversion[] = {
	{ 0, NULL, belkin_online_conversion_fun }
};

static char *belkin_lowbatt_conversion_fun(double value)
{
	if ((long)value & 0x0004) {
		return "lowbatt";
	} else {
		return "!lowbatt";
	}
}

static info_lkp_t belkin_lowbatt_conversion[] = {
	{ 0, NULL, belkin_lowbatt_conversion_fun }
};

static char *belkin_depleted_conversion_fun(double value)
{
	if ((long)value & 0x0040) {
		return "depleted";
	} else {
		return "!depleted";
	}
}

static info_lkp_t belkin_depleted_conversion[] = {
	{ 0, NULL, belkin_depleted_conversion_fun }
};

static char *belkin_replacebatt_conversion_fun(double value)
{
	if ((long)value & 0x0080) {
		return "replacebatt";
	} else {
		return "!replacebatt";
	}		
}

static info_lkp_t belkin_replacebatt_conversion[] = {
	{ 0, NULL, belkin_replacebatt_conversion_fun }
};

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* BELKIN usage table */
/* Note: these seem to have been wrongly encoded by Belkin */
/* Pages 84 to 88 are reserved for official HID definition! */
static usage_lkp_t belkin_usage_lkp[] = {
	{ "BELKINConfig",			0x00860026 },
	{ "BELKINConfigVoltage",		0x00860040 }, /* (V) */
	{ "BELKINConfigFrequency",		0x00860042 }, /* (Hz) */
	{ "BELKINConfigApparentPower",		0x00860043 }, /* (VA) */
	{ "BELKINConfigBatteryVoltage",		0x00860044 }, /* (V) */
	{ "BELKINConfigOverloadTransfer",	0x00860045 }, /* (%) */
	{ "BELKINLowVoltageTransfer",		0x00860053 }, /* R/W (V) */
	{ "BELKINHighVoltageTransfer",		0x00860054 }, /* R/W (V)*/
	{ "BELKINLowVoltageTransferMax",	0x0086005b }, /* (V) */
	{ "BELKINLowVoltageTransferMin",	0x0086005c }, /* (V) */
	{ "BELKINHighVoltageTransferMax",	0x0086005d }, /* (V) */
	{ "BELKINHighVoltageTransferMin",	0x0086005e }, /* (V) */

	{ "BELKINControls",			0x00860027 },
	{ "BELKINLoadOn",			0x00860050 }, /* R/W: write: 1=do action. Read: 0=none, 1=started, 2=in progress, 3=complete */
	{ "BELKINLoadOff",			0x00860051 }, /* R/W: ditto */
	{ "BELKINLoadToggle",			0x00860052 }, /* R/W: ditto */
	{ "BELKINDelayBeforeReboot",		0x00860055 }, /* R/W: write: 0=start shutdown using default delay. */
	{ "BELKINDelayBeforeStartup",		0x00860056 }, /* R/W (minutes) */
	{ "BELKINDelayBeforeShutdown",		0x00860057 }, /* R/W (seconds) */
	{ "BELKINTest",				0x00860058 }, /* R/W: write: 0=no test, 1=quick test, 2=deep test, 3=abort test. Read: 0=no test, 1=passed, 2=warning, 3=error, 4=abort, 5=in progress */
	{ "BELKINAudibleAlarmControl",		0x0086005a }, /* R/W: 1=disabled, 2=enabled, 3=muted */
	
	{ "BELKINDevice",			0x00860029 },
	{ "BELKINVoltageSensitivity",		0x00860074 }, /* R/W: 0=normal, 1=reduced, 2=low */
	{ "BELKINModelString",			0x00860075 },
	{ "BELKINModelStringOffset",		0x00860076 }, /* offset of Model name in Model String */
	{ "BELKINUPSType",			0x0086007c }, /* high nibble: firmware version. Low nibble: 0=online, 1=offline, 2=line-interactive, 3=simple online, 4=simple offline, 5=simple line-interactive */

	{ "BELKINPowerState",			0x0086002a },
	{ "BELKINInput",			0x0086001a },
	{ "BELKINOutput",			0x0086001c },
	{ "BELKINBatterySystem",		0x00860010 },
	{ "BELKINVoltage",			0x00860030 }, /* (0.1 Volt) */
	{ "BELKINFrequency",			0x00860032 }, /* (0.1 Hz) */
	{ "BELKINPower",			0x00860034 }, /* (Watt) */
	{ "BELKINPercentLoad",			0x00860035 }, /* (%) */
	{ "BELKINTemperature",			0x00860036 }, /* (Celsius) */
	{ "BELKINCharge",			0x00860039 }, /* (%) */
	{ "BELKINRunTimeToEmpty",		0x0086006c }, /* (minutes) */

	{ "BELKINStatus",			0x00860028 },
	{ "BELKINBatteryStatus",		0x00860022 }, /* 1 byte: bit2=low battery, bit4=charging, bit5=discharging, bit6=battery empty, bit7=replace battery */
	{ "BELKINPowerStatus",			0x00860021 }, /* 2 bytes: bit0=ac failure, bit4=overload, bit5=load is off, bit6=overheat, bit7=UPS fault, bit13=awaiting power, bit15=alarm status */
	{ NULL, 0 }
};

static usage_tables_t belkin_utab[] = {
	belkin_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t belkin_hid2nut[] = {

  /* interpreted Belkin variables */
  { "battery.charge", 0, 0, "UPS.BELKINBatterySystem.BELKINCharge", NULL, "%.0f", 0, NULL },
  /* { "battery.charge.broken", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL }, */
  { "battery.charge.low", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", 0, NULL },
  { "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", 0, NULL }, /* Read only */
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
  { "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", 0, stringid_conversion },
  { "battery.voltage", 0, 0, "UPS.BELKINBatterySystem.BELKINVoltage", NULL, "%s", 0, divide_by_10_conversion },
  { "battery.voltage.nominal", 0, 0, "UPS.BELKINConfig.BELKINConfigBatteryVoltage", NULL, "%.0f", 0, NULL },
  { "input.frequency", 0, 0, "UPS.BELKINPowerState.BELKINInput.BELKINFrequency", NULL, "%s", 0, divide_by_10_conversion },
  { "input.frequency.nominal", 0, 0, "UPS.BELKINConfig.BELKINConfigFrequency", NULL, "%.0f", 0, NULL },
  { "input.sensitivity", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.BELKINDevice.BELKINVoltageSensitivity", NULL, "%s", 0, belkin_sensitivity_conversion },
  { "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.BELKINConfig.BELKINHighVoltageTransfer", NULL, "%.0f", 0, NULL },
  { "input.transfer.high.max", 0, 0, "UPS.BELKINConfig.BELKINHighVoltageTransferMax", NULL, "%.0f", 0, NULL },
  { "input.transfer.high.min", 0, 0, "UPS.BELKINConfig.BELKINHighVoltageTransferMin", NULL, "%.0f", 0, NULL },
  { "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.BELKINConfig.BELKINLowVoltageTransfer", NULL, "%.0f", 0, NULL },
  { "input.transfer.low.max", 0, 0, "UPS.BELKINConfig.BELKINLowVoltageTransferMax", NULL, "%.0f", 0, NULL },
  { "input.transfer.low.min", 0, 0, "UPS.BELKINConfig.BELKINLowVoltageTransferMin", NULL, "%.0f", 0, NULL },
  { "input.voltage", 0, 0, "UPS.BELKINPowerState.BELKINInput.BELKINVoltage", NULL, "%s", 0, divide_by_10_conversion },
  { "input.voltage.nominal", 0, 0, "UPS.BELKINConfig.BELKINConfigVoltage", NULL, "%.0f", 0, NULL },
  { "output.frequency", 0, 0, "UPS.BELKINPowerState.BELKINOutput.BELKINFrequency", NULL, "%s", 0, divide_by_10_conversion },
  { "output.voltage", 0, 0, "UPS.BELKINPowerState.BELKINOutput.BELKINVoltage", NULL, "%s", 0, divide_by_10_conversion },
  { "ups.beeper.status", 0, 0, "UPS.BELKINControls.BELKINAudibleAlarmControl", NULL, "%s", 0, beeper_info },
  { "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.BELKINControls.BELKINDelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_FLAG_ABSENT, NULL },
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.BELKINControls.BELKINDelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL },
  { "ups.timer.start", 0, 0, "UPS.BELKINControls.BELKINDelayBeforeStartup", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },
  { "ups.timer.shutdown", 0, 0, "UPS.BELKINControls.BELKINDelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },
  { "ups.timer.reboot", 0, 0, "UPS.BELKINControls.BELKINDelayBeforeReboot", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },
  { "ups.firmware", 0, 0, "UPS.BELKINDevice.BELKINUPSType", NULL, "%s", 0, belkin_firmware_conversion },
  { "ups.load", 0, 0, "UPS.BELKINPowerState.BELKINOutput.BELKINPercentLoad", NULL, "%.0f", 0, NULL },
  { "ups.load.high", 0, 0, "UPS.BELKINConfig.BELKINConfigOverloadTransfer", NULL, "%.0f", 0, NULL },
  { "ups.mfr.date", 0, 0, "UPS.PowerSummary.ManufacturerDate", NULL, "%s", 0, date_conversion },
  { "ups.power.nominal", 0, 0, "UPS.BELKINConfig.BELKINConfigApparentPower", NULL, "%.0f", 0, NULL },
  { "ups.serial", 0, 0, "UPS.PowerSummary.iSerialNumber", NULL, "%s", 0, stringid_conversion },
  { "ups.test.result", 0, 0, "UPS.BELKINControls.BELKINTest", NULL, "%s", 0, belkin_test_info },
  { "ups.type", 0, 0, "UPS.BELKINDevice.BELKINUPSType", NULL, "%s", 0, belkin_upstype_conversion },

  /* status */
  { "BOOL", 0, 0, "UPS.PowerSummary.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
  /* { "BOOL", 0, 0, "UPS.PowerSummary.BelowRemainingCapacityLimit", NULL, "%s", 0, lowbatt_info }, broken! */
  { "BOOL", 0, 0, "UPS.BELKINStatus.BELKINPowerStatus", NULL, NULL, 0, belkin_overload_conversion },
  { "BOOL", 0, 0, "UPS.BELKINStatus.BELKINPowerStatus", NULL, NULL, 0, belkin_overheat_conversion },
  { "BOOL", 0, 0, "UPS.BELKINStatus.BELKINPowerStatus", NULL, NULL, 0, belkin_commfault_conversion },
  { "BOOL", 0, 0, "UPS.BELKINStatus.BELKINPowerStatus", NULL, NULL, 0, belkin_awaitingpower_conversion },
  { "BOOL", 0, 0, "UPS.BELKINStatus.BELKINPowerStatus", NULL, NULL, HU_FLAG_QUICK_POLL, belkin_online_conversion },
  { "BOOL", 0, 0, "UPS.BELKINStatus.BELKINBatteryStatus", NULL, NULL, HU_FLAG_QUICK_POLL, belkin_depleted_conversion },
  { "BOOL", 0, 0, "UPS.BELKINStatus.BELKINBatteryStatus", NULL, NULL, 0, belkin_replacebatt_conversion },
  { "BOOL", 0, 0, "UPS.BELKINStatus.BELKINBatteryStatus", NULL, NULL, HU_FLAG_QUICK_POLL, belkin_lowbatt_conversion },

  /* instant commands. */
  /* split into subsets while waiting for extradata support
   * ie: test.battery.start quick
   */
  { "test.battery.start.quick", 0, 0, "UPS.BELKINControls.BELKINTest", NULL, "1", HU_TYPE_CMD, NULL },
  { "test.battery.start.deep", 0, 0, "UPS.BELKINControls.BELKINTest", NULL, "2", HU_TYPE_CMD, NULL },
  { "test.battery.stop", 0, 0, "UPS.BELKINControls.BELKINTest", NULL, "3", HU_TYPE_CMD, NULL },
  { "beeper.on", 0, 0, "UPS.BELKINControls.BELKINAudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
  { "beeper.off", 0, 0, "UPS.BELKINControls.BELKINAudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },
  { "beeper.disable", 0, 0, "UPS.BELKINControls.BELKINAudibleAlarmControl", NULL, "1", HU_TYPE_CMD, NULL },
  { "beeper.enable", 0, 0, "UPS.BELKINControls.BELKINAudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
  { "beeper.mute", 0, 0, "UPS.BELKINControls.BELKINAudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },
  { "load.off", 0, 0, "UPS.BELKINControls.BELKINDelayBeforeShutdown", NULL, "1", HU_TYPE_CMD, NULL },
  { "load.on", 0, 0, "UPS.BELKINControls.BELKINDelayBeforeStartup", NULL, "1", HU_TYPE_CMD, NULL },
  { "load.off.delay", 0, 0, "UPS.BELKINControls.BELKINDelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
  { "load.on.delay", 0, 0, "UPS.BELKINControls.BELKINDelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },
  { "shutdown.stop", 0, 0, "UPS.BELKINControls.BELKINDelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD, NULL },
  { "shutdown.reboot", 0, 0, "UPS.BELKINControls.BELKINDelayBeforeReboot", NULL, "10", HU_TYPE_CMD, NULL },

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

static char *belkin_format_model(HIDDevice_t *hd) {
	if ((hd->Product) && (strlen(hd->Product) > 0)) {
		return hd->Product;
	}

	return "unknown";
}

static char *belkin_format_mfr(HIDDevice_t *hd) {
	char *mfr;
	mfr = hd->Vendor ? hd->Vendor : "Belkin";
	/* trim leading whitespace */
	while (*mfr == ' ') {
		mfr++;
	}
	if (strlen(mfr) == 0) {
		mfr = "Belkin";
	}
	return mfr;
}

static char *belkin_format_serial(HIDDevice_t *hd) {
	char serial[64];

	if (hd->Serial) {
		return hd->Serial;
	}

	/* try UPS.PowerSummary.iSerialNumber */
	HIDGetItemString(udev, "UPS.PowerSummary.iSerialNumber",
		serial, sizeof(serial), belkin_utab);

	if (strlen(serial) < 1) {
		return NULL;
	}

	/* free(hd->Serial); not needed, we already know it is NULL */
	hd->Serial = strdup(serial);
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int belkin_claim(HIDDevice_t *hd) {

	int status = is_usb_device_supported(belkin_usb_device_table, hd->VendorID,
								 hd->ProductID);

	switch (status) {

		case POSSIBLY_SUPPORTED:
			/* reject any known non-UPS */
			if (hd->ProductID == 0x0218)  /* F5U218-MOB 4-Port USB Hub */
				return 0;

			/* by default, reject, unless the productid option is given */
			if (getval("productid")) {
				return 1;
			}
			possibly_supported("Belkin", hd);
			return 0;

		case SUPPORTED:
			return 1;

		case NOT_SUPPORTED:
		default:
			return 0;
	}
}

subdriver_t belkin_subdriver = {
	BELKIN_HID_VERSION,
	belkin_claim,
	belkin_utab,
	belkin_hid2nut,
	belkin_format_model,
	belkin_format_mfr,
	belkin_format_serial,
};
