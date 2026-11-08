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

ssize_t command_read_sequence(unsigned char command, unsigned char *data)
{
	NUT_UNUSED_VARIABLE(command);
	NUT_UNUSED_VARIABLE(data);
	return -1;
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

int main(void)
{
	unsigned char map[] = { 0x05 };
	unsigned char future_map[BCMXCP_ALARM_MAP_MAX / 8] = { 0 };
	unsigned char answer[12] = { 0 };
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

	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
