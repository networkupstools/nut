/* apc_common.h - Shared APC driver helpers
 *
 * Copyright (C) 2026 Lukas Schmid <lukas.schmid@netcube.li>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef APC_COMMON_H
#define APC_COMMON_H

#include <stddef.h>
#include <stdint.h>

typedef struct apc_value_map_s {
	int value;
	const char *text;
} apc_value_map_t;

typedef enum apc_test_status_bits_e {
	APC_TEST_STATUS_PENDING = (1ULL << 0),
	APC_TEST_STATUS_IN_PROGRESS = (1ULL << 1),
	APC_TEST_STATUS_PASSED = (1ULL << 2),
	APC_TEST_STATUS_FAILED = (1ULL << 3),
	APC_TEST_STATUS_REFUSED = (1ULL << 4),
	APC_TEST_STATUS_ABORTED = (1ULL << 5),
	APC_TEST_STATUS_LOCAL_USER = (1ULL << 7),
	APC_TEST_STATUS_INTERNAL = (1ULL << 8),
	APC_TEST_STATUS_INVALID_STATE = (1ULL << 9),
	APC_TEST_STATUS_INTERNAL_FAULT = (1ULL << 10),
	APC_TEST_STATUS_STATE_OF_CHARGE_NOT_ACCEPTABLE = (1ULL << 11)
} apc_test_status_bits_t;

typedef enum apc_calibration_status_bits_e {
	APC_CAL_STATUS_PENDING = (1ULL << 0),
	APC_CAL_STATUS_IN_PROGRESS = (1ULL << 1),
	APC_CAL_STATUS_PASSED = (1ULL << 2),
	APC_CAL_STATUS_FAILED = (1ULL << 3),
	APC_CAL_STATUS_REFUSED = (1ULL << 4),
	APC_CAL_STATUS_ABORTED = (1ULL << 5),
	APC_CAL_STATUS_INVALID_STATE = (1ULL << 9),
	APC_CAL_STATUS_INTERNAL_FAULT = (1ULL << 10),
	APC_CAL_STATUS_STATE_OF_CHARGE_NOT_ACCEPTABLE = (1ULL << 11),
	APC_CAL_STATUS_LOAD_CHANGE = (1ULL << 12),
	APC_CAL_STATUS_AC_INPUT_NOT_ACCEPTABLE = (1ULL << 13),
	APC_CAL_STATUS_LOAD_TOO_LOW = (1ULL << 14),
	APC_CAL_STATUS_OVER_CHARGE_IN_PROGRESS = (1ULL << 15)
} apc_calibration_status_bits_t;

/*
 * APC outlets and command bit definitions are shared across protocol variants.
 * Keep these values protocol-agnostic so both drivers can use the same
 * semantic names while mapping them to different transport encodings.
 */
typedef enum apc_outlet_command_bits_e {
	APC_OUTLET_CMD_CANCEL = (1ULL << 0),
	APC_OUTLET_CMD_OUTPUT_ON = (1ULL << 1),
	APC_OUTLET_CMD_OUTPUT_OFF = (1ULL << 2),
	APC_OUTLET_CMD_OUTPUT_SHUTDOWN = (1ULL << 3),
	APC_OUTLET_CMD_OUTPUT_REBOOT = (1ULL << 4),
	APC_OUTLET_CMD_COLD_BOOT_ALLOWED = (1ULL << 5),
	APC_OUTLET_CMD_USE_ON_DELAY = (1ULL << 6),
	APC_OUTLET_CMD_USE_OFF_DELAY = (1ULL << 7),
	APC_OUTLET_CMD_TARGET_MAIN = (1ULL << 8),
	APC_OUTLET_CMD_TARGET_SWITCHED0 = (1ULL << 9),
	APC_OUTLET_CMD_TARGET_SWITCHED1 = (1ULL << 10),
	APC_OUTLET_CMD_TARGET_SWITCHED2 = (1ULL << 11),
	APC_OUTLET_CMD_SOURCE_USB_PORT = (1ULL << 12),
	APC_OUTLET_CMD_SOURCE_LOCAL_USER = (1ULL << 13),
	APC_OUTLET_CMD_SOURCE_RJ45_PORT = (1ULL << 14),
	APC_OUTLET_CMD_SOURCE_SMART_SLOT_1 = (1ULL << 15),
	APC_OUTLET_CMD_SOURCE_SMART_SLOT_2 = (1ULL << 16),
	APC_OUTLET_CMD_SOURCE_INTERNAL_NETWORK_1 = (1ULL << 17),
	APC_OUTLET_CMD_SOURCE_INTERNAL_NETWORK_2 = (1ULL << 18),
	APC_OUTLET_CMD_TARGET_SWITCHED3 = (1ULL << 19)
} apc_outlet_command_bits_t;

typedef enum apc_ups_command_bits_e {
	APC_UPS_CMD_RESTORE_FACTORY_SETTINGS = (1ULL << 3),
	APC_UPS_CMD_OUTPUT_INTO_BYPASS = (1ULL << 4),
	APC_UPS_CMD_OUTPUT_OUT_OF_BYPASS = (1ULL << 5),
	APC_UPS_CMD_CLEAR_FAULTS = (1ULL << 9),
	APC_UPS_CMD_RESET_STRINGS = (1ULL << 13),
	APC_UPS_CMD_RESET_LOGS = (1ULL << 14),
	APC_UPS_CMD_LOCAL_USER = (1ULL << 29),
	APC_UPS_CMD_SMART_SLOT_1 = (1ULL << 30)
} apc_ups_command_bits_t;

typedef enum apc_battery_test_command_bits_e {
	APC_BATTERY_TEST_CMD_START = (1ULL << 0),
	APC_BATTERY_TEST_CMD_ABORT = (1ULL << 1),
	APC_BATTERY_TEST_CMD_LOCAL_USER = (1ULL << 9)
} apc_battery_test_command_bits_t;

typedef enum apc_runtime_calibration_command_bits_e {
	APC_RUNTIME_CAL_CMD_START = (1ULL << 0),
	APC_RUNTIME_CAL_CMD_ABORT = (1ULL << 1),
	APC_RUNTIME_CAL_CMD_LOCAL_USER = (1ULL << 9)
} apc_runtime_calibration_command_bits_t;

typedef enum apc_user_interface_command_bits_e {
	APC_USER_IF_CMD_SHORT_TEST = (1ULL << 0),
	APC_USER_IF_CMD_CONTINUOUS_TEST = (1ULL << 1),
	APC_USER_IF_CMD_MUTE_ALL_ACTIVE_AUDIBLE_ALARMS = (1ULL << 2),
	APC_USER_IF_CMD_CANCEL_MUTE = (1ULL << 3),
	APC_USER_IF_CMD_ACKNOWLEDGE_BATTERY_ALARMS = (1ULL << 5),
	APC_USER_IF_CMD_ACKNOWLEDGE_SITE_WIRING_ALARM = (1ULL << 6)
} apc_user_interface_command_bits_t;

typedef enum apc_outlet_command_type_e {
	APC_OUTLET_OP_NULL = 0,
	APC_OUTLET_OP_LOAD_OFF,
	APC_OUTLET_OP_LOAD_ON,
	APC_OUTLET_OP_LOAD_CYCLE,
	APC_OUTLET_OP_LOAD_OFF_DELAY,
	APC_OUTLET_OP_LOAD_ON_DELAY,
	APC_OUTLET_OP_SHUTDOWN_RETURN,
	APC_OUTLET_OP_SHUTDOWN_STAYOFF,
	APC_OUTLET_OP_SHUTDOWN_REBOOT,
	APC_OUTLET_OP_SHUTDOWN_REBOOT_GRACEFUL
} apc_outlet_command_type_t;

typedef struct apc_outlet_command_suffix_s {
	const char *suffix;
	apc_outlet_command_type_t type;
} apc_outlet_command_suffix_t;

extern const apc_value_map_t apc_countdown_map[];
extern const apc_value_map_t apc_countdown_setting_map[];
extern const apc_value_map_t apc_status_map[];
extern const apc_value_map_t apc_alarm_map[];
extern const apc_value_map_t apc_input_quality_map[];
extern const apc_value_map_t apc_efficiency_map[];
extern const apc_value_map_t apc_test_status_map[];
extern const apc_value_map_t apc_calibration_status_map[];
extern const apc_outlet_command_suffix_t apc_outlet_command_suffixes[];

int apc_format_countdown_value(int64_t value, char *output, size_t output_len);
int apc_format_date_from_days_offset(int64_t value, char *output, size_t output_len);
int apc_parse_date_to_days_offset(const char *value, uint64_t *days_out);
int apc_format_test_status_value(const apc_value_map_t *result_map, const apc_value_map_t *source_map,
	const apc_value_map_t *modifier_map, uint64_t value, char *output, size_t output_len);
uint64_t apc_build_outlet_command(apc_outlet_command_type_t type, uint64_t target_bits);
const char *apc_lookup_value_map_text(const apc_value_map_t *map, int value);
const char *apc_lookup_first_set_value_map_text(const apc_value_map_t *map, uint64_t value);

#endif /* APC_COMMON_H */
