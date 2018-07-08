/**
 * @file
 * @brief	Partial libusb 1.0 API unreliably implemented using libusb 0.1.
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
 * Most of the supported entities should behave (mostly) like real libusb 1.0,
 * as such, here are documented only the few noteworthy things that differ.
 * For more info, refer to libusb docs.
 */

#ifndef LIBUSB_COMPAT_1_0_H
#define LIBUSB_COMPAT_1_0_H

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#include <sys/types.h>
#include "nut_stdint.h"

/* Standard USB stuff --------------------------------------------------------*/

/* Device and/or Interface Class codes. */
enum libusb_class_code {
	LIBUSB_CLASS_PER_INTERFACE			=    0,
	LIBUSB_CLASS_AUDIO				=    1,
	LIBUSB_CLASS_COMM				=    2,
	LIBUSB_CLASS_HID				=    3,
	LIBUSB_CLASS_PHYSICAL				=    5,
	LIBUSB_CLASS_PRINTER				=    7,
	LIBUSB_CLASS_PTP				=    6,
	LIBUSB_CLASS_IMAGE				=    6,
	LIBUSB_CLASS_MASS_STORAGE			=    8,
	LIBUSB_CLASS_HUB				=    9,
	LIBUSB_CLASS_DATA				=   10,
	LIBUSB_CLASS_SMART_CARD				= 0x0B,
	LIBUSB_CLASS_CONTENT_SECURITY			= 0x0D,
	LIBUSB_CLASS_VIDEO				= 0x0E,
	LIBUSB_CLASS_PERSONAL_HEALTHCARE		= 0x0F,
	LIBUSB_CLASS_DIAGNOSTIC_DEVICE			= 0xDC,
	LIBUSB_CLASS_WIRELESS				= 0xE0,
	LIBUSB_CLASS_APPLICATION			= 0xFE,
	LIBUSB_CLASS_VENDOR_SPEC			= 0xFF
};

/* Descriptor types as defined by the USB specification. */
enum libusb_descriptor_type {
	LIBUSB_DT_DEVICE				= 0x01,
	LIBUSB_DT_CONFIG				= 0x02,
	LIBUSB_DT_STRING				= 0x03,
	LIBUSB_DT_INTERFACE				= 0x04,
	LIBUSB_DT_ENDPOINT				= 0x05,
	LIBUSB_DT_BOS					= 0x0F,
	LIBUSB_DT_DEVICE_CAPABILITY			= 0x10,
	LIBUSB_DT_HID					= 0x21,
	LIBUSB_DT_REPORT				= 0x22,
	LIBUSB_DT_PHYSICAL				= 0x23,
	LIBUSB_DT_HUB					= 0x29,
	LIBUSB_DT_SUPERSPEED_HUB			= 0x2A,
	LIBUSB_DT_SS_ENDPOINT_COMPANION			= 0x30
};

/* Descriptor sizes per descriptor type. */
#define LIBUSB_DT_DEVICE_SIZE				18
#define LIBUSB_DT_CONFIG_SIZE				 9
#define LIBUSB_DT_INTERFACE_SIZE			 9
#define LIBUSB_DT_ENDPOINT_SIZE				 7
#define LIBUSB_DT_ENDPOINT_AUDIO_SIZE			 9
#define LIBUSB_DT_HUB_NONVAR_SIZE			 7
#define LIBUSB_DT_SS_ENDPOINT_COMPANION_SIZE		 6
#define LIBUSB_DT_BOS_SIZE				 5
#define LIBUSB_DT_DEVICE_CAPABILITY_SIZE		 3

/* Endpoint direction. */
enum libusb_endpoint_direction {
	LIBUSB_ENDPOINT_IN				= 0x80,
	LIBUSB_ENDPOINT_OUT				= 0x00
};

/* Standard requests, as defined in table 9-5 of the USB 3.0 specifications. */
enum libusb_standard_request {
	LIBUSB_REQUEST_GET_STATUS			= 0x00,
	LIBUSB_REQUEST_CLEAR_FEATURE			= 0x01,
							/* 0x02 is reserved */
	LIBUSB_REQUEST_SET_FEATURE			= 0x03,
							/* 0x04 is reserved */
	LIBUSB_REQUEST_SET_ADDRESS			= 0x05,
	LIBUSB_REQUEST_GET_DESCRIPTOR			= 0x06,
	LIBUSB_REQUEST_SET_DESCRIPTOR			= 0x07,
	LIBUSB_REQUEST_GET_CONFIGURATION		= 0x08,
	LIBUSB_REQUEST_SET_CONFIGURATION		= 0x09,
	LIBUSB_REQUEST_GET_INTERFACE			= 0x0A,
	LIBUSB_REQUEST_SET_INTERFACE			= 0x0B,
	LIBUSB_REQUEST_SYNCH_FRAME			= 0x0C,
	LIBUSB_REQUEST_SET_SEL				= 0x30,
	LIBUSB_SET_ISOCH_DELAY				= 0x31
};

/* Request type bits of the `bmRequestType` field in control transfers. */
enum libusb_request_type {
	LIBUSB_REQUEST_TYPE_STANDARD			= (0x00 << 5),
	LIBUSB_REQUEST_TYPE_CLASS			= (0x01 << 5),
	LIBUSB_REQUEST_TYPE_VENDOR			= (0x02 << 5),
	LIBUSB_REQUEST_TYPE_RESERVED			= (0x03 << 5)
};

/* Recipient bits of the `bmRequestType` field in control transfers. */
enum libusb_request_recipient {
	LIBUSB_RECIPIENT_DEVICE				= 0x00,
	LIBUSB_RECIPIENT_INTERFACE			= 0x01,
	LIBUSB_RECIPIENT_ENDPOINT			= 0x02,
	LIBUSB_RECIPIENT_OTHER				= 0x03
};

/* A structure representing the standard USB device descriptor. */
struct libusb_device_descriptor {
	uint8_t						bLength;
	uint8_t						bDescriptorType;
	uint16_t					bcdUSB;
	uint8_t						bDeviceClass;
	uint8_t						bDeviceSubClass;
	uint8_t						bDeviceProtocol;
	uint8_t						bMaxPacketSize0;
	uint16_t					idVendor;
	uint16_t					idProduct;
	uint16_t					bcdDevice;
	uint8_t						iManufacturer;
	uint8_t						iProduct;
	uint8_t						iSerialNumber;
	uint8_t						bNumConfigurations;
};

/* A structure representing the standard USB endpoint descriptor. */
struct libusb_endpoint_descriptor {
	uint8_t						 bLength;
	uint8_t						 bDescriptorType;
	uint8_t						 bEndpointAddress;
	uint8_t						 bmAttributes;
	uint16_t					 wMaxPacketSize;
	uint8_t						 bInterval;
	uint8_t						 bRefresh;
	uint8_t						 bSynchAddress;
	const unsigned char				*extra;
	int						 extra_length;
};

/* A structure representing the standard USB interface descriptor. */
struct libusb_interface_descriptor {
	uint8_t						 bLength;
	uint8_t						 bDescriptorType;
	uint8_t						 bInterfaceNumber;
	uint8_t						 bAlternateSetting;
	uint8_t						 bNumEndpoints;
	uint8_t						 bInterfaceClass;
	uint8_t						 bInterfaceSubClass;
	uint8_t						 bInterfaceProtocol;
	uint8_t						 iInterface;
	const struct libusb_endpoint_descriptor		*endpoint;
	const unsigned char				*extra;
	int						 extra_length;
};

/* A collection of alternate settings for a particular USB interface. */
struct libusb_interface {
	const struct libusb_interface_descriptor	*altsetting;
	int						 num_altsetting;
};

/* A structure representing the standard USB configuration descriptor. */
struct libusb_config_descriptor {
	uint8_t						 bLength;
	uint8_t						 bDescriptorType;
	uint16_t					 wTotalLength;
	uint8_t						 bNumInterfaces;
	uint8_t						 bConfigurationValue;
	uint8_t						 iConfiguration;
	uint8_t						 bmAttributes;
	uint8_t						 MaxPower;
	const struct libusb_interface			*interface;
	const unsigned char				*extra;
	int						 extra_length;
};

/* libusb --------------------------------------------------------------------*/

/** The whole *libusb_context* concept is not supported/implemented. */
typedef struct libusb_context				libusb_context;

/** Because of how libusb 0.1 handles device discovery and enumeration
 * on certain platforms and/or libusb 0.1 implementations,
 * after a subsequent call to libusb_get_device_list(),
 * a still attached previously returned *libusb_device* with no handles on it
 * (i.e. not even libusb_open()'d or all handles on it already libusb_close()'d)
 * cannot be guaranteed to be exactly the same device as before the call
 * (it could be a very similar one): so don't assume it to be. */
typedef struct libusb_device				libusb_device;

/** It's highly advisable to libusb_close() any *libusb_device_handle*,
 * before calling libusb_get_device_list() anew,
 * since, on certain platforms and/or libusb 0.1 implementations,
 * it may not be always safe to do it after. */
typedef struct libusb_device_handle			libusb_device_handle;

/* Structure providing the version of the libusb runtime. */
struct libusb_version {
	const uint16_t					 major;
	const uint16_t					 minor;
	const uint16_t					 micro;
	const uint16_t					 nano;
	const char					*rc;
	const char					*describe;
};

/* libusb 1.0 error codes. */
enum libusb_error {
	LIBUSB_SUCCESS					=   0,
	LIBUSB_ERROR_IO					=  -1,
	LIBUSB_ERROR_INVALID_PARAM			=  -2,
	LIBUSB_ERROR_ACCESS				=  -3,
	LIBUSB_ERROR_NO_DEVICE				=  -4,
	LIBUSB_ERROR_NOT_FOUND				=  -5,
	LIBUSB_ERROR_BUSY				=  -6,
	LIBUSB_ERROR_TIMEOUT				=  -7,
	LIBUSB_ERROR_OVERFLOW				=  -8,
	LIBUSB_ERROR_PIPE				=  -9,
	LIBUSB_ERROR_INTERRUPTED			= -10,
	LIBUSB_ERROR_NO_MEM				= -11,
	LIBUSB_ERROR_NOT_SUPPORTED			= -12,
	LIBUSB_ERROR_OTHER				= -99
};

/* Library initialization/deinitialization. */
int	libusb_init(
	libusb_context	**ctx
);
void	libusb_exit(
	libusb_context	*ctx
);

/* Misc stuff. */
/** @return the libusb_version of this library, with:
 * - *major* and *minor* hardcoded to `0` and `1` (tellingly, isn't it?),
 * - *micro* storing the actual version of this library. */
const struct libusb_version	*libusb_get_version(void);

/** @return a **non-localized** short description of *errcode*. */
const char	*libusb_strerror(
	enum libusb_error	errcode
);

/* Device handling and enumeration. */
ssize_t	libusb_get_device_list(
	libusb_context	  *ctx,
	libusb_device	***list
);
void	libusb_free_device_list(
	libusb_device	**list,
	int		  unref_devices
);
libusb_device	*libusb_ref_device(
	libusb_device	*dev
);
void	libusb_unref_device(
	libusb_device	*dev
);

uint8_t	libusb_get_bus_number(
	libusb_device	*dev
);

int	libusb_open(
	libusb_device		 *dev,
	libusb_device_handle	**dev_handle
);
void	libusb_close(
	libusb_device_handle	*dev_handle
);

int	libusb_set_configuration(
	libusb_device_handle	*dev_handle,
	int			 configuration
);
int	libusb_claim_interface(
	libusb_device_handle	*dev_handle,
	int			 interface_number
);
int	libusb_release_interface(
	libusb_device_handle	*dev_handle,
	int			 interface_number
);

/** For this function to correctly work as expected and not error out,
 * *interface_number* must be the most recently successfully claimed,
 * and not yet released, interface on a given *dev_handle*, and,
 * depending on the platform and/or libusb 0.1 implementation,
 * this may not work, if any of its interfaces has been released ever since. */
int	libusb_set_interface_alt_setting(
	libusb_device_handle	*dev_handle,
	int			 interface_number,
	int			 alternate_setting
);
int	libusb_clear_halt(
	libusb_device_handle	*dev_handle,
	unsigned char		 endpoint
);
int	libusb_reset_device(
	libusb_device_handle	*dev_handle
);

int	libusb_detach_kernel_driver(
	libusb_device_handle	*dev_handle,
	int			 interface_number
);

/* Sync I/O. */
int	libusb_control_transfer(
	libusb_device_handle	*dev_handle,
	uint8_t			 bmRequestType,
	uint8_t			 bRequest,
	uint16_t		 wValue,
	uint16_t		 wIndex,
	unsigned char		*data,
	uint16_t		 wLength,
	unsigned int		 timeout
);
int	libusb_bulk_transfer(
	libusb_device_handle	*dev_handle,
	unsigned char		 endpoint,
	unsigned char		*data,
	int			 length,
	int			*transferred,
	unsigned int		 timeout
);
int	libusb_interrupt_transfer(
	libusb_device_handle	*dev_handle,
	unsigned char		 endpoint,
	unsigned char		*data,
	int			 length,
	int			*transferred,
	unsigned int		 timeout
);

/* USB descriptors functions. */
int	libusb_get_device_descriptor(
	libusb_device			*dev,
	struct libusb_device_descriptor	*desc
);
int	libusb_get_config_descriptor(
	libusb_device			 *dev,
	uint8_t				  config_index,
	struct libusb_config_descriptor	**config
);
void	libusb_free_config_descriptor(
	struct libusb_config_descriptor	*config
);

int	libusb_get_string_descriptor(
	libusb_device_handle	*dev_handle,
	uint8_t			 desc_index,
	uint16_t		 langid,
	unsigned char		*data,
	int			 length
);
int	libusb_get_string_descriptor_ascii(
	libusb_device_handle	*dev_handle,
	uint8_t			 desc_index,
	unsigned char		*data,
	int			 length
);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* LIBUSB_COMPAT_1_0_H */
