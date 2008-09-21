/* hp-hid.c - subdriver to monitor HP USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2005 Arnaud Quette <arnaud.quette@free.fr>
 *  2005 - 2006 Peter Selinger <selinger@users.sourceforge.net>
 *  2008        Arjen de Korte <adkorte-guest@alioth.debian.org>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "usbhid-ups.h"
#include "hp-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "main.h"     /* for getval() */
#include "common.h"

#define HP_HID_VERSION      "HP HID 0.1"

#define HP_VENDORID 0x03f0

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* HP usage table */
static usage_lkp_t hp_usage_lkp[] = {
	{  NULL, 0x0 }
};

static usage_tables_t hp_utab[] = {
	hp_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t hp_hid2nut[] = {
  /* Server side variables */
  { "driver.version.internal", ST_FLAG_STRING, sizeof(DRIVER_VERSION), NULL, NULL, DRIVER_VERSION, HU_FLAG_ABSENT, NULL },
  { "driver.version.data", ST_FLAG_STRING, sizeof(HP_HID_VERSION), NULL, NULL, HP_HID_VERSION, HU_FLAG_ABSENT, NULL },
/*
  { "UPS.Flow.0xffff0097", 0, 0, "UPS.Flow.0xffff0097", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff0075", 0, 0, "UPS.0xffff0010.[1].0xffff0075", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff0076", 0, 0, "UPS.0xffff0010.[1].0xffff0076", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff007c", 0, 0, "UPS.0xffff0010.[1].0xffff007c", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff007d", 0, 0, "UPS.0xffff0010.[1].0xffff007d", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff00e0", 0, 0, "UPS.0xffff0010.[1].0xffff00e0", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff00e1", 0, 0, "UPS.0xffff0010.[1].0xffff00e1", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff00e2", 0, 0, "UPS.0xffff0010.[1].0xffff00e2", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff00e3", 0, 0, "UPS.0xffff0010.[1].0xffff00e3", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff00e4", 0, 0, "UPS.0xffff0010.[1].0xffff00e4", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff00e5", 0, 0, "UPS.0xffff0010.[1].0xffff00e5", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff00e6", 0, 0, "UPS.0xffff0010.[1].0xffff00e6", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff00e7", 0, 0, "UPS.0xffff0010.[1].0xffff00e7", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0010.[1].0xffff00e8", 0, 0, "UPS.0xffff0010.[1].0xffff00e8", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0015.[1].0xffff00c0", 0, 0, "UPS.0xffff0015.[1].0xffff00c0", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0015.[1].0xffff00c1", 0, 0, "UPS.0xffff0015.[1].0xffff00c1", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0015.[1].0xffff00c2", 0, 0, "UPS.0xffff0015.[1].0xffff00c2", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0015.[1].0xffff00c3", 0, 0, "UPS.0xffff0015.[1].0xffff00c3", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0015.[1].0xffff00c4", 0, 0, "UPS.0xffff0015.[1].0xffff00c4", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0015.[1].0xffff00c5", 0, 0, "UPS.0xffff0015.[1].0xffff00c5", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0015.[1].0xffff00d2", 0, 0, "UPS.0xffff0015.[1].0xffff00d2", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0015.[1].0xffff00d3", 0, 0, "UPS.0xffff0015.[1].0xffff00d3", NULL, "%.0f", 0, NULL },
  { "UPS.0xffff0015.[1].0xffff00d6", 0, 0, "UPS.0xffff0015.[1].0xffff00d6", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff0056", 0, 0, "UPS.OutletSystem.Outlet.0xffff0056", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff0081", 0, 0, "UPS.OutletSystem.Outlet.0xffff0081", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff0091", 0, 0, "UPS.OutletSystem.Outlet.0xffff0091", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff0092", 0, 0, "UPS.OutletSystem.Outlet.0xffff0092", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff0093", 0, 0, "UPS.OutletSystem.Outlet.0xffff0093", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff0095", 0, 0, "UPS.OutletSystem.Outlet.0xffff0095", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff0096", 0, 0, "UPS.OutletSystem.Outlet.0xffff0096", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff0098", 0, 0, "UPS.OutletSystem.Outlet.0xffff0098", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff00a2", 0, 0, "UPS.OutletSystem.Outlet.0xffff00a2", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff00a4", 0, 0, "UPS.OutletSystem.Outlet.0xffff00a4", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff00a7", 0, 0, "UPS.OutletSystem.Outlet.0xffff00a7", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff00a9", 0, 0, "UPS.OutletSystem.Outlet.0xffff00a9", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff00aa", 0, 0, "UPS.OutletSystem.Outlet.0xffff00aa", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff00ab", 0, 0, "UPS.OutletSystem.Outlet.0xffff00ab", NULL, "%.0f", 0, NULL },
  { "UPS.OutletSystem.Outlet.0xffff00ac", 0, 0, "UPS.OutletSystem.Outlet.0xffff00ac", NULL, "%.0f", 0, NULL },
  { "UPS.PowerSummary.iOEMInformation", 0, 0, "UPS.PowerSummary.iOEMInformation", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
*/
  { "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, 0, lowbatt_info },
  { "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.Charging", NULL, NULL, 0, charging_info },
  { "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.Discharging", NULL, NULL, 0, discharging_info },
  { "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.FullyCharged", NULL, NULL, 0, fullycharged_info },
  { "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.FullyDischarged", NULL, NULL, 0, depleted_info },
  { "BOOL", 0, 0, "UPS.BatterySystem.Battery.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info },
  { "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.AwaitingPower", NULL, NULL, 0, awaitingpower_info },
  { "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Boost", NULL, NULL, 0, boost_info },
  { "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Buck", NULL, NULL, 0, trim_info },
  { "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.InternalFailure", NULL, NULL, 0, commfault_info },
  { "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Overload", NULL, NULL, 0, overload_info },
  { "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.OverTemperature", NULL, NULL, 0, overheat_info },
  { "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Used", NULL, NULL, 0, nobattery_info },
  { "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.VoltageOutOfRange", NULL, NULL, 0, vrange_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyCharged", NULL, NULL, HU_FLAG_QUICK_POLL, fullycharged_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyDischarged", NULL, NULL, HU_FLAG_QUICK_POLL, depleted_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.InternalFailure", NULL, NULL, 0, commfault_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },

  { "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", HU_FLAG_SEMI_STATIC, beeper_info },
  { "ups.load", 0, 0, "UPS.OutletSystem.Outlet.PercentLoad", NULL, "%.0f", 0, NULL },
  { "ups.test.result", 0, 0, "UPS.BatterySystem.Test", NULL, "%s", 0, test_read_info },
  { "ups.power.nominal", 0, 0, "UPS.Flow.ConfigApparentPower", NULL, "%.0f", HU_FLAG_STATIC, NULL },

  { "battery.voltage.nominal", 0, 0, "UPS.BatterySystem.Battery.ConfigVoltage", NULL, "%.1f", HU_FLAG_STATIC, NULL },
  { "battery.voltage", 0, 0, "UPS.BatterySystem.Battery.Voltage", NULL, "%.1f", 0, NULL },
  { "battery.charge", 0, 0, "UPS.BatterySystem.Battery.RemainingCapacity", NULL, "%.0f", 0, NULL },
  { "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
  { "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "battery.charge.warning", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "battery.temperature", 0, 0, "UPS.BatterySystem.Temperature", NULL, "%s", 0, kelvin_celsius_conversion },
  { "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },

  { "input.voltage.nominal", 0, 0, "UPS.PowerSummary.Input.ConfigVoltage", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "input.voltage", 0, 0, "UPS.PowerSummary.Input.Voltage", NULL, "%.0f", 0, NULL },
  { "input.voltage", 0, 0, "UPS.PowerConverter.Input.Voltage", NULL, "%.0f", 0, NULL },
  { "input.frequency", 0, 0, "UPS.PowerConverter.Input.Frequency", NULL, "%.0f", 0, NULL },
  { "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },

  { "output.frequency.nominal", 0, 0, "UPS.Flow.ConfigFrequency", NULL, "%.0f", HU_FLAG_STATIC, NULL },
  { "output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", NULL, "%.0f", 0, NULL },
  { "output.voltage.nominal", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.Flow.ConfigVoltage", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", NULL, "%.0f", 0, NULL },

  /* instant commands. */
  { "test.battery.start.quick", 0, 0, "UPS.BatterySystem.Test", NULL, "1", HU_TYPE_CMD, NULL },
  { "test.battery.start.deep", 0, 0, "UPS.BatterySystem.Test", NULL, "2", HU_TYPE_CMD, NULL },
  { "test.battery.stop", 0, 0, "UPS.BatterySystem.Test", NULL, "3", HU_TYPE_CMD, NULL },

  { "load.off.delay", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
  { "load.on.delay", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },

  { "shutdown.stop", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD, NULL },
  { "shutdown.reboot", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeReboot", NULL, "10", HU_TYPE_CMD, NULL },

  { "beeper.disable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "1", HU_TYPE_CMD, NULL },
  { "beeper.enable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
  { "beeper.mute", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static char *hp_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static char *hp_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "Hewlett Packard";
}

static char *hp_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int hp_claim(HIDDevice_t *hd) {
	if (hd->VendorID != HP_VENDORID) {
		return 0;
	}
	switch (hd->ProductID) {

	/* accept any known UPS - add devices here as needed */
	case 0x1f0a:
		return 1;

	/* by default, reject, unless the productid option is given */
	default:
		if (getval("productid")) {
			return 1;
		}
		possibly_supported("HP", hd);
		return 0;
	}
}

subdriver_t hp_subdriver = {
	HP_HID_VERSION,
	hp_claim,
	hp_utab,
	hp_hid2nut,
	hp_format_model,
	hp_format_mfr,
	hp_format_serial,
};
