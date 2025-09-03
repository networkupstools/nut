/*vim ts=4*/
/* powervar_cx.h - Common items for Powervar UPS CUSPP drivers.
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
 * - 24 June 2025
 * Common file structure improved by Jim K.
 * Driver file names changed to powervar_cx_ser and powervar_cx_usb.
 * - 7 February 2025, Bill Elliot
 * Working well with USB (...-cu) or serial (...-cs)
 * - 2 December 2024.  Bill Elliot
 * Breaking out common items for serial and USB drivers. This file is not
 *  intended to be compiled as a stand-alone file but is to be included
 *  into the serial or USB driver files that need it.
 *
 */

#ifndef NUT_POWERVAR_CX_H_SEEN
#define NUT_POWERVAR_CX_H_SEEN 1

#include "nut_stdint.h"

/* Prototypes for calls that may be prior to function definition */
void GetInitFormatAndOrData (const char* sReq, char* sF, const size_t sFSize, char* sD, const size_t sDSize);
uint8_t GetUPSData (char* sReq, char* sD, const size_t sDSize);
int instcmd(const char *cmdname, const char *extra);
int setcmd(const char* varname, const char* setvalue);

size_t GetSubstringPosition (const char* chResponse, const char* chSub);

void PvarCommon_Initinfo(void);
void PvarCommon_Updateinfo(void);

/* Calls defined in Ser/USB implementations */
size_t SendRequest (const char* sRequest);
ssize_t PowervarGetResponse (char* chBuff, const size_t BuffSize);

/* Common CUSPP stuff here */

/*misc stuff*/
#define BUFFSIZE		512
#define SUBBUFFSIZE		48
#define REQBUFFSIZE		40
#define STDREQSIZE		3	/* Length of all primary requests */

/* For use in some instant commands...if requested */
#define BEEPENABLE		"1"
#define BEEPDISABLE		"0"
#define BEEPMUTE		"2"
#define GET_SHUTDOWN_RESP_SIZE		5	/* Max chars for variable */
#define GET_STARTDELAY_RESP_SIZE	5	/* Max chars for variable */
#define GETX_DATE_RESP_SIZE		6	/* Max chars for variable */

#define IGNCHARS		"\n"
#define ENDCHAR			'\r'
#define COMMAND_END		"\r\n"
#define CHAR_EQ			'='
#define FORMAT_TAIL		".FORMAT"
#define DATADELIM		";\r"
#define FRMTDELIM		";.\r"
#define DONT_UNDERSTAND		'?'

/* Information requests -- CUSPP */

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
#define PID_UID_SUB		"UID"
#define PID_BAT_SUB		"BAT"
#define PID_INP_SUB		"INP"
#define PID_OUT_SUB		"OUT"
#define PID_ALM_SUB		"ALM"
#define PID_TST_SUB		"TST"
#define PID_SET_SUB		"SET"
#define PID_PDU_SUB		"PDU"
#define PID_SYS_SUB		"SYS"
#define PID_BTM_SUB		"BTM"
#define PID_OEM_SUB		"OEM"
#define PID_BUZ_SUB		"BUZ"
#define PID_EVT_SUB		"EVT"

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
#define BAT_PVOLT_SUB		"PVOLT"

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
#define SET_OFFNOW_SUB		"OFFNOW"
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
#define TST_ABORT_REQ		"TST.ABORT.W=1"
#define TST_DISP_REQ		"TST.DISP.W="

#define SET_OFFNOW_REQ		"SET.OFFNOW.W=1"
#define SET_OFFDLY_REQ		"SET.OFFDLY.W="
#define SET_SRTNOW_REQ		"SET.SRTDLY.W=1"
#define SET_SRTDLY_REQ		"SET.SRTDLY.W="
#define SET_OFFON_REQ		"SET.OFFON.W="
#define SET_OFFSTP_REQ		"SET.OFFSTP.W=1"
#define SET_AUDIBL_REQ		"SET.AUDIBL.W="
#define SET_ATOSRT0_REQ		"SET.ATOSRT.W=0"
#define SET_ATOSRT1_REQ		"SET.ATOSRT.W=1"
#define SET_RSTINP_REQ		"SET.RSTINP.W=1"

#define SYS_BATDTE_CMD		"SYS.BATDTE.W="

/* Common CUSPP variables here */

extern char UpsFamily [SUBBUFFSIZE];		/* Hold family that was found */
extern char UpsProtVersion [SUBBUFFSIZE];	/* Hold protocol version string */

/* Dynamic CUSPP response information positions (0 = data not available) */
extern uint8_t byPIDProtPos;
extern uint8_t byPIDVerPos;
extern uint8_t byPIDUidPos;
extern uint8_t byPIDBatPos;
extern uint8_t byPIDInpPos;
extern uint8_t byPIDOutPos;
extern uint8_t byPIDAlmPos;
extern uint8_t byPIDTstPos;
extern uint8_t byPIDSetPos;
extern uint8_t byPIDPduPos;
extern uint8_t byPIDSysPos;
extern uint8_t byPIDBtmPos;
extern uint8_t byPIDOemPos;
extern uint8_t byPIDBuzPos;
extern uint8_t byPIDEvtPos;

extern uint8_t byUIDManufPos;
extern uint8_t byUIDModelPos;
extern uint8_t byUIDSwverPos;
extern uint8_t byUIDSernumPos;
extern uint8_t byUIDFamilyPos;
extern uint8_t byUIDMfgdtPos;
extern uint8_t byUIDCSWVERPos;

extern uint8_t byBATStatusPos;
extern uint8_t byBATTmleftPos;
extern uint8_t byBATEstcrgPos;
extern uint8_t byBATVoltPos;
extern uint8_t byBATTempPos;
extern uint8_t byBATPVoltPos;

extern uint8_t byINPStatusPos;
extern uint8_t byINPFreqPos;
extern uint8_t byINPVoltPos;
extern uint8_t byINPAmpPos;
extern uint8_t byINPMaxvltPos;
extern uint8_t byINPMinvltPos;

extern uint8_t byOUTSourcePos;
extern uint8_t byOUTFreqPos;
extern uint8_t byOUTVoltPos;
extern uint8_t byOUTAmpPos;
extern uint8_t byOUTPercntPos;

extern uint8_t bySYSInvoltPos;
extern uint8_t bySYSInfrqPos;
extern uint8_t bySYSOutvltPos;
extern uint8_t bySYSOutfrqPos;
extern uint8_t bySYSBatdtePos;
extern uint8_t bySYSOvrlodPos;
extern uint8_t bySYSOutvaPos;
extern uint8_t bySYSBtstdyPos;

extern uint8_t bySETAudiblPos;
extern uint8_t bySETAtosrtPos;
extern uint8_t bySETOffnowPos;
extern uint8_t bySETOffdlyPos;
extern uint8_t bySETOffstpPos;
extern uint8_t bySETSrtdlyPos;
extern uint8_t bySETRstinpPos;
extern uint8_t bySETRsttmpPos;

extern uint8_t byALMOnbatPos;
extern uint8_t byALMLowbatPos;
extern uint8_t byALMBadbatPos;
extern uint8_t byALMTempPos;
extern uint8_t byALMOvrlodPos;
extern uint8_t byALMTstbadPos;
extern uint8_t byALMTestngPos;
extern uint8_t byALMChngbtPos;

extern uint8_t byTSTTimermPos;
extern uint8_t byTSTAbortPos;
extern uint8_t byTSTBatqckPos;
extern uint8_t byTSTBatdepPos;
extern uint8_t byTSTBatrunPos;
extern uint8_t byTSTBtemtyPos;
extern uint8_t byTSTDispPos;

extern uint8_t byEVTUptimePos;

#endif	/* !NUT_POWERVAR_CX_H_SEEN */
