/*
 * blazer_usb.c: support for Megatec/Q1 USB protocol based UPSes
 *
 * A document describing the protocol implemented by this driver can be
 * found online at "http://www.networkupstools.org/protocols/megatec.html".
 *
 * Copyright (C) 2008 - Arjen de Korte <adkorte-guest@alioth.debian.org>
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
#include "libusb.h"
#include "usb-common.h"
#include "blazer.h"

#define DRIVER_NAME	"Megatec/Q1 protocol USB driver"
#define DRIVER_VERSION	"0.03"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arjen de Korte <adkorte-guest@alioth.debian.org>",
	DRV_BETA,
	{ NULL }
};

static usb_communication_subdriver_t *usb = &usb_subdriver;
static usb_dev_handle		*udev = NULL;
static USBDevice_t		usbdevice;
static USBDeviceMatcher_t	*reopen_matcher = NULL;
static USBDeviceMatcher_t	*regex_matcher = NULL;

static int (*subdriver_command)(const char *cmd, char *buf, size_t buflen) = NULL;


static int phoenix_command(const char *cmd, char *buf, size_t buflen)
{
	char	tmp[SMALLBUF];
	int	ret;
	size_t	i;

	memset(tmp, 0, sizeof(tmp));
	snprintf(tmp, sizeof(tmp), "%s", cmd);

	for (i = 0; i < strlen(tmp); i += ret) {

		/* Write data in 8-byte chunks */
		/* ret = usb->set_report(udev, 0, (unsigned char *)&tmp[i], 8); */
		ret = usb_control_msg(udev, USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
			0x09, 0x200, 0, &tmp[i], 8, 1000);

		if (ret <= 0) {
			upsdebugx(3, "send: %s", ret ? usb_strerror() : "timeout");
			return ret;
		}
	}

	upsdebugx(3, "send: %.*s", strcspn(tmp, "\r"), tmp);

	memset(buf, 0, buflen);

	for (i = 0; (i <= buflen-8) && (strchr(buf, '\r') == NULL); i += ret) {

		/* Read data in 8-byte chunks */
		/* ret = usb->get_interrupt(udev, (unsigned char *)&buf[i], 8, 1000); */
		ret = usb_interrupt_read(udev, 0x81, &buf[i], 8, 1000);

		/*
		 * Any errors here mean that we are unable to read a reply (which
		 * will happen after successfully writing a command to the UPS)
		 */
		if (ret <= 0) {
			upsdebugx(3, "read: timeout");
			return 0;
		}
	}

	upsdebugx(3, "read: %.*s", strcspn(buf, "\r"), buf);
	return i;
}


static int krauler_command(const char *cmd, char *buf, size_t buflen)
{
	/*
	 * Still not implemented:
	 * 0x6	T<n>	(don't know how to pass the parameter)
	 * 0x68 and 0x69 both cause shutdown after an undefined interval
	*/
	const struct {
		const char	*str;		/* Megatec command */
		const int	index;	/* Krauler string index for this command */
		const char	prefix;	/* character to replace the first byte in reply */
	}  command[] = {
		{ "Q1\r", 0x03, '(' },
		{ "F\r",  0x0d, '#' },
		{ "I\r",  0x0c, '#' },
		{ "T\r",  0x04, '\r' },
		{ "TL\r", 0x05, '\r' },
		{ "Q\r",  0x07, '\r' },
		{ "C\r",  0x0b, '\r' },
		{ "CT\r", 0x0b, '\r' },
		{ NULL }
	};

	int	i;

	upsdebugx(3, "send: %.*s", strcspn(cmd, "\r"), cmd);
	
	for (i = 0; command[i].str; i++) {
		int	retry;

		if (strcmp(cmd, command[i].str)) {
			continue;
		}

		for (retry = 0; retry < 10; retry++) {
			int	ret;

			ret = usb_get_string_simple(udev, command[i].index, buf, buflen);

			if (ret <= 0) {
				upsdebugx(3, "read: %s", ret ? usb_strerror() : "timeout");
				return ret;
			}

			/* "UPS No Ack" has a special meaning */
			if (!strcasecmp(buf, "UPS No Ack")) {
				continue;
			}

			/* Replace the first byte of what we received with the correct one */
			buf[0] = command[i].prefix;

			return ret;
		}

		upsdebugx(3, "read: %.*s", strcspn(buf, "\r"), buf);
		return 0;
	}

	/* echo the unknown command back */
	upsdebugx(3, "read: %.*s", strcspn(cmd, "\r"), cmd);
	return snprintf(buf, buflen, "%s", cmd);
}


static void *phoenix_subdriver(void)
{
	subdriver_command = &phoenix_command;
	return NULL;
}


static void *krauler_subdriver(void)
{
	subdriver_command = &krauler_command;
	return NULL;
}


static usb_device_id_t blazer_usb_id[] = {
	{ USB_DEVICE(0x05b8, 0x0000), &phoenix_subdriver },	/* Agiler UPS */
	{ USB_DEVICE(0x0001, 0x0000), &krauler_subdriver },	/* Krauler UP-M500VA */
	{ USB_DEVICE(0xffff, 0x0000), &krauler_subdriver },	/* Ablerex 625L USB */
	{ USB_DEVICE(0x0665, 0x5161), &phoenix_subdriver },	/* Belkin F6C1200-UNV */
	{ USB_DEVICE(0x06da, 0x0003), &phoenix_subdriver },	/* Mustek Powermust */
	{ USB_DEVICE(0x0f03, 0x0001), &phoenix_subdriver },	/* Unitek Alpha 1200Sx */
	/* end of list */
	{-1, -1, NULL}
};


static int device_match_func(USBDevice_t *hd, void *privdata)
{
	if (subdriver_command) {
		return 1;
	}

	switch (is_usb_device_supported(blazer_usb_id, hd->VendorID, hd->ProductID))
	{
	case SUPPORTED:
		return 1;

	case POSSIBLY_SUPPORTED:
	case NOT_SUPPORTED:
	default:
		return 0;
	}
}


static USBDeviceMatcher_t device_matcher = {
	&device_match_func,
	NULL,
	NULL
};


/*
 * Generic command processing function. Send a command and read a reply.
 * Returns < 0 on error, 0 on timeout and the number of bytes read on
 * success.
 */
int blazer_command(const char *cmd, char *buf, size_t buflen)
{
#ifndef TESTING
	int	ret;

	if (udev == NULL) {
		ret = usb->open(&udev, &usbdevice, reopen_matcher, NULL);

		if (ret < 1) {
			return ret;
		}
	}

	ret = (*subdriver_command)(cmd, buf, buflen);
	if (ret >= 0) {
		return ret;
	}

	switch (ret)
	{
	case -EBUSY:
		fatal_with_errno(EXIT_FAILURE, "Got disconnected by another driver");

	case -EPERM:
		fatal_with_errno(EXIT_FAILURE, "Permissions problem");

	case -EPIPE:
		if (!usb_clear_halt(udev, 0x81)) {
			/* stall condition cleared */
			break;
		}
	case -ENODEV:
	case -EACCES:
	case -EIO:
	case -ENOENT:
	case -ETIMEDOUT:
	default:
		/* Uh oh, got to reconnect! */
		usb->close(udev);
		udev = NULL;
		break;
	}

	return ret;
#else
	const struct {
		const char	*command;
		const char	*answer;
	} testing[] = {
		{ "Q1\r", "(215.0 195.0 230.0 014 49.0 2.27 30.0 00101000\r" },
		{ "F\r",  "#230.0 000 024.0 50.0\r" },
		{ "I\r",  "#-------------   ------     VT12046Q  \r" },
		{ NULL }
	};

	int	i;

	memset(buf, 0, buflen);

	for (i = 0; testing[i].command; i++) {

		if (strcasecmp(cmd, testing[i].command)) {
			continue;
		}

		return snprintf(buf, buflen, "%s", testing[i].answer);
	}

	return snprintf(buf, buflen, "%s", testing[i].command);
#endif
}


void upsdrv_help(void)
{
	printf("Read The Fine Manual ('man 8 blazer')\n");
}


void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "subdriver", "Serial-over-USB subdriver selection");
	addvar(VAR_VALUE, "vendorid", "Regular expression to match UPS Manufacturer numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "productid", "Regular expression to match UPS Product numerical ID (4 digits hexadecimal)");

	addvar(VAR_VALUE, "vendor", "Regular expression to match UPS Manufacturer string");
	addvar(VAR_VALUE, "product", "Regular expression to match UPS Product string");
	addvar(VAR_VALUE, "serial", "Regular expression to match UPS Serial number");

	addvar(VAR_VALUE, "bus", "Regular expression to match USB bus name");

	blazer_makevartable();
}


void upsdrv_initups(void)
{
#ifndef TESTING
	const struct {
		const char	*name;
		int		(*command)(const char *cmd, char *buf, size_t buflen);
	} subdriver[] = {
		{ "phoenix", &phoenix_command },
		{ "krauler", &krauler_command },
		{ NULL }
	};

	int	ret;
	char	*regex_array[6];

	char	*subdrv = getval("subdriver");

	regex_array[0] = getval("vendorid");
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor");
	regex_array[3] = getval("product");
	regex_array[4] = getval("serial");
	regex_array[5] = getval("bus");

	/* pick up the subdriver name if set explicitly */
	if (subdrv) {
		int	i;

		if (!regex_array[0] || !regex_array[1]) {
			fatalx(EXIT_FAILURE, "When specifying a subdriver, 'vendorid' and 'productid' are mandatory.");
		}

		for (i = 0; subdriver[i].name; i++) {

			if (strcasecmp(subdrv, subdriver[i].name)) {
				continue;
			}

			subdriver_command =  subdriver[i].command;
			break;
		}

		if (!subdriver_command) {
			fatalx(EXIT_FAILURE, "Subdriver \"%s\" not found!", subdrv);
		}
	}

	ret = USBNewRegexMatcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
	switch (ret)
	{
	case -1:
		fatal_with_errno(EXIT_FAILURE, "USBNewRegexMatcher");
	case 0:
		break;	/* all is well */
	default:
		fatalx(EXIT_FAILURE, "invalid regular expression: %s", regex_array[ret]);
	}

	/* link the matchers */
	regex_matcher->next = &device_matcher;

	ret = usb->open(&udev, &usbdevice, regex_matcher, NULL);
	if (ret < 0) {
		fatalx(EXIT_FAILURE, 
			"No supported devices found. Please check your device availability with 'lsusb'\n"
			"and make sure you have an up-to-date version of NUT. If this does not help,\n"
			"try running the driver with at least 'subdriver', 'vendorid' and 'productid'\n"
			"options specified. Please refer to the man page for details about these options\n"
			"(man 8 blazer).\n");
	}

	if (!subdriver_command) {
		fatalx(EXIT_FAILURE, "No subdriver selected");
	}

	/* create a new matcher for later reopening */
	ret = USBNewExactMatcher(&reopen_matcher, &usbdevice);
	if (ret) {
		fatal_with_errno(EXIT_FAILURE, "USBNewExactMatcher");
	}

	/* link the matchers */
	reopen_matcher->next = regex_matcher;
#endif
	blazer_initups();
}


void upsdrv_initinfo(void)
{
	blazer_initinfo();

	dstate_setinfo("ups.vendorid", "%04x", usbdevice.VendorID);
	dstate_setinfo("ups.productid", "%04x", usbdevice.ProductID);
}


void upsdrv_cleanup(void)
{
#ifndef TESTING
	usb->close(udev);
	USBFreeExactMatcher(reopen_matcher);
	USBFreeRegexMatcher(regex_matcher);
	free(usbdevice.Vendor);
	free(usbdevice.Product);
	free(usbdevice.Serial);
	free(usbdevice.Bus);
#endif
}
