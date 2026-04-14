/* apcmicrolink-maps.h - APC Microlink descriptor maps
 *
 * Copyright (C) 2026 Lukas Schmid <lukas.schmid@netcube.li>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef APCMICROLINK_MAPS_H
#define APCMICROLINK_MAPS_H

#include <stddef.h>
#include <stdint.h>

typedef enum microlink_map_mode_e {
	MLINK_MAP_VALUE = 0,
	MLINK_MAP_BITFIELD = 1
} microlink_map_mode_t;

typedef enum microlink_desc_value_type_e {
	MLINK_DESC_NONE,
	MLINK_DESC_STRING,
	MLINK_DESC_FIXED_POINT,
	MLINK_DESC_DATE,
	MLINK_DESC_TIME,
	MLINK_DESC_BITFIELD_MAP,
	MLINK_DESC_ENUM_MAP
} microlink_desc_value_type_t;

typedef enum microlink_desc_access_e {
	MLINK_DESC_RO = 0,
	MLINK_DESC_RW = 1 << 0
} microlink_desc_access_t;

typedef enum microlink_desc_write_type_e {
	MLINK_DESC_WRITE_NONE,
	MLINK_DESC_WRITE_TYPED,
	MLINK_DESC_WRITE_BITMASK
} microlink_desc_write_type_t;

typedef enum microlink_desc_numeric_sign_e {
	MLINK_DESC_UNSIGNED = 0,
	MLINK_DESC_SIGNED = 1
} microlink_desc_numeric_sign_t;

typedef enum microlink_desc_name_index_e {
	MLINK_NAME_INDEX_NONE = 0,
	MLINK_NAME_INDEX_ZERO_BASED,
	MLINK_NAME_INDEX_ONE_BASED
} microlink_desc_name_index_t;

typedef struct microlink_bitfield_map_s {
	int value;
	const char *text;
} microlink_value_map_t;

typedef struct microlink_desc_value_map_s {
	const char *path;
	const char *upsd_name;
	microlink_desc_value_type_t type;
	microlink_desc_numeric_sign_t sign;
	unsigned int bin_point;
	unsigned int access;
	microlink_desc_name_index_t name_index;
	const microlink_value_map_t *map;
} microlink_desc_value_map_t;

typedef struct microlink_desc_command_map_s {
	const char *path;
	const char *cmd_name;
	microlink_desc_write_type_t write_type;
	uint64_t bit_mask;
	const char *value;
	const char *presence_path;
} microlink_desc_command_map_t;

typedef struct microlink_desc_publish_map_s {
	const char *path;
	const microlink_value_map_t *status_map;
	const microlink_value_map_t *alarm_map;
} microlink_desc_publish_map_t;

extern const microlink_desc_publish_map_t microlink_desc_publish_map[];
extern const microlink_desc_value_map_t microlink_desc_value_map[];
extern const microlink_desc_command_map_t microlink_desc_command_map[];
extern const size_t microlink_desc_value_map_count;
extern const size_t microlink_desc_command_map_count;

#endif /* APCMICROLINK_MAPS_H */
