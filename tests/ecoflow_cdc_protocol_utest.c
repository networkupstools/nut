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
#include "nut_float.h"
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

static uint16_t load_le16(const uint8_t *data)
{
	return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static void store_le32(uint8_t *data, uint32_t value)
{
	data[0] = (uint8_t)(value & 0xffU);
	data[1] = (uint8_t)((value >> 8) & 0xffU);
	data[2] = (uint8_t)((value >> 16) & 0xffU);
	data[3] = (uint8_t)((value >> 24) & 0xffU);
}

static void refresh_crc(uint8_t *frame, size_t length)
{
	uint16_t crc = ecoflow_cdc_crc16(frame, length - 2);

	frame[length - 2] = (uint8_t)(crc & 0xffU);
	frame[length - 1] = (uint8_t)((crc >> 8) & 0xffU);
}

static uint8_t *find_segment_data(uint8_t *frame, size_t length,
	uint16_t wanted_type, size_t wanted_length)
{
	size_t offset = 22;
	size_t payload_end;

	if (length < offset + 2)
		return NULL;
	payload_end = length - 2;
	while (offset < payload_end) {
		uint16_t type;
		size_t segment_length;

		if (payload_end - offset < 3)
			return NULL;
		type = load_le16(frame + offset);
		segment_length = frame[offset + 2];
		offset += 3;
		if (segment_length > payload_end - offset)
			return NULL;
		if (type == wanted_type && segment_length == wanted_length)
			return frame + offset;
		offset += segment_length;
	}

	return NULL;
}

static int parse_modified_float(const uint8_t *source, size_t length,
	uint16_t type, uint32_t value, ecoflow_cdc_metrics_t *metrics)
{
	uint8_t frame[ECOFLOW_CDC_MAX_FRAME_SIZE];
	uint8_t *segment_data;

	if (length > sizeof(frame))
		return -8;
	memcpy(frame, source, length);
	segment_data = find_segment_data(frame, length, type, 4);
	if (segment_data == NULL)
		return -8;

	/* The captured response uses sequence zero, so segment bytes are not
	 * changed by the protocol's sequence-byte XOR encoding. */
	store_le32(segment_data, value);
	refresh_crc(frame, length);
	return ecoflow_cdc_parse_frame(frame, length, 0, metrics);
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
	check(metrics.has_system_temperature && f_equal_1e_2(metrics.system_temperature, 26.0),
		"system temperature parses");
	check(metrics.has_battery_temperature && f_equal_1e_2(metrics.battery_temperature, 28.0),
		"battery temperature parses");
	check(metrics.has_output_power && f_equal_1e_2(metrics.output_power, 258.768),
		"total output power parses");
	check(metrics.has_input_power && f_equal_1e_2(metrics.input_power, 29.204),
		"total input power parses");
	check(metrics.has_solar_input_power && f_equal_1e_2(metrics.solar_input_power, 29.204),
		"solar input power parses");
	check(metrics.has_rated_output_power && metrics.rated_output_power == 600,
		"rated output power parses");
	check(metrics.has_ac_output_power && f_equal_1e_2(metrics.ac_output_power, 258.768),
		"AC output power parses as an absolute value");
	check(metrics.has_ac_output_frequency && f_equal_1e_2(metrics.ac_output_frequency, 60.0),
		"AC output frequency parses");
	check(!metrics.has_charging_runtime, "not-charging sentinel is not published");
	check(metrics.has_ems_version && metrics.ems_version[0] == 0x23 &&
		metrics.ems_version[3] == 0x02, "EMS version bytes parse");

	parsed = parse_modified_float(response, response_length, 7, 0xc3816254U, &metrics);
	check(parsed == 0, "negative finite response parses");
	check(metrics.has_output_power && f_equal_1e_2(metrics.output_power, -258.768),
		"signed power remains signed");

	parsed = parse_modified_float(response, response_length, 7, 0x7fc00000U, &metrics);
	check(parsed == 0, "response containing NaN parses");
	check(!metrics.has_output_power, "NaN power is not published");

	parsed = parse_modified_float(response, response_length, 8, 0x7f800000U, &metrics);
	check(parsed == 0, "response containing positive infinity parses");
	check(!metrics.has_input_power, "positive infinity is not published");

	parsed = parse_modified_float(response, response_length, 14, 0xff800000U, &metrics);
	check(parsed == 0, "response containing negative infinity parses");
	check(!metrics.has_ac_output_power, "negative infinity is not published");

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
