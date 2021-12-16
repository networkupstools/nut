/* usb-common.h - prototypes for the common useful USB functions
 * NOTE that it aims to consolidate use of different USB-related APIs
 * such as libusb-0.1 and libusb-1.0 in a way that minimizes the coding
 * difference for majority of NUT - so typedef'ing or converting various
 * data types and method signatures.
 *
 * Beside your system headers (content varies between distros) you can
 * find some documentation online:
 * - libusb-1.0:
 *   https://github.com/libusb/libusb/blob/master/libusb/libusb.h
 *   https://libusb.sourceforge.io/api-1.0/
 *   https://libusb.sourceforge.io/api-1.0/libusb_api.html
 *   https://github.com/libusb/libusb/wiki
 *   https://nxmnpg.lemoda.net/3/libusb (one page, easy to search)
 * - libusb-0.1 is nowadays hard to find, original web-site and
 *   sourceforge project were discontinued over the past years.
 *   A rendered copy of the libusb-0.1 Developers Guide was noted at:
 *   http://transit.iut2.upmf-grenoble.fr/doc/libusb-dev/html/index.html
 *   http://transit.iut2.upmf-grenoble.fr/doc/libusb-dev/html/functions.html
 *   Original SGML for that seems to be in source tarball such as
 *   http://deb.debian.org/debian/pool/main/libu/libusb/libusb_0.1.12.orig.tar.gz
 *
 * Related (but currently not directly used) projects include:
 * - libusb-win32 port based on libusb-0.1 API (bug-fix-only mode,
 *   new projects should use libusb Windows backend):
 *   https://sourceforge.net/p/libusb-win32/wiki/Documentation/
 * - (Currently not in NUT codebase scope, but might help...)
 *   > A compatibility layer allowing applications written for
 *   > libusb-0.1 to work with libusb-1.0. libusb-compat-0.1
 *   > attempts to look, feel, smell and walk like libusb-0.1.
 *   Mostly. Details (and known differences) documented at:
 *   https://github.com/libusb/libusb-compat-0.1
 *
 * Also note that at least currently this does not deal with non-libusb
 * APIs (important when looking for method signatures in documentation,
 * since e.g. Linux Kernel USB subsystem uses some of libusb-0.1 method
 * names, but with different set, type and order of arguments!)

   Copyright (C) 2008 - 2016  Arnaud Quette <arnaud.quette@gmail.com>
   Copyright (C) 2021  Jim Klimov <jimklimov+nut@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef NUT_USB_COMMON_H
#define NUT_USB_COMMON_H

/* Note: usb-common.h (this file) is included by nut_libusb.h,
 * so not looping the includes ;)
 */
#include "nut_stdint.h"	/* for uint16_t, UINT16_MAX, etc. */

#include <regex.h>

#if (!WITH_LIBUSB_1_0) && (!WITH_LIBUSB_0_1)
#error "configure script error: Neither WITH_LIBUSB_1_0 nor WITH_LIBUSB_0_1 is set"
#endif

#if (WITH_LIBUSB_1_0) && (WITH_LIBUSB_0_1)
#error "configure script error: Both WITH_LIBUSB_1_0 and WITH_LIBUSB_0_1 are set"
#endif

/* Select version-specific libusb header file and define a sort of
 * "Compatibility layer" between libusb 0.1 and 1.0
 */
#if WITH_LIBUSB_1_0
# include <libusb.h>

 /* Simply remap libusb functions/structures from 0.1 to 1.0 */

 /* Structures */
 /* #define usb_dev_handle libusb_device_handle */
 typedef libusb_device_handle usb_dev_handle;
 typedef uint8_t usb_ctrl_requesttype;
 typedef uint8_t usb_ctrl_request;
 typedef unsigned char usb_ctrl_endpoint;
 typedef uint16_t usb_ctrl_repindex;
 typedef uint8_t usb_ctrl_descindex;
 typedef unsigned char* usb_ctrl_charbuf;
 typedef uint16_t usb_ctrl_charbufsize;
 typedef unsigned int usb_ctrl_timeout_msec;	/* in milliseconds */
	/* Note: there does not seem to be a standard type
	 * for milliseconds, like there is an useconds_t */

 /* defines */
 #define USB_CLASS_PER_INTERFACE LIBUSB_CLASS_PER_INTERFACE
 #define USB_DT_STRING LIBUSB_DT_STRING
 #define USB_ENDPOINT_IN LIBUSB_ENDPOINT_IN
 #define USB_ENDPOINT_OUT LIBUSB_ENDPOINT_OUT
 #define USB_RECIP_ENDPOINT LIBUSB_RECIPIENT_ENDPOINT
 #define USB_RECIP_INTERFACE LIBUSB_RECIPIENT_INTERFACE
 #define USB_REQ_SET_DESCRIPTOR LIBUSB_REQUEST_SET_DESCRIPTOR
 #define USB_TYPE_CLASS LIBUSB_REQUEST_TYPE_CLASS
 #define USB_TYPE_VENDOR LIBUSB_REQUEST_TYPE_VENDOR

 #define ERROR_PIPE LIBUSB_ERROR_PIPE
 #define ERROR_TIMEOUT LIBUSB_ERROR_TIMEOUT
 #define ERROR_BUSY	LIBUSB_ERROR_BUSY
 #define ERROR_NO_DEVICE LIBUSB_ERROR_NO_DEVICE
 #define ERROR_ACCESS LIBUSB_ERROR_ACCESS
 #define ERROR_IO LIBUSB_ERROR_IO
 #define ERROR_OVERFLOW LIBUSB_ERROR_OVERFLOW
 #define ERROR_NOT_FOUND LIBUSB_ERROR_NOT_FOUND

 /* Functions, including range-checks to convert data types of the two APIs.
  * Follows an example from libusb-1.0 headers that liberally cast int args
  * of one method to uint16_t to call another; at least we do so with checks: */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNUSED_FUNCTION)
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNUSED_FUNCTION
# pragma GCC diagnostic ignored "-Wunused-function"
#endif
 /* #define usb_control_msg libusb_control_transfer */
 static inline  int usb_control_msg(usb_dev_handle *dev, int requesttype,
                    int request, int value, int index,
                    usb_ctrl_charbuf bytes, int size, int timeout)
 {
	/*
	Map from libusb-0.1 API => libusb-1.0 API:
	 int LIBUSB_CALL libusb_control_transfer(
		libusb_device_handle *dev_handle, uint8_t request_type,
		uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
		unsigned char *data, uint16_t wLength, unsigned int timeout);
	Note: In libusb-0.1 bytes was a (char*) but our consumer code
	was already fixed to use "usb_ctrl_charbuf" to match other methods.
	*/

	if (requesttype < 0 || (uintmax_t)requesttype > UINT8_MAX
	||  request < 0 || (uintmax_t)request > UINT8_MAX
	||  value < 0 || (uintmax_t)value > UINT16_MAX
	||  index < 0 || (uintmax_t)index > UINT16_MAX
	||  size < 0 || (uintmax_t)size > UINT16_MAX
	||  timeout < 0
	) {
		fatalx(EXIT_FAILURE,
			"usb_control_msg() args out of range for libusb_control_transfer() implementation");
	}

	return libusb_control_transfer(
		dev,
		(uint8_t)requesttype,
		(uint8_t)request,
		(uint16_t)value,
		(uint16_t)index,
		(unsigned char *)bytes,
		(uint16_t)size,
		(unsigned int) timeout
		);
 }

 static inline  int usb_interrupt_read(usb_dev_handle *dev, int ep,
                usb_ctrl_charbuf bytes, int size, int timeout)
 {
	/* NOTE: Also for routines below:
	Map from libusb-0.1 API => libusb-1.0 API plus change of logic per below code:
	 int LIBUSB_CALL libusb_interrupt_transfer(libusb_device_handle *dev_handle,
		unsigned char endpoint, unsigned char *data, int length,
		int *actual_length, unsigned int timeout);
	Note: In libusb-0.1 bytes was a (char*) but our consumer code
	was already fixed to use "usb_ctrl_charbuf" to match other methods.
	*/
	int ret;

	if (ep < 0 || (uintmax_t)ep > UCHAR_MAX
	||  timeout < 0
	) {
		fatalx(EXIT_FAILURE,
			"usb_interrupt_read() args out of range for libusb_interrupt_transfer() implementation");
	}

	ret = libusb_interrupt_transfer(dev, (unsigned char)ep, (unsigned char *) bytes,
			size, &size, (unsigned int)timeout);
	/* In case of success, return the operation size, as done with libusb 0.1 */
	return (ret == LIBUSB_SUCCESS)?size:ret;
 }

 static inline  int usb_interrupt_write(usb_dev_handle *dev, int ep,
                const usb_ctrl_charbuf bytes, int size, int timeout)
 {
	/* See conversion comments above */
	int ret;

	if (ep < 0 || (uintmax_t)ep > UCHAR_MAX
	||  timeout < 0
	) {
		fatalx(EXIT_FAILURE,
			"usb_interrupt_write() args out of range for libusb_interrupt_transfer() implementation");
	}

	ret = libusb_interrupt_transfer(dev, (unsigned char)ep, (unsigned char *) bytes,
			size, &size, (unsigned int)timeout);
	/* In case of success, return the operation size, as done with libusb 0.1 */
	return (ret == LIBUSB_SUCCESS)?size:ret;
 }

 static inline  int usb_bulk_read(usb_dev_handle *dev, int ep,
                usb_ctrl_charbuf bytes, int size, int timeout)
 {
	/* See conversion comments above */
	int ret;

	if (ep < 0 || (uintmax_t)ep > UCHAR_MAX
	||  timeout < 0
	) {
		fatalx(EXIT_FAILURE,
			"usb_bulk_read() args out of range for libusb_interrupt_transfer() implementation");
	}

	ret = libusb_interrupt_transfer(dev, (unsigned char)ep, (unsigned char *) bytes,
			size, &size, (unsigned int)timeout);
	/* In case of success, return the operation size, as done with libusb 0.1 */
	return (ret == LIBUSB_SUCCESS)?size:ret;
 }

 static inline  int usb_bulk_write(usb_dev_handle *dev, int ep,
                usb_ctrl_charbuf bytes, int size, int timeout)
 {
	/* See conversion comments above */
	int ret;

	if (ep < 0 || (uintmax_t)ep > UCHAR_MAX
	||  timeout < 0
	) {
		fatalx(EXIT_FAILURE,
			"usb_bulk_write() args out of range for libusb_interrupt_transfer() implementation");
	}

	ret = libusb_interrupt_transfer(dev, (unsigned char)ep, (unsigned char *) bytes,
			size, &size, (unsigned int)timeout);
	/* In case of success, return the operation size, as done with libusb 0.1 */
	return (ret == LIBUSB_SUCCESS)?size:ret;
 }

 static inline  int usb_get_string(usb_dev_handle *dev, int index, int langid,
                usb_ctrl_charbuf buf, size_t buflen)
 {
	/*
	Map from libusb-0.1 API (originally "char* buf") => libusb-1.0 API:
	int libusb_get_string_descriptor(libusb_device_handle *dev_handle,
		uint8_t desc_index, uint16_t langid, unsigned char *data, int length)
	*/

	if (index < 0 || (uintmax_t)index > UINT8_MAX
	||  langid < 0 || (uintmax_t)langid > UINT16_MAX
	||  (uintmax_t)buflen > INT_MAX
	) {
		fatalx(EXIT_FAILURE,
			"usb_get_string() args out of range for libusb_get_string_descriptor() implementation");
	}

	return libusb_get_string_descriptor(
		dev,
		(uint8_t)index,
		(uint16_t)langid,
		(unsigned char *)buf,
		(int) buflen
		);

 }

 static inline  int usb_get_string_simple(usb_dev_handle *dev, int index,
                usb_ctrl_charbuf buf, size_t buflen)
 {
	/*
	Map from libusb-0.1 API (originally "char* buf") => libusb-1.0 API:
	int LIBUSB_CALL libusb_get_string_descriptor_ascii(libusb_device_handle *dev_handle,
		uint8_t desc_index, unsigned char *data, int length);
	*/

	if (index < 0 || (uintmax_t)index > UINT8_MAX
	||  (uintmax_t)buflen > INT_MAX
	) {
		fatalx(EXIT_FAILURE,
			"usb_get_string_simple() args out of range for libusb_get_string_descriptor_ascii() implementation");
	}

	return libusb_get_string_descriptor_ascii(
		dev,
		(uint8_t)index,
		(unsigned char *)buf,
		(int) buflen
		);
 }
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNUSED_FUNCTION)
# pragma GCC diagnostic pop
#endif

 /* Functions for which simple mappings seem to suffice (no build warnings emitted): */
 #define usb_claim_interface libusb_claim_interface
 #define usb_clear_halt libusb_clear_halt
 #define usb_close libusb_close
 #define usb_set_configuration libusb_set_configuration
 #define usb_release_interface libusb_release_interface
 #define usb_reset libusb_reset_device

 /* FIXME: some original libusb1.c code cast the (int) argument
  * as (enum libusb_error) - should we force that in the macro? */
 #define nut_usb_strerror(a) libusb_strerror(a)
#endif  /* WITH_LIBUSB_1_0 */

/* Note: Checked above that in practice we handle some one libusb API */
#if WITH_LIBUSB_0_1
# include <usb.h>
 /* Structures */
 typedef int usb_ctrl_requesttype;
 typedef int usb_ctrl_request;
 typedef int usb_ctrl_endpoint;
 typedef int usb_ctrl_repindex;
 typedef int usb_ctrl_descindex;
 typedef char* usb_ctrl_charbuf;
 typedef int usb_ctrl_charbufsize;
 typedef int usb_ctrl_timeout_msec;	/* in milliseconds */

 /* defines */
 #define ERROR_PIPE -EPIPE
 #define ERROR_TIMEOUT -ETIMEDOUT
 #define ERROR_BUSY	-EBUSY
 #define ERROR_NO_DEVICE -ENODEV
 #define ERROR_ACCESS -EACCES
 #define ERROR_IO -EIO
 #define ERROR_OVERFLOW -EOVERFLOW
 #define ERROR_NOT_FOUND -ENOENT

 /* Functions for which simple mappings seem to suffice (no build warnings emitted): */
 #define nut_usb_strerror(a) usb_strerror()
#endif  /* WITH_LIBUSB_0_1 */

/* USB standard timeout [ms] */
#define USB_TIMEOUT 5000

/*!
 * USBDevice_t: Describe a USB device. This structure contains exactly
 * the 5 pieces of information by which a USB device identifies
 * itself, so it serves as a kind of "fingerprint" of the device. This
 * information must be matched exactly when reopening a device, and
 * therefore must not be "improved" or updated by a client
 * program. Vendor, Product, and Serial can be NULL if the
 * corresponding string did not exist or could not be retrieved.
 */
typedef struct USBDevice_s {
	/* These 5 data points are common properties of an USB device: */
	uint16_t	VendorID;  /*!< Device's Vendor ID    */
	uint16_t	ProductID; /*!< Device's Product ID   */
	char		*Vendor;   /*!< Device's Vendor Name  */
	char		*Product;  /*!< Device's Product Name */
	char		*Serial;   /*!< Product serial number */
	/* These data points can be determined by the driver for some devices
	   or by libusb to detail its connection topology: */
	char		*Bus;      /*!< Bus name, e.g. "003"  */
	uint16_t	bcdDevice; /*!< Device release number */
	char		*Device;   /*!< Device name on the bus, e.g. "001"  */
} USBDevice_t;

/*!
 * USBDeviceMatcher_t: A "USB matcher" is a callback function that
 * inputs a USBDevice_t structure, and returns 1 for a match and 0
 * for a non-match.  Thus, a matcher provides a criterion for
 * selecting a USB device.  The callback function further is
 * expected to return -1 on error with errno set, and -2 on other
 * errors. Matchers can be connected in a linked list via the
 * "next" field.
 */
typedef struct USBDeviceMatcher_s {
	int	(*match_function)(USBDevice_t *device, void *privdata);
	void	*privdata;
	struct USBDeviceMatcher_s	*next;
} USBDeviceMatcher_t;

/* constructors and destructors for specific types of matchers. An
   exact matcher matches a specific usb_device_t structure (except for
   the Bus component, which is ignored). A regex matcher matches
   devices based on a set of regular expressions. The USBNew* functions
   return a matcher on success, or -1 on error with errno set. Note
   that the "USBFree*" functions only free the current matcher, not
   any others that are linked via "next" fields. */
int USBNewExactMatcher(USBDeviceMatcher_t **matcher, USBDevice_t *hd);
int USBNewRegexMatcher(USBDeviceMatcher_t **matcher, char **regex, int cflags);
void USBFreeExactMatcher(USBDeviceMatcher_t *matcher);
void USBFreeRegexMatcher(USBDeviceMatcher_t *matcher);

/* dummy USB function and macro, inspired from the Linux kernel
 * this allows USB information extraction */
#define USB_DEVICE(vendorID, productID)	vendorID, productID

typedef struct {
	uint16_t	vendorID;
	uint16_t	productID;
	void	*(*fun)(USBDevice_t *);		/* handler for specific processing */
} usb_device_id_t;

#define NOT_SUPPORTED		0
#define POSSIBLY_SUPPORTED	1
#define SUPPORTED			2

/* Function used to match a VendorID/ProductID pair against a list of
 * supported devices. Return values:
 * NOT_SUPPORTED (0), POSSIBLY_SUPPORTED (1) or SUPPORTED (2) */
int is_usb_device_supported(usb_device_id_t *usb_device_id_list,
							USBDevice_t *device);

void nut_usb_addvars(void);

#endif /* NUT_USB_COMMON_H */
