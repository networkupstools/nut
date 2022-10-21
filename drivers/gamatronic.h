/* gamatronic.h
 *
 * SEC UPS Driver ported to the new NUT API for Gamatronic UPS Usage.
 *
 * Copyright (C)
 *   2001 John Marley <John.Marley@alcatel.com.au>
 *   2002 Jules Taplin <jules@netsitepro.co.uk>
 *   2002 Eric Lawson <elawson@inficad.com>
 *   2005 Arnaud Quette <http://arnaud.quette.free.fr/contact.html>
 *   2005 Nadav Moskovitch <blutz@walla.com / http://www.gamatronic.com>
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

#ifndef NUT_GAMATRONIC_H_SEEN
#define NUT_GAMATRONIC_H_SEEN 1

#define SEC_MSG_STARTCHAR	'^'
#define SEC_POLLCMD		'P'
#define SEC_SETCMD		'S'
#define SEC_DATAMSG		'D'
#define SEC_UPSMSG		'*'
#define SEC_ACK			'1'
#define SEC_NAK			'0'

/* commands */
#define SEC_AVAILP1	"AP1"		/* Part1 of available variables */
#define SEC_AVAILP2	"AP2"		/* Part1 of available variables */
#define SEC_AUTORESTART	"ATR"		/* Enable/disable auto restart */
#define SEC_MFR		"MAN"		/* UPS Manufacturer */
#define SEC_MOD		"MOD"		/* UPS Model */
#define SEC_NOMINAL	"NOM"		/* Nominal Values */
#define SEC_SHUTDOWN	"PSD" 		/* Shutdown after delay/cancel */
#define SEC_REBOOT	"RWD"		/* Reboot with duration/cancel */
#define SEC_SHUTTYPE	"SDA"		/* Shutdown Type */
#define SEC_BATTSTAT	"ST1" 		/* Battery Status */
#define SEC_INPUTSTAT	"ST2" 		/* Input Status */
#define SEC_OUTPUTSTAT	"ST3" 	 	/* Output Status */
#define SEC_BYPASSSTAT	"ST4" 		/* Bypass Status */
#define SEC_ALARMSTAT	"ST5" 		/* UPS Alarms */
#define SEC_STARTDELAY	"STD"		/* Startup after delay */
#define SEC_TESTRESULT	"STR" 		/* Test Results */
#define SEC_TEST	"TST"		/* UPS Test/abort */
#define SEC_BAUDRATE	"UBR"		/* UPS Baud Rate */
#define SEC_UPSID	"UID"		/* UPS Identifier */
#define SEC_VERSION	"VER"		/* UPS Software Version */

#define FLAG_STRING 1
#define FLAG_RW 2
#define FLAG_WONLY 3 /* Dont waste time on reading commands that are read only */
#define FLAG_ALARM 4 /* If the value of a var with this flag equals to 1, then its name added to alarms list */
#define FLAG_MULTI 5 /* Multiple UNIT value instead of Dividing It */

#define FLAG_POLL 0 /* For commands that polled normaly */
#define FLAG_POLLONCE 1 /* For commands that only polled once */

/* Some baud rates for setup_serial() */
static struct {
    speed_t rate;	/* Value like B19200 defined in termios.h; note: NOT the bitrate numerically */
    size_t name;	/* Actual rate... WHY is this "name" - number to print interactively? */
} baud_rates[] = {
    { B1200,  1200 },
    { B2400,  2400 },
    { B4800,  4800 },
    { B9600,  9600 },
    { B19200, 19200 },
};

#define SEC_NUMVARS 89
#define SEC_MAX_VARSIZE 65

/* macro for checking whether a variable is supported */

typedef struct {
    const char *setcmd;	/* INFO_x define from shared.h */
    const char *name;		/* Human readable text (also in shared-tables.h) */
    int  unit;		/* Variable should be divided by this */
    const char *cmd;		/* Command to send to pool/set variable */
    int  field;		/* Which returned field variable corresponsd to */
    int  size;		/* string length/integer max/enum count */
    int  poll;		/* poll flag */
    int  flags;		/* Flags for addinfo() */
    char value[SEC_MAX_VARSIZE];
} sec_varlist_t;

static sec_varlist_t sec_varlist[] = {
    { "",			"",                          0, "",              0,       0,  0, 0, "" },
    /*setcmd		 name                        unit   cmd        field   size  poll  flags  value */
    { "",			"Awaiting Power ",           1, SEC_ALARMSTAT,  13,       2,  0, FLAG_ALARM, ""},
    { "",			"Bypass Bad ",               1, SEC_ALARMSTAT,   5,       2,  0, FLAG_ALARM, ""},
    { "",			"Charger Failure ",          1, SEC_ALARMSTAT,   8,       2,  0, FLAG_ALARM, ""},
    { "",			"Fan Failure ",              1, SEC_ALARMSTAT,  10,       2,  0, FLAG_ALARM, ""},
    { "",			"Fuse Failure ",             1, SEC_ALARMSTAT,  11,       2,  0, FLAG_ALARM, ""},
    { "",			"General Fault ",            1, SEC_ALARMSTAT,  12,       2,  0, FLAG_ALARM, ""},
    { "",			"Input Bad ",                1, SEC_ALARMSTAT,   2,       2,  0, FLAG_ALARM, ""},
    { "",			"Output Bad ",               1, SEC_ALARMSTAT,   3,       2,  0, FLAG_ALARM, ""},
    { "",			"Output Off ",               1, SEC_ALARMSTAT,   6,       2,  0, FLAG_ALARM, ""},
    { "",			"Overload ",                 1, SEC_ALARMSTAT,   4,       2,  0, FLAG_ALARM, ""},
    { "",			"Shutdown Imminent ",        1, SEC_ALARMSTAT,  15,       2,  0, FLAG_ALARM, ""},
    { "",			"Shutdown Pending ",         1, SEC_ALARMSTAT,  14,       2,  0, FLAG_ALARM, ""},
    { "",			"System Off ",               1, SEC_ALARMSTAT,   9,       2,  0, FLAG_ALARM, ""},
    { "",			"Temperature ",              1, SEC_ALARMSTAT,   1,       2,  0, FLAG_ALARM, ""},
    { "",			"UPS Shutdown ",             1, SEC_ALARMSTAT,   7,       2,  0, FLAG_ALARM, ""},
    { "",			"Audible Alarm",             1, SEC_NOMINAL,     8,       4,  FLAG_POLLONCE, FLAG_RW, ""},
    { "",			"Auto Restart",              1, SEC_AUTORESTART, 1,       2,  FLAG_POLLONCE, FLAG_RW, ""},
    { "",			"Battery Charge",            1, SEC_BATTSTAT,    3,       4,  0, 0, ""},
    { "",			"Battery Condition",         1, SEC_BATTSTAT,    1,       3,  0, 0, ""},
    { "battery.current",	"Battery Current",          10, SEC_BATTSTAT,    8,    9999,  0, 0, ""},
    { "battery.date",		"Battery Installed",         1, SEC_NOMINAL,    11,       8,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "",			"Battery Status",            1, SEC_BATTSTAT,    2,       3,  0, 0, ""},
    { "battery.temperature",	"Battery Temperature",       1, SEC_BATTSTAT,    9,      99,  0, 0, ""},
    { "battery.voltage",	"Battery Voltage",          10, SEC_BATTSTAT,    7,    9999,  0, 0, ""},
    { "",			"Bypass Current 1",         10, SEC_BYPASSSTAT,  4,    9999,  0, 0, ""},
    { "",			"Bypass Current 2",         10, SEC_BYPASSSTAT,  7,    9999,  0, 0, ""},
    { "",			"Bypass Current 3",         10, SEC_BYPASSSTAT, 10,    9999,  0, 0, ""},
    { "",			"Bypass Frequency",         10, SEC_BYPASSSTAT,  1,     999,  0, 0, ""},
    { "",			"Bypass Num Lines",          1, SEC_BYPASSSTAT,  2,       9,  0, 0, ""},
    { "",			"Bypass Power 1",            1, SEC_BYPASSSTAT,  5,   99999,  0, 0, ""},
    { "",			"Bypass Power 2",            1, SEC_BYPASSSTAT,  8,   99999,  0, 0, ""},
    { "",			"Bypass Power 3",            1, SEC_BYPASSSTAT, 11,   99999,  0, 0, ""},
    { "",			"Bypass Voltage 1",         10, SEC_BYPASSSTAT,  3,    9999,  0, 0, ""},
    { "",			"Bypass Voltage 2",         10, SEC_BYPASSSTAT,  6,    9999,  0, 0, ""},
    { "",			"Bypass Voltage 3",         10, SEC_BYPASSSTAT,  9,    9999,  0, 0, ""},
    { "battery.charge",		"Estimated Charge",          1, SEC_BATTSTAT,    6,     999,  0, 0, ""},
    { "battery.runtime.low",	"Estimated Minutes",        60, SEC_BATTSTAT,    5,     999,  0, FLAG_MULTI, ""},
    { "input.transfer.high",	"High Volt Xfer Pt",         1, SEC_NOMINAL,    10,     999,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "ups.id",			"Identification",            1, SEC_UPSID,       1,      64,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "",			"Input Current 1",          10, SEC_INPUTSTAT,   5,    9999,  0, 0, ""},
    { "",			"Input Current 2",          10, SEC_INPUTSTAT,   9,    9999,  0, 0, ""},
    { "",			"Input Current 3",          10, SEC_INPUTSTAT,  13,    9999,  0, 0, ""},
    { "input.frequency",	"Input Frequency 1",        10, SEC_INPUTSTAT,   3,     999,  0, 0, ""},
    { "",			"Input Frequency 2",        10, SEC_INPUTSTAT,   7,     999,  0, 0, ""},
    { "",			"Input Frequency 3",        10, SEC_INPUTSTAT,  11,     999,  0, 0, ""},
    { "",			"Input Line Bads",           1, SEC_INPUTSTAT,   1,     999,  0, 0, ""},
    { "",			"Input Num Lines",           1, SEC_INPUTSTAT,   2,       9,  0, 0, ""},
    { "",			"Input Power 1",             1, SEC_INPUTSTAT,   6,   99999,  0, 0, ""},
    { "",			"Input Power 2",             1, SEC_INPUTSTAT,  10,   99999,  0, 0, ""},
    { "",			"Input Power 3",             1, SEC_INPUTSTAT,  14,   99999,  0, 0, ""},
    { "input.voltage",		"Input Voltage 1",          10, SEC_INPUTSTAT,   4,    9999,  0, 0, ""},
    { "",			"Input Voltage 2",          10, SEC_INPUTSTAT,   8,    9999,  0, 0, ""},
    { "",			"Input Voltage 3",          10, SEC_INPUTSTAT,  12,    9999,  0, 0, ""},
    { "input.transfer.low",	"Low Volt Xfer Pt",          1, SEC_NOMINAL,     9,     999,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "ups.mfr",		"Manufacturer",              1, SEC_MFR,         1,      32,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "ups.model",		"Model",                     1, SEC_MOD,         1,      64,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "",			"Nominal Battery Life",      1, SEC_NOMINAL,    12,   99999,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "",			"Nominal Input Frequency",  10, SEC_NOMINAL,     2,     999,  FLAG_POLLONCE, FLAG_RW, ""},
    { "input.voltage.nominal",	"Nominal Input Voltage",     1, SEC_NOMINAL,     1,     999,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "",			"Nominal Low Battery Time",  1, SEC_NOMINAL,     7,      99,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "",			"Nominal Output Frequency", 10, SEC_NOMINAL,     4,     999,  FLAG_POLLONCE, FLAG_RW, ""},
    { "",			"Nominal Output Power",      1, SEC_NOMINAL,     6,   99999,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "",			"Nominal Output Voltage",    1, SEC_NOMINAL,     3,     999,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "ups.power.nominal",	"Nominal VA Rating",         1, SEC_NOMINAL,     5,   99999,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "output.current",		"Output Current 1",         10, SEC_OUTPUTSTAT,  5,    9999,  0, 0, ""},
    { "",			"Output Current 2",         10, SEC_OUTPUTSTAT,  9,    9999,  0, 0, ""},
    { "",			"Output Current 3",         10, SEC_OUTPUTSTAT, 13,    9999,  0, 0, ""},
    { "output.frequency",	"Output Frequency",         10, SEC_OUTPUTSTAT,  2,     999,  0, 0, ""},
    { "ups.load",		"Output Load 1",             1, SEC_OUTPUTSTAT,  7,     999,  0, 0, ""},
    { "",			"Output Load 2",             1, SEC_OUTPUTSTAT, 11,     999,  0, 0, ""},
    { "",			"Output Load 3",             1, SEC_OUTPUTSTAT, 15,     999,  0, 0, ""},
    { "",			"Output Num Lines",          1, SEC_OUTPUTSTAT,  3,       9,  0, 0, ""},
    { "",			"Output Power 1",            1, SEC_OUTPUTSTAT,  6,   99999,  0, 0, ""},
    { "",			"Output Power 2",            1, SEC_OUTPUTSTAT, 10,   99999,  0, 0, ""},
    { "",			"Output Power 3",            1, SEC_OUTPUTSTAT, 14,   99999,  0, 0, ""},
    { "",			"Output Source",             1, SEC_OUTPUTSTAT,  1,       6,  0, 0, ""},
    { "output.voltage",		"Output Voltage 1",         10, SEC_OUTPUTSTAT,  4,    9999,  0, 0, ""},
    { "",			"Output Voltage 2",         10, SEC_OUTPUTSTAT,  8,    9999,  0, 0, ""},
    { "",			"Output Voltage 3",         10, SEC_OUTPUTSTAT, 12,    9999,  0, 0, ""},
    { "",			"Reboot With Duration",      1, SEC_REBOOT,      1, 9999999,  FLAG_POLLONCE, FLAG_WONLY, ""},
    { "battery.runtime",	"Seconds on Battery",        1, SEC_BATTSTAT,    4,   99999,  0, 0, ""},
    { "",			"Shutdown Type",             1, SEC_SHUTTYPE,    1,       2,  FLAG_POLLONCE, FLAG_RW, ""},
    { "ups.delay.shutdown",	"Shutdown After Delay",      1, SEC_STARTDELAY,  1, 9999999,  FLAG_POLLONCE, FLAG_WONLY, ""},
    { "ups.firmware",		"Software Version",          1, SEC_VERSION,     1,      32,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "",			"Startup After Delay",       1, SEC_STARTDELAY,  1, 9999999,  FLAG_POLLONCE, FLAG_WONLY, ""},
    { "",			"Test Results Detail",       1, SEC_TESTRESULT,  2,      64,  FLAG_POLLONCE, FLAG_STRING, ""},
    { "",			"Test Results Summary",      1, SEC_TESTRESULT,  1,       6,  FLAG_POLLONCE, 0, ""},
    { "",			"Test Type",                 1, SEC_TEST,        1,       5,  FLAG_POLLONCE, FLAG_WONLY, ""},
    { "",			"Baud Rate",                 1, SEC_BAUDRATE,    1,   19200,  FLAG_POLLONCE, FLAG_RW, ""},
};


/* a type for the supported variables */
#define SEC_QUERYLIST_LEN	17
#define SEC_MAXFIELDS		16
#define SEC_POLL			1
#define SEC_POLLONCE		0

static struct {
    const char *command;	/* sec command */
    int  varnum[SEC_MAXFIELDS];	/* sec variable number for each field */
    int  pollflag;
} sec_querylist[SEC_QUERYLIST_LEN];

#define sqv(a,b) sec_querylist[a].varnum[b]

#endif	/* NUT_GAMATRONIC_H_SEEN */
