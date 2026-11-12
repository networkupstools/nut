/* vida_no_usb_driver.c - the production VIDA driver and entry point, with
 * enumeration forced empty. This executable can never open a USB device.
 * Used only by the CLI shutdown regression; never installed.
 * Copyright 2026 NUT contributors. GPL version 2 or later.
 */
#include "config.h"
#define libusb_init no_usb_init
#define libusb_exit no_usb_exit
#define libusb_get_device_list no_usb_list
#define libusb_free_device_list no_usb_free
#define libusb_open no_usb_open
#define libusb_control_transfer no_usb_transfer
#include "nutdrv_vida.c"

int no_usb_init(libusb_context **context) { *context = NULL; return 0; }
void no_usb_exit(libusb_context *context) { NUT_UNUSED_VARIABLE(context); }
ssize_t no_usb_list(libusb_context *context, libusb_device ***list)
{
	NUT_UNUSED_VARIABLE(context);
	*list = NULL;
	return 0;
}
void no_usb_free(libusb_device **list, int unref)
{
	NUT_UNUSED_VARIABLE(list);
	NUT_UNUSED_VARIABLE(unref);
}
int no_usb_open(libusb_device *device, libusb_device_handle **handle)
{
	NUT_UNUSED_VARIABLE(device);
	NUT_UNUSED_VARIABLE(handle);
	abort();
}
int no_usb_transfer(libusb_device_handle *handle, uint8_t type, uint8_t request,
	uint16_t value, uint16_t index, unsigned char *data, uint16_t length,
	unsigned int timeout)
{
	NUT_UNUSED_VARIABLE(handle);
	NUT_UNUSED_VARIABLE(type);
	NUT_UNUSED_VARIABLE(request);
	NUT_UNUSED_VARIABLE(value);
	NUT_UNUSED_VARIABLE(index);
	NUT_UNUSED_VARIABLE(data);
	NUT_UNUSED_VARIABLE(length);
	NUT_UNUSED_VARIABLE(timeout);
	abort();
}
