/* bcmxcp_runtime_low_utest.c - unit tests for BCM/XCP runtime-low units
 *
 * Copyright (C) 2026 Network UPS Tools contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "main.h"

unsigned int bcmxcp_test_sleep(unsigned int seconds);
#ifdef sleep
#undef sleep
#endif
#define sleep bcmxcp_test_sleep
#include "bcmxcp.c"
#undef sleep

upsdrv_info_t comm_upsdrv_info = {
	"BCMXCP test transport",
	"0.01",
	NULL,
	0,
	{ NULL }
};

static unsigned char last_command[5];
static size_t last_command_length;
static int authorization_count;
static int command_write_count;
static int comm_good_count;

unsigned int bcmxcp_test_sleep(unsigned int seconds)
{
	NUT_UNUSED_VARIABLE(seconds);
	return 0;
}

ssize_t command_read_sequence(unsigned char command, unsigned char *data)
{
	if (command != PW_LIMIT_BLOCK_REQ)
		return -1;

	memset(data, 0, 31);
	data[BCMXCP_EXT_LIMITS_BLOCK_LOW_BATT_WARNING] = 2;
	return 31;
}

ssize_t command_write_sequence(unsigned char *command, size_t command_length, unsigned char *answer)
{
	command_write_count++;
	last_command_length = command_length;
	if (command_length > sizeof(last_command))
		return -1;
	memcpy(last_command, command, command_length);

	if (command_length == 4 && command[0] == PW_SET_CONF_COMMAND && command[1] == PW_CONF_REQ) {
		memset(answer, 0, 4);
		answer[3] = PW_CONF_LOW_BATT;
		return 4;
	}

	answer[0] = BCMXCP_RETURN_ACCEPTED;
	return 1;
}

void send_read_command(unsigned char command)
{
	NUT_UNUSED_VARIABLE(command);
}

void send_write_command(unsigned char *command, size_t command_length)
{
	if (command_length == 4 && !memcmp(command, AUTHOR, 4))
		authorization_count++;
}

ssize_t get_answer(unsigned char *data, unsigned char command)
{
	NUT_UNUSED_VARIABLE(data);
	NUT_UNUSED_VARIABLE(command);
	return -1;
}

void upsdrv_comm_good(void)
{
	comm_good_count++;
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

static void reset_transport(void)
{
	memset(last_command, 0, sizeof(last_command));
	last_command_length = 0;
	authorization_count = 0;
	command_write_count = 0;
	comm_good_count = 0;
}

static int check_setvar(const char *seconds, int expected_result, int expected_minutes)
{
	int failures = 0;
	int result;

	reset_transport();
	result = setvar("battery.runtime.low", seconds);
	failures += check(result == expected_result, "setvar returned an unexpected result");

	if (expected_result == STAT_SET_HANDLED) {
		failures += check(authorization_count == 1, "valid input should authorize once");
		failures += check(command_write_count == 1, "valid input should write once");
		failures += check(comm_good_count == 1, "accepted write should mark communication good");
		failures += check(last_command_length == 4, "setting command should contain four bytes");
		failures += check(last_command[0] == PW_SET_CONF_COMMAND, "setting command should use configuration opcode");
		failures += check(last_command[1] == PW_CONF_LOW_BATT, "setting command should select low-battery warning");
		failures += check(last_command[2] == expected_minutes, "seconds should convert to whole protocol minutes");
		failures += check(last_command[3] == 0, "setting command high byte should be zero");
	} else {
		failures += check(authorization_count == 0, "invalid input must not authorize");
		failures += check(command_write_count == 0, "invalid input must not write");
	}

	return failures;
}

int main(void)
{
	const st_tree_t *entry;
	const char *runtime_low;
	int failures = 0;
	const char *invalid_values[] = { "-60", "1", "59", "61", "1799", "1801" };
	const char *malformed_values[] = { "", "abc", "1.5", "60s" };
	size_t i;

	bcmxcp_status.shutdowndelay = 0;
	init_limit();
	runtime_low = dstate_getinfo("battery.runtime.low");
	failures += check(runtime_low && !strcmp(runtime_low, "120"),
		"two protocol minutes should publish as 120 seconds");
	failures += check(bcmxcp_status.lowbatt == 120,
		"internal low-battery threshold should remain in seconds");

	reset_transport();
	init_ext_vars();
	entry = dstate_tree_find("battery.runtime.low");
	failures += check(entry != NULL, "runtime-low writable metadata should exist");
	if (entry) {
		failures += check((entry->flags & (ST_FLAG_RW | ST_FLAG_STRING)) == (ST_FLAG_RW | ST_FLAG_STRING),
			"runtime-low should remain a writable string");
		failures += check(entry->aux == 4, "runtime-low should allow four second digits");
	}

	failures += check_setvar("0", STAT_SET_HANDLED, 0);
	failures += check_setvar("60", STAT_SET_HANDLED, 1);
	failures += check_setvar("1800", STAT_SET_HANDLED, 30);

	for (i = 0; i < sizeof(invalid_values) / sizeof(invalid_values[0]); i++)
		failures += check_setvar(invalid_values[i], STAT_SET_INVALID, 0);

	for (i = 0; i < sizeof(malformed_values) / sizeof(malformed_values[0]); i++)
		failures += check_setvar(malformed_values[i], STAT_SET_CONVERSION_FAILED, 0);

	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
