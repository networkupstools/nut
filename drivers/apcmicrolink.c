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

#include "apc_common.h"
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

#define MLINK_DESC_OP_USAGE_SIZE	0xFC
#define MLINK_DESC_OP_COLLECTION	0xFD
#define MLINK_DESC_OP_CHILD_NEXT	0xFE
#define MLINK_DESC_OP_BLOCK_END		0xFF
#define MLINK_DESC_OP_SKIP_USAGE	0xFB
#define MLINK_DESC_OP_DOUBLE_SKIP	0xFA
#define MLINK_DESC_OP_SKIP_USAGE_ALT	0xF9
#define MLINK_DESC_OP_ENTER_BLOCK	0xF8
#define MLINK_DESC_OP_NOOP		0xF7
#define MLINK_DESC_OP_EXIT_BLOCK	0xF6
#define MLINK_DESC_OP_SKIP_NEXT		0xF5
#define MLINK_DESC_OP_RECURSE		0xF4
#define MLINK_DESC_OP_MIN		0xF4
#define MLINK_DESC_USAGE_MAX		0xDF

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
typedef enum microlink_command_source_e {
	MLINK_CMD_SOURCE_RJ45 = 0,
	MLINK_CMD_SOURCE_USB,
	MLINK_CMD_SOURCE_USER,
	MLINK_CMD_SOURCE_SMARTSLOT1,
	MLINK_CMD_SOURCE_INTERNAL1
} microlink_command_source_t;

static microlink_command_source_t microlink_command_source = MLINK_CMD_SOURCE_RJ45;

typedef enum microlink_command_domain_e {
	MLINK_CMD_DOMAIN_OUTLET = 0,
	MLINK_CMD_DOMAIN_BATTERY_TEST,
	MLINK_CMD_DOMAIN_RUNTIME_CAL,
	MLINK_CMD_DOMAIN_UPS
} microlink_command_domain_t;

static int microlink_send_simple(unsigned char byte);
static int microlink_send_write(unsigned char id, unsigned char offset,
	unsigned char len, const unsigned char *data);
static int microlink_update_blob(void);
static int microlink_parse_descriptor(void);
static int microlink_send_descriptor_mask_value(const char *path, uint64_t mask);
static int microlink_send_command_descriptor_mask_value(const char *path, uint64_t mask);
static int microlink_parse_descriptor_string_value(const char *val, size_t size,
	unsigned char *payload);
static int microlink_parse_descriptor_fixed_point_value(const microlink_desc_value_map_t *entry,
	const char *val, size_t size, unsigned char *payload);
static int microlink_parse_descriptor_hex_value(const char *val, size_t size,
	unsigned char *payload);
static int microlink_parse_descriptor_fixed_point_map_value(
	const microlink_desc_value_map_t *entry, const char *val, size_t size,
	unsigned char *payload);
static int microlink_parse_descriptor_date_value(const char *val, size_t size,
	unsigned char *payload);
static int microlink_parse_descriptor_time_value(const char *val, size_t size,
	unsigned char *payload);
static int microlink_parse_descriptor_map_value(const microlink_desc_value_map_t *entry,
	const char *val, size_t size, unsigned char *payload);
static const unsigned char *microlink_get_descriptor_data(const char *path, size_t size);
static int microlink_receive_once(void);
static const microlink_object_t *microlink_get_object(unsigned int id);
static microlink_object_t *microlink_get_object_mut(unsigned int id);
static size_t microlink_parse_descriptor_block(const unsigned char *blob, size_t blob_len,
	size_t pos, size_t *data_offset, const char *path);
static int microlink_set_descriptor_string_info(const char *name,
	const unsigned char *data, size_t size);
static int microlink_set_descriptor_hex_info(const char *name,
	const unsigned char *data, size_t size);
static int microlink_set_descriptor_fixed_point_info(const char *name,
	const unsigned char *data, size_t size, microlink_desc_numeric_sign_t sign,
	unsigned int bin_point);
static int microlink_set_descriptor_fixed_point_map_info(const char *name,
	const unsigned char *data, size_t size, microlink_desc_numeric_sign_t sign,
	unsigned int bin_point, const microlink_value_map_t *map);
static int microlink_set_descriptor_date_info(const char *name,
	const unsigned char *data, size_t size);
static int microlink_set_descriptor_time_info(const char *name,
	const unsigned char *data, size_t size);
static int microlink_publish_descriptor_entry(const char *name, const char *path,
	const microlink_desc_value_map_t *entry);
static size_t microlink_parse_descriptor_collection(const unsigned char *blob, size_t blob_len,
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

static int microlink_parse_command_source(const char *text, microlink_command_source_t *source)
{
	if (text == NULL || source == NULL) {
		return 0;
	}

	if (!strcasecmp(text, "rj45")) {
		*source = MLINK_CMD_SOURCE_RJ45;
		return 1;
	}

	if (!strcasecmp(text, "usb")) {
		*source = MLINK_CMD_SOURCE_USB;
		return 1;
	}

	if (!strcasecmp(text, "localuser")) {
		*source = MLINK_CMD_SOURCE_USER;
		return 1;
	}

	if (!strcasecmp(text, "smartslot1")) {
		*source = MLINK_CMD_SOURCE_SMARTSLOT1;
		return 1;
	}

	if (!strcasecmp(text, "internalnetwork1")) {
		*source = MLINK_CMD_SOURCE_INTERNAL1;
		return 1;
	}

	return 0;
}

static uint64_t microlink_command_source_bit(microlink_command_domain_t domain)
{
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic push
#endif
#pragma GCC diagnostic ignored "-Wswitch-default"
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE_BREAK
#pragma GCC diagnostic ignored "-Wunreachable-code-break"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-default"
#pragma clang diagnostic ignored "-Wunreachable-code-break"
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif
	switch (domain) {
	case MLINK_CMD_DOMAIN_OUTLET:
		switch (microlink_command_source) {
		case MLINK_CMD_SOURCE_RJ45:
			return APC_OUTLET_CMD_SOURCE_RJ45_PORT;
		case MLINK_CMD_SOURCE_USB:
			return APC_OUTLET_CMD_SOURCE_USB_PORT;
		case MLINK_CMD_SOURCE_USER:
			return APC_OUTLET_CMD_SOURCE_LOCAL_USER;
		case MLINK_CMD_SOURCE_SMARTSLOT1:
			return APC_OUTLET_CMD_SOURCE_SMART_SLOT_1;
		case MLINK_CMD_SOURCE_INTERNAL1:
			return APC_OUTLET_CMD_SOURCE_INTERNAL_NETWORK_1;
		}
		break;
	case MLINK_CMD_DOMAIN_BATTERY_TEST:
	case MLINK_CMD_DOMAIN_RUNTIME_CAL:
		switch (microlink_command_source) {
		case MLINK_CMD_SOURCE_RJ45:
			return (1ULL << 10);
		case MLINK_CMD_SOURCE_USB:
			return (1ULL << 8);
		case MLINK_CMD_SOURCE_USER:
			return (1ULL << 9);
		case MLINK_CMD_SOURCE_SMARTSLOT1:
			return (1ULL << 11);
		case MLINK_CMD_SOURCE_INTERNAL1:
			return (1ULL << 13);
		}
		break;
	case MLINK_CMD_DOMAIN_UPS:
		switch (microlink_command_source) {
		case MLINK_CMD_SOURCE_RJ45:
			return (1ULL << 27);
		case MLINK_CMD_SOURCE_USB:
			return (1ULL << 28);
		case MLINK_CMD_SOURCE_USER:
			return (1ULL << 29);
		case MLINK_CMD_SOURCE_SMARTSLOT1:
			return (1ULL << 30);
		case MLINK_CMD_SOURCE_INTERNAL1:
			return (1ULL << 31);
		}
		break;
	}

	return 0;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic pop
#endif
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

	if (testvar("cmdsrc")) {
		value = getval("cmdsrc");
		if (value == NULL) {
			microlink_command_source = MLINK_CMD_SOURCE_RJ45;
		} else if (!microlink_parse_command_source(value, &microlink_command_source)) {
			fatalx(EXIT_FAILURE, "apcmicrolink: invalid cmdsrc value '%s'",
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
	return token >= MLINK_DESC_OP_MIN;
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

static const microlink_descriptor_usage_t *microlink_find_descriptor_usage_validated(const char *path)
{
	const microlink_descriptor_usage_t *usage;

	if (!descriptor_ready) {
		upsdebugx(1, "descriptor not ready!");
		return NULL;
	}

	usage = microlink_find_descriptor_usage(path);
	if (usage == NULL || usage->skipped ||
		usage->data_offset + usage->size > descriptor_blob_len) {
		return NULL;
	}

	return usage;
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

static size_t microlink_outlet_group_count(void)
{
	size_t group_count = 1;
	char path[32];
	size_t i;

	if (microlink_find_descriptor_usage("2:4.3E.B6") == NULL) {
		return 0;
	}

	for (i = 0; i < 4; i++) {
		snprintf(path, sizeof(path), "2:4.3D[%zu].B6", i);
		if (microlink_find_descriptor_usage(path) == NULL) {
			break;
		}
		group_count++;
	}

	return group_count;
}

static uint64_t microlink_outlet_target_bits_for_group(size_t group_idx)
{
	switch (group_idx) {
	case 0:
		return APC_OUTLET_CMD_TARGET_MAIN;
	case 1:
		return APC_OUTLET_CMD_TARGET_SWITCHED0;
	case 2:
		return APC_OUTLET_CMD_TARGET_SWITCHED1;
	case 3:
		return APC_OUTLET_CMD_TARGET_SWITCHED2;
	case 4:
		return APC_OUTLET_CMD_TARGET_SWITCHED3;
	default:
		return 0;
	}
}

static uint64_t microlink_outlet_all_targets(size_t group_count)
{
	uint64_t targets = 0;
	size_t i;

	for (i = 0; i < group_count; i++) {
		targets |= microlink_outlet_target_bits_for_group(i);
	}

	return targets;
}

static int microlink_set_descriptor_string_info(const char *name,
	const unsigned char *data, size_t size)
{
	char value[MLINK_MAX_PAYLOAD + 1];

	if (data == NULL || size == 0 || size > MLINK_MAX_PAYLOAD) {
		return 0;
	}

	microlink_format_ascii(data, size, value, sizeof(value));
	if (value[0] == '\0') {
		return 0;
	}

	dstate_setinfo(name, "%s", value);
	return 1;
}

static int microlink_set_descriptor_hex_info(const char *name,
	const unsigned char *data, size_t size)
{
	uint64_t raw = 0;
	size_t i;
	char text[32];

	if (data == NULL || size == 0) {
		return 0;
	}

	if (size > sizeof(raw)) {
		return 0;
	}

	for (i = 0; i < size; i++) {
		raw = (raw << 8) | data[i];
	}

	/* Keep fixed-size identity fields readable and comparable with Modbus. */
	snprintf(text, sizeof(text), "%0*llx", (int)(size * 2),
		(unsigned long long)raw);
	dstate_setinfo(name, "%s", text);
	return 1;
}

typedef enum microlink_map_mode_e {
	MLINK_MAP_BITFIELD,
	MLINK_MAP_VALUE
} microlink_map_mode_t;

static int microlink_set_descriptor_map_info(const char *name,
	const unsigned char *data, size_t size, const microlink_value_map_t *map,
	microlink_map_mode_t mode)
{
	const char *zero_text = NULL;
	uint32_t raw = 0;
	int32_t value = 0;
	char buf[128];
	size_t i, used = 0;
	int matched = 0;

	if (data == NULL || size == 0 || size > sizeof(raw)) {
		return 0;
	}

	for (i = 0; i < size; i++) {
		raw = (raw << 8) | data[i];
	}

	/* Bitfields concatenate set labels; enums resolve to one label/value. */
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
			/* Keep the raw value visible when no label matches. */
			snprintf(buf, sizeof(buf), "0x%0*lX",
				(int)(size * 2), (unsigned long)raw);
			dstate_setinfo(name, "%s", buf);
		}

		return 1;
	}

	{
		/* Only interpret the top bit as sign when the descriptor expects it. */
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

	/* Fall back to the numeric value if the map does not know the label. */
	dstate_setinfo(name, "%ld", (long)value);
	return 1;
}

static int microlink_set_descriptor_fixed_point_map_info(const char *name,
	const unsigned char *data, size_t size, microlink_desc_numeric_sign_t sign,
	unsigned int bin_point, const microlink_value_map_t *map)
{
	uint32_t raw = 0;
	int32_t signed_raw = 0;
	double value;
	char text[64];

	size_t i;

	if (data == NULL || size == 0) {
		return 0;
	}

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

		for (i = 0; map != NULL && map[i].text != NULL; i++) {
			if (signed_raw == map[i].value) {
				dstate_setinfo(name, "%s", map[i].text);
				return 1;
			}
		}

		value = (double)signed_raw;
	} else {
		for (i = 0; map != NULL && map[i].text != NULL; i++) {
			if ((int32_t)raw == map[i].value) {
				dstate_setinfo(name, "%s", map[i].text);
				return 1;
			}
		}

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

static int microlink_handle_outlet_cmd(const char *nut_cmdname, const char *extra, int *result)
{
	size_t group_count, group_idx = 0;
	uint64_t target_bits = 0;
	apc_outlet_command_type_t cmd_type = APC_OUTLET_OP_NULL;
	const char *suffix = NULL;
	char *endptr = NULL;
	size_t i;

	if (nut_cmdname == NULL || result == NULL) {
		return 0;
	}

	group_count = microlink_outlet_group_count();
	if (group_count == 0) {
		return 0;
	}

	if (strncmp(nut_cmdname, "load.", 5) == 0 || strncmp(nut_cmdname, "shutdown.", 9) == 0) {
		suffix = (strcmp(nut_cmdname, "shutdown.default") == 0) ? "shutdown.return" : nut_cmdname;
		target_bits = microlink_outlet_all_targets(group_count);
	} else if (strncmp(nut_cmdname, "outlet.group.", 13) == 0) {
		const char *p = nut_cmdname + 13;

		group_idx = strtoul(p, &endptr, 10);
		if (endptr == p || endptr == NULL || *endptr != '.') {
			return 0;
		}

		if (group_idx >= group_count) {
			upslogx(LOG_ERR, "%s: Invalid outlet group index %zu in command [%s]",
				__func__, group_idx, nut_cmdname);
			*result = STAT_INSTCMD_INVALID;
			return 1;
		}

		suffix = endptr + 1;
		target_bits = microlink_outlet_target_bits_for_group(group_idx);
		if (target_bits == 0) {
			upslogx(LOG_ERR, "%s: Outlet group %zu not available for command [%s]",
				__func__, group_idx, nut_cmdname);
			*result = STAT_INSTCMD_INVALID;
			return 1;
		}
	} else {
		return 0;
	}

	for (i = 0; apc_outlet_command_suffixes[i].suffix; i++) {
		if (strcmp(suffix, apc_outlet_command_suffixes[i].suffix) == 0) {
			cmd_type = apc_outlet_command_suffixes[i].type;
			break;
		}
	}

	if (cmd_type == APC_OUTLET_OP_NULL) {
		return 0;
	}

	upslog_INSTCMD_POWERSTATE_CHECKED(nut_cmdname, extra);
	if (!microlink_send_command_descriptor_mask_value("2:4.B5",
		apc_build_outlet_command(cmd_type, target_bits) | microlink_command_source_bit(MLINK_CMD_DOMAIN_OUTLET))) {
		*result = STAT_INSTCMD_FAILED;
		return 1;
	}

	*result = STAT_INSTCMD_HANDLED;
	return 1;
}

static int microlink_handle_simple_instcmd(const char *nut_cmdname, const char *extra, int *result)
{
	uint64_t value;

	if (nut_cmdname == NULL || result == NULL) {
		return 0;
	}

	if (!strcasecmp(nut_cmdname, "test.battery.start")) {
		value = APC_BATTERY_TEST_CMD_START;
	} else if (!strcasecmp(nut_cmdname, "test.battery.stop")) {
		value = APC_BATTERY_TEST_CMD_ABORT;
	} else if (!strcasecmp(nut_cmdname, "test.panel.start")) {
		value = APC_USER_IF_CMD_SHORT_TEST;
	} else if (!strcasecmp(nut_cmdname, "beeper.mute")) {
		value = APC_USER_IF_CMD_MUTE_ALL_ACTIVE_AUDIBLE_ALARMS;
	} else if (!strcasecmp(nut_cmdname, "calibrate.start")) {
		value = APC_RUNTIME_CAL_CMD_START;
	} else if (!strcasecmp(nut_cmdname, "calibrate.stop")) {
		value = APC_RUNTIME_CAL_CMD_ABORT;
	} else if (!strcasecmp(nut_cmdname, "bypass.start")) {
		value = APC_UPS_CMD_OUTPUT_INTO_BYPASS;
	} else if (!strcasecmp(nut_cmdname, "bypass.stop")) {
		value = APC_UPS_CMD_OUTPUT_OUT_OF_BYPASS;
	} else {
		return 0;
	}

	upslog_INSTCMD_POWERSTATE_CHECKED(nut_cmdname, extra);

	if (!strcasecmp(nut_cmdname, "test.battery.start") || !strcasecmp(nut_cmdname, "test.battery.stop")) {
		value |= microlink_command_source_bit(MLINK_CMD_DOMAIN_BATTERY_TEST);
		*result = microlink_send_command_descriptor_mask_value("2:10", value)
			? STAT_INSTCMD_HANDLED : STAT_INSTCMD_FAILED;
	} else if (!strcasecmp(nut_cmdname, "test.panel.start") || !strcasecmp(nut_cmdname, "beeper.mute")) {
		*result = microlink_send_command_descriptor_mask_value("2:4.B.3B", value)
			? STAT_INSTCMD_HANDLED : STAT_INSTCMD_FAILED;
	} else if (!strcasecmp(nut_cmdname, "calibrate.start") || !strcasecmp(nut_cmdname, "calibrate.stop")) {
		value |= microlink_command_source_bit(MLINK_CMD_DOMAIN_RUNTIME_CAL);
		*result = microlink_send_command_descriptor_mask_value("2:12", value)
			? STAT_INSTCMD_HANDLED : STAT_INSTCMD_FAILED;
	} else {
		value |= microlink_command_source_bit(MLINK_CMD_DOMAIN_UPS);
		*result = microlink_send_command_descriptor_mask_value("2:14", value)
			? STAT_INSTCMD_HANDLED : STAT_INSTCMD_FAILED;
	}

	return 1;
}

static int microlink_set_descriptor_fixed_point_info(const char *name,
	const unsigned char *data, size_t size, microlink_desc_numeric_sign_t sign,
	unsigned int bin_point)
{
	uint32_t raw = 0;
	int32_t signed_raw = 0;
	double value;
	char text[32];
	size_t i;

	if (data == NULL || size == 0) {
		return 0;
	}

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

static uint64_t microlink_max_unsigned_for_size(size_t size)
{
	if (size >= sizeof(uint64_t)) {
		return UINT64_MAX;
	}

	return ((uint64_t)1 << (size * 8U)) - 1U;
}

static int microlink_value_fits_descriptor(int64_t raw, microlink_desc_numeric_sign_t sign,
	size_t size)
{
	if (size == 0 || size > 8) {
		return 0;
	}

	if (sign == MLINK_DESC_SIGNED) {
		int64_t min_raw, max_raw;

		if (size >= sizeof(int64_t)) {
			min_raw = INT64_MIN;
			max_raw = INT64_MAX;
		} else {
			uint64_t limit = 1ULL << ((size * 8U) - 1U);

			min_raw = -(int64_t)limit;
			max_raw = (int64_t)(limit - 1U);
		}

		return raw >= min_raw && raw <= max_raw;
	}

	return raw >= 0 && (uint64_t)raw <= microlink_max_unsigned_for_size(size);
}

static int microlink_parse_fixed_point_value(const microlink_desc_value_map_t *entry,
	const char *val, int64_t *raw_out)
{
	char *endptr = NULL;
	int64_t raw;

	if (entry == NULL || val == NULL || raw_out == NULL) {
		return 0;
	}

	if (entry->bin_point == 0U) {
		long long parsed = strtoll(val, &endptr, 10);

		if (endptr == val || *endptr != '\0') {
			return 0;
		}

		raw = (int64_t)parsed;
	} else {
		double numeric = strtod(val, &endptr);
		double scaled;

		if (endptr == val || *endptr != '\0') {
			return 0;
		}

		scaled = numeric * (double)(1U << entry->bin_point);
		raw = (int64_t)((scaled >= 0.0) ? (scaled + 0.5) : (scaled - 0.5));
	}

	*raw_out = raw;
	return 1;
}

static int microlink_lookup_value_map(const microlink_value_map_t *map, const char *val,
	int64_t *raw_out)
{
	size_t j;

	if (map == NULL || val == NULL || raw_out == NULL) {
		return 0;
	}

	for (j = 0; map[j].text != NULL; j++) {
		if (!strcasecmp(map[j].text, val)) {
			*raw_out = map[j].value;
			return 1;
		}
	}

	return 0;
}

/* Strings are copied into fixed-width payloads and zero-padded if needed. */
static int microlink_parse_descriptor_string_value(const char *val, size_t size,
	unsigned char *payload)
{
	size_t i;

	if (val == NULL || payload == NULL) {
		return 0;
	}

	memset(payload, 0, size);
	for (i = 0; i < size && val[i] != '\0'; i++) {
		payload[i] = (unsigned char)val[i];
	}

	return 1;
}

static int microlink_parse_descriptor_fixed_point_value(const microlink_desc_value_map_t *entry,
	const char *val, size_t size, unsigned char *payload)
{
	int64_t raw;
	size_t i;

	if (entry == NULL || val == NULL || payload == NULL) {
		return 0;
	}

	if (size == 0 || size > 8) {
		return 0;
	}

	if (!microlink_parse_fixed_point_value(entry, val, &raw)
	 || !microlink_value_fits_descriptor(raw, entry->sign, size)) {
		return 0;
	}

	for (i = 0; i < size; i++) {
		size_t shift = (size - 1U - i) * 8U;
		payload[i] = (unsigned char)(((uint64_t)raw >> shift) & 0xFFU);
	}

	return 1;
}

static int microlink_parse_descriptor_hex_value(const char *val, size_t size,
	unsigned char *payload)
{
	uint64_t raw;
	char *endptr = NULL;
	size_t i;

	if (val == NULL || payload == NULL || size == 0 || size > 8) {
		return 0;
	}

	errno = 0;
	raw = strtoull(val, &endptr, 16);
	if (endptr == val || *endptr != '\0' || errno > 0
	 || raw > microlink_max_unsigned_for_size(size)) {
		return 0;
	}

	for (i = 0; i < size; i++) {
		size_t shift = (size - 1U - i) * 8U;
		payload[i] = (unsigned char)((raw >> shift) & 0xFFU);
	}

	return 1;
}

static int microlink_parse_descriptor_fixed_point_map_value(
	const microlink_desc_value_map_t *entry, const char *val, size_t size,
	unsigned char *payload)
{
	int64_t raw;
	size_t i;

	if (entry == NULL || val == NULL || payload == NULL) {
		return 0;
	}

	if (size == 0 || size > 8) {
		return 0;
	}

	/* Try the symbolic label first, then fall back to the fixed-point parser. */
	if (!microlink_lookup_value_map(entry->map, val, &raw)) {
		if (!microlink_parse_fixed_point_value(entry, val, &raw)) {
			return 0;
		}
	}

	if (!microlink_value_fits_descriptor(raw, entry->sign, size)) {
		return 0;
	}

	for (i = 0; i < size; i++) {
		size_t shift = (size - 1U - i) * 8U;
		payload[i] = (unsigned char)(((uint64_t)raw >> shift) & 0xFFU);
	}

	return 1;
}

static int microlink_parse_descriptor_date_value(const char *val, size_t size,
	unsigned char *payload)
{
	uint64_t raw;
	size_t i;

	if (val == NULL || payload == NULL) {
		return 0;
	}

	if (size == 0 || size > 8) {
		return 0;
	}

	if (!apc_parse_date_to_days_offset(val, &raw)
	 || raw > microlink_max_unsigned_for_size(size)) {
		return 0;
	}

	/* Date fields are packed as day counts in big-endian byte order. */
	for (i = 0; i < size; i++) {
		size_t shift = (size - 1U - i) * 8U;
		payload[i] = (unsigned char)((raw >> shift) & 0xFFU);
	}

	return 1;
}

static int microlink_parse_descriptor_time_value(const char *val, size_t size,
	unsigned char *payload)
{
	unsigned int hours, minutes, seconds;
	uint64_t raw;
	size_t i;

	if (val == NULL || payload == NULL) {
		return 0;
	}

	if (size == 0 || size > 8) {
		return 0;
	}

	if (sscanf(val, "%u:%u:%u", &hours, &minutes, &seconds) != 3) {
		return 0;
	}

	if (minutes > 59U || seconds > 59U) {
		return 0;
	}

	raw = ((uint64_t)hours * 3600U) + ((uint64_t)minutes * 60U) + (uint64_t)seconds;
	if (raw > microlink_max_unsigned_for_size(size)) {
		return 0;
	}

	/* Time fields are packed as elapsed seconds in big-endian byte order. */
	for (i = 0; i < size; i++) {
		size_t shift = (size - 1U - i) * 8U;
		payload[i] = (unsigned char)((raw >> shift) & 0xFFU);
	}

	return 1;
}

static int microlink_parse_descriptor_map_value(const microlink_desc_value_map_t *entry,
	const char *val, size_t size, unsigned char *payload)
{
	int64_t raw = 0;
	char *endptr = NULL;
	size_t i;

	if (entry == NULL || val == NULL || payload == NULL) {
		return 0;
	}

	if (!microlink_lookup_value_map(entry->map, val, &raw)) {
		if (entry->type == MLINK_DESC_ENUM_MAP) {
			raw = (int64_t)strtoll(val, &endptr, 0);
		} else {
			raw = (int64_t)strtoull(val, &endptr, 0);
		}

		if (endptr == val || *endptr != '\0') {
			return 0;
		}
	}

	if (!microlink_value_fits_descriptor(raw,
		entry->type == MLINK_DESC_ENUM_MAP ? entry->sign : MLINK_DESC_UNSIGNED,
		size)) {
		return 0;
	}

	/* Enums and bitfields share the same width checks and packing. */
	for (i = 0; i < size; i++) {
		size_t shift = (size - 1U - i) * 8U;
		payload[i] = (unsigned char)(((uint64_t)raw >> shift) & 0xFFU);
	}

	return 1;
}

static int microlink_publish_descriptor_entry(const char *name, const char *path,
	const microlink_desc_value_map_t *entry)
{
	const microlink_descriptor_usage_t *usage;
	const unsigned char *data;
	size_t size;

	usage = microlink_find_descriptor_usage(path);
	if (usage == NULL || usage->skipped) {
		return 0;
	}

	size = usage->size;
	if (usage->data_offset + size > descriptor_blob_len) {
		return 0;
	}

	data = microlink_get_descriptor_data(path, size);
	if (data == NULL) {
		return 0;
	}

	/* Keep the per-type export logic centralized but short. */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wcovered-switch-default"
# pragma clang diagnostic ignored "-Wunreachable-code"
#endif
	switch (entry->type) {
	case MLINK_DESC_STRING:
		return microlink_set_descriptor_string_info(name, data, size);
	case MLINK_DESC_HEX:
		return microlink_set_descriptor_hex_info(name, data, size);
	case MLINK_DESC_FIXED_POINT:
		return microlink_set_descriptor_fixed_point_info(name, data, size,
			entry->sign, entry->bin_point);
	case MLINK_DESC_FIXED_POINT_MAP:
		return microlink_set_descriptor_fixed_point_map_info(name, data, size,
			entry->sign, entry->bin_point, entry->map);
	case MLINK_DESC_DATE:
		return microlink_set_descriptor_date_info(name, data, size);
	case MLINK_DESC_TIME:
		return microlink_set_descriptor_time_info(name, data, size);
	case MLINK_DESC_BITFIELD_MAP:
		return microlink_set_descriptor_map_info(name, data, size, entry->map,
			MLINK_MAP_BITFIELD);
	case MLINK_DESC_ENUM_MAP:
		return microlink_set_descriptor_map_info(name, data, size, entry->map,
			MLINK_MAP_VALUE);
	case MLINK_DESC_NONE:
	default:
		return 0;
	}
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
}

static size_t microlink_parse_descriptor_collection(const unsigned char *blob, size_t blob_len,
	size_t pos, size_t *data_offset, const char *path)
{
	unsigned char collection_id;
	unsigned char count;
	size_t block_start;
	size_t block_end = 0;
	unsigned int idx;

	if (pos + 1 >= blob_len) {
		return 0;
	}

	/* Collections reuse the same payload block for each indexed child entry. */
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

	return block_end;
}

static int microlink_set_descriptor_date_info(const char *name,
	const unsigned char *data, size_t size)
{
	uint64_t raw = 0;
	char text[16];
	size_t i;

	if (data == NULL || size == 0 || size > sizeof(raw)) {
		return 0;
	}

	for (i = 0; i < size; i++) {
		raw = (raw << 8) | data[i];
	}

	return apc_format_date_from_days_offset((int64_t)raw, text, sizeof(text)) &&
		dstate_setinfo(name, "%s", text);
}

static int microlink_set_descriptor_time_info(const char *name,
	const unsigned char *data, size_t size)
{
	uint64_t raw = 0;
	unsigned int hours, minutes, seconds;
	char text[16];
	size_t i;

	if (data == NULL || size == 0 || size > sizeof(raw)) {
		return 0;
	}

	for (i = 0; i < size; i++) {
		raw = (raw << 8) | data[i];
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
	 || usage->size > sizeof(raw)) {
		return 0;
	}

	data = microlink_get_descriptor_data(path, usage->size);
	if (data == NULL) {
		return 0;
	}

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

	if ((page0.flags & (MLINK_PAGE0_FLAG_DESCRIPTOR_PRESENT | MLINK_PAGE0_FLAG_AUTH_REQUIRED)) != 0U
	 && !descriptor_ready) {
		return 0;
	}

	if ((page0.flags & MLINK_PAGE0_FLAG_AUTH_REQUIRED) == 0U) {
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
			if (strcmp(map[i].text, "None") == 0) {
				continue;
			}
			matched = 1;
			alarm_set(map[i].text);
		}
	}

	if (!matched) {
		for (i = 0; map[i].text != NULL; i++) {
			if (map[i].value == 0) {
				if (strcmp(map[i].text, "None") != 0) {
					alarm_set(map[i].text);
				}
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
			if (strcmp(map[i].text, "None") == 0) {
				continue;
			}
			matched = 1;
			status_set(map[i].text);
		}
	}

	if (!matched) {
		for (i = 0; map[i].text != NULL; i++) {
			if (map[i].value == 0) {
				if (strcmp(map[i].text, "None") != 0) {
					status_set(map[i].text);
				}
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

	/* Make the attempted descriptor write visible before the raw frame is sent. */
	upsdebugx(2, "microlink: write %s page=%zu offset=%zu size=%zu",
		path, page, offset, usage->size);

	if (!microlink_send_write((unsigned char)page, (unsigned char)offset,
		(unsigned char)usage->size, payload)) {
		return 0;
	}

	memcpy(descriptor_blob + usage->data_offset, payload, usage->size);
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_TYPE_LIMIT_COMPARE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_TYPE_LIMIT_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-type-limit-compare"
#endif
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wtautological-compare"
# pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
	if (page <= (size_t)UCHAR_MAX) {
		microlink_object_t *obj = microlink_get_object_mut((unsigned int)page);
		if (obj->seen && obj->len >= offset + usage->size) {
			memcpy(obj->data + offset, payload, usage->size);
		}
	}
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_TYPE_LIMIT_COMPARE) )
# pragma GCC diagnostic pop
#endif

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

static int microlink_send_command_descriptor_mask_value(const char *path, uint64_t mask)
{
	/* Command writes should happen after the current poll turn has been consumed. */
	if (poll_primed) {
		upsdebugx(2, "microlink: draining in-flight poll before command write");
		if (!microlink_receive_once()) {
			return 0;
		}
		poll_primed = 0;
		consecutive_timeouts = 0;
	}

	return microlink_send_descriptor_mask_value(path, mask);
}

static int microlink_send_descriptor_typed_value(const microlink_desc_value_map_t *entry,
	const char *path, const char *val)
{
	const microlink_descriptor_usage_t *usage;
	unsigned char payload[MLINK_MAX_PAYLOAD];
	size_t size = 0;

	if (entry == NULL || path == NULL || val == NULL) {
		return 0;
	}

	usage = microlink_find_descriptor_usage(path);
	if (usage == NULL || usage->skipped || !usage->valid || usage->size == 0
	 || usage->size > sizeof(payload)) {
		return 0;
	}

	size = usage->size;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
# pragma GCC diagnostic ignored "-Wswitch-enum"
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wswitch-enum"
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
	switch (entry->type) {
	case MLINK_DESC_STRING:
		/* Strings are written as fixed-width payloads. */
		if (!microlink_parse_descriptor_string_value(val, size, payload)) {
			return 0;
		}
		break;
	case MLINK_DESC_FIXED_POINT:
		/* Fixed-point descriptors use the configured binary scale. */
		if (!microlink_parse_descriptor_fixed_point_value(entry, val, size, payload)) {
			return 0;
		}
		break;
	case MLINK_DESC_HEX:
		if (!microlink_parse_descriptor_hex_value(val, size, payload)) {
			return 0;
		}
		break;
	case MLINK_DESC_FIXED_POINT_MAP:
		if (!microlink_parse_descriptor_fixed_point_map_value(entry, val, size, payload)) {
			return 0;
		}
		break;
	case MLINK_DESC_DATE:
		if (!microlink_parse_descriptor_date_value(val, size, payload)) {
			return 0;
		}
		break;
	case MLINK_DESC_TIME:
		if (!microlink_parse_descriptor_time_value(val, size, payload)) {
			return 0;
		}
		break;
	case MLINK_DESC_ENUM_MAP:
	case MLINK_DESC_BITFIELD_MAP:
		if (!microlink_parse_descriptor_map_value(entry, val, size, payload)) {
			return 0;
		}
		break;
	case MLINK_DESC_NONE:
	default:
		return 0;
	}
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif

	return microlink_send_descriptor_write(path, payload, size);
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
			microlink_publish_descriptor_entry(name, usage->path, entry);

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

		if (token == MLINK_DESC_OP_USAGE_SIZE) {
			if (pos + 1 >= blob_len) {
				return 0;
			}
			usage_size = blob[pos + 1];
			pos += 2;
			continue;
		}

		if (token == MLINK_DESC_OP_SKIP_USAGE || token == MLINK_DESC_OP_SKIP_USAGE_ALT) {
			pos++;
			if (pos + usage_size > blob_len) {
				return 0;
			}
			pos += usage_size;
			continue;
		}

		if (token == MLINK_DESC_OP_DOUBLE_SKIP) {
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
		case MLINK_DESC_OP_RECURSE:
			pos = microlink_parse_descriptor_block(blob, blob_len, pos, data_offset, path);
			if (pos == 0) {
				return 0;
			}
			break;
		case MLINK_DESC_OP_SKIP_NEXT:
			skip_next = 1;
			break;
		case MLINK_DESC_OP_EXIT_BLOCK:
		case MLINK_DESC_OP_BLOCK_END:
			return pos;
		case MLINK_DESC_OP_NOOP:
			break;
		case MLINK_DESC_OP_ENTER_BLOCK:
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
		case MLINK_DESC_OP_CHILD_NEXT:
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
		case MLINK_DESC_OP_COLLECTION:
		{
			pos = microlink_parse_descriptor_collection(blob, blob_len, pos, data_offset, path);
			if (pos == 0) {
				return 0;
			}
			break;
		}
		default:
			if (token == 0x00 || microlink_is_descriptor_operator(token) || token > MLINK_DESC_USAGE_MAX) {
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

	if ((page0.flags & MLINK_PAGE0_FLAG_DESCRIPTOR_PRESENT) == 0U
	 || page0.descriptor_version != 0x01U) {
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
		page0.flags = (obj->len >= 7) ? obj->data[6] : 0;
		page0.descriptor_version = (obj->len >= 9) ? obj->data[8] : 0;
		page0.descriptor_ptr = (obj->len >= 12)
			? (uint16_t)(((uint16_t)obj->data[10] << 8) | (uint16_t)obj->data[11])
			: 0;
		upsdebugx(2, "microlink: page0 version=%u width=%u pages=%u flags=0x%02X",
			(unsigned int)page0.version,
			(unsigned int)page0.width,
			page0.count,
			(unsigned int)page0.flags);
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

static int microlink_try_extract_frame_at(unsigned char *frame, size_t *framelen,
	const unsigned char *const sourcebuf, const size_t sourcebuf_len)
{
	size_t current_framelen = 0;

	*framelen = 0;

	if (!microlink_get_object(MLINK_OBJ_PROTOCOL)->seen) {
		/* If page0 not already seen, get page0.width manually */
		if (sourcebuf_len < 3) {
			return 0;
		}

		if (sourcebuf[0] != 0x00) {
			return 0;
		}

		current_framelen = sourcebuf[2] + 3;
	} else {
		/* Else, use page0 */
		current_framelen = page0.width + 3;
	}

	if (sourcebuf_len < current_framelen) {
		return 0;
	}

	if (current_framelen > MLINK_MAX_FRAME) {
		return 0;
	}

	if (!microlink_checksum_valid(sourcebuf, current_framelen)) {
		return 0;
	}

	memcpy(frame, sourcebuf, current_framelen);
	*framelen = current_framelen;

	return 1;
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

		if (microlink_try_extract_frame_at(frame, framelen, rxbuf + start, rxbuf_len - start)) {
			if (start > 0) {
				upsdebugx(2, "microlink: skipped %u stray byte(s) before record 0x%02X",
					(unsigned int)start, rxbuf[start]);
				memmove(rxbuf, rxbuf + start, rxbuf_len - start);
				rxbuf_len -= start;
			}

			memmove(rxbuf, rxbuf + *framelen, rxbuf_len - *framelen);
			rxbuf_len -= *framelen;
			microlink_trace_frame(2, "RX record", frame, *framelen);
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
	const microlink_descriptor_usage_t *serial_usage = NULL;
	const unsigned char *master_password;
	unsigned char s0, s1;
	unsigned char payload[4];

	if (!protocol->seen || protocol->len < 8) {
		upsdebugx(1, "microlink: authentication requested before protocol header was cached");
		return 0;
	}

	serial_usage = microlink_find_descriptor_usage_validated(MLINK_DESC_SERIALNUMBER);
	master_password = microlink_get_descriptor_data(MLINK_DESC_MASTER_PASSWORD, 4);

	if (serial_usage == NULL || master_password == NULL) {
		upsdebugx(1, "microlink: authentication requested before required descriptors were cached");
		return 0;
	}

	s0 = protocol->data[4];
	s1 = protocol->data[3];

	microlink_auth_update(&s0, &s1, protocol->data, 8);
	microlink_auth_update(&s0, &s1, descriptor_blob + serial_usage->data_offset, serial_usage->size);
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
		if ((page0.flags & MLINK_PAGE0_FLAG_DESCRIPTOR_PRESENT) != 0U) {
			if (descriptor_ready) {
				microlink_update_blob();
			} else {
				microlink_parse_descriptor();
			}
		}

		if ((page0.flags & MLINK_PAGE0_FLAG_AUTH_REQUIRED) != 0U
		 && descriptor_ready && !authentication_sent) {
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
		poll_primed = 0;
		return 1;
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
	dstate_setinfo("device.type", "ups");
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
		snprintf(flags, sizeof(flags), "0x%02X", page0.flags);
		dstate_setinfo("microlink.flags", "%s", flags);
		dstate_setinfo("microlink.flag.auth_required", "%u",
			(unsigned int)((page0.flags & MLINK_PAGE0_FLAG_AUTH_REQUIRED) != 0U));
		dstate_setinfo("microlink.flag.implicit_stuffing", "%u",
			(unsigned int)((page0.flags & MLINK_PAGE0_FLAG_IMPLICIT_STUFFING) != 0U));
		dstate_setinfo("microlink.flag.descriptor_present", "%u",
			(unsigned int)((page0.flags & MLINK_PAGE0_FLAG_DESCRIPTOR_PRESENT) != 0U));
		dstate_setinfo("microlink.flag.firmware_update_needed", "%u",
			(unsigned int)((page0.flags & MLINK_PAGE0_FLAG_FIRMWARE_UPDATE_NEEDED) != 0U));
	}

	if (protocol->seen && protocol->len >= 12
	 && (page0.flags & MLINK_PAGE0_FLAG_DESCRIPTOR_PRESENT) != 0U) {
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
		upsdebugx(2, "microlink: setvar %s -> %s via %s", varname, val, path);
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
	int ret = STAT_INSTCMD_INVALID;

	upsdebug_INSTCMD_STARTING(cmdname, extra);

	if (microlink_handle_outlet_cmd(cmdname, extra, &ret)) {
		upslog_INSTCMD_RESULT(ret, cmdname, extra);
		return ret;
	}

	if (microlink_handle_simple_instcmd(cmdname, extra, &ret)) {
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
	size_t outlet_group_count, g;
	static const char *const outlet_suffixes[] = {
		"load.off",
		"load.on",
		"load.cycle",
		"load.off.delay",
		"load.on.delay",
		"shutdown.default",
		"shutdown.return",
		"shutdown.stayoff",
		"shutdown.reboot",
		"shutdown.reboot.graceful",
		NULL
	};

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

	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("test.panel.start");
	dstate_addcmd("beeper.mute");
	dstate_addcmd("calibrate.start");
	dstate_addcmd("calibrate.stop");
	dstate_addcmd("bypass.start");
	dstate_addcmd("bypass.stop");

	outlet_group_count = microlink_outlet_group_count();
	if (outlet_group_count > 0) {
		char cmd[64];

		dstate_setinfo("outlet.group.count", "%u", (unsigned int)outlet_group_count);

		dstate_addcmd("load.off");
		dstate_addcmd("load.on");
		dstate_addcmd("load.cycle");
		dstate_addcmd("load.off.delay");
		dstate_addcmd("load.on.delay");
		dstate_addcmd("shutdown.default");
		dstate_addcmd("shutdown.return");
		dstate_addcmd("shutdown.stayoff");
		dstate_addcmd("shutdown.reboot");
		dstate_addcmd("shutdown.reboot.graceful");

		for (g = 0; g < outlet_group_count; g++) {
			for (i = 0; outlet_suffixes[i] != NULL; i++) {
				snprintf(cmd, sizeof(cmd), "outlet.group.%zu.%s", g, outlet_suffixes[i]);
				dstate_addcmd(cmd);
			}
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
	addvar(VAR_VALUE, "cmdsrc",
		"Microlink command source: rj45, usb, localuser, smartslot1, internalnetwork1 (default: rj45)");
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
