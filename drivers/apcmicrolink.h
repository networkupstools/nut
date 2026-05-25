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
#define MLINK_RECORD_LEN			(MLINK_MAX_FRAME)
#define MLINK_DESCRIPTOR_MAX_BLOB	(256 * MLINK_MAX_PAYLOAD)
#define MLINK_DESCRIPTOR_MAX_USAGES	1024

#define MLINK_OBJ_PROTOCOL			0x00

#define MLINK_DESC_SLAVE_PASSWORD  "2:4.8.5"
#define MLINK_DESC_MASTER_PASSWORD "2:4.8.6"
#define MLINK_DESC_AUTH_STATUS     "2:4.8.9"
#define MLINK_DESC_SERIALNUMBER    "2:4.9.40"

#define MLINK_PAGE0_FLAG_AUTH_REQUIRED		(1U << 0)
#define MLINK_PAGE0_FLAG_IMPLICIT_STUFFING	(1U << 1)
#define MLINK_PAGE0_FLAG_DESCRIPTOR_PRESENT	(1U << 3)
#define MLINK_PAGE0_FLAG_FIRMWARE_UPDATE_NEEDED	(1U << 4)

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

typedef struct microlink_page0_state_s {
	size_t width;
	unsigned int count;
	unsigned char version;
	unsigned char series_data_version;
	unsigned char descriptor_version;
	unsigned char flags;
	uint16_t series_id;
	uint16_t descriptor_ptr;
} microlink_page0_state_t;

#endif /* APCMICROLINK_H */
