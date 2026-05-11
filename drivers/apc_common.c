/* apc_common.c - Shared APC driver helpers
 *
 * Copyright (C) 2026 Lukas Schmid <lukas.schmid@netcube.li>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "main.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "timehead.h"

#include "apc_common.h"

const apc_value_map_t apc_input_quality_map[] = {
	{ (1U << 0), "Acceptable" },
	{ (1U << 1), "PendingAcceptable" },
	{ (1U << 2), "VoltageTooLow" },
	{ (1U << 3), "VoltageTooHigh" },
	{ (1U << 4), "Distorted" },
	{ (1U << 5), "BOOST" },
	{ (1U << 6), "TRIM" },
	{ (1U << 7), "FrequencyTooLow" },
	{ (1U << 8), "FrequencyTooHigh" },
	{ (1U << 9), "FreqAndPhaseNotLocked" },
	{ (1U << 10), "PhaseDeltaOutOfRange" },
	{ (1U << 11), "NeutralNotConnected" },
	{ (1U << 12), "NotAcceptable" },
	{ (1U << 13), "PlugRatingExceeded" },
	{ (1U << 14), "PhaseBotAcceptable" },
	{ (1U << 15), "PoweringLoad" },
	{ 0, NULL }
};

const apc_value_map_t apc_efficiency_map[] = {
	{ -1, "NotAvailable" },
	{ -2, "LoadTooLow" },
	{ -3, "OutputOff" },
	{ -4, "OnBattery" },
	{ -5, "InBypass" },
	{ -6, "BatteryCharging" },
	{ -7, "PoorACInput" },
	{ -8, "BatteryDisconnected" },
	{ 0, NULL }
};

const apc_value_map_t apc_test_status_map[] = {
	{ 0,  "None" },
	{ (1ULL << 0),  "Pending" },
	{ (1ULL << 1),  "InProgress" },
	{ (1ULL << 2),  "Passed" },
	{ (1ULL << 3),  "Failed" },
	{ (1ULL << 4),  "Refused" },
	{ (1ULL << 5),  "Aborted" },
	{ (1ULL << 7),  "LocalUser" },
	{ (1ULL << 8),  "Internal" },
	{ (1ULL << 9),  "InvalidState" },
	{ (1ULL << 10), "InternalFault" },
	{ (1ULL << 11), "StateOfChargeNotAcceptable" },
	{ 0, NULL }
};

const apc_value_map_t apc_calibration_status_map[] = {
	{ 0,  "None" },
	{ (1ULL << 0),  "Pending" },
	{ (1ULL << 1),  "InProgress" },
	{ (1ULL << 2),  "Passed" },
	{ (1ULL << 3),  "Failed" },
	{ (1ULL << 4),  "Refused" },
	{ (1ULL << 5),  "Aborted" },
	{ (1ULL << 9),  "InvalidState" },
	{ (1ULL << 10), "InternalFault" },
	{ (1ULL << 11), "StateOfChargeNotAcceptable" },
	{ (1ULL << 12), "LoadChange" },
	{ (1ULL << 13), "ACInputNotAcceptable" },
	{ (1ULL << 14), "LoadTooLow" },
	{ (1ULL << 15), "OverChargeInProgress" },
	{ 0, NULL }
};

const apc_value_map_t apc_status_map[] = {
	{ (1ULL << 1), "OL" },
	{ (1ULL << 2), "OB" },
	{ (1ULL << 3), "BYPASS" },
	{ (1ULL << 4), "OFF" },
	{ (1ULL << 7), "TEST" },
	{ (1ULL << 21), "OVER" },
	{ 0, NULL }
};

const apc_value_map_t apc_alarm_map[] = {
	{ (1ULL << 5), "General fault" },
	{ (1ULL << 6), "Input not acceptable" },
	{ 0, NULL }
};

const apc_outlet_command_suffix_t apc_outlet_command_suffixes[] = {
	{ "load.off", APC_OUTLET_OP_LOAD_OFF },
	{ "load.on", APC_OUTLET_OP_LOAD_ON },
	{ "load.cycle", APC_OUTLET_OP_LOAD_CYCLE },
	{ "load.off.delay", APC_OUTLET_OP_LOAD_OFF_DELAY },
	{ "load.on.delay", APC_OUTLET_OP_LOAD_ON_DELAY },
	{ "shutdown.return", APC_OUTLET_OP_SHUTDOWN_RETURN },
	{ "shutdown.stayoff", APC_OUTLET_OP_SHUTDOWN_STAYOFF },
	{ "shutdown.reboot", APC_OUTLET_OP_SHUTDOWN_REBOOT },
	{ "shutdown.reboot.graceful", APC_OUTLET_OP_SHUTDOWN_REBOOT_GRACEFUL },
	{ NULL, APC_OUTLET_OP_NULL }
};

const apc_value_map_t apc_countdown_map[] = {
	{ -1, "NotActive" },
	{ 0,  "CountdownExpired" },
	{ 0, NULL }
};

const apc_value_map_t apc_countdown_setting_map[] = {
	{ -1, "Disabled" },
	{ 0, NULL }
};

int apc_format_countdown_value(int64_t value, char *output, size_t output_len)
{
	const char *text;
	int res;

	if (output == NULL || output_len == 0) {
		return 0;
	}

	if (value == -1) {
		text = "NotActive";
	} else if (value == 0) {
		text = "CountdownExpired";
	} else {
		res = snprintf(output, output_len, "%" PRIi64, value);
		if (res < 0 || (size_t)res >= output_len) {
			return 0;
		}
		return 1;
	}

	res = snprintf(output, output_len, "%s", text);
	if (res < 0 || (size_t)res >= output_len) {
		return 0;
	}

	return 1;
}

/* Format a day counter as YYYY-MM-DD. Microlink and Modbus both store their
 * documented date fields as days since 2000-01-01, so the offset is fixed
 * here.
 */
int apc_format_date_from_days_offset(int64_t value, char *output, size_t output_len)
{
	struct tm tm_info;
	time_t time_stamp;
	int res;

	if (output == NULL || output_len == 0) {
		return 0;
	}

	time_stamp = ((time_t)value * 86400) + ((time_t)10957 * 86400);
	if (gmtime_r(&time_stamp, &tm_info) == NULL) {
		return 0;
	}

	res = strftime(output, output_len, "%Y-%m-%d", &tm_info);
	if (res == 0) {
		return 0;
	}

	return 1;
}

/* Parse YYYY-MM-DD into day counts. The on-wire format is a day count since
 * 2000-01-01, so the fixed offset is handled here.
 */
int apc_parse_date_to_days_offset(const char *value, uint64_t *days_out)
{
	struct tm tm_struct;
	time_t epoch_time;
	uint64_t uint_value;

	if (value == NULL || days_out == NULL) {
		return 0;
	}

	memset(&tm_struct, 0, sizeof(tm_struct));
	if (strptime(value, "%Y-%m-%d", &tm_struct) == NULL) {
		return 0;
	}

	if ((epoch_time = timegm(&tm_struct)) == (time_t)-1) {
		return 0;
	}

	uint_value = (uint64_t)((epoch_time - ((time_t)10957 * 86400)) / 86400);
	*days_out = uint_value;
	return 1;
}

int apc_format_test_status_value(const apc_value_map_t *result_map, const apc_value_map_t *source_map,
	const apc_value_map_t *modifier_map, uint64_t value, char *output, size_t output_len)
{
	const char *parts[3];
	size_t part_count = 0;
	size_t i;
	int res;

	if (output == NULL || output_len == 0) {
		return 0;
	}

	output[0] = '\0';

	if (result_map != NULL) {
		parts[part_count] = apc_lookup_first_set_value_map_text(result_map, value);
		if (parts[part_count] != NULL) {
			part_count++;
		}
	}

	if (source_map != NULL) {
		parts[part_count] = apc_lookup_first_set_value_map_text(source_map, value);
		if (parts[part_count] != NULL) {
			part_count++;
		}
	}

	if (modifier_map != NULL) {
		parts[part_count] = apc_lookup_first_set_value_map_text(modifier_map, value);
		if (parts[part_count] != NULL) {
			part_count++;
		}
	}

	for (i = 0; i < part_count; i++) {
		res = snprintf(output + strlen(output), output_len - strlen(output), "%s%s",
			(i > 0) ? ", " : "", parts[i]);
		if (res < 0 || (size_t)res >= output_len - strlen(output)) {
			return 0;
		}
	}

	return 1;
}

uint64_t apc_build_outlet_command(apc_outlet_command_type_t type, uint64_t target_bits)
{
	uint64_t cmd = target_bits;

	switch (type) {
	case APC_OUTLET_OP_LOAD_OFF:
		cmd |= APC_OUTLET_CMD_OUTPUT_OFF;
		break;
	case APC_OUTLET_OP_LOAD_ON:
		cmd |= APC_OUTLET_CMD_OUTPUT_ON;
		break;
	case APC_OUTLET_OP_LOAD_CYCLE:
		cmd |= APC_OUTLET_CMD_OUTPUT_REBOOT;
		break;
	case APC_OUTLET_OP_LOAD_OFF_DELAY:
		cmd |= APC_OUTLET_CMD_OUTPUT_OFF | APC_OUTLET_CMD_USE_OFF_DELAY;
		break;
	case APC_OUTLET_OP_LOAD_ON_DELAY:
		cmd |= APC_OUTLET_CMD_OUTPUT_ON | APC_OUTLET_CMD_USE_ON_DELAY;
		break;
	case APC_OUTLET_OP_SHUTDOWN_RETURN:
		cmd |= APC_OUTLET_CMD_OUTPUT_SHUTDOWN | APC_OUTLET_CMD_USE_OFF_DELAY;
		break;
	case APC_OUTLET_OP_SHUTDOWN_STAYOFF:
		cmd |= APC_OUTLET_CMD_OUTPUT_OFF | APC_OUTLET_CMD_USE_OFF_DELAY;
		break;
	case APC_OUTLET_OP_SHUTDOWN_REBOOT:
		cmd |= APC_OUTLET_CMD_OUTPUT_REBOOT;
		break;
	case APC_OUTLET_OP_SHUTDOWN_REBOOT_GRACEFUL:
		cmd |= APC_OUTLET_CMD_OUTPUT_REBOOT | APC_OUTLET_CMD_USE_OFF_DELAY;
		break;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
	case APC_OUTLET_OP_NULL:
	default:
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
		/* Must not occur. */
		break;
	}

	return cmd;
}

const char *apc_lookup_value_map_text(const apc_value_map_t *map, int value)
{
	size_t i;

	if (map == NULL) {
		return NULL;
	}

	for (i = 0; map[i].text != NULL; i++) {
		if (map[i].value == value) {
			return map[i].text;
		}
	}

	return NULL;
}

const char *apc_lookup_first_set_value_map_text(const apc_value_map_t *map, uint64_t value)
{
	size_t i;

	if (map == NULL) {
		return NULL;
	}

	for (i = 0; map[i].text != NULL; i++) {
		if (map[i].value == 0) {
			if (value == 0) {
				return map[i].text;
			}
			continue;
		}

		if ((value & (uint64_t)map[i].value) != 0) {
			return map[i].text;
		}
	}

	return NULL;
}
