/* lakeview_usb.h - driver for UPS with lakeview chipset, such as
   'Sweex Manageable UPS 1000VA' (ca. 2006)

   May also work on 'Kebo UPS-650D', not tested as of 05/23/2007

   Copyright (C) 2007 Peter van Valderen <p.v.valderen@probu.nl>
                      Dirk Teurlings <dirk@upexia.nl>

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

#include "main.h"
#include "libusb.h"
#include "usb-common.h"
#include "lakeview_usb.h"

static usb_device_id_t lakeview_usb_id[] = {
	/* Sweex 1000VA */
	{ USB_DEVICE(0x0925, 0x1234),  NULL },
	/* end of list */
	{-1, -1, NULL}
};

usb_dev_handle *upsdev = NULL;

static unsigned int	comm_failures = 0;

static int execute_and_retrieve_query(char *query, char *reply)
{
	int ret;

	ret = usb_control_msg(upsdev, STATUS_REQUESTTYPE, REQUEST_VALUE,
		MESSAGE_VALUE, INDEX_VALUE, query, sizeof(query), 1000);

	if (ret < 0) {
		upsdebug_with_errno(3, "send");
		return ret;
	}

	if (ret == 0) {
		upsdebugx(3, "send: timeout");
		return ret;
	}

	upsdebug_hex(3, "send", query, ret);

	ret = usb_interrupt_read(upsdev, REPLY_REQUESTTYPE, reply, sizeof(REPLY_PACKETSIZE), 1000);

	if (ret < 0) {
		upsdebug_with_errno(3, "read");
		return ret;
	}

	if (ret == 0) {
		upsdebugx(3, "read: timeout");
		return ret;
	}

	upsdebug_hex(3, "read", reply, ret);
	return ret;
}

static int query_ups (char *reply) {
        char buf[4];

        /*
         * This packet is a status request to the UPS
         */
        buf[0]=0x01;
        buf[1]=0x00;
        buf[2]=0x00;
        buf[3]=0x30;

        return execute_and_retrieve_query(buf, reply);
}

static void usb_open_error(const char *port)
{
        printf("Unable to find Lakeview UPS device on USB bus \n\n");
	
        printf("Things to try:\n\n");
        printf(" - Connect UPS device to USB bus\n\n");
        printf(" - Run this driver as another user (upsdrvctl -u or 'user=...' in ups.conf).\n");
        printf("   See upsdrvctl(8) and ups.conf(5).\n\n");
					
        fatalx(EXIT_FAILURE, "Fatal error: unusable configuration");
}

static void usb_comm_fail(const char *fmt, ...)
{
        int     ret;
        char    why[SMALLBUF];
        va_list ap;

        /* this means we're probably here because select was interrupted */
        if (exit_flag != 0)
                return;         /* ignored, since we're about to exit anyway */

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

static void usb_comm_good()
{
	if (comm_failures == 0)
		return;
		
	upslogx(LOG_NOTICE, "Communications with UPS re-established");	
	comm_failures = 0;
}

static usb_dev_handle *open_lakeview_usb()
{
        struct usb_bus *busses = usb_get_busses();
        struct usb_bus *bus;

        for (bus = busses; bus; bus = bus->next)
        {
                struct usb_device *dev;

                for (dev = bus->devices; dev; dev = dev->next) {
                        /* XXX Check for Lakeview USB compatible devices  */
                        if (dev->descriptor.bDeviceClass != USB_CLASS_PER_INTERFACE) {
                        	continue;
                        }

                        if (is_usb_device_supported(lakeview_usb_id,
                        		dev->descriptor.idVendor, dev->descriptor.idProduct) == SUPPORTED) {
                        	return usb_open(dev);
                        }
                }
        }

        return 0;
}

/*
 * Connect to the UPS
 */

static usb_dev_handle *open_ups(const char *port) {
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

                dev_h = open_lakeview_usb();
                if (!dev_h) {
                        upslogx(LOG_WARNING, "Can't open Lakeview USB device, retrying ...");
                        if (nanosleep(&t, NULL) < 0 && errno == EINTR)
                                break;
                }
        }

        if (!dev_h)
        {
                upslogx(LOG_ERR, "Can't open Lakeview USB device");
                goto errout;
        }

#if LIBUSB_HAS_DETACH_KRNL_DRV
        /* this method requires at least libusb 0.1.8:
         * it force device claiming by unbinding
         * attached driver... From libhid */
        retry = 3;
        while (usb_set_configuration(dev_h, 1) != 0 && retry-- > 0) {
//        while ((dev_claimed = usb_claim_interface(dev_h, 0)) != 0 && retry-- > 0) {
                upsdebugx(2, "Can't set Lakeview USB configuration, trying %d more time(s)...", retry);

                upsdebugx(2, "detaching kernel driver from USB device...");
                if (usb_detach_kernel_driver_np(dev_h, 0) < 0) {
                        upsdebugx(2, "failed to detach kernel driver from USB device...");
                }

                upsdebugx(2, "trying again to set USB configuration...");
        }

	if (retry < 3) {
		upsdebugx(2, "USB configuration successfully set");
	}
#else
        if (usb_set_configuration(dev_h, 1) < 0)
        {
                upslogx(LOG_ERR, "Can't set Lakeview USB configuration");
                goto errout;
        }
#endif

        if (usb_claim_interface(dev_h, 0) < 0)
	{
	        upslogx(LOG_ERR, "Can't claim Lakeview USB interface");
	        goto errout;
	}
	else
		dev_claimed = 1;

        if (usb_set_altinterface(dev_h, 0) < 0)
        {
                upslogx(LOG_ERR, "Can't set Lakeview USB alternate interface");
                goto errout;
        }

        if (usb_clear_halt(dev_h, 0x81) < 0)
        {
                upslogx(LOG_ERR, "Can't reset Lakeview USB endpoint");
                goto errout;
        }

        return dev_h;

errout:
        if (dev_h && dev_claimed)
                usb_release_interface(dev_h, 0);
        if (dev_h)
                usb_close(dev_h);

	

        usb_open_error(port);
        return 0;
}

static int close_ups(usb_dev_handle *dev_h, const char *port)
{
        if (dev_h)
        {
                usb_release_interface(dev_h, 0);
                return usb_close(dev_h);
        }
						
        return 0;
}


/*
 * Initialise the UPS
 */

void upsdrv_initups(void)
{
        char reply[REPLY_PACKETSIZE];
        int i;

	/* open the USB connection to the UPS */
        upsdev = open_ups("USB");

        /*
         * Read rubbish data a few times; the UPS doesn't seem to respond properly
         * the first few times after connecting
         */

        for (i=0;i<5;i++) {
                query_ups(reply);
                sleep(1);
        }
}

void upsdrv_cleanup(void)
{
        upslogx(LOG_ERR, "CLOSING\n");
        close_ups(upsdev, "USB");
}

static void usb_reconnect(void)
{

        upslogx(LOG_WARNING, "RECONNECT USB DEVICE\n");
	close_ups(upsdev, "USB");

        upsdev = NULL;
        sleep(3);
        upsdrv_initups();
}

void upsdrv_initinfo(void)
{
        dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);

        dstate_setinfo("ups.mfr", "Lakeview Research compatible");
        dstate_setinfo("ups.model","Unknown");
}

void upsdrv_updateinfo(void)
{
        char reply[REPLY_PACKETSIZE];
        int ret, online, battery_normal;

        ret = query_ups(reply);

        if (ret < 4) {
                usb_comm_fail("Query to UPS failed");
                dstate_datastale();

		/* reconnect the UPS */
                usb_reconnect();

                return;
        }

	usb_comm_good();

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
        }
        else {
            status_set("OB");
        }

        if (!battery_normal) {
            status_set("LB");
        }

        status_commit();
        dstate_dataok();
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
	char reply[REPLY_PACKETSIZE];
        char buf[4];

        /*
         * This packet shuts down the UPS, that is, if it is
         * not currently on line power
         */

        buf[0]=0x02;
        buf[1]=0x00;
        buf[2]=0x00;
        buf[3]=0x00;

        execute_and_retrieve_query(buf, reply);

	sleep(1); /* have to, the previous command seems to be
                   * ignored if the second command comes right
		   * behind it
                   */

	/*
	 * This should make the UPS turn itself back on once the
	 * power comes back on; which is probably what we want
	 */
	buf[0]=0x02;
	buf[1]=0x01;
	buf[2]=0x00;
	buf[3]=0x00;

	execute_and_retrieve_query(buf, reply);
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}

void upsdrv_banner(void)
{
        printf("Network UPS Tools - Lakeview Research compatible USB UPS driver %s (%s)\n\n",
                DRV_VERSION, UPS_VERSION);

	experimental_driver = 1;	
}
