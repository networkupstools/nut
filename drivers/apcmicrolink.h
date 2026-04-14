/* apcmicrolink.h - APC Microlink protocol driver definitions
 *
 * Copyright (C) 2026 Lukas Schmid <lukas.schmid@netcube.li>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef APCMICROLINK_H
#define APCMICROLINK_H

#include <stddef.h>
#include <stdint.h>

#define MLINK_MAX_FRAME				256
#define MLINK_MAX_PAYLOAD			(MLINK_MAX_FRAME - 3)
#define MLINK_FIXED_PAYLOAD_LEN		32
#define MLINK_RECORD_LEN			(1 + MLINK_FIXED_PAYLOAD_LEN + 2)
#define MLINK_DESCRIPTOR_MAX_BLOB	(256 * MLINK_FIXED_PAYLOAD_LEN)
#define MLINK_DESCRIPTOR_MAX_USAGES	1024

#define MLINK_OBJ_PROTOCOL			0x00

#define MLINK_DESC_SLAVE_PASSWORD  "2:4.8.5"
#define MLINK_DESC_MASTER_PASSWORD "2:4.8.6"
#define MLINK_DESC_AUTH_STATUS     "2:4.8.9"
#define MLINK_DESC_SERIALNUMBER    "2:4.9.40"

typedef struct microlink_object_s {
	int seen;
	size_t len;
	unsigned char data[MLINK_MAX_PAYLOAD];
} microlink_object_t;

typedef struct microlink_descriptor_usage_s {
	int valid;
	int skipped;
	char path[64];
	size_t data_offset;
	size_t size;
} microlink_descriptor_usage_t;

typedef union microlink_page0_flags_u {
	unsigned char raw;
	struct {
		unsigned char auth_required : 1;
		unsigned char implicit_stuffing : 1;
		unsigned char reserved_2 : 1;
		unsigned char descriptor_present : 1;
		unsigned char firmware_update_needed : 1;
		unsigned char reserved_5_7 : 3;
	} bits;
} microlink_page0_flags_t;

typedef struct microlink_page0_state_s {
	size_t width;
	unsigned int count;
	unsigned char version;
	unsigned char series_data_version;
	unsigned char descriptor_version;
	microlink_page0_flags_t flags;
	uint16_t series_id;
	uint16_t descriptor_ptr;
} microlink_page0_state_t;

#endif /* APCMICROLINK_H */
