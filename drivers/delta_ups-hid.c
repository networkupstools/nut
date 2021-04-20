/*  delta_ups-hid.c - data to monitor Delta UPS USB/HID devices with NUT
 *
 *  Copyright (C)
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "usbhid-ups.h"
#include "delta_ups-hid.h"
#include "usb-common.h"

#define DELTA_UPS_HID_VERSION "Delta UPS HID 0.1"
#define DELTA_UPS_VENDORID 0x05dd

static usb_device_id_t delta_ups_usb_device_table[] = {
	/* Delta UPS Amplon R Series, Single Phase UPS, 1/2/3 kVA */
	{ USB_DEVICE(DELTA_UPS_VENDORID, 0x041b), NULL },

	/* Terminating entry */
	{ -1, -1, NULL }
};

static usage_lkp_t delta_ups_usage_lkp[] = {
	{ "DeltaCustom",							0xffff0010 },

	{ "DeltaConfigTransferLowMax",				0xffff0057 },
	{ "DeltaConfigTransferLowMin",				0xffff0058 },
	{ "DeltaConfigTransferHighMax",				0xffff0059 },
	{ "DeltaConfigTransferHighMin",				0xffff005a },
	{ "DeltaConfigStartPowerRestoreDelay",			0xffff0081 },
	{ "DeltaOutputSource",						0xffff0091 },
	{ "DeltaModelName",							0xffff0075 },
	{ "DeltaUPSType",							0xffff007c },
	{ "DeltaConfigSensitivity",					0xffff009b },
	{ "DeltaConfigStartPowerRestore",			0xffff009c },
	{ "DeltaConfigExternalBatteryPack",			0xffff0061 },

	/* Terminating entry */
	{ NULL, 0 }
};

static usage_tables_t delta_ups_utab[] = {
	delta_ups_usage_lkp,
	hid_usage_lkp,
	NULL,
};

static info_lkp_t delta_ups_sensitivity_info[] = {
	{ 0, "normal", NULL, NULL },
	{ 1, "reduced", NULL, NULL },
	{ 2, "low", NULL, NULL },

	/* Terminating entry */
	{ 0, NULL, NULL, NULL }
};

static const char *delta_ups_type_fun(double value)
{
	static const char* upstypes[] = {
		"online",
		"offline",
		"line-interactive",
		"3-phase",
		"split-phase"
	};

	int type = (int)value & 0xf;
	if (type == 6) {
		type = 4;
	} else if (2 < type && type <= 5) {
		type -= 2;
	}

	if (type < 0 || type > 4) {
		return NULL;
	}

	return upstypes[type];
}

static info_lkp_t delta_ups_type_info[] = {
	{ 0, NULL, delta_ups_type_fun },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t delta_ups_output_source_info[] = {
	{ 0, "normal", NULL, NULL },
	{ 1, "battery", NULL, NULL },
	{ 2, "bypass/reserve", NULL, NULL },
	{ 3, "reducing", NULL, NULL },
	{ 4, "boosting", NULL, NULL },
	{ 5, "manual bypass", NULL, NULL },
	{ 6, "other", NULL, NULL },
	{ 7, "no output", NULL, NULL },
	{ 8, "on eco", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static hid_info_t delta_ups_hid2nut[] = {
	{ "input.sensitivity", ST_FLAG_RW, 0, "UPS.DeltaCustom.[1].DeltaConfigSensitivity", NULL, "%s", 0, delta_ups_sensitivity_info },
	{ "input.voltage", 0, 0, "UPS.PowerSummary.Input.Voltage", NULL, "%.1f", HU_FLAG_QUICK_POLL, NULL },
	{ "input.transfer.low", ST_FLAG_RW, 0, "UPS.PowerConverter.Output.LowVoltageTransfer", NULL, "%.0f", 0, NULL },
	{ "input.transfer.high", ST_FLAG_RW, 0, "UPS.PowerConverter.Output.HighVoltageTransfer", NULL, "%.0f", 0, NULL },
	{ "input.transfer.low.min", 0, 0, "UPS.PowerConverter.Output.DeltaConfigTransferLowMin", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.transfer.low.max", 0, 0, "UPS.PowerConverter.Output.DeltaConfigTransferLowMax", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.transfer.high.min", 0, 0, "UPS.PowerConverter.Output.DeltaConfigTransferHighMin", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.transfer.high.max", 0, 0, "UPS.PowerConverter.Output.DeltaConfigTransferHighMax", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.source", 0, 0, "UPS.OutletSystem.Outlet.DeltaOutputSource", NULL, "%s", 0, delta_ups_output_source_info },
	{ "input.frequency", 0, 0, "UPS.PowerConverter.Input.Frequency", NULL, "%.1f", 0, NULL },

	{ "battery.voltage.nominal", 0, 0, "UPS.BatterySystem.Battery.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "battery.voltage", 0, 0, "UPS.BatterySystem.Battery.Voltage", NULL, "%.1f", 0, NULL },
	{ "battery.charge", 0, 0, "UPS.BatterySystem.Battery.RemainingCapacity", NULL, "%.1f", 0, NULL },
	{ "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", 0, NULL },
	{ "battery.temperature", 0, 0, "UPS.BatterySystem.Temperature", NULL, "%s", 0, kelvin_celsius_conversion },
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
	{ "battery.capacity", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", 0, NULL },

	{ "output.voltage.nominal", 0, 0, "UPS.Flow.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "output.frequency.nominal", 0, 0, "UPS.Flow.ConfigFrequency", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", NULL, "%.1f", 0, NULL },
	{ "output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", NULL, "%.1f", 0, NULL },
	{ "output.current", 0, 0, "UPS.PowerConverter.Output.Current", NULL, "%.1f", 0, NULL },

	{ "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", 0, beeper_info },
	{ "ups.test.result", 0, 0, "UPS.BatterySystem.Test", NULL, "%s", 0, test_read_info },
	{ "ups.type", 0, 0, "UPS.DeltaCustom.[1].DeltaUPSType", NULL, "%s", HU_FLAG_STATIC, delta_ups_type_info },
	{ "ups.start.auto", ST_FLAG_RW, 0, "UPS.DeltaCustom.[1].DeltaConfigStartPowerRestore", NULL, "%s", 0, yes_no_info },
	{ "ups.power.nominal", 0, 0, "UPS.Flow.ConfigApparentPower", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "ups.realpower", 0, 0, "UPS.PowerConverter.Output.ActivePower", NULL, "%.1f", 0, NULL },
	{ "ups.load", 0, 0, "UPS.OutletSystem.Outlet.PercentLoad", NULL, "%.1f", 0, NULL },
	{ "ups.delay.start", ST_FLAG_RW, 0, "UPS.OutletSystem.Outlet.DeltaConfigStartPowerRestoreDelay", NULL, "%.0f", 0, NULL},
	{ "ups.timer.start", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
	{ "ups.timer.shutdown", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
	{ "ups.timer.reboot", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeReboot", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },

	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Good", NULL, NULL, HU_FLAG_QUICK_POLL, off_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.InternalFailure", NULL, NULL, HU_FLAG_QUICK_POLL, commfault_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, HU_FLAG_QUICK_POLL, shutdownimm_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyCharged", NULL, NULL, HU_FLAG_QUICK_POLL, fullycharged_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyDischarged", NULL, NULL, 0, depleted_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info},
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.VoltageOutOfRange", NULL, NULL, 0, vrange_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Buck", NULL, NULL, 0, trim_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Boost", NULL, NULL, 0, boost_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Overload", NULL, NULL, 0, overload_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.Used", NULL, NULL, 0, nobattery_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.OverTemperature", NULL, NULL, 0, overheat_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.InternalFailure", NULL, NULL, 0, commfault_info },
	{ "BOOL", 0, 0, "UPS.PowerConverter.PresentStatus.AwaitingPower", NULL, NULL, 0, awaitingpower_info },

	{ "beeper.on", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
	{ "beeper.off", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },
	{ "beeper.enable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
	{ "beeper.disable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "1", HU_TYPE_CMD, NULL },
	{ "beeper.mute", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },

	/* 10 seoncds battery test */
	{ "test.battery.start.quick", 0, 0, "UPS.BatterySystem.Test", NULL, "1", HU_TYPE_CMD, NULL },
	/* test until battery low */
	{ "test.battery.start.deep", 0, 0, "UPS.BatterySystem.Test", NULL, "2", HU_TYPE_CMD, NULL },
	{ "test.battery.stop", 0, 0, "UPS.BatterySystem.Test", NULL, "3", HU_TYPE_CMD, NULL },

	{ "load.on.delay", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },
	{ "load.off.delay", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },

	{ "shutdown.stop", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD, NULL },
	{ "shutdown.reboot", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeReboot", NULL, "10", HU_TYPE_CMD, NULL },

	/* Terminating entry */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *delta_ups_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor;
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

static int delta_ups_claim(HIDDevice_t *hd) {
	int status = is_usb_device_supported(delta_ups_usb_device_table, hd);

	switch (status) {
		case SUPPORTED:
			return 1;
		case POSSIBLY_SUPPORTED:
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
