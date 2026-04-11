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

/* Descriptor names and enumerations were derived from Microlink descriptors
 * published by https://ulexplorer-07aa30.gitlab.io/
 */

#include "apcmicrolink.h"
#include "apcmicrolink-maps.h"

static const microlink_value_map_t cal_status_map[] = {
	{ 0,  "None" },
	{ (1UL << 0),  "Pending" },
	{ (1UL << 1),  "InProgress" },
	{ (1UL << 2),  "Passed" },
	{ (1UL << 3),  "Failed" },
	{ (1UL << 4),  "Refused" },
	{ (1UL << 5),  "Aborted" },
	{ (1UL << 9),  "InvalidState" },
	{ (1UL << 10), "InternalFault" },
	{ (1UL << 11), "StateOfChargeNotAcceptable" },
	{ (1UL << 12), "LoadChange" },
	{ (1UL << 13), "ACInputNotAcceptable" },
	{ (1UL << 14), "LoadTooLow" },
	{ (1UL << 15), "OverChargeInProgress" },
	{ 0, NULL }
};

static const microlink_value_map_t test_status_map[] = {
	{ 0,  "None" },
	{ (1U << 0),  "Pending" },
	{ (1U << 1),  "InProgress" },
	{ (1U << 2),  "Passed" },
	{ (1U << 3),  "Failed" },
	{ (1U << 4),  "Refused" },
	{ (1U << 5),  "Aborted" },
	{ (1U << 9),  "InvalidState" },
	{ (1U << 10), "InternalFault" },
	{ (1U << 11), "StateOfChargeNotAcceptable" },
	{ 0, NULL }
};

static const microlink_value_map_t microlink_outlet_status_map[] = {
	{ (1U << 0), "StateOn" },
	{ (1U << 1), "StateOff" },
	{ (1U << 2), "ProcessReboot" },
	{ (1U << 3), "ProcessShutdown" },
	{ (1U << 4), "ProcessSleep" },
	{ (1U << 8), "PendingOnDelay" },
	{ (1U << 9), "PendingOffDelay" },
	{ (1U << 10), "PendingOnACPresence" },
	{ (1U << 11), "PendingOnMinRuntime" },
	{ (1U << 14), "LowRuntime" },
	{ 0, NULL }
};

static const microlink_value_map_t battery_error_map[] = {
	{ 0,  "None" },
	{ (1UL << 0),  "Disconnected" },
	{ (1UL << 1),  "Overvoltage" },
	{ (1UL << 2),  "NeedsReplacement" },
	{ (1UL << 3),  "OvertemperatureCritical" },
	{ (1UL << 4),  "Charger" },
	{ (1UL << 5),  "TemperatureSensor" },
	{ (1UL << 6),  "BusSoftStart" },
	{ (1UL << 7),  "OvertemperatureWarning" },
	{ (1UL << 8),  "GeneralError" },
	{ (1UL << 9),  "Communication" },
	{ (1UL << 10), "DisconnectedFrame" },
	{ (1UL << 11), "FirmwareMismatch" },
	{ (1UL << 12), "VoltageSenseError" },
	{ (1UL << 13), "IncompatiblePack" },
	{ (1UL << 14), "ChemistryMismatch" },
	{ (1UL << 16), "PositiveFuseOrRelayError" },
	{ (1UL << 17), "NegativeFuseOrRelayError" },
	{ 0, NULL }
};

static const microlink_value_map_t general_error_map[] = {
	{ 0,  "None" },
	{ (1UL << 0),  "SiteWiring" },
	{ (1UL << 1),  "EEPROM" },
	{ (1UL << 2),  "ADConverter" },
	{ (1UL << 3),  "LogicPowerSupply" },
	{ (1UL << 4),  "InternalCommunication" },
	{ (1UL << 5),  "UIButton" },
	{ (1UL << 6),  "NeedsFactorySetup" },
	{ (1UL << 7),  "EPOActive" },
	{ (1UL << 8),  "FirmwareMismatch" },
	{ (1UL << 9),  "Oscillator" },
	{ (1UL << 10), "MeasurementMismatch" },
	{ (1UL << 11), "Subsystem" },
	{ (1UL << 12), "LogicPowerSupplyRelay" },
	{ (1UL << 13), "NetworkWarning" },
	{ (1UL << 14), "InputContactOutputRelay" },
	{ (1UL << 15), "AirFilterWarning" },
	{ (1UL << 16), "DisplayCommunication" },
	{ 0, NULL }
};

static const microlink_value_map_t power_error_map[] = {
	{ 0,  "None" },
	{ (1UL << 0),  "OutputOverload" },
	{ (1UL << 1),  "OutputShortCircuit" },
	{ (1UL << 2),  "OutputOvervoltage" },
	{ (1UL << 3),  "TransformerDCImbalance" },
	{ (1UL << 4),  "Overtemperature" },
	{ (1UL << 5),  "BackfeedRelay" },
	{ (1UL << 6),  "AVRRelay" },
	{ (1UL << 7),  "PFCInputRelay" },
	{ (1UL << 8),  "OutputRelay" },
	{ (1UL << 9),  "BypassRelay" },
	{ (1UL << 10), "Fan" },
	{ (1UL << 11), "PFC" },
	{ (1UL << 12), "DCBusOvervoltage" },
	{ (1UL << 13), "Inverter" },
	{ (1UL << 14), "OverCurrent" },
	{ (1UL << 15), "BypassPFCRelay" },
	{ (1UL << 16), "BusSoftStart" },
	{ (1UL << 17), "GreenRelay" },
	{ (1UL << 18), "DCOutput" },
	{ (1UL << 19), "DCBusConverter" },
	{ (1UL << 20), "Sensor" },
	{ (1UL << 21), "InstallationWiring" },
	{ 0, NULL }
};

static const microlink_value_map_t ups_status_map[] = {
	{ (1UL << 0),  "StatusChange" },
	{ (1UL << 1),  "StateOnline" },
	{ (1UL << 2),  "StateOnBattery" },
	{ (1UL << 3),  "StateBypass" },
	{ (1UL << 4),  "StateOutputOff" },
	{ (1UL << 5),  "Fault" },
	{ (1UL << 6),  "InputBad" },
	{ (1UL << 7),  "Test" },
	{ (1UL << 8),  "PendingOutputOn" },
	{ (1UL << 9),  "PendingOutputOff" },
	{ (1UL << 10), "Commanded" },
	{ (1UL << 11), "Maintenance" },
	{ (1UL << 12), "Inquiring" },
	{ (1UL << 13), "HighEfficiency" },
	{ (1UL << 14), "InformationalAlert" },
	{ (1UL << 15), "FaultState" },
	{ (1UL << 16), "StaticBypassStandby" },
	{ (1UL << 17), "InverterStandby" },
	{ (1UL << 18), "MegaTie" },
	{ (1UL << 19), "MainsBadState" },
	{ (1UL << 20), "FaultRecoveryState" },
	{ (1UL << 21), "OverloadState" },
	{ (1UL << 22), "MaintenanceMode" },
	{ (1UL << 23), "EfficiencyTestMode" },
	{ (1UL << 24), "ForcedInternal" },
	{ 0, NULL }
};

static const microlink_value_map_t ups_status_status_map[] = {
	{ (1UL << 1),  "OL" },
	{ (1UL << 2),  "OB" },
	{ (1UL << 3),  "BYPASS" },
	{ (1UL << 4),  "OFF" },
	{ (1UL << 7),  "CAL" },
	{ (1UL << 21), "OVER" },
	{ 0, NULL }
};

static const microlink_value_map_t ups_status_alarm_map[] = {
	{ (1UL << 5),  "Fault" },
	{ (1UL << 6),  "InputBad" },
	{ (1UL << 14), "InformationalAlert" },
	{ 0, NULL }
};

static const microlink_value_map_t cal_status_status_map[] = {
	{ (1UL << 1), "CAL" },
	{ 0, NULL }
};

static const microlink_value_map_t battery_error_status_map[] = {
	{ (1UL << 2), "RB" },
	{ 0, NULL }
};

static const microlink_value_map_t power_error_status_map[] = {
	{ (1UL << 0), "OVER" },
	{ 0, NULL }
};

const microlink_desc_publish_map_t microlink_desc_publish_map[] = {
	{ "2:4.A", ups_status_status_map, ups_status_alarm_map },
	{ "2:13", cal_status_status_map, NULL },
	{ "2:4.34", battery_error_status_map, battery_error_map },
	{ "2:4.38", NULL, general_error_map },
	{ "2:4.36", power_error_status_map, power_error_map },
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

static const microlink_value_map_t input_status_map[] = {
	{ (1U << 0),  "Acceptable" },
	{ (1U << 1),  "PendingAcceptable" },
	{ (1U << 2),  "VoltageTooLow" },
	{ (1U << 3),  "VoltageTooHigh" },
	{ (1U << 4),  "Distorted" },
	{ (1U << 5),  "Boost" },
	{ (1U << 6),  "Trim" },
	{ (1U << 7),  "FrequencyTooLow" },
	{ (1U << 8),  "FrequencyTooHigh" },
	{ (1U << 9),  "FreqAndPhaseNotLocked" },
	{ (1U << 10), "PhaseDeltaOutOfRange" },
	{ (1U << 11), "NeutralNotConnected" },
	{ (1U << 12), "NotAcceptable" },
	{ (1U << 13), "PlugRatingExceeded" },
	{ (1U << 14), "PhaseBotAcceptable" },
	{ (1U << 15), "PoweringLoad" },
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

static const microlink_value_map_t countdown_map[] = {
	{ -1,  "NotActive" },
	{ 0,  "CountdownExpired" },
	{ 0, NULL }
};

static const microlink_value_map_t countdown_setting_map[] = {
	{ -1,  "Disabled" },
	{ 0, NULL }
};

#define MLINK_BATTERY_TEST_CMD_START	(1ULL << 0)
#define MLINK_BATTERY_TEST_CMD_ABORT	(1ULL << 1)
#define MLINK_BATTERY_TEST_CMD_LOCALUSER	(1ULL << 9)

#define MLINK_RUNTIME_CAL_CMD_START	(1ULL << 0)
#define MLINK_RUNTIME_CAL_CMD_ABORT	(1ULL << 1)
#define MLINK_RUNTIME_CAL_CMD_LOCALUSER	(1ULL << 9)

#define MLINK_UPS_CMD_OUTPUT_INTO_BYPASS	(1ULL << 4)
#define MLINK_UPS_CMD_OUTPUT_OUT_OF_BYPASS	(1ULL << 5)
#define MLINK_UPS_CMD_LOCALUSER		(1ULL << 29)
#define MLINK_UPS_CMD_SMARTSLOT1	(1ULL << 30)

#define MLINK_UPS_CMD_PANEL_SHORT_TEST	(1ULL << 0)
#define MLINK_UPS_CMD_PANEL_CONT_TEST	(1ULL << 1)

#define MLINK_OUTLET_CMD_CANCEL		(1ULL << 0)
#define MLINK_OUTLET_CMD_OUTPUT_ON	(1ULL << 1)
#define MLINK_OUTLET_CMD_OUTPUT_OFF	(1ULL << 2)
#define MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN	(1ULL << 3)
#define MLINK_OUTLET_CMD_OUTPUT_REBOOT	(1ULL << 4)
#define MLINK_OUTLET_CMD_COLD_BOOT_ALLOWED	(1ULL << 5)
#define MLINK_OUTLET_CMD_USE_ON_DELAY	(1ULL << 6)
#define MLINK_OUTLET_CMD_USE_OFF_DELAY	(1ULL << 7)
#define MLINK_OUTLET_CMD_TARGET_UNSWITCHED	(1ULL << 8)
#define MLINK_OUTLET_CMD_TARGET_SWITCHED0	(1ULL << 9)
#define MLINK_OUTLET_CMD_TARGET_SWITCHED1	(1ULL << 10)
#define MLINK_OUTLET_CMD_TARGET_SWITCHED2	(1ULL << 11)
#define MLINK_OUTLET_CMD_TARGET_SWITCHED3	(1ULL << 19)
#define MLINK_OUTLET_CMD_LOCALUSER	(1ULL << 13)
#define MLINK_OUTLET_CMD_SMARTSLOT1	(1ULL << 15)

const microlink_desc_value_map_t microlink_desc_value_map[] = {
	{ "2:4.9.40", "ups.serial", MLINK_DESC_STRING, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.9.44", "ups.model", MLINK_DESC_STRING, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.82", "ups.id", MLINK_DESC_STRING, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.28", "ups.load", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 8, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.22", "ups.temperature", MLINK_DESC_FIXED_POINT, MLINK_DESC_SIGNED, 7, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:11", "ups.test.result", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, test_status_map },
	{ "2:4.5.13", "experimental.ups.calibration.result", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, cal_status_map },
	{ "2:4.34", "experimental.ups.battery.error", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, battery_error_map },
	{ "2:4.38", "experimental.ups.general.error", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, general_error_map },
	{ "2:4.36", "experimental.ups.power.error", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, power_error_map },

	{ "2:4.9.40", "device.serial", MLINK_DESC_STRING, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.9.42", "experimental.device.sku", MLINK_DESC_STRING, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.A", "experimental.device.status", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, ups_status_map },

	/* Switched Outlet Groups */
	{ "2:4.3D[%u].2D", "experimental.outlet.group.%u.offcountdown", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, countdown_map },
	{ "2:4.3D[%u].B8", "experimental.outlet.group.%u.offcountdown.setting", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, countdown_setting_map },
	{ "2:4.3D[%u].2E", "experimental.outlet.group.%u.stayoffcountdown", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, countdown_map },
	{ "2:4.3D[%u].B7", "experimental.outlet.group.%u.stayoffcountdown.setting", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, countdown_setting_map },
	{ "2:4.3D[%u].AD", "experimental.outlet.group.%u.oncountdown", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, countdown_map },
	{ "2:4.3D[%u].B9", "experimental.outlet.group.%u.oncountdown.setting", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, countdown_setting_map },
	{ "2:4.3D[%u].30", "outlet.group.%u.minimumreturnruntime", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, NULL },
	{ "2:4.3D[%u].31", "outlet.group.%u.lowruntimewarning", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, NULL },
	{ "2:4.3D[%u].82", "outlet.group.%u.id", MLINK_DESC_STRING, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_ONE_BASED, NULL },
	{ "2:4.3D[%u].84", "experimental.outlet.group.%u.status.cause", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_ONE_BASED, outlet_status_change_cause_map },
	{ "2:4.3D[%u].B6", "outlet.group.%u.status", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_ONE_BASED, microlink_outlet_status_map },

	/* Unswitched Outlet Group */
	{ "2:4.3E.2D", "experimental.outlet.group.0.offcountdown", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, countdown_map },
	{ "2:4.3E.B8", "experimental.outlet.group.0.offcountdown.setting", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, countdown_setting_map },
	{ "2:4.3E.2E", "experimental.outlet.group.0.stayoffcountdown", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, countdown_map },
	{ "2:4.3E.B7", "experimental.outlet.group.0.stayoffcountdown.setting", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, countdown_setting_map },
	{ "2:4.3E.AD", "experimental.outlet.group.0.oncountdown", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, countdown_map },
	{ "2:4.3E.B9", "experimental.outlet.group.0.oncountdown.setting", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, countdown_setting_map },
	{ "2:4.3E.30", "outlet.group.0.minimumreturnruntime", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.3E.82", "outlet.group.0.id", MLINK_DESC_STRING, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.3E.84", "experimental.outlet.group.0.status.cause", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, outlet_status_change_cause_map },
	{ "2:4.3E.B6", "outlet.group.0.status", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, microlink_outlet_status_map },

	/* Status */
	{ "2:4.5.20", "battery.charge", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 9, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.21", "battery.voltage", MLINK_DESC_FIXED_POINT, MLINK_DESC_SIGNED, 5, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.22", "battery.temperature", MLINK_DESC_FIXED_POINT, MLINK_DESC_SIGNED, 7, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.31", "battery.lowruntimewarning", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.42", "experimental.battery.sku", MLINK_DESC_STRING, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.9F", "battery.runtime", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.19", "battery.date", MLINK_DESC_DATE, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.48", "battery.date.setting", MLINK_DESC_DATE, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.5.18", "ups.test.interval", MLINK_DESC_ENUM_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, battery_test_interval_map },
	{ "2:4.5.74", "battery.lifetime.status", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, battery_lifetime_status_map },
	{ "2:4.5.11", "experimental.battery.test.result", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, test_status_map },
	{ "2:B", "input.transfer.reason", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, ups_status_change_cause_map },

	/* Input */
	{ "2:4.6.16", "input.quality", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, input_status_map },
	{ "2:4.6.25", "input.voltage", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 6, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.6.27", "input.frequency", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 7, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.6.BA", "input.transfer.delay", MLINK_DESC_ENUM_MAP, MLINK_DESC_SIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, retransfer_delay_map },

	/* Output */
	{ "2:4.7.D", "input.transfer.high", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.E", "input.transfer.low", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.25", "output.voltage", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 6, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.26", "output.current", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 5, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.27", "output.frequency", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 7, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.28", "output.realpower", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 8, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.2A", "output.realpower.rating", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.2B", "output.apparentpower.rating", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.2C", "experimental.output.voltage.setting", MLINK_DESC_BITFIELD_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, output_voltage_setting_map },
	{ "2:4.7.49", "output.apparentpower", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 8, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },

	/* Settings & Statistics */
	{ "3:2B", "ups.display.language", MLINK_DESC_ENUM_MAP, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RW, MLINK_NAME_INDEX_NONE, language_map },
	{ "2:4.5.F.69", "experimental.statistics.battery.totaltime", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.6.F.69", "experimental.statistics.input.totaltime", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.7.F.69", "experimental.statistics.output.totaltime", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
	{ "2:4.F.69", "experimental.statistics.ups.totaltime", MLINK_DESC_FIXED_POINT, MLINK_DESC_UNSIGNED, 0, MLINK_DESC_RO, MLINK_NAME_INDEX_NONE, NULL },
};

const microlink_desc_command_map_t microlink_desc_command_map[] = {
	{ "2:10", "test.battery.start", MLINK_DESC_WRITE_BITMASK, 0, MLINK_BATTERY_TEST_CMD_START | MLINK_BATTERY_TEST_CMD_LOCALUSER, NULL, NULL },
	{ "2:10", "test.battery.stop", MLINK_DESC_WRITE_BITMASK, 1, MLINK_BATTERY_TEST_CMD_ABORT | MLINK_BATTERY_TEST_CMD_LOCALUSER, NULL, NULL },
	{ "2:12", "calibrate.start", MLINK_DESC_WRITE_BITMASK, 0, MLINK_RUNTIME_CAL_CMD_START | MLINK_RUNTIME_CAL_CMD_LOCALUSER, NULL, NULL },
	{ "2:12", "calibrate.stop", MLINK_DESC_WRITE_BITMASK, 1, MLINK_RUNTIME_CAL_CMD_ABORT | MLINK_RUNTIME_CAL_CMD_LOCALUSER, NULL, NULL },
	{ "2:14", "bypass.start", MLINK_DESC_WRITE_BITMASK, 4, MLINK_UPS_CMD_OUTPUT_INTO_BYPASS | MLINK_UPS_CMD_LOCALUSER | MLINK_UPS_CMD_SMARTSLOT1, NULL, NULL },
	{ "2:14", "bypass.stop", MLINK_DESC_WRITE_BITMASK, 5, MLINK_UPS_CMD_OUTPUT_OUT_OF_BYPASS | MLINK_UPS_CMD_LOCALUSER | MLINK_UPS_CMD_SMARTSLOT1, NULL, NULL },

	{ "2:4.B5", "load.off", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_OFF | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "load.off.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_OFF | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "load.on", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_ON | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "load.on.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_ON | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "load.cycle", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "load.cycle.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "shutdown.return", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN | MLINK_OUTLET_CMD_COLD_BOOT_ALLOWED | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "shutdown.stayoff", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "shutdown.reboot", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },

	{ "2:4.B5", "outlet.group.0.load.off", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_OFF | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "outlet.group.0.load.off.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_OFF | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "outlet.group.0.load.on", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_ON | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "outlet.group.0.load.on.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_ON | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "outlet.group.0.load.cycle", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "outlet.group.0.load.cycle.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "outlet.group.0.shutdown.return", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN | MLINK_OUTLET_CMD_COLD_BOOT_ALLOWED | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "outlet.group.0.shutdown.stayoff", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },
	{ "2:4.B5", "outlet.group.0.shutdown.reboot", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_UNSWITCHED | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3E.B6" },

	{ "2:4.B5", "outlet.group.1.load.off", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_OFF | MLINK_OUTLET_CMD_TARGET_SWITCHED0 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[0].B6" },
	{ "2:4.B5", "outlet.group.1.load.off.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_OFF | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED0 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[0].B6" },
	{ "2:4.B5", "outlet.group.1.load.on", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_ON | MLINK_OUTLET_CMD_TARGET_SWITCHED0 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[0].B6" },
	{ "2:4.B5", "outlet.group.1.load.on.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_ON | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED0 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[0].B6" },
	{ "2:4.B5", "outlet.group.1.load.cycle", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_TARGET_SWITCHED0 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[0].B6" },
	{ "2:4.B5", "outlet.group.1.load.cycle.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED0 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[0].B6" },
	{ "2:4.B5", "outlet.group.1.shutdown.return", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN | MLINK_OUTLET_CMD_COLD_BOOT_ALLOWED | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED0 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[0].B6" },
	{ "2:4.B5", "outlet.group.1.shutdown.stayoff", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED0 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[0].B6" },
	{ "2:4.B5", "outlet.group.1.shutdown.reboot", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED0 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[0].B6" },

	{ "2:4.B5", "outlet.group.2.load.off", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_OFF | MLINK_OUTLET_CMD_TARGET_SWITCHED1 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[1].B6" },
	{ "2:4.B5", "outlet.group.2.load.off.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_OFF | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED1 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[1].B6" },
	{ "2:4.B5", "outlet.group.2.load.on", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_ON | MLINK_OUTLET_CMD_TARGET_SWITCHED1 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[1].B6" },
	{ "2:4.B5", "outlet.group.2.load.on.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_ON | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED1 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[1].B6" },
	{ "2:4.B5", "outlet.group.2.load.cycle", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_TARGET_SWITCHED1 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[1].B6" },
	{ "2:4.B5", "outlet.group.2.load.cycle.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED1 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[1].B6" },
	{ "2:4.B5", "outlet.group.2.shutdown.return", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN | MLINK_OUTLET_CMD_COLD_BOOT_ALLOWED | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED1 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[1].B6" },
	{ "2:4.B5", "outlet.group.2.shutdown.stayoff", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED1 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[1].B6" },
	{ "2:4.B5", "outlet.group.2.shutdown.reboot", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED1 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[1].B6" },

	{ "2:4.B5", "outlet.group.3.load.off", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_OFF | MLINK_OUTLET_CMD_TARGET_SWITCHED2 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[2].B6" },
	{ "2:4.B5", "outlet.group.3.load.off.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_OFF | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED2 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[2].B6" },
	{ "2:4.B5", "outlet.group.3.load.on", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_ON | MLINK_OUTLET_CMD_TARGET_SWITCHED2 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[2].B6" },
	{ "2:4.B5", "outlet.group.3.load.on.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_ON | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED2 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[2].B6" },
	{ "2:4.B5", "outlet.group.3.load.cycle", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_TARGET_SWITCHED2 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[2].B6" },
	{ "2:4.B5", "outlet.group.3.load.cycle.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED2 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[2].B6" },
	{ "2:4.B5", "outlet.group.3.shutdown.return", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN | MLINK_OUTLET_CMD_COLD_BOOT_ALLOWED | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED2 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[2].B6" },
	{ "2:4.B5", "outlet.group.3.shutdown.stayoff", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED2 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[2].B6" },
	{ "2:4.B5", "outlet.group.3.shutdown.reboot", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED2 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[2].B6" },

	{ "2:4.B5", "outlet.group.4.load.off", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_OFF | MLINK_OUTLET_CMD_TARGET_SWITCHED3 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[3].B6" },
	{ "2:4.B5", "outlet.group.4.load.off.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_OFF | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED3 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[3].B6" },
	{ "2:4.B5", "outlet.group.4.load.on", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_ON | MLINK_OUTLET_CMD_TARGET_SWITCHED3 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[3].B6" },
	{ "2:4.B5", "outlet.group.4.load.on.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_ON | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED3 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[3].B6" },
	{ "2:4.B5", "outlet.group.4.load.cycle", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_TARGET_SWITCHED3 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[3].B6" },
	{ "2:4.B5", "outlet.group.4.load.cycle.delay", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED3 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[3].B6" },
	{ "2:4.B5", "outlet.group.4.shutdown.return", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN | MLINK_OUTLET_CMD_COLD_BOOT_ALLOWED | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED3 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[3].B6" },
	{ "2:4.B5", "outlet.group.4.shutdown.stayoff", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_SHUTDOWN | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED3 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[3].B6" },
	{ "2:4.B5", "outlet.group.4.shutdown.reboot", MLINK_DESC_WRITE_BITMASK, 0, MLINK_OUTLET_CMD_OUTPUT_REBOOT | MLINK_OUTLET_CMD_USE_ON_DELAY | MLINK_OUTLET_CMD_USE_OFF_DELAY | MLINK_OUTLET_CMD_TARGET_SWITCHED3 | MLINK_OUTLET_CMD_LOCALUSER, NULL, "2:4.3D[3].B6" }
};

const size_t microlink_desc_value_map_count =
	sizeof(microlink_desc_value_map) / sizeof(microlink_desc_value_map[0]);
const size_t microlink_desc_command_map_count =
	sizeof(microlink_desc_command_map) / sizeof(microlink_desc_command_map[0]);
