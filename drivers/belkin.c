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
#define DRIVER_VERSION	"0.24"

static int init_communication(void);
static int get_belkin_reply(char *buf);

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
	ser_flush_io(upsfd);

	ser_send(upsfd, "~00%c%03d%s%s", cmd, (int)strlen(data) + 3, subcmd, data);

	upsdebugx(3, "Send Command: %s, %s", subcmd, data);
}

static int init_communication(void)
{
	int	i, res;
	char	temp[SMALLBUF];

	for (i = 0; i < 10; i++) {
		send_belkin_command(STATUS, MANUFACTURER, "");
		res = get_belkin_reply(temp);

		if (res > 0) {
			/* return the number of retries needed before a valid reply is read (discard contents) */
			return i;
		}
	}

	/* no valid reply read */
	return -1;
}

static char *get_belkin_field(const char *in, char *out, size_t outlen, size_t num)
{
	size_t	i, c = 1;
	char	*ptr;

	/* special case */
	if (num == 1) {
		snprintf(out, outlen, "%s", in);
		ptr = strchr(out, ';');

		if (ptr) {
			*ptr = '\0';
		}

		return out;
	}

	for (i = 0; i < strlen(in); i++) {
		if (in[i] == ';') {
			c++;
		}

		if (c == num) {
			snprintf(out, outlen, "%s", &in[i + 1]);
			ptr = strchr(out, ';');

			if (ptr) {
				*ptr = '\0';
			}

			return out;
		}
	}

	return NULL;
}

static int get_belkin_reply(char *buf)
{
	int	ret;
	long	cnt;
	char	tmp[8];

	usleep(25000);

	/* pull first 7 bytes to get data length - like ~00D004 */
	ret = ser_get_buf_len(upsfd, (unsigned char *)tmp, 7, 2, 0);

	if (ret != 7) {
		ser_comm_fail("Initial read returned %d bytes", ret);
		return -1;
	}

	tmp[7] = 0;
	cnt = strtol(tmp + 4, NULL, 10);
	upsdebugx(3, "Received: %s", tmp);

	if (cnt == 0) {	/* possible to have ~00R000, return empty response */
		buf[0] = 0;
		return 0;
	}

	if ((cnt < 0) || (cnt > 255)) {
		return -1;
	}

	/* give it time to respond to us */
	usleep(5000 * cnt);

	ret = ser_get_buf_len(upsfd, (unsigned char *)buf, cnt, 2, 0);

	buf[cnt] = 0;
	upsdebugx(3, "Received: %s", buf);

	if (ret != cnt) {
		ser_comm_fail("Second read returned %d bytes, expected %ld", ret, cnt);
		return -1;
	}

	ser_comm_good();

	return ret;
}

static int do_broken_rat(char *buf)
{
	int	ret;
	long	cnt;
	char	tmp[8];

	usleep(25000);

	/* pull first 7 bytes to get data length - like ~00D004 */
	ret = ser_get_buf_len(upsfd, (unsigned char *)tmp, 7, 2, 0);

	if (ret != 7) {
		ser_comm_fail("Initial read returned %d bytes", ret);
		return -1;
	}

	tmp[7] = 0;
	cnt = strtol(tmp + 4, NULL, 10);
	upsdebugx(3, "Received: %s", tmp);

	if (cnt == 0) {	/* possible to have ~00R000, return empty response */
		buf[0] = 0;
		return 0;
	}

	if ((cnt < 0) || (cnt > 255)) {
		return -1;
	}

	/* give it time to respond to us */
	usleep(5000 * cnt);

	/* firmware 001 only sends 50 bytes instead of the proper 53 */
	if (cnt == 53) {
		cnt = 50;
	}

	ret = ser_get_buf_len(upsfd, (unsigned char *)buf, cnt, 2, 0);

	buf[cnt] = 0;
	upsdebugx(3, "Received: %s", buf);

	if (ret != cnt) {
		ser_comm_fail("Second read returned %d bytes, expected %ld", ret, cnt);
		return -1;
	}

	ser_comm_good();

	return ret;
}

/* normal idle loop - keep up with the current state of the UPS */
void upsdrv_updateinfo(void)
{
	static int retry = 0;
	int	res;
	char	temp[SMALLBUF], st[SMALLBUF];

	send_belkin_command(STATUS, STAT_STATUS, "");
	res = get_belkin_reply(temp);
	if (res < 0) {
		if (retry < MAXTRIES) {
			upsdebugx(1, "Communications with UPS lost: status read failed!");
			retry++;
		} else {	/* too many retries */
			upslogx(LOG_WARNING, "Communications with UPS lost: status read failed!");
			dstate_datastale();
		}
		return;
	}

	if (retry) {	/* previous attempt had failed */
		upslogx(LOG_WARNING, "Communications with UPS re-established");
		retry = 0;
	}

	if (res == 0) {
		upsdebugx(1, "Ignoring empty return value after status query");
		return;
	}

	status_init();

	get_belkin_field(temp, st, sizeof(st), 6);
	if (*st == '1') {
		status_set("OFF");
	}

	get_belkin_field(temp, st, sizeof(st), 2);
	if (*st == '1') {
		status_set("OB");	/* on battery */
	} else {
		status_set("OL");	/* on line */
	}

	get_belkin_field(temp, st, sizeof(st), 16);
	dstate_setinfo("ups.beeper.status", "%s", (*st == '0' ? "disabled" : "enabled"));

	send_belkin_command(STATUS, STAT_BATTERY, "");
	res = get_belkin_reply(temp);
	if (res > 0) {
		/* report the compiled in battery charge where the driver assumes the battery is low */
		dstate_setinfo("battery.charge.low", "%d", LOW_BAT);

		get_belkin_field(temp, st, sizeof(st), 10);
		res = atoi(st);
		get_belkin_field(temp, st, sizeof(st), 2);

		if (*st == '1' || res < LOW_BAT) {
			status_set("LB");	/* low battery */
		}

		get_belkin_field(temp, st, sizeof(st), 10);
		dstate_setinfo("battery.charge", "%.0f", strtod(st, NULL));

		get_belkin_field(temp, st, sizeof(st), 9);
		dstate_setinfo("battery.temperature", "%.0f", strtod(st, NULL));

		get_belkin_field(temp, st, sizeof(st), 7);
		dstate_setinfo("battery.voltage", "%.1f", strtod(st, NULL) / 10);

		get_belkin_field(temp, st, sizeof(st), 9);
		dstate_setinfo("ups.temperature", "%.0f", strtod(st, NULL));
	}

	send_belkin_command(STATUS, STAT_INPUT, "");
	res = get_belkin_reply(temp);
	if (res > 0) {
		get_belkin_field(temp, st, sizeof(st), 3);
		dstate_setinfo("input.voltage", "%.1f", strtod(st, NULL) / 10);

		get_belkin_field(temp, st, sizeof(st), 2);
		dstate_setinfo("input.frequency", "%.1f", strtod(st, NULL) / 10);
	}

	send_belkin_command(STATUS, STAT_OUTPUT, "");
	res = get_belkin_reply(temp);
	if (res > 0) {
		get_belkin_field(temp, st, sizeof(st), 2);
		dstate_setinfo("output.frequency", "%.1f", strtod(st, NULL) / 10);

		get_belkin_field(temp, st, sizeof(st), 4);
		dstate_setinfo("output.voltage", "%.1f", strtod(st, NULL) / 10);

		get_belkin_field(temp, st, sizeof(st), 7);
		dstate_setinfo("ups.load", "%.0f", strtod(st, NULL));
	}

	send_belkin_command(STATUS, TEST_RESULT, "");
	res = get_belkin_reply(temp);
	if (res > 0) {
		get_belkin_field(temp, st, sizeof(st), 1);
		switch (*st)
		{
		case '0':
			dstate_setinfo("ups.test.result", "%s", "No test performed");
			break;

		case '1':
			dstate_setinfo("ups.test.result", "%s", "Passed");
			break;

		case '2':
			dstate_setinfo("ups.test.result", "%s", "In progress");
			break;

		case '3':
		case '4':
			dstate_setinfo("ups.test.result", "%s", "10s test failed");
			break;

		case '5':
			dstate_setinfo("ups.test.result", "%s", "deep test failed");
			break;

		case '6':
			dstate_setinfo("ups.test.result", "%s", "Aborted");
			break;

		default:
			upsdebugx(3, "Unhandled test status '%c'", *st);
			break;
		}
	}

	status_commit();

	dstate_dataok();
}

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{
	int	res;

	res = init_communication();
	if (res < 0) {
		printf("Detection failed.  Trying a shutdown command anyway.\n");
	}

	/* tested on a F6C525-SER: this works when OL and OB */

	/* shutdown type 2 (UPS system) */
	send_belkin_command(CONTROL, "SDT", "2");

	/* SDR means "do SDT and SDA, then reboot after n minutes" */
	send_belkin_command(CONTROL, "SDR", "1");

	printf("UPS should power off load in 5 seconds\n");

	/* shutdown in 5 seconds */
	send_belkin_command(CONTROL, "SDA", "5");
}

/* handle "beeper.disable" */
static void do_beeper_off(void) {
	int	res;
	char	temp[SMALLBUF];
	const char	*arg;

	/* Compare the model name, as the BUZZER_OFF argument depends on it */
	send_belkin_command(STATUS, MODEL, "");
	res = get_belkin_reply(temp);
	if (res == -1)
		return;

	if (!strcmp(temp, "F6C1400-EUR")) {
		arg = BUZZER_OFF2;
	} else {
		arg = BUZZER_OFF0;
	}

	send_belkin_command(CONTROL,BUZZER,arg);
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
	if (!strcasecmp(cmdname, "beeper.disable")) {
		do_beeper_off();
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "beeper.enable")) {
		send_belkin_command(CONTROL,BUZZER,BUZZER_ON);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "load.off")) {
		do_off();
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "load.on")) {
		send_belkin_command(CONTROL,POWER_ON,"1;1");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start.quick")) {
		send_belkin_command(CONTROL,TEST,TEST_10SEC);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start.deep")) {
		send_belkin_command(CONTROL,TEST,TEST_DEEP);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.stop")) {
		send_belkin_command(CONTROL,TEST,TEST_CANCEL);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
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

	/* set DTR to low and RTS to high */
	ser_set_dtr(upsfd, 0);
	ser_set_rts(upsfd, 1);

	sleep(1);

	ser_flush_io(upsfd);
}

void upsdrv_initinfo(void)
{
	int	res;
	char	temp[SMALLBUF], st[SMALLBUF];

	res = init_communication();
	if (res < 0) {
		fatalx(EXIT_FAILURE,
			"Unable to detect an Belkin Smart protocol UPS on port %s\n"
			"Check the cabling, port name or model name and try again", device_path
			);
	}

	dstate_setinfo("ups.mfr", "BELKIN");

	send_belkin_command(STATUS, MODEL, "");
	res = get_belkin_reply(temp);
	if (res > 0) {
		dstate_setinfo("ups.model", "%s", temp);
	}

	send_belkin_command(STATUS, VERSION_CMD, "");
	res = get_belkin_reply(temp);
	if (res > 0) {
		dstate_setinfo("ups.firmware", "%s", temp);
	}

	/* deal with stupid firmware that breaks RAT */
	send_belkin_command(STATUS, RATING, "");

	if (!strcmp(temp, "001")) {
		res = do_broken_rat(temp);
	} else {
		res = get_belkin_reply(temp);
	}

	if (res > 0) {
		get_belkin_field(temp, st, sizeof(st), 8);
		dstate_setinfo("input.transfer.low", "%.0f", strtod(st, NULL) / 0.88);

		get_belkin_field(temp, st, sizeof(st), 9);
		dstate_setinfo("input.transfer.high", "%.0f", strtod(st, NULL) * 0.88);
	}

	dstate_addcmd("beeper.disable");
	dstate_addcmd("beeper.enable");
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	dstate_addcmd("test.battery.start.quick");
	dstate_addcmd("test.battery.start.deep");
	dstate_addcmd("test.battery.stop");

	upsh.instcmd = instcmd;
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
