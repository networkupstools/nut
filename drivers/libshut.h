/**
 * @file
 * @brief	HID Library - Generic SHUT backend.
 *		SHUT stands for Serial HID UPS Transfer, and was created by MGE UPS SYSTEMS.
 * @copyright	@parblock
 * Copyright (C):
 * - 2006-2007 -- Arnaud Quette (<aquette.dev@gmail.com>), for MGE UPS SYSTEMS
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

#ifndef LIBSHUT_H
#define LIBSHUT_H

#include "main.h"	/* for upsdrv_info_t */
#include "nut_stdint.h"	/* for uint*_t */

/** @brief SHUT communication subdriver description. */
extern upsdrv_info_t	comm_upsdrv_info;

/** @brief Structure/type describing a SHUT device.
 *
 * This structure contains exactly the pieces of information by which a SHUT device identifies itself,
 * so it serves as a kind of "fingerprint" of the device.
 *
 * @ref Vendor, @ref Product, and @ref Serial can be `NULL` if the corresponding string did not exist or could not be retrieved. */
typedef struct SHUTDevice_s {
	uint16_t			 VendorID;	/**< @brief Device's vendor ID. */
	uint16_t			 ProductID;	/**< @brief Device's product ID. */
	char				*Vendor;	/**< @brief Device's vendor name. */
	char				*Product;	/**< @brief Device's product name. */
	char				*Serial;	/**< @brief Product serial number. */
	char				*Bus;		/**< @brief Bus name (hardcoded to "serial", for SHUT devices). */
	uint16_t			 bcdDevice;	/**< @brief Device release number. */
} SHUTDevice_t;

/** @brief Error codes returned by functions on failure.
 * @note Keep sync'd with libusb 1.0. */
enum shut_error {
	SHUT_SUCCESS			=   0,		/**< Success (no error). */
	SHUT_ERROR_IO			=  -1,		/**< Input/output error. */
	SHUT_ERROR_INVALID_PARAM	=  -2,		/**< Invalid parameter. */
	SHUT_ERROR_ACCESS		=  -3,		/**< Access denied (insufficient permissions). */
	SHUT_ERROR_NO_DEVICE		=  -4,		/**< No such device (it may have been disconnected). */
	SHUT_ERROR_NOT_FOUND		=  -5,		/**< Entity not found. */
	SHUT_ERROR_BUSY			=  -6,		/**< Resource busy. */
	SHUT_ERROR_TIMEOUT		=  -7,		/**< Operation timed out. */
	SHUT_ERROR_OVERFLOW		=  -8,		/**< Overflow. */
	SHUT_ERROR_PIPE			=  -9,		/**< Pipe error. */
	SHUT_ERROR_INTERRUPTED		= -10,		/**< System call interrupted (perhaps due to signal). */
	SHUT_ERROR_NO_MEM		= -11,		/**< Insufficient memory. */
	SHUT_ERROR_NOT_SUPPORTED	= -12,		/**< Operation not supported or unimplemented on this platform. */
	SHUT_ERROR_OTHER		= -99		/**< Other error. */
};

/** @brief Structure/type describing the SHUT communication routines. */
typedef struct shut_communication_subdriver_s {
	const char	  *name;		/**< @brief Name of this subdriver. */
	const char	  *version;		/**< @brief Version of this subdriver. */
	int		 (*open)		/** @brief (Re)Open a device.
						 *
						 * If *fd* refers to an already opened device, it is closed before attempting the reopening.
						 *
						 * @warning This function calls exit() on fatal errors.
						 *
						 * @return @ref SHUT_SUCCESS, with *curDevice* filled (IFF *callback* is not `NULL`), on success,
						 * @return a @ref shut_error "SHUT_ERROR" code, on errors. */
	(
		int		 *fd,			/**< [in,out]	storage location for the file descriptor of the (already) opened device */
		SHUTDevice_t	 *curDevice,		/**<    [out]	@ref SHUTDevice_t that has to be populated on success and representing the opened device */
		char		 *device_path,		/**< [in]	pathname of the device that has to be opened */
		int		(*callback)		/**< [in]	@parblock
							 * (optional) function to tell whether the opened device is accepted by the caller or not
							 *
							 * **Returns**
							 * - 1, if the device is accepted,
							 * - 0, if not.
							 *
							 * **Parameters**
							 * - `[in]` **fd**: file descriptor of the opened device
							 * - `[in]` **hd**: @ref SHUTDevice_t of the opened device (*curDevice*)
							 * - `[in]` **rdbuf**: report descriptor of the opened device
							 * - `[in]` **rdlen**: length of the report descriptor (*rdbuf* is guaranteed to be at least this size)
							 *		@endparblock */
		(
			int		 fd,
			SHUTDevice_t	*hd,
			unsigned char	*rdbuf,
			int		 rdlen
		)
	);
	void		 (*close)		/** @brief Close the opened device *fd* refers to. */
	(
		int		fd			/**< [in] file descriptor of an opened device */
	);
	int		 (*get_report)		/** @brief Retrieve a HID report from a device.
						 * @return length of the retrieved data, on success,
						 * @return a @ref shut_error "SHUT_ERROR" code, on errors. */
	(
		int		 fd,			/**< [in] file descriptor of an already opened device */
		int		 ReportId,		/**< [in] ID of the report that has to be retrieved */
		unsigned char	*raw_buf,		/**< [out] storage location for the retrieved data */
		int		 ReportSize		/**< [in] size of the report (*raw_buf* should be at least this size) */
	);
	int		 (*set_report)		/** @brief Set a HID report in a device.
						 * @return the number of bytes sent to the device, on success,
						 * @return a @ref shut_error "SHUT_ERROR" code, on errors. */
	(
		int		 fd,			/**< [in] file descriptor of an already opened device */
		int		 ReportId,		/**< [in] ID of the report that has to be set */
		unsigned char	*raw_buf,		/**< [in] data that has to be set */
		int		 ReportSize		/**< [in] size of the report (*raw_buf* should be at least this size) */
	);
	int		 (*get_string)		/** @brief Retrieve a string descriptor from a device.
						 * @return the number of bytes read and stored in *buf*, on success,
						 * @return a @ref shut_error "SHUT_ERROR" code, on errors. */
	(
		int		 fd,			/**< [in] file descriptor of an already opened device */
		int		 StringIdx,		/**< [in] index of the descriptor to retrieve */
		char		*buf,			/**< [out] storage location for the retrieved string */
		size_t		 buflen			/**< [in] size of *buf* */
	);
	int		 (*get_interrupt)	/** @brief Retrieve data from an interrupt endpoint of a device.
						 * @note At the moment, this function actually does nothing and it always returns @ref SHUT_ERROR_NOT_SUPPORTED, if *fd* is not invalid.
						 * @return the number of bytes read and stored in *buf*, on success,
						 * @return a @ref shut_error "SHUT_ERROR" code, on errors. */
	(
		int		 fd,			/**< [in] file descriptor of an already opened device */
		unsigned char	*buf,			/**< [out] storage location for the retrieved data */
		int		 bufsize,		/**< [in] size of *buf* */
		int		 timeout		/**< [in] allowed timeout (ms) for the operation */
	);
	const char	*(*strerror)		/** @brief Return a constant string with a short description of *errcode*.
						 * @return a short description of *errcode*. */
	(
		enum shut_error	errcode			/**< [in] the @ref shut_error code whose description is desired */
	);
} shut_communication_subdriver_t;

/** @brief Actual SHUT communication subdriver. */
extern shut_communication_subdriver_t	shut_subdriver;

#endif	/* LIBSHUT_H */
