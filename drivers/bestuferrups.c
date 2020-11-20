/*
   bestuferrups.c - model specific routines for Best Power Micro-Ferrups

   This module is a 40% rewritten mangle of the bestfort module by
   Grant, which is a 75% rewritten mangle of the bestups module by
   Russell.   It has no test battery command since my ME3100 does this
   by itself. (same as Grant's driver in this respect)

   Support for model RE added by Tim Thompson (7/22/04)

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

#define DRIVER_NAME	"Best Ferrups Series ME/RE/MD driver"
#define DRIVER_VERSION	"0.03"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Andreas Wrede  <andreas@planix.com>\n" \
	"John Stone  <johns@megapixel.com>\n" \
	"Grant Taylor <gtaylor@picante.com>\n" \
	"Russell Kroll <rkroll@exploits.org>\n" \
	"Tim Thompson",
	DRV_BETA, /* FIXME: STABLE? */
	{ NULL }
};

#define ENDCHAR		'\r'
#define IGNCHARS	"\012"

/* UPS Model Codes */
#define UNKNOWN         100
#define ME3100          200
#define MD1KVA          300 /* Software version P5.05 dated 05/18/89 */
#define RE1800          400

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int debugging = 0;

/* Blob of UPS configuration data from the formatconfig string */
static struct {
	int valid;			/* set to 1 when this is filled in */

	float idealbvolts;		/* various interestin battery voltages */
	float fullvolts;
	float emptyvolts;
	int va;			/* capacity of UPS in Volt-Amps */
	int watts;			/* capacity of UPS in watts */
	int model;			/* enumerated model type */
} fc;


/* Forward decls */

/* Set up all the funky shared memory stuff used to communicate with upsd */
void upsdrv_initinfo (void)
{
	/* now set up room for all future variables that are supported */

	dstate_setinfo("ups.mfr", "%s", "Best Power");
	switch(fc.model)
	{
		case ME3100:
			dstate_setinfo("ups.model", "Micro Ferrups (ME) %d", fc.va);
			break;
		case MD1KVA:
			dstate_setinfo("ups.model", "Micro Ferrups (MD) %d", fc.va);
			break;
		case RE1800:
			dstate_setinfo("ups.model", "Micro Ferrups (RE) %d", fc.va);
			break;
		default:
			fatalx(EXIT_FAILURE, "UPS model not matched!"); /* Will never get here, upsdrv_initups() will catch */
	}
	fprintf(stderr, "Best Power %s detected\n",
		dstate_getinfo("ups.model"));
	fprintf(stderr, "Battery voltages %5.1f nominal, %5.1f full, %5.1f empty\n",
	 fc.idealbvolts,
	 fc.fullvolts,
	 fc.emptyvolts);
}


/* Debugging display from kermit:

----------------------------------------------------
time^M^M^JFeb 20, 22:13:32^M^J^M^J=>id^M^JUnit ID "ME3.1K12345"^M^J^M^J=>
----------------------------------------------------
*/

static int execute(const char *cmd, char *result, int resultsize)
{
	int ret;
	char buf[256];

	ser_send(upsfd, "%s", cmd);
	ser_get_line(upsfd, buf, sizeof(buf), '\012', "", 3, 0);
	ret = ser_get_line(upsfd, result, resultsize, '\015', "\012", 3, 0);
	ser_get_line(upsfd, buf, sizeof(buf), '>', "", 3, 0);
	return ret;

}


void upsdrv_updateinfo(void)
{
	char fstring[512];

	if (! fc.valid) {
		fprintf(stderr,
			"upsupdate run before ups_ident() read ups config\n");
		assert(0);
	}

	if (execute("f\r", fstring, sizeof(fstring)) > 0) {
		int inverter=0, charger=0, vin=0, vout=0, btimeleft=0, linestat=0,
			alstat=0, vaout=0;
		double ampsout=0.0, vbatt=0.0, battpercent=0.0, loadpercent=0.0,
			hstemp=0.0, acfreq=0.0, ambtemp=0.0;
		char tmp[16];

		/* Inverter status: 0=off 1=on */
		memcpy(tmp, fstring+16, 2);
		tmp[2] = '\0';
		inverter = atoi(tmp);

		/* Charger status: 0=off 1=on */
		memcpy(tmp, fstring+18, 2);
		tmp[2] = '\0';
		charger = atoi(tmp);

		/* Input Voltage. integer number */
		memcpy(tmp, fstring+24, 4);
		tmp[4] = '\0';
		vin = atoi(tmp);

		/* Output Voltage. integer number */
		memcpy(tmp, fstring+28, 4);
		tmp[4] = '\0';
		vout = atoi(tmp);

		/* Iout: int times 10 */
		memcpy(tmp, fstring+36, 4);
		tmp[4] = '\0';
		ampsout = ((double)(atoi(tmp)) / 10.0);

		/* Battery voltage: int times 10 */
		memcpy(tmp, fstring+50, 4);
		tmp[4] = '\0';
		vbatt = ((double)(atoi(tmp)) / 10.0);

		/* Volt-amps out: int */
		memcpy(tmp, fstring+40, 6);
		tmp[6] = '\0';
		vaout = atoi(tmp);

		/* Line status.  Bitmask */
		memcpy(tmp, fstring+72, 2);
		tmp[2] = '\0';
		linestat = atoi(tmp);

		/* Alarm status reg 1.  Bitmask */
		memcpy(tmp, fstring+20, 2);
		tmp[2] = '\0';
		alstat = atoi(tmp);

		/* Alarm status reg 2.  Bitmask */
		memcpy(tmp, fstring+22, 2);
		tmp[2] = '\0';
		alstat = alstat | (atoi(tmp) << 8);

		/* AC line frequency */
		memcpy(tmp, fstring+54, 4);
		tmp[4]= '\0';
		acfreq = ((double)(atoi(tmp)) / 100.0);

		/* Runtime remaining */
		memcpy(tmp, fstring+58, 4);
		tmp[4]= '\0';
		btimeleft = atoi(tmp);

		/* UPS Temperature */
		memcpy(tmp, fstring+62, 4);
		tmp[4]= '\0';
		ambtemp = (double)(atoi(tmp));

		/* Percent Load */
		switch(fc.model)
		{
			case ME3100:
				if (execute("d 16\r", fstring, sizeof(fstring)) > 0) {
					int l;
					sscanf(fstring, "16 FullLoad%% %d", &l);
					loadpercent = (double) l;
				}
				break;
			case RE1800:
				if (execute("d 16\r", fstring, sizeof(fstring)) > 0) {
					int l;
					sscanf(fstring, "16 FullLoad%% %d", &l);
					loadpercent = (double) l;
				}
				if (execute("d 12\r", fstring, sizeof(fstring)) > 0) {
					int l;
					sscanf(fstring, "12 HS Temp  %dC", &l);
					hstemp = (double) l;
				}
				break;
			case MD1KVA:
				if (execute("d 22\r", fstring, sizeof(fstring)) > 0) {
					int l;
					sscanf(fstring, "22 FullLoad%% %d", &l);
					loadpercent = (double) l;
				}
				break;
			default: /* Will never happen, caught in upsdrv_initups() */
				fatalx(EXIT_FAILURE, "Unknown model in upsdrv_updateinfo()");
		}
		/* Compute battery percent left based on battery voltages. */
		battpercent = ((vbatt - fc.emptyvolts)
		              / (fc.fullvolts - fc.emptyvolts) * 100.0);
		if (battpercent < 0.0)
			battpercent = 0.0;
		else if (battpercent > 100.0)
			battpercent = 100.0;

		/* Compute status string */
		{
			int lowbatt, overload, replacebatt, boosting, trimming;

			lowbatt = alstat & (1<<1);
			overload = alstat & (1<<6);
			replacebatt = alstat & (1<<10);
			boosting = inverter && (linestat & (1<<2)) && (vin < 115);
			trimming = inverter && (linestat & (1<<2)) && (vin > 115);

			status_init();

			if (inverter)
				status_set("OB");
			else
				status_set("OL");

			if (lowbatt)
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

		/* FIXME: change to upsdebugx() and friends */
		if (debugging) {
			fprintf(stderr,
				"Poll: inverter %d charger %d vin %d vout %d vaout %d btimeleft %d\n",
				inverter, charger, vin, vout, vaout, btimeleft);
			fprintf(stderr,
				"      ampsout %5.1f vbatt %5.1f batpcnt %5.1f loadpcnt %5.1f upstemp %5.1f acfreq %5.2f ambtemp %5.1f\n",
				ampsout, vbatt, battpercent, loadpercent, hstemp, acfreq, ambtemp);

		}

		/* Stuff information into info structures */

		dstate_setinfo("input.voltage", "%05.1f", (double)vin);
		dstate_setinfo("output.voltage", "%05.1f", (double)vout);
		dstate_setinfo("battery.charge", "%02.1f", battpercent);
		dstate_setinfo("ups.load", "%02.1f", loadpercent);
		dstate_setinfo("battery.voltage", "%02.1f", vbatt);
		dstate_setinfo("input.frequency", "%05.2f", (double)acfreq);
		dstate_setinfo("ups.temperature", "%05.1f", (double)hstemp);
		dstate_setinfo("battery.runtime", "%d", btimeleft);
		dstate_setinfo("ambient.temperature", "%05.1f", (double)ambtemp);

		dstate_dataok();
		/* Tim: With out this return, it always falls over to the
		    datastate() at the end of the function */
		return;
	} else {

		dstate_datastale();

	} /* if (execute("f\r", fstring, sizeof(fstring)) > 0) */

	dstate_datastale();
	return;
}


static void ups_sync(void)
{
	char	buf[256];

	printf ("Syncing: ");
	fflush (stdout);

	/* A bit better sanity might be good here.  As is, we expect the
	   human to observe the time being totally not a time. */

	if (execute("time\r", buf, sizeof(buf)) > 0) {
		fprintf(stderr, "UPS Time: %s\n", buf);
	} else {
		fatalx(EXIT_FAILURE, "Error connecting to UPS");
	}
}

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{
/* NB: hard-wired password */
	ser_send(upsfd, "pw377\r");
	ser_send(upsfd, "off 1 a\r");	/* power off in 1 second and restart when line power returns */
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

	ser_send(upsfd, "\r");
	ser_get_line(upsfd, buffer, sizeof(buffer), '\r', "\012", 3, 0);
	ser_get_line(upsfd, buffer, sizeof(buffer), ENDCHAR, IGNCHARS, 3, 0);

	while (ser_get_line(upsfd, buffer, sizeof(buffer), '>', "\012", 3, 0) <= 0) {
		ser_send(upsfd, "\r");
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


void upsdrv_initups ()
{
	char	temp[256], fcstring[512];

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B1200);
	setup_serial();
	ups_sync();

	fc.model = UNKNOWN;
	/* Obtain Model */
	if (execute("id\r", fcstring, sizeof(fcstring)) < 1) {
		fatalx(EXIT_FAILURE, "Failed execute in ups_ident()");
	}

	/* response is a one-line packed string starting with $ */
	if (memcmp(fcstring, "Unit", 4)) {
		fatalx(EXIT_FAILURE,
			"Bad response from formatconfig command in ups_ident()\n"
			"id: %s\n", fcstring
		);
	}

	/* FIXME: upsdebugx() */
	if (debugging)
		fprintf(stderr, "id: %s\n", fcstring);

	/* chars 4:2 are a two-digit ascii hex enumerated model code */
	memcpy(temp, fcstring+9, 2);
	temp[2] = '\0';

	if (memcmp(temp, "ME", 2) == 0) {
		fc.model = ME3100;
	} else if ((memcmp(temp, "RE", 2) == 0)) {
		fc.model = RE1800;
	} else if (memcmp(temp, "C1", 2) == 0) {
		/* Better way to identify unit is using "d 15\r", which results in
		   "15 M#    MD1KVA", "id\r" yields "Unit ID "C1K03588"" */
		fc.model = MD1KVA;
	}

	switch(fc.model) {
		case ME3100:
			fc.va = 3100;
			fc.watts = 2200;
			/* determine shutdown battery voltage */
			if (execute("d 29\r", fcstring, sizeof(fcstring)) > 0) {
				sscanf(fcstring, "29 LowBat   %f", &fc.emptyvolts);
			}
			/* determine fully charged battery voltage */
			if (execute("d 31\r", fcstring, sizeof(fcstring)) > 0) {
				sscanf(fcstring, "31 HiBatt   %f", &fc.fullvolts);
			}
			fc.fullvolts = 54.20;
			/* determine "ideal" voltage by a guess */
			fc.idealbvolts = ((fc.fullvolts - fc.emptyvolts) * 0.7) + fc.emptyvolts;
			break;
		case RE1800:
			fc.va = 1800;
			fc.watts = 1200;
			/* determine shutdown battery voltage */
			if (execute("d 29\r", fcstring, sizeof(fcstring)) > 0) {
				sscanf(fcstring, "29 LowBat   %f", &fc.emptyvolts);
			}
			/* determine fully charged battery voltage */
			if (execute("d 31\r", fcstring, sizeof(fcstring)) > 0) {
				sscanf(fcstring, "31 HiBatt   %f", &fc.fullvolts);
			}
			fc.fullvolts = 54.20;
			/* determine "ideal" voltage by a guess */
			fc.idealbvolts = ((fc.fullvolts - fc.emptyvolts) * 0.7) + fc.emptyvolts;
			break;
		case MD1KVA:
			fc.va = 1100;
			fc.watts = 770; /* Approximate, based on 0.7 power factor */
			/* determine shutdown battery voltage */
			if (execute("d 27\r", fcstring, sizeof(fcstring)) > 0) {
				sscanf(fcstring, "27 LowBatt  %f", &fc.emptyvolts);
			}
			/* determine fully charged battery voltage */
			if (execute("d 28\r", fcstring, sizeof(fcstring)) > 0) {
				sscanf(fcstring, "28 Hi Batt  %f", &fc.fullvolts);
			}
			fc.fullvolts = 13.70;
			/* determine "ideal" voltage by a guess */
			fc.idealbvolts = ((fc.fullvolts - fc.emptyvolts) * 0.7) + fc.emptyvolts;
			break;
		default:
			fatalx(EXIT_FAILURE, "Unknown model %s in ups_ident()", temp);
	}

	fc.valid = 1;
	return;
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
