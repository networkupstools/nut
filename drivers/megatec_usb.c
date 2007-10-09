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

enum subdriver_flags_t {
	SF_NONE = 0,			/* no flags set */
	SF_FLUSH_NOT_SUPPORTED = 1	/* subdriver doesn't support flushing IO buffers */
};

typedef struct {
	char	*name;
	char	flags;
	int	(*get_data) (char *buffer, int buffer_size);
	int	(*set_data) (const char *str);
} subdriver_t;

/* agiler subdriver definition */
static int get_data_agiler(char *buffer, int buffer_size);
static int set_data_agiler(const char *str);

static subdriver_t agiler_subdriver = {
	"agiler",
	SF_NONE,
	get_data_agiler,
	set_data_agiler
};

/* krauler (ablerex) subdriver definition */
static int get_data_krauler(char *buffer, int buffer_size);
static int set_data_krauler(const char *str);

static subdriver_t krauler_subdriver = {
	"krauler",
	SF_FLUSH_NOT_SUPPORTED,
	get_data_krauler,
	set_data_krauler
};

/* list of subdrivers */
static subdriver_t *subdriver_list[] = {
	&agiler_subdriver,
	&krauler_subdriver,
	NULL	/* end of list */
};

/* selected subdriver */
subdriver_t *subdriver = NULL;

typedef struct {
	int vid;
	int pid;
	subdriver_t *subdriver;
} usb_ups_t;

/* list of all known devices */
static usb_ups_t KnownDevices[] = {
	{0x05b8, 0x0000, &agiler_subdriver},	/* Agiler UPS */
	{0x0001, 0x0000, &krauler_subdriver},	/* Krauler UP-M500VA */
	{0xffff, 0x0000, &krauler_subdriver},	/* Ablerex 625L USB */
	{0x0665, 0x5161, &agiler_subdriver},	/* Belkin F6C1200-UNV */
	{-1, -1, NULL}		/* end of list */
};

static int comm_usb_match(USBDevice_t *d, void *privdata)
{
	usb_ups_t *p;

	if(getval("subdriver"))
		return 1;

	for (p = KnownDevices; p->vid != -1; p++) {
		if ((p->vid == d->VendorID) && (p->pid == d->ProductID)) {
			subdriver = p->subdriver;
			return 1;
		}
	}

	return 0;
}

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

void megatec_subdrv_banner()
{
	printf("Serial-over-USB transport layer for Megatec protocol driver [%s]\n", progname);
	printf("Andrey Lelikov (c) 2006, Alexander Gordeev (c) 2006-2007, Jon Gough (c) 2007\n\n");
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
	USBDeviceMatcher_t subdriver_matcher;
	int ret;

	USBDeviceMatcher_t *regex_matcher = NULL;
	int r;
	char *regex_array[6];

	char *subdrv = getval("subdriver");
	char *vid = getval("vendorid");
	char *pid = getval("productid");
	char *vend = getval("vendor");
	char *prod = getval("product");

	if(subdrv)
	{
		subdriver_t **p;

		if(!vid && !pid && !vend && !prod)
		{
			upslogx(LOG_WARNING, "It's unsafe to select a subdriver but not specify device!\n"
				"Please set some of \"vendor\", \"product\", \"vendorid\", \"productid\""
				" variables.\n");
		}

		for (p = subdriver_list; *p; p++)
		{
			if (!strcasecmp(subdrv, (*p)->name))
			{
				subdriver = *p;
				break;
			}
		}

		if(!subdriver)
			fatalx(EXIT_FAILURE, "No subdrivers named \"%s\" found!", subdrv);
	}

	memset(&subdriver_matcher, 0, sizeof(subdriver_matcher));
	subdriver_matcher.match_function = &comm_usb_match;

	/* FIXME: fix "serial" variable */
        /* process the UPS selection options */
	regex_array[0] = vid;
	regex_array[1] = pid;
	regex_array[2] = vend;
	regex_array[3] = prod;
	regex_array[4] = NULL; /* getval("serial"); */
	regex_array[5] = getval("bus");

	r = USBNewRegexMatcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
	if (r==-1) {
		fatal_with_errno(EXIT_FAILURE, "USBNewRegexMatcher");
	} else if (r) {
		fatalx(EXIT_FAILURE, "invalid regular expression: %s", regex_array[r]);
	}
	/* link the matchers */
	regex_matcher->next = &subdriver_matcher;

	ret = usb->open(&udev, &usbdevice, regex_matcher, NULL);
	if (ret < 0)
		usb_open_error(port);

	/* TODO: Add make exact matcher for reconnecting feature support */
	USBFreeRegexMatcher(regex_matcher);

	/* This is here until ser_flush_io() is used in megatec.c */
	ser_flush_io(0);

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

	if(!(subdriver->flags & SF_FLUSH_NOT_SUPPORTED))
		/* flush input buffers */
		for (i = 0; i < 10; i++) {
			if (subdriver->get_data(flush_buf, sizeof(flush_buf)) < 1)
				break;
		}

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

	return subdriver->set_data(buf);
}

int ser_get_line(int fd, char *buf, size_t buflen, char endchar, const char *ignset, long d_sec, long d_usec)
{
	int len;
	char *src, *dst, c;

	if (NULL == udev)
		return -1;

	len = subdriver->get_data(buf, buflen);
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

static int set_data_agiler(const char *str)
{
	return usb->set_report(udev, 0, (unsigned char *)str, strlen(str));
}

static int get_data_agiler(char *buffer, int buffer_size)
{
	return usb->get_interrupt(udev, (unsigned char *)buffer, buffer_size, 1000);
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
	char	prefix;	/* character to prepend after stripping first four bytes in reply */
	int	retry;	/* 0 for immediate action, >0 if used in get_data_krauler */
} krauler_command_t;

static krauler_command_t krauler_command_lst[] = {
	{ "T\r",  0x04, '\0', 0 },
	{ "TL\r", 0x05, '\0', 0 },
	{ "Q\r",  0x07, '\0', 0 },
	{ "C\r",  0x0b, '\0', 0 },
	{ "CT\r", 0x0b, '\0', 0 },
	{ "Q1\r", 0x03, '(', KRAULER_MAX_ATTEMPTS_Q1 },
	{ "I\r",  0x0c, '#', KRAULER_MAX_ATTEMPTS_I },
	{ "F\r",  0x0d, '#', KRAULER_MAX_ATTEMPTS_F },
	{ NULL, 0, '\0', 0 }
};

static krauler_command_t *command = NULL;

static int set_data_krauler(const char *str)
{
	/*
	Still not implemented:
		0x6	T<n>	(don't know how to pass the parameter)
		0x68 and 0x69 both cause shutdown after an undefined interval
	*/
	for (command = krauler_command_lst; command->str != NULL; command++) {

		if (strcmp(str, command->str)) {
			continue;
		}

		/* Immediate action */
		if (command->retry == 0) {
			int	res;
			char	buf[SMALLBUF];

			res = usb->get_string(udev, command->index, buf, sizeof(buf));
			if (res > 0) {
				upsdebugx(5, "set_data_krauler: dump [%s]", buf);
			}
		}

		return strlen(str);
	}

	upsdebugx(4, "set_data_krauler: unknown command [%s]", str);
	return 0;
}

static int get_data_krauler(char *buffer, int buffer_size)
{
	int	retry;

	if (!command || !command->str) {
		upsdebugx(3, "get_data_krauler: no command set");
		return 0;
	}

	upsdebugx(3, "get_data_krauler: index [%02x], prefix [%c]", command->index, command->prefix);

	for (retry = 0; retry < command->retry; retry++) {

		int	res;

		res = usb->get_string(udev, command->index, buffer, buffer_size);
		if (res < 1) {
			upsdebugx(2, "get_data_krauler: connection failure");
			return res;
		}

		if (!strcmp(buffer, "UPS No Ack")) {
			upsdebugx(4, "get_data_krauler: retry [%s]", buffer);
			continue;
		}

		/* Replace the first byte of what we received with the correct one */
		buffer[0] = command->prefix;

		return res;
	}

	upsdebugx(4, "get_data_krauler: too many attempts");
	return 0;
}
