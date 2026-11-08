/* bcmxcp_alarm_utest.c - unit tests for BCM/XCP alarm parsing
 *
 * Copyright (C) 2026 Network UPS Tools contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include "bcmxcp.c"

upsdrv_info_t comm_upsdrv_info = {
	"BCMXCP test transport",
	"0.01",
	NULL,
	0,
	{ NULL }
};

static ssize_t mock_alarm_len = -1;
static unsigned char mock_alarm_answer[2];
static unsigned char mock_status = BCMXCP_STATUS_ONLINE;

ssize_t command_read_sequence(unsigned char command, unsigned char *data)
{
	switch (command) {
		case PW_METER_BLOCK_REQ:
			memset(data, 0, PW_ANSWER_MAX_SIZE);
			return PW_ANSWER_MAX_SIZE;
		case PW_CUR_ALARM_REQ:
			if (mock_alarm_len > 0)
				memcpy(data, mock_alarm_answer, (size_t)mock_alarm_len);
			return mock_alarm_len;
		case PW_STATUS_REQ:
			data[0] = mock_status;
			data[1] = 0;
			return 2;
		default:
			return -1;
	}
}

ssize_t command_write_sequence(unsigned char *command, size_t command_length, unsigned char *answer)
{
	NUT_UNUSED_VARIABLE(command);
	NUT_UNUSED_VARIABLE(command_length);
	NUT_UNUSED_VARIABLE(answer);
	return -1;
}

void send_read_command(unsigned char command)
{
	NUT_UNUSED_VARIABLE(command);
}

void send_write_command(unsigned char *command, size_t command_length)
{
	NUT_UNUSED_VARIABLE(command);
	NUT_UNUSED_VARIABLE(command_length);
}

ssize_t get_answer(unsigned char *data, unsigned char command)
{
	NUT_UNUSED_VARIABLE(data);
	NUT_UNUSED_VARIABLE(command);
	return -1;
}

void upsdrv_comm_good(void)
{
}

void upsdrv_reconnect(void)
{
}

static int check(int condition, const char *message)
{
	if (condition)
		return 0;

	fprintf(stderr, "FAIL: %s\n", message);
	return 1;
}

static int check_contains(const char *value, const char *expected, const char *message)
{
	return check(value && strstr(value, expected), message);
}

int main(void)
{
	unsigned char map[] = { 0x05 };
	/* Eaton 5110i, captured 2026-11-08; ID serial/config data excluded. */
	const unsigned char captured_alarm_map[] = {
		0xc0, 0x01, 0x00, 0x02, 0x00, 0x00, 0x80, 0x0f,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x80
	};
	const unsigned char captured_online[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00
	};
	const unsigned char captured_on_battery[] = {
		0x00, 0x08, 0x08, 0x00, 0x00, 0x00,
		0x08, 0x00, 0x08, 0x08, 0x00
	};
	unsigned char future_map[BCMXCP_ALARM_MAP_MAX / 8] = { 0 };
	unsigned char answer[12] = { 0 };
	const char *alarms;
	const char *ups_status;
	int failures = 0;
	int i;

	init_alarm_map();
	for (i = 0; i < BCMXCP_ALARM_MAP_MAX; i++)
		failures += check(bcmxcp_alarm_map[i].alarm_block_index == -1,
			"alarm map entries must default to unsupported");

	init_ups_alarm_map(map, sizeof(map));
	failures += check(expected_alarm_block_len == 2,
		"alarm block length should equal the advertised map population");
	failures += check(bcmxcp_alarm_map[0].alarm_block_index == 0,
		"first advertised alarm should use block index 0");
	failures += check(bcmxcp_alarm_map[1].alarm_block_index == -1,
		"unadvertised alarm should remain unsupported");
	failures += check(bcmxcp_alarm_map[2].alarm_block_index == 1,
		"second advertised alarm should use block index 1");
	failures += check(bcmxcp_alarm_map[8].alarm_block_index == -1,
		"alarms beyond the advertised map must remain unsupported");

	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_NEEDS_SERVICE].alarm_block_index = 11;
	answer[11] = 1;
	failures += check(!alarm_is_active(
		&bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_NEEDS_SERVICE], answer, 11),
		"bytes beyond a short alarm response must not raise RB");
	failures += check(alarm_is_active(
		&bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_NEEDS_SERVICE], answer, 12),
		"an in-range active alarm should be reported");

	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_NEEDS_SERVICE].alarm_block_index = 1;
	answer[1] = 1;
	failures += check(update_alarms(answer, 2),
		"a complete alarm response should be committed");
	failures += check(bcmxcp_status.alarm_replace_battery == 1,
		"a complete active alarm should update derived status");

	answer[1] = 0;
	failures += check(!update_alarms(answer, 1),
		"a truncated alarm response should not be committed");
	failures += check(bcmxcp_status.alarm_replace_battery == 1,
		"a truncated response should preserve prior derived status");
	failures += check(update_alarms(answer, 2),
		"a later complete response should be committed");
	failures += check(bcmxcp_status.alarm_replace_battery == 0,
		"a complete inactive alarm should clear prior derived status");

	init_alarm_map();
	future_map[sizeof(future_map) - 1] = 0x80;
	init_ups_alarm_map(future_map, sizeof(future_map));
	failures += check(expected_alarm_block_len == 1,
		"unknown future map bits should contribute to the alarm block length");
	failures += check(update_alarms(answer, 1),
		"an exact response for an unknown future alarm should be accepted");
	failures += check(!update_alarms(answer, 2),
		"an overlong alarm response should not be committed");

	init_alarm_map();
	init_ups_alarm_map(NULL, 0);
	failures += check(expected_alarm_block_len == 0,
		"an empty map should advertise an empty alarm block");
	failures += check(update_alarms(NULL, 0),
		"an empty map should commit an empty alarm set");

	init_alarm_map();
	init_ups_alarm_map(map, sizeof(map));
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_NEEDS_SERVICE].alarm_block_index = 1;
	answer[1] = 1;
	update_alarms(answer, 2);
	status_init();
	status_set("OL");
	status_commit();

	mock_alarm_len = 1;
	mock_alarm_answer[0] = 0;
	mock_status = BCMXCP_STATUS_ONBATTERY;
	upsdrv_updateinfo();
	ups_status = dstate_getinfo("ups.status");
	failures += check(ups_status != NULL && strstr(ups_status, "OB") != NULL,
		"a malformed alarm snapshot should not suppress fresh status data");
	failures += check(ups_status != NULL && strstr(ups_status, "RB") != NULL,
		"a malformed alarm snapshot should preserve prior derived status");

	mock_alarm_len = 2;
	mock_alarm_answer[1] = 0;
	upsdrv_updateinfo();
	ups_status = dstate_getinfo("ups.status");
	failures += check(ups_status != NULL && strstr(ups_status, "OB") != NULL,
		"status polling should continue after alarm recovery");
	failures += check(ups_status != NULL && strstr(ups_status, "RB") == NULL,
		"a later complete alarm snapshot should clear prior derived status");

	init_alarm_map();
	init_ups_alarm_map(captured_alarm_map, sizeof(captured_alarm_map));
	failures += check(expected_alarm_block_len == sizeof(captured_online),
		"captured 5110i alarm map should advertise eleven statuses");
	failures += check(update_alarms(captured_online, sizeof(captured_online)),
		"captured online alarm response should be accepted");
	failures += check(dstate_getinfo("ups.alarm") == NULL,
		"captured online response should not publish alarms");

	failures += check(update_alarms(captured_on_battery, sizeof(captured_on_battery)),
		"captured on-battery alarm response should be accepted");
	alarms = dstate_getinfo("ups.alarm");
	failures += check_contains(alarms, "INPUT_AC_UNDER_VOLTAGE",
		"captured response should report input undervoltage");
	failures += check_contains(alarms, "INPUT_UNDER_OR_OVER_FREQ",
		"captured response should report input frequency outside limits");
	failures += check_contains(alarms, "UTILITY_FAIL",
		"captured response should report utility failure");
	failures += check_contains(alarms, "UTILITY_NOT_PRESENT",
		"captured response should report utility absence");
	failures += check_contains(alarms, "UPS_ON_BATTERY",
		"captured response should report operation on battery");
	failures += check(bcmxcp_status.alarm_on_battery == 1,
		"captured response should set derived on-battery status");
	failures += check(bcmxcp_status.alarm_low_battery == 0,
		"captured response should not invent low battery");
	failures += check(bcmxcp_status.alarm_replace_battery == 0,
		"captured response should not invent replace battery");

	failures += check(update_alarms(captured_online, sizeof(captured_online)),
		"captured return-to-line response should be accepted");
	failures += check(dstate_getinfo("ups.alarm") == NULL,
		"return to line should clear captured alarms");
	failures += check(bcmxcp_status.alarm_on_battery == 0,
		"return to line should clear derived on-battery status");

	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
