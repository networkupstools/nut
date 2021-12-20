/*
 * blazer_usb.c: support for Megatec/Q1 USB protocol based UPSes
 *
 * OBSOLETION WARNING: Please to not base new development on this
 * codebase, instead create a new subdriver for nutdrv_qx which
 * generally covers all Megatec/Qx protocol family and aggregates
 * device support from such legacy drivers over time.
 *
 * A document describing the protocol implemented by this driver can be
 * found online at "http://www.networkupstools.org/protocols/megatec.html".
 *
 * Copyright (C) 2003-2009  Arjen de Korte <adkorte-guest@alioth.debian.org>
 * Copyright (C) 2011-2012  Arnaud Quette <arnaud.quette@free.fr>
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
#define DRIVER_VERSION	"0.12"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arjen de Korte <adkorte-guest@alioth.debian.org>\n" \
	"Arnaud Quette <arnaud.quette@free.fr>",
	DRV_BETA,
	{ NULL }
};

#ifndef TESTING

static usb_communication_subdriver_t *usb = &usb_subdriver;
static usb_dev_handle		*udev = NULL;
static USBDevice_t		usbdevice;
static USBDeviceMatcher_t	*reopen_matcher = NULL;
static USBDeviceMatcher_t	*regex_matcher = NULL;
static int					langid_fix = -1;

static int (*subdriver_command)(const char *cmd, char *buf, size_t buflen) = NULL;


static int cypress_command(const char *cmd, char *buf, size_t buflen)
{
	char	tmp[SMALLBUF];
	int	ret;
	size_t	i;

	memset(tmp, 0, sizeof(tmp));
	snprintf(tmp, sizeof(tmp), "%s", cmd);

	for (i = 0; i < strlen(tmp); i += (size_t)ret) {

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

	for (i = 0; (i <= buflen-8) && (strchr(buf, '\r') == NULL); i += (size_t)ret) {

		/* Read data in 8-byte chunks */
		/* ret = usb->get_interrupt(udev, (unsigned char *)&buf[i], 8, 1000); */
		ret = usb_interrupt_read(udev, 0x81, &buf[i], 8, 1000);

		/*
		 * Any errors here mean that we are unable to read a reply (which
		 * will happen after successfully writing a command to the UPS)
		 */
		if (ret <= 0) {
			upsdebugx(3, "read: %s", ret ? usb_strerror() : "timeout");
			return ret;
		}
	}

	upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);
	/* TODO: Range-check before cast */
	return (int)i;
}


static int phoenix_command(const char *cmd, char *buf, size_t buflen)
{
	char	tmp[SMALLBUF];
	int	ret;
	size_t	i;

	for (i = 0; i < 8; i++) {

		/* Read data in 8-byte chunks */
		/* ret = usb->get_interrupt(udev, (unsigned char *)tmp, 8, 1000); */
		ret = usb_interrupt_read(udev, 0x81, tmp, 8, 1000);

		/*
		 * This USB to serial implementation is crappy. In order to read correct
		 * replies we need to flush the output buffers of the converter until we
		 * get no more data (ie, it times out).
		 */
		switch (ret)
		{
		case -EPIPE:		/* Broken pipe */
			usb_clear_halt(udev, 0x81);
		case -ETIMEDOUT:	/* Connection timed out */
			break;
		}

		if (ret < 0) {
			upsdebugx(3, "flush: %s", usb_strerror());
			break;
		}

		upsdebug_hex(4, "dump", tmp, (size_t)ret);
	}

	memset(tmp, 0, sizeof(tmp));
	snprintf(tmp, sizeof(tmp), "%s", cmd);

	for (i = 0; i < strlen(tmp); i += (size_t)ret) {

		/* Write data in 8-byte chunks */
		/* ret = usb->set_report(udev, 0, (unsigned char *)&tmp[i], 8); */
		ret = usb_control_msg(udev, USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
			0x09, 0x200, 0, &tmp[i], 8, 1000);

		if (ret <= 0) {
			upsdebugx(3, "send: %s", ret ? usb_strerror() : "timeout");
			return ret;
		}
	}

	upsdebugx(3, "send: %.*s", (int)strcspn(tmp, "\r"), tmp);

	memset(buf, 0, buflen);

	for (i = 0; (i <= buflen-8) && (strchr(buf, '\r') == NULL); i += (size_t)ret) {

		/* Read data in 8-byte chunks */
		/* ret = usb->get_interrupt(udev, (unsigned char *)&buf[i], 8, 1000); */
		ret = usb_interrupt_read(udev, 0x81, &buf[i], 8, 1000);

		/*
		 * Any errors here mean that we are unable to read a reply (which
		 * will happen after successfully writing a command to the UPS)
		 */
		if (ret <= 0) {
			upsdebugx(3, "read: %s", ret ? usb_strerror() : "timeout");
			return ret;
		}
	}

	upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);
	/* TODO: Range-check before cast */
	return (int)i;
}


static int ippon_command(const char *cmd, char *buf, size_t buflen)
{
	char	tmp[64];
	int	ret, len;
	size_t	i;

	snprintf(tmp, sizeof(tmp), "%s", cmd);

	for (i = 0; i < strlen(tmp); i += (size_t)ret) {

		/* Write data in 8-byte chunks */
		ret = usb_control_msg(udev, USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
			0x09, 0x2, 0, &tmp[i], 8, 1000);

		if (ret <= 0) {
			upsdebugx(3, "send: %s", (ret != -ETIMEDOUT) ? usb_strerror() : "Connection timed out");
			return ret;
		}
	}

	upsdebugx(3, "send: %.*s", (int)strcspn(tmp, "\r"), tmp);

	/* Read all 64 bytes of the reply in one large chunk */
	ret = usb_interrupt_read(udev, 0x81, tmp, sizeof(tmp), 1000);

	/*
	 * Any errors here mean that we are unable to read a reply (which
	 * will happen after successfully writing a command to the UPS)
	 */
	if (ret <= 0) {
		upsdebugx(3, "read: %s", (ret != -ETIMEDOUT) ? usb_strerror() : "Connection timed out");
		return ret;
	}

	/*
	 * As Ippon will always return 64 bytes in response, we have to
	 * calculate and return length of actual response data here.
	 * Empty response will look like 0x00 0x0D, otherwise it will be
	 * data string terminated by 0x0D.
	 */
	len = (int)strcspn(tmp, "\r");
	upsdebugx(3, "read: %.*s", len, tmp);
	if (len > 0) {
		len ++;
	}
	snprintf(buf, buflen, "%.*s", len, tmp);
	return len;
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
		{ NULL, 0, '\0' }
	};

	int	i;

	upsdebugx(3, "send: %.*s", (int)strcspn(cmd, "\r"), cmd);

	for (i = 0; command[i].str; i++) {
		int	retry;

		if (strcmp(cmd, command[i].str)) {
			continue;
		}

		for (retry = 0; retry < 10; retry++) {
			int	ret;

			if (langid_fix != -1) {
				/* Apply langid_fix value */
				ret = usb_get_string(udev, command[i].index, langid_fix, buf, buflen);
			}
			else {
				ret = usb_get_string_simple(udev, command[i].index, buf, buflen);
			}

			if (ret <= 0) {
				upsdebugx(3, "read: %s", ret ? usb_strerror() : "timeout");
				return ret;
			}

			/* this may serve in the future */
			upsdebugx(1, "received %d (%d)", ret, buf[0]);

			if (langid_fix != -1) {
				/* Limit this check, at least for now */
				/* Invalid receive size - message corrupted */
				if (ret != buf[0])
				{
					upsdebugx(1, "size mismatch: %d / %d", ret, buf[0]);
					continue;
				}

				/* Simple unicode -> ASCII inplace conversion
				 * FIXME: this code is at least shared with mge-shut/libshut
				 * Create a common function? */
				size_t di, si, size = (size_t)buf[0];
				for (di = 0, si = 2; si < size; si += 2) {
					if (di >= (buflen - 1))
						break;

					if (buf[si + 1]) /* high byte */
						buf[di++] = '?';
					else
						buf[di++] = buf[si];
				}
				buf[di] = 0;
				/* with buf a char* array, practical "size" limit and
				 * so "di" are small enough to cast to int */
				ret = (int)di;
			}

			/* "UPS No Ack" has a special meaning */
			if (!strcasecmp(buf, "UPS No Ack")) {
				upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);
				continue;
			}

			/* Replace the first byte of what we received with the correct one */
			buf[0] = command[i].prefix;

			upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);
			return ret;
		}

		return 0;
	}

	/* echo the unknown command back */
	upsdebugx(3, "read: %.*s", (int)strcspn(cmd, "\r"), cmd);
	return snprintf(buf, buflen, "%s", cmd);
}


static void *cypress_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &cypress_command;
	return NULL;
}


static void *ippon_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &ippon_command;
	return NULL;
}


static void *krauler_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &krauler_command;
	return NULL;
}


static void *phoenix_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &phoenix_command;
	return NULL;
}


static usb_device_id_t blazer_usb_id[] = {
	{ USB_DEVICE(0x05b8, 0x0000), &cypress_subdriver },	/* Agiler UPS */
	{ USB_DEVICE(0x0001, 0x0000), &krauler_subdriver },	/* Krauler UP-M500VA */
	{ USB_DEVICE(0xffff, 0x0000), &krauler_subdriver },	/* Ablerex 625L USB */
	{ USB_DEVICE(0x0665, 0x5161), &cypress_subdriver },	/* Belkin F6C1200-UNV */
	{ USB_DEVICE(0x06da, 0x0002), &cypress_subdriver },	/* Online Yunto YQ450 */
	{ USB_DEVICE(0x06da, 0x0003), &ippon_subdriver },	/* Mustek Powermust */
	{ USB_DEVICE(0x06da, 0x0004), &cypress_subdriver },	/* Phoenixtec Innova 3/1 T */
	{ USB_DEVICE(0x06da, 0x0005), &cypress_subdriver },	/* Phoenixtec Innova RT */
	{ USB_DEVICE(0x06da, 0x0201), &cypress_subdriver },	/* Phoenixtec Innova T */
	{ USB_DEVICE(0x06da, 0x0601), &phoenix_subdriver },	/* Online Zinto A */
	{ USB_DEVICE(0x0f03, 0x0001), &cypress_subdriver },	/* Unitek Alpha 1200Sx */
	{ USB_DEVICE(0x14f0, 0x00c9), &phoenix_subdriver },	/* GE EP series */

	/* Terminating entry */
	{ 0, 0, NULL }
};


static int device_match_func(USBDevice_t *hd, void *privdata)
{
	NUT_UNUSED_VARIABLE(privdata);

	if (subdriver_command) {
		return 1;
	}

	switch (is_usb_device_supported(blazer_usb_id, hd))
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

#endif	/* TESTING */


/*
 * Generic command processing function. Send a command and read a reply.
 * Returns < 0 on error, 0 on timeout and the number of bytes read on
 * success.
 */
ssize_t blazer_command(const char *cmd, char *buf, size_t buflen)
{
#ifndef TESTING
	ssize_t	ret;

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
#ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif

	case -EPERM:		/* Operation not permitted */
		fatal_with_errno(EXIT_FAILURE, "Permissions problem");
#ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif

	case -EPIPE:		/* Broken pipe */
		if (usb_clear_halt(udev, 0x81) == 0) {
			upsdebugx(1, "Stall condition cleared");
			break;
		}
#ifdef ETIME
		goto fallthrough_case_etime;
	case -ETIME:		/* Timer expired */
	fallthrough_case_etime:
#endif
		if (usb_reset(udev) == 0) {
			upsdebugx(1, "Device reset handled");
		}
		goto fallthrough_case_reconnect;
	case -ENODEV:		/* No such device */
	case -EACCES:		/* Permission denied */
	case -EIO:		/* I/O error */
	case -ENXIO:		/* No such device or address */
	case -ENOENT:		/* No such file or directory */
	fallthrough_case_reconnect:
		/* Uh oh, got to reconnect! */
		usb->close(udev);
		udev = NULL;
		break;

	case -ETIMEDOUT:	/* Connection timed out */
	case -EOVERFLOW:	/* Value too large for defined data type */
#ifdef EPROTO
	case -EPROTO:		/* Protocol error */
#endif
	default:
		break;
	}

	return ret;
#else	/* if TESTING: */
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

		/* TODO: Range-check int vs ssize_t values */
		return (ssize_t)snprintf(buf, buflen, "%s", testing[i].answer);
	}

	return (ssize_t)snprintf(buf, buflen, "%s", testing[i].command);
#endif	/* TESTING */
}

#ifndef TESTING
static const struct subdriver_t {
	const char	*name;
	int		(*command)(const char *cmd, char *buf, size_t buflen);
} subdriver[] = {
	{ "cypress", &cypress_command },
	{ "phoenix", &phoenix_command },
	{ "ippon", &ippon_command },
	{ "krauler", &krauler_command },
	{ NULL, NULL }
};
#endif	/* TESTING */

void upsdrv_help(void)
{
#ifndef TESTING
	printf("\nAcceptable values for 'subdriver' via -x or ups.conf in this driver: ");
	size_t i;

	for (i = 0; subdriver[i].name != NULL; i++) {
		if (i>0)
			printf(", ");
		printf("%s", subdriver[i].name);
	}
	printf("\n\n");
#endif	/* TESTING */

	printf("Read The Fine Manual ('man 8 blazer_usb')\n");
}


void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "subdriver", "Serial-over-USB subdriver selection");
	nut_usb_addvars();

	addvar(VAR_VALUE, "langid_fix", "Apply the language ID workaround to the krauler subdriver (0x409 or 0x4095)");

	blazer_makevartable();
}


void upsdrv_initups(void)
{
#ifndef TESTING
	int	ret, langid;
	char	tbuf[255]; /* Some devices choke on size > 255 */
	char	*regex_array[7];

	char	*subdrv = getval("subdriver");

	regex_array[0] = getval("vendorid");
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor");
	regex_array[3] = getval("product");
	regex_array[4] = getval("serial");
	regex_array[5] = getval("bus");
	regex_array[6] = getval("device");

	/* check for language ID workaround (#1) */
	if (getval("langid_fix")) {
		/* skip "0x" prefix and set back to hexadecimal */
		unsigned int u_langid_fix;
		if ( (sscanf(getval("langid_fix") + 2, "%x", &u_langid_fix) != 1) || (u_langid_fix > INT_MAX) ) {
			upslogx(LOG_NOTICE, "Error enabling language ID workaround");
		}
		else {
			langid_fix = (int)u_langid_fix;
			upsdebugx(2, "language ID workaround enabled (using '0x%x')", langid_fix);
		}
	}

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
			"(man 8 blazer_usb).\n");
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

	/* check for language ID workaround (#2) */
	if (langid_fix != -1) {
		/* Future improvement:
		 *   Asking for the zero'th index is special - it returns a string
		 *   descriptor that contains all the language IDs supported by the
		 *   device. Typically there aren't many - often only one. The
		 *   language IDs are 16 bit numbers, and they start at the third byte
		 *   in the descriptor. See USB 2.0 specification, section 9.6.7, for
		 *   more information on this.
		 * This should allow automatic application of the workaround */
		ret = usb_get_string(udev, 0, 0, tbuf, sizeof(tbuf));
		if (ret >= 4) {
			langid = tbuf[2] | (tbuf[3] << 8);
			upsdebugx(1, "First supported language ID: 0x%x (please report to the NUT maintainer!)", langid);
		}
	}
#endif	/* TESTING */
	blazer_initups();
}


void upsdrv_initinfo(void)
{
	blazer_initinfo();
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
	free(usbdevice.Device);
#endif	/* TESTING */
}
