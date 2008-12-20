/* nut_usb.c - common usb functions for Network UPS Tools drivers

   Copyright (C) 2005 Wolfgang Ocker <weo@weo1.de>
   Based upon serial.c: Copyright (C) 2003  Russell Kroll <rkroll@exploits.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "common.h"
#include "timehead.h"

#include <ctype.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <usb.h>

#include "nut_stdint.h" /* for uint16_t */
#include "nut_usb.h"

extern	int		exit_flag;
static	unsigned int	comm_failures = 0;

/* Powerware */
#define POWERWARE	0x0592

/* Phoenixtec Power Co., Ltd */
#define PHOENIXTEC	0x06da

/* usb_set_descriptor() for Powerware devices */
static int usb_set_powerware(usb_dev_handle *udev, unsigned char type, unsigned char index, void *buf, int size)
{
	return usb_control_msg(udev, USB_ENDPOINT_OUT, USB_REQ_SET_DESCRIPTOR, (type << 8) + index, 0, buf, size, 1000);
}

static void *powerware_ups(void) {
	usb_set_descriptor = &usb_set_powerware;
	return NULL;
}

/* usb_set_descriptor() for Phoenixtec devices */
static int usb_set_phoenixtec(usb_dev_handle *udev, unsigned char type, unsigned char index, void *buf, int size)
{
	return usb_control_msg(udev, 0x42, 0x0d, (0x00 << 8) + 0x0, 0, buf, size, 1000);
}

static void *phoenixtec_ups(void) {
	usb_set_descriptor = &usb_set_phoenixtec;
	return NULL;
}

/* USB IDs device table */
static usb_device_id_t pw_usb_device_table[] = {
	/* various models */
	{ USB_DEVICE(POWERWARE, 0x0002), &powerware_ups },

	/* various models */
	{ USB_DEVICE(PHOENIXTEC, 0x0002), &phoenixtec_ups },
	
	/* Terminating entry */
	{ -1, -1, NULL }
};

static void nutusb_open_error(const char *port)
{
	printf("Unable to find POWERWARE UPS device on USB bus \n\n");

	printf("Things to try:\n\n");
	printf(" - Connect UPS device to USB bus\n\n");
	printf(" - Run this driver as another user (upsdrvctl -u or 'user=...' in ups.conf).\n");
	printf("   See upsdrvctl(8) and ups.conf(5).\n\n");

	fatalx(EXIT_FAILURE, "Fatal error: unusable configuration");
}

static usb_dev_handle *open_powerware_usb()
{
	struct usb_bus *busses = usb_get_busses();  
	struct usb_bus *bus;
    
	for (bus = busses; bus; bus = bus->next)
	{
		struct usb_device *dev;
    
		for (dev = bus->devices; dev; dev = dev->next)
		{
			if (dev->descriptor.bDeviceClass != USB_CLASS_PER_INTERFACE) {
				continue;
			}

			if (is_usb_device_supported(pw_usb_device_table, dev->descriptor.idVendor, dev->descriptor.idProduct) == SUPPORTED) {
				return usb_open(dev);
			}
		}
	}

	return 0;
}

usb_dev_handle *nutusb_open(const char *port)
{
	static int     libusb_init = 0;
	int            dev_claimed = 0;
	usb_dev_handle *dev_h = NULL;
	int            retry;
	
	if (!libusb_init)
	{
		/* Initialize Libusb */
		usb_init();
		libusb_init = 1;
	}

	for (retry = 0; dev_h == NULL && retry < 32; retry++)
	{
		struct timespec t = {5, 0};
		usb_find_busses();
		usb_find_devices();

		dev_h = open_powerware_usb();
		if (!dev_h) {
			upslogx(LOG_WARNING, "Can't open POWERWARE USB device, retrying ...");
			if (nanosleep(&t, NULL) < 0 && errno == EINTR)
				break;
		}
	}
	
	if (!dev_h)
	{
		upslogx(LOG_ERR, "Can't open POWERWARE USB device");
		goto errout;
	}

	if (usb_set_configuration(dev_h, 1) < 0)
	{
		upslogx(LOG_ERR, "Can't set POWERWARE USB configuration");
		goto errout;
	}

	if (usb_claim_interface(dev_h, 0) < 0)
	{
		upslogx(LOG_ERR, "Can't claim POWERWARE USB interface");
  	        goto errout;
	}
	else
		dev_claimed = 1;

	if (usb_set_altinterface(dev_h, 0) < 0)
	{
		upslogx(LOG_ERR, "Can't set POWERWARE USB alternate interface");
  	        goto errout;
	}

	if (usb_clear_halt(dev_h, 0x81) < 0)
	{
		upslogx(LOG_ERR, "Can't reset POWERWARE USB endpoint");
		goto errout;
	}
    
	return dev_h;

errout:
	if (dev_h && dev_claimed)
		usb_release_interface(dev_h, 0);
	if (dev_h)
		usb_close(dev_h);

	nutusb_open_error(port);
	return 0;
}

int nutusb_close(usb_dev_handle *dev_h, const char *port)
{
	if (dev_h)
	{
		usb_release_interface(dev_h, 0);
		return usb_close(dev_h);
	}
	
	return 0;
}

void nutusb_comm_fail(const char *fmt, ...)
{
	int	ret;
	char	why[SMALLBUF];
	va_list	ap;

	/* this means we're probably here because select was interrupted */
	if (exit_flag != 0)
		return;		/* ignored, since we're about to exit anyway */

	comm_failures++;

	if ((comm_failures == USB_ERR_LIMIT) ||
		((comm_failures % USB_ERR_RATE) == 0))
	{
		upslogx(LOG_WARNING, "Warning: excessive comm failures, "
			"limiting error reporting");
	}

	/* once it's past the limit, only log once every USB_ERR_LIMIT calls */
	if ((comm_failures > USB_ERR_LIMIT) &&
		((comm_failures % USB_ERR_LIMIT) != 0))
		return;

	/* generic message if the caller hasn't elaborated */
	if (!fmt)
	{
		upslogx(LOG_WARNING, "Communications with UPS lost"
			" - check cabling");
		return;
	}

	va_start(ap, fmt);
	ret = vsnprintf(why, sizeof(why), fmt, ap);
	va_end(ap);

	if ((ret < 1) || (ret >= (int) sizeof(why)))
		upslogx(LOG_WARNING, "usb_comm_fail: vsnprintf needed "
			"more than %d bytes", sizeof(why));

	upslogx(LOG_WARNING, "Communications with UPS lost: %s", why);
}

void nutusb_comm_good(void)
{
	if (comm_failures == 0)
		return;

	upslogx(LOG_NOTICE, "Communications with UPS re-established");
	comm_failures = 0;
}
