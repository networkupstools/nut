/*
 * powerpanel.c - Model specific routines for CyberPower text/binary
 *                protocol UPSes
 *
 * Copyright (C)
 *	2007        Doug Reynolds <mav@wastegate.net>
 *	2007-2008   Arjen de Korte <adkorte-guest@alioth.debian.org>
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

#include "powerp-bin.h"
#include "powerp-txt.h"

static int	mode = 0;

static subdriver_t *subdriver[] = {
	&powpan_binary,
	&powpan_text,
	NULL
};

#define DRIVER_NAME	"CyberPower text/binary protocol UPS driver"
#define DRIVER_VERSION	"0.27"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Doug Reynolds <mav@wastegate.net>\n" \
	"Arjen de Korte <adkorte-guest@alioth.debian.org>\n" \
	"Timothy Pearson <kb9vqf@pearsoncomputing.net>",
	DRV_EXPERIMENTAL,
	{ NULL }
};
/* FIXME: add a sub version for binary and text subdrivers? */

void upsdrv_initinfo(void)
{
	char	*s;

	dstate_setinfo("ups.mfr", "CyberPower");
	dstate_setinfo("ups.model", "[unknown]");
	dstate_setinfo("ups.serial", "[unknown]");

	subdriver[mode]->initinfo();

	/*
	 * Allow to override the following parameters
	 */
	if ((s = getval("manufacturer")) != NULL) {
		dstate_setinfo("ups.mfr", "%s", s);
	}
	if ((s = getval("model")) != NULL) {
		dstate_setinfo("ups.model", "%s", s);
	}
	if ((s = getval("serial")) != NULL) {
		dstate_setinfo("ups.serial", "%s", s);
	}

	upsh.instcmd = subdriver[mode]->instcmd;
	upsh.setvar = subdriver[mode]->setvar;
}

void upsdrv_updateinfo(void)
{
	static int	retry = 0;

	if (subdriver[mode]->updateinfo() < 0) {
		ser_comm_fail("Status read failed!");

		if (retry < 3) {
			retry++;
		} else {
			dstate_datastale();
		}

		return;
	}

	retry = 0;

	ser_comm_good();

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	int	i, ret;

	/*
	 * Try to shutdown with delay and automatic reboot if the power
	 * returned in the mean time (newer models support this).
	 */
	if (subdriver[mode]->instcmd("shutdown.return", NULL) == STAT_INSTCMD_HANDLED) {
		/* Shutdown successful */
		return;
	}

	/*
	 * Looks like an older type. Assume we're still on battery if
	 * we can't read status or it is telling us we're on battery.
	 */
	for (i = 0; i < MAXTRIES; i++) {

		ret = subdriver[mode]->updateinfo();
		if (ret >= 0) {
			break;
		}
	}

	if (ret) {
		/*
		 * When on battery, the 'shutdown.stayoff' command will make
		 * the UPS switch back on when the power returns.
		 */
		if (subdriver[mode]->instcmd("shutdown.stayoff", NULL) == STAT_INSTCMD_HANDLED) {
			upslogx(LOG_INFO, "Waiting for power to return...");
			return;
		}
	} else {
		/*
		 * Apparently, the power came back already, so we just need to reboot.
		 */
		if (subdriver[mode]->instcmd("shutdown.reboot", NULL) == STAT_INSTCMD_HANDLED) {
			upslogx(LOG_INFO, "Rebooting now...");
			return;
		}
	}

	upslogx(LOG_ERR, "Shutdown command failed!");
}

void upsdrv_initups(void)
{
	char	*version;

	version = getval("protocol");
	upsfd = ser_open(device_path);

	ser_set_rts(upsfd, 0);

	/*
	 * Try to autodetect which UPS is connected.
	 */
	for (mode = 0; subdriver[mode] != NULL; mode++) {

		if ((version != NULL) && strcasecmp(version, subdriver[mode]->version)) {
			continue;
		}

		ser_set_dtr(upsfd, 1);
		usleep(10000);

		if (subdriver[mode]->initups() > 0) {
			upslogx(LOG_INFO, "CyberPower UPS with %s protocol on %s detected", subdriver[mode]->version, device_path);
			return;
		}

		ser_set_dtr(upsfd, 0);
		usleep(10000);
	}

	fatalx(EXIT_FAILURE, "CyberPower UPS not found on %s", device_path);
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "ondelay", "Delay before UPS startup");
	addvar(VAR_VALUE, "offdelay", "Delay before UPS shutdown");

	addvar(VAR_VALUE, "manufacturer", "manufacturer");
	addvar(VAR_VALUE, "model", "modelname");
	addvar(VAR_VALUE, "serial", "serialnumber");
	addvar(VAR_VALUE, "protocol", "protocol to use [text|binary] (default: autodection)");
}

void upsdrv_cleanup(void)
{
	ser_set_dtr(upsfd, 0);
	ser_close(upsfd, device_path);
}
