/*vim ts=4*/
/* powervar_cx.c - Common items (code) for Powervar UPS CUSPP drivers.
 *
 * Copyright (C)
 *     2024, 2025 by Bill Elliot <bill@wreassoc.com>
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
 * - 7 February 2025, Bill Elliot
 * Working well with USB (...-cu) or serial (...-cs)
 * - 2 December 2024.  Bill Elliot
 * Breaking out common items for serial and USB drivers. This file is not
 *  intended to be compiled as a stand-alone file but is to be included
 *  into the serial or USB driver files that need it.
 *
 */

#include "config.h"
#include "common.h"
#include "powervar_cx.h"

#include "nut_stdint.h"
#include "dstate.h"

#if defined PVAR_SERIAL && PVAR_SERIAL
# include "serial.h"
#endif	/* PVAR_SERIAL */

/* Common CUSPP variables here */

char UpsFamily [SUBBUFFSIZE];		/* Hold family that was found */
char UpsProtVersion [SUBBUFFSIZE];	/* Hold protocol version string */

/* Dynamic CUSPP response information positions (0 = data not available) */
uint8_t byPIDProtPos = 0;
uint8_t byPIDVerPos = 0;
uint8_t byPIDUidPos = 0;
uint8_t byPIDBatPos = 0;
uint8_t byPIDInpPos = 0;
uint8_t byPIDOutPos = 0;
uint8_t byPIDAlmPos = 0;
uint8_t byPIDTstPos = 0;
uint8_t byPIDSetPos = 0;
uint8_t byPIDPduPos = 0;
uint8_t byPIDSysPos = 0;
uint8_t byPIDBtmPos = 0;
uint8_t byPIDOemPos = 0;
uint8_t byPIDBuzPos = 0;
uint8_t byPIDEvtPos = 0;

uint8_t byUIDManufPos = 0;
uint8_t byUIDModelPos = 0;
uint8_t byUIDSwverPos = 0;
uint8_t byUIDSernumPos = 0;
uint8_t byUIDFamilyPos = 0;
uint8_t byUIDMfgdtPos = 0;
uint8_t byUIDCSWVERPos = 0;

uint8_t byBATStatusPos = 0;
uint8_t byBATTmleftPos = 0;
uint8_t byBATEstcrgPos = 0;
uint8_t byBATVoltPos = 0;
uint8_t byBATTempPos = 0;
uint8_t byBATPVoltPos = 0;

uint8_t byINPStatusPos = 0;
uint8_t byINPFreqPos = 0;
uint8_t byINPVoltPos = 0;
uint8_t byINPAmpPos = 0;
uint8_t byINPMaxvltPos = 0;
uint8_t byINPMinvltPos = 0;

uint8_t byOUTSourcePos = 0;
uint8_t byOUTFreqPos = 0;
uint8_t byOUTVoltPos = 0;
uint8_t byOUTAmpPos = 0;
uint8_t byOUTPercntPos = 0;

uint8_t bySYSInvoltPos = 0;
uint8_t bySYSInfrqPos = 0;
uint8_t bySYSOutvltPos = 0;
uint8_t bySYSOutfrqPos = 0;
uint8_t bySYSBatdtePos = 0;
uint8_t bySYSOvrlodPos = 0;
uint8_t bySYSOutvaPos = 0;
uint8_t bySYSBtstdyPos = 0;

uint8_t bySETAudiblPos = 0;
uint8_t bySETAtosrtPos = 0;
uint8_t bySETOffnowPos = 0;
uint8_t bySETOffdlyPos = 0;
uint8_t bySETOffstpPos = 0;
uint8_t bySETSrtdlyPos = 0;
uint8_t bySETRstinpPos = 0;
uint8_t bySETRsttmpPos = 0;

uint8_t byALMOnbatPos = 0;
uint8_t byALMLowbatPos = 0;
uint8_t byALMBadbatPos = 0;
uint8_t byALMTempPos = 0;
uint8_t byALMOvrlodPos = 0;
uint8_t byALMTstbadPos = 0;
uint8_t byALMTestngPos = 0;
uint8_t byALMChngbtPos = 0;

uint8_t byTSTTimermPos = 0;
uint8_t byTSTAbortPos = 0;
uint8_t byTSTBatqckPos = 0;
uint8_t byTSTBatdepPos = 0;
uint8_t byTSTBatrunPos = 0;
uint8_t byTSTBtemtyPos = 0;
uint8_t byTSTDispPos = 0;

uint8_t byEVTUptimePos = 0;

/******************************************
 * Functions for getting strings from UPS *
 ******************************************/

#ifdef never
/* A data dumper...for debug use */
/* Just prints out contents of buffer but in hex representation then ASCII
 * NOTE: consider upsdebug_hex() instead
 */
void ShowStringHex (const char* pS)
{
	size_t Len = strlen(pS);
	uint32_t i = 0;

	/* Limit length to USB buffer size for now */
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

	printf (COMMAND_END);		/* Just finish the line */
}
#endif


/* Get the FORMAT and/or data for the requested CUSPP group.
   This is used during initialization to establish the substring position values
    and to set the initial NUT data. This function can exit/abort and should only
    be used during initialization. For updates use: GetUPSData function.
*/
void GetInitFormatAndOrData (const char* sReq, char* sF, const size_t sFSize, char* sD, const size_t sDSize)
{
	char sRequest[15];
	unsigned int SendTries = 0;

	upsdebugx (5, "In GetInitFormatAndOrData...");

	if (sFSize)
	{
		memset(sF, 0, sFSize);
		strcpy(sRequest, sReq);
		strcat(sRequest, FORMAT_TAIL);

		/* Get sReq.FORMAT response */
		upsdebugx (2, "Requesting '%s'", sRequest);

		do
		{
			SendRequest ((const char*)sRequest);

			if(PowervarGetResponse (sF, sFSize))
			{
				fatalx(EXIT_FAILURE, "'%s.FORMAT' timeout getting UPS data on %s\n", sReq, device_path);
			}

			if ((sF[0] == DONT_UNDERSTAND) || (strncmp(sReq, sF, STDREQSIZE) != 0))
			{
				if (sF[0] != DONT_UNDERSTAND)
				{
					upsdebugx (4, "[GetInitF] unexpected response: '%s'", sF);
				}

				sF[0] = 0;	/* Show invalid response */
			}
			else
			{
				break;		/* Valid response */
			}

		} while (++SendTries < 2);
	}
	else
	{
		upsdebugx (4, "Bypassed requesting '%s.FORMAT'", sReq);
	}

	if (sDSize)
	{
		memset(sD, 0, sDSize);

		/* Get sReq data */
		upsdebugx (2, "Requesting '%s' data", sReq);

		do
		{

			SendRequest((const char*)sReq);

			if(PowervarGetResponse (sD, sDSize))
			{
				fatalx(EXIT_FAILURE, "'%s' timeout getting UPS data on '%s'\n", sReq, device_path);
			}

			if ((sD[0] == '?') || (strncmp(sReq, sD, STDREQSIZE) != 0))
			{
				if (sD[0] != DONT_UNDERSTAND)
				{
					upsdebugx (4, "[GetInitD] unexpected response: '%s'", sD);
				}

				sD[0] = 0;	/* Show invalid response */
			}
			else
			{
				break;		/* Valid response */
			}
		} while (++SendTries < 2);
	}
	else
	{
		upsdebugx (4, "Bypassed requesting '%s' data", sReq);
	}

	upsdebugx (5, "Leaving GetInitFormatAndOrData...");
}


/* Get the requested data group from the UPS. This is used for NUT updates
    and must not exit or abort. As long as this driver is being called it
    must try to communicate with the UPS.
*/
uint8_t GetUPSData (char* sReq, char* sD, const size_t sDSize)
{
	uint8_t byReturn = 1;		/* Set up for good return, '0' is bad */
	unsigned int SendTries = 0;		/* Try sending request twice */

	memset(sD, 0, sDSize);

	/* Get sReq data */
	upsdebugx (2, "Requesting %s update", sReq);

	do
	{
		SendRequest((const char*)sReq);

		if((PowervarGetResponse (sD, sDSize) != 0) || (strncmp(sReq, sD, STDREQSIZE) != 0))
		{
			byReturn = 0;		/* Show invalid data */

			if (sD[0] != DONT_UNDERSTAND)
			{
				upsdebugx (3, "GetUPSData invalid response: '%s', %d", sD, strncmp(sReq, sD, STDREQSIZE));
			}
		}
		else
		{
			byReturn = 1;		/* Valid data */
			break;
		}

	} while (++SendTries < 2);

	return byReturn;
}

/***************************************************
 * Functions for handling commands sent to the UPS *
 ***************************************************/

/* This function is called to send commands to the initialized device. */
/* It then clears the USB hardware by pulling any response from the UPS */
static size_t SendCommand (const char* sCmd)
{
	size_t ret = 0;
	char chInBuff[REQBUFFSIZE];

	memset(chInBuff, 0, sizeof(chInBuff));

	ret = SendRequest(sCmd);

	PowervarGetResponse (chInBuff, sizeof(chInBuff));

	/* Anything we want to do with the response or return value?? */

	return ret;
}



/***************************************
 * CUSPP string handling functions     *
 ***************************************/

/* This function parses responses to pull the desired substring from the buffer.
 * SubPosition is normal counting (start with 1 not 0).
 * chDst will have just the substring requested or will be set to NULL if
 *  substring is not found.
 */

static unsigned int GetSubstringFromBuffer (char* chDst, const char* chSrc, const unsigned int SubPosition)
{
	unsigned int RetVal = 0;		/* Start with a bad return value */
	char WorkBuffer [BUFFSIZE];	/* For copy of passed in buffer */
	char* chWork;			/* Pointer into working buffer */
	char* chTok;			/* Pointer to token */
	unsigned int Pos;			/* Token position down-counter */

	if (SubPosition)	/* Don't accept a '0' request */
	{
		/* Make a local copy of the source string so strtok doesn't corrupt original. */
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
static unsigned int GetSubstringPosition (const char* chResponse, const char* chSub)
{
	unsigned int uiReturn = 0;		/* Substring position or 0 if not found */
	unsigned int uiPos = 1;			/* Substring position counter */
	unsigned int uiLen = 0;
	char WorkBuffer [BUFFSIZE];
	char* chSrc;
	char* chTok;			/* Individual tokens as they are found */

	/* Make a local copy of the source string so strtok doesn't corrupt original. */
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


/*****************************************
 * Common portions of standard functions *
 *****************************************/

void PvarCommon_Initinfo (void)
{
	int Vlts = 1;
	char sFBuff[BUFFSIZE];
	char sDBuff[BUFFSIZE];
	char SubBuff[SUBBUFFSIZE];
	char Msg[SUBBUFFSIZE + REQBUFFSIZE + 60];

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

	/* Get the rest of the PID substrings that are available.
	 * This may allow some logic flow control for segments of the code.
	 */
	byPIDUidPos = GetSubstringPosition (sFBuff, PID_UID_SUB);
	byPIDBatPos = GetSubstringPosition (sFBuff, PID_BAT_SUB);
	byPIDInpPos = GetSubstringPosition (sFBuff, PID_INP_SUB);
	byPIDOutPos = GetSubstringPosition (sFBuff, PID_OUT_SUB);
	byPIDAlmPos = GetSubstringPosition (sFBuff, PID_ALM_SUB);
	byPIDTstPos = GetSubstringPosition (sFBuff, PID_TST_SUB);
	byPIDSetPos = GetSubstringPosition (sFBuff, PID_SET_SUB);
	byPIDPduPos = GetSubstringPosition (sFBuff, PID_PDU_SUB);
	byPIDSysPos = GetSubstringPosition (sFBuff, PID_SYS_SUB);
	byPIDBtmPos = GetSubstringPosition (sFBuff, PID_BTM_SUB);
	byPIDOemPos = GetSubstringPosition (sFBuff, PID_OEM_SUB);
	byPIDBuzPos = GetSubstringPosition (sFBuff, PID_BUZ_SUB);
	byPIDEvtPos = GetSubstringPosition (sFBuff, PID_EVT_SUB);

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
		memcpy (UpsFamily, SubBuff, STDREQSIZE);	/* "GTS" or "UPM" */
		dstate_setinfo("device.description", "%s", UpsFamily);
		dstate_setinfo("ups.id", "%s", UpsFamily);
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

	/* Manufacture date (UPM only) */
	if (GetSubstringFromBuffer (SubBuff, sDBuff, byUIDMfgdtPos))
	{
		dstate_setinfo("ups.mfr.date", "%s (yyyymmdd)", SubBuff);
	}

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

		if(!byUIDMfgdtPos)
		{
			dstate_setinfo("ups.mfr.date", "%.3s (yyq)", (strrchr(SubBuff, '-') + 1));
		}
	}

	sprintf (Msg, "Found a Powervar '%s' UPS with serial number: '%s'", UpsFamily, SubBuff);
	upsdebugx (2, "%s", Msg);
	upslogx(LOG_INFO, "%s", Msg);

	/* Get BAT format and populate needed data string positions... */
	GetInitFormatAndOrData(BAT_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

	if(sFBuff[0])
	{
		byBATStatusPos = GetSubstringPosition (sFBuff, X_STATUS_SUB);
		byBATTmleftPos = GetSubstringPosition (sFBuff, BAT_TMLEFT_SUB);
		byBATEstcrgPos = GetSubstringPosition (sFBuff, BAT_ESTCRG_SUB);
		byBATVoltPos = GetSubstringPosition (sFBuff, X_VOLT_SUB);
		byBATTempPos = GetSubstringPosition (sFBuff, X_TEMP_SUB);
		byBATPVoltPos = GetSubstringPosition (sFBuff, BAT_PVOLT_SUB);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, byBATVoltPos))
	{
		Vlts = atoi(SubBuff);
		dstate_setinfo("battery.voltage.nominal", "%s", (Vlts < 36 ? (Vlts < 18 ? "12" : "24") : "48"));
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

	/* Keep the next two 'if's' together as they potentially share Vlts information */
	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSOutvltPos))
	{
		dstate_setinfo("output.voltage.nominal", "%s", SubBuff);
		Vlts = atoi(SubBuff);		/* Keep for nominal current calc */
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSOutvaPos))
	{
		dstate_setinfo("output.current.nominal", "%d", atoi(SubBuff)/Vlts);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSOutfrqPos))
	{
		dstate_setinfo("output.frequency.nominal", "%s", SubBuff);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSBtstdyPos))
	{
		dstate_setinfo("ups.test.interval", "%ld", atoi(SubBuff) * 86400L);
	}

	if (GetSubstringFromBuffer (SubBuff, sDBuff, bySYSBatdtePos))
	{
		dstate_setinfo("battery.date", "%s (yyyymm)", SubBuff);
		dstate_setflags("battery.date", ST_FLAG_STRING | ST_FLAG_RW);
		dstate_setaux("battery.date", GETX_DATE_RESP_SIZE);
	}

	if (byBATPVoltPos != 0)
	{
		dstate_setinfo("battery.type", "%s", "LFP");
	}
	else
	{
		/* For now, all others are lead acid. */
		dstate_setinfo("battery.type", "%s", "PbAc");
	}

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
		bySETOffnowPos = GetSubstringPosition (sFBuff, SET_OFFNOW_SUB);
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

		dstate_setinfo("ups.shutdown", "enabled");
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
	if(byPIDEvtPos)
	{
		GetInitFormatAndOrData(EVT_REQ, sFBuff, sizeof(sFBuff), sDBuff, sizeof(sDBuff));

		if(sFBuff[0])
		{
			byEVTUptimePos = GetSubstringPosition (sFBuff, EVT_UPTIME_SUB);
		}

		if (GetSubstringFromBuffer (SubBuff, sDBuff, byEVTUptimePos))
		{
			dstate_setinfo("device.uptime", "%s", SubBuff);
		}
	}

	/* Add NUT commands that are available to either GTS or UPM */
	if(byTSTBatqckPos)
	{
		dstate_addcmd("test.battery.start.quick");
	}

	if(byTSTBatdepPos)
	{
		dstate_addcmd("test.battery.start.deep");

		/* Failure test will use deep command */
		dstate_addcmd("test.failure.start");
		dstate_addcmd("test.failure.stop");
	}

	if(byTSTAbortPos)
	{
		dstate_addcmd("test.battery.stop");
	}

	if(bySETOffnowPos || bySETOffdlyPos || bySETOffstpPos)
	{
		dstate_addcmd("shutdown.return");
		dstate_addcmd("shutdown.stop");
		dstate_addcmd("shutdown.reboot");
		dstate_addcmd("load.off");
		dstate_addcmd("load.off.delay");
	}

	if(bySETAtosrtPos)
	{
		dstate_addcmd("shutdown.stayoff");
	}

	if(bySETAudiblPos)
	{
		dstate_addcmd("beeper.enable");
		dstate_addcmd("beeper.disable");
		dstate_addcmd("beeper.mute");
	}

	if(bySETSrtdlyPos)
	{
		dstate_addcmd("load.on");
		dstate_addcmd("load.on.delay");
	}

	if (bySETRstinpPos)
	{
		dstate_addcmd("reset.input.minmax");
	}

	if(byTSTDispPos)	/* UPM only */
	{
		dstate_addcmd("test.panel.start");
	}

	if (getval("offdelay") == NULL)
	{
		dstate_setinfo("ups.delay.shutdown", "1");
	}
	else
	{
		dstate_setinfo("ups.delay.shutdown", "%s", getval("offdelay"));
	}

	if (getval("startdelay") == NULL)
	{
		dstate_setinfo("ups.delay.start", "1");
	}
	else
	{
		dstate_setinfo("ups.delay.start", "%s", getval("startdelay"));
	}

	dstate_setflags("ups.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW);
	dstate_setaux("ups.delay.shutdown", GET_SHUTDOWN_RESP_SIZE);

	dstate_setflags("ups.delay.start", ST_FLAG_STRING | ST_FLAG_RW);
	dstate_setaux("ups.delay.start", GET_STARTDELAY_RESP_SIZE);

	upsh.setvar = setcmd;
	upsh.instcmd = instcmd;
}


void PvarCommon_Updateinfo (void)
{
	char sData[BUFFSIZE];
	char SubString[SUBBUFFSIZE];
	uint8_t byOnBat = 0;		    /* Keep flag between OUT and BAT groups */
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
	if (byPIDInpPos && GetUPSData (INP_DATA_REQ, sData, sizeof (sData)))
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
	if (byPIDBatPos && GetUPSData (BAT_REQ, sData, sizeof (sData)))
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
	if (byPIDSetPos && GetUPSData (SET_REQ, sData, sizeof (sData)))
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
	if (byPIDAlmPos && GetUPSData (ALM_REQ, sData, sizeof (sData)))
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

	/* Get EVT data next... */
	if (byPIDEvtPos && GetUPSData (EVT_REQ, sData, sizeof (sData)))
	{
		if(GetSubstringFromBuffer (SubString, sData, byEVTUptimePos))
		{
			dstate_setinfo("device.uptime", "%s", SubString);
		}
	}


	alarm_commit();
	status_commit();

	dstate_dataok();
}


/* Support functions for Powervar UPS drivers */
static void HandleBeeper (char* chS)
{
	char chBuff[18];

	memset(chBuff, 0, sizeof(chBuff));
	sprintf(chBuff, "%s%s", SET_AUDIBL_REQ, chS);

	SendCommand (chBuff);
}

static void HandleOffDelay (void)
{
	char chOutBuff[32];
	int delay = atoi(dstate_getinfo("ups.delay.shutdown"));

	memset(chOutBuff, 0, sizeof(chOutBuff));

	if (0 == delay)
	{
		delay++;		/* Make it '1' */
	}

	sprintf(chOutBuff, "%s%d", SET_OFFDLY_REQ, delay);

	SendCommand (chOutBuff);
}

static void HandleOnDelay (void)
{
	char chBuff[32];

	memset(chBuff, 0, sizeof(chBuff));

	if (dstate_getinfo("ups.delay.start") == NULL)
	{
		sprintf(chBuff,"%s1", SET_SRTDLY_REQ);
	}
	else
	{
		sprintf(chBuff,"%s%s", SET_SRTDLY_REQ, dstate_getinfo("ups.delay.start"));
	}

	SendCommand (chBuff);
}


/* Functions called by NUT that impact or use the Powervar UPS driver */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "startdelay", "Change start delay time from the 1 second default (1-65535).");

	addvar(VAR_VALUE, "offdelay", "Change shutdown delay time from 0 second default (1-65535).");

	addvar(VAR_VALUE, "disptesttime", "Change display test time from the 10 second default (UPM only, 11-255).");

#ifdef PVAR_SERIAL
	addvar(VAR_VALUE, "pvbaud", "(UPM dev only) *Possibly* use a baud rate other than the default of 9600.");
#endif
}

void upsdrv_shutdown(void)
{
	HandleOffDelay ();
}


int instcmd(const char *cmdname, const char *extra)
{

	upsdebugx(2, "In instcmd with %s and extra %s.", cmdname, extra);

	if (!strcasecmp(cmdname, "test.battery.start.quick"))
	{
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		SendCommand (TST_BATQCK_REQ);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start.deep"))
	{
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		SendCommand (TST_BATDEP_REQ);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.stop"))
	{
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		SendCommand (TST_ABORT_REQ);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.failure.start"))
	{
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		SendCommand (TST_BATDEP_REQ);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.failure.stop"))
	{
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		SendCommand (TST_ABORT_REQ);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stop"))
	{
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		SendCommand (SET_OFFSTP_REQ);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "beeper.enable"))
	{
		HandleBeeper (BEEPENABLE);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "beeper.disable"))
	{
		HandleBeeper (BEEPDISABLE);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "beeper.mute"))
	{
		HandleBeeper (BEEPMUTE);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "load.off"))
	{
		SendCommand (SET_OFFNOW_REQ);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "load.off.delay"))
	{
		HandleOffDelay ();
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "load.on"))
	{
		SendCommand (SET_SRTNOW_REQ);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "load.on.delay"))
	{
		HandleOnDelay();
		return STAT_INSTCMD_HANDLED;
	}


	if (!strcasecmp(cmdname, "shutdown.return") ||
	    !strcasecmp(cmdname, "shutdown.reboot"))
	{
		upslog_INSTCMD_POWERSTATE_CHANGE(cmdname, extra);
		SendCommand (SET_ATOSRT1_REQ);
		HandleOffDelay ();
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stayoff"))
	{
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		SendCommand (SET_ATOSRT0_REQ);
		HandleOffDelay ();
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stop"))
	{
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		SendCommand (SET_OFFSTP_REQ);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "reset.input.minmax"))
	{
		SendCommand (SET_RSTINP_REQ);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.panel.start"))
	{
		char chBuff[REQBUFFSIZE];
		unsigned int ShowTime = 10;

		if (getval("disptesttime") != NULL)
		{
			ShowTime = atoi(getval("disptesttime"));
		}

		memset(chBuff, 0, sizeof(chBuff));

		sprintf(chBuff, "%s%d", TST_DISP_REQ, ShowTime);
		SendCommand (chBuff);
	}

	return STAT_INSTCMD_UNKNOWN;
}

int setcmd(const char* varname, const char* setvalue)
{
	upsdebugx(2, "In setcmd for %s with %s...", varname, setvalue);

	if (!strcasecmp(varname, "ups.delay.shutdown"))
	{
		if (atoi(setvalue) > 65535)
		{
			upsdebugx(2, "Too big (>65535)...(%s)", setvalue);
			return STAT_SET_UNKNOWN;
		}

		dstate_setinfo("ups.delay.shutdown", "%s", setvalue);
		return STAT_SET_HANDLED;
	}

	if (!strcasecmp(varname, "ups.delay.start"))
	{
		if (atoi(setvalue) > 65535)
		{
			upsdebugx(2, "Too big (>65535)...(%s)", setvalue);
			return STAT_SET_UNKNOWN;
		}

		dstate_setinfo("ups.delay.start", "%s", setvalue);
		return STAT_SET_HANDLED;
	}

	if (!strcasecmp(varname, "battery.date"))
	{
		char chBuff[SUBBUFFSIZE];

		if(strlen(setvalue) == GETX_DATE_RESP_SIZE)
		{
			memset (chBuff, 0, sizeof(chBuff));
			strcpy (chBuff, SYS_BATDTE_CMD);
			strcat (chBuff, setvalue);
			SendRequest(chBuff);

			dstate_setinfo("battery.date", "%s (yyyymm)", setvalue);
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
			SendRequest(SET_ATOSRT1_REQ);
			dstate_setinfo("ups.start.auto", "yes");
			return STAT_SET_HANDLED;
		}
		else if (!strcasecmp(setvalue, "no"))
		{
			SendRequest(SET_ATOSRT0_REQ);
			dstate_setinfo("ups.start.auto", "no");
			return STAT_SET_HANDLED;
		}

		return STAT_SET_UNKNOWN;
	}

	upslogx(LOG_NOTICE, "setcmd: unknown command [%s]", varname);

	return STAT_SET_UNKNOWN;
}

/* End of powervar_cx.c file */
