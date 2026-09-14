/*
 * safenet.h - defines/macros for the safenet driver
 *
 * Copyright (C) 2003  Arjen de Korte <adkorte-guest@alioth.debian.org>
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
 */

#ifndef NUT_SAFENET_H_SEEN
#define NUT_SAFENET_H_SEEN 1

/*
 * The following commands where traced on the serial port. From these, the
 * COM_POLL_STAT command is just an example of how this command looks.
 * Inside the driver, we'll overwrite the default with specially crafted
 * 'random' data (see the comments).
 */
#define	COM_INITIALIZE	"ZCADLIOPERJD\r"
#define	COM_MAINS_TEST	"ZFSDERBTRFGY\r"
#define	COM_BATT_TEST	"ZAVLEJFICOPR\r"
#define	COM_STOP_TEST	"ZGWLEJFICOPR\r"
#define	COM_TOGGLE_BEEP	"ZELWSABPMBEQ\r"
#define	COM_POLL_STAT	"ZHDGFGDJELBC\r"

/*
 * The following command is "ZBASdddWLPGE\r", where 'ddd' equals the number of
 * seconds delay before the UPS switches off. Value must be greater than or
 * equal to 1. Mapping of the numerals is 0=A, 1=B, 2=C, etc.
 */
#define	SHUTDOWN_RETURN	"ZBASAAAWLPGE\r"	/* shutdown in 1 second */

/*
 * The following commands are "ZAFdddRrrrrO\r", where 'ddd' equals the number
 * of seconds delay before the UPS switches off and 'rrrr' the number of
 * minutes before it restarts. Both values must be greater than or equal to 1.
 * Mapping of the numerals is 0=A, 1=B, 2=C, etc.
 */
#define	SHUTDOWN_REBOOT	"ZAFAAARAAAAO\r"	/* shutdown in 1 second, return after 1 minute */

struct safenet {
	char	onbattery;
	char	dunno_02;
	char	batterylow;
	char	overload;
	char	dunno_05;
	char	silenced;
	char	batteryfail;
	char	systemfail;
	char	systemtest;
	char	dunno_10;
};

#endif	/* NUT_SAFENET_H_SEEN */
