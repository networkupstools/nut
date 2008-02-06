/* powerpanel.c	Model specific routines for CyberPower text/binary
			protocol UPSes 

   Copyright (C)
	2007		Doug Reynolds <mav@wastegate.net>
	2007-2008	Arjen de Korte <adkorte-guest@alioth.debian.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/*
   Throughout this driver, READ and WRITE comments are shown. These are
   the typical commands to and replies from the UPS that was used for
   decoding the protocol (with a serial logger).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "main.h"
#include "serial.h"

#include "powerpanel.h"
#include "powerp-bin.h"
#include "powerp-txt.h"

static int	mode = 0;

static subdriver_t *subdriver[] = {
	&powpan_binary,
	&powpan_text,
	NULL
};

void upsdrv_initinfo(void)
{
	char	*s;

	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);

	dstate_setinfo("ups.mfr", "CyberPower");
	dstate_setinfo("ups.model", "[unknown]");
	dstate_setinfo("ups.serial", "[unknown]");

	subdriver[mode]->initinfo();

	/*
	 * Allow to override the following parameters
	 */
	if ((s = getval("manufacturer")) != NULL) {
		dstate_setinfo("ups.mfr", s);
	}
	if ((s = getval("model")) != NULL) {
		dstate_setinfo("ups.model", s);
	}
	if ((s = getval("serial")) != NULL) {
		dstate_setinfo("ups.serial", s);
	}
}


void upsdrv_updateinfo(void)
{
	subdriver[mode]->updateinfo();
}

void upsdrv_shutdown(void)
{
	subdriver[mode]->shutdown();
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
	addvar(VAR_VALUE, "manufacturer", "manufacturer");
	addvar(VAR_VALUE, "model", "modelname");
	addvar(VAR_VALUE, "serial", "serialnumber");
	addvar(VAR_VALUE, "protocol", "protocol to use [text|binary] (default: autodection)");
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools -  CyberPower text/binary protocol UPS driver %s (%s)\n",
		DRV_VERSION, UPS_VERSION);
	experimental_driver = 1;
}

void upsdrv_cleanup(void)
{
	ser_set_dtr(upsfd, 0);
	ser_close(upsfd, device_path);
}
