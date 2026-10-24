/* CyberPower HID text transport, documented from observed USB traffic.
 * Initial support is deliberately limited to OLS2000ERT2UA queries.
 * No vendor implementation is included. GNU GPL version 2 or later.
 *
 * Observed framing: 64-byte interrupt OUT packets on endpoint 0x02 start
 * with report 0x29 and a payload byte count, then CR-terminated text and
 * zero padding. Endpoint 0x81 replies use report 0x28 and the same length
 * prefix. A reply can span packets, even splitting a numeric field.
 * Feature reports 0x25 and 0x27 perform the observed communication setup.
 * The application also sends C* messages; those are intentionally excluded
 * from this initial query-only transport.
 */
#include "main.h"
#include "powerp-usb.h"

#if WITH_LIBUSB_1_0
#include "usb-common.h"

/* Cyber Power Systems */
#define POWERPANEL_VENDORID 0x0764

static usb_device_id_t powerpanel_usb_device_table[] = {
	{ USB_DEVICE(POWERPANEL_VENDORID, 0x0601), NULL },
	{ 0, 0, NULL }
};

static libusb_context *context;
static libusb_device_handle *handle;
static int claimed, detached;

static int feature(unsigned int id, unsigned char value)
{
	unsigned char data[2];
	data[0] = (unsigned char)id;
	data[1] = value;
	return libusb_control_transfer(handle, 0x21, 9, (uint16_t)(0x300 | id),
		0, data, sizeof(data), 1000) == 2 ? 0 : -1;
}

int powerpanel_usb_connected(void)
{
	return handle != NULL;
}

void powerpanel_usb_cleanup(void)
{
	if (handle) {
		if (claimed) {
			libusb_release_interface(handle, 0);
		}
		if (detached) {
			libusb_attach_kernel_driver(handle, 0);
		}
		libusb_close(handle);
		handle = NULL;
	}
	if (context) {
		libusb_exit(context);
		context = NULL;
	}
	claimed = detached = 0;
}

int powerpanel_usb_init(void)
{
	libusb_device **devices = NULL;
	libusb_device *candidate = NULL;
	struct libusb_device_descriptor descriptor;
	unsigned char model[128], packet[64];
	ssize_t count, i;
	int matches = 0, result, length;

	powerpanel_usb_cleanup();
	if (libusb_init(&context) != 0) {
		return -1;
	}
	count = libusb_get_device_list(context, &devices);
	if (count < 0) {
		goto failed;
	}
	for (i = 0; i < count; i++) {
		if (libusb_get_device_descriptor(devices[i], &descriptor) == 0
		&& descriptor.idVendor == powerpanel_usb_device_table[0].vendorID
		&& descriptor.idProduct == powerpanel_usb_device_table[0].productID) {
			candidate = devices[i];
			matches++;
		}
	}
	/* Do not choose an arbitrary UPS if more than one shares this USB ID. */
	if (matches != 1 || libusb_open(candidate, &handle) != 0) {
		upslogx(LOG_WARNING, "USB text transport requires exactly one accessible 0764:0601 UPS");
		goto failed;
	}
	if (libusb_get_device_descriptor(candidate, &descriptor) != 0) {
		goto failed;
	}
	result = libusb_get_string_descriptor_ascii(handle, descriptor.iProduct, model, sizeof(model)-1);
	if (result < 1) {
		goto failed;
	}
	model[result] = '\0';
	if (strcmp((char *)model, "OLS2000ERT2UA") != 0) {
		upslogx(LOG_WARNING, "USB text transport has only been validated for OLS2000ERT2UA");
		goto failed;
	}
	if (libusb_kernel_driver_active(handle, 0) == 1) {
		if (libusb_detach_kernel_driver(handle, 0) != 0) {
			goto failed;
		}
		detached = 1;
	}
	if (libusb_claim_interface(handle, 0) != 0) {
		goto failed;
	}
	claimed = 1;
	/* Observed communication-mode setup, not an electrical output command. */
	if (feature(0x25, 1) < 0) {
		goto failed;
	}
	for (i = 0; i < 4; i++) {
		result = libusb_interrupt_transfer(handle, 0x81, packet, sizeof(packet), &length, 100);
		if (result == LIBUSB_ERROR_TIMEOUT) {
			break;
		}
		if (result != 0) {
			goto failed;
		}
	}
	if (feature(0x27, 0) < 0) {
		goto failed;
	}
	usleep(400000);
	if (feature(0x27, 1) < 0) {
		goto failed;
	}
	usleep(400000);
	libusb_free_device_list(devices, 1);
	return 0;

failed:
	if (devices) {
		libusb_free_device_list(devices, 1);
	}
	powerpanel_usb_cleanup();
	return -1;
}

static int query_allowed(const char *command)
{
	const char *queries[] = {"D\r", "P1\r", "P2\r", "P3\r", "P4\r", "P5\r",
		"P6\r", "P7\r", "P8\r", "P9\r", "\r", "\r\r", NULL};
	size_t i;
	for (i = 0; queries[i]; i++) {
		if (strcmp(command, queries[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

ssize_t powerpanel_usb_command(const char *command, char *reply, size_t size)
{
	unsigned char packet[64];
	size_t used = 0, i, command_length;
	int result, transferred = 0, attempt;
	if (!handle || !command || !reply || size < 2 || !query_allowed(command)) {
		return -1;
	}
	command_length = strlen(command);
	if (command_length > sizeof(packet)-2) {
		return -1;
	}
	memset(packet, 0, sizeof(packet));
	packet[0] = 0x29;
	packet[1] = (unsigned char)command_length;
	memcpy(packet+2, command, command_length);
	result = libusb_interrupt_transfer(handle, 0x02, packet, sizeof(packet), &transferred, 1000);
	if (result != 0 || transferred != (int)sizeof(packet)) {
		return -1;
	}
	/* Bound the complete operation: no unlimited wait for a terminator. */
	for (attempt = 0; attempt < 8; attempt++) {
		transferred = 0;
		result = libusb_interrupt_transfer(handle, 0x81, packet, sizeof(packet), &transferred, 250);
		if (result == LIBUSB_ERROR_TIMEOUT && transferred == 0) {
			continue;
		}
		if (result != 0 && result != LIBUSB_ERROR_TIMEOUT) {
			return -1;
		}
		if (transferred < 2 || packet[0] != 0x28) {
			continue;
		}
		if (packet[1] > sizeof(packet)-2 || (int)packet[1]+2 > transferred) {
			return -1;
		}
		for (i = 0; i < packet[1]; i++) {
			unsigned char byte = packet[i+2];
			if (byte == '\r') {
				if (i+1 != packet[1]) {
					return -1;
				}
				reply[used] = '\0';
				return (ssize_t)used;
			}
			if (used >= size-1) {
				return -1;
			}
			reply[used++] = (char)byte;
		}
	}
	return -1;
}
#endif
