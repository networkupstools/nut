/* apcsmart.h - command table for APC smart protocol units

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
             (C) 2000  Nigel Metheringham <Nigel.Metheringham@Intechnology.co.uk>
             (C) 2011  Michal Soltys <soltys@ziu.info>

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

#include <ctype.h>
#include <sys/ioctl.h>
#include "serial.h"
#include "timehead.h"

#define APC_TABLE_VERSION	"version 2.3"

/*
 * alerts and other stuff for quick reference:
 *
 * $ OL
 * ! OB
 * % LB
 * + not LB
 * # RB
 * ? OVER
 * = not OVER
 * * powering down now (only older models ?), handled by upsread()
 * 	otherwise ignored (it doesn't have to be in ignore sets)
 *
 * | eeprom change
 * & check alarm register for fail
 * ~ ???
 */

/*
 * old ones for reference:
 * 	#define IGNCHARS "\015+$|!~%?=#&"
 * 	#define POLL_IGNORE "\015&|"
 *	#define POLL_ALERT "$!%+#?="
 *	#define MINIGNCHARS "\015+$|!"
 * notice ~ that was present in IGNCHARS, but not in POLL_IGNORE - this kinda
 * didn't make sense (?); new versions doesn't filter ~, but keep that in mind
 * in case something obscure surfaces
 * due to switch to ICANON tty mode, we removed \015 from ignored characters,
 * as it's handled by IGNCR at read() level
 */

/* Basic UPS reply line structure */
#define ENDCHAR 10		/* APC ends responses with LF */

/* what to ignore during alert aware serial reads */
#define IGN_AACHARS "|&"

/* what alert_handler() should care about */
#define ALERT_CHARS "$!%+#?="

/* characters ignored by alertless reads */
#define IGN_CHARS IGN_AACHARS ALERT_CHARS

/*
 * these ones are used only during capability read, due to ^Z sending certain
 * characters such as #; it seems it could be equal to just IGN_CHARS w/o #
 */
#define IGN_CCCHARS "|$!+"	/* capability check ignore set */

#define UPSDELAY	  50000	/* slow down multicharacter commands        */
#define CMDLONGDELAY	1500000	/* some commands need a 1.5s gap for safety */

/* dangerous instant commands must be reconfirmed within a 12 second window */
#define MINCMDTIME	3
#define MAXCMDTIME	15

/* it only does two strings, and they're both the same length */
#define APC_STRLEN	8

/* how upsread() should behave */
#define SER_AL  0x01		/* run with alarm handler */
#define SER_TO  0x02		/* allow timeout without error */
#define SER_CC  0x04		/* prepare for capability check (^Z) processing */
#define SER_SD  0x08		/* prepare for shutdown command processing */
#define SER_AX  0x10		/* prepare for '*' handling */

/*
 * Some cmd codes to ignore (nut doesn't expose those, though the driver might
 * use them internally (e.g. [a]). If you decide to support them at some
 * point, remember about removing them from here !
 */
#define APC_UNR_CMDS "\032\177')-8QRYayz"

/* --------------- */

/* status bits */

#define APC_STAT_CAL	1	/* calibration */
#define APC_STAT_TRIM	2	/* SmartTrim */
#define APC_STAT_BOOST	4	/* SmartBoost */
#define APC_STAT_OL	8	/* on line */
#define APC_STAT_OB	16	/* on battery */
#define APC_STAT_OVER	32	/* overload */
#define APC_STAT_LB	64	/* low battery */
#define APC_STAT_RB	128	/* replace battery */

/*
 * serial protocol: special commands - initialization and such
 * these are not exposed as instant commands
 */
#define APC_STATUS	'Q'
#define APC_GOSMART	'Y'
#define APC_GODUMB	'R'
#define APC_CMDSET	'a'
#define APC_CAPS	'\032'	/* ^Z */
#define APC_NEXTVAL	'-'
#define APC_FW_OLD	'V'
#define APC_FW_NEW	'b'

/* --------------- */

/* Driver command table flag values */

#define APC_POLL	0x0001	/* Poll this variable regularly		*/
#define APC_PRESENT	0x0002	/* Capability seen on this UPS		*/

#define APC_RW		0x0004	/* read-write variable			*/
#define APC_ENUM	0x0008	/* enumerated type			*/
#define APC_STRING	0x0010	/* string				*/
#define APC_MULTI	0x0020	/* there're other vars like that	*/
#define APC_DEPR	0x0040	/* deprecated				*/

#define APC_NASTY	0x0100	/* Nasty command - must be reconfirmed	*/
#define APC_REPEAT	0x0200	/* Command needs sending twice		*/

#define APC_F_MASK	0xFF0000 /* Mask for apc data formats		*/
#define APC_F_PERCENT	0x020000 /* Data in a percent format		*/
#define APC_F_VOLT	0x030000 /* Data in a voltage format		*/
#define APC_F_AMP	0x040000 /* Data in a current/amp format	*/
#define APC_F_CELSIUS	0x050000 /* Data in a temp/C format		*/
#define APC_F_HEX	0x060000 /* Data in a hex number format		*/
#define APC_F_DEC	0x070000 /* Data in a decimal format		*/
#define APC_F_SECONDS	0x100000 /* Time in seconds			*/
#define APC_F_MINUTES	0x110000 /* Time in minutes			*/
#define APC_F_HOURS	0x120000 /* Time in hours			*/
#define APC_F_REASON	0x130000 /* Reason of transfer			*/
#define APC_F_LEAVE	0x000000 /* Just pass this through		*/

typedef struct {
	const char	*name;		/* the variable name	*/
	char		cmd;		/* variable character	*/
	unsigned int	flags;	 	/* various flags	*/
} apc_vartab_t;

/*
 * APC_MULTI variables *must* be listed in order of preference
 */
apc_vartab_t apc_vartab[] = {

	{ "ups.temperature",		'C',	APC_POLL|APC_F_CELSIUS },
	{ "ups.load",			'P',	APC_POLL|APC_F_PERCENT },
	{ "ups.test.interval",		'E',	APC_F_HOURS },
	{ "ups.test.result",		'X',	APC_POLL },
	{ "ups.delay.start",		'r',	APC_F_SECONDS },
	{ "ups.delay.shutdown",		'p',	APC_F_SECONDS },
	{ "ups.id",			'c',	APC_STRING },
	{ "ups.contacts",		'i',	APC_POLL|APC_F_HEX },
	{ "ups.display.language",	'\014',	0 },
	{ "input.voltage",		'L',	APC_POLL|APC_F_VOLT },
	{ "input.frequency",		'F',	APC_POLL|APC_F_DEC },
	{ "input.sensitivity",		's',	0 },
	{ "input.quality",		'9',	APC_POLL|APC_F_HEX },
	{ "input.transfer.low",		'l',	APC_F_VOLT },
	{ "input.transfer.high",	'u',	APC_F_VOLT },
	{ "input.transfer.reason",	'G',	APC_POLL|APC_F_REASON },
	{ "input.voltage.maximum",	'M',	APC_POLL|APC_F_VOLT },
	{ "input.voltage.minimum",	'N',	APC_POLL|APC_F_VOLT },
	{ "output.current",		'/',	APC_POLL|APC_F_AMP },
	{ "output.voltage",		'O',	APC_POLL|APC_F_VOLT },
	{ "output.voltage.nominal",	'o',	APC_F_VOLT },
	{ "ambient.humidity",		'h',	APC_POLL|APC_F_PERCENT },
	{ "ambient.humidity.high",	'{',	APC_F_PERCENT },
	{ "ambient.humidity.low",	'}',	APC_F_PERCENT },
	{ "ambient.temperature",	't',	APC_POLL|APC_F_CELSIUS },
	{ "ambient.temperature.high",	'[',	APC_F_CELSIUS },
	{ "ambient.temperature.low",	']',	APC_F_CELSIUS },
	{ "battery.date",		'x',	APC_STRING },
	{ "battery.charge",		'f',	APC_POLL|APC_F_PERCENT },
	{ "battery.charge.restart",	'e',	APC_F_PERCENT },
	{ "battery.voltage",		'B',	APC_POLL|APC_F_VOLT },
	{ "battery.voltage.nominal",	'g',	0 },
	{ "battery.runtime",		'j',	APC_POLL|APC_F_MINUTES },
	{ "battery.runtime.low",	'q',	APC_F_MINUTES },
	{ "battery.packs",		'>',	APC_F_DEC },
	{ "battery.packs.bad",		'<',	APC_F_DEC },
	{ "battery.alarm.threshold",	'k',	0 },
	{ "ups.serial",			'n',	0 },
	{ "ups.mfr.date",		'm',	0 },
	{ "ups.model",			'\001',	0 },
	{ "ups.firmware.aux",		'v',	0 },
	{ "ups.firmware",		'b',	APC_MULTI },
	{ "ups.firmware",		'V',	APC_MULTI },

	{ NULL, 0, 0 }
	/* todo:

	   I = alarm enable (hex field) - split into alarm.n.enable
	   J = alarm status (hex field) - split into alarm.n.status

	0x15 = output voltage selection (APC_F_VOLT)
	0x5C = load power (APC_POLL|APC_F_PERCENT)

	 */
};

/* ------ instant commands ------ */

#define APC_CMD_OFF		'Z'
#define APC_CMD_ON		'\016'		/* ^N */
#define APC_CMD_FPTEST		'A'
#define APC_CMD_SIMPWF		'U'
#define APC_CMD_BTESTTOGGLE	'W'
#define APC_CMD_GRACEDOWN	'@'
#define APC_CMD_SOFTDOWN	'S'
#define APC_CMD_SHUTDOWN	'K'
#define APC_CMD_CALTOGGLE	'D'
#define APC_CMD_BYPTOGGLE	'^'

typedef struct {
	const char	*name;
	char		cmd;
	int		flags;
} apc_cmdtab_t;

apc_cmdtab_t	apc_cmdtab[] =
{
	{ "load.off",			APC_CMD_OFF,		APC_NASTY|APC_REPEAT },
	{ "load.on",			APC_CMD_ON,		APC_REPEAT },
	{ "test.panel.start",		APC_CMD_FPTEST,		0 },
	{ "test.failure.start",		APC_CMD_SIMPWF,		0 },
	{ "test.battery.start",		APC_CMD_BTESTTOGGLE,	0 },
	{ "test.battery.stop",		APC_CMD_BTESTTOGGLE,	0 },
	{ "shutdown.return.grace",	APC_CMD_GRACEDOWN,	APC_NASTY },
	{ "shutdown.return",		APC_CMD_SOFTDOWN,	APC_NASTY },
	{ "shutdown.stayoff",		APC_CMD_SHUTDOWN,	APC_NASTY|APC_REPEAT },
	{ "calibrate.start",		APC_CMD_CALTOGGLE,	0 },
	{ "calibrate.stop",		APC_CMD_CALTOGGLE,	0 },
	{ "bypass.start",		APC_CMD_BYPTOGGLE,	0 },
	{ "bypass.stop",		APC_CMD_BYPTOGGLE,	0 },

	{ NULL, 0, 0 }
};

/* compatibility with hardware that doesn't do APC_CMDSET ('a') */

struct {
	const char	*firmware;
	const char	*cmdchars;
	int		flags;
} compat_tab[] = {
	/* APC Matrix */
	{ "0XI",	"@789ABCDEFGKLMNOPQRSTUVWXYZcefgjklmnopqrsuwxz/<>\\^\014\026", 0 },
	{ "0XM",	"@789ABCDEFGKLMNOPQRSTUVWXYZcefgjklmnopqrsuwxz/<>\\^\014\026", 0 },
	{ "0ZI",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz/<>", 0 },
	{ "5UI",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz/<>", 0 },
	{ "5ZM",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz/<>", 0 },
	/* APC600 */
	{ "6QD",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "6QI",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "6TD",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "6TI",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	/* SmartUPS 900 */
	{ "7QD",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "7QI",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "7TD",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "7TI",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	/* SmartUPS 900I */
	{ "7II",	"@79ABCEFGKLMNOPQSUVWXYZcfg", 0 },
	/* SmartUPS 2000I */
	{ "9II",	"@79ABCEFGKLMNOPQSUVWXYZcfg", 0 },
	{ "9GI",	"@79ABCEFGKLMNOPQSUVWXYZcfg", 0 },
	/* SmartUPS 1250 */
	{ "8QD",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "8QI",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "8TD",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "8TI",	"@79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	/* CS 350 */
	{ "5.4.D",	"@\1ABPQRSUYbdfgjmnx9",	0 },
	/* Smart-UPS 600 */
	{  "D9",	"@789ABCEFGKLMNOPQRSUVWXYZ", 0 },
	{  "D8",	"@789ABCEFGKLMNOPQRSUVWXYZ", 0 },
	{  "D7",	"@789ABCEFGKLMNOPQRSUVWXYZ", 0 },
	{  "D6",	"@789ABCEFGKLMNOPQRSUVWXYZ", 0 },
	{  "D5",	"@789ABCEFGKLMNOPQRSUVWXYZ", 0 },
	{  "D4",	"@789ABCEFGKLMNOPQRSUVWXYZ", 0 },

	{ NULL, NULL, 0 }
};
