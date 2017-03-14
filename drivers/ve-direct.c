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
#define DRIVER_VERSION	"0.20"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Petr Kubanek <kubanek@fzu.cz>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

char line[500];
size_t r_start = 0;

int process_text_buffer()
{
	char vn[50];

	char *v_name;
	char *v_value;

	upsdebugx (3, "%s: r_start %d buf '%s'", __func__, r_start, line);
	// find checksum, verify checksum
	char *checksum = strstr(line, "Checksum\t");
	if (checksum == NULL || checksum + 9 >= line + r_start)
		return -1;

	// calculate checksum
	char ch = 0;
	for (v_name = line; v_name <= checksum + 8; v_name++)
		ch += *v_name;
	ch = ~ch + 1;
	if (ch != checksum[9])
	{
		upslogx(1, "invalid checksum: %d %d", ch, checksum[9]);
		goto endprocess;
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

endprocess:
	{
		size_t left = (line + r_start) - (checksum + 10);
		if (left > 0)
			memmove(line, checksum + 10, left);

		r_start = left;
	}
	return 0;
}

unsigned char ve_checksum(const char ve_cmd, const char *ve_extra)
{
	unsigned char ch = ve_cmd - '0';
	ch = ch ^ 0x55;
	return ch;
}

int ve_command(const char ve_cmd, const char *ve_extra)
{
	int ret;

	ser_send(upsfd, ":%c%02X\n", ve_cmd, ve_checksum(ve_cmd, ve_extra));

	char *reply = NULL;
	char *endl = NULL;

	while (endl == NULL)
	{
		ret = ser_get_buf(upsfd, line + r_start, sizeof(line) - r_start, 2, 0);
		if (ret < 0)
			return STAT_INSTCMD_FAILED;
		r_start += ret;
		// check if we read full line with the reply..
		reply = strchr(line, ':');
		endl = NULL;
		if (reply != NULL)
			endl = strchr(reply, '\n');
		// already seen start of command reply
		if (reply != NULL)
			break;

		if (process_text_buffer() < 0)
			break;
	}

	// process what's left
	while (strncmp(line, "\r\n:", 3))
	{
		if (process_text_buffer() < 0)
			break;
	}

	endl = NULL;
	reply = line + 2;
	endl = strchr(reply, '\n');

	upsdebugx(1, "reply to command: %ld %s", r_start, line);

	if (reply != NULL && endl != NULL)
	{
		upsdebugx(1, "reply to command: %ld %s", r_start, line);
	}

	return 0;
}

static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "ve-direct.ping"))
		return ve_command('1',NULL);
	upsdebugx(1, "instcmd: unknown command: %s", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}
	
void upsdrv_initinfo(void)
{
	dstate_addcmd("ve-direct.ping");

	upsh.instcmd = instcmd;
}

void upsdrv_updateinfo(void)
{
	/* ups.status */ 
	status_init();
	status_set("OL");
	status_commit();

	upsdebugx(1, "upsdrv_updateinfo: ups.status = %s", dstate_getinfo("ups.status"));

	/************** ups.x ***************************/
	while (1)
	{
		if (r_start >= sizeof(line))
			r_start = 0;
		memset(line + r_start, 0, sizeof(line) - r_start);
		int nred = ser_get_buf(upsfd, line + r_start, sizeof(line) - r_start, 0, 200);
		if (nred < 0)
			break;
		if (nred == 0)
			continue;
		r_start += nred;

		while (process_text_buffer() >= 0)
		{
		}

		break;
	}

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
}

void upsdrv_help(void)
{
	printf("Read The Fine Manual ('man 8 ve-direct')\n");
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
}

void upsdrv_initups(void)
{
#ifndef TESTING
	poll_interval = 1;
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B19200);
#endif
}

void upsdrv_cleanup(void)
{
#ifndef TESTING
	ser_close(upsfd, device_path);
#endif
}
