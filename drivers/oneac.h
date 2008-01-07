/*
Copyright (C) 2002 Eric Lawson <elawson@inficad.com>

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

/*misc stuff*/
#define ENDCHAR '\r'
#define IGNCHARS "\n"
#define COMMAND_END "\r\n"
#define DEFAULT_BAT_TEST_TIME "02"

/*Information requests*/

#define GET_ALL '%'
#define GET_ALL_EXT_2 '^'
#define GET_ALL_EXT_1 '&'
#define GET_MFR	'M'
#define GET_FAMILY	'F'
#define GET_VERSION	'N'
#define GET_ON_INVERTER	'G'
#define GET_BATLOW	'K'
#define GET_STATUS	'X'
#define GET_LAST_XFER	'W'
#define GET_INVERTER_RDY	'I'
#define GET_TEST_TIME	'Q'
#define GET_NOM_FREQ	'H'
#define GET_NOM_VOLTAGE 'V'
#define GET_DISPLAY_CODE	'D'
#define	GET_CONDITION_CODE	'C'
#define GET_PERCENT_LOAD	'P'
#define GET_PERCENT_BAT_REM	'T'
#define	GET_INPUT_LINE_VOLT	'L'
#define GET_MIN_INPUT_VOLT	'A'
#define GET_MAX_INPUT_VOLT	'E'
#define	GET_OUTPUT_VOLT	'S'
#define GET_BOOSTING	'B'

/*Control functions*/
#define	SIM_PWR_FAIL	"\x02\x15"		/*^B^U   15 second battery test*/
#define SHUTDOWN	"\x0f\x06"			/*^O^F   (a letter O)*/
#define RESET_MIN_MAX	'R'
#define BAT_TEST_PREFIX	"\x02"		/*needs 2 more chars. minutes*/
#define DELAYED_SHUTDOWN_PREFIX	'Z'	/*needs 3 more chars.  seconds */

#define DONT_UNDERSTAND '*'
#define CANT_COMPLY	'#'
#define NO_VALUE_YET '.'


/*responses*/
#define MFGR "ONEAC"
#define FAMILY_ON	"ON"
#define FAMILY_ON_EXT	"OZ"
#define FAMILY_EG	"EG"
#define YES	'Y'
#define NO 'N'
#define NORMAL '@'
#define ON_BAT_LOW_LINE 'A'
#define ON_BAT_HI_LINE 'Q'
#define LO_BAT_LOW_LINE 'C'
#define LO_BAT_HI_LINE 'S'
#define TOO_HOT '`'
#define FIX_ME 'D'
#define BAD_BAT 'H'
#define V230AC '2'
#define V120AC '1'
#define XFER_BLACKOUT 'B'
#define XFER_LOW_VOLT 'L'
#define XFER_HI_VOLT 'H' 

/*front panel alarm codes*/
#define CODE_BREAKER_OPEN "c1"		/*input circuit breaker open*/
#define CODE_BAT_FUSE_OPEN "c2"		/*battery not connected. Open fuse?*/
#define CODE_TOO_HOT "c3"			/*UPS too hot*/
#define CODE_CHARGING "c4"			/*recharging battery pack*/
#define CODE_LOW_BAT_CAP "c5"		/*batteries getting too old*/
#define CODE_OVERLOAD "c8"			/*"slight" overload*/
#define CODE_GROSS_OVLE "c9"		/*gross overload 1 minute to power off*/
#define CODE_CHRGR_FUSE_OPEN "u1"	/*battery charger fuse probably open*/
