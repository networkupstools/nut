/* apcmicrolink-usb.h - USB HID tunnel transport for the APC Microlink protocol driver
 *
 * Copyright (C)
 *   2026 Lukas Schmid <lukas.schmid@netcube.li>
 *   2026 Nicolai 'nmbro' Brogaard <nicolai.brogaard+nut@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef APCMICROLINK_USB_H
#define APCMICROLINK_USB_H

#include <stddef.h>

/* Register the usual vendorid/productid/vendor/product/serial/bus/device
 * (and busport, if supported) addvar() options used to disambiguate which
 * USB device to open. */
void microlink_usb_addvars(void);

/* Open the USB Microlink UPS matching the driver's vendorid/productid/
 * vendor/product/serial/bus/device filters (or the first supported device
 * if none are given). Fatal on failure, like the sibling USB drivers'
 * open helpers - never returns 0/negative. */
int microlink_usb_open(void);

/* Close the USB device and release matcher/device string resources. */
void microlink_usb_close(void);

/* Discard any not-yet-consumed bytes from the last received HID report,
 * and drain whatever the interrupt endpoint has queued up. Mirrors
 * ser_flush_io()'s "forget stale input" semantics for the USB transport. */
void microlink_usb_flush_io(void);

/* Send len raw protocol bytes (a full [id,offset,len,...data,cb0,cb1]
 * write frame, or a single control byte) as one HID Output Report.
 * Returns 1 on success, 0 on failure - mirrors ser_send_buf()'s use here. */
int microlink_usb_send_bytes(const unsigned char *buf, size_t len);

/* Fetch the next unconsumed byte of the Microlink byte stream, refilling
 * from a fresh HID Input Report via the interrupt endpoint as needed.
 * Mirrors ser_get_char()'s contract: 1 with *ch filled on success,
 * 0 on timeout/no data yet, negative on hard error. */
int microlink_usb_get_char(unsigned char *ch, long d_usec);

/* Standard-HID-Power-Device-usage fallback: this device also
 * pushes autonomous Input reports for PresentStatus/RemainingCapacity/
 * RunTimeToEmpty on the same interrupt pipe as the Microlink tunnel,
 * independent of whether the tunnel itself is responding.
 * microlink_usb_get_char() opportunistically decodes and stashes these
 * whenever they arrive. This accessor returns 1 and fills the out-params
 * if this device was found (at open time) to expose all the usages
 * needed, and the last decoded snapshot is no older than max_age_sec;
 * returns 0 otherwise (unsupported device, or no fresh-enough snapshot
 * yet). battery_charge/battery_runtime are set to -1 if that particular
 * field wasn't found on this device, even when the call otherwise
 * succeeds for the status booleans. */
int microlink_usb_get_hid_fallback(int max_age_sec,
	int *ac_present, int *discharging, int *below_rcl,
	long *battery_charge, long *battery_runtime);

/* 1 if this device was found (at open time) to expose the usages the
 * fallback above needs, regardless of whether a fresh snapshot is
 * currently available. For "is the capability there at all" checks that
 * shouldn't depend on timing, e.g. deciding whether to publish a status
 * field advertising the fallback's current active/inactive state. */
int microlink_usb_hid_fallback_supported(void);

/* Reset the USB device and reopen the handle using the existing matcher.
 * Returns 1 on success (udev valid, HID descriptor re-parsed), 0 on failure
 * (udev left NULL). If udev is already NULL, skips the reset and just tries
 * to reopen. The regex_matcher is never freed or modified. Call this when
 * microlink_usb_device_gone() reports a genuine disconnect - a live-but-
 * unresponsive device is a different problem this doesn't fix (confirmed
 * by testing: it recovers a real unplug/power-cycle, not a stalled tunnel). */
int microlink_usb_reset_and_reopen(void);

/* 1 if a read, write, or async transfer has reported the USB device
 * genuinely gone (unplugged, power-cycled) since the last successful
 * microlink_usb_open(); 0 otherwise. This is the signal to act on, not a
 * retry count - a live-but-stalled device never benefits from a reset. */
int microlink_usb_device_gone(void);

#endif /* APCMICROLINK_USB_H */
