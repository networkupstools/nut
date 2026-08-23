/* apcmicrolink-usb.c - USB HID tunnel transport for the APC Microlink protocol driver
 *
 * The SCL500RMI1UC (and presumably other USB-only Microlink devices) has no
 * serial port; it exposes the same Microlink byte protocol that
 * apcmicrolink.c already speaks over RS232, tunneled instead through two
 * HID vendor-page reports on the generic APC HID vendor tunnel (the same
 * one apc_modbus.c uses for Modbus-RTU-over-USB):
 *
 *   - Output Report (host->device writes), HID vendor page 0xFF86 usage
 *     0xFC. Confirmed live as Report ID 0x90 on the SCL500RMI1UC.
 *   - Input Report (device->host reads), HID vendor page 0xFF86 usage
 *     0xFD. Confirmed live as Report ID 0x89 on the SCL500RMI1UC.
 *
 * Report IDs are discovered dynamically from the device's HID report
 * descriptor (they could differ on other devices/firmware revisions),
 * mirroring apc_modbus.c's _apc_modbus_usb_callback().
 *
 * This file only replaces the "send N bytes" / "read next available byte"
 * primitives that apcmicrolink.c's microlink_send_simple()/
 * microlink_send_write()/microlink_receive_once() sit on top of; the frame
 * construction, checksum, object cache, descriptor parser, auth and
 * outlet-group logic in apcmicrolink.c are untouched and unaware of which
 * transport is underneath.
 *
 * A background-thread reader (keeping a read continuously pending on the
 * interrupt IN endpoint, decoupled from write timing, mirroring the real
 * Windows driver's design) was tried here to see if it would improve this
 * device's occasionally-slow-to-respond behavior. A controlled A/B test
 * (two minimal standalone libusb programs, byte-identical 0xFD/0xFE
 * sequence, one threaded one not) showed the threaded version getting
 * *zero* successful replies over 10s while the sequential version got
 * several - a clear regression, not an improvement, likely from libusb's
 * synchronous API serializing event-handling between the two threads and
 * delaying the write. Reverted; this file is back to the simple
 * synchronous write-then-read design that is confirmed working.
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

#include <string.h>
#include <unistd.h>
#include <time.h>

#include "nut_stdint.h"
#include "nut_libusb.h"
#include "usb-common.h"
#include "hidparser.h"

#include "apcmicrolink-usb.h"

/* American Power Conversion */
#define APC_VENDORID	0x051d

/* HID vendor page 0xFF86, usages 0xFC (host->device) / 0xFD (device->host) -
 * the same generic raw-byte HID tunnel apc_modbus.c's
 * modbus_rtu_usb_usage_rx/tx use for Modbus-RTU-over-USB. Numerically
 * identical usages, different upper protocol riding over them. */
static const HIDNode_t mlink_usb_usage_out = 0xff8600fcUL;
static const HIDNode_t mlink_usb_usage_in  = 0xff8600fdUL;

/* Report IDs behind those usages, discovered per-device at open time.
 * Expected to be 0x90 (out) / 0x89 (in) on the SCL500RMI1UC, but never
 * hardcoded - other devices/firmware could number them differently. */
static int mlink_report_out = 0;
static int mlink_report_in = 0;

/* Standard HID Power/Battery System Page usages behind the "foreign"
 * background reports this device also pushes on the same interrupt pipe,
 * independent of the Microlink tunnel's health - confirmed live against
 * this exact device (report descriptor walk + a real usbhid-ups side by
 * side run). Looked up by usage rather than assumed to be at fixed report
 * IDs/offsets, same as mlink_usb_usage_out/in above - a firmware revision
 * or a different "5G model" device could number/order these differently. */
static const HIDNode_t hid_usage_charging            = 0x00850044UL;
static const HIDNode_t hid_usage_discharging         = 0x00850045UL;
static const HIDNode_t hid_usage_ac_present          = 0x008500D0UL;
static const HIDNode_t hid_usage_below_rcl           = 0x00850042UL;
static const HIDNode_t hid_usage_remaining_capacity  = 0x00850066UL;
static const HIDNode_t hid_usage_runtime_to_empty    = 0x00850068UL;

typedef struct {
	int report_id;	/* 0 = not found on this device */
	int offset;	/* bit offset within the report (excludes the leading Report ID byte) */
	int size;	/* bit size */
} hid_fallback_field_t;

static hid_fallback_field_t ff_charging;
static hid_fallback_field_t ff_discharging;
static hid_fallback_field_t ff_ac_present;
static hid_fallback_field_t ff_below_rcl;
static hid_fallback_field_t ff_remaining_capacity;
static hid_fallback_field_t ff_runtime_to_empty;

/* Latest opportunistically-decoded fallback snapshot. fb_last_update==0
 * means "nothing decoded yet this session". */
static int fb_ac_present = 0;
static int fb_discharging = 0;
static int fb_below_rcl = 0;
static long fb_battery_charge = -1;
static long fb_battery_runtime = -1;
static time_t fb_last_update = 0;

/* Confirmed live: Output/Input report Byte Length 64 (1 Report ID byte +
 * 63 bytes of value data, BitSize=8 Count=63 on both sides). */
#define MLINK_USB_REPORT_PAYLOAD_LEN	63U
#define MLINK_USB_REPORT_TOTAL_LEN	(1U + MLINK_USB_REPORT_PAYLOAD_LEN)

/* The decompiled Windows driver used a 400ms timeout for Output report
 * writes (ApcUsb_ul.dll, writeMultiByteMessage/writeSingleByteCommand);
 * kept a bit more generous here since we are not racing a UI. */
#define MLINK_USB_WRITE_TIMEOUT_MS	1000U

static usb_dev_handle *udev = NULL;
static USBDevice_t curDevice;
static USBDeviceMatcher_t *regex_matcher = NULL;
static usb_communication_subdriver_t *comm_driver = &usb_subdriver;

static unsigned char in_report[MLINK_USB_REPORT_TOTAL_LEN];
static size_t in_report_len = 0;	/* bytes valid in in_report (0 = empty) */
static size_t in_report_pos = 0;	/* next unconsumed index */

static usb_device_id_t apcmicrolink_usb_device_table[] = {
	/* SCL500RMI1UC, confirmed live (VID/PID + product string
	 * "Smart-UPS 500 FW:UPS 15.6 / ID=1036"). Also already recognized
	 * by apc-hid.c's generic apc_usb_device_table ("various 5G models") -
	 * expected overlap, same as apc_modbus vs. usbhid-ups for several
	 * Smart-UPS models; the user picks whichever driver suits them. */
	{ USB_DEVICE(APC_VENDORID, 0x0003), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};

static int microlink_usb_match(USBDevice_t *device, void *privdata)
{
	NUT_UNUSED_VARIABLE(privdata);

	switch (is_usb_device_supported(apcmicrolink_usb_device_table, device))
	{
	case SUPPORTED:
		return 1;

	case POSSIBLY_SUPPORTED:
	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

static USBDeviceMatcher_t microlink_usb_device_matcher = {
	&microlink_usb_match,
	NULL,
	NULL
};

/* Called by comm_driver->open_dev() once a candidate device's HID report
 * descriptor has been fetched. Parse it and pick out the Report IDs behind
 * our two vendor-page usages, exactly as apc_modbus.c's
 * _apc_modbus_usb_callback() does for its own pair of usages. */
static int microlink_usb_report_callback(usb_dev_handle *arg_udev, USBDevice_t *hd,
	usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen)
{
	HIDDesc_t *hid_desc;
	size_t i;

	NUT_UNUSED_VARIABLE(arg_udev);
	NUT_UNUSED_VARIABLE(hd);

	mlink_report_out = 0;
	mlink_report_in = 0;

	memset(&ff_charging, 0, sizeof(ff_charging));
	memset(&ff_discharging, 0, sizeof(ff_discharging));
	memset(&ff_ac_present, 0, sizeof(ff_ac_present));
	memset(&ff_below_rcl, 0, sizeof(ff_below_rcl));
	memset(&ff_remaining_capacity, 0, sizeof(ff_remaining_capacity));
	memset(&ff_runtime_to_empty, 0, sizeof(ff_runtime_to_empty));
	fb_last_update = 0;

	if (rdbuf == NULL || rdlen <= 0) {
		upsdebugx(1, "microlink_usb: no HID report descriptor available");
		return -1;
	}

	hid_desc = Parse_ReportDesc(rdbuf, rdlen);
	if (!hid_desc) {
		upsdebug_with_errno(1, "microlink_usb: failed to parse HID report descriptor");
		return -1;
	}

	for (i = 0; i < hid_desc->nitems; i++) {
		HIDData_t *item = &hid_desc->item[i];
		HIDNode_t usage = item->Path.Node[item->Path.Size - 1];
		hid_fallback_field_t *target = NULL;

		if (usage == mlink_usb_usage_out) {
			mlink_report_out = item->ReportID;
			continue;
		}
		if (usage == mlink_usb_usage_in) {
			mlink_report_in = item->ReportID;
			continue;
		}

		/* Only the autonomous Input variant is useful here - the
		 * duplicate Feature-report entries for the same usage would
		 * need an explicit GET_REPORT control transfer we never send. */
		if (item->Type != ITEM_INPUT) {
			continue;
		}

		if (usage == hid_usage_charging) {
			target = &ff_charging;
		} else if (usage == hid_usage_discharging) {
			target = &ff_discharging;
		} else if (usage == hid_usage_ac_present) {
			target = &ff_ac_present;
		} else if (usage == hid_usage_below_rcl) {
			target = &ff_below_rcl;
		} else if (usage == hid_usage_remaining_capacity) {
			target = &ff_remaining_capacity;
		} else if (usage == hid_usage_runtime_to_empty) {
			target = &ff_runtime_to_empty;
		}

		if (target != NULL) {
			target->report_id = item->ReportID;
			target->offset = item->Offset;
			target->size = item->Size;
		}
	}

	if (ff_ac_present.report_id != 0 && ff_discharging.report_id != 0) {
		upsdebugx(1, "microlink_usb: HID PDC status fallback available "
			"(PresentStatus on Report 0x%02X)", (unsigned int)ff_ac_present.report_id);
	} else {
		upsdebugx(1, "microlink_usb: HID PDC status fallback not available on this device "
			"(ACPresent/Discharging usages not found)");
	}

	Free_ReportDesc(hid_desc);

	if (mlink_report_out == 0 || mlink_report_in == 0) {
		upsdebugx(1, "microlink_usb: Microlink USB HID tunnel (vendor page 0xFF86, "
			"usages 0xFC/0xFD) not found on this device");
		return -1;
	}

	upsdebugx(1, "microlink_usb: found HID tunnel: Output Report 0x%02X, Input Report 0x%02X",
		(unsigned int)mlink_report_out, (unsigned int)mlink_report_in);

	return 1;
}

void microlink_usb_addvars(void)
{
	nut_usb_addvars();
}

int microlink_usb_open(void)
{
	char *regex_array[USBMATCHER_REGEXP_ARRAY_LIMIT];
	int ret;

	warn_if_bad_usb_port_filename(device_path);

	regex_array[0] = getval("vendorid");
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor");
	regex_array[3] = getval("product");
	regex_array[4] = getval("serial");
	regex_array[5] = getval("bus");
	regex_array[6] = getval("device");
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	regex_array[7] = getval("busport");
#else
	if (getval("busport")) {
		upslogx(LOG_WARNING, "\"busport\" is configured for the device, but is not "
			"actually handled by current build combination of NUT and libusb (ignored)");
	}
#endif

	ret = USBNewRegexMatcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
	if (ret < 0) {
		fatal_with_errno(EXIT_FAILURE, "USBNewRegexMatcher");
	} else if (ret) {
		fatalx(EXIT_FAILURE, "invalid regular expression: %s", regex_array[ret]);
	}

	regex_matcher->next = &microlink_usb_device_matcher;

	ret = comm_driver->open_dev(&udev, &curDevice, regex_matcher, microlink_usb_report_callback);
	if (ret < 1) {
		fatalx(EXIT_FAILURE, "apcmicrolink: no matching USB Microlink UPS found");
	}

	dstate_setinfo("ups.vendorid", "%04x", curDevice.VendorID);
	dstate_setinfo("ups.productid", "%04x", curDevice.ProductID);

	upsdebugx(1, "microlink_usb: opened %s/%s (USB %04x:%04x)",
		curDevice.Vendor ? curDevice.Vendor : "unknown",
		curDevice.Product ? curDevice.Product : "unknown",
		curDevice.VendorID, curDevice.ProductID);

	in_report_len = 0;
	in_report_pos = 0;

	return 1;
}

void microlink_usb_close(void)
{
	if (udev) {
		comm_driver->close_dev(udev);
		udev = NULL;
	}

	USBFreeRegexMatcher(regex_matcher);
	regex_matcher = NULL;

	free(curDevice.Vendor);
	free(curDevice.Product);
	free(curDevice.Serial);
	free(curDevice.Bus);
	free(curDevice.Device);
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	free(curDevice.BusPort);
#endif
	memset(&curDevice, 0, sizeof(curDevice));

	in_report_len = 0;
	in_report_pos = 0;
}

int microlink_usb_reset_and_reopen(void)
{
	int ret;

	if (udev) {
		/* Send USB bus reset. The handle is invalid afterward regardless
		 * of the return value -- close it unconditionally. usb_reset()
		 * is NUT's usb-common.h abstraction (libusb_reset_device on
		 * libusb-1.0, usb_reset on libusb-0.1); calling libusb_reset_device
		 * directly breaks NUT_USB_VARIANT=0.1 builds. */
		usb_reset(udev);
		comm_driver->close_dev(udev);
		udev = NULL;
	}

	/* Discard old device strings; open_dev() will re-populate them. */
	free(curDevice.Vendor);
	free(curDevice.Product);
	free(curDevice.Serial);
	free(curDevice.Bus);
	free(curDevice.Device);
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	free(curDevice.BusPort);
#endif
	memset(&curDevice, 0, sizeof(curDevice));
	in_report_len = 0;
	in_report_pos = 0;

	/* Give the device time to complete re-enumeration before opening. */
	sleep(2);

	/* Re-open using the still-valid regex_matcher (never freed here). */
	ret = comm_driver->open_dev(&udev, &curDevice, regex_matcher,
		microlink_usb_report_callback);
	if (ret < 1) {
		udev = NULL;
		return 0;
	}

	upsdebugx(1, "microlink_usb: reset and reopened %s/%s (USB %04x:%04x)",
		curDevice.Vendor ? curDevice.Vendor : "unknown",
		curDevice.Product ? curDevice.Product : "unknown",
		curDevice.VendorID, curDevice.ProductID);

	return 1;
}

/* A timeout of 0 to libusb_interrupt_transfer() means "wait forever", not
 * "don't block" - confirmed the hard way against real hardware, where a
 * 0ms drain call could stall this function (and everything after it) for
 * however long the device's interrupt pipe happened to be idle, since
 * this device also pushes unrelated Input reports on the same pipe only
 * every 100-200ms (see apcmicrolink-usb.c's header comment) and the pipe
 * can be briefly, genuinely empty. Use a short real timeout instead, and
 * cap the number of drained reports as a defensive bound. */
#define MLINK_USB_FLUSH_TIMEOUT_MS	20U
#define MLINK_USB_FLUSH_MAX_REPORTS	32U

void microlink_usb_flush_io(void)
{
	unsigned char discard[MLINK_USB_REPORT_TOTAL_LEN];
	int ret;
	unsigned int drained;

	in_report_len = 0;
	in_report_pos = 0;

	if (!udev) {
		return;
	}

	for (drained = 0; drained < MLINK_USB_FLUSH_MAX_REPORTS; drained++) {
		ret = comm_driver->get_interrupt(udev, (usb_ctrl_charbuf)discard,
			(usb_ctrl_charbufsize)sizeof(discard),
			(usb_ctrl_timeout_msec)MLINK_USB_FLUSH_TIMEOUT_MS);
		if (ret <= 0) {
			break;
		}
	}
}

/* Confirmed live on real hardware: the Linux kernel's own generic "usbhid"
 * driver can reclaim this device's interface out from under an already-open,
 * already-claimed libusb handle - observed once, apparently following a USB
 * reset/re-enumeration event this driver has no visibility into. Once that
 * happens, every USBDEVFS_SUBMITURB this process makes is rejected by the
 * kernel with EBUSY (surfaces here as LIBUSB_ERROR_BUSY) until the process
 * is restarted - and because that failure returns near-instantly, unlike a
 * real timeout, nothing above this file paces the resulting retries: one
 * observed incident spun at ~14,000 failed submissions/sec for 30+ minutes
 * straight, saturating a CPU core and, via dmesg's own "did not claim
 * interface" message at the same rate, completely overwriting the kernel
 * ring buffer before the actual triggering event could ever be identified.
 *
 * A permanent udev rule that unbinds usbhid from this VID:PID (installed
 * outside this source tree, e.g. as a distro packaging step) avoids this in
 * practice. This handler is defense in depth for that rule being absent, or
 * for some other cause of the same kernel-level symptom: back off hard
 * instead of spinning, and log loudly (not upsdebugx, which is invisible
 * without -D) but rate-limited, so a recurrence is visible in syslog
 * without flooding it or dmesg the way the uncaught case did. */
#define MLINK_USB_BUSY_BACKOFF_USEC	1000000U
#define MLINK_USB_BUSY_LOG_EVERY	30U

static void microlink_usb_handle_busy(const char *context)
{
	static unsigned long busy_count = 0;

	busy_count++;

	if (busy_count == 1 || (busy_count % MLINK_USB_BUSY_LOG_EVERY) == 0) {
		upslogx(LOG_WARNING, "microlink_usb: %s: USBDEVFS_SUBMITURB rejected "
			"with EBUSY (occurrence %lu) - another driver (commonly the "
			"kernel's own \"usbhid\") may have reclaimed this device's USB "
			"interface out from under this process; check \"lsusb -t\" and "
			"dmesg for a competing driver bound to this device. "
			"Backing off %.1fs before the next attempt instead of retrying "
			"immediately.",
			context, busy_count, (double)MLINK_USB_BUSY_BACKOFF_USEC / 1000000.0);
	}

	usleep(MLINK_USB_BUSY_BACKOFF_USEC);
}

int microlink_usb_send_bytes(const unsigned char *buf, size_t len)
{
	unsigned char raw_buf[MLINK_USB_REPORT_TOTAL_LEN];
	int ret;

	if (!udev || mlink_report_out == 0) {
		return 0;
	}

	if (len > MLINK_USB_REPORT_PAYLOAD_LEN) {
		upsdebugx(1, "microlink_usb: refusing to send a %" PRIuSIZE
			"-byte frame, exceeds the %u-byte USB Output report capacity",
			len, (unsigned int)MLINK_USB_REPORT_PAYLOAD_LEN);
		return 0;
	}

	/* Confirmed against real SCL500RMI1UC hardware: comm_driver->set_report()
	 * (nut_libusb_set_report(), HID class SET_REPORT control transfer with
	 * Report Type Output 0x02<<8 instead of its hardcoded Feature 0x03<<8)
	 * was tried first, and while the control transfer itself succeeds at
	 * the USB level (no stall, no error), the device never actually reacts
	 * to it - no reply ever appears on the Input Report 0x89 channel,
	 * regardless of init byte or how long you wait. Switching to a genuine
	 * interrupt-OUT write on the Output endpoint (matching the real
	 * Windows driver's behavior) gets an immediate, correctly-framed,
	 * checksum-valid response stream. So this device's raw HID tunnel
	 * genuinely requires interrupt-OUT, not just a HID-spec-valid control
	 * transfer to the same Report ID - apparently the firmware's tunnel
	 * implementation only watches the interrupt endpoint's hardware FIFO,
	 * not the control endpoint. */
	raw_buf[0] = (unsigned char)mlink_report_out;
	memset(raw_buf + 1, 0, sizeof(raw_buf) - 1);
	memcpy(raw_buf + 1, buf, len);

	ret = usb_interrupt_write(udev,
		USB_ENDPOINT_OUT + usb_subdriver.hid_ep_out,
		(usb_ctrl_charbuf)raw_buf,
		(int)sizeof(raw_buf),
		(int)MLINK_USB_WRITE_TIMEOUT_MS);

	if (ret == LIBUSB_ERROR_PIPE) {
		upsdebugx(2, "microlink_usb: interrupt-OUT write (Output 0x%02X) stalled",
			(unsigned int)mlink_report_out);
		return 0;
	}

	if (ret == LIBUSB_ERROR_BUSY) {
		microlink_usb_handle_busy("interrupt-OUT write");
		return 0;
	}

	if (ret < 0) {
		upsdebugx(1, "microlink_usb: interrupt-OUT write (Output 0x%02X) failed: %s",
			(unsigned int)mlink_report_out, nut_usb_strerror(ret));
		return 0;
	}

	return 1;
}

/* Extract a little-endian, LSB-first-packed bit field from a HID report's
 * data (everything after the leading Report ID byte), matching how
 * Parse_ReportDesc()'s Offset/Size describe fields - confirmed against
 * real captured bytes from this device (e.g. a 32-bit RunTimeToEmpty
 * field at offset 0 decoded correctly as little-endian this way).
 * bit_size is capped to fit in an unsigned long as this is only ever
 * used for <=32-bit fields here. */
static unsigned long hid_extract_bits(const unsigned char *data, size_t data_len,
	int bit_offset, int bit_size)
{
	unsigned long value = 0;
	int i;

	if (bit_offset < 0 || bit_size <= 0 || bit_size > 32) {
		return 0;
	}

	for (i = 0; i < bit_size; i++) {
		int bit_pos = bit_offset + i;
		size_t byte_idx = (size_t)(bit_pos / 8);
		int bit_idx = bit_pos % 8;

		if (byte_idx >= data_len) {
			break;
		}
		if ((data[byte_idx] & (1 << bit_idx)) != 0) {
			value |= (1UL << i);
		}
	}

	return value;
}

/* Opportunistically decode the standard-HID-PDC fallback fields if this
 * report happens to be one of them - called on every Input report we see,
 * whether or not it turns out to be our own Microlink tunnel reply, since
 * these arrive autonomously and independently of tunnel health. */
static void microlink_usb_try_decode_fallback(const unsigned char *report, size_t report_len)
{
	int report_id;
	const unsigned char *data;
	size_t data_len;

	if (report_len < 1) {
		return;
	}
	report_id = (int)report[0];
	data = report + 1;
	data_len = report_len - 1;

	if (ff_ac_present.report_id == report_id) {
		fb_ac_present = (int)hid_extract_bits(data, data_len, ff_ac_present.offset, ff_ac_present.size);
	}
	if (ff_discharging.report_id == report_id) {
		fb_discharging = (int)hid_extract_bits(data, data_len, ff_discharging.offset, ff_discharging.size);
	}
	if (ff_below_rcl.report_id == report_id) {
		fb_below_rcl = (int)hid_extract_bits(data, data_len, ff_below_rcl.offset, ff_below_rcl.size);
	}
	if (ff_remaining_capacity.report_id == report_id) {
		fb_battery_charge = (long)hid_extract_bits(data, data_len,
			ff_remaining_capacity.offset, ff_remaining_capacity.size);
	}
	if (ff_runtime_to_empty.report_id == report_id) {
		fb_battery_runtime = (long)hid_extract_bits(data, data_len,
			ff_runtime_to_empty.offset, ff_runtime_to_empty.size);
	}

	if (report_id == ff_ac_present.report_id || report_id == ff_discharging.report_id
	 || report_id == ff_below_rcl.report_id || report_id == ff_remaining_capacity.report_id
	 || report_id == ff_runtime_to_empty.report_id) {
		fb_last_update = time(NULL);
	}
}

int microlink_usb_get_hid_fallback(int max_age_sec,
	int *ac_present, int *discharging, int *below_rcl,
	long *battery_charge, long *battery_runtime)
{
	if (ff_ac_present.report_id == 0 || ff_discharging.report_id == 0) {
		return 0;
	}

	if (fb_last_update == 0 || max_age_sec < 0
	 || difftime(time(NULL), fb_last_update) > (double)max_age_sec) {
		return 0;
	}

	if (ac_present != NULL) {
		*ac_present = fb_ac_present;
	}
	if (discharging != NULL) {
		*discharging = fb_discharging;
	}
	if (below_rcl != NULL) {
		*below_rcl = fb_below_rcl;
	}
	if (battery_charge != NULL) {
		*battery_charge = (ff_remaining_capacity.report_id != 0) ? fb_battery_charge : -1;
	}
	if (battery_runtime != NULL) {
		*battery_runtime = (ff_runtime_to_empty.report_id != 0) ? fb_battery_runtime : -1;
	}

	return 1;
}

int microlink_usb_hid_fallback_supported(void)
{
	return (ff_ac_present.report_id != 0 && ff_discharging.report_id != 0);
}

int microlink_usb_get_char(unsigned char *ch, long d_usec)
{
	int ret;
	usb_ctrl_timeout_msec timeout_ms;

	if (!udev || mlink_report_in == 0) {
		return -1;
	}

	if (in_report_pos < in_report_len) {
		*ch = in_report[in_report_pos++];
		return 1;
	}

	if (d_usec < 0) {
		d_usec = 0;
	}
	timeout_ms = (usb_ctrl_timeout_msec)((d_usec / 1000L) + ((d_usec % 1000L) ? 1L : 0L));

	ret = comm_driver->get_interrupt(udev, (usb_ctrl_charbuf)in_report,
		(usb_ctrl_charbufsize)sizeof(in_report), timeout_ms);

	if (ret == LIBUSB_ERROR_BUSY) {
		microlink_usb_handle_busy("interrupt-IN read");
		in_report_len = 0;
		in_report_pos = 0;
		return -1;
	}

	if (ret < 0) {
		in_report_len = 0;
		in_report_pos = 0;
		return -1;
	}

	/* The interrupt IN endpoint can also carry other Input reports this
	 * device pushes (e.g. the mirrored 0x84:0x24 scalars) - ignore
	 * anything that is not our row-push channel rather than feeding
	 * foreign report bytes into the Microlink byte stream. Before
	 * discarding, though: some of those "other" reports are exactly the
	 * standard-HID-PDC fallback fields, which keep arriving
	 * independent of whether the tunnel itself is responding - worth
	 * decoding regardless of what this specific read call was for. */
	if (ret >= 1) {
		microlink_usb_try_decode_fallback(in_report, (size_t)ret);
	}

	if (ret < 2 || (int)in_report[0] != mlink_report_in) {
		in_report_len = 0;
		in_report_pos = 0;
		return 0;
	}

	in_report_len = (size_t)ret;
	in_report_pos = 1; /* skip the leading Report ID byte */

	*ch = in_report[in_report_pos++];
	return 1;
}
