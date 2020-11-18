/* mge-utalk.h - monitor MGE UPS for NUT with UTalk protocol
 *
 *  Copyright (C) 2002 - 2005
 *     Arnaud Quette <arnaud.quette@free.fr>  & <arnaud.quette@mgeups.com>
 *     Hans Ekkehard Plesser <hans.plesser@itf.nlh.no>
 *     Martin Loyer <martin@degraaf.fr>
 *     Patrick Agrain <patrick.agrain@alcatel.fr>
 *     Nicholas Reilly <nreilly@magma.ca>
 *     Dave Abbott <d.abbott@dcs.shef.ac.uk>
 *     Marek Kralewski <marek@mercy49.de>
 *
 *  This driver is a collaborative effort by the above people,
 *  Sponsored by MGE UPS SYSTEMS <http://opensource.mgeups.com/>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef NUT_MGE_UTALK_H_SEEN
#define NUT_MGE_UTALK_H_SEEN 1

/* --------------------------------------------------------------- */
/*                 Default Values for UPS Variables                */
/* --------------------------------------------------------------- */

#define DEFAULT_LOWBATT   30   /* low battery level, in %          */

/* delay between return of utility power and powering up of load (in MINUTES) */
#define DEFAULT_ONDELAY    1
#define DEFAULT_OFFDELAY  20   /* delay before power off, in SECONDS */

#define MIN_CONFIRM_TIME   3   /* shutdown must be confirmed in    */
#define MAX_CONFIRM_TIME  15   /* this interval                    */

/* --------------------------------------------------------------- */
/*      Model Name formating entries                               */
/* --------------------------------------------------------------- */
typedef struct {
  const char	*basename; /* as returned by Si 1 <data 2> */
  const char	*finalname;
} models_name_t;

models_name_t Si1_models_names [] =
  {
	/* Pulsar EX */
	{ "Pulsar EX7", "Pulsar EX 7" },
	{ "Pulsar EX10", "Pulsar EX 10" },
	{ "Pulsar EX15", "Pulsar EX 15" },
	{ "Pulsar EX20", "Pulsar EX 20" },
	{ "Pulsar EX30", "Pulsar EX 30" },
	{ "Pulsar EX40", "Pulsar EX 40" },

	/* Pulsar ES+ */
	{ "Pulsar ES2+", "Pulsar ES 2+" },
	{ "Pulsar ES5+", "Pulsar ES 5+" },

	/* Pulsar ESV+ */
	{ "Pulsar ESV5+", "Pulsar ESV 5+" },
	{ "Pulsar ESV8+", "Pulsar ESV 8+" },
	{ "Pulsar ESV11+", "Pulsar ESV 11+" },
	{ "Pulsar ESV14+", "Pulsar ESV 14+" },
	{ "Pulsar ESV22+", "Pulsar ESV 22+" },

	/* Pulsar EXtreme */
	{ "EXTREME 1500", "Pulsar EXtreme 1500" },
	{ "EXTREME 2000", "Pulsar EXtreme 2000" },
	{ "EXTREME 2500", "Pulsar EXtreme 2500" },
	{ "EXTREME 3000", "Pulsar EXtreme 3000" },

	/* Comet EXtreme */
	{ "EXTREME 4.5", "Comet EXtreme 4.5" },
	{ "EXTREME 6", "Comet EXtreme 6" },
	{ "EXTREME 9", "Comet EXtreme 9" },
	{ "EXTREME 12", "Comet EXtreme 12" },

	/* Comet */
	{ "COMET 5", "Comet 5" },
	{ "COMET 7", "Comet 7.5" },
	{ "COMET 10", "Comet 10" },
	{ "COMET 15", "Comet 15" },
	{ "COMET 20", "Comet 20" },
	{ "COMET 30", "Comet 30" },
	{ "COMET 12", "Comet 12" },
	{ "COMET 18", "Comet 18" },
	{ "COMET 24", "Comet 24" },
	{ "COMET 36", "Comet 36" },

	/* FIXME: complete with Pulsar ?EL 2/4/7?, EXL, SX, PSX/CSX, Evolution?,... */

	/* end of structure. */
	{ NULL, "Generic UTalk model" }
};

/* --------------------------------------------------------------- */
/*      Model Information for legacy models                        */
/* --------------------------------------------------------------- */

/* Structure defining how to get name and model for a particular Si output
 * This is for older UPS (Pulsar ESV,SV) which don't support Plug'n'Play 'Si 1' command
 */
typedef struct {
  int   Data1;               /* Data1, Family model          */
  int   Data2;               /* Data2, Type                  */
  /* Data3, SoftLevel is not implemented here, while it's always null or zero.*/
  const char *name;         /* ASCII model name (like 'Si 1' output */
} mge_model_info_t;

/* Array containing Model information for legacy models
 * NOTE:
 *      - Array is terminated by element with type NULL.
 */

static mge_model_info_t mge_model[] = {
  /* Pulsar SV page */
  { 3000, 5, "Pulsar SV3" },
  { 3000, 6, "Pulsar SV5/8/11" },
  { 3000, 7, "Pulsar SV3" },
  { 3000, 8, "Pulsar SV5/8" },
  { 3000, 10, "Pulsar SV11/ESV13" },
  /* Pulsar ESV page */
  { 3000, 11, "Pulsar ESV17" },
  { 3000, 12, "Pulsar ESV20" },
  { 3000, 13, "Pulsar ESV13" },
  { 3000, 14, "Pulsar ESV17" },
  { 3000, 15, "Pulsar ESV20" },
  { 3000, 17, "Pulsar ESV8" },
  /* Pulsar ESV+ compatibility (though these support Si1) */
  { 3000, 51, "Pulsar ESV 11+" },
  { 0, 0, NULL }
};

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
{ VOLT = 0, AMPERE, HERTZ, VOLTAMP, WATT, DEGCELS, MIN2SEC, NONE } units_t;

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
typedef enum ebool { FALSE=0, TRUE } bool_t;


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
	const char  *cmd;          /* UPS command string to requets element */
	const char  *fmt;          /* printf format string for INFO entry   */
	units_t unit;              /* unit of measurement, or NONE          */
	bool_t  ok;                /* flag indicating if item is available  */
} mge_info_item_t;

/* Array containing information to translate between UTalk and NUT info
 * NOTE:
 *	- Array is terminated by element with type NULL.
 *	- Essential INFO items (_MFR, _MODEL, _FIRMWARE, _STATUS) are
 *		handled separately.
 *	- Array is NOT const, since "ok" can be changed.
 */

static mge_info_item_t mge_info[] = {
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
	{ NULL, 0, 0, "\0", "\0", NONE, FALSE }
};

#endif	/* NUT_MGE_UTALK_H_SEEN */
