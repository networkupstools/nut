/* apcmicrolink-maps.c - APC Microlink descriptor maps
 *
 * Copyright (C) 2026 Lukas Schmid <lukas.schmid@netcube.li>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "main.h"
#include "apc_common.h"

/* Descriptor names and enumerations were derived from Microlink descriptors
 * published by https://ulexplorer-07aa30.gitlab.io/
 */

#include "apcmicrolink.h"
#include "apcmicrolink-maps.h"

static const microlink_value_map_t microlink_outlet_status_map[] = {
	{ (1U << 0), "StateOn" },
	{ (1U << 1), "StateOff" },
	{ (1U << 2), "ProcessReboot" },
	{ (1U << 3), "ProcessShutdown" },
	{ (1U << 4), "ProcessSleep" },
	{ (1U << 7), "PendingLoadShed" },
	{ (1U << 8), "PendingOnDelay" },
	{ (1U << 9), "PendingOffDelay" },
	{ (1U << 10), "PendingOnACPresence" },
	{ (1U << 11), "PendingOnMinRuntime" },
	{ (1U << 12), "MemberGroupProcess1" },
	{ (1U << 13), "MemberGroupProcess2" },
	{ (1U << 14), "LowRuntime" },
	{ 0, NULL }
};

const microlink_desc_publish_map_t microlink_desc_publish_map[] = {
	{ "2:4.A", apc_status_map, apc_alarm_map },
	{ "2:13", apc_calibration_status_map, NULL },
	{ NULL, NULL, NULL }
};

static const microlink_value_map_t outlet_status_change_cause_map[] = {
	{ (1UL << 0),  "SystemInitialization" },
	{ (1UL << 1),  "UPSStatusChange" },
	{ (1UL << 2),  "LocalUserCommand" },
	{ (1UL << 3),  "USBPortCommand" },
	{ (1UL << 4),  "SmartSlot1Command" },
	{ (1UL << 5),  "LoadShedCommand" },
	{ (1UL << 6),  "RJ45PortCommand" },
	{ (1UL << 7),  "ACInputBad" },
	{ (1UL << 8),  "UnknownCommand" },
	{ (1UL << 9),  "ConfigurationChange" },
	{ (1UL << 10), "SmartSlot2Command" },
	{ (1UL << 11), "InternalNetwork1Command" },
	{ (1UL << 12), "InternalNetwork2Command" },
	{ (1UL << 13), "LowRuntimeSet" },
	{ (1UL << 14), "LowRuntimeClear" },
	{ (1UL << 15), "ScheduledCommand" },
	{ (1UL << 16), "LoadRebootCommand" },
	{ (1UL << 17), "InputContactCommand" },
	{ 0, NULL }
};

static const microlink_value_map_t retransfer_delay_map[] = {
	{ 0, "NoDelay" },
	{ 0, NULL }
};

static const microlink_value_map_t output_voltage_setting_map[] = {
	{ (1UL << 0),  "VAC100" },
	{ (1UL << 1),  "VAC120" },
	{ (1UL << 2),  "VAC200" },
	{ (1UL << 3),  "VAC208" },
	{ (1UL << 4),  "VAC220" },
	{ (1UL << 5),  "VAC230" },
	{ (1UL << 6),  "VAC240" },
	{ (1UL << 7),  "VAC220_380" },
	{ (1UL << 8),  "VAC230_400" },
	{ (1UL << 9),  "VAC240_415" },
	{ (1UL << 10), "VAC277_480" },
	{ (1UL << 11), "VAC110" },
	{ (1UL << 12), "VAC127" },
	{ (1UL << 13), "VACAuto120_208or240" },
	{ (1UL << 14), "VAC120_208" },
	{ (1UL << 15), "VAC120_240" },
	{ (1UL << 16), "VAC100_200" },
	{ (1UL << 17), "VAC254_440" },
	{ (1UL << 18), "VAC115" },
	{ (1UL << 19), "VAC125" },
	{ 0, NULL }
};

static const microlink_value_map_t language_map[] = {
	{ (1U << 0),  "en" },
	{ (1U << 1),  "fr" },
	{ (1U << 2),  "it" },
	{ (1U << 3),  "de" },
	{ (1U << 4),  "es" },
	{ (1U << 5),  "pt" },
	{ (1U << 6),  "ja" },
	{ (1U << 7),  "ru" },
	{ 0, NULL }
};

static const microlink_value_map_t battery_test_interval_map[] = {
	{ (1U << 0),  "Never" },
	{ (1U << 1),  "OnStartUpOnly" },
	{ (1U << 2),  "OnStartUpPlus7" },
	{ (1U << 3),  "OnStartUpPlus14" },
	{ (1U << 4),  "OnStartUp7Since" },
	{ (1U << 5),  "OnStartUp14Since" },
	{ 0, NULL }
};

static const microlink_value_map_t battery_lifetime_status_map[] = {
	{ (1U << 0),  "LifeTimeStatusOK" },
	{ (1U << 1),  "LifeTimeNearEnd" },
	{ (1U << 2),  "LifeTimeExceeded" },
	{ (1U << 3),  "LifeTimeNearEndAcknowledged" },
	{ (1U << 4),  "LifeTimeExceededAcknowledged" },
	{ (1U << 5),  "MeasuredLifeTimeNearEnd" },
	{ (1U << 6),  "MeasuredLifeTimeNearEndAcknowledged" },
	{ 0, NULL }
};

static const microlink_value_map_t ups_status_change_cause_map[] = {
	{ 0,  "SystemInitialization" },
	{ 1,  "HighInputVoltage" },
	{ 2,  "LowInputVoltage" },
	{ 3,  "DistortedInput" },
	{ 4,  "RapidChangeOfInputVoltage" },
	{ 5,  "HighInputFrequency" },
	{ 6,  "LowInputFrequency" },
	{ 7,  "FreqAndOrPhaseDifference" },
	{ 8,  "AcceptableInput" },
	{ 9,  "AutomaticTest" },
	{ 10, "TestEnded" },
	{ 11, "LocalUICommand" },
	{ 12, "ProtocolCommand" },
	{ 13, "LowBatteryVoltage" },
	{ 14, "GeneralError" },
	{ 15, "PowerSystemError" },
	{ 16, "BatterySystemError" },
	{ 17, "ErrorCleared" },
	{ 18, "AutomaticRestart" },
	{ 19, "DistortedInverterOutput" },
	{ 20, "InverterOutputAcceptable" },
	{ 21, "EPOInterface" },
	{ 22, "InputPhaseDeltaOutOfRange" },
	{ 23, "InputNeutralNotConnected" },
	{ 24, "ATSTransfer" },
	{ 25, "ConfigurationChange" },
	{ 26, "AlertAsserted" },
	{ 27, "AlertCleared" },
	{ 28, "PlugRatingExceeded" },
	{ 29, "OutletGroupStateChange" },
	{ 30, "FailureBypassExpired" },
	{ 31, "InternalCommand" },
	{ 32, "USBCommand" },
	{ 33, "SmartSlot1Command" },
	{ 34, "InternalNetwork1Command" },
	{ 35, "FollowingSystemController" },
	{ 0, NULL }
};

const microlink_desc_value_map_t microlink_desc_value_map[] = {
	/* Inventory / identity */
	{ "2:4.9.40", "ups.serial",          MLINK_DESC_STRING,        MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.9.40", "device.serial",       MLINK_DESC_STRING,        MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.9.42", "experimental.device.sku",
	                                      MLINK_DESC_STRING,        MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.9.44", "ups.model",           MLINK_DESC_STRING,        MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.82",   "ups.id",              MLINK_DESC_STRING,        MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.9.1B", "ups.productid",       MLINK_DESC_HEX,           MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.9.1C", "ups.vendorid",        MLINK_DESC_HEX,           MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.9.19", "ups.mfr.date",        MLINK_DESC_DATE,          MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "3:4A",     "ups.firmware",        MLINK_DESC_STRING,        MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },

	/* Status / health */
	{ "2:4.A",    "experimental.device.status",
	                                      MLINK_DESC_BITFIELD_MAP,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, apc_status_map },
	{ "2:11",     "ups.test.result",     MLINK_DESC_BITFIELD_MAP,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, apc_test_status_map },
	{ "2:13",     "experimental.ups.calibration.result",
	                                      MLINK_DESC_BITFIELD_MAP,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, apc_calibration_status_map },
	{ "2:4.5.11", "experimental.battery.test.result",
	                                      MLINK_DESC_BITFIELD_MAP,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, apc_test_status_map },
	{ "2:4.5.13", "experimental.ups.calibration.result",
	                                      MLINK_DESC_BITFIELD_MAP,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, apc_calibration_status_map },
	{ "2:B",      "input.transfer.reason",
	                                      MLINK_DESC_ENUM_MAP,      MLINK_DESC_SIGNED,   0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, ups_status_change_cause_map },
	{ "2:4.5.18", "ups.test.interval",   MLINK_DESC_ENUM_MAP,      MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, battery_test_interval_map },
	{ "2:4.5.19", "battery.date.maintenance",
	                                      MLINK_DESC_DATE,          MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.22", "ups.temperature",     MLINK_DESC_FIXED_POINT,   MLINK_DESC_SIGNED,   7, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.22", "battery.temperature", MLINK_DESC_FIXED_POINT,   MLINK_DESC_SIGNED,   7, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.31", "battery.lowruntimewarning",
	                                      MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.42", "experimental.battery.sku",
	                                      MLINK_DESC_STRING,        MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.48", "battery.date",        MLINK_DESC_DATE,          MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.74", "battery.lifetime.status",
	                                      MLINK_DESC_BITFIELD_MAP,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, battery_lifetime_status_map },
	{ "2:4.5.9F", "battery.runtime",     MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.20", "battery.charge",      MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 9, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.21", "battery.voltage",     MLINK_DESC_FIXED_POINT,   MLINK_DESC_SIGNED,   5, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },

	/* Input / output */
	{ "2:4.6.16", "input.quality",       MLINK_DESC_BITFIELD_MAP,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, apc_input_quality_map },
	{ "2:4.6.25", "input.voltage",       MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 6, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.6.27", "input.frequency",     MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 7, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.6.BA", "input.transfer.delay", MLINK_DESC_ENUM_MAP,     MLINK_DESC_SIGNED,   0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, retransfer_delay_map },
	{ "2:4.7.D",  "input.transfer.high",  MLINK_DESC_FIXED_POINT,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.E",  "input.transfer.low",   MLINK_DESC_FIXED_POINT,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.25", "output.voltage",      MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 6, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.26", "output.current",      MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 5, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.27", "output.frequency",    MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 7, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.28", "ups.realpower",       MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 8, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.2A", "ups.realpower.nominal",
	                                      MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.2B", "ups.power.nominal",   MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.2C", "experimental.output.voltage.setting",
	                                      MLINK_DESC_BITFIELD_MAP,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, output_voltage_setting_map },
	{ "2:4.7.49", "ups.power",           MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 8, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "3:17",     "ups.efficiency",      MLINK_DESC_FIXED_POINT_MAP, MLINK_DESC_SIGNED, 7, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, apc_efficiency_map },
	{ "3:26",     "experimental.output.energy",
	                                      MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },

	/* Outlet groups */
	{ "2:4.3E.B8", "outlet.group.0.delay.shutdown",
	                                      MLINK_DESC_ENUM_MAP,      MLINK_DESC_SIGNED,   0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, apc_countdown_setting_map },
	{ "2:4.3E.B7", "outlet.group.0.delay.reboot",
	                                      MLINK_DESC_ENUM_MAP,      MLINK_DESC_SIGNED,   0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, apc_countdown_setting_map },
	{ "2:4.3E.B9", "outlet.group.0.delay.start",
	                                      MLINK_DESC_ENUM_MAP,      MLINK_DESC_SIGNED,   0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, apc_countdown_setting_map },
	{ "2:4.3E.30", "outlet.group.0.minimumreturnruntime",
	                                      MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.3E.31", "outlet.group.0.lowruntimewarning",
	                                      MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.3E.82", "outlet.group.0.name", MLINK_DESC_STRING,       MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.3E.84", "experimental.outlet.group.0.status.cause",
	                                      MLINK_DESC_BITFIELD_MAP,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, outlet_status_change_cause_map },
	{ "2:4.3E.B6", "outlet.group.0.status", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, microlink_outlet_status_map },
	{ "2:4.3D[%u].2D", "outlet.group.%u.timer.shutdown",
	                                      MLINK_DESC_ENUM_MAP,      MLINK_DESC_SIGNED,   0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, apc_countdown_map },
	{ "2:4.3D[%u].B8", "outlet.group.%u.delay.shutdown",
	                                      MLINK_DESC_ENUM_MAP,      MLINK_DESC_SIGNED,   0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, apc_countdown_setting_map },
	{ "2:4.3D[%u].2E", "outlet.group.%u.timer.reboot",
	                                      MLINK_DESC_ENUM_MAP,      MLINK_DESC_SIGNED,   0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, apc_countdown_map },
	{ "2:4.3D[%u].B7", "outlet.group.%u.delay.reboot",
	                                      MLINK_DESC_ENUM_MAP,      MLINK_DESC_SIGNED,   0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, apc_countdown_setting_map },
	{ "2:4.3D[%u].AD", "outlet.group.%u.timer.start",
	                                      MLINK_DESC_ENUM_MAP,      MLINK_DESC_SIGNED,   0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, apc_countdown_map },
	{ "2:4.3D[%u].B9", "outlet.group.%u.delay.start",
	                                      MLINK_DESC_ENUM_MAP,      MLINK_DESC_SIGNED,   0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, apc_countdown_setting_map },
	{ "2:4.3D[%u].30", "outlet.group.%u.minimumreturnruntime",
	                                      MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, NULL },
	{ "2:4.3D[%u].31", "outlet.group.%u.lowruntimewarning",
	                                      MLINK_DESC_FIXED_POINT,   MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, NULL },
	{ "2:4.3D[%u].82", "outlet.group.%u.name", MLINK_DESC_STRING,   MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, NULL },
	{ "2:4.3D[%u].84", "experimental.outlet.group.%u.status.cause",
	                                      MLINK_DESC_BITFIELD_MAP,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_ONE_BASED, outlet_status_change_cause_map },
	{ "2:4.3D[%u].B6", "outlet.group.%u.status", MLINK_DESC_BITFIELD_MAP,
	                                      MLINK_DESC_UNSIGNED,      0, MLINK_DESC_RO, MLINK_NAME_INDEX_ONE_BASED, microlink_outlet_status_map },

	/* Settings / statistics */
	{ "3:2B",     "ups.display.language", MLINK_DESC_ENUM_MAP,    MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, language_map },
	{ "2:4.5.F.69", "experimental.statistics.battery.totaltime",
	                                      MLINK_DESC_FIXED_POINT,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.6.F.69", "experimental.statistics.input.totaltime",
	                                      MLINK_DESC_FIXED_POINT,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.F.69", "experimental.statistics.output.totaltime",
	                                      MLINK_DESC_FIXED_POINT,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.F.69",  "experimental.statistics.ups.totaltime",
	                                      MLINK_DESC_FIXED_POINT,  MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
};

const size_t microlink_desc_value_map_count =
	sizeof(microlink_desc_value_map) / sizeof(microlink_desc_value_map[0]);
