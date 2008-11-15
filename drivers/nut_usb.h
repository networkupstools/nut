#ifndef NUT_USB_H_SEEN
#define NUT_USB_H_SEEN 1

#include "attribute.h"

#include "config.h"
#include <usb.h>
#include "usb-common.h"

/* FIXME: this has to be moved to bcmxcp_usb, 
 * in order to free nut_usb.h for general purpose use */ 

/* Powerware */
#define POWERWARE	0x0592

/* Phoenixtec Power Co., Ltd */
#define PHOENIXTEC	0x06da
 
/* USB IDs device table */
static usb_device_id pw_usb_device_table [] = {
	/* various models */
	{ USB_DEVICE(POWERWARE, 0x0002), NULL },

	/* various models */
	{ USB_DEVICE(PHOENIXTEC, 0x0002), NULL },
	
	/* Terminating entry */
	{ -1, -1, NULL }
};
/* end of FIXME */

/* limit the amount of spew that goes in the syslog when we lose the UPS */
#define USB_ERR_LIMIT 10	/* start limiting after 10 in a row  */
#define USB_ERR_RATE 10		/* then only print every 10th error */

usb_dev_handle *nutusb_open(const char *port);

int nutusb_close(usb_dev_handle *dev_h, const char *port);

/* unified failure reporting: call these often */
void nutusb_comm_fail(const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 1, 2)));
void nutusb_comm_good(void);

int usb_set_descriptor(usb_dev_handle *udev, unsigned char type,
		       unsigned char index, void *buf, int size);

#endif	/* NUT_USB_H_SEEN */
