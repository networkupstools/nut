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
#include "usb-common.h"

/* driver version */
#define DRIVER_NAME	"Richcomm dry-contact to USB driver"
#define DRIVER_VERSION	"0.09"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Peter van Valderen <p.v.valderen@probu.nl>\n"
	"Dirk Teurlings <dirk@upexia.nl>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define STATUS_REQUESTTYPE	0x21
#define REPLY_REQUESTTYPE	0x81
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

/* Compatibility layer between libusb 0.1 and 1.0 */
#ifdef WITH_LIBUSB_1_0
 /* Simply remap libusb functions/structures from 0.1 to 1.0 */
 typedef libusb_device_handle usb_dev_handle;
 typedef unsigned char* usb_ctrl_char;
 #define usb_control_msg libusb_control_transfer
 static inline  int usb_interrupt_read(libusb_device_handle *dev, int ep,
        unsigned char *bytes, int size, int timeout)
 {
	int ret = libusb_interrupt_transfer(dev, ep, (unsigned char *) bytes,
			size, &size, timeout);
	/* In case of success, return the operation size, as done with libusb 0.1 */
	return (ret == LIBUSB_SUCCESS)?size:ret;
 }
 #define usb_claim_interface libusb_claim_interface
 #define usb_close libusb_close
 #define usb_set_configuration libusb_set_configuration
 #define usb_reset libusb_reset_device
 #define usb_clear_halt libusb_clear_halt
 #define usb_get_string libusb_get_string_descriptor
 #define usb_get_string_simple libusb_get_string_descriptor_ascii
 #define nut_usb_strerror(a) libusb_strerror(a)
#else
 typedef char* usb_ctrl_char;
 #define nut_usb_strerror(a) usb_strerror()
#endif /* WITH_LIBUSB_1_0 */

static usb_dev_handle	*udev = NULL;
static USBDevice_t	usbdevice;
static unsigned int	comm_failures = 0;

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

static int execute_and_retrieve_query(char *query, char *reply)
{
	int	ret;

	ret = usb_control_msg(udev, STATUS_REQUESTTYPE, REQUEST_VALUE,
		MESSAGE_VALUE, INDEX_VALUE, (usb_ctrl_char)query, QUERY_PACKETSIZE, 1000);

	if (ret <= 0) {
		upsdebugx(3, "send: %s", ret ? nut_usb_strerror(ret) : "timeout");
		return ret;
	}

	upsdebug_hex(3, "send", query, ret);

	ret = usb_interrupt_read(udev, REPLY_REQUESTTYPE, (usb_ctrl_char)reply, REPLY_PACKETSIZE, 1000);

	if (ret <= 0) {
		upsdebugx(3, "read: %s", ret ? nut_usb_strerror(ret) : "timeout");
		return ret;
	}

	upsdebug_hex(3, "read", reply, ret);
	return ret;
}

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
static int driver_callback(usb_dev_handle *handle, USBDevice_t *device)
{
	if (usb_set_configuration(handle, 1) < 0) {
		upsdebugx(5, "Can't set USB configuration");
		return -1;
	}

	if (usb_claim_interface(handle, 0) < 0) {
		upsdebugx(5, "Can't claim USB interface");
		return -1;
	}

#ifdef WITH_LIBUSB_0_1
	if (usb_set_altinterface(handle, 0) < 0) {
		upsdebugx(5, "Can't set USB alternate interface");
		return -1;
	}
#elif WITH_LIBUSB_1_0
	int ret = 0;

	if ((ret = libusb_set_interface_alt_setting(handle, 0, 0)) < 0) {
		upsdebugx(5, "Can't set USB alternate interface: %s", nut_usb_strerror(ret));
		return -1;
	}
#endif /* WITH_LIBUSB_1_0 */

	if (usb_clear_halt(handle, 0x81) < 0) {
		upsdebugx(5, "Can't reset USB endpoint");
		return -1;
	}

	return 1;
}

static int usb_device_close(usb_dev_handle *handle)
{
	int ret = 0;

	if (!handle) {
		return 0;
	}

	/* usb_release_interface() sometimes blocks and goes
	into uninterruptible sleep.  So don't do it. */
	/* usb_release_interface(handle, 0); */
#ifdef WITH_LIBUSB_1_0
		libusb_close(handle);
		libusb_exit(NULL);
#else
		ret = usb_close(handle);
#endif
	return ret;
}

static int usb_device_open(usb_dev_handle **handlep, USBDevice_t *device, USBDeviceMatcher_t *matcher,
	int (*callback)(usb_dev_handle *handle, USBDevice_t *device))
{
	int ret = 0;
	uint8_t iManufacturer = 0, iProduct = 0, iSerialNumber = 0;

	/* libusb base init */
#ifdef WITH_LIBUSB_1_0
	if (libusb_init(NULL) < 0) {
		libusb_exit(NULL);
		fatal_with_errno(EXIT_FAILURE, "Failed to init libusb 1.0");
	}
#else
	usb_init();
	usb_find_busses();
	usb_find_devices();
#endif /* WITH_LIBUSB_1_0 */

#ifndef __linux__ /* SUN_LIBUSB (confirmed to work on Solaris and FreeBSD) */
	/* Causes a double free corruption in linux if device is detached! */
	if (*handlep)
		usb_close(*handlep);
#endif

#ifdef WITH_LIBUSB_1_0
	libusb_device **devlist;
	ssize_t devcount = 0;
	libusb_device_handle *handle;
	struct libusb_device_descriptor dev_desc;
	uint8_t bus;
	int i;

	devcount = libusb_get_device_list(NULL, &devlist);
	if (devcount <= 0)
		fatal_with_errno(EXIT_FAILURE, "No USB device found");

	for (i = 0; i < devcount; i++) {

		USBDeviceMatcher_t	*m;
		libusb_device *dev = devlist[i];
		libusb_get_device_descriptor(dev, &dev_desc);
		ret = libusb_open(dev, &handle);
		*handlep = handle;
#else
	struct usb_bus	*bus;
	for (bus = usb_busses; bus; bus = bus->next) {

		struct usb_device	*dev;
		usb_dev_handle		*handle;

		for (dev = bus->devices; dev; dev = dev->next) {

			int	i;
			USBDeviceMatcher_t	*m;

			upsdebugx(3, "Checking USB device [%04x:%04x] (%s/%s)", dev->descriptor.idVendor,
				dev->descriptor.idProduct, bus->dirname, dev->filename);
			
			/* supported vendors are now checked by the supplied matcher */

			/* open the device */
			*handlep = handle = usb_open(dev);
#endif /* WITH_LIBUSB_1_0 */
			if (!handle) {
				upsdebugx(4, "Failed to open USB device, skipping: %s", nut_usb_strerror(ret));
				continue;
			}

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

#ifdef WITH_LIBUSB_1_0
			device->VendorID = dev_desc.idVendor;
			device->ProductID = dev_desc.idProduct;
			bus = libusb_get_bus_number(dev);
			device->Bus = (char *)malloc(4);
			if (device->Bus == NULL) {
				libusb_free_device_list(devlist, 1);
				fatal_with_errno(EXIT_FAILURE, "Out of memory");
			}
			sprintf(device->Bus, "%03d", bus);
			iManufacturer = dev_desc.iManufacturer;
			iProduct = dev_desc.iProduct;
			iSerialNumber = dev_desc.iSerialNumber;
#else
			device->VendorID = dev->descriptor.idVendor;
			device->ProductID = dev->descriptor.idProduct;
			device->Bus = xstrdup(bus->dirname);
			iManufacturer = dev->descriptor.iManufacturer;
			iProduct = dev->descriptor.iProduct;
			iSerialNumber = dev->descriptor.iSerialNumber;
#endif /* WITH_LIBUSB_1_0 */
			
			if (iManufacturer) {
				char	buf[SMALLBUF];
				ret = usb_get_string_simple(handle, iManufacturer,
					(usb_ctrl_char)buf, sizeof(buf));
				if (ret > 0) {
					device->Vendor = strdup(buf);
					if (device->Vendor == NULL) {
#ifdef WITH_LIBUSB_1_0
						libusb_free_device_list(devlist, 1);
#endif	/* WITH_LIBUSB_1_0 */
						fatal_with_errno(EXIT_FAILURE, "Out of memory");
					}
				}
			}

			if (iProduct) {
				char	buf[SMALLBUF];
				ret = usb_get_string_simple(handle, iProduct,
					(usb_ctrl_char)buf, sizeof(buf));
				if (ret > 0) {
					device->Product = strdup(buf);
					if (device->Product == NULL) {
#ifdef WITH_LIBUSB_1_0
						libusb_free_device_list(devlist, 1);
#endif	/* WITH_LIBUSB_1_0 */
						fatal_with_errno(EXIT_FAILURE, "Out of memory");
					}
				}
			}

			if (iSerialNumber) {
				char	buf[SMALLBUF];
				ret = usb_get_string_simple(handle, iSerialNumber,
					(usb_ctrl_char)buf, sizeof(buf));
				if (ret > 0) {
					device->Serial = strdup(buf);
					if (device->Serial == NULL) {
#ifdef WITH_LIBUSB_1_0
						libusb_free_device_list(devlist, 1);
#endif	/* WITH_LIBUSB_1_0 */
						fatal_with_errno(EXIT_FAILURE, "Out of memory");
					}
				}
			}

			upsdebugx(4, "- VendorID     : %04x", device->VendorID);
			upsdebugx(4, "- ProductID    : %04x", device->ProductID);
			upsdebugx(4, "- Manufacturer : %s", device->Vendor ? device->Vendor : "unknown");
			upsdebugx(4, "- Product      : %s", device->Product ? device->Product : "unknown");
			upsdebugx(4, "- Serial Number: %s", device->Serial ? device->Serial : "unknown");
			upsdebugx(4, "- Bus          : %s", device->Bus ? device->Bus : "unknown");

			for (m = matcher; m; m = m->next) {
				
				switch (m->match_function(device, m->privdata))
				{
				case 0:
					upsdebugx(4, "Device does not match - skipping");
					goto next_device;
				case -1:
#ifdef WITH_LIBUSB_1_0
					libusb_free_device_list(devlist, 1);
#endif	/* WITH_LIBUSB_1_0 */
					fatal_with_errno(EXIT_FAILURE, "matcher");
				case -2:
					upsdebugx(4, "matcher: unspecified error");
					goto next_device;
				}
			}
#ifdef HAVE_LIBUSB_SET_AUTO_DETACH_KERNEL_DRIVER
		/* First, try the auto-detach kernel driver method
		 * This function is not available on FreeBSD 10.1-10.3 */
		if ((ret = libusb_set_auto_detach_kernel_driver(handle, 1)) < 0)
			upsdebugx(2, "failed to auto detach kernel driver from USB device: %s",
				libusb_strerror((enum libusb_error)ret));
		else
			upsdebugx(2, "auto detached kernel driver from USB device");
#endif

			for (i = 0; i < 3; i++) {

				ret = callback(handle, device);
				if (ret >= 0) {
					upsdebugx(4, "USB device [%04x:%04x] opened", device->VendorID, device->ProductID);
#ifdef WITH_LIBUSB_1_0
					libusb_free_device_list(devlist, 1);
#endif	/* WITH_LIBUSB_1_0 */
					return ret;
				}
#ifdef HAVE_USB_DETACH_KERNEL_DRIVER_NP
				/* this method requires at least libusb 0.1.8:
				 * it force device claiming by unbinding
				 * attached driver... From libhid */
				if (usb_detach_kernel_driver_np(handle, 0) < 0) {
					upsdebugx(4, "failed to detach kernel driver from USB device: %s", usb_strerror());
				} else {
					upsdebugx(4, "detached kernel driver from USB device...");
				}
#elif HAVE_LIBUSB_DETACH_KERNEL_DRIVER
				if ((ret = libusb_detach_kernel_driver(handle, 0)) < 0) {
					upsdebugx(4, "failed to detach kernel driver from USB device: %s", nut_usb_strerror(ret));
				} else {
					upsdebugx(4, "detached kernel driver from USB device...");
				}
#endif
			}

#ifdef WITH_LIBUSB_1_0
			libusb_free_device_list(devlist, 1);
#endif	/* WITH_LIBUSB_1_0 */
			fatalx(EXIT_FAILURE, "USB device [%04x:%04x] matches, but driver callback failed: %s",
				device->VendorID, device->ProductID, nut_usb_strerror(ret));

		next_device:
			usb_close(handle);
#ifndef WITH_LIBUSB_1_0
		}
#endif /* WITH_LIBUSB_1_0 */
	}

	*handlep = NULL;
#ifdef WITH_LIBUSB_1_0
	libusb_free_device_list(devlist, 1);
#endif	/* WITH_LIBUSB_1_0 */
	upsdebugx(4, "No matching USB device found");

	return -1;
}

/*
 * Initialise the UPS
 */
void upsdrv_initups(void)
{
	char	reply[REPLY_PACKETSIZE];
	int	i;

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
	usb_device_close(udev);

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
