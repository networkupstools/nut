#include "main.h"
#include "bcmxcp.h"
#include "bcmxcp_io.h"
#include "common.h"
#include "usb-common.h"
#include "timehead.h"
#include "nut_stdint.h" /* for uint16_t */
#include <ctype.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
/* libusb header file */
#ifdef WITH_LIBUSB_1_0
#include <libusb.h>
#endif
#ifdef WITH_LIBUSB_0_1
#include <usb.h>
#endif

#define SUBDRIVER_NAME    "USB communication subdriver"
#define SUBDRIVER_VERSION "0.26"

/* communication driver description structure */
upsdrv_info_t comm_upsdrv_info = {
	SUBDRIVER_NAME,
	SUBDRIVER_VERSION,
	NULL,
	0,
	{ NULL }
};

#define MAX_TRY 4

/* Powerware */
#define POWERWARE 0x0592

/* Phoenixtec Power Co., Ltd */
#define PHOENIXTEC 0x06da

/* Hewlett Packard */
#define HP_VENDORID 0x03f0

USBDevice_t curDevice;

#ifdef WITH_LIBUSB_1_0
 /* Simply remap libusb functions/structures from 0.1 to 1.0 */
 /* Structures */
 #define usb_dev_handle libusb_device_handle
 /* defines */
 #define USB_DT_STRING LIBUSB_DT_STRING
 #define USB_ENDPOINT_OUT LIBUSB_ENDPOINT_OUT
 #define USB_REQ_SET_DESCRIPTOR LIBUSB_REQUEST_SET_DESCRIPTOR
 #define USB_CLASS_PER_INTERFACE LIBUSB_CLASS_PER_INTERFACE
 /* Functions */
 #define usb_control_msg libusb_control_transfer
 static inline  int usb_interrupt_read(usb_dev_handle *dev, int ep,
        char *bytes, int size, int timeout)
 {
	int ret = libusb_interrupt_transfer(dev, ep, (unsigned char *) bytes,
			size, &size, timeout);
	/* In case of success, return the operation size, as done with libusb 0.1 */
	return (ret == LIBUSB_SUCCESS)?size:ret;
 }

 #define usb_claim_interface libusb_claim_interface
 #define usb_release_interface libusb_release_interface
 #define usb_reset libusb_reset_device
 #define usb_clear_halt libusb_clear_halt
 #define nut_usb_strerror(a) libusb_strerror(a)
#else
 #define nut_usb_strerror(a) usb_strerror()
#endif /* #ifdef WITH_LIBUSB_1_0 */

/* USB functions */
usb_dev_handle *nutusb_open(const char *port);
int nutusb_close(usb_dev_handle *dev_h, const char *port);
/* unified failure reporting: call these often */
void nutusb_comm_fail(const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 1, 2)));
void nutusb_comm_good(void);
/* function pointer, set depending on which device is used */
int (*usb_set_descriptor)(usb_dev_handle *udev, unsigned char type,
	unsigned char index, void *buf, int size);



/* usb_set_descriptor() for Powerware devices */
static int usb_set_powerware(usb_dev_handle *udev, unsigned char type, unsigned char index, void *buf, int size)
{
	return usb_control_msg(udev, USB_ENDPOINT_OUT, USB_REQ_SET_DESCRIPTOR, (type << 8) + index, 0, buf, size, 1000);
}

static void *powerware_ups(USBDevice_t *device) {
	usb_set_descriptor = &usb_set_powerware;
	return NULL;
}

/* usb_set_descriptor() for Phoenixtec devices */
static int usb_set_phoenixtec(usb_dev_handle *udev, unsigned char type, unsigned char index, void *buf, int size)
{
	return usb_control_msg(udev, 0x42, 0x0d, (0x00 << 8) + 0x0, 0, buf, size, 1000);
}

static void *phoenixtec_ups(USBDevice_t *device) {
	usb_set_descriptor = &usb_set_phoenixtec;
	return NULL;
}

/* USB IDs device table */
static usb_device_id_t pw_usb_device_table[] = {
	/* various models */
	{ USB_DEVICE(POWERWARE, 0x0002), &powerware_ups },

	/* various models */
	{ USB_DEVICE(PHOENIXTEC, 0x0002), &phoenixtec_ups },

	/* T500 */
	{ USB_DEVICE(HP_VENDORID, 0x1f01), &phoenixtec_ups },
	/* T750 */
	{ USB_DEVICE(HP_VENDORID, 0x1f02), &phoenixtec_ups },
	
	/* Terminating entry */
	{ -1, -1, NULL }
};

/* limit the amount of spew that goes in the syslog when we lose the UPS */
#define USB_ERR_LIMIT 10 /* start limiting after 10 in a row  */
#define USB_ERR_RATE 10  /* then only print every 10th error */
#define XCP_USB_TIMEOUT 5000

/* global variables */
usb_dev_handle *upsdev = NULL;
extern int exit_flag;
static unsigned int comm_failures = 0;

/* Functions implementations */
void send_read_command(unsigned char command)
{
	unsigned char buf[4];

	if (upsdev) {
		buf[0] = PW_COMMAND_START_BYTE;
		buf[1] = 0x01;                    /* data length */
		buf[2] = command;                 /* command to send */
		buf[3] = calc_checksum(buf);      /* checksum */
		upsdebug_hex (3, "send_read_command", buf, 4);
		usb_set_descriptor(upsdev, USB_DT_STRING, 4, buf, 4); /* FIXME: Ignore error */
	}
}

void send_write_command(unsigned char *command, int command_length)
{
	unsigned char sbuf[128];

	if (upsdev) {
		/* Prepare the send buffer */
		sbuf[0] = PW_COMMAND_START_BYTE;
		sbuf[1] = (unsigned char)(command_length);
		memcpy(sbuf+2, command, command_length);
		command_length += 2;

		/* Add checksum */
		sbuf[command_length] = calc_checksum(sbuf);
		command_length += 1;
		upsdebug_hex (3, "send_write_command", sbuf, command_length);
		usb_set_descriptor(upsdev, USB_DT_STRING, 4, sbuf, command_length);  /* FIXME: Ignore error */
	}
}

#define PW_HEADER_SIZE (PW_HEADER_LENGTH + 1)
#define PW_CMD_BUFSIZE	256
/* get the answer of a command from the ups. And check that the answer is for this command */
int get_answer(unsigned char *data, unsigned char command)
{
	unsigned char buf[PW_CMD_BUFSIZE], *my_buf = buf;
	int length, end_length, res, endblock, bytes_read, ellapsed_time, need_data;
	int tail;
	unsigned char block_number, sequence, seq_num;
	struct timeval start_time, now;

	if (upsdev == NULL)
		return -1;

	need_data = PW_HEADER_SIZE; /* 4 - cmd response header length, 1 for csum */
	end_length = 0;    /* total length of sequence(s), not counting header(s) */
	endblock = 0;      /* signal the last sequence in the block */
	bytes_read = 0;    /* total length of data read, including XCP header */
	res = 0;
	ellapsed_time = 0;
	seq_num = 1;       /* current theoric sequence */

	upsdebugx(1, "entering get_answer(%x)", command);

	/* Store current time */
	gettimeofday(&start_time, NULL);
	memset(&buf, 0x0, PW_CMD_BUFSIZE);

	while ( (!endblock) && ((XCP_USB_TIMEOUT - ellapsed_time)  > 0) ) {

		/* Get (more) data if needed */
		if (need_data > 0) {
			res = usb_interrupt_read(upsdev, 0x81, (char *) buf + bytes_read,
				128,
				(XCP_USB_TIMEOUT - ellapsed_time));

			/* Update time */
			gettimeofday(&now, NULL);
			ellapsed_time = (now.tv_sec - start_time.tv_sec)*1000 +
					(now.tv_usec - start_time.tv_usec)/1000;

			/* Check libusb return value */
			if (res < 0)
			{
				/* Clear any possible endpoint stalls */
				usb_clear_halt(upsdev, 0x81);
				/* continue; */ /* FIXME: seems a break would be better! */
				break;
			}

			/* this seems to occur on XSlot USB card */
			if (res == 0)
			{
				/* FIXME: */
				continue;
			}
			/* Else, we got some input bytes */
			bytes_read += res;
			need_data -= res;
			upsdebug_hex(1, "get_answer", buf, bytes_read);
		}

		if (need_data > 0) /* We need more data */
		    continue;

		/* Now validate XCP frame */
		/* Check header */
		if ( my_buf[0] != PW_COMMAND_START_BYTE ) {
			upsdebugx(2, "get_answer: wrong header 0xab vs %02x", my_buf[0]);
			/* Sometime we read something wrong. bad cables? bad ports? */
			my_buf = memchr(my_buf, PW_COMMAND_START_BYTE, bytes_read);
			if (!my_buf)
			    return -1;
		}

		/* Read block number byte */
		block_number = my_buf[1];
		upsdebugx(1, "get_answer: block_number = %x", block_number);

		/* Check data length byte (remove the header length) */
		length = my_buf[2];
		upsdebugx(3, "get_answer: data length = %d", length);
		if (bytes_read - (length + PW_HEADER_SIZE) < 0) {
			if (need_data < 0) --need_data; /* count zerro byte too */
			need_data += length + 1; /* packet lenght + checksum */
			upsdebugx(2, "get_answer: need to read %d more data", need_data);
			continue;
		}
		/* Check if Length conforms to XCP (121 for normal, 140 for Test mode) */
		/* Use the more generous length for testing */
		if (length > 140) {
			upsdebugx(2, "get_answer: bad length");
			return -1;
		}

		/* Test the Sequence # */
		sequence = my_buf[3];
		if ((sequence & PW_SEQ_MASK) != seq_num) {
			nutusb_comm_fail("get_answer: not the right sequence received %x!!!\n", (sequence & PW_SEQ_MASK));
			return -1;
		}
		else {
			upsdebugx(2, "get_answer: sequence number (%x) is ok", (sequence & PW_SEQ_MASK));
		}

		/* Validate checksum */
		if (!checksum_test(my_buf)) {
			nutusb_comm_fail("get_answer: checksum error! ");
			return -1;
		}
		else {
			upsdebugx(2, "get_answer: checksum is ok");
		}

		/* Check if it's the last sequence */
		if (sequence & PW_LAST_SEQ) {
			/* we're done receiving data */
			upsdebugx(2, "get_answer: all data received");
			endblock = 1;
		}
		else {
			seq_num++;
			upsdebugx(2, "get_answer: next sequence is %d", seq_num);
		}

		/* copy the current valid XCP frame back */
		memcpy(data+end_length, my_buf + 4, length);
		/* increment pointers to process the next sequence */
		end_length += length;
		tail = bytes_read - (length + PW_HEADER_SIZE);
		if (tail > 0)
		    my_buf = memmove(&buf[0], my_buf + length + PW_HEADER_SIZE, tail);
		else if (tail == 0)
		    my_buf = &buf[0];
		bytes_read = tail;
	}

	upsdebug_hex (5, "get_answer", data, end_length);
	return end_length;
}

/* Sends a single command (length=1). and get the answer */
int command_read_sequence(unsigned char command, unsigned char *data)
{
	int bytes_read = 0;
	int retry = 0;
	
	while ((bytes_read < 1) && (retry < 5)) {
		send_read_command(command);
		bytes_read = get_answer(data, command);
		retry++;
	}

	if (bytes_read < 1) {
		nutusb_comm_fail("Error executing command");
		dstate_datastale();
		return -1;
	}

	return bytes_read;
}

/* Sends a setup command (length > 1) */
int command_write_sequence(unsigned char *command, int command_length, unsigned char *answer)
{
	int bytes_read = 0;
	int retry = 0;

	while ((bytes_read < 1) && (retry < 5)) {
		send_write_command(command, command_length);
		sleep(PW_SLEEP);
		bytes_read = get_answer(answer, command[0]);
		retry ++;
	}

	if (bytes_read < 1) {
		nutusb_comm_fail("Error executing command");
		dstate_datastale();
		return -1;
	}

	return bytes_read;
}


void upsdrv_comm_good(void)
{
	nutusb_comm_good();
}

void upsdrv_initups(void)
{
	upsdev = nutusb_open("USB");
}

void upsdrv_cleanup(void)
{
	upslogx(LOG_ERR, "CLOSING\n");
	nutusb_close(upsdev, "USB");
	free(curDevice.Vendor);
	free(curDevice.Product);
	free(curDevice.Serial);
	free(curDevice.Bus);
}

void upsdrv_reconnect(void)
{
	upsdebugx(4, "==================================================");
	upsdebugx(4, "= device has been disconnected, try to reconnect =");
	upsdebugx(4, "==================================================");

	nutusb_close(upsdev, "USB");
	upsdev = NULL;
	upsdrv_initups();
}

/* USB functions */
static void nutusb_open_error(const char *port)
{
	printf("Unable to find POWERWARE UPS device on USB bus (%s)\n\n", port);

	printf("Things to try:\n\n");
	printf(" - Connect UPS device to USB bus\n\n");
	printf(" - Run this driver as another user (upsdrvctl -u or 'user=...' in ups.conf).\n");
	printf("   See upsdrvctl(8) and ups.conf(5).\n\n");

	fatalx(EXIT_FAILURE, "Fatal error: unusable configuration");
}

/* FIXME: this part of the opening can go into common... */
static usb_dev_handle *open_powerware_usb(void)
{
#ifdef WITH_LIBUSB_1_0
	libusb_device **devlist;
	ssize_t devcount = 0;
	libusb_device_handle *udev;
	struct libusb_device_descriptor dev_desc;
	uint8_t bus;
	int i;

	devcount = libusb_get_device_list(NULL, &devlist);
	if (devcount <= 0)
		fatal_with_errno(EXIT_FAILURE, "No USB device found");

	for (i = 0; i < devcount; i++) {

		libusb_device *device = devlist[i];
		libusb_get_device_descriptor(device, &dev_desc);

		if (dev_desc.bDeviceClass != LIBUSB_CLASS_PER_INTERFACE) {
			continue;
		}

		curDevice.VendorID = dev_desc.idVendor;
		curDevice.ProductID = dev_desc.idProduct;
		bus = libusb_get_bus_number(device);
		curDevice.Bus = (char *)malloc(4);
		if (curDevice.Bus == NULL) {
			libusb_free_device_list(devlist, 1);
			fatal_with_errno(EXIT_FAILURE, "Out of memory");
		}
		sprintf(curDevice.Bus, "%03d", bus);

		/* FIXME: we should also retrieve
		 * dev->descriptor.iManufacturer
		 * dev->descriptor.iProduct
		 * dev->descriptor.iSerialNumber
		 * as in libusb.c->libusb_open()
		 * This is part of the things to put in common... */

		if (is_usb_device_supported(pw_usb_device_table, &curDevice) == SUPPORTED) {
			libusb_open(device, &udev);
			libusb_free_device_list(devlist, 1);
			return udev;
		}
	}
	libusb_free_device_list(devlist, 1);
#else
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

			curDevice.VendorID = dev->descriptor.idVendor;
			curDevice.ProductID = dev->descriptor.idProduct;
			curDevice.Bus = xstrdup(bus->dirname);

			/* FIXME: we should also retrieve
			 * dev->descriptor.iManufacturer
			 * dev->descriptor.iProduct
			 * dev->descriptor.iSerialNumber
			 * as in libusb.c->libusb_open()
			 * This is part of the things to put in common... */

			if (is_usb_device_supported(pw_usb_device_table, &curDevice) == SUPPORTED) {
				return usb_open(dev);
			}
		}
	}
#endif
	return 0;
}

usb_dev_handle *nutusb_open(const char *port)
{
	int            dev_claimed = 0;
	usb_dev_handle *dev_h = NULL;
	int            retry, errout = 0, ret;

	upsdebugx(1, "entering nutusb_open()");

	/* Initialize Libusb */
#ifdef WITH_LIBUSB_1_0
	if (libusb_init(NULL) < 0) {
		libusb_exit(NULL);
		fatal_with_errno(EXIT_FAILURE, "Failed to init libusb 1.0");
	}
#else
	usb_init();
	usb_find_busses();
	usb_find_devices();
#endif /* WITH_LIBUSB_1_0 */

	for (retry = 0; retry < MAX_TRY ; retry++)
	{
		dev_h = open_powerware_usb();
		if (!dev_h) {
			upsdebugx(1, "Can't open POWERWARE USB device");
			errout = 1;
		}
		else {
			upsdebugx(1, "device %s opened successfully", curDevice.Bus);
			errout = 0;

			if ((ret = usb_claim_interface(dev_h, 0)) < 0)
			{
				upsdebugx(1, "Can't claim POWERWARE USB interface: %s", nut_usb_strerror(ret));
				errout = 1;
			}
			else {
				dev_claimed = 1;
				errout = 0;
			}
/* FIXME: the above part of the opening can go into common... up to here at least */

			if ((ret = usb_clear_halt(dev_h, 0x81)) < 0)
			{
				upsdebugx(1, "Can't reset POWERWARE USB endpoint: %s", nut_usb_strerror(ret));
				if (dev_claimed)
				    usb_release_interface(dev_h, 0);
				usb_reset(dev_h);
				sleep(5);	/* Wait reconnect */
				errout = 1;
			}
			else
				errout = 0;
		}

		/* Test if we succeeded */
		if ( (dev_h != NULL) && dev_claimed && (errout == 0) )
			break;
		else {
			/* Clear errors, and try again */
			errout = 0;
		}
	}

	if (!dev_h && !dev_claimed && retry == MAX_TRY)
		errout = 1;
	else
		return dev_h;

	if (dev_h && dev_claimed)
		usb_release_interface(dev_h, 0);

	nutusb_close(dev_h, port);

	if (errout == 1)
		nutusb_open_error(port);

	return NULL;
}

/* FIXME: this part can go into common... */
int nutusb_close(usb_dev_handle *dev_h, const char *port)
{
	int ret = 0;

	if (dev_h)
	{
		usb_release_interface(dev_h, 0);
#ifdef WITH_LIBUSB_1_0
		libusb_close(dev_h);
		libusb_exit(NULL);
#else
		ret = usb_close(dev_h);
#endif
	}
	return ret;
}

void nutusb_comm_fail(const char *fmt, ...)
{
	int ret;
	char why[SMALLBUF];
	va_list ap;

	/* this means we're probably here because select was interrupted */
	if (exit_flag != 0)
		return; /* ignored, since we're about to exit anyway */

	comm_failures++;

	if ((comm_failures == USB_ERR_LIMIT) ||
		((comm_failures % USB_ERR_RATE) == 0))
	{
		upslogx(LOG_WARNING, "Warning: excessive comm failures, "
			"limiting error reporting");
	}

	/* once it's past the limit, only log once every USB_ERR_LIMIT calls */
	if ((comm_failures > USB_ERR_LIMIT) &&
		((comm_failures % USB_ERR_LIMIT) != 0)) {
		/* Try reconnection */
		upsdebugx(1, "Got to reconnect!\n");
		upsdrv_reconnect();
		return;
	}

	/* generic message if the caller hasn't elaborated */
	if (!fmt)
	{
		upslogx(LOG_WARNING, "Communications with UPS lost - check cabling");
		return;
	}

	va_start(ap, fmt);
	ret = vsnprintf(why, sizeof(why), fmt, ap);
	va_end(ap);

	if ((ret < 1) || (ret >= (int) sizeof(why)))
		upslogx(LOG_WARNING, "usb_comm_fail: vsnprintf needed "
			"more than %d bytes", (int)sizeof(why));

	upslogx(LOG_WARNING, "Communications with UPS lost: %s", why);
}

void nutusb_comm_good(void)
{
	if (comm_failures == 0)
		return;

	upslogx(LOG_NOTICE, "Communications with UPS re-established");
	comm_failures = 0;
}

