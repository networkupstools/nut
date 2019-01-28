/*
 * nutdrv_atcl_usb.c - driver for generic-brand "ATCL FOR UPS"
 *
 * Copyright (C) 2013-2014 Charles Lepple <clepple+nut@gmail.com>
 * Copyright (C) 2016      Eaton
 *
 * Loosely based on richcomm_usb.c,
 * Copyright (C) 2007 Peter van Valderen <p.v.valderen@probu.nl>
 *                    Dirk Teurlings <dirk@upexia.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "main.h"
#include "bool.h"
#include "nut_libusb.h"

/* driver version */
#define DRIVER_NAME	"'ATCL FOR UPS' USB driver"
#define DRIVER_VERSION	"1.30"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Charles Lepple <clepple+nut@gmail.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define STATUS_ENDPOINT		(LIBUSB_ENDPOINT_IN  | 1)
#define SHUTDOWN_ENDPOINT	(LIBUSB_ENDPOINT_OUT | 2)
#define STATUS_PACKETSIZE	8
#define SHUTDOWN_PACKETSIZE	8

/* Probably can reduce this, since the pcap file shows mostly 1050-ish ms response times */
#define ATCL_USB_TIMEOUT USB_TIMEOUT

/* limit the amount of spew that goes in the syslog when we lose the UPS (from nut_usb.h) */
#define USB_ERR_LIMIT	10	/* start limiting after 10 in a row */
#define USB_ERR_RATE	10	/* then only print every 10th error */

#define USB_VENDOR_STRING "ATCL FOR UPS"

static usb_device_id_t atcl_usb_id[] = {
	/* ATCL FOR UPS */
	{ USB_DEVICE(0x0001, 0x0000),  NULL },

	/* end of list */
	{-1, -1, NULL}
};

static libusb_device_handle	*udev = NULL;
static USBDevice_t		usbdevice;
static unsigned int		comm_failures = 0;

static int device_match_func(USBDevice_t *device, void *privdata)
{
	char *requested_vendor;
	switch (is_usb_device_supported(atcl_usb_id, device))
	{
	case SUPPORTED:
		if(!device->Vendor) { 
			upsdebugx(1, "Couldn't retrieve USB string descriptor for vendor. Check permissions?");
			requested_vendor = getval("vendor");
			if(requested_vendor) {
				if(!strcmp("NULL", requested_vendor)) {
					upsdebugx(3, "Matched device with NULL vendor string.");
					return 1;
				}
			}	
			upsdebugx(1, "To keep trying (in case your device does not have a vendor string), use vendor=NULL");
			return 0;
		}

		if(!strcmp(device->Vendor, USB_VENDOR_STRING)) {
			upsdebugx(4, "Matched expected vendor='%s'.", USB_VENDOR_STRING);
			return 1;
		}
		/* Didn't match, but the user provided an alternate vendor ID: */
		requested_vendor = getval("vendor");
		if(requested_vendor) {
			if(!strcmp(device->Vendor, requested_vendor)) {
				upsdebugx(3, "Matched device with vendor='%s'.", requested_vendor);
				return 1;
			} else {
				upsdebugx(2, "idVendor=%04x and idProduct=%04x, but provided vendor '%s' does not match device: '%s'.",
					device->VendorID, device->ProductID, requested_vendor, device->Vendor);
				return 0;
			}
		}

		/* TODO: automatic way of suggesting other drivers? */
		upsdebugx(2, "idVendor=%04x and idProduct=%04x, but device vendor string '%s' does not match expected string '%s'. "
				"Have you tried the nutdrv_qx driver?",
				device->VendorID, device->ProductID, device->Vendor, USB_VENDOR_STRING);
		return 0;

	case POSSIBLY_SUPPORTED:
	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

static USBDeviceMatcher_t device_matcher = {
	&device_match_func,
	NULL,
	NULL
};

/** @brief Query the device for status, read its reply and store it in *reply*.
 * @return 0, on failure,
 * @return the number of bytes read, on success. */
static int query_ups(char *reply)
{
	int	ret, transferred;

	ret = libusb_interrupt_transfer(udev, STATUS_ENDPOINT, (unsigned char *)reply, STATUS_PACKETSIZE, &transferred, ATCL_USB_TIMEOUT);

	if (ret == LIBUSB_ERROR_NO_MEM)
		fatalx(EXIT_FAILURE, "Out of memory.");

	if (ret != LIBUSB_SUCCESS || transferred == 0) {
		upsdebugx(2, "status interrupt read: %s", ret ? libusb_strerror(ret) : "timeout");
		return 0;
	}

	upsdebug_hex(3, "read", reply, transferred);
	return transferred;
}

static void usb_comm_fail(const char *fmt, ...)
{
	int	ret;
	char	why[SMALLBUF];
	va_list	ap;

	/* this means we're probably here because select was interrupted */
	if (exit_flag != 0) {
		return;	 /* ignored, since we're about to exit anyway */
	}

	comm_failures++;

	if ((comm_failures == USB_ERR_LIMIT) || ((comm_failures % USB_ERR_RATE) == 0)) {
		upslogx(LOG_WARNING, "Warning: excessive comm failures, limiting error reporting");
	}

	/* once it's past the limit, only log once every USB_ERR_LIMIT calls */
	if ((comm_failures > USB_ERR_LIMIT) && ((comm_failures % USB_ERR_LIMIT) != 0)) {
		return;
	}

	/* generic message if the caller hasn't elaborated */
	if (!fmt) {
		upslogx(LOG_WARNING, "Communications with UPS lost - check cabling");
		return;
	}

	va_start(ap, fmt);
	ret = vsnprintf(why, sizeof(why), fmt, ap);
	va_end(ap);

	if ((ret < 1) || (ret >= (int) sizeof(why))) {
		upslogx(LOG_WARNING, "usb_comm_fail: vsnprintf needed more than %d bytes", (int)sizeof(why));
	}

	upslogx(LOG_WARNING, "Communications with UPS lost: %s", why);
}

static void usb_comm_good(void)
{
	if (comm_failures == 0) {
		return;
	}

	upslogx(LOG_NOTICE, "Communications with UPS re-established");	
	comm_failures = 0;
}

/** @brief Try to open a USB device matching @ref device_matcher.
 *
 * If @ref udev refers to an already opened device (i.e. it is not `NULL`), it is closed before attempting the reopening.
 *
 * @return @ref TRUE, with @ref udev being the handle of the opened device, and with @ref usbdevice filled, on success,
 * @return @ref FALSE, with @ref udev being `NULL`, on failure. */
static bool_t	open_device(void)
{
	int	ret;

	ret = usb_subdriver.open(&udev, &usbdevice, &device_matcher, 1, NULL);
	if (ret != LIBUSB_SUCCESS)
		return FALSE;

	/* TODO: HID SET_IDLE to 0 (not necessary?) */

	return TRUE;
}

/*
 * Initialise the UPS
 */
void upsdrv_initups(void)
{
	int	i;

	upsdebugx(1, "Searching for USB device...");

	/* Initialise the communication subdriver */
	usb_subdriver.init();

	/* Try to open the device */

	for (i = 0; !open_device(); i++) {

		if ((i < 3) && (sleep(5) == 0)) {
			usb_comm_fail("Can't open USB device, retrying ...");
			continue;
		}

		fatalx(EXIT_FAILURE,
			"Unable to find ATCL FOR UPS\n\n"

			"Things to try:\n"
			" - Connect UPS device to USB bus\n"
			" - Run this driver as another user (upsdrvctl -u or 'user=...' in ups.conf).\n"
			"   See upsdrvctl(8) and ups.conf(5).\n\n"

			"Fatal error: unusable configuration");
	}

}

void upsdrv_cleanup(void)
{
	usb_subdriver.close(udev);
	usb_subdriver.deinit();

	free(usbdevice.Vendor);
	free(usbdevice.Product);
	free(usbdevice.Serial);
	free(usbdevice.Bus);
}

void upsdrv_initinfo(void)
{
	dstate_setinfo("ups.mfr", "%s", usbdevice.Vendor ? usbdevice.Vendor : "unknown");
	dstate_setinfo("ups.model", "%s", usbdevice.Product ? usbdevice.Product : "unknown");
	if(usbdevice.Serial && usbdevice.Product && strcmp(usbdevice.Serial, usbdevice.Product)) {
		/* Only set "ups.serial" if it isn't the same as "ups.model": */
		dstate_setinfo("ups.serial", "%s", usbdevice.Serial);
	}

	dstate_setinfo("ups.vendorid", "%04x", usbdevice.VendorID);
	dstate_setinfo("ups.productid", "%04x", usbdevice.ProductID);
}

void upsdrv_updateinfo(void)
{
	char	reply[STATUS_PACKETSIZE];
	int	ret;

	if (!udev && !open_device())
		return;

	ret = query_ups(reply);

	if (ret != STATUS_PACKETSIZE) {
		usb_comm_fail("Query to UPS failed");
		dstate_datastale();

		usb_subdriver.close(udev);
		udev = NULL;

		return;
	}

	usb_comm_good();
	dstate_dataok();

	status_init();

	switch(reply[0]) {
		case 3:
			upsdebugx(2, "reply[0] = 0x%02x -> OL", reply[0]);
			status_set("OL");
			break;
		case 2:
			upsdebugx(2, "reply[0] = 0x%02x -> LB", reply[0]);
			status_set("LB");
			/* fall through */
		case 1:
			upsdebugx(2, "reply[0] = 0x%02x -> OB", reply[0]);
			status_set("OB");
			break;
		default:
			upslogx(LOG_ERR, "Unknown status: 0x%02x", reply[0]);
	}
	if(strnlen(reply + 1, 7) != 0) {
		upslogx(LOG_NOTICE, "Status bytes 1-7 are not all zero");
	}

	status_commit();
}

/* If the UPS is on battery, it should shut down about 30 seconds after
 * receiving this packet.
 */
void upsdrv_shutdown(void)
{
	const char	shutdown_packet[SHUTDOWN_PACKETSIZE] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	int		i, ret, transferred;

	upslogx(LOG_DEBUG, "%s: attempting to call libusb_interrupt_transfer(01 00 00 00 00 00 00 00)", __func__);

	for (i = 1; i < 3; i++) {

		ret = libusb_interrupt_transfer(udev, SHUTDOWN_ENDPOINT, (unsigned char *)shutdown_packet, SHUTDOWN_PACKETSIZE, &transferred, ATCL_USB_TIMEOUT);

		if (ret == LIBUSB_ERROR_NO_MEM)
			fatalx(EXIT_FAILURE, "Out of memory.");

		if (ret != LIBUSB_SUCCESS || transferred == 0)
			upslogx(i == 1 ? LOG_NOTICE : LOG_ERR, "%s: libusb_interrupt_transfer() #%d failed: %s", __func__, i, ret ? libusb_strerror(ret) : "timeout");

		/* Totally guessing from the .pcap file here. TODO: configurable delay? */
		if (i == 1)
			usleep(170*1000);

	}

}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
        addvar(VAR_VALUE, "vendor", "USB vendor string (or NULL if none)");
}
