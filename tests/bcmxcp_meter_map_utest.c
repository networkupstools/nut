/* bcmxcp_meter_map_utest.c - unit tests for BCM/XCP meter mappings
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
	int failures = 0;

	nphases = 1;
	init_meter_map();

	failures += check(
		bcmxcp_meter_map[BCMXCP_METER_MAP_LOW_BATTERY_WARNING_V_BAR_CHART].nut_entity == NULL,
		"low-battery warning voltage should remain unmapped");

	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
