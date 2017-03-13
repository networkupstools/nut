/* ve-direct.c - Model specific routines for Victron Energy Direct units
 *
 * Copyright (C) 2017  Petr Kubanek <kubanek@fzu.cz>
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
 */

#include "main.h"
#include "serial.h"

#define DRIVER_NAME	"Victron Energy Direct UPS and solar controller driver"
#define DRIVER_VERSION	"0.10"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Petr Kubanek <kubanek@fzu.cz>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define ENDCHAR	'\r'
#define IGNCHARS "\n"

int  sdwdelay = 0;  /* shutdown after 0 second */

static int start_is_datastale = 1;

static int instcmd(const char *cmdname, const char *extra)
{
	upsdebugx(1, "instcmd: unknown command: %s", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}
	
void upsdrv_initinfo(void)
{
	upsh.instcmd = instcmd;
}


void upsdrv_updateinfo(void)
{
	/* ups.status */ 
	char line[200];
	char var[200];
	char vn[200];
	
	status_init();
	status_set("OL");
	status_commit();

	upsdebugx(1, "upsdrv_updateinfo: ups.status = %s\n", dstate_getinfo("ups.status"));

	/************** ups.x ***************************/
	while (1)
	{
		int nred = ser_get_line(upsfd, line, sizeof(line), '\t', "", 1, 0);
		if (nred < 0)
			break;
		if (nred == 0)
			continue;
		if (!strcmp(line, "Checksum"))
			break;
		nred = ser_get_line(upsfd, var, sizeof(var), '\n', "", 1, 0);
		if (nred < 0)
			break;
		if (nred == 0)
			continue;
		upsdebugx(1, "upsdrv_updateinfo: read %s %s\n", line, var);
		snprintf (vn, "ve-direct.%s", line);

		dstate_setinfo(vn, "%s", var);
	}
  

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
}

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "usd", "Seting delay before shutdown");
}

void upsdrv_initups(void)
{
	char *usd = NULL;
   
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B19200);


	if ((usd = getval("usd"))) {
		sdwdelay=atoi(usd);
		upsdebugx(1, "(-x) Delay before shutdown %i",sdwdelay);
	}
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
