/* apcmicrolink-usb.c - USB HID tunnel transport for the APC Microlink protocol driver
 *
 * USB-only Microlink devices (e.g. the SCL500RMI1UC) have no serial port;
 * they carry the same Microlink byte protocol that apcmicrolink.c speaks over
 * RS232, tunneled through two HID vendor page 0xFF86 reports on the generic
 * APC HID vendor tunnel (the one apc_modbus.c uses for Modbus-RTU-over-USB):
 * usage 0xFC for host->device writes, usage 0xFD for device->host reads. The
 * Report IDs behind them are discovered from the device's HID report
 * descriptor, as in apc_modbus.c's _apc_modbus_usb_callback().
 *
 * Only the "send N bytes" / "read next available byte" primitives are replaced
 * here; apcmicrolink.c's framing, checksums, object cache, descriptor parser,
 * auth and outlet-group logic are unaware of which transport is underneath.
 *
 * On libusb-1.0 + pthread builds, reads are served by an always-outstanding
 * async interrupt-IN transfer serviced by a dedicated pump thread: the device
 * can go several seconds between replies, and a synchronous read only listens
 * while the driver happens to be blocked waiting on one. Only the pump thread
 * may call libusb_handle_events*() on this context. Builds without libusb-1.0
 * or pthreads, and any failure to start the listener, fall back to a
 * synchronous per-call read (microlink_usb_get_char_sync()).
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
#include <sys/time.h>
#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif /* HAVE_PTHREAD */

#include "nut_stdint.h"
#include "nut_libusb.h"
#include "usb-common.h"
#include "hidparser.h"
#include "strcasestr-static.h"

#include "apcmicrolink.h"
#include "apcmicrolink-usb.h"

/* American Power Conversion */
#define APC_VENDORID	0x051d

/* HID vendor page 0xFF86, usages 0xFC (host->device) / 0xFD (device->host):
 * the same raw-byte tunnel apc_modbus.c rides Modbus-RTU over. */
static const HIDNode_t mlink_usb_usage_out = 0xff8600fcUL;
static const HIDNode_t mlink_usb_usage_in  = 0xff8600fdUL;

/* Report IDs behind those usages, discovered per-device at open time. */
static int mlink_report_out = 0;
static int mlink_report_in = 0;

/* Standard HID Power/Battery System Page usages, carried by the background
 * reports this device also pushes on the same interrupt pipe independent of
 * the Microlink tunnel's health. Looked up by usage rather than assumed to be
 * at fixed report IDs/offsets, same as the tunnel usages above. */
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

/* Latest opportunistically decoded fallback snapshot; fb_last_update == 0
 * means nothing decoded yet this session. */
static int fb_ac_present = 0;
static int fb_discharging = 0;
static int fb_below_rcl = 0;
static long fb_battery_charge = -1;
static long fb_battery_runtime = -1;
static time_t fb_last_update = 0;

/* Output/Input reports are 64 bytes: 1 Report ID byte + 63 data bytes. */
#define MLINK_USB_REPORT_PAYLOAD_LEN	63U
#define MLINK_USB_REPORT_TOTAL_LEN	(1U + MLINK_USB_REPORT_PAYLOAD_LEN)

#define MLINK_USB_WRITE_TIMEOUT_MS	1000U

static usb_dev_handle *udev = NULL;
static USBDevice_t curDevice;
static USBDeviceMatcher_t *regex_matcher = NULL;
static usb_communication_subdriver_t *comm_driver = &usb_subdriver;

/* Set when a read, write or transfer completion reports NO_DEVICE: a genuine
 * physical disconnect, not just "no reply yet". Cleared once
 * microlink_usb_open() succeeds again. apcmicrolink.c polls this to decide
 * when a USB reset-and-reopen is actually warranted. */
static int usb_device_gone = 0;

#if WITH_LIBUSB_1_0 && defined(HAVE_PTHREAD)
/* Always-outstanding async interrupt-IN listener, serviced by a dedicated pump
 * thread: one libusb_transfer is kept permanently submitted, and its callback
 * (running on the pump thread) queues completed reports here and resubmits
 * itself. */
#define MLINK_USB_ASYNC_QUEUE_LEN	8U

typedef struct {
	unsigned char data[MLINK_USB_REPORT_TOTAL_LEN];
	size_t len;
} microlink_async_report_t;

/* The transfer buffer is heap-allocated per instance: an abandoned transfer
 * (see microlink_usb_async_stop()) has to be left to free itself, so a
 * listener started in the meantime must not be sharing that buffer. */
static struct libusb_transfer *async_xfer = NULL;
static int async_xfer_active = 0;	/* 1 while a transfer is submitted/outstanding */

/* The queue, async_xfer_active and the fb_* globals are touched by both the
 * pump thread and the main thread, so all go through async_lock. async_cond is
 * signalled whenever the callback queues a report, so get_char() can block on
 * it instead of polling. */
static pthread_mutex_t async_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t async_cond = PTHREAD_COND_INITIALIZER;

static microlink_async_report_t async_queue[MLINK_USB_ASYNC_QUEUE_LEN];
static unsigned int async_queue_head = 0;	/* next slot to pop */
static unsigned int async_queue_count = 0;	/* valid entries currently queued */

/* Keeps libusb's event loop serviced so the outstanding transfer is reaped and
 * resubmitted promptly. */
static pthread_t async_pump_tid;
static volatile int async_pump_stop = 0;

/* Defined below microlink_usb_try_decode_fallback(), which the transfer
 * callback calls. */
static int microlink_usb_async_start(void);
static void microlink_usb_async_stop(void);
#endif /* WITH_LIBUSB_1_0 && HAVE_PTHREAD */

static unsigned char in_report[MLINK_USB_REPORT_TOTAL_LEN];
static size_t in_report_len = 0;	/* bytes valid in in_report (0 = empty) */
static size_t in_report_pos = 0;	/* next unconsumed index */

static usb_device_id_t apcmicrolink_usb_device_table[] = {
	/* SCL500RMI1UC. Also matched by apc-hid.c's generic APC table ("various 5G
	 * models") - expected overlap, same as apc_modbus vs. usbhid-ups for several
	 * Smart-UPS models. */
	{ USB_DEVICE(APC_VENDORID, 0x0003), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};

/* Common USB-to-serial bridge chips. A match here on "port=auto" means the
 * right cable, wrong driver mode: use this driver's serial transport against
 * the /dev/ttyUSB*|COM* node the adapter created instead. */
static const struct {
	uint16_t vendorid;
	uint16_t productid;
	const char *name;
} known_usb_serial_bridges[] = {
	{ 0x0403, 0x6001, "FTDI FT232R" },
	{ 0x0403, 0x6014, "FTDI FT232H" },
	{ 0x0403, 0x6015, "FTDI FT230X" },
	{ 0x067b, 0x2303, "Prolific PL2303" },
	/* Also a legitimate UPS VID/PID for nutdrv_qx (see
	 * scripts/udev/nut-usbups.rules.in) - only the Product string heuristic
	 * below can tell the two uses apart. */
	{ 0x1a86, 0x7523, "WCH CH340/CH341" },
	{ 0x10c4, 0xea60, "Silicon Labs CP2102/CP2109" },
	{ 0x10c4, 0xea70, "Silicon Labs CP2105" },
	{ 0x10c4, 0xea71, "Silicon Labs CP2108" },
};

/* Lower-confidence signal for chips not in the table above: USB-serial bridges
 * almost always name the chip or say "serial"/"UART" in their Product string,
 * which a Microlink device's ("Smart-UPS ...") never does. */
static const char *serial_bridge_product_keywords[] = {
	"FTDI", "FT232", "FT231", "FT230",
	"PL2303", "PROLIFIC",
	"CH340", "CH341",
	"CP210", "CP2102", "CP2105", "CP2108",
	"USB SERIAL", "USB-SERIAL", "USB TO SERIAL", "UART BRIDGE",
};

/* Returns a short chip description if `device` looks like a generic
 * USB-to-serial bridge rather than a Microlink-over-USB-HID device, NULL
 * otherwise. Heuristic. */
static const char *microlink_usb_describe_serial_bridge(const USBDevice_t *device)
{
	size_t i;

	for (i = 0; i < SIZEOF_ARRAY(known_usb_serial_bridges); i++) {
		if (device->VendorID == known_usb_serial_bridges[i].vendorid
		&&  device->ProductID == known_usb_serial_bridges[i].productid
		) {
			return known_usb_serial_bridges[i].name;
		}
	}

	if (device->Product != NULL) {
		for (i = 0; i < SIZEOF_ARRAY(serial_bridge_product_keywords); i++) {
			if (strcasestr(device->Product, serial_bridge_product_keywords[i]) != NULL) {
				return device->Product;
			}
		}
	}

	return NULL;
}

/* Rate-limit the diagnostics below to once per (kind, VID:PID) per driver run:
 * "port=auto" re-enumerates the whole USB bus on every reconnect, so an
 * unrelated device left plugged in would otherwise warn on every cycle. */
enum {
	MLINK_USB_DIAG_SERIAL_BRIDGE = 1,	/* known/likely USB-serial bridge chip */
	MLINK_USB_DIAG_NOT_A_UPS_HID = 2	/* HID device, but neither our tunnel nor HID-PDC */
};

#define MLINK_USB_DIAG_WARNED_MAX	6U
static uint64_t mlink_usb_diag_warned[MLINK_USB_DIAG_WARNED_MAX];
static size_t mlink_usb_diag_warned_count = 0;

/* Returns nonzero the first time this (kind, vendorid, productid) is seen this
 * run, zero on every repeat. */
static int microlink_usb_diag_warn_once(int kind, uint16_t vendorid, uint16_t productid)
{
	uint64_t key = ((uint64_t)(unsigned int)kind << 32) | ((uint32_t)vendorid << 16) | productid;
	size_t i;

	for (i = 0; i < mlink_usb_diag_warned_count; i++) {
		if (mlink_usb_diag_warned[i] == key) {
			return 0;
		}
	}

	if (mlink_usb_diag_warned_count < MLINK_USB_DIAG_WARNED_MAX) {
		mlink_usb_diag_warned[mlink_usb_diag_warned_count++] = key;
	}
	return 1;
}

static void microlink_usb_warn_serial_bridge_once(const USBDevice_t *device, const char *desc)
{
	if (!microlink_usb_diag_warn_once(MLINK_USB_DIAG_SERIAL_BRIDGE,
		device->VendorID, device->ProductID)
	) {
		return;
	}

	upslogx(LOG_WARNING,
		"microlink_usb: USB device %04x:%04x (%s) matched your USB "
		"port/vendorid/productid settings, but looks like a generic "
		"USB-to-serial adapter, not a Microlink-over-USB-HID device. "
		"If your UPS has a serial Microlink port wired to this adapter, "
		"configure this driver in serial mode instead: point \"port\" "
		"at the /dev/ttyUSB*, /dev/ttyACM* or COM* device this adapter "
		"created, and remove vendorid/productid/port=auto.",
		device->VendorID, device->ProductID, desc);
}

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
		{
			const char *bridge_desc = microlink_usb_describe_serial_bridge(device);
			if (bridge_desc != NULL) {
				microlink_usb_warn_serial_bridge_once(device, bridge_desc);
			}
		}
		return 0;
	}
}

static USBDeviceMatcher_t microlink_usb_device_matcher = {
	&microlink_usb_match,
	NULL,
	NULL
};

/* Called by comm_driver->open_dev() with a candidate device's HID report
 * descriptor: pick out the Report IDs behind our two vendor-page usages, as
 * apc_modbus.c's _apc_modbus_usb_callback() does for its own pair. */
static int microlink_usb_report_callback(usb_dev_handle *arg_udev, USBDevice_t *hd,
	usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen)
{
	HIDDesc_t *hid_desc;
	size_t i;

	NUT_UNUSED_VARIABLE(arg_udev);

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

		/* Only the autonomous Input variant is useful here - the duplicate
		 * Feature entries would need a GET_REPORT we never send. */
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
		int has_hid_pdc = (ff_ac_present.report_id != 0 && ff_discharging.report_id != 0);

		upsdebugx(1, "microlink_usb: Microlink USB HID tunnel (vendor page 0xFF86, "
			"usages 0xFC/0xFD) not found on this device");

		/* Neither our vendor tunnel nor standard HID Power Device usages:
		 * not a supported UPS HID interface at all. More reliable than
		 * matching an inevitably incomplete VID/PID list. */
		if (!has_hid_pdc
		&&  microlink_usb_diag_warn_once(MLINK_USB_DIAG_NOT_A_UPS_HID,
			hd->VendorID, hd->ProductID)
		) {
			upslogx(LOG_WARNING,
				"microlink_usb: USB device %04x:%04x (%s) exposes a HID "
				"interface but has neither the Microlink vendor tunnel nor "
				"standard HID Power Device usages - this does not look "
				"like a supported UPS HID interface at all (untested, "
				"probably-wrong-driver territory). It may be an unrelated "
				"HID device, or a USB-to-serial adapter's incidental HID "
				"interface. If your UPS has a serial Microlink port wired "
				"through a USB-to-serial adapter, configure this driver "
				"in serial mode instead: point \"port\" at the adapter's "
				"/dev/ttyUSB*, /dev/ttyACM* or COM* device, and remove "
				"vendorid/productid/port=auto.",
				hd->VendorID, hd->ProductID,
				hd->Product ? hd->Product : "unknown product");
		}

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

#if WITH_LIBUSB_0_1
	{
		/* Not validated for this driver: on at least one tested system the
		 * installed libusb-0.1's interrupt-IN read did not honor its timeout,
		 * hanging the driver instead of timing out and retrying. Log once per
		 * process, not on every reconnect. */
		static int warned_libusb01 = 0;

		if (!warned_libusb01) {
			warned_libusb01 = 1;
			upslogx(LOG_WARNING,
				"apcmicrolink: built against libusb-0.1 - this transport is "
				"untested for this driver and has shown a real interrupt-IN "
				"read timeout defect on at least one system, causing the "
				"driver to hang rather than retry. libusb-1.0 is the "
				"tested/supported USB backend; if this driver becomes "
				"unresponsive, rebuild NUT with --with-usb=libusb-1.0 "
				"before investigating further.");
		}
	}
#endif /* WITH_LIBUSB_0_1 */

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
	usb_device_gone = 0;

	dstate_setinfo("ups.vendorid", "%04x", curDevice.VendorID);
	dstate_setinfo("ups.productid", "%04x", curDevice.ProductID);

	upsdebugx(1, "microlink_usb: opened %s/%s (USB %04x:%04x)",
		curDevice.Vendor ? curDevice.Vendor : "unknown",
		curDevice.Product ? curDevice.Product : "unknown",
		curDevice.VendorID, curDevice.ProductID);

	in_report_len = 0;
	in_report_pos = 0;

#if WITH_LIBUSB_1_0 && defined(HAVE_PTHREAD)
	if (!microlink_usb_async_start()) {
		upsdebugx(1, "microlink_usb: continuous async listener unavailable, "
			"falling back to per-call synchronous interrupt-IN reads");
	}
#endif /* WITH_LIBUSB_1_0 && HAVE_PTHREAD */

	return 1;
}

void microlink_usb_close(void)
{
#if WITH_LIBUSB_1_0 && defined(HAVE_PTHREAD)
	microlink_usb_async_stop();
#endif /* WITH_LIBUSB_1_0 && HAVE_PTHREAD */

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

#if WITH_LIBUSB_1_0 && defined(HAVE_PTHREAD)
	/* Cancel the outstanding async transfer before resetting and closing
	 * the handle it is submitted against. */
	microlink_usb_async_stop();
#endif /* WITH_LIBUSB_1_0 && HAVE_PTHREAD */

	if (udev) {
		/* Send USB bus reset. The handle is invalid afterward regardless of
		 * the return value - close it unconditionally. usb_reset() is NUT's
		 * usb-common.h abstraction; calling libusb_reset_device() directly
		 * breaks NUT_USB_VARIANT=0.1 builds. */
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
	usb_device_gone = 0;

	upsdebugx(1, "microlink_usb: reset and reopened %s/%s (USB %04x:%04x)",
		curDevice.Vendor ? curDevice.Vendor : "unknown",
		curDevice.Product ? curDevice.Product : "unknown",
		curDevice.VendorID, curDevice.ProductID);

#if WITH_LIBUSB_1_0 && defined(HAVE_PTHREAD)
	if (!microlink_usb_async_start()) {
		upsdebugx(1, "microlink_usb: continuous async listener unavailable after "
			"reset, falling back to per-call synchronous interrupt-IN reads");
	}
#endif /* WITH_LIBUSB_1_0 && HAVE_PTHREAD */

	return 1;
}

int microlink_usb_device_gone(void)
{
	return usb_device_gone;
}

/* A timeout of 0 to libusb_interrupt_transfer() means "wait forever", not
 * "don't block", and this device's interrupt pipe can be genuinely empty for a
 * while - so use a short real timeout, and cap the reports drained. */
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

#if WITH_LIBUSB_1_0 && defined(HAVE_PTHREAD)
	if (async_xfer_active) {
		/* The async listener owns the interrupt-IN endpoint, and its pump
		 * thread already services the event loop continuously, so just drop
		 * whatever is queued. This function must never call
		 * libusb_handle_events*() itself - that would make it a second caller
		 * of the event loop.
		 *
		 * This discards unread genuine tunnel replies too, so it is only
		 * correct for clearing stale data after a hard reset - which
		 * microlink_usb_async_stop() already handles independently. No caller
		 * remains; the discard logging is kept for any future one. */
		{
			unsigned int discarded_count;
			unsigned int discarded_tunnel_reports = 0;

			pthread_mutex_lock(&async_lock);
			discarded_count = async_queue_count;
			if (mlink_report_in != 0) {
				unsigned int i;

				for (i = 0; i < async_queue_count; i++) {
					unsigned int slot = (async_queue_head + i) % MLINK_USB_ASYNC_QUEUE_LEN;

					if (async_queue[slot].len > 0
					&&  (int)async_queue[slot].data[0] == mlink_report_in
					) {
						discarded_tunnel_reports++;
					}
				}
			}
			async_queue_head = 0;
			async_queue_count = 0;
			pthread_mutex_unlock(&async_lock);

			if (discarded_count > 0) {
				upsdebugx(discarded_tunnel_reports > 0 ? 1 : 3,
					"microlink_usb: flushing %u queued async report(s) "
					"(%u were our tunnel's Input report 0x%02X) before a "
					"fresh session attempt",
					discarded_count, discarded_tunnel_reports,
					(unsigned int)mlink_report_in);
			}
		}
		return;
	}
#endif /* WITH_LIBUSB_1_0 && HAVE_PTHREAD */

	for (drained = 0; drained < MLINK_USB_FLUSH_MAX_REPORTS; drained++) {
		ret = comm_driver->get_interrupt(udev, (usb_ctrl_charbuf)discard,
			(usb_ctrl_charbufsize)sizeof(discard),
			(usb_ctrl_timeout_msec)MLINK_USB_FLUSH_TIMEOUT_MS);
		if (ret <= 0) {
			break;
		}
	}
}

/* The kernel's own generic "usbhid" driver can reclaim this device's interface
 * out from under an already-claimed libusb handle - observed live, apparently
 * following a USB reset this driver has no visibility into. Every submission
 * then fails with LIBUSB_ERROR_BUSY until the process is restarted, and
 * because that fails near-instantly, nothing above this file paces the
 * retries: one observed incident spun at ~14,000 failed submissions/sec for
 * 30+ minutes, saturating a CPU core and overwriting the kernel ring buffer
 * before the triggering event could be identified.
 *
 * A udev rule unbinding usbhid from this VID:PID avoids this in practice; this
 * handler is defense in depth for that rule being absent - back off hard
 * instead of spinning, and log loudly but rate-limited. */
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

	/* This tunnel genuinely requires interrupt-OUT: a HID SET_REPORT control
	 * transfer to the same Report ID succeeds at the USB level, but the device
	 * never reacts to it - no reply ever appears on the Input report channel -
	 * while an interrupt-OUT write on the Output endpoint gets an immediate,
	 * correctly framed, checksum-valid response stream. */
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

	if (ret == LIBUSB_ERROR_NO_DEVICE) {
		usb_device_gone = 1;
	}

	if (ret < 0) {
		upsdebugx(1, "microlink_usb: interrupt-OUT write (Output 0x%02X) failed: %s",
			(unsigned int)mlink_report_out, nut_usb_strerror(ret));
		return 0;
	}

	return 1;
}

/* Extract a little-endian, LSB-first-packed bit field from a report's data
 * (everything after the leading Report ID byte), matching how
 * Parse_ReportDesc()'s Offset/Size describe fields. Only used for fields of at
 * most 32 bits here. */
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

/* Decode the standard HID PDC fallback fields if this report happens to be one
 * of them - called for every Input report seen, since these arrive
 * autonomously and independently of tunnel health. */
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
		fb_last_update = microlink_now();
	}
}

#if WITH_LIBUSB_1_0 && defined(HAVE_PTHREAD)
/* Keeps libusb's event loop serviced so async_xfer is reaped and resubmitted
 * promptly, independent of what the main thread is doing. Nothing else may
 * call libusb_handle_events*() on this context while this thread runs. */
static void *microlink_usb_async_pump(void *arg)
{
	NUT_UNUSED_VARIABLE(arg);

	while (!async_pump_stop) {
		struct timeval tv;

		tv.tv_sec = 0;
		tv.tv_usec = 50000; /* 50ms - bounds how fast a stop request is noticed */
		libusb_handle_events_timeout_completed(nut_libusb_get_context(), &tv, NULL);
	}

	return NULL;
}

/* Runs on the pump thread from inside libusb_handle_events*(). Queues the
 * completed report and immediately resubmits the same transfer, so exactly one
 * interrupt-IN request stays outstanding at all times.
 *
 * transfer->user_data doubles as an "abandoned" marker (see
 * microlink_usb_async_stop()): libusb forbids freeing a transfer before its
 * callback has fired, so an abandoned one frees itself here instead of ever
 * resubmitting. */
static void LIBUSB_CALL microlink_usb_async_cb(struct libusb_transfer *transfer)
{
	int abandoned = (transfer->user_data != NULL);

	if (!abandoned && transfer->status == LIBUSB_TRANSFER_COMPLETED && transfer->actual_length > 0) {
		size_t copy_len = (size_t)transfer->actual_length;

		if (copy_len > MLINK_USB_REPORT_TOTAL_LEN) {
			copy_len = MLINK_USB_REPORT_TOTAL_LEN;
		}

		upsdebugx(4, "microlink_usb: async transfer completed: report 0x%02X, "
			"%u bytes",
			copy_len > 0 ? (unsigned int)transfer->buffer[0] : 0U,
			(unsigned int)copy_len);

		/* Decoded on receipt whether or not this also turns out to be our own
		 * tunnel reply; the fb_* globals are read by the main thread in
		 * microlink_usb_get_hid_fallback(), hence the lock. */
		pthread_mutex_lock(&async_lock);

		microlink_usb_try_decode_fallback(transfer->buffer, copy_len);

		if (async_queue_count < MLINK_USB_ASYNC_QUEUE_LEN) {
			unsigned int slot = (async_queue_head + async_queue_count) % MLINK_USB_ASYNC_QUEUE_LEN;

			memcpy(async_queue[slot].data, transfer->buffer, copy_len);
			async_queue[slot].len = copy_len;
			async_queue_count++;
			pthread_cond_signal(&async_cond);
		} else {
			upsdebugx(3, "microlink_usb: async read queue full (%u), dropping "
				"incoming report", MLINK_USB_ASYNC_QUEUE_LEN);
		}

		pthread_mutex_unlock(&async_lock);
	}

	if (abandoned) {
		free(transfer->buffer);
		libusb_free_transfer(transfer);
		return;
	}

	switch (transfer->status) {
	case LIBUSB_TRANSFER_CANCELLED:
	case LIBUSB_TRANSFER_NO_DEVICE:
		/* Teardown in progress, or the device is gone - don't resubmit;
		 * microlink_usb_async_stop() frees this transfer once it observes
		 * async_xfer_active go to 0. */
		if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
			usb_device_gone = 1;
		}
		pthread_mutex_lock(&async_lock);
		async_xfer_active = 0;
		pthread_mutex_unlock(&async_lock);
		return;
	case LIBUSB_TRANSFER_COMPLETED:
	case LIBUSB_TRANSFER_ERROR:
	case LIBUSB_TRANSFER_TIMED_OUT:
	case LIBUSB_TRANSFER_STALL:
	case LIBUSB_TRANSFER_OVERFLOW:
	default:
		/* Anything else - resubmit below and keep listening. */
		break;
	}

	if (libusb_submit_transfer(transfer) != 0) {
		upsdebugx(1, "microlink_usb: failed to resubmit async interrupt-IN "
			"transfer, continuous listener stopped");
		pthread_mutex_lock(&async_lock);
		async_xfer_active = 0;
		pthread_mutex_unlock(&async_lock);
	}
}

/* Start the listener and its pump thread. Returns 1 if running (or already
 * was), 0 if it could not be started - callers fall back to per-call
 * synchronous reads then, so this is never fatal. */
static int microlink_usb_async_start(void)
{
	int ep_in;
	unsigned char *buf;

	if (!udev) {
		return 0;
	}
	if (async_xfer_active) {
		return 1;
	}

	ep_in = USB_ENDPOINT_IN + usb_subdriver.hid_ep_in;

	/* Heap-allocated per instance - see async_xfer's declaration. */
	buf = xmalloc(MLINK_USB_REPORT_TOTAL_LEN);

	async_xfer = libusb_alloc_transfer(0);
	if (!async_xfer) {
		upsdebugx(1, "microlink_usb: libusb_alloc_transfer failed for the "
			"async listener");
		free(buf);
		return 0;
	}

	/* Timeout 0 on an async transfer means "no timeout on this submission",
	 * unlike the synchronous API - exactly what we want here: stay outstanding
	 * until data arrives or it is cancelled. user_data starts NULL ("owned");
	 * microlink_usb_async_stop() sets it to mark the transfer abandoned. */
	libusb_fill_interrupt_transfer(async_xfer, udev, ep_in,
		buf, (int)MLINK_USB_REPORT_TOTAL_LEN,
		microlink_usb_async_cb, NULL, 0);

	if (libusb_submit_transfer(async_xfer) != 0) {
		upsdebugx(1, "microlink_usb: failed to submit the initial async "
			"interrupt-IN transfer");
		libusb_free_transfer(async_xfer);
		free(buf);
		async_xfer = NULL;
		return 0;
	}

	async_queue_head = 0;
	async_queue_count = 0;
	async_xfer_active = 1;

	async_pump_stop = 0;
	if (pthread_create(&async_pump_tid, NULL, microlink_usb_async_pump, NULL) != 0) {
		upsdebugx(1, "microlink_usb: failed to start the async pump thread, "
			"abandoning the just-submitted transfer and falling back to "
			"synchronous reads");
		/* Nothing will service the event loop until some later synchronous
		 * call incidentally does, so request cancellation now and then walk
		 * away exactly like the timeout path in microlink_usb_async_stop()
		 * below. */
		libusb_cancel_transfer(async_xfer);
		async_xfer->user_data = (void *)1;
		async_xfer = NULL;
		async_xfer_active = 0;
		async_queue_head = 0;
		async_queue_count = 0;
		return 0;
	}

	upsdebugx(2, "microlink_usb: continuous async interrupt-IN listener started");
	return 1;
}

/* Stop the pump thread first; once it has exited, this function is again the
 * only thing that might touch libusb's event loop.
 *
 * Freeing a transfer libusb has not yet reported as complete or cancelled is
 * undefined behavior (observed live as a "usbi_mutex_lock" assertion crash
 * during a USB reset), so cancel it and wait, bounded, for confirmation. If
 * the bound is hit - realistically only when the device is already too wedged
 * to answer a cancel, i.e. exactly what leads here - mark the transfer
 * abandoned via user_data and let its callback free it whenever libusb
 * eventually completes it. Never touch it again after that. */
static void microlink_usb_async_stop(void)
{
	int i;

	if (!async_xfer) {
		return;
	}

	async_pump_stop = 1;
	pthread_join(async_pump_tid, NULL);

	if (async_xfer_active) {
		libusb_cancel_transfer(async_xfer);

		for (i = 0; i < 50 && async_xfer_active; i++) {
			struct timeval tv;

			tv.tv_sec = 0;
			tv.tv_usec = 20000;
			libusb_handle_events_timeout_completed(nut_libusb_get_context(), &tv, NULL);
		}

		if (async_xfer_active) {
			upsdebugx(1, "microlink_usb: async transfer did not confirm "
				"cancellation in time, abandoning it (it will free itself "
				"once libusb eventually completes it)");
			async_xfer->user_data = (void *)1;
			async_xfer = NULL;
			async_xfer_active = 0;
			async_queue_head = 0;
			async_queue_count = 0;
			return;
		}
	}

	free(async_xfer->buffer);
	libusb_free_transfer(async_xfer);
	async_xfer = NULL;
	async_queue_head = 0;
	async_queue_count = 0;
}
#endif /* WITH_LIBUSB_1_0 && HAVE_PTHREAD */

int microlink_usb_get_hid_fallback(int max_age_sec,
	int *ac_present, int *discharging, int *below_rcl,
	long *battery_charge, long *battery_runtime)
{
	if (ff_ac_present.report_id == 0 || ff_discharging.report_id == 0) {
		return 0;
	}

#if WITH_LIBUSB_1_0 && defined(HAVE_PTHREAD)
	pthread_mutex_lock(&async_lock);
#endif /* WITH_LIBUSB_1_0 && HAVE_PTHREAD */

	if (fb_last_update == 0 || max_age_sec < 0
	 || difftime(microlink_now(), fb_last_update) > (double)max_age_sec) {
#if WITH_LIBUSB_1_0 && defined(HAVE_PTHREAD)
		pthread_mutex_unlock(&async_lock);
#endif /* WITH_LIBUSB_1_0 && HAVE_PTHREAD */
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

#if WITH_LIBUSB_1_0 && defined(HAVE_PTHREAD)
	pthread_mutex_unlock(&async_lock);
#endif /* WITH_LIBUSB_1_0 && HAVE_PTHREAD */

	return 1;
}

int microlink_usb_hid_fallback_supported(void)
{
	return (ff_ac_present.report_id != 0 && ff_discharging.report_id != 0);
}

/* Per-call synchronous read: issue one interrupt-IN transfer and wait up to
 * d_usec for it. Used as-is on non-libusb-1.0 builds, and as the fallback when
 * the async listener could not be started. */
static int microlink_usb_get_char_sync(unsigned char *ch, long d_usec)
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

	if (ret == LIBUSB_ERROR_NO_DEVICE) {
		usb_device_gone = 1;
	}

	if (ret < 0) {
		in_report_len = 0;
		in_report_pos = 0;
		return -1;
	}

	/* The interrupt-IN endpoint also carries other Input reports this device
	 * pushes - ignore anything that is not our tunnel's rather than feeding
	 * foreign bytes into the Microlink stream. Decode the HID PDC fallback
	 * fields first, though: those keep arriving whether or not the tunnel
	 * itself is responding. */
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

#if WITH_LIBUSB_1_0 && defined(HAVE_PTHREAD)
/* Mirrors ser_get_char()'s contract, same as the synchronous version above: 1
 * with *ch filled, 0 on timeout or a non-matching report, negative on hard
 * error - the caller (microlink_receive_once()) already loops on 0. The
 * difference is where the wait happens: on async_cond, for the pump thread to
 * deliver a report into the listener's queue, including one that completed
 * before this call started (e.g. during the caller's sleep between retries).
 * This function must never call libusb_handle_events*() itself. */
int microlink_usb_get_char(unsigned char *ch, long d_usec)
{
	struct timespec deadline;
	microlink_async_report_t slot;
	size_t copy_len;

	if (!async_xfer_active) {
		return microlink_usb_get_char_sync(ch, d_usec);
	}

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

	/* pthread_cond_timedwait() takes an absolute deadline on the system
	 * (CLOCK_REALTIME) clock by default - no portable way to request
	 * CLOCK_MONOTONIC here without pthread_condattr_setclock(), a POSIX
	 * extension not available everywhere this codebase targets. */
	clock_gettime(CLOCK_REALTIME, &deadline);
	deadline.tv_sec += d_usec / 1000000L;
	deadline.tv_nsec += (d_usec % 1000000L) * 1000L;
	if (deadline.tv_nsec >= 1000000000L) {
		deadline.tv_sec++;
		deadline.tv_nsec -= 1000000000L;
	}

	pthread_mutex_lock(&async_lock);

	while (async_queue_count == 0) {
		if (pthread_cond_timedwait(&async_cond, &async_lock, &deadline) != 0) {
			/* Timed out (or some other wait error) - no report showed up
			 * within the caller's budget. */
			pthread_mutex_unlock(&async_lock);
			return 0;
		}
	}

	slot = async_queue[async_queue_head];
	async_queue_head = (async_queue_head + 1) % MLINK_USB_ASYNC_QUEUE_LEN;
	async_queue_count--;

	pthread_mutex_unlock(&async_lock);

	copy_len = (slot.len > sizeof(in_report)) ? sizeof(in_report) : slot.len;
	memcpy(in_report, slot.data, copy_len);

	if (copy_len < 2 || (int)in_report[0] != mlink_report_in) {
		upsdebugx(4, "microlink_usb: discarding queued report 0x%02X "
			"(%u bytes, want 0x%02X) - not our tunnel's Input report",
			copy_len > 0 ? (unsigned int)in_report[0] : 0U,
			(unsigned int)copy_len, (unsigned int)mlink_report_in);
		in_report_len = 0;
		in_report_pos = 0;
		return 0;
	}

	in_report_len = copy_len;
	in_report_pos = 1; /* skip the leading Report ID byte */

	*ch = in_report[in_report_pos++];
	return 1;
}
#else /* !(WITH_LIBUSB_1_0 && HAVE_PTHREAD) */
int microlink_usb_get_char(unsigned char *ch, long d_usec)
{
	return microlink_usb_get_char_sync(ch, d_usec);
}
#endif /* WITH_LIBUSB_1_0 && HAVE_PTHREAD */
