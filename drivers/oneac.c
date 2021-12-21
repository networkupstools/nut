/*vim ts=4*/
/* oneac.c - Driver for Oneac UPS using the Advanced Interface.
 *
 * Supported Oneac UPS families in this driver:
 * EG (late 80s, early 90s, plug-in serial interface card)
 * ON (early and mid-90s, plug-in serial interface card)
 * OZ (mid-90s on, DB-25 std., interface slot)
 * OB (early 2000's on, big cabinet, DB-25 std., interface slot)
 *
 * Copyright (C)
 *     2003 by Eric Lawson <elawson@inficad.com>
 *     2012 by Bill Elliot <bill@wreassoc.com>
 *
 * This program was sponsored by MGE UPS SYSTEMS, and now Eaton
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
 * History:
 * - 7 February 2012.  Bill Elliot
 * Enhancing the driver for additional capabilities and later units.
 *
 * - 28 November 2003.  Eric Lawson
 * More or less complete re-write for NUT 1.5.9
 * This was somewhat easier than trying to beat the old driver code
 * into submission
 *
 */

#include "main.h"
#include "serial.h"
#include "oneac.h"

/* Prototypes to allow setting pointer before function is defined */
int setcmd(const char* varname, const char* setvalue);
int instcmd(const char *cmdname, const char *extra);

#define DRIVER_NAME	"Oneac EG/ON/OZ/OB UPS driver"
#define DRIVER_VERSION	"0.80"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Bill Elliot <bill@wreassoc.com>\n"
	"Eric Lawson <elawson@inficad.com>",
	DRV_STABLE,
	{ NULL }
};

#define SECS 0			/* Serial function wait time*/
#define USEC 500000		/* Rest of serial function wait time*/

#define COMM_TRIES	3	/* Serial retries before "stale" */

static char UpsFamily [3];

/****************************************************************
 * Below are functions used only in this oneac driver			*
 ***************************************************************/

/* Since an installed network card may delay responses from the UPS
 *  allow for a repeat of the get request.  Also confirm that
 *  the correct number of characters are returned.
 */

static ssize_t OneacGetResponse (char* chBuff, const size_t BuffSize, int ExpectedCount)
{
	int Retries = 10;		/* x/2 seconds max with 500000 USEC */
	ssize_t return_val;

	do
	{
		return_val = ser_get_line(upsfd,
			chBuff, BuffSize, ENDCHAR, IGNCHARS, SECS, USEC);

		if (return_val == ExpectedCount)
			break;

		upsdebugx (3,
			"!OneacGetResponse retry (%zd, %d)...",
			return_val, Retries);

	} while (--Retries > 0);

	upsdebugx (4,"OneacGetResponse buffer: %s",chBuff);

	if (Retries == 0)
	{
		upsdebugx (2,"!!OneacGetResponse timeout...");
		return_val = 1;					/* Comms error */
	}
	else
	{
		if (Retries < 10)
			upsdebugx (2,"OneacGetResponse recovered (%d)...", Retries);

		return_val = 0;					/* Good comms */
	}

	return return_val;
}

static void do_battery_test(void)
{
	char buffer[32];

	if (getval("testtime") == NULL)
		snprintf(buffer, 3, "%s", DEFAULT_BAT_TEST_TIME);
	else {
		snprintf(buffer, 3, "%s", getval("testtime"));

	/*the UPS wants this value to always be two characters long*/
	/*so put a zero in front of the string, if needed....      */
		if (strlen(buffer) < 2) {
			buffer[2] = '\0';
			buffer[1] = buffer[0];
			buffer[0] = '0';
		}
	}
	ser_send(upsfd,"%s%s%s",BAT_TEST_PREFIX,buffer,COMMAND_END);
}

static int SetOutputAllow(const char* lowval, const char* highval)
{
	char buffer[32];

	snprintf(buffer, 4, "%.3s", lowval);

	/*the UPS wants this value to always be three characters long*/
	/*so put a zero in front of the string, if needed....      */

	if (strlen(buffer) < 3)
	{
		buffer[3] = '\0';
		buffer[2] = buffer[1];
		buffer[1] = buffer[0];
		buffer[0] = '0';
	}

	upsdebugx (2,"SetOutputAllow sending %s%.3s,%.3s...",
											SETX_OUT_ALLOW, buffer, highval);

	ser_send(upsfd,"%s%.3s,%.3s%s", SETX_OUT_ALLOW, buffer, highval,
																COMMAND_END);
	ser_get_line(upsfd,buffer,sizeof(buffer), ENDCHAR, IGNCHARS,SECS,USEC);

	if(buffer[0] == DONT_UNDERSTAND)
	{
		upsdebugx (2,"SetOutputAllow got asterisk back...");

		return 1;					/* Invalid command */
	}

	return 0;						/* Valid command */
}

static void EliminateLeadingZeroes (const char* buff1, int StringSize, char* buff2,
															const size_t buff2size)
{
	int i = 0;
	int j = 0;

	memset(buff2, '\0', buff2size);			/* Fill with nulls */

	/* Find first non-'0' */
	while ((i < (StringSize - 1) && (buff1[i] == '0')))
	{
		i++;
	}

	while (i < StringSize)					/* Move rest of string */
	{
		buff2[j++] = buff1[i++];
	}
}


/****************************************************************
 * Below are the commands that are called by main				*
 ***************************************************************/

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);

	/*get the UPS in the right frame of mind*/
	ser_send_pace(upsfd, 100, "%s", COMMAND_END);
	ser_send_pace(upsfd, 100, "%s", COMMAND_END);
	sleep (1);
}

void upsdrv_initinfo(void)
{
	int i,j, k;
	int VRange=0;
	int timevalue;
	ssize_t RetValue;
	char buffer[256], buffer2[32];

	/* All families should reply to this request so we can confirm that it is
	 *  an ONEAC UPS
	 */

	ser_flush_in(upsfd,"",0);
	ser_send(upsfd,"%c%s",GET_FAMILY,COMMAND_END);

	if(OneacGetResponse (buffer, sizeof(buffer), 2))
	{
		fatalx(EXIT_FAILURE, "Serial timeout with ONEAC UPS on %s\n",
																device_path);
	}
	else
	{
		if (strncmp(buffer,FAMILY_ON,FAMILY_SIZE) != 0 &&
			strncmp(buffer,FAMILY_OZ,FAMILY_SIZE) != 0 &&
			strncmp(buffer,FAMILY_OB,FAMILY_SIZE) != 0 &&
			strncmp(buffer,FAMILY_EG,FAMILY_SIZE) != 0)
		{
			fatalx(EXIT_FAILURE, "Did not find an ONEAC UPS on %s\n",
																device_path);
		}
	}

	/* UPS Model (either EG, ON, OZ or OB series of UPS) */
	strncpy(UpsFamily, buffer, FAMILY_SIZE);
	UpsFamily[2] = '\0';
	dstate_setinfo("device.model", "%s",UpsFamily);
	printf("Found %s family of Oneac UPS\n", UpsFamily);

	dstate_setinfo("ups.type", "%s", "Line Interactive");

	dstate_addcmd("test.battery.start.quick");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("test.failure.start");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("shutdown.reboot");

	upsh.setvar = setcmd;
	upsh.instcmd = instcmd;

	/* set some stuff that shouldn't change after initialization */
	/* this stuff is common to all families of UPS */

	ser_send(upsfd,"%c%s",GET_ALL,COMMAND_END);

	if (strncmp(UpsFamily, FAMILY_EG, FAMILY_SIZE) == 0)
	{
		RetValue = OneacGetResponse (buffer, sizeof(buffer),
														GETALL_EG_RESP_SIZE);
	}
	else
	{
		RetValue = OneacGetResponse (buffer, sizeof(buffer), GETALL_RESP_SIZE);
	}

	if(RetValue)
	{
		fatalx(EXIT_FAILURE, "Serial timeout(2) with ONEAC UPS on %s\n",
																device_path);
	}

	/* Manufacturer */
	dstate_setinfo("device.mfr", "%.5s", buffer);

	/*firmware revision*/
	dstate_setinfo("ups.firmware", "%.3s",buffer+7);

	/*nominal AC frequency setting --either 50 or 60*/
	dstate_setinfo("input.frequency.nominal", "%.2s", buffer+20);
	dstate_setinfo("output.frequency.nominal", "%.2s", buffer+20);

	/* Shutdown delay in seconds...can be changed by user */
	if (getval("offdelay") == NULL)
		dstate_setinfo("ups.delay.shutdown", "0");
	else
		dstate_setinfo("ups.delay.shutdown", "%s", getval("offdelay"));

	dstate_setflags("ups.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW);
	dstate_setaux("ups.delay.shutdown", GET_SHUTDOWN_RESP_SIZE);

	/* Setup some ON/OZ/OB only stuff ... i.e. not EG */

	if (strncmp(UpsFamily, FAMILY_EG, FAMILY_SIZE) != 0)
	{
		dstate_addcmd("reset.input.minmax");

		/*nominal input voltage*/

		VRange = buffer[26];			/* Keep for later use also */

		switch (VRange)					/* Will be '1' or '2' */
		{
			case V120AC:
				dstate_setinfo("input.voltage.nominal", "120");
				dstate_setinfo("output.voltage.nominal", "120");
				break;

			case V230AC:
				dstate_setinfo("input.voltage.nominal", "230");
				dstate_setinfo("output.voltage.nominal", "230");
				break;

			default:
				upslogx(LOG_INFO,"Oneac: "
					"Invalid nom voltage parameter from UPS [%c]", VRange);
		}
	}

	/* Setup some OZ/OB only stuff */

	if ((strncmp (UpsFamily, FAMILY_OZ, FAMILY_SIZE) == 0) ||
		(strncmp (UpsFamily, FAMILY_OB, FAMILY_SIZE) == 0))
	{
		dstate_addcmd("test.panel.start");
		dstate_addcmd("test.battery.start.deep");
		dstate_addcmd("beeper.enable");
		dstate_addcmd("beeper.disable");
		dstate_addcmd("beeper.mute");

		dstate_setaux("ups.delay.shutdown", GETX_SHUTDOWN_RESP_SIZE);

		ser_flush_in(upsfd,"",0);
		ser_send(upsfd,"%c%s",GETX_ALL_2,COMMAND_END);
		if(OneacGetResponse (buffer, sizeof(buffer), GETX_ALL2_RESP_SIZE))
		{
			fatalx(EXIT_FAILURE, "Serial timeout(3) with ONEAC UPS on %s\n",
																device_path);
		}

		/* Low and high output trip points */
		EliminateLeadingZeroes (buffer+73, 3, buffer2, sizeof(buffer2));
		dstate_setinfo("input.transfer.low", "%s", buffer2);
		dstate_setflags("input.transfer.low", ST_FLAG_STRING | ST_FLAG_RW );
		dstate_setaux("input.transfer.low", 3);

		EliminateLeadingZeroes (buffer+76, 3, buffer2, sizeof(buffer2));
		dstate_setinfo("input.transfer.high", "%s", buffer2);
		dstate_setflags("input.transfer.high", ST_FLAG_STRING | ST_FLAG_RW);
		dstate_setaux("input.transfer.high", 3);

		/* Restart delay */
		EliminateLeadingZeroes (buffer+84, 4, buffer2, sizeof(buffer2));
		dstate_setinfo("ups.delay.start", "%s", buffer2);
		dstate_setflags("ups.delay.start", ST_FLAG_STRING | ST_FLAG_RW);
		dstate_setaux("ups.delay.start", 4);

		/* Low Batt at time */
		strncpy(buffer2, buffer+82, 2);
		buffer2[2]='\0';
		timevalue = atoi(buffer2) * 60;		/* Change minutes to seconds */
		dstate_setinfo("battery.runtime.low", "%d",timevalue);
		dstate_setflags("battery.runtime.low", ST_FLAG_STRING | ST_FLAG_RW);
		dstate_setaux("battery.runtime.low", 2);

		/*Get the actual model string for ON UPS reported as OZ/OB family*/

		/*UPS Model (full string)*/
		memset(buffer2, '\0', 32);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRINGOP_TRUNCATION
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
		strncpy(buffer2, buffer + 5, 10);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic pop
#endif
		for (i = 9; i >= 0 && buffer2[i] == ' '; --i)
		{
			buffer2[i] = '\0';
		}

		dstate_setinfo("device.model", "%s", buffer2);

		/* Serial number */
		dstate_setinfo("device.serial", "%.4s-%.4s", buffer+18, buffer+22);
		printf("Found %.10s UPS with serial number %.4s-%.4s\n",
												buffer2, buffer+18, buffer+22);

		/* Manufacture Date */
		dstate_setinfo("ups.mfr.date", "%.6s (yymmdd)", buffer+38);

		/* Battery Replace Date */
		dstate_setinfo("battery.date", "%.6s (yymmdd)", buffer+44);
		dstate_setflags("battery.date", ST_FLAG_STRING | ST_FLAG_RW);
		dstate_setaux("battery.date", 6);

		/* Real power nominal */
		EliminateLeadingZeroes (buffer+55, 5, buffer2, sizeof(buffer2));
		dstate_setinfo("ups.realpower.nominal", "%s", buffer2);

		/* Set up ups.start.auto to be writable */
		dstate_setinfo("ups.start.auto", "yes");
		dstate_setflags("ups.start.auto", ST_FLAG_STRING | ST_FLAG_RW);
		dstate_setaux("ups.start.auto", 3);

		/* Get output window min/max points from OB or OZ v1.9 or later */
		if ((strncmp (UpsFamily, FAMILY_OB, FAMILY_SIZE) == 0) ||
			(strcmp (dstate_getinfo("ups.firmware"), MIN_ALLOW_FW) >= 0 ))
		{
			upsdebugx (2,"Can get output window min/max! (%s)",
												dstate_getinfo("ups.firmware"));

			ser_send(upsfd,"%s%s",GETX_ALLOW_RANGE,COMMAND_END);
			if(OneacGetResponse (buffer, sizeof(buffer), GETX_RANGE_RESP_SIZE))
			{
				fatalx(EXIT_FAILURE,
						"Serial timeout(4) with ONEAC UPS on %s\n",device_path);
			}

			strncpy(buffer2, buffer, 3);
			buffer2[3]='\0';
			i = atoi(buffer2);		/* Minimum voltage */

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_STRINGOP_TRUNCATION
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
			strncpy(buffer2, buffer + 4, 3);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic pop
#endif
			j = atoi(buffer2);		/* Maximum voltage */

			strncpy(buffer2, buffer+8, 2);
			buffer2[2]='\0';
			k = atoi(buffer2);		/* Spread between */

			dstate_setinfo("input.transfer.low.min", "%3d", i);
			dstate_setinfo("input.transfer.low.max", "%3d", j-k);
			dstate_setinfo("input.transfer.high.min", "%3d", i+k);
			dstate_setinfo("input.transfer.high.max", "%3d", j);

		}
		else
		{
			/* Use default values from firmware */
			upsdebugx (2,"Using trip defaults (%s)...",
												dstate_getinfo("ups.firmware"));

			switch (VRange)				/* Held from initial use */
			{
				case V120AC:
					dstate_setinfo("input.transfer.low.min", "90");
					dstate_setinfo("input.transfer.low.max", "120");
					dstate_setinfo("input.transfer.high.min", "110");
					dstate_setinfo("input.transfer.high.max", "140");
					break;

				case V230AC:
					dstate_setinfo("input.transfer.low.min", "172");
					dstate_setinfo("input.transfer.low.max", "228");
					dstate_setinfo("input.transfer.high.min", "212");
					dstate_setinfo("input.transfer.high.max", "268");
					break;

				default:
					;

			}
		}
	}
}

void upsdrv_updateinfo(void)
{
	static int CommTry = COMM_TRIES;		/* Comm loss counter */
	char buffer[256];	/* Main response buffer */
	char buffer2[32];	/* Conversion buffer */
	char s;
	ssize_t RetValue;
	int timevalue;

	/* Start with EG/ON information */
	ser_flush_in(upsfd,"",0);  /*just in case*/
	ser_send (upsfd,"%c%s", GET_ALL, COMMAND_END);

	if (strncmp(UpsFamily, FAMILY_EG, FAMILY_SIZE) == 0)
	{
		RetValue = OneacGetResponse (buffer,sizeof(buffer),GETALL_EG_RESP_SIZE);
	}
	else
	{
		RetValue = OneacGetResponse (buffer, sizeof(buffer), GETALL_RESP_SIZE);
	}

	if ((RetValue != 0) && (CommTry == 0))
	{
		ser_comm_fail("Oneac UPS Comm failure continues on port %s",
																device_path);
	}
	else if (RetValue != 0)
	{
		if (--CommTry == 0)
		{
			ser_comm_fail("Oneac UPS Comm failure on port %s",device_path);
			dstate_datastale();
		}
		upsdebugx(2,"Oneac: Update serial comm retry value: %d", CommTry);

		return;
	}
	else
	{
		CommTry = COMM_TRIES;			/* Reset serial retries */

		s = buffer[12];

		status_init();
		alarm_init();

		/*take care of the UPS status information*/
		if (s == '@')
		{
			status_set("OL");
		}
		else
		{
			if (s & 0x01)			/* On Battery */
			{
				status_set("OB");
			}
			else
			{
				status_set("OL");
			}

			if (s & 0x02)			/* Low Battery */
				status_set("LB");

			if (s & 0x04)			/* General fault */
			{
				dstate_setinfo("ups.test.result","UPS Internal Failure");
			}
			else
			{
				dstate_setinfo("ups.test.result","Normal");
			}

			if (s & 0x08)			/* Replace Battery */
				status_set("RB");

/*			if (s & 0x10)	*/		/* High Line */

			if (s & 0x20)			/* Unit is hot */
				alarm_set("OVERHEAT");
		}

		/*take care of the reason why the UPS last transferred to battery*/
		switch (buffer[13]) {
			case XFER_BLACKOUT :
				dstate_setinfo("input.transfer.reason",	"Blackout");
				break;
			case XFER_LOW_VOLT :
				dstate_setinfo("input.transfer.reason",
					"Low Input Voltage");
				break;
			case XFER_HI_VOLT :
				dstate_setinfo("input.transfer.reason",
					"High Input Voltage");
				break;
			case NO_VALUE_YET :
				dstate_setinfo("input.transfer.reason",
					"No transfer yet.");
				break;
			default :
				upslogx(LOG_INFO,"Oneac: Unknown reason for UPS battery"
										" transfer [%c]", buffer[13]);
		}

		/* now update info for only the non-EG families of UPS*/

		if (strncmp(UpsFamily, FAMILY_EG, FAMILY_SIZE) != 0)
		{
			dstate_setinfo("ups.load", "0%.2s",buffer+31);

			/* Output ON or OFF? */
			if(buffer[27] == NO_VALUE_YET)
				status_set("OFF");

			/*battery charge*/
			if(buffer[10] == YES)
				dstate_setinfo("battery.charge", "0%.2s",buffer+33);
			else
				dstate_setinfo("battery.charge", "100");

			EliminateLeadingZeroes (buffer+35, 3, buffer2, sizeof(buffer2));
			dstate_setinfo("input.voltage", "%s",buffer2);

			EliminateLeadingZeroes (buffer+38, 3, buffer2, sizeof(buffer2));
			dstate_setinfo("input.voltage.minimum", "%s",buffer2);

			EliminateLeadingZeroes (buffer+41, 3, buffer2, sizeof(buffer2));
			dstate_setinfo("input.voltage.maximum", "%s",buffer2);

			EliminateLeadingZeroes (buffer+44, 3, buffer2, sizeof(buffer2));
			dstate_setinfo("output.voltage", "%s",buffer2);

			if (buffer[15] == NO_VALUE_YET)
			{
				dstate_delinfo("ups.timer.shutdown");
			}
			else
			{
				/* A shutdown is underway! */
				status_set("FSD");

				if(buffer[15] != HIGH_COUNT)
				{
					EliminateLeadingZeroes (buffer+15, 3, buffer2,
															sizeof(buffer2));
					dstate_setinfo("ups.timer.shutdown", "%s", buffer2);
				}
				else
				{
					dstate_setinfo("ups.timer.shutdown", "999");
				}
			}

			if (buffer[47] == YES)
				status_set("BOOST");
		}

		/* Now update info for only the OZ/OB families of UPS */

		if ((strncmp(UpsFamily, FAMILY_OZ, FAMILY_SIZE) == 0) ||
			(strncmp(UpsFamily, FAMILY_OB, FAMILY_SIZE) == 0))
		{
			ser_flush_in(upsfd,"",0);  /*just in case*/
			ser_send (upsfd,"%c%s",GETX_ALL_1,COMMAND_END);
			RetValue = OneacGetResponse (buffer, sizeof(buffer),
														GETX_ALL1_RESP_SIZE);

			if(RetValue)
			{
				if (--CommTry == 0)
				{
					ser_comm_fail("Oneac (OZ) UPS Comm failure on port %s",
																device_path);
					dstate_datastale();
				}

				upsdebugx(2,"Oneac: "
					"Update (OZ) serial comm retry value: %d", CommTry);
			}
			else
			{
				CommTry = COMM_TRIES;		/* Reset count */

				EliminateLeadingZeroes (buffer+57, 5, buffer2, sizeof(buffer2));
				dstate_setinfo("ups.realpower", "%s",buffer2);

				dstate_setinfo("input.frequency", "%.2s.%c",
														buffer+42,buffer[44]);
				dstate_setinfo("output.frequency", "%.2s.%c",
														buffer+76, buffer[78]);

				EliminateLeadingZeroes (buffer+29, 3, buffer2, sizeof(buffer2));
				dstate_setinfo("battery.voltage", "%s.%c",buffer2, buffer[32]);

				dstate_setinfo("ups.temperature", "%.2s",buffer+13);
				dstate_setinfo("ups.load", "%.3s",buffer+73);

				strncpy(buffer2, buffer+19, 4);
				buffer2[4]='\0';
				timevalue = atoi(buffer2) * 60;		/* Change mins to secs */
				dstate_setinfo("battery.runtime", "%d",timevalue);

				/* Now some individual requests... */

				/* Battery replace date */
				ser_send (upsfd,"%c%s",GETX_BATT_REPLACED,COMMAND_END);
				if(!OneacGetResponse (buffer, sizeof(buffer),
														GETX_DATE_RESP_SIZE))
					dstate_setinfo("battery.date", "%.6s (yymmdd)", buffer);

				/* Low and high output trip points */
				ser_send (upsfd,"%c%s",GETX_LOW_OUT_ALLOW,COMMAND_END);
				if(!OneacGetResponse (buffer, sizeof(buffer),
														GETX_ALLOW_RESP_SIZE))
				{
					EliminateLeadingZeroes (buffer, 3, buffer2,sizeof(buffer2));
					dstate_setinfo("input.transfer.low", "%s", buffer2);
				}

				ser_send (upsfd,"%c%s",GETX_HI_OUT_ALLOW,COMMAND_END);
				if(!OneacGetResponse (buffer, sizeof(buffer),
														GETX_ALLOW_RESP_SIZE))
					dstate_setinfo("input.transfer.high", "%s", buffer);

				/* Restart delay */
				ser_send (upsfd,"%c%s",GETX_RESTART_DLY,COMMAND_END);
				if(!OneacGetResponse (buffer, sizeof(buffer),
														GETX_RSTRT_RESP_SIZE))
				{
					EliminateLeadingZeroes (buffer, 4, buffer2,
															sizeof(buffer2));
					dstate_setinfo("ups.delay.start", "%s", buffer2);
				}

				/* Buzzer state */
				ser_send (upsfd,"%s%s",GETX_BUZZER_WHAT,COMMAND_END);
				if(!OneacGetResponse (buffer, sizeof(buffer), 1))
				{
					switch (buffer[0])
					{
						case BUZZER_ENABLED :
							dstate_setinfo("ups.beeper.status",	"enabled");
							break;
						case BUZZER_DISABLED :
							dstate_setinfo("ups.beeper.status",	"disabled");
							break;
						case BUZZER_MUTED :
							dstate_setinfo("ups.beeper.status",	"muted");
							break;
						default :
							dstate_setinfo("ups.beeper.status",	"enabled");
					}
				}

				/* Auto start setting */
				ser_send (upsfd,"%s%s",GETX_AUTO_START,COMMAND_END);
				if(!OneacGetResponse (buffer, sizeof(buffer), 1))
				{
					if (buffer[0] == '0')
						dstate_setinfo("ups.start.auto", "yes");
					else
						dstate_setinfo("ups.start.auto", "no");
				}

				/* Low Batt at time */
				ser_send (upsfd,"%c%s",GETX_LOW_BATT_TIME,COMMAND_END);
				if(!OneacGetResponse (buffer, sizeof(buffer), 2))
				{
					strncpy(buffer2, buffer, 2);
					buffer2[2]='\0';
					timevalue = atoi(buffer2) * 60;		/* Mins to secs */
					dstate_setinfo("battery.runtime.low", "%d",timevalue);
				}

				/* Shutdown timer */
				ser_send (upsfd,"%c%s",GETX_SHUTDOWN,COMMAND_END);
				if(!OneacGetResponse (buffer, sizeof(buffer),
													GETX_SHUTDOWN_RESP_SIZE))
				{
					/* ON would have handled NO_VALUE_YET and setting FSD
					 *  above so only deal with counter value here.
					 */
					if (buffer[0] != NO_VALUE_YET)
					{
						EliminateLeadingZeroes (buffer, 5, buffer2,
															sizeof(buffer2));
						dstate_setinfo("ups.timer.shutdown", "%s", buffer2);
					}
				}

				/* Restart timer */
				ser_send (upsfd,"%s%s",GETX_RESTART_COUNT,COMMAND_END);
				if(!OneacGetResponse (buffer, sizeof(buffer),
														GETX_RSTRT_RESP_SIZE))
				{
					if (atoi(buffer) == 0)
					{
						dstate_delinfo("ups.timer.start");
					}
					else
					{
						EliminateLeadingZeroes (buffer, 4, buffer2,
															sizeof(buffer2));
						dstate_setinfo("ups.timer.start", "%s", buffer2);
					}
				}
			}
		}

		alarm_commit();
		status_commit();

		/* If the comm retry counter is zero then datastale has been set.
		 *  We don't want to set dataok or ser_comm_good if that is the case.
		 */

		if (CommTry != 0)
		{
			dstate_dataok();
			ser_comm_good();
		}
	}
}

void upsdrv_shutdown(void)
{
	ser_send(upsfd,"%s",SHUTDOWN);
}

void upsdrv_help(void)
{
	printf("\n---------\nNOTE:\n");
	printf("You must set the UPS interface card DIP switch to 9600 BPS\n");
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE,"testtime",
		"Change battery test time from the 2 minute default.");

	addvar(VAR_VALUE,"offdelay",
		"Change shutdown delay time from 0 second default.");
}

int instcmd(const char *cmdname, const char *extra)
{
	int i;

	upsdebugx(2, "In instcmd with %s and extra %s.", cmdname, extra);

	if (!strcasecmp(cmdname, "test.failure.start")) {
		ser_send(upsfd,"%s%s",SIM_PWR_FAIL,COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.return")) {

		i = atoi(dstate_getinfo("ups.delay.shutdown"));

		if ((strncmp (UpsFamily, FAMILY_OZ, FAMILY_SIZE) == 0) ||
			(strncmp (UpsFamily, FAMILY_OB, FAMILY_SIZE) == 0))
		{
			upsdebugx(3, "Shutdown using %c%d...", DELAYED_SHUTDOWN_PREFIX, i);
			ser_send(upsfd,"%c%d%s",DELAYED_SHUTDOWN_PREFIX, i, COMMAND_END);
		}
		else
		{
			upsdebugx(3, "Shutdown using %c%03d...",DELAYED_SHUTDOWN_PREFIX, i);
			ser_send(upsfd,"%c%03d%s",DELAYED_SHUTDOWN_PREFIX, i, COMMAND_END);
		}

		return STAT_INSTCMD_HANDLED;
	}

	if(!strcasecmp(cmdname, "shutdown.reboot")) {
		ser_send(upsfd, "%s", SHUTDOWN);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stop")) {
		ser_send(upsfd,"%c%s",DELAYED_SHUTDOWN_PREFIX,COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start.quick")) {
		do_battery_test();
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start.deep")) {
		ser_send(upsfd, "%s%s", TEST_BATT_DEEP, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.stop"))
	{
		if ((strncmp (UpsFamily, FAMILY_EG, FAMILY_SIZE) == 0) ||
			(strncmp (UpsFamily, FAMILY_ON, FAMILY_SIZE) == 0))
		{
			ser_send(upsfd,"%s00%s",BAT_TEST_PREFIX,COMMAND_END);
		}
		else
		{
			ser_send(upsfd,"%c%s",TEST_ABORT,COMMAND_END);
		}
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "reset.input.minmax")) {
		ser_send(upsfd,"%c%s",RESET_MIN_MAX, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "beeper.enable")) {
		ser_send(upsfd,"%c%c%s",SETX_BUZZER_PREFIX, BUZZER_ENABLED,COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "beeper.disable")) {
		ser_send(upsfd,"%c%c%s",SETX_BUZZER_PREFIX,BUZZER_DISABLED,COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "beeper.mute")) {
		ser_send(upsfd,"%c%c%s",SETX_BUZZER_PREFIX, BUZZER_MUTED, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.panel.start")) {
		ser_send(upsfd,"%s%s",TEST_INDICATORS, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}


int setcmd(const char* varname, const char* setvalue)
{
	upsdebugx(2, "In setcmd for %s with %s...", varname, setvalue);

	if (!strcasecmp(varname, "ups.delay.shutdown"))
	{
		if ((strncmp (UpsFamily, FAMILY_OZ, FAMILY_SIZE) == 0) ||
			(strncmp (UpsFamily, FAMILY_OB, FAMILY_SIZE) == 0))
		{
			if (atoi(setvalue) > 65535)
			{
				upsdebugx(2, "Too big for OZ/OB (>65535)...(%s)", setvalue);
				return STAT_SET_UNKNOWN;
			}
		}
		else
		{
			if (atoi(setvalue) > 999)
			{
				upsdebugx(2, "Too big for EG/ON (>999)...(%s)", setvalue);
				return STAT_SET_UNKNOWN;
			}
		}

		dstate_setinfo("ups.delay.shutdown", "%s", setvalue);
		return STAT_SET_HANDLED;
	}

	if (!strcasecmp(varname, "input.transfer.low"))
	{
		if (SetOutputAllow(setvalue, dstate_getinfo("input.transfer.high")))
		{
			return STAT_SET_UNKNOWN;
		}
		else
		{
			dstate_setinfo("input.transfer.low" , "%s", setvalue);
			return STAT_SET_HANDLED;
		}
	}

	if (!strcasecmp(varname, "input.transfer.high"))
	{
		if (SetOutputAllow(dstate_getinfo("input.transfer.low"), setvalue))
		{
			return STAT_SET_UNKNOWN;
		}
		else
		{
			dstate_setinfo("input.transfer.high" , "%s", setvalue);
			return STAT_SET_HANDLED;
		}
	}

	if (!strcasecmp(varname, "battery.date"))
	{
		if(strlen(setvalue) == GETX_DATE_RESP_SIZE)		/* yymmdd (6 chars) */
		{
			ser_send(upsfd, "%s%s%s", SETX_BATTERY_DATE, setvalue, COMMAND_END);
			dstate_setinfo("battery.date", "%s (yymmdd)", setvalue);
			return STAT_SET_HANDLED;
		}
		else
		{
			return STAT_SET_UNKNOWN;
		}
	}

	if (!strcasecmp(varname, "ups.delay.start"))
	{
		if (atoi(setvalue) <= 9999)
		{
			ser_send(upsfd,"%s%s%s",SETX_RESTART_DELAY, setvalue, COMMAND_END);

			dstate_setinfo("ups.delay.start", "%s", setvalue);
			return STAT_SET_HANDLED;
		}
		else
		{
			return STAT_SET_UNKNOWN;
		}
	}

	if (!strcasecmp(varname, "battery.runtime.low"))
	{
		if (atoi(setvalue) <= 99)
		{
			ser_send(upsfd,"%s%s%s",SETX_LOWBATT_AT, setvalue, COMMAND_END);

			dstate_setinfo("battery.runtime.low", "%s", setvalue);
			return STAT_SET_HANDLED;
		}
		else
		{
			return STAT_SET_UNKNOWN;
		}
	}

	if (!strcasecmp(varname, "ups.start.auto"))
	{
		if (!strncasecmp(setvalue, "yes", 3))
		{
			ser_send(upsfd,"%c0%s",SETX_AUTO_START, COMMAND_END);
			dstate_setinfo("ups.start.auto", "yes");
			return STAT_SET_HANDLED;
		}
		else if (!strncasecmp(setvalue, "no", 2))
		{
			ser_send(upsfd,"%c1%s",SETX_AUTO_START, COMMAND_END);
			dstate_setinfo("ups.start.auto", "no");
			return STAT_SET_HANDLED;
		}

		return STAT_SET_UNKNOWN;
	}

	upslogx(LOG_NOTICE, "setcmd: unknown command [%s]", varname);

	return STAT_SET_UNKNOWN;
}
