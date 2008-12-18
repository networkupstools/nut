/* belkin.c - model specific routines for Belkin Smart-UPS units.

   Copyright (C) 2000 Marcus Müller <marcus@ebootis.de>

   based on:

   apcsmart.c - model specific routines for APC smart protocol units

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

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

#include "main.h"
#include "serial.h"
#include "belkin.h"

#define DRIVER_NAME	"Belkin Smart protocol driver"
#define DRIVER_VERSION	"0.22"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Marcus Müller <marcus@ebootis.de>",
	DRV_STABLE,
	{ NULL }
};

static void send_belkin_command(char cmd, const char *subcmd, const char *data)
{
	ser_send(upsfd, "~00%c%03d%s%s", cmd, strlen(data) + 3, subcmd, data);
}

static int init_communication(void)
{
	int	i;
	int	res;
	char	temp[SMALLBUF];

	res = -1;
	for (i = 1; i <= 10 && res == -1; i++) {
		send_belkin_command(STATUS,MANUFACTURER,"");
		res = get_belkin_reply(temp);
	}

	if (res == -1 || strcmp(temp,"BELKIN")) 
		return res;

	return 0;
}

static char *get_belkin_field(const char *in, char *out, size_t outlen, 
	size_t num)
{
	size_t	i, c = 1;
	char	*ptr;

	/* special case */
	if (num == 1) {
		snprintf(out, outlen, "%s", in);
		ptr = strchr(out, ';');

		if (ptr)
			*ptr = '\0';

		return out;
	}		

	for (i = 0; i < strlen(in); i++) {
		if (in[i] == ';')
			c++;

		if (c == num) {
			snprintf(out, outlen, "%s", &in[i + 1]);
			ptr = strchr(out, ';');

			if (ptr)
				*ptr = '\0';
			return out;
		}
	}

	return NULL;
}

static int do_status(void)
{
	char	temp[SMALLBUF], st[SMALLBUF];
	int	res;

	send_belkin_command(STATUS,STAT_STATUS,"");
	res = get_belkin_reply(temp);
	if (res == -1) {
		dstate_datastale();
		return 0;
	}

	status_init();

	get_belkin_field(temp, st, sizeof(st), 6);
	if (*st == '1') {
		status_set("OFF");

	} else {	/* (OFF) and (OB | OL) are mutually exclusive */

		get_belkin_field(temp, st, sizeof(st), 2);
		if (*st == '1') {
			status_set("OB");

			send_belkin_command(STATUS,STAT_BATTERY,"");
			res = get_belkin_reply(temp);

			if (res == -1) {
				dstate_datastale();
				return 0;
			}

			get_belkin_field(temp, st, sizeof(st), 10);
			res = atoi(st);
			get_belkin_field(temp, st, sizeof(st), 2);

			if (*st == '1' || res < LOW_BAT) 
				status_set("LB");	/* low battery */
		}
		else
			status_set("OL");	/* on line */
	}

	status_commit();
	dstate_dataok();

	return 1;
}

static int do_broken_rat(char *buf)
{
	int	ret, cnt;
	char	tmp[8];

	usleep(25000);

	ret = ser_get_buf_len(upsfd, (unsigned char *)tmp, 7, 3, 0);

	if (ret != 7)
		return -1;

	tmp[7] = '\0';
	cnt = atoi(tmp + 4);

	if ((cnt < 1) || (cnt > 255))
		return -1;

	usleep(5000 * cnt);

	/* firmware 001 only sends 50 bytes instead of the proper 53 */
	if (cnt == 53)
		cnt = 50;

	ret = ser_get_buf_len(upsfd, (unsigned char *)buf, 50, cnt, 0);
	buf[cnt] = 0;

	return ret;
}	

static int init_ups_data(void)
{
	int	res;
	double	low, high;
	char	temp[SMALLBUF], st[SMALLBUF];

	send_belkin_command(STATUS, MODEL, "");
	res = get_belkin_reply(temp);
	if (res == -1)
		return res;

	dstate_setinfo("ups.model", "%s", temp);

	send_belkin_command(STATUS, VERSION_CMD, "");
	res = get_belkin_reply(temp);
	if (res == -1)
		return res;

	dstate_setinfo("ups.firmware", "%s", temp);

	/* deal with stupid firmware that breaks RAT */

	send_belkin_command(STATUS, RATING, "");

	if (!strcmp(temp, "001"))
		res = do_broken_rat(temp);
	else
		res = get_belkin_reply(temp);

	if (res > 0) {
		get_belkin_field(temp, st, sizeof(st), 8);
		low = atof(st) / 0.88;

		get_belkin_field(temp, st, sizeof(st), 9);
		high = atof(st) * 0.88;

		dstate_setinfo("input.transfer.low", "%03.1f", low);
		dstate_setinfo("input.transfer.high", "%03.1f", high);
	}

	ser_flush_io(upsfd);

	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	upsdrv_updateinfo();
	return 0;
}

/* normal idle loop - keep up with the current state of the UPS */
void upsdrv_updateinfo(void)
{
	int	res;
	double	val;
	char	temp[SMALLBUF], st[SMALLBUF];

	if (!do_status())
		return;

	send_belkin_command(STATUS, STAT_INPUT, "");
	res = get_belkin_reply(temp);
	if (res == -1)
		return;

	get_belkin_field(temp, st, sizeof(st), 3);
	val = atof(st) / 10;
	dstate_setinfo("input.voltage", "%05.1f", val);

	get_belkin_field(temp, st, sizeof(st), 2);
	val = atof(st) / 10;
	dstate_setinfo("input.frequency", "%.1f", val);

	send_belkin_command(STATUS,STAT_BATTERY, "");
	res = get_belkin_reply(temp);
	if (res == -1)
		return;

	get_belkin_field(temp, st, sizeof(st), 10);
	val = atof(st);
	dstate_setinfo("battery.charge", "%03.0f", val);

	get_belkin_field(temp, st, sizeof(st), 9);
	val = atof(st);
	dstate_setinfo("battery.temperature", "%03.0f", val);

	get_belkin_field(temp, st, sizeof(st), 7);
	val = atof(st) / 10;
	dstate_setinfo("battery.voltage", "%4.1f", val);

	get_belkin_field(temp, st, sizeof(st), 9);
	val = atof(st);
	dstate_setinfo("ups.temperature", "%03.0f", val);

	send_belkin_command(STATUS, STAT_OUTPUT, "");
	res = get_belkin_reply(temp);
	if (res == -1)
		return;

	get_belkin_field(temp, st, sizeof(st), 2);
	val = atof(st) / 10;
	dstate_setinfo("output.frequency", "%.1f", val);

	get_belkin_field(temp, st, sizeof(st), 4);
	val = atof(st) / 10;
	dstate_setinfo("output.voltage", "%05.1f", val);

	get_belkin_field(temp, st, sizeof(st), 7);
	val = atof(st);
	dstate_setinfo("ups.load", "%03.0f", val);
}

static int get_belkin_reply(char *buf)
{
	int	ret, cnt;
	char	tmp[8];

	usleep(25000);

	/* pull first 7 bytes to get data length - like ~00S004 */
	ret = ser_get_buf_len(upsfd, (unsigned char *)tmp, 7, 3, 0);

	if (ret != 7) {
		ser_comm_fail("Initial read returned %d bytes", ret);
		return -1;
	}

	tmp[7] = 0;
	cnt = atoi(tmp + 4);

	if ((cnt < 1) || (cnt > 255))
		return -1;

	/* give it time to respond to us */
	usleep(5000 * cnt);

	ret = ser_get_buf_len(upsfd, (unsigned char *)buf, cnt, 3, 0);

	buf[cnt] = 0;

	if (ret != cnt) {
		ser_comm_fail("Second read returned %d bytes, expected %d",
			ret, cnt);
		return -1;
	}

	ser_comm_good();

	return ret;
}

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{
	int	res;

	res = init_communication();
	if (res == -1)
		printf("Detection failed.  Trying a shutdown command anyway.\n");

	/* tested on a F6C525-SER: this works when OL and OB */

	/* shutdown type 2 (UPS system) */
	send_belkin_command(CONTROL, "SDT", "2");

	/* SDR means "do SDT and SDA, then reboot after n minutes" */
	send_belkin_command(CONTROL, "SDR", "1");

	printf("UPS should power off load in 5 seconds\n");

	/* shutdown in 5 seconds */
	send_belkin_command(CONTROL, "SDA", "5");
}

/* handle the "load.off" with some paranoia */
static void do_off(void)
{
	static	time_t lastcmd = 0;
	time_t	now, elapsed;
#ifdef CONFIRM_DANGEROUS_COMMANDS
	time(&now);
	elapsed = now - lastcmd;

	/* reset the timer every call - this means if you call it too      *
	 * early, then you have to wait MINCMDTIME again before sending #2 */
	lastcmd = now;

	if ((elapsed < MINCMDTIME) || (elapsed > MAXCMDTIME)) {

		/* FUTURE: tell the user (via upsd) to try it again */
		return;
	}
#endif

	upslogx(LOG_INFO, "Sending powerdown command to UPS\n");
	send_belkin_command(CONTROL,POWER_OFF,"1;1");
	usleep(1500000);
	send_belkin_command(CONTROL,POWER_OFF,"1;1");
}

static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "load.off")) {
		do_off();
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "load.on")) {
		send_belkin_command(CONTROL,POWER_ON,"1;1");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static void set_serialDTR0RTS1(void)
{
	/* set DTR to low and RTS to high */
	ser_set_dtr(upsfd, 0);
	ser_set_rts(upsfd, 1);
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}

/* prep the serial port */
void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);
	
	set_serialDTR0RTS1();

	sleep(1);

	ser_flush_io(upsfd);
}

void upsdrv_initinfo(void)
{
	int	res;

	res = init_communication();
	if (res == -1) {
		fatalx(EXIT_FAILURE, 
			"Unable to detect an Belkin Smart protocol UPS on port %s\n"
			"Check the cabling, port name or model name and try again", device_path
			);
	}

	dstate_setinfo("ups.mfr", "BELKIN");

	/* see what's out there */
	init_ups_data();

	printf("Detected %s on %s\n", dstate_getinfo("ups.model"),
		device_path);

	upsh.instcmd = instcmd;
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
