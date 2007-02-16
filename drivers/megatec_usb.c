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
#include "serial.h"

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <usb.h>

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

static communication_subdriver_t *usb = &usb_subdriver;
static usb_dev_handle *udev = NULL;
static HIDDevice_t hiddevice;

typedef struct {
	int vid;
	int pid;
	int (*get_data) (char *buffer, int buffer_size);
	int (*set_data) (const char *str);
} usb_ups_t;

usb_ups_t *usb_ups_device = NULL;

/*
    All devices known to this driver go here
    along with their set/get routines
*/

static int get_data_agiler(char *buffer, int buffer_size);
static int set_data_agiler(const char *str);

static int get_data_krauler(char *buffer, int buffer_size);
static int set_data_krauler(const char *str);

static int get_data_ablerex(char *buffer, int buffer_size);
static int set_data_ablerex(const char *str);

static usb_ups_t KnownDevices[] = {
	{0x05b8, 0x0000, get_data_agiler, set_data_agiler},   /* Agiler UPS */
	{0x0001, 0x0000, get_data_krauler, set_data_krauler}, /* Krauler UP-M500VA */
	{0xffff, 0x0000, get_data_krauler, set_data_krauler}, /* Ablerex 625L USB */
	{-1, -1, NULL, NULL}		/* end of list */
};

/* TODO: Fix matching non-auto selected devices */
static int comm_usb_match(HIDDevice_t *d, void *privdata)
{
	usb_ups_t *p;

	for (p = KnownDevices; p->vid != -1; p++) {
		if ((p->vid == d->VendorID) && (p->pid == d->ProductID)) {
			usb_ups_device = p;
			return 1;
		}
	}

	p = (usb_ups_t *) privdata;

	if (NULL != p) {
		if ((p->vid == d->VendorID) && (p->pid == d->ProductID)) {
			usb_ups_device = p;
			return 1;
		}
	}

	return 0;
}

static void usb_open_error(const char *port)
{
	exit(EXIT_FAILURE);
}

int ser_open(const char *port)
{
	HIDDeviceMatcher_t match;
	static usb_ups_t param_arg;
	const char *p;
	int ret, i;
	union _u {
		unsigned char report_desc[4096];
		char flush_buf[256];
	} u;

	memset(&match, 0, sizeof(match));
	match.match_function = &comm_usb_match;

	if (0 != strcasecmp(port, "auto")) {
		param_arg.vid = (uint16_t) strtoul(port, NULL, 16);
		p = strchr(port, ':');
		if (NULL != p) {
			param_arg.pid = (uint16_t) strtoul(p + 1, NULL, 16);
		} else {
			param_arg.vid = 0;
		}

		/* pure heuristics - assume this unknown device speaks agiler protocol */
		param_arg.get_data = get_data_agiler;
		param_arg.set_data = set_data_agiler;

		if (0 != param_arg.vid) {
			match.privdata = &param_arg;
		} else {
			upslogx(LOG_ERR, "ser_open: invalid usb device specified, must be \"auto\" or \"vid:pid\"");
			return -1;
		}
	}

	ret = usb->open(&udev, &hiddevice, &match, u.report_desc, MODE_NOHID);
	if (ret < 0)
		usb_open_error(port);

	/* flush input buffers */
	for (i = 0; i < 10; i++) {
		if (ser_get_line(upsfd, u.flush_buf, sizeof(u.flush_buf), 0, NULL, 0, 0) < 1)
			break;
	}

	return 0;
}

int ser_set_speed(int fd, const char *port, speed_t speed)
{
	return 0;
}

int ser_close(int fd, const char *port)
{
	usb->close(udev);
	return 0;
}

unsigned int ser_send_pace(int fd, unsigned long d_usec, const char *fmt, ...)
{
	char buf[128];
	size_t len;
	va_list ap;

	if (NULL == udev)
		return -1;

	va_start(ap, fmt);

	len = vsnprintf(buf, sizeof(buf), fmt, ap);

	va_end(ap);

	if ((len < 1) || (len >= (int) sizeof(buf))) {
		upslogx(LOG_WARNING, "ser_send_pace: vsnprintf needed more than %d bytes", (int) sizeof(buf));
		buf[sizeof(buf) - 1] = 0;
	}

	return usb_ups_device->set_data(buf);
}

int ser_get_line(int fd, char *buf, size_t buflen, char endchar, const char *ignset, long d_sec, long d_usec)
{
	int len;
	char *src, *dst, c;

	if (NULL == udev)
		return -1;

	len = usb_ups_device->get_data(buf, buflen);
	if (len < 0)
		return len;

	dst = buf;

	for (src = buf; src != (buf + len); src++) {
		c = *src;

		if (c == endchar)
			break;

		if ((c == 0) || ((ignset != NULL) && (strchr(ignset, c) != NULL)))
			continue;

		*(dst++) = c;
	}

	/* terminate string if we have space */
	if (dst != (buf + len))
		*dst = 0;

	return (dst - buf);
}

/************** minidrivers go after this point **************************/


/*
    Agiler serial-to-usb device.

    Protocol was reverse-engineered from Windows driver
    HID tables are completely bogus
    Data is transferred out as one 8-byte packet with report ID 0
    Data comes in as 6 8-byte reports per line , padded with zeroes
    All constants are hardcoded in windows driver
*/

#define AGILER_REPORT_SIZE      8
#define AGILER_REPORT_COUNT     6
#define AGILER_TIMEOUT          5000

static int set_data_agiler(const char *str)
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

static int get_data_agiler(char *buffer, int buffer_size)
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
    Krauler serial-to-usb device.

    Protocol was reverse-engineered using Windows driver.
*/

#define KRAULER_COMMAND_BUFFER_SIZE	9
#define KRAULER_TIMEOUT		5000
#define KRAULER_WRONG_ANSWER		"PS No Ack"
#define KRAULER_MAX_ATTEMPTS_Q1		4
#define KRAULER_MAX_ATTEMPTS_F		31
#define KRAULER_MAX_ATTEMPTS_I		15

static char krauler_command_buffer[KRAULER_COMMAND_BUFFER_SIZE];

static int set_data_krauler(const char *str)
{
	unsigned char index = 0;
	int len;

	/*
	Still not implemented:
		0x6	T<n>	(don't know how to pass the parameter)
		0x68 and 0x69 both cause shutdown after an undefined interval
		C	(know nothing about this command)
	*/

	if (strcmp(str, "T\r") == 0)
		index = 0x04;
	else if (strcmp(str, "TL\r") == 0)
		index = 0x05;
	else if (strcmp(str, "Q\r") == 0)
		index = 0x07;
	else if (strcmp(str, "CT\r") == 0)
		index = 0x0b;

	if (index > 0)
	{
		/* usb_get_descriptor(udev, USB_DT_STRING, index, buffer, buffer_size); */
		usb_control_msg(udev, USB_ENDPOINT_IN + 1, USB_REQ_GET_DESCRIPTOR, (USB_DT_STRING << 8) + index, 0, NULL, 0, KRAULER_TIMEOUT);
		return 0;
	}

	len = strlen(str);
	if (len >= KRAULER_COMMAND_BUFFER_SIZE) {
		upslogx(LOG_ERR, "set_data_krauler: output string too large");
		return -1;
	}

	krauler_command_buffer[len] = 0;
	memcpy(krauler_command_buffer, str, len);

	return len;
}

static int get_data_krauler(char *buffer, int buffer_size)
{
	int res = 0;
	unsigned char index = 0;
	int i, j;
	int attempts = 1;

	if (strcmp(krauler_command_buffer, "Q1\r") == 0)
	{
		index = 0x03;
		attempts = KRAULER_MAX_ATTEMPTS_Q1;
	}
	else if (strcmp(krauler_command_buffer, "I\r") == 0)
	{
		index = 0x0c;
		attempts = KRAULER_MAX_ATTEMPTS_I;
	}
	else if (strcmp(krauler_command_buffer, "F\r") == 0)
	{
		index = 0x0d;
		attempts = KRAULER_MAX_ATTEMPTS_F;
	}

	if (index > 0)
		while (attempts) {
			/* res = usb_get_descriptor(udev, USB_DT_STRING, index, buffer, buffer_size); */
			res = usb_control_msg(udev, USB_ENDPOINT_IN + 1, USB_REQ_GET_DESCRIPTOR, (USB_DT_STRING << 8) + index, 0, buffer, buffer_size, KRAULER_TIMEOUT);

			if (res > 0) {
				for (i = 4, j = 0; i < res; i++)
					if (buffer[i] != 0) {
						buffer[j] = buffer[i];
						j++;
					}
				buffer[j] = 0;
				res = j;

				upsdebugx(5, "get_data_krauler: got data");
				/* upsdebugx(5, "get_data_krauler: %s", buffer); */
				if (strcmp(buffer, KRAULER_WRONG_ANSWER) != 0)
					break;
				else
					upsdebugx(5, "get_data_krauler: ups no ack");
			} else
				break;

			attempts--;
		}


	krauler_command_buffer[0] = 0;
	return res;
}
