/*
 * ivtscd.c - model specific routines for the IVT Solar Controller driver
 *
 * Copyright (C) 2009  Arjen de Korte <adkorte-guest@alioth.debian.org>
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

#define DRIVER_NAME	"IVT Solar Controller driver"
#define DRIVER_VERSION	"0.02"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arjen de Korte <adkorte-guest@alioth.debian.org>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

static struct {
	struct {
		float	act;
		float	low;
		float	min;
		float	nom;
		float	max;
	} voltage;
	struct {
		float	min;
		float	act;
		float	max;
	} current;
	float	temperature;
} battery;

static int ivt_status(void)
{
	char	reply[SMALLBUF];
	int	ret, i, j = 0;

	ser_flush_io(upsfd);

	/*
	 * send: F\n
	 */
	ret = ser_send(upsfd, "F");

	if (ret < 0) {
		upsdebug_with_errno(3, "send");
		return -1;
	}

	if (ret == 0) {
		upsdebug_with_errno(3, "send: timeout");
		return -1;
	}

	upsdebugx(3, "send: F");
	sleep(1);	/* allow controller some time to digest this */

	/*
	 * read: R:12,57;- 1,1;20;12,57;13,18;- 2,1; 1,5;\n
	 */
	ret = ser_get_buf(upsfd, reply, sizeof(reply), 1, 0);

	if (ret < 0) {
		upsdebug_with_errno(3, "read");
		return -1;
	}

	if (ret == 0) {
		upsdebugx(3, "read: timeout");
		return -1;
	}

	upsdebugx(3, "read: %.*s", (int)strcspn(reply, "\r\n"), reply);
	upsdebug_hex(4, "  \\_", reply, ret);

	for (i = 0; i < ret; i++) {
		switch(reply[i])
		{
		case ',':	/* convert ',' to '.' */
			reply[j++] = '.';
			break;
		case ' ':	/* skip over white space */
		case '\0':	/* skip over null characters */
			break;
		default:	/* leave the rest as is */
			reply[j++] = reply[i];
			break;
		}
	}

	reply[j++] = '\0';

	ret = sscanf(reply, "R:%f;%f;%f;%f;%f;%f;%f;", &battery.voltage.act, &battery.current.act, &battery.temperature,
					&battery.voltage.min, &battery.voltage.max, &battery.current.min, &battery.current.max);

	upsdebugx(3, "Parsed %d parameters from reply", ret);
	return ret;
}

static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "reset.input.minmax")) {
		ser_send(upsfd, "L");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	if (ivt_status() < 7) {
		fatal_with_errno(EXIT_FAILURE, "IVT Solar Controller not detected");
	}

	/* set the device general information */
	dstate_setinfo("device.mfr", "IVT");
	dstate_setinfo("device.model", "Solar Controller Device");
	dstate_setinfo("device.type", "scd");

	dstate_addcmd("reset.input.minmax");

	upsh.instcmd = instcmd;
}

void upsdrv_updateinfo(void)
{
	if (ivt_status() < 7) {
		dstate_datastale();
		return;
	}

	dstate_setinfo("battery.voltage", "%.2f", battery.voltage.act);
	dstate_setinfo("battery.voltage.minimum", "%.2f", battery.voltage.min);
	dstate_setinfo("battery.voltage.maximum", "%.2f", battery.voltage.max);

	dstate_setinfo("battery.current", "%.1f", battery.current.act);
	dstate_setinfo("battery.current.minimum", "%.1f", battery.current.min);
	dstate_setinfo("battery.current.maximum", "%.1f", battery.current.max);

	dstate_setinfo("battery.temperature", "%.0f", battery.temperature);

	status_init();

	if (battery.current.act > 0) {
		status_set("OL");		/* charging */
	} else {
		status_set("OB");		/* discharging */
	}

	if (battery.voltage.act < battery.voltage.low) {
		status_set("LB");
	}

	status_commit();

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	while (1) {

		if (ivt_status() < 7) {
			continue;
		}

		if (battery.voltage.act < battery.voltage.nom) {
			continue;
		}

		fatalx(EXIT_SUCCESS, "Power is back!");
	}
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}

void upsdrv_initups(void)
{
	struct termios	tio;
	const char	*val;

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B1200);

	if (tcgetattr(upsfd, &tio)) {
		fatal_with_errno(EXIT_FAILURE, "tcgetattr");
	}

	/*
	 * Use canonical mode input processing (to read reply line)
	 */
	tio.c_lflag |= ICANON;	/* Canonical input (erase and kill processing) */
	tio.c_iflag |= IGNCR;	/* Ignore CR */
	tio.c_iflag |= IGNBRK;	/* Ignore break condition */
	tio.c_oflag |= ONLCR;	/* Map NL to CR-NL on output */

	tio.c_cc[VEOF] = _POSIX_VDISABLE;
	tio.c_cc[VEOL] = _POSIX_VDISABLE;
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

	/*
	 * Set DTR and clear RTS to provide power for the serial interface.
	 */
	ser_set_dtr(upsfd, 1);
	ser_set_rts(upsfd, 0);

	val = dstate_getinfo("battery.voltage.nominal");
	battery.voltage.nom = (val) ? strtod(val, NULL) : 12.00;

	val = dstate_getinfo("battery.voltage.low");
	battery.voltage.low = (val) ? strtod(val, NULL) : 10.80;

	if (battery.voltage.nom <= battery.voltage.low) {
		fatalx(EXIT_FAILURE, "Nominal battery voltage must be higher than low battery voltage!");
	}
}

void upsdrv_cleanup(void)
{
	ser_set_dtr(upsfd, 0);
	ser_close(upsfd, device_path);
}
