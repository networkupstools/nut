/*
 * riello_usb.c: support for Riello USB protocol based UPSes
 *
 * A document describing the protocol implemented by this driver can be
 * found online at:
 *
 *   https://www.networkupstools.org/protocols/riello/PSGPSER-0104.pdf
 *
 * Copyright (C) 2012 - Elio Parisi <e.parisi@riello-ups.com>
 * Copyright (C) 2016   Eaton
 * Copyright (C) 2022-2024 "amikot"
 * Copyright (C) 2022-2024 Jim Klimov <jimklimov+nut@gmail.com>
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
 *
 * Reference of the derivative work: blazer driver
 */

#include "config.h" /* must be the first header */

#include "main.h"
#include "nut_libusb.h"
#include "usb-common.h"
#include "riello.h"

#define DRIVER_NAME	"Riello USB driver"
#define DRIVER_VERSION	"0.14"

#define DEFAULT_OFFDELAY   5  /*!< seconds (max 0xFF) */
#define DEFAULT_BOOTDELAY  5  /*!< seconds (max 0xFF) */

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Elio Parisi <e.parisi@riello-ups.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

static uint8_t bufOut[BUFFER_SIZE];
static uint8_t bufIn[BUFFER_SIZE];

static uint8_t gpser_error_control;

static uint8_t input_monophase;
static uint8_t output_monophase;

/*! Time in seconds to delay before shutting down. */
static unsigned int offdelay = DEFAULT_OFFDELAY;
static unsigned int bootdelay = DEFAULT_BOOTDELAY;

static TRielloData DevData;

static usb_communication_subdriver_t *usb = &usb_subdriver;
static usb_dev_handle *udev = NULL;
static USBDevice_t usbdevice;
static USBDeviceMatcher_t *reopen_matcher = NULL;
static USBDeviceMatcher_t *regex_matcher = NULL;

/* Flag for estimation of battery.runtime and battery.charge */
static int localcalculation = 0;
static int localcalculation_logged = 0;
/* NOTE: Do not change these default, they refer to battery.voltage.nominal=12.0
 * and used in related maths later */
static double batt_volt_nom = 12.0;
static double batt_volt_low = 10.4;
static double batt_volt_high = 13.0;

static int (*subdriver_command)(uint8_t *cmd, uint8_t *buf, uint16_t length, uint16_t buflen) = NULL;

static void ussleep(useconds_t usec)
{

	if (usec == 1)
		usec = 400;
	else
		usec *= 1000;

	usleep(usec);
}

static int cypress_setfeatures(void)
{
	int ret;

	bufOut[0] = 0xB0;
	bufOut[1] = 0x4;
	bufOut[2] = 0x0;
	bufOut[3] = 0x0;
	bufOut[4] = 0x3;

	/* Write features report */
	ret = usb_control_msg(udev, USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
		0x09,				/* HID_REPORT_SET = 0x09 */
		0 + (0x03 << 8),		/* HID_REPORT_TYPE_FEATURE */
		0, (usb_ctrl_charbuf) bufOut, 0x5, 1000);

	if (ret <= 0) {
		upsdebugx(3, "send: %s", ret ? nut_usb_strerror(ret) : "error");
		return ret;
	}

	upsdebugx(3, "send: features report ok");
	return ret;
}

static int Send_USB_Packet(uint8_t *send_str, uint16_t numbytes)
{
	uint8_t USB_buff_pom[10];
	int i, err, size;
	/*int errno;*/

	/* is input correct ? */
	if ((!send_str) || (!numbytes))
		return -1;

	size = 7;

	/* routine which parse report into 4-bytes packet */
	for (i=0; i<(numbytes/size); i++) {
		USB_buff_pom[0] = 0x37;
		USB_buff_pom[1] = send_str[(i*7)];
		USB_buff_pom[2] = send_str[(i*7)+1];
		USB_buff_pom[3] = send_str[(i*7)+2];
		USB_buff_pom[4] = send_str[(i*7)+3];
		USB_buff_pom[5] = send_str[(i*7)+4];
		USB_buff_pom[6] = send_str[(i*7)+5];
		USB_buff_pom[7] = send_str[(i*7)+6];

		err = usb_bulk_write(udev, 0x2, (usb_ctrl_charbuf) USB_buff_pom, 8, 1000);

		if (err < 0) {
			upsdebug_with_errno(3, "USB: Send_USB_Packet: send_usb_packet, err = %d %s ", err, nut_usb_strerror(err));
			return err;
		}
		ussleep(USB_WRITE_DELAY);
	}

	/* send rest of packet */
	if (numbytes % size) {
		i = numbytes/size;
		memset(USB_buff_pom, '0', sizeof(USB_buff_pom));

		USB_buff_pom[0] = 0x30+(numbytes%7);
		if ((i*7)<numbytes)
			USB_buff_pom[1] = send_str[(i*7)];
		if (((i*7)+1)<numbytes)
			USB_buff_pom[2] = send_str[(i*7)+1];
		if (((i*7)+2)<numbytes)
			USB_buff_pom[3] = send_str[(i*7)+2];
		if (((i*7)+3)<numbytes)
			USB_buff_pom[4] = send_str[(i*7)+3];
		if (((i*7)+4)<numbytes)
			USB_buff_pom[5] = send_str[(i*7)+4];
		if (((i*7)+5)<numbytes)
			USB_buff_pom[6] = send_str[(i*7)+5];
		if (((i*7)+6)<numbytes)
			USB_buff_pom[7] = send_str[(i*7)+6];

		err = usb_bulk_write(udev, 0x2, (usb_ctrl_charbuf) USB_buff_pom, 8, 1000);

		if (err < 0) {
			upsdebug_with_errno(3, "USB: Send_USB_Packet: send_usb_packet, err = %d %s ", err, nut_usb_strerror(err));
			return err;
		}
		ussleep(USB_WRITE_DELAY);
	}
	return (0);
}

static int Get_USB_Packet(uint8_t *buffer)
{
	char inBuf[10];
	int err, ep;
	size_t size;
	/*int errno;*/

	/* note: this function stop until some byte(s) is not arrived */
	size = 8;

	/* Note: depending on libusb API version, size is either int or uint16_t
	 * either way, likely less than size_t limit. But we don't assign much.
	 */
	ep = 0x81 | USB_ENDPOINT_IN;
	err = usb_bulk_read(udev, ep, (usb_ctrl_charbuf) inBuf, (int)size, 1000);

	if (err > 0)
		upsdebugx(3, "read: %02X %02X %02X %02X %02X %02X %02X %02X", inBuf[0], inBuf[1], inBuf[2], inBuf[3], inBuf[4], inBuf[5], inBuf[6], inBuf[7]);

	if (err < 0){
		upsdebug_with_errno(3, "USB: Get_USB_Packet: send_usb_packet, err = %d %s ", err, nut_usb_strerror(err));
		return err;
	}

	/* copy to buffer */
	size = (unsigned char)(inBuf[0]) & 0x07;
	if (size)
		memcpy(buffer, &inBuf[1], size);

	if (size > INT_MAX)
		return -1;

	return (int)size;
}

static int cypress_command(uint8_t *buffer, uint8_t *buf, uint16_t length, uint16_t buflen)
{
	int loop = 0;
	int	ret, i = 0;
	uint8_t USB_buff[BUFFER_SIZE];

	/* read to flush buffer */
	riello_init_serial();

	/* send packet */
	ret = Send_USB_Packet(buffer, length);

	if (ret < 0) {
		upsdebugx(3, "Cypress_command send: err %d", ret );
		return ret;
	}

	upsdebugx(3, "send ok");

	memset(buf, 0, buflen);

	while ((buf_ptr_length < BUFFER_SIZE) && wait_packet) {

		memset(USB_buff, 0, sizeof(USB_buff));
		ret = Get_USB_Packet(USB_buff);

		/*
		 * Any errors here mean that we are unable to read a reply (which
		 * will happen after successfully writing a command to the UPS)
		 */
		if (ret < 0) {
			upsdebugx(3, "Cypress_command read: err %d", ret );
			return ret;
		}

		for (i = 0; i < ret; i++ ) {
			commbyte = USB_buff[i];
			riello_parse_serialport(DEV_RIELLOGPSER, buf, gpser_error_control);
		}

		loop++;
		if (loop>300){
			wait_packet=0;
			upsdebugx(1, "wait_packet reset");
		}

		ussleep(10);
	}

	upsdebugx(3, "in read: %u", buf_ptr_length);

	return buf_ptr_length;
}

static void *cypress_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &cypress_command;
	return NULL;
}

/* Riello (Cypress Semiconductor Corp.) */
#define RIELLO_VENDORID 0x04b4

static usb_device_id_t riello_usb_id[] = {
	/* various models */
	{ USB_DEVICE(RIELLO_VENDORID, 0x5500), &cypress_subdriver },

	/* Terminating entry */
	{ 0, 0, NULL }
};


static int device_match_func(USBDevice_t *hd, void *privdata)
{
	NUT_UNUSED_VARIABLE(privdata);

	if (subdriver_command) {
		return 1;
	}

	switch (is_usb_device_supported(riello_usb_id, hd))
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


/*
 * Callback that is called by usb_device_open() that handles USB device
 * settings prior to accepting the devide. At the very least claim the
 * device here. Detaching the kernel driver will be handled by the
 * caller, don't do this here. Return < 0 on error, 0 or higher on
 * success.
 */
static int driver_callback(usb_dev_handle *handle, USBDevice_t *device, usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen)
{
	int ret = 0;
	NUT_UNUSED_VARIABLE(device);
	NUT_UNUSED_VARIABLE(rdbuf);
	NUT_UNUSED_VARIABLE(rdlen);

/*
	if ((ret = usb_set_configuration(handle, 1)) < 0) {
		upslogx(LOG_WARNING, "Can't set USB configuration: %s", nut_usb_strerror(ret));
		return -1;
	}
*/

	if ((ret = usb_claim_interface(handle, 0)) < 0) {
		upslogx(LOG_WARNING, "Can't claim USB interface: %s", nut_usb_strerror(ret));
		return -1;
	}

	/* TODO: HID SET_IDLE to 0 (not necessary?) */

	return 1;
}

/*
 * Generic command processing function. Send a command and read a reply.
 * Returns < 0 on error, 0 on timeout and the number of bytes read on
 * success.
 */
static int riello_command(uint8_t *cmd, uint8_t *buf, uint16_t length, uint16_t buflen)
{
	int ret;

	if (udev == NULL) {
		dstate_setinfo("driver.state", "reconnect.trying");

		ret = usb->open_dev(&udev, &usbdevice, reopen_matcher, &driver_callback);

		upsdebugx (3, "riello_command err udev NULL : %d ", ret);
		if (ret < 0)
			return ret;

		dstate_setinfo("driver.state", "reconnect.updateinfo");
		upsdrv_initinfo();	/* reconnect usb cable */
		dstate_setinfo("driver.state", "quiet");
	}

	ret = (*subdriver_command)(cmd, buf, length, buflen);
	if (ret >= 0) {
		upsdebugx (3, "riello_command ok: %d", ret);
		return ret;
	}

	upsdebugx (3, "riello_command err: %d", ret);

	switch (ret)
	{
	case LIBUSB_ERROR_BUSY:			/* Device or resource busy */
		fatal_with_errno(EXIT_FAILURE, "Got disconnected by another driver");
#ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif

#if WITH_LIBUSB_0_1 /* limit to libusb 0.1 implementation */
	case -EPERM:				/* Operation not permitted */
		fatal_with_errno(EXIT_FAILURE, "Permissions problem");
# ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
# endif
#endif

	case LIBUSB_ERROR_PIPE:			/* Broken pipe */
		if (usb_clear_halt(udev, 0x81) == 0) {
			upsdebugx(1, "Stall condition cleared");
			break;
		}
#if (defined ETIME) && ETIME && WITH_LIBUSB_0_1
		goto fallthrough_case_etime;
	case -ETIME:				/* Timer expired */
	fallthrough_case_etime:
#endif
		if (usb_reset(udev) == 0) {
			upsdebugx(1, "Device reset handled");
		}
		goto fallthrough_case_reconnect;
	case LIBUSB_ERROR_NO_DEVICE: /* No such device */
	case LIBUSB_ERROR_ACCESS:    /* Permission denied */
	case LIBUSB_ERROR_IO:        /* I/O error */
#if WITH_LIBUSB_0_1 /* limit to libusb 0.1 implementation */
	case -ENXIO:				/* No such device or address */
#endif
	case LIBUSB_ERROR_NOT_FOUND:		/* No such file or directory */
	fallthrough_case_reconnect:
		/* Uh oh, got to reconnect! */
		dstate_setinfo("driver.state", "reconnect.trying");
		usb->close_dev(udev);
		udev = NULL;
		break;

	case LIBUSB_ERROR_TIMEOUT:  /* Connection timed out */
		upsdebugx (3, "riello_command err: Resource temporarily unavailable");
		break;

#ifndef WIN32
/* libusb win32 does not know EPROTO and EOVERFLOW,
 * it only returns EIO for any IO errors */
	case LIBUSB_ERROR_OVERFLOW: /* Value too large for defined data type */
# if EPROTO && WITH_LIBUSB_0_1
	case -EPROTO:		/* Protocol error */
# endif
		break;
#endif	/* !WIN32 */

	default:
		break;
	}

	return ret;
}

static int get_ups_nominal(void)
{

	uint8_t length;
	int recv;

	length = riello_prepare_gn(&bufOut[0], gpser_error_control);

	recv = riello_command(&bufOut[0], &bufIn[0], length, LENGTH_GN);

	if (recv < 0){
		upsdebugx (3, "Get nominal err: read byte: %d", recv);
		return recv;
	}

	if (!wait_packet && foundbadcrc) {
		upsdebugx (3, "Get nominal Ko: bad CRC or Checksum");
		return -1;
	}

	/* mandatory */
	if (!wait_packet && foundnak) {
		upsdebugx (3, "Get nominal Ko: command not supported");
		return -1;
	}

	upsdebugx (3, "Get nominal Ok: read byte: %d", recv);

	riello_parse_gn(&bufIn[0], &DevData);

	return 0;
}

static int get_ups_status(void)
{
	uint8_t numread, length;
	int recv;

	length = riello_prepare_rs(&bufOut[0], gpser_error_control);

	if (input_monophase)
		numread = LENGTH_RS_MM;
	else if (output_monophase)
		numread = LENGTH_RS_TM;
	else
		numread = LENGTH_RS_TT;

	recv = riello_command(&bufOut[0], &bufIn[0], length, numread);

	if (recv < 0){
		upsdebugx (3, "Get status err: read byte: %d", recv);
		return recv;
	}

	if (!wait_packet && foundbadcrc) {
		upsdebugx (3, "Get status Ko: bad CRC or Checksum");
		return -1;
	}

	/* mandatory */
	if (!wait_packet && foundnak) {
		upsdebugx (3, "Get status Ko: command not supported");
		return -1;
	}

	upsdebugx (3, "Get status Ok: read byte: %d", recv);

	riello_parse_rs(&bufIn[0], &DevData, numread);

	return 0;
}

static int get_ups_extended(void)
{
	uint8_t length;
	int recv;

	length = riello_prepare_re(&bufOut[0], gpser_error_control);

	recv = riello_command(&bufOut[0], &bufIn[0], length, LENGTH_RE);

	if (recv < 0){
		upsdebugx (3, "Get extended err: read byte: %d", recv);
		return recv;
	}

	if (!wait_packet && foundbadcrc) {
		upsdebugx (3, "Get extended Ko: bad CRC or Checksum");
		return -1;
	}

	/* optional */
	if (!wait_packet && foundnak) {
		upsdebugx (3, "Get extended Ko: command not supported");
		return 0;
	}

	upsdebugx (3, "Get extended Ok: read byte: %d", recv);

	riello_parse_re(&bufIn[0], &DevData);

	return 0;
}

/* Not static, exposed via header. Not used though, currently... */
int get_ups_statuscode(void)
{
	uint8_t length;
	int recv;

	length = riello_prepare_rc(&bufOut[0], gpser_error_control);

	recv = riello_command(&bufOut[0], &bufIn[0], length, LENGTH_RC);

	if (recv < 0){
		upsdebugx (3, "Get statuscode err: read byte: %d", recv);
		return recv;
	}

	if (!wait_packet && foundbadcrc) {
		upsdebugx (3, "Get statuscode Ko: bad CRC or Checksum");
		return -1;
	}

	/* optional */
	if (!wait_packet && foundnak) {
		upsdebugx (3, "Get statuscode Ko: command not supported");
		return 0;
	}

	upsdebugx (3, "Get statuscode Ok: read byte: %d", recv);

	riello_parse_rc(&bufIn[0], &DevData);

	return 0;
}

static int riello_instcmd(const char *cmdname, const char *extra)
{
	uint8_t length;
	int recv;
	uint16_t delay;
	const char	*delay_char;

	if (!riello_test_bit(&DevData.StatusCode[0], 1)) {

		if (!strcasecmp(cmdname, "load.off")) {
			delay = 0;

			length = riello_prepare_cs(bufOut, gpser_error_control, delay);
			recv = riello_command(&bufOut[0], &bufIn[0], length, LENGTH_DEF);

			if (recv < 0) {
				upsdebugx (3, "Command load.off err: read byte: %d", recv);
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundbadcrc) {
				upsdebugx (3, "Command load.off Ko: bad CRC or Checksum");
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundnak) {
				upsdebugx (3, "Command load.off Ko: command not supported");
				return STAT_INSTCMD_FAILED;
			}

			upsdebugx (3, "Command load.off Ok: read byte: %d", recv);
			return STAT_INSTCMD_HANDLED;
		}

		if (!strcasecmp(cmdname, "load.off.delay")) {
			int ipv;
			delay_char = dstate_getinfo("ups.delay.shutdown");
			ipv = atoi(delay_char);
			/* With a "char" in the name, might assume we fit... but :) */
			if (ipv < 0 || (intmax_t)ipv > (intmax_t)UINT16_MAX) return STAT_INSTCMD_FAILED;
			delay = (uint16_t)ipv;

			length = riello_prepare_cs(bufOut, gpser_error_control, delay);
			recv = riello_command(&bufOut[0], &bufIn[0], length, LENGTH_DEF);

			if (recv < 0) {
				upsdebugx (3, "Command load.off.delay err: read byte: %d", recv);
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundbadcrc) {
				upsdebugx (3, "Command load.off.delay Ko: bad CRC or Checksum");
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundnak) {
				upsdebugx (3, "Command load.off.delay Ko: command not supported");
				return STAT_INSTCMD_FAILED;
			}

			upsdebugx (3, "Command load.off.delay Ok: read byte: %d", recv);
			return STAT_INSTCMD_HANDLED;
		}

		if (!strcasecmp(cmdname, "load.on")) {
			delay = 0;

			length = riello_prepare_cr(bufOut, gpser_error_control, delay);
			recv = riello_command(&bufOut[0], &bufIn[0], length, LENGTH_DEF);

			if (recv < 0) {
				upsdebugx (3, "Command load.on err: read byte: %d", recv);
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundbadcrc) {
				upsdebugx (3, "Command load.on Ko: bad CRC or Checksum");
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundnak) {
				upsdebugx (3, "Command load.on Ko: command not supported");
				return STAT_INSTCMD_FAILED;
			}

			upsdebugx (3, "Command load.on Ok: read byte: %d", recv);
			return STAT_INSTCMD_HANDLED;
		}

		if (!strcasecmp(cmdname, "load.on.delay")) {
			int ipv;
			delay_char = dstate_getinfo("ups.delay.reboot");
			ipv = atoi(delay_char);
			/* With a "char" in the name, might assume we fit... but :) */
			if (ipv < 0 || (intmax_t)ipv > (intmax_t)UINT16_MAX) return STAT_INSTCMD_FAILED;
			delay = (uint16_t)ipv;

			length = riello_prepare_cr(bufOut, gpser_error_control, delay);
			recv = riello_command(&bufOut[0], &bufIn[0], length, LENGTH_DEF);

			if (recv < 0) {
				upsdebugx (3, "Command load.on.delay err: read byte: %d", recv);
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundbadcrc) {
				upsdebugx (3, "Command load.on.delay Ko: bad CRC or Checksum");
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundnak) {
				upsdebugx (3, "Command load.on.delay Ko: command not supported");
				return STAT_INSTCMD_FAILED;
			}

			upsdebugx (3, "Command load.on.delay Ok: read byte: %d", recv);
			return STAT_INSTCMD_HANDLED;
		}
	}
	else {
		if (!strcasecmp(cmdname, "shutdown.return")) {
			int ipv;
			delay_char = dstate_getinfo("ups.delay.shutdown");
			ipv = atoi(delay_char);
			/* With a "char" in the name, might assume we fit... but :) */
			if (ipv < 0 || (intmax_t)ipv > (intmax_t)UINT16_MAX) return STAT_INSTCMD_FAILED;
			delay = (uint16_t)ipv;

			length = riello_prepare_cs(bufOut, gpser_error_control, delay);
			recv = riello_command(&bufOut[0], &bufIn[0], length, LENGTH_DEF);

			if (recv < 0) {
				upsdebugx (3, "Command shutdown.return err: read byte: %d", recv);
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundbadcrc) {
				upsdebugx (3, "Command shutdown.return Ko: bad CRC or Checksum");
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundnak) {
				upsdebugx (3, "Command shutdown.return Ko: command not supported");
				return STAT_INSTCMD_FAILED;
			}

			upsdebugx (3, "Command shutdown.return Ok: read byte: %d", recv);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "shutdown.stop")) {
		length = riello_prepare_cd(bufOut, gpser_error_control);
		recv = riello_command(&bufOut[0], &bufIn[0], length, LENGTH_DEF);

		if (recv < 0) {
			upsdebugx (3, "Command shutdown.stop err: read byte: %d", recv);
			return STAT_INSTCMD_FAILED;
		}

		if (!wait_packet && foundbadcrc) {
			upsdebugx (3, "Command shutdown.stop Ko: bad CRC or Checksum");
			return STAT_INSTCMD_FAILED;
		}

		if (!wait_packet && foundnak) {
			upsdebugx (3, "Command shutdown.stop Ko: command not supported");
			return STAT_INSTCMD_FAILED;
		}

		upsdebugx (3, "Command shutdown.stop Ok: read byte: %d", recv);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.panel.start")) {
		length = riello_prepare_tp(bufOut, gpser_error_control);
		recv = riello_command(&bufOut[0], &bufIn[0], length, LENGTH_DEF);

		if (recv < 0) {
			upsdebugx (3, "Command test.panel.start err: read byte: %d", recv);
			return STAT_INSTCMD_FAILED;
		}

		if (!wait_packet && foundbadcrc) {
			upsdebugx (3, "Command test.panel.start Ko: bad CRC or Checksum");
			return STAT_INSTCMD_FAILED;
		}

		if (!wait_packet && foundnak) {
			upsdebugx (3, "Command test.panel.start Ko: command not supported");
			return STAT_INSTCMD_FAILED;
		}

		upsdebugx (3, "Command test.panel.start Ok: read byte: %d", recv);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start")) {
		length = riello_prepare_tb(bufOut, gpser_error_control);
		recv = riello_command(&bufOut[0], &bufIn[0], length, LENGTH_DEF);

		if (recv < 0) {
			upsdebugx (3, "Command test.battery.start err: read byte: %d", recv);
			return STAT_INSTCMD_FAILED;
		}

		if (!wait_packet && foundbadcrc) {
			upsdebugx (3, "Command test.battery.start Ko: bad CRC or Checksum");
			return STAT_INSTCMD_FAILED;
		}

		if (!wait_packet && foundnak) {
			upsdebugx (3, "Command test.battery.start Ko: command not supported");
			return STAT_INSTCMD_FAILED;
		}

		upsdebugx (3, "Command test.battery.start Ok: read byte: %d", recv);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

static int start_ups_comm(void)
{
	uint16_t length;
	int recv;

	upsdebugx (2, "entering start_ups_comm()\n");

	cypress_setfeatures();

	length = riello_prepare_gi(&bufOut[0]);

	recv = riello_command(&bufOut[0], &bufIn[0], length, LENGTH_GI);

	if (recv < 0) {
		upsdebugx (3, "Get identif err: read byte: %d", recv);
		return recv;
	}

	if (!wait_packet && foundbadcrc) {
		upsdebugx (3, "Get identif Ko: bad CRC or Checksum");
		return 1;
	}

	if (!wait_packet && foundnak) {
		upsdebugx (3, "Get identif Ko: command not supported");
		return 1;
	}

	upsdebugx (3, "Get identif Ok: read byte: %d", recv);

	return 0;
}

void upsdrv_help(void)
{

}


void upsdrv_makevartable(void)
{
	/* allow -x vendor=X, vendorid=X, product=X, productid=X, serial=X */
	nut_usb_addvars();

	addvar(VAR_FLAG, "localcalculation", "Calculate battery charge and runtime locally");
}

void upsdrv_initups(void)
{
	const struct {
		const char	*name;
		int		(*command)(uint8_t *cmd, uint8_t *buf, uint16_t length, uint16_t buflen);
	} subdriver[] = {
		{ "cypress", &cypress_command },
		{ NULL, NULL }
	};

	int	ret;
	char	*regex_array[USBMATCHER_REGEXP_ARRAY_LIMIT];

	char	*subdrv = getval("subdriver");

	warn_if_bad_usb_port_filename(device_path);

	regex_array[0] = getval("vendorid");
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor");
	regex_array[3] = getval("product");
	regex_array[4] = getval("serial");
	regex_array[5] = getval("bus");
	regex_array[6] = getval("device");
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	regex_array[7] = getval("busport");
#else
	if (getval("busport")) {
		upslogx(LOG_WARNING, "\"busport\" is configured for the device, but is not actually handled by current build combination of NUT and libusb (ignored)");
	}
#endif

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

	ret = usb->open_dev(&udev, &usbdevice, regex_matcher, &driver_callback);
	if (ret < 0) {
		fatalx(EXIT_FAILURE,
			"No supported devices found. Please check your device availability with 'lsusb'\n"
			"and make sure you have an up-to-date version of NUT. If this does not help,\n"
			"try running the driver with at least 'subdriver', 'vendorid' and 'productid'\n"
			"options specified. Please refer to the man page for details about these options\n"
			"(man 8 riello_usb).\n");
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
}

void upsdrv_initinfo(void)
{
	int ret;
	const char *valN = NULL, *valL = NULL, *valH = NULL;

	ret = start_ups_comm();

	if (ret < 0)
		fatalx(EXIT_FAILURE, "No communication with UPS");
	else if (ret > 0)
		fatalx(EXIT_FAILURE, "Bad checksum or NACK");
	else
		upsdebugx(2, "Communication with UPS established");

	if (testvar("localcalculation")) {
		localcalculation = 1;
		upsdebugx(1, "Will guesstimate battery charge and runtime "
			"instead of trusting device readings (if any); "
			"consider also setting default.battery.voltage.low "
			"and default.battery.voltage.high for this device");
	}
	dstate_setinfo("driver.parameter.localcalculation", "%d", localcalculation);

	riello_parse_gi(&bufIn[0], &DevData);

	gpser_error_control = DevData.Identif_bytes[4]-0x30;
	if ((DevData.Identif_bytes[0] == '1') || (DevData.Identif_bytes[0] == '2'))
		input_monophase = 1;
	else {
		input_monophase = 0;
		dstate_setinfo("input.phases", "%d", 3);
		dstate_setinfo("input.phases", "%d", 3);
		dstate_setinfo("input.bypass.phases", "%d", 3);
	}
	if ((DevData.Identif_bytes[0] == '1') || (DevData.Identif_bytes[0] == '3'))
		output_monophase = 1;
	else {
		output_monophase = 0;
		dstate_setinfo("output.phases", "%d", 3);
	}

	dstate_setinfo("device.mfr", "RPS S.p.a.");
	dstate_setinfo("device.model", "%s", (unsigned char*) DevData.ModelStr);
	dstate_setinfo("device.serial", "%s", (unsigned char*) DevData.Identification);
	dstate_setinfo("device.type", "ups");

	dstate_setinfo("ups.mfr", "RPS S.p.a.");
	dstate_setinfo("ups.model", "%s", (unsigned char*) DevData.ModelStr);
	dstate_setinfo("ups.serial", "%s", (unsigned char*) DevData.Identification);
	dstate_setinfo("ups.firmware", "%s", (unsigned char*) DevData.Version);

	/* Is it set by user default/override configuration?
	 * NOTE: "valN" is also used for a check just below.
	 */
	valN = dstate_getinfo("battery.voltage.nominal");
	if (valN) {
		batt_volt_nom = strtod(valN, NULL);
		upsdebugx(1, "Using battery.voltage.nominal=%.1f "
			"likely coming from user configuration",
			batt_volt_nom);
	}

	if (get_ups_nominal() == 0) {
		dstate_setinfo("ups.realpower.nominal", "%u", DevData.NomPowerKW);
		dstate_setinfo("ups.power.nominal", "%u", DevData.NomPowerKVA);
		dstate_setinfo("output.voltage.nominal", "%u", DevData.NominalUout);
		dstate_setinfo("output.frequency.nominal", "%.1f", DevData.NomFout/10.0);

		/* Is it set by user default/override configuration (see just above)? */
		if (valN) {
			upsdebugx(1, "...instead of battery.voltage.nominal=%u "
				"reported by the device", DevData.NomUbat);
		} else {
			dstate_setinfo("battery.voltage.nominal", "%u", DevData.NomUbat);
			batt_volt_nom = (double)DevData.NomUbat;
		}

		dstate_setinfo("battery.capacity", "%u", DevData.NomBatCap);
	} else {
		/* TOTHINK: Check the momentary reading of battery.voltage
		 * or would it be too confusing (especially if it is above
		 * 12V and might correspond to a discharged UPS when the
		 * driver starts up after an outage?)
		 * NOTE: DevData.Ubat would be scaled by 10!
		 */
		if (!valN) {
			/* The nominal was not already set by user configuration... */
			upsdebugx(1, "Using built-in default battery.voltage.nominal=%.1f",
				batt_volt_nom);
			dstate_setinfo("battery.voltage.nominal", "%.1f", batt_volt_nom);
		}
	}

	/* We have a nominal voltage by now - either from user configuration
	 * or from the device itself (or initial defaults for 12V). Do we have
	 * any low/high range from HW/FW or defaults from ups.conf? */
	valL = dstate_getinfo("battery.voltage.low");
	valH = dstate_getinfo("battery.voltage.high");

	{	/* scoping */
		/* Pick a suitable low/high range (or keep built-in default).
		 * The factor may be a count of battery packs in the UPS.
		 */
		int times12 = batt_volt_nom / 12;
		if (times12 > 1) {
			/* Scale up the range for 24V (X=2) etc. */
			upsdebugx(3, "%s: Using %i times the voltage range of 12V PbAc battery",
				__func__, times12);
			batt_volt_low  *= times12;
			batt_volt_high *= times12;
		}
	}

	if (!valL && !valH) {
		/* Both not set (NULL) => pick by nominal (X times 12V above). */
		upsdebugx(3, "Neither battery.voltage.low=%.1f "
			"nor battery.voltage.high=%.1f is set via "
			"driver configuration or by device; keeping "
			"at built-in default value (aligned "
			"with battery.voltage.nominal=%.1f)",
			batt_volt_low, batt_volt_high, batt_volt_nom);
	} else {
		if (valL) {
			batt_volt_low = strtod(valL, NULL);
			upsdebugx(2, "%s: Using battery.voltage.low=%.1f from device or settings",
				__func__, batt_volt_low);
		}

		if (valH) {
			batt_volt_high = strtod(valH, NULL);
			upsdebugx(2, "%s: Using battery.voltage.high=%.1f from device or settings",
				__func__, batt_volt_high);
		}

		/* If just one of those is set, then what? */
		if (valL || valH) {
			upsdebugx(1, "WARNING: Only one of battery.voltage.low=%.1f "
				"or battery.voltage.high=%.1f is set via "
				"driver configuration; keeping the other "
				"at built-in default value (aligned "
				"with battery.voltage.nominal=%.1f)",
				batt_volt_low, batt_volt_high, batt_volt_nom);
		} else {
			upsdebugx(1, "Both of battery.voltage.low=%.1f "
				"or battery.voltage.high=%.1f are set via "
				"driver configuration; not aligning "
				"with battery.voltage.nominal=%.1f",
				batt_volt_low, batt_volt_high, batt_volt_nom);
		}
	}

	/* Whatever the origin, make the values known via dstate */
	dstate_setinfo("battery.voltage.low",  "%.1f", batt_volt_low);
	dstate_setinfo("battery.voltage.high", "%.1f", batt_volt_high);

	/* commands ----------------------------------------------- */
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	dstate_addcmd("load.off.delay");
	dstate_addcmd("load.on.delay");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.panel.start");

	dstate_setinfo("ups.delay.shutdown", "%u", offdelay);
	dstate_setflags("ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.shutdown", 3);
	dstate_setinfo("ups.delay.reboot", "%u", bootdelay);
	dstate_setflags("ups.delay.reboot", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.reboot", 3);

	/* install handlers */
/*	upsh.setvar = hid_set_value; setvar; */

	/* note: for a transition period, these data are redundant! */
	/* dstate_setinfo("device.mfr", "skel manufacturer"); */
	/* dstate_setinfo("device.model", "longrun 15000"); */

	upsh.instcmd = riello_instcmd;
}

void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */
	int retry;

	/* maybe try to detect the UPS here, but try a shutdown even if
	 it doesn't respond at first if possible */

	/* replace with a proper shutdown function */


	/* you may have to check the line status since the commands
	 for toggling power are frequently different for OL vs. OB */

	/* OL: this must power cycle the load if possible */

	/* OB: the load must remain off until the power returns */

	for (retry = 1; retry <= MAXTRIES; retry++) {
		/* By default, abort a previously requested shutdown
		 * (if any) and schedule a new one from this moment. */
		if (riello_instcmd("shutdown.stop", NULL) != STAT_INSTCMD_HANDLED) {
			continue;
		}

		if (riello_instcmd("shutdown.return", NULL) != STAT_INSTCMD_HANDLED) {
			continue;
		}

		upslogx(LOG_ERR, "Shutting down");
		if (handling_upsdrv_shutdown > 0)
			set_exit_flag(EF_EXIT_SUCCESS);
		return;
	}

	upslogx(LOG_ERR, "Shutdown failed!");
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(EF_EXIT_FAILURE);
}

void upsdrv_updateinfo(void)
{
	uint8_t getextendedOK;
	static int countlost = 0;
	int stat;
	int battcharge;
	float battruntime;
	float upsloadfactor;
#ifdef RIELLO_DYNAMIC_BATTVOLT_INFO
	const char *val = NULL;
#endif

	upsdebugx(1, "countlost %d",countlost);

	if (countlost > 0){
		upsdebugx(1, "Communication with UPS is lost: status read failed!");

		if (countlost == COUNTLOST) {
			dstate_datastale();
			upslogx(LOG_WARNING, "Communication with UPS is lost: status read failed!");
		}
	}

	stat = get_ups_status();

	upsdebugx(1, "get_ups_status() %d",stat );

	if (stat < 0) {
		if (countlost < COUNTLOST)
			countlost++;
		return;
	}

	if (get_ups_extended() == 0)
		getextendedOK = 1;
	else
		getextendedOK = 0;

	if (countlost == COUNTLOST)
		upslogx(LOG_NOTICE, "Communication with UPS is re-established!");

	dstate_setinfo("input.frequency", "%.2f", DevData.Finp/10.0);
	dstate_setinfo("input.bypass.frequency", "%.2f", DevData.Fbypass/10.0);
	dstate_setinfo("output.frequency", "%.2f", DevData.Fout/10.0);
	dstate_setinfo("battery.voltage", "%.1f", DevData.Ubat/10.0);

#ifdef RIELLO_DYNAMIC_BATTVOLT_INFO
	/* Can be set via default.* or override.* driver options
	 * if not served by the device HW/FW */
	val = dstate_getinfo("battery.voltage.low");
	if (val) {
		batt_volt_low = strtod(val, NULL);
	}

	val = dstate_getinfo("battery.voltage.high");
	if (val) {
		batt_volt_high = strtod(val, NULL);
	}
#endif

	if (localcalculation) {
		/* NOTE: at this time "localcalculation" is a configuration toggle.
		 * Maybe later it can be replaced by a common "runtimecal" setting. */
		/* Considered "Ubat" physical range here (e.g. 10.7V to 12.9V) is
		 * seen as "107" or "129" integers in the DevData properties: */
		uint16_t	Ubat_low  = batt_volt_low  * 10;	/* e.g. 107 */
		uint16_t	Ubat_high = batt_volt_high * 10;	/* e.g. 129 */
		static int batt_volt_logged = 0;

		if (!batt_volt_logged) {
			upsdebugx(0, "\nUsing battery.voltage.low=%.1f and "
				"battery.voltage.high=%.1f for \"localcalculation\" "
				"guesstimates of battery.charge and battery.runtime",
				batt_volt_low, batt_volt_high);
			batt_volt_logged = 1;
		}

		battcharge = ((DevData.Ubat <= Ubat_high) && (DevData.Ubat >= Ubat_low))
			? (((DevData.Ubat - Ubat_low)*100) / (Ubat_high - Ubat_low))
			: ((DevData.Ubat < Ubat_low) ? 0 : 100);
		battruntime = (DevData.NomBatCap * DevData.NomUbat * 3600.0/DevData.NomPowerKW) * (battcharge/100.0);
		upsloadfactor = (DevData.Pout1 > 0) ? (DevData.Pout1/100.0) : 1;

		dstate_setinfo("battery.charge", "%d", battcharge);
		dstate_setinfo("battery.runtime", "%.0f", battruntime/upsloadfactor);
	}
	else {
		if (!localcalculation_logged) {
			upsdebugx(0, "\nIf you don't see values for battery.charge and "
				"battery.runtime or values are incorrect,"
				"try setting \"localcalculation\" flag in \"ups.conf\" "
				"options section for this driver!\n");
			localcalculation_logged = 1;
		}
		if ((DevData.BatCap < 0xFFFF) && (DevData.BatTime < 0xFFFF)) {
			/* Use values reported by the driver unless they are marked
			 * invalid/unknown by HW/FW (all bits in the word are set).
			 */
			dstate_setinfo("battery.charge", "%u", DevData.BatCap);
			dstate_setinfo("battery.runtime", "%u", (unsigned int)DevData.BatTime*60);
		}
	}

	if (DevData.Tsystem == 255) {
		/* Use values reported by the driver unless they are marked
		 * invalid/unknown by HW/FW (all bits in the word are set).
		 */
		/*dstate_setinfo("ups.temperature", "%u", 0);*/
		upsdebugx(4, "Reported temperature value is 0xFF, "
			"probably meaning \"-1\" for error or "
			"missing sensor - ignored");
	}
	else if (DevData.Tsystem < 0xFF) {
		dstate_setinfo("ups.temperature", "%u", DevData.Tsystem);
	}

	if (input_monophase) {
		dstate_setinfo("input.voltage", "%u", DevData.Uinp1);
		dstate_setinfo("input.bypass.voltage", "%u", DevData.Ubypass1);
	}
	else {
		dstate_setinfo("input.L1-N.voltage", "%u", DevData.Uinp1);
		dstate_setinfo("input.L2-N.voltage", "%u", DevData.Uinp2);
		dstate_setinfo("input.L3-N.voltage", "%u", DevData.Uinp3);
		dstate_setinfo("input.bypass.L1-N.voltage", "%u", DevData.Ubypass1);
		dstate_setinfo("input.bypass.L2-N.voltage", "%u", DevData.Ubypass2);
		dstate_setinfo("input.bypass.L3-N.voltage", "%u", DevData.Ubypass3);
	}

	if (output_monophase) {
		dstate_setinfo("output.voltage", "%u", DevData.Uout1);
		dstate_setinfo("output.power.percent", "%u", DevData.Pout1);
		dstate_setinfo("ups.load", "%u", DevData.Pout1);
	}
	else {
		dstate_setinfo("output.L1-N.voltage", "%u", DevData.Uout1);
		dstate_setinfo("output.L2-N.voltage", "%u", DevData.Uout2);
		dstate_setinfo("output.L3-N.voltage", "%u", DevData.Uout3);
		dstate_setinfo("output.L1.power.percent", "%u", DevData.Pout1);
		dstate_setinfo("output.L2.power.percent", "%u", DevData.Pout2);
		dstate_setinfo("output.L3.power.percent", "%u", DevData.Pout3);
		dstate_setinfo("ups.load", "%u", (unsigned int)(DevData.Pout1+DevData.Pout2+DevData.Pout3)/3);
	}

	status_init();

	/* AC Fail */
	if (riello_test_bit(&DevData.StatusCode[0], 1))
		status_set("OB");
	else
		status_set("OL");

	/* LowBatt */
	if ((riello_test_bit(&DevData.StatusCode[0], 1)) &&
		(riello_test_bit(&DevData.StatusCode[0], 0)))
		status_set("LB");

	/* Standby */
	if (!riello_test_bit(&DevData.StatusCode[0], 3))
		status_set("OFF");

	/* On Bypass */
	if (riello_test_bit(&DevData.StatusCode[1], 3))
		status_set("BYPASS");

	/* Overload */
	if (riello_test_bit(&DevData.StatusCode[4], 2))
		status_set("OVER");

	/* Buck */
	if (riello_test_bit(&DevData.StatusCode[1], 0))
		status_set("TRIM");

	/* Boost */
	if (riello_test_bit(&DevData.StatusCode[1], 1))
		status_set("BOOST");

	/* Replace battery */
	if (riello_test_bit(&DevData.StatusCode[2], 0))
		status_set("RB");

	/* Charging battery */
	if (riello_test_bit(&DevData.StatusCode[2], 2))
		status_set("CHRG");

	status_commit();

	dstate_dataok();

	if (getextendedOK) {
		dstate_setinfo("output.L1.power", "%u", DevData.Pout1VA);
		dstate_setinfo("output.L2.power", "%u", DevData.Pout2VA);
		dstate_setinfo("output.L3.power", "%u", DevData.Pout3VA);
		dstate_setinfo("output.L1.realpower", "%u", DevData.Pout1W);
		dstate_setinfo("output.L2.realpower", "%u", DevData.Pout2W);
		dstate_setinfo("output.L3.realpower", "%u", DevData.Pout3W);
		dstate_setinfo("output.L1.current", "%u", DevData.Iout1);
		dstate_setinfo("output.L2.current", "%u", DevData.Iout2);
		dstate_setinfo("output.L3.current", "%u", DevData.Iout3);
	}

	poll_interval = 2;

	countlost = 0;

/*	if (get_ups_statuscode() != 0)
		upsdebugx(2, "Communication is lost");
	else {
	}*/

	/*
	 * ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, IGNCHARS);
	 *
	 * if (ret < STATUS_LEN) {
	 * 	upslogx(LOG_ERR, "Short read from UPS");
	 *	dstate_datastale();
	 *	return;
	 * }
	 */

	/* dstate_setinfo("var.name", ""); */

	/* if (ioctl(upsfd, TIOCMGET, &flags)) {
	 *	upslog_with_errno(LOG_ERR, "TIOCMGET");
	 *	dstate_datastale();
	 *	return;
	 * }
	 */

	/* status_init();
	 *
	 * if (ol)
	 * 	status_set("OL");
	 * else
	 * 	status_set("OB");
	 * ...
	 *
	 * status_commit();
	 *
	 * dstate_dataok();
	 */

	/*
	 * poll_interval = 2;
	 */
}

void upsdrv_cleanup(void)
{
	usb->close_dev(udev);
	USBFreeExactMatcher(reopen_matcher);
	USBFreeRegexMatcher(regex_matcher);
	free(usbdevice.Vendor);
	free(usbdevice.Product);
	free(usbdevice.Serial);
	free(usbdevice.Bus);
	free(usbdevice.Device);
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	free(usbdevice.BusPort);
#endif
}
