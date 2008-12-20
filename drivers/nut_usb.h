#ifndef NUT_USB_H_SEEN
#define NUT_USB_H_SEEN 1

#include "attribute.h"

#include "config.h"
#include <usb.h>
#include "usb-common.h"

/* limit the amount of spew that goes in the syslog when we lose the UPS */
#define USB_ERR_LIMIT 10	/* start limiting after 10 in a row  */
#define USB_ERR_RATE 10		/* then only print every 10th error */

usb_dev_handle *nutusb_open(const char *port);

int nutusb_close(usb_dev_handle *dev_h, const char *port);

/* unified failure reporting: call these often */
void nutusb_comm_fail(const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 1, 2)));
void nutusb_comm_good(void);

/* function pointer, set depending on which device is used */
int (*usb_set_descriptor)(usb_dev_handle *udev, unsigned char type,
	unsigned char index, void *buf, int size);

#endif	/* NUT_USB_H_SEEN */
