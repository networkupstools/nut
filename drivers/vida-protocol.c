/* vida-protocol.c - validate VIDA Armour's string-descriptor telemetry.
 *
 * The ARM3000 overloads other descriptor indices with operational commands.
 * The transport uses a closed query enum, not an arbitrary command string.
 * Parse recordings through these same production functions in unit tests.
 * Copyright (C) 2026 NUT contributors. GPL version 2 or later.
 */
#include "config.h"
#include "common.h"
#include "str.h"
#include "vida-protocol.h"

int vida_query_value(vida_query_t query, uint16_t *value)
{
	if (!value) {
		return 0;
	}
	/* Reject out-of-enum tags too; dispatch integers so Clang's exhaustive-enum
	 * and required-default diagnostics agree about that explicit error path. */
	switch ((int)query) {
	case VIDA_STATUS:
		*value = 0x0303;
		return 1;
	case VIDA_RATINGS:
		*value = 0x030d;
		return 1;
	case VIDA_CHARGE:
		*value = 0x03f3;
		return 1;
	case VIDA_FIRMWARE:
		*value = 0x030c;
		return 1;
	case VIDA_QUERY_COUNT:
	default:
		break;
	}
	return 0;
}

int vida_decode_string(const unsigned char *raw, size_t length,
	char *text, size_t capacity)
{
	size_t i, declared, characters;
	if (!raw || !text || length < 4 || raw[1] != 3) {
		return 0;
	}
	declared = raw[0];
	if (declared < 4 || declared > length || declared % 2) {
		return 0;
	}
	characters = (declared - 2) / 2;
	if (characters >= capacity) {
		return 0;
	}
	for (i = declared; i < length; i++) {
		if (raw[i]) {
			return 0;
		}
	}
	for (i = 0; i < characters; i++) {
		unsigned char character = raw[2 + 2 * i];
		if (raw[3 + 2 * i] || (i + 1 == characters ? character != '\r' : character < 32 || character > 126)) {
			return 0;
		}
		text[i] = (char)character;
	}
	text[characters - 1] = '\0';
	return 1;
}

/* Split bounded fields without accepting extra trailing values. */
static int vida_fields(const char *text, char prefix, size_t expected,
	char fields[][32])
{
	size_t field, length;
	const char *start;
	if (!text || *text++ != prefix) {
		return 0;
	}
	for (field = 0; field < expected; field++) {
		while (*text == ' ') {
			text++;
		}
		start = text;
		while (*text && *text != ' ') {
			text++;
		}
		length = (size_t)(text - start);
		if (!length || length >= 32) {
			return 0;
		}
		memcpy(fields[field], start, length);
		fields[field][length] = '\0';
	}
	while (*text == ' ') {
		text++;
	}
	return *text == '\0';
}

static int vida_number(const char *text, double low, double high, double *value)
{
	/* Restrict to decimal data before using NUT's checked numeric helper. */
	if (!*text || strspn(text, "-.0123456789") != strlen(text)
	|| !str_to_double_strict(text, value, 10)) {
		return 0;
	}
	return *value >= low && *value <= high;
}

int vida_parse_record(vida_query_t query, const char *text, vida_sample_t *sample)
{
	char fields[8][32];
	size_t i;
	if (!text || !sample) {
		return 0;
	}
	switch ((int)query) {
	case VIDA_STATUS:
		if (!vida_fields(text, '(', 8, fields)
		|| strlen(fields[7]) != 8 || strspn(fields[7], "01") != 8
		|| !vida_number(fields[0], 0, 500, &sample->input_voltage)
		|| !vida_number(fields[1], 0, 500, &sample->fault_voltage)
		|| !vida_number(fields[2], 0, 500, &sample->output_voltage)
		|| !vida_number(fields[3], 0, 150, &sample->load)
		|| !vida_number(fields[4], 0, 100, &sample->frequency)
		|| !vida_number(fields[5], 0, 100, &sample->battery_voltage)
		|| !vida_number(fields[6], -40, 150, &sample->temperature)) {
			return 0;
		}
		memcpy(sample->flags, fields[7], 9);
		return 1;
	case VIDA_RATINGS:
		return vida_fields(text, '#', 4, fields)
			&& vida_number(fields[0], 0, 500, &sample->nominal_voltage)
			&& vida_number(fields[1], 0, 100, &sample->nominal_current)
			&& vida_number(fields[2], 0, 100, &sample->nominal_battery_voltage)
			&& vida_number(fields[3], 0, 100, &sample->nominal_frequency);
	case VIDA_CHARGE:
		return strlen(text) == 5 && text[0] == 'B' && text[1] == 'L'
			&& strspn(text + 2, "0123456789") == 3
			&& vida_number(text + 2, 0, 100, &sample->charge);
	case VIDA_FIRMWARE:
		if (!vida_fields(text, '#', 1, fields) || fields[0][0] != 'V') {
			return 0;
		}
		for (i = 1; fields[0][i]; i++) {
			if (!strchr("0123456789.", fields[0][i])) {
				return 0;
			}
		}
		if (i < 2) {
			return 0;
		}
		memcpy(sample->firmware, fields[0], i + 1);
		return 1;
	case VIDA_QUERY_COUNT:
	default:
		break;
	}
	return 0;
}
