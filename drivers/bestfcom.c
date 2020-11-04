/*
   bestfcom.c - model specific routines for Best Power F-Command ups models

   This module is yet another rewritten mangle of the bestuferrups
   driver.  This driver was written in an attempt to consolidate
   the various Best Fortress/FERRUPS modules that support the
   'f'-command set and provide support for more of these models.

   Models tested with this new version:
   FortressII	LI720
   FERRUPS	FE2.1K
   FERRUPS	FE4.3K
   FERRUPS	FE18K
   FERRUPS	FD4.3K

   From bestuferrups.c :

   This module is a 40% rewritten mangle of the bestfort module by
   Grant, which is a 75% rewritten mangle of the bestups module by
   Russell.   It has no test battery command since my ME3100 does this
   by itself. (same as Grant's driver in this respect)

   Copyright (C) 2002  Andreas Wrede  <andreas@planix.com>
   Copyright (C) 2000  John Stone  <johns@megapixel.com>
   Copyright (C) 2000  Grant Taylor <gtaylor@picante.com>
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

#define DRIVER_NAME	"Best Ferrups/Fortress driver"
#define DRIVER_VERSION	"0.12"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Andreas Wrede  <andreas@planix.com>\n" \
	"John Stone  <johns@megapixel.com>\n" \
	"Grant Taylor <gtaylor@picante.com>\n" \
	"Russell Kroll <rkroll@exploits.org>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define ENDCHAR			'\r'
#define IGNCHARS		"\012"
#define UPSDELAY		1

/* BEST Factory UPS Model Codes */
#define FORTRESS		00
#define PATRIOT			01
#define FORTRESSII		02
#define FERRUPS			03
#define UNITY1			04

/* Internal driver UPS Model Codes */
#define UNKNOWN			000
#define FDxxxx			100
#define FExxxx			200
#define LIxxxx			300
#define MExxxx			400
#define MDxxxx			500

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Blob of UPS configuration data from the formatconfig string */
struct {
	int valid;			/* set to 1 when this is filled in */

	float  idealbvolts;		/* various interesting battery voltages */
	float  fullvolts;
	float  lowvolts;
	float  emptyvolts;

	int va;				/* capacity of UPS in Volt-Amps */
	int watts;			/* capacity of UPS in watts */
	int model;			/* enumerated model type */
	int type;			/* enumerated ups type*/

	char name[16];			/* ups type name*/

} fc;

static int inverter_status;

/* Forward decls */

/* Set up all the funky shared memory stuff used to communicate with upsd */
void  upsdrv_initinfo (void)
{
	/* now set up room for all future variables that are supported */

	/*
	dstate_setinfo("driver.name", "%s", "bestfcom");
	*/
	dstate_setinfo("ups.mfr", "%s", "Best Power");
	switch(fc.model) {
		case MExxxx:
			dstate_setinfo("ups.model", "%s ME%d", fc.name, fc.va);
			break;
		case MDxxxx:
			dstate_setinfo("ups.model", "%s MD%d", fc.name, fc.va);
			break;
		case FDxxxx:
			dstate_setinfo("ups.model", "%s FD%d", fc.name, fc.va);
			break;
		case FExxxx:
			dstate_setinfo("ups.model", "%s FE%d", fc.name, fc.va);
			break;
		case LIxxxx:
			dstate_setinfo("ups.model", "%s LI%d", fc.name, fc.va);
			break;
		default:
			fatalx(EXIT_FAILURE, "Unknown model - oops!"); /* Will never get here, upsdrv_initups() will catch */
	}

	dstate_setinfo("ups.power.nominal", "%d", fc.va);
	dstate_setinfo("ups.realpower.nominal", "%d", fc.watts);

	/* Do we really need to waste time on this? */
	/*
	if (fc.model != FDxxxx) {
		if (execute("d 00\r", tmp, sizeof(tmp)) > 0)
				sscanf(tmp, "00 Time %8s", time);

		if (execute("d 10\r", tmp, sizeof(tmp)) > 0)
				sscanf(tmp, "10 Date %8s", date);

		dstate_setinfo("ups.time", "%s", time);
		dstate_setinfo("ups.date", "%s", date);
		}
	*/

	dstate_setinfo("battery.voltage.nominal", "%05.2f", (double)fc.idealbvolts);

	upsdebugx(1, "Best Power %s detected", dstate_getinfo("ups.model"));
	upsdebugx(1, "Battery voltages: %5.2f nominal, %5.2f full, %5.2f low, %5.2f empty",
		fc.idealbvolts,
		fc.fullvolts,
		fc.lowvolts,
		fc.emptyvolts);
}


/* atoi() without the freebie octal conversion */
int bcd2i (const char *bcdstring, const int bcdlen)
{
	int i, digit, total = 0, factor = 1;
	for (i = 1; i < bcdlen; i++)
		factor *= 10;
	for (i = 0; i < bcdlen; i++) {
		digit = bcdstring[i] - '0';
		if (digit > 9) {
			digit = 0;
		}
		total += digit * factor;
		factor /= 10;
	}
	return total;
}

#define POLL_ALERT "{"
static void alert_handler(char ch)
{
	char buf[SMALLBUF];

	/* Received an Inverter status alarm :
	 * "\r\n{Inverter:     On}\r\n=>"
	 * Try to flush the message
	 */
	ser_get_line(upsfd, buf, sizeof(buf), '\012', "", 0, 20000);
}

/* Debugging display from kermit:
----------------------------------------------------
time^M^M^JFeb 20, 22:13:32^M^J^M^J=>id^M^JUnit ID "ME3.1K12345"^M^J^M^J=>
----------------------------------------------------
*/
static int execute(const char *cmd, char *result, int resultsize)
{
	int ret;
	char buf[SMALLBUF];
	unsigned char ch;

	/* Check for the Inverter status alarm if pending :
	 * "\r\n{Inverter:     On}\r\n=>"
	 */
	ser_get_line_alert(upsfd, buf, sizeof(buf), '\012', "",
		POLL_ALERT, alert_handler, 0, 20000);

	ser_send(upsfd, "%s", cmd);

	/* Give the UPS some time to chew on what we just sent */
	usleep(50000);

	/* delete command echo up to \012 but no further */
	for (ch = '\0'; ch != '\012'; ser_get_char(upsfd, &ch, 0, 10000));

	/* get command response	*/
	ret = ser_get_line(upsfd, result, resultsize, '\015', "\012", 3, 0);

	return ret;
}


/*
format command response -> 80 chars
                 chrg                                               line status
                  ||alrm                                                ||
Date          Invtr |12|                                             error
|  ||Time|      ||      |Vi||Vo|    |Io|| VA |    |Vb||Hz||rt|        ||  |vr|CS
011314581801000000010000011601160000002300026600000265600000190000000000E00106E6\r
01161706430100010001000002040121000000980011890000057959980001002200000064080727\r
011800364801000100010000021301200000003100037100001343599803060024000000680807A0\r
0121022719010001000100000208011900190000000000000005676001082200350000000006101A\r
00000000000100000000000002370236000000220005190000026850000009002600000000030161\r
0    0    1    1    2    2    3    3    4    4    5    5    6    6    7    7    8
0    5    0    5    0    5    0    5    0    5    0    5    0    5    0    5    0

Above f-responses listed in this order:
 FortressII	LI720
 FERRUPS	FE4.3K
 FERRUPS	FE18K
 FERRUPS	FD4.3K
 Fortress	?????? (from Holger's old Best Fortress notes)
*/

void upsdrv_updateinfo(void)
{
	char fstring[512];

	if (! fc.valid) {
		upsdebugx(1, "upsupdate run before ups_ident() read ups config");
		assert(0);
	}

	if (execute("f\r", fstring, sizeof(fstring)) >= 80) {
		int inverter=0, charger=0, vin=0, vout=0, btimeleft=0, linestat=0,
			alstat=0, vaout=0;

		double ampsout=0.0, vbatt=0.0, battpercent=0.0, loadpercent=0.0,
			upstemp=0.0, acfreq=0.0;

		char tmp[32];

		upsdebugx(3, "f response: %d %s", (int)strlen(fstring), fstring);

		/* Inverter status.	 0=off 1=on */
		inverter = bcd2i(&fstring[16], 2);

		/* Charger status.	0=off 1=on */
		charger = bcd2i(&fstring[18], 2);

		/* Input Voltage. integer number */
		vin	 = bcd2i(&fstring[24], 4);

		/* Output Voltage. integer number */
		vout = bcd2i(&fstring[28], 4);

		/* Battery voltage.	 int times 10 */
		vbatt = ((double)bcd2i(&fstring[50], 4) / 10.0);

		/* Alarm status reg 1.	Bitmask */
		alstat = bcd2i(&fstring[20], 2);

		/* Alarm status reg 2.	Bitmask */
		alstat = alstat | (bcd2i(&fstring[22], 2) << 8);

		/* AC line frequency */
		acfreq = ((double)bcd2i(&fstring[54], 4) / 100.0);

		/* Runtime remaining (UPS reports minutes) */
		btimeleft = bcd2i(&fstring[58], 4) * 60;

		if (fc.model != FDxxxx) {
			/* Iout.  int times 10 */
			ampsout = ((double)bcd2i(&fstring[36], 4) / 10.0);

			/* Volt-amps out.  int	*/
			vaout = bcd2i(&fstring[40], 6);

			/* Line status.	 Bitmask */
			linestat = bcd2i(&fstring[72], 2);

		}

		if (fc.model != LIxxxx) {
			upstemp = (double) bcd2i(&fstring[62], 4);
		}

		/* Percent Load */
		switch(fc.model) {
			case LIxxxx:
			case FDxxxx:
			case FExxxx:
			case MExxxx:
				if (execute("d 16\r", tmp, sizeof(tmp)) > 0) {
					int l;
					sscanf(tmp, "16 FullLoad%% %d", &l);
					loadpercent = (double) l;
				}
				break;
			case MDxxxx:
				if (execute("d 22\r", tmp, sizeof(tmp)) > 0) {
					int l;
					sscanf(tmp, "22 FullLoad%% %d", &l);
					loadpercent = (double) l;
				}
				break;
			default: /* Will never happen, caught in upsdrv_initups() */
				fatalx(EXIT_FAILURE, "Unknown model in upsdrv_updateinfo()");
		}

		/* Compute battery percent left based on battery voltages. */
		battpercent = ((vbatt - fc.emptyvolts)
					   / (fc.idealbvolts - fc.emptyvolts) * 100.0);
		if (battpercent < 0.0)
			battpercent = 0.0;
		else if (battpercent > 100.0)
			battpercent = 100.0;

		/* Compute status string */
		{
			int lowbatt, lowvolts, overload, replacebatt, boosting, trimming;

			lowbatt = alstat & (1<<1);
			overload = alstat & (1<<6);
			replacebatt = alstat & (1<<10);

			boosting = inverter && (linestat & (1<<2)) && (vin < 115);
			trimming = inverter && (linestat & (1<<2)) && (vin > 115);

			/* status bits can be unreliable, so try to help it out */
			lowvolts = (vbatt <= fc.lowvolts);

			status_init();

			if (inverter) {
				if (inverter_status < 1) {
					upsdebugx(1, "Inverter On, charger: %d battery time left: %d",
						charger, btimeleft);
				}
				inverter_status = 1;
				status_set("OB");
			} else {
				if (inverter_status) {
					upsdebugx(1, "Inverter Off, charger: %d battery time left: %d",
						charger, btimeleft);
				}
				inverter_status = 0;
				status_set("OL");
			}

			if (lowbatt | lowvolts)
				status_set("LB");

			if (trimming)
				status_set("TRIM");

			if (boosting)
				status_set("BOOST");

			if (replacebatt)
				status_set("RB");

			if (overload)
				status_set("OVER");

			status_commit();
		}

		upsdebugx(2,
			"Poll: inverter %d charger %d vin %d vout %d vaout %d btimeleft %d",
				inverter, charger, vin, vout, vaout, btimeleft);
		upsdebugx(2,
			"      vbatt %5.1f batpcnt %5.1f loadpcnt %5.1f upstemp %5.1f ampsout %5.1f acfreq %5.2f",
				vbatt, battpercent, loadpercent, upstemp, ampsout, acfreq);

		/* Stuff information into info structures */
		dstate_setinfo("input.voltage", "%05.1f", (double)vin);
		dstate_setinfo("input.frequency", "%05.2f", acfreq);
		dstate_setinfo("output.voltage", "%05.1f", (double)vout);
		dstate_setinfo("output.current", "%04.1f", ampsout);
		dstate_setinfo("battery.charge", "%02.1f", battpercent);
		dstate_setinfo("battery.voltage", "%02.1f", vbatt);
		dstate_setinfo("battery.runtime", "%d", btimeleft);
		dstate_setinfo("ups.load", "%02.1f", loadpercent);
		if (vaout)
			dstate_setinfo("ups.power", "%d", vaout);
		if (upstemp)
			dstate_setinfo("ups.temperature", "%05.1f", (double)upstemp);

		dstate_dataok();

	} else {

		upsdebugx(1, "failed f response. strlen: %d", (int)strlen(fstring));
		dstate_datastale();

	} /* if (execute("f\r", fstring, sizeof(fstring)) >= 80) */

	return;
}


static void ups_sync(void)
{
	char buf[256];

	/* A bit better sanity might be good here. As is, we expect the
	 human to observe the time being totally not a time. */

	if (execute("time\r", buf, sizeof(buf)) > 0) {
		upsdebugx(1, "UPS Time: %s", buf);
	} else {
		fatalx(EXIT_FAILURE, "Error connecting to UPS.");
	}
	/* old Ferrups prompt for new time so send a blank line */
	execute("\r", buf, sizeof(buf));
	ser_get_line(upsfd, buf, sizeof(buf), '>', "\012", 3, 0);
}

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{
	/* NB: hard-wired password */
	ser_send(upsfd, "pw377\r");
	ser_send(upsfd, "o 10 a\r");	/* power off in 10 seconds and restart when line power returns, FE7K required a min of 5 seconds for off to function */
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
}

void upsdrv_help(void)
{
}

static void sync_serial(void) {
	char buffer[10];

	ser_flush_in(upsfd, "", 1);

	ser_send(upsfd, "\r");
	sleep(UPSDELAY);
	ser_get_line(upsfd, buffer, sizeof(buffer), '\r', "\012", 3, 0);
	ser_get_line(upsfd, buffer, sizeof(buffer), ENDCHAR, IGNCHARS, 3, 0);

	while (ser_get_line(upsfd, buffer, sizeof(buffer), '>', "\012", 3, 0) <= 0) {
		printf(".");
		ser_send(upsfd, "\r");
		sleep(UPSDELAY);
	}
}

/* Begin code stolen from bestups.c */
static void setup_serial(void)
{
	struct termios tio;

	if (tcgetattr(upsfd, &tio) == -1)
		fatal_with_errno(EXIT_FAILURE, "tcgetattr");

	tio.c_iflag = IXON | IXOFF;
	tio.c_oflag = 0;
	tio.c_cflag = (CS8 | CREAD | HUPCL | CLOCAL);
	tio.c_lflag = 0;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;

#ifdef HAVE_CFSETISPEED
	cfsetispeed(&tio, B1200); /* baud change here */
	cfsetospeed(&tio, B1200);
#else
#error This system lacks cfsetispeed() and has no other means to set the speed
#endif

	if (tcsetattr(upsfd, TCSANOW, &tio) == -1)
		fatal_with_errno(EXIT_FAILURE, "tcsetattr");
/* end code stolen from bestups.c */

	sync_serial();
}

/*
These models don't support the formatconfig (fc) command so use
the identify command.

"id\r" returns :

FERRUPS Uninterruptible Power System
By Best Power Technology, Inc.
Route 1 Highway 80 / P.O. Box 280
Necedah, WI	 54646	USA
Sales:	 (800) 356-5794
Service: (800) 356-5737
FAX:	 (608) 565-2221

Copyright (C) 1993, 1994, 1995 Best Power Technology, Inc.

Model:	  FE4.3KVA
Unit ID:  FE4.3K02376
Serial #: FE4.3K02376
Version:  8.07
Released: 08/01/1995
*/

void upsdrv_init_nofc(void)
{
	char tmp[256], rstring[1024];

	/* This is a Best UPS
	 * Set initial values for old Fortress???
	 */

	/* Attempt the id command	*/
	ser_send(upsfd, "id\r");

	/* prevent upsrecv from timing out	*/
	sleep(UPSDELAY);

	ser_get_line(upsfd, rstring, sizeof(rstring), '>', "", 3, 0);

	rstring[sizeof(rstring) - 1] = '\0';
	upsdebugx(2, "id response: %s", rstring);

	/* Better way to identify this unit is using "d 15\r", which results in
	   "15 M#	 MD1KVA", "id\r" yields "Unit ID "C1K03588"" */
	if (strstr(rstring, "Unit ID \"C1K")){
		fc.model = MDxxxx;
		snprintf(fc.name, sizeof(fc.name), "%s", "Micro Ferrups");

		/* Determine load rating by Unit Id? */
		if (strstr(rstring, "Unit ID \"C1K"))  {
			fc.va = 1100;
			fc.watts = 770; /* Approximate, based on 0.7 power factor */
		}
	} else
	if (strstr(rstring, "Unit ID \"ME")){
		fc.model = MExxxx;
		snprintf(fc.name, sizeof(fc.name), "%s", "Micro Ferrups");

		/* Determine load rating by Unit Id? */
		if (strstr(rstring, "Unit ID \"ME3.1K"))  {
			fc.va = 3100;
			fc.watts = 2200;
		}
	} else
	if (strstr(rstring, "Unit ID \"FD")){
		fc.model = FDxxxx;
		snprintf(fc.name, sizeof(fc.name), "%s", "Ferrups");

		/* Determine load rating by Unit Id? */
		if (strstr(rstring, "Unit ID \"FD4.3K"))  {
			fc.va = 4300;
			fc.watts = 3000;
		}
	} else
	if (strstr(rstring, "Model:	   FE")
	    || strstr(rstring, "Model:    FE"))
	{
		fc.model = FExxxx;
		fc.type = FERRUPS;
		snprintf(fc.name, sizeof(fc.name), "%s", "Ferrups");
	} else
	if (strlen(rstring) < 300 ) {
		/* How does the old Fortress respond to this? */
		upsdebugx(2, "Old Best Fortress???");
		/* fc.model = FORTRESS; */
	}

	if (fc.model == UNKNOWN) {
		fatalx(EXIT_FAILURE, "Unknown model %s in upsdrv_init_nofc()", rstring);
	}

	switch(fc.model) {
		case MExxxx:
		case MDxxxx:
		case FDxxxx:
			/* determine shutdown battery voltage */
			if (execute("d 27\r", tmp, sizeof(tmp)) > 0) {
				sscanf(tmp, "27 LowBatt %f", &fc.emptyvolts);
			}
			/* determine near low battery voltage */
			if (execute("d 30\r", tmp, sizeof(tmp)) > 0) {
				sscanf(tmp, "30 NLBatt %f", &fc.lowvolts);
			}
			/* determine fully charged battery voltage */
			if (execute("d 28\r", tmp, sizeof(tmp)) > 0) {
				sscanf(tmp, "28 Hi Batt %f", &fc.fullvolts);
			}
			fc.fullvolts = 13.70;
			/* determine "ideal" voltage by a guess */
			fc.idealbvolts = ((fc.fullvolts - fc.emptyvolts) * 0.7) + fc.emptyvolts;
			break;
		case FExxxx:
			if (execute("d 45\r", tmp, sizeof(tmp)) > 0) {
				sscanf(tmp, "45 RatedVA %d", &fc.va);			/* 4300  */
			}
			if (execute("d 46\r", tmp, sizeof(tmp)) > 0) {
				sscanf(tmp, "46 RatedW %d", &fc.watts);		/* 3000  */
			}
			if (execute("d 65\r", tmp, sizeof(tmp)) > 0) {
				sscanf(tmp, "65 LoBatV %f", &fc.emptyvolts);	/* 41.00 */
			}
			if (execute("d 66\r", tmp, sizeof(tmp)) > 0) {
				sscanf(tmp, "66 NLBatV %f", &fc.lowvolts);		/* 44.00 */
			}
			if (execute("d 67\r", tmp, sizeof(tmp)) > 0) {
				sscanf(tmp, "67 HiBatV %f", &fc.fullvolts);	/* 59.60 */
			}
			fc.idealbvolts = ((fc.fullvolts - fc.emptyvolts) * 0.7) + fc.emptyvolts;
			if (fc.va < 1.0) {
				fatalx(EXIT_FAILURE, "Error determining Ferrups UPS rating.");
			}
			break;
		default:
			fatalx(EXIT_FAILURE, "Unknown model %s in upsdrv_init_nofc()", rstring);
			break;
	}
	fc.valid = 1;
}

/*
These models support the formatconfig (fc) command

formatconfig (fc) response is a one-line packed string starting with $
   Model        Wt rat  Vout  VHi    FrLo    BatVN  BatNLo  LRuntime    Model
   |  |         |   |   | |   | |    |  |    |  |    |  |    | |       |      |  E
rev       VA rat     Vin   VLo   FrN     FrHi    BatHi   BatLo      Opt          O
 ||        |   |     | |   | |   |  |    |  |    |  |    |  |         |          T
$010207010600720004701201200911446000570063000240028802150190003??????\LI????VA\\|
$010207010600720004701201200911446000570063000240028802150190003??????\LI720VU\LI720VU18112\|
0    0    1    1    2    2    3    3    4    4    5    5    6    6    7    7    8
0    5    0    5    0    5    0    5    0    5    0    5    0    5    0    5    0

Model: [0,1] => 00 = unk, 01 = Patriot/SPS, 02 = FortressII, 03 = Ferrups, 04 = Unity/1
       [2,3] => 00 = LI520, 01 = LI720, 02 = LI1020, 03 = LI1420, 07 = ???
*/

void upsdrv_init_fc(const char *fcstring)
{
	char tmp[256];

	upsdebugx(3, "fc response: %d %s", (int)strlen(fcstring), fcstring);

	/* Obtain Model */
	if (memcmp(fcstring, "$", 1)) {
		fatalx(EXIT_FAILURE, "Bad response from formatconfig command in upsdrv_init_fc()");
	}
	if (memcmp(fcstring+3, "00", 2) == 0) {
		fatalx(EXIT_FAILURE, "UPS type unknown in upsdrv_init_fc()");
	}

	if (memcmp(fcstring+3, "01", 2) == 0) {
		fatalx(EXIT_FAILURE, "Best Patriot UPS not supported");
	}
	else if (memcmp(fcstring+3, "02", 2) == 0) {
		snprintf(fc.name, sizeof(fc.name), "%s", "FortressII");
		fc.type = FORTRESSII;
	}
	else if (memcmp(fcstring+3, "03", 2) == 0) {
		snprintf(fc.name, sizeof(fc.name), "%s", "Ferrups");
		fc.type = FERRUPS;
	}
	else if (memcmp(fcstring+3, "04", 2) == 0) {
		snprintf(fc.name, sizeof(fc.name), "%s", "Unity/1");
		fc.type = UNITY1;
	}

	/*	(fc.type == FORTRESSII || fc.type == FERRUPS || fc.type == UNITY1) */
	if (memcmp(fcstring+5, "00", 2) == 0) {
		/* fc.model = LI520; */
		fc.model = LIxxxx;
	}
	else if (memcmp(fcstring+5, "01", 2) == 0) {
		/* fc.model = LI720; */
		fc.model = LIxxxx;
	}
	else if (memcmp(fcstring+5, "02", 2) == 0) {
		/* fc.model = LI1020; */
		fc.model = LIxxxx;
	}
	else if (memcmp(fcstring+5, "03", 2) == 0) {
		/* fc.model = LI1420; */
		fc.model = LIxxxx;
	}
	else if (memcmp(fcstring+71, "LI", 2) == 0) {
		fc.model = LIxxxx;
	}

	switch(fc.model) {
		case LIxxxx:
			fc.va = bcd2i(&fcstring[11], 5);
			fc.watts = bcd2i(&fcstring[16], 5);

			/* determine shutdown battery voltage */
			fc.emptyvolts= ((double)bcd2i(&fcstring[57], 4) / 10.0);

			/* determine fully charged battery voltage */
			fc.lowvolts= ((double)bcd2i(&fcstring[53], 4) / 10.0);

			/* determine fully charged battery voltage */
			fc.fullvolts= ((double)bcd2i(&fcstring[49], 4) / 10.0);

			/* determine "ideal" voltage by a guess */
			fc.idealbvolts = ((fc.fullvolts - fc.emptyvolts) * 0.7) + fc.emptyvolts;
			break;
		default:
			fatalx(EXIT_FAILURE, "Unknown model %s in upsdrv_init_fc()", tmp);
	}
	fc.valid = 1;
}

void upsdrv_initups(void)
{
	char rstring[256];

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B1200);
	setup_serial();
	ups_sync();

	inverter_status = 0;
	fc.model = UNKNOWN;
	if (execute("f\r", rstring, sizeof(rstring)) < 1 ) {
		fatalx(EXIT_FAILURE, "Failed format request in upsdrc_initups()");
	}

	execute("fc\r", rstring, sizeof(rstring));
	if (strlen(rstring) < 80 ) {
		ser_get_line(upsfd, rstring, sizeof(rstring), '>', "\012", 3, 0);
		upsdrv_init_nofc();
	} else {
		upsdrv_init_fc(rstring);
	}

	return;
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
