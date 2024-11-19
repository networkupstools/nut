/*
 * riello_ser.c: support for Riello serial protocol based UPSes
 *
 * A document describing the protocol implemented by this driver can be
 * found online at "https://www.networkupstools.org/protocols/riello/PSGPSER-0104.pdf"
 * and "https://www.networkupstools.org/protocols/riello/PSSENTR-0100.pdf".
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

#include "config.h" /* must be the first header */
#include "common.h" /* for upsdebugx() etc */

#include <string.h>

#ifdef WIN32
# include "wincompat.h"
#endif

#include "main.h"
#include "serial.h"
#include "timehead.h"

/*
 * // The serial driver has no need for HID structures/code currently
 * // (maybe there is/was a plan for sharing something between siblings).
 * // Note that HID is tied to libusb or libshut definitions at the moment.
#include "hidparser.h"
#include "hidtypes.h"
 */

#include "riello.h"

#define DRIVER_NAME	"Riello serial driver"
#define DRIVER_VERSION	"0.12"

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
static uint8_t typeRielloProtocol;

static uint8_t input_monophase;
static uint8_t output_monophase;

static unsigned int offdelay = DEFAULT_OFFDELAY;
static unsigned int bootdelay = DEFAULT_BOOTDELAY;

static TRielloData DevData;

/* Flag for estimation of battery.runtime and battery.charge */
static int localcalculation = 0;
static int localcalculation_logged = 0;
/* NOTE: Do not change these default, they refer to battery.voltage.nominal=12.0
 * and used in related maths later */
static double batt_volt_nom = 12.0;
static double batt_volt_low = 10.4;
static double batt_volt_high = 13.0;

/**********************************************************************
 * char_read (char *bytes, size_t size, int read_timeout)
 *
 * reads size bytes from the serial port
 *
 * bytes			- buffer to store the data
 * size				- size of the data to get
 * read_timeout 	- serial timeout (in milliseconds)
 *
 * return -1 on error, -2 on timeout, nb_bytes_readen on success
 *
 * See also select_read() in common.c (TODO: standardize codebase)
 *
 *********************************************************************/
static ssize_t char_read (char *bytes, size_t size, int read_timeout)
{
	ssize_t readen = 0;

#ifndef WIN32
	int rc = 0;
	struct timeval serial_timeout;
	fd_set readfs;

	FD_ZERO (&readfs);
	FD_SET (upsfd, &readfs);

	serial_timeout.tv_usec = (read_timeout % 1000) * 1000;
	serial_timeout.tv_sec = (read_timeout / 1000);

	rc = select (upsfd + 1, &readfs, NULL, NULL, &serial_timeout);
	if (0 == rc)
		return -2;			/* timeout */

	if (FD_ISSET (upsfd, &readfs)) {
#else
		DWORD timeout;
		COMMTIMEOUTS TOut;

		timeout = read_timeout;	/* recast */

		GetCommTimeouts(upsfd, &TOut);
		TOut.ReadIntervalTimeout = MAXDWORD;
		TOut.ReadTotalTimeoutMultiplier = 0;
		TOut.ReadTotalTimeoutConstant = timeout;
		SetCommTimeouts(upsfd, &TOut);
#endif

		ssize_t now;
#ifndef WIN32
		now = read (upsfd, bytes, size - (size_t)readen);
#else
		/* FIXME? for some reason this compiles, but the first
		 * arg to the method should be serial_handler_t* - not
		 * a HANDLE as upsfd is (in main.c)... then again, many
		 * other drivers seem to use it just fine...
		 */
		now = w32_serial_read(upsfd, bytes, size - (size_t)readen, timeout);
#endif

		if (now < 0) {
			return -1;
		}
		else {
			readen += now;
		}
#ifndef WIN32
	}
	else {
		return -1;
	}
#endif
	return readen;
}

/**********************************************************************
 * serial_read (int read_timeout)
 *
 * return data one byte at a time
 *
 * read_timeout - serial timeout (in milliseconds)
 *
 * returns 0 on success, -1 on error, -2 on timeout
 *
 **********************************************************************/
static ssize_t serial_read (int read_timeout, unsigned char *readbuf)
{
	static unsigned char cache[512];
	static unsigned char *cachep = cache;
	static unsigned char *cachee = cache;
	ssize_t recv;
	*readbuf = '\0';

	/* if still data in cache, get it */
	if (cachep < cachee) {
		*readbuf = *cachep++;
		return 0;
		/* return (int) *cachep++; */
	}
	recv = char_read ((char *)cache, 1, read_timeout);

	if ((recv == -1) || (recv == -2))
		return recv;

	cachep = cache;
	cachee = cache + recv;
	cachep = cache;
	cachee = cache + recv;

	if (recv) {
		upsdebugx(5,"received: %02x", *cachep);
		*readbuf = *cachep++;
		return 0;
	}
	return -1;
}

static void riello_serialcomm(uint8_t* arg_bufIn, uint8_t typedev)
{
	time_t realt, nowt;
	uint8_t commb = 0;

	realt = time(NULL);
	while (wait_packet) {
		serial_read(1000, &commb);
		nowt = time(NULL);
		commbyte = commb;
		riello_parse_serialport(typedev, arg_bufIn, gpser_error_control);

		if ((nowt - realt) > 4)
			break;
	}
}

static int get_ups_nominal(void)
{
	uint8_t length;

	riello_init_serial();

	length = riello_prepare_gn(&bufOut[0], gpser_error_control);

	if (ser_send_buf(upsfd, bufOut, length) == 0) {
		upsdebugx (3, "Communication error while writing to port");
		return -1;
	}

	riello_serialcomm(&bufIn[0], DEV_RIELLOGPSER);

	if (!wait_packet && foundbadcrc) {
		upsdebugx (3, "Get nominal Ko: bad CRC or Checksum");
		return -1;
	}

	/* mandatory */
	if (!wait_packet && foundnak) {
		upsdebugx (3, "Get nominal Ko: command not supported");
		return -1;
	}

	upsdebugx (3, "Get nominal Ok: received byte %u", buf_ptr_length);

	riello_parse_gn(&bufIn[0], &DevData);

	return 0;
}

static int get_ups_status(void)
{
	uint8_t numread, length;

	riello_init_serial();

	length = riello_prepare_rs(&bufOut[0], gpser_error_control);

	if (ser_send_buf(upsfd, bufOut, length) == 0) {
		upsdebugx (3, "Communication error while writing to port");
		return -1;
	}

	if (input_monophase)
		numread = LENGTH_RS_MM;
	else if (output_monophase)
		numread = LENGTH_RS_TM;
	else
		numread = LENGTH_RS_TT;

	riello_serialcomm(&bufIn[0], DEV_RIELLOGPSER);

	if (!wait_packet && foundbadcrc) {
		upsdebugx (3, "Get status Ko: bad CRC or Checksum");
		return -1;
	}

	/* mandatory */
	if (!wait_packet && foundnak) {
		upsdebugx (3, "Get status Ko: command not supported");
		return -1;
	}

	upsdebugx (3, "Get status Ok: received byte %u", buf_ptr_length);

	riello_parse_rs(&bufIn[0], &DevData, numread);

	return 0;
}

static int get_ups_extended(void)
{
	uint8_t length;

	riello_init_serial();

	length = riello_prepare_re(&bufOut[0], gpser_error_control);

	if (ser_send_buf(upsfd, bufOut, length) == 0) {
		upsdebugx (3, "Communication error while writing to port");
		return -1;
	}

	riello_serialcomm(&bufIn[0], DEV_RIELLOGPSER);

	if (!wait_packet && foundbadcrc) {
		upsdebugx (3, "Get extended Ko: bad CRC or Checksum");
		return -1;
	}

	/* optonal */
	if (!wait_packet && foundnak) {
		upsdebugx (3, "Get extended Ko: command not supported");
		return 0;
	}

	upsdebugx (3, "Get extended Ok: received byte %u", buf_ptr_length);

	riello_parse_re(&bufIn[0], &DevData);

	return 0;
}

/* Not static, exposed via header. Not used though, currently... */
int get_ups_statuscode(void)
{
	uint8_t length;

	riello_init_serial();

	length = riello_prepare_rc(&bufOut[0], gpser_error_control);

	if (ser_send_buf(upsfd, bufOut, length) == 0) {
		upsdebugx (3, "Communication error while writing to port");
		return -1;
	}

	riello_serialcomm(&bufIn[0], DEV_RIELLOGPSER);

	if (!wait_packet && foundbadcrc) {
		upsdebugx (3, "Get statuscode Ko: bad CRC or Checksum");
		return -1;
	}

	/* optional */
	if (!wait_packet && foundnak) {
		upsdebugx (3, "Get statuscode Ko: command not supported");
		return 0;
	}

	upsdebugx (3, "Get statuscode Ok: received byte %u", buf_ptr_length);

	riello_parse_rc(&bufIn[0], &DevData);

	return 0;
}

static int get_ups_sentr(void)
{
	uint8_t length;

	riello_init_serial();

	bufOut[0] = requestSENTR;

	if (requestSENTR == SENTR_EXT176) {
		bufOut[1] = 103;
		bufOut[2] = 1;
		bufOut[3] = 0;
		bufOut[4] = 24;
		length = 5;
	}
	else
		length = 1;

	if (ser_send_buf(upsfd, bufOut, length) == 0) {
		upsdebugx (3, "Communication error while writing to port");
		return -1;
	}

	riello_serialcomm(&bufIn[0], DEV_RIELLOSENTRY);

	if (!wait_packet && foundbadcrc) {
		upsdebugx (3, "Get sentry Ko: bad CRC or Checksum");
		return -1;
	}

	/* mandatory */
	if (!wait_packet && foundnak) {
		upsdebugx (3, "Get sentry Ko: command not supported");
		return -1;
	}

	upsdebugx (3, "Get sentry Ok: received byte %u", buf_ptr_length);

	riello_parse_sentr(&bufIn[0], &DevData);

	return 0;
}

static int riello_instcmd(const char *cmdname, const char *extra)
{
	uint8_t length;
	uint16_t delay;
	const char	*delay_char;

	if (!riello_test_bit(&DevData.StatusCode[0], 1)) {
		if (!strcasecmp(cmdname, "load.off")) {
			delay = 0;
			riello_init_serial();

			if (typeRielloProtocol == DEV_RIELLOGPSER)
				length = riello_prepare_cs(bufOut, gpser_error_control, delay);
			else
				length = riello_prepare_shutsentr(bufOut, delay);

			if (ser_send_buf(upsfd, bufOut, length) == 0) {
				upsdebugx (3, "Command load.off communication error");
				return STAT_INSTCMD_FAILED;
			}

			riello_serialcomm(&bufIn[0], typeRielloProtocol);
			if (!wait_packet && foundbadcrc) {
				upsdebugx (3, "Command load.off Ko: bad CRC or Checksum");
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundnak) {
				upsdebugx (3, "Command load.off Ko: command not supported");
				return STAT_INSTCMD_FAILED;
			}

			upsdebugx (3, "Command load.off Ok");
			return STAT_INSTCMD_HANDLED;
		}

		if (!strcasecmp(cmdname, "load.off.delay")) {
			int	ipv;
			delay_char = dstate_getinfo("ups.delay.shutdown");
			ipv = atoi(delay_char);
			if (ipv < 0 || (intmax_t)ipv > (intmax_t)UINT16_MAX) return STAT_INSTCMD_FAILED;
			delay = (uint16_t)ipv;
			riello_init_serial();

			if (typeRielloProtocol == DEV_RIELLOGPSER)
				length = riello_prepare_cs(bufOut, gpser_error_control, delay);
			else
				length = riello_prepare_shutsentr(bufOut, delay);

			if (ser_send_buf(upsfd, bufOut, length) == 0) {
				upsdebugx (3, "Command load.off delay communication error");
				return STAT_INSTCMD_FAILED;
			}

			riello_serialcomm(&bufIn[0], typeRielloProtocol);
			if (!wait_packet && foundbadcrc) {
				upsdebugx (3, "Command load.off.delay Ko: bad CRC or Checksum");
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundnak) {
				upsdebugx (3, "Command load.off.delay Ko: command not supported");
				return STAT_INSTCMD_FAILED;
			}

			upsdebugx (3, "Command load.off delay Ok");
			return STAT_INSTCMD_HANDLED;
		}

		if (!strcasecmp(cmdname, "load.on")) {
			delay = 0;
			riello_init_serial();

			if (typeRielloProtocol == DEV_RIELLOGPSER)
				length = riello_prepare_cr(bufOut, gpser_error_control, delay);
			else {
				length = riello_prepare_setrebsentr(bufOut, delay);

				if (ser_send_buf(upsfd, bufOut, length) == 0) {
					upsdebugx (3, "Command load.on communication error");
					return STAT_INSTCMD_FAILED;
				}

				riello_serialcomm(&bufIn[0], typeRielloProtocol);
				if (!wait_packet && foundbadcrc) {
					upsdebugx (3, "Command load.on Ko: bad CRC or Checksum");
					return STAT_INSTCMD_FAILED;
				}

				if (!wait_packet && foundnak) {
					upsdebugx (3, "Command load.on Ko: command not supported");
					return STAT_INSTCMD_FAILED;
				}

				length = riello_prepare_rebsentr(bufOut, delay);
			}

			if (ser_send_buf(upsfd, bufOut, length) == 0) {
				upsdebugx (3, "Command load.on communication error");
				return STAT_INSTCMD_FAILED;
			}

			riello_serialcomm(&bufIn[0], typeRielloProtocol);
			if (!wait_packet && foundbadcrc) {
				upsdebugx (3, "Command load.on Ko: bad CRC or Checksum");
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundnak) {
				upsdebugx (3, "Command load.on Ko: command not supported");
				return STAT_INSTCMD_FAILED;
			}

			upsdebugx (3, "Command load.on Ok");
			return STAT_INSTCMD_HANDLED;
		}

		if (!strcasecmp(cmdname, "load.on.delay")) {
			int	ipv;
			delay_char = dstate_getinfo("ups.delay.reboot");
			ipv = atoi(delay_char);
			if (ipv < 0 || (intmax_t)ipv > (intmax_t)UINT16_MAX) return STAT_INSTCMD_FAILED;
			delay = (uint16_t)ipv;

			riello_init_serial();

			if (typeRielloProtocol == DEV_RIELLOGPSER)
				length = riello_prepare_cr(bufOut, gpser_error_control, delay);
			else {
				length = riello_prepare_setrebsentr(bufOut, delay);

				if (ser_send_buf(upsfd, bufOut, length) == 0) {
					upsdebugx (3, "Command load.on delay communication error");
					return STAT_INSTCMD_FAILED;
				}

				riello_serialcomm(&bufIn[0], typeRielloProtocol);
				if (!wait_packet && foundbadcrc) {
					upsdebugx (3, "Command load.on delay Ko: bad CRC or Checksum");
					return STAT_INSTCMD_FAILED;
				}

				if (!wait_packet && foundnak) {
					upsdebugx (3, "Command load.on delay Ko: command not supported");
					return STAT_INSTCMD_FAILED;
				}

				length = riello_prepare_rebsentr(bufOut, delay);
			}

			if (ser_send_buf(upsfd, bufOut, length) == 0) {
				upsdebugx (3, "Command load.on delay communication error");
				return STAT_INSTCMD_FAILED;
			}

			riello_serialcomm(&bufIn[0], typeRielloProtocol);
			if (!wait_packet && foundbadcrc) {
				upsdebugx (3, "Command load.on.delay Ko: bad CRC or Checksum");
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundnak) {
				upsdebugx (3, "Command load.on.delay Ko: command not supported");
				return STAT_INSTCMD_FAILED;
			}

			upsdebugx (3, "Command load.on delay Ok");
			return STAT_INSTCMD_HANDLED;
		}
	}
	else {
		if (!strcasecmp(cmdname, "shutdown.return")) {
			int	ipv;
			delay_char = dstate_getinfo("ups.delay.shutdown");
			ipv = atoi(delay_char);
			if (ipv < 0 || (intmax_t)ipv > (intmax_t)UINT16_MAX) return STAT_INSTCMD_FAILED;
			delay = (uint16_t)ipv;

			riello_init_serial();

			if (typeRielloProtocol == DEV_RIELLOGPSER)
				length = riello_prepare_cs(bufOut, gpser_error_control, delay);
			else
				length = riello_prepare_shutsentr(bufOut, delay);

			if (ser_send_buf(upsfd, bufOut, length) == 0) {
				upsdebugx (3, "Command shutdown.return communication error");
				return STAT_INSTCMD_FAILED;
			}

			riello_serialcomm(&bufIn[0], typeRielloProtocol);
			if (!wait_packet && foundbadcrc) {
				upsdebugx (3, "Command shutdown.return Ko: bad CRC or Checksum");
				return STAT_INSTCMD_FAILED;
			}

			if (!wait_packet && foundnak) {
				upsdebugx (3, "Command shutdown.return Ko: command not supported");
				return STAT_INSTCMD_FAILED;
			}

			upsdebugx (3, "Command shutdown.return Ok");
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "shutdown.stop")) {
		riello_init_serial();

		if (typeRielloProtocol == DEV_RIELLOGPSER)
			length = riello_prepare_cd(bufOut, gpser_error_control);
		else
			length = riello_prepare_cancelsentr(bufOut);

		if (ser_send_buf(upsfd, bufOut, length) == 0) {
			upsdebugx (3, "Command shutdown.stop communication error");
			return STAT_INSTCMD_FAILED;
		}

		riello_serialcomm(&bufIn[0], typeRielloProtocol);
		if (!wait_packet && foundbadcrc) {
			upsdebugx (3, "Command shutdown.stop Ko: bad CRC or Checksum");
			return STAT_INSTCMD_FAILED;
		}

		if (!wait_packet && foundnak) {
			upsdebugx (3, "Command shutdown.stop Ko: command not supported");
			return STAT_INSTCMD_FAILED;
		}

		upsdebugx (3, "Command shutdown.stop Ok");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.panel.start")) {
		riello_init_serial();
		length = riello_prepare_tp(bufOut, gpser_error_control);

		if (ser_send_buf(upsfd, bufOut, length) == 0) {
			upsdebugx (3, "Command test.panel.start communication error");
			return STAT_INSTCMD_FAILED;
		}

		riello_serialcomm(&bufIn[0], DEV_RIELLOGPSER);
		if (!wait_packet && foundbadcrc) {
			upsdebugx (3, "Command test.panel.start Ko: bad CRC or Checksum");
			return STAT_INSTCMD_FAILED;
		}

		if (!wait_packet && foundnak) {
			upsdebugx (3, "Command test.panel.start Ko: command not supported");
			return STAT_INSTCMD_FAILED;
		}

		upsdebugx (3, "Command test.panel.start Ok");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start")) {
		riello_init_serial();
		if (typeRielloProtocol == DEV_RIELLOGPSER)
			length = riello_prepare_tb(bufOut, gpser_error_control);
		else
			length = riello_prepare_tbsentr(bufOut);

		if (ser_send_buf(upsfd, bufOut, length) == 0) {
			upsdebugx (3, "Command test.battery.start communication error");
			return STAT_INSTCMD_FAILED;
		}

		riello_serialcomm(&bufIn[0], typeRielloProtocol);
		if (!wait_packet && foundbadcrc) {
			upsdebugx (3, "Command battery.start Ko: bad CRC or Checksum");
			return STAT_INSTCMD_FAILED;
		}

		if (!wait_packet && foundnak) {
			upsdebugx (3, "Command battery.start Ko: command not supported");
			return STAT_INSTCMD_FAILED;
		}

		upsdebugx (3, "Command test.battery.start Ok");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

static int start_ups_comm(void)
{
	uint8_t length;

	upsdebugx (2, "entering start_ups_comm()\n");

	riello_init_serial();

	if (typeRielloProtocol == DEV_RIELLOGPSER) {
		length = riello_prepare_gi(&bufOut[0]);

		if (ser_send_buf(upsfd, bufOut, length) == 0) {
			upsdebugx (3, "Communication error while writing to port");
			return -1;
		}

		riello_serialcomm(&bufIn[0], DEV_RIELLOGPSER);
	}
	else {
		bufOut[0] = 192;
		length = 1;

		if (ser_send_buf(upsfd, bufOut, length) == 0) {
			upsdebugx (3, "Communication error while writing to port");
			return -1;
		}

		riello_serialcomm(&bufIn[0], DEV_RIELLOSENTRY);
	}

	if (!wait_packet && foundbadcrc) {
		upsdebugx (3, "Get identif Ko: bad CRC or Checksum");
		return 1;
	}

	if (!wait_packet && foundnak) {
		upsdebugx (3, "Get identif Ko: command not supported");
		return 1;
	}

	upsdebugx (3, "Get identif Ok: received byte %u", buf_ptr_length);
	return 0;

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

	if (typeRielloProtocol == DEV_RIELLOGPSER)
		riello_parse_gi(&bufIn[0], &DevData);
	else
		riello_parse_sentr(&bufIn[0], &DevData);

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

	if (typeRielloProtocol == DEV_RIELLOGPSER) {
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
		}
	}
	else {
		if (get_ups_sentr() == 0) {
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

	dstate_setinfo("ups.delay.shutdown", "%u", offdelay);
	dstate_setflags("ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.shutdown", 3);
	dstate_setinfo("ups.delay.reboot", "%u", bootdelay);
	dstate_setflags("ups.delay.reboot", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.reboot", 3);


	if (typeRielloProtocol == DEV_RIELLOGPSER)
		dstate_addcmd("test.panel.start");

	/* install handlers */
/*	upsh.setvar = hid_set_value; setvar; */
	upsh.instcmd = riello_instcmd;
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

	if (typeRielloProtocol == DEV_RIELLOGPSER) {
		stat = get_ups_status();
		upsdebugx(1, "get_ups_status() %d", stat );
	} else {
		stat = get_ups_sentr();
		upsdebugx(1, "get_ups_sentr() %d", stat );
	}

	if (stat < 0) {
		if (countlost < COUNTLOST)
			countlost++;
		return;
	}

	if (typeRielloProtocol == DEV_RIELLOGPSER) {
		if (get_ups_extended() == 0)
			getextendedOK = 1;
		else
			getextendedOK = 0;
	}
	else
		getextendedOK = 1;

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

		dstate_setinfo("battery.charge", "%u", battcharge);
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
			dstate_setinfo("battery.runtime", "%u", DevData.BatTime*60);
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
	 * poll_interval = 2;
	 */
}

void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */
	int	retry;

	/* maybe try to detect the UPS here, but try a shutdown even if
		it doesn't respond at first if possible */

	/* replace with a proper shutdown function */


	/* you may have to check the line status since the commands
		for toggling power are frequently different for OL vs. OB */

	/* OL: this must power cycle the load if possible */

	/* OB: the load must remain off until the power returns */
	upsdebugx(2, "upsdrv Shutdown execute");

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


/*
static int setvar(const char *varname, const char *val)
{
	if (!strcasecmp(varname, "ups.test.interval")) {
		ser_send_buf(upsfd, ...);
		return STAT_SET_HANDLED;
	}

	upslogx(LOG_NOTICE, "setvar: unknown variable [%s]", varname);
	return STAT_SET_UNKNOWN;
}
*/

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	/* allow '-x xyzzy' */
	/* addvar(VAR_FLAG, "xyzzy", "Enable xyzzy mode"); */

	/* allow '-x foo=<some value>' */
	/* addvar(VAR_VALUE, "foo", "Override foo setting"); */

	addvar(VAR_FLAG, "localcalculation", "Calculate battery charge and runtime locally");
}

void upsdrv_initups(void)
{
	upsdebugx(2, "entering upsdrv_initups()");

	upsfd = ser_open(device_path);

	riello_comm_setup(device_path);

	/* probe ups type */

	/* to get variables and flags from the command line, use this:
	 *
	 * first populate with upsdrv_buildvartable above, then...
	 *
	 *	  				set flag foo : /bin/driver -x foo
	 * set variable 'cable' to '1234' : /bin/driver -x cable=1234
	 *
	 * to test flag foo in your code:
	 *
	 * 	if (testvar("foo"))
	 * 		do_something();
	 *
	 * to show the value of cable:
	 *
	 *	if ((cable = getval("cable")))
	 *		printf("cable is set to %s\n", cable);
	 *	else
	 *		printf("cable is not set!\n");
	 *
	 * don't use NULL pointers - test the return result first!
	 */

	/* the upsh handlers can't be done here, as they get initialized
	 * shortly after upsdrv_initups returns to main.
	 */

	/* don't try to detect the UPS here */

	/* initialise communication */
}

void upsdrv_cleanup(void)
{
	/* free(dynamic_mem); */
	ser_close(upsfd, device_path);
}

void riello_comm_setup(const char *port)
{
	uint8_t length;

	upsdebugx(2, "set baudrate 9600");
	ser_set_speed(upsfd, device_path, B9600);

	upsdebugx(2, "try to detect SENTR");
	riello_init_serial();
	bufOut[0] = 192;
	ser_send_buf(upsfd, bufOut, 1);

	riello_serialcomm(&bufIn[0], DEV_RIELLOSENTRY);

	if (buf_ptr_length == 103) {
		typeRielloProtocol = DEV_RIELLOSENTRY;
		upslogx(LOG_INFO, "Connected to UPS SENTR on %s with baudrate %d", port, 9600);
		return;
	}

	upsdebugx(2, "try to detect GPSER");
	riello_init_serial();
	length = riello_prepare_gi(&bufOut[0]);

	ser_send_buf(upsfd, bufOut, length);

	riello_serialcomm(&bufIn[0], DEV_RIELLOGPSER);

	if (!wait_packet && !foundbadcrc && !foundnak) {
		typeRielloProtocol = DEV_RIELLOGPSER;
		upslogx(LOG_INFO, "Connected to UPS GPSER on %s with baudrate %d", port, 9600);
		return;
	}

	upsdebugx(2, "set baudrate 1200");
	ser_set_speed(upsfd, device_path, B1200);

	upsdebugx(2, "try to detect SENTR");
	riello_init_serial();
	bufOut[0] = 192;
	ser_send_buf(upsfd, bufOut, 1);

	riello_serialcomm(&bufIn[0], DEV_RIELLOSENTRY);

	if (buf_ptr_length == 103) {
		typeRielloProtocol = DEV_RIELLOSENTRY;
		upslogx(LOG_INFO, "Connected to UPS SENTR on %s with baudrate %d", port, 1200);
		return;
	}

	upsdebugx(2, "try to detect GPSER");
	riello_init_serial();
	length = riello_prepare_gi(&bufOut[0]);

	ser_send_buf(upsfd, bufOut, length);

	riello_serialcomm(&bufIn[0], DEV_RIELLOGPSER);

	if (!wait_packet && !foundbadcrc && !foundnak) {
		typeRielloProtocol = DEV_RIELLOGPSER;
		upslogx(LOG_INFO, "Connected to UPS GPSER on %s with baudrate %d", port, 1200);
		return;
	}

	fatalx(EXIT_FAILURE, "Can't connect to the UPS on port %s!\n", port);
}

