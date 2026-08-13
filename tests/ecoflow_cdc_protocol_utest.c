/* ecoflow_cdc_protocol_utest.c - tests for EcoFlow CDC framing and parsing
 *
 * Copyright (C) 2026 Network UPS Tools contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "ecoflow-cdc-protocol.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(int condition, const char *description)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", description);
		failures++;
	}
}

static size_t decode_hex(const char *hex, uint8_t *output, size_t output_size)
{
	size_t length = strlen(hex) / 2;
	size_t i;

	if ((strlen(hex) % 2) != 0 || length > output_size)
		return 0;
	for (i = 0; i < length; i++) {
		unsigned int value;
		if (sscanf(hex + i * 2, "%2x", &value) != 1)
			return 0;
		output[i] = (uint8_t)value;
	}
	return length;
}

int main(void)
{
	static const char request_hex[] =
		"aa030000de2d00000000ffff22020101660286ef";
	static const char response_hex[] =
		"aa03b800392f0000000001440222010166020101000002000400000000"
		"030004003200000400041a1c1919050004000000000600040000000007"
		"000454628143080004b7a1e941090004000000000a0004000000000b00"
		"04000000000c0004b7a1e9410d0004580200000e0004546281c30f0004"
		"3c00000010000400000000110004000000001200040000000013000400"
		"0000001400040000000015000400000000160010ffffffffffffffffffff"
		"ffffffffffff17000433170000180004000000001900042301000204a3";
	uint8_t request[ECOFLOW_CDC_REQUEST_SIZE];
	uint8_t expected_request[ECOFLOW_CDC_REQUEST_SIZE];
	uint8_t response[ECOFLOW_CDC_MAX_FRAME_SIZE];
	size_t response_length;
	ecoflow_cdc_metrics_t metrics;
	int parsed;

	check(ecoflow_cdc_build_request(0, request, sizeof(request)) == sizeof(request),
		"request has expected length");
	check(decode_hex(request_hex, expected_request, sizeof(expected_request)) ==
		sizeof(expected_request), "request fixture decodes");
	check(memcmp(request, expected_request, sizeof(request)) == 0,
		"request matches captured frame");
	check(ecoflow_cdc_crc16(request, sizeof(request)) == 0,
		"request CRC is valid");

	response_length = decode_hex(response_hex, response, sizeof(response));
	check(response_length == 204, "response fixture has expected length");
	check(ecoflow_cdc_crc16(response, response_length) == 0,
		"response CRC is valid");
	parsed = ecoflow_cdc_parse_frame(response, response_length, 0, &metrics);
	if (parsed != 0)
		fprintf(stderr, "Captured response parser error: %d\n", parsed);
	check(parsed == 0, "captured response parses");
	check(metrics.has_design_capacity_mah && metrics.design_capacity_mah == 12800,
		"design capacity parses");
	check(metrics.has_system_temperature && metrics.system_temperature == 26,
		"system temperature parses");
	check(metrics.has_battery_temperature && metrics.battery_temperature == 28,
		"battery temperature parses");
	check(metrics.has_output_power && fabs(metrics.output_power - 258.768) < 0.01,
		"total output power parses");
	check(metrics.has_input_power && fabs(metrics.input_power - 29.204) < 0.01,
		"total input power parses");
	check(metrics.has_solar_input_power && fabs(metrics.solar_input_power - 29.204) < 0.01,
		"solar input power parses");
	check(metrics.has_rated_output_power && metrics.rated_output_power == 600,
		"rated output power parses");
	check(metrics.has_ac_output_power && fabs(metrics.ac_output_power - 258.768) < 0.01,
		"AC output power parses as an absolute value");
	check(metrics.has_ac_output_frequency && metrics.ac_output_frequency == 60,
		"AC output frequency parses");
	check(!metrics.has_charging_runtime, "not-charging sentinel is not published");
	check(metrics.has_ems_version && metrics.ems_version[0] == 0x23 &&
		metrics.ems_version[3] == 0x02, "EMS version bytes parse");

	check(ecoflow_cdc_parse_frame(response, response_length, 1, &metrics) == -5,
		"sequence mismatch is rejected");
	response[response_length - 1] ^= 0x01;
	check(ecoflow_cdc_parse_frame(response, response_length, 0, &metrics) == -4,
		"CRC mismatch is rejected");

	if (failures != 0)
		return 1;
	printf("All EcoFlow CDC protocol tests passed\n");
	return 0;
}
