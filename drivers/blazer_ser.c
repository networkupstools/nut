/*
 * blazer_ser.c: support for Megatec/Q1 serial protocol based UPSes
 *
 * OBSOLETION WARNING: Please to not base new development on this
 * codebase, instead create a new subdriver for nutdrv_qx which
 * generally covers all Megatec/Qx protocol family and aggregates
 * device support from such legacy drivers over time.
 *
 * A document describing the protocol implemented by this driver can be
 * found online at "http://www.networkupstools.org/protocols/megatec.html".
 *
 * Copyright (C) 2008 - Arjen de Korte <adkorte-guest@alioth.debian.org>
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
#include "blazer.h"

#define DRIVER_NAME	"Megatec/Q1 protocol serial driver"
#define DRIVER_VERSION	"1.57"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arjen de Korte <adkorte-guest@alioth.debian.org>",
	DRV_BETA,
	{ NULL }
};

#define SER_WAIT_SEC	1 /* 3 seconds for Best UPS */

/*
 * Generic command processing function. Send a command and read a reply.
 * Returns < 0 on error, 0 on timeout and the number of bytes read on
 * success.
 */
ssize_t blazer_command(const char *cmd, char *buf, size_t buflen)
{
#ifndef TESTING
	ssize_t	ret;

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
		{ "Q1\r", "(215.0 195.0 230.0 014 49.0 2.27 30.0 00101000\r" },
		{ "F\r",  "#230.0 000 024.0 50.0\r" },
		{ "I\r",  "#NOT_A_LIVE_UPS  TESTING    TESTING   \r" },
		{ NULL }
	};

	int	i;

	memset(buf, 0, buflen);

	for (i = 0; cmd && testing[i].cmd; i++) {

		if (strcasecmp(cmd, testing[i].cmd)) {
			continue;
		}

		/* TODO: Range-check int vs ssize_t values */
		return (ssize_t)snprintf(buf, buflen, "%s", testing[i].answer);
	}

	return (ssize_t)snprintf(buf, buflen, "%s", testing[i].cmd);
#endif
}


void upsdrv_help(void)
{
	printf("Read The Fine Manual ('man 8 blazer_ser')\n");
}


void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "cablepower", "Set cable power for serial interface");

	blazer_makevartable();
}


void upsdrv_initups(void)
{
#ifndef TESTING
	const struct {
		const char	*val;
		const int	dtr;
		const int	rts;
	} cablepower[] = {
		{ "normal",	1, 0 }, /* default */
		{ "reverse",	0, 1 },
		{ "both",	1, 1 },
		{ "none",	0, 0 },
		{ NULL, 0, 0 }
	};

	int	i;

	const char	*val;

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

	val = getval("cablepower");
	for (i = 0; val && cablepower[i].val; i++) {

		if (!strcasecmp(val, cablepower[i].val)) {
			break;
		}
	}

	if (!cablepower[i].val) {
		fatalx(EXIT_FAILURE, "Value '%s' not valid for 'cablepower'", val);
	}

	ser_set_dtr(upsfd, cablepower[i].dtr);
	ser_set_rts(upsfd, cablepower[i].rts);

	/*
	 * Allow some time to settle for the cablepower
	 */
	usleep(100000);
#endif
	blazer_initups();
}


void upsdrv_initinfo(void)
{
	blazer_initinfo();
}


void upsdrv_cleanup(void)
{
#ifndef TESTING
	ser_set_dtr(upsfd, 0);
	ser_close(upsfd, device_path);
#endif
}
