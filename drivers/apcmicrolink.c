/* apcmicrolink.c - APC Microlink protocol driver
 *
 * Copyright (C) 2026 Lukas Schmid <lukas.schmid@netcube.li>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "main.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "serial.h"
#include "nut_stdint.h"

#include "apcmicrolink.h"
#include "apcmicrolink-maps.h"

#define DRIVER_NAME	"APC Microlink protocol driver"
#define DRIVER_VERSION	"0.01"

upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Lukas Schmid <lukas.schmid@netcube.li>\n",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define MLINK_DEFAULT_BAUDRATE		B9600
#define MLINK_NEXT_BYTE			0xFE
#define MLINK_INIT_BYTE			0xFD
#define MLINK_HANDSHAKE_RETRIES	3
#define MLINK_READ_TIMEOUT_USEC	100000

static const struct {
	const char *value;
	speed_t speed;
} microlink_speed_table[] = {
#ifdef B1200
	{ "1200", B1200 },
#endif
#ifdef B2400
	{ "2400", B2400 },
#endif
#ifdef B4800
	{ "4800", B4800 },
#endif
	{ "9600", B9600 },
#ifdef B19200
	{ "19200", B19200 },
#endif
#ifdef B38400
	{ "38400", B38400 },
#endif
#ifdef B57600
	{ "57600", B57600 },
#endif
#ifdef B115200
	{ "115200", B115200 },
#endif
	{ NULL, MLINK_DEFAULT_BAUDRATE }
};

static microlink_object_t objects[256];
static speed_t microlink_baudrate = MLINK_DEFAULT_BAUDRATE;
static int session_ready = 0;
static unsigned char rxbuf[MLINK_MAX_FRAME * 2];
static size_t rxbuf_len = 0;
static unsigned int parsed_frames = 0;
static unsigned int consecutive_timeouts = 0;
static int poll_primed = 0;
static int authentication_sent = 0;
static microlink_page0_state_t page0;
static int descriptor_ready = 0;
static size_t descriptor_usage_count = 0;
static size_t descriptor_blob_len = 0;
static microlink_descriptor_usage_t descriptor_usages[MLINK_DESCRIPTOR_MAX_USAGES];
static unsigned char descriptor_blob[MLINK_DESCRIPTOR_MAX_BLOB];
static int show_internals = -1;
static int show_unmapped = -1;

static int microlink_send_simple(unsigned char byte);
static int microlink_send_write(unsigned char id, unsigned char offset,
	unsigned char len, const unsigned char *data);
static int microlink_update_blob(void);
static int microlink_parse_descriptor(void);
static int microlink_send_descriptor_mask_value(const char *path, uint64_t mask);
static int microlink_command_available(const microlink_desc_command_map_t *entry);
static const microlink_object_t *microlink_get_object(unsigned int id);
static microlink_object_t *microlink_get_object_mut(unsigned int id);
static size_t microlink_parse_descriptor_block(const unsigned char *blob, size_t blob_len,
	size_t pos, size_t *data_offset, const char *path);

static int microlink_parse_baudrate(const char *text, speed_t *baudrate)
{
	size_t i;

	if (text == NULL || baudrate == NULL) {
		return 0;
	}

	for (i = 0; microlink_speed_table[i].value != NULL; i++) {
		if (!strcmp(text, microlink_speed_table[i].value)) {
			*baudrate = microlink_speed_table[i].speed;
			return 1;
		}
	}

	return 0;
}

static int microlink_parse_bool(const char *text, int *value)
{
	if (text == NULL || value == NULL) {
		return 0;
	}

	if (!strcasecmp(text, "true") || !strcasecmp(text, "on")
	 || !strcasecmp(text, "yes") || !strcmp(text, "1")) {
		*value = 1;
		return 1;
	}

	if (!strcasecmp(text, "false") || !strcasecmp(text, "off")
	 || !strcasecmp(text, "no") || !strcmp(text, "0")) {
		*value = 0;
		return 1;
	}

	return 0;
}

static int microlink_show_unmapped(void)
{
	if (show_unmapped >= 0) {
		return show_unmapped;
	}

	return (nut_debug_level > 0);
}

static int microlink_show_internals(void)
{
	if (show_internals >= 0) {
		return show_internals;
	}

	return (nut_debug_level > 0);
}

static void microlink_read_config(void)
{
	const char *value;

	if (testvar("baudrate")) {
		value = getval("baudrate");
		if (!microlink_parse_baudrate(value, &microlink_baudrate)) {
			fatalx(EXIT_FAILURE, "apcmicrolink: invalid baudrate '%s'",
				value ? value : "");
		}
	}

	if (testvar("showunmapped")) {
		int parsed = 0;

		value = getval("showunmapped");
		if (value == NULL) {
			show_unmapped = 1;
		} else if (microlink_parse_bool(value, &parsed)) {
			show_unmapped = parsed;
		} else {
			fatalx(EXIT_FAILURE, "apcmicrolink: invalid showunmapped value '%s'",
				value);
		}
	}

	if (testvar("showinternals")) {
		int parsed = 0;

		value = getval("showinternals");
		if (value == NULL) {
			show_internals = 1;
		} else if (microlink_parse_bool(value, &parsed)) {
			show_internals = parsed;
		} else {
			fatalx(EXIT_FAILURE, "apcmicrolink: invalid showinternals value '%s'",
				value);
		}
	}
}

static int microlink_timeout_expired(const st_tree_timespec_t *start,
	time_t d_sec, useconds_t d_usec)
{
	st_tree_timespec_t now;
	double timeout = (double)d_sec + ((double)d_usec / 1000000.0);

	state_get_timestamp(&now);
	return difftime_st_tree_timespec(now, *start) >= timeout;
}

static int microlink_prime_poll(void)
{
	if (!microlink_send_simple(MLINK_NEXT_BYTE)) {
		ser_comm_fail("microlink: failed to send poll byte");
		poll_primed = 0;
		return 0;
	}

	poll_primed = 1;
	return 1;
}

static void microlink_trace_frame(int level, const char *label,
	const unsigned char *buf, size_t len)
{
	char msg[64];

	snprintf(msg, sizeof(msg), "microlink %s", label);
	upsdebug_hex(level, msg, buf, len);
}

static void microlink_checksum(const unsigned char *buf, size_t len,
	unsigned char *cb0, unsigned char *cb1)
{
	unsigned int c0 = 0;
	unsigned int c1 = 0;
	size_t i;

	for (i = 0; i < len; i++) {
		c0 = (c0 + buf[i]) % 255U;
		c1 = (c1 + c0) % 255U;
	}

	*cb0 = (unsigned char)(255U - ((c0 + c1) % 255U));
	*cb1 = (unsigned char)(255U - ((c0 + *cb0) % 255U));
}

static int microlink_checksum_valid(const unsigned char *frame, size_t len)
{
	unsigned char cb0, cb1;

	if (len < 3) {
		return 0;
	}

	microlink_checksum(frame, len - 2, &cb0, &cb1);
	return (frame[len - 2] == cb0 && frame[len - 1] == cb1);
}

static void microlink_format_hex(const unsigned char *buf, size_t len,
	char *out, size_t outlen)
{
	size_t i;
	size_t pos = 0;

	if (outlen == 0) {
		return;
	}

	out[0] = '\0';

	for (i = 0; i < len && pos + 3 < outlen; i++) {
		pos += snprintf(out + pos, outlen - pos, "%02X", buf[i]);
		if (i + 1 < len && pos + 2 < outlen) {
			out[pos++] = ' ';
			out[pos] = '\0';
		}
	}
}

static void microlink_format_ascii(const unsigned char *buf, size_t len,
	char *out, size_t outlen)
{
	size_t i;
	size_t pos = 0;

	if (outlen == 0) {
		return;
	}

	for (i = 0; i < len && pos + 1 < outlen; i++) {
		unsigned char ch = buf[i];

		if (ch == '\0') {
			continue;
		}

		if (isprint((int)ch)) {
			out[pos++] = (char)ch;
		}
	}

	while (pos > 0 && isspace((unsigned char)out[pos - 1])) {
		pos--;
	}

	out[pos] = '\0';
}

static const microlink_object_t *microlink_get_object(unsigned int id)
{
	return &objects[id & 0xFFU];
}

static microlink_object_t *microlink_get_object_mut(unsigned int id)
{
	return &objects[id & 0xFFU];
}

static int microlink_is_descriptor_operator(unsigned char token)
{
	return token >= 0xF4;
}

static int microlink_path_append(char *buf, size_t buflen, size_t *pos,
	const char *fmt, ...)
{
	va_list ap;
	int written;

	if (*pos >= buflen) {
		return 0;
	}

	va_start(ap, fmt);
	written = vsnprintf_dynamic(buf + *pos, buflen - *pos, fmt, fmt, ap);
	va_end(ap);

	if (written < 0 || (size_t)written >= buflen - *pos) {
		return 0;
	}

	*pos += (size_t)written;
	return 1;
}

static int microlink_build_usage_path(char *buf, size_t buflen, const char *path,
	unsigned char usage_id)
{
	size_t pos = 0;
	size_t pathlen = strlen(path);

	buf[0] = '\0';

	if (!microlink_path_append(buf, buflen, &pos, "%s", path)) {
		return 0;
	}

	if (pathlen > 0 && path[pathlen - 1] != ':' && path[pathlen - 1] != '.') {
		if (!microlink_path_append(buf, buflen, &pos, ".")) {
			return 0;
		}
	}

	return microlink_path_append(buf, buflen, &pos, "%X", usage_id);
}

static int microlink_build_child_path(char *buf, size_t buflen, const char *path,
	unsigned char id, const char *suffix)
{
	size_t pos = 0;

	buf[0] = '\0';

	return microlink_path_append(buf, buflen, &pos, "%s", path)
		&& microlink_path_append(buf, buflen, &pos, "%X%s", id, suffix);
}

static int microlink_build_collection_path(char *buf, size_t buflen, const char *path,
	unsigned char collection_id, unsigned int index)
{
	size_t pos = 0;

	buf[0] = '\0';

	return microlink_path_append(buf, buflen, &pos, "%s", path)
		&& microlink_path_append(buf, buflen, &pos, "%X[%u].", collection_id, index);
}

static void microlink_record_descriptor_usage(const char *path, size_t data_offset,
	size_t size, int skipped)
{
	microlink_descriptor_usage_t *usage;

	if (descriptor_usage_count >= MLINK_DESCRIPTOR_MAX_USAGES) {
		return;
	}

	usage = &descriptor_usages[descriptor_usage_count++];
	memset(usage, 0, sizeof(*usage));
	usage->valid = 1;
	usage->skipped = skipped;
	usage->data_offset = data_offset;
	usage->size = size;
	snprintf(usage->path, sizeof(usage->path), "%s", path);
}

static int microlink_match_path_template(const char *templ, const char *path,
	unsigned int *index)
{
	const char *slot = strstr(templ, "%u");
	const char *suffix;
	char *endptr = NULL;
	unsigned long parsed;
	size_t prefix_len;

	if (index != NULL) {
		*index = 0;
	}

	if (slot == NULL) {
		return !strcmp(templ, path);
	}

	prefix_len = (size_t)(slot - templ);
	suffix = slot + 2;

	if (strncmp(templ, path, prefix_len) != 0) {
		return 0;
	}

	parsed = strtoul(path + prefix_len, &endptr, 10);
	if (endptr == path + prefix_len || strcmp(endptr, suffix) != 0) {
		return 0;
	}

	if (index != NULL) {
		*index = (unsigned int)parsed;
	}

	return 1;
}

static void microlink_format_name_template(const char *templ, unsigned int index,
	microlink_desc_name_index_t name_index, char *out, size_t outlen)
{
	unsigned int rendered_index = index;

	if (name_index == MLINK_NAME_INDEX_ONE_BASED) {
		rendered_index++;
	}

	if (strstr(templ, "%u") != NULL) {
		snprintf_dynamic(out, outlen, templ, "%u", rendered_index);
	} else {
		snprintf(out, outlen, "%s", templ);
	}
}

static const microlink_descriptor_usage_t *microlink_find_descriptor_usage(const char *path)
{
	size_t i;

	for (i = 0; i < descriptor_usage_count; i++) {
		if (descriptor_usages[i].valid && !strcmp(descriptor_usages[i].path, path)) {
			return &descriptor_usages[i];
		}
	}

	return NULL;
}

static const microlink_desc_value_map_t *microlink_find_desc_value_by_path(const char *path,
	unsigned int *index)
{
	size_t i;

	for (i = 0; i < microlink_desc_value_map_count; i++) {
		if (microlink_match_path_template(microlink_desc_value_map[i].path, path, index)) {
			return &microlink_desc_value_map[i];
		}
	}

	return NULL;
}

static const microlink_desc_value_map_t *microlink_find_desc_value_by_var(const char *varname,
	unsigned int *index)
{
	size_t i;

	if (index != NULL) {
		*index = 0;
	}

	for (i = 0; i < descriptor_usage_count; i++) {
		const microlink_descriptor_usage_t *usage = &descriptor_usages[i];
		const microlink_desc_value_map_t *entry;
		unsigned int matched_index;
		char name[96];

		if (!usage->valid || usage->skipped) {
			continue;
		}

		entry = microlink_find_desc_value_by_path(usage->path, &matched_index);
		if (entry == NULL || entry->upsd_name == NULL) {
			continue;
		}

		microlink_format_name_template(entry->upsd_name, matched_index, entry->name_index,
			name, sizeof(name));
		if (!strcmp(name, varname)) {
			if (index != NULL) {
				*index = matched_index;
			}
			return entry;
		}
	}

	return NULL;
}

static const microlink_desc_command_map_t *microlink_find_desc_command_by_name(const char *cmdname)
{
	size_t i;

	for (i = 0; i < microlink_desc_command_map_count; i++) {
		if (microlink_desc_command_map[i].cmd_name != NULL
		 && !strcmp(microlink_desc_command_map[i].cmd_name, cmdname)
		 && microlink_command_available(&microlink_desc_command_map[i])) {
			return &microlink_desc_command_map[i];
		}
	}

	return NULL;
}

static int microlink_command_available(const microlink_desc_command_map_t *entry)
{
	if (entry == NULL || entry->presence_path == NULL) {
		return 1;
	}

	return (microlink_find_descriptor_usage(entry->presence_path) != NULL);
}

static int microlink_set_descriptor_string_info(const char *name, const char *path)
{
	const microlink_descriptor_usage_t *usage;
	char value[MLINK_MAX_PAYLOAD + 1];

	usage = microlink_find_descriptor_usage(path);
	if (usage == NULL || usage->skipped
	 || usage->data_offset + usage->size > descriptor_blob_len) {
		return 0;
	}

	microlink_format_ascii(descriptor_blob + usage->data_offset, usage->size,
		value, sizeof(value));
	if (value[0] == '\0') {
		return 0;
	}

	dstate_setinfo(name, "%s", value);
	return 1;
}

static int microlink_set_descriptor_map_info(const char *name, const char *path,
	const microlink_value_map_t *map, microlink_map_mode_t mode)
{
	const microlink_descriptor_usage_t *usage;
	const unsigned char *data;
	const char *zero_text = NULL;
	uint32_t raw = 0;
	int32_t value = 0;
	char buf[128];
	size_t i, size, used = 0;
	int matched = 0;

	usage = microlink_find_descriptor_usage(path);
	if (usage == NULL || usage->skipped ||
		usage->data_offset + usage->size > descriptor_blob_len) {
		return 0;
	}

	size = usage->size;
	if (size == 0 || size > sizeof(raw)) {
		return 0;
	}

	data = descriptor_blob + usage->data_offset;
	for (i = 0; i < size; i++) {
		raw = (raw << 8) | data[i];
	}

	if (mode == MLINK_MAP_BITFIELD) {
		buf[0] = '\0';

		for (i = 0; map[i].text != NULL; i++) {
			int ret;

			if (map[i].value == 0) {
				zero_text = map[i].text;
				continue;
			}

			if ((raw & (uint32_t)map[i].value) == 0) {
				continue;
			}

			matched = 1;
			ret = snprintf(buf + used, sizeof(buf) - used, "%s%s",
				used ? " " : "", map[i].text);
			if (ret < 0) {
				return 0;
			}

			if ((size_t)ret >= sizeof(buf) - used) {
				used = sizeof(buf) - 1;
				break;
			}

			used += (size_t)ret;
		}

		if (used > 0) {
			dstate_setinfo(name, "%s", buf);
		} else if (!matched && zero_text != NULL) {
			dstate_setinfo(name, "%s", zero_text);
		} else {
			snprintf(buf, sizeof(buf), "0x%0*lX",
				(int)(size * 2), (unsigned long)raw);
			dstate_setinfo(name, "%s", buf);
		}

		return 1;
	}

	{
		uint32_t sign_bit = 1U << ((size * 8U) - 1U);
		if (raw & sign_bit) {
			uint32_t full_scale = (size >= sizeof(uint32_t))
				? 0U : (1U << (size * 8U));
			value = (full_scale != 0U)
				? (int32_t)(raw - full_scale)
				: (int32_t)raw;
		} else {
			value = (int32_t)raw;
		}
	}

	for (i = 0; map[i].text != NULL; i++) {
		if (value == map[i].value) {
			dstate_setinfo(name, "%s", map[i].text);
			return 1;
		}
	}

	dstate_setinfo(name, "%ld", (long)value);
	return 1;
}

static int microlink_set_descriptor_fixed_point_info(const char *name, const char *path,
	microlink_desc_numeric_sign_t sign, unsigned int bin_point)
{
	const microlink_descriptor_usage_t *usage;
	const unsigned char *data;
	uint32_t raw = 0;
	int32_t signed_raw = 0;
	double value;
	char text[32];
	size_t i, size;

	usage = microlink_find_descriptor_usage(path);
	if (usage == NULL || usage->skipped || usage->size == 0
	 || usage->data_offset + usage->size > descriptor_blob_len) {
		return 0;
	}

	size = usage->size;
	data = descriptor_blob + usage->data_offset;
	for (i = 0; i < size; i++) {
		raw = (raw << 8) | data[i];
	}

	if (sign == MLINK_DESC_SIGNED) {
		uint32_t sign_bit = 1U << ((size * 8U) - 1U);
		if (raw & sign_bit) {
			uint32_t full_scale = (size >= sizeof(uint32_t))
				? 0U : (1U << (size * 8U));
			signed_raw = (full_scale != 0U)
				? (int32_t)(raw - full_scale)
				: (int32_t)raw;
		} else {
			signed_raw = (int32_t)raw;
		}
		value = (double)signed_raw;
	} else {
		value = (double)raw;
	}

	if (bin_point > 0U) {
		value /= (double)(1U << bin_point);

		snprintf(text, sizeof(text), "%.6f", value);
		for (i = strlen(text); i > 0 && text[i - 1] == '0'; i--) {
			text[i - 1] = '\0';
		}
		if (i > 0 && text[i - 1] == '.') {
			text[i - 1] = '\0';
		}
	} else {
		if (sign == MLINK_DESC_SIGNED) {
			snprintf(text, sizeof(text), "%ld", (long)signed_raw);
		} else {
			snprintf(text, sizeof(text), "%lu", (unsigned long)raw);
		}
	}

	dstate_setinfo(name, "%s", text);
	return 1;
}

static int microlink_get_descriptor_integer(const char *path, size_t max_size,
	uint64_t *raw_out, size_t *size_out)
{
	const microlink_descriptor_usage_t *usage;
	const unsigned char *data;
	uint64_t raw = 0;
	size_t i;

	if (raw_out == NULL) {
		return 0;
	}

	usage = microlink_find_descriptor_usage(path);
	if (usage == NULL || usage->skipped || usage->size == 0
	 || usage->size > max_size
	 || usage->data_offset + usage->size > descriptor_blob_len) {
		return 0;
	}

	data = descriptor_blob + usage->data_offset;
	for (i = 0; i < usage->size; i++) {
		raw = (raw << 8) | data[i];
	}

	*raw_out = raw;
	if (size_out != NULL) {
		*size_out = usage->size;
	}

	return 1;
}

static void microlink_civil_from_days(int64_t days, int *year, unsigned int *month,
	unsigned int *day)
{
	int64_t z = days + 730425;
	int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	unsigned int doe = (unsigned int)(z - era * 146097);
	unsigned int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	int y = (int)yoe + (int)(era * 400);
	unsigned int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	unsigned int mp = (5 * doy + 2) / 153;

	*day = doy - (153 * mp + 2) / 5 + 1;
	*month = mp + (mp < 10 ? 3U : (unsigned int)-9);
	*year = y + (*month <= 2U);
}

static int64_t microlink_days_from_civil(int year, unsigned int month, unsigned int day)
{
	int y = year - (month <= 2U);
	int era = (y >= 0 ? y : y - 399) / 400;
	unsigned int yoe = (unsigned int)(y - era * 400);
	unsigned int doy = (153 * (month + (month > 2U ? (unsigned int)-3 : 9U)) + 2) / 5 + day - 1;
	unsigned int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

	return (int64_t)(era * 146097 + (int)doe) - 730425;
}

static uint64_t microlink_max_unsigned_for_size(size_t size)
{
	if (size >= sizeof(uint64_t)) {
		return UINT64_MAX;
	}

	return ((uint64_t)1 << (size * 8U)) - 1U;
}

static int microlink_set_descriptor_date_info(const char *name, const char *path)
{
	uint64_t raw;
	int year;
	unsigned int month, day;
	char text[16];

	if (!microlink_get_descriptor_integer(path, sizeof(uint32_t), &raw, NULL)) {
		return 0;
	}

	microlink_civil_from_days((int64_t)raw, &year, &month, &day);
	snprintf(text, sizeof(text), "%04d-%02u-%02u", year, month, day);
	dstate_setinfo(name, "%s", text);
	return 1;
}

static int microlink_set_descriptor_time_info(const char *name, const char *path)
{
	uint64_t raw;
	unsigned int hours, minutes, seconds;
	char text[16];

	if (!microlink_get_descriptor_integer(path, sizeof(uint32_t), &raw, NULL)) {
		return 0;
	}

	hours = (unsigned int)(raw / 3600U);
	minutes = (unsigned int)((raw % 3600U) / 60U);
	seconds = (unsigned int)(raw % 60U);
	snprintf(text, sizeof(text), "%02u:%02u:%02u", hours, minutes, seconds);
	dstate_setinfo(name, "%s", text);
	return 1;
}

static int microlink_get_descriptor_map_bits(const char *path, uint32_t *bits)
{
	const microlink_descriptor_usage_t *usage;
	const unsigned char *data;
	uint32_t raw = 0;
	size_t i;

	if (bits == NULL) {
		return 0;
	}

	usage = microlink_find_descriptor_usage(path);
	if (usage == NULL || usage->skipped || usage->size == 0
	 || usage->size > sizeof(raw)
	 || usage->data_offset + usage->size > descriptor_blob_len) {
		return 0;
	}

	data = descriptor_blob + usage->data_offset;
	for (i = 0; i < usage->size; i++) {
		raw = (raw << 8) | data[i];
	}

	*bits = raw;
	return 1;
}

static int microlink_auth_data_valid(void)
{
	uint32_t bits = 0;

	if (!microlink_get_descriptor_map_bits(MLINK_DESC_AUTH_STATUS, &bits)) {
		return 0;
	}

	return ((bits & (1U << 0)) != 0);
}

static int microlink_startup_ready(void)
{
	if (!session_ready || !microlink_get_object(MLINK_OBJ_PROTOCOL)->seen) {
		return 0;
	}

	if ((page0.flags.bits.descriptor_present || page0.flags.bits.auth_required)
	 && !descriptor_ready) {
		return 0;
	}

	if (!page0.flags.bits.auth_required) {
		return 1;
	}

	if (microlink_auth_data_valid()) {
		return 1;
	}

	return authentication_sent;
}

static void microlink_set_alarms_from_descriptor_map(const char *path,
	const microlink_value_map_t *map)
{
	uint32_t raw = 0;
	size_t i;
	int matched = 0;

	if (!microlink_get_descriptor_map_bits(path, &raw)) {
		return;
	}

	for (i = 0; map[i].text != NULL; i++) {
		if (map[i].value == 0) {
			continue;
		}

		if ((raw & (uint32_t)map[i].value) != 0) {
			matched = 1;
			alarm_set(map[i].text);
		}
	}

	if (!matched) {
		for (i = 0; map[i].text != NULL; i++) {
			if (map[i].value == 0) {
				alarm_set(map[i].text);
				break;
			}
		}
	}
}

static void microlink_set_status_from_descriptor_map(const char *path,
	const microlink_value_map_t *map)
{
	uint32_t raw = 0;
	size_t i;
	int matched = 0;

	if (!microlink_get_descriptor_map_bits(path, &raw)) {
		return;
	}

	for (i = 0; map[i].text != NULL; i++) {
		if (map[i].value == 0) {
			continue;
		}

		if ((raw & (uint32_t)map[i].value) != 0) {
			matched = 1;
			status_set(map[i].text);
		}
	}

	if (!matched) {
		for (i = 0; map[i].text != NULL; i++) {
			if (map[i].value == 0) {
				status_set(map[i].text);
				break;
			}
		}
	}
}

static int microlink_send_descriptor_write(const char *path, const unsigned char *payload,
	size_t payload_len)
{
	const microlink_descriptor_usage_t *usage;
	size_t page;
	size_t offset;

	usage = microlink_find_descriptor_usage(path);
	if (usage == NULL || usage->skipped || !usage->valid || usage->size == 0
	 || payload_len == 0 || payload_len != usage->size || payload_len > MLINK_MAX_PAYLOAD) {
		return 0;
	}

	if (usage->data_offset + usage->size > descriptor_blob_len || page0.width == 0) {
		return 0;
	}

	page = usage->data_offset / page0.width;
	offset = usage->data_offset % page0.width;
	if (page > 0xFFU || offset > 0xFFU) {
		return 0;
	}

	if (!microlink_send_write((unsigned char)page, (unsigned char)offset,
		(unsigned char)usage->size, payload)) {
		return 0;
	}

	memcpy(descriptor_blob + usage->data_offset, payload, usage->size);
	if (page < 256U) {
		microlink_object_t *obj = microlink_get_object_mut((unsigned int)page);
		if (obj->seen && obj->len >= offset + usage->size) {
			memcpy(obj->data + offset, payload, usage->size);
		}
	}

	return 1;
}

static int microlink_send_descriptor_mask_value(const char *path, uint64_t mask)
{
	const microlink_descriptor_usage_t *usage;
	unsigned char payload[MLINK_MAX_PAYLOAD];
	size_t i;

	usage = microlink_find_descriptor_usage(path);
	if (usage == NULL || usage->skipped || !usage->valid || usage->size == 0
	 || usage->size > sizeof(payload) || usage->size > sizeof(mask)) {
		return 0;
	}

	memset(payload, 0, usage->size);
	for (i = 0; i < usage->size; i++) {
		size_t shift = (usage->size - 1U - i) * 8U;
		payload[i] = (unsigned char)((mask >> shift) & 0xFFU);
	}

	return microlink_send_descriptor_write(path, payload, usage->size);
}

static int microlink_send_descriptor_typed_value(const microlink_desc_value_map_t *entry,
	const char *path, const char *val)
{
	const microlink_descriptor_usage_t *usage;
	unsigned char payload[MLINK_MAX_PAYLOAD];
	size_t i;

	if (entry == NULL || path == NULL || val == NULL) {
		return 0;
	}

	usage = microlink_find_descriptor_usage(path);
	if (usage == NULL || !usage->valid || usage->skipped || usage->size == 0
	 || usage->size > sizeof(payload)) {
		return 0;
	}

	switch (entry->type) {
	case MLINK_DESC_STRING:
		memset(payload, 0, usage->size);
		for (i = 0; i < usage->size && val[i] != '\0'; i++) {
			payload[i] = (unsigned char)val[i];
		}
		return microlink_send_descriptor_write(path, payload, usage->size);

	case MLINK_DESC_FIXED_POINT:
	{
		char *endptr = NULL;
		int64_t raw;

		if (usage->size == 0 || usage->size > 8) {
			return 0;
		}

		if (entry->bin_point == 0U) {
			/* strict integer parsing */
			long long parsed = strtoll(val, &endptr, 10);

			if (endptr == val || *endptr != '\0') {
				return 0;
			}

			raw = (int64_t)parsed;
		} else {
			/* fixed-point parsing */
			double numeric = strtod(val, &endptr);
			double scaled;

			if (endptr == val || *endptr != '\0') {
				return 0;
			}

			scaled = numeric * (double)(1U << entry->bin_point);
			raw = (int64_t)((scaled >= 0.0) ? (scaled + 0.5) : (scaled - 0.5));
		}

		/* shared range + packing logic */

		{
			int64_t min_raw, max_raw;

			if (entry->sign == MLINK_DESC_SIGNED) {
				max_raw = ((int64_t)1 << ((usage->size * 8U) - 1U)) - 1;
				min_raw = -((int64_t)1 << ((usage->size * 8U) - 1U));
			} else {
				min_raw = 0;
				max_raw = ((int64_t)1 << (usage->size * 8U)) - 1;
			}

			if (raw < min_raw || raw > max_raw) {
				return 0;
			}
		}

		memset(payload, 0, usage->size);
		for (i = 0; i < usage->size; i++) {
			size_t shift = (usage->size - 1U - i) * 8U;
			payload[i] = (unsigned char)(((uint64_t)raw >> shift) & 0xFFU);
		}

		return microlink_send_descriptor_write(path, payload, usage->size);
	}

	case MLINK_DESC_DATE:
	{
		int year;
		int check_year;
		unsigned int month, day;
		unsigned int check_month, check_day;
		int64_t raw;

		if (usage->size == 0 || usage->size > 8) {
			return 0;
		}

		if (sscanf(val, "%d-%u-%u", &year, &month, &day) != 3) {
			return 0;
		}

		if (month < 1U || month > 12U || day < 1U || day > 31U) {
			return 0;
		}

		raw = microlink_days_from_civil(year, month, day);
		if (raw < 0 || (uint64_t)raw > microlink_max_unsigned_for_size(usage->size)) {
			return 0;
		}

		microlink_civil_from_days(raw, &check_year, &check_month, &check_day);
		if (check_year != year || check_month != month || check_day != day) {
			return 0;
		}

		memset(payload, 0, usage->size);
		for (i = 0; i < usage->size; i++) {
			size_t shift = (usage->size - 1U - i) * 8U;
			payload[i] = (unsigned char)(((uint64_t)raw >> shift) & 0xFFU);
		}

		return microlink_send_descriptor_write(path, payload, usage->size);
	}

	case MLINK_DESC_TIME:
	{
		unsigned int hours, minutes, seconds;
		uint64_t raw;

		if (usage->size == 0 || usage->size > 8) {
			return 0;
		}

		if (sscanf(val, "%u:%u:%u", &hours, &minutes, &seconds) != 3) {
			return 0;
		}

		if (minutes > 59U || seconds > 59U) {
			return 0;
		}

		raw = ((uint64_t)hours * 3600U) + ((uint64_t)minutes * 60U) + (uint64_t)seconds;
		if (raw > microlink_max_unsigned_for_size(usage->size)) {
			return 0;
		}

		memset(payload, 0, usage->size);
		for (i = 0; i < usage->size; i++) {
			size_t shift = (usage->size - 1U - i) * 8U;
			payload[i] = (unsigned char)((raw >> shift) & 0xFFU);
		}

		return microlink_send_descriptor_write(path, payload, usage->size);
	}

	case MLINK_DESC_ENUM_MAP:
	case MLINK_DESC_BITFIELD_MAP:
	{
		char *endptr = NULL;
		int64_t raw = 0;
		size_t j;

		if (usage->size == 0 || usage->size > 8) {
			return 0;
		}

		if (entry->map != NULL) {
			for (j = 0; entry->map[j].text != NULL; j++) {
				if (!strcasecmp(entry->map[j].text, val)) {
					raw = entry->map[j].value;
					break;
				}
			}

			if (entry->map[j].text == NULL) {
				if (entry->type == MLINK_DESC_ENUM_MAP) {
					raw = (int64_t)strtoll(val, &endptr, 0);
				} else {
					raw = (int64_t)strtoull(val, &endptr, 0);
				}

				if (endptr == val || *endptr != '\0') {
					return 0;
				}
			}
		}

		{
			int64_t min_raw, max_raw;

			if (entry->type == MLINK_DESC_ENUM_MAP && entry->sign == MLINK_DESC_SIGNED) {
				max_raw = ((int64_t)1 << ((usage->size * 8U) - 1U)) - 1;
				min_raw = -((int64_t)1 << ((usage->size * 8U) - 1U));
			} else {
				min_raw = 0;
				max_raw = ((int64_t)1 << (usage->size * 8U)) - 1;
			}

			if (raw < min_raw || raw > max_raw) {
				return 0;
			}
		}

		memset(payload, 0, usage->size);
		for (i = 0; i < usage->size; i++) {
			size_t shift = (usage->size - 1U - i) * 8U;
			payload[i] = (unsigned char)(((uint64_t)raw >> shift) & 0xFFU);
		}

		return microlink_send_descriptor_write(path, payload, usage->size);
	}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
	case MLINK_DESC_NONE:
	default:
		return 0;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}
}

static int microlink_descriptor_value_is_printable(const unsigned char *buf, size_t len)
{
	size_t i;

	if (len == 0) {
		return 0;
	}

	for (i = 0; i < len; i++) {
		if (!isprint((int)buf[i])) {
			return 0;
		}
	}

	return 1;
}

static void microlink_publish_descriptor_exports(void)
{
	size_t i;

	if (!descriptor_ready) {
		return;
	}

	for (i = 0; i < descriptor_usage_count; i++) {
		char name[96];
		char value[(MLINK_MAX_PAYLOAD * 3) + 1];
		const unsigned char *data;
		const microlink_descriptor_usage_t *usage = &descriptor_usages[i];
		int mapped = 0;
		size_t j;

		if (!usage->valid || usage->skipped) {
			continue;
		}

		if (usage->data_offset + usage->size > descriptor_blob_len) {
			continue;
		}

		for (j = 0; j < microlink_desc_value_map_count; j++) {
			const microlink_desc_value_map_t *entry = &microlink_desc_value_map[j];
			unsigned int index = 0;

			if (entry->upsd_name == NULL
			 || !microlink_match_path_template(entry->path, usage->path, &index)) {
				continue;
			}

			mapped = 1;
			microlink_format_name_template(entry->upsd_name, index, entry->name_index,
				name, sizeof(name));

			switch (entry->type) {
			case MLINK_DESC_STRING:
				microlink_set_descriptor_string_info(name, usage->path);
				break;
			case MLINK_DESC_FIXED_POINT:
				microlink_set_descriptor_fixed_point_info(name, usage->path,
					entry->sign, entry->bin_point);
				break;
			case MLINK_DESC_DATE:
				microlink_set_descriptor_date_info(name, usage->path);
				break;
			case MLINK_DESC_TIME:
				microlink_set_descriptor_time_info(name, usage->path);
				break;
			case MLINK_DESC_BITFIELD_MAP:
				microlink_set_descriptor_map_info(name, usage->path, entry->map, MLINK_MAP_BITFIELD);
				break;
			case MLINK_DESC_ENUM_MAP:
				microlink_set_descriptor_map_info(name, usage->path, entry->map, MLINK_MAP_VALUE);
				break;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
			case MLINK_DESC_NONE:
			default:
				break;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
			}

			if (entry->access & MLINK_DESC_RW) {
				int flags = ST_FLAG_RW;

				if (entry->type == MLINK_DESC_STRING) {
					flags |= ST_FLAG_STRING;
				}

				dstate_setflags(name, flags);
				if (entry->type == MLINK_DESC_STRING && usage->size > 0 && usage->size < INT_MAX) {
					dstate_setaux(name, (int)usage->size);
				}
			}
		}

		if (mapped) {
			continue;
		}

		if (!microlink_show_unmapped()) {
			continue;
		}

		data = descriptor_blob + usage->data_offset;
		snprintf(name, sizeof(name), "microlink.unmapped.%s", usage->path);
		if (microlink_descriptor_value_is_printable(data, usage->size)) {
			microlink_format_ascii(data, usage->size, value, sizeof(value));
		} else if (usage->size == 2) {
			snprintf(value, sizeof(value), "0x%02X%02X @ %04" PRIxSIZE ":%" PRIuSIZE,
				data[0], data[1], usage->data_offset, usage->size);
		} else if (usage->size == 4) {
			snprintf(value, sizeof(value), "0x%02X%02X%02X%02X @ %04" PRIxSIZE ":%" PRIuSIZE,
				data[0], data[1], data[2], data[3], usage->data_offset, usage->size);
		} else {
			microlink_format_hex(data, usage->size, value, sizeof(value));
		}
		dstate_setinfo(name, "%s", value);
	}
}

static size_t microlink_parse_descriptor_usage(const unsigned char *blob, size_t blob_len,
	size_t pos, size_t *data_offset, const char *path, unsigned char usage_id, int skipped)
{
	char usage_path[64];
	size_t usage_size = 2;

	if (!microlink_build_usage_path(usage_path, sizeof(usage_path), path, usage_id)) {
		return 0;
	}

	while (pos < blob_len) {
		unsigned char token = blob[pos];

		if (token == 0xFC) {
			if (pos + 1 >= blob_len) {
				return 0;
			}
			usage_size = blob[pos + 1];
			pos += 2;
			continue;
		}

		if (token == 0xFB || token == 0xF9) {
			pos++;
			if (pos + usage_size > blob_len) {
				return 0;
			}
			pos += usage_size;
			continue;
		}

		if (token == 0xFA) {
			pos++;
			if (pos + (usage_size * 2U) > blob_len) {
				return 0;
			}
			pos += usage_size * 2U;
			continue;
		}

		break;
	}

	microlink_record_descriptor_usage(usage_path, *data_offset, usage_size, skipped);
	*data_offset += usage_size;
	return pos;
}

static size_t microlink_parse_descriptor_block(const unsigned char *blob, size_t blob_len,
	size_t pos, size_t *data_offset, const char *path)
{
	int skip_next = 0;

	while (pos < blob_len) {
		unsigned char token = blob[pos++];

		switch (token) {
		case 0xF4:
			pos = microlink_parse_descriptor_block(blob, blob_len, pos, data_offset, path);
			if (pos == 0) {
				return 0;
			}
			break;
		case 0xF5:
			skip_next = 1;
			break;
		case 0xF6:
		case 0xFF:
			return pos;
		case 0xF7:
			break;
		case 0xF8:
		{
			char child[64];
			if (pos >= blob_len) {
				return 0;
			}
			if (!microlink_build_child_path(child, sizeof(child), "", blob[pos++], ":")) {
				return 0;
			}
			pos = microlink_parse_descriptor_block(blob, blob_len, pos, data_offset, child);
			if (pos == 0) {
				return 0;
			}
			break;
		}
		case 0xFE:
		{
			char child[64];
			if (pos >= blob_len) {
				return 0;
			}
			if (!microlink_build_child_path(child, sizeof(child), path, blob[pos++], ".")) {
				return 0;
			}
			pos = microlink_parse_descriptor_block(blob, blob_len, pos, data_offset, child);
			if (pos == 0) {
				return 0;
			}
			break;
		}
		case 0xFD:
		{
			unsigned char collection_id;
			unsigned char count;
			size_t block_start;
			size_t block_end = 0;
			unsigned int idx;

			if (pos + 1 >= blob_len) {
				return 0;
			}

			collection_id = blob[pos++];
			count = blob[pos++];
			block_start = pos;

			for (idx = 0; idx < count; idx++) {
				char child[64];
				size_t sub_pos;

				if (!microlink_build_collection_path(child, sizeof(child), path,
					collection_id, idx)) {
					return 0;
				}
				sub_pos = microlink_parse_descriptor_block(blob, blob_len, block_start, data_offset, child);
				if (sub_pos == 0) {
					return 0;
				}
				block_end = sub_pos;
			}

			pos = block_end;
			break;
		}
		default:
			if (token == 0x00 || microlink_is_descriptor_operator(token) || token > 0xDF) {
				return 0;
			}

			pos = microlink_parse_descriptor_usage(blob, blob_len, pos, data_offset, path,
				token, skip_next);
			if (pos == 0) {
				return 0;
			}
			skip_next = 0;
			break;
		}
	}

	return pos;
}

static int microlink_update_blob(void)
{
	unsigned int row;

	if (!page0.flags.bits.descriptor_present || page0.descriptor_version != 0x01U) {
		return 0;
	}

	if (page0.width == 0 || page0.count == 0) {
		return 0;
	}

	descriptor_blob_len = page0.width * page0.count;
	if (descriptor_blob_len > sizeof(descriptor_blob)) {
		descriptor_blob_len = sizeof(descriptor_blob);
	}
	memset(descriptor_blob, 0, descriptor_blob_len);

	for (row = 0; row < page0.count; row++) {
		const microlink_object_t *obj = microlink_get_object(row);
		size_t copylen;
		size_t dst;

		dst = ((size_t)row) * page0.width;
		if (dst >= descriptor_blob_len) {
			break;
		}

		if (!obj->seen || obj->len == 0) {
			continue;
		}

		copylen = obj->len;
		if (copylen > page0.width) {
			copylen = page0.width;
		}
		if (dst + copylen > descriptor_blob_len) {
			copylen = descriptor_blob_len - dst;
		}

		memcpy(descriptor_blob + dst, obj->data, copylen);
	}

	return 1;
}

static int microlink_parse_descriptor(void)
{
	const microlink_object_t *protocol = microlink_get_object(MLINK_OBJ_PROTOCOL);
	uint16_t data_ptr;
	size_t data_ptr_offset;
	size_t data_offset;

	descriptor_ready = 0;
	descriptor_usage_count = 0;
	descriptor_blob_len = 0;

	if (!protocol->seen || protocol->len < 12) {
		return 0;
	}

	if (!microlink_update_blob()) {
		return 0;
	}

	data_ptr = page0.descriptor_ptr;
	data_ptr_offset = ((((size_t)data_ptr) >> 8) * page0.width) + (((size_t)data_ptr) & 0xFFU);
	if (data_ptr_offset >= descriptor_blob_len || 12 >= descriptor_blob_len) {
		return 0;
	}

	data_offset = data_ptr_offset;
	if (microlink_parse_descriptor_block(descriptor_blob, descriptor_blob_len, 12, &data_offset, "") == 0) {
		descriptor_usage_count = 0;
		return 0;
	}

	descriptor_ready = 1;
	return 1;
}

static void microlink_cache_object(const unsigned char *frame, size_t len)
{
	unsigned int id;
	microlink_object_t *obj;

	if (len < 3) {
		return;
	}

	id = frame[0];
	obj = microlink_get_object_mut(id);
	obj->seen = 1;
	obj->len = len - 3;
	memcpy(obj->data, frame + 1, obj->len);

	if (id == MLINK_OBJ_PROTOCOL && obj->len >= 3) {
		page0.version = obj->data[0];
		page0.width = obj->data[1];
		page0.count = obj->data[2];
		page0.series_id = (obj->len >= 5)
			? (uint16_t)(((uint16_t)obj->data[3] << 8) | (uint16_t)obj->data[4])
			: 0;
		page0.series_data_version = (obj->len >= 6) ? obj->data[5] : 0;
		page0.flags.raw = (obj->len >= 7) ? obj->data[6] : 0;
		page0.descriptor_version = (obj->len >= 9) ? obj->data[8] : 0;
		page0.descriptor_ptr = (obj->len >= 12)
			? (uint16_t)(((uint16_t)obj->data[10] << 8) | (uint16_t)obj->data[11])
			: 0;
		upsdebugx(2, "microlink: page0 version=%u width=%u pages=%u flags=0x%02X",
			(unsigned int)page0.version,
			(unsigned int)page0.width,
			page0.count,
			(unsigned int)page0.flags.raw);
	}
}

static int microlink_send_write(unsigned char id, unsigned char offset,
	unsigned char len, const unsigned char *data)
{
	unsigned char frame[MLINK_MAX_FRAME];
	unsigned char cb0, cb1;
	size_t framelen = 0;

	frame[framelen++] = id;
	frame[framelen++] = offset;
	frame[framelen++] = len;
	memcpy(frame + framelen, data, len);
	framelen += len;
	microlink_checksum(frame, framelen, &cb0, &cb1);
	frame[framelen++] = cb0;
	frame[framelen++] = cb1;
	microlink_trace_frame(2, "TX write", frame, framelen);

	if (ser_send_buf(upsfd, frame, framelen) != (ssize_t)framelen) {
		return 0;
	}

	return 1;
}

static int microlink_send_simple(unsigned char byte)
{
	microlink_trace_frame(2, "TX ctrl", &byte, 1);
	return ser_send_buf(upsfd, &byte, 1) == 1;
}

static int microlink_try_extract_frame(unsigned char *frame, size_t *framelen)
{
	size_t start;

	*framelen = 0;

	while (rxbuf_len > 0 && rxbuf[0] == MLINK_NEXT_BYTE) {
		memmove(rxbuf, rxbuf + 1, --rxbuf_len);
	}

	for (start = 0; start < rxbuf_len; start++) {
		if (rxbuf[start] == MLINK_NEXT_BYTE) {
			continue;
		}

		if (rxbuf_len - start >= MLINK_RECORD_LEN
		 && microlink_checksum_valid(rxbuf + start, MLINK_RECORD_LEN)) {
			if (start > 0) {
				upsdebugx(2, "microlink: skipped %u stray byte(s) before record 0x%02X",
					(unsigned int)start, rxbuf[start]);
				memmove(rxbuf, rxbuf + start, rxbuf_len - start);
				rxbuf_len -= start;
			}

			memcpy(frame, rxbuf, MLINK_RECORD_LEN);
			*framelen = MLINK_RECORD_LEN;
			memmove(rxbuf, rxbuf + MLINK_RECORD_LEN, rxbuf_len - MLINK_RECORD_LEN);
			rxbuf_len -= MLINK_RECORD_LEN;
			microlink_trace_frame(2, "RX record", frame, MLINK_RECORD_LEN);
			return 1;
		}
	}

	if (rxbuf_len >= sizeof(rxbuf)) {
		upsdebugx(1, "microlink: dropping %u bytes while resynchronizing",
			(unsigned int)(rxbuf_len - (MLINK_RECORD_LEN - 1)));
		memmove(rxbuf, rxbuf + (rxbuf_len - (MLINK_RECORD_LEN - 1)), MLINK_RECORD_LEN - 1);
		rxbuf_len = MLINK_RECORD_LEN - 1;
	}

	return 0;
}

static const unsigned char *microlink_get_descriptor_data(const char *path, size_t size)
{
	const microlink_descriptor_usage_t *usage;

	if (!descriptor_ready) {
		upsdebugx(1, "descriptor not ready!");
		return NULL;
	}

	usage = microlink_find_descriptor_usage(path);
	if (usage == NULL || usage->skipped || usage->size != size ||
		usage->data_offset + usage->size > descriptor_blob_len) {
		return NULL;
	}

	return descriptor_blob + usage->data_offset;
}

static void microlink_auth_update(unsigned char *s0, unsigned char *s1,
	const unsigned char *data, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		*s0 = (unsigned char)((*s0 + data[i]) % 255U);
		*s1 = (unsigned char)((*s1 + *s0) % 255U);
	}
}

static int microlink_authenticate(void)
{
	const microlink_object_t *protocol = microlink_get_object(MLINK_OBJ_PROTOCOL);
	const unsigned char *serial;
	const unsigned char *master_password;
	unsigned char s0, s1;
	unsigned char payload[4];

	if (!protocol->seen || protocol->len < 8) {
		upsdebugx(1, "microlink: authentication requested before protocol header was cached");
		return 0;
	}

	serial = microlink_get_descriptor_data(MLINK_DESC_SERIALNUMBER, 16);
	master_password = microlink_get_descriptor_data(MLINK_DESC_MASTER_PASSWORD, 4);

	if (serial == NULL || master_password == NULL) {
		upsdebugx(1, "microlink: authentication requested before required descriptors were cached");
		return 0;
	}

	s0 = protocol->data[4];
	s1 = protocol->data[3];

	microlink_auth_update(&s0, &s1, protocol->data, 8);
	microlink_auth_update(&s0, &s1, serial, 16);
	microlink_auth_update(&s0, &s1, master_password, 2);

	payload[0] = 0x00;
	payload[1] = 0x00;
	payload[2] = s0;
	payload[3] = s1;

	upsdebugx(2, "microlink: sending slave password %02X %02X",
		payload[2], payload[3]);

	return microlink_send_descriptor_write(
		MLINK_DESC_SLAVE_PASSWORD,
		payload,
		sizeof(payload)
	);
}

static int microlink_process_frame(const unsigned char *frame, size_t framelen)
{
	if (!microlink_checksum_valid(frame, framelen)) {
		ser_comm_fail("microlink: checksum failure on object 0x%02X", frame[0]);
		return 0;
	}

	parsed_frames++;
	microlink_cache_object(frame, framelen);

	if (page0.count > 0 && frame[0] == (unsigned char)(page0.count - 1U)) {
		if (page0.flags.bits.descriptor_present) {
			if (descriptor_ready) {
				microlink_update_blob();
			} else {
				microlink_parse_descriptor();
			}
		}

		if (page0.flags.bits.auth_required && descriptor_ready && !authentication_sent) {
			if (!microlink_authenticate()) {
				ser_comm_fail("microlink: failed to authenticate");
				return 0;
			}
			authentication_sent = 1;
		}
	}

	return 1;
}

static int microlink_receive_once(void)
{
	unsigned char frame[MLINK_MAX_FRAME];
	size_t framelen = 0;
	st_tree_timespec_t start;

	state_get_timestamp(&start);

	for (;;) {
		unsigned char ch;
		ssize_t ret;

		if (microlink_try_extract_frame(frame, &framelen)) {
			return microlink_process_frame(frame, framelen);
		}

		ret = ser_get_char(upsfd, &ch, 0, MLINK_READ_TIMEOUT_USEC);
		if (ret < 0) {
			return 0;
		}

		if (ret == 0) {
			if (microlink_timeout_expired(&start, 0, MLINK_READ_TIMEOUT_USEC)) {
				return 0;
			}
			continue;
		}

		if (rxbuf_len < sizeof(rxbuf)) {
			rxbuf[rxbuf_len++] = ch;
			upsdebug_hex(5, "microlink RX byte", &ch, 1);
		} else {
			upsdebugx(1, "microlink: receive buffer overflow, resetting parser");
			rxbuf_len = 0;
		}
	}
}

static int microlink_poll_once(void)
{
	if (!poll_primed) {
		if (!microlink_prime_poll()) {
			return 0;
		}
	}

	if (microlink_receive_once()) {
		consecutive_timeouts = 0;
		return microlink_prime_poll();
	}

	poll_primed = 0;
	consecutive_timeouts++;
	return 0;
}

static int microlink_start_session(void)
{
	unsigned int attempt;

	rxbuf_len = 0;
	poll_primed = 0;
	authentication_sent = 0;
	memset(&page0, 0, sizeof(page0));
	descriptor_ready = 0;
	descriptor_usage_count = 0;
	descriptor_blob_len = 0;
	ser_flush_io(upsfd);

	for (attempt = 0; attempt < MLINK_HANDSHAKE_RETRIES; attempt++) {
		if (!microlink_send_simple(MLINK_INIT_BYTE)) {
			return 0;
		}

		if (microlink_receive_once()) {
			consecutive_timeouts = 0;
			session_ready = 1;
			return microlink_prime_poll();
		}
	}

	return 0;
}

static int microlink_reconnect_session(void)
{
	upsdebugx(1, "microlink: reconnecting session after %u consecutive timeouts",
		consecutive_timeouts);
	session_ready = 0;
	return microlink_start_session();
}

static void microlink_publish_identity(void)
{
	dstate_setinfo("ups.mfr", "APC");
	dstate_setinfo("device.mfr", "APC");
	microlink_publish_descriptor_exports();
}

static void microlink_publish_status(void)
{
	size_t i;

	status_init();
	alarm_init();

	for (i = 0; microlink_desc_publish_map[i].path != NULL; i++) {
		if (microlink_desc_publish_map[i].status_map != NULL) {
			microlink_set_status_from_descriptor_map(
				microlink_desc_publish_map[i].path,
				microlink_desc_publish_map[i].status_map);
		}
		if (microlink_desc_publish_map[i].alarm_map != NULL) {
			microlink_set_alarms_from_descriptor_map(
				microlink_desc_publish_map[i].path,
				microlink_desc_publish_map[i].alarm_map);
		}
	}

	status_commit();
	alarm_commit();
}

static void microlink_publish_runtime(void)
{
	uint16_t descriptor_ptr = 0;
	size_t descriptor_data_offset = 0;
	char hex[16];
	char flags[16];
	const microlink_object_t *protocol = microlink_get_object(MLINK_OBJ_PROTOCOL);

	if (!microlink_show_internals()) {
		return;
	}

	if (protocol->seen && protocol->len >= 7) {
		dstate_setinfo("microlink.version", "%u", (unsigned int)page0.version);
		dstate_setinfo("microlink.series.id", "%u", (unsigned int)page0.series_id);
		dstate_setinfo("microlink.series.data.version", "%u",
			(unsigned int)page0.series_data_version);
		snprintf(flags, sizeof(flags), "0x%02X", page0.flags.raw);
		dstate_setinfo("microlink.flags", "%s", flags);
		dstate_setinfo("microlink.flag.auth_required", "%u",
			(unsigned int)page0.flags.bits.auth_required);
		dstate_setinfo("microlink.flag.implicit_stuffing", "%u",
			(unsigned int)page0.flags.bits.implicit_stuffing);
		dstate_setinfo("microlink.flag.descriptor_present", "%u",
			(unsigned int)page0.flags.bits.descriptor_present);
		dstate_setinfo("microlink.flag.firmware_update_needed", "%u",
			(unsigned int)page0.flags.bits.firmware_update_needed);
	}

	if (protocol->seen && protocol->len >= 12 && page0.flags.bits.descriptor_present) {
		dstate_setinfo("microlink.descriptor.version", "%u",
			(unsigned int)page0.descriptor_version);
		descriptor_ptr = page0.descriptor_ptr;
		descriptor_data_offset = ((((size_t)descriptor_ptr) >> 8) * page0.width)
			+ (((size_t)descriptor_ptr) & 0xFFU);
		dstate_setinfo("microlink.descriptor.table_offset", "%u", 12U);
		snprintf(hex, sizeof(hex), "0x%04X", descriptor_ptr);
		dstate_setinfo("microlink.descriptor.pointer", "%s", hex);
		dstate_setinfo("microlink.descriptor.data_offset", "%u",
			(unsigned int)descriptor_data_offset);
	}

	dstate_setinfo("microlink.session", "%s", session_ready ? "ready" : "syncing");
	dstate_setinfo("microlink.timeouts", "%u", consecutive_timeouts);
	dstate_setinfo("microlink.rxbuf", "%u", (unsigned int)rxbuf_len);
	dstate_setinfo("microlink.page.width", "%u", (unsigned int)page0.width);
	dstate_setinfo("microlink.page.count", "%u", page0.count);
	dstate_setinfo("microlink.descriptor.ready", "%u", (unsigned int)descriptor_ready);
	dstate_setinfo("microlink.descriptor.usages", "%u", (unsigned int)descriptor_usage_count);
}

static int setvar(const char *varname, const char *val)
{
	const microlink_desc_value_map_t *entry;
	unsigned int index = 0;
	char path[64];

	upsdebug_SET_STARTING(varname, val);

	entry = microlink_find_desc_value_by_var(varname, &index);
	if (entry != NULL && (entry->access & MLINK_DESC_RW)) {
		microlink_format_name_template(entry->path, index,
			MLINK_NAME_INDEX_ZERO_BASED, path, sizeof(path));
		if (microlink_send_descriptor_typed_value(entry, path, val)) {
			microlink_publish_identity();
			microlink_publish_runtime();
			return STAT_SET_HANDLED;
		}
		return STAT_SET_FAILED;
	}

	upslog_SET_UNKNOWN(varname, val);
	return STAT_SET_UNKNOWN;
}

static int instcmd(const char *cmdname, const char *extra)
{
	const microlink_desc_command_map_t *entry;
	const microlink_desc_value_map_t *value_entry;
	int ret = STAT_INSTCMD_INVALID;

	NUT_UNUSED_VARIABLE(extra);
	upsdebug_INSTCMD_STARTING(cmdname, extra);

	entry = microlink_find_desc_command_by_name(cmdname);
	if (entry != NULL) {
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		switch (entry->write_type) {
		case MLINK_DESC_WRITE_BITMASK:
			ret = microlink_send_descriptor_mask_value(entry->path,	entry->bit_mask)
				? STAT_INSTCMD_HANDLED : STAT_INSTCMD_FAILED;
			break;
		case MLINK_DESC_WRITE_TYPED:
			value_entry = microlink_find_desc_value_by_path(entry->path, NULL);
			ret = (value_entry != NULL && entry->value != NULL
				&& microlink_send_descriptor_typed_value(value_entry, entry->path, entry->value))
				? STAT_INSTCMD_HANDLED : STAT_INSTCMD_FAILED;
			break;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
		case MLINK_DESC_WRITE_NONE:
		default:
			ret = STAT_INSTCMD_FAILED;
			break;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
		}
		upslog_INSTCMD_RESULT(ret, cmdname, extra);
		return ret;
	}

	upslog_INSTCMD_UNKNOWN(cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_initups(void)
{
	microlink_read_config();
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, microlink_baudrate);
	ser_set_dtr(upsfd, 1);
}

void upsdrv_initinfo(void)
{
	int i;

	memset(objects, 0, sizeof(objects));
	session_ready = 0;
	rxbuf_len = 0;
	parsed_frames = 0;
	consecutive_timeouts = 0;
	poll_primed = 0;
	authentication_sent = 0;
	memset(&page0, 0, sizeof(page0));
	descriptor_ready = 0;
	poll_interval = 0;
	if (!microlink_start_session()) {
		fatalx(EXIT_FAILURE, "apcmicrolink: failed to start Microlink session on %s", device_path);
	}

	while (!microlink_startup_ready()) {
		if (!microlink_poll_once() && consecutive_timeouts >= MLINK_HANDSHAKE_RETRIES) {
			fatalx(EXIT_FAILURE,
				"apcmicrolink: timed out waiting for Microlink startup readiness on %s",
				device_path);
		}
	}

	microlink_publish_identity();
	microlink_publish_status();
	microlink_publish_runtime();

	for (i = 0; i < (int)microlink_desc_command_map_count; i++) {
		if (microlink_command_available(&microlink_desc_command_map[i])) {
			dstate_addcmd(microlink_desc_command_map[i].cmd_name);
		}
	}
	upsh.instcmd = instcmd;
	upsh.setvar = setvar;
}

void upsdrv_updateinfo(void)
{
	int good = 0;

	if (!session_ready && !microlink_start_session()) {
		dstate_datastale();
		return;
	}

	if (microlink_poll_once()) {
		good = 1;
	}

	if (!good && consecutive_timeouts >= MLINK_HANDSHAKE_RETRIES) {
		if (!microlink_reconnect_session()) {
			dstate_datastale();
			return;
		}
		good = 1;
	}

	if (!good) {
		if (parsed_frames == 0) {
			session_ready = 0;
			dstate_datastale();
			return;
		}

		microlink_publish_identity();
		microlink_publish_status();
		microlink_publish_runtime();
		dstate_dataok();
		return;
	}

	ser_comm_good();
	microlink_publish_identity();
	microlink_publish_status();
	microlink_publish_runtime();
	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	int ret;

	ret = instcmd("shutdown.return", NULL);
	if (ret != STAT_INSTCMD_HANDLED) {
		upslogx(LOG_ERR, "apcmicrolink: failed to issue shutdown.return");
		set_exit_flag(EF_EXIT_FAILURE);
	}
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "baudrate", "Serial port baud rate (e.g. 9600, 19200, 38400)");
	addvar(VAR_VALUE, "showinternals",
		"Show Microlink internal runtime values (yes/no, default follows debug mode)");
	addvar(VAR_VALUE, "showunmapped",
		"Show unmapped Microlink descriptor values (yes/no, default follows debug mode)");
}

void upsdrv_help(void)
{
}

void upsdrv_tweak_prognames(void)
{
}

void upsdrv_cleanup(void)
{
	if (VALID_FD(upsfd)) {
		ser_close(upsfd, device_path);
		upsfd = ERROR_FD;
	}
}
