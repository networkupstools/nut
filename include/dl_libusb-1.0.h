/**
 * @file
 * @brief	Dynamic loading of libusb 1.0.
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
 * Load lib and symbols with dl_libusb10_init(), unload with dl_libusb10_exit():
 * they'll take care of lt_dlinit()'ing/lt_dlexit()'ing a matching number of times,
 * in order to avoid that a surplus call to lt_dlexit() automatically unloads other lt_dlopen()'d modules.
 */

#ifndef DL_LIBUSB_1_0_H
#define DL_LIBUSB_1_0_H

#include <ltdl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <libusb.h>

#include "common.h"	/* for get_libname() */

/** @name libusb 1.0 symbols
 * For each pointer an alias with the original symbol name is #define'd, so that already written code doesn't have to be modified.
 * @{ */
#define	libusb_init				 (*dl_libusb_init)
int						 (*dl_libusb_init)(
	libusb_context			 **ctx
);
#define	libusb_exit				 (*dl_libusb_exit)
void						 (*dl_libusb_exit)(
	libusb_context			  *ctx
);
#define	libusb_strerror				 (*dl_libusb_strerror)
const char					*(*dl_libusb_strerror)(
	enum libusb_error		   errcode
);
#define	libusb_get_device_list			 (*dl_libusb_get_device_list)
ssize_t						 (*dl_libusb_get_device_list)(
	libusb_context			  *ctx,
	libusb_device			***list
);
#define	libusb_free_device_list			 (*dl_libusb_free_device_list)
void						 (*dl_libusb_free_device_list)(
	libusb_device			 **list,
	int				   unref_devices
);
#define	libusb_get_bus_number			 (*dl_libusb_get_bus_number)
uint8_t						 (*dl_libusb_get_bus_number)(
	libusb_device			  *dev
);
#define	libusb_open				 (*dl_libusb_open)
int						 (*dl_libusb_open)(
	libusb_device			  *dev,
	libusb_device_handle		 **dev_handle
);
#define	libusb_close				 (*dl_libusb_close)
void						 (*dl_libusb_close)(
	libusb_device_handle		  *dev_handle
);
#define	libusb_get_device_descriptor		 (*dl_libusb_get_device_descriptor)
int						 (*dl_libusb_get_device_descriptor)(
	libusb_device			  *dev,
	struct libusb_device_descriptor	  *desc
);
#define	libusb_get_string_descriptor_ascii	 (*dl_libusb_get_string_descriptor_ascii)
int						 (*dl_libusb_get_string_descriptor_ascii)(
	libusb_device_handle		  *dev_handle,
	uint8_t				   desc_index,
	unsigned char			  *data,
	int				   length
);
/** @} */

/** @brief Handle of the lt_dlopen()'d module.
 * @note Set to `(void *)1` to signal errors. */
static lt_dlhandle	dl_libusb10_handle = NULL;

/** @brief Call lt_dlsym() with module @ref dl_libusb10_handle and *item* name and assign the returned address to 'dl_<*item*>'.
 *
 * On errors, `goto err`, with the description of the error (as returned by lt_dlerror()) stored in a variable named 'dl_error'.
 *
 * @param[in] item	name of the desired symbol. */
#define dl_libusb10_get(item)\
	*(void **)(&dl_##item) = lt_dlsym(dl_libusb10_handle, #item);\
	if ((dl_error = lt_dlerror()) != NULL)\
		goto err;

/** @brief Init libltdl and load libusb 1.0 lib (setting @ref dl_libusb10_handle) and symbols.
 *
 * @note
 * Unless dl_libusb10_exit() has been called, subsequent calls to this function will do nothing and:
 * - a previous successful call will make it succeed,
 * - errors on a previous call will error out.
 *
 * @return 1, on success,
 * @return 0, on errors, with the reason of the failure stored in *error* and @ref dl_libusb10_handle set to `(void *)1`. */
static inline int	dl_libusb10_init(
	char		*error,		/**< [out] string to hold a short description of any error. */
	const size_t	 errorlen	/**< [in]  size of *error*. */
) {
	char		*libname = NULL;
	const char	*dl_error = NULL;

	/* Already been here */
	if (dl_libusb10_handle != NULL) {
		if (dl_libusb10_handle == (void *)1) {
			snprintf(error, errorlen, "Loading USB library has already failed, not trying again.");
			return 0;
		}
		return 1;
	}

	libname = get_libname("libusb-1.0.so");
	if (!libname) {
		/* Sometimes (e.g. in FreeBSD) only this one is available. */
		libname = get_libname("libusb.so");
		if (!libname) {
			snprintf(error, errorlen, "USB library not found.");
			dl_libusb10_handle = (void *)1;
			return 0;
		}
	}


	if (lt_dlinit() != 0) {
		snprintf(error, errorlen, "Error initializing libltdl: %s.", lt_dlerror());
		free(libname);
		dl_libusb10_handle = (void *)1;
		return 0;
	}

	dl_libusb10_handle = lt_dlopen(libname);
	if (!dl_libusb10_handle) {
		dl_error = lt_dlerror();
		goto err;
	}

	/* Clear any existing error */
	lt_dlerror();

	dl_libusb10_get(libusb_init);
	dl_libusb10_get(libusb_exit);
	dl_libusb10_get(libusb_strerror);
	dl_libusb10_get(libusb_get_device_list);
	dl_libusb10_get(libusb_free_device_list);
	dl_libusb10_get(libusb_get_bus_number);
	dl_libusb10_get(libusb_open);
	dl_libusb10_get(libusb_close);
	dl_libusb10_get(libusb_get_device_descriptor);
	dl_libusb10_get(libusb_get_string_descriptor_ascii);

	free(libname);
	return 1;
err:
	snprintf(error, errorlen, "Cannot load USB library (%s): %s.", libname, dl_error);
	free(libname);
	if (dl_libusb10_handle)
		lt_dlclose(dl_libusb10_handle);
	lt_dlexit();
	dl_libusb10_handle = (void *)1;
	return 0;
}

/** @brief Unload libusb 1.0 lib shutting down libltdl (and reset @ref dl_libusb10_handle). */
static inline void	dl_libusb10_exit(void)
{
	/* Nothing to do */
	if (dl_libusb10_handle == NULL)
		return;

	/* On errors, we've already lt_dlclose()'d and lt_dlexit()'d, if applicable. */
	if (dl_libusb10_handle != (void *)1) {
		lt_dlclose(dl_libusb10_handle);
		lt_dlexit();
	}

	dl_libusb10_handle = NULL;
}

#endif	/* DL_LIBUSB_1_0_H */
