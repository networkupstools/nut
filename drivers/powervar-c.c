/*vim ts=4*/
/* powervar-c.c - Driver for Powervar UPM UPS using CUSPP.
 *
 * Supported Powervar UPS families in this driver:
 * UPM (All)
 *
 * Copyright (C)
 *     2024 by Bill Elliot <bill@wreassoc.com>
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
 * - 2 October 2024.  Bill Elliot
 * Used pieces of oneac.c driver to get jump-started.
 *
 */

#include "main.h"
#include "serial.h"
#include "powervar-c.h"
#include "nut_stdint.h"

/* Prototypes to allow setting pointer before function is defined */
int setcmd(const char* varname, const char* setvalue);
int instcmd(const char *cmdname, const char *extra);

#define DRIVER_NAME	"Powervar-C UPS driver"
#define DRIVER_VERSION	"0.01"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Bill Elliot <bill@wreassoc.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define SECS 0			/* Serial function wait time*/
#define USEC 500000		/* Rest of serial function wait time*/

#define COMM_TRIES	3	/* Serial retries before "stale" */

static char UpsFamily [SUBBUFFSIZE];		/* Hold family that was found */
static char UpsProtVersion [SUBBUFFSIZE];	/* Hold protocol version string */

/* Configurable CUSPP response information positions */
static uint8_t byPIDProtPos = 0;
static uint8_t byPIDVerPos = 0;

static uint8_t byUIDManufPos = 0;
static uint8_t byUIDModelPos = 0;
static uint8_t byUIDSwverPos = 0;
static uint8_t byUIDSernumPos = 0;
static uint8_t byUIDFamilyPos = 0;
static uint8_t byUIDMfgdtPos = 0;
static uint8_t byUIDCSWVERPos = 0;

static uint8_t byBATStatusPos = 0;
static uint8_t byBATTmleftPos = 0;
static uint8_t byBATEstcrgPos = 0;
static uint8_t byBATVoltPos = 0;
static uint8_t byBATTempPos = 0;

static uint8_t byINPStatusPos = 0;
static uint8_t byINPFreqPos = 0;
static uint8_t byINPVoltPos = 0;
static uint8_t byINPAmpPos = 0;
static uint8_t byINPMaxvltPos = 0;
static uint8_t byINPMinvltPos = 0;

static uint8_t byOUTSourcePos = 0;
static uint8_t byOUTFreqPos = 0;
static uint8_t byOUTVoltPos = 0;
static uint8_t byOUTAmpPos = 0;
static uint8_t byOUTPercntPos = 0;

static uint8_t bySYSInvoltPos = 0;
static uint8_t bySYSInfrqPos = 0;
static uint8_t bySYSOutvltPos = 0;
static uint8_t bySYSOutfrqPos = 0;
static uint8_t bySYSBatdtePos = 0;
static uint8_t bySYSOvrlodPos = 0;
static uint8_t bySYSOutvaPos = 0;

static uint8_t bySETAudiblPos = 0;
static uint8_t bySETAtosrtPos = 0;

static uint8_t byALMOnbatPos = 0;
static uint8_t byALMLowbatPos = 0;
static uint8_t byALMBadbatPos = 0;
static uint8_t byALMTempPos = 0;
static uint8_t byALMOvrlodPos = 0;


/****************************************************************
 * Below are functions used only in this Powervar driver        *
 ***************************************************************/

/* Since an installed network card may slightly delay responses from
 *  the UPS allow for a repeat of the get request.
 * Leave ExpectedCount in parameters while allowing comms with EG/OB/ON UPS.
 *  It is not needed for CUSPP protocol.
 */
/* TBD, Remove ExpectedCount parameter when all EG/OB/ON stuff is removed?? */
static ssize_t PowervarGetResponse (char* chBuff, const size_t BuffSize, int ExpectedCount)
{
	int Retries = 10;		/* x/2 seconds max with 500000 USEC */
	ssize_t return_val;

	do
	{
		return_val = ser_get_line(upsfd, chBuff, BuffSize, ENDCHAR, IGNCHARS, SECS, USEC);

		if (((ExpectedCount == 0) && (return_val > 0)) || (return_val == ExpectedCount))
		{
			break;
		}

		upsdebugx (3, "!PowervarGetResponse retry (%" PRIiSIZE ", %d)...", return_val, Retries);

	} while (--Retries > 0);

	upsdebugx (4,"PowervarGetResponse buffer: %s",chBuff);

	if (Retries == 0)
	{
		upsdebugx (2,"!!PowervarGetResponse timeout...");
		return_val = 1;					/* Comms error */
	}
	else
	{
		if (Retries < 10)
		{
			upsdebugx (2,"PowervarGetResponse recovered (%d)...", Retries);
		}

		return_val = 0;					/* Good comms */
	}

	return return_val;
}


/* Get the FORMAT and/or data for the requested CUSPP group.
   This is used during initialization to establish the substring position values
    and to set the initial NUT data.
   [TBD, Since it is our driver, just use BUFFSIZE for length...don't pass sizes?]
*/
static void GetInitFormatAndData (const char* sReq, char* sF, const size_t sFSize, char* sD, const size_t sDSize)
{
	if (sF)
	{
		/* Get sReq format response */
		upsdebugx (4, "Requesting %s.FORMAT", sReq);

		ser_send(upsfd, "%s%s%c", sReq, FORMAT_TAIL, ENDCHAR);

		if(PowervarGetResponse (sF, sFSize, 0))
		{
			fatalx(EXIT_FAILURE, "%s.FORMAT Serial timeout getting UPS data on %s\n", sReq, device_path);
		}
	}
	else
	{
		upsdebugx (4, "Bypassed requesting %s.FORMAT", sReq);
	}

	if (sD)
	{
		/* Get sReq data */
		upsdebugx (4, "Requesting %s data", sReq);

		ser_send(upsfd,"%s%c", sReq, ENDCHAR);

		if(PowervarGetResponse (sD, sDSize, 0))
		{
			fatalx(EXIT_FAILURE, "%s Serial timeout getting UPS data on %s\n", sReq, device_path);
		}
	}
	else
	{
		upsdebugx (4, "Bypassed requesting %s data", sReq);
	}
}


/* This function parses responses to pull the desired substring from the buffer.
 * SubPosition is normal counting (start with 1 not 0).
 * chDst will have just the substring requested or will be set to NULL if
 *  substring is not found.
 */

static void GetSubstringFromBuffer (char* chDst, const char* chSrc, const uint SubPosition)
{
char WorkBuffer [BUFFSIZE];
char* chWork;
char* chTok;
uint Pos;

	if (SubPosition)	/* Don't accept a '0' request */
	{
		/* Make a local copy of the source string so strtok doesn't corrupt original. */
		/*  [TBD, -OR- pass chSrc buffer by value??] */
		strcpy (WorkBuffer, chSrc);

		/* Get to '=' of request response and then point at next character... */
		chWork = strchr (WorkBuffer, CHAR_EQ);
		chWork++;

		Pos = SubPosition - 1;	/* Zero-reference the count */

		/* Find the substring...if it exists. */
		/* Get first substring... */
		chTok = strtok (chWork, DATADELIM);

		if (chTok == NULL)
		{
			upsdebugx (4,"First Token == 0!");
		}

		while ((chTok != NULL) && (Pos > 0))
		{
			chTok = strtok (0, DATADELIM);
			Pos--;
		}

		if (chTok != 0)
		{
			/* Copy substring to destination buffer */
			strcpy(chDst, chTok);

			upsdebugx (3,"Substring %d returning: \"%s\".", SubPosition, chDst);
		}
		else
		{
			chDst[0] = 0;	/* Make sure Substring is null. */
			upsdebugx (4,"Substring not found!");
		}
	}
	else
	{
		chDst[0] = 0;		/* Make sure Substring is null. */
		upsdebugx (4,"Position parameter was zero.");
	}
}


/* This function finds the position of a substring in a CUSPP Format response. */

static uint GetSubstringPosition (const char* chResponse, const char* chSub)
{
uint uiReturn = 0;
uint uiPos = 1;
uint uiLen = 0;
char WorkBuffer [BUFFSIZE];
char* chSrc;
char* chTok;

	/* Make a local copy of the source string so strtok doesn't corrupt original. */
	/*  [TBD, -OR- pass chResponse buffer by value??] */
	strcpy (WorkBuffer, chResponse);

	/* Find the '=' in the response string and get past it */
	chSrc = strchr (WorkBuffer, CHAR_EQ);
	chSrc++;

	/* Get the first token */
	chTok = strtok (chSrc, FRMTDELIM);

	/* Get rest of tokens until match is found... */
	while (chTok != 0)
	{
		uiLen = strlen (chTok);

		if (uiLen > 2)	/* Less than three characters is not valid. */
		{
			if (strcmp (chTok, chSub) == 0)
			{
				uiReturn = uiPos;
				upsdebugx (3,"Found substring \"%s\" at position: %d.", chSub, uiReturn);
				break;
			}

			uiPos++;
		}

		chTok = strtok (0, FRMTDELIM);
	}

	if (uiReturn == 0)
	{
		upsdebugx (4,"Substring was not found!");
	}

	return uiReturn;
}


static void do_battery_test(void)
{
	char buffer[32];

	if (getval("testtime") == NULL)
	{
		snprintf(buffer, 3, "%s", DEFAULT_BAT_TEST_TIME);
	}
	else
	{
		snprintf(buffer, 3, "%s", getval("testtime"));

	/*the UPS wants this value to always be two characters long*/
	/*so put a zero in front of the string, if needed....      */
		if (strlen(buffer) < 2) {
			buffer[2] = '\0';
			buffer[1] = buffer[0];
			buffer[0] = '0';
		}
	}
	ser_send(upsfd, "%s%s%s", BAT_TEST_PREFIX, buffer, COMMAND_END);
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

	upsdebugx (2,"SetOutputAllow sending %s%.3s,%.3s...", SETX_OUT_ALLOW, buffer, highval);

	ser_send(upsfd, "%s%.3s,%.3s%s", SETX_OUT_ALLOW, buffer, highval, COMMAND_END);
	ser_get_line(upsfd,buffer, sizeof(buffer), ENDCHAR, IGNCHARS, SECS,USEC);

	if(buffer[0] == DONT_UNDERSTAND)
	{
		upsdebugx (2,"SetOutputAllow got asterisk back...");

		return 1;					/* Invalid command */
	}

	return 0;						/* Valid command */
}

static void EliminateLeadingZeroes (const char* buff1, int StringSize, char* buff2, const size_t buff2size)
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
 * Below are the commands that are called by main               *
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
	int i, j, k;
	int VRange = 0;
	int Vlts = 1;
	ssize_t RetValue;
	char sFBuff[BUFFSIZE];
	char sDBuff[BUFFSIZE];
	char buffer2[SUBBUFFSIZE];
	char SubBuff[SUBBUFFSIZE];

	/* Get serial port ready */
	ser_flush_in(upsfd,"",0);

	/* First, try to get UPM PID.FORMAT response. All UPSs with CUSPP should reply
	 *  to this request so we can confirm that it is a Powervar UPS.
	 */

	GetInitFormatAndData (PID_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

	/* Check for standard CUSPP PROT request, exit if not there. */
	byPIDProtPos = GetSubstringPosition (sFBuff, PID_PROT_SUB);

	if(byPIDProtPos == 0)
	{
		fatalx(EXIT_FAILURE, "[%s] Not a UPS that handles CUSPP\n", PID_PROT_SUB);
	}

	byPIDVerPos = GetSubstringPosition (sFBuff, PID_VER_SUB);

	/* Check for 'CUSPP' */
	GetSubstringFromBuffer (SubBuff, sDBuff, byPIDProtPos);

	if(strcmp(SubBuff, PID_PROT_DATA) != 0)
	{
		fatalx(EXIT_FAILURE, "[%s] Not a UPS that handles CUSPP\n", PID_PROT_DATA);
	}

	/* Keep CUSPP version string ... for now. */
	GetSubstringFromBuffer (SubBuff, sDBuff, byPIDVerPos);
	strcpy (UpsProtVersion, SubBuff);

	/* TBD, This assumes that UID exists. Check first or is it known to always be there? */
	/* Get UID format and data then populate needed data string positions... */
	GetInitFormatAndData(UID_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

	byUIDManufPos = GetSubstringPosition (sFBuff, UID_MANUF_SUB);
	byUIDModelPos = GetSubstringPosition (sFBuff, UID_MODEL_SUB);
	byUIDSwverPos = GetSubstringPosition (sFBuff, UID_SWVER_SUB);
	byUIDSernumPos = GetSubstringPosition (sFBuff, UID_SERNUM_SUB);
	byUIDFamilyPos = GetSubstringPosition (sFBuff, UID_FAMILY_SUB);
	byUIDMfgdtPos = GetSubstringPosition (sFBuff, UID_MFGDT_SUB);
	byUIDCSWVERPos = GetSubstringPosition (sFBuff, UID_CSWVER_SUB);

	/* Finally begin to populate NUT data */
	/* Get FAMILY substring and keep for any later messages */
	if (byUIDFamilyPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, byUIDFamilyPos);
		strcpy(UpsFamily, SubBuff);
		dstate_setinfo("device.description", "%s", SubBuff);
	}

	if (byUIDModelPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, byUIDModelPos);
		dstate_setinfo("device.model", "%s",SubBuff);
	}

	dstate_setinfo("device.type", "ups");
	dstate_setinfo("ups.type", "%s", "Line Interactive");

	dstate_addcmd("reset.input.minmax");		/* TBD, UPM only!! */
	dstate_addcmd("test.battery.start.quick");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("test.failure.start");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("shutdown.reboot");
	dstate_addcmd("test.panel.start");
	dstate_addcmd("test.battery.start.deep");
	dstate_addcmd("beeper.enable");
	dstate_addcmd("beeper.disable");
	dstate_addcmd("beeper.mute");

	upsh.setvar = setcmd;
	upsh.instcmd = instcmd;

	/* Manufacturer */
	if (byUIDManufPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, byUIDManufPos);
		dstate_setinfo("device.mfr", "%s", SubBuff);
	}

	/* Manufacture date */
	if (byUIDMfgdtPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, byUIDMfgdtPos);
		dstate_setinfo("ups.mfr.date", "%s (yyyymmdd)", SubBuff);
	}
	/* TBD, Pull yyq from GTS serial number...if family is tracked. */
	/* dstate_setinfo("ups.mfr.date", "%.3s (yyq)", SubBuff+<offset>);	*/

	/* Firmware revision(s) */
	if (byUIDSwverPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, byUIDSwverPos);
		dstate_setinfo("ups.firmware", "%s", SubBuff);
	}

	if (byUIDCSWVERPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, byUIDCSWVERPos);
		dstate_setinfo("ups.firmware.aux", "%s", SubBuff);
	}

	/* Serial number */
	if (byUIDSernumPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, byUIDSernumPos);
		dstate_setinfo("device.serial", "%s", SubBuff);
	}

	/* TBD, Put this in the log?? */
	printf ("Found a %s UPS with serial number %s\n", UpsFamily, SubBuff);


	/* Get BAT format and populate needed data string positions... */
	GetInitFormatAndData(BAT_REQ, sFBuff, sizeof(sFBuff), 0, 0);

	byBATStatusPos = GetSubstringPosition (sFBuff, X_STATUS_SUB);
	byBATTmleftPos = GetSubstringPosition (sFBuff, BAT_TMLEFT_SUB);
	byBATEstcrgPos = GetSubstringPosition (sFBuff, BAT_ESTCRG_SUB);
	byBATVoltPos = GetSubstringPosition (sFBuff, X_VOLT_SUB);
	byBATTempPos = GetSubstringPosition (sFBuff, X_TEMP_SUB);

/* MOVE THIS GROUP TO UPDATE
	if (byBATTmleftPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, byBATTmleftPos);
		timevalue = atoi(SubBuff) * 60;		// Change minutes to seconds
		dstate_setinfo("battery.runtime", "%d", timevalue);
	}

	if (byBATEstcrgPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, byBATEstcrgPos);
		dstate_setinfo("battery.charge", "%s", SubBuff);
	}

	if (byBATVoltPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, byBATVoltPos);
		dstate_setinfo("battery.voltage", "%s", SubBuff);
	}

	if (byBATTempPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, byBATTempPos);
		dstate_setinfo("ups.temperature", "%s", SubBuff);
		dstate_setinfo("battery.temperature", "%s", SubBuff);
	}
 */
	/* Get INP format and populate needed data string positions... */
	GetInitFormatAndData(INP_FMT_REQ, sFBuff, sizeof(sFBuff), 0, 0);

	byINPStatusPos = GetSubstringPosition (sFBuff, X_STATUS_SUB);
	byINPFreqPos = GetSubstringPosition (sFBuff, X_FREQ_SUB);
	byINPVoltPos = GetSubstringPosition (sFBuff, X_VOLT_SUB);
	byINPAmpPos = GetSubstringPosition (sFBuff, X_AMP_SUB);
	byINPMaxvltPos = GetSubstringPosition (sFBuff, INP_MAXVLT_SUB);
	byINPMinvltPos = GetSubstringPosition (sFBuff, INP_MINVLT_SUB);


	/* Get OUT format and populate needed data string positions... */
	GetInitFormatAndData(OUT_REQ, sFBuff, sizeof(sFBuff), 0, 0);

	byOUTSourcePos = GetSubstringPosition (sFBuff, OUT_SOURCE_SUB);
	byOUTFreqPos = GetSubstringPosition (sFBuff, X_FREQ_SUB);
	byOUTVoltPos = GetSubstringPosition (sFBuff, X_VOLT_SUB);
	byOUTAmpPos = GetSubstringPosition (sFBuff, X_AMP_SUB);
	byOUTPercntPos = GetSubstringPosition (sFBuff, OUT_PERCNT_SUB);


	/* Get SYS format and populate needed data string positions... */
	GetInitFormatAndData(SYS_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

	bySYSInvoltPos = GetSubstringPosition (sFBuff, SYS_INVOLT_SUB);
	bySYSInfrqPos = GetSubstringPosition (sFBuff, SYS_INFRQ_SUB);
	bySYSOutvltPos = GetSubstringPosition (sFBuff, SYS_OUTVLT_SUB);
	bySYSOutfrqPos = GetSubstringPosition (sFBuff, SYS_OUTFRQ_SUB);
	bySYSBatdtePos = GetSubstringPosition (sFBuff, SYS_BATDTE_SUB);
	bySYSOvrlodPos = GetSubstringPosition (sFBuff, X_OVRLOD_SUB);
	bySYSOutvaPos = GetSubstringPosition (sFBuff, SYS_OUTVA_SUB);

	if (bySYSInvoltPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, bySYSInvoltPos);
		dstate_setinfo("input.voltage.nominal", "%s", SubBuff);
	}

	if (bySYSInfrqPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, bySYSInfrqPos);
		dstate_setinfo("input.frequency.nominal", "%s", SubBuff);
	}

	if (bySYSOutvltPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, bySYSOutvltPos);
		dstate_setinfo("output.voltage.nominal", "%s", SubBuff);
		Vlts = atoi(SubBuff);		/* Keep for nominal current calc */
	}

	if (bySYSOutfrqPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, bySYSOutfrqPos);
		dstate_setinfo("output.frequency.nominal", "%s", SubBuff);
	}

	if (bySYSOutvaPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, bySYSOutvaPos);
		dstate_setinfo("output.current.nominal", "%d", atoi(SubBuff)/Vlts);
	}

	if (bySYSBatdtePos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, bySYSBatdtePos);
		dstate_setinfo("battery.date", "%s (yyyymm)", SubBuff);
		dstate_setflags("battery.date", ST_FLAG_STRING | ST_FLAG_RW);
		dstate_setaux("battery.date", 6);
	}

	if (bySYSOvrlodPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, bySYSOvrlodPos);
		dstate_setinfo("ups.load.high", "%s", SubBuff);
	}

	/* Get SET format and populate needed data string positions... */
	GetInitFormatAndData(SET_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

	bySETAudiblPos = GetSubstringPosition (sFBuff, SET_AUDIBL_SUB);
	bySETAtosrtPos = GetSubstringPosition (sFBuff, SET_ATOSRT_SUB);

	if (bySETAtosrtPos)
	{
		GetSubstringFromBuffer (SubBuff, sDBuff, bySETAtosrtPos);

		/* Set up ups.start.auto to be writable */
		if (SubBuff[0] == '1')
		{
			dstate_setinfo("ups.start.auto", "yes");
		}
		else
		{
			dstate_setinfo("ups.start.auto", "no");
		}
		dstate_setflags("ups.start.auto", ST_FLAG_STRING | ST_FLAG_RW);
		dstate_setaux("ups.start.auto", 3);
	}

	/* Get ALM format and populate needed data string positions... */
	GetInitFormatAndData(ALM_REQ, sFBuff, sizeof(sFBuff), 0, 0);

	byALMOnbatPos = GetSubstringPosition (sFBuff, ALM_ONBAT_SUB);
	byALMLowbatPos = GetSubstringPosition (sFBuff, ALM_LOWBAT_SUB);
	byALMBadbatPos = GetSubstringPosition (sFBuff, ALM_BADBAT_SUB);
	byALMTempPos = GetSubstringPosition (sFBuff, X_TEMP_SUB);
	byALMOvrlodPos = GetSubstringPosition (sFBuff, X_OVRLOD_SUB);

	/* Set status OB, LB, OL -- MOVE TO UPDATEINFO */
/* 	GetSubstringFromBuffer (SubBuff, sDBuff, byALMOnbatPos);

	if (SubBuff [0] == '0')
	{
		status_set("OL");	This should probably be driven by OUT.SOURCE
	}
	else
	{
		status_set("OB")
		GetSubstringFromBuffer (SubBuff, sDBuff, byALMLowbatPos);

		if (SubBuff [0] == '1')
		{
			status_set("LB");
		}
	}
 */


//++++++




	/* Original code here down (some removed with each step of development)*/
	ser_send(upsfd,"%c%s", GET_FAMILY,COMMAND_END);

	if(PowervarGetResponse (sDBuff, sizeof(sDBuff), 2))
	{
		fatalx(EXIT_FAILURE, "Serial timeout with ONEAC UPS on %s\n", device_path);
	}
	else
	{
		if (strncmp(sDBuff, FAMILY_ON, FAMILY_SIZE) != 0 &&
			strncmp(sDBuff, FAMILY_OZ, FAMILY_SIZE) != 0 &&
			strncmp(sDBuff, FAMILY_OB, FAMILY_SIZE) != 0 &&
			strncmp(sDBuff, FAMILY_EG, FAMILY_SIZE) != 0)
		{
			fatalx(EXIT_FAILURE, "Did not find an Oneac UPS on %s\n", device_path);
		}
	}

	/* UPS Model (either EG, ON, OZ or OB series of UPS) */
	strncpy(UpsFamily, sDBuff, FAMILY_SIZE);
	UpsFamily[2] = '\0';
//	dstate_setinfo("device.model", "%s",UpsFamily);			// Done
	printf("Found %s family of Oneac UPS\n", UpsFamily);

	/* set some stuff that shouldn't change after initialization */
	/* this stuff is common to all families of UPS */

	ser_send(upsfd,"%c%s", GET_ALL, COMMAND_END);

	if (strncmp(UpsFamily, FAMILY_EG, FAMILY_SIZE) == 0)
	{
		RetValue = PowervarGetResponse (sDBuff, sizeof(sDBuff), GETALL_EG_RESP_SIZE);
	}
	else
	{
		RetValue = PowervarGetResponse (sDBuff, sizeof(sDBuff), GETALL_RESP_SIZE);
	}

	if(RetValue)
	{
		fatalx(EXIT_FAILURE, "Serial timeout(2) with Powervar UPS on %s\n", device_path);
	}

/* TBD, CUSPP...how do we implement this?? */
	/* Shutdown delay in seconds...can be changed by user */
	if (getval("offdelay") == NULL)
	{
		dstate_setinfo("ups.delay.shutdown", "0");
	}
	else
	{
		dstate_setinfo("ups.delay.shutdown", "%s", getval("offdelay"));
	}

	dstate_setflags("ups.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW);
	dstate_setaux("ups.delay.shutdown", GET_SHUTDOWN_RESP_SIZE);

	/* Setup some ON/OZ/OB only stuff ... i.e. not EG */

	if (strncmp(UpsFamily, FAMILY_EG, FAMILY_SIZE) != 0)
	{
//		dstate_addcmd("reset.input.minmax");			// DONE

		/*nominal input voltage*/

		VRange = sDBuff[26];			/* Keep for later use also */

	}

	/* Setup some OZ/OB only stuff */

	if ((strncmp (UpsFamily, FAMILY_OZ, FAMILY_SIZE) == 0) ||
		(strncmp (UpsFamily, FAMILY_OB, FAMILY_SIZE) == 0))
	{
		dstate_setaux("ups.delay.shutdown", GETX_SHUTDOWN_RESP_SIZE);

		ser_flush_in(upsfd, "", 0);
		ser_send(upsfd, "%c%s", GETX_ALL_2, COMMAND_END);
		if(PowervarGetResponse (sDBuff, sizeof(sDBuff), GETX_ALL2_RESP_SIZE))
		{
			fatalx(EXIT_FAILURE, "Serial timeout(3) with Powervar UPS on %s\n", device_path);
		}
/* TBD, CUSPP?? */
		/* Low and high output trip points */
		EliminateLeadingZeroes (sDBuff+73, 3, buffer2, sizeof(buffer2));
		dstate_setinfo("input.transfer.low", "%s", buffer2);
		dstate_setflags("input.transfer.low", ST_FLAG_STRING | ST_FLAG_RW );
		dstate_setaux("input.transfer.low", 3);

/* TBD, CUSPP?? */
		EliminateLeadingZeroes (sDBuff+76, 3, buffer2, sizeof(buffer2));
		dstate_setinfo("input.transfer.high", "%s", buffer2);
		dstate_setflags("input.transfer.high", ST_FLAG_STRING | ST_FLAG_RW);
		dstate_setaux("input.transfer.high", 3);


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
		strncpy(buffer2, sDBuff + 5, 10);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic pop
#endif
		for (i = 9; i >= 0 && buffer2[i] == ' '; --i)
		{
			buffer2[i] = '\0';
		}


		/* Battery Replace Date */
//		dstate_setinfo("battery.date", "%.6s (yymmdd)", sDBuff+44);	// DONE
//		dstate_setflags("battery.date", ST_FLAG_STRING | ST_FLAG_RW);
//		dstate_setaux("battery.date", 6);

		/* Set up ups.start.auto to be writable */
//		dstate_setinfo("ups.start.auto", "yes");			// DONE
//		dstate_setflags("ups.start.auto", ST_FLAG_STRING | ST_FLAG_RW);
//		dstate_setaux("ups.start.auto", 3);

/* TBD, CUSPP?? */
		/* Get output window min/max points from OB or OZ v1.9 or later */
		if ((strncmp (UpsFamily, FAMILY_OB, FAMILY_SIZE) == 0) ||
			(strcmp (dstate_getinfo("ups.firmware"), MIN_ALLOW_FW) >= 0 ))
		{
			upsdebugx (2,"Can get output window min/max! (%s)", dstate_getinfo("ups.firmware"));

			ser_send(upsfd, "%s%s", GETX_ALLOW_RANGE, COMMAND_END);
			if(PowervarGetResponse (sDBuff, sizeof(sDBuff), GETX_RANGE_RESP_SIZE))
			{
				fatalx(EXIT_FAILURE, "Serial timeout(4) with ONEAC UPS on %s\n",device_path);
			}

			strncpy(buffer2, sDBuff, 3);
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
			strncpy(buffer2, sDBuff + 4, 3);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_TRUNCATION
#pragma GCC diagnostic pop
#endif
			j = atoi(buffer2);		/* Maximum voltage */

			strncpy(buffer2, sDBuff+8, 2);
			buffer2[2]='\0';
			k = atoi(buffer2);		/* Spread between */

			dstate_setinfo("input.transfer.low.min", "%3d", i);
			dstate_setinfo("input.transfer.low.max", "%3d", j-k);
			dstate_setinfo("input.transfer.high.min", "%3d", i+k);
			dstate_setinfo("input.transfer.high.max", "%3d", j);

		}
		else
		{
/* TBD, CUSPP MAYBE?? (Not required) */
			/* Use default values from firmware */
			upsdebugx (2, "Using trip defaults (%s)...", dstate_getinfo("ups.firmware"));

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

/* This function is called regularly for data updates. */
void upsdrv_updateinfo(void)
{
	static int CommTry = COMM_TRIES;		/* Comm loss counter */
	char buffer[BUFFSIZE];		/* Main response buffer */
	char buffer2[SUBBUFFSIZE];	/* Conversion buffer */
	char s;
	ssize_t RetValue;
	int timevalue;


/*++++++++++*/
	/* Keep moving this down as we develop init function. Just exits so driver
	 *  doesn't stay in memory and need to be exited
	 */

	printf ("Development Exit. Press ^C.\n");
//	dstate_datastale();

/*++++++++++*/


/* Original code from here down (Stuff removed over time during development) */
	/* Start with EG/ON information */
	ser_flush_in(upsfd, "", 0);  /*just in case*/
	ser_send (upsfd, "%c%s", GET_ALL, COMMAND_END);

	if (strncmp(UpsFamily, FAMILY_EG, FAMILY_SIZE) == 0)
	{
		RetValue = PowervarGetResponse (buffer, sizeof(buffer), GETALL_EG_RESP_SIZE);
	}
	else
	{
		RetValue = PowervarGetResponse (buffer, sizeof(buffer), GETALL_RESP_SIZE);
	}

	if ((RetValue != 0) && (CommTry == 0))
	{
		ser_comm_fail("Oneac UPS Comm failure continues on port %s", device_path);
	}
	else if (RetValue != 0)
	{
		if (--CommTry == 0)
		{
			ser_comm_fail("Oneac UPS Comm failure on port %s", device_path);
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
			{
				status_set("LB");
			}

			if (s & 0x04)			/* General fault */
			{
				dstate_setinfo("ups.test.result","UPS Internal Failure");
			}
			else
			{
				dstate_setinfo("ups.test.result","Normal");
			}

			if (s & 0x08)			/* Replace Battery */
			{
				status_set("RB");
			}

			if (s & 0x20)			/* Unit is hot */
			{
				alarm_set("OVERHEAT");
			}
		}

		/*take care of the reason why the UPS last transferred to battery*/
		switch (buffer[13]) {
			case XFER_BLACKOUT :
				dstate_setinfo("input.transfer.reason",	"Blackout");
				break;
			case XFER_LOW_VOLT :
				dstate_setinfo("input.transfer.reason", "Low Input Voltage");
				break;
			case XFER_HI_VOLT :
				dstate_setinfo("input.transfer.reason", "High Input Voltage");
				break;
			case NO_VALUE_YET :
				dstate_setinfo("input.transfer.reason", "No transfer yet.");
				break;
			default :
				upslogx(LOG_INFO,"Oneac: Unknown reason for UPS battery transfer [%c]", buffer[13]);
		}

		/* now update info for only the non-EG families of UPS*/

		if (strncmp(UpsFamily, FAMILY_EG, FAMILY_SIZE) != 0)
		{
			dstate_setinfo("ups.load", "0%.2s", buffer+31);

			/* Output ON or OFF? */
			if(buffer[27] == NO_VALUE_YET)
			{
				status_set("OFF");
			}

			/*battery charge*/
			if(buffer[10] == YES)
			{
				dstate_setinfo("battery.charge", "0%.2s", buffer+33);
			}
			else
			{
				dstate_setinfo("battery.charge", "100");
			}

			EliminateLeadingZeroes (buffer+35, 3, buffer2, sizeof(buffer2));
			dstate_setinfo("input.voltage", "%s", buffer2);

			EliminateLeadingZeroes (buffer+38, 3, buffer2, sizeof(buffer2));
			dstate_setinfo("input.voltage.minimum", "%s", buffer2);

			EliminateLeadingZeroes (buffer+41, 3, buffer2, sizeof(buffer2));
			dstate_setinfo("input.voltage.maximum", "%s", buffer2);

			EliminateLeadingZeroes (buffer+44, 3, buffer2, sizeof(buffer2));
			dstate_setinfo("output.voltage", "%s", buffer2);

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
					EliminateLeadingZeroes (buffer+15, 3, buffer2, sizeof(buffer2));
					dstate_setinfo("ups.timer.shutdown", "%s", buffer2);
				}
				else
				{
					dstate_setinfo("ups.timer.shutdown", "999");
				}
			}

			if (buffer[47] == YES)
			{
				status_set("BOOST");
			}
		}

		/* Now update info for only the OZ/OB families of UPS */

		if ((strncmp(UpsFamily, FAMILY_OZ, FAMILY_SIZE) == 0) ||
			(strncmp(UpsFamily, FAMILY_OB, FAMILY_SIZE) == 0))
		{
			ser_flush_in(upsfd,"",0);  /*just in case*/
			ser_send (upsfd, "%c%s", GETX_ALL_1, COMMAND_END);
			RetValue = PowervarGetResponse (buffer, sizeof(buffer), GETX_ALL1_RESP_SIZE);

			if(RetValue)
			{
				if (--CommTry == 0)
				{
					ser_comm_fail("Oneac (OZ) UPS Comm failure on port %s", device_path);
					dstate_datastale();
				}

				upsdebugx(2,"Oneac: "
					"Update (OZ) serial comm retry value: %d", CommTry);
			}
			else
			{
				CommTry = COMM_TRIES;		/* Reset count */

				EliminateLeadingZeroes (buffer+57, 5, buffer2, sizeof(buffer2));
				dstate_setinfo("ups.realpower", "%s", buffer2);

				dstate_setinfo("input.frequency", "%.2s.%c", buffer+42, buffer[44]);
				dstate_setinfo("output.frequency", "%.2s.%c", buffer+76, buffer[78]);

				EliminateLeadingZeroes (buffer+29, 3, buffer2, sizeof(buffer2));
				dstate_setinfo("battery.voltage", "%s.%c", buffer2, buffer[32]);

				dstate_setinfo("ups.temperature", "%.2s", buffer+13);
				dstate_setinfo("ups.load", "%.3s", buffer+73);

				strncpy(buffer2, buffer+19, 4);
				buffer2[4]='\0';
				timevalue = atoi(buffer2) * 60;		/* Change mins to secs */
				dstate_setinfo("battery.runtime", "%d", timevalue);

				/* Now some individual requests... */

				/* Battery replace date */
				ser_send (upsfd, "%c%s", GETX_BATT_REPLACED, COMMAND_END);
				if(!PowervarGetResponse (buffer, sizeof(buffer), GETX_DATE_RESP_SIZE))
				{
					dstate_setinfo("battery.date", "%.6s (yymmdd)", buffer);
				}

				/* Low and high output trip points */
				ser_send (upsfd, "%c%s", GETX_LOW_OUT_ALLOW, COMMAND_END);
				if(!PowervarGetResponse (buffer, sizeof(buffer), GETX_ALLOW_RESP_SIZE))
				{
					EliminateLeadingZeroes (buffer, 3, buffer2, sizeof(buffer2));
					dstate_setinfo("input.transfer.low", "%s", buffer2);
				}

				ser_send (upsfd, "%c%s", GETX_HI_OUT_ALLOW, COMMAND_END);
				if(!PowervarGetResponse (buffer, sizeof(buffer), GETX_ALLOW_RESP_SIZE))
				{
					dstate_setinfo("input.transfer.high", "%s", buffer);
				}

				/* Restart delay */
				ser_send (upsfd, "%c%s", GETX_RESTART_DLY, COMMAND_END);
				if(!PowervarGetResponse (buffer, sizeof(buffer), GETX_RSTRT_RESP_SIZE))
				{
					EliminateLeadingZeroes (buffer, 4, buffer2, sizeof(buffer2));
					dstate_setinfo("ups.delay.start", "%s", buffer2);
				}

				/* Buzzer state */
				ser_send (upsfd,"%s%s",GETX_BUZZER_WHAT,COMMAND_END);
				if(!PowervarGetResponse (buffer, sizeof(buffer), 1))
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
				ser_send (upsfd, "%s%s", GETX_AUTO_START, COMMAND_END);
				if(!PowervarGetResponse (buffer, sizeof(buffer), 1))
				{
					if (buffer[0] == '0')
					{
						dstate_setinfo("ups.start.auto", "yes");
					}
					else
					{
						dstate_setinfo("ups.start.auto", "no");
					}
				}

				/* Low Batt at time */
				ser_send (upsfd, "%c%s", GETX_LOW_BATT_TIME, COMMAND_END);
				if(!PowervarGetResponse (buffer, sizeof(buffer), 2))
				{
					strncpy(buffer2, buffer, 2);
					buffer2[2]='\0';
					timevalue = atoi(buffer2) * 60;		/* Mins to secs */
					dstate_setinfo("battery.runtime.low", "%d", timevalue);
				}

				/* Shutdown timer */
				ser_send (upsfd, "%c%s", GETX_SHUTDOWN, COMMAND_END);
				if(!PowervarGetResponse (buffer, sizeof(buffer), GETX_SHUTDOWN_RESP_SIZE))
				{
					/* ON would have handled NO_VALUE_YET and setting FSD
					 *  above so only deal with counter value here.
					 */
					if (buffer[0] != NO_VALUE_YET)
					{
						EliminateLeadingZeroes (buffer, 5, buffer2, sizeof(buffer2));
						dstate_setinfo("ups.timer.shutdown", "%s", buffer2);
					}
				}

				/* Restart timer */
				ser_send (upsfd, "%s%s", GETX_RESTART_COUNT, COMMAND_END);
				if(!PowervarGetResponse (buffer, sizeof(buffer), GETX_RSTRT_RESP_SIZE))
				{
					if (atoi(buffer) == 0)
					{
						dstate_delinfo("ups.timer.start");
					}
					else
					{
						EliminateLeadingZeroes (buffer, 4, buffer2, sizeof(buffer2));
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
	ser_send(upsfd, "%s", SHUTDOWN);
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
	addvar(VAR_VALUE, "testtime", "Change battery test time from the 2 minute default.");

	addvar(VAR_VALUE, "offdelay", "Change shutdown delay time from 0 second default.");
}

int instcmd(const char *cmdname, const char *extra)
{
	int i;

	upsdebugx(2, "In instcmd with %s and extra %s.", cmdname, extra);

	if (!strcasecmp(cmdname, "test.failure.start"))
	{
		ser_send(upsfd,"%s%s",SIM_PWR_FAIL,COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.return"))
	{

		i = atoi(dstate_getinfo("ups.delay.shutdown"));

		if ((strncmp (UpsFamily, FAMILY_OZ, FAMILY_SIZE) == 0) ||
			(strncmp (UpsFamily, FAMILY_OB, FAMILY_SIZE) == 0))
		{
			upsdebugx(3, "Shutdown using %c%d...", DELAYED_SHUTDOWN_PREFIX, i);
			ser_send(upsfd,"%c%d%s", DELAYED_SHUTDOWN_PREFIX, i, COMMAND_END);
		}
		else
		{
			upsdebugx(3, "Shutdown using %c%03d...", DELAYED_SHUTDOWN_PREFIX, i);
			ser_send(upsfd, "%c%03d%s", DELAYED_SHUTDOWN_PREFIX, i, COMMAND_END);
		}

		return STAT_INSTCMD_HANDLED;
	}

	if(!strcasecmp(cmdname, "shutdown.reboot"))
	{
		ser_send(upsfd, "%s", SHUTDOWN);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stop"))
	{
		ser_send(upsfd, "%c%s", DELAYED_SHUTDOWN_PREFIX, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start.quick"))
	{
		do_battery_test();
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start.deep"))
	{
		ser_send(upsfd, "%s%s", TEST_BATT_DEEP, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.stop"))
	{
		if ((strncmp (UpsFamily, FAMILY_EG, FAMILY_SIZE) == 0) ||
			(strncmp (UpsFamily, FAMILY_ON, FAMILY_SIZE) == 0))
		{
			ser_send(upsfd, "%s00%s", BAT_TEST_PREFIX, COMMAND_END);
		}
		else
		{
			ser_send(upsfd, "%c%s", TEST_ABORT, COMMAND_END);
		}
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "reset.input.minmax"))
	{
		ser_send(upsfd, "%c%s", RESET_MIN_MAX, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "beeper.enable"))
	{
		ser_send(upsfd, "%c%c%s", SETX_BUZZER_PREFIX, BUZZER_ENABLED, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "beeper.disable"))
	{
		ser_send(upsfd, "%c%c%s", SETX_BUZZER_PREFIX, BUZZER_DISABLED, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "beeper.mute"))
	{
		ser_send(upsfd,"%c%c%s", SETX_BUZZER_PREFIX, BUZZER_MUTED, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.panel.start"))
	{
		ser_send(upsfd,"%s%s", TEST_INDICATORS, COMMAND_END);
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
			ser_send(upsfd,"%s%s%s", SETX_RESTART_DELAY, setvalue, COMMAND_END);

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
			ser_send(upsfd,"%s%s%s", SETX_LOWBATT_AT, setvalue, COMMAND_END);

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
		if (!strcasecmp(setvalue, "yes"))
		{
			ser_send(upsfd,"%c0%s", SETX_AUTO_START, COMMAND_END);
			dstate_setinfo("ups.start.auto", "yes");
			return STAT_SET_HANDLED;
		}
		else if (!strcasecmp(setvalue, "no"))
		{
			ser_send(upsfd,"%c1%s", SETX_AUTO_START, COMMAND_END);
			dstate_setinfo("ups.start.auto", "no");
			return STAT_SET_HANDLED;
		}

		return STAT_SET_UNKNOWN;
	}

	upslogx(LOG_NOTICE, "setcmd: unknown command [%s]", varname);

	return STAT_SET_UNKNOWN;
}
