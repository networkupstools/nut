/**
 * @file
 * @brief	Dynamic loading of libusb 0.1.
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
 *
 * @note
 * This thing should not interfere with other concurrently lt_dlopen()'d modules.
 * Load lib and symbols with dl_libusb01_init(), unload with dl_libusb01_exit():
 * they'll take care of lt_dlinit()'ing/lt_dlexit()'ing a matching number of times,
 * in order to avoid that a surplus call to lt_dlexit() automatically unloads other lt_dlopen()'d modules.
 */

#ifndef DL_LIBUSB_0_1_H
#define DL_LIBUSB_0_1_H

#include <ltdl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <usb.h>

#include "common.h"	/* for get_libname() */
#include "config.h"	/* for HAVE_USB_DETACH_KERNEL_DRIVER_NP flag */

/** @name libusb 0.1 symbols
 * For each pointer an alias with the original symbol name is #define'd, so that already written code doesn't have to be modified.
 * @{ */
#define	usb_open			 (*dl_usb_open)
static usb_dev_handle			*(*dl_usb_open)(
	struct usb_device	*dev
);
#define	usb_close			 (*dl_usb_close)
static int				 (*dl_usb_close)(
	usb_dev_handle		*dev
);
#define	usb_get_string			 (*dl_usb_get_string)
static int				 (*dl_usb_get_string)(
	usb_dev_handle		*dev,
	int			 index,
	int			 langid,
	char			*buf,
	size_t			 buflen
);
#define	usb_get_string_simple		 (*dl_usb_get_string_simple)
static int				 (*dl_usb_get_string_simple)(
	usb_dev_handle		*dev,
	int			 index,
	char			*buf,
	size_t			 buflen
);
#define	usb_bulk_write			 (*dl_usb_bulk_write)
static int				 (*dl_usb_bulk_write)(
	usb_dev_handle		*dev,
	int			 ep,
	char			*bytes,
	int			 size,
	int			 timeout
);
#define	usb_bulk_read			 (*dl_usb_bulk_read)
static int				 (*dl_usb_bulk_read)(
	usb_dev_handle		*dev,
	int			 ep,
	char			*bytes,
	int			 size,
	int			 timeout
);
#define	usb_interrupt_write		 (*dl_usb_interrupt_write)
static int				 (*dl_usb_interrupt_write)(
	usb_dev_handle		*dev,
	int			 ep,
	char			*bytes,
	int			 size,
	int			 timeout
);
#define	usb_interrupt_read		 (*dl_usb_interrupt_read)
static int				 (*dl_usb_interrupt_read)(
	usb_dev_handle		*dev,
	int			 ep,
	char			*bytes,
	int			 size,
	int			 timeout
);
#define	usb_control_msg			 (*dl_usb_control_msg)
static int				 (*dl_usb_control_msg)(
	usb_dev_handle		*dev,
	int			 requesttype,
	int			 request,
	int			 value,
	int			 index,
	char			*bytes,
	int			 size,
	int			 timeout
);
#define	usb_set_configuration		 (*dl_usb_set_configuration)
static int				 (*dl_usb_set_configuration)(
	usb_dev_handle		*dev,
	int			 configuration
);
#define	usb_claim_interface		 (*dl_usb_claim_interface)
static int				 (*dl_usb_claim_interface)(
	usb_dev_handle		*dev,
	int			 interface
);
#define	usb_release_interface		 (*dl_usb_release_interface)
static int				 (*dl_usb_release_interface)(
	usb_dev_handle		*dev,
	int			 interface
);
#define	usb_set_altinterface		 (*dl_usb_set_altinterface)
static int				 (*dl_usb_set_altinterface)(
	usb_dev_handle		*dev,
	int			 alternate
);
#define	usb_clear_halt			 (*dl_usb_clear_halt)
static int				 (*dl_usb_clear_halt)(
	usb_dev_handle		*dev,
	unsigned int		 ep
);
#define	usb_reset			 (*dl_usb_reset)
static int				 (*dl_usb_reset)(
	usb_dev_handle		*dev
);
#ifdef HAVE_USB_DETACH_KERNEL_DRIVER_NP
#define	usb_detach_kernel_driver_np	 (*dl_usb_detach_kernel_driver_np)
static int				 (*dl_usb_detach_kernel_driver_np)(
	usb_dev_handle		*dev,
	int			 interface
);
#endif	/* HAVE_USB_DETACH_KERNEL_DRIVER_NP */
#define	usb_strerror			 (*dl_usb_strerror)
static char				*(*dl_usb_strerror)(void);
#define	usb_init			 (*dl_usb_init)
static void				 (*dl_usb_init)(void);
#define	usb_set_debug			 (*dl_usb_set_debug)
static void				 (*dl_usb_set_debug)(
	int			 level
);
#define	usb_find_busses			 (*dl_usb_find_busses)
static int				 (*dl_usb_find_busses)(void);
#define	usb_find_devices		 (*dl_usb_find_devices)
static int				 (*dl_usb_find_devices)(void);
#define	usb_get_busses			 (*dl_usb_get_busses)
static struct usb_bus			*(*dl_usb_get_busses)(void);
/** @} */

/** @brief Handle of the lt_dlopen()'d module.
 * @note Set to `(void *)1` to signal errors. */
static lt_dlhandle	dl_libusb01_handle = NULL;

/** @brief Call lt_dlsym() with module @ref dl_libusb01_handle and *item* name and assign the returned address to 'dl_<*item*>'.
 *
 * On errors, `goto err`, with the description of the error (as returned by lt_dlerror()) stored in a variable named 'dl_error'.
 *
 * @param[in] item	name of the desired symbol. */
#define dl_libusb01_get(item)\
	*(void **)(&dl_##item) = lt_dlsym(dl_libusb01_handle, #item);\
	if ((dl_error = lt_dlerror()) != NULL)\
		goto err;

/** @brief Init libltdl and load libusb 0.1 lib (setting @ref dl_libusb01_handle) and symbols.
 *
 * @note
 * Unless dl_libusb01_exit() has been called, subsequent calls to this function will do nothing and:
 * - a previous successful call will make it succeed,
 * - errors on a previous call will error out.
 *
 * @return 1, on success,
 * @return 0, on errors, with the reason of the failure stored in *error* and @ref dl_libusb01_handle set to `(void *)1`. */
static inline int	dl_libusb01_init(
	char		*error,		/**< [out] string to hold a short description of any error. */
	const size_t	 errorlen	/**< [in]  size of *error*. */
) {
	char		*libname = NULL;
	const char	*dl_error = NULL;

	/* Already been here */
	if (dl_libusb01_handle != NULL) {
		if (dl_libusb01_handle == (void *)1) {
			snprintf(error, errorlen, "Loading USB library has already failed, not trying again.");
			return 0;
		}
		return 1;
	}

	libname = get_libname("libusb-0.1.so");
	if (!libname) {
		/* Sometimes (e.g. with some libusb-compat-0.1 versions) only this one is available. */
		libname = get_libname("libusb.so");
		if (!libname) {
			snprintf(error, errorlen, "USB library not found.");
			dl_libusb01_handle = (void *)1;
			return 0;
		}
	}

	if (lt_dlinit() != 0) {
		snprintf(error, errorlen, "Error initializing libltdl: %s.", lt_dlerror());
		free(libname);
		dl_libusb01_handle = (void *)1;
		return 0;
	}

	dl_libusb01_handle = lt_dlopen(libname);
	if (!dl_libusb01_handle) {
		dl_error = lt_dlerror();
		goto err;
	}

	/* Clear any existing error */
	lt_dlerror();

	dl_libusb01_get(usb_open);
	dl_libusb01_get(usb_close);
	dl_libusb01_get(usb_get_string);
	dl_libusb01_get(usb_get_string_simple);
	dl_libusb01_get(usb_bulk_write);
	dl_libusb01_get(usb_bulk_read);
	dl_libusb01_get(usb_interrupt_write);
	dl_libusb01_get(usb_interrupt_read);
	dl_libusb01_get(usb_control_msg);
	dl_libusb01_get(usb_set_configuration);
	dl_libusb01_get(usb_claim_interface);
	dl_libusb01_get(usb_release_interface);
	dl_libusb01_get(usb_set_altinterface);
	dl_libusb01_get(usb_clear_halt);
	dl_libusb01_get(usb_reset);
#ifdef HAVE_USB_DETACH_KERNEL_DRIVER_NP
	dl_libusb01_get(usb_detach_kernel_driver_np);
#endif	/* HAVE_USB_DETACH_KERNEL_DRIVER_NP */
	dl_libusb01_get(usb_strerror);
	dl_libusb01_get(usb_init);
	dl_libusb01_get(usb_set_debug);
	dl_libusb01_get(usb_find_busses);
	dl_libusb01_get(usb_find_devices);
	dl_libusb01_get(usb_get_busses);

	free(libname);
	return 1;
err:
	snprintf(error, errorlen, "Cannot load USB library (%s): %s.", libname, dl_error);
	free(libname);
	if (dl_libusb01_handle)
		lt_dlclose(dl_libusb01_handle);
	lt_dlexit();
	dl_libusb01_handle = (void *)1;
	return 0;
}

/** @brief Unload libusb 0.1 lib shutting down libltdl (and reset @ref dl_libusb01_handle). */
static inline void	dl_libusb01_exit(void)
{
	/* Nothing to do */
	if (dl_libusb01_handle == NULL)
		return;

	/* On errors, we've already lt_dlclose()'d and lt_dlexit()'d, if applicable. */
	if (dl_libusb01_handle != (void *)1) {
		lt_dlclose(dl_libusb01_handle);
		lt_dlexit();
	}

	dl_libusb01_handle = NULL;
}

#endif	/* DL_LIBUSB_0_1_H */
