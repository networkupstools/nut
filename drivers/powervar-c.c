/*vim ts=4*/
/* powervar-c.c - Serial driver for Powervar UPM UPS using CUSPP.
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

//#include "config.h"		/* Must be first */

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

/* Serial comm stuff here */
#define SECS 0			/* Serial function wait time*/
#define USEC 500000		/* Rest of serial function wait time*/

#define COMM_TRIES	3	/* Serial retries before "stale" */

/* Common CUSPP stuff here */
static char UpsFamily [SUBBUFFSIZE];		/* Hold family that was found */
static char UpsProtVersion [SUBBUFFSIZE];	/* Hold protocol version string */

/* Dynamic CUSPP response information positions (0 = data not available) */
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
static uint8_t bySETOffdlyPos = 0;
static uint8_t bySETOffstpPos = 0;
static uint8_t bySETSrtdlyPos = 0;
static uint8_t bySETRstinpPos = 0;
static uint8_t bySETRsttmpPos = 0;

static uint8_t byALMOnbatPos = 0;
static uint8_t byALMLowbatPos = 0;
static uint8_t byALMBadbatPos = 0;
static uint8_t byALMTempPos = 0;
static uint8_t byALMOvrlodPos = 0;
static uint8_t byALMTstbadPos = 0;
static uint8_t byALMTestngPos = 0;
static uint8_t byALMChngbtPos = 0;

static uint8_t byTSTTimermPos = 0;
static uint8_t byTSTAbortPos = 0;
static uint8_t byTSTBatqckPos = 0;
static uint8_t byTSTBatdepPos = 0;
static uint8_t byTSTBatrunPos = 0;
static uint8_t byTSTBtemtyPos = 0;
static uint8_t byTSTDispPos = 0;

/**************************************
 * Serial communication functions     *
 *************************************/

/* Since an installed network card may slightly delay responses from
 *  the UPS allow for a repeat of the get request.
 */
#define RETRIES 4
static ssize_t PowervarGetResponse (char* chBuff, const size_t BuffSize)
{
	int Retries = RETRIES;		/* x/2 seconds max with 500000 USEC */
	ssize_t return_val;

	do
	{
		return_val = ser_get_line(upsfd, chBuff, BuffSize, ENDCHAR, IGNCHARS, SECS, USEC);

		if (return_val > 0)
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
		if (Retries < RETRIES)
		{
			upsdebugx (2,"PowervarGetResponse recovered (%d)...", Retries);
		}

		return_val = 0;					/* Good comms */
	}

	return return_val;
}


/* Get the FORMAT and/or data for the requested CUSPP group.
   This is used during initialization to establish the substring position values
    and to set the initial NUT data. This function can exit/abort and should only
    be used during initialization. For updates use: GetUPSData function.
*/
static void GetInitFormatAndOrData (const char* sReq, char* sF, const size_t sFSize, char* sD, const size_t sDSize)
{
	if (sF)
	{
		/* Get sReq format response */
		upsdebugx (2, "Requesting %s.FORMAT", sReq);

		ser_send(upsfd, "%s%s%c", sReq, FORMAT_TAIL, ENDCHAR);

		if(PowervarGetResponse (sF, sFSize))
		{
			fatalx(EXIT_FAILURE, "%s.FORMAT Serial timeout getting UPS data on %s\n", sReq, device_path);
		}

		if ((sF[0] == '?') || (strncmp(sReq, sF, STDREQSIZE) != 0))
		{
			upsdebugx (4, "[GetInitF] unexpected response: %s", sF);
			sF[0] = 0;		/* Show bad data */
			/* TBD, Retry?? */
		}
	}
	else
	{
		upsdebugx (4, "Bypassed requesting %s.FORMAT", sReq);
	}

	if (sD)
	{
		/* Get sReq data */
		upsdebugx (2, "Requesting %s data", sReq);

		ser_send(upsfd,"%s%c", sReq, ENDCHAR);

		if(PowervarGetResponse (sD, sDSize))
		{
			fatalx(EXIT_FAILURE, "%s Serial timeout getting UPS data on %s\n", sReq, device_path);
		}

		if ((sD[0] == '?') || (strncmp(sReq, sD, STDREQSIZE) != 0))
		{
			upsdebugx (4, "[GetInitD] unexpected response: %s", sD);

			sD[0]=0;		/* Show invalid response */
			/* TBD, Retry?? */
		}
	}
	else
	{
		upsdebugx (4, "Bypassed requesting %s data", sReq);
	}
}


/* Get the requested data group from the UPS. This is used for NUT updates
    and must not exit or abort. As long as this driver is being called it
    must try to communicate with the UPS.
*/
static uint8_t GetUPSData (const char* sReq, char* sD, const size_t sDSize)
{
uint8_t byReturn = 1;		/* Set up for good return, '1' is bad */

	/* Get sReq data */
	upsdebugx (2, "Requesting %s update", sReq);

	ser_send(upsfd,"%s%c", sReq, ENDCHAR);

	if((PowervarGetResponse (sD, sDSize) != 0) || (strncmp(sReq, sD, STDREQSIZE) != 0))
	{
		byReturn = 0;		/* Show invalid data */
		upsdebugx (3, "GetUPSData invalid response: %s, %d", sD, strncmp(sReq, sD, STDREQSIZE));
	}

	return byReturn;
}

/***************************************
 * CUSPP string handling functions     *
 **************************************/

/* This function parses responses to pull the desired substring from the buffer.
 * SubPosition is normal counting (start with 1 not 0).
 * chDst will have just the substring requested or will be set to NULL if
 *  substring is not found.
 */

static uint GetSubstringFromBuffer (char* chDst, const char* chSrc, const uint SubPosition)
{
uint RetVal = 0;		/* Start with a bad return value */
char WorkBuffer [BUFFSIZE];	/* Copy of passed in buffer */
char* chWork;			/* Pointer into working buffer */
char* chTok;			/* Pointer to token */
uint Pos;			/* Token position down-counter */

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
			RetVal = 1;

			upsdebugx (3,"Substring %d returning: \"%s\".", SubPosition, chDst);
		}
		else
		{
			upsdebugx (3,"Substring not found!");
		}
	}
	else
	{
		upsdebugx (4,"Position parameter was zero!");
	}

	return RetVal;
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
		upsdebugx (3,"Substring was not found!");
	}

	return uiReturn;
}



/****************************************************************
 * Below are the primary commands that are called by main       *
 ***************************************************************/

void upsdrv_initups(void)
{
	/* Serial comm init here */
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);

	/*get the UPS in the right frame of mind*/
	ser_send_pace(upsfd, 100, "%s", COMMAND_END);
	ser_send_pace(upsfd, 100, "%s", COMMAND_END);
	sleep (1);
}

/* TBD, Implement commands based on capability of the found UPS. */
/* TBD, Finish implementation of available data */
void upsdrv_initinfo(void)
{
//	int i, j, k;
	int Vlts = 1;
	char sFBuff[BUFFSIZE];
	char sDBuff[BUFFSIZE];
	char SubBuff[SUBBUFFSIZE];

	/* Get serial port ready */
	ser_flush_in(upsfd,"",0);

	/* First, try to get UPM PID.FORMAT response. All UPSs with CUSPP should reply
	 *  to this request so we can confirm that it is a Powervar UPS.
	 */

	GetInitFormatAndOrData (PID_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

	if(!sFBuff[0])
	{
		fatalx(EXIT_FAILURE, "[%s] Not a UPS that handles CUSPP\n", PID_PROT_SUB);
	}

	/* Check for standard CUSPP PROT request, exit if not there. */
	byPIDProtPos = GetSubstringPosition (sFBuff, PID_PROT_SUB);

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
	GetInitFormatAndOrData(UID_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

	if(sFBuff[0])
	{
		byUIDManufPos = GetSubstringPosition (sFBuff, UID_MANUF_SUB);
		byUIDModelPos = GetSubstringPosition (sFBuff, UID_MODEL_SUB);
		byUIDSwverPos = GetSubstringPosition (sFBuff, UID_SWVER_SUB);
		byUIDSernumPos = GetSubstringPosition (sFBuff, UID_SERNUM_SUB);
		byUIDFamilyPos = GetSubstringPosition (sFBuff, UID_FAMILY_SUB);
		byUIDMfgdtPos = GetSubstringPosition (sFBuff, UID_MFGDT_SUB);
		byUIDCSWVERPos = GetSubstringPosition (sFBuff, UID_CSWVER_SUB);
	}

	/* Finally begin to populate NUT data */
	/* Get FAMILY substring and keep for any later messages */
	if (GetSubstringFromBuffer (SubBuff, sDBuff, byUIDFamilyPos))
	{
		strcpy(UpsFamily, SubBuff);
		dstate_setinfo("device.description", "%s", SubBuff);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, byUIDModelPos))
	{
		dstate_setinfo("device.model", "%s",SubBuff);
	}

	dstate_setinfo("device.type", "ups");
	dstate_setinfo("ups.type", "%s", "Line Interactive");

	dstate_addcmd("reset.input.minmax");		/* TBD, UPM only!! */
	dstate_addcmd("test.battery.start.quick");
	dstate_addcmd("test.battery.stop");
//	dstate_addcmd("test.failure.start");
//	dstate_addcmd("shutdown.return");
//	dstate_addcmd("shutdown.stop");
//	dstate_addcmd("shutdown.reboot");
	dstate_addcmd("test.panel.start");
	dstate_addcmd("test.battery.start.deep");
	dstate_addcmd("beeper.enable");
	dstate_addcmd("beeper.disable");
	dstate_addcmd("beeper.mute");

	upsh.setvar = setcmd;
	upsh.instcmd = instcmd;

	/* Manufacturer */
	if (GetSubstringFromBuffer (SubBuff, sDBuff, byUIDManufPos))
	{
		dstate_setinfo("device.mfr", "%s", SubBuff);
	}

	/* Manufacture date */
	if (GetSubstringFromBuffer (SubBuff, sDBuff, byUIDMfgdtPos))
	{
		dstate_setinfo("ups.mfr.date", "%s (yyyymmdd)", SubBuff);
	}
	/* TBD, Pull yyq from GTS serial number...if family is tracked. */
	/* dstate_setinfo("ups.mfr.date", "%.3s (yyq)", SubBuff+<offset>);	*/

	/* Firmware revision(s) */
	if (GetSubstringFromBuffer (SubBuff, sDBuff, byUIDSwverPos))
	{
		dstate_setinfo("ups.firmware", "%s", SubBuff);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, byUIDCSWVERPos))
	{
		dstate_setinfo("ups.firmware.aux", "%s", SubBuff);
	}

	/* Serial number */
	if (GetSubstringFromBuffer (SubBuff, sDBuff, byUIDSernumPos))
	{
		dstate_setinfo("device.serial", "%s", SubBuff);
	}

	/* TBD, Put this in the log?? */
	printf ("Found a %s UPS with serial number %s\n", UpsFamily, SubBuff);


	/* Get BAT format and populate needed data string positions... */
	GetInitFormatAndOrData(BAT_REQ, sFBuff, sizeof(sFBuff), 0, 0);

	if(sFBuff[0])
	{
		byBATStatusPos = GetSubstringPosition (sFBuff, X_STATUS_SUB);
		byBATTmleftPos = GetSubstringPosition (sFBuff, BAT_TMLEFT_SUB);
		byBATEstcrgPos = GetSubstringPosition (sFBuff, BAT_ESTCRG_SUB);
		byBATVoltPos = GetSubstringPosition (sFBuff, X_VOLT_SUB);
		byBATTempPos = GetSubstringPosition (sFBuff, X_TEMP_SUB);
	}

	/* Get INP format and populate needed data string positions... */
	GetInitFormatAndOrData(INP_FMT_REQ, sFBuff, sizeof(sFBuff), 0, 0);

	if(sFBuff[0])
	{
		byINPStatusPos = GetSubstringPosition (sFBuff, X_STATUS_SUB);
		byINPFreqPos = GetSubstringPosition (sFBuff, X_FREQ_SUB);
		byINPVoltPos = GetSubstringPosition (sFBuff, X_VOLT_SUB);
		byINPAmpPos = GetSubstringPosition (sFBuff, X_AMP_SUB);
		byINPMaxvltPos = GetSubstringPosition (sFBuff, INP_MAXVLT_SUB);
		byINPMinvltPos = GetSubstringPosition (sFBuff, INP_MINVLT_SUB);
	}

	/* Get OUT format and populate needed data string positions... */
	GetInitFormatAndOrData(OUT_REQ, sFBuff, sizeof(sFBuff), 0, 0);

	if(sFBuff[0])
	{
		byOUTSourcePos = GetSubstringPosition (sFBuff, OUT_SOURCE_SUB);
		byOUTFreqPos = GetSubstringPosition (sFBuff, X_FREQ_SUB);
		byOUTVoltPos = GetSubstringPosition (sFBuff, X_VOLT_SUB);
		byOUTAmpPos = GetSubstringPosition (sFBuff, X_AMP_SUB);
		byOUTPercntPos = GetSubstringPosition (sFBuff, OUT_PERCNT_SUB);
	}

	/* Get SYS format and populate needed data string positions... */
	GetInitFormatAndOrData(SYS_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

	if(sFBuff[0])
	{
		bySYSInvoltPos = GetSubstringPosition (sFBuff, SYS_INVOLT_SUB);
		bySYSInfrqPos = GetSubstringPosition (sFBuff, SYS_INFRQ_SUB);
		bySYSOutvltPos = GetSubstringPosition (sFBuff, SYS_OUTVLT_SUB);
		bySYSOutfrqPos = GetSubstringPosition (sFBuff, SYS_OUTFRQ_SUB);
		bySYSBatdtePos = GetSubstringPosition (sFBuff, SYS_BATDTE_SUB);
		bySYSOvrlodPos = GetSubstringPosition (sFBuff, X_OVRLOD_SUB);
		bySYSOutvaPos = GetSubstringPosition (sFBuff, SYS_OUTVA_SUB);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSInvoltPos))
	{
		dstate_setinfo("input.voltage.nominal", "%s", SubBuff);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSInfrqPos))
	{
		dstate_setinfo("input.frequency.nominal", "%s", SubBuff);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSOutvltPos))
	{
		dstate_setinfo("output.voltage.nominal", "%s", SubBuff);
		Vlts = atoi(SubBuff);		/* Keep for nominal current calc */
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSOutfrqPos))
	{
		dstate_setinfo("output.frequency.nominal", "%s", SubBuff);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSOutvaPos))
	{
		dstate_setinfo("output.current.nominal", "%d", atoi(SubBuff)/Vlts);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSBatdtePos))
	{
		dstate_setinfo("battery.date", "%s (yyyymm)", SubBuff);
		dstate_setflags("battery.date", ST_FLAG_STRING | ST_FLAG_RW);
		dstate_setaux("battery.date", 6);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSOvrlodPos))
	{
		dstate_setinfo("ups.load.high", "%s", SubBuff);
	}

	/* Get SET format and populate needed data string positions... */
	GetInitFormatAndOrData(SET_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

	if(sFBuff[0])
	{
		bySETAudiblPos = GetSubstringPosition (sFBuff, SET_AUDIBL_SUB);
		bySETAtosrtPos = GetSubstringPosition (sFBuff, SET_ATOSRT_SUB);
		bySETOffdlyPos = GetSubstringPosition (sFBuff, SET_OFFDLY_SUB);
		bySETSrtdlyPos = GetSubstringPosition (sFBuff, SET_SRTDLY_SUB);
		bySETOffstpPos = GetSubstringPosition (sFBuff, SET_OFFSTP_SUB);
		bySETRstinpPos = GetSubstringPosition (sFBuff, SET_RSTINP_SUB);
		bySETRsttmpPos = GetSubstringPosition (sFBuff, SET_RSTTMP_SUB);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySETAtosrtPos))
	{
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

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySETOffdlyPos))
	{
		if (SubBuff[0] != '0')
		{
//TBD, FIX			dstate_setinfo("ups.timer.shutdown", "%s", SubBuff);
		}
	}

	/* Get ALM format and populate needed data string positions... */
	GetInitFormatAndOrData(ALM_REQ, sFBuff, sizeof(sFBuff), 0, 0);

	if(sFBuff[0])
	{
		byALMOnbatPos = GetSubstringPosition (sFBuff, ALM_ONBAT_SUB);
		byALMLowbatPos = GetSubstringPosition (sFBuff, ALM_LOWBAT_SUB);
		byALMBadbatPos = GetSubstringPosition (sFBuff, ALM_BADBAT_SUB);
		byALMTempPos = GetSubstringPosition (sFBuff, X_TEMP_SUB);
		byALMOvrlodPos = GetSubstringPosition (sFBuff, X_OVRLOD_SUB);
		byALMTstbadPos = GetSubstringPosition (sFBuff, ALM_TSTBAD_SUB);
		byALMTestngPos = GetSubstringPosition (sFBuff, ALM_TESTNG_SUB);
		byALMChngbtPos = GetSubstringPosition (sFBuff, ALM_CHNGBT_SUB);
	}

	/* Get TST format and populate needed data string positions... */
	GetInitFormatAndOrData(TST_REQ, sFBuff, sizeof(sFBuff), 0, 0);

	if(sFBuff[0])
	{
		byTSTTimermPos = GetSubstringPosition (sFBuff, TST_TIMERM_SUB);
		byTSTAbortPos = GetSubstringPosition (sFBuff, TST_ABORT_SUB);
		byTSTBatqckPos = GetSubstringPosition (sFBuff, TST_BATQCK_SUB);
		byTSTBatdepPos = GetSubstringPosition (sFBuff, TST_BATDEP_SUB);
		byTSTBatrunPos = GetSubstringPosition (sFBuff, TST_BATRUN_SUB);
		byTSTBtemtyPos = GetSubstringPosition (sFBuff, TST_BTEMTY_SUB);
		byTSTDispPos = GetSubstringPosition (sFBuff, TST_DISP_SUB);
	}
}

/*
	Stuff to follow up on realated to implementation for init

	 Shutdown delay in seconds...can be changed by user
	dstate_setinfo("ups.delay.shutdown", "%s", getval("offdelay"));

	dstate_setflags("ups.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW);
	dstate_setaux("ups.delay.shutdown", GET_SHUTDOWN_RESP_SIZE);

	 Low and high output trip points
	EliminateLeadingZeroes (sDBuff+73, 3, buffer2, sizeof(buffer2));
	dstate_setinfo("input.transfer.low", "%s", buffer2);
	dstate_setflags("input.transfer.low", ST_FLAG_STRING | ST_FLAG_RW );
	dstate_setaux("input.transfer.low", 3);

	EliminateLeadingZeroes (sDBuff+76, 3, buffer2, sizeof(buffer2));
	dstate_setinfo("input.transfer.high", "%s", buffer2);
	dstate_setflags("input.transfer.high", ST_FLAG_STRING | ST_FLAG_RW);
	dstate_setaux("input.transfer.high", 3);

	 Get output window min/max points from OB or OZ v1.9 or later

	i = atoi(buffer2);		Minimum voltage

	j = atoi(buffer2);		Maximum voltage

	strncpy(buffer2, sDBuff+8, 2);
	buffer2[2]='\0';
	k = atoi(buffer2);		Spread between

	dstate_setinfo("input.transfer.low.min", "%3d", i);
	dstate_setinfo("input.transfer.low.max", "%3d", j-k);
	dstate_setinfo("input.transfer.high.min", "%3d", i+k);
	dstate_setinfo("input.transfer.high.max", "%3d", j);
*/

/* This function is called regularly for data updates. */
void upsdrv_updateinfo(void)
{
	char sData[BUFFSIZE];
	char SubString[SUBBUFFSIZE];
	uint8_t byOnBat = 0;		    /* Keep flag between OUT and BAT groups */
//	uint8_t byBadBat = 0;		    /* Keep flag for 'RB' logic */
	char chC;			    /* Character being worked with */
	int timevalue;

	/* Get serial port ready */
	ser_flush_in(upsfd,"",0);

	/* Get output data first... */
	if (GetUPSData (OUT_REQ, sData, sizeof (sData)) == 0)
	{
		ser_comm_fail("Powervar UPS Comm failure on port %s", device_path);
		dstate_datastale ();
		return;
	}
	else
	{
		status_init();
		alarm_init();

		/* Handle output source information...*/
		if (GetSubstringFromBuffer (SubString, sData, byOUTSourcePos))
		{
			chC = SubString[0];

			if (chC == '0')
			{
				status_set("OFF");
			}
			else if (chC == '1')
			{
				status_set("OL");
			}
			else if (chC == '3')
			{
				status_set("OB");
				byOnBat = 1;		/* Keep flag for ALM section */
			}
			else if ((chC == '4') || (chC == '5') || (chC == '8'))
			{
				status_set("BOOST");
			}
			else if ((chC == '6') || (chC == '7'))
			{
				status_set("TRIM");
			}
		}

		/* Handle output percent information...*/
		if(GetSubstringFromBuffer (SubString, sData, byOUTPercntPos))
		{
			dstate_setinfo ("ups.load", "%s", SubString);
		}

		/* Handle output voltage information...*/
		if(GetSubstringFromBuffer (SubString, sData, byOUTVoltPos))
		{
			dstate_setinfo ("output.voltage", "%s", SubString);
		}

		/* Handle output frequency information...*/
		if(GetSubstringFromBuffer (SubString, sData, byOUTFreqPos))
		{
			dstate_setinfo ("output.frequency", "%s", SubString);
		}

		/* Handle output current information...*/
		if(GetSubstringFromBuffer (SubString, sData, byOUTAmpPos))
		{
			dstate_setinfo ("output.current", "%s", SubString);
		}
	}

	/* Get input data next... */
	if (GetUPSData (INP_DATA_REQ, sData, sizeof (sData)))
	{
		/* Handle input voltage information...*/
		if(GetSubstringFromBuffer (SubString, sData, byINPVoltPos))
		{
			dstate_setinfo ("input.voltage", "%s", SubString);
		}

		/* Handle input frequency information...*/
		if(GetSubstringFromBuffer (SubString, sData, byINPFreqPos))
		{
			dstate_setinfo ("input.frequency", "%s", SubString);
		}

		/* Handle input max-voltage information...*/
		if(GetSubstringFromBuffer (SubString, sData, byINPMaxvltPos))
		{
			dstate_setinfo ("input.voltage.maximum", "%s", SubString);
		}

		/* Handle input min-voltage information...*/
		if(GetSubstringFromBuffer (SubString, sData, byINPMinvltPos))
		{
			dstate_setinfo ("input.voltage.minimum", "%s", SubString);
		}

		/* Handle input current information...*/
		if(GetSubstringFromBuffer (SubString, sData, byINPAmpPos))
		{
			dstate_setinfo ("input.current", "%s", SubString);
		}
	}

	/* Get battery data next... */
	if (GetUPSData (BAT_REQ, sData, sizeof (sData)))
	{
		/* Handle battery status information...*/
		if(GetSubstringFromBuffer (SubString, sData, byBATStatusPos))
		{
			if ((byOnBat) && (SubString[0] == '3'))
			{
				status_set ("LB");
			}
		}

		/* Handle battery estimated charge information...*/
		if(GetSubstringFromBuffer (SubString, sData, byBATEstcrgPos))
		{
			dstate_setinfo ("battery.charge", "%s", SubString);
		}

		/* Handle battery voltage information...*/
		if(GetSubstringFromBuffer (SubString, sData, byBATVoltPos))
		{
			dstate_setinfo ("battery.voltage", "%s", SubString);
		}

		/* Handle battery temperature information...*/
		if(GetSubstringFromBuffer (SubString, sData, byBATTempPos))
		{
			dstate_setinfo ("battery.temperature", "%s", SubString);
		}

		/* Handle battery time left information...*/
		if (GetSubstringFromBuffer (SubString, sData, byBATTmleftPos))
		{
			timevalue = (int)(strtol(SubString, 0, 10) * 60);    /* Min to Secs */
			dstate_setinfo ("battery.runtime", "%d", timevalue);
		}
	}

	/* Get SET data next... */
	if (GetUPSData (SET_REQ, sData, sizeof (sData)))
	{
		/* Handle audible status information...*/
		if(GetSubstringFromBuffer (SubString, sData, bySETAudiblPos))
		{
			if (SubString[0] == '1')
			{
				dstate_setinfo ("ups.beeper.status", "enabled");
			}
			else if (SubString[0] == '0')
			{
				dstate_setinfo ("ups.beeper.status", "disabled");
			}
			else if (SubString[0] == '2')
			{
				dstate_setinfo ("ups.beeper.status", "muted");
			}
		}

		/* Handle OFFDLY status information...*/
		if(GetSubstringFromBuffer (SubString, sData, bySETOffdlyPos))
		{
/* 			if (SubString[0] == '0')
			{
				dstate_delinfo("ups.timer.shutdown");
			}
			else
			{
				dstate_setinfo("ups.timer.shutdown", "%s", SubString);
			}
 */		}

		/* Handle SRTDLY status information...*/
		if(GetSubstringFromBuffer (SubString, sData, bySETSrtdlyPos))
		{
			if (SubString[0] == '0')
			{
				dstate_delinfo("ups.timer.start");
			}
			else
			{
				dstate_setinfo("ups.timer.start", "%s", SubString);
			}
		}

	}

	/* Get ALM data next... */
	if (GetUPSData (ALM_REQ, sData, sizeof (sData)))
	{
		/* Handle replace battery alarm information...*/
		if(GetSubstringFromBuffer (SubString, sData, byALMBadbatPos))
		{
			if (SubString[0] == '1')
			{
				status_set ("RB");
			}
		}

		/* Handle overload alarm information...*/
		if(GetSubstringFromBuffer (SubString, sData, byALMOvrlodPos))
		{
			if (SubString[0] == '1')
			{
				status_set ("OVER");
			}
		}

		/* Handle temperature alarm information...*/
		/* May not actually be handled by NUT. "No official list of alarm words." */
/* 		if(GetSubstringFromBuffer (SubString, sData, byALMTempPos))
		{
			if (SubString[0] == '1')
			{
				alarm_set ("OVERHEAT");
			}
		}
 */
		/* Handle testing alarm information...*/
		/* UPM only. Set when battery run tests are active. */
		/* May not actually be handled by NUT. "No official list of alarm words." */
/* 		if(GetSubstringFromBuffer (SubString, sData, byALMTestngPos))
		{
			if (SubString[0] == '1')
			{
				alarm_set ("TEST_RUNNING");
			}
		}
 */
		/* Handle testing alarm information...*/
		/* UPM only. Means Battery Life Test failed. */
		if(GetSubstringFromBuffer (SubString, sData, byALMTstbadPos))
		{
			if (SubString[0] == '1')
			{
				dstate_setinfo("ups.test.result","Change Battery");
				status_set ("RB");
			}
			else
			{
				dstate_setinfo("ups.test.result","Normal");

			}
		}
	}


	/* Get SYS data next... */
	if (GetUPSData (SYS_REQ, sData, sizeof (sData)))
	{
		/* Handle audible status information...*/
		if(GetSubstringFromBuffer (SubString, sData, bySYSBatdtePos))
		{
			dstate_setinfo ("battery.date", "%s (yyyymm)", SubString);
		}
	}

	/* Need any TST data?? */


	alarm_commit();
	status_commit();

	dstate_dataok();
	ser_comm_good();
}

/* Items to look into more for implementation:

 	 Low and high output trip points
	dstate_setinfo("input.transfer.low", "%s", buffer2);
	dstate_setinfo("input.transfer.high", "%s", buffer);

	 Low Batt at time
	timevalue = atoi(buffer2) * 60;		Mins to secs
	dstate_setinfo("battery.runtime.low", "%d", timevalue);

	 Shutdown timer
	dstate_setinfo("ups.timer.shutdown", "%s", buffer2);

	 Restart timer
	ser_send (upsfd, "%s%s", GETX_RESTART_COUNT, COMMAND_END);
	dstate_delinfo("ups.timer.start");
	dstate_setinfo("ups.timer.start", "%s", buffer2);

*/

/**********************************************************
 * Powervar support functions for NUT command calls       *
 *********************************************************/

static void do_battery_test(void)
{
	if (byTSTBatrunPos)
	{
		char buffer[32];

		if (getval("battesttime") == NULL)
		{
			snprintf(buffer, 3, "%s", DEFAULT_BAT_TEST_TIME);
		}
		else
		{
			snprintf(buffer, 6, "%s", getval("battesttime"));
		}

		ser_send(upsfd, "%s%s%s", TST_BATRUN_REQ, buffer, COMMAND_END);
	}
}

/**************************************
 * Handlers for NUT command calls     *
 *************************************/

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
	addvar(VAR_VALUE, "battesttime", "Change battery test time from the 10 second default.");

	addvar(VAR_VALUE, "disptesttime", "Change display test time from the 10 second default.");

	addvar(VAR_VALUE, "offdelay", "Change shutdown delay time from 0 second default.");
}

void upsdrv_shutdown(void)
{
//	ser_send(upsfd, "%s", SHUTDOWN);
}

int instcmd(const char *cmdname, const char *extra)
{
//	int i;
	char buffer [10];

	upsdebugx(2, "In instcmd with %s and extra %s.", cmdname, extra);

	if (!strcasecmp(cmdname, "test.battery.start.quick"))
	{
		if (byTSTBatqckPos)
		{
			do_battery_test();
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "test.battery.start.deep"))
	{
		if (byTSTBatdepPos)
		{
			ser_send(upsfd, "%s%s", TST_BATDEP_REQ, COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "test.battery.stop"))
	{
		if (byTSTAbortPos)
		{
			ser_send(upsfd, "%s%s", TST_ABORT_REQ, COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "reset.input.minmax"))
	{
		if (bySETRstinpPos)
		{
			ser_send(upsfd, "%s%s", SET_RSTINP_REQ, COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "beeper.enable"))
	{
		if (bySETAudiblPos)
		{
			ser_send(upsfd, "%s%c%s", SET_AUDIBL_REQ, '1', COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "beeper.disable"))
	{
		if (bySETAudiblPos)
		{
			ser_send(upsfd, "%s%c%s", SET_AUDIBL_REQ, '0', COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "beeper.mute"))
	{
		if (bySETAudiblPos)
		{
			ser_send(upsfd, "%s%c%s", SET_AUDIBL_REQ, '2', COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "test.panel.start"))
	{
		if (byTSTDispPos)
		{

			if (getval("disptesttime") == NULL)
			{
				snprintf(buffer, 2, "1");
			}
			else
			{
				snprintf(buffer, 4, "%s", getval("disptesttime"));
			}

			ser_send(upsfd,"%s%s%s", TST_DISP_REQ, buffer, COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}


int setcmd(const char* varname, const char* setvalue)
{
	upsdebugx(2, "In setcmd for %s with %s...", varname, setvalue);



	upslogx(LOG_NOTICE, "setcmd: unknown command [%s]", varname);

	return STAT_SET_UNKNOWN;
}
