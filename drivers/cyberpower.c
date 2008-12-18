/* cyberpower.c - Network UPS Tools driver for Cyber Power Systems units

   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>
   Copyright (C) 2002  Len White     <lwhite@darkfires.net>

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "main.h"
#include "serial.h"
#include "timehead.h"

#define DRIVER_NAME	"CyberPower driver"
#define DRIVER_VERSION	"1.01"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Russell Kroll <rkroll@exploits.org>\n" \
	"Len White <lwhite@darkfires.net>",
	DRV_STABLE,
	{ NULL }
};

/* window for repeating dangerous command (shutdown.stayoff) */
#define MINCMDTIME	3
#define MAXCMDTIME	15

#define UPSDELAY 50000

/* ups frequency */
static float frequency(unsigned char in)
{
	float freq[22] = { 63.0, 62.7, 62.4, 62.1, 61.8, 61.4, 61.1, 60.8, 
		60.5, 60.2, 60.0, 59.7, 59.4, 59.1, 58.8, 58.5, 58.3, 58.0,
		57.7, 57.4, 57.2, 57.0 };
	int i, j;

	for (i = 0, j = 168; i < 23; j++, i++)
		if (in == j)
			return (float)freq[i];

	return (float)60;
}

/* adjust bizarre UPS data to observed voltage data */
static int voltconvert(unsigned char in)
{
	int v_end[43] = { 36, 51, 55, 60, 65, 70, 75, 80, 85, 91, 98, 103, 
		108, 113, 118, 123, 128, 133, 138, 143, 148, 153, 158, 163, 
		168, 173, 178, 183, 188, 193, 198, 203, 208, 213, 218, 223, 
		228, 233, 238, 243, 248, 253, 255 };
	int v_adj[43] = {  3,  4,  5,  4,  3,  2,  1,  0, -1, -2, -3,  -4,  
		-5,  -6,  -7,  -8,  -9, -10, -11, -12, -13, -14, -15, -16, 
		-17, -18, -19, -20, -21, -22, -23, -24, -25, -26, -27, -28, 
		-29, -30, -31, -32, -33, -34, -35 };
	int	i;

	if (in < 27)
		return 0;

	for (i = 0; i < 19; i++) { 
		if (in <= v_end[i]) {
			return (in + v_adj[i]);
		}
	}

	return 0;
}

/* map UPS data to realistic percentages */
static int battconvert(unsigned char in)
{
	/* these may only be valid for a load of 0 */
	int b_val[26] = {0, 1, 1, 2, 3, 4, 6, 8, 10, 12, 15, 18, 22, 26, 30, 
		35, 40, 46, 52, 58, 66, 73, 81, 88, 99, 100 };

	if (in > 185)
		return 100;

	if (in < 160)
		return 0;

	return (b_val[in - 160]);
}

/* Model mapping */
struct {
	int	first;
	int	second;
	const	char	*pcode;		/* product code - presently ignored */
	const	char	*model;
}	modelmap[] = {

	{ 51, 51, "OP850", "850AVR"	},	/* O33 */
	{ 52, 53, "OP1500", "1500AVR"	},	/* O45 */
	{ 52, 51, "OP1250", "1250AVR"	},	/* O43 */
	{ 52, 49, "OP700", "700AVR"	},	/* O41 */
	{ 51, 57, "OP650", "650AVR"	},	/* O39 */
	{ 51, 55, "OP900", "900AVR"	},	/* O37 */
	{ 51, 49, "OP800", "800AVR"	},	/* O31 */
	{ 50, 57, "OP500", "500AVR"	},	/* O29 */
	{ 50, 55, "OP320", "320AVR"	},	/* O27 */
	{ 49, 48, "OP1000", "1000AVR"	},	/* O10 */
	{  0,  0, (char*) NULL, (char *) NULL }
};

/* more wacky mapping - get the picture yet? */

struct {
	int	st;
	int	end;
	int	sz;
	int	base;
}	temptab[] =
{
	{   0,  39, 5,  0 },
	{  40,  43, 4,  8 }, 
	{  44,  78, 5,  9 },
	{  79,  82, 4, 16 },
	{  83, 117, 5, 17 },
	{ 118, 121, 4, 24 },
	{ 122, 133, 3, 25 },
	{ 134, 135, 2, 29 },
	{ 136, 143, 4, 30 },
	{ 144, 146, 3, 32 },
	{ 147, 150, 4, 33 },
	{ 151, 156, 3, 34 },
	{ 157, 164, 2, 36 },
	{ 165, 170, 3, 40 },
	{ 171, 172, 2, 42 },
	{ 173, 175, 3, 43 },
	{ 176, 183, 2, 44 },
	{ 184, 184, 1, 48 },
	{ 185, 188, 2, 49 },
	{ 189, 190, 2, 51 },
	{ 191, 191, 1, 52 },
	{ 192, 193, 2, 53 },
	{ 194, 194, 1, 54 },
	{ 195, 196, 2, 55 },
	{ 197, 197, 1, 56 },
	{ 198, 199, 2, 57 },
	{ 200, 200, 1, 58 },
	{ 201, 202, 2, 59 },
	{ 203, 203, 1, 60 },
	{ 204, 205, 2, 61 },
	{ 206, 206, 1, 62 },
	{ 207, 208, 2, 63 },
	{ 209, 209, 1, 64 },
	{ 210, 211, 2, 65 },
	{ 212, 212, 1, 66 },
	{ 213, 213, 1, 67 },
	{ 214, 214, 1, 68 },
	{ 215, 215, 1, 69 },
	{ 216, 255, 40, 70 },
	{   0,   0, 0,  0 },
};

static float tempconvert(unsigned char in)
{
	int	i, j, found, count;

	found = -1;
	for (i = 0; temptab[i].sz != 0; i++)
		if ((temptab[i].st <= in) && (temptab[i].end >= in))
			found = i;

	if (found == -1) {
		upslogx(LOG_ERR, "tempconvert: unhandled value %d", in);
		return 0;
	}

	count = temptab[found].end - temptab[found].st + 1;

	for (i = 0; i < count; i++) {
		j = temptab[found].st + (i * temptab[found].sz);

		if ((in - j) < temptab[found].sz) {
			return ((float)((in - j) / temptab[found].sz) + 
				temptab[found].base + i);
		}
	}

	upslogx(LOG_ERR, "tempconvert: fell through with %d", in);
	return 0;
}

static int confirm_write(const unsigned char *buf, size_t buflen)
{
	int	ret;
	char	verify[16];
	unsigned int	i;

	ret = ser_send_buf_pace(upsfd, UPSDELAY, buf, buflen);

	if (ret < 1) {
		upsdebugx(1, "confirm_write: ser_send_buf_pace failed");
		return 0;
	}

	/* don't try to read back the \r */
	ret = ser_get_buf_len(upsfd, (unsigned char *)verify, buflen - 1, 5, 0);

	if ((ret < 1) || (ret < ((int) buflen - 1))) {
		upsdebugx(1, "confirm_write: ret=%d, needed %d",
			ret, buflen - 1);
		return 0;
	}

	for (i = 0; i < buflen - 1; i++) {
		if (buf[i] != verify[i]) {
			upsdebugx(1, "mismatch at position %d",	i);
			return 0;
		}
	}

	/* made it this far, so it must have heard us */

	return 1;	/* success */
}

/* provide a quick status check to select the right shutdown command */
static int ups_on_line(void)
{
	int	ret;
	char	buf[SMALLBUF];

	ser_send_pace(upsfd, UPSDELAY, "D\r");

	/* give it a chance to reply completely */
	usleep(100000);

	ret = ser_get_buf_len(upsfd, (unsigned char *)buf, 14, 3, 0);

	if (ret < 14) {
		upslogx(LOG_ERR, "Status read failed: assuming on battery");
		return 0;
	}

	if (buf[9] & 128)
		return 0;	/* on battery */
	
	return 1;	/* on line */
}

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{
	int	i, ret, sdlen;
	char	buf[256];
	unsigned char	sdbuf[16];

	/* get this thing's attention */
	for (i = 0; i < 10; i++) {
		printf("Trying to wake up the ups... ");
		fflush(stdout);

		ser_send_char(upsfd, 13);
		ret = ser_get_buf_len(upsfd, (unsigned char *)buf, 1, 5, 0);

		if (ret > 0) {
			printf("OK\n");
			break;
		}

		printf("failed\n");
	}

	usleep(250000);

	memset(sdbuf, '\0', sizeof(sdbuf));

	if (ups_on_line() == 1) {

		/* this does not come back when on battery! */

		printf("Online: sending 7 byte command (back in about 45 sec)\n");

		sdlen = 8;
		sdbuf[0] = 'S';

		sdbuf[1] = 0x00;		/* how long until shutdown */
		sdbuf[2] = 0x00;		/* 0 = nearly immediate */

		sdbuf[3] = 'R';
		sdbuf[4] = 0x00;		/* how long to stay off */
		sdbuf[5] = 0x01;		/* 0 = about 13 seconds */
						/* 1 = about 45 seconds */

		sdbuf[6] = 'W';
		sdbuf[7] = '\r';
	
	} else {

		/* this one is one-way when on-line, but is the only
		 * known way to come back online when on battery ... 
		 */

		printf("On battery: sending 4 byte command (back when line power returns)\n");

		sdlen = 5;
		sdbuf[0] = 'S';
		sdbuf[1] = 0x00;
		sdbuf[2] = 0x00;
		sdbuf[3] = 'W';
		sdbuf[4] = '\r';
	}

	for (i = 0; i < 10; i++) {
		printf("Sending command...");
		fflush(stdout);

		if (confirm_write(sdbuf, sdlen)) {
			printf(" confirmed\n");
			break;
		}

		printf("failed, retrying...\n");
		fflush(stdout);
	}
}

void upsdrv_updateinfo(void)
{
	int	ret;
	char	buf[SMALLBUF];

	ser_send_pace(upsfd, UPSDELAY, "D\r");

	/* give it a chance to reply completely */
	usleep(100000);

	ret = ser_get_buf_len(upsfd, (unsigned char *)buf, 14, 3, 0);

	if (ret < 14) {
		ser_comm_fail("Short read from UPS");
		dstate_datastale();
		return;
	}

	if (buf[0] != '#') {
		ser_comm_fail("Invalid start char 0x%02x", buf[0] & 0xff);
		dstate_datastale();
		return;
	}

	if ((buf[4] != 46) || (buf[8] != 46)) {
		ser_comm_fail("Invalid separator in response (0x%02x, 0x%02x)", 
			buf[4], buf[8]);
		dstate_datastale();
		return;
	}

	ser_comm_good();

	dstate_setinfo("input.frequency", "%2.1f", frequency(buf[7]));
	dstate_setinfo("ups.temperature", "%2.1f", tempconvert(buf[6]));
	dstate_setinfo("battery.charge", "%03d", battconvert(buf[5]));
	dstate_setinfo("ups.load", "%03d", (buf[3] & 0xff) * 2);
	dstate_setinfo("input.voltage", "%03d", voltconvert(buf[1]));

	status_init();

	if (buf[9] & 2)
		status_set("OFF");

	if (buf[9] & 64)
		status_set("LB");

	if (buf[9] & 128)
		status_set("OB");
	else
		status_set("OL");

	status_commit();
	dstate_dataok();
}

static int get_ident(char *buf, size_t bufsize)
{
	int	ret, tries;

	for (tries = 0; tries < 3; tries++) {
		ret = ser_send_pace(upsfd, UPSDELAY, "F\r");

		if (ret != 2)
			continue;

		/* give it a chance to reply completely */
		usleep(400000);

		ret = ser_get_line(upsfd, buf, bufsize, '\r', "", 3, 0);

		if (ret < 1)
			continue;

		if (buf[0] == '.')
			return 1;

		/* if we got here, then the read failed somehow */
	}

	upslogx(LOG_INFO, "Giving up on hardware detection after 3 tries");

	return 0;
}

static int detect_hardware(void)
{
	int	i, foundmodel = 0;
	char	buf[SMALLBUF];

	if (!get_ident(buf, sizeof(buf)))
		fatalx(EXIT_FAILURE, "Unable to get initial hardware info string");

	if (buf[0] != '.') {
		upslogx(LOG_ERR, "Invalid start model string 0x%02x", 
			buf[0] & 0xff);
		return -1;
	}

	for (i = 0; modelmap[i].model != NULL; i++) {
		if (buf[3] == modelmap[i].first &&
			buf[4] == modelmap[i].second) {

			dstate_setinfo("ups.model", "%s", modelmap[i].model);
			foundmodel = 1;
			break;
		}
	}

	if (!foundmodel)
		dstate_setinfo("ups.model", "Unknown model - %c%c", 
			buf[3], buf[4]);

	dstate_setinfo("ups.firmware", "%c.%c%c%c", 
		buf[16], buf[17], buf[18], buf[19]);
	return 0;
}

/* FUTURE: allow variable length tests (additional data from instcmd) */
static int instcmd_btest(void)
{
	int	i, clen;
	unsigned char	cbuf[8];

	clen = 2;
	cbuf[0] = 'T';
	cbuf[1] = 0x80;		/* try for a brief test: about 20 seconds */

	for (i = 0; i < 3; i++)
		if (confirm_write(cbuf, clen))
			return STAT_INSTCMD_HANDLED;	/* FUTURE: success */

	upslogx(LOG_WARNING, "btest: UPS failed to confirm command");

	return STAT_INSTCMD_HANDLED;		/* FUTURE: failed */
}

static int instcmd_stayoff(void)
{
	int	i, clen;
	unsigned char	cbuf[8];
	double	elapsed;
	time_t	now;
	static	time_t	last = 0;

	time(&now);

	elapsed = difftime(now, last);
	last = now;

	/* for safety this must be repeated in a small window of time */
	if ((elapsed < MINCMDTIME) || (elapsed > MAXCMDTIME)) {
		upsdebugx(1, "instcmd_shutdown: outside window (%2.0f)",
			elapsed);
		return STAT_INSTCMD_HANDLED;		/* FUTURE: again */
	}

	/* this is a one-way trip unless you happen to be on battery */

	clen = 5;
	cbuf[0] = 'S';
	cbuf[1] = 0x00;
	cbuf[2] = 0x00;		
	cbuf[3] = 'W';
	cbuf[4] = '\r';

	for (i = 0; i < 3; i++)
		if (confirm_write(cbuf, clen))
			return STAT_INSTCMD_HANDLED;	/* FUTURE: success */

	upslogx(LOG_WARNING, "instcmd_shutdown: UPS failed to confirm command");

	return STAT_INSTCMD_HANDLED;		/* FUTURE: failed */
}	

static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "test.battery.start"))
		return instcmd_btest();
	if (!strcasecmp(cmdname, "shutdown.stayoff"))
		return instcmd_stayoff();

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

/* install pointers to functions for msg handlers called from msgparse */
static void setuphandlers(void)
{
	upsh.instcmd = instcmd;
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
	ser_set_speed(upsfd, device_path, B1200);

	/* dtr high, rts high */
	ser_set_dtr(upsfd, 1);
	ser_set_rts(upsfd, 1);
}

void upsdrv_initinfo(void)
{
	if (detect_hardware() == -1) {
		fatalx(EXIT_FAILURE, 
			"Unable to detect a CyberPower UPS on port %s\n"
			"Check the cabling, port name or model name and try again", device_path
		);
	}

	dstate_setinfo("ups.mfr", "CyberPower");

	/* poll once to put in some good data */
	upsdrv_updateinfo();

	printf("Detected %s on %s\n", dstate_getinfo("ups.model"), device_path); 

	dstate_addcmd("test.battery.start");
	dstate_addcmd("shutdown.stayoff");

	setuphandlers(); 
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
