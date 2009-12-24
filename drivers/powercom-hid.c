/* powercom-hid.c - subdriver to monitor PowerCOM USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2009	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  path-to-subdriver script. It must be customized.
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
 */

#include "usbhid-ups.h"
#include "powercom-hid.h"
#include "extstate.h"	/* for ST_FLAG_STRING */
#include "main.h"	/* for getval() */
#include "common.h"
#include "usb-common.h"

#define POWERCOM_HID_VERSION	"PowerCOM HID 0.1"
/* FIXME: experimental flag to be put in upsdrv_info */

/* PowerCOM */
#define POWERCOM_VENDORID	0x0d9f

/* USB IDs device table */
static usb_device_id_t powercom_usb_device_table[] = {
	/* PowerCOM IMP - IMPERIAL Series */
	{ USB_DEVICE(POWERCOM_VENDORID, 0x00a2), NULL },
	/* PowerCOM SKP - Smart KING Pro (all Smart series) */
	{ USB_DEVICE(POWERCOM_VENDORID, 0x00a3), NULL },
	/* PowerCOM WOW */
	{ USB_DEVICE(POWERCOM_VENDORID, 0x00a4), NULL },
	/* PowerCOM VGD - Vanguard */
	{ USB_DEVICE(POWERCOM_VENDORID, 0x00a5), NULL },
	/* PowerCOM BNT - Black Knight Pro */
	{ USB_DEVICE(POWERCOM_VENDORID, 0x00a6), NULL },

	/* Terminating entry */
	{ -1, -1, NULL }
};

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* POWERCOM usage table */
static usage_lkp_t powercom_usage_lkp[] = {
	{ "POWERCOM1",	0x0084002f },
	{ "POWERCOM2",	0xff860060 },
	{ "POWERCOM3",	0xff860080 },
	{  NULL, 0 }
};

static usage_tables_t powercom_utab[] = {
	powercom_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t powercom_hid2nut[] = {
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, 0, online_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BatteryPresent", NULL, NULL, 0, nobattery_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, 0, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, 0, charging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.CommunicationLost", NULL, NULL, 0, commfault_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, 0, discharging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, NULL, 0, overload_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL, NULL, 0, timelimitexpired_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },
/*	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.POWERCOM3", NULL, "%.0f", 0, NULL }, */
/*	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownRequested", NULL, "%.0f", 0, NULL }, */
/*	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.VoltageNotRegulated", NULL, "%.0f", 0, NULL }, */
	{ "BOOL", 0, 0, "UPS.PresentStatus.ACPresent", NULL, NULL, 0, online_info },
	{ "BOOL", 0, 0, "UPS.PresentStatus.BatteryPresent", NULL, NULL, 0, nobattery_info },
	{ "BOOL", 0, 0, "UPS.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, 0, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.PresentStatus.Boost", NULL, NULL, 0, boost_info },
	{ "BOOL", 0, 0, "UPS.PresentStatus.Buck", NULL, NULL, 0, trim_info },
	{ "BOOL", 0, 0, "UPS.PresentStatus.Charging", NULL, NULL, 0, charging_info },
	{ "BOOL", 0, 0, "UPS.PresentStatus.CommunicationLost", NULL, NULL, 0, commfault_info },
	{ "BOOL", 0, 0, "UPS.PresentStatus.Discharging", NULL, NULL, 0, discharging_info },
	{ "BOOL", 0, 0, "UPS.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info },
	{ "BOOL", 0, 0, "UPS.PresentStatus.Overload", NULL, NULL, 0, overload_info },
	{ "BOOL", 0, 0, "UPS.PresentStatus.RemainingTimeLimitExpired", NULL, NULL, 0, timelimitexpired_info },
	{ "BOOL", 0, 0, "UPS.PresentStatus.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },
/*	{ "BOOL", 0, 0, "UPS.PresentStatus.POWERCOM3", NULL, "%.0f", 0, NULL }, */
/*	{ "BOOL", 0, 0, "UPS.PresentStatus.ShutdownRequested", NULL, "%.0f", 0, NULL }, */
/*	{ "BOOL", 0, 0, "UPS.PresentStatus.Tested", NULL, "%.0f", 0, NULL }, */
/*	{ "BOOL", 0, 0, "UPS.PresentStatus.VoltageNotRegulated", NULL, "%.0f", 0, NULL }, */

/*
 * According to the HID PDC specifications, the below values should report the battery voltage. But in
 * the Black Knight 500AP these seem to report output.voltage(.nominal) instead, so we don't use them. 
 *	{ "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.2f", 0, NULL },
 *	{ "battery.voltage", 0, 0, "UPS.Battery.Voltage", NULL, "%.2f", 0, NULL },
 *	{ "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
 *	{ "battery.voltage.nominal", 0, 0, "UPS.Battery.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
 */
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.charge", 0, 0, "UPS.Battery.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.charge.low", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
	{ "battery.date", 0, 0, "UPS.Battery.ManufacturerDate", NULL, "%s", HU_FLAG_STATIC, date_conversion },
	{ "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
	{ "unmapped.ups.battery.delaybeforestartup", 0, 0, "UPS.Battery.DelayBeforeStartup", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.battery.initialized", 0, 0, "UPS.Battery.Initialized", NULL, "%.0f", 0, NULL },

	{ "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", HU_FLAG_SEMI_STATIC, beeper_info },
	{ "ups.beeper.status", 0, 0, "UPS.AudibleAlarmControl", NULL, "%s", HU_FLAG_SEMI_STATIC, beeper_info },
	{ "ups.load", 0, 0, "UPS.Output.PercentLoad", NULL, "%.0f", 0, NULL },
	{ "ups.date", 0, 0, "UPS.PowerSummary.ManufacturerDate", NULL, "%s", HU_FLAG_STATIC, date_conversion },
	{ "ups.test.result", 0, 0, "UPS.Battery.Test", NULL, "%s", HU_FLAG_SEMI_STATIC, test_read_info },
/*	{ "unmapped.ups.powersummary.imanufacturer", 0, 0, "UPS.PowerSummary.iManufacturer", NULL, "%s", HU_FLAG_STATIC, stringid_conversion }, */
/*	{ "unmapped.ups.powersummary.iproduct", 0, 0, "UPS.PowerSummary.iProduct", NULL, "%s", HU_FLAG_STATIC, stringid_conversion }, */
/*	{ "unmapped.ups.powersummary.iserialnumber", 0, 0, "UPS.PowerSummary.iSerialNumber", NULL, "%s", HU_FLAG_STATIC, stringid_conversion }, */
	{ "unmapped.ups.iname", 0, 0, "UPS.iName", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
	{ "unmapped.ups.powersummary.ioeminformation", 0, 0, "UPS.PowerSummary.iOEMInformation", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
	{ "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_FLAG_ABSENT, NULL},
	{ "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL},
	{ "ups.timer.start", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
	{ "ups.timer.shutdown", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},

	{ "input.voltage", 0, 0, "UPS.Input.Voltage", NULL, "%.1f", 0, NULL },
	{ "input.voltage.nominal", ST_FLAG_RW | ST_FLAG_STRING, 3, "UPS.Input.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.frequency", 0, 0, "UPS.Input.Frequency", NULL, "%.1f", 0, NULL },

	{ "output.voltage", 0, 0, "UPS.Output.Voltage", NULL, "%.1f", 0, NULL },
	{ "output.voltage.nominal", ST_FLAG_RW | ST_FLAG_STRING, 3, "UPS.Output.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "output.frequency", 0, 0, "UPS.Output.Frequency", NULL, "%.0f", 0, NULL },

/*	{ "unmapped.ups.powercom1", 0, 0, "UPS.POWERCOM1", NULL, "%.0f", 0, NULL }, broken pipe */
/*	{ "unmapped.ups.powercom2", 0, 0, "UPS.POWERCOM2", NULL, "%.0f", 0, NULL }, broken pipe */
	{ "unmapped.ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.shutdownimminent", 0, 0, "UPS.ShutdownImminent", NULL, "%.0f", 0, NULL },

	/* instcmds */
	{ "test.battery.start.quick", 0, 0, "UPS.Battery.Test", NULL, "1", HU_TYPE_CMD, NULL },
	{ "test.battery.start.deep", 0, 0, "UPS.Battery.Test", NULL, "2", HU_TYPE_CMD, NULL },
	{ "test.battery.stop", 0, 0, "UPS.Battery.Test", NULL, "3", HU_TYPE_CMD, NULL },
	{ "load.off.delay", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
	{ "load.on.delay", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },
	{ "shutdown.stop", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "0", HU_TYPE_CMD, NULL },
	{ "beeper.disable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "1", HU_TYPE_CMD, NULL },
	{ "beeper.enable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
	{ "beeper.mute", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static char *powercom_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static char *powercom_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "PowerCOM";
}

static char *powercom_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int powercom_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(powercom_usb_device_table, hd->VendorID, hd->ProductID);

	switch (status)
	{
	case POSSIBLY_SUPPORTED:
		/* by default, reject, unless the productid option is given */
		if (getval("productid")) {
			return 1;
		}
		possibly_supported("PowerCOM", hd);
		return 0;

	case SUPPORTED:
		return 1;

	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

subdriver_t powercom_subdriver = {
	POWERCOM_HID_VERSION,
	powercom_claim,
	powercom_utab,
	powercom_hid2nut,
	powercom_format_model,
	powercom_format_mfr,
	powercom_format_serial,
};
