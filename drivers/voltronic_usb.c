/*
 * voltronic_usb.c: support for Voltronic Power UPSes
 *
 * A document describing the protocol implemented by this driver can be
 * found online at http://www.networkupstools.org/ups-protocols/
 *
 * Copyright (C)
 *   2013 - Daniele Pezzini <hyouko@gmail.com>
 * Based on blazer_usb.c - Copyright (C)
 *   2003-2009  Arjen de Korte <adkorte-guest@alioth.debian.org>
 *   2011-2012  Arnaud Quette <arnaud.quette@free.fr>
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
#include "voltronic.h"

#define DRIVER_NAME	"Voltronic Power USB driver"
#define DRIVER_VERSION	"0.02"

/* For testing purposes */
/*#define TESTING*/

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Daniele Pezzini <hyouko@gmail.com>\n" \
	"Arjen de Korte <adkorte-guest@alioth.debian.org>\n" \
	"Arnaud Quette <arnaud.quette@free.fr>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

static usb_communication_subdriver_t *usb = &usb_subdriver;
static usb_dev_handle		*udev = NULL;
static USBDevice_t		usbdevice;
static USBDeviceMatcher_t	*reopen_matcher = NULL;
static USBDeviceMatcher_t	*regex_matcher = NULL;

static int (*subdriver_command)(const char *cmd, char *buf, size_t buflen) = NULL;


static int cypress_command(const char *cmd, char *buf, size_t buflen)
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
			0x09, 0x200, 0, &tmp[i], 8, 5000);

		if (ret <= 0) {
			upsdebugx(3, "send: %s", ret ? usb_strerror() : "timeout");
			return ret;
		}
	}

	upsdebugx(3, "send: %.*s", (int)strcspn(tmp, "\r"), tmp);

	memset(buf, 0, buflen);

	for (i = 0; (i <= buflen-8) && (strchr(buf, '\r') == NULL); i += ret) {

		/* Read data in 8-byte chunks */
		/* ret = usb->get_interrupt(udev, (unsigned char *)&buf[i], 8, 1000); */
		ret = usb_interrupt_read(udev, 0x81, &buf[i], 8, 1000);

		/*
		 * Any errors here mean that we are unable to read a reply
		 */
		if (ret <= 0) {
			upsdebugx(3, "read: %s", ret ? usb_strerror() : "timeout");
			return ret;
		}
	}

	upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);
	return i;
}


static void *cypress_subdriver(USBDevice_t *device)
{
	subdriver_command = &cypress_command;
	return NULL;
}


static usb_device_id_t voltronic_usb_id[] = {
	{ USB_DEVICE(0x0665, 0x5161), &cypress_subdriver },	/* Voltronic Power UPSes */
	/* end of list */
	{-1, -1, NULL}
};


static int device_match_func(USBDevice_t *hd, void *privdata)
{
	if (subdriver_command) {
		return 1;
	}

	switch (is_usb_device_supported(voltronic_usb_id, hd))
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
int voltronic_command(const char *cmd, char *buf, size_t buflen)
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
	case -EBUSY:		/* Device or resource busy */
		fatal_with_errno(EXIT_FAILURE, "Got disconnected by another driver");

	case -EPERM:		/* Operation not permitted */
		fatal_with_errno(EXIT_FAILURE, "Permissions problem");

	case -EPIPE:		/* Broken pipe */
		if (usb_clear_halt(udev, 0x81) == 0) {
			upsdebugx(1, "Stall condition cleared");
			break;
		}
#ifdef ETIME
	case -ETIME:		/* Timer expired */
#endif
		if (usb_reset(udev) == 0) {
			upsdebugx(1, "Device reset handled");
		}
	case -ENODEV:		/* No such device */
	case -EACCES:		/* Permission denied */
	case -EIO:		/* I/O error */
	case -ENXIO:		/* No such device or address */
	case -ENOENT:		/* No such file or directory */
		/* Uh oh, got to reconnect! */
		usb->close(udev);
		udev = NULL;
		break;

	case -ETIMEDOUT:	/* Connection timed out */
	case -EOVERFLOW:	/* Value too large for defined data type */
	case -EPROTO:		/* Protocol error */
	default:
		break;
	}

	return ret;
#else
	const struct {
		const char	*command;
		const char	*answer;
	} testing[] = {
		{ "QGS\r", "(234.9 50.0 229.8 50.0 000.0 000 369.1 ---.- 026.5 ---.- 018.8 100000000001\r" },
		{ "QPI\r", "(PI03\r" },
		{ "QRI\r", "(230.0 004 024.0 50.0\r" },
		{ "QMF\r", "(#####VOLTRONIC\r" },
		{ "I\r", "#-------------   ------     VT12046Q  \r" },
		{ "F\r", "#220.0 000 024.0 50.0\r" },
		{ "QMD\r", "(#######OLHVT1K0 ###1000 80 3/3 230 230 02 12.0\r" },
		{ "QFS\r", "(14 212.1 50.0 005.6 49.9 006 010.6 343.8 ---.- 026.2 021.8 01101100\r" },
		{ "QMOD\r", "(Y\r" },
		{ "QVFW\r", "(VERFW:00322.02\r" },
		{ "QID\r", "(685653211455\r" },
		{ "QBV\r", "(026.5 02 01 068 255\r" },
		{ "QFLAG\r", "(EpashcjDbroegfl\r" },
		{ "QWS\r", "(0000000000000000000000000000000000000000000000000000000000000000\r" },
		{ "QHE\r", "(242 218\r" },
		{ "QBYV\r", "(264 170\r" },
		{ "QBYF\r", "(53.0 47.0\r" },
		{ "QSK1\r", "(1\r" },
		{ "QSK2\r", "(0\r" },
		{ "QSK3\r", "(1\r" },
		{ "QSK4\r", "(NAK\r" },
		{ "QSKT1\r", "(008\r" },
		{ "QSKT2\r", "(012\r" },
		{ "QSKT3\r", "(NAK\r" },
		{ "QSKT4\r", "(007\r" },
		{ "RE0\r", "#20\r" },
		{ "W0E24\r", "(ACK\r" },
		{ "PF\r", "(ACK\r" },
		{ "PEA\r", "(ACK\r" },
		{ "PDR\r", "(NAK\r" },
		{ "HEH250\r", "(ACK\r" },
		{ "HEL210\r", "(ACK\r" },
		{ "PHV260\r", "(NAK\r" },
		{ "PLV190\r", "(ACK\r" },
		{ "PGF51.0\r", "(NAK\r" },
		{ "PSF47.5\r", "(ACK\r" },
		{ "BATN3\r", "(ACK\r" },
		{ "BATGN04\r", "(ACK\r" },
		{ "QBT\r", "(01\r" },
		{ "PBT02\r", "(ACK\r" },
		{ "QGR\r", "(00\r" },
		{ "PGR01\r", "(ACK\r" },
		{ "PSK1008\r", "(ACK\r" },
		{ "PSK3987\r", "(ACK\r" },
		{ "PSK2009\r", "(ACK\r" },
		{ "PSK4012\r", "(ACK\r" },
		{ "Q3PV\r", "(123.4 456.4 789.4 012.4 323.4 223.4\r" },
		{ "Q3OV\r", "(253.4 163.4 023.4 143.4 103.4 523.4\r" },
		{ "Q3OC\r", "(109 069 023\r" },
		{ "Q3LD\r", "(005 033 089\r" },
		{ "Q3YV\r", "(303.4 245.4 126.4 222.4 293.4 321.4\r" },
		{ "Q3PC\r", "(002 023 051\r" },
		{ "SOFF\r", "(NAK\r" },
		{ "SON\r", "(ACK\r" },
		{ "T\r", "(NAK\r" },
		{ "TL\r", "(ACK\r" },
		{ "CS\r", "(ACK\r" },
		{ "CT\r", "(NAK\r" },
		{ "BZOFF\r", "(ACK\r" },
		{ "BZON\r", "(ACK\r" },
		{ "S.3R0002\r", "(ACK\r" },
		{ "S02R0024\r", "(NAK\r" },
		{ "S.5\r", "(ACK\r" },
		{ "T.3\r", "(ACK\r" },
		{ "T02\r", "(NAK\r" },
		{ "SKON1\r", "(ACK\r" },
		{ "SKOFF1\r", "(NAK\r" },
		{ "SKON2\r", "(ACK\r" },
		{ "SKOFF2\r", "(ACK\r" },
		{ "SKON3\r", "(NAK\r" },
		{ "SKOFF3\r", "(ACK\r" },
		{ "SKON4\r", "(NAK\r" },
		{ "SKOFF4\r", "(NAK\r" },
		{ "QPAR\r", "(003\r" },
		{ "QPD\r", "(000 240\r" },
		{ "PPD120\r", "(ACK\r" },
		{ "QLDL\r", "(005 080\r" },
		{ "QBDR\r", "(1234\r" },
		{ "QFRE\r", "(50.0 00.0\r" },
		{ "FREH54.0\r", "(ACK\r" },
		{ "FREL47.0\r", "(ACK\r" },
		{ "PEP\r", "(ACK\r" },
		{ "PDP\r", "(ACK\r" },
		{ "PEB\r", "(ACK\r" },
		{ "PDB\r", "(ACK\r" },
		{ "PER\r", "(NAK\r" },
		{ "PDR\r", "(NAK\r" },
		{ "PEO\r", "(ACK\r" },
		{ "PDO\r", "(ACK\r" },
		{ "PEA\r", "(ACK\r" },
		{ "PDA\r", "(ACK\r" },
		{ "PES\r", "(ACK\r" },
		{ "PDS\r", "(ACK\r" },
		{ "PEV\r", "(ACK\r" },
		{ "PDV\r", "(ACK\r" },
		{ "PEE\r", "(ACK\r" },
		{ "PDE\r", "(ACK\r" },
		{ "PEG\r", "(ACK\r" },
		{ "PDG\r", "(NAK\r" },
		{ "PED\r", "(ACK\r" },
		{ "PDD\r", "(ACK\r" },
		{ "PEC\r", "(ACK\r" },
		{ "PDC\r", "(NAK\r" },
		{ "PEF\r", "(NAK\r" },
		{ "PDF\r", "(ACK\r" },
		{ "PEJ\r", "(NAK\r" },
		{ "PDJ\r", "(ACK\r" },
		{ "PEL\r", "(ACK\r" },
		{ "PDL\r", "(ACK\r" },
		{ "PEN\r", "(ACK\r" },
		{ "PDN\r", "(ACK\r" },
		{ "PEQ\r", "(ACK\r" },
		{ "PDQ\r", "(ACK\r" },
		{ "PEW\r", "(NAK\r" },
		{ "PDW\r", "(ACK\r" },
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
	printf("Read The Fine Manual ('man 8 voltronic_usb')\n");
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

	voltronic_makevartable();
}


void upsdrv_initups(void)
{
#ifndef TESTING
	const struct {
		const char	*name;
		int		(*command)(const char *cmd, char *buf, size_t buflen);
	} subdriver[] = {
		{ "cypress", &cypress_command },
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
			"(man 8 voltronic_usb).\n");
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

	dstate_setinfo("ups.vendorid", "%04x", usbdevice.VendorID);
	dstate_setinfo("ups.productid", "%04x", usbdevice.ProductID);

#endif
	voltronic_initups();
}


void upsdrv_initinfo(void)
{
	voltronic_initinfo();
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
