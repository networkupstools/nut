/*
 * riello_usb.c: support for Riello USB protocol based UPSes
 *
 * A document describing the protocol implemented by this driver can be
 * found online at:
 *
 *   http://www.networkupstools.org/ups-protocols/riello/PSGPSER-0104.pdf
 *
 * Copyright (C) 2012 - Elio Parisi <e.parisi@riello-ups.com>
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

#include <stdint.h>

#include "main.h"
#include "libusb.h"
#include "usb-common.h"
#include "riello.h"

#define DRIVER_NAME	"Riello USB driver"
#define DRIVER_VERSION	"0.03"

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

static TRielloData DevData;

static usb_communication_subdriver_t *usb = &usb_subdriver;
static usb_dev_handle *udev = NULL;
static USBDevice_t usbdevice;
static USBDeviceMatcher_t *reopen_matcher = NULL;
static USBDeviceMatcher_t *regex_matcher = NULL;

static int (*subdriver_command)(uint8_t *cmd, uint8_t *buf, uint16_t length, uint16_t buflen) = NULL;

static void ussleep(long usec)
{

	if (usec == 1)
		usec = 400;
	else
		usec *= 1000;

	usleep(usec);
}

static int cypress_setfeatures()
{
	int ret;

	bufOut[0] = 0xB0;
	bufOut[1] = 0x4;
	bufOut[2] = 0x0;
	bufOut[3] = 0x0;
	bufOut[4] = 0x3;

	/* Write features report */
	ret = usb_control_msg(udev, USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
								0x09,						/* HID_REPORT_SET = 0x09 */
								0 + (0x03 << 8),		/* HID_REPORT_TYPE_FEATURE */
								0, (char*) bufOut, 0x5, 1000);

	if (ret <= 0) {
		upsdebugx(3, "send: %s", ret ? usb_strerror() : "error");
		return ret;
	}

	upsdebugx(3, "send: features report ok");
	return ret;
}

static int Send_USB_Packet(uint8_t *send_str, uint16_t numbytes)
{
	uint8_t USB_buff_pom[10];
	int i, err, size;

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

		err = usb_bulk_write(udev, 0x2, (char*) USB_buff_pom, 8, 1000);

		if (err < 0) {
			upsdebugx(3, "USB: Send_USB_Packet: send_usb_packet, err = %d %s ", err, strerror(errno));
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

		err = usb_bulk_write(udev, 0x2, (char*) USB_buff_pom, 8, 1000);

		if (err < 0) {
			upsdebugx(3, "USB: Send_USB_Packet: send_usb_packet, err = %d %s ", err, strerror(errno));
			return err;
		}
		ussleep(USB_WRITE_DELAY);
	}
	return (0);
}

static int Get_USB_Packet(uint8_t *buffer)
{
	char inBuf[10];
	int err, size, ep;

	/* note: this function stop until some byte(s) is not arrived */
	size = 8;

	ep = 0x81 | USB_ENDPOINT_IN;
	err = usb_bulk_read(udev, ep, (char*) inBuf, size, 1000);

	if (err > 0)
		upsdebugx(3, "read: %02X %02X %02X %02X %02X %02X %02X %02X", inBuf[0], inBuf[1], inBuf[2], inBuf[3], inBuf[4], inBuf[5], inBuf[6], inBuf[7]);

	if (err < 0){
		upsdebugx(3, "USB: Get_USB_Packet: send_usb_packet, err = %d %s ", err, strerror(errno));
		return err;
	}

	/* copy to buffer */
	size = inBuf[0] & 0x07;
	if (size)
		memcpy(buffer, &inBuf[1], size);

	return(size);
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
static int driver_callback(usb_dev_handle *handle, USBDevice_t *device, unsigned char *rdbuf, int rdlen)
{
	 NUT_UNUSED_VARIABLE(device);
	 NUT_UNUSED_VARIABLE(rdbuf);
	 NUT_UNUSED_VARIABLE(rdlen);

/*
	if (usb_set_configuration(handle, 1) < 0) {
		upslogx(LOG_WARNING, "Can't set USB configuration: %s", usb_strerror());
		return -1;
	}
*/

	if (usb_claim_interface(handle, 0) < 0) {
		upslogx(LOG_WARNING, "Can't claim USB interface: %s", usb_strerror());
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
		ret = usb->open(&udev, &usbdevice, reopen_matcher, &driver_callback);

		upsdebugx (3, "riello_command err udev NULL : %d ", ret);
		if (ret < 0)
			return ret;

		upsdrv_initinfo();	//reconekt usb cable
	}

	ret = (*subdriver_command)(cmd, buf, length, buflen);
	if (ret >= 0) {
		upsdebugx (3, "riello_command ok: %u", ret);
		return ret;
	}

	upsdebugx (3, "riello_command err: %d", ret);

	switch (ret)
	{
	case -EBUSY:		/* Device or resource busy */
		fatal_with_errno(EXIT_FAILURE, "Got disconnected by another driver");
#ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif

	case -EPERM:		/* Operation not permitted */
		fatal_with_errno(EXIT_FAILURE, "Permissions problem");
#ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif

	case -EPIPE:		/* Broken pipe */
		if (usb_clear_halt(udev, 0x81) == 0) {
			upsdebugx(1, "Stall condition cleared");
			break;
		}
#ifdef ETIME
		goto fallthrough_case_etime;
	case -ETIME:		/* Timer expired */
	fallthrough_case_etime:
#endif
		if (usb_reset(udev) == 0) {
			upsdebugx(1, "Device reset handled");
		}
		goto fallthrough_case_reconnect;
	case -ENODEV:		/* No such device */
	case -EACCES:		/* Permission denied */
	case -EIO:		/* I/O error */
	case -ENXIO:		/* No such device or address */
	case -ENOENT:		/* No such file or directory */
	fallthrough_case_reconnect:
		/* Uh oh, got to reconnect! */
		usb->close(udev);
		udev = NULL;
		break;

	case -ETIMEDOUT:	/* Connection timed out */
		upsdebugx (3, "riello_command err: Resource temporarily unavailable");
		break;

	case -EOVERFLOW:	/* Value too large for defined data type */
#ifdef EPROTO
	case -EPROTO:		/* Protocol error */
#endif
		break;
	default:
		break;
	}

	return ret;
}

static int get_ups_nominal()
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

static int get_ups_status()
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

static int get_ups_extended()
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
int get_ups_statuscode()
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
			delay_char = dstate_getinfo("ups.delay.shutdown");
			delay = atoi(delay_char);

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
			delay_char = dstate_getinfo("ups.delay.reboot");
			delay = atoi(delay_char);

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
			delay_char = dstate_getinfo("ups.delay.shutdown");
			delay = atoi(delay_char);

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

static int start_ups_comm()
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

	upsdebugx (3, "Get identif Ok: read byte: %u", recv);

	return 0;
}

void upsdrv_help(void)
{

}


void upsdrv_makevartable(void)
{

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
	char	*regex_array[7];

	char	*subdrv = getval("subdriver");

	regex_array[0] = getval("vendorid");
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor");
	regex_array[3] = getval("product");
	regex_array[4] = getval("serial");
	regex_array[5] = getval("bus");
	regex_array[6] = getval("device");

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

	ret = usb->open(&udev, &usbdevice, regex_matcher, &driver_callback);
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

	ret = start_ups_comm();

	if (ret < 0)
		fatalx(EXIT_FAILURE, "No communication with UPS");
	else if (ret > 0)
		fatalx(EXIT_FAILURE, "Bad checksum or NACK");
	else
		upsdebugx(2, "Communication with UPS established");

	riello_parse_gi(&bufIn[0], &DevData);

	gpser_error_control = DevData.Identif_bytes[4]-0x30;
	if ((DevData.Identif_bytes[0] == '1') || (DevData.Identif_bytes[0] == '2'))
		input_monophase = 1;
	else {
		input_monophase = 0;
		dstate_setinfo("input.phases", "%u", 3);
		dstate_setinfo("input.phases", "%u", 3);
		dstate_setinfo("input.bypass.phases", "%u", 3);
	}
	if ((DevData.Identif_bytes[0] == '1') || (DevData.Identif_bytes[0] == '3'))
		output_monophase = 1;
	else {
		output_monophase = 0;
		dstate_setinfo("output.phases", "%u", 3);
	}

	dstate_setinfo("device.mfr", "RPS S.p.a.");
	dstate_setinfo("device.model", "%s", (unsigned char*) DevData.ModelStr);
	dstate_setinfo("device.serial", "%s", (unsigned char*) DevData.Identification);
	dstate_setinfo("device.type", "ups");

	dstate_setinfo("ups.mfr", "RPS S.p.a.");
	dstate_setinfo("ups.model", "%s", (unsigned char*) DevData.ModelStr);
	dstate_setinfo("ups.serial", "%s", (unsigned char*) DevData.Identification);
	dstate_setinfo("ups.firmware", "%s", (unsigned char*) DevData.Version);

	if (get_ups_nominal() == 0) {
		dstate_setinfo("ups.realpower.nominal", "%u", DevData.NomPowerKW);
		dstate_setinfo("ups.power.nominal", "%u", DevData.NomPowerKVA);
		dstate_setinfo("output.voltage.nominal", "%u", DevData.NominalUout);
		dstate_setinfo("output.frequency.nominal", "%.1f", DevData.NomFout/10.0);
		dstate_setinfo("battery.voltage.nominal", "%u", DevData.NomUbat);
		dstate_setinfo("battery.capacity", "%u", DevData.NomBatCap);
	}

	/* commands ----------------------------------------------- */
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	dstate_addcmd("load.off.delay");
	dstate_addcmd("load.on.delay");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.panel.start");

	/* install handlers */
/*	upsh.setvar = hid_set_value; setvar; */

	/* note: for a transition period, these data are redundant! */
	/* dstate_setinfo("device.mfr", "skel manufacturer"); */
	/* dstate_setinfo("device.model", "longrun 15000"); */

	upsh.instcmd = riello_instcmd;
}

void upsdrv_shutdown(void)
	__attribute__((noreturn));

void upsdrv_shutdown(void)
{
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

		if (riello_instcmd("shutdown.stop", NULL) != STAT_INSTCMD_HANDLED) {
			continue;
		}

		if (riello_instcmd("shutdown.return", NULL) != STAT_INSTCMD_HANDLED) {
			continue;
		}

		fatalx(EXIT_SUCCESS, "Shutting down");
	}

	fatalx(EXIT_FAILURE, "Shutdown failed!");
}

void upsdrv_updateinfo(void)
{
	uint8_t getextendedOK;
	static int countlost = 0;
	int stat;

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
	dstate_setinfo("battery.charge", "%u", DevData.BatCap);
	dstate_setinfo("battery.runtime", "%u", DevData.BatTime*60);
	dstate_setinfo("ups.temperature", "%u", DevData.Tsystem);

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
		dstate_setinfo("ups.load", "%u", (DevData.Pout1+DevData.Pout2+DevData.Pout3)/3);
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
	usb->close(udev);
	USBFreeExactMatcher(reopen_matcher);
	USBFreeRegexMatcher(regex_matcher);
	free(usbdevice.Vendor);
	free(usbdevice.Product);
	free(usbdevice.Serial);
	free(usbdevice.Bus);
	free(usbdevice.Device);
}
