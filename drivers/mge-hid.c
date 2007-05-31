/*  mge-hid.c - data to monitor MGE UPS SYSTEMS HID (USB and serial) devices
 *
 *  Copyright (C) 2003 - 2005
 *  			Arnaud Quette <arnaud.quette@mgeups.fr>
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
#include "mge-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "dstate.h"   /* for STAT_INSTCMD_HANDLED */
#include "main.h"     /* for getval() */
#include "common.h"

#define MGE_HID_VERSION	"MGE HID 1.0"

#define MGE_VENDORID 0x0463

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* MGE UPS SYSTEMS usage table */
static usage_lkp_t mge_usage_lkp[] = {
	{ "Undefined",				0xffff0000 },
	{ "STS",				0xffff0001 },
	{ "Environment",			0xffff0002 },
	/* 0xffff0003-0xffff000f	=>	Reserved */
	{ "Phase",				0xffff0010 },
	{ "PhaseID",				0xffff0011 },
	{ "Chopper",				0xffff0012 },
	{ "ChopperID",				0xffff0013 },
	{ "Inverter",				0xffff0014 },
	{ "InverterID",				0xffff0015 },
	{ "Rectifier",				0xffff0016 },
	{ "RectifierID",			0xffff0017 },
	{ "LCMSystem",				0xffff0018 },
	{ "LCMSystemID",			0xffff0019 },
	{ "LCMAlarm",				0xffff001a },
	{ "LCMAlarmID",				0xffff001b },
	{ "HistorySystem",			0xffff001c },
	{ "HistorySystemID",			0xffff001d },
	{ "Event",				0xffff001e },
	{ "EventID",				0xffff001f },
	{ "CircuitBreaker",			0xffff0020 },
	{ "TransferForbidden",			0xffff0021 },
	{ "OverallAlarm",			0xffff0022 },
	{ "Dephasing",				0xffff0023 },
	{ "BypassBreaker",			0xffff0024 },
	{ "PowerModule",			0xffff0025 },
	{ "PowerRate",				0xffff0026 },
	{ "PowerSource",			0xffff0027 },
	{ "CurrentPowerSource",			0xffff0028 },
	{ "RedundancyLevel",			0xffff0029 },
	{ "RedundancyLost",			0xffff002a },
	{ "NotificationStatus",			0xffff002b },
	/* 0xffff002c-0xffff003f	=>	Reserved */
	{ "SwitchType",				0xffff0040 },
	{ "ConverterType",			0xffff0041 },
	{ "FrequencyConverterMode",		0xffff0042 },
	{ "AutomaticRestart",			0xffff0043 },
	{ "ForcedReboot",			0xffff0044 },
	{ "TestPeriod",				0xffff0045 },
	{ "EnergySaving",			0xffff0046 },
	{ "StartOnBattery",			0xffff0047 },
	{ "Schedule",				0xffff0048 },
	{ "DeepDischargeProtection",		0xffff0049 },
	{ "ShortCircuit",			0xffff004a },
	{ "ExtendedVoltageMode",		0xffff004b },
	{ "SensitivityMode",			0xffff004c },
	{ "RemainingCapacityLimitSetting",	0xffff004d },
	{ "ExtendedFrequencyMode",		0xffff004e },
	{ "FrequencyConverterModeSetting",	0xffff004f },
	{ "LowVoltageBoostTransfer",		0xffff0050 },
	{ "HighVoltageBoostTransfer",		0xffff0051 },
	{ "LowVoltageBuckTransfer",		0xffff0052 },
	{ "HighVoltageBuckTransfer",		0xffff0053 },
	{ "OverloadTransferEnable",		0xffff0054 },
	{ "OutOfToleranceTransferEnable",	0xffff0055 },
	{ "ForcedTransferEnable",		0xffff0056 },
	{ "LowVoltageBypassTransfer",		0xffff0057 },
	{ "HighVoltageBypassTransfer",		0xffff0058 },
	{ "FrequencyRangeBypassTransfer",	0xffff0059 },
	{ "LowVoltageEcoTransfer",		0xffff005a },
	{ "HighVoltageEcoTransfer",		0xffff005b },
	{ "FrequencyRangeEcoTransfer",		0xffff005c },
	{ "ShutdownTimer",			0xffff005d },
	{ "StartupTimer",			0xffff005e },
	{ "RestartLevel",			0xffff005f },
	{ "PhaseOutOfRange", 			0xffff0060 },
	{ "CurrentLimitation", 			0xffff0061 },
	{ "ThermalOverload", 			0xffff0062 },
	{ "SynchroSource", 			0xffff0063 },
	{ "FuseFault", 				0xffff0064 },
	{ "ExternalProtectedTransfert", 	0xffff0065 },
	{ "ExternalForcedTransfert", 		0xffff0066 },
	{ "Compensation", 			0xffff0067 },
	{ "EmergencyStop", 			0xffff0068 },
	{ "PowerFactor", 			0xffff0069 },
	{ "PeakFactor", 			0xffff006a },
	{ "ChargerType", 			0xffff006b },
	{ "HighPositiveDCBusVoltage", 		0xffff006c },
	{ "LowPositiveDCBusVoltage", 		0xffff006d },
	{ "HighNegativeDCBusVoltage", 		0xffff006e },
	{ "LowNegativeDCBusVoltage", 		0xffff006f },
	{ "FrequencyRangeTransfer", 		0xffff0070 },
	{ "WiringFaultDetection", 		0xffff0071 },
	{ "ControlStandby", 			0xffff0072 },
	{ "ShortCircuitTolerance", 		0xffff0073 },
	{ "VoltageTooHigh", 			0xffff0074 },
	{ "VoltageTooLow", 			0xffff0075 },
	{ "DCBusUnbalanced", 			0xffff0076 },
	{ "FanFailure", 			0xffff0077 },
	{ "WiringFault", 			0xffff0078 },
	/* 0xffff0079-0xffff007f	=>	Reserved */
	{ "Sensor",				0xffff0080 },
	{ "LowHumidity",			0xffff0081 },
	{ "HighHumidity",			0xffff0082 },
	{ "LowTemperature",			0xffff0083 },
	{ "HighTemperature",			0xffff0084 },
	/* 0xffff0085-0xffff008f	=>	Reserved */
	{ "Count",				0xffff0090 },
	{ "Timer",				0xffff0091 },
	{ "Interval",				0xffff0092 },
	{ "TimerExpired",			0xffff0093 },
	{ "Mode",				0xffff0094 },
	{ "Country",				0xffff0095 },
	{ "State",				0xffff0096 },
	{ "Time",				0xffff0097 },
	{ "Code",				0xffff0098 },
	{ "DataValid",				0xffff0099 },
	/* 0xffff009a-0xffff00df	=>	Reserved */
	{ "COPIBridge",				0xffff00e0 },
	/* 0xffff00e1-0xffff00ef	=>	Reserved */
	{ "iModel",				0xffff00f0 },
	{ "iVersion",				0xffff00f1 },
	/* 0xffff00f2-0xffff00ff	=>	Reserved */
	/* MGE indexed collections */
	{ "[1]", 				0x00ff0001 },
	{ "[2]",				0x00ff0002 },
	{ "[3]",				0x00ff0003 },
	{ "[4]",				0x00ff0004 },
	/* end of table */
	{  "\0",				0x00000000 }
};

static usage_tables_t mge_utab[] = {
	mge_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/*      Model Name formating entries                               */
/* --------------------------------------------------------------- */

static models_name_t mge_model_names [] =
{
	/* Ellipse models */
	{ "ELLIPSE", "300", -1, "ellipse 300" },
	{ "ELLIPSE", "500", -1, "ellipse 500" },
	{ "ELLIPSE", "650", -1, "ellipse 650" },
	{ "ELLIPSE", "800", -1, "ellipse 800" },
	{ "ELLIPSE", "1200", -1, "ellipse 1200" },
	/* Ellipse Premium models */
	{ "ellipse", "PR500", -1, "ellipse premium 500" },
	{ "ellipse", "PR650", -1, "ellipse premium 650" },
	{ "ellipse", "PR800", -1, "ellipse premium 800" },
	{ "ellipse", "PR1200", -1, "ellipse premium 1200" },
	/* Ellipse "Pro" */
	{ "ELLIPSE", "600", -1, "Ellipse 600" },
	{ "ELLIPSE", "750", -1, "Ellipse 750" },
	{ "ELLIPSE", "1000", -1, "Ellipse 1000" },
	{ "ELLIPSE", "1500", -1, "Ellipse 1500" },
	/* Ellipse "MAX" */
	{ "Ellipse MAX", "600", -1, "Ellipse MAX 600" },
	{ "Ellipse MAX", "850", -1, "Ellipse MAX 850" },
	{ "Ellipse MAX", "1100", -1, "Ellipse MAX 1100" },
	{ "Ellipse MAX", "1500", -1, "Ellipse MAX 1500" },
	/* Protection Center */
	{ "PROTECTIONCENTER", "420", -1, "Protection Center 420" },
	{ "PROTECTIONCENTER", "500", -1, "Protection Center 500" },
	{ "PROTECTIONCENTER", "675", -1, "Protection Center 675" },
	/* Evolution models */
	{ "Evolution", "500", -1, "Pulsar Evolution 500" },
	{ "Evolution", "800", -1, "Pulsar Evolution 800" },
	{ "Evolution", "1100", -1, "Pulsar Evolution 1100" },
	{ "Evolution", "1500", -1, "Pulsar Evolution 1500" },
	{ "Evolution", "2200", -1, "Pulsar Evolution 2200" },
	{ "Evolution", "3000", -1, "Pulsar Evolution 3000" },
	{ "Evolution", "3000XL", -1, "Pulsar Evolution 3000 XL" },
	/* Newer Evolution models */
	{ "Evolution", "650", -1, "Evolution 650" },
	{ "Evolution", "850", -1, "Evolution 850" },
	{ "Evolution", "1150", -1, "Evolution 1150" },
	{ "Evolution", "S 1250", -1, "Evolution S 1250" },
	{ "Evolution", "1550", -1, "Evolution 1550" },
	{ "Evolution", "S 1750", -1, "Evolution S 1750" },
	{ "Evolution", "2000", -1, "Evolution 2000" },
	{ "Evolution", "S 2500", -1, "Evolution S 2500" },
	{ "Evolution", "S 3000", -1, "Evolution S 3000" },
	/* Pulsar M models */
	{ "PULSAR M", "2200", -1, "Pulsar M 2200" },
	{ "PULSAR M", "3000", -1, "Pulsar M 3000" },
	{ "PULSAR M", "3000 XL", -1, "Pulsar M 3000 XL" },
	/* Pulsar models */
	{ "Pulsar", "700", -1, "Pulsar 700" },
	{ "Pulsar", "1000", -1, "Pulsar 1000" },
	{ "Pulsar", "1500", -1, "Pulsar 1500" },
	{ "Pulsar", "1000 RT2U", -1, "Pulsar 1000 RT2U" },
	{ "Pulsar", "1500 RT2U", -1, "Pulsar 1500 RT2U" },
	/* Pulsar MX models */
	{ "PULSAR", "MX4000", -1, "Pulsar MX 4000 RT" },
	{ "PULSAR", "MX5000", -1, "Pulsar MX 5000 RT" },
	/* NOVA models */	
	{ "NOVA AVR", "600", -1, "NOVA 600 AVR" },
	{ "NOVA AVR", "1100", -1, "NOVA 1100 AVR" },
	/* EXtreme C (EMEA) */
	{ "EXtreme", "700C", -1, "Pulsar EXtreme 700C" },
	{ "EXtreme", "1000C", -1, "Pulsar EXtreme 1000C" },
	{ "EXtreme", "1500C", -1, "Pulsar EXtreme 1500C" },
	{ "EXtreme", "1500CCLA", -1, "Pulsar EXtreme 1500C CLA" },
	{ "EXtreme", "2200C", -1, "Pulsar EXtreme 2200C" },
	{ "EXtreme", "3200C", -1, "Pulsar EXtreme 3200C" },
	/* EXtreme C (USA, aka "EX RT") */
	{ "EX", "700RT", -1, "Pulsar EX 700 RT" },
	{ "EX", "1000RT", -1, "Pulsar EX 1000 RT" },
	{ "EX", "1500RT", -1, "Pulsar EX 1500 RT" },
	{ "EX", "2200RT", -1, "Pulsar EX 2200 RT" },
	{ "EX", "3200RT", -1, "Pulsar EX 3200 RT" },
	/* Comet EX RT three phased */
	{ "EX", "5RT31", -1, "EX 5 RT 3:1" },
	{ "EX", "7RT31", -1, "EX 7 RT 3:1" },
	{ "EX", "11RT31", -1, "EX 11 RT 3:1" },
	/* Comet EX RT mono phased */
	{ "EX", "5RT", -1, "EX 5 RT" },
	{ "EX", "7RT", -1, "EX 7 RT" },
	{ "EX", "11RT", -1, "EX 11 RT" },
	/* Galaxy 3000 */
	{ "GALAXY", "3000_10", -1, "Galaxy 3000 10 kVA" },
	{ "GALAXY", "3000_15", -1, "Galaxy 3000 15 kVA" },
	{ "GALAXY", "3000_20", -1, "Galaxy 3000 20 kVA" },
	{ "GALAXY", "3000_30", -1, "Galaxy 3000 30 kVA" },

	/* FIXME: To be completed (Comet, Galaxy, Esprit, ...) */

	/* end of structure. */
	{ NULL, NULL, -1, "Generic MGE HID model" }
};

/* --------------------------------------------------------------- */
/*                 Data lookup table (HID <-> NUT)                 */
/* --------------------------------------------------------------- */

static hid_info_t mge_hid2nut[] =
{
	/* Server side variables */
	{ "driver.version.internal", ST_FLAG_STRING, 5, NULL, NULL,
		DRIVER_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
	{ "driver.version.data", ST_FLAG_STRING, 11, NULL, NULL,
		MGE_HID_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },

	/* Battery page */
	{ "battery.charge", 0, 1, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 5, 
	"UPS.PowerSummary.RemainingCapacityLimitSetting", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.charge.low", ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimit", NULL,
	"%.0f", HU_FLAG_OK | HU_FLAG_STATIC , NULL }, /* Read only */
	{ "battery.charge.restart", ST_FLAG_RW | ST_FLAG_STRING, 3,
	"UPS.PowerSummary.RestartLevel", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "battery.temperature", 0, 0, 
		"UPS.BatterySystem.Battery.Temperature", NULL, "%.1f", HU_FLAG_OK, NULL },
	{ "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_OK, stringid_conversion },
	{ "battery.voltage",  0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
	{ "battery.voltage.nominal", 0, 0, "UPS.BatterySystem.ConfigVoltage", NULL,
		"%.1f", HU_FLAG_OK, NULL },

	/* UPS page */
	{ "ups.load", 0, 1, "UPS.PowerSummary.PercentLoad", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5,
		"UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL},
	{ "ups.delay.reboot", ST_FLAG_RW | ST_FLAG_STRING, 5,
		"UPS.PowerSummary.DelayBeforeReboot", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL},
	{ "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5,
		"UPS.PowerSummary.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL},
	{ "ups.test.result", 0, 0,
		"UPS.BatterySystem.Battery.Test", NULL, "%s", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, &test_read_info[0] },
	{ "ups.test.interval", ST_FLAG_RW | ST_FLAG_STRING, 8,
		"UPS.BatterySystem.Battery.TestPeriod", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
	{ "ups.beeper.status", 0, 1, 
	"UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, &beeper_info[0] },
	{ "ups.temperature", 0, 0, 
		"UPS.PowerSummary.Temperature", NULL, "%.1f", HU_FLAG_OK, NULL },
	/* FIXME: miss ups.power */
	{ "ups.power.nominal", ST_FLAG_STRING, 5, "UPS.Flow.[4].ConfigApparentPower",
		NULL, "%.0f",HU_FLAG_OK, NULL },

	/* Special case: ups.status */
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, 
		"%.0f", HU_FLAG_OK | HU_FLAG_QUICK_POLL, &online_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Discharging", NULL, 
		"%.0f", HU_FLAG_OK | HU_FLAG_QUICK_POLL, &discharging_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Charging", NULL, 
		"%.0f", HU_FLAG_OK | HU_FLAG_QUICK_POLL, &charging_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL,
		"%.0f", HU_FLAG_OK | HU_FLAG_QUICK_POLL, &shutdownimm_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL,
		"%.0f", HU_FLAG_OK | HU_FLAG_QUICK_POLL, &lowbatt_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Overload", NULL,
		"%.0f", HU_FLAG_OK, &overload_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL,
		"%.0f", HU_FLAG_OK, &replacebatt_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerConverter.Input.[1].PresentStatus.Buck", NULL,
		"%.0f", HU_FLAG_OK, &trim_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerConverter.Input.[1].PresentStatus.Boost", NULL,
		"%.0f", HU_FLAG_OK, &boost_info[0] },
	{ "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Good", NULL,
		"%.0f", HU_FLAG_OK, &off_info[0] },  
	/* FIXME: extend ups.status for BYPASS: */
	/* Manual bypass */
	{ "ups.status", 0, 1, "UPS.PowerConverter.Input[4].PresentStatus.Used", NULL,
		"%.0f", HU_FLAG_OK, &bypass_info[0] },
	/* Automatic bypass */
	{ "ups.status", 0, 1, "UPS.PowerConverter.Input[2].PresentStatus.Used", NULL,
		"%.0f", HU_FLAG_OK, &bypass_info[0] },

	/* Input page */
	{ "input.voltage", 0, 0, "UPS.PowerConverter.Input.[1].Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
	{ "input.frequency", 0, 0, "UPS.PowerConverter.Input.[1].Frequency", NULL, "%.1f", HU_FLAG_OK, NULL },
	/* same as "input.transfer.boost.low" */
	{ "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 5,
		"UPS.PowerConverter.Output.LowVoltageTransfer", NULL, "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
	{ "input.transfer.boost.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerConverter.Output.LowVoltageBoostTransfer",NULL, "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
	{ "input.transfer.boost.high", ST_FLAG_RW | ST_FLAG_STRING, 5,
		"UPS.PowerConverter.Output.HighVoltageBoostTransfer", NULL, "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
	{ "input.transfer.trim.low", ST_FLAG_RW | ST_FLAG_STRING, 5,
		"UPS.PowerConverter.Output.LowVoltageBuckTransfer", NULL, "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
	/* same as "input.transfer.trim.high" */
	{ "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 5,
		"UPS.PowerConverter.Output.HighVoltageTransfer", NULL, "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
	{ "input.transfer.trim.high", ST_FLAG_RW | ST_FLAG_STRING, 5,
		"UPS.PowerConverter.Output.HighVoltageBuckTransfer", NULL, "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },

	/* Output page */
	{ "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
	{ "output.current", 0, 0, "UPS.PowerConverter.Output.Current", NULL, "%.2f", HU_FLAG_OK, NULL },
	{ "output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", NULL, "%.1f", HU_FLAG_OK, NULL },
	{ "output.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.1f", HU_FLAG_OK, NULL },

	/* Outlet page (using MGE UPS SYSTEMS - PowerShare technology) */
	/* TODO: add an iterative semantic [%x] to factorise outlets */
	{ "outlet.0.id", 0, 0, "UPS.OutletSystem.Outlet.[1].OutletID",
		NULL, "%.0f", HU_FLAG_OK | HU_FLAG_STATIC, NULL },
	{ "outlet.0.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "UPS.OutletSystem.Outlet.[1].OutletID",
		NULL, "Main Outlet", HU_FLAG_ABSENT | HU_FLAG_OK | HU_FLAG_STATIC, NULL },
	{ "outlet.0.switchable", 0, 0, "UPS.OutletSystem.Outlet.[1].PresentStatus.Switchable",
		NULL, "%s", HU_FLAG_OK | HU_FLAG_STATIC, &yes_no_info[0] },
	{ "outlet.1.id", 0, 0, "UPS.OutletSystem.Outlet.[2].OutletID",
		NULL, "%.0f", HU_FLAG_OK | HU_FLAG_STATIC, NULL },	
	{ "outlet.1.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "UPS.OutletSystem.Outlet.[2].OutletID",
		NULL, "PowerShare Outlet 1", HU_FLAG_ABSENT | HU_FLAG_OK | HU_FLAG_STATIC, NULL },
	{ "outlet.1.switchable", 0, 0, "UPS.OutletSystem.Outlet.[2].PresentStatus.Switchable",
	NULL, "%s", HU_FLAG_OK | HU_FLAG_STATIC, &yes_no_info[0] },
	{ "outlet.1.status", ST_FLAG_STRING, 3, "UPS.OutletSystem.Outlet.[2].PresentStatus.SwitchOn/Off",
	NULL, "%s", HU_FLAG_OK, &on_off_info[0] },
	/* For low end models, with 1 non backup'ed outlet */
	{ "outlet.1.status", ST_FLAG_STRING, 3, "UPS.PowerSummary.PresentStatus.ACPresent",
	NULL, "%s", HU_FLAG_OK, &on_off_info[0] },
	{ "outlet.1.autoswitch.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 3,
	  "UPS.OutletSystem.Outlet.[2].RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
	/* FIXME: use UPS.OutletSystem.Outlet.[x].ShutdownTimer */
	{ "outlet.1.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5, 
	"UPS.OutletSystem.Outlet.[2].ShutdownTimer", NULL, "%.0f", HU_FLAG_OK, NULL },
	/* FIXME: use UPS.OutletSystem.Outlet.[x].StartupTimer */
	{ "outlet.1.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5,
	"UPS.OutletSystem.Outlet.[2].StartupTimer", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "outlet.2.id", 0, 0, "UPS.OutletSystem.Outlet.[3].OutletID", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_STATIC, NULL },	
	{ "outlet.2.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "UPS.OutletSystem.Outlet.[3].OutletID",
	  NULL, "PowerShare Outlet 2", HU_FLAG_ABSENT | HU_FLAG_OK | HU_FLAG_STATIC, NULL },
	{ "outlet.2.switchable", 0, 0, "UPS.OutletSystem.Outlet.[3].PresentStatus.Switchable",
	NULL, "%s", HU_FLAG_OK | HU_FLAG_STATIC, &yes_no_info[0] },
	{ "outlet.2.status", ST_FLAG_STRING, 3, "UPS.OutletSystem.Outlet.[3].PresentStatus.SwitchOn/Off",
	NULL, "%s", HU_FLAG_OK, &on_off_info[0] },
	{ "outlet.2.autoswitch.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 3,
	"UPS.OutletSystem.Outlet.[3].RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
	/* FIXME: use UPS.OutletSystem.Outlet.[x].ShutdownTimer */
	{ "outlet.2.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5,
	"UPS.OutletSystem.Outlet.[3].ShutdownTimer", NULL, "%.0f", HU_FLAG_OK, NULL },
	/* FIXME: use UPS.OutletSystem.Outlet.[x].StartupTimer */
	{ "outlet.2.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5,
	"UPS.OutletSystem.Outlet.[3].StartupTimer", NULL, "%.0f", HU_FLAG_OK, NULL },

	/* instant commands. */
	/* splited into subset while waiting for extradata support
	* ie: test.battery.start quick
	*/
	{ "test.battery.start.quick", 0, 0,
	"UPS.BatterySystem.Battery.Test", NULL, "1", /* point to good value */
	HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] }, /* TODO: lookup needed? */
	{ "test.battery.start.deep", 0, 0,
	"UPS.BatterySystem.Battery.Test", NULL, "2", /* point to good value */
	HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] },
	{ "test.battery.stop", 0, 0,
	"UPS.BatterySystem.Battery.Test", NULL, "3", /* point to good value */
	HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] },
	{ "load.off", 0, 0,
	"UPS.PowerSummary.DelayBeforeShutdown", NULL, "0", /* point to good value */
	HU_TYPE_CMD | HU_FLAG_OK, NULL },
	{ "load.on", 0, 0,
	"UPS.PowerSummary.DelayBeforeStartup", NULL, "0", /* point to good value */
	HU_TYPE_CMD | HU_FLAG_OK, NULL },
	{ "beeper.off", 0, 0,
	"UPS.PowerSummary.AudibleAlarmControl", NULL, "1", /* point to good value */
	HU_TYPE_CMD | HU_FLAG_OK, NULL },
	{ "beeper.on", 0, 0,
	"UPS.PowerSummary.AudibleAlarmControl", NULL, "2", /* point to good value */
	HU_TYPE_CMD | HU_FLAG_OK, NULL },
	/* FIXME: add beeper.mute , value "3" */

	/* Command for the outlet collection */
	/* FIXME: not existing in new-names.txt => complete it or use "load.off {all, outletX}" ? */
	{ "outlet.1.load.off", 0, 0, "UPS.OutletSystem.Outlet.[2].DelayBeforeShutdown",
	NULL, "0", HU_TYPE_CMD | HU_FLAG_OK, NULL },
	{ "outlet.1.load.on", 0, 0, "UPS.OutletSystem.Outlet.[2].DelayBeforeStartup",
	NULL, "0", HU_TYPE_CMD | HU_FLAG_OK, NULL },
	{ "outlet.2.load.off", 0, 0, "UPS.OutletSystem.Outlet.[3].DelayBeforeShutdown",
	NULL, "0", HU_TYPE_CMD | HU_FLAG_OK, NULL },
	{ "outlet.2.load.on", 0, 0, "UPS.OutletSystem.Outlet.[3].DelayBeforeStartup",
	NULL, "0", HU_TYPE_CMD | HU_FLAG_OK, NULL },

  /* TODO: bypass.start/stop, shutdown.return/stayoff/stop/reboot[.graceful] */

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

/* shutdown method for MGE */
static int mge_shutdown(int ondelay, int offdelay) {
	char delay[7];

	/* 1) set DelayBeforeStartup */
	sprintf(delay, "%i", ondelay);
	if (setvar("ups.delay.start", delay) != STAT_SET_HANDLED) {
		upsdebugx(2, "Shutoff command failed (setting ondelay)");
		return 0;
	}	

	/* 2) set DelayBeforeShutdown */
	sprintf(delay, "%i", offdelay);
	if (setvar("ups.delay.shutdown", delay) == STAT_SET_HANDLED) {
		return 1;
	}
	upsdebugx(2, "Shutoff command failed (setting offdelay)");
	return 0;
}

/* All the logic for finely formatting the MGE model name */
static char *get_model_name(const char *iProduct, char *iModel)
{
  models_name_t *model = NULL;

  upsdebugx(2, "get_model_name(%s, %s)\n", iProduct, iModel);

  /* Search for formatting rules */
  for ( model = mge_model_names ; model->iProduct != NULL ; model++ )
	{
	  upsdebugx(2, "comparing with: %s", model->finalname);
	  /* FIXME: use comp_size if not -1 */
	  if ( (!strncmp(iProduct, model->iProduct, strlen(model->iProduct)))
		   && (!strncmp(iModel, model->iModel, strlen(model->iModel))) )
		{
		  upsdebugx(2, "Found %s\n", model->finalname);
		  break;
		}
	}
  /* FIXME: if we end up with model->iProduct == NULL
   * then process name in a generic way (not yet supported models!)
   * Will the following do?
   */
  if (model->iProduct == NULL) {
	  return iModel;
  }
  return model->finalname;
}

static char *mge_format_model(HIDDevice_t *hd) {
	char *product;
	char *model;
        char *string;
	float appPower;
	unsigned char rawbuf[100];

	/* Get iModel and iProduct strings */
	product = hd->Product ? hd->Product : "unknown";
	if ((string = HIDGetItemString(udev, "UPS.PowerSummary.iModel", rawbuf, mge_utab)) != NULL)
		model = get_model_name(product, string);
	else
	{
		/* Try with ConfigApparentPower */
		if (HIDGetItemValue(udev, "UPS.Flow.[4].ConfigApparentPower", &appPower, mge_utab) != 0 )
		{
			string = xmalloc(16);
			sprintf(string, "%i", (int)appPower);
			model = get_model_name(product, string);
			free (string);
		}
		else
			model = product;
	}
	return model;
}

static char *mge_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "MGE UPS SYSTEMS";
}

static char *mge_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int mge_claim(HIDDevice_t *hd) {
	if (hd->VendorID != MGE_VENDORID) {
		return 0;
	}
	switch (hd->ProductID) {

	case  0x0001:
	case  0xffff:
		return 1;  /* accept known UPSs */

	default:
		if (getval("productid")) {
			return 1;
		} else {
			upsdebugx(1,
"This MGE device (%04x/%04x) is not (or perhaps not yet) supported\n"
"by usbhid-ups. Please make sure you have an up-to-date version of NUT. If\n"
"this does not fix the problem, try running the driver with the\n"
"'-x productid=%04x' option. Please report your results to the NUT user's\n"
"mailing list <nut-upsuser@lists.alioth.debian.org>.\n",
						 hd->VendorID, hd->ProductID, hd->ProductID);
			return 0;
		}
	}
}

subdriver_t mge_subdriver = {
	MGE_HID_VERSION,
	mge_claim,
	mge_utab,
	mge_hid2nut,
	mge_shutdown,
	mge_format_model,
	mge_format_mfr,
	mge_format_serial,
};
