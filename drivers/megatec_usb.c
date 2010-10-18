/* megatec_usb.c - usb communication layer for Megatec protocol based UPSes
 * 
 * Copyright (C) 2006 Andrey Lelikov <nut-driver@lelik.org>
 * Copyright (C) 2007 Alexander Gordeev <lasaine@lvk.cs.msu.su>
 * Copyright (C) 2007 Jon Gough <jon.gough at eclipsesystems.com.au>
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
 */

#include "main.h"
#include "megatec.h"
#include "libusb.h"
#ifndef WIN32 /*FIXME*/
#include "serial.h"
#else
/*FIXME : is this define correct ?*/
#define speed_t int
#endif
#include "usb-common.h"

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#define SUB_DRIVER_VERSION	"0.10"
#define SUB_DRIVER_NAME	"Serial-over-USB transport layer"

/* driver description structure */
upsdrv_info_t megatec_subdrv_info = {
	SUB_DRIVER_NAME,
	SUB_DRIVER_VERSION,
	"Andrey Lelikov <nut-driver@lelik.org>\n" \
	"Alexander Gordeev <lasaine@lvk.cs.msu.su>\n" \
	"Jon Gough <jon.gough@eclipsesystems.com.au>",
	DRV_STABLE,
	{ NULL }
};

/*
    This is a communication driver for "USB HID" UPS-es which use proprietary
usb-to-serial converter and speak megatec protocol. Usually these are cheap
models and usb-to-serial converter is a huge oem hack - HID tables are bogus,
device has no UPS reports, etc.
    This driver has a table of all known devices which has pointers to device-
specific communication functions (namely send a string to UPS and read a string
from it). Driver takes care of detection, opening a usb device, string
formatting etc. So in order to add support for another usb-to-serial device one
only needs to implement device-specific get/set functions and add an entry into
KnownDevices table.

*/

static usb_communication_subdriver_t *usb = &usb_subdriver;
static usb_dev_handle *udev = NULL;
static USBDevice_t usbdevice;
static USBDeviceMatcher_t *reopen_matcher = NULL;
static USBDeviceMatcher_t *regex_matcher = NULL;

static int (*get_data)(char *buffer, int buffer_size) = NULL;
static int (*set_data)(const char *str) = NULL;

/* agiler-old subdriver definition */
static int get_data_agiler_old(char *buffer, int buffer_size);
static int set_data_agiler_old(const char *str);

static void *agiler_old_subdriver(void)
{
	get_data = &get_data_agiler_old;
	set_data = &set_data_agiler_old;
	return NULL;
}

/* agiler subdriver definition */
static int get_data_agiler(char *buffer, int buffer_size);
static int set_data_agiler(const char *str);

static void *agiler_subdriver(void)
{
	get_data = &get_data_agiler;
	set_data = &set_data_agiler;
	return NULL;
}

/* Phoenixtec Power Co subdriver definition */
static int get_data_phoenix(char *buffer, int buffer_size);
static int set_data_phoenix(const char *str);

static void *phoenix_subdriver(void)
{
	get_data = &get_data_phoenix;
	set_data = &set_data_phoenix;
	return NULL;
}

/* krauler (ablerex) subdriver definition */
static int get_data_krauler(char *buffer, int buffer_size);
static int set_data_krauler(const char *str);

static void *krauler_subdriver(void)
{
	get_data = &get_data_krauler;
	set_data = &set_data_krauler;
	return NULL;
}

/* list of subdrivers for manual overrides */
static const struct {
	const char	*name;
	void		*(*handler)(void);
} subdriver[] = {
	{ "agiler-old",	&agiler_old_subdriver },
	{ "agiler",	&agiler_subdriver },
	{ "phoenix",	&phoenix_subdriver },
	{ "krauler",	&krauler_subdriver },
	/* end of list */
	{ NULL }
};

/* list of all known devices */
static usb_device_id_t megatec_usb_id[] = {
	/* Agiler UPS */
	{ USB_DEVICE(0x05b8, 0x0000), &agiler_subdriver},
	/* Krauler UP-M500VA */
	{ USB_DEVICE(0x0001, 0x0000), &krauler_subdriver},
	/* Ablerex 625L USB */
	{ USB_DEVICE(0xffff, 0x0000), &krauler_subdriver},
	/* Belkin F6C1200-UNV */
	{ USB_DEVICE(0x0665, 0x5161), &phoenix_subdriver},
	/* Mustek Powermust */
	{ USB_DEVICE(0x06da, 0x0003), &phoenix_subdriver},
	/* Unitek Alpha 1200Sx */
	{ USB_DEVICE(0x0f03, 0x0001), &phoenix_subdriver},
	/* end of list */
	{-1, -1, NULL}
};

static int subdriver_match_func(USBDevice_t *d, void *privdata)
{
	if(getval("subdriver"))
		return 1;

	switch (is_usb_device_supported(megatec_usb_id, d->VendorID, d->ProductID))
	{
	case SUPPORTED:
		return 1;

	case POSSIBLY_SUPPORTED:
	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

static USBDeviceMatcher_t subdriver_matcher = {
	&subdriver_match_func,
	NULL,
	NULL
};

static void usb_open_error(const char *port)
{
	fatalx(EXIT_FAILURE, 
"No supported devices found. Please check your device availability with 'lsusb'\n"
"and make sure you have an up-to-date version of NUT. If this does not help,\n"
"try running the driver with at least 'vendorid' and 'subdriver' options\n"
"specified. Please refer to the man page for details about these options\n"
"(man 8 megatec_usb).\n"
"Please report your results to the NUT user's mailing list\n"
"<nut-upsuser@lists.alioth.debian.org>.\n"
		);
}

/* FIXME: Fix "serial" variable (which conflicts with "serial" variable in megatec.c) */
void megatec_subdrv_makevartable()
{
	addvar(VAR_VALUE, "vendor", "Regular expression to match UPS Manufacturer string");
	addvar(VAR_VALUE, "product", "Regular expression to match UPS Product string");
	/* addvar(VAR_VALUE, "serial", "Regular expression to match UPS Serial number"); */
	addvar(VAR_VALUE, "vendorid", "Regular expression to match UPS Manufacturer numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "productid", "Regular expression to match UPS Product numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "bus", "Regular expression to match USB bus name");
	addvar(VAR_VALUE, "subdriver", "Serial-over-USB subdriver selection");
}

int ser_open(const char *port)
{
	char *regex_array[6];
	int ret;

	char *subdrv = getval("subdriver");
	char *vid = getval("vendorid");
	char *pid = getval("productid");
	char *vend = getval("vendor");
	char *prod = getval("product");

	/* pick up the subdriver name if set explicitly */
	if(subdrv)
	{
		int	i;

		if(!vid && !pid && !vend && !prod)
		{
			upslogx(LOG_WARNING, "It's unsafe to select a subdriver but not specify device!\n"
				"Please set some of \"vendor\", \"product\", \"vendorid\", \"productid\""
				" variables.\n");
		}

		for (i = 0; subdriver[i].name; i++)
		{
			if (!strcasecmp(subdrv, subdriver[i].name))
			{
				(*subdriver[i].handler)();
				break;
			}
		}

		if(!subdriver[i].name)
			fatalx(EXIT_FAILURE, "No subdrivers named \"%s\" found!", subdrv);
	}

	/* FIXME: fix "serial" variable */
        /* process the UPS selection options */
	regex_array[0] = vid;
	regex_array[1] = pid;
	regex_array[2] = vend;
	regex_array[3] = prod;
	regex_array[4] = NULL; /* getval("serial"); */
	regex_array[5] = getval("bus");

	ret = USBNewRegexMatcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
	if (ret == -1) {
		fatal_with_errno(EXIT_FAILURE, "USBNewRegexMatcher");
	} else if (ret) {
		fatalx(EXIT_FAILURE, "invalid regular expression: %s", regex_array[ret]);
	}
	/* link the matchers */
	regex_matcher->next = &subdriver_matcher;

	ret = usb->open(&udev, &usbdevice, regex_matcher, NULL);
	if (ret < 0)
		usb_open_error(port);

	/* create a new matcher for later reopening */
	ret = USBNewExactMatcher(&reopen_matcher, &usbdevice);
	if (ret) {
		fatal_with_errno(EXIT_FAILURE, "USBNewExactMatcher");
	}
	/* link the matchers */
	reopen_matcher->next = regex_matcher;

	/* NOTE: This is here until ser_flush_io() is used in megatec.c */
#ifndef WIN32 /*FIXME*/
	ser_flush_io(0);
#endif

	return 0;
}

int ser_set_speed(int fd, const char *port, speed_t speed)
{
	return 0;
}

int ser_set_dtr(int fd, int state)
{
	return 0;
}

int ser_set_rts(int fd, int state)
{
	return 0;
}

int ser_flush_io(int fd)
{
	char flush_buf[256];
	int i;

	/* flush input buffers */
	for (i = 0; i < 10; i++) {
		if ((*get_data)(flush_buf, sizeof(flush_buf)) < 1)
			break;
	}

	return 0;
}

void ser_comm_fail(const char *fmt, ...)
{
}

void ser_comm_good(void)
{
}

int ser_close(int fd, const char *port)
{
	usb->close(udev);
	USBFreeExactMatcher(reopen_matcher);
	USBFreeRegexMatcher(regex_matcher);
	return 0;
}

/*!@brief Try to reconnect once.
 * @return 1 if reconnection was successful.
 */
static int reconnect_ups(void)
{
	int ret;

	upsdebugx(2, "==================================================");
	upsdebugx(2, "= device has been disconnected, try to reconnect =");
	upsdebugx(2, "==================================================");

	usb->close(udev);

	ret = usb->open(&udev, &usbdevice, reopen_matcher, NULL);
	if (ret < 1) {
		upslogx(LOG_INFO, "Reconnecting to UPS failed; will retry later...");
		udev = NULL;
		return 0;
	} else
		upslogx(LOG_NOTICE, "Successfully reconnected");

	return ret;
}

/*!@brief Report a USB comm failure, and reconnect if necessary
 * 
 * @param[in] res	Result code from libusb/libhid call
 * @param[in] msg	Error message to display
 */
void usb_comm_fail(int res, const char *msg)
{
	switch(res) {
		case -EBUSY:
			upslogx(LOG_WARNING, "%s: Device claimed by another process", msg);
			fatalx(EXIT_FAILURE, "Terminating: EBUSY");

		default:
			upslogx(LOG_WARNING, "%s: Device detached? (error %d: %s)", msg, res, usb_strerror());

			if(reconnect_ups()) {
				/* upsdrv_initinfo(); */
			}
			break;
	}
}

int ser_send_pace(int fd, unsigned long d_usec, const char *fmt, ...)
{
	char buf[128];
	size_t len;
	va_list ap;
	int ret;

	if ((udev == NULL) && (! reconnect_ups()))
		return -1;

	va_start(ap, fmt);

	len = vsnprintf(buf, sizeof(buf), fmt, ap);

	va_end(ap);

	if ((len < 1) || (len >= (int) sizeof(buf))) {
		upslogx(LOG_WARNING, "ser_send_pace: vsnprintf needed more than %d bytes", (int) sizeof(buf));
		buf[sizeof(buf) - 1] = 0;
	}

	ret = (*set_data)(buf);
	if(ret < 0) {
		usb_comm_fail(ret, "ser_send_pace");
	}

	return ret;
}

int ser_get_line(int fd, void *buf, size_t buflen, char endchar, const char *ignset, long d_sec, long d_usec)
{
	int len;
	char *src, *dst, c;

	if ((udev == NULL) && (! reconnect_ups()))
		return -1;

	len = (*get_data)((char *)buf, buflen);
	if (len < 0) {
		usb_comm_fail(len, "ser_get_line");
		return len;
	}

	dst = (char *)buf;

	for (src = (char *)buf; src != ((char *)buf + len); src++) {
		c = *src;

		if (c == endchar)
			break;

		if ((c == 0) || ((ignset != NULL) && (strchr(ignset, c) != NULL)))
			continue;

		*(dst++) = c;
	}

	/* terminate string if we have space */
	if (dst != ((char *)buf + len))
		*dst = 0;

	return (dst - (char *)buf);
}

/************** minidrivers go after this point **************************/

/*
    agiler-old subdriver
*/
/*  Protocol was reverse-engineered from Windows driver
    HID tables are completely bogus
    Data is transferred out as one 8-byte packet with report ID 0
    Data comes in as 6 8-byte reports per line , padded with zeroes
    All constants are hardcoded in windows driver
*/

#define AGILER_REPORT_SIZE      8
#define AGILER_REPORT_COUNT     6
#define AGILER_TIMEOUT          5000

static int set_data_agiler_old(const char *str)
{
        unsigned char report_buf[AGILER_REPORT_SIZE];

        if (strlen(str) > AGILER_REPORT_SIZE) {
                upslogx(LOG_ERR, "set_data_agiler: output string too large");
                return -1;
        }

        memset(report_buf, 0, sizeof(report_buf));
        memcpy(report_buf, str, strlen(str));

        return usb->set_report(udev, 0, report_buf, sizeof(report_buf));
}

static int get_data_agiler_old(char *buffer, int buffer_size)
{
        int i, len;
        char buf[AGILER_REPORT_SIZE * AGILER_REPORT_COUNT + 1];

        memset(buf, 0, sizeof(buf));

        for (i = 0; i < AGILER_REPORT_COUNT; i++) {
                len = usb->get_interrupt(udev, (unsigned char *) buf + i * AGILER_REPORT_SIZE, AGILER_REPORT_SIZE, AGILER_TIMEOUT);
                if (len != AGILER_REPORT_SIZE) {
                        if (len < 0)
                                len = 0;
                        buf[i * AGILER_REPORT_SIZE + len] = 0;
                        upsdebug_hex(5, "get_data_agiler: raw dump", buf, i * AGILER_REPORT_SIZE + len);
                        break;
                }
        }

        len = strlen(buf);

        if (len > buffer_size) {
                upslogx(LOG_ERR, "get_data_agiler: input buffer too small");
                len = buffer_size;
        }

        memcpy(buffer, buf, len);
        return len;
}


/*
    Agiler serial-to-usb device.
*/

static int set_data_agiler(const char *str)
{
	return usb->set_report(udev, 0, (unsigned char *)str, strlen(str));
}

static int get_data_agiler(char *buffer, int buffer_size)
{
	return usb->get_interrupt(udev, (unsigned char *)buffer, buffer_size, 1000);
}


/*
    Phoenixtec Power Co serial-to-usb device.
*/
static char	phoenix_buffer[32];

static int set_data_phoenix(const char *str)
{
	unsigned int	count;

	memset(phoenix_buffer, '\0', sizeof(phoenix_buffer));

	snprintf(phoenix_buffer, sizeof(phoenix_buffer), "%s", str);

	if (!strcmp(phoenix_buffer, "I\r") || !strcmp(phoenix_buffer, "C\r")) {
		/* Ignore these, since they seem to lock up the connection */
		return strlen(phoenix_buffer);
	}

	for (count = 0; count < strlen(phoenix_buffer); count += 8) {

		if (usb->set_report(udev, 0, (unsigned char *)(phoenix_buffer + count), 8) < 1) {
			return -1;
		}
	}

	return strlen(phoenix_buffer);
}

static int get_data_phoenix(char *buffer, int buffer_size)
{
	int	count;

	memset(buffer, '\0', buffer_size);

	if (!strcmp(phoenix_buffer, "I\r") || !strcmp(phoenix_buffer, "C\r")) {
		/* Echo back unsupported commands */
		snprintf(buffer, buffer_size, "%s", phoenix_buffer);
		return strlen(buffer);
	}

	for (count = 8; count <= buffer_size; count += 8) {

		/* Read data in 8-byte chunks, break on a timeout */
		if (usb->get_interrupt(udev, (unsigned char *)&buffer[count-8], 8, 1000) < 0) {
			return count-8;
		}

		upsdebugx(3, "get_data_phoenix: got so far [%s]", buffer);
		upsdebug_hex(4, "get_data_phoenix", (unsigned char *)buffer, count);
	}

	upsdebugx(3, "get_data_phoenix: buffer too small");
	return -1;
}


/*
    Krauler serial-over-usb device.

    Protocol was reverse-engineered using Windows driver.
*/

#define KRAULER_MAX_ATTEMPTS_Q1		4
#define KRAULER_MAX_ATTEMPTS_F		31
#define KRAULER_MAX_ATTEMPTS_I		15

typedef struct {
	char	*str;	/* Megatec command */
	int	index;	/* Krauler string index for this command */
	char	prefix;	/* character to replace the first byte in reply */
	int	retry;	/* number of retries (1 is typically for instant commands) */
} krauler_command_t;

static krauler_command_t krauler_command_lst[] = {
	{ "T\r",  0x04, '\0', 1 },
	{ "TL\r", 0x05, '\0', 1 },
	{ "Q\r",  0x07, '\0', 1 },
	{ "C\r",  0x0b, '\0', 1 },
	{ "CT\r", 0x0b, '\0', 1 },
	{ "Q1\r", 0x03, '(',  KRAULER_MAX_ATTEMPTS_Q1 },
	{ "I\r",  0x0c, '#',  KRAULER_MAX_ATTEMPTS_I },
	{ "F\r",  0x0d, '#',  KRAULER_MAX_ATTEMPTS_F },
	{ NULL, 0, '\0', 0 }
};

/*
Still not implemented:
	0x6	T<n>	(don't know how to pass the parameter)
	0x68 and 0x69 both cause shutdown after an undefined interval
*/


/* an intermediate buffer for 1 command's output */
static char krauler_line_buf[255];
static char krauler_line_buf_len = 0;

static int set_data_krauler(const char *str)
{
	krauler_command_t *command;
	int retval = strlen(str);

	for (command = krauler_command_lst; command->str != NULL; command++) {
		int	retry;

		if (strcmp(str, command->str)) {
			continue;
		}

		upsdebugx(3, "set_data_krauler: index [%02x]", command->index);

		krauler_line_buf_len = 0;
		for (retry = 0; retry < command->retry; retry++) {
			int	res;

			res = usb->get_string(udev, command->index, krauler_line_buf, sizeof(krauler_line_buf));
			if (res < 1) {
				/* TODO: handle_error(res) */
				upsdebugx(2, "set_data_krauler: connection failure");
				return res;
			}

			/* "UPS No Ack" has a special meaning */
			if (!strcmp(krauler_line_buf, "UPS No Ack")) {
				upsdebugx(4, "set_data_krauler: retry [%s]", krauler_line_buf);
				continue;
			}

			/* Replace the first byte of what we received with the correct one */
			if(command->prefix)
				krauler_line_buf[0] = command->prefix;

			krauler_line_buf_len = res;
			return retval;
		}

		if(command->retry > 1 && retry == command->retry)
			upsdebugx(2, "set_data_krauler: too many attempts, the UPS is probably switched off!");

		return retval;
	}

	upsdebugx(4, "set_data_krauler: unknown command [%s]", str);

	/* echo the unknown command back */
	strcpy(krauler_line_buf, str);
	krauler_line_buf_len = retval;

	return retval;
}

static int get_data_krauler(char *buffer, int buffer_size)
{
	int retrieved = (buffer_size < krauler_line_buf_len) ? buffer_size : krauler_line_buf_len;
	int left = krauler_line_buf_len - retrieved;

	memcpy(buffer, krauler_line_buf, retrieved);
	memmove(krauler_line_buf, krauler_line_buf + retrieved, left);
	krauler_line_buf_len = left;

	return retrieved;
}
