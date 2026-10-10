/* hashx-usb-protocol.c - bounded HashX USB reply framing
 *
 * Copyright (C) 2026 Network UPS Tools contributors
 *
 * A passive nJoy Keen 2000 capture provided report 0x28 followed by a
 * content-length byte and fragments of a CR-terminated HashX status reply.
 * Only declared contents are used; transport padding is ignored.
 * This file has no USB, serial or hardware access dependencies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "hashx-usb-protocol.h"

#include <string.h>

static int hashx_usb_byte_valid(size_t offset, unsigned char value)
{
	static const char format[] = "#Iddd.dOddd.dLdddBdddVdd.dFdd.dHdd.dRdddS";

	if (offset < sizeof(format) - 1) {
		if (format[offset] == 'd')
			return value >= '0' && value <= '9';
		return value == (unsigned char)format[offset];
	}

	/* HashX status bytes carry a common high bit; unlike the preceding
	 * fields, they are binary and must not be processed with strlen(). */
	if (offset < HASHX_USB_FRAME_SIZE - 1)
		return (value & 0x80U) != 0U;

	return offset == HASHX_USB_FRAME_SIZE - 1 && value == '\r';
}

static int hashx_usb_charge_valid(const char *data, size_t length)
{
	unsigned int charge;

	if (length < 21)
		return 1;
	charge = (unsigned int)(data[18] - '0') * 100U
		+ (unsigned int)(data[19] - '0') * 10U
		+ (unsigned int)(data[20] - '0');
	return charge <= 100U;
}

int hashx_usb_status_valid(const char *data, size_t length)
{
	size_t offset;

	if (!data || length != HASHX_USB_FRAME_SIZE)
		return 0;
	for (offset = 0; offset < length; offset++) {
		if (!hashx_usb_byte_valid(offset, (unsigned char)data[offset]))
			return 0;
	}

	/* Load is intentionally not capped at 100: an overload is valid
	 * telemetry. A genuine L000 must also remain valid. */
	return hashx_usb_charge_valid(data, length);
}

void hashx_usb_reply_reset(struct hashx_usb_reply *reply)
{
	if (reply)
		memset(reply, 0, sizeof(*reply));
}

static int hashx_usb_reply_reject(struct hashx_usb_reply *reply)
{
	hashx_usb_reply_reset(reply);
	return -1;
}

int hashx_usb_reply_append(struct hashx_usb_reply *reply,
	const unsigned char *packet, size_t length)
{
	size_t count;
	size_t offset;

	if (!reply)
		return -1;
	if (!packet || length == 0 || length > HASHX_USB_PACKET_SIZE)
		return hashx_usb_reply_reject(reply);
	if (packet[0] != 0x28U)
		return 0;
	if (length < 2 || reply->length > HASHX_USB_FRAME_SIZE)
		return hashx_usb_reply_reject(reply);

	count = packet[1];
	if (count == 0)
		return 0;
	if (count > length - 2 || count > HASHX_USB_FRAME_SIZE - reply->length)
		return hashx_usb_reply_reject(reply);

	/* Validate each fragment at its final offset. This rejects a new
	 * '#' while a reply is partial, early CR, embedded NUL and bad fields
	 * immediately, without joining bytes from different reply cycles. */
	for (offset = 0; offset < count; offset++) {
		if (!hashx_usb_byte_valid(reply->length + offset, packet[2 + offset]))
			return hashx_usb_reply_reject(reply);
	}
	memcpy(reply->data + reply->length, packet + 2, count);
	reply->length += count;
	reply->data[reply->length] = '\0';
	if (!hashx_usb_charge_valid(reply->data, reply->length))
		return hashx_usb_reply_reject(reply);
	if (reply->length == HASHX_USB_FRAME_SIZE) {
		if (!hashx_usb_status_valid(reply->data, reply->length))
			return hashx_usb_reply_reject(reply);
		return 1;
	}
	return 0;
}

int hashx_usb_status_query(void *context,
	hashx_usb_read_packet_fn read_packet,
	hashx_usb_write_packet_fn write_packet,
	struct hashx_usb_reply *reply)
{
	unsigned char packet[HASHX_USB_PACKET_SIZE];
	unsigned int attempt;
	int count;
	int result;
	int drained = 0;

	hashx_usb_reply_reset(reply);
	if (!reply || !read_packet || !write_packet)
		return -1;

	/* Require an observed empty queue before transmitting. A continuous
	 * stream must exhaust the bounded drain without sending a new query. */
	for (attempt = 0; attempt < 8; attempt++) {
		count = read_packet(context, packet, sizeof(packet), 1);
		if (count < 0 || count > HASHX_USB_PACKET_SIZE)
			return hashx_usb_reply_reject(reply);
		if (count == 0) {
			drained = 1;
			break;
		}
	}
	if (!drained)
		return hashx_usb_reply_reject(reply);

	memset(packet, 0, sizeof(packet));
	packet[0] = 0x29U;
	packet[1] = 2;
	packet[2] = 'B';
	packet[3] = '\r';
	count = write_packet(context, packet, sizeof(packet), 1000);
	if (count != HASHX_USB_PACKET_SIZE)
		return hashx_usb_reply_reject(reply);

	for (attempt = 0; attempt < 8; attempt++) {
		count = read_packet(context, packet, sizeof(packet), 250);
		if (count <= 0 || count > HASHX_USB_PACKET_SIZE)
			return hashx_usb_reply_reject(reply);
		result = hashx_usb_reply_append(reply, packet, (size_t)count);
		if (result == 1)
			return 1;
		if (result < 0)
			return hashx_usb_reply_reject(reply);
	}
	return hashx_usb_reply_reject(reply);
}
