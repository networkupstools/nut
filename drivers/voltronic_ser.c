/*
 * voltronic_ser.c: support for Voltronic Power UPSes
 *
 * A document describing the protocol implemented by this driver can be
 * found online at http://www.networkupstools.org/ups-protocols/
 *
 * Copyright (C)
 *   2013 - Daniele Pezzini <hyouko@gmail.com>
 * Based on blazer_ser.c - Copyright (C)
 *   2008 - Arjen de Korte <adkorte-guest@alioth.debian.org>
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
 */

#include "main.h"
#include "serial.h"
#include "voltronic.h"

#define DRIVER_NAME	"Voltronic Power serial driver"
#define DRIVER_VERSION	"0.04"

/* For testing purposes */
/*#define TESTING*/

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Daniele Pezzini <hyouko@gmail.com>\n" \
	"Arjen de Korte <adkorte-guest@alioth.debian.org>",
	DRV_BETA,
	{ NULL }
};

#define SER_WAIT_SEC	1

/*
 * Generic command processing function. Send a command and read a reply.
 * Returns < 0 on error, 0 on timeout and the number of bytes read on
 * success.
 */
int voltronic_command(const char *cmd, char *buf, size_t buflen)
{
#ifndef TESTING
	int	ret;

	ser_flush_io(upsfd);

	ret = ser_send(upsfd, "%s", cmd);

	if (ret <= 0) {
		upsdebugx(3, "send: %s", ret ? strerror(errno) : "timeout");
		return ret;
	}

	upsdebugx(3, "send: '%.*s'", (int)strcspn(cmd, "\r"), cmd);

	ret = ser_get_buf(upsfd, buf, buflen, SER_WAIT_SEC, 0);

	if (ret <= 0) {
		upsdebugx(3, "read: %s", ret ? strerror(errno) : "timeout");
		return ret;
	}

	upsdebugx(3, "read: '%.*s'", (int)strcspn(buf, "\r"), buf);
	return ret;
#else
	const struct {
		const char	*cmd;
		const char	*answer;
	} testing[] = {
		{ "QGS\r", "(234.9 50.0 229.8 50.0 000.0 000 369.1 ---.- 026.5 ---.- 018.8 100000000001\r" },
		{ "QPI\r", "(PI09\r" },
		{ "QRI\r", "(230.0 004 024.0 50.0\r" },
		{ "QMF\r", "(#####VOLTRONIC\r" },
		{ "I\r", "#-------------   ------     VT12046Q  \r" },
		{ "F\r", "#220.0 000 024.0 50.0\r" },
		{ "QMD\r", "(#######OLHVT1K0 ###1000 80 2/2 230 230 02 12.0\r" },
		{ "QFS\r", "(14 212.1 50.0 005.6 49.9 006 010.6 343.8 ---.- 026.2 021.8 01101100\r" },
		{ "QMOD\r", "(S\r" },
		{ "QVFW\r", "(VERFW:00322.02\r" },
		{ "QID\r", "(685653211455\r" },
		{ "QBV\r", "(026.5 02 01 068 255\r" },
		{ "QFLAG\r", "(EpashcjDbroegfl\r" },
		{ "QWS\r", "(0000000000000000000000000000000000000000000000000000000000000000\r" },
		{ "QHE\r", "(242 218\r" },
		{ "QBYV\r", "(264 170\r" },
		{ "QBYF\r", "(53.0 47.0\r" },
		{ "QSK1\r", "(1\r" },
		{ "QSK2\r", "(0\r" },
		{ "QSK3\r", "(1\r" },
		{ "QSK4\r", "(NAK\r" },
		{ "QSKT1\r", "(008\r" },
		{ "QSKT2\r", "(012\r" },
		{ "QSKT3\r", "(NAK\r" },
		{ "QSKT4\r", "(007\r" },
		{ "RE0\r", "#20\r" },
		{ "W0E24\r", "(ACK\r" },
		{ "PF\r", "(ACK\r" },
		{ "PEA\r", "(ACK\r" },
		{ "PDR\r", "(NAK\r" },
		{ "HEH250\r", "(ACK\r" },
		{ "HEL210\r", "(ACK\r" },
		{ "PHV260\r", "(NAK\r" },
		{ "PLV190\r", "(ACK\r" },
		{ "PGF51.0\r", "(NAK\r" },
		{ "PSF47.5\r", "(ACK\r" },
		{ "BATN3\r", "(ACK\r" },
		{ "BATGN04\r", "(ACK\r" },
		{ "QBT\r", "(01\r" },
		{ "PBT02\r", "(ACK\r" },
		{ "QGR\r", "(00\r" },
		{ "PGR01\r", "(ACK\r" },
		{ "PSK1008\r", "(ACK\r" },
		{ "PSK3987\r", "(ACK\r" },
		{ "PSK2009\r", "(ACK\r" },
		{ "PSK4012\r", "(ACK\r" },
		{ "Q3PV\r", "(123.4 456.4 789.4 012.4 323.4 223.4\r" },
		{ "Q3OV\r", "(253.4 163.4 023.4 143.4 103.4 523.4\r" },
		{ "Q3OC\r", "(109 069 023\r" },
		{ "Q3LD\r", "(005 033 089\r" },
		{ "Q3YV\r", "(303.4 245.4 126.4 222.4 293.4 321.4\r" },
		{ "Q3PC\r", "(002 023 051\r" },
		{ "SOFF\r", "(NAK\r" },
		{ "SON\r", "(ACK\r" },
		{ "T\r", "(NAK\r" },
		{ "TL\r", "(ACK\r" },
		{ "CS\r", "(ACK\r" },
		{ "CT\r", "(NAK\r" },
		{ "BZOFF\r", "(ACK\r" },
		{ "BZON\r", "(ACK\r" },
		{ "S.3R0002\r", "(ACK\r" },
		{ "S02R0024\r", "(NAK\r" },
		{ "S.5\r", "(ACK\r" },
		{ "T.3\r", "(ACK\r" },
		{ "T02\r", "(NAK\r" },
		{ "SKON1\r", "(ACK\r" },
		{ "SKOFF1\r", "(NAK\r" },
		{ "SKON2\r", "(ACK\r" },
		{ "SKOFF2\r", "(ACK\r" },
		{ "SKON3\r", "(NAK\r" },
		{ "SKOFF3\r", "(ACK\r" },
		{ "SKON4\r", "(NAK\r" },
		{ "SKOFF4\r", "(NAK\r" },
		{ "QPAR\r", "(003\r" },
		{ "QPD\r", "(000 240\r" },
		{ "PPD120\r", "(ACK\r" },
		{ "QLDL\r", "(005 080\r" },
		{ "QBDR\r", "(1234\r" },
		{ "QFRE\r", "(50.0 00.0\r" },
		{ "FREH54.0\r", "(ACK\r" },
		{ "FREL47.0\r", "(ACK\r" },
		{ "PEP\r", "(ACK\r" },
		{ "PDP\r", "(ACK\r" },
		{ "PEB\r", "(ACK\r" },
		{ "PDB\r", "(ACK\r" },
		{ "PER\r", "(NAK\r" },
		{ "PDR\r", "(NAK\r" },
		{ "PEO\r", "(ACK\r" },
		{ "PDO\r", "(ACK\r" },
		{ "PEA\r", "(ACK\r" },
		{ "PDA\r", "(ACK\r" },
		{ "PES\r", "(ACK\r" },
		{ "PDS\r", "(ACK\r" },
		{ "PEV\r", "(ACK\r" },
		{ "PDV\r", "(ACK\r" },
		{ "PEE\r", "(ACK\r" },
		{ "PDE\r", "(ACK\r" },
		{ "PEG\r", "(ACK\r" },
		{ "PDG\r", "(NAK\r" },
		{ "PED\r", "(ACK\r" },
		{ "PDD\r", "(ACK\r" },
		{ "PEC\r", "(ACK\r" },
		{ "PDC\r", "(NAK\r" },
		{ "PEF\r", "(NAK\r" },
		{ "PDF\r", "(ACK\r" },
		{ "PEJ\r", "(NAK\r" },
		{ "PDJ\r", "(ACK\r" },
		{ "PEL\r", "(ACK\r" },
		{ "PDL\r", "(ACK\r" },
		{ "PEN\r", "(ACK\r" },
		{ "PDN\r", "(ACK\r" },
		{ "PEQ\r", "(ACK\r" },
		{ "PDQ\r", "(ACK\r" },
		{ "PEW\r", "(NAK\r" },
		{ "PDW\r", "(ACK\r" },
		{ NULL }
	};

	int	i;

	memset(buf, 0, buflen);

	for (i = 0; cmd && testing[i].cmd; i++) {

		if (strcasecmp(cmd, testing[i].cmd)) {
			continue;
		}

		return snprintf(buf, buflen, "%s", testing[i].answer);
	}

	return snprintf(buf, buflen, "%s", testing[i].cmd);
#endif
}


void upsdrv_help(void)
{
	printf("Read The Fine Manual ('man 8 voltronic_ser')\n");
}


void upsdrv_makevartable(void)
{
	voltronic_makevartable();
}


void upsdrv_initups(void)
{
#ifndef TESTING
	struct termios		tio;

	/*
	 * Open and lock the serial port and set the speed to 2400 baud.
	 */
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);

	if (tcgetattr(upsfd, &tio)) {
		fatal_with_errno(EXIT_FAILURE, "tcgetattr");
	}

	/*
	 * Use canonical mode input processing (to read reply line)
	 */
	tio.c_lflag |= ICANON;	/* Canonical input (erase and kill processing) */

	tio.c_cc[VEOF]   = _POSIX_VDISABLE;
	tio.c_cc[VEOL]   = '\r';
	tio.c_cc[VERASE] = _POSIX_VDISABLE;
	tio.c_cc[VINTR]  = _POSIX_VDISABLE;
	tio.c_cc[VKILL]  = _POSIX_VDISABLE;
	tio.c_cc[VQUIT]  = _POSIX_VDISABLE;
	tio.c_cc[VSUSP]  = _POSIX_VDISABLE;
	tio.c_cc[VSTART] = _POSIX_VDISABLE;
	tio.c_cc[VSTOP]  = _POSIX_VDISABLE;

	if (tcsetattr(upsfd, TCSANOW, &tio)) {
		fatal_with_errno(EXIT_FAILURE, "tcsetattr");
	}

	ser_set_dtr(upsfd, 1);
	ser_set_rts(upsfd, 0);

	/*
	 * Allow some time to settle
	 */
	usleep(100000);
#endif
	voltronic_initups();
}


void upsdrv_initinfo(void)
{
	voltronic_initinfo();
}


void upsdrv_cleanup(void)
{
#ifndef TESTING
	ser_set_dtr(upsfd, 0);
	ser_close(upsfd, device_path);
#endif
}
