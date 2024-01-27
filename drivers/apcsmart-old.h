/* apcsmart.h - command table for APC smart protocol units

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
             (C) 2000  Nigel Metheringham <Nigel.Metheringham@Intechnology.co.uk>

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

#ifndef NUT_APCSMART_OLD_H_SEEN
#define NUT_APCSMART_OLD_H_SEEN 1

#include <ctype.h>
#include <sys/ioctl.h>
#include "serial.h"
#include "timehead.h"

#define APC_TABLE_VERSION	"version 2.2"

/* Basic UPS reply line structure */
#define ENDCHAR 10		/* APC ends responses with LF */

/* characters ignored by default */
#define IGNCHARS "\015+$|!~%?=#&"	/* special characters to ignore */

/* these one is used only during startup, due to ^Z sending certain characters such as # */
#define MINIGNCHARS "\015+$|!"	/* minimum set of special characters to ignore */

/* normal polls: characters we don't want to parse (including a few alerts) */
#define POLL_IGNORE "\015&|"

/* alert characters we care about - OL, OB, LB, not LB, RB, OVER, not OVER */
#define POLL_ALERT "$!%+#?="

#define UPSDELAY	  50000	/* slow down multicharacter commands        */
#define CMDLONGDELAY	1500000	/* some commands need a 1.5s gap for safety */

#define SER_WAIT_SEC	3	/* wait up to 3.0 sec for ser_get calls */
#define SER_WAIT_USEC	0

/* dangerous instant commands must be reconfirmed within a 12 second window */
#define MINCMDTIME	3
#define MAXCMDTIME	15

/* it only does two strings, and they're both the same length */
#define APC_STRLEN	8

/* --------------- */

/* status bits */

#define APC_STAT_CAL	1L	/* calibration */
#define APC_STAT_TRIM	2L	/* SmartTrim */
#define APC_STAT_BOOST	4L	/* SmartBoost */
#define APC_STAT_OL	8L	/* on line */
#define APC_STAT_OB	16L	/* on battery */
#define APC_STAT_OVER	32L	/* overload */
#define APC_STAT_LB	64L	/* low battery */
#define APC_STAT_RB	128L	/* replace battery */

/* serial protocol: special commands - initialization and such */
#define APC_STATUS	'Q'
#define APC_GOSMART	'Y'
#define APC_GODUMB	'R'
#define APC_CMDSET	'a'
#define APC_CAPABILITY	26	/* ^Z */
#define APC_NEXTVAL	'-'

/* --------------- */

/* Driver command table flag values */

#define APC_POLL	0x0001	/* Poll this variable regularly		*/
#define APC_PRESENT	0x0004	/* Capability seen on this UPS		*/

#define APC_RW		0x0010	/* read-write variable			*/
#define APC_ENUM	0x0020	/* enumerated type			*/
#define APC_STRING	0x0040	/* string				*/

#define APC_NASTY	0x0100	/* Nasty command - take care		*/
#define APC_REPEAT	0x0200	/* Command needs sending twice		*/

#define APC_FORMATMASK	0xFF0000 /* Mask for apc data formats */

#define APC_F_PERCENT	0x020000 /* Data in a percent format */
#define APC_F_VOLT	0x030000 /* Data in a voltage format */
#define APC_F_AMP	0x040000 /* Data in a current/amp format */
#define APC_F_CELSIUS	0x050000 /* Data in a temp/C format */
#define APC_F_HEX	0x060000 /* Data in a hex number format */
#define APC_F_DEC	0x070000 /* Data in a decimal format */
#define APC_F_SECONDS	0x100000 /* Time in seconds */
#define APC_F_MINUTES	0x110000 /* Time in minutes */
#define APC_F_HOURS	0x120000 /* Time in hours */
#define APC_F_REASON	0x130000 /* Reason of transfer */
#define APC_F_LEAVE	0	/* Just pass this through */

typedef struct {
	const	char	*name;		/* the variable name */
	unsigned int	flags;	 	/* various flags		*/
	unsigned char	cmd;		/* command character */
} apc_vartab_t;

static apc_vartab_t	apc_vartab[] = {

	{ "ups.firmware.old",  	0,			'V' },
	{ "ups.firmware",  	0,			'b' },
	{ "ups.firmware.aux",	0,			'v' },
	{ "ups.model",		0,			0x01 },

	{ "ups.serial",		0,			'n' },
	{ "ups.mfr.date", 	0,			'm' },

	{ "ups.temperature", 	APC_POLL|APC_F_CELSIUS, 'C' },
	{ "ups.load",  		APC_POLL|APC_F_PERCENT, 'P' },

	{ "ups.test.interval",  APC_F_HOURS,		'E' },
	{ "ups.test.result", 	APC_POLL,     		'X' },

	{ "ups.delay.start", 	APC_F_SECONDS,		'r' },
	{ "ups.delay.shutdown",	APC_F_SECONDS,		'p' },

	{ "ups.id",  		APC_STRING,		'c' },

	{ "ups.contacts", 	APC_POLL|APC_F_HEX,	'i' },
	{ "ups.display.language",
			 	0,			0x0C },

	{ "input.voltage", 	APC_POLL|APC_F_VOLT,	'L' },
	{ "input.frequency", 	APC_POLL|APC_F_DEC,	'F' },
	{ "input.sensitivity",	0,			's' },
	{ "input.quality",  	APC_POLL|APC_F_HEX,	'9' },

	{ "input.transfer.low",	APC_F_VOLT,		'l' },
	{ "input.transfer.high",
				APC_F_VOLT,		'u' },
	{ "input.transfer.reason",
				APC_POLL|APC_F_REASON,	'G' },

	{ "input.voltage.maximum",
				APC_POLL|APC_F_VOLT,	'M' },
	{ "input.voltage.minimum",
				APC_POLL|APC_F_VOLT,	'N' },

	{ "output.current", 	APC_POLL|APC_F_AMP,	'/' },
	{ "output.voltage", 	APC_POLL|APC_F_VOLT,	'O' },
	{ "output.voltage.nominal",
				APC_F_VOLT,		'o' },

	{ "ambient.humidity",  	APC_POLL|APC_F_PERCENT,	'h' },
	{ "ambient.humidity.high",
				APC_F_PERCENT,		'{' },
	{ "ambient.humidity.low",
				APC_F_PERCENT,		'}' },

	{ "ambient.temperature",
				APC_POLL|APC_F_CELSIUS, 't' },
	{ "ambient.temperature.high",
				APC_F_CELSIUS,		'[' },
	{ "ambient.temperature.low",
				APC_F_CELSIUS,		']' },

	{ "battery.date",	APC_STRING,		'x' },

	{ "battery.charge",  	APC_POLL|APC_F_PERCENT,	'f' },
	{ "battery.charge.restart",
				APC_F_PERCENT,		'e' },

	{ "battery.voltage", 	APC_POLL|APC_F_VOLT,	'B' },
	{ "battery.voltage.nominal",
				0,			'g' },

	{ "battery.runtime", 	APC_POLL|APC_F_MINUTES,	'j' },
	{ "battery.runtime.low",
			 	APC_F_MINUTES,		'q' },

	{ "battery.packs", 	APC_F_DEC,		'>' },
	{ "battery.packs.bad", 	APC_F_DEC,		'<' },
	{ "battery.alarm.threshold",
				0,			'k' },
	/* todo:

	   I = alarm enable (hex field) - split into alarm.n.enable
	   J = alarm status (hex field) - split into alarm.n.status

	0x15 = output voltage selection (APC_F_VOLT)
	0x5C = load power (APC_POLL|APC_F_PERCENT)

	 */

	{NULL,		0,				0},
};

/* ------ instant commands ------ */

#define APC_CMD_FPTEST		'A'
#define APC_CMD_CALTOGGLE	'D'
#define APC_CMD_SHUTDOWN	'K'
#define APC_CMD_SOFTDOWN	'S'
#define APC_CMD_GRACEDOWN	'@'
#define APC_CMD_SIMPWF		'U'
#define APC_CMD_BTESTTOGGLE	'W'
#define APC_CMD_OFF		'Z'

#define APC_CMD_ON		0x0E		/* ^N */
#define APC_CMD_BYPTOGGLE	'^'

typedef struct {
	const	char	*name;
	int	flags;
	unsigned char	cmd;
} apc_cmdtab_t;

static apc_cmdtab_t	apc_cmdtab[] =
{
	{ "load.off",		APC_NASTY|APC_REPEAT,	APC_CMD_OFF       },
	{ "load.on",		APC_REPEAT,		APC_CMD_ON        },

	{ "test.panel.start",	0,			APC_CMD_FPTEST	  },

	{ "test.failure.start",	0,			APC_CMD_SIMPWF    },

	{ "test.battery.start",	0,			APC_CMD_BTESTTOGGLE },
	{ "test.battery.stop",	0,			APC_CMD_BTESTTOGGLE },

	{ "shutdown.return.grace",
				APC_NASTY,		APC_CMD_GRACEDOWN  },
	{ "shutdown.return",	APC_NASTY,		APC_CMD_SOFTDOWN  },
	{ "shutdown.stayoff",	APC_NASTY|APC_REPEAT,	APC_CMD_SHUTDOWN  },

	{ "calibrate.start",	0,			APC_CMD_CALTOGGLE },
	{ "calibrate.stop",	0,			APC_CMD_CALTOGGLE },

	{ "bypass.start",	0,			APC_CMD_BYPTOGGLE },
	{ "bypass.stop",	0,			APC_CMD_BYPTOGGLE },

	{ NULL, 0, 0					}
};

/* compatibility with hardware that doesn't do APC_CMDSET ('a') */

static struct {
	const	char	*firmware;
	const	char	*cmdchars;
	int	flags;
} compat_tab[] = {
	/* APC Matrix */
	{ "0XI",	"789ABCDEFGKLMNOPQRSTUVWXYZcefgjklmnopqrsuwxz/<>\\^\014\026", 0 },
	{ "0XM",	"789ABCDEFGKLMNOPQRSTUVWXYZcefgjklmnopqrsuwxz/<>\\^\014\026", 0 },
	{ "0ZI",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz/<>", 0 },
	{ "5UI",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz/<>", 0 },
	{ "5ZM",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz/<>", 0 },
	/* APC600 */
	{ "6QD",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "6QI",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "6TD",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "6TI",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	/* SmartUPS 900 */
	{ "7QD",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "7QI",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "7TD",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "7TI",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	/* SmartUPS 900I */
	{ "7II",	"79ABCEFGKLMNOPQSUVWXYZcfg", 0 },
	/* SmartUPS 2000I */
	{ "9II",	"79ABCEFGKLMNOPQSUVWXYZcfg", 0 },
	{ "9GI",	"79ABCEFGKLMNOPQSUVWXYZcfg", 0 },
	/* SmartUPS 1250 */
	{ "8QD",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "8QI",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "8TD",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	{ "8TI",	"79ABCDEFGKLMNOPQRSUVWXYZcefgjklmnopqrsuxz", 0 },
	/* CS 350 */
	{ "5.4.D",	"\1ABPQRSUYbdfgjmnx9",	0 },
	/* Smart-UPS 600 */
	{  "D9",	"789ABCEFGKLMNOPQRSUVWXYZ", 0 },
	{  "D8",	"789ABCEFGKLMNOPQRSUVWXYZ", 0 },
	{  "D7",	"789ABCEFGKLMNOPQRSUVWXYZ", 0 },
	{  "D6",	"789ABCEFGKLMNOPQRSUVWXYZ", 0 },
	{  "D5",	"789ABCEFGKLMNOPQRSUVWXYZ", 0 },
	{  "D4",	"789ABCEFGKLMNOPQRSUVWXYZ", 0 },

	{ NULL,		NULL,			0 },
};

#endif  /* NUT_APCSMART_OLD_H_SEEN */
