/*
 * richcomm_usb.c - driver for UPS with lakeview chipset, such as
 *                  'Sweex Manageable UPS 1000VA' (ca. 2006)
 *
 * May also work on 'Kebo UPS-650D', not tested as of 05/23/2007
 *
 * Copyright (C) 2007 - Peter van Valderen <p.v.valderen@probu.nl>
 *               2007 - Dirk Teurlings <dirk@upexia.nl>
 *               2008 - Arjen de Korte <adkorte-guest@alioth.debian.org>
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

#include <usb.h>

#include "main.h"
#include "libusb.h"
#include "usb-common.h"
#include "richcomm_usb.h"

/* driver definitions */
#define QUERY_PACKETSIZE	4
#define REPLY_PACKETSIZE	8
#define MAXTRIES		3

static usb_communication_subdriver_t *usb = &usb_subdriver;
static usb_dev_handle		*udev = NULL;
static USBDevice_t		usbdevice;
static USBDeviceMatcher_t	*reopen_matcher = NULL;
static USBDeviceMatcher_t	*regex_matcher = NULL;

typedef enum {
	STATUS = 0,
	SHUTDOWN_STAYOFF,
	SHUTDOWN_RETURN
} richcomm_query_t;

static char richcomm_usb_cmd[][QUERY_PACKETSIZE] = {
	/* This packet is a status request to the UPS */
	{ 0x01, 0x00, 0x00, 0x30 },
	/* This packet shuts down the UPS, that is, if it is not currently on line power */
	{ 0x02, 0x00, 0x00, 0x00 },
	/* This should make the UPS turn itself back on once the power comes back on; which is probably what we want */
	{ 0x02, 0x01, 0x00, 0x00 }
}; 

static usb_device_id_t richcomm_usb_id[] = {
	/* Sweex 1000VA */
	{ USB_DEVICE(0x0925, 0x1234),  NULL },
	/* end of list */
	{-1, -1, NULL}
};

static int richcomm_match_func(USBDevice_t *hd, void *privdata)
{
	switch (is_usb_device_supported(richcomm_usb_id, hd->VendorID, hd->ProductID))
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
	&richcomm_match_func,
	NULL,
	NULL
};

static int richcomm_command(char *query, char *reply)
{
	int ret;

	ret = usb_control_msg(udev, USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
				0x09, /* HID_REPORT_SET */
				0x200, /* HID_REPORT_TYPE_OUTPUT */
				0, query, QUERY_PACKETSIZE, 1000);
	if (ret < 0) {
		upsdebug_with_errno(3, "send");
		return ret;
	} else if (ret == 0) {
		upsdebugx(3, "send: timeout");
		return ret;
	}

	upsdebug_hex(3, "send", query, ret);

	ret = usb_interrupt_read(udev, 0x81, reply, REPLY_PACKETSIZE, 1000);
	if (ret < 0) {
		upsdebug_with_errno(3, "read");
		return ret;
	} else if (ret == 0) {
		upsdebugx(3, "read: timeout");
		return ret;
	}

	upsdebug_hex(3, "read", reply, ret);
	return ret;
}

static int do_command(char *query, char *reply)
{
	int	ret;

	if (udev == NULL) {
		ret = usb->open(&udev, &usbdevice, reopen_matcher, NULL);

		if (ret < 1) {
			return ret;
		}
	}

	ret = richcomm_command(query, reply);
	if (ret >= 0) {
		return ret;
	}

	upsdebug_with_errno(2, "%s", __func__);

	switch (errno)
	{
	case EBUSY:
		fatal_with_errno(EXIT_FAILURE, "Got disconnected by another driver");

	case EPERM:
		fatal_with_errno(EXIT_FAILURE, "Permissions problem");

	case EPIPE:
	case ENODEV:
	case EACCES:
	case EIO:
	case ENOENT:
		/* Uh oh, got to reconnect! */
		usb->close(udev);
		udev = NULL;
		break;
	}

	return ret;
}

/*
 * Initialise the UPS
 */
void upsdrv_initups(void)
{
	int	i, ret;
	char	reply[REPLY_PACKETSIZE];
	char	*regex_array[6];

	regex_array[0] = NULL;
	regex_array[1] = NULL;
	regex_array[2] = getval("vendor");
	regex_array[3] = getval("product");
	regex_array[4] = getval("serial");
	regex_array[5] = getval("bus");

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
		fatalx(EXIT_FAILURE, "No supported devices found");
	}

	/* create a new matcher for later reopening */
	ret = USBNewExactMatcher(&reopen_matcher, &usbdevice);
	if (ret) {
		fatal_with_errno(EXIT_FAILURE, "USBNewExactMatcher");
	}

	/* link the matchers */
	reopen_matcher->next = regex_matcher;

	if (usb_clear_halt(udev, 0x81) < 0) {
		fatalx(EXIT_FAILURE, "Can't reset USB endpoint");
	}

	/*
	 * Read rubbish data a few times; the UPS doesn't seem to respond properly
	 * the first few times after connecting
	 */
	for (i = 0; i < 5; i++) {
		if (do_command(richcomm_usb_cmd[STATUS], reply) < 1) {
			break;
		}
	}
}

void upsdrv_cleanup(void)
{
	usb->close(udev);
	USBFreeExactMatcher(reopen_matcher);
	USBFreeRegexMatcher(regex_matcher);
	free(usbdevice.Vendor);
	free(usbdevice.Product);
	free(usbdevice.Serial);
	free(usbdevice.Bus);
}

void upsdrv_initinfo(void)
{
	dstate_setinfo("ups.mfr", usbdevice.Vendor ? usbdevice.Vendor : "Richcomm compatible");
	dstate_setinfo("ups.model", usbdevice.Product ? usbdevice.Product : "[unknown]");
	dstate_setinfo("ups.serial", usbdevice.Serial ? usbdevice.Serial : "[unknown]");

	dstate_setinfo("ups.vendorid", "%04x", usbdevice.VendorID);
	dstate_setinfo("ups.productid", "%04x", usbdevice.ProductID);
}

void upsdrv_updateinfo(void)
{
	static int	retry = 0;
	char	reply[REPLY_PACKETSIZE];

	if (do_command(richcomm_usb_cmd[STATUS], reply) < 4) {

		if (retry < MAXTRIES) {
			upslogx(LOG_WARNING, "Communications with UPS lost: status read failed!");
			retry++;
		} else {
			dstate_datastale();
		}

		return;
	}

	if (retry) {
		upslogx(LOG_NOTICE, "Communications with UPS re-established");
	}

	retry = 0;

	status_init();

	/*
	 * 3rd bit of 4th byte indicates whether the UPS is on line (1) or on battery (0)
	 */
	if (reply[3] & 0x04) {
	    status_set("OL");
	}
	else {
	    status_set("OB");
	}

	/*
	 * 2nd bit of 4th byte indicates battery status; normal (1) or low (0)
	 */
	if (!(reply[3] & 0x02)) {
	    status_set("LB");
	}

	status_commit();
	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	char	reply[REPLY_PACKETSIZE];

	do_command(richcomm_usb_cmd[SHUTDOWN_STAYOFF], reply);

	sleep(1);

	do_command(richcomm_usb_cmd[SHUTDOWN_RETURN], reply);
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - Richcomm compatible USB UPS driver %s (%s)\n\n",
		DRV_VERSION, UPS_VERSION);

	experimental_driver = 1;	
}
