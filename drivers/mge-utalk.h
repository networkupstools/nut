/* mge-utalk.h

   Type Declarations and Constant Definitions for MGE UTalk Driver

   Copyright (C) 2002 - 2004
   
   	Hans Ekkehard Plesser <hans.plesser@itf.nlh.no>
	Arnaud Quette <arnaud.quette@free.fr>

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

/* --------------------------------------------------------------- */
/*                 Default Values for UPS Variables                */
/* --------------------------------------------------------------- */

#define DEFAULT_LOWBATT   30   /* low battery level, in %          */

/* delay between return of utility power and powering up of load (in MINUTES) */
#define DEFAULT_ONDELAY    1   
#define DEFAULT_OFFDELAY 20   /* delay before power off, in SECONDS */ 

#define MIN_CONFIRM_TIME   3   /* shutdown must be confirmed in    */
#define MAX_CONFIRM_TIME  15   /* this interval                    */

/* --------------------------------------------------------------- */
/*                       Multiplier Tables                         */
/* --------------------------------------------------------------- */

/* First index : Table number, fetched with "Ai" command 
 * Second index: unit, as in enum above                               
 * NOTE:
 *	- to make the table index the same as the MGE table number, 
 *		dummy table row multiplier[0][] is inserted
 *	- unit MIN2SEC is used to convert values in minutes sent by
 * 	the UPS (WAKEDELAY) to seconds
 *	- the final column corresponds to unit NONE
*/

/* units in multiplier table */
typedef enum eunits 
{ VOLT = 0, AMPERE, HERTZ, VOLTAMP, WATT, DEGCELS, MIN2SEC, NONE } units;

static const double multiplier[4][8] = {
/*   V     A     Hz   VA     W   C  MIN2SEC NONE */ 
  { 1  , 1  , 1  ,    1,    1, 1,    60,   1 },
  { 1  , 1  , 0.1 , 1000, 1000, 1,    60,   1 },
  { 0.01, 0.01, 1  ,    1,    1, 1,    60,   1 },
  { 1  , 0.01, 0.1 ,    1,    1, 1,    60,   1 }
};

/* --------------------------------------------------------------- */
/*                       Explicit Booleans                         */
/* --------------------------------------------------------------- */

/* use explicit booleans */
#ifdef FALSE
	#undef FALSE
#endif /* FALSE */
#ifdef TRUE
	#undef TRUE
#endif /* TRUE */
typedef enum ebool { FALSE=0, TRUE } bool;


/* --------------------------------------------------------------- */
/*      Query Commands and their Mapping to INFO_ Variables        */
/* --------------------------------------------------------------- */

/* Structure defining how to query UPS for a variable and write
   information to INFO structure.
*/
typedef struct {
	const char *type;          /* INFO_* element                        */
	int   flags;               /* INFO-element flags to set in addinfo  */
	int   length;              /* INFO-element length of strings        */  
	const char  cmd[32];       /* UPS command string to requets element */
	const char  fmt[32];       /* printf format string for INFO entry   */
	units unit;                /* unit of measurement, or NONE          */
	bool  ok;                  /* flag indicating if item is available  */
} mge_info_item;

/* Array containing information to translate between UTalk and NUT info
 * NOTE: 
 *	- Array is terminated by element with type NULL.
 *	- Essential INFO items (_MFR, _MODEL, _FIRMWARE, _STATUS) are
 *		handled separately.
 *	- Array is NOT const, since "ok" can be changed.
 */

static mge_info_item mge_info[] = {
	/* Battery page */
	{ "battery.charge", 0, 0, "Bl", "%05.1f", NONE, TRUE },
	{ "battery.runtime", 0, 0, "Bn", "%05d", NONE,	TRUE },
	{ "battery.voltage",  0, 0, "Bv", "%05.1f", VOLT, TRUE },
	{ "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 2, "Bl ?", "%02d", NONE, TRUE },
	{ "battery.voltage.nominal", 0, 0, "Bv ?", "%05.1f", VOLT,	 TRUE },
	/* UPS page */
	{ "ups.temperature", 0, 0, "St", "%05.1f", DEGCELS, TRUE },
	{ "ups.load", 0, 0, "Ll", "%05.1f", NONE, TRUE },
	{ "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5, "Sm ?", "%03d", NONE, TRUE },
	{ "ups.delay.shutdown",  ST_FLAG_RW | ST_FLAG_STRING, 5, "Sn ?", "%03d", NONE, TRUE },
	{ "ups.test.interval", ST_FLAG_RW | ST_FLAG_STRING, 5, "Bp ?", "%03d", NONE, TRUE },
	/* Output page */
	{ "output.voltage", 0, 0, "Lv", "%05.1f", VOLT, TRUE },
	{ "output.current", 0, 0, "Lc", "%05.1f", AMPERE, TRUE },
	/* Input page */
	{ "input.voltage", 0, 0, "Uv", "%05.1f", VOLT, TRUE },
	{ "input.frequency", 0, 0, "Uf", "%05.2f", HERTZ, TRUE },
	/* same as LOBOOSTXFER */
	{ "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "Ee ?", "%05.1f", VOLT, TRUE },
	{ "input.transfer.boost.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "Ee ?", "%05.1f", VOLT, TRUE },
	{ "input.transfer.boost.high", ST_FLAG_RW | ST_FLAG_STRING, 5, "Eo ?", "%05.1f", VOLT, TRUE },
	{ "input.transfer.trim.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "Ea ?", "%05.1f", VOLT, TRUE },
	/* same as HITRIMXFER */
	{ "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 5, "Eu ?", "%05.1f", VOLT, TRUE },
	{ "input.transfer.trim.high", ST_FLAG_RW | ST_FLAG_STRING, 5, "Eu ?", "%05.1f", VOLT, TRUE },
	/* terminating element */
	{ NULL, 0, 0, "\0",	"\0", NONE, FALSE } 
};
