/*  nutdrv_modbus.c - Driver for Modbus power devices (UPS, meters, ...)
 *
 *  Copyright (C)
 *    2012-2014  Arnaud Quette <arnaud.quette@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * TODO list:
 * - everything...
 * - create the format for the "NUT Modbus definition file"
 * - complete initups
 * - create initinfo
 * - create updateinfo
 * 
 * "NUT Modbus definition file"		xxx.modbus
 * # A comment header may contain information such as
 * # - author and device information
 * # - default communication settings (baudrate, parity, data_bit, stop_bit)
 * device.type: <ups,meter,...>
 * [device.mfr: <Manufacturer name>]
 * [device.model: <Model name>]
 * <nut varname>: <modbus address>, <modbus register>, ..., <convertion function name>
 */

#include "main.h"
#include <modbus.h>

#define DRIVER_NAME	"NUT Modbus driver"
#define DRIVER_VERSION	"0.01"

/* Variables */
modbus_t *ctx = NULL;

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <arnaud.quette@gmail.com>\n",
	DRV_EXPERIMENTAL,
	{ NULL }
};

void upsdrv_initinfo(void)
{
	upsdebugx(2, "upsdrv_initinfo");

	/* try to detect the device here - call fatal_with_errno(EXIT_FAILURE, ) if it fails */

	/* 1/ Open the NUT Modbus definition file and load the data
	 * 2/ Iterate through these data and call dstate_setinfo */

	/* dstate_setinfo("device.mfr", "skel manufacturer"); */
	/* dstate_setinfo("device.model", "longrun 15000"); */
	/* dstate_setinfo("device.type", "longrun 15000"); */
	/* ... */

	/* upsh.instcmd = instcmd; */
	/* upsh.setvar = setvar; */
}

void upsdrv_updateinfo(void)
{
	upsdebugx(2, "upsdrv_updateinfo");

	/* int flags; */
	/* char temp[256]; */

	/* ser_sendchar(upsfd, 'A'); */
	/* ser_send(upsfd, "foo%d", 1234); */
	/* ser_send_buf(upsfd, bincmd, 12); */

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

void upsdrv_shutdown(void)
{
	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */

	/* maybe try to detect the UPS here, but try a shutdown even if
	   it doesn't respond at first if possible */

	/* replace with a proper shutdown function */
	fatalx(EXIT_FAILURE, "shutdown not supported");

	/* you may have to check the line status since the commands
	   for toggling power are frequently different for OL vs. OB */

	/* OL: this must power cycle the load if possible */

	/* OB: the load must remain off until the power returns */
}

/*
static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send_buf(upsfd, ...);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}
*/

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
	upsdebugx(2, "upsdrv_initups");

	/* Determine if it's a RTU (serial) or ethernet (TCP) connection */
	/* FIXME: need to address windows COM port too!
	 * || !strncmp(device_path[0], "COM", 3) */
	if (device_path[0] == '/') {
		upsdebugx(2, "upsdrv_initups: RTU (serial) device");

		/* FIXME: handle serial comm. params (params in ups.conf
		 * and/or definition file) */
		ctx = modbus_new_rtu(device_path, 115200, 'N', 8, 1);
		if (ctx == NULL)
			fatalx(EXIT_FAILURE, "Unable to create the libmodbus context");
	}
	/* else {
		 * upscli_splitaddr(device_path[0] ? device_path[0] : "localhost", &hostname, &port
		upsdebugx(2, "upsdrv_initups: TCP (network) device");
		ctx = modbus_new_tcp(device_path, 1502); 
		if (ctx == NULL)
			fatalx(EXIT_FAILURE, "Unable to create the libmodbus context");

		if (modbus_connect(ctx) == -1) {
			modbus_free(ctx);
			fatalx(EXIT_FAILURE, "Connection failed: %s\n", modbus_strerror(errno));
		}
	}
	*/

	/* don't try to detect the device here */
}

void upsdrv_cleanup(void)
{
	/* free(dynamic_mem); */
	if (ctx != NULL) {
		modbus_close(ctx);
		modbus_free(ctx);
	}
}
