/*!
 * @file libshut.c
 * @brief HID Library - SHUT communication sub driver
 *
 * @author Copyright (C)
 *  2006 - 2009 Arnaud Quette <aquette.dev@gmail.com>
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

/* TODO list
 * - cleanup, cleanup, cleanup
 * - manage interrupt and complete libshut_get_interrupt / shut_control_msg routing
 * - baudrate negotiation
 * - complete shut_strerror
 * - validate / complete commands and data table in mge-hid from mge-shut
 */

#include "config.h" /* must be the first header */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "nut_stdint.h" /* for uint8_t, uint16_t, uint32_t */

#include "serial.h"
#include "libshut.h"
#include "common.h" /* for xmalloc, upsdebugx prototypes */

#define SHUT_DRIVER_NAME	"SHUT communication driver"
#define SHUT_DRIVER_VERSION	"0.89"

/* communication driver description structure */
upsdrv_info_t comm_upsdrv_info = {
	SHUT_DRIVER_NAME,
	SHUT_DRIVER_VERSION,
	NULL,
	0,
	{ NULL }
};

#define MAX_TRY 4
#define MAX_STRING_SIZE 128

/*!
 * HID descriptor, completed with desc{type,len}
 */
struct my_hid_descriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bcdHID;
	uint8_t  bCountryCode;
	uint8_t  bNumDescriptors;
	uint8_t  bReportDescriptorType;
	uint16_t wDescriptorLength;
};

/*!********************************************************
 * USB spec information, synchronised with (ripped from) libusb
 */

/*
 * Device and/or Interface Class codes
 */
#define USB_CLASS_PER_INTERFACE		0	/* for DeviceClass */
#define USB_CLASS_AUDIO			1
#define USB_CLASS_COMM			2
#define USB_CLASS_HID			3
#define USB_CLASS_PRINTER		7
#define USB_CLASS_MASS_STORAGE		8
#define USB_CLASS_HUB			9
#define USB_CLASS_DATA			10
#define USB_CLASS_VENDOR_SPEC		0xff

/*
 * Descriptor types
 */
#define USB_DT_DEVICE			0x01
#define USB_DT_CONFIG			0x02
#define USB_DT_STRING			0x03
#define USB_DT_INTERFACE		0x04
#define USB_DT_ENDPOINT			0x05

#define USB_DT_HID			0x21
#define USB_DT_REPORT			0x22
#define USB_DT_PHYSICAL			0x23
#define USB_DT_HUB			0x29

/*
 * Descriptor sizes per descriptor type
 */
#define USB_DT_DEVICE_SIZE		18
#define USB_DT_CONFIG_SIZE		9
#define USB_DT_INTERFACE_SIZE		9
#define USB_DT_ENDPOINT_SIZE		7
#define USB_DT_ENDPOINT_AUDIO_SIZE	9	/* Audio extension */
#define USB_DT_HUB_NONVAR_SIZE		7

/*
 * Standard requests
 */
#define USB_REQ_GET_STATUS              0x00
#define USB_REQ_CLEAR_FEATURE           0x01
/* 0x02 is reserved */
#define USB_REQ_SET_FEATURE             0x03
/* 0x04 is reserved */
#define USB_REQ_SET_ADDRESS             0x05
#define USB_REQ_GET_DESCRIPTOR          0x06
#define USB_REQ_SET_DESCRIPTOR          0x07
#define USB_REQ_GET_CONFIGURATION       0x08
#define USB_REQ_SET_CONFIGURATION       0x09
#define USB_REQ_GET_INTERFACE           0x0A
#define USB_REQ_SET_INTERFACE           0x0B
#define USB_REQ_SYNCH_FRAME             0x0C

#define USB_TYPE_STANDARD               (0x00 << 5)
#define USB_TYPE_CLASS                  (0x01 << 5)
#define USB_TYPE_VENDOR                 (0x02 << 5)
#define USB_TYPE_RESERVED               (0x03 << 5)

#define USB_RECIP_DEVICE                0x00
#define USB_RECIP_INTERFACE             0x01
#define USB_RECIP_ENDPOINT              0x02
#define USB_RECIP_OTHER                 0x03

/*
 * Various libusb API related stuff
 */
#define USB_ENDPOINT_IN                 0x80
#define USB_ENDPOINT_OUT                0x00
/*!
 * end of USB spec information
 *********************************************************/

/*!
 * HID definitions
 */
#define HID_REPORT_TYPE_INPUT	0x01
#define HID_REPORT_TYPE_OUTPUT	0x02
#define HID_REPORT_TYPE_FEATURE	0x03

#define REQUEST_TYPE_USB        0x80
#define REQUEST_TYPE_HID        0x81
#define REQUEST_TYPE_GET_REPORT 0xa1
#define REQUEST_TYPE_SET_REPORT 0x21

#define MAX_REPORT_SIZE         0x1800

/*!
 * SHUT definitions - From Simplified SHUT spec
 */

#define SHUT_TYPE_REQUEST       0x01
#define SHUT_TYPE_RESPONSE      0x04
#define SHUT_TYPE_NOTIFY        0x05
#define SHUT_OK                 0x06
#define SHUT_NOK                0x15
/* sync signals are also used to set the notification level */
#define SHUT_SYNC               0x16 /* complete notifications - not yet managed */
                                     /* but needed for some early Ellipse models */
#define SHUT_SYNC_LIGHT         0x17 /* partial notifications */
#define SHUT_SYNC_OFF           0x18 /* disable notifications - only do polling */
#define SHUT_PKT_LAST           0x80

#define SHUT_TIMEOUT 3000

/*!
 * SHUT functions for HID marshalling
 */

/* Expected evaluated types for the API after typedefs:
 * static int shut_get_descriptor(int upsfd, unsigned char type,
 *			   unsigned char index, void *buf, int size);
 * static int shut_get_string_simple(int upsfd, int index,
 *			   char *buf, size_t buflen);
 * static int libshut_get_report(int upsfd, int ReportId,
 *			   unsigned char *raw_buf, int ReportSize );
 * static int libshut_set_report(int upsfd, int id, unsigned char *pkt,
 *             int reportlen);
 * static int libshut_get_interrupt(int upsfd, unsigned char *buf,
 *			   int bufsize, int timeout);
 * static void libshut_close(int upsfd);
 */
static int shut_get_descriptor(
	usb_dev_handle upsfd,
	usb_ctrl_requesttype type,
	usb_ctrl_descindex index,
	void *buf,
	usb_ctrl_charbufsize size);

static int shut_get_string_simple(
	usb_dev_handle upsfd,
	usb_ctrl_strindex index,
	char *buf,
	usb_ctrl_charbufsize buflen);

static int libshut_get_report(
	usb_dev_handle upsfd,
	usb_ctrl_repindex ReportId,
	usb_ctrl_charbuf raw_buf,
	usb_ctrl_charbufsize ReportSize);

static int libshut_set_report(
	usb_dev_handle upsfd,
	usb_ctrl_repindex id,
	usb_ctrl_charbuf pkt,
	usb_ctrl_charbufsize reportlen);

static int libshut_get_interrupt(
	usb_dev_handle upsfd,
	usb_ctrl_charbuf buf,
	usb_ctrl_charbufsize bufsize,
	usb_ctrl_timeout_msec timeout);

static void libshut_close(usb_dev_handle upsfd);

/* FIXME */
static const char * shut_strerror(void) { return ""; }

/*!
 * From SHUT specifications
 * sync'ed with libusb
 */

typedef struct shut_ctrltransfer_s {
	uint8_t  bRequestType;
	uint8_t  bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;

	uint32_t timeout;      /* in milliseconds */

	/* pointer to data */
	void *data;
	/*   uint8_t  padding[8]; for use with shut_set_report?! */
} shut_ctrltransfer_t;


typedef union hid_data_t {
	shut_ctrltransfer_t  hid_pkt;
	uint8_t raw_pkt[8]; /* max report lengh, was 8 */
} hid_data_t;

typedef struct shut_packet_s {
	uint8_t bType;
	uint8_t bLength;
	hid_data_t    data;
	uint8_t bChecksum;
} shut_packet_t;

typedef union shut_data_t {
	shut_packet_t shut_pkt;
	uint8_t raw_pkt[11];
} shut_data_t;

typedef union hid_desc_data_t {
	struct my_hid_descriptor hid_desc;
	uint8_t    raw_desc[9]; /* max report lengh, aws 9 */
} hid_desc_data_t;

/* Device descriptor */
typedef struct device_descriptor_s {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bcdUSB;
	uint8_t  bDeviceClass;
	uint8_t  bDeviceSubClass;
	uint8_t  bDeviceProtocol;
	uint8_t  bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t  iManufacturer;
	uint8_t  iProduct;
	uint8_t  iSerialNumber;
	uint8_t  bNumConfigurations;
} device_descriptor_t;
#if 0
typedef union device_desc_data_t {
	device_descriptor_t dev_desc;
	uint8_t       raw_desc[18];
} device_desc_data_t;
#endif

/* Low level SHUT (Serial HID UPS Transfer) routines  */

/* Expected evaluated types for the API after typedefs:
static void setline(int upsfd, int set);
static int shut_synchronise(int upsfd);
static int shut_wait_ack(int upsfd);
static int shut_interrupt_read(int upsfd, int ep, unsigned char *bytes,
                        int size, int timeout);
static int shut_control_msg(int upsfd, int requesttype, int request, int value,
                        int index, unsigned char *bytes, int size, int timeout);
 */
static void setline(
	usb_dev_handle upsfd,
	int set);

static int shut_synchronise(usb_dev_handle upsfd);

static int shut_wait_ack(usb_dev_handle upsfd);

static int shut_interrupt_read(
	usb_dev_handle upsfd,
	usb_ctrl_endpoint ep,
	usb_ctrl_charbuf bytes,
	usb_ctrl_charbufsize size,
	usb_ctrl_timeout_msec timeout);

static int shut_control_msg(
	usb_dev_handle upsfd,
	usb_ctrl_requesttype requesttype,
	usb_ctrl_request request,
	usb_ctrl_msgvalue value,
	usb_ctrl_repindex index,
	usb_ctrl_charbuf bytes,
	usb_ctrl_charbufsize size,
	usb_ctrl_timeout_msec timeout);

/* Data portability */
/* realign packet data according to Endianess */
#define BYTESWAP(in) ((((uint16_t)in & 0x00FF) << 8) + (((uint16_t)in & 0xFF00) >> 8))
static void align_request(struct shut_ctrltransfer_s *ctrl)
{
#if (defined (WORDS_BIGENDIAN)) && (WORDS_BIGENDIAN)
	/* Sparc/Mips/... are big endian, USB/SHUT little endian */
	(*ctrl).wValue    = BYTESWAP((*ctrl).wValue);
	(*ctrl).wIndex    = BYTESWAP((*ctrl).wIndex);
	(*ctrl).wLength   = BYTESWAP((*ctrl).wLength);
#else
	NUT_UNUSED_VARIABLE(ctrl);
#endif
}

#if (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC)
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#if (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC)
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
/* On success, fill in the curDevice structure and return the report
 * descriptor length. On failure, return -1.
 * Note: When callback is not NULL, the report descriptor will be
 * passed to this function together with the upsfd and SHUTDevice_t
 * information. This callback should return a value > 0 if the device
 * is accepted, or < 1 if not.
 */

/* Expected evaluated types for the API after typedefs:
 * static int libshut_open(int *arg_upsfd, SHUTDevice_t *curDevice, char *arg_device_path,
 *                  int (*callback)(int arg_upsfd, SHUTDevice_t *hd,
 *                  unsigned char *rdbuf, int rdlen))
 */
static int libshut_open(
	usb_dev_handle *arg_upsfd,
	SHUTDevice_t *curDevice,
	char *arg_device_path,
	int (*callback)(usb_dev_handle arg_upsfd, SHUTDevice_t *hd,
		            usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen))
{
	int ret, res;
	/* Below we cast this buffer as sometimes containing entried of type
	 * "struct device_descriptor_s" or "struct my_hid_descriptor".
	 * Currently both of these are sized "2", and I don't see a way
	 * to require a "max()" of such sizes to align for generally.
	 */
	usb_ctrl_char buf[20] __attribute__((aligned(4)));
	char string[MAX_STRING_SIZE];
	struct my_hid_descriptor *desc;
	struct device_descriptor_s *dev_descriptor;

	/* report descriptor */
	usb_ctrl_char	rdbuf[MAX_REPORT_SIZE];
	usb_ctrl_charbufsize			rdlen;
	/* All devices use HID descriptor at index 0. However, some newer
	 * Eaton units have a light HID descriptor at index 0, and the full
	 * version is at index 1 (in which case, bcdDevice == 0x0202) */
	usb_ctrl_descindex	hid_desc_index = 0;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_TYPE_LIMIT_COMPARE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-unsigned-zero-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_TYPE_LIMIT_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-type-limit-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#pragma clang diagnostic ignored "-Wtautological-compare"
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
	static int usb_hid_number_opts_parsed = 0;
	if (!usb_hid_number_opts_parsed) {
		const char *s;
		unsigned short us = 0;
		if ((s = getval("usb_hid_desc_index"))) {
			if (!str_to_ushort(s, &us, 16) || (us > USB_CTRL_DESCINDEX_MAX)) {
				fatalx(EXIT_FAILURE, "%s: could not parse usb_hid_desc_index", __func__);
			}
			hid_desc_index = (usb_ctrl_descindex)us;
		}
		usb_hid_number_opts_parsed = 1;
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_TYPE_LIMIT_COMPARE) )
# pragma GCC diagnostic pop
#endif

	if (!arg_device_path) {
		fatalx(EXIT_FAILURE, "%s: arg_device_path=null", __func__);
	}

	upsdebugx(2, "libshut_open: using port %s", arg_device_path);

	/* If device is still open, close it */
	if (VALID_FD_SER(*arg_upsfd)) {
		ser_close(*arg_upsfd, arg_device_path);
	}

	/* initialize serial port */
	/* FIXME: add variable baudrate detection */
	*arg_upsfd = ser_open(arg_device_path);
	ser_set_speed(*arg_upsfd, arg_device_path, B2400);
	setline(*arg_upsfd, 1);

	/* initialise communication */
	if (!shut_synchronise(*arg_upsfd))
	{
		upsdebugx(2, "No communication with UPS");
		return -1;
	}

	upsdebugx(2, "Communication with UPS established");

	/* we can skip the rest due to serial bus specifics! */
	if (!callback) {
		return 1;
	}

	/* Get DEVICE descriptor */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-align"
#endif
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wcast-align"
#endif
	dev_descriptor = (struct device_descriptor_s *)buf;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic pop
#endif
	res = shut_get_descriptor(*arg_upsfd, USB_DT_DEVICE, 0, buf, USB_DT_DEVICE_SIZE);
	/* res = shut_control_msg(devp, USB_ENDPOINT_IN+1, USB_REQ_GET_DESCRIPTOR,
	(USB_DT_DEVICE << 8) + 0, 0, buf, 0x9, SHUT_TIMEOUT); */
	if (res < 0)
	{
		upsdebugx(2, "Unable to get DEVICE descriptor (%s)", shut_strerror());
		return -1;
	}

	if (res < 9)
	{
		upsdebugx(2, "DEVICE descriptor too short (expected %d, got %d)",
		      USB_DT_DEVICE_SIZE, res);
		return -1;
	}

	/* collect the identifying information of this
		device. Note that this is safe, because
		there's no need to claim an interface for
		this (and therefore we do not yet need to
		detach any kernel drivers). */

	free(curDevice->Vendor);
	free(curDevice->Product);
	free(curDevice->Serial);
	free(curDevice->Bus);
	free(curDevice->Device);
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	free(curDevice->BusPort);
#endif
	memset(curDevice, '\0', sizeof(*curDevice));

	curDevice->VendorID = dev_descriptor->idVendor;
	curDevice->ProductID = dev_descriptor->idProduct;
	curDevice->Bus = strdup("serial");
	curDevice->Device = strdup(arg_device_path);
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	curDevice->BusPort = (char *)malloc(4);
	if (curDevice->BusPort == NULL) {
		fatal_with_errno(EXIT_FAILURE, "Out of memory");
	}
	upsdebugx(2, "%s: NOTE: BusPort is always zero with libshut", __func__);
	sprintf(curDevice->BusPort, "%03d", 0);
#endif

	curDevice->bcdDevice = dev_descriptor->bcdDevice;
	curDevice->Vendor = NULL;
	if (dev_descriptor->iManufacturer) {
		ret = shut_get_string_simple(*arg_upsfd, dev_descriptor->iManufacturer,
			string, MAX_STRING_SIZE);
		if (ret > 0) {
			curDevice->Vendor = strdup(string);
		}
	}
	if (curDevice->Vendor == NULL) {
		curDevice->Vendor = strdup("Eaton");
	}

	/* ensure iProduct retrieval */
	if (dev_descriptor->iProduct) {
		ret = shut_get_string_simple(*arg_upsfd, dev_descriptor->iProduct, string, MAX_STRING_SIZE);
	} else {
		ret = 0;
	}
	if (ret > 0) {
		curDevice->Product = strdup(string);
	} else {
		curDevice->Product = strdup("unknown");
	}

	if (dev_descriptor->iSerialNumber) {
		ret = shut_get_string_simple(*arg_upsfd, dev_descriptor->iSerialNumber, string, 0x25);
	} else {
		ret = 0;
	}

	if (ret > 0) {
		curDevice->Serial = strdup(string);
	} else {
		curDevice->Serial = strdup("unknown");
	}

	upsdebugx(2, "- VendorID: %04x", curDevice->VendorID);
	upsdebugx(2, "- ProductID: %04x", curDevice->ProductID);
	upsdebugx(2, "- Manufacturer: %s", curDevice->Vendor);
	upsdebugx(2, "- Product: %s", curDevice->Product);
	upsdebugx(2, "- Serial Number: %s", curDevice->Serial);
	upsdebugx(2, "- Bus: %s", curDevice->Bus);
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	upsdebugx(2, "- Bus Port: %s", curDevice->BusPort ? curDevice->BusPort : "unknown");
#endif
	upsdebugx(2, "- Device: %s", curDevice->Device ? curDevice->Device : "unknown");
	upsdebugx(2, "- Device release number: %04x", curDevice->bcdDevice);
	upsdebugx(2, "Device matches");

	if ((curDevice->VendorID == 0x463) && (curDevice->bcdDevice == 0x0202)) {
		upsdebugx(1, "Eaton device v2.02. Using full report descriptor");
		if (!getval("usb_hid_desc_index"))
			hid_desc_index = 1;
	}

	/* Get HID descriptor */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-align"
#endif
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wcast-align"
#endif
	desc = (struct my_hid_descriptor *)buf;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_CAST_ALIGN)
# pragma GCC diagnostic pop
#endif
	res = shut_get_descriptor(*arg_upsfd, USB_DT_HID, hid_desc_index, buf, 0x9);
	/* res = shut_control_msg(devp, USB_ENDPOINT_IN+1, USB_REQ_GET_DESCRIPTOR,
			(USB_DT_HID << 8) + 0, 0, buf, 0x9, SHUT_TIMEOUT); */

	if (res < 0)
	{
		upsdebugx(2, "Unable to get HID descriptor (%s)", shut_strerror());
		return -1;
	}

	if (res < 9)
	{
		upsdebugx(2, "HID descriptor too short (expected %d, got %d)", 8, res);
		return -1;
	}

	/* USB_LE16_TO_CPU(desc->wDescriptorLength); */
	desc->wDescriptorLength = (0x00FF & (uint8_t)buf[7]) | ((0x00FF & (uint8_t)buf[8]) << 8);
	upsdebugx(2, "HID descriptor retrieved (Reportlen = %u)", desc->wDescriptorLength);

/*
	if (!dev->config) {
		upsdebugx(2, "  Couldn't retrieve descriptors");
		return -1;
	}
*/

	rdlen = desc->wDescriptorLength;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
	if (rdlen > sizeof(rdbuf) || rdlen > INT_MAX) {
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
# pragma GCC diagnostic pop
#endif
		upsdebugx(2,
			"HID descriptor too long %" PRI_NUT_USB_CTRL_CHARBUFSIZE
			" (max %" PRIuSIZE ")",
			rdlen, sizeof(rdbuf));
		return -1;
	}

	/* Get REPORT descriptor */
	res = shut_get_descriptor(*arg_upsfd, USB_DT_REPORT, hid_desc_index, rdbuf, rdlen);
	/* res = shut_control_msg(devp, USB_ENDPOINT_IN+1, USB_REQ_GET_DESCRIPTOR,
				(USB_DT_REPORT << 8) + 0, 0, ReportDesc,
			desc->wDescriptorLength, SHUT_TIMEOUT); */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
	if (res >= 0 && (uintmax_t)res < (uintmax_t)SIZE_MAX && (size_t)res == rdlen)
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) )
# pragma GCC diagnostic pop
#endif
	{
		res = callback(*arg_upsfd, curDevice, rdbuf, rdlen);
		if (res < 1) {
			upsdebugx(2, "Caller doesn't like this device");
			return -1;
		}

		upsdebugx(2,
			"Report descriptor retrieved (Reportlen = %"
			PRI_NUT_USB_CTRL_CHARBUFSIZE ")", rdlen);
		upsdebugx(2, "Found HID device");
		fflush(stdout);

		return (int)rdlen;
	}

	if (res < 0)
	{
		upsdebugx(2, "Unable to get Report descriptor (%d)", res);
	}
	else
	{
		upsdebugx(2,
			"Report descriptor too short (expected %"
			PRI_NUT_USB_CTRL_CHARBUFSIZE ", got %d)",
			rdlen, res);
	}

	upsdebugx(2, "libshut: No appropriate HID device found");
	fflush(stdout);

	return -1;
}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && (!defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_INSIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE_BESIDEFUNC) )
# pragma GCC diagnostic pop
#endif

/* Expected evaluated types for the API after typedefs:
 * static void libshut_close(int arg_upsfd)
 */
static void libshut_close(usb_dev_handle arg_upsfd)
{
	if (INVALID_FD_SER(arg_upsfd)) {
		return;
	}

	ser_close(arg_upsfd, NULL);
}

/* return the report of ID=type in report
 * return -1 on failure, report length on success
 */
/* Expected evaluated types for the API after typedefs:
 * static int libshut_get_report(int arg_upsfd, int ReportId,
 *                      unsigned char *raw_buf, int ReportSize)
 */
static int libshut_get_report(
	usb_dev_handle arg_upsfd,
	usb_ctrl_repindex ReportId,
	usb_ctrl_charbuf raw_buf,
	usb_ctrl_charbufsize ReportSize)
{
	if (INVALID_FD_SER(arg_upsfd)) {
		return 0;
	}

	upsdebugx(4, "Entering libshut_get_report");

	return shut_control_msg(arg_upsfd,
		REQUEST_TYPE_GET_REPORT,
		/* == USB_ENDPOINT_IN + USB_TYPE_CLASS + USB_RECIP_INTERFACE, */
		 0x01,
		 ReportId+(0x03<<8), /* HID_REPORT_TYPE_FEATURE */
		 0, raw_buf, ReportSize, SHUT_TIMEOUT);
}

/* return ReportSize upon success ; -1 otherwise */
/* Expected evaluated types for the API after typedefs:
 * static int libshut_set_report(int arg_upsfd, int ReportId,
 *                     unsigned char *raw_buf, int ReportSize)
 */
static int libshut_set_report(
	usb_dev_handle arg_upsfd,
	usb_ctrl_repindex ReportId,
	usb_ctrl_charbuf raw_buf,
	usb_ctrl_charbufsize ReportSize)
{
	int ret;

	if (INVALID_FD_SER(arg_upsfd)) {
		return 0;
	}

	upsdebugx(1,
		"Entering libshut_set_report (report %x, "
		"len %" PRI_NUT_USB_CTRL_CHARBUFSIZE ")",
		ReportId, ReportSize);

	if ((uintmax_t)ReportSize > (uintmax_t)INT_MAX) {
		upsdebugx(1, "%s: ReportSize exceeds INT_MAX", __func__);
		return -1;
	}

	upsdebug_hex (4, "==> Report after set", raw_buf, ReportSize);

	ret = shut_control_msg(arg_upsfd,
		REQUEST_TYPE_SET_REPORT,
		/* == USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE, */
		0x09,
		ReportId+(0x03<<8), /* HID_REPORT_TYPE_FEATURE */
		0, raw_buf, ReportSize, SHUT_TIMEOUT);

	return ((ret == 0) ? (int)ReportSize : ret);
}

/* Expected evaluated types for the API after typedefs:
 * static int libshut_get_string(int arg_upsfd,
 *                          int StringIdx, char *buf, size_t buflen)
 */
static int libshut_get_string(
	usb_dev_handle arg_upsfd,
	usb_ctrl_strindex StringIdx,
	char *buf,
	usb_ctrl_charbufsize buflen)
{
	int ret;

	if (INVALID_FD_SER(arg_upsfd)) {
		return -1;
	}

	ret = shut_get_string_simple(arg_upsfd, StringIdx, buf, buflen);
	if (ret > 0)
		upsdebugx(2, "-> String: %s (len = %i/%i)", buf, ret, (int)buflen);
	else
		upsdebugx(2, "- Unable to fetch buf");

	return ret;
}

/* Expected evaluated types for the API after typedefs:
 * static int libshut_get_interrupt(int arg_upsfd, unsigned char *buf,
 *                        int bufsize, int timeout)
 */
static int libshut_get_interrupt(
	usb_dev_handle arg_upsfd,
	usb_ctrl_charbuf buf,
	usb_ctrl_charbufsize bufsize,
	usb_ctrl_timeout_msec timeout)
{
	int ret;

	if (INVALID_FD_SER(arg_upsfd)) {
		return -1;
	}

	/* FIXME: hardcoded interrupt EP => need to get EP descr for IF descr */
	ret = shut_interrupt_read(arg_upsfd, 0x81, buf, bufsize, timeout);
	if (ret > 0)
		upsdebugx(6, " ok");
	else
		upsdebugx(6, " none (%i)", ret);

	return ret;
}

shut_communication_subdriver_t shut_subdriver = {
	SHUT_DRIVER_NAME,
	SHUT_DRIVER_VERSION,
	libshut_open,
	libshut_close,
	libshut_get_report,
	libshut_set_report,
	libshut_get_string,
	libshut_get_interrupt
};

/***********************************************************************/
/********** Low level SHUT (Serial HID UPS Transfer) routines **********/
/***********************************************************************/

/*
 * set RTS to on and DTR to off
 *
 * set : 1 to set comm
 * set : 0 to stop commupsh.
 */
/* Expected evaluated types for the API after typedefs:
 * void setline(int arg_upsfd, int set)
 */
void setline(usb_dev_handle arg_upsfd, int set)
{
	if (INVALID_FD_SER(arg_upsfd)) {
		return;
	}

	upsdebugx(3, "entering setline(%i)", set);

	if (set == 1) {
		ser_set_dtr(arg_upsfd, 0);
		ser_set_rts(arg_upsfd, 1);
	} else {
		ser_set_dtr(arg_upsfd, 1);
		ser_set_rts(arg_upsfd, 0);
	}
}

/*****************************************************************************
 * shut_synchronise ()
 *
 * initiate communication with the UPS
 *
 * return TRUE on success, FALSE on failure
 *
 *****************************************************************************/
/* Expected evaluated types for the API after typedefs:
 * int shut_synchronise(int arg_upsfd)
 */
int shut_synchronise(usb_dev_handle arg_upsfd)
{
	int retCode = 0;
	unsigned char c = SHUT_SYNC_OFF, reply;
	int try;

	upsdebugx (2, "entering shut_synchronise()");
	reply = '\0';

	/* FIXME: re enable notification support?
	switch (notification)
	{
		case OFF_NOTIFICATION:
			c = SHUT_SYNC_OFF;
			break;
		case LIGHT_NOTIFICATION:
			c = SHUT_SYNC_LIGHT;
			break;
		default:
		case COMPLETE_NOTIFICATION:
			c = SHUT_SYNC;
			break;
	}
	*/

	/* Sync with the UPS according to notification */
	for (try = 0; try < MAX_TRY; try++)
	{
		upsdebugx (3, "Syncing communication (try %i)", try);

		if ((ser_send_char(arg_upsfd, c)) == -1)
		{
			upsdebugx (3, "Communication error while writing to port");
			continue;
		}

		ser_get_char(arg_upsfd, &reply, 1, 0);
		if (reply == c)
		{
			upsdebugx (3, "Syncing and notification setting done");
			return 1;
		}
	}
	return retCode;
}

/*!
 * Compute a SHUT checksum for the packet "buf"
 */
/* Expected evaluated types for the API after typedefs:
 * static unsigned char shut_checksum(const unsigned char *buf, int bufsize)
 */
static unsigned char shut_checksum(
	const usb_ctrl_charbuf buf,
	usb_ctrl_charbufsize bufsize)
{
	usb_ctrl_charbufsize i;
	unsigned char chk=0;

	for(i=0; i<bufsize; i++)
		chk ^= (unsigned char)buf[i];

	upsdebugx (4, "shut_checksum: %02x", chk);
	return chk;
}

/* Expected evaluated types for the API after typedefs:
 * static int shut_packet_recv(int arg_upsfd, unsigned char *Buf, int datalen)
 */
static int shut_packet_recv(
	usb_dev_handle arg_upsfd,
	usb_ctrl_charbuf Buf,
	usb_ctrl_charbufsize datalen)
{
	unsigned char   Start[2];
	unsigned char   Frame[8];
	unsigned char   Chk[1];
	unsigned short  Size=8;
	unsigned short  Pos=0;
	unsigned char   Retry=0;
	int recv;
	/* FIXME: use this
	 * shut_data_t   sdata; */

	upsdebugx (4,
		"entering shut_packet_recv (%" PRI_NUT_USB_CTRL_CHARBUFSIZE ")",
		datalen);

	while(datalen>0 && Retry<3)
	{
		/* if(serial_read (SHUT_TIMEOUT, &Start[0]) > 0) */
		if(ser_get_char(arg_upsfd, &Start[0], SHUT_TIMEOUT/1000, 0) > 0)
		{
			/* sdata.shut_pkt.bType = Start[0]; */
			if(Start[0]==SHUT_SYNC)
			{
				upsdebugx (4, "received SYNC token");
				memcpy(Buf, Start, 1);
				return 1;
			}
			else if(Start[0]==SHUT_SYNC_OFF)
			{
				upsdebugx (4, "received SYNC_OFF token");
				memcpy(Buf, Start, 1);
				return 1;
			}
			else
			{
				/* if((serial_read (SHUT_TIMEOUT, &Start[1]) > 0) && */
				if( (ser_get_char(arg_upsfd, &Start[1], SHUT_TIMEOUT/1000, 0) > 0) &&
							((Start[1]>>4)==(Start[1]&0x0F)))
				{
					upsdebug_hex(4, "Receive", Start, 2);
					Size=Start[1]&0x0F;
					if( Size > 8 ) {
						upsdebugx (4,
							"shut_packet_recv: invalid frame size = %d",
							Size);
						ser_send_char(arg_upsfd, SHUT_NOK);
						Retry++;
						break;
					}
					/* sdata.shut_pkt.bLength = Size; */
					for(recv=0;recv<Size;recv++)
					{
						/* if(serial_read (SHUT_TIMEOUT, &Frame[recv]) < 1) */
						if(ser_get_char(arg_upsfd, &Frame[recv], SHUT_TIMEOUT/1000, 0) < 1)
							break;
					}
					upsdebug_hex(4, "Receive", Frame, Size);

					/* serial_read (SHUT_TIMEOUT, &Chk[0]); */
					ser_get_char(arg_upsfd, &Chk[0], SHUT_TIMEOUT/1000, 0);
					if(Chk[0]==shut_checksum(Frame, Size))
					{
						upsdebugx (4, "shut_checksum: %02x => OK", Chk[0]);
						memcpy(Buf, Frame, Size);
						datalen-=Size;
						Buf+=Size;
						Pos+=Size;
						Retry=0;

						ser_send_char(arg_upsfd, SHUT_OK);
						/* shut_token_send(SHUT_OK); */

						/* Check if there are more data to receive */
						if((Start[0] & 0xf0) == SHUT_PKT_LAST)
						{
							/* Check if it's a notification */
							if ((Start[0] & 0x0f) == SHUT_TYPE_NOTIFY)
							{
								/* TODO: process notification (dropped for now) */
								upsdebugx (4, "=> notification");
								datalen+=Pos;
								Pos=0;
							}
							else {
								return Pos;
							}
						}
						else
							upsdebugx (4,
								"need more data (%" PRI_NUT_USB_CTRL_CHARBUFSIZE ")!",
								datalen);
					}
					else
					{
						upsdebugx (4, "shut_checksum: %02x => NOK", Chk[0]);
						ser_send_char(arg_upsfd, SHUT_NOK);
						/* shut_token_send(SHUT_NOK); */
						Retry++;
					}
				}
				else {
					return 0;
				}
			}
		}
		else {
			Retry++;
		}
	} /* while */

	return 0;
}

/**********************************************************************/
/* Expected evaluated types for the API after typedefs:
 * static int shut_interrupt_read(int arg_upsfd, int ep,
 *                      unsigned char *bytes, int size,
 *                      int timeout)
 */
static int shut_interrupt_read(
	usb_dev_handle arg_upsfd,
	usb_ctrl_endpoint ep,
	usb_ctrl_charbuf bytes,
	usb_ctrl_charbufsize size,
	usb_ctrl_timeout_msec timeout)
{
/*
	usleep(timeout * 1000);
*/
	/* FIXME: to be written */
	NUT_UNUSED_VARIABLE(arg_upsfd);
	NUT_UNUSED_VARIABLE(ep);
	NUT_UNUSED_VARIABLE(bytes);
	NUT_UNUSED_VARIABLE(size);
	NUT_UNUSED_VARIABLE(timeout);
	return 0;
}

/**********************************************************************/
/* Expected evaluated types for the API after typedefs:
 * static int shut_get_string_simple(int arg_upsfd, int index,
 *                          char *buf, size_t buflen)
 */
static int shut_get_string_simple(
	usb_dev_handle arg_upsfd,
	usb_ctrl_strindex index,
	char *buf,
	usb_ctrl_charbufsize buflen)
{
	usb_ctrl_char tbuf[255];       /* Some devices choke on size > 255 */
	int ret, si, di;

	ret = shut_control_msg(arg_upsfd, USB_ENDPOINT_IN, USB_REQ_GET_DESCRIPTOR,
			(USB_DT_STRING << 8) + index, 0x0, tbuf, buflen, SHUT_TIMEOUT);
	if (ret < 0)
		return ret;

	if (tbuf[1] != USB_DT_STRING)
		return -EIO;

	if (tbuf[0] > ret)
		return -EFBIG;

	/* skip the UTF8 zero'ed high bytes */
	for (di = 0, si = 2; si < tbuf[0]; si += 2)
	{
		if (di >= (int)(buflen - 1))
			break;

		if (tbuf[si + 1])   /* high byte */
			buf[di++] = '?';
		else
			buf[di++] = (char)tbuf[si];
	}

	buf[di] = 0;
	return di;
}

/*
 * Human Interface Device (HID) functions
 *********************************************************************/

/**********************************************************************
 * shut_get_descriptor(int desctype, usb_ctrl_charbuf pkt)
 *
 * get descriptor specified by DescType and return it in Buf
 *
 * desctype  - from shutdataType
 * pkt       - where to store the report received
 *
 * return 0 on success, -1 on failure, -2 on NACK
 *
 *********************************************************************/
/* Expected evaluated types for the API after typedefs:
 * static int shut_get_descriptor(int arg_upsfd, unsigned char type,
 *                        unsigned char index, void *buf, int size)
 */
static int shut_get_descriptor(
	usb_dev_handle arg_upsfd,
	usb_ctrl_requesttype type,
	usb_ctrl_descindex index,
	void *buf,
	usb_ctrl_charbufsize size)
{
	memset(buf, 0, size);

	upsdebugx (2,
		"entering shut_get_descriptor(n %02x, %" PRI_NUT_USB_CTRL_CHARBUFSIZE ")",
		type, size);

	return shut_control_msg(arg_upsfd, USB_ENDPOINT_IN+(type>=USB_DT_HID?1:0),
				 USB_REQ_GET_DESCRIPTOR, (type << 8) + index, 0, buf, size, SHUT_TIMEOUT);
}

/* Take care of a SHUT transfer (sending and receiving data) */
/* Expected evaluated types for the API after typedefs:
 * static int shut_control_msg(int arg_upsfd, int requesttype, int request,
 *                    int value, int index, unsigned char *bytes, int size,
 *                    int timeout)
 */
static int shut_control_msg(
	usb_dev_handle arg_upsfd,
	usb_ctrl_requesttype requesttype,
	usb_ctrl_request request,
	usb_ctrl_msgvalue value,
	usb_ctrl_repindex index,
	usb_ctrl_charbuf bytes,
	usb_ctrl_charbufsize size,
	usb_ctrl_timeout_msec timeout)
{
	unsigned char shut_pkt[11];
	short Retry=1, set_pass = -1;
	usb_ctrl_charbufsize data_size, remaining_size = size;
	int i;
	struct shut_ctrltransfer_s ctrl;
	int ret = 0;

	upsdebugx (3, "entering shut_control_msg");

	/* deal for set requests */
	if (requesttype == REQUEST_TYPE_SET_REPORT)
	{
		set_pass = 1;
		/* add 8 for the first frame that declares a coming set */
		remaining_size+= 8;
	}

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-unsigned-zero-compare"
#endif
	/* Note: checking against limits of protocol struct fields,
	 * not against USB_CTRL_REQUEST_MAX et al, which are mostly int
	 */
	if (requesttype < 0 || (uintmax_t)requesttype > UINT8_MAX
	||  request < 0 || (uintmax_t)request > UINT8_MAX
	||  value < 0 || (uintmax_t)value > UINT16_MAX
	||  index < 0 || (uintmax_t)index > UINT16_MAX
	||  (uintmax_t)size > UINT16_MAX
	||  (uintmax_t)timeout > UINT32_MAX
	) {
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
# pragma GCC diagnostic pop
#endif
		upsdebugx (1, "%s: input values out of range", __func__);
		return -1;
	}

	/* build the control request */
	ctrl.bRequestType = (uint8_t)requesttype;
	ctrl.bRequest = (uint8_t)request;
	ctrl.wValue = (uint16_t)value;
	ctrl.wIndex = (uint16_t)index;
	ctrl.wLength = (uint16_t)size;
	ctrl.data = bytes;
	ctrl.timeout = (uint32_t)timeout;	/* in milliseconds */

	align_request(&ctrl);

	/* Send all data */
	while(remaining_size > 0 && Retry > 0) {

		if (requesttype == REQUEST_TYPE_SET_REPORT) {
			if (set_pass == 1) {
				data_size = 8;
				set_pass++; /* prepare for the next step */
			}
			else {
				data_size = size;
				upsdebug_hex(4, "data", bytes, data_size);
			}
		}
		else {
			/* Always 8 bytes payload for GET_REPORT with SHUT */
			data_size = 8;
		}

		/* Forge the SHUT Frame */
		shut_pkt[0] = SHUT_TYPE_REQUEST + ( ((requesttype == REQUEST_TYPE_SET_REPORT) && (remaining_size>8))? 0 : SHUT_PKT_LAST);
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-unsigned-zero-compare"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
		if (data_size < 0 || data_size > UCHAR_MAX) {
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
# pragma GCC diagnostic pop
#endif
			upsdebugx(1, "%s: ERROR: data_size %" PRI_NUT_USB_CTRL_CHARBUFSIZE
				" is out of range for SHUT packet",
				__func__, data_size);
			return -1;
		}
		if (data_size > 0x0F) {
			upsdebugx(1, "%s: WARNING: data_size %" PRI_NUT_USB_CTRL_CHARBUFSIZE
				" may be too large for SHUT packet?",
				__func__, data_size);
			/* Do not abort here - maybe there is intentional maths
			 * in the protocol with overlapping/shifted-away numbers?
			 */
		}
		shut_pkt[1] = (unsigned char)(data_size<<4) + (unsigned char)data_size;
		if ( (requesttype == REQUEST_TYPE_SET_REPORT) && (remaining_size < 8) )
			memcpy(&shut_pkt[2], bytes, data_size); /* we need to send ctrl.data  */
		else
			memcpy(&shut_pkt[2], &ctrl, 8);
		shut_pkt[(data_size+3) - 1] = shut_checksum(&shut_pkt[2], (unsigned char)data_size);

		/* Packets need only to be sent once
		* NACK handling should take care of the rest */
		if (Retry == 1)
		{
			ser_send_buf(arg_upsfd, shut_pkt, data_size+3);
			upsdebug_hex(3, "shut_control_msg", shut_pkt, data_size+3);
			/* serial_send (shut_pkt, data_size+3); */
		}

		i = shut_wait_ack (arg_upsfd);
		switch (i)
		{
			case 0:
				if (requesttype == REQUEST_TYPE_SET_REPORT)
					remaining_size-=data_size;
				else
					remaining_size = 0;

				Retry=1;
				break;

			case -1:
				if (Retry >= MAX_TRY)
				{
					upsdebugx(2, "Max tries reached while waiting for ACK, still getting errors");

					/* try to resync, and give one more try */
					Retry--;
					shut_synchronise(arg_upsfd);
					return i;
				}
				else
				{
					upsdebugx(4, "Retry = %i", Retry);
					/* Send a NACK to get a resend from the UPS */
					ser_send_char(arg_upsfd, SHUT_NOK);
					Retry++;
				}
				break;

			case -3:
				/* FIXME: notification caught => to be processed */

				/* Send a NACK for the moment, to get a resend from the UPS */
				ser_send_char(arg_upsfd, SHUT_NOK);
				Retry++;
				goto fallthrough_default;

			default:
			fallthrough_default:
				break;
		}
	}
	if (remaining_size != 0)
		return -1;

	/* now receive data, except for SET_REPORT */
	if (requesttype != REQUEST_TYPE_SET_REPORT)
		ret = shut_packet_recv (arg_upsfd, bytes, size);

	return ret;
}

/**********************************************************************
 * shut_wait_ack()
 *
 * wait for an ACK packet
 *
 * returns 0 on success, -1 on error, -2 on NACK, -3 on NOTIFICATION
 *
 *********************************************************************/
/* Expected evaluated types for the API after typedefs:
 * int shut_wait_ack(int arg_upsfd)
 */
int shut_wait_ack(usb_dev_handle arg_upsfd)
{
	int retCode = -1;
	unsigned char c = '\0';

	ser_get_char(arg_upsfd, &c, SHUT_TIMEOUT/1000, 0);
	if (c == SHUT_OK)
	{
		upsdebugx (2, "shut_wait_ack(): ACK received");
		retCode = 0;
	}
	else if (c == SHUT_NOK)
	{
		upsdebugx (2, "shut_wait_ack(): NACK received");
		retCode = -2;
	}
	else if ((c & 0x0f) == SHUT_TYPE_NOTIFY)
	{
		upsdebugx (2, "shut_wait_ack(): NOTIFY received");
		retCode = -3;
	}
	else if (c == '\0')
		upsdebugx (2, "shut_wait_ack(): Nothing received");

	return retCode;
}
