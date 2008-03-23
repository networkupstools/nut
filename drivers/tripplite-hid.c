/*  tripplite-hid.c - data to monitor Tripp Lite USB/HID devices with NUT
 *
 *  Copyright (C)  
 *	2003 - 2005 Arnaud Quette <arnaud.quette@free.fr>
 *	2005 - 2006 Peter Selinger <selinger@users.sourceforge.net>
 *	2008        Arjen de Korte <adkorte-guest@alioth.debian.org>
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
#include "tripplite-hid.h"
#include "main.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "common.h"

#define TRIPPLITE_HID_VERSION "TrippLite HID 0.2 (experimental)"

#define TRIPPLITE_VENDORID 0x09ae 

/* For some devices, the reported battery voltage is off by
 * factor of 10 (due to an error in the report descriptor),
 * so we need to apply a scale factor to it to get the actual
 * battery voltage. By default, the factor is 1 (no scaling).
 */
static double	battery_scale = 1.0;

/* returns statically allocated string - must not use it again before
   done with result! */
static char *tripplite_chemistry_fun(double value)
{
	static char	buf[20];
	const char	*model;

	model = dstate_getinfo("ups.productid");

	/* Workaround for AVR 550U firmware bug */
	if (!strcmp(model, "1003")) {
		return "unknown";
	}

	/* Workaround for OMNI1000LCD firmware bug */
	if (!strcmp(model, "2005")) {
		return "unknown";
	}

	return HIDGetIndexString(udev, (int)value, buf, sizeof(buf));
}

static info_lkp_t tripplite_chemistry[] = {
	{ 0, NULL, tripplite_chemistry_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *tripplite_battvolt_fun(double value)
{
	static char	buf[8];

	snprintf(buf, sizeof(buf), "%.1f", battery_scale * value);

	return buf;
}

static info_lkp_t tripplite_battvolt[] = {
	{ 0, NULL, tripplite_battvolt_fun }
};

/* --------------------------------------------------------------- */
/*	Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* TRIPPLITE usage table */
static usage_lkp_t tripplite_usage_lkp[] = {
	/* currently unknown: 
	   ffff0010, 00ff0001, ffff007d, ffff00c0, ffff00c1, ffff00c2,
	   ffff00c3, ffff00c4, ffff00c5, ffff00d2, ffff0091, ffff00c7 */

	{ "TLLowVoltageTransferMax",	0xffff0057 },
	{ "TLLowVoltageTransferMin",	0xffff0058 },
	{ "TLHighVoltageTransferMax",	0xffff0059 },
	{ "TLHighVoltageTransferMin",	0xffff005a },
	{ "TLWatchdog",			0xffff0092 },

	/* it looks like Tripp Lite confused pages 0x84 and 0x85 for the
	   following 4 items, on some OMNI1000LCD devices. */
	{ "TLCharging",			0x00840044 },  /* conflicts with HID spec! */
	{ "TLDischarging",		0x00840045 },  /* conflicts with HID spec! */
	{ "TLNeedReplacement",		0x0084004b },
	{ "TLACPresent",		0x008400d0 },
	{ NULL, 0 }
};

static usage_tables_t tripplite_utab[] = {
	tripplite_usage_lkp, /* tripplite lookup table; make sure this is first */
	hid_usage_lkp,       /* generic lookup table */
	NULL,
};

/* --------------------------------------------------------------- */
/*	HID2NUT lookup table					   */
/* --------------------------------------------------------------- */

/* HID2NUT lookup table */
static hid_info_t tripplite_hid2nut[] = {

#ifdef USBHID_UPS_TRIPPLITE_DEBUG

	/* unmapped variables - meaning unknown */
	{ "UPS.ffff0010.[1].ffff007d", 0, 0, "UPS.ffff0010.[1].ffff007d", NULL, "%.0f", 0, NULL },
	{ "UPS.ffff0015.[1].ffff00c0", 0, 0, "UPS.ffff0015.[1].ffff00c0", NULL, "%.0f", 0, NULL },
	{ "UPS.ffff0015.[1].ffff00c1", 0, 0, "UPS.ffff0015.[1].ffff00c1", NULL, "%.0f", 0, NULL },
	{ "UPS.ffff0015.[1].ffff00c2", 0, 0, "UPS.ffff0015.[1].ffff00c2", NULL, "%.0f", 0, NULL },
	{ "UPS.ffff0015.[1].ffff00c3", 0, 0, "UPS.ffff0015.[1].ffff00c3", NULL, "%.0f", 0, NULL },
	{ "UPS.ffff0015.[1].ffff00c4", 0, 0, "UPS.ffff0015.[1].ffff00c4", NULL, "%.0f", 0, NULL },
	{ "UPS.ffff0015.[1].ffff00c5", 0, 0, "UPS.ffff0015.[1].ffff00c5", NULL, "%.0f", 0, NULL },
	{ "UPS.ffff0015.[1].ffff00d2", 0, 0, "UPS.ffff0015.[1].ffff00d2", NULL, "%.0f", 0, NULL },
	{ "UPS.OutletSystem.Outlet.ffff0091", 0, 0, "UPS.OutletSystem.Outlet.ffff0091", NULL, "%.0f", 0, NULL },
	{ "UPS.OutletSystem.Outlet.ffff00c7", 0, 0, "UPS.OutletSystem.Outlet.ffff00c7", NULL, "%.0f", 0, NULL },

#endif /* USBHID_UPS_TRIPPLITE_DEBUG */

	/* Server side variables */
	{ "driver.version.internal", ST_FLAG_STRING, sizeof(DRIVER_VERSION), NULL, NULL, DRIVER_VERSION, HU_FLAG_ABSENT, NULL },
	{ "driver.version.data", ST_FLAG_STRING, sizeof(TRIPPLITE_HID_VERSION), NULL, NULL, TRIPPLITE_HID_VERSION, HU_FLAG_ABSENT, NULL },
	
	/* Battery page */
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.charge", 0, 0, "UPS.BatterySystem.Battery.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", 0, NULL },
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
	{ "battery.voltage.nominal", 0, 0, "UPS.BatterySystem.Battery.ConfigVoltage", NULL, "%.1f", HU_FLAG_STATIC, NULL },
	{ "battery.voltage",  0, 0, "UPS.BatterySystem.Battery.Voltage", NULL, "%s", 0, tripplite_battvolt },
	{ "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_STATIC, tripplite_chemistry },
	{ "battery.temperature", 0, 0, "UPS.BatterySystem.Temperature", NULL, "%s", 0, kelvin_celsius_conversion },

	/* UPS page */
	{ "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.OutletSystem.Outlet.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_FLAG_ABSENT, NULL},
	{ "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL},
	{ "ups.timer.start", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeStartup", NULL, "%.0f", 0, NULL},
	{ "ups.timer.shutdown", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, "%.0f", 0, NULL},
	{ "ups.timer.reboot", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeReboot", NULL, "%.0f", 0, NULL },
	{ "ups.test.result", 0, 0, "UPS.BatterySystem.Test", NULL, "%s", 0, test_read_info },
	{ "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", 0, beeper_info },
	{ "ups.power.nominal", 0, 0, "UPS.Flow.ConfigApparentPower", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "ups.power", 0, 0, "UPS.OutletSystem.Outlet.ActivePower", NULL, "%.1f", 0, NULL },
	{ "ups.power", 0, 0, "UPS.PowerConverter.Output.ActivePower", NULL, "%.1f", 0, NULL },
	{ "ups.load", 0, 0, "UPS.OutletSystem.Outlet.PercentLoad", NULL, "%.0f", 0, NULL },

	/* Number of seconds left before the watchdog reboots the UPS (0 = disabled) */
	{ "ups.watchdog.status", 0, 0, "UPS.OutletSystem.Outlet.TLWatchdog", NULL, "%.0f", 0, NULL },

	/* Special case: ups.status */
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.InternalFailure", NULL, NULL, 0, commfault_info }, 
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyCharged", NULL, NULL, HU_FLAG_QUICK_POLL, fullycharged_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyDischarged", NULL, NULL, HU_FLAG_QUICK_POLL, depleted_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info },
	/* repeat some of the above for faulty usage codes (seen on OMNI1000LCD, untested) */
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.TLACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.TLDischarging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.TLCharging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.TLNeedReplacement", NULL, NULL, 0, replacebatt_info },

	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.VoltageOutOfRange", NULL, NULL, 0, vrange_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Buck", NULL, NULL, 0, trim_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Boost", NULL, NULL, 0, boost_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Overload", NULL, NULL, 0, overload_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Used", NULL, NULL, 0, nobattery_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.OverTemperature", NULL, NULL, 0, overheat_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.InternalFailure", NULL, NULL, 0, commfault_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.AwaitingPower", NULL, NULL, 0, awaitingpower_info },

	/* Duplicated values
	{ "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.FullyCharged", NULL, NULL, HU_FLAG_QUICK_POLL, fullycharged_info },
	{ "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
	{ "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
	{ "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.FullyDischarged", NULL, NULL, HU_FLAG_QUICK_POLL, depleted_info },
	{ "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info },
	 */

	/* Input page */
	{ "input.voltage.nominal", 0, 0, "UPS.PowerSummary.Input.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.voltage", 0, 0, "UPS.PowerSummary.Input.Voltage", NULL, "%.1f", 0, NULL },
	{ "input.voltage", 0, 0, "UPS.PowerConverter.Input.Voltage", NULL, "%.1f", 0, NULL },
	{ "input.frequency", 0, 0, "UPS.PowerConverter.Input.Frequency", NULL, "%.1f", 0, NULL },
	{ "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.LowVoltageTransfer", NULL, "%.1f", HU_FLAG_SEMI_STATIC, NULL },
	{ "input.transfer.low.max", 0, 0, "UPS.PowerConverter.Output.TLLowVoltageTransferMax", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.transfer.low.min", 0, 0, "UPS.PowerConverter.Output.TLLowVoltageTransferMin", NULL, "%.0f", HU_FLAG_STATIC, NULL }, 
	{ "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.HighVoltageTransfer", NULL, "%.1f", HU_FLAG_SEMI_STATIC, NULL },
	{ "input.transfer.high.max", 0, 0, "UPS.PowerConverter.Output.TLHighVoltageTransferMax", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.transfer.high.min", 0, 0, "UPS.PowerConverter.Output.TLHighVoltageTransferMin", NULL, "%.0f", HU_FLAG_STATIC, NULL },

	/* Output page */
	{ "output.voltage.nominal", 0, 0, "UPS.Flow.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", NULL, "%.1f", 0, NULL },
	{ "output.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", 0, NULL },
	{ "output.current", 0, 0, "UPS.PowerConverter.Output.Current", NULL, "%.2f", 0, NULL },
	{ "output.frequency.nominal", 0, 0, "UPS.Flow.ConfigFrequency", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", NULL, "%.1f", 0, NULL },

	/* instant commands. */
	{ "test.battery.start.quick", 0, 0, "UPS.BatterySystem.Test", NULL, "1", HU_TYPE_CMD, NULL }, /* reported to work on OMNI1000 */
	{ "test.battery.start.deep", 0, 0, "UPS.BatterySystem.Test", NULL, "2", HU_TYPE_CMD, NULL }, /* reported not to work */
	{ "test.battery.stop", 0, 0, "UPS.BatterySystem.Test", NULL, "3", HU_TYPE_CMD, NULL }, /* reported not to work */
	
	{ "load.off.delay", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
	{ "load.on.delay", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },

	{ "shutdown.stop", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD, NULL },
	{ "shutdown.reboot", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeReboot", NULL, "10", HU_TYPE_CMD, NULL },

	/* WARNING: if this timer expires, the UPS will reboot! Defaults to 60 seconds */
	{ "reset.watchdog", 0, 0, "UPS.OutletSystem.Outlet.TLWatchdog", NULL, "60", HU_TYPE_CMD, NULL },

	{ "beeper.on", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
	{ "beeper.off", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },
	{ "beeper.disable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "1", HU_TYPE_CMD, NULL },
	{ "beeper.enable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
	{ "beeper.mute", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },
	
	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static char *tripplite_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static char *tripplite_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor;
}

static char *tripplite_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int tripplite_claim(HIDDevice_t *hd) {
	if (hd->VendorID != TRIPPLITE_VENDORID) {
		return 0;
	}

	/* accept any known UPS - add devices here as needed.
	   Remember: also update scripts/udev/nutusb-ups.rules.in
	   and scripts/hotplug/libhid.usermap */
	switch (hd->ProductID)
	{
	case 0x1003:  /* e.g. AVR550U */
	case 0x2005:  /* e.g. OMNI1000LCD */
	case 0x2007:  /* e.g. OMNI900LCD */
		battery_scale = 0.1;
		return 1;

	case 0x3012:  /* e.g. smart2200RMXL2U */
	case 0x4002:  /* e.g. SmartOnline SU6000RT4U? */
	case 0x4003:  /* e.g. SmartOnline SU1500RTXL2ua */
		battery_scale = 1.0;
		return 1;

	/* reject known non-HID devices */
	/* not all Tripp Lite products are HID, some are "serial over USB". */
	case 0x0001:  /* e.g. SMART550USB, SMART3000RM2U */
		upsdebugx(0,
"This Tripp Lite device (%04x/%04x) is not supported by usbhid-ups.\n"
"Please use the tripplite_usb driver instead.\n",
					 hd->VendorID, hd->ProductID);
		return 0;

	/* by default, reject, unless the productid option is given */
	default:
		if (getval("productid")) {
			return 1;
		}
		possibly_supported("Tripp Lite", hd);
		return 0;
	}
}

subdriver_t tripplite_subdriver = {
	TRIPPLITE_HID_VERSION,
	tripplite_claim,
	tripplite_utab,
	tripplite_hid2nut,
	tripplite_format_model,
	tripplite_format_mfr,
	tripplite_format_serial,
};
