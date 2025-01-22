/*vim ts=4*/
/* powervar-cx.c - Common items for Powervar UPS CUSPP drivers.
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
 * - 2 December 2024.  Bill Elliot
 * Breaking out common items for serial and USB drivers. This file is not
 *  intended to be compiled as a stand-alone file but is to be included
 *  into the driver files that need it.
 *
 */

#ifndef NUT_POWERVAR_CX_H_SEEN
#define NUT_POWERVAR_CX_H_SEEN 1

/* Prototypes for calls that may be prior to function definition */
static void GetInitFormatAndOrData (const char* sReq, char* sF, const size_t sFSize, char* sD, const size_t sDSize);
static uint8_t GetUPSData (char* sReq, char* sD, const size_t sDSize);
int instcmd(const char *cmdname, const char *extra);
int setcmd(const char* varname, const char* setvalue);


/* Common CUSPP stuff here */
#define FALSE			0
#define TRUE			1

/*misc stuff*/
#define BUFFSIZE		512
#define SUBBUFFSIZE		48
#define STDREQSIZE		3	/* Length of all primary requests */

/* For use in some instant commands...if requested */
#define DEFAULT_BAT_TEST_TIME 	"10"	/* TST.BATRUN */
#define DEFAULT_DISP_TEST_TIME	"10"	/* TST.DISP */

#define ENDCHAR			'\r'
#define ENDCHARS		"\r"
#define CHAR_EQ			'='
#define IGNCHARS 		"\n"
#define COMMAND_END 		"\r\n"
#define FORMAT_TAIL		".FORMAT"
#define FORMAT_TAILR		".FORMAT\r"
#define DATADELIM		";\r"
#define FRMTDELIM		";."
#define DONT_UNDERSTAND		'?'

/*Information requests -- CUSPP*/

/*Anticipated FAMILY responses*/
#define FAMILY_GTS		"GTS"
#define FAMILY_UPM		"UPM"

/* Some common substrings */
#define X_STATUS_SUB		"STATUS"
#define X_VOLT_SUB		"VOLT"
#define X_FREQ_SUB		"FREQ"
#define X_AMP_SUB		"AMP"
#define X_TEMP_SUB		"TEMP"
#define X_OVRLOD_SUB		"OVRLOD"

/* Now individual request substrings */
#define PID_REQ			"PID"
#define PID_VER_SUB		"VER"
#define PID_PROT_SUB		"PROT"
#define PID_PROT_DATA		"CUSPP"

#define UID_REQ			"UID"
#define UID_MANUF_SUB		"MANUF"
#define UID_MODEL_SUB		"MODEL"
#define UID_SWVER_SUB		"SWVER"
#define UID_SERNUM_SUB		"SERNUM"
#define UID_FAMILY_SUB		"FAMILY"
#define UID_MFGDT_SUB		"MFGDT"
#define UID_CSWVER_SUB		"CSWVER"

#define BAT_REQ			"BAT"
#define BAT_TMLEFT_SUB		"TMLEFT"
#define BAT_ESTCRG_SUB		"ESTCRG"
#define BAT_TEMP_SUB		"TEMP"

#define INP_FMT_REQ		"INP"
#define INP_DATA_REQ		"INP.1"
#define INP_MAXVLT_SUB		"MAXVLT"
#define INP_MINVLT_SUB		"MINVLT"

#define OUT_REQ			"OUT"
#define OUT_SOURCE_SUB		"SOURCE"
#define OUT_PERCNT_SUB		"PERCNT"

#define SYS_REQ			"SYS"
#define SYS_INVOLT_SUB		"INVOLT"
#define SYS_INFRQ_SUB		"INFRQ"
#define SYS_OUTVLT_SUB		"OUTVLT"
#define SYS_OUTFRQ_SUB		"OUTFRQ"
#define SYS_BATDTE_SUB		"BATDTE"
#define SYS_OUTVA_SUB		"OUTVA"
#define SYS_BTSTDY_SUB		"BTSTDY"

#define SET_REQ			"SET"
#define SET_AUDIBL_SUB		"AUDIBL"
#define SET_ATOSRT_SUB		"ATOSRT"
#define SET_OFFDLY_SUB		"OFFDLY"
#define SET_SRTDLY_SUB		"SRTDLY"
#define SET_OFFSTP_SUB		"OFFSTP"
#define SET_RSTINP_SUB		"RSTINP"
#define SET_RSTTMP_SUB		"RSTTMP"

#define ALM_REQ			"ALM"
#define ALM_ONBAT_SUB		"ONBAT"
#define ALM_LOWBAT_SUB		"LOWBAT"
#define ALM_BADBAT_SUB		"BADBAT"
#define ALM_TSTBAD_SUB		"TSTBAD"
#define ALM_TESTNG_SUB		"TESTNG"
#define ALM_CHNGBT_SUB		"CHNGBT"

#define TST_REQ			"TST"
#define TST_TIMERM_SUB		"TIMERM"
#define TST_ABORT_SUB		"ABORT"
#define TST_BATQCK_SUB		"BATQCK"
#define TST_BATDEP_SUB		"BATDEP"
#define TST_BATRUN_SUB		"BATRUN"
#define TST_BTEMTY_SUB		"BTEMTY"
#define TST_DISP_SUB		"DISP"

#define EVT_REQ			"EVT"
#define EVT_UPTIME_SUB		"UPTIME"

/* Control requests */
#define TST_BATQCK_REQ		"TST.BATQCK.W=1"
#define TST_BATDEP_REQ		"TST.BATDEP.W=1"
#define TST_BATRUN_REQ		"TST.BATRUN.W="
#define TST_ABORT_REQ		"TST.ABORT.W=1"
#define TST_DISP_REQ		"TST.DISP.W="

#define SET_OFFNOW_REQ		"SET.OFFNOW.W=1"
#define SET_OFFDLY_REQ		"SET.OFFDLY.W="
#define SET_SRTDLY_REQ		"SET.SRTDLY.W="
#define SET_OFFON_REQ		"SET.OFFON.W="
#define SET_OFFSTP_REQ		"SET.OFFSTP.W=1"
#define SET_AUDIBL_REQ		"SET.AUDIBL.W="
#define SET_ATOSRT_REQ		"SET.ATOSRT.W="
#define SET_RSTINP_REQ		"SET.RSTINP.W=1"

/* Common CUSPP variables here */

static char UpsFamily [SUBBUFFSIZE];		/* Hold family that was found */
static u_char sbUPM = FALSE;
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
static uint8_t bySYSBtstdyPos = 0;

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

static uint8_t byEVTUptimePos = 0;

/******************************************
 * Functions for getting strings from UPS *
 ******************************************/

/* Get the FORMAT and/or data for the requested CUSPP group.
   This is used during initialization to establish the substring position values
    and to set the initial NUT data. This function can exit/abort and should only
    be used during initialization. For updates use: GetUPSData function.
*/
static void GetInitFormatAndOrData (const char* sReq, char* sF, const size_t sFSize, char* sD, const size_t sDSize)
{
char sRequest[15];

//	printf ("In GetInitFormatAndOrData... \n");

	if (sFSize)
	{
		memset(sF, 0, sFSize);
		strcpy(sRequest, sReq);
		strcat(sRequest, FORMAT_TAIL);

		/* Get sReq.FORMAT response */
		upsdebugx (2, "Requesting '%s'", sRequest);

		SendRequest ((const char*)sRequest);

		if(PowervarGetResponse (sF, sFSize))
		{
			fatalx(EXIT_FAILURE, "'%s.FORMAT' timeout getting UPS data on %s\n", sReq, device_path);
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
		upsdebugx (4, "Bypassed requesting '%s.FORMAT'", sReq);
	}

	if (sDSize)
	{
		memset(sD, 0, sDSize);

//		strcpy(sRequest, sReq);

		/* Get sReq data */
		upsdebugx (2, "Requesting '%s' data", sReq);
//		upsdebugx (2, "Requesting '%s' data", sRequest);

		SendRequest((const char*)sReq);
//		SendRequest(sRequest);
//		SendRequest ((const char*)sRequest);

		if(PowervarGetResponse (sD, sDSize))
		{
			fatalx(EXIT_FAILURE, "'%s' timeout getting UPS data on '%s'\n", sReq, device_path);
		}

		if ((sD[0] == '?') || (strncmp(sReq, sD, STDREQSIZE) != 0))
		{
			upsdebugx (4, "[GetInitD] unexpected response: '%s'", sD);

			sD[0] = 0;		/* Show invalid response */
			/* TBD, Retry?? */
		}
	}
	else
	{
		upsdebugx (4, "Bypassed requesting '%s' data", sReq);
	}

//	printf ("Leaving GetInitFormatAndOrData... \n");
}


/* Get the requested data group from the UPS. This is used for NUT updates
    and must not exit or abort. As long as this driver is being called it
    must try to communicate with the UPS.
*/
static uint8_t GetUPSData (char* sReq, char* sD, const size_t sDSize)
{
uint8_t byReturn = 1;		/* Set up for good return, '0' is bad */
//char sRequest[10];

	memset(sD, 0, sDSize);

//	strcpy(sRequest, sReq);

	/* Get sReq data */
	upsdebugx (2, "Requesting %s update", sReq);
//	upsdebugx (2, "Requesting %s update", sRequest);

	SendRequest((const char*)sReq);
//	SendRequest((const char*)sRequest);

	if((PowervarGetResponse (sD, sDSize) != 0) || (strncmp(sReq, sD, STDREQSIZE) != 0))
	{
		byReturn = 0;		/* Show invalid data */

		if (sD[0] != DONT_UNDERSTAND)
		{
			upsdebugx (3, "GetUPSData invalid response: '%s', %d", sD, strncmp(sReq, sD, STDREQSIZE));
		}
	}

	return byReturn;
}


/***************************************
 * CUSPP string handling functions     *
 ***************************************/

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

			upsdebugx (3,"Substring %d returned: \"%s\".", SubPosition, chDst);
		}
		else
		{
			upsdebugx (3,"Substring not found!");
		}
	}
	else
	{
		upsdebugx (4,"GetSubstringFromBuffer: Position parameter was zero!");
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
				upsdebugx (3,"Found substring '%s' at position: %d.", chSub, uiReturn);
				break;
			}

			uiPos++;
		}

		chTok = strtok (0, FRMTDELIM);
	}

	if (uiReturn == 0)
	{
		upsdebugx (3,"GetSubstringPosition: Substring was not found!");
	}

	return uiReturn;
}


/* Just prints out contents of buffer but in hex representation */
void ShowStringHex (const char* pS)
{
	size_t Len = strlen(pS);
	uint32_t i = 0;

	/* Limit length to USB packet size for now (plus a little bit) */
	if (Len > BUFFSIZE)
	{
		Len = BUFFSIZE;
	}

	/* Show individual character hex values */
	printf ("Hex|ASCII: ");
	do
	{
		printf ("%02X ", pS[i++]);
	} while (i <= (Len - 1));

	printf ("| ");			/* Divide hex from ASCII */

	i = 0;
	/* Show individual printable characters or dot '.' */
	do
	{
		printf ("%c", pS[i] > 32 ? pS[i] : '.');
		i++;
	} while (i <= (Len - 1));

	printf ("%s", COMMAND_END);	/* Just finish the line */
}


/*****************************************
 * Common portions of standard functions *
 *****************************************/

void PvarCommon_Initinfo (void)
{
	int Vlts = 1;
	char sFBuff[BUFFSIZE];
	char sDBuff[BUFFSIZE];
	char SubBuff[SUBBUFFSIZE];

	/* First, try to get UPM PID.FORMAT response. All UPSs with CUSPP should reply
	 *  to this request so we can confirm that it is a Powervar UPS.
	 */

	GetInitFormatAndOrData (PID_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

	if(!sFBuff[0] || !sDBuff[0])
	{
		fatalx(EXIT_FAILURE, "[%s] Comm error or not a UPS that handles CUSPP\n", PID_REQ);
	}

	/* Check for standard CUSPP PROT request, exit if not there. */
	byPIDProtPos = GetSubstringPosition (sFBuff, PID_PROT_SUB);

	/* Check for 'CUSPP' */

	GetSubstringFromBuffer (SubBuff, sDBuff, byPIDProtPos);

	if(strcmp(SubBuff, PID_PROT_DATA) != 0)
	{
		fatalx(EXIT_FAILURE, "[%s] Not a UPS that handles CUSPP\n", PID_PROT_DATA);
	}

	/* Keep CUSPP version string ... for now. */
	byPIDVerPos = GetSubstringPosition (sFBuff, PID_VER_SUB);

	GetSubstringFromBuffer (SubBuff, sDBuff, byPIDVerPos);
	strcpy (UpsProtVersion, SubBuff);

	/* If we have gotten this far, the UPS being talked to will have some
	 *  portion of the following requested variables.
	 */

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
		/*TBD, Fix...don't copy <CR> into UpsFamily or below uses of SubBuff */
		strncpy(UpsFamily, SubBuff, STDREQSIZE);	/* GTS or UPM */
		dstate_setinfo("device.description", "%s", UpsFamily);
		dstate_setinfo("ups.id", "%s", UpsFamily);

		if(strcmp(SubBuff, FAMILY_UPM) != 0)
		{
			sbUPM = TRUE;		/* This is a UPM ups */
		}
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, byUIDModelPos))
	{
		dstate_setinfo("device.model", "%s",SubBuff);
	}

	dstate_setinfo("device.type", "ups");
	dstate_setinfo("ups.type", "%s", "Line Interactive");

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
	upsdebugx (2, "Found a '%s' UPS with serial number: %s", UpsFamily, SubBuff);

	/* Get BAT format and populate needed data string positions... */
	GetInitFormatAndOrData(BAT_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

	if(sFBuff[0])
	{
		byBATStatusPos = GetSubstringPosition (sFBuff, X_STATUS_SUB);
		byBATTmleftPos = GetSubstringPosition (sFBuff, BAT_TMLEFT_SUB);
		byBATEstcrgPos = GetSubstringPosition (sFBuff, BAT_ESTCRG_SUB);
		byBATVoltPos = GetSubstringPosition (sFBuff, X_VOLT_SUB);
		byBATTempPos = GetSubstringPosition (sFBuff, X_TEMP_SUB);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, byBATVoltPos))
	{
		dstate_setinfo("battery.voltage.nominal", "%s", (atoi(SubBuff) < 300 ? "24" : "48"));
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
		bySYSBtstdyPos = GetSubstringPosition (sFBuff, SYS_BTSTDY_SUB);
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

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSBtstdyPos))
	{
		dstate_setinfo("ups.test.interval", "%ld", atoi(SubBuff) * 86400L);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSBatdtePos))
	{
		dstate_setinfo("battery.date", "%s (yyyymm)", SubBuff);

		if (sbUPM) /* Writable in UPM only!! */
		{
			dstate_setflags("battery.date", ST_FLAG_STRING | ST_FLAG_RW);
			dstate_setaux("battery.date", 6);
		}
	}

	/* For now, all are lead acid. */
	dstate_setinfo("battery.type", "%s", "PbAc");

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSOvrlodPos))
	{
		dstate_setinfo("ups.load.high", "%s", SubBuff);
	}
	else
	{
		dstate_setinfo("ups.load.high", "100");	/* For GTS */
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
			dstate_setinfo("ups.timer.shutdown", "%s", SubBuff);
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

	/* Get EVT format and populate possible data string postions... */
	GetInitFormatAndOrData(EVT_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

	if(sFBuff[0])
	{
		byEVTUptimePos = GetSubstringPosition (sFBuff, EVT_UPTIME_SUB);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, byEVTUptimePos))
	{
		dstate_setinfo("device.uptime", "%s", SubBuff);
	}

	/* Add NUT commands that are available to either GTS or UPM */
	dstate_addcmd("test.battery.start.quick");
	dstate_addcmd("test.battery.start.deep");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("test.failure.start");
	dstate_addcmd("test.failure.stop");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("shutdown.reboot");
	dstate_addcmd("beeper.enable");
	dstate_addcmd("beeper.disable");
	dstate_addcmd("beeper.mute");
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	dstate_addcmd("load.off.delay");
	dstate_addcmd("load.on.delay");


	if (sbUPM)	/* Only available in UPM family of ups */
	{
		dstate_addcmd("reset.input.minmax");
		dstate_addcmd("test.panel.start");
	}

	upsh.setvar = setcmd;
	upsh.instcmd = instcmd;


/*
	Stuff to follow up on realated to implementation for initinfo

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

}


void PvarCommon_Updateinfo (void)
{
	char sData[BUFFSIZE];
	char SubString[SUBBUFFSIZE];
	uint8_t byOnBat = 0;		    /* Keep flag between OUT and BAT groups */
//	uint8_t byBadBat = 0;		    /* Keep flag for 'RB' logic */
	char chC;			    /* Character being worked with */
	int timevalue;

	/* Get output data first... to see if we are still talking. */
	if (GetUPSData (OUT_REQ, sData, sizeof (sData)) == 0)
	{
#if defined PVAR_SERIAL
		ser_comm_fail("Powervar UPS Comm failure on port %s", device_path);
#endif
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
			dstate_setinfo ("ups.temperature", "%s", SubString);
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
 			if (SubString[0] == '0')
			{
				dstate_delinfo("ups.timer.shutdown");
			}
			else
			{
				dstate_setinfo("ups.timer.shutdown", "%s", SubString);
			}
		}

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
		/* Handle battery date information...*/
		if(GetSubstringFromBuffer (SubString, sData, bySYSBatdtePos))
		{
			dstate_setinfo ("battery.date", "%s (yyyymm)", SubString);
		}
	}

	/* Get EVT data next... */
	if (GetUPSData (EVT_REQ, sData, sizeof (sData)))
	{
		if(GetSubstringFromBuffer (SubString, sData, byEVTUptimePos))
		{
			dstate_setinfo("device.uptime", "%s", SubString);
		}
	}

	/* Need any TST data?? */


	alarm_commit();
	status_commit();

	dstate_dataok();

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

}

/* Support functions for Powervar UPS drivers */
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
//	ser_send(upsfd, "%s%s%s", BAT_TEST_PREFIX, buffer, COMMAND_END);
}


/* Functions called by NUT that impact or use the Powervar UPS driver */
void upsdrv_makevartable(void)
{
//	addvar(VAR_VALUE, "battesttime", "Change battery test time from the 10 second default.");

//	addvar(VAR_VALUE, "disptesttime", "Change display test time from the 10 second default.");

//	addvar(VAR_VALUE, "offdelay", "Change shutdown delay time from 0 second default.");
}

void upsdrv_shutdown(void)
{
//	ser_send(upsfd, "%s", SHUTDOWN);
}


int instcmd(const char *cmdname, const char *extra)
{
//	int i;

	upsdebugx(2, "In instcmd with %s and extra %s.", cmdname, extra);

	if (!strcasecmp(cmdname, "test.failure.start"))
	{
//		ser_send(upsfd,"%s%s",SIM_PWR_FAIL,COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.return"))
	{
/* 		i = atoi(dstate_getinfo("ups.delay.shutdown"));

		if ((strncmp (UpsFamily, FAMILY_OZ, FAMILY_SIZE) == 0) ||
			(strncmp (UpsFamily, FAMILY_OB, FAMILY_SIZE) == 0))
		{
			upsdebugx(3, "Shutdown using %c%d...", DELAYED_SHUTDOWN_PREFIX, i);
			ser_send(upsfd,"%c%d%s", DELAYED_SHUTDOWN_PREFIX, i, COMMAND_END);
		}
		else
		{
			upsdebugx(3, "Shutdown using %c%03d...", DELAYED_SHUTDOWN_PREFIX, i)
			ser_send(upsfd, "%c%03d%s", DELAYED_SHUTDOWN_PREFIX, i, COMMAND_END);
		}

		return STAT_INSTCMD_HANDLED;
 */	}

	if(!strcasecmp(cmdname, "shutdown.reboot"))
	{
//		ser_send(upsfd, "%s", SHUTDOWN);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stop"))
	{
//		ser_send(upsfd, "%c%s", DELAYED_SHUTDOWN_PREFIX, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start.quick"))
	{
		do_battery_test();
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start.deep"))
	{
//		ser_send(upsfd, "%s%s", TEST_BATT_DEEP, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	return STAT_INSTCMD_UNKNOWN;

}

int setcmd(const char* varname, const char* setvalue)
{
	upsdebugx(2, "In setcmd for %s with %s...", varname, setvalue);

	if (!strcasecmp(varname, "ups.delay.shutdown"))
	{
/* 		if ((strncmp (UpsFamily, FAMILY_OZ, FAMILY_SIZE) == 0) ||
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
 */	}

	if (!strcasecmp(varname, "input.transfer.low"))
	{
/* 		if (SetOutputAllow(setvalue, dstate_getinfo("input.transfer.high")))
		{
			return STAT_SET_UNKNOWN;
		}
		else
		{
			dstate_setinfo("input.transfer.low" , "%s", setvalue);
			return STAT_SET_HANDLED;
		}
 */
	}

	if (!strcasecmp(varname, "input.transfer.high"))
	{
/* 		if (SetOutputAllow(dstate_getinfo("input.transfer.low"), setvalue))
		{
			return STAT_SET_UNKNOWN;
		}
		else
		{
			dstate_setinfo("input.transfer.high" , "%s", setvalue);
			return STAT_SET_HANDLED;
		}
 */
	}

	if (!strcasecmp(varname, "battery.date"))
	{
/* 		if(strlen(setvalue) == GETX_DATE_RESP_SIZE)
		{
			ser_send(upsfd, "%s%s%s", SETX_BATTERY_DATE, setvalue, COMMAND_END);
			dstate_setinfo("battery.date", "%s (yymmdd)", setvalue);
			return STAT_SET_HANDLED;
		}
		else
		{
			return STAT_SET_UNKNOWN;
		}
 */	}

	if (!strcasecmp(varname, "ups.delay.start"))
	{
		if (atoi(setvalue) <= 9999)
		{
//			ser_send(upsfd,"%s%s%s", SETX_RESTART_DELAY, setvalue, COMMAND_END);

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
//			ser_send(upsfd,"%s%s%s", SETX_LOWBATT_AT, setvalue, COMMAND_END);

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
//			ser_send(upsfd,"%c0%s", SETX_AUTO_START, COMMAND_END);
			dstate_setinfo("ups.start.auto", "yes");
			return STAT_SET_HANDLED;
		}
		else if (!strcasecmp(setvalue, "no"))
		{
//			ser_send(upsfd,"%c1%s", SETX_AUTO_START, COMMAND_END);
			dstate_setinfo("ups.start.auto", "no");
			return STAT_SET_HANDLED;
		}

		return STAT_SET_UNKNOWN;
	}

	upslogx(LOG_NOTICE, "setcmd: unknown command [%s]", varname);

	return STAT_SET_UNKNOWN;
}

#endif	/* !NUT_POWERVAR_CX_H_SEEN */
