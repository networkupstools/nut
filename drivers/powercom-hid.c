/* powercom-hid.c - subdriver to monitor PowerCOM USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2009	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
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

#include "main.h"	/* for getval() */
#include "usbhid-ups.h"
#include "powercom-hid.h"
#include "usb-common.h"

#define POWERCOM_HID_VERSION	"PowerCOM HID 0.5"
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
	/* PowerCOM Vanguard and BNT-xxxAP */
	{ USB_DEVICE(POWERCOM_VENDORID, 0x0004), NULL },
	{ USB_DEVICE(POWERCOM_VENDORID, 0x0001), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};

static char powercom_scratch_buf[32];

static const char *powercom_startup_fun(double value)
{
	uint16_t	i = value;

	snprintf(powercom_scratch_buf, sizeof(powercom_scratch_buf), "%d", 60 * (((i & 0x00FF) << 8) + (i >> 8)));
	upsdebugx(3, "%s: value = %.0f, buf = %s", __func__, value, powercom_scratch_buf);

	return powercom_scratch_buf;
}

static double powercom_startup_nuf(const char *value)
{
	const char	*s = dstate_getinfo("ups.delay.start");
	uint16_t	val, command;
	int iv;

	iv = atoi(value ? value : s) / 60;
	if (iv < 0 || (intmax_t)iv > (intmax_t)UINT16_MAX) {
		upsdebugx(0, "%s: value = %d is not in uint16_t range", __func__, iv);
		return 0;
	}

	/* COMMENTME: What are we doing here, a byte-swap in the word? */
	val = (uint16_t)iv;
	command =  (uint16_t)(val << 8);
	command += (uint16_t)(val >> 8);
	upsdebugx(3, "%s: value = %s, command = %04X", __func__, value, command);

	return command;
}

static info_lkp_t powercom_startup_info[] = {
	{ 0, NULL, powercom_startup_fun, powercom_startup_nuf }
};

static const char *powercom_shutdown_fun(double value)
{
	uint16_t	i = value;

	snprintf(powercom_scratch_buf, sizeof(powercom_scratch_buf), "%d", 60 * (i & 0x00FF) + (i >> 8));
	upsdebugx(3, "%s: value = %.0f, buf = %s", __func__, value, powercom_scratch_buf);

	return powercom_scratch_buf;
}

static double powercom_shutdown_nuf(const char *value)
{
	const char	*s = dstate_getinfo("ups.delay.shutdown");
	uint16_t	val, command;
	int iv;

	iv = atoi(value ? value : s);
	if (iv < 0 || (intmax_t)iv > (intmax_t)UINT16_MAX) {
		upsdebugx(0, "%s: value = %d is not in uint16_t range", __func__, iv);
		return 0;
	}

	val = (uint16_t)iv;
	val = val ? val : 1;    /* 0 sets the maximum delay */
	command = ((uint16_t)((val % 60) << 8)) + (uint16_t)(val / 60);
	command |= 0x4000;	/* AC RESTART NORMAL ENABLE */
	upsdebugx(3, "%s: value = %s, command = %04X", __func__, value, command);

	return command;
}

static info_lkp_t powercom_shutdown_info[] = {
	{ 0, NULL, powercom_shutdown_fun, powercom_shutdown_nuf }
};

static double powercom_stayoff_nuf(const char *value)
{
	const char	*s = dstate_getinfo("ups.delay.shutdown");
	uint16_t	val, command;
	int iv;

	iv = atoi(value ? value : s);
	if (iv < 0 || (intmax_t)iv > (intmax_t)UINT16_MAX) {
		upsdebugx(0, "%s: value = %d is not in uint16_t range", __func__, iv);
		return 0;
	}

	val = (uint16_t)iv;
	val = val ? val : 1;    /* 0 sets the maximum delay */
	command = ((uint16_t)((val % 60) << 8)) + (uint16_t)(val / 60);
	command |= 0x8000;	/* AC RESTART NORMAL DISABLE */
	upsdebugx(3, "%s: value = %s, command = %04X", __func__, value, command);

	return command;
}

static info_lkp_t powercom_stayoff_info[] = {
	{ 0, NULL, NULL, powercom_stayoff_nuf }
};

static info_lkp_t powercom_beeper_info[] = {
	{ 1, "enabled", NULL, NULL },
	{ 2, "disabled", NULL, NULL },	/* muted? */
	{ 0, NULL, NULL, NULL }
};

static const char *powercom_voltage_conversion_fun(double value)
{
	static char buf[20];
	snprintf(buf, sizeof(buf), "%0.0f", value * 4);
	return buf;
}

static info_lkp_t powercom_voltage_conversion[] = {
	{ 0, NULL, powercom_voltage_conversion_fun, NULL }
};

static const char *powercom_upsfail_conversion_fun(double value)
{
	if ((long)value & 0x0001) {
		return "fanfail";
	} else {
		return "!fanfail";
	}
}

static info_lkp_t powercom_upsfail_conversion[] = {
	{ 0, NULL, powercom_upsfail_conversion_fun, NULL }
};

static const char *powercom_replacebatt_conversion_fun(double value)
{
	if ((long)value & 0x0002) {
		return "replacebatt";
	} else {
		return "!replacebatt";
	}
}

static info_lkp_t powercom_replacebatt_conversion[] = {
	{ 0, NULL, powercom_replacebatt_conversion_fun, NULL }
};

static const char *powercom_test_conversion_fun(double value)
{
	if ((long)value & 0x0004) {
		return "cal";
	} else {
		return "!cal";
	}
}

static info_lkp_t powercom_test_conversion[] = {
	{ 0, NULL, powercom_test_conversion_fun, NULL }
};

static const char *powercom_shutdownimm_conversion_fun(double value)
{
	if ((long)value & 0x0010) {
		return "shutdownimm";
	} else {
		return "!shutdownimm";
	}
}

static info_lkp_t powercom_shutdownimm_conversion[] = {
	{ 0, NULL, powercom_shutdownimm_conversion_fun, NULL }
};

static const char *powercom_online_conversion_fun(double value)
{
	if ((long)value & 0x0001) {
		return "!online";
	} else {
		return "online";
	}
}

static info_lkp_t powercom_online_conversion[] = {
	{ 0, NULL, powercom_online_conversion_fun, NULL }
};

static const char *powercom_lowbatt_conversion_fun(double value)
{
	if ((long)value & 0x0002) {
		return "lowbatt";
	} else {
		return "!lowbatt";
	}
}

static info_lkp_t powercom_lowbatt_conversion[] = {
	{ 0, NULL, powercom_lowbatt_conversion_fun, NULL }
};

static const char *powercom_trim_conversion_fun(double value)
{
	if (((long)value & 0x0018) == 0x0008) {
		return "trim";
	} else {
		return "!trim";
	}
}

static info_lkp_t powercom_trim_conversion[] = {
	{ 0, NULL, powercom_trim_conversion_fun, NULL }
};

static const char *powercom_boost_conversion_fun(double value)
{
	if (((long)value & 0x0018) == 0x0018) {
		return "boost";
	} else {
		return "!boost";
	}
}

static info_lkp_t powercom_boost_conversion[] = {
	{ 0, NULL, powercom_boost_conversion_fun, NULL }
};

static const char *powercom_overload_conversion_fun(double value)
{
	if ((long)value & 0x0020) {
		return "overload";
	} else {
		return "!overload";
	}
}

static info_lkp_t powercom_overload_conversion[] = {
	{ 0, NULL, powercom_overload_conversion_fun, NULL }
};

/* --------------------------------------------------------------- */
/* Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* POWERCOM usage table */
static usage_lkp_t powercom_usage_lkp[] = {
	{ "PowercomUPS",                      0x00020004 },
	{ "PowercomBatterySystem",            0x00020010 },
	{ "PowercomPowerConverter",           0x00020016 },
	{ "PowercomInput",                    0x0002001a },
	{ "PowercomOutput",                   0x0002001c },
	{ "PowercomVoltage",                  0x00020030 },
	{ "PowercomFrequency",                0x00020032 },
	{ "PowercomPercentLoad",              0x00020035 },
	{ "PowercomTemperature",              0x00020036 },
	{ "PowercomDelayBeforeStartup",       0x00020056 },
	{ "PowercomDelayBeforeShutdown",      0x00020057 },
	{ "PowercomTest",                     0x00020058 },
	{ "PowercomShutdownRequested",        0x00020068 },
	{ "PowercomInternalChargeController", 0x00020081 },
	{ "PowercomPrimaryBatterySupport",    0x00020082 },
	{ "PowercomDesignCapacity",           0x00020083 },
	{ "PowercomSpecificationInfo",        0x00020084 },
	{ "PowercomManufacturerDate",         0x00020085 },
	{ "PowercomSerialNumber",             0x00020086 },
	{ "PowercomManufacturerName",         0x00020087 },
	{ "POWERCOM1",	0x0084002f },
	{ "POWERCOM2",	0xff860060 },
	{ "POWERCOM3",	0xff860080 },
	{ "PCMDelayBeforeStartup",	0x00ff0056 },
	{ "PCMDelayBeforeShutdown",	0x00ff0057 },
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
 * According to the HID PDC specifications, the below values should report battery.voltage(.nominal)
 * PowerCOM duplicates the output.voltage(.nominal) here, so we ignore them
 *	{ "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.2f", 0, NULL },
 *	{ "battery.voltage", 0, 0, "UPS.Battery.Voltage", NULL, "%.2f", 0, NULL },
 *	{ "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
 *	{ "battery.voltage.nominal", 0, 0, "UPS.Battery.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
 */
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.charge", 0, 0, "UPS.Battery.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.charge.low", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", 0, NULL },
	{ "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", 0, NULL },
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
	{ "battery.date", 0, 0, "UPS.Battery.ManufacturerDate", NULL, "%s", HU_FLAG_STATIC, date_conversion },
	{ "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
/*	{ "unmapped.ups.battery.delaybeforestartup", 0, 0, "UPS.Battery.DelayBeforeStartup", NULL, "%.0f", 0, NULL }, */
/*	{ "unmapped.ups.battery.initialized", 0, 0, "UPS.Battery.Initialized", NULL, "%.0f", 0, NULL }, */

	{ "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", 0, powercom_beeper_info },
	{ "ups.beeper.status", 0, 0, "UPS.AudibleAlarmControl", NULL, "%s", 0, powercom_beeper_info },
	{ "ups.load", 0, 0, "UPS.Output.PercentLoad", NULL, "%.0f", 0, NULL },
	{ "ups.date", 0, 0, "UPS.PowerSummary.ManufacturerDate", NULL, "%s", HU_FLAG_STATIC, date_conversion },
	{ "ups.test.result", 0, 0, "UPS.Battery.Test", NULL, "%s", 0, test_read_info },
/*	{ "unmapped.ups.powersummary.imanufacturer", 0, 0, "UPS.PowerSummary.iManufacturer", NULL, "%s", HU_FLAG_STATIC, stringid_conversion }, */
/*	{ "unmapped.ups.powersummary.iproduct", 0, 0, "UPS.PowerSummary.iProduct", NULL, "%s", HU_FLAG_STATIC, stringid_conversion }, */
/*	{ "unmapped.ups.powersummary.iserialnumber", 0, 0, "UPS.PowerSummary.iSerialNumber", NULL, "%s", HU_FLAG_STATIC, stringid_conversion }, */
/*	{ "unmapped.ups.iname", 0, 0, "UPS.iName", NULL, "%s", HU_FLAG_STATIC, stringid_conversion }, */
/*	{ "unmapped.ups.powersummary.ioeminformation", 0, 0, "UPS.PowerSummary.iOEMInformation", NULL, "%s", HU_FLAG_STATIC, stringid_conversion }, */

/* The implementation of the HID path UPS.PowerSummary.DelayBeforeStartup is unconventional:
 * Read:
 *	Byte 7, byte 8 (min)
 * Write:
 *	Command 4, high byte min, low byte min
 */
	{ "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 8, "UPS.PowerSummary.DelayBeforeStartup", NULL, "60", HU_FLAG_ABSENT, NULL },
	{ "ups.timer.start", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, "%.0f", 0, powercom_startup_info },

/* The implementation of the HID path UPS.PowerSummary.DelayBeforeShutdown is unconventional:
 * Read:
 *	Byte 13, Byte 14 (min, sec)
 * Write:
 *	If Byte(sec), bit7=0 and bit6=0 Then
 *		If Byte 9, bit0=1 Then command 185, 188, min, sec (OL -> shutdown.return)
 *		If Byte 9, bit0=0 Then command 186, 188, min, sec (OB -> shutdown.stayoff)
 *	If Byte(sec), bit7=0 and bit6=1
 *		Then command 185, 188, min, sec (shutdown.return)
 *	If Byte(sec), bit7=1 and bit6=0 Then
 *		Then command 186, 188, min, sec (shutdown.stayoff)
 *	If Byte(sec), bit7=1 and bit6=1 Then
 *		No actions
 */
	{ "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 8, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL },
	{ "ups.timer.shutdown", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, powercom_shutdown_info },
	{ "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 8, "UPS.PowerSummary.PCMDelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL },
	{ "ups.timer.shutdown", 0, 0, "UPS.PowerSummary.PCMDelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, powercom_shutdown_info },

	{ "input.voltage", 0, 0, "UPS.Input.Voltage", NULL, "%.1f", 0, NULL },
	{ "input.voltage.nominal", 0, 0, "UPS.Input.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.frequency", 0, 0, "UPS.Input.Frequency", NULL, "%.1f", 0, NULL },

	{ "output.voltage", 0, 0, "UPS.Output.Voltage", NULL, "%.1f", 0, NULL },
	{ "output.voltage.nominal", 0, 0, "UPS.Output.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "output.frequency", 0, 0, "UPS.Output.Frequency", NULL, "%.1f", 0, NULL },

/*	{ "unmapped.ups.powercom1", 0, 0, "UPS.POWERCOM1", NULL, "%.0f", 0, NULL }, broken pipe */
/*	{ "unmapped.ups.powercom2", 0, 0, "UPS.POWERCOM2", NULL, "%.0f", 0, NULL }, broken pipe */
/* 	{ "unmapped.ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", 0, NULL }, */
/*	{ "unmapped.ups.shutdownimminent", 0, 0, "UPS.ShutdownImminent", NULL, "%.0f", 0, NULL }, */

	/* instcmds */
	{ "beeper.toggle", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "1", HU_TYPE_CMD, NULL },
	{ "beeper.enable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "1", HU_TYPE_CMD, NULL },
	{ "beeper.disable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "0", HU_TYPE_CMD, NULL },
	{ "test.battery.start.quick", 0, 0, "UPS.Battery.Test", NULL, "1", HU_TYPE_CMD, NULL },
	{ "load.on.delay", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, NULL, HU_TYPE_CMD, powercom_startup_info },
	{ "shutdown.return", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, NULL, HU_TYPE_CMD, powercom_shutdown_info },
	{ "shutdown.stayoff", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, NULL, HU_TYPE_CMD, powercom_stayoff_info },
	{ "load.on", 0, 0, "UPS.PowerSummary.PCMDelayBeforeStartup", NULL, "0", HU_TYPE_CMD, powercom_startup_info },
	{ "load.off", 0, 0, "UPS.PowerSummary.PCMDelayBeforeShutdown", NULL, "0", HU_TYPE_CMD, powercom_stayoff_info },
	{ "shutdown.return", 0, 0, "UPS.PowerSummary.PCMDelayBeforeShutdown", NULL, NULL, HU_TYPE_CMD, powercom_shutdown_info },
	{ "shutdown.stayoff", 0, 0, "UPS.PowerSummary.PCMDelayBeforeShutdown", NULL, NULL, HU_TYPE_CMD, powercom_stayoff_info },

	{ "ups.serial", 0, 0, "PowercomUPS.PowercomSerialNumber", NULL, "%s", 0, stringid_conversion },
	{ "ups.mfr", 0, 0, "PowercomUPS.PowercomManufacturerName", NULL, "%s", 0, stringid_conversion },
/*	{ "UPS.DesignCapacity", 0, 0, "PowercomUPS.PowercomDesignCapacity", NULL, "%.0f", 0, NULL }, is always 255 */
	{ "ups.mfr.date", 0, 0, "PowercomUPS.PowercomManufacturerDate", NULL, "%s", 0, date_conversion },
	{ "battery.temperature", 0, 0, "PowercomUPS.PowercomBatterySystem.PowercomTemperature", NULL, "%.0f", 0, NULL },
	{ "battery.temperature", 0, 0, "UPS.Battery.Temperature", NULL, "%.1f", 0, NULL },	
	{ "battery.charge", 0, 0, "PowercomUPS.PowercomBatterySystem.PowercomVoltage", NULL, "%.0f", 0, NULL },
/*	{ "UPS.BatterySystem.SpecificationInfo", 0, 0, "PowercomUPS.PowercomBatterySystem.PowercomSpecificationInfo", NULL, "%.0f", 0, NULL }, */
	{ "input.frequency", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomInput.PowercomFrequency", NULL, "%.0f", 0, NULL },
	{ "input.voltage", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomInput.PowercomVoltage", NULL, "%.0f", 0, powercom_voltage_conversion },
	{ "output.voltage", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomVoltage", NULL, "%.0f", 0, powercom_voltage_conversion },
	{ "ups.load", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomPercentLoad", NULL, "%.0f", 0, NULL },
	/* flags: 4 - Testing, 8 - Probably mute (it's set on battery with muted beeper and sometimes during ups test)
	 * bit 0  UPS fault  (1 = FAILT)
	 * bit 1  Battery status (1 = BAD, 0 = NORMAL)
	 * bit 2  Test  mode  (1 = TEST, 0 = NORMAL)
	 * bit 3  X
	 * bit 4  Pre-SD count mode (1 = ACTIVE)
	 * bit 5  Schedule count mode (1 = ACTIVE)
	 * bit 6  Disable NO LOAD SHUTDOWN (1 = ACTIVE)
	 * bit 7  0
	 */
/*	{ "UPS.PowerConverter.Output.InternalChargeController", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomInternalChargeController", NULL, "%.0f", 0, NULL }, */
	{ "BOOL", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomInternalChargeController", NULL, NULL, HU_FLAG_QUICK_POLL, powercom_upsfail_conversion },
	{ "BOOL", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomInternalChargeController", NULL, NULL, HU_FLAG_QUICK_POLL, powercom_replacebatt_conversion },
	{ "BOOL", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomInternalChargeController", NULL, NULL, HU_FLAG_QUICK_POLL, powercom_test_conversion },
	{ "BOOL", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomInternalChargeController", NULL, NULL, HU_FLAG_QUICK_POLL, powercom_shutdownimm_conversion },
	/* flags: 1 - On battery, 2 - Low Battery, 8 - Trim, 8+16 - Boost
	 * bit 0  is line fail (1 = INV, 0 = LINE)
	 * bit 1  is low battery (1 = BAT_ LOW, 0 = NORMAL)
	 * bit 2  X
	 * bit 3  AVR status (1 = AVR, 0 = NO_AVR)
	 * bit 4  AVR mode (1 = BOOST, 0 = BUCK)
	 * bit 5  Load status (1 = OVER LOAD, 0 = NORMAL)
	 * bit 6  X
	 * bit 7  SD mode display
	 */
/*	{ "UPS.PowerConverter.Output.PrimaryBatterySupport", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomPrimaryBatterySupport", NULL, "%.0f", 0, NULL }, */
	{ "BOOL", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomPrimaryBatterySupport", NULL, NULL, HU_FLAG_QUICK_POLL, powercom_online_conversion },
	/* Low battery status may not work */
	{ "BOOL", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomPrimaryBatterySupport", NULL, NULL, HU_FLAG_QUICK_POLL, powercom_lowbatt_conversion },
	{ "BOOL", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomPrimaryBatterySupport", NULL, NULL, HU_FLAG_QUICK_POLL, powercom_trim_conversion },
	{ "BOOL", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomPrimaryBatterySupport", NULL, NULL, HU_FLAG_QUICK_POLL, powercom_boost_conversion },
	{ "BOOL", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomPrimaryBatterySupport", NULL, NULL, HU_FLAG_QUICK_POLL, powercom_overload_conversion },
	{ "output.frequency", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomOutput.PowercomFrequency", NULL, "%.0f", 0, NULL },
	{ "ups.test.result", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomTest", NULL, "%s", 0, test_read_info },
/*	{ "UPS.PowerConverter.ShutdownRequested", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomShutdownRequested", NULL, "%.0f", 0, NULL }, */
	{ "ups.delay.shutdown", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomDelayBeforeShutdown", NULL, "%.0f", 0, NULL },
	{ "ups.delay.start", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomDelayBeforeStartup", NULL, "%.0f", 0, NULL },
	{ "load.off", 0, 0, "PowercomUPS.PowercomPowerConverter.PowercomDelayBeforeShutdown", NULL, "0", HU_TYPE_CMD, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *powercom_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *powercom_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "PowerCOM";
}

static const char *powercom_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int powercom_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(powercom_usb_device_table, hd);

	switch (status)
	{
	case POSSIBLY_SUPPORTED:
		if (hd->ProductID == 0x0002) {
			upsdebugx(0,
				"This Powercom device (%04x/%04x) is not supported by usbhid-ups.\n"
				"Please use the 'powercom' driver instead.\n", hd->VendorID, hd->ProductID);
			return 0;
		}
		/* by default, reject, unless the productid option is given */
		if (getval("productid")) {
			return 1;
		}
		possibly_supported("PowerCOM", hd);
		return 0;

	case SUPPORTED:
		if (hd->ProductID == 0x0001) {
			interrupt_only = 1;
			interrupt_size = 8;
		}
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
