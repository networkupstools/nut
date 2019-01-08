/*
 * richcomm_usb.c - driver for UPS with Richcomm dry-contact to USB
 *                  solution, such as 'Sweex Manageable UPS 1000VA'
 *
 * May also work on 'Kebo UPS-650D', not tested as of 05/23/2007
 *
 * Copyright (C) 2007 Peter van Valderen <p.v.valderen@probu.nl>
 *                    Dirk Teurlings <dirk@upexia.nl>
 * Copyright (C) 2016 Eaton
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
#include "bool.h"
#include "nut_libusb.h"

/* driver version */
#define DRIVER_NAME	"Richcomm dry-contact to USB driver"
#define DRIVER_VERSION	"0.23"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Peter van Valderen <p.v.valderen@probu.nl>\n"
	"Dirk Teurlings <dirk@upexia.nl>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define STATUS_REQUESTTYPE	(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE)
#define REPLY_ENDPOINT		(LIBUSB_ENDPOINT_IN | 1)
#define QUERY_PACKETSIZE	4
#define REPLY_PACKETSIZE	6
#define REQUEST_VALUE		0x09
#define MESSAGE_VALUE		0x200
#define INDEX_VALUE		0

/* limit the amount of spew that goes in the syslog when we lose the UPS (from nut_usb.h) */
#define USB_ERR_LIMIT	10	/* start limiting after 10 in a row */
#define USB_ERR_RATE	10	/* then only print every 10th error */

static usb_device_id_t richcomm_usb_id[] = {
	/* Sweex 1000VA */
	{ USB_DEVICE(0x0925, 0x1234),  NULL },

	/* end of list */
	{-1, -1, NULL}
};

static libusb_device_handle	*udev = NULL;
static USBDevice_t		usbdevice;
static unsigned int		comm_failures = 0;

static int device_match_func(USBDevice_t *device, void *privdata)
{
	switch (is_usb_device_supported(richcomm_usb_id, device))
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

/** @brief Send the command stored in *query*, read the reply and store it in *reply*.
 * @return 0, on failure,
 * @return the number of bytes read, on success. */
static int execute_and_retrieve_query(char *query, char *reply)
{
	int	ret, transferred;

	ret = libusb_control_transfer(
		udev,
		STATUS_REQUESTTYPE,
		REQUEST_VALUE,
		MESSAGE_VALUE,
		INDEX_VALUE,
		(unsigned char *)query,
		QUERY_PACKETSIZE,
		1000
	);

	if (ret <= 0) {
		upsdebugx(3, "send: %s", ret ? libusb_strerror(ret) : "timeout");
		return 0;
	}

	upsdebug_hex(3, "send", query, ret);

	ret = libusb_interrupt_transfer(udev, REPLY_ENDPOINT, (unsigned char *)reply, REPLY_PACKETSIZE, &transferred, 1000);

	if (ret != LIBUSB_SUCCESS || transferred == 0) {
		upsdebugx(3, "read: %s", ret ? libusb_strerror(ret) : "timeout");
		return 0;
	}

	upsdebug_hex(3, "read", reply, transferred);
	return transferred;
}

/** @brief Prepare the status request query and call execute_and_retrieve_query() with it and *reply*.
 * @return see execute_and_retrieve_query(). */
static int query_ups(char *reply)
{
	/*
	 * This packet is a status request to the UPS
	 */
	char	query[QUERY_PACKETSIZE] = { 0x01, 0x00, 0x00, 0x30 };

	return execute_and_retrieve_query(query, reply);
}

static void usb_comm_fail(const char *fmt, ...)
{
	int	ret;
	char	why[SMALLBUF];
	va_list	ap;

	/* this means we're probably here because select was interrupted */
	if (exit_flag != 0) {
		return;	 /* ignored, since we're about to exit anyway */
	}

	comm_failures++;

	if ((comm_failures == USB_ERR_LIMIT) || ((comm_failures % USB_ERR_RATE) == 0)) {
		upslogx(LOG_WARNING, "Warning: excessive comm failures, limiting error reporting");
	}

	/* once it's past the limit, only log once every USB_ERR_LIMIT calls */
	if ((comm_failures > USB_ERR_LIMIT) && ((comm_failures % USB_ERR_LIMIT) != 0)) {
		return;
	}

	/* generic message if the caller hasn't elaborated */
	if (!fmt) {
		upslogx(LOG_WARNING, "Communications with UPS lost - check cabling");
		return;
	}

	va_start(ap, fmt);
	ret = vsnprintf(why, sizeof(why), fmt, ap);
	va_end(ap);

	if ((ret < 1) || (ret >= (int) sizeof(why))) {
		upslogx(LOG_WARNING, "usb_comm_fail: vsnprintf needed more than %d bytes", (int)sizeof(why));
	}

	upslogx(LOG_WARNING, "Communications with UPS lost: %s", why);
}

static void usb_comm_good(void)
{
	if (comm_failures == 0) {
		return;
	}

	upslogx(LOG_NOTICE, "Communications with UPS re-established");	
	comm_failures = 0;
}

/** @brief Try to open a USB device matching @ref device_matcher.
 *
 * If @ref udev refers to an already opened device (i.e. it is not `NULL`), it is closed before attempting the reopening.
 *
 * @return @ref TRUE, with @ref udev being the handle of the opened device, and with @ref usbdevice filled, on success,
 * @return @ref FALSE, with @ref udev being `NULL`, on failure. */
static bool_t	open_device(void)
{
	int	ret;

	ret = usb_subdriver.open(&udev, &usbdevice, &device_matcher, 1, NULL);
	if (ret != LIBUSB_SUCCESS)
		return FALSE;

	ret = libusb_clear_halt(udev, LIBUSB_ENDPOINT_IN | 1);
	if (ret != LIBUSB_SUCCESS) {
		upsdebugx(1, "%s: can't reset USB endpoint: %s.", __func__, libusb_strerror(ret));
		usb_subdriver.close(udev);
		udev = NULL;
		return FALSE;
	}

	return TRUE;
}

/*
 * Initialise the UPS
 */
void upsdrv_initups(void)
{
	char	reply[REPLY_PACKETSIZE];
	int	i;

	/* Initialise the communication subdriver */
	usb_subdriver.init();

	/* Try to open the device */
	for (i = 0; !open_device(); i++) {
		if ((i < 32) && (sleep(5) == 0)) {
			usb_comm_fail("Can't open USB device, retrying ...");
			continue;
		}

		fatalx(EXIT_FAILURE,
			"Unable to find Richcomm dry-contact to USB solution\n\n"

			"Things to try:\n"
			" - Connect UPS device to USB bus\n"
			" - Run this driver as another user (upsdrvctl -u or 'user=...' in ups.conf).\n"
			"   See upsdrvctl(8) and ups.conf(5).\n\n"

			"Fatal error: unusable configuration");
	}

	/*
	 * Read rubbish data a few times; the UPS doesn't seem to respond properly
	 * the first few times after connecting
	 */
	for (i = 0; i < 5; i++) {
		query_ups(reply);
		sleep(1);
	}
}

void upsdrv_cleanup(void)
{
	usb_subdriver.close(udev);
	usb_subdriver.deinit();

	free(usbdevice.Vendor);
	free(usbdevice.Product);
	free(usbdevice.Serial);
	free(usbdevice.Bus);
}

void upsdrv_initinfo(void)
{
	dstate_setinfo("ups.mfr", "%s", "Richcomm dry-contact to USB solution");
	dstate_setinfo("ups.model", "%s", usbdevice.Product ? usbdevice.Product : "unknown");
	dstate_setinfo("ups.serial", "%s", usbdevice.Serial ? usbdevice.Serial : "unknown");

	dstate_setinfo("ups.vendorid", "%04x", usbdevice.VendorID);
	dstate_setinfo("ups.productid", "%04x", usbdevice.ProductID);
}

void upsdrv_updateinfo(void)
{
	char	reply[REPLY_PACKETSIZE];
	int	ret, online, battery_normal;

	if (!udev && !open_device())
		return;

	ret = query_ups(reply);

	if (ret < 4) {
		usb_comm_fail("Query to UPS failed");
		dstate_datastale();

		usb_subdriver.close(udev);
		udev = NULL;

		return;
	}

	usb_comm_good();
	dstate_dataok();

	/*
	 * 3rd bit of 4th byte indicates whether the UPS is on line (1)
	 * or on battery (0)
	 */
	online = (reply[3]&4)>>2;

	/*
	 * 2nd bit of 4th byte indicates battery status; normal (1)
	 * or low (0)
	 */
	battery_normal = (reply[3]&2)>>1;

	status_init();

	if (online) {
	    status_set("OL");
	} else {
	    status_set("OB");
	}

	if (!battery_normal) {
	    status_set("LB");
	}

	status_commit();
}

/*
 * The shutdown feature is a bit strange on this UPS IMHO, it
 * switches the polarity of the 'Shutdown UPS' signal, at which
 * point it will automatically power down once it loses power.
 *
 * It will still, however, be possible to poll the UPS and
 * reverse the polarity _again_, at which point it will
 * start back up once power comes back.
 *
 * Maybe this is the normal way, it just seems a bit strange.
 *
 * Please note, this function doesn't power the UPS off if
 * line power is connected.
 */
void upsdrv_shutdown(void)
{
	/*
	 * This packet shuts down the UPS, that is,
	 * if it is not currently on line power
	 */
	char	prepare[QUERY_PACKETSIZE] = { 0x02, 0x00, 0x00, 0x00 };

	/*
	 * This should make the UPS turn itself back on once the
	 * power comes back on; which is probably what we want
	 */
	char	restart[QUERY_PACKETSIZE] = { 0x02, 0x01, 0x00, 0x00 };
	char	reply[REPLY_PACKETSIZE];

	execute_and_retrieve_query(prepare, reply);

	/*
	 * have to, the previous command seems to be
	 * ignored if the second command comes right
	 * behind it
	 */
	sleep(1);


	execute_and_retrieve_query(restart, reply);
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}
