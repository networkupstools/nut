/**
 * @file
 * @brief	HID Library - USB communication subdriver.
 * @copyright	@parblock
 * Copyright (C):
 * - 2003-2016 -- Arnaud Quette (<aquette.dev@gmail.com>), for MGE UPS SYSTEMS and Eaton
 * - 2005-2007 -- Peter Selinger (<selinger@users.sourceforge.net>)
 * - 2018      -- Daniele Pezzini (<hyouko@gmail.com>)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *		@endparblock
 */

#include "nut_libusb.h"

#include "config.h"	/* for HAVE_LIBUSB_* flags */
#include "common.h"	/* for upsdebug*() */
#include "dstate.h"	/* for dstate_setinfo() */
#include "hiddefs.h"
#include "nut_stdint.h"	/* for uint*_t */
#include "str.h"

/** @name Driver info
 * @{ *************************************************************************/

#define NUT_USB_DRIVER_NAME	"USB communication driver"			/**< @brief Name of this driver. */
#define NUT_USB_DRIVER_VERSION	"0.29"						/**< @brief Version of this driver. */

upsdrv_info_t	comm_upsdrv_info = {
	NUT_USB_DRIVER_NAME,
	NUT_USB_DRIVER_VERSION,
	NULL,
	0,
	{ NULL }
};

/** @} ************************************************************************/


/** @name Common useful stuff
 * @{ *************************************************************************/

#define NUT_USB_MAX_STRING_SIZE		256					/**< @brief Maximum string length... */

/** @brief USB interface number.
 *
 * So far, all of the supported devices use interface 0.
 * Let's make this a constant rather than a magic number. */
static const int	nut_usb_if_num = 0;

/** @} ************************************************************************/


/** @name Support functions
 * @{ *************************************************************************/

/** @brief Claim the @ref nut_usb_if_num interface on a given device handle, trying to detach the kernel driver (if the operation is supported and the driver active).
 * @return @ref LIBUSB_SUCCESS, on success,
 * @return a @ref libusb_error "LIBUSB_ERROR" code, on errors. */
static int	nut_usb_claim_interface(
	libusb_device_handle	*udev	/**< [in] handle of an already opened device */
) {
	int	ret;
#ifdef HAVE_LIBUSB_DETACH_KERNEL_DRIVER
	int	retries;
#endif	/* HAVE_LIBUSB_DETACH_KERNEL_DRIVER */

#if defined(HAVE_LIBUSB_KERNEL_DRIVER_ACTIVE) && defined(HAVE_LIBUSB_SET_AUTO_DETACH_KERNEL_DRIVER)
	/* Due to the way FreeBSD implements libusb_set_auto_detach_kernel_driver(),
	 * check to see if the kernel driver is active before setting the auto-detach flag.
	 * Otherwise, libusb_claim_interface() with the auto-detach flag only works if the driver is running as root. */
	ret = libusb_kernel_driver_active(udev, nut_usb_if_num);
	/* Is the kernel driver active? Consider the unimplemented return code to be equivalent to inactive here. */
	if (ret == 1) {
		upsdebugx(3, "%s: libusb_kernel_driver_active() returned 1 (driver active).", __func__);
		/* Try the auto-detach kernel driver method.
		 * This function is not available on FreeBSD 10.1-10.3. */
		ret = libusb_set_auto_detach_kernel_driver(udev, 1);
		if (ret != LIBUSB_SUCCESS)
			upsdebugx(1, "%s: failed to set kernel driver auto-detach driver flag for USB device (%s).", __func__, libusb_strerror(ret));
		else
			upsdebugx(2, "%s: successfully set kernel driver auto-detach flag.", __func__);
	} else {
		upsdebugx(3, "%s: libusb_kernel_driver_active() returned %d (%s).", __func__, ret, ret ? libusb_strerror(ret) : "no driver active");
	}
#endif	/* HAVE_LIBUSB_KERNEL_DRIVER_ACTIVE + HAVE_LIBUSB_SET_AUTO_DETACH_KERNEL_DRIVER */

#if HAVE_LIBUSB_DETACH_KERNEL_DRIVER
	/* Then, try the explicit detach method.
	 * This function is available on FreeBSD 10.1-10.3. */
	retries = 3;
	while ((ret = libusb_claim_interface(udev, nut_usb_if_num)) != LIBUSB_SUCCESS) {

		upsdebugx(2, "%s: failed to claim USB device (%s).", __func__, libusb_strerror(ret));

		if (retries-- == 0)
			return ret;

		ret = libusb_detach_kernel_driver(udev, nut_usb_if_num);
		if (ret == LIBUSB_SUCCESS)
			upsdebugx(2, "%s: detached kernel driver from USB device...", __func__);
		else if (ret == LIBUSB_ERROR_NOT_FOUND)
			upsdebugx(2, "%s: kernel driver already detached.", __func__);
		else
			upsdebugx(1, "%s: failed to detach kernel driver from USB device (%s).", __func__, libusb_strerror(ret));

	}
#else	/* HAVE_LIBUSB_DETACH_KERNEL_DRIVER */
	if ((ret = libusb_claim_interface(udev, nut_usb_if_num)) != LIBUSB_SUCCESS)
		return ret;
#endif	/* HAVE_LIBUSB_DETACH_KERNEL_DRIVER */

	return LIBUSB_SUCCESS;
}

/** @brief Set the USB alternate interface, if needed.
 *
 * In NUT 2.7.2 and earlier, the following (libusb-0.1) call was made unconditionally:
 *
 *     usb_set_altinterface(udev, 0);
 *
 * Although harmless on Linux and *BSD, this extra call prevents old Tripp Lite devices from working on Mac OS X
 * (presumably the OS is already setting altinterface to 0).
 *
 * @return @ref LIBUSB_SUCCESS, on success,
 * @return a @ref libusb_error "LIBUSB_ERROR" code, on errors. */
static int	nut_usb_set_altinterface(
	libusb_device_handle	*udev	/**< [in] handle of an already opened device */
) {
	int		 ret,
			 altinterface = 0;
	const char	*alt_string;

	if (!testvar("usb_set_altinterface")) {
		upsdebugx(3, "%s: skipped libusb_set_interface_alt_setting(udev, %d, 0).", __func__, nut_usb_if_num);
		return LIBUSB_SUCCESS;
	}

	alt_string = getval("usb_set_altinterface");
	if (alt_string && !str_to_int(alt_string, &altinterface, 10))
		upslogx(LOG_WARNING, "%s: could not convert to an int the provided value (%s) for 'usb_set_altinterface' (%s).", __func__, alt_string, strerror(errno));
	else if (altinterface < 0 || altinterface > 255)
		upslogx(LOG_WARNING, "%s: setting bAlternateInterface to %d will probably not work.", __func__, altinterface);

	/* Set default interface */
	upsdebugx(2, "%s: calling libusb_set_interface_alt_setting(udev, %d, %d).", __func__, nut_usb_if_num, altinterface);
	ret = libusb_set_interface_alt_setting(udev, nut_usb_if_num, altinterface);
	if (ret != LIBUSB_SUCCESS)
		upslogx(LOG_WARNING, "%s: libusb_set_interface_alt_setting(udev, %d, %d) error (%s).", __func__, nut_usb_if_num, altinterface, libusb_strerror(ret));

	upslogx(LOG_NOTICE, "%s: libusb_set_interface_alt_setting() should not be necessary - please email the nut-upsdev list with information about your device.", __func__);

	return ret;
}

/** @brief Log errors, if any, of nut_libusb_get_report(), nut_libusb_set_report(), nut_libusb_get_string(), nut_libusb_get_interrupt().
 * @return *ret*. */
static int	nut_usb_logerror(
	const int	 ret,	/**< [in] a libusb return code (negative, possibly a @ref libusb_error "LIBUSB_ERROR" code, on errors) */
	const char	*desc	/**< [in] text to print alongside the short description of the error */
) {
	if (ret >= 0)
		return ret;

	switch (ret)
	{
	case LIBUSB_ERROR_INVALID_PARAM:
	case LIBUSB_ERROR_INTERRUPTED:
	case LIBUSB_ERROR_NO_MEM:
	case LIBUSB_ERROR_TIMEOUT:
	case LIBUSB_ERROR_OVERFLOW:
		upsdebugx(2, "%s: %s.", desc, libusb_strerror(ret));
		return ret;
	case LIBUSB_ERROR_BUSY:
	case LIBUSB_ERROR_NO_DEVICE:
	case LIBUSB_ERROR_ACCESS:
	case LIBUSB_ERROR_IO:
	case LIBUSB_ERROR_NOT_FOUND:
	case LIBUSB_ERROR_PIPE:
	case LIBUSB_ERROR_NOT_SUPPORTED:
	case LIBUSB_ERROR_OTHER:
	default:
		upslogx(LOG_DEBUG, "%s: %s.", desc, libusb_strerror(ret));
		return ret;
	}
}

/** @} ************************************************************************/


/** @name Communication subdriver implementation
 * @{ *************************************************************************/

/** @brief See usb_communication_subdriver_t::open(). */
static int	nut_libusb_open(
	libusb_device_handle	**udevp,
	USBDevice_t		 *curDevice,
	USBDeviceMatcher_t	 *matcher,
	int			(*callback)(
		libusb_device_handle	*udev,
		USBDevice_t		*hd,
		unsigned char		*rdbuf,
		int			 rdlen
	)
) {
	int		  ret;
	libusb_device	**devlist;
	ssize_t		  devcount,
			  devnum;

	/* libusb base init */
	if ((ret = libusb_init(NULL)) != LIBUSB_SUCCESS) {
		libusb_exit(NULL);
		fatalx(EXIT_FAILURE, "Failed to init libusb (%s).", libusb_strerror(ret));
	}

#ifndef __linux__	/* (confirmed to work on Solaris and FreeBSD) */
	/* If device is still open, close it.
	 * Note: causes a double free corruption in linux if device is detached! */
	if (*udevp)
		libusb_close(*udevp);
#endif	/* __linux__ */

	/* Process available devices (if any) until we get a match */
	devcount = libusb_get_device_list(NULL, &devlist);
	for (devnum = 0; devnum < devcount; devnum++) {

	/*	int				 if_claimed = 0;	*/

		libusb_device			*device = devlist[devnum];
		libusb_device_handle		*udev;

		char				 string[NUT_USB_MAX_STRING_SIZE];
		uint8_t				 bus;

		USBDeviceMatcher_t		*match;

		/* DEVICE descriptor */
		struct libusb_device_descriptor	 device_desc;

		/* HID descriptor */
		unsigned char			 hid_desc_buf[HID_DT_HID_SIZE];
		/* All devices use HID descriptor at index 0.
		 * However, some newer Eaton units have a light HID descriptor at index 0,
		 * and the full version is at index 1 (in which case, bcdDevice == 0x0202). */
		int				 hid_desc_index = 0;

		/* REPORT descriptor */
		unsigned char			 report_desc_buf[HID_DT_REPORT_SIZE_MAX];
		uint16_t			 report_desc_len,
						 rdlen1,
						 rdlen2;
		int				 got_rdlen1 = 0,
						 got_rdlen2 = 0;

		upsdebugx(2, "Checking device %lu of %lu.", devnum + 1, devcount);

		/* Get DEVICE descriptor */
		ret = libusb_get_device_descriptor(device, &device_desc);
		if (ret != LIBUSB_SUCCESS) {
			upsdebugx(2, "Unable to get DEVICE descriptor (%s).", libusb_strerror(ret));
			continue;
		}

		/* Open the device */
		ret = libusb_open(device, udevp);
		if (ret != LIBUSB_SUCCESS) {
			upsdebugx(2, "Failed to open device %04X:%04X (%s), skipping.", device_desc.idVendor, device_desc.idProduct, libusb_strerror(ret));
			continue;
		}
		udev = *udevp;

		/* Collect the identifying information of this device.
		 * Note that this is safe, because there's no need to claim an interface for this
		 * (and therefore we do not yet need to detach any kernel driver). */

		free(curDevice->Vendor);
		free(curDevice->Product);
		free(curDevice->Serial);
		free(curDevice->Bus);
		memset(curDevice, 0, sizeof(*curDevice));

		bus = libusb_get_bus_number(device);
		if ((curDevice->Bus = (char *)malloc(4)) == NULL)
			goto oom_error;
		sprintf(curDevice->Bus, "%03d", bus);
		curDevice->VendorID = device_desc.idVendor;
		curDevice->ProductID = device_desc.idProduct;
		curDevice->bcdDevice = device_desc.bcdDevice;

		if (device_desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(udev, device_desc.iManufacturer, (unsigned char*)string, sizeof(string));
			if (ret > 0 && (curDevice->Vendor = strdup(string)) == NULL)
				goto oom_error;
		}

		if (device_desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(udev, device_desc.iProduct, (unsigned char*)string, sizeof(string));
			if (ret > 0 && (curDevice->Product = strdup(string)) == NULL)
				goto oom_error;
		}

		if (device_desc.iSerialNumber) {
			ret = libusb_get_string_descriptor_ascii(udev, device_desc.iSerialNumber, (unsigned char*)string, sizeof(string));
			if (ret > 0 && (curDevice->Serial = strdup(string)) == NULL)
				goto oom_error;
		}

		upsdebugx(2, "- VendorID: %04x", curDevice->VendorID);
		upsdebugx(2, "- ProductID: %04x", curDevice->ProductID);
		upsdebugx(2, "- Manufacturer: %s", curDevice->Vendor ? curDevice->Vendor : "unknown");
		upsdebugx(2, "- Product: %s", curDevice->Product ? curDevice->Product : "unknown");
		upsdebugx(2, "- Serial Number: %s", curDevice->Serial ? curDevice->Serial : "unknown");
		upsdebugx(2, "- Bus: %s", curDevice->Bus);
		upsdebugx(2, "- Device release number: %04x", curDevice->bcdDevice);

		/* Check device against the supplied matcher */
		upsdebugx(2, "Trying to match device");
		for (match = matcher; match; match = match->next) {
			ret = match->match_function(curDevice, match->privdata);
			if (ret == 0) {
				upsdebugx(2, "Device does not match - skipping");
				goto next_device;
			}
			if (ret == -1) {
				libusb_free_device_list(devlist, 1);
				fatal_with_errno(EXIT_FAILURE, "matcher");
			}
			if (ret == -2) {
				upsdebugx(2, "matcher: unspecified error");
				goto next_device;
			}
		}
		upsdebugx(2, "Device matches");

		/* Now that we have matched the device we wanted, claim it. */
		ret = nut_usb_claim_interface(udev);
		if (ret != LIBUSB_SUCCESS) {
			libusb_free_device_list(devlist, 1);
			fatalx(EXIT_FAILURE, "Can't claim USB device %04x:%04x (%s).", curDevice->VendorID, curDevice->ProductID, libusb_strerror(ret));
		}
	/*	if_claimed = 1;	*/
		upsdebugx(2, "Claimed interface %d successfully", nut_usb_if_num);

		/* Set the USB alternate setting for the interface, if needed. */
		nut_usb_set_altinterface(udev);

		/* Done, if no callback is provided */
		if (!callback) {
			libusb_free_device_list(devlist, 1);
			return LIBUSB_SUCCESS;
		}

		/* FIXME: extend to Eaton OEMs (HP, IBM, ...) */
		if ((curDevice->VendorID == 0x463) && (curDevice->bcdDevice == 0x0202)) {
			upsdebugx(1, "Eaton device v2.02. Using full report descriptor");
			hid_desc_index = 1;
		}

		/* Get HID descriptor */

		/* FIRST METHOD: ask for HID descriptor directly. */
		ret = libusb_control_transfer(
			udev,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_INTERFACE,
			LIBUSB_REQUEST_GET_DESCRIPTOR,
			(LIBUSB_DT_HID << 8) | hid_desc_index,
			nut_usb_if_num,
			hid_desc_buf,
			sizeof(hid_desc_buf),
			USB_TIMEOUT
		);
		if (ret < 0) {
			upsdebugx(2, "Unable to get HID descriptor (%s)", libusb_strerror((enum libusb_error)ret));
		} else if ((size_t)ret < sizeof(hid_desc_buf)) {
			upsdebugx(2, "HID descriptor too short (expected %lu, got %d)", sizeof(hid_desc_buf), ret);
		} else {
			upsdebug_hex(3, "HID descriptor, method 1", hid_desc_buf, sizeof(hid_desc_buf));
			rdlen1 = hid_desc_buf[7] | (hid_desc_buf[8] << 8);
			got_rdlen1 = 1;
			upsdebugx(3, "HID descriptor length (method 1) %u", rdlen1);
		}
		if (!got_rdlen1)
			upsdebugx(2, "Warning: HID descriptor, method 1 failed");

		/* SECOND METHOD: find HID descriptor among "extra" bytes of interface descriptor, i.e., bytes tucked onto the end of descriptor 2.
		 * Note: on some broken devices (e.g. Tripp Lite Smart1000LCD), only this second method gives the correct result. */
		if ((curDevice->VendorID != 0x463) && (curDevice->bcdDevice != 0x0202)) {
			upsdebugx(2, "Eaton device v2.02. Skipping method 2 for retrieving HID descriptor.");
		} else {
			struct libusb_config_descriptor	*conf_desc;

			/* For now, we always assume configuration 0, interface 0, altsetting 0. */
			ret = libusb_get_config_descriptor(device, 0, &conf_desc);
			if (ret != LIBUSB_SUCCESS) {
				upsdebugx(2, "%s: unable to get the first configuration descriptor (%s).", __func__, libusb_strerror(ret));
			} else {
				const struct libusb_interface_descriptor	*if_desc = &(conf_desc->interface[0].altsetting[0]);
				int						 i;

				for (i = 0; i <= if_desc->extra_length - HID_DT_HID_SIZE; i += if_desc->extra[i]) {
					const unsigned char	*hid_desc_ptr;

					upsdebugx(4, "i=%d, extra[i]=%02x, extra[i+1]=%02x", i, if_desc->extra[i], if_desc->extra[i+1]);

					/* Not big enough to be a HID descriptor... */
					if (if_desc->extra[i] < HID_DT_HID_SIZE)
						continue;
					/* Definitely not a HID descriptor... */
					if (if_desc->extra[i + 1] != LIBUSB_DT_HID)
						continue;

					/* HID descriptor found! */
					hid_desc_ptr = &if_desc->extra[i];
					upsdebug_hex(4, "HID descriptor, method 2", hid_desc_ptr, HID_DT_HID_SIZE);
					rdlen2 = hid_desc_ptr[7] | (hid_desc_ptr[8] << 8);
					got_rdlen2 = 1;
					upsdebugx(3, "HID descriptor length (method 2) %u", rdlen2);
					break;
				}
				/* We can now free the config descriptor. */
				libusb_free_config_descriptor(conf_desc);
			}
			if (!got_rdlen2)
				upsdebugx(2, "Warning: HID descriptor, method 2 failed");
		}

		if (!got_rdlen1 && !got_rdlen2) {
			upsdebugx(2, "Unable to retrieve any HID descriptor");
			goto next_device;
		}

		if (got_rdlen1 && got_rdlen2 && rdlen1 != rdlen2)
			upsdebugx(2, "Warning: two different HID descriptors retrieved (Reportlen = %u vs. %u)", rdlen1, rdlen2);

		/* When available, always choose the second value, as it seems to be more reliable
		 * (it is the one reported e.g. by lsusb).
		 * Note: if the need arises, we can change this to use the maximum of the two values instead. */
		report_desc_len = got_rdlen2 ? rdlen2 : rdlen1;
		upsdebugx(2, "HID descriptor length %d", report_desc_len);

		if (report_desc_len > sizeof(report_desc_buf)) {
			upsdebugx(2, "HID descriptor too long %u (max %lu)", report_desc_len, sizeof(report_desc_buf));
			goto next_device;
		}

		/* Get REPORT descriptor */
		ret = libusb_control_transfer(
			udev,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_INTERFACE,
			LIBUSB_REQUEST_GET_DESCRIPTOR,
			(LIBUSB_DT_REPORT << 8) | hid_desc_index,
			nut_usb_if_num,
			report_desc_buf,
			report_desc_len,
			USB_TIMEOUT
		);

		if (ret < 0) {
			upsdebugx(2, "Unable to get Report descriptor (%s)", libusb_strerror(ret));
			goto next_device;
		}

		if (ret < report_desc_len) {
			upsdebugx(2, "Warning: report descriptor too short (expected %u, got %d)", report_desc_len, ret);
			/* Correct length, if necessary */
			report_desc_len = ret;
		}

		upsdebugx(2, "Report descriptor retrieved (Reportlen = %u)", report_desc_len);

		if (!callback(udev, curDevice, report_desc_buf, report_desc_len)) {
			upsdebugx(2, "Caller doesn't like this device");
			goto next_device;
		}

		upsdebugx(2, "Found HID device");
		fflush(stdout);
		libusb_free_device_list(devlist, 1);

		return LIBUSB_SUCCESS;

	next_device:
		/* libusb_release_interface() sometimes blocks and goes into uninterruptible sleep. So don't do it. */
	/*	if (if_claimed)
			libusb_release_interface(udev, nut_usb_if_num);	*/
		libusb_close(udev);
		continue;

	oom_error:
		libusb_free_device_list(devlist, 1);
		fatal_with_errno(EXIT_FAILURE, "Out of memory");

	}

	*udevp = NULL;
	libusb_free_device_list(devlist, 1);
	upsdebugx(2, "No appropriate HID device found");
	fflush(stdout);

	return LIBUSB_ERROR_NOT_FOUND;
}

/** @brief See usb_communication_subdriver_t::close(). */
static void	nut_libusb_close(
	libusb_device_handle	*udev
) {
	if (!udev)
		return;

	/* libusb_release_interface() sometimes blocks and goes into uninterruptible sleep. So don't do it. */
/*	libusb_release_interface(udev, nut_usb_if_num);	*/
	libusb_close(udev);
	libusb_exit(NULL);
}

/** @brief See usb_communication_subdriver_t::get_report(). */
static int	nut_libusb_get_report(
	libusb_device_handle	*udev,
	int			 ReportId,
	unsigned char		*raw_buf,
	int			 ReportSize
) {
	int	ret;

	upsdebugx(4, "Entering libusb_get_report");

	if (!udev)
		return LIBUSB_ERROR_INVALID_PARAM;

	ret = libusb_control_transfer(
		udev,
		LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
		HID_REQUEST_GET_REPORT,
		(HID_RT_FEATURE << 8) | ReportId,
		nut_usb_if_num,
		raw_buf,
		ReportSize,
		USB_TIMEOUT
	);

	/* Ignore "protocol stall" (for unsupported request) on control endpoint */
	if (ret == LIBUSB_ERROR_PIPE)
		return 0;

	return nut_usb_logerror(ret, __func__);
}

/** @brief See usb_communication_subdriver_t::set_report(). */
static int	nut_libusb_set_report(
	libusb_device_handle	*udev,
	int			 ReportId,
	unsigned char		*raw_buf,
	int			 ReportSize
) {
	int	ret;

	if (!udev)
		return LIBUSB_ERROR_INVALID_PARAM;

	ret = libusb_control_transfer(
		udev,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
		HID_REQUEST_SET_REPORT,
		(HID_RT_FEATURE << 8) | ReportId,
		nut_usb_if_num,
		raw_buf,
		ReportSize,
		USB_TIMEOUT
	);

	/* Ignore "protocol stall" (for unsupported request) on control endpoint */
	if (ret == LIBUSB_ERROR_PIPE)
		return 0;

	return nut_usb_logerror(ret, __func__);
}

/** @brief See usb_communication_subdriver_t::get_string(). */
static int	nut_libusb_get_string(
	libusb_device_handle	*udev,
	int			 StringIdx,
	char			*buf,
	size_t			 buflen
) {
	int	ret;

	if (!udev)
		return LIBUSB_ERROR_INVALID_PARAM;

	ret = libusb_get_string_descriptor_ascii(udev, StringIdx, (unsigned char*)buf, buflen);

	return nut_usb_logerror(ret, __func__);
}

/** @brief See usb_communication_subdriver_t::get_interrupt(). */
static int	nut_libusb_get_interrupt(
	libusb_device_handle	*udev,
	unsigned char		*buf,
	int			 bufsize,
	int			 timeout
) {
	int	ret;

	if (!udev)
		return LIBUSB_ERROR_INVALID_PARAM;

	/* FIXME: hardcoded interrupt EP => need to get EP descr for IF descr */
	ret = libusb_interrupt_transfer(udev, LIBUSB_ENDPOINT_IN | 1, buf, bufsize, &bufsize, timeout);

	if (ret == LIBUSB_SUCCESS)
		return bufsize;

	/* Clear stall condition */
	if (ret == LIBUSB_ERROR_PIPE)
		ret = libusb_clear_halt(udev, LIBUSB_ENDPOINT_IN | 1);

	return nut_usb_logerror(ret, __func__);
}

/** @brief See usb_communication_subdriver_t::add_nutvars(). */
void	nut_libusb_add_nutvars(void)
{
	const struct libusb_version	*v = libusb_get_version();

	addvar(VAR_VALUE, "vendor", "Regular expression to match UPS Manufacturer string");
	addvar(VAR_VALUE, "product", "Regular expression to match UPS Product string");
	addvar(VAR_VALUE, "serial", "Regular expression to match UPS Serial number");

	addvar(VAR_VALUE, "vendorid", "Regular expression to match UPS Manufacturer numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "productid", "Regular expression to match UPS Product numerical ID (4 digits hexadecimal)");

	addvar(VAR_VALUE, "bus", "Regular expression to match USB bus name");
	addvar(VAR_VALUE, "usb_set_altinterface", "Force redundant call to libusb_set_interface_alt_setting() (value=bAlternateSetting; default=0)");

#ifdef LIBUSB_API_VERSION
	dstate_setinfo("driver.version.usb", "libusb-%u.%u.%u (API: 0x%x)", v->major, v->minor, v->micro, LIBUSB_API_VERSION);
#else  /* LIBUSB_API_VERSION */
	dstate_setinfo("driver.version.usb", "libusb-%u.%u.%u", v->major, v->minor, v->micro);
#endif /* LIBUSB_API_VERSION */
}

usb_communication_subdriver_t	usb_subdriver = {
	NUT_USB_DRIVER_VERSION,
	NUT_USB_DRIVER_NAME,
	nut_libusb_open,
	nut_libusb_close,
	nut_libusb_get_report,
	nut_libusb_set_report,
	nut_libusb_get_string,
	nut_libusb_get_interrupt,
	nut_libusb_add_nutvars
};

/** @} ************************************************************************/
