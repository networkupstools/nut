/* nutdrv_hashx.c - Driver for serial and USB UPS units with #* protocols
 *
 * Copyright (C)
 *   2025 Marco Trevisan (Treviño) <mail@3v1n0.net>
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
 *
 */

#include "common.h"
#include "config.h"
#include "dstate.h"
#include "main.h"
#include "nut_stdint.h"
#include "powerpanel.h"
#ifdef HASHX_SERIAL
#include "serial.h"
#endif
#ifdef HASHX_USB
#include "nut_libusb.h"
#include "hashx-usb-protocol.h"
#endif
#include "str.h"
#include "upshandler.h"
#include <stdio.h>
#include <string.h>

#define ENDCHAR		'\r'
#define IGNCHARS	""

#if defined(HASHX_USB) && defined(HASHX_SERIAL)
#define DRIVER_NAME	"Generic #* USB/Serial driver"
#elif defined(HASHX_USB)
#define DRIVER_NAME	"Generic #* USB driver"
#else
#define DRIVER_NAME	"Generic #* Serial driver"
#endif
#define DRIVER_VERSION	"0.04"

#define SESSION_ID	"OoNUTisAMAZINGoO"
#define SESSION_HASH	"74279F35A48F5F13"

/* These commands have been tested on an Atlantis Land A03-S1200 */
#define COMMAND_GET_SESSION_HASH		"X19" /*-> #16-hex-values */
#define COMMAND_SET_SESSION_ID			"K19" /* :value -> Status value */
#define COMMAND_SET_BEEPER_ENABLED		"K60" /* :0|1 -> Status value */
#define COMMAND_SET_FREQUENCY_MODE		"K39" /* :0|1 -> Status value */
#define COMMAND_GET_BYPASS_CONDITION_MANDATORY	"X87" /* -> #\xc7\x97\xf0\xf0 */
#define COMMAND_SET_BYPASS_CONDITION_MANDATORY	"K87" /* :0|1 -> Status value */
#define COMMAND_GET_BYPASS_CONDITION_OVERLOAD	"X15" /* -> #10 */
#define COMMAND_SET_BYPASS_CONDITION_OVERLOAD	"K15" /* :0|1 -> Status value */
#define COMMAND_GET_UPS_DEVICE_INFO		"X41" /* -> #1200,PE02022.002,000000000000,000000000000000000 */
#define COMMAND_GET_INPUT_VOLTAGE_SETTINGS	"X27" /* -> #230,290,162,10,300 */
#define COMMAND_GET_INPUT_VOLTAGE_UPPER_BOUND	"X51" /* -> #290 */
#define COMMAND_GET_INPUT_VOLTAGE_LOWER_BOUND	"X60" /* -> #162 */
#define COMMAND_GET_UPS_POWER_INFO		"X72" /* -> #1200,0720,230,40,70,05.20 */
#define COMMAND_GET_STATUS			"B"   /* -> #I239.0O237.0L007B100V27.0F50.1H50.1R040S\x80\0x84\0xd0\x80\x80\0xc0 */
#define COMMAND_START_BATTERY_TEST		"A"   /* -> Status value */
#define COMMAND_START_BATTERY_RUNTIME_TEST	"AH"  /* -> Status value */
#define COMMAND_START_BEEPER_TEST		"AU"  /* -> Status value */
#define COMMAND_STOP_TEST			"KA"  /* -> Status value */

#define COMMAND_UNKNOWN5			"X5"  /* -> #12,2,7,0,5,480 */
#define COMMAND_UNKNOWN71			"X71" /* -> #\x80\x80 */

/* Commands supported by the protocol that PowerMaster+ uses and not supported
 * by the devices tested so far:
 *  X4
 *  X5
 *  X15
 *  X14 | K14:(0|1)
 *  X17
 *  X23
 *  X24
 *  X27
 *  X28
 *  X29
 *  X30
 *  X33
 *  X41
 *  X43
 *  X51
 *  X53
 *  X58
 *  X60
 *  X61
 *  X71
 *  X72
 *  X73
 *  X80
 *  X87
 *  X88
 *  X98
 *  X99
 */

enum {
	STATUS_SUCCESS			= 0,
	/* Generic failure */
	STATUS_FAILURE			= -1,
	/* If the command is not known or supported by the device */
	STATUS_COMMAND_NOT_SUPPORTED	= -3,
	/* When a command parameter is not valid (e.g. "K60:foo") */
	STATUS_COMMAND_VALUE_INVALID	= -4,
	/* When a command is currently unavailable (e.g. "AH" while charging ) */
	STATUS_COMMAND_UNAVAILABLE	= -10,

	/* Local error: when a parsing error occurred */
	STATUS_UNKNOWN_FAILURE          = -255,
};

upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Marco Trevisan <mail@3v1n0.net>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#ifdef HASHX_SERIAL
static struct hashx_cmd_t {
	const char *cmd_name;
	const char *ups_cmd;
} hashx_cmd[] =
{
	{ "beeper.enable", COMMAND_SET_BEEPER_ENABLED":1" },
	{ "beeper.disable", COMMAND_SET_BEEPER_ENABLED":0" },
	{ "experimental.test.beeper.start", COMMAND_START_BEEPER_TEST },
	{ "experimental.test.beeper.stop", COMMAND_STOP_TEST },
	{ "test.battery.start.deep", COMMAND_START_BATTERY_RUNTIME_TEST },
	{ "test.battery.start.quick", COMMAND_START_BATTERY_TEST },
	{ "test.battery.stop", COMMAND_STOP_TEST },
	{ "calibrate.start", COMMAND_START_BATTERY_RUNTIME_TEST },
	{ "calibrate.stop", COMMAND_STOP_TEST },
};

static int hashx_instcmd(const char *cmd_name, const char *extra);
#endif

static void hashx_apply_status(char *buf, ssize_t transferred);

#ifdef HASHX_USB
#define HASHX_USB_EP_IN		0x81
#define HASHX_USB_EP_OUT		0x02

static int hashx_is_usb = 0;
static usb_dev_handle *hashx_udev = NULL;
static USBDevice_t hashx_usbdevice;
static USBDeviceMatcher_t *hashx_regex_matcher = NULL;
static USBDeviceMatcher_t *hashx_reopen_matcher = NULL;

/* The ID is also used by unrelated CyberPower HID devices. USB support
 * requires an explicit opt-in and a valid HashX status response.
 */
static usb_device_id_t hashx_usb_ids[] = {
	{ USB_DEVICE(0x0764, 0x0601), NULL },
	{ 0, 0, NULL }
};

static int hashx_usb_match(USBDevice_t *device, void *privdata)
{
	NUT_UNUSED_VARIABLE(privdata);
	return is_usb_device_supported(hashx_usb_ids, device) == SUPPORTED;
}

static USBDeviceMatcher_t hashx_device_matcher = {
	&hashx_usb_match,
	NULL,
	NULL
};

static void hashx_usb_free_device(void)
{
	free(hashx_usbdevice.Vendor);
	free(hashx_usbdevice.Product);
	free(hashx_usbdevice.Serial);
	free(hashx_usbdevice.Bus);
	free(hashx_usbdevice.Device);
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	free(hashx_usbdevice.BusPort);
#endif
	memset(&hashx_usbdevice, 0, sizeof(hashx_usbdevice));
}

static int hashx_usb_open(void)
{
	int ret;
	USBDeviceMatcher_t *matcher;

	matcher = hashx_reopen_matcher ? hashx_reopen_matcher : hashx_regex_matcher;
	hashx_usb_free_device();
	ret = usb_subdriver.open_dev(&hashx_udev, &hashx_usbdevice, matcher, NULL);
	if (ret < 1) {
		return -1;
	}
	if (!hashx_reopen_matcher) {
		ret = USBNewExactMatcher(&hashx_reopen_matcher, &hashx_usbdevice);
		if (ret != 0) {
			fatal_with_errno(EXIT_FAILURE, "USBNewExactMatcher");
		}
		hashx_reopen_matcher->next = hashx_regex_matcher;
	}
	return 0;
}

static void hashx_usb_check_disconnect(int ret)
{
#if WITH_LIBUSB_1_0
	if (ret != LIBUSB_ERROR_NO_DEVICE) {
#else
	if (ret != -ENODEV) {
#endif
		return;
	}
	usb_subdriver.close_dev(hashx_udev);
	hashx_udev = NULL;
}

static int hashx_usb_read_packet(void *context, unsigned char *packet,
	size_t length, unsigned int timeout)
{
	int ret;

	if (length > INT_MAX || timeout > INT_MAX) {
		return -1;
	}
	ret = usb_interrupt_read((usb_dev_handle *)context, HASHX_USB_EP_IN,
		(usb_ctrl_charbuf)packet, (int)length, (int)timeout);
#if WITH_LIBUSB_1_0
	if (ret == LIBUSB_ERROR_TIMEOUT) {
#else
	if (ret == -ETIMEDOUT) {
#endif
		return 0;
	}
	if (ret < 0) {
		hashx_usb_check_disconnect(ret);
		return -1;
	}
	return ret;
}

static int hashx_usb_write_packet(void *context, const unsigned char *packet,
	size_t length, unsigned int timeout)
{
	int ret;

	if (length > INT_MAX || timeout > INT_MAX) {
		return -1;
	}
	ret = usb_interrupt_write((usb_dev_handle *)context, HASHX_USB_EP_OUT,
		(usb_ctrl_charbuf)packet, (int)length, (int)timeout);
	if (ret < 0) {
		hashx_usb_check_disconnect(ret);
		return -1;
	}
	return ret;
}

static ssize_t hashx_usb_status(char *buf, size_t bufsize)
{
	struct hashx_usb_reply reply;

	if (bufsize < HASHX_USB_FRAME_SIZE || (!hashx_udev && hashx_usb_open() < 0)) {
		return -1;
	}
	if (hashx_usb_status_query(hashx_udev, hashx_usb_read_packet,
		hashx_usb_write_packet, &reply) != 1
	 || !hashx_usb_status_valid(reply.data, reply.length)) {
		return -1;
	}

	/* The serial receiver excludes CR. Preserve that convention for the
	 * existing status mapping, including its binary status-byte tail.
	 */
	memcpy(buf, reply.data, reply.length - 1);
	buf[reply.length - 1] = '\0';
	return (ssize_t)(reply.length - 1);
}

static void hashx_usb_init(void)
{
	char *regex_array[USBMATCHER_REGEXP_ARRAY_LIMIT];
	int ret;

	regex_array[0] = getval("vendorid");
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor");
	regex_array[3] = getval("product");
	regex_array[4] = getval("serial");
	regex_array[5] = getval("bus");
	regex_array[6] = getval("device");
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	regex_array[7] = getval("busport");
#else
	if (getval("busport")) {
		fatalx(EXIT_FAILURE, "busport matching is unavailable in this build");
	}
#endif
	ret = USBNewRegexMatcher(&hashx_regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
	if (ret < 0) {
		fatal_with_errno(EXIT_FAILURE, "USBNewRegexMatcher");
	}
	if (ret > 0) {
		fatalx(EXIT_FAILURE, "invalid USB matching expression: %s", regex_array[ret - 1]);
	}
	hashx_regex_matcher->next = &hashx_device_matcher;
	if (hashx_usb_open() < 0) {
		fatalx(EXIT_FAILURE, "No matching USB 0764:0601 device available for HashX monitoring");
	}
	dstate_setinfo("ups.vendorid", "%04x", hashx_usbdevice.VendorID);
	dstate_setinfo("ups.productid", "%04x", hashx_usbdevice.ProductID);
}
#endif /* HASHX_USB */

#ifdef HASHX_SERIAL

static ssize_t hashx_recv(char *buf, size_t bufsize)
{
	ssize_t nread;

	nread = ser_get_line(
		upsfd, buf, bufsize - 1, ENDCHAR, IGNCHAR,
		SER_WAIT_SEC, SER_WAIT_USEC);

	assert(nread < (ssize_t) bufsize);
	buf[nread] = '\0';

	if (nut_debug_level >= 4) {
		char base_msg[128];
		int n;

		n = snprintf(
			base_msg, sizeof(base_msg),
			"%s: read %" PRIiSIZE ": ",
			__func__, nread);
		assert(n > 0);
		upsdebug_ascii(4, base_msg, buf, nread);
	}

	if (nread > 0 && *buf != '#') {
		upslogx(LOG_ERR, "Unexpected reply for #-protocol: %s", buf);
	}

	return nread;
}

static int hashx_status_value(char *buf, size_t bufsize)
{
	ssize_t nread;
	int ret = STATUS_UNKNOWN_FAILURE;

	if ((nread = hashx_recv(buf, bufsize)) < 2 || nread > 3) {
		upslogx(LOG_ERR, "Not an UPS status value: %s", buf);
		return ret;
	}

	if (!str_to_int(buf + 1, &ret, 10)) {
		upslogx(LOG_ERR, "Failed to convert %s to number", buf + 1);
		return ret;
	}

	upsdebugx(5, "Read status is %d", ret);
	return ret;
}

static int hashx_send_command(const char *command)
{
	size_t transferred;
	char buf[16];
	int status;

	if ((transferred = ser_send(upsfd, "%s%c", command, ENDCHAR)) != strlen (command) + 1) {
		upslogx(LOG_ERR,
			"%s: Failed to send command: %s",
			__func__, command);
		return STATUS_UNKNOWN_FAILURE;
	}

	if ((status = hashx_status_value(buf, sizeof(buf))) != STATUS_SUCCESS) {
		upslogx(LOG_ERR,
			"%s: unexpected hash protocol response: %s (status %d)",
			__func__, buf, status);
	}

	return status;
}
#endif /* HASHX_SERIAL */

/*
 * This is how PowerMaster+ 1.2.3 does when connecting to the UPS:
 * [pid 22999] write(219, "\r", 1)         = 1
 * [pid 22999] write(219, "X19\r", 4)      = 4
 * [pid 22999] write(219, "K19:RRO3143QJqhb1GCj\r", 21) = 21
 * [pid 22999] write(219, "X19\r", 4)      = 4
 * [pid 22999] write(219, "X41\r", 4)      = 4
 * [pid 23000] write(219, "\r", 1)         = 1
 * [pid 23000] write(219, "X41\r", 4)      = 4
 * [pid 23000] write(219, "B\r", 2)        = 2
 * [pid 23000] write(219, "X27\r", 4)      = 4
 * [pid 23000] write(219, "X72\r", 4)      = 4
 * [pid 23000] write(219, "X5\r", 3)       = 3
 * [pid 23000] write(219, "X53\r", 4)      = 4
 * [pid 23000] write(219, "X87\r", 4)      = 4
 * [pid 23000] write(219, "X28\r", 4)      = 4
 * [pid 23000] write(219, "X51\r", 4)      = 4
 * [pid 23000] write(219, "X60\r", 4)      = 4
 * [pid 23000] write(219, "X15\r", 4)      = 4
 * [pid 23000] write(219, "K14:0\r", 6)    = 6
 * [pid 23000] write(219, "X4\r", 3)       = 3
 * [pid 23000] write(219, "X58\r", 4)      = 4
 * [pid 23000] write(219, "X30\r", 4)      = 4
 * [pid 23000] write(219, "X71\r", 4)      = 4
 * [pid 23000] write(219, "X43\r", 4)      = 4
 * [pid 23000] write(219, "X73\r", 4)      = 4
 * [pid 23000] write(219, "X88\r", 4)      = 4
 * [pid 23000] write(219, "X17\r", 4)      = 4
 * [pid 23000] write(219, "X98\r", 4)      = 4
 * [pid 23000] write(219, "X14\r", 4)      = 4
 * [pid 23000] write(219, "X99\r", 4)      = 4
 * [pid 23000] write(219, "X29\r", 4)      = 4
 * [pid 23000] write(219, "X61\r", 4)      = 4
 * [pid 23000] write(219, "X33\r", 4)      = 4
 * [pid 23000] write(219, "X23\r", 4)      = 4
 * [pid 23000] write(219, "X24\r", 4)      = 4
 * [pid 23000] write(219, "X80\r", 4)      = 4
 * [pid 23000] write(219, "B\r", 2)        = 2
 * [pid 23000] write(219, "B\r", 2)        = 2
 * .... These last ones are the polling requests repeated forever ....
 */

void upsdrv_initinfo(void)
{
#ifdef HASHX_SERIAL
	ssize_t transferred;
	char buf[256];
	char *reply;
	int status;
	int ival;
	double dval;
	size_t i;
#endif

#ifdef HASHX_USB
	if (hashx_is_usb) {
		char status_buf[256];
		ssize_t status_length;

		status_length = hashx_usb_status(status_buf, sizeof(status_buf));
		if (status_length < 0) {
			fatalx(EXIT_FAILURE, "USB monitoring probe did not return a complete valid HashX status frame");
		}
		dstate_setinfo("device.type", "ups");
		dstate_setinfo("driver.parameter.usb_monitor", "enabled");
		hashx_apply_status(status_buf, status_length);
		return;
	}
#endif

#ifdef HASHX_SERIAL

	for (i = 0; ; ++i) {
		upsdebugx(4, "Checking if device supports the #-protocol... [%" PRIuSIZE "]", i+1);
		if ((transferred = ser_send_char(upsfd, ENDCHAR)) != 1) {
			fatalx(EXIT_FAILURE,
				"%s: Initialization failure %s",
				__func__, device_path);
		}
		if ((status = hashx_status_value(buf, sizeof(buf))) == -1) {
			break;
		}

		if (i < 3) {
			continue;
		}

		fatalx(EXIT_FAILURE,
			"%s: unexpected hash protocol response: %s (status %d)",
			__func__, buf, status);
	}

	/* The K19 function needs a 16 bytes value and the device should reply
	 * with an hash or a crc of it, it's not clear what's the algorithm it
	 * uses, but given that the returned value is static, we can just always
	 * send one value we know about, to verify the protocol.
	 */
	if ((status = hashx_send_command(COMMAND_SET_SESSION_ID":"SESSION_ID)) != STATUS_SUCCESS) {
		fatalx(EXIT_FAILURE,
			"%s: Failed to set session ID: %s",
			__func__, SESSION_ID);
	}

	if ((transferred = ser_send(upsfd, COMMAND_GET_SESSION_HASH"%c", ENDCHAR)) != 4) {
		fatalx(EXIT_FAILURE, "%s: Failed to get session hash", __func__);
	}
	if ((transferred = hashx_recv(buf, sizeof(buf))) != strlen(SESSION_HASH) + 1
	 || (transferred > 0 && strncmp(buf, "#"SESSION_HASH, sizeof(buf)) != 0)
	) {
		fatalx(EXIT_FAILURE,
			"%s: unexpected hash protocol response: %s",
			__func__, buf);
	}

	/* XXX: Maybe now repeat the same with a random value, without checking
	 * the content of the returned value, but just in case the UPS uses this
	 * value as a "session" identifier, to make sure we are starting a new
	 * one.
	 * Another option could be to keep a list of known request, response
	 * pair values and use them randomly.
	 * However, not doing it for now since it seems unneeded, while just
	 * always reusing the same value works good enough in my tests.
	 */

	dstate_setinfo("device.type", "ups");
	dstate_setinfo("ups.type", "online");

	/* FIXME: We need to understand how PowerManager+ figures this out,
	 * as it supports both On-Line and Line Interactive devices.
	 * Or is it related to the support to the bypass feature?
	 * Maybe it's what `X5` command provides? We need to test more devices!
	 */
	dstate_setinfo("ups.mode", "line-interactive");

	if ((transferred = ser_send(upsfd, COMMAND_GET_UPS_DEVICE_INFO"%c", ENDCHAR)) != 4) {
		fatalx(EXIT_FAILURE, "%s: Failed to get UPS info", __func__);
	}
	if ((transferred = hashx_recv(buf, sizeof(buf))) < 3) {
		fatalx(EXIT_FAILURE,
			"%s: unexpected UPS info response: %s",
			__func__, buf);
	}
	/* This is in the format:
	 * #1200,PE02022.002,000000000000,000000000000000000
	 *  1    2           3            4
	 * 1) Model
	 * 2) Firmware version
	 * 3) ??? [maybe device serial number?]
	 * 4) ??? [as above?]
	 */
	reply = buf + 1;
	dstate_setinfo("device.model", "%s", strsep(&reply, ","));
	if (reply) {
		dstate_setinfo("ups.firmware", "%s", strsep(&reply, ","));
	}

	if ((transferred = ser_send(upsfd, COMMAND_GET_INPUT_VOLTAGE_SETTINGS"%c", ENDCHAR)) != 4) {
		upslogx(LOG_ERR, "%s: Failed to get UPS voltage settings", __func__);
	}
	if ((transferred = hashx_recv(buf, sizeof(buf))) < 3) {
		upslogx(LOG_ERR, "%s: unexpected UPS voltage settings response: %s",
			__func__, buf);
	}
	/* This is in the format:
	 * #230,290,162,10,300
	 *  1   2   3   4  5
	 * 1) Supplied power voltage
	 * 2) Power failure voltage upper bound
	 * 3) Power failure voltage lower bound
	 * 4) ???
	 * 5) ???
	 */
	reply = buf + 1;
	if (str_to_int(strsep(&reply, ","), &ival, 10)) {
		dstate_setinfo("input.voltage.nominal", "%d", ival);
	}
	if (reply && str_to_int(strsep(&reply, ","), &ival, 10)) {
		dstate_setinfo("input.voltage.high.critical", "%d", ival);
	}
	if (reply && str_to_int(strsep(&reply, ","), &ival, 10)) {
		dstate_setinfo("input.voltage.low.critical", "%d", ival);
	}

	if ((transferred = ser_send(upsfd, COMMAND_GET_UPS_POWER_INFO"%c", ENDCHAR)) != 4) {
		upslogx(LOG_ERR, "%s: Failed to get UPS power info", __func__);
	}
	if ((transferred = hashx_recv(buf, sizeof(buf))) < 3) {
		upslogx(LOG_ERR,
			"%s: unexpected UPS power info response: %s",
			__func__, buf);
	}
	/* This is in the format:
	 * #1200,0720,230,40,70,05.20
	 *  1    2    3   4  5  6
	 * 1) Volt-Amp nominal rating
	 * 2) Real power nominal rating
	 * 3) Input voltage nominal rating
	 * 4) Frequency rating lower bound
	 * 5) Frequency rating upper bound
	 * 6) Output Current nominal rating
	 */
	reply = buf + 1;
	if (str_to_int(strsep(&reply, ","), &ival, 10)) {
		dstate_setinfo("ups.power.nominal", "%d", ival);
	}
	if (reply && str_to_int(strsep(&reply, ","), &ival, 10)) {
		dstate_setinfo("ups.realpower.nominal", "%d", ival);
	}
	if (reply && str_to_int(strsep(&reply, ","), &ival, 10)) {
		dstate_setinfo("input.voltage.nominal", "%d", ival);
	}
	if (reply && str_to_double(strsep(&reply, ","), &dval, 10)) {
		dstate_setinfo("input.frequency.low", "%.1f", dval);
	}
	if (reply && str_to_double(strsep(&reply, ","), &dval, 10)) {
		dstate_setinfo("input.frequency.high", "%.1f", dval);
	}
	if (reply && str_to_double(strsep(&reply, ","), &dval, 10)) {
		dstate_setinfo("output.current.nominal", "%.1f", dval);
	}

	if (strsep(&reply, ",") != NULL) {
		upslogx(LOG_WARNING,
			"This UPS returns unexpected power info fields (%s), "
			"previously decoded values may have not been correctly assigned!",
			buf + 1);
	}

	upsh.instcmd = hashx_instcmd;
	for (i = 0; i < sizeof (hashx_cmd) / sizeof (*hashx_cmd); ++i) {
		dstate_addcmd(hashx_cmd[i].cmd_name);
	}
#endif /* HASHX_SERIAL */
}

void upsdrv_updateinfo(void)
{
	char buf[256];
	ssize_t transferred;

#ifdef HASHX_USB
	if (hashx_is_usb) {
		transferred = hashx_usb_status(buf, sizeof(buf));
		if (transferred < 0) {
			upslogx(LOG_ERR, "USB HashX status query failed or returned an incomplete/malformed frame");
			dstate_datastale();
			return;
		}
		hashx_apply_status(buf, transferred);
		return;
	}
#endif

#ifdef HASHX_SERIAL
	if ((transferred = ser_send(upsfd, COMMAND_GET_STATUS"%c", ENDCHAR)) != 2) {
		upslogx(LOG_ERR, "%s: Failed to get status", __func__);
		dstate_datastale();
		return;
	}
	if ((transferred = hashx_recv(buf, sizeof(buf))) <= 0) {
		upslogx(LOG_ERR, "%s: unexpected status response: %s", __func__, buf);
		dstate_datastale();
		return;
	}
	hashx_apply_status(buf, transferred);
#endif
}

static void hashx_apply_status(char *buf, ssize_t transferred)
{
	size_t i;
	char *status;
	int ret;
	float input_voltage = -1;
	float output_voltage = -1;
	float battery_voltage = -1;
	float input_frequency = -1;
	float output_frequency = -1;
	int output_load = -1;
	int battery_charge = -1;
	int remaining_runtime = -1;
	char *status_bytes = NULL;
	size_t status_bytes_size = 0;

	/* This is in the format:
	 * #I239.0O237.0L007B100V27.0F50.1H50.1R040S\x80\0x84\0xd0\x80\x80\0xc0
	 *  I     O     L   B   V    F    H    R   S
	 * I) Input voltage
	 * O) Output voltage
	 * L) Load
	 * B) Battery capacity
	 * V) Battery voltage
	 * F) Utility frequency
	 * H) Output frequency
	 * R) Runtime (in minutes)
	 * S) UPS Status bytes (see below)
	 */
	status = buf + 1;
	if ((ret = sscanf(
		status, "I%fO%fL%dB%dV%fF%fH%fR%dS",
		&input_voltage, &output_voltage, &output_load,
		&battery_charge, &battery_voltage, &input_frequency,
		&output_frequency, &remaining_runtime)) == EOF ||
		ret < 1
	) {
		upslogx(LOG_ERR, "%s: Impossible to parse: %s", __func__, status);
		dstate_datastale();
		return;
	}

	upsdebugx(6, "Poll: input voltage %.1f, output voltage %.1f, load balance %d%%, "
		"battery level: %d%%, battery voltage: %.1f, input frequency: %.1f, "
		"output frequency %.1f, remaining runtime: %d minutes",
		input_voltage, output_voltage, output_load,
		battery_charge, battery_voltage, input_frequency,
		output_frequency, remaining_runtime);

	if (input_voltage >= 0) {
		dstate_setinfo("input.voltage", "%.1f", input_voltage);
	}

	if (output_voltage >= 0) {
		dstate_setinfo("output.voltage", "%.1f", output_voltage);
	}

	if (battery_voltage >= 0) {
		dstate_setinfo("battery.voltage", "%.1f", battery_voltage);
	}

	if (input_frequency >= 0) {
		dstate_setinfo("input.frequency", "%.1f", input_frequency);
	}

	if (output_frequency >= 0) {
		dstate_setinfo("output.frequency", "%.1f", output_frequency);
	}

	if (output_load >= 0) {
		dstate_setinfo("ups.load", "%d", output_load);
	}

	if (battery_charge >= 0) {
		/* In some devices battery.charge.approx seems to be more appropriate */
		dstate_setinfo("battery.charge", "%d", battery_charge);
	}

	if (remaining_runtime >= 0) {
		dstate_setinfo("battery.runtime", "%d", remaining_runtime * 60);
	}

	if ((status_bytes = strchr(buf, 'S'))) {
		status_bytes += 1;
		status_bytes_size = transferred - (status_bytes - buf);
	}

	assert(status_bytes != NULL || status_bytes_size == 0);
	upsdebug_hex(6, "Poll: status bytes", status_bytes, status_bytes_size);

	if (!status_bytes || status_bytes_size < 5) {
		upslogx(LOG_ERR, "Status bytes are not enough: %" PRIuSIZE,
			status_bytes_size);
		dstate_dataok();
		return;
	}

	/* Status bytes values:                 0  1  2  3  4  5
	 *   On AC, fully charged:              80 84 d0 80 80 c0
	 *   On AC, high battery, charging:     80 84 90 80 80 c0
	 *   Testing battery, fully charged:    88 80 88 88 80 c0
	 *   Testing battery, charging:         88 80 8c 88 80 c0
	 *        ... battery level at 0:       88 80 8c 88 a0 c0
	 *   Runtime Estimation, fully charged: 88 80 c8 88 80 c0
	 *        ... lowest level, charging:   a0 84 94 80 a0 c0
	 *   Battery low charging:              80 84 94 80 80 c0
	 *   Battery low charging + beep:       90 84 94 80 80 c0
	 *   Battery very low, charging:        80 84 94 88 a0 c0
	 *        ... after more charge:        80 84 94 80 a0 c0
	 *        ... after more charge:        80 84 94 88 80 c0
	 *        ...testing beep:              90 84 94 88 80 c0
	 *   Discharging from fully:            c0 83 88 88 80 c0
	 *   Discharging:                       c0 83 8c 80 80 c0
	 *   Discharging, more load:            c0 83 8c 88 80 c0
	 *   Discharging Battery very low:      c0 83 8c 80 a0 c0
	 *   Discharging Battery alarm:         e0 83 8c 80 a0 c0
	 *   UPS output off, charging:          80 84 94 80 c0 c0
	 */

	alarm_init();
	status_init();

	/* Remove the common bit from the status bytes. */
	for (i = 0; i < status_bytes_size; ++i) {
		status_bytes[i] &= ~0x80;
	}

	if (status_bytes[0] & 0x08) {
		upsdebugx(6, "Poll, status: Performing calibration test");
		status_set("CAL");
	}

	if (!(status_bytes[1] & 0x04)) {
		upsdebugx(6, "Poll, status: Running on battery");
		dstate_setinfo("battery.charger.status", "discharging");
		status_set("OB");
	} else if ((status_bytes[2] & 0x50) == 0x50) {
		upsdebugx(6, "Poll, status: Running on AC");
		dstate_setinfo("battery.charger.status", "resting");
		status_set("OL");
	} else {
		upsdebugx(6, "Poll, status: Running on AC, battery is charging");
		dstate_setinfo("battery.charger.status", "charging");
		status_set("OL");
	}

	/* XXX: This happens when battery level is marked as 0 here, but there's
	 * still time until the UPS is actually beeping as the critical low level.
	 * Maybe we should ignore this instead and just consider this state as
	 * only OB and set the LB bit only when critically low?
	 */
	if (status_bytes[4] & 0x20) {
		upsdebugx(6, "Poll, status: Battery is low");
		status_set("LB");
	} else {
		upsdebugx(6, "Poll, status: Battery is high");
		status_set("HB");
	}

	if (status_bytes[0] & 0x20) {
		upsdebugx(6, "Poll, status: Battery is critically low");
		if ((status_bytes[0] & 0x08)) {
			alarm_set("TEST MODE: Battery is critically low!");
		} else {
			alarm_set("Battery is critically low, UPS is about to turn off!");
		}
	}

	if ((status_bytes[4] & 0x40)) {
		upsdebugx(6, "Poll, status: UPS outlets are off");
		status_set("OFF");
	}

	/* TODO: Find out the bit in case of OVER, but I wasn't able to get mine
	 * there, as I was either consuming too much (making it go crazy and in
	 * self-protection) or too low :(
	 */

	status_commit();
	dstate_dataok();
	alarm_commit();
}

#ifdef HASHX_SERIAL
static int hashx_instcmd(const char *cmd_name, const char *extra)
{
	size_t i;

	/* May be used in logging below, but not as a command argument */
	NUT_UNUSED_VARIABLE(extra);
	upsdebug_INSTCMD_STARTING(cmd_name, extra);

	for (i = 0; i < sizeof (hashx_cmd) / sizeof (*hashx_cmd); ++i) {
		int status;

		if (strcasecmp(cmd_name, hashx_cmd[i].cmd_name) != 0) {
			continue;
		}

		upslog_INSTCMD_POWERSTATE_CHECKED(cmd_name, extra);
		if ((status = hashx_send_command(hashx_cmd[i].ups_cmd)) == STATUS_SUCCESS)
			return STAT_INSTCMD_HANDLED;

		upslogx(LOG_INSTCMD_FAILED, "Failed to execute command [%s] [%s]: %d",
			NUT_STRARG(cmd_name), NUT_STRARG(extra), status);
		return STAT_INSTCMD_FAILED;
	}

	upslog_INSTCMD_UNKNOWN(cmd_name, extra);
	return STAT_INSTCMD_UNKNOWN;
}
#endif

void upsdrv_shutdown(void)
{
	/* TODO: Find the command used to shutdown... It's definitely supported.
	 */
	upslogx(LOG_ERR, "shutdown not supported");
	if (handling_upsdrv_shutdown > 0) {
		set_exit_flag(EF_EXIT_FAILURE);
	}
}

void upsdrv_help(void)
{
}

/* optionally tweak prognames[] entries */
void upsdrv_tweak_prognames(void)
{
}

void upsdrv_makevartable(void)
{
#ifdef HASHX_USB
	addvar(VAR_FLAG, "usb_monitor", "Explicitly enable experimental USB monitoring with port=auto; no power-control commands");
	nut_usb_addvars();
#endif
}

void upsdrv_initups(void)
{
#ifdef HASHX_USB
	if (testvar("usb_monitor")) {
		if (strcmp(device_path, "auto") != 0) {
			fatalx(EXIT_FAILURE, "USB monitoring requires both port=auto and usb_monitor");
		}
		if (getval("usb_config_index") || getval("usb_hid_rep_index")
		 || getval("usb_hid_desc_index") || getval("usb_hid_ep_in")
		 || getval("usb_hid_ep_out") || testvar("usb_set_altinterface")) {
			fatalx(EXIT_FAILURE, "USB HashX monitoring supports only the observed default interface and endpoints");
		}
		hashx_is_usb = 1;
		upslogx(LOG_WARNING, "Experimental USB monitoring only: startup sessions and power controls are unverified and unavailable");
		hashx_usb_init();
		return;
	}
	if (strcmp(device_path, "auto") == 0) {
		fatalx(EXIT_FAILURE, "USB monitoring requires explicit usb_monitor opt-in");
	}
#endif

#ifdef HASHX_SERIAL
	upsdebugx(3, "Opening device %s", device_path);

	upsfd = ser_open(device_path);

	if (INVALID_FD_SER(upsfd)) {
		fatalx(EXIT_FAILURE, "%s: failed to open %s", __func__, device_path);
	}

	ser_set_speed(upsfd, device_path, B2400);
#else
	fatalx(EXIT_FAILURE, "Serial support is unavailable; USB requires port=auto and usb_monitor");
#endif
}

void upsdrv_cleanup(void)
{
#ifdef HASHX_USB
	if (hashx_is_usb) {
		if (hashx_udev) {
			usb_subdriver.close_dev(hashx_udev);
			hashx_udev = NULL;
		}
		USBFreeExactMatcher(hashx_reopen_matcher);
		USBFreeRegexMatcher(hashx_regex_matcher);
		hashx_usb_free_device();
		return;
	}
#endif
#ifdef HASHX_SERIAL
	ser_close(upsfd, device_path);
#endif
}
