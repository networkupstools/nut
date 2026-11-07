/* bcmxcp_limits_utest.c - unit tests for BCM/XCP Extended Limits parsing
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

static const unsigned char short_limits_response[] = {
	0xe6, 0x00, 0x32, 0x00, 0x06, 0x00, 0x43, 0x30,
	0xf4, 0x01, 0xcf, 0x00, 0xfa, 0x00, 0x00, 0x00,
	0x01, 0x01, 0xb2, 0x00, 0x13, 0x01
};

ssize_t command_read_sequence(unsigned char command, unsigned char *data)
{
	if (command != PW_LIMIT_BLOCK_REQ)
		return -1;

	memset(data, 0xdc, PW_ANSWER_MAX_SIZE);
	memcpy(data, short_limits_response, sizeof(short_limits_response));
	return (ssize_t)sizeof(short_limits_response);
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

static int check_value(const char *name, const char *expected)
{
	const char *actual = dstate_getinfo(name);

	if (actual && !strcmp(actual, expected))
		return 0;

	fprintf(stderr, "FAIL: %s should be %s, got %s\n",
		name, expected, actual ? actual : "(null)");
	return 1;
}

static int check_absent(const char *name)
{
	const char *actual = dstate_getinfo(name);

	if (!actual)
		return 0;

	fprintf(stderr, "FAIL: %s should be absent, got %s\n", name, actual);
	return 1;
}

int main(void)
{
	int failures = 0;

	bcmxcp_status.shutdowndelay = 0;
	init_limit();

	failures += check_value("input.voltage.nominal", "230");
	failures += check_value("input.frequency.nominal", "50");
	failures += check_value("input.frequency.low", "45");
	failures += check_value("input.frequency.high", "55");
	failures += check_value("input.transfer.boost.high", "207");
	failures += check_value("input.transfer.trim.low", "250");
	failures += check_value("input.transfer.low", "178");
	failures += check_value("input.transfer.high", "275");
	failures += check_value("ups.beeper.status", "enabled");
	failures += check_value("battery.runtime.low", "1");

	failures += check_absent("input.transfer.delay");
	failures += check_absent("battery.charge.restart");
	failures += check_absent("ambient.temperature.low");
	failures += check_absent("ambient.temperature.high");
	failures += check_absent("battery.energysave.load");
	failures += check_absent("battery.energysave.delay");

	if (bcmxcp_status.lowbatt != 60) {
		fprintf(stderr, "FAIL: low battery warning should be 60 seconds, got %u\n",
			bcmxcp_status.lowbatt);
		failures++;
	}

	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
