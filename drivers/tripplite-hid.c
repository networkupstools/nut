/*  tripplite-hid.c - data to monitor Tripp Lite USB/HID devices with NUT
 *
 *  Copyright (C)  
 *	2003 - 2005	Arnaud Quette <arnaud.quette@free.fr>
 *	2005		Peter Selinger <selinger@users.sourceforge.net>
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

#include "newhidups.h"
#include "tripplite-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "dstate.h"   /* for STAT_INSTCMD_HANDLED */
#include "common.h"

#define TRIPPLITE_HID_VERSION "TrippLite HID 0.0 (incomplete)"

#define TRIPPLITE_VENDORID 0x09ae 
#define TRIPPLITE_HID_PRODUCTID 0x2005 
/* not all Tripp Lite products are HID, some are "serial over USB" */

/* --------------------------------------------------------------- */
/*	Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* TRIPPLITE usage table */
static usage_lkp_t tripplite_usage_lkp[] = {
	/* currently unknown: 
	   ffff0010, 00ff0001, ffff007d, ffff00c0, ffff00c1, ffff00c2,
	   ffff00c3, ffff00c4, ffff00c5, ffff00d2, ffff0091, ffff0092,
	   ffff00c7, 0084004b, 008400d0 
	*/

	{  "\0", 0x0 }
};

static usage_tables_t tripplite_utab[] = {
	tripplite_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/*	HID2NUT lookup table					   */
/* --------------------------------------------------------------- */

/* HID2NUT lookup table */
static hid_info_t tripplite_hid2nut[] = {
	{ "UPS.BatterySystem.Battery.PresentStatus.Charging", 0, 1, "UPS.BatterySystem.Battery.PresentStatus.Charging", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.BatterySystem.Battery.PresentStatus.Discharging", 0, 1, "UPS.BatterySystem.Battery.PresentStatus.Discharging", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.BatterySystem.Battery.PresentStatus.NeedReplacement", 0, 1, "UPS.BatterySystem.Battery.PresentStatus.NeedReplacement", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.ffff0010.00ff0001.ffff007d", 0, 1, "UPS.ffff0010.00ff0001.ffff007d", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.ffff0015.00ff0001.ffff00c0", 0, 1, "UPS.ffff0015.00ff0001.ffff00c0", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.ffff0015.00ff0001.ffff00c1", 0, 1, "UPS.ffff0015.00ff0001.ffff00c1", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.ffff0015.00ff0001.ffff00c2", 0, 1, "UPS.ffff0015.00ff0001.ffff00c2", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.ffff0015.00ff0001.ffff00c3", 0, 1, "UPS.ffff0015.00ff0001.ffff00c3", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.ffff0015.00ff0001.ffff00c4", 0, 1, "UPS.ffff0015.00ff0001.ffff00c4", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.ffff0015.00ff0001.ffff00c5", 0, 1, "UPS.ffff0015.00ff0001.ffff00c5", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.ffff0015.00ff0001.ffff00d2", 0, 1, "UPS.ffff0015.00ff0001.ffff00d2", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.Flow.ConfigFrequency", 0, 1, "UPS.Flow.ConfigFrequency", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.Flow.ConfigVoltage", 0, 1, "UPS.Flow.ConfigVoltage", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.OutletSystem.Outlet.DelayBeforeReboot", 0, 1, "UPS.OutletSystem.Outlet.DelayBeforeReboot", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.OutletSystem.Outlet.DelayBeforeShutdown", 0, 1, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.OutletSystem.Outlet.ffff0091", 0, 1, "UPS.OutletSystem.Outlet.ffff0091", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.OutletSystem.Outlet.ffff0092", 0, 1, "UPS.OutletSystem.Outlet.ffff0092", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.OutletSystem.Outlet.ffff00c7", 0, 1, "UPS.OutletSystem.Outlet.ffff00c7", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerConverter.Input.Frequency", 0, 1, "UPS.PowerConverter.Input.Frequency", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerConverter.PresentStatus.AwaitingPower", 0, 1, "UPS.PowerConverter.PresentStatus.AwaitingPower", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerConverter.PresentStatus.InternalFailure", 0, 1, "UPS.PowerConverter.PresentStatus.InternalFailure", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerConverter.PresentStatus.OverTemperature", 0, 1, "UPS.PowerConverter.PresentStatus.OverTemperature", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerConverter.PresentStatus.Used", 0, 1, "UPS.PowerConverter.PresentStatus.Used", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerConverter.PresentStatus.VoltageOutOfRange", 0, 1, "UPS.PowerConverter.PresentStatus.VoltageOutOfRange", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerSummary.CapacityMode", 0, 1, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerSummary.FullChargeCapacity", 0, 1, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerSummary.iManufacturer", 0, 1, "UPS.PowerSummary.iManufacturer", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerSummary.iProduct", 0, 1, "UPS.PowerSummary.iProduct", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerSummary.iSerialNumber", 0, 1, "UPS.PowerSummary.iSerialNumber", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerSummary.PresentStatus.0084004b", 0, 1, "UPS.PowerSummary.PresentStatus.0084004b", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerSummary.PresentStatus.008400d0", 0, 1, "UPS.PowerSummary.PresentStatus.008400d0", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerSummary.PresentStatus.ConfigActivePower", 0, 1, "UPS.PowerSummary.PresentStatus.ConfigActivePower", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerSummary.PresentStatus.ConfigPercentLoad", 0, 1, "UPS.PowerSummary.PresentStatus.ConfigPercentLoad", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "UPS.PowerSummary.Voltage", 0, 1, "UPS.PowerSummary.Voltage", NULL, "%.0f", HU_FLAG_OK, NULL },

	/* Server side variables */
	{ "driver.version.internal", ST_FLAG_STRING, sizeof(DRIVER_VERSION), NULL, NULL, DRIVER_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
	{ "driver.version.data", ST_FLAG_STRING, sizeof(TRIPPLITE_HID_VERSION), NULL, NULL, TRIPPLITE_HID_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
	
	/* Battery page */
	{ "battery.charge", 0, 1, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "battery.voltage",  0, 0, "UPS.BatterySystem.Battery.Voltage", NULL, "%.1f", HU_FLAG_OK, divide_by_10_conversion },
	{ "battery.voltage.nominal", 0, 0, "UPS.BatterySystem.Battery.ConfigVoltage", NULL, "%.1f", HU_FLAG_OK, NULL },
	{ "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_OK, stringid_conversion },
	
	/* UPS page */
	{ "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL},
	{ "ups.test.result", 0, 0, "UPS.BatterySystem.Test", NULL, "%s", HU_FLAG_OK, &test_read_info[0] },
	{ "ups.beeper.status", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", HU_FLAG_OK, &beeper_info[0] },
	{ "ups.power.nominal", 0, 0, "UPS.Flow.ConfigApparentPower", NULL, "%s", HU_FLAG_OK, NULL },
	
	/* Special case: ups.status */
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, "%.0f", HU_FLAG_OK, &online_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Discharging", NULL, "%.0f", HU_FLAG_OK, &discharging_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Charging", NULL, "%.0f", HU_FLAG_OK, &charging_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, "%.0f", HU_FLAG_OK, &shutdownimm_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerConverter.PresentStatus.Overload", NULL, "%.0f", HU_FLAG_OK, &overload_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, "%.0f", HU_FLAG_OK, &replacebatt_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerConverter.PresentStatus.Boost", NULL, "%.0f", HU_FLAG_OK, &boost_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerConverter.PresentStatus.Buck", NULL, "%.0f", HU_FLAG_OK, &trim_info[0] },
	
	/* Input page */
	{ "input.voltage", 0, 0, "UPS.PowerConverter.Input.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
	{ "input.voltage.nominal", 0, 0, "UPS.PowerSummary.Input.ConfigVoltage", NULL, "%.0f", HU_FLAG_OK, NULL },
	
	/* Output page */
	{ "output.voltage.nominal", 0, 0, "UPS.Flow.ConfigVoltage", NULL, "%.1f", HU_FLAG_OK, NULL },
	
	/* instant commands. */
	{ "test.battery.start.quick", 0, 0, "UPS.BatterySystem.Test", NULL, "1", HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] }, /* TODO: lookup needed? */
	{ "test.battery.start.deep", 0, 0, "UPS.BatterySystem.Test", NULL, "2", HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] },
	{ "test.battery.stop", 0, 0, "UPS.BatterySystem.Test", NULL, "3", HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] },
	
	{ "load.off", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, "0", HU_TYPE_CMD | HU_FLAG_OK, NULL },
	
	/* FIXME: which of the behaviors does this implement? */
	{ "shutdown.return", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, "1", HU_TYPE_CMD | HU_FLAG_OK, NULL },
	{ "shutdown.stop", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD | HU_FLAG_OK, NULL },
	
	{ "beeper.on", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD | HU_FLAG_OK, NULL },
	{ "beeper.off", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD | HU_FLAG_OK, NULL },
	
	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

/* shutdown method for Tripp Lite */
static int tripplite_shutdown(int ondelay, int offdelay) {
	
	upsdebugx(2, "Trying load.off.");
	if (instcmd("load.off", NULL) == STAT_INSTCMD_HANDLED) {
		return 1;
	}
	upsdebugx(2, "Load.off failed.");
	
	upsdebugx(2, "Trying shutdown.return.");
	if (instcmd("shutdown.return", NULL) == STAT_INSTCMD_HANDLED) {
		return 1;
	}
	upsdebugx(2, "shutdown.return failed.");

	return 0;
}

static char *tripplite_format_model(HIDDevice *hd) {
	return hd->Product;
}

static char *tripplite_format_mfr(HIDDevice *hd) {
	return hd->Vendor;
}

static char *tripplite_format_serial(HIDDevice *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int tripplite_claim(HIDDevice *hd) {
	if (hd->VendorID == TRIPPLITE_VENDORID) {
		if (hd->ProductID == TRIPPLITE_HID_PRODUCTID) {
			return 1;
		}

		upsdebugx(1, "This particular Tripp Lite device (%04x/%04x) is not (or perhaps not\n"
			  "yet) supported by newhidups. First try the tripplite_usb driver. If\n"
			  "this fails, please write to the NUT developer's mailing list.\n", 
			  hd->VendorID, hd->ProductID);
	}
	return 0;
}

subdriver_t tripplite_subdriver = {
	TRIPPLITE_HID_VERSION,
	tripplite_claim,
	tripplite_utab,
	tripplite_hid2nut,
	tripplite_shutdown,
	tripplite_format_model,
	tripplite_format_mfr,
	tripplite_format_serial,
};
