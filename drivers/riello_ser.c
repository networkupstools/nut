/*
 * riello_ser.c: support for Riello serial protocol based UPSes
 *
 * A document describing the protocol implemented by this driver can be
 * found online at "http://www.networkupstools.org/ups-protocols/riello/PSGPSER-0104.pdf"
 * and "http://www.networkupstools.org/ups-protocols/riello/PSSENTR-0100.pdf".
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

#include <string.h>
#include <stdint.h>

#include "config.h"
#include "main.h"
#include "serial.h"
#include "timehead.h"
#include "hidparser.h"
#include "hidtypes.h"
#include "common.h" /* for upsdebugx() etc */
#include "riello.h"

#define DRIVER_NAME	"Riello serial driver"
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
static uint8_t typeRielloProtocol;

static uint8_t input_monophase;
static uint8_t output_monophase;

extern uint8_t commbyte;
extern uint8_t wait_packet;
extern uint8_t foundnak;
extern uint8_t foundbadcrc;
extern uint8_t buf_ptr_length;
extern uint8_t requestSENTR;

static TRielloData DevData;

/**********************************************************************
 * char_read (char *bytes, int size, int read_timeout)
 *
 * reads size bytes from the serial port
 *
 * bytes	  			- buffer to store the data
 * size				- size of the data to get
 * read_timeout 	- serial timeout (in milliseconds)
 *
 * return -1 on error, -2 on timeout, nb_bytes_readen on success
 *
 *********************************************************************/
static int char_read (char *bytes, int size, int read_timeout)
{
	struct timeval serial_timeout;
	fd_set readfs;
	int readen = 0;
	int rc = 0;

	FD_ZERO (&readfs);
	FD_SET (upsfd, &readfs);

	serial_timeout.tv_usec = (read_timeout % 1000) * 1000;
	serial_timeout.tv_sec = (read_timeout / 1000);

	rc = select (upsfd + 1, &readfs, NULL, NULL, &serial_timeout);
	if (0 == rc)
		return -2;			/* timeout */

	if (FD_ISSET (upsfd, &readfs)) {
		int now = read (upsfd, bytes, size - readen);

		if (now < 0) {
			return -1;
		}
		else {
			readen += now;
		}
	}
	else {
		return -1;
	}
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
static int serial_read (int read_timeout, unsigned char *readbuf)
{
	static unsigned char cache[512];
	static unsigned char *cachep = cache;
	static unsigned char *cachee = cache;
	int recv;
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

static int get_ups_nominal()
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

static int get_ups_status()
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

static int get_ups_extended()
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

static int get_ups_statuscode()
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

static int get_ups_sentr()
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
			delay_char = dstate_getinfo("ups.delay.shutdown");
			delay = atoi(delay_char);
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
			delay_char = dstate_getinfo("ups.delay.reboot");
			delay = atoi(delay_char);
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
			delay_char = dstate_getinfo("ups.delay.shutdown");
			delay = atoi(delay_char);
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

static int start_ups_comm()
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

	ret = start_ups_comm();

	if (ret < 0)
		fatalx(EXIT_FAILURE, "No communication with UPS");
	else if (ret > 0)
		fatalx(EXIT_FAILURE, "Bad checksum or NACK");
	else
		upsdebugx(2, "Communication with UPS established");

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

	if (typeRielloProtocol == DEV_RIELLOGPSER) {
		if (get_ups_nominal() == 0) {
			dstate_setinfo("ups.realpower.nominal", "%u", DevData.NomPowerKW);
			dstate_setinfo("ups.power.nominal", "%u", DevData.NomPowerKVA);
			dstate_setinfo("output.voltage.nominal", "%u", DevData.NominalUout);
			dstate_setinfo("output.frequency.nominal", "%.1f", DevData.NomFout/10.0);
			dstate_setinfo("battery.voltage.nominal", "%u", DevData.NomUbat);
			dstate_setinfo("battery.capacity", "%u", DevData.NomBatCap);
		}
	}
	else {
		if (get_ups_sentr() == 0) {
			dstate_setinfo("ups.realpower.nominal", "%u", DevData.NomPowerKW);
			dstate_setinfo("ups.power.nominal", "%u", DevData.NomPowerKVA);
			dstate_setinfo("output.voltage.nominal", "%u", DevData.NominalUout);
			dstate_setinfo("output.frequency.nominal", "%.1f", DevData.NomFout/10.0);
			dstate_setinfo("battery.voltage.nominal", "%u", DevData.NomUbat);
			dstate_setinfo("battery.capacity", "%u", DevData.NomBatCap);
		}
	}


	/* commands ----------------------------------------------- */
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	dstate_addcmd("load.off.delay");
	dstate_addcmd("load.on.delay");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("test.battery.start");

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

	upsdebugx(1, "countlost %d",countlost);

	if (countlost > 0){
		upsdebugx(1, "Communication with UPS is lost: status read failed!");

		if (countlost == COUNTLOST) {
			dstate_datastale();
			upslogx(LOG_WARNING, "Communication with UPS is lost: status read failed!");
		}
	}

	if (typeRielloProtocol == DEV_RIELLOGPSER)
		stat = get_ups_status();
	else
		stat = get_ups_sentr();

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
	 * poll_interval = 2;
	 */
}

void upsdrv_shutdown(void)
	__attribute__((noreturn));

void upsdrv_shutdown(void)
{
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

