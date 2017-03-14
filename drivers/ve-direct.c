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
	char line[2000];
	char vn[50];

	char *v_name;
	char *v_value;

	size_t r_start = 0;

	status_init();
	status_set("OL");
	status_commit();

	upsdebugx(1, "upsdrv_updateinfo: ups.status = %s\n", dstate_getinfo("ups.status"));

	/************** ups.x ***************************/
	while (1)
	{
		if (r_start >= sizeof(line))
			r_start = 0;
		memset(line + r_start, 0, sizeof(line) - r_start);
		int nred = ser_get_buf(upsfd, line + r_start, sizeof(line) - r_start, 1, 0);
		if (nred < 0)
			break;
		if (nred == 0)
			continue;
		upsdebugx (3, "%s: buf '%s'", __func__, line);
		r_start += nred;
		// find checksum, verify checksum
		char *checksum = strstr(line, "Checksum\t");
		if (checksum == NULL || checksum + 9 >= line + r_start)
			continue;

		// calculate checksum
		char ch = 0;
		for (v_name = line; v_name <= checksum + 8; v_name++)
			ch += *v_name;
		ch = ~ch + 1;
		if (ch != checksum[9])
		{
			upslogx(1, "invalid checksum: %d %d", ch, checksum[9]);
			r_start = 0;
			continue;
		}

		checksum[9] = '\0';
		upsdebugx(2, "buffer: %s", line);

		// parse what is in buffer..
		v_name = line;
		while (v_name < checksum)
		{
			if (v_name[0] != '\r' || v_name[1] != '\n')
				break;
			v_name += 2;
			v_value = v_name;
			while (*v_value != '\t' && *v_value != '\0')
				v_value++;
			if (v_value >= checksum)
				break;
			*v_value = '\0';
			v_value++;
			char *end = v_value;
			while (*end != '\r' && *end != '\0')
				end++;
			if (*end != '\r')
				break;
			*end = '\0';

			snprintf(vn, sizeof(vn), "ve-direct.%s", v_name);
			upsdebugx(1, "name %s value %s", vn, v_value);
			dstate_setinfo(vn, "%s", v_value);

			v_name = end;
			*v_name = '\r';
		}
		size_t left = (line + r_start) - (checksum + 9);
		if (left > 0)
			memmove(line, checksum + 10, left);

		r_start = left;
		break;
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
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B19200);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
