/* oneac.h - Driver for Oneac UPS using the Advanced Interface.
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
*/

#ifndef NUT_ONEAC_H_SEEN
#define NUT_ONEAC_H_SEEN 1

/*misc stuff*/
#define ENDCHAR '\r'
#define IGNCHARS "\n"
#define COMMAND_END "\r\n"
#define DEFAULT_BAT_TEST_TIME "02"

/*Information requests -- EG level */

#define GET_ALL 	'%'
#define	GETALL_EG_RESP_SIZE	22
#define	GETALL_RESP_SIZE	48

#define GET_MFR				'M'
#define GET_FAMILY			'F'
#define GET_VERSION			'N'
#define GET_ON_INVERTER		'G'
#define GET_BATLOW			'K'
#define GET_STATUS			'X'
#define GET_LAST_XFER		'W'
#define GET_INVERTER_RDY	'I'
#define	GET_SHUTDOWN		'O'
#define GET_TEST_TIME		'Q'
#define GET_NOM_FREQ		'H'

/*Information requests -- ON level (EG plus these) */

#define GET_NOM_VOLTAGE 	'V'
#define GET_DISPLAY_CODE	'D'
#define	GET_CONDITION_CODE	'C'
#define GET_PERCENT_LOAD	'P'
#define GET_PERCENT_BAT_REM	'T'
#define	GET_INPUT_LINE_VOLT	'L'
#define GET_MIN_INPUT_VOLT	'A'
#define GET_MAX_INPUT_VOLT	'E'
#define	GET_OUTPUT_VOLT		'S'
#define GET_BOOSTING		'B'

#define	GET_SHUTDOWN_RESP_SIZE	3

/*Information requests -- OZ/OB level (ON plus these) */

#define GETX_ALL_1 	'&'
#define	GETX_ALL1_RESP_SIZE	79

#define	GETX_OUTSOURCE		'a'
#define GETX_FRONTDISP		'b'
#define GETX_INT_TEMP		'g'		/* Degrees C */
#define GETX_BATT_STAT		'h'		/* Unknown(1), Normal, Low, Depleted */
#define GETX_BATT_CHG_PERC	'i'		/* 0 - 100 */
#define GETX_EST_MIN_REM	'j'
#define GETX_ONBATT_TIME	'k'		/* In seconds */
#define GETX_BATT_VOLTS		'l'		/* Read as xxx.x */
#define GETX_INP_FREQ		'p'
#define GETX_MIN_IN_VOLTS	'q'
#define GETX_MAX_IN_VOLTS	'r'
#define GETX_IN_VOLTS		's'
#define GETX_IN_WATTS		'u'
#define GETX_OUT_VOLTS		'v'
#define GETX_OUT_WATTS		'x'
#define GETX_OUT_LOAD_PERC	'y'
#define GETX_OUT_FREQ		'z'

#define GETX_ALL_2	'^'
#define	GETX_ALL2_RESP_SIZE	92

#define GETX_MODEL			'J'
#define GETX_FW_REV			'U'		/* Read as xx.x */
#define GETX_SERIAL_NUM		'Y'

#define GETX_MAN_DATE		'$'		/* yymmdd */
#define GETX_BATT_REPLACED	'+'		/* yymmdd */
#define	GETX_DATE_RESP_SIZE	6

/* FIXME: Both of the following constants are unused, and the first is not
 * valid C syntax (breaks LLVM). */
#if 0
#define GETX_UNIT_KVA		''''	/* Read as xxx.xx */
#define GETX_UNIT_WATTS		"''"	/* 2-character string request */
#endif
#define GETX_LOW_OUT_ALLOW	'['		/* Tap up or inverter at this point */
#define GETX_HI_OUT_ALLOW	']'		/* Tap down or inverter at this point */
#define GETX_NOTIFY_DELAY	','		/* Secs of delay for power fail alert */
#define	GETX_ALLOW_RESP_SIZE	3

#define GETX_LOW_BATT_TIME	'"'		/* Low batt alarm at xx minutes */

#define	GETX_RESTART_DLY	'_'		/* Restart delay */
#define	GETX_RESTART_COUNT	"_?"	/* Returns actual counter value */
#define	GETX_RSTRT_RESP_SIZE	4

/*Other requests */
#define	GETX_SHUTDOWN		'}'			/* Shutdown counter (..... for none) */
#define	GETX_SHUTDOWN_RESP_SIZE	5

#define GETX_BATT_TEST_DAYS	"\x02\x1A"	/* Days between battery tests */
#define	GETX_BUZZER_WHAT	"\x07?"		/* What is buzzer state */
#define	GETX_AUTO_START		"<?"		/* Restart type */

#define	GETX_ALLOW_RANGE	"[=?"		/* Responds with min,max,spread */
#define	GETX_RANGE_RESP_SIZE	10

/*Control functions (All levels) */
#define	SIM_PWR_FAIL	"\x02\x15"	/*^B^U   15 second battery test*/
#define SHUTDOWN		"\x0f\x06"	/*^O^F   (a letter O)*/
#define RESET_MIN_MAX	'R'
#define BAT_TEST_PREFIX	"\x02"		/*^B needs 2 more chars. minutes*/
#define DELAYED_SHUTDOWN_PREFIX	'Z'	/* EG/ON needs 3 more chars.  seconds */
									/* ON96 needs 1 to 5 chars. of seconds */

/*Control functions (ON96) */
#define TEST_INDICATORS	"\x09\x14"	/*^I^T flashed LEDs and beeper */
#define	TEST_BATT_DEEP	"\x02\x04"	/*^B^D runs until low batt */
#define	TEST_BATT_DEAD	"\x02\x12"	/*^B^R run until battery dead */
#define	TEST_ABORT			'\x01'	/* Abort any running test */
#define	REBOOT_LOAD			"\x12@"	/*^R@xxx needs 3 chars of secs */
#define	SETX_BUZZER_PREFIX	'\x07'	/*^G needs one more character */
#define	SETX_OUT_ALLOW		"[="	/* [=lll,hhh */
#define	SETX_BUZZER_OFF		"\x070"	/*^G0 disables buzzer */
#define	SETX_BUZZER_ON		"\x071"	/*^G1 enables buzzer */
#define	SETX_BUZZER_MUTE	"\x072"		/*^G2 mutes current conditions */
#define SETX_BATT_TEST_DAYS	"\x02\x1A="	/* Needs 0 - 129 days, 0 is disable */
#define	SETX_LOWBATT_AT		"\"="		/* Low batt at (max 99) */
#define	SETX_RESTART_DELAY	"_="		/* _=xxxx, up to 9999 seconds */
#define	SETX_AUTO_START		'<'		/* <0 / <1 for auto / manual */
#define	SETX_BATTERY_DATE	"+="	/* Set battery replace date */

#define DONT_UNDERSTAND '*'
#define CANT_COMPLY		'#'
#define NO_VALUE_YET 	'.'
#define HIGH_COUNT		'+'		/* Shutdown counter > 999 on OZ */

#define MIN_ALLOW_FW	"1.9"	/* At or above provides output allow range */

/*responses*/
#define FAMILY_EG		"EG"	/* 3 tri-color LEDs and big ON/OFF on front */
#define FAMILY_ON		"ON"	/* Serial port avail only on interface card */
#define FAMILY_OZ		"OZ"	/* DB-25 std., plus interface slot */
#define	FAMILY_OB		"OB"	/* Lg. cab with removable modules */
#define	FAMILY_SIZE		2
#define YES				'Y'
#define NO				'N'
#define V230AC			'2'
#define V120AC			'1'
#define XFER_BLACKOUT	'B'
#define XFER_LOW_VOLT	'L'
#define XFER_HI_VOLT	'H'
#define	BUZZER_ENABLED	'1'
#define BUZZER_DISABLED	'0'
#define	BUZZER_MUTED	'2'

/*front panel alarm codes*/
#define CODE_BREAKER_OPEN	"c1"	/*input circuit breaker open*/
#define CODE_BAT_FUSE_OPEN	"c2"	/*battery not connected. Open fuse?*/
#define CODE_TOO_HOT		"c3"	/*UPS too hot*/
#define CODE_CHARGING		"c4"	/*recharging battery pack*/
#define CODE_LOW_BAT_CAP	"c5"	/*batteries getting too old*/
#define CODE_OVERLOAD		"c8"	/*"slight" overload*/
#define CODE_GROSS_OVLE		"c9"	/*gross overload 1 minute to power off*/
#define CODE_CHRGR_FUSE_OPEN "u1"	/*battery charger fuse probably open*/

#endif	/* NUT_ONEAC_H_SEEN */
