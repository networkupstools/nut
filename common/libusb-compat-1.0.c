/**
 * @file
 * @brief	Actual implementation of libusb-compat-1.0 functions.
 * @copyright	@parblock
 * Copyright (C):
 * - 2018 -- Daniele Pezzini (<hyouko@gmail.com>)
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

#include "libusb-compat-1.0.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <usb.h>

#include "common.h"	/* for upsdebugx() (and SMALLBUF) */
#include "config.h"	/* for HAVE_USB_DETACH_KERNEL_DRIVER_NP/DYNAMICALLY_LOAD_LIBUSB_0_1/WITH_LIBLTDL flags */
#include "str.h"

#if defined(DYNAMICALLY_LOAD_LIBUSB_0_1) && defined(WITH_LIBLTDL)
#include "dl_libusb-0.1.h"
#endif	/* DYNAMICALLY_LOAD_LIBUSB_0_1 + WITH_LIBLTDL */

/** @name Globals
 * @{ *************************************************************************/

/** @brief Library version
 *
 * We're not really libusb 1.0, don't pretend to be it.
 * @note increment *micro* on changes. */
static const struct libusb_version	libusb_version_internal = {
	0,	/* major	*/
	1,	/* minor	*/
	4,	/* micro: *we*	*/
	0,	/* nano		*/
	"",	/* rc		*/
	""	/* describe	*/
};

/** @brief Whether (non-zero) or not (zero) this lib should trust libusb 0.1 error codes.
 *
 * Some libusb 0.1 implementations don't return `-errno` on errors, but `-1`
 * (and, at least on FreeBSD, usb_strerror() is not of any utility).
 * In such cases we'll try to go deeper and see if an `errno` is set. */
#ifndef __FreeBSD__
static const int	trust_libusb01_error_codes = 1;
#else	/* __FreeBSD__ */
static const int	trust_libusb01_error_codes = 0;
#endif	/* __FreeBSD__ */

/** @} ************************************************************************/


/** @name libusb 1.0 opaque types and friends
 * @{ *************************************************************************/

/** @brief Handle for libusb_open()'d devices. */
struct libusb_device_handle {
	/** @brief Corresponding libusb 0.1 device handle. */
	usb_dev_handle	*libusb01_dev_handle;

	/** @brief Most recently successfully claimed (and not yet released) interface.
	 *
	 * Since libusb 0.1 only tracks the most recently successfully claimed interface and, on it, performs the changing of the `bAlternateSetting` value,
	 * we need to do it ourselves, to avoid setting the value on a wrong interface (i.e. on the most recently claimed one),
	 * when, for example, libusb_set_interface_alt_setting() is called with an interface other than the most recently claimed one.
	 *
	 * Because of that, this field should be given a value of @ref NONE_OR_UNKNOWN_INTERFACE
	 * - when no interface has been successfully claimed yet,
	 * - or **the most recently claimed one** has been successfully released.
	 *
	 * On the other hand, this field should not be touched when libusb_release()'ing another interface (i.e. not the last one):
	 * - while most implementations alter the tracked interface on every successfull operation (i.e. both claiming *and* releasing),
	 *   putting their internally tracked interface in an unusable status after any release, and making our libusb_set_interface_alt_setting() fail
	 *   (so, we don't need to set this to @ref NONE_OR_UNKNOWN_INTERFACE ourselves, as the function will fail anyway when called after any release),
	 * - some implementations (e.g. FreeBSD's) don't alter the tracked interface when releasing, but only when claiming:
	 *   a subsequent call to libusb_set_interface_alt_setting() should succeed and be executed on the right interface,
	 *   if done on the most recently claimed one, even if another interface has been released since the last claiming. */
	int		 last_claimed_interface;
};

/** @brief Default value for libusb_device_handle::last_claimed_interface. */
#define NONE_OR_UNKNOWN_INTERFACE	-1

/** @} ************************************************************************/


/** @name Support functions
 * @{ *************************************************************************/

/** @brief Call upsdebugx() with the names of this lib and of the 'caller' function.
 *
 * @param[in] fmt	(optional) string literal to pass to upsdebugx(), after the lib and function names.
 *			If it is a printf() format holding conversion specifications, arguments for them must be specified.
 * @param[in] ...	(optional) arguments specifying data as per *fmt*.
 *
 * @todo Standardize the use of (USB-related) debug levels in all the tree
 * (see also https://github.com/networkupstools/nut/issues/211). */
#define dbg(fmt, ...)	upsdebugx(3, "libusb-compat-1.0, %s(): " fmt, __func__, ##__VA_ARGS__)

/** @brief Morph a libusb 0.1 return value into a libusb 1.0 one.
 *
 * This function is meant to be used with those libusb 0.1 functions that return:
 * - a zero or positive value, if successful,
 * - a negative value, in case of errors.
 *
 * And to map that value to what would've been returned by libusb 1.0 functions that are expected to return:
 * - a zero (or LIBUSB_SUCCESS) or positive value, if successful,
 * - a LIBUSB_ERROR code, in case of errors.
 *
 * As such, if *ret* is not negative, we assume a no-error condition and it is returned untouched and unmolested.
 * Otherwise, we assume an error condition and, as libusb 0.1 error code, is used:
 * - if @ref trust_libusb01_error_codes evaluates to true (i.e. it's not `0`) or *ret* is not `-1`, *ret* (negated),
 * - otherwise, `errno`, if it is set.
 *
 * @return *ret*, when it is not negative,
 * @return a @ref libusb_error "LIBUSB_ERROR" code mapping the libusb 0.1 error code,
 * @return @ref LIBUSB_ERROR_OTHER, if a libusb 0.1 error code can't be identified. */
static int	libusb01_ret_to_libusb10_ret(
	const int	ret	/**< [in] a libusb 0.1 return value, negative (possibly `-errno`), if an error happened. */
) {
	int	e;

	if (ret >= 0)
		return ret;

	dbg("libusb 0.1 returned %d (%s)...", ret, usb_strerror());
	if (trust_libusb01_error_codes || ret != -1) {
		e = -ret;
		dbg("using it as error #%d (%s).", e, strerror(e));
	} else if (errno) {
		e = errno;
		dbg("ignoring it and using errno #%d (%s).", e, strerror(e));
	} else {
		dbg("cannot determine libusb 0.1 error.");
		return LIBUSB_ERROR_OTHER;
	}

	switch (e)
	{
	case 0:		/* This should never happen, but still... */
		return LIBUSB_SUCCESS;
	case EIO:
		return LIBUSB_ERROR_IO;
	case EINVAL:
		return LIBUSB_ERROR_INVALID_PARAM;
	case EACCES:
	case EPERM:
		return LIBUSB_ERROR_ACCESS;
	case ENODEV:
	case ENXIO:
		return LIBUSB_ERROR_NO_DEVICE;
	case ENOENT:
		return LIBUSB_ERROR_NOT_FOUND;
	case EBUSY:
		return LIBUSB_ERROR_BUSY;
	case ETIMEDOUT:
		return LIBUSB_ERROR_TIMEOUT;
	case EOVERFLOW:
		return LIBUSB_ERROR_OVERFLOW;
	case EPIPE:
		return LIBUSB_ERROR_PIPE;
	case EINTR:
		return LIBUSB_ERROR_INTERRUPTED;
	case ENOMEM:
		return LIBUSB_ERROR_NO_MEM;
	case ENOSYS:
		return LIBUSB_ERROR_NOT_SUPPORTED;
	default:
		return LIBUSB_ERROR_OTHER;
	}
}

/** @} ************************************************************************/


/** @name libusb 1.0 functions implementation
 * @{ *************************************************************************/

int	libusb_init(
	libusb_context	**ctx
) {
	const char	*debug_level_str = getenv("LIBUSB_DEBUG");
	int		 debug_level;
#if defined(DYNAMICALLY_LOAD_LIBUSB_0_1) && defined(WITH_LIBLTDL)
	char		 error[SMALLBUF];

	if (!dl_libusb01_init(error, sizeof(error))) {
		dbg("%s", error);
		return LIBUSB_ERROR_OTHER;
	}
#endif	/* DYNAMICALLY_LOAD_LIBUSB_0_1 + WITH_LIBLTDL */

	if (str_to_int(debug_level_str, &debug_level, 10))
		usb_set_debug(debug_level);

	usb_init();

	return LIBUSB_SUCCESS;
}

void	libusb_exit(
	libusb_context	*ctx
) {
#if defined(DYNAMICALLY_LOAD_LIBUSB_0_1) && defined(WITH_LIBLTDL)
	dl_libusb01_exit();
#endif	/* DYNAMICALLY_LOAD_LIBUSB_0_1 + WITH_LIBLTDL */
	return;
}

const struct libusb_version	*libusb_get_version(void)
{
	return &libusb_version_internal;
}

const char	*libusb_strerror(
	enum libusb_error	errcode
) {
	switch (errcode)
	{
	case LIBUSB_SUCCESS:
		return "Success";
	case LIBUSB_ERROR_IO:
		return "Input/Output Error";
	case LIBUSB_ERROR_INVALID_PARAM:
		return "Invalid parameter";
	case LIBUSB_ERROR_ACCESS:
		return "Access denied (insufficient permissions)";
	case LIBUSB_ERROR_NO_DEVICE:
		return "No such device (it may have been disconnected)";
	case LIBUSB_ERROR_NOT_FOUND:
		return "Entity not found";
	case LIBUSB_ERROR_BUSY:
		return "Resource busy";
	case LIBUSB_ERROR_TIMEOUT:
		return "Operation timed out";
	case LIBUSB_ERROR_OVERFLOW:
		return "Overflow";
	case LIBUSB_ERROR_PIPE:
		return "Pipe error";
	case LIBUSB_ERROR_INTERRUPTED:
		return "System call interrupted (perhaps due to signal)";
	case LIBUSB_ERROR_NO_MEM:
		return "Insufficient memory";
	case LIBUSB_ERROR_NOT_SUPPORTED:
		return "Operation not supported or unimplemented on this platform";
	case LIBUSB_ERROR_OTHER:
		return "Other error";
	default:
		return "Unknown error";
	}
}

ssize_t	libusb_get_device_list(
	libusb_context	  *ctx,
	libusb_device	***list
) {
	struct usb_device	 *dev,
				**devlist;
	struct usb_bus		 *bus;
	ssize_t			  i,
				  len;
	int			  ret;

	if (
		(ret = usb_find_busses()) < 0 ||
		(ret = usb_find_devices()) < 0
	)
		return libusb01_ret_to_libusb10_ret(ret);

	for (bus = usb_get_busses(), len = 0; bus; bus = bus->next)
		for (dev = bus->devices; dev; dev = dev->next)
			len++;

	devlist = calloc(len + 1, sizeof(*devlist));
	if (!devlist)
		return LIBUSB_ERROR_NO_MEM;

	devlist[len] = NULL;
	for (bus = usb_get_busses(), i = 0; bus; bus = bus->next)
		for (dev = bus->devices; dev && i < len; dev = dev->next, i++)
			devlist[i] = dev;

	*list = (libusb_device **)devlist;
	return len;
}

void	libusb_free_device_list(
	libusb_device	**list,
	int		  unref_devices
) {
	if (!list)
		return;
	free(list);
}

uint8_t	libusb_get_bus_number(
	libusb_device	*dev
) {
	unsigned short		 bus_number;
	int			 length = 0;
	struct usb_device	*device = (struct usb_device *)dev;

	if (
		str_to_ushort_strict(device->bus->dirname, &bus_number, 10) &&
		bus_number <= UINT8_MAX
	)
		return bus_number;

	/* In FreeBSD (possibly elsewhere), dirname is hardcoded to '/dev/usb',
	 * try filename (on FreeBSD "/dev/ugen<bus-number>.<dev-number>") */
	if (
		sscanf(device->filename, "/dev/ugen%hu.%*u%n", &bus_number, &length) == 1 &&
		(size_t)length == strlen(device->filename) &&
		bus_number <= UINT8_MAX
	)
		return bus_number;

	dbg("cannot determine bus number.");
	return 0;
}

int	libusb_open(
	libusb_device		 *dev,
	libusb_device_handle	**dev_handle
) {
	*dev_handle = calloc(1, sizeof(**dev_handle));
	if (!*dev_handle)
		return LIBUSB_ERROR_NO_MEM;

	errno = 0;
	(*dev_handle)->libusb01_dev_handle = usb_open((struct usb_device *)dev);

	if (!(*dev_handle)->libusb01_dev_handle) {
		free(*dev_handle);
		*dev_handle = NULL;
		if (errno)
			return libusb01_ret_to_libusb10_ret(-errno);
		return LIBUSB_ERROR_OTHER;
	}

	(*dev_handle)->last_claimed_interface = NONE_OR_UNKNOWN_INTERFACE;

	return LIBUSB_SUCCESS;
}

void	libusb_close(
	libusb_device_handle	*dev_handle
) {
	usb_close(dev_handle->libusb01_dev_handle);
	free(dev_handle);
}

int	libusb_set_configuration(
	libusb_device_handle	*dev_handle,
	int			 configuration
) {
	int	ret;

	errno = 0;
	ret = usb_set_configuration(
		dev_handle->libusb01_dev_handle,
		configuration
	);

	return libusb01_ret_to_libusb10_ret(ret);
}

int	libusb_claim_interface(
	libusb_device_handle	*dev_handle,
	int			 interface_number
) {
	int	ret;

	if (interface_number < 0)
		return LIBUSB_ERROR_INVALID_PARAM;

	errno = 0;
	ret = usb_claim_interface(
		dev_handle->libusb01_dev_handle,
		interface_number
	);

	if (ret < 0)
		return libusb01_ret_to_libusb10_ret(ret);

	dev_handle->last_claimed_interface = interface_number;
	return LIBUSB_SUCCESS;
}

int	libusb_release_interface(
	libusb_device_handle	*dev_handle,
	int			 interface_number
) {
	int	ret;

	if (interface_number < 0)
		return LIBUSB_ERROR_INVALID_PARAM;

	errno = 0;
	ret = usb_release_interface(
		dev_handle->libusb01_dev_handle,
		interface_number
	);

	if (ret < 0)
		return libusb01_ret_to_libusb10_ret(ret);

	if (dev_handle->last_claimed_interface == interface_number)
		dev_handle->last_claimed_interface = NONE_OR_UNKNOWN_INTERFACE;

	return LIBUSB_SUCCESS;
}

int	libusb_set_interface_alt_setting(
	libusb_device_handle	*dev_handle,
	int			 interface_number,
	int			 alternate_setting
) {
	int	ret;

	if (
		interface_number < 0 ||
		interface_number != dev_handle->last_claimed_interface
	)
		return LIBUSB_ERROR_INVALID_PARAM;

	errno = 0;
	ret = usb_set_altinterface(
		dev_handle->libusb01_dev_handle,
		alternate_setting
	);

	return libusb01_ret_to_libusb10_ret(ret);
}

int	libusb_clear_halt(
	libusb_device_handle	*dev_handle,
	unsigned char		 endpoint
) {
	int	ret;

	errno = 0;
	ret = usb_clear_halt(
		dev_handle->libusb01_dev_handle,
		endpoint
	);

	return libusb01_ret_to_libusb10_ret(ret);
}

int	libusb_reset_device(
	libusb_device_handle	*dev_handle
) {
	int	ret;

	errno = 0;
	ret = usb_reset(
		dev_handle->libusb01_dev_handle
	);

	return libusb01_ret_to_libusb10_ret(ret);
}

int	libusb_detach_kernel_driver(
	libusb_device_handle	*dev_handle,
	int			 interface_number
) {
#ifdef HAVE_USB_DETACH_KERNEL_DRIVER_NP
	int	ret;
#endif	/* HAVE_USB_DETACH_KERNEL_DRIVER_NP */

	if (interface_number < 0)
		return LIBUSB_ERROR_INVALID_PARAM;

#ifndef HAVE_USB_DETACH_KERNEL_DRIVER_NP
	return LIBUSB_ERROR_NOT_SUPPORTED;
#else	/* HAVE_USB_DETACH_KERNEL_DRIVER_NP */
	errno = 0;
	ret = usb_detach_kernel_driver_np(
		dev_handle->libusb01_dev_handle,
		interface_number
	);

	return libusb01_ret_to_libusb10_ret(ret);
#endif	/* HAVE_USB_DETACH_KERNEL_DRIVER_NP */
}

int	libusb_control_transfer(
	libusb_device_handle	*dev_handle,
	uint8_t			 bmRequestType,
	uint8_t			 bRequest,
	uint16_t		 wValue,
	uint16_t		 wIndex,
	unsigned char		*data,
	uint16_t		 wLength,
	unsigned int		 timeout
) {
	int	ret;

	errno = 0;
	ret = usb_control_msg(
		dev_handle->libusb01_dev_handle,
		bmRequestType,
		bRequest,
		wValue,
		wIndex,
		(char *)data,
		wLength,
		timeout
	);

	return libusb01_ret_to_libusb10_ret(ret);
}

int	libusb_bulk_transfer(
	libusb_device_handle	*dev_handle,
	unsigned char		 endpoint,
	unsigned char		*data,
	int			 length,
	int			*transferred,
	unsigned int		 timeout
) {
	int	ret;

	errno = 0;
	if (endpoint & LIBUSB_ENDPOINT_IN)
		ret = usb_bulk_read(
			dev_handle->libusb01_dev_handle,
			endpoint,
			(char *)data,
			length,
			timeout
		);
	else
		ret = usb_bulk_write(
			dev_handle->libusb01_dev_handle,
			endpoint,
			(char *)data,
			length,
			timeout
		);

	if (ret < 0)
		return libusb01_ret_to_libusb10_ret(ret);

	if (transferred)
		*transferred = ret;
	return LIBUSB_SUCCESS;
}

int	libusb_interrupt_transfer(
	libusb_device_handle	*dev_handle,
	unsigned char		 endpoint,
	unsigned char		*data,
	int			 length,
	int			*transferred,
	unsigned int		 timeout
) {
	int	ret;

	errno = 0;
	if (endpoint & LIBUSB_ENDPOINT_IN)
		ret = usb_interrupt_read(
			dev_handle->libusb01_dev_handle,
			endpoint,
			(char *)data,
			length,
			timeout
		);
	else
		ret = usb_interrupt_write(
			dev_handle->libusb01_dev_handle,
			endpoint,
			(char *)data,
			length,
			timeout
		);

	if (ret < 0)
		return libusb01_ret_to_libusb10_ret(ret);

	if (transferred)
		*transferred = ret;
	return LIBUSB_SUCCESS;
}

int	libusb_get_device_descriptor(
	libusb_device			*dev,
	struct libusb_device_descriptor	*desc
) {
	struct usb_device	*device = (struct usb_device *)dev;

	memcpy(
		(unsigned char *)desc,
		(unsigned char *)&device->descriptor,
		sizeof(device->descriptor)
	);
	return LIBUSB_SUCCESS;
}

int	libusb_get_config_descriptor(
	libusb_device			 *dev,
	uint8_t				  config_index,
	struct libusb_config_descriptor	**config
) {
	struct usb_device		*device = (struct usb_device *)dev;
	struct usb_config_descriptor	*l01_config;
	struct libusb_config_descriptor	*l10_config;
	int				 iface;

	if (config_index >= device->descriptor.bNumConfigurations)
		return LIBUSB_ERROR_NOT_FOUND;

	if (!device->config)
		return LIBUSB_ERROR_OTHER;

	l01_config = &device->config[config_index];

	l10_config = calloc(1, sizeof(*l10_config));
	if (!l10_config)
		return LIBUSB_ERROR_NO_MEM;

	/* libusb_config_descriptor.bLength ... libusb_config_descriptor.MaxPower */
	memcpy(l10_config, l01_config, USB_DT_CONFIG_SIZE);

	/* libusb_config_descriptor.interface */
	l10_config->interface = calloc(l01_config->bNumInterfaces, sizeof(*l10_config->interface));
	if (!l10_config->interface) {
		free(l10_config);
		return LIBUSB_ERROR_NO_MEM;
	}
	for (iface = 0; iface < l01_config->bNumInterfaces; iface++) {
		struct usb_interface	*l01_iface = &l01_config->interface[iface];
		struct libusb_interface	*l10_iface = (struct libusb_interface *)&l10_config->interface[iface];
		int			 altsetting;

		/* libusb_interface.num_altsetting */
		l10_iface->num_altsetting = l01_iface->num_altsetting;

		/* libusb_interface.altsetting */
		l10_iface->altsetting = calloc(l01_iface->num_altsetting, sizeof(*l10_iface->altsetting));
		if (!l10_iface->altsetting) {
			libusb_free_config_descriptor(l10_config);
			return LIBUSB_ERROR_NO_MEM;
		}
		for (altsetting = 0; altsetting < l01_iface->num_altsetting; altsetting++) {
			struct usb_interface_descriptor		*l01_altsetting = &l01_iface->altsetting[altsetting];
			struct libusb_interface_descriptor	*l10_altsetting = (struct libusb_interface_descriptor *)&l10_iface->altsetting[altsetting];
			int					 ep;

			/* libusb_interface_descriptor.bLength ... libusb_interface_descriptor.iInterface */
			memcpy(l10_altsetting, l01_altsetting, USB_DT_INTERFACE_SIZE);

			/* libusb_interface_descriptor.endpoint */
			l10_altsetting->endpoint = calloc(l01_altsetting->bNumEndpoints, sizeof(*l10_altsetting->endpoint));
			if (!l10_altsetting->endpoint) {
				libusb_free_config_descriptor(l10_config);
				return LIBUSB_ERROR_NO_MEM;
			}
			for (ep = 0; ep < l01_altsetting->bNumEndpoints; ep++) {
				struct usb_endpoint_descriptor		*l01_ep = &l01_altsetting->endpoint[ep];
				struct libusb_endpoint_descriptor	*l10_ep = (struct libusb_endpoint_descriptor *)&l10_altsetting->endpoint[ep];

				/* libusb_endpoint_descriptor.bLength ... libusb_endpoint_descriptor.bSynchAddress */
				memcpy(l10_ep, l01_ep, USB_DT_ENDPOINT_AUDIO_SIZE);

				/* libusb_endpoint_descriptor.extra_length */
				l10_ep->extra_length = l01_ep->extralen;

				/* libusb_endpoint_descriptor.extra */
				if (l01_ep->extralen) {
					l10_ep->extra = malloc(l01_ep->extralen);
					if (!l10_ep->extra) {
						libusb_free_config_descriptor(l10_config);
						return LIBUSB_ERROR_NO_MEM;
					}
					memcpy((unsigned char *)l10_ep->extra, l01_ep->extra, l01_ep->extralen);
				}
			}

			/* libusb_interface_descriptor.extra_length */
			l10_altsetting->extra_length = l01_altsetting->extralen;

			/* libusb_interface_descriptor.extra */
			if (l01_altsetting->extralen) {
				l10_altsetting->extra = malloc(l01_altsetting->extralen);
				if (!l10_altsetting->extra) {
					libusb_free_config_descriptor(l10_config);
					return LIBUSB_ERROR_NO_MEM;
				}
				memcpy((unsigned char *)l10_altsetting->extra, l01_altsetting->extra, l01_altsetting->extralen);
			}
		}
	}

	/* libusb_config_descriptor.extra_length */
	l10_config->extra_length = l01_config->extralen;

	/* libusb_config_descriptor.extra */
	if (l01_config->extralen) {
		l10_config->extra = malloc(l01_config->extralen);
		if (!l10_config->extra) {
			libusb_free_config_descriptor(l10_config);
			return LIBUSB_ERROR_NO_MEM;
		}
		memcpy((unsigned char *)l10_config->extra, l01_config->extra, l01_config->extralen);
	}

	*config = l10_config;
	return LIBUSB_SUCCESS;
}

void	libusb_free_config_descriptor(
	struct libusb_config_descriptor	*config
) {
	int	iface_n;

	if (!config)
		return;

	/* libusb_config_descriptor.extra */
	if (config->extra)
		free((void *)config->extra);

	if (!config->interface) {
		free(config);
		return;
	}

	/* libusb_config_descriptor.interface */
	for (iface_n = 0; iface_n < config->bNumInterfaces; iface_n++) {
		const struct libusb_interface	*iface = &config->interface[iface_n];
		int				 altsetting_n;

		if (!iface->altsetting)
			continue;

		/* libusb_interface.altsetting */
		for (altsetting_n = 0; altsetting_n < iface->num_altsetting; altsetting_n++) {
			const struct libusb_interface_descriptor	*altsetting = &iface->altsetting[altsetting_n];
			int						 ep_n;

			/* libusb_interface_descriptor.extra */
			if (altsetting->extra)
				free((void *)altsetting->extra);

			if (!altsetting->endpoint)
				continue;

			/* libusb_interface_descriptor.endpoint */
			for (ep_n = 0; ep_n < altsetting->bNumEndpoints; ep_n++) {
				const struct libusb_endpoint_descriptor	*ep = &altsetting->endpoint[ep_n];

				if (ep->extra)
					free((void *)ep->extra);
			}
			free((void *)altsetting->endpoint);
		}
		free((void *)iface->altsetting);
	}
	free((void *)config->interface);

	free(config);
}

int	libusb_get_string_descriptor(
	libusb_device_handle	*dev_handle,
	uint8_t			 desc_index,
	uint16_t		 langid,
	unsigned char		*data,
	int			 length
) {
	int	ret;

	errno = 0;
	ret = usb_get_string(
		dev_handle->libusb01_dev_handle,
		desc_index,
		langid,
		(char *)data,
		length
	);

	return libusb01_ret_to_libusb10_ret(ret);
}

int	libusb_get_string_descriptor_ascii(
	libusb_device_handle	*dev_handle,
	uint8_t			 desc_index,
	unsigned char		*data,
	int			 length
) {
	int	ret;

	errno = 0;
	ret = usb_get_string_simple(
		dev_handle->libusb01_dev_handle,
		desc_index,
		(char *)data,
		length
	);

	return libusb01_ret_to_libusb10_ret(ret);
}

/** @} ************************************************************************/
