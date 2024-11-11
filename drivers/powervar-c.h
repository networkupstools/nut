/* powervar-c.h - Driver for Powervar UPS using CUSPP.
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
*/

#ifndef NUT_POWERVAR_C_H_SEEN
#define NUT_POWERVAR_C_H_SEEN 1

/*misc stuff*/
#define BUFFSIZE		512
#define SUBBUFFSIZE		48
#define STDREQSIZE		3

/* For use in some instant commands...if requested */
#define DEFAULT_BAT_TEST_TIME 	"10"	/* TST.BATRUN */
#define DEFAULT_DISP_TEST_TIME	"10"	/* TST.DISP */

#define ENDCHAR			'\r'
#define CHAR_EQ			'='
#define IGNCHARS 		"\n"
#define COMMAND_END 		"\r\n"
#define FORMAT_TAIL		".FORMAT"
#define DATADELIM		";"
#define FRMTDELIM		";."


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

#endif	/* NUT_POWERVAR_C_H_SEEN */
