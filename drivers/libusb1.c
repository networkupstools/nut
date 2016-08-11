/*!
 * @file libusb.c
 * @brief Generic USB communication backend (using libusb 1.0)
 *
 * @author Copyright (C) 2016 Eaton
 *         Copyright (C) 2016 Arnaud Quette <aquette.dev@gmail.com>
 *
 *      The logic of this file is ripped from mge-shut driver (also from
 *      Arnaud Quette), which is a "HID over serial link" UPS driver for
 *      Network UPS Tools <http://www.networkupstools.org/>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * -------------------------------------------------------------------------- */

#include "config.h" /* for HAVE_LIBUSB_DETACH_KERNEL_DRIVER flag */
#include "common.h" /* for xmalloc, upsdebugx prototypes */
#include "usb-common.h"
#include "nut_libusb.h"

#define USB_DRIVER_NAME		"USB communication driver (libusb 1.0)"
#define USB_DRIVER_VERSION	"0.01"

/* driver description structure */
upsdrv_info_t comm_upsdrv_info = {
	USB_DRIVER_NAME,
	USB_DRIVER_VERSION,
	NULL,
	0,
	{ NULL }
};

#define MAX_REPORT_SIZE         0x1800

static void nut_libusb_close(libusb_device_handle *udev);

/*! Add USB-related driver variables with addvar().
 * This removes some code duplication across the USB drivers.
 */
void nut_usb_addvars(void)
{
	/* allow -x vendor=X, vendorid=X, product=X, productid=X, serial=X */
	addvar(VAR_VALUE, "vendor", "Regular expression to match UPS Manufacturer string");
	addvar(VAR_VALUE, "product", "Regular expression to match UPS Product string");
	addvar(VAR_VALUE, "serial", "Regular expression to match UPS Serial number");

	addvar(VAR_VALUE, "vendorid", "Regular expression to match UPS Manufacturer numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "productid", "Regular expression to match UPS Product numerical ID (4 digits hexadecimal)");

	addvar(VAR_VALUE, "bus", "Regular expression to match USB bus name");
	addvar(VAR_VALUE, "usb_set_altinterface", "Force redundant call to usb_set_altinterface() (value=bAlternateSetting; default=0)");
}

/* From usbutils: workaround libusb API goofs:  "byte" should never be sign extended;
 * using "char" is trouble.  Likewise, sizes should never be negative.
 */

static inline int typesafe_control_msg(libusb_device_handle *dev,
        unsigned char requesttype, unsigned char request,
        int value, int index,
        unsigned char *bytes, unsigned size, int timeout)
{
		return libusb_control_transfer(dev, requesttype, request, value, index,
                (unsigned char *) bytes, (int) size, timeout);
}

/* invoke matcher against device */
static inline int matches(USBDeviceMatcher_t *matcher, USBDevice_t *device) {
	if (!matcher) {
		return 1;
	}
	return matcher->match_function(device, matcher->privdata);
}

/*! If needed, set the USB alternate interface.
 *
 * In NUT 2.7.2 and earlier, the following call was made unconditionally:
 *   usb_set_altinterface(udev, 0);
 *
 * Although harmless on Linux and *BSD, this extra call prevents old Tripp Lite
 * devices from working on Mac OS X (presumably the OS is already setting
 * altinterface to 0).
 */
static int nut_usb_set_altinterface(libusb_device_handle *udev)
{
	int altinterface = 0, ret = 0;
	char *alt_string, *endp = NULL;

	if(testvar("usb_set_altinterface")) {
		alt_string = getval("usb_set_altinterface");
		if(alt_string) {
			altinterface = (int)strtol(alt_string, &endp, 10);
			if(endp && !(endp[0] == 0)) {
				upslogx(LOG_WARNING, "%s: '%s' is not a valid number", __func__, alt_string);
			}
			if(altinterface < 0 || altinterface > 255) {
				upslogx(LOG_WARNING, "%s: setting bAlternateInterface to %d will probably not work", __func__, altinterface);
			}
		}
		/* set default interface */
#if 0
		upsdebugx(2, "%s: calling usb_set_altinterface(udev, %d)", __func__, altinterface);
		ret = usb_set_altinterface(udev, altinterface);
		if(ret != 0) {
			upslogx(LOG_WARNING, "%s: usb_set_altinterface(udev, %d) returned %d (%s)",
					__func__, altinterface, ret, usb_strerror() );
		}
		upslogx(LOG_NOTICE, "%s: usb_set_altinterface() should not be necessary - please email the nut-upsdev list with information about your UPS.", __func__);
	} else {
		upsdebugx(3, "%s: skipped usb_set_altinterface(udev, 0)", __func__);
	}
#else
	}
	upsdebugx(2, "%s is not implemented yet on libusb 1.0", __func__);
#endif // 0
	return ret;
}
/* FIXME: still needed?! */
#define usb_control_msg         typesafe_control_msg

/* On success, fill in the curDevice structure and return the report
 * descriptor length. On failure, return -1.
 * Note: When callback is not NULL, the report descriptor will be
 * passed to this function together with the udev and USBDevice_t
 * information. This callback should return a value > 0 if the device
 * is accepted, or < 1 if not. If it isn't accepted, the next device
 * (if any) will be tried, until there are no more devices left.
 */
static int nut_libusb_open(libusb_device_handle **udevp, USBDevice_t *curDevice, USBDeviceMatcher_t *matcher,
	int (*callback)(libusb_device_handle *udev, USBDevice_t *hd, unsigned char *rdbuf, int rdlen))
{
#ifdef HAVE_LIBUSB_DETACH_KERNEL_DRIVER
	int retries;
#endif
	int rdlen1, rdlen2; /* report descriptor length, method 1+2 */
	USBDeviceMatcher_t *m;
	libusb_device **devlist;
	ssize_t devcount = 0;
	struct libusb_device_descriptor dev_desc;
	struct libusb_config_descriptor *conf_desc = NULL;
	const struct libusb_interface_descriptor *if_desc;
	libusb_device_handle *udev;
	uint8_t bus, port_path[8];
	int ret, res;
	unsigned char buf[20];
	const unsigned char *p;
	char string[256];
	int i;
	/* All devices use HID descriptor at index 0. However, some newer
	 * Eaton units have a light HID descriptor at index 0, and the full
	 * version is at index 1 (in which case, bcdDevice == 0x0202) */
	int hid_desc_index = 0;

	/* report descriptor */
	unsigned char	rdbuf[MAX_REPORT_SIZE];
	int		rdlen;

	/* libusb base init */
	if (libusb_init(NULL) < 0) {
		libusb_exit(NULL);
		fatal_with_errno(EXIT_FAILURE, "Failed to init libusb 1.0");
	}

#ifndef __linux__ /* SUN_LIBUSB (confirmed to work on Solaris and FreeBSD) */
	/* Causes a double free corruption in linux if device is detached! */
	nut_libusb_close(*udevp);
#endif

	devcount = libusb_get_device_list(NULL, &devlist);
	if (devcount <= 0)
		fatal_with_errno(EXIT_FAILURE, "No USB device found");

	for (i = 0; i < devcount; i++) {

		libusb_device *device = devlist[i];
		libusb_get_device_descriptor(device, &dev_desc);
		upsdebugx(2, "Checking device (%04X/%04X)",
					dev_desc.idVendor, dev_desc.idProduct);

		/* supported vendors are now checked by the supplied matcher */

		/* open the device */
		ret = libusb_open(device, udevp);
		if (!*udevp) {
			upsdebugx(2, "Failed to open device, skipping. (%s)",
						libusb_strerror((enum libusb_error)ret));
			continue;
		}
		udev = *udevp;

		/* collect the identifying information of this
		   device. Note that this is safe, because
		   there's no need to claim an interface for
		   this (and therefore we do not yet need to
		   detach any kernel drivers). */

		free(curDevice->Vendor);
		free(curDevice->Product);
		free(curDevice->Serial);
		free(curDevice->Bus);
		memset(curDevice, '\0', sizeof(*curDevice));

		bus = libusb_get_bus_number(device);
		ret = libusb_get_port_numbers(device, port_path, sizeof(port_path));
		if (ret > 0) {
			upsdebugx(2, "bus number: %d, port path: %d (nb elem %i)", bus, port_path[0], ret);
			curDevice->Bus = (char *)xmalloc(10);
			sprintf(curDevice->Bus, "%03d/%03d", bus, port_path[0]);
		}
		curDevice->VendorID = dev_desc.idVendor;
		curDevice->ProductID = dev_desc.idProduct;
		curDevice->bcdDevice = dev_desc.bcdDevice;

		if (dev_desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(udev, dev_desc.iManufacturer,
				(unsigned char*)string, sizeof(string));
			if (ret > 0) {
				curDevice->Vendor = strdup(string);
			}
		}

		if (dev_desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(udev, dev_desc.iProduct,
				(unsigned char*)string, sizeof(string));
			if (ret > 0) {
				curDevice->Product = strdup(string);
			}
		}

		if (dev_desc.iSerialNumber) {
			ret = libusb_get_string_descriptor_ascii(udev, dev_desc.iSerialNumber,
				(unsigned char*)string, sizeof(string));
			if (ret > 0) {
				curDevice->Serial = strdup(string);
			}
		}

		upsdebugx(2, "- VendorID: %04x", curDevice->VendorID);
		upsdebugx(2, "- ProductID: %04x", curDevice->ProductID);
		upsdebugx(2, "- Manufacturer: %s", curDevice->Vendor ? curDevice->Vendor : "unknown");
		upsdebugx(2, "- Product: %s", curDevice->Product ? curDevice->Product : "unknown");
		upsdebugx(2, "- Serial Number: %s", curDevice->Serial ? curDevice->Serial : "unknown");
		upsdebugx(2, "- Bus: %s", curDevice->Bus ? curDevice->Bus : "unknown");
		upsdebugx(2, "- Device release number: %04x", curDevice->bcdDevice);

		/* FIXME: extend to Eaton OEMs (HP, IBM, ...) */
		if ((curDevice->VendorID == 0x463) && (curDevice->bcdDevice == 0x0202)) {
			hid_desc_index = 1;
		}

		upsdebugx(2, "Trying to match device");
		for (m = matcher; m; m=m->next) {
			ret = matches(m, curDevice);
			if (ret==0) {
				upsdebugx(2, "Device does not match - skipping");
				goto next_device;
			} else if (ret==-1) {
				fatal_with_errno(EXIT_FAILURE, "matcher");
				goto next_device;
			} else if (ret==-2) {
				upsdebugx(2, "matcher: unspecified error");
				goto next_device;
			}
		}
		upsdebugx(2, "Device matches");


		upsdebugx(2, "Reading first configuration descriptor");
		ret = libusb_get_config_descriptor(device, 0, &conf_desc);
		/*ret = libusb_get_active_config_descriptor(device, &conf_desc);*/
		upsdebugx(2, "got %i (%s)", ret, libusb_strerror((enum libusb_error)ret));

		/* Now we have matched the device we wanted. Claim it. */

#ifdef HAVE_LIBUSB_DETACH_KERNEL_DRIVER
		libusb_set_auto_detach_kernel_driver (udev, 1);
#endif
		retries = 3;
		while ((ret = libusb_claim_interface(udev, 0) != 0)) {

			upsdebugx(2, "failed to claim USB device: %s (%i)", libusb_strerror((enum libusb_error)ret), ret);

			if (retries-- > 0) {
				continue;
			}

			fatalx(EXIT_FAILURE, "Can't claim USB device [%04x:%04x]: %s", curDevice->VendorID, curDevice->ProductID, libusb_strerror((enum libusb_error)ret));
		}
		upsdebugx(2, "Claimed interface 0 successfully");

		nut_usb_set_altinterface(udev);

		if (!callback) {
			return 1;
		}

		if (!conf_desc) { /* ?? this should never happen */
			upsdebugx(2, "  Couldn't retrieve descriptors");
			goto next_device;
		}

		rdlen1 = -1;
		rdlen2 = -1;

		/* Get HID descriptor */
		/* FIRST METHOD: ask for HID descriptor directly. */
		res = libusb_control_transfer(udev, LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_STANDARD|LIBUSB_RECIPIENT_INTERFACE,
			LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_HID<<8) + hid_desc_index, 0, buf, 0x9, USB_TIMEOUT);

		if (res < 0) {
			upsdebugx(2, "Unable to get HID descriptor (%s)", libusb_strerror((enum libusb_error)res));
		} else if (res < 9) {
			upsdebugx(2, "HID descriptor too short (expected %d, got %d)", 8, res);
		} else {

			upsdebug_hex(3, "HID descriptor, method 1", buf, 9);

			rdlen1 = buf[7] | (buf[8] << 8);
		}

		if (rdlen1 < -1) {
			upsdebugx(2, "Warning: HID descriptor, method 1 failed");
		}
		upsdebugx(3, "HID descriptor length (method 1) %d", rdlen1);

		/* SECOND METHOD: find HID descriptor among "extra" bytes of
		   interface descriptor, i.e., bytes tucked onto the end of
		   descriptor 2. */

		/* Note: on some broken UPS's (e.g. Tripp Lite Smart1000LCD),
			only this second method gives the correct result */

		/* for now, we always assume configuration 0, interface 0,
		   altsetting 0, as above. */

		if_desc = &(conf_desc->interface[0].altsetting[0]);
		for (i=0; i<if_desc->extra_length; i+=if_desc->extra[i]) {
			upsdebugx(4, "i=%d, extra[i]=%02x, extra[i+1]=%02x", i,
				if_desc->extra[i], if_desc->extra[i+1]);
			if (i+9 <= if_desc->extra_length && if_desc->extra[i] >= 9 && if_desc->extra[i+1] == 0x21) {
				p = &if_desc->extra[i];
				upsdebug_hex(3, "HID descriptor, method 2", p, 9);
				rdlen2 = p[7] | (p[8] << 8);
				break;
			}
		}

		/* we can now free the config descriptor */
		libusb_free_config_descriptor(conf_desc);
		
		if (rdlen2 < -1) {
			upsdebugx(2, "Warning: HID descriptor, method 2 failed");
		}
		upsdebugx(3, "HID descriptor length (method 2) %d", rdlen2);

		/* when available, always choose the second value, as it
			seems to be more reliable (it is the one reported e.g. by
			lsusb). Note: if the need arises, can change this to use
			the maximum of the two values instead. */
		if ((curDevice->VendorID == 0x463) && (curDevice->bcdDevice == 0x0202)) {
			upsdebugx(1, "Eaton device v2.02. Using full report descriptor");
			rdlen = rdlen1;
		}
		else {
			rdlen = rdlen2 >= 0 ? rdlen2 : rdlen1;
		}

		if (rdlen < 0) {
			upsdebugx(2, "Unable to retrieve any HID descriptor");
			goto next_device;
		}
		if (rdlen1 >= 0 && rdlen2 >= 0 && rdlen1 != rdlen2) {
			upsdebugx(2, "Warning: two different HID descriptors retrieved (Reportlen = %d vs. %d)", rdlen1, rdlen2);
		}

		upsdebugx(2, "HID descriptor length %d", rdlen);

		if (rdlen > (int)sizeof(rdbuf)) {
			upsdebugx(2, "HID descriptor too long %d (max %d)", rdlen, (int)sizeof(rdbuf));
			goto next_device;
		}

		res = libusb_control_transfer(udev, LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_STANDARD|LIBUSB_RECIPIENT_INTERFACE,
			LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_REPORT<<8) + hid_desc_index, 0, rdbuf, rdlen, USB_TIMEOUT);

		if (res < 0)
		{
			upsdebug_with_errno(2, "Unable to get Report descriptor");
			goto next_device;
		}

		if (res < rdlen)
		{
			upsdebugx(2, "Warning: report descriptor too short (expected %d, got %d)", rdlen, res);
			rdlen = res; /* correct rdlen if necessary */
		}

		res = callback(udev, curDevice, rdbuf, rdlen);
		if (res < 1) {
			upsdebugx(2, "Caller doesn't like this device");
			goto next_device;
		}

		upsdebugx(2, "Report descriptor retrieved (Reportlen = %d)", rdlen);
		upsdebugx(2, "Found HID device");
		fflush(stdout);

		return rdlen;

		next_device:
			libusb_close(udev);
	}

	*udevp = NULL;
	upsdebugx(2, "No appropriate HID device found");
	fflush(stdout);

	return -1;
}

/*
 * Error handler for usb_get/set_* functions. Return value > 0 success,
 * 0 unknown or temporary failure (ignored), < 0 permanent failure (reconnect)
 */
static int nut_libusb_strerror(const int ret, const char *desc)
{
	if (ret > 0) {
		return ret;
	}

	switch(ret)
	{
#if 0
	/* FIXME: not sure how to map these ones! */
	case LIBUSB_ERROR_INVALID_PARAM: /** Invalid parameter */
	/** System call interrupted (perhaps due to signal) */
	case LIBUSB_ERROR_INTERRUPTED:
	/** Insufficient memory */
	case LIBUSB_ERROR_NO_MEM:
#endif
	case LIBUSB_ERROR_BUSY:	     /** Resource busy */
	case LIBUSB_ERROR_NO_DEVICE: /** No such device (it may have been disconnected) */
	case LIBUSB_ERROR_ACCESS:    /** Access denied (insufficient permissions) */
	case LIBUSB_ERROR_IO:        /** Input/output error */
	case LIBUSB_ERROR_NOT_FOUND: /** Entity not found */
	case LIBUSB_ERROR_PIPE:	     /** Pipe error */
	/** Operation not supported or unimplemented on this platform */
	case LIBUSB_ERROR_NOT_SUPPORTED: 
		upslogx(LOG_DEBUG, "%s: %s", desc, libusb_strerror((enum libusb_error)ret));
		return ret;
	case LIBUSB_ERROR_TIMEOUT:	 /** Operation timed out */
		upsdebugx(2, "%s: Connection timed out", desc);
		return 0;
	case LIBUSB_ERROR_OVERFLOW:	 /** Overflow */
#ifdef EPROTO
/* FIXME: not sure how to map this one! */
	case -EPROTO:	/* Protocol error */
#endif
		upsdebugx(2, "%s: %s", desc, libusb_strerror((enum libusb_error)ret));
		return 0;

	case LIBUSB_ERROR_OTHER:     /** Other error */
	default:                     /** Undetermined, log only */
		upslogx(LOG_DEBUG, "%s: %s", desc, libusb_strerror((enum libusb_error)ret));
		return 0;
	}
}

/* return the report of ID=type in report
 * return -1 on failure, report length on success
 */

static int nut_libusb_get_report(libusb_device_handle *udev, int ReportId, unsigned char *raw_buf, int ReportSize )
{
	int	ret;

	upsdebugx(4, "Entering libusb_get_report");

	if (!udev) {
		return 0;
	}

	ret = libusb_control_transfer(udev,
		LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE,
		0x01, /* HID_REPORT_GET */
		ReportId+(0x03<<8), /* HID_REPORT_TYPE_FEATURE */
		0, raw_buf, ReportSize, USB_TIMEOUT);

	/* Ignore "protocol stall" (for unsupported request) on control endpoint */
	if (ret == LIBUSB_ERROR_PIPE) {
		return 0;
	}

	return nut_libusb_strerror(ret, __func__);
}

static int nut_libusb_set_report(libusb_device_handle *udev, int ReportId, unsigned char *raw_buf, int ReportSize )
{
	int	ret;

	if (!udev) {
		return 0;
	}

	ret = libusb_control_transfer(udev,
		LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE,
		0x09, /* HID_REPORT_SET = 0x09*/
		ReportId+(0x03<<8), /* HID_REPORT_TYPE_FEATURE */
		0, raw_buf, ReportSize, USB_TIMEOUT);

	/* Ignore "protocol stall" (for unsupported request) on control endpoint */
	if (ret == LIBUSB_ERROR_PIPE) {
		return 0;
	}

	return nut_libusb_strerror(ret, __func__);
}

static int nut_libusb_get_string(libusb_device_handle *udev, int StringIdx, char *buf, size_t buflen)
{
	int ret;

	if (!udev) {
		return -1;
	}
	ret = libusb_get_string_descriptor_ascii(udev, StringIdx,
		(unsigned char*)buf, buflen);

	return nut_libusb_strerror(ret, __func__);
}

static int nut_libusb_get_interrupt(libusb_device_handle *udev, unsigned char *buf, int bufsize, int timeout)
{
	int ret;

	if (!udev) {
		return -1;
	}

	/* FIXME: hardcoded interrupt EP => need to get EP descr for IF descr */
	ret = libusb_interrupt_transfer(udev, 0x81, buf, bufsize, &bufsize, timeout);

	/* Clear stall condition */
	if (ret == LIBUSB_ERROR_PIPE) {
		ret = libusb_clear_halt(udev, 0x81);
	}

	return nut_libusb_strerror(ret, __func__);
}

static void nut_libusb_close(libusb_device_handle *udev)
{
	if (!udev) {
		return;
	}

	/* usb_release_interface() sometimes blocks and goes
	into uninterruptible sleep.  So don't do it. */
	/* libusb_release_interface(udev, 0); */
	libusb_close(udev);
	libusb_exit(NULL);
}

usb_communication_subdriver_t usb_subdriver = {
	USB_DRIVER_VERSION,
	USB_DRIVER_NAME,
	nut_libusb_open,
	nut_libusb_close,
	nut_libusb_get_report,
	nut_libusb_set_report,
	nut_libusb_get_string,
	nut_libusb_get_interrupt
};
