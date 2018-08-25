/*
 * richcomm_usb.c - driver for UPS with Richcomm dry-contact to USB
 *                  solution, such as 'Sweex Manageable UPS 1000VA'
 *
 * May also work on 'Kebo UPS-650D', not tested as of 05/23/2007
 *
 * Copyright (C) 2007 Peter van Valderen <p.v.valderen@probu.nl>
 *                    Dirk Teurlings <dirk@upexia.nl>
 * Copyright (C) 2016 Eaton
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
#include "usb-common.h"
#include "str.h"

/* driver version */
#define DRIVER_NAME	"Richcomm dry-contact to USB driver"
#define DRIVER_VERSION	"0.22"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Peter van Valderen <p.v.valderen@probu.nl>\n"
	"Dirk Teurlings <dirk@upexia.nl>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

/** @brief Whether libusb has been successfully initialised or not. */
static bool_t	inited_libusb = FALSE;

#define STATUS_REQUESTTYPE	(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE)
#define REPLY_ENDPOINT		(LIBUSB_ENDPOINT_IN | 1)
#define QUERY_PACKETSIZE	4
#define REPLY_PACKETSIZE	6
#define REQUEST_VALUE		0x09
#define MESSAGE_VALUE		0x200
#define INDEX_VALUE		0

/* limit the amount of spew that goes in the syslog when we lose the UPS (from nut_usb.h) */
#define USB_ERR_LIMIT	10	/* start limiting after 10 in a row */
#define USB_ERR_RATE	10	/* then only print every 10th error */

static usb_device_id_t richcomm_usb_id[] = {
	/* Sweex 1000VA */
	{ USB_DEVICE(0x0925, 0x1234),  NULL },

	/* end of list */
	{-1, -1, NULL}
};

static libusb_device_handle	*udev = NULL;
static USBDevice_t		usbdevice;
static unsigned int		comm_failures = 0;

static int device_match_func(USBDevice_t *device, void *privdata)
{
	switch (is_usb_device_supported(richcomm_usb_id, device))
	{
	case SUPPORTED:
		return 1;

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

/** @brief Send the command stored in *query*, read the reply and store it in *reply*.
 * @return 0, on failure,
 * @return the number of bytes read, on success. */
static int execute_and_retrieve_query(char *query, char *reply)
{
	int	ret, transferred;

	ret = libusb_control_transfer(
		udev,
		STATUS_REQUESTTYPE,
		REQUEST_VALUE,
		MESSAGE_VALUE,
		INDEX_VALUE,
		(unsigned char *)query,
		QUERY_PACKETSIZE,
		1000
	);

	if (ret <= 0) {
		upsdebugx(3, "send: %s", ret ? libusb_strerror(ret) : "timeout");
		return 0;
	}

	upsdebug_hex(3, "send", query, ret);

	ret = libusb_interrupt_transfer(udev, REPLY_ENDPOINT, (unsigned char *)reply, REPLY_PACKETSIZE, &transferred, 1000);

	if (ret != LIBUSB_SUCCESS || transferred == 0) {
		upsdebugx(3, "read: %s", ret ? libusb_strerror(ret) : "timeout");
		return 0;
	}

	upsdebug_hex(3, "read", reply, transferred);
	return transferred;
}

/** @brief Prepare the status request query and call execute_and_retrieve_query() with it and *reply*.
 * @return see execute_and_retrieve_query(). */
static int query_ups(char *reply)
{
	/*
	 * This packet is a status request to the UPS
	 */
	char	query[QUERY_PACKETSIZE] = { 0x01, 0x00, 0x00, 0x30 };

	return execute_and_retrieve_query(query, reply);
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

/*
 * Callback that is called by usb_device_open() that handles USB device
 * settings prior to accepting the devide. At the very least claim the
 * device here. Detaching the kernel driver will be handled by the
 * caller, don't do this here. Return < 0 on error, 0 or higher on
 * success.
 */
static int driver_callback(libusb_device_handle *handle, USBDevice_t *device)
{
	int	ret;

	if ((ret = libusb_set_configuration(handle, 1)) != LIBUSB_SUCCESS) {
		upsdebugx(5, "Can't set USB configuration: %s", libusb_strerror(ret));
		return -1;
	}

	if ((ret = libusb_claim_interface(handle, 0)) != LIBUSB_SUCCESS) {
		upsdebugx(5, "Can't claim USB interface: %s", libusb_strerror(ret));
		return -1;
	}

	if ((ret = libusb_set_interface_alt_setting(handle, 0, 0)) != LIBUSB_SUCCESS) {
		upsdebugx(5, "Can't set USB alternate interface: %s", libusb_strerror(ret));
		return -1;
	}

	if ((ret = libusb_clear_halt(handle, LIBUSB_ENDPOINT_IN | 1)) != LIBUSB_SUCCESS) {
		upsdebugx(5, "Can't reset USB endpoint: %s", libusb_strerror(ret));
		return -1;
	}

	return 1;
}

static void usb_device_close(libusb_device_handle *handle)
{
	if (!handle)
		return;

	/* libusb_release_interface() sometimes blocks and goes into uninterruptible sleep. So don't do it. */
/*	libusb_release_interface(handle, 0);	*/
	libusb_close(handle);
}

static int usb_device_open(libusb_device_handle **handlep, USBDevice_t *device, USBDeviceMatcher_t *matcher,
	int (*callback)(libusb_device_handle *handle, USBDevice_t *device))
{
	int		  ret;
	libusb_device	**devlist;
	ssize_t		  devcount,
			  devnum;

	/* If device is still open, close it. */
	if (*handlep) {
		libusb_close(*handlep);
		/* Also, reset the handle now, to avoid possible problems
		 * (e.g. in case we exit() before the reset at the end of this function takes place,
		 * upsdrv_cleanup() may attempt to libusb_close() the device again, if not nullified) */
		*handlep = NULL;
	}

	devcount = libusb_get_device_list(NULL, &devlist);
	if (devcount < 0)
		upsdebugx(2, "Could not get the list of USB devices (%s).", libusb_strerror(devcount));

	for (devnum = 0; devnum < devcount; devnum++) {

		libusb_device			*dev = devlist[devnum];
		libusb_device_handle		*handle;
		USBDeviceMatcher_t		*m;
		struct libusb_device_descriptor	 dev_desc;
		int				 i;
		char				 string[SMALLBUF];
		uint8_t				 bus;

		ret = libusb_get_device_descriptor(dev, &dev_desc);
		if (ret != LIBUSB_SUCCESS) {
			upsdebugx(2, "Unable to get DEVICE descriptor (%s).", libusb_strerror(ret));
			continue;
		}

		ret = libusb_open(dev, &handle);
		if (ret != LIBUSB_SUCCESS) {
			upsdebugx(4, "Failed to open USB device, skipping: %s", libusb_strerror(ret));
			continue;
		}
		*handlep = handle;

		/* collect the identifying information of this
		   device. Note that this is safe, because
		   there's no need to claim an interface for
		   this (and therefore we do not yet need to
		   detach any kernel drivers). */

		free(device->Vendor);
		free(device->Product);
		free(device->Serial);
		free(device->Bus);
		memset(device, 0, sizeof(*device));

		device->VendorID = dev_desc.idVendor;
		device->ProductID = dev_desc.idProduct;

		bus = libusb_get_bus_number(dev);
		snprintf(string, sizeof(string), "%03u", bus);
		if ((device->Bus = strdup(string)) == NULL)
			goto oom_error;

		if (dev_desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(handle, dev_desc.iManufacturer, (unsigned char *)string, sizeof(string));
			if (ret > 0 && *str_trim_space(string) && (device->Vendor = strdup(string)) == NULL)
				goto oom_error;
		}

		if (dev_desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(handle, dev_desc.iProduct, (unsigned char *)string, sizeof(string));
			if (ret > 0 && *str_trim_space(string) && (device->Product = strdup(string)) == NULL)
				goto oom_error;
		}

		if (dev_desc.iSerialNumber) {
			ret = libusb_get_string_descriptor_ascii(handle, dev_desc.iSerialNumber, (unsigned char *)string, sizeof(string));
			if (ret > 0 && *str_trim_space(string) && (device->Serial = strdup(string)) == NULL)
				goto oom_error;
		}

		upsdebugx(4, "- VendorID     : %04x", device->VendorID);
		upsdebugx(4, "- ProductID    : %04x", device->ProductID);
		upsdebugx(4, "- Manufacturer : %s", device->Vendor ? device->Vendor : "unknown");
		upsdebugx(4, "- Product      : %s", device->Product ? device->Product : "unknown");
		upsdebugx(4, "- Serial Number: %s", device->Serial ? device->Serial : "unknown");
		upsdebugx(4, "- Bus          : %s", device->Bus);

		for (m = matcher; m; m = m->next) {
			switch (m->match_function(device, m->privdata))
			{
			case 0:
				upsdebugx(4, "Device does not match - skipping");
				goto next_device;
			case -1:
				libusb_free_device_list(devlist, 1);
				fatal_with_errno(EXIT_FAILURE, "matcher");
			case -2:
				upsdebugx(4, "matcher: unspecified error");
				goto next_device;
			}
		}

#if defined(HAVE_LIBUSB_KERNEL_DRIVER_ACTIVE) && defined(HAVE_LIBUSB_SET_AUTO_DETACH_KERNEL_DRIVER)
		ret = libusb_kernel_driver_active(handle, 0);
		/* Is the kernel driver active? Consider the unimplemented return code to be equivalent to inactive here. */
		if (ret == 1) {
			upsdebugx(2, "%s: libusb_kernel_driver_active() returned 1 (driver active).", __func__);
			/* In FreeBSD, currently (FreeBSD from 10.2 to 11.2, at least), it is not necessary to detach the kernel driver.
			 * Further, the detaching can only be done with root privileges.
			 * So, don't set the auto-detach flag, or libusb_claim_interface() will fail if the driver is not running as root. */
#ifndef __FreeBSD__
			/* Try the auto-detach kernel driver method. */
			ret = libusb_set_auto_detach_kernel_driver(handle, 1);
			if (ret != LIBUSB_SUCCESS)
				upsdebugx(2, "%s: failed to set kernel driver auto-detach driver flag for USB device (%s).", __func__, libusb_strerror(ret));
			else
				upsdebugx(2, "%s: successfully set kernel driver auto-detach flag.", __func__);
#endif	/* __FreeBSD__ */
		} else {
			upsdebugx(2, "%s: libusb_kernel_driver_active() returned %d (%s).", __func__, ret, ret ? libusb_strerror(ret) : "no driver active");
		}
#endif	/* HAVE_LIBUSB_KERNEL_DRIVER_ACTIVE + HAVE_LIBUSB_SET_AUTO_DETACH_KERNEL_DRIVER */

		for (i = 0; i < 3; i++) {

			ret = callback(handle, device);
			if (ret >= 0) {
				upsdebugx(4, "USB device [%04x:%04x] opened", device->VendorID, device->ProductID);
				libusb_free_device_list(devlist, 1);
				return ret;
			}
#ifdef HAVE_LIBUSB_DETACH_KERNEL_DRIVER
			/* this method forces device claiming by unbinding
			 * attached driver... From libhid */
			if ((ret = libusb_detach_kernel_driver(handle, 0)) != LIBUSB_SUCCESS) {
				upsdebugx(4, "failed to detach kernel driver from USB device: %s", libusb_strerror(ret));
			} else {
				upsdebugx(4, "detached kernel driver from USB device...");
			}
#endif
		}

		libusb_free_device_list(devlist, 1);
		fatalx(EXIT_FAILURE, "USB device [%04x:%04x] matches, but driver callback failed: %s",
			device->VendorID, device->ProductID, libusb_strerror(ret));

	next_device:
		libusb_close(handle);
		continue;

	oom_error:
		libusb_free_device_list(devlist, 1);
		fatal_with_errno(EXIT_FAILURE, "Out of memory");

	}

	*handlep = NULL;
	libusb_free_device_list(devlist, 1);
	upsdebugx(4, "No matching USB device found");

	return -1;
}

/*
 * Initialise the UPS
 */
void upsdrv_initups(void)
{
	char	reply[REPLY_PACKETSIZE];
	int	i,
		ret;

	/* libusb base init */
	if ((ret = libusb_init(NULL)) != LIBUSB_SUCCESS)
		fatalx(EXIT_FAILURE, "Failed to init libusb (%s).", libusb_strerror(ret));
	inited_libusb = TRUE;

	for (i = 0; usb_device_open(&udev, &usbdevice, &device_matcher, &driver_callback) < 0; i++) {

		if ((i < 32) && (sleep(5) == 0)) {
			usb_comm_fail("Can't open USB device, retrying ...");
			continue;
		}

		fatalx(EXIT_FAILURE,
			"Unable to find Richcomm dry-contact to USB solution\n\n"

			"Things to try:\n"
			" - Connect UPS device to USB bus\n"
			" - Run this driver as another user (upsdrvctl -u or 'user=...' in ups.conf).\n"
			"   See upsdrvctl(8) and ups.conf(5).\n\n"

			"Fatal error: unusable configuration");
	}

	/*
	 * Read rubbish data a few times; the UPS doesn't seem to respond properly
	 * the first few times after connecting
	 */
	for (i = 0; i < 5; i++) {
		query_ups(reply);
		sleep(1);
	}
}

void upsdrv_cleanup(void)
{
	if (!inited_libusb)
		return;

	usb_device_close(udev);
	libusb_exit(NULL);

	free(usbdevice.Vendor);
	free(usbdevice.Product);
	free(usbdevice.Serial);
	free(usbdevice.Bus);
}

void upsdrv_initinfo(void)
{
	dstate_setinfo("ups.mfr", "%s", "Richcomm dry-contact to USB solution");
	dstate_setinfo("ups.model", "%s", usbdevice.Product ? usbdevice.Product : "unknown");
	dstate_setinfo("ups.serial", "%s", usbdevice.Serial ? usbdevice.Serial : "unknown");

	dstate_setinfo("ups.vendorid", "%04x", usbdevice.VendorID);
	dstate_setinfo("ups.productid", "%04x", usbdevice.ProductID);
}

void upsdrv_updateinfo(void)
{
	char	reply[REPLY_PACKETSIZE];
	int	ret, online, battery_normal;

	if (!udev) {
		ret = usb_device_open(&udev, &usbdevice, &device_matcher, &driver_callback);

		if (ret < 0) {
			return;
		}
	}

	ret = query_ups(reply);

	if (ret < 4) {
		usb_comm_fail("Query to UPS failed");
		dstate_datastale();

		usb_device_close(udev);
		udev = NULL;

		return;
	}

	usb_comm_good();
	dstate_dataok();

	/*
	 * 3rd bit of 4th byte indicates whether the UPS is on line (1)
	 * or on battery (0)
	 */
	online = (reply[3]&4)>>2;

	/*
	 * 2nd bit of 4th byte indicates battery status; normal (1)
	 * or low (0)
	 */
	battery_normal = (reply[3]&2)>>1;

	status_init();

	if (online) {
	    status_set("OL");
	} else {
	    status_set("OB");
	}

	if (!battery_normal) {
	    status_set("LB");
	}

	status_commit();
}

/*
 * The shutdown feature is a bit strange on this UPS IMHO, it
 * switches the polarity of the 'Shutdown UPS' signal, at which
 * point it will automatically power down once it loses power.
 *
 * It will still, however, be possible to poll the UPS and
 * reverse the polarity _again_, at which point it will
 * start back up once power comes back.
 *
 * Maybe this is the normal way, it just seems a bit strange.
 *
 * Please note, this function doesn't power the UPS off if
 * line power is connected.
 */
void upsdrv_shutdown(void)
{
	/*
	 * This packet shuts down the UPS, that is,
	 * if it is not currently on line power
	 */
	char	prepare[QUERY_PACKETSIZE] = { 0x02, 0x00, 0x00, 0x00 };

	/*
	 * This should make the UPS turn itself back on once the
	 * power comes back on; which is probably what we want
	 */
	char	restart[QUERY_PACKETSIZE] = { 0x02, 0x01, 0x00, 0x00 };
	char	reply[REPLY_PACKETSIZE];

	execute_and_retrieve_query(prepare, reply);

	/*
	 * have to, the previous command seems to be
	 * ignored if the second command comes right
	 * behind it
	 */
	sleep(1);


	execute_and_retrieve_query(restart, reply);
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}
