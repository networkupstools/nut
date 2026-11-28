/* hashx-usb-protocol.h - bounded HashX USB reply framing
 *
 * Copyright (C) 2026 Network UPS Tools contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef HASHX_USB_PROTOCOL_H
#define HASHX_USB_PROTOCOL_H

#include <stddef.h>

#define HASHX_USB_PACKET_SIZE 64
#define HASHX_USB_FRAME_SIZE 48

struct hashx_usb_reply {
	char data[HASHX_USB_FRAME_SIZE + 1];
	size_t length;
};

void hashx_usb_reply_reset(struct hashx_usb_reply *reply);

/* Return -1 and reset on malformed input, 0 for an incomplete reply or
 * an unrelated report, and 1 for a complete recognized status reply.
 * Unknown reports and zero-length chunks leave the pending reply intact.
 * The caller must bound the receive loop and reset between requests.
 * A complete reply retains CR and is followed by an additional NUL.
 */
int hashx_usb_reply_append(struct hashx_usb_reply *reply,
	const unsigned char *packet, size_t length);

/* Validate the observed 48-byte status schema, including its final CR.
 * This does not validate device identity, power events or command support.
 */
int hashx_usb_status_valid(const char *data, size_t length);

/* Packet callbacks must respect the supplied buffer size. Reads return
 * 0 for an empty queue/timeout, a negative value on error, or a byte count.
 * Writes return the byte count, or a negative value on error.
 */
typedef int (*hashx_usb_read_packet_fn)(void *context,
	unsigned char *packet, size_t size, unsigned int timeout_ms);
typedef int (*hashx_usb_write_packet_fn)(void *context,
	const unsigned char *packet, size_t size, unsigned int timeout_ms);

/* Drain at most eight queued packets before sending only the B status
 * query, then read at most eight reply packets. Return 1 for a complete
 * recognized reply, or -1 with a reset reply on any failure.
 * The drain discards already queued replies. Without response sequence
 * identifiers it cannot identify a delayed reply arriving after the drain.
 * This helper performs no hardware access itself.
 */
int hashx_usb_status_query(void *context,
	hashx_usb_read_packet_fn read_packet,
	hashx_usb_write_packet_fn write_packet,
	struct hashx_usb_reply *reply);

#endif /* HASHX_USB_PROTOCOL_H */
