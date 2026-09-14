/* ecoflow-cdc-protocol.c - read-only EcoFlow CDC telemetry protocol
 *
 * Copyright (C) 2026 Network UPS Tools contributors
 *
 * Protocol framing was documented by the r3pcomms project and independently
 * verified against an EcoFlow RIVER 3 Plus.  Only the read-only telemetry
 * request is implemented here.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "ecoflow-cdc-protocol.h"
#include "nut_float.h"

#include <string.h>

#define ECOFLOW_CDC_PREAMBLE 0x03aaU
#define ECOFLOW_CDC_XOR_OFFSET 18U
#define ECOFLOW_CDC_SEGMENT_OFFSET 22U
#define ECOFLOW_CDC_FLOAT_SIGN 0x80000000U
#define ECOFLOW_CDC_FLOAT_EXPONENT 0x7f800000U
#define ECOFLOW_CDC_FLOAT_FRACTION 0x007fffffU
#define ECOFLOW_CDC_FLOAT_IMPLICIT_BIT 0x00800000U

static uint16_t load_le16(const uint8_t *data)
{
	return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t load_le32(const uint8_t *data)
{
	return (uint32_t)data[0] |
	       ((uint32_t)data[1] << 8) |
	       ((uint32_t)data[2] << 16) |
	       ((uint32_t)data[3] << 24);
}

static void store_le16(uint8_t *data, uint16_t value)
{
	data[0] = (uint8_t)(value & 0xffU);
	data[1] = (uint8_t)((value >> 8) & 0xffU);
}

static void store_le32(uint8_t *data, uint32_t value)
{
	data[0] = (uint8_t)(value & 0xffU);
	data[1] = (uint8_t)((value >> 8) & 0xffU);
	data[2] = (uint8_t)((value >> 16) & 0xffU);
	data[3] = (uint8_t)((value >> 24) & 0xffU);
}

static int load_le_float(const uint8_t *data, double *value)
{
	uint32_t bits = load_le32(data);
	uint32_t fraction = bits & ECOFLOW_CDC_FLOAT_FRACTION;
	int exponent = (int)((bits & ECOFLOW_CDC_FLOAT_EXPONENT) >> 23);
	double decoded;

	/* EcoFlow encodes these values as IEEE-754 binary32.  Decode the wire
	 * representation directly so parsing does not depend on the host's
	 * isfinite() implementation or native float representation. */
	if (exponent == 0xff)
		return 0;
	if (exponent == 0)
		decoded = ldexp((double)fraction, -149);
	else
		decoded = ldexp((double)(fraction | ECOFLOW_CDC_FLOAT_IMPLICIT_BIT),
			exponent - 150);
	if ((bits & ECOFLOW_CDC_FLOAT_SIGN) != 0U)
		decoded = -decoded;

	*value = decoded;
	return 1;
}

static int load_le_float_absolute(const uint8_t *data, double *value)
{
	if (!load_le_float(data, value))
		return 0;
	*value = fabs(*value);
	return 1;
}

uint16_t ecoflow_cdc_crc16(const uint8_t *data, size_t length)
{
	uint16_t crc = 0;
	size_t i;

	for (i = 0; i < length; i++) {
		unsigned int bit;
		crc ^= data[i];
		for (bit = 0; bit < 8; bit++) {
			crc = (crc & 1U) ? (uint16_t)((crc >> 1) ^ 0xa001U) :
				(uint16_t)(crc >> 1);
		}
	}

	return crc;
}

size_t ecoflow_cdc_build_request(uint32_t sequence, uint8_t *buffer, size_t size)
{
	static const uint8_t command_tail[] = {
		0xff, 0xff, 0x22, 0x02, 0x01, 0x01, 0x66, 0x02
	};
	uint16_t crc;

	if (buffer == NULL || size < ECOFLOW_CDC_REQUEST_SIZE)
		return 0;

	store_le16(buffer, ECOFLOW_CDC_PREAMBLE);
	store_le16(buffer + 2, 0);
	buffer[4] = 0xde;
	buffer[5] = 0x2d;
	store_le32(buffer + 6, sequence);
	memcpy(buffer + 10, command_tail, sizeof(command_tail));
	crc = ecoflow_cdc_crc16(buffer, ECOFLOW_CDC_REQUEST_SIZE - 2);
	store_le16(buffer + ECOFLOW_CDC_REQUEST_SIZE - 2, crc);

	return ECOFLOW_CDC_REQUEST_SIZE;
}

static void parse_segment(uint16_t type, const uint8_t *data, size_t length,
	ecoflow_cdc_metrics_t *metrics)
{
	switch (type) {
	case 3:
		if (length == 4) {
			metrics->has_design_capacity_mah = 1;
			metrics->design_capacity_mah = load_le32(data);
		}
		break;
	case 4:
		if (length >= 2) {
			metrics->has_system_temperature = 1;
			metrics->system_temperature = data[0];
			metrics->has_battery_temperature = 1;
			metrics->battery_temperature = data[1];
		}
		break;
#define ECOFLOW_PARSE_FLOAT(segment, member, loader) \
	case segment: \
		if (length == 4 \
		 && loader(data, &metrics->member) \
		) { \
			metrics->has_##member = 1; \
		} \
		break
	ECOFLOW_PARSE_FLOAT(7, output_power, load_le_float);
	ECOFLOW_PARSE_FLOAT(8, input_power, load_le_float);
	ECOFLOW_PARSE_FLOAT(9, ac_input_power, load_le_float);
	ECOFLOW_PARSE_FLOAT(11, ac_input_voltage, load_le_float);
	ECOFLOW_PARSE_FLOAT(12, solar_input_power, load_le_float);
	ECOFLOW_PARSE_FLOAT(14, ac_output_power, load_le_float_absolute);
	ECOFLOW_PARSE_FLOAT(16, dc_output_power, load_le_float_absolute);
	ECOFLOW_PARSE_FLOAT(17, usb_a_output_power, load_le_float_absolute);
	ECOFLOW_PARSE_FLOAT(18, usb_c_output_power, load_le_float_absolute);
	ECOFLOW_PARSE_FLOAT(20, extra_battery_input_power, load_le_float);
	ECOFLOW_PARSE_FLOAT(21, extra_battery_output_power, load_le_float_absolute);
#undef ECOFLOW_PARSE_FLOAT
	case 10:
		if (length == 4) {
			metrics->has_ac_input_frequency = 1;
			metrics->ac_input_frequency = load_le16(data);
		}
		break;
	case 13:
		if (length == 4) {
			metrics->has_rated_output_power = 1;
			metrics->rated_output_power = load_le32(data);
		}
		break;
	case 15:
		if (length == 4) {
			metrics->has_ac_output_frequency = 1;
			metrics->ac_output_frequency = load_le16(data);
		}
		break;
	case 23:
		if (length >= 2 && load_le16(data) != 0x1733U) {
			metrics->has_charging_runtime = 1;
			metrics->charging_runtime = (uint32_t)load_le16(data) * 60U;
		}
		break;
	case 25:
		if (length == 4) {
			metrics->has_ems_version = 1;
			memcpy(metrics->ems_version, data, 4);
		}
		break;
	default:
		break;
	}
}

int ecoflow_cdc_parse_frame(const uint8_t *frame, size_t length,
	uint32_t expected_sequence, ecoflow_cdc_metrics_t *metrics)
{
	uint8_t decoded[ECOFLOW_CDC_MAX_FRAME_SIZE];
	uint32_t sequence;
	size_t expected_length, offset, payload_end;

	if (frame == NULL || metrics == NULL || length < ECOFLOW_CDC_SEGMENT_OFFSET + 2 ||
	    length > sizeof(decoded))
		return -1;
	if (load_le16(frame) != ECOFLOW_CDC_PREAMBLE)
		return -2;

	expected_length = 20U + load_le16(frame + 2);
	if (expected_length != length)
		return -3;
	if (ecoflow_cdc_crc16(frame, length) != 0)
		return -4;

	sequence = load_le32(frame + 6);
	if (sequence != expected_sequence)
		return -5;

	memcpy(decoded, frame, length - 2);
	for (offset = ECOFLOW_CDC_XOR_OFFSET; offset < length - 2; offset++)
		decoded[offset] ^= (uint8_t)(sequence & 0xffU);

	memset(metrics, 0, sizeof(*metrics));
	offset = ECOFLOW_CDC_SEGMENT_OFFSET;
	payload_end = length - 2;
	while (offset < payload_end) {
		uint16_t type;
		size_t segment_length;

		if (payload_end - offset < 3)
			return -6;
		type = load_le16(decoded + offset);
		segment_length = decoded[offset + 2];
		offset += 3;
		if (segment_length > payload_end - offset)
			return -7;
		parse_segment(type, decoded + offset, segment_length, metrics);
		offset += segment_length;
	}

	return 0;
}
