/*  delta_ups-hid.c - data mapping subdriver to monitor Delta UPS USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2012	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  2013 Charles Lepple <clepple+nut@gmail.com>
 *  2020 Luka Kovacic <luka.kovacic@builtin.io>
 *	2021		Jungeon Kim <me@jungeon.kim>
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
#include "delta_ups-hid.h"
#include "main.h"	/* for getval() */
#include "usb-common.h"

#define DELTA_UPS_HID_VERSION	"Delta UPS HID 0.5"

/* Delta UPS */
#define DELTA_UPS_VENDORID	0x05dd

/* USB IDs device table */
static usb_device_id_t delta_ups_usb_device_table[] = {
	/* Delta RT Series, Single Phase, 1/2/3 kVA */
	/* Delta UPS Amplon R Series, Single Phase UPS, 1/2/3 kVA */
	{ USB_DEVICE(DELTA_UPS_VENDORID, 0x041b), NULL },

	/* Terminating entry */
	{ -1, -1, NULL }
};


/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* DELTA usage table */
static usage_lkp_t delta_ups_usage_lkp[] = {
	{ "DELTA1",									0x00000000 },
	{ "DELTA2",									0xff000055 },
/*	{ "DELTA3",									0xffff0010 }, */
	{ "DeltaCustom",								0xffff0010 },
	{ "DELTA4",									0xffff0056 },
/*	{ "DELTA5",									0xffff0057 }, */
	{ "DeltaConfigTransferLowMax",				0xffff0057 },
/*	{ "DELTA6",									0xffff0058 }, */
	{ "DeltaConfigTransferLowMin",				0xffff0058 },
/*	{ "DELTA7",									0xffff0059 }, */
	{ "DeltaConfigTransferHighMax",				0xffff0059 },
/*	{ "DELTA8",									0xffff005a }, */
	{ "DeltaConfigTransferHighMin",				0xffff005a },
	{ "DELTA9",									0xffff0060 },
/*	{ "DELTA10",									0xffff0061 }, */
	{ "DeltaConfigExternalBatteryPack",			0xffff0061 },
	{ "DELTA11",									0xffff0062 },
	{ "DELTA12",									0xffff0063 },
	{ "DELTA13",									0xffff0064 },
	{ "DELTA14",									0xffff0065 },
	{ "DELTA15",									0xffff0066 },
	{ "DELTA16",									0xffff0067 },
	{ "DELTA17",									0xffff0068 },
/*	{ "DELTA18",									0xffff0075 }, */
	{ "DeltaModelName",							0xffff0075 },
	{ "DELTA19",									0xffff0076 },
/*	{ "DELTA20",									0xffff007c }, */
	{ "DeltaUPSType",							0xffff007c },
	{ "DELTA21",									0xffff007d },
/*	{ "DELTA22",									0xffff0081 }, */
	{ "DeltaConfigStartPowerRestoreDelay",		0xffff0081 },
/*	{ "DELTA23",									0xffff0091 }, */
	{ "DeltaOutputSource",						0xffff0091 },
	{ "DELTA24",									0xffff0092 },
	{ "DELTA25",									0xffff0093 },
	{ "DELTA26",									0xffff0094 },
	{ "DELTA27",									0xffff0095 },
	{ "DELTA28",									0xffff0096 },
	{ "DELTA29",									0xffff0097 },
	{ "DELTA30",									0xffff0098 },
	{ "DELTA31",									0xffff0099 },
	{ "DELTA32",									0xffff009a },
/*	{ "DELTA33",									0xffff009b }, */
	{ "DeltaConfigSensitivity",					0xffff009b },
/*	{ "DELTA34",									0xffff009c }, */
	{ "DeltaConfigStartPowerRestore",			0xffff009c },

	/* Terminating entry */
	{ NULL, 0 }
};

static usage_tables_t delta_ups_utab[] = {
	delta_ups_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t delta_ups_hid2nut[] = {
	{ "battery.voltage.nominal", 0, 0, "UPS.BatterySystem.Battery.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "battery.voltage", 0, 0, "UPS.BatterySystem.Battery.Voltage", NULL, "%.1f", HU_FLAG_QUICK_POLL, NULL },
	{ "battery.temperature", 0, 0, "UPS.BatterySystem.Temperature", NULL, "%s", HU_FLAG_QUICK_POLL, kelvin_celsius_conversion },
	{ "test.battery.start.quick", 0, 0, "UPS.BatterySystem.Test", NULL, "1", HU_TYPE_CMD, NULL },
	{ "ups.power.nominal", 0, 0, "UPS.Flow.ConfigApparentPower", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "output.frequency.nominal", 0, 0, "UPS.Flow.ConfigFrequency", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "output.voltage.nominal", 0, 0, "UPS.Flow.ConfigVoltage", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "ups.realpower", 0, 0, "UPS.OutletSystem.Outlet.ActivePower", NULL, "%.1f", HU_FLAG_QUICK_POLL, NULL },
	{ "ups.delay.reboot", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeReboot", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "ups.delay.shutdown", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "ups.delay.start", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "ups.load", 0, 0, "UPS.OutletSystem.Outlet.PercentLoad", NULL, "%.1f", HU_FLAG_QUICK_POLL, NULL },
	{ "input.frequency", 0, 0, "UPS.PowerConverter.Input.Frequency", NULL, "%.1f", HU_FLAG_QUICK_POLL, NULL },
	{ "input.voltage", 0, 0, "UPS.PowerConverter.Input.Voltage", NULL, "%.1f", HU_FLAG_QUICK_POLL, NULL },
	{ "output.current", 0, 0, "UPS.PowerConverter.Output.Current", NULL, "%.1f", HU_FLAG_QUICK_POLL, NULL },
	{ "output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },
	{ "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Boost", NULL, NULL, HU_FLAG_QUICK_POLL, boost_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Buck", NULL, NULL, HU_FLAG_QUICK_POLL, trim_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Overload", NULL, NULL, HU_FLAG_QUICK_POLL, overload_info },
	{ "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", HU_FLAG_QUICK_POLL, beeper_info },
	{ "battery.capacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.voltage.nominal", 0, 0, "UPS.PowerSummary.Input.ConfigVoltage", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "input.voltage", 0, 0, "UPS.PowerSummary.Input.Voltage", NULL, "%.1f", HU_FLAG_QUICK_POLL, NULL },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, HU_FLAG_QUICK_POLL, replacebatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, HU_FLAG_QUICK_POLL, shutdownimm_info },
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },
	{ "battery.charge.low", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },
	{ "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
	{ "ups.mfr", 0, 0, "UPS.PowerSummary.iManufacturer", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
	{ "ups.model", 0, 0, "UPS.PowerSummary.iProduct", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
	{ "ups.serial", 0, 0, "UPS.PowerSummary.iSerialNumber", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },

	/* Terminating entry */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *delta_ups_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "Delta";
}

static const char *delta_ups_format_model(HIDDevice_t *hd) {
	static char model[SMALLBUF];
	HIDGetItemString(udev, "UPS.DeltaCustom.[1].DeltaModelName", model, sizeof(model), delta_ups_utab);

	if (strlen(model) < 1) {
		return hd->Product;
	}

	return model;
}

static const char *delta_ups_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int delta_ups_claim(HIDDevice_t *hd) {
	int status = is_usb_device_supported(delta_ups_usb_device_table, hd);

	switch (status) {
		case SUPPORTED:
			return 1;

		case POSSIBLY_SUPPORTED:
			/* by default, reject, unless the productid option is given */
			if (getval("productid")) {
				return 1;
			}
			possibly_supported("Delta", hd);
			return 0;

		case NOT_SUPPORTED:
		default:
			return 0;
	}
}

subdriver_t delta_ups_subdriver = {
	DELTA_UPS_HID_VERSION,
	delta_ups_claim,
	delta_ups_utab,
	delta_ups_hid2nut,
	delta_ups_format_model,
	delta_ups_format_mfr,
	delta_ups_format_serial,
};
