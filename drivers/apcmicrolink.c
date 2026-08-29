/* apcmicrolink.c - APC Microlink protocol driver
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
#ifdef WITH_USB
#include "apcmicrolink-usb.h"
#endif /* WITH_USB */

#define DRIVER_NAME	"APC Microlink protocol driver"
#define DRIVER_VERSION	"0.03"

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
/* 0xFE/0xFD are the same ACK/NAK bytes APC's own PowerChute Serial
 * Shutdown client uses (matching MLINK_NEXT_BYTE/MLINK_INIT_BYTE above
 * exactly), and 0xF7 is its STOP byte - sent, unconditionally,
 * immediately before every NAK it issues while recovering from a comms
 * timeout. We had never sent this byte at all; see MLINK_STOP_THEN_INIT()
 * below for why that appears to matter. Also cross-confirms an
 * independent third-party finding already in this codebase's history: the
 * two-byte APC_CMD_INIT = [0xF7, 0xFD] sequence documented for the older
 * Smart-UPS C1000/SMC1000i in SCL500RM1UC-protocol-notes.md isn't a
 * different device's alternate init byte - it's this exact STOP-then-NAK
 * pair. */
#define MLINK_STOP_BYTE			0xF7
#define MLINK_HANDSHAKE_RETRIES	3
#define MLINK_READ_TIMEOUT_USEC	100000
/* Confirmed against real SCL500RMI1UC hardware over USB: unlike serial,
 * where a reply to the first poll after init/next-byte typically arrives
 * within MLINK_READ_TIMEOUT_USEC, the USB HID interrupt pipe on this
 * device carries other unrelated Input reports interleaved with our own
 * (see apcmicrolink-usb.c), and the first genuine Report 0x89 reply after
 * a fresh session init has been observed taking upwards of a second to
 * appear - so a 100ms budget starves the handshake before it ever sees
 * real data. Give the USB path a longer per-round budget.
 *
 * A prior attempt raised this to 20s on the theory that a cold session-init
 * reply, which can itself take 13-14s to arrive, just needed more room to
 * land. Live testing showed the opposite: the follow-up next-byte polls
 * that come right after a successful init can then fail three consecutive
 * 20s rounds with zero replies at all, not just a slow one - so whatever
 * gates a reply isn't simply "wait long enough per attempt", and the longer
 * timeout only turned a ~10s failed-startup cost into ~81s with no gain in
 * success rate. Left at 1s; do not raise this without new evidence that a
 * longer per-round budget improves the poll success rate, not just the
 * session-init rate. */
#define MLINK_USB_READ_TIMEOUT_USEC	1000000
/* Confirmed against real hardware: this device's USB HID interrupt pipe
 * also carries other, unrelated Input reports (see apcmicrolink-usb.c),
 * so an occasional poll round finds nothing at all within
 * MLINK_USB_READ_TIMEOUT_USEC even mid-session - give the USB path more
 * consecutive-timeout tolerance than serial's tight retry count before
 * treating it as a real communication failure. */
#define MLINK_USB_HANDSHAKE_RETRIES	10

/* EXPERIMENTAL (stability variant A, 2026-08-24): MLINK_USB_HANDSHAKE_RETRIES
 * (10 poll cycles, ~10-20s depending on poll_interval) was, until this
 * change, ALSO the threshold for tearing down an already-established,
 * partway-through-the-96-row-descriptor-fetch session and restarting it
 * from zero (see the reconnect check in upsdrv_updateinfo()). That reuse
 * conflated two very different situations: giving up on the very first
 * handshake (cheap, nothing lost) versus giving up mid-fetch (expensive
 * at the time - later fixes made reconnecting non-destructive, see
 * febb2e55c/9a8a78bf3). This constant is deliberately separate, much
 * larger than the handshake retry budget, and wall-clock-based (not a
 * poll-count) so it stays correct if MLINK_USB_READ_TIMEOUT_USEC or
 * poll_interval ever change.
 *
 * Value derived 2026-08-27 from a direct tshark/usbmon measurement of
 * real tunnel Input-report (0x89) arrival gaps over ~7 clean hours on
 * real hardware (825 replies). The gap distribution was sharply
 * trimodal, not a smooth spread: ~80% under 5s, a tight cluster at
 * 11.0-13.0s, then a completely empty band from 13s to 289s, then a
 * tight cluster at 288.6-290.6s recurring roughly every 5m10s. No
 * legitimate reply gap was ever observed between 13s and 289s. 30s sits
 * with >2x margin above the 13s cluster's ceiling while staying nowhere
 * near the 289s one, so it should never misfire on a normal pause while
 * still catching a real stall far sooner than 90s did.
 *
 * Caveat: the measured window predated the async-queue-flush fix
 * (f7f10fc70, same day) - what caused the ~289s cluster specifically is
 * not established (an earlier theory that it reflected a genuine
 * independent device heartbeat did not hold up to scrutiny). Only the
 * empirical gap-distribution finding is treated as reliable here, not
 * any particular explanation for it; re-derive from fresh post-fix data
 * if this stops matching observed reconnect behavior. */
#define MLINK_USB_MIDSESSION_IDLE_SEC	30

/* How stale a standard-HID-PDC fallback snapshot is allowed to be and still
 * be considered good enough to publish.
 *
 * The two streams share one interrupt endpoint and do not run in parallel:
 * a usbmon capture of an SCL500RMI1UC showed the PDC reports arriving every
 * 6.0s while the tunnel was idle, then stopping for 19.2s the moment the
 * driver started a session and the device answered tunnel traffic. A window
 * of 10s expired inside that gap, so the fallback went unpublishable exactly
 * when the tunnel was also producing nothing usable, and ups.status went
 * empty once per cycle. 30s clears the observed gap with margin while still
 * being short enough that a genuinely unplugged device does not leave stale
 * fallback data looking current. */
#define MLINK_HID_FALLBACK_MAX_AGE_SEC	30

/* How long the Microlink side may go without delivering real polled data
 * before the standard-HID-PDC fallback takes over, when one is available.
 * Completing the session handshake does not reset this - a device can answer
 * the handshake on every retry while its tunnel stays mute, which is exactly
 * the case this exists to catch. Kept above MLINK_HID_FALLBACK_MAX_AGE_SEC so
 * the fallback snapshot we switch to is always fresher than the Microlink
 * data we are abandoning. */
#define MLINK_DATA_STALE_SEC	45

/* Why the standard-HID-PDC fallback is (or is not) being published right now.
 * Only used to log handovers once per transition - see
 * microlink_log_fallback_reason(). */
#define MLINK_FB_REASON_NONE	0
#define MLINK_FB_REASON_STALE	1
#define MLINK_FB_REASON_EMPTY	2

/* How often to repeat the "tunnel is answering but reporting nothing" warning
 * while that state persists. The condition can toggle on every poll, so this
 * is deliberately coarse - it is a "your UPS needs attention" notice, not a
 * per-poll trace. */
#define MLINK_DEGENERATE_WARN_INTERVAL_SEC	3600

/* How long the Microlink source has to look plausible again, continuously,
 * before the driver hands back to it. Without this the two sources swap on
 * alternate polls whenever the device dribbles out an occasional status flag
 * between all-zero ones, and ups.status oscillates every couple of seconds -
 * measured at 56 handovers in 3 minutes on a degraded SCL500RMI1UC. A stable
 * status matters more to upsmon than using the freshest possible source. */
#define MLINK_FALLBACK_DWELL_SEC	30
/* How long to wait between individual Microlink session-start probes when
 * retrying from upsdrv_updateinfo() after a fallback start. Each probe
 * sends one INIT_BYTE and listens for up to MLINK_USB_READ_TIMEOUT_USEC, so
 * combined with that this constant gives a per-cycle time of roughly
 * MLINK_USB_READ_TIMEOUT_USEC + this many seconds (~2 s at the default 1 s
 * USB read timeout). Tightened from an earlier 5 s: APC's own client
 * appears to re-probe (write) far more often than that rather than relying
 * on any special listening trick at the transport layer - its apparent
 * "never stalls" behavior seems to come from writing often, not from a
 * smarter read path. This constant is the driver's equivalent knob: probe
 * far more often than the original 5 s so a device that's only briefly
 * reachable gets more chances to be caught, without spinning (each cycle
 * still spends most of its time in a real blocking wait, not
 * busy-polling). */
#define MLINK_SESSION_RETRY_INTERVAL_SEC	1

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
#ifdef WITH_USB
static int is_usb = 0;
#endif /* WITH_USB */
static unsigned char rxbuf[MLINK_MAX_FRAME * 2];
static size_t rxbuf_len = 0;
static unsigned int parsed_frames = 0;
static unsigned int consecutive_timeouts = 0;
static time_t last_poll_success = 0;
/* Distinct from last_poll_success, which microlink_start_session() also
 * refreshes on a bare handshake: this only ever advances when real polled
 * data arrives. See microlink_data_stale(). */
static time_t last_microlink_data = 0;
/* When the "tunnel answers but reports nothing" warning was last emitted;
 * 0 while the condition is not in effect. */
static time_t degenerate_warned_at = 0;
/* Fallback hysteresis: whether the standard-HID source currently owns the
 * published status, and since when the Microlink source has been looking
 * plausible again (0 = not currently recovering). */
static int fallback_engaged = 0;
static time_t fallback_recover_since = 0;
static int poll_primed = 0;
static int authentication_sent = 0;
static microlink_page0_state_t page0;
static int warned_implicit_stuffing = 0;
static int descriptor_ready = 0;
static int outlet_commands_registered = 0;
static time_t microlink_session_next_retry = 0;
static time_t microlink_fallback_since = 0;
static unsigned int microlink_fallback_retries = 0;
/* main.c's read_upsconf() applies the user's "pollinterval" ups.conf setting
 * (or its own 2s default) to the global poll_interval before calling
 * upsdrv_initinfo() - captured here so a successfully-connected session can
 * restore that value instead of hardcoding one, after the "not ready" paths
 * below have temporarily lowered poll_interval to pace their own retries. */
static time_t microlink_configured_poll_interval = 2;
static size_t descriptor_usage_count = 0;
static size_t descriptor_blob_len = 0;
static microlink_descriptor_usage_t descriptor_usages[MLINK_DESCRIPTOR_MAX_USAGES];
static unsigned char descriptor_blob[MLINK_DESCRIPTOR_MAX_BLOB];
static int show_internals = -1;
static int show_unmapped = -1;
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
/* Whether to publish ups.status/battery.charge/battery.runtime from the
 * standard-HID-PDC fallback source when the Microlink tunnel itself has
 * nothing to offer, instead of going straight to Data stale. USB-only; defaults on -
 * for most users, a status derived from a slightly different source is
 * more useful than a gap, especially for "are we on battery" specifically.
 * Some users doing data gathering may want the opposite (an honest gap in
 * the record rather than a value with different provenance/precision
 * mixed into the same series) - "hid_fallback=no" opts back out to the
 * original behavior. */
static int hid_fallback_enabled = 1;

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
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SWITCH_DEFAULT
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE_BREAK
#pragma GCC diagnostic ignored "-Wunreachable-code-break"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-default"
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

	if (testvar("hid_fallback")) {
		int parsed = 0;

		value = getval("hid_fallback");
		if (value == NULL) {
			hid_fallback_enabled = 1;
		} else if (microlink_parse_bool(value, &parsed)) {
			hid_fallback_enabled = parsed;
		} else {
			fatalx(EXIT_FAILURE, "apcmicrolink: invalid hid_fallback value '%s'",
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

static unsigned int microlink_handshake_retries(void)
{
#ifdef WITH_USB
	if (is_usb) {
		return MLINK_USB_HANDSHAKE_RETRIES;
	}
#endif /* WITH_USB */
	return MLINK_HANDSHAKE_RETRIES;
}

/* Whether an already-established session has gone quiet long enough to be
 * torn down and restarted. USB uses a much more patient, wall-clock-based
 * budget than the initial handshake (see MLINK_USB_MIDSESSION_IDLE_SEC);
 * serial keeps the original poll-count behavior, since the too-short-tolerance
 * problem was only observed and characterized on USB. */
/* 1 if the Microlink side has not delivered real polled data recently.
 * Deliberately ignores the session handshake: a device that answers the
 * handshake but never sends another frame used to look healthy here, so the
 * driver kept republishing its last values - all zeroes, with an empty
 * ups.status - for days, while usable standard-HID-PDC reports were arriving
 * on the same endpoint the whole time. */
static int microlink_data_stale(time_t now)
{
	if (last_microlink_data == 0) {
		return 1;
	}
	return difftime(now, last_microlink_data) >= MLINK_DATA_STALE_SEC;
}

/* 1 if this device could publish standard-HID-PDC fallback data, whether or
 * not any has been decoded yet. microlink_publish_hid_fallback() answers the
 * narrower "is there a fresh snapshot right now"; this answers "is it worth
 * staying alive and waiting for one". */
static int microlink_hid_fallback_possible(void)
{
#ifdef WITH_USB
	return (is_usb && hid_fallback_enabled && microlink_usb_hid_fallback_supported());
#else
	return 0;
#endif /* WITH_USB */
}

static int microlink_midsession_timed_out(time_t now)
{
#ifdef WITH_USB
	if (is_usb) {
		if (last_poll_success == 0) {
			return consecutive_timeouts >= microlink_handshake_retries();
		}
		return difftime(now, last_poll_success) >= MLINK_USB_MIDSESSION_IDLE_SEC;
	}
#endif /* WITH_USB */
	return consecutive_timeouts >= microlink_handshake_retries();
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

/* Group 0 maps to the 2:4.3E descriptor page: the "Unswitched Group" on
 * APC UPS devices -- outlets that follow the UPS output but have no
 * independent on/off control. Groups 1+ map to 2:4.3D[i] pages and are
 * the independently switchable outlet banks (with timers, etc.). */
static int microlink_outlet_group_is_switched(size_t group_idx)
{
	return group_idx > 0;
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
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_SWITCH_ENUM
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
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
	upsdebugx(1, "microlink: STABILITY descriptor_ready usages=%zu blob_len=%zu",
		descriptor_usage_count, descriptor_blob_len);
	return 1;
}

static void microlink_cache_object(const unsigned char *frame, size_t len)
{
	unsigned int id;
	microlink_object_t *obj;
	size_t datalen;

	if (len < 3) {
		return;
	}

	id = frame[0];
	datalen = len - 3;

	/* Page 0 announces the width every later frame is parsed with, so a
	 * corrupt copy is unrecoverable: the bogus width becomes the length
	 * the parser demands, no frame ever checksum-validates again, and the
	 * session dies without the device having done anything wrong. A real
	 * page 0 arrives in a frame of exactly the width it announces, so
	 * reject any copy that contradicts itself instead of caching it.
	 *
	 * Seen live on a Smart-UPS X 1500 (FW 03.8): after its last populated
	 * page the device sent STOP-filled pages and restarted its page index,
	 * and the resulting checksum-valid "page 0" announced width 247 inside
	 * a 16-byte frame. */
	if (id == MLINK_OBJ_PROTOCOL && datalen >= 3 && (size_t)frame[2] != datalen) {
		upsdebugx(1, "microlink: ignoring implausible page0 - announces width "
			"%u but arrived in a %" PRIuSIZE "-byte frame; keeping the "
			"previous page0 (width %" PRIuSIZE ")",
			(unsigned int)frame[2], datalen, page0.width);
		return;
	}

	obj = microlink_get_object_mut(id);
	obj->seen = 1;
	obj->len = datalen;
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

		/* Nothing in this parser implements byte stuffing; it is only
		 * published as microlink.flag.implicit_stuffing. No device seen
		 * so far sets the bit, so rather than guess at an unexercised
		 * unstuffing routine, say plainly that frames will be read as if
		 * it were clear - which is what any resulting checksum failures
		 * would otherwise be blamed on. */
		if ((page0.flags & MLINK_PAGE0_FLAG_IMPLICIT_STUFFING) != 0U
		&&  !warned_implicit_stuffing
		) {
			warned_implicit_stuffing = 1;
			upslogx(LOG_WARNING, "microlink: this device requests implicit byte "
				"stuffing (page0 flags 0x%02X), which this driver does not "
				"implement - frames are parsed as if stuffing were disabled, "
				"so expect checksum failures or missing data. Please report "
				"this at https://github.com/networkupstools/nut/issues/ with "
				"a debug log, as no device known to this driver sets that bit.",
				(unsigned int)page0.flags);
		}
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

#ifdef WITH_USB
	if (is_usb) {
		return microlink_usb_send_bytes(frame, framelen);
	}
#endif /* WITH_USB */

	if (ser_send_buf(upsfd, frame, framelen) != (ssize_t)framelen) {
		return 0;
	}

	return 1;
}

static int microlink_send_simple(unsigned char byte)
{
	microlink_trace_frame(2, "TX ctrl", &byte, 1);

#ifdef WITH_USB
	if (is_usb) {
		return microlink_usb_send_bytes(&byte, 1);
	}
#endif /* WITH_USB */

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
		size_t drop_len = rxbuf_len - (MLINK_RECORD_LEN - 1);

		upsdebugx(1, "microlink: dropping %u bytes while resynchronizing",
			(unsigned int)drop_len);
		microlink_trace_frame(1, "dropped (resync)", rxbuf, drop_len);
		memmove(rxbuf, rxbuf + drop_len, MLINK_RECORD_LEN - 1);
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

	upsdebugx(3, "microlink: auth seed s0=%02X (protocol[4]) s1=%02X (protocol[3])",
		s0, s1);
	microlink_trace_frame(3, "auth protocol[0:8]", protocol->data, 8);
	microlink_trace_frame(3, "auth serial_usage bytes", descriptor_blob + serial_usage->data_offset,
		serial_usage->size);
	microlink_trace_frame(3, "auth master_password[0:2] (only first 2 used)", master_password, 2);

	microlink_auth_update(&s0, &s1, protocol->data, 8);
	upsdebugx(3, "microlink: auth after protocol header: s0=%02X s1=%02X", s0, s1);
	microlink_auth_update(&s0, &s1, descriptor_blob + serial_usage->data_offset, serial_usage->size);
	upsdebugx(3, "microlink: auth after serial number: s0=%02X s1=%02X", s0, s1);
	microlink_auth_update(&s0, &s1, master_password, 2);
	upsdebugx(3, "microlink: auth after master_password: s0=%02X s1=%02X (-> SPC[2:4])", s0, s1);

	/* SPC[0:2]: our own challenge. APC's own PowerChute client draws this
	 * from a real random source; a fixed 0x00 0x00 here made the exchange
	 * trivially predictable, and a failed auth is suspected of locking out
	 * comms until a device reset. Randomization only - the reply itself is
	 * not verified. */
	payload[0] = (unsigned char)(rand() % 256);
	payload[1] = (unsigned char)(rand() % 256);
	payload[2] = s0;
	payload[3] = s1;

	upsdebugx(1, "microlink: STABILITY auth_sent %02X %02X",
		payload[2], payload[3]);
	microlink_trace_frame(1, "auth SLAVE_PASSWORD payload (SPC[0:4])", payload, sizeof(payload));

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
	useconds_t read_timeout_usec = MLINK_READ_TIMEOUT_USEC;

#ifdef WITH_USB
	if (is_usb) {
		read_timeout_usec = MLINK_USB_READ_TIMEOUT_USEC;
	}
#endif /* WITH_USB */

	state_get_timestamp(&start);

	for (;;) {
		unsigned char ch;
		ssize_t ret;

		if (microlink_try_extract_frame(frame, &framelen)) {
			return microlink_process_frame(frame, framelen);
		}

#ifdef WITH_USB
		if (is_usb) {
			ret = microlink_usb_get_char(&ch, (long)read_timeout_usec);
		} else
#endif /* WITH_USB */
		{
			ret = ser_get_char(upsfd, &ch, 0, read_timeout_usec);
		}

		if (ret < 0) {
			return 0;
		}

		if (ret == 0) {
			if (microlink_timeout_expired(&start, 0, read_timeout_usec)) {
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

static int microlink_poll_once(time_t now)
{
	if (!poll_primed) {
		if (!microlink_prime_poll()) {
			return 0;
		}
	}

	if (microlink_receive_once()) {
		consecutive_timeouts = 0;
		last_poll_success = now;
		last_microlink_data = now;
		poll_primed = 0;
		return 1;
	}

	poll_primed = 0;
	consecutive_timeouts++;
	return 0;
}

static int microlink_start_session_impl(unsigned int max_attempts)
{
	unsigned int attempt;

	rxbuf_len = 0;
	poll_primed = 0;
	authentication_sent = 0;

	/* NOT resetting page0/descriptor_ready/descriptor_usage_count/
	 * descriptor_blob_len: this runs on every reconnect, not just the
	 * first session. objects[]->seen (which gates whether frame-length
	 * parsing trusts page0.width) only resets at process start, so
	 * zeroing page0.width here left every reconnect after the first
	 * computing frame length as 0+3=3 - too short to ever checksum-valid.
	 * The device was replying fine the whole time; we just stopped being
	 * able to parse it. page0 is static per device/firmware and gets
	 * refreshed on receipt anyway, so keeping stale values costs nothing. */

	/* NOT calling microlink_usb_flush_io() for USB (unlike ser_flush_io()
	 * below): it wipes the async queue, and this runs every retry in a
	 * "not ready yet" loop that can span minutes. Isolated test: this
	 * discarded ~50% of otherwise-valid replies. The only real use case
	 * (clearing stale data after a hard reset) is already handled by
	 * microlink_usb_async_stop(), making this call redundant there and
	 * harmful everywhere else. */
#ifdef WITH_USB
	if (!is_usb)
#endif /* WITH_USB */
	{
		ser_flush_io(upsfd);
	}

	for (attempt = 0; attempt < max_attempts; attempt++) {
		/* PowerChute's own comms-lost recovery (MicroLinkTranslator.
		 * sendStop()/sendNak()) always sends STOP immediately before NAK,
		 * never NAK alone - see MLINK_STOP_BYTE's comment. A STOP write
		 * failure is treated the same as an INIT_BYTE write failure
		 * (hard I/O error, not just "no reply yet"), matching how
		 * MLINK_INIT_BYTE's own failure is handled just below. */
		if (!microlink_send_simple(MLINK_STOP_BYTE)) {
			return 0;
		}

		if (!microlink_send_simple(MLINK_INIT_BYTE)) {
			return 0;
		}

		if (microlink_receive_once()) {
			consecutive_timeouts = 0;
			last_poll_success = microlink_now();
			session_ready = 1;
			upsdebugx(1, "microlink: STABILITY session_established attempt=%u",
				attempt + 1);
			return microlink_prime_poll();
		}
	}

	return 0;
}

static int microlink_start_session(void)
{
	return microlink_start_session_impl(microlink_handshake_retries());
}

static int microlink_reconnect_session(void)
{
	upsdebugx(1, "microlink: STABILITY reconnecting after %u consecutive timeouts"
		" descriptor_usage_count=%zu descriptor_ready=%d auth_sent=%d",
		consecutive_timeouts, descriptor_usage_count, descriptor_ready,
		authentication_sent);
	session_ready = 0;
	return microlink_start_session();
}

static void microlink_publish_load(void)
{
	const char *realpower_str = dstate_getinfo("ups.realpower");
	const char *nominal_str = dstate_getinfo("ups.realpower.nominal");
	double realpower, nominal;
	char *end;

	/* The Microlink descriptor blob has no direct load-percentage field on
	 * this hardware, at either the UPS-total or per-outlet-group level -
	 * derive it client-side from the already-mapped real-power reading
	 * against its nominal rating, same as the value PowerChute/other
	 * tools show. */
	if (realpower_str == NULL || nominal_str == NULL) {
		return;
	}

	realpower = strtod(realpower_str, &end);
	if (end == realpower_str) {
		return;
	}

	nominal = strtod(nominal_str, &end);
	if (end == nominal_str || nominal <= 0.0) {
		return;
	}

	dstate_setinfo("ups.load", "%.0f", (realpower / nominal) * 100.0);
}

static void microlink_publish_identity(void)
{
	dstate_setinfo("ups.mfr", "APC");
	dstate_setinfo("device.mfr", "APC");
	dstate_setinfo("device.type", "ups");
	microlink_publish_descriptor_exports();
	microlink_publish_load();
}

/* Standard-HID-PDC fallback: publish ups.status/battery.charge/
 * battery.runtime from a source that keeps arriving on this device's
 * interrupt pipe independent of Microlink tunnel health, rather than
 * going straight to Data stale when the tunnel itself has nothing.
 * USB-only, and only when this device was found (at open time) to expose
 * the needed usages. Returns 1 if it published something, 0 if the
 * fallback isn't available/enabled/fresh enough right now (caller should
 * fall back to its own normal stale handling in that case). */
static int microlink_publish_hid_fallback(void)
{
#ifdef WITH_USB
	int ac_present = 0, discharging = 0, below_rcl = 0;
	long charge = -1, runtime = -1;

	if (!is_usb || !hid_fallback_enabled) {
		return 0;
	}

	if (!microlink_usb_get_hid_fallback(MLINK_HID_FALLBACK_MAX_AGE_SEC,
			&ac_present, &discharging, &below_rcl, &charge, &runtime)) {
		return 0;
	}

	/* Publish the measurements before committing the status, not after.
	 * dstate's status_commit() infers CHRG/DISCHRG from a change in
	 * battery.charge when the driver reports neither, and by this point
	 * microlink_publish_runtime() has already written the degenerate
	 * Microlink value. Committing first let dstate compare that value
	 * against the previous fallback one and synthesize a DISCHRG that
	 * contradicted the OL we were setting in the same breath. */
	if (charge >= 0) {
		dstate_setinfo("battery.charge", "%ld", charge);
	}
	if (runtime >= 0) {
		dstate_setinfo("battery.runtime", "%ld", runtime);
	}

	status_init();
	if (discharging || !ac_present) {
		status_set("OB");
	} else {
		status_set("OL");
	}
	if (below_rcl) {
		status_set("LB");
	}
	/* Report the charging state explicitly rather than leaving dstate to
	 * guess it from charge movement: the PDC stream tells us directly. */
	if (discharging) {
		status_set("DISCHRG");
	} else if (charge >= 0 && charge < 100) {
		status_set("CHRG");
	}
	status_commit();
	dstate_setinfo("experimental.hid_fallback.active", "%u", 1U);

	return 1;
#else
	return 0;
#endif /* WITH_USB */
}

/* Mark the fallback as NOT currently in use, when this device is known to
 * support it at all - so "experimental.hid_fallback.active" is always a
 * reliable answer to "is right now's data coming from the fallback path",
 * not a flag that only ever gets set and never cleared. */
static void microlink_publish_hid_fallback_inactive(void)
{
#ifdef WITH_USB
	if (!is_usb || !hid_fallback_enabled) {
		return;
	}

	if (!microlink_usb_hid_fallback_supported()) {
		return;
	}

	dstate_setinfo("experimental.hid_fallback.active", "%u", 0U);
#endif /* WITH_USB */
}

/* Log a handover to or away from the standard-HID-PDC fallback, with the
 * reason. Runs on every poll, so it stays silent unless the reason actually
 * changed - otherwise a device that sits in one state would repeat this line
 * every couple of seconds forever. */
static void microlink_log_fallback_reason(int reason, time_t now)
{
	static int last_reason = -1;

	if (reason == last_reason) {
		return;
	}
	last_reason = reason;

	switch (reason) {
	case MLINK_FB_REASON_STALE:
		upsdebugx(1, "microlink: publishing standard-HID fallback data - no "
			"Microlink data for %.0f s (threshold %d s)",
			(last_microlink_data == 0)
				? 0.0 : difftime(now, last_microlink_data),
			MLINK_DATA_STALE_SEC);
		break;

	case MLINK_FB_REASON_EMPTY:
		upsdebugx(1, "microlink: publishing standard-HID fallback data - the "
			"Microlink data is arriving but yielded no ups.status flags");
		break;

	case MLINK_FB_REASON_NONE:
	default:
		upsdebugx(1, "microlink: publishing Microlink data again, standard-HID "
			"fallback no longer needed");
		break;
	}
}

/* Hand over to the standard-HID-PDC snapshot when the Microlink data just
 * published did not yield a single ups.status flag. Returns 1 if it took over.
 *
 * Staleness alone does not catch this: a device can answer every poll on time
 * and still report an all-zero status word (seen live on an SCL500RMI1UC stuck
 * in "SystemInitialization" - full page walks, fresh frames, every measurement
 * zero). An empty ups.status is indistinguishable from a dead UPS to upsmon,
 * so where a real one is available from the HID PDC stream, publish that
 * instead of nothing. */
/* 1 if the Microlink data just published looks like a real reading rather
 * than the all-zero state a stalled device reports.
 *
 * Only ups.status is safe to judge this from. microlink_publish_status()
 * rewrites it on every poll, so it always reflects the Microlink source
 * alone. Other variables are not rewritten unconditionally, so a leftover
 * value the fallback itself published on the previous poll can still be
 * sitting there - testing battery.charge here made the fallback read its own
 * output back, conclude the Microlink side had recovered, and hand over to a
 * source that was still publishing nothing. */
static int microlink_data_plausible(void)
{
	const char *status = dstate_getinfo("ups.status");

	return (status != NULL
		&& (strstr(status, "OL") != NULL || strstr(status, "OB") != NULL));
}

/* Decide whether the standard-HID-PDC snapshot should own this poll's status,
 * and publish it if so. Returns 1 if it took over.
 *
 * Asymmetric on purpose: the fallback takes over as soon as the Microlink
 * source stops looking plausible, but only hands back once Microlink has
 * looked plausible continuously for MLINK_FALLBACK_DWELL_SEC. A UPS status
 * that flips every couple of seconds is worse than one that lags a real
 * recovery by half a minute.
 */
static int microlink_fallback_takes_over(time_t now)
{
	if (microlink_data_plausible()) {
		if (!fallback_engaged) {
			fallback_recover_since = 0;
			return 0;
		}

		if (fallback_recover_since == 0) {
			fallback_recover_since = now;
		}

		if (difftime(now, fallback_recover_since) < MLINK_FALLBACK_DWELL_SEC
		 && microlink_publish_hid_fallback()
		) {
			return 1;
		}

		fallback_engaged = 0;
		fallback_recover_since = 0;
		dstate_setinfo("microlink.diag.status_degenerate", "%u", 0U);
		microlink_log_fallback_reason(MLINK_FB_REASON_NONE, now);
		return 0;
	}

	fallback_recover_since = 0;

	if (!microlink_publish_hid_fallback()) {
		return 0;
	}

	/* Worth saying out loud, not just at debug level: the tunnel is healthy
	 * at the protocol layer - frames arrive, checksums pass, page walks
	 * complete - and the device is still reporting an all-zero state, so
	 * every derived measurement reads 0 and nothing sets a status flag.
	 * Observed live on an SCL500RMI1UC; a USB bus reset does not clear it
	 * (tested), and it survived driver restarts for days, so the user needs
	 * to know that only power-cycling the UPS itself is likely to help. */
	dstate_setinfo("microlink.diag.status_degenerate", "%u", 1U);

	if (degenerate_warned_at == 0
	 || difftime(now, degenerate_warned_at) >= MLINK_DEGENERATE_WARN_INTERVAL_SEC
	) {
		degenerate_warned_at = now;
		upslogx(LOG_WARNING, "apcmicrolink: the Microlink tunnel is responding "
			"normally but the device is reporting an all-zero state (no status "
			"flags, all measurements 0) - publishing standard-HID data instead. "
			"A USB bus reset does not clear this; the UPS itself likely needs a "
			"power cycle. Reported once per hour while it lasts.");
	}

	fallback_engaged = 1;
	microlink_log_fallback_reason(MLINK_FB_REASON_EMPTY, now);
	return 1;
}


/* If the Microlink tunnel has nothing fresh right now, fall back to the
 * standard-HID-PDC source instead of an unconditional Data stale. */
static void microlink_datastale_or_fallback(void)
{
	if (microlink_publish_hid_fallback()) {
		dstate_dataok();
	} else {
		dstate_datastale();
	}
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
	int use_usb = 0;
	time_t now = microlink_now();

	srand((unsigned int)now);

	microlink_read_config();

#ifdef WITH_USB
	/* Follow the same "port=auto means USB" convention as nutdrv_qx:
	 * any USB-matching option, or an explicit port=auto, selects USB;
	 * anything else in "port" is a serial device path. */
	if (
		!strcasecmp(device_path, "auto") ||
		getval("vendorid") || getval("productid") ||
		getval("vendor") || getval("product") ||
		getval("serial") || getval("bus") || getval("device")
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
		|| getval("busport")
#endif
	) {
		use_usb = 1;

		if (strcasecmp(device_path, "auto")) {
			upslogx(LOG_WARNING, "apcmicrolink: port='%s' would be ignored, "
				"since other options indicate USB mode", device_path);
		}
	}

	is_usb = use_usb;
	if (use_usb) {
		microlink_usb_open();
	}
#else
	if (!strcasecmp(device_path, "auto")) {
		fatalx(EXIT_FAILURE, "apcmicrolink: port=auto requires USB support, "
			"but this driver was not compiled with USB support");
	}
#endif /* WITH_USB */

	if (!use_usb) {
		upsfd = ser_open(device_path);
		ser_set_speed(upsfd, device_path, microlink_baudrate);
		ser_set_dtr(upsfd, 1);
	}
}

static void microlink_register_outlet_commands(void)
{
	size_t outlet_group_count, switched_group_count = 0, g;
	int i;
	char cmd[64];

	if (outlet_commands_registered)
		return;

	outlet_group_count = microlink_outlet_group_count();
	if (outlet_group_count == 0)
		return;

	dstate_setinfo("outlet.group.count", "%u", (unsigned int)outlet_group_count);

	for (g = 0; g < outlet_group_count; g++) {
		int is_switched = microlink_outlet_group_is_switched(g);

		snprintf(cmd, sizeof(cmd), "outlet.group.%zu.switchable", g);
		dstate_setinfo(cmd, "%s", is_switched ? "yes" : "no");
		if (is_switched)
			switched_group_count++;
	}

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
		if (!microlink_outlet_group_is_switched(g))
			continue;
		for (i = 0; outlet_suffixes[i] != NULL; i++) {
			snprintf(cmd, sizeof(cmd), "outlet.group.%zu.%s", g, outlet_suffixes[i]);
			dstate_addcmd(cmd);
		}
	}

	outlet_commands_registered = 1;
	upslogx(LOG_NOTICE, "apcmicrolink: registered %zu outlet group(s) "
		"(%zu switchable) and their instant commands",
		outlet_group_count, switched_group_count);
}

void upsdrv_initinfo(void)
{
	int microlink_ready = 0;

	memset(objects, 0, sizeof(objects));
	session_ready = 0;
	rxbuf_len = 0;
	parsed_frames = 0;
	consecutive_timeouts = 0;
	poll_primed = 0;
	authentication_sent = 0;
	memset(&page0, 0, sizeof(page0));
	descriptor_ready = 0;
	outlet_commands_registered = 0;
	microlink_session_next_retry = 0;
	microlink_fallback_since = 0;
	/* Capture the user's configured pollinterval (or main.c's own default)
	 * before anything below has a chance to lower poll_interval to pace
	 * connection retries - a successfully connected session restores this
	 * value rather than busy-polling at 0s once the tunnel is up, since
	 * ups.status/battery.charge/outlet state don't change fast enough to
	 * need that. */
	microlink_configured_poll_interval = poll_interval;

	if (microlink_start_session()) {
		microlink_ready = 1;
		while (microlink_ready && !microlink_startup_ready()) {
			time_t now = microlink_now();

			if (!microlink_poll_once(now) && consecutive_timeouts >= microlink_handshake_retries()) {
				microlink_ready = 0;
			}
		}
	}

	/* Historically this was a hard fatalx() in both failure cases above,
	 * refusing to start at all whenever the Microlink handshake failed -
	 * on this device that handshake is intermittently unreliable enough
	 * (session startup can take 90s+ to become responsive) that this was
	 * the single biggest source of "driver won't even start" complaints.
	 * If the standard-HID-PDC fallback is available, start up on that
	 * instead of refusing to start at all:
	 * ups.status/battery.charge/battery.runtime come from the fallback
	 * source, everything descriptor-derived (identity strings, outlet
	 * groups and their commands) is simply not published yet, and
	 * `session_ready` stays 0 so upsdrv_updateinfo()'s normal retry logic
	 * keeps trying the real Microlink handshake on every subsequent poll.
	 * Outlet-group commands are registered by microlink_register_outlet_commands(),
	 * which is called both here (for a clean startup) and from upsdrv_updateinfo()
	 * (for the post-fallback transition), so no driver restart is needed. */
	if (microlink_ready) {
		microlink_publish_identity();
		microlink_publish_status();
		microlink_publish_runtime();
		microlink_publish_hid_fallback_inactive();
	} else if (microlink_publish_hid_fallback()) {
		session_ready = 0;
		/* Backdate microlink_fallback_since by the probing time the startup
		 * handshake already spent, so the diagnostic log message below
		 * reports a realistic elapsed time instead of ~0s. */
		microlink_fallback_since = microlink_now() - (time_t)microlink_handshake_retries();
		upslogx(LOG_WARNING, "apcmicrolink: could not complete Microlink startup on %s - "
			"starting up with standard-HID fallback data only (ups.status/"
			"battery.charge/battery.runtime); outlet-group data and commands "
			"will become available automatically once the Microlink session connects",
			device_path);
	} else if (microlink_hid_fallback_possible()) {
		/* The usages are there, no report carrying them has just happened
		 * to arrive yet. They stream independently of the Microlink tunnel,
		 * so waiting costs nothing and dying here threw away a device that
		 * could report ups.status within seconds. */
		session_ready = 0;
		microlink_fallback_since = microlink_now() - (time_t)microlink_handshake_retries();
		upslogx(LOG_WARNING, "apcmicrolink: could not complete Microlink startup on %s - "
			"this device does expose standard HID Power Device usages, but none "
			"have been decoded yet; starting up anyway and publishing "
			"ups.status/battery.charge/battery.runtime from them as soon as they "
			"arrive. Outlet-group data and commands will become available "
			"automatically once the Microlink session connects", device_path);
	} else {
		fatalx(EXIT_FAILURE, "apcmicrolink: failed to start Microlink session on %s "
			"and this device exposes no standard HID Power Device usages to fall "
			"back on", device_path);
	}

	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("test.panel.start");
	dstate_addcmd("beeper.mute");
	dstate_addcmd("calibrate.start");
	dstate_addcmd("calibrate.stop");
	dstate_addcmd("bypass.start");
	dstate_addcmd("bypass.stop");

	microlink_register_outlet_commands();
	upsh.instcmd = instcmd;
	upsh.setvar = setvar;
}

void upsdrv_updateinfo(void)
{
	int good = 0;
	time_t now = microlink_now();

	if (!session_ready) {
		int reopening = 0;

		if (microlink_fallback_since == 0)
			microlink_fallback_since = now;

		if (now < microlink_session_next_retry) {
			poll_interval = MLINK_SESSION_RETRY_INTERVAL_SEC;
			microlink_datastale_or_fallback();
			return;
		}

#ifdef WITH_USB
		/* A reset only ever helps a genuine USB disconnect (confirmed live:
		 * it recovered a real unplug/power-cycle); it never once helped a
		 * live-but-stalled tunnel in testing, so that's the only condition
		 * that triggers one now - not a blind retry count. */
		if (is_usb && microlink_usb_device_gone()) {
			upslogx(LOG_NOTICE, "apcmicrolink: USB device appears to have been "
				"disconnected (unresponsive for %ld s) - reopening once it "
				"reappears", (long)(now - microlink_fallback_since));
			reopening = 1;
			if (microlink_usb_reset_and_reopen())
				microlink_start_session();
		} else if (microlink_fallback_retries > 0 && microlink_fallback_retries % 64 == 0) {
			upslogx(LOG_NOTICE, "apcmicrolink: Microlink tunnel unresponsive for "
				"%ld s (%u consecutive failed retries)",
				(long)(now - microlink_fallback_since), microlink_fallback_retries);
		}
#endif

		if (!reopening)
			microlink_start_session_impl(1);

		if (!session_ready) {
			microlink_fallback_retries++;
			microlink_session_next_retry = now + MLINK_SESSION_RETRY_INTERVAL_SEC;
			poll_interval = MLINK_SESSION_RETRY_INTERVAL_SEC;
			microlink_datastale_or_fallback();
			return;
		}
		microlink_fallback_since = 0;
		microlink_fallback_retries = 0;
		poll_interval = microlink_configured_poll_interval;
	}

	if (microlink_poll_once(now)) {
		good = 1;
	}

	if (!good && microlink_midsession_timed_out(now)) {
		if (!microlink_reconnect_session()) {
			microlink_datastale_or_fallback();
			return;
		}
		good = 1;
	}

	/* A reconnect only proves the device still answers the handshake, not
	 * that the tunnel is delivering anything. Where it is not, prefer the
	 * fallback over republishing whatever the Microlink cache last held. */
	if (good && microlink_data_stale(now) && microlink_publish_hid_fallback()) {
		microlink_log_fallback_reason(MLINK_FB_REASON_STALE, now);
		dstate_dataok();
		return;
	}

	if (!good) {
		if (parsed_frames == 0) {
			session_ready = 0;
			microlink_datastale_or_fallback();
			return;
		}

		microlink_register_outlet_commands();
		microlink_publish_identity();
		microlink_publish_status();
		microlink_publish_runtime();
		if (microlink_fallback_takes_over(now)) {
			dstate_dataok();
			return;
		}
		microlink_publish_hid_fallback_inactive();
		dstate_dataok();
		return;
	}

	ser_comm_good();
	microlink_register_outlet_commands();
	microlink_publish_identity();
	microlink_publish_status();
	microlink_publish_runtime();
	if (microlink_fallback_takes_over(now)) {
		dstate_dataok();
		return;
	}
	microlink_publish_hid_fallback_inactive();
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
#ifdef WITH_USB
	microlink_usb_addvars();
	addvar(VAR_VALUE, "hid_fallback",
		"Publish ups.status/battery.charge/battery.runtime from standard HID "
		"Power Device usages when the Microlink tunnel has nothing fresh, "
		"instead of going stale (USB only; yes/no, default yes)");
#endif /* WITH_USB */
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
#ifdef WITH_USB
	if (is_usb) {
		microlink_usb_close();
		return;
	}
#endif /* WITH_USB */

	if (VALID_FD(upsfd)) {
		ser_close(upsfd, device_path);
		upsfd = ERROR_FD;
	}
}
