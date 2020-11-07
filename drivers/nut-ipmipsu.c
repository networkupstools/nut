/*  nut-ipmipsu.c - Driver for IPMI Power Supply Units (PSU)
 *
 *  Copyright (C)
 *    2011 - 2012  Arnaud Quette <arnaud.quette@free.fr>
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
 * - PSU sensor monitoring (how to find the right one?)
 * - dump all value at init, so that we can check for other interesting data
 */

#include "main.h"
#include "nut-ipmi.h"

#define DRIVER_NAME	"IPMI PSU driver"
#define DRIVER_VERSION	"0.30"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <arnaud.quette@free.fr>\n",
	DRV_EXPERIMENTAL,
	{ NULL }
};

/* Note on device.status
 * OL: present and providing power
 * OFF: present but not providing power (power cable removed)
 * stale: not present (PSU removed)
 * 	=> should we prefer RB, MISSING, ABSENT, ???
 */

/* Abstract structure to allow different IPMI implementation
 * We currently use FreeIPMI, but OpenIPMI and others are serious
 * candidates! */
IPMIDevice_t ipmi_dev;

/* Currently used to store FRU ID, but will probably evolve... */
int ipmi_id = -1;

void upsdrv_initinfo(void)
{
	/* try to detect the PSU here - call fatal_with_errno(EXIT_FAILURE, ) if it fails */
	upsdebugx(1, "upsdrv_initinfo...");

	/* print what we detected during IPMI open */
	upsdebugx(1, "Detected a PSU: %s/%s",
		ipmi_dev.manufacturer ? ipmi_dev.manufacturer : "unknown",
		ipmi_dev.product ? ipmi_dev.product : "unknown");

	dstate_setinfo ("device.type", "psu");

	/* Publish information from the IPMI structure */
	if (ipmi_dev.manufacturer)
		dstate_setinfo("device.mfr", "%s", ipmi_dev.manufacturer);

	if (ipmi_dev.product)
		dstate_setinfo("device.model", "%s", ipmi_dev.product);

	if (ipmi_dev.serial)
		dstate_setinfo("device.serial", "%s", ipmi_dev.serial);

	if (ipmi_dev.part)
		dstate_setinfo("device.part", "%s", ipmi_dev.part);

	if (ipmi_dev.date)
		dstate_setinfo("device.mfr.date", "%s", ipmi_dev.date);

	/* FIXME: move to device.id */
	dstate_setinfo("ups.id", "%i", ipmi_id);
	/* FIXME: move to device.realpower.nominal */
	if (ipmi_dev.overall_capacity != -1)
		dstate_setinfo("ups.realpower.nominal", "%i", ipmi_dev.overall_capacity);

	if (ipmi_dev.input_minvoltage != -1)
		dstate_setinfo("input.voltage.minimum", "%i", ipmi_dev.input_minvoltage);

	if (ipmi_dev.input_maxvoltage != -1)
		dstate_setinfo("input.voltage.maximum", "%i", ipmi_dev.input_maxvoltage);

	if (ipmi_dev.input_minfreq != -1)
		dstate_setinfo("input.frequency.low", "%i", ipmi_dev.input_minfreq);

	if (ipmi_dev.input_maxfreq != -1)
		dstate_setinfo("input.frequency.high", "%i", ipmi_dev.input_maxfreq);

	/* FIXME: move to device.voltage */
	if (ipmi_dev.voltage != -1)
		dstate_setinfo("ups.voltage", "%i", ipmi_dev.voltage);

	if (nut_ipmi_monitoring_init() != 0)
		fatalx(EXIT_FAILURE, "Can't initialize IPMI monitoring");

	if (nut_ipmi_get_sensors_status(&ipmi_dev) != 0) {
		upsdebugx(1, "Error while updating sensors values");
		dstate_datastale();
	}
	else {
		dstate_dataok();
	}

	/* upsh.instcmd = instcmd; */
}

void upsdrv_updateinfo(void)
{
	upsdebugx(1, "upsdrv_updateinfo...");

	/* FIXME: implement sensors monitoring */

	if (nut_ipmi_get_sensors_status(&ipmi_dev) != 0) {
		upsdebugx(1, "Error while updating sensors values");
		dstate_datastale();
	}
	else {
		dstate_dataok();
	}

	/*
	 * poll_interval = 2;
	 */
}

void upsdrv_shutdown(void)
{
	fatalx(EXIT_FAILURE, "shutdown not supported");
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
	/* FIXME: need more params.
	addvar(VAR_VALUE, "username", "Remote server username");
	addvar(VAR_VALUE, "password", "Remote server password");
	addvar(VAR_VALUE, "authtype",
			"Authentication type to use during lan session activation");
	addvar(VAR_VALUE, "type",
		"Type of the device to match ('psu' for \"Power Supply\")");

	addvar(VAR_VALUE, "serial", "Serial number to match a specific device");
	addvar(VAR_VALUE, "fruid", "FRU identifier to match a specific device"); */
}

void upsdrv_initups(void)
{
	upsdebugx(1, "upsdrv_initups...");

	/* port can be expressed in various forms:
	 * - inband:
	 *   "id?" for device (FRU) ID 0x?
	 *   "psu?" for PSU number ?
	 * - out of band
	 *   "id?@host"
	 *   "host" => requires serial or ...
	 */
	if (!strncmp( device_path, "id", 2))
	{
		ipmi_id = atoi(device_path+2);
		upsdebugx(2, "Device ID 0x%i", ipmi_id);
	}
	/* else... <psuX> to select PSU number X */

	/* Clear the interface structure */
	ipmi_dev.ipmi_id = -1;
	ipmi_dev.manufacturer = NULL;
	ipmi_dev.product = NULL;
	ipmi_dev.serial = NULL;
	ipmi_dev.part = NULL;
	ipmi_dev.date = NULL;
	ipmi_dev.overall_capacity = -1;
	ipmi_dev.input_minvoltage = -1;
	ipmi_dev.input_maxvoltage = -1;
	ipmi_dev.input_minfreq = -1;
	ipmi_dev.input_maxfreq = -1;
	ipmi_dev.voltage = -1;
	ipmi_dev.sensors_count = 0;
	ipmi_dev.status = -1;
	ipmi_dev.input_voltage = -1;
	ipmi_dev.input_current = -1;
	ipmi_dev.temperature = -1;

	/* Open IPMI using the above */
	nut_ipmi_open(ipmi_id, &ipmi_dev);

	/* the upsh handlers can't be done here, as they get initialized
	 * shortly after upsdrv_initups returns to main.
	 */

	/* don't try to detect the UPS here */
}

void upsdrv_cleanup(void)
{
	upsdebugx(1, "upsdrv_cleanup...");
	nut_ipmi_close();
}
