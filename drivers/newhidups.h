/* newhidups.c - New prototype HID UPS driver for Network UPS Tools
 *
 * Copyright (C) 2003-2004
 * Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"

#define DRIVER_VERSION		"0.12"

/* FIXME: make a Mfr/models table?! */
#define MGE_UPS_SYSTEMS		0x0463		/* All models */
#define APC					0x051d
#define MUSTEK				0x0665		/* models: 0x5161... */

/* TODO: add other VendorID with HID compliant devices */

/* for formating finely model name */
typedef struct {
  int	VendorID;	/* ... */
  char	*basename;	/* Name start by ... */
  int	size;		/* size of comparison */
  char *finalname;	/* replace basename with this */
  char *data2;		/* complement iModel (MGE), or string to be searched for (APC) */
  int d2_offset; /* */
} models_name_t;

models_name_t models_names [] = {
	/* MGE UPS SYSTEMS - EMOA models */
	{ MGE_UPS_SYSTEMS, "ELLIPSE", 7,"ellipse", "UPS.Flow.[4].ConfigApparentPower", 0 },
	{ MGE_UPS_SYSTEMS, "ellipse", 7, "ellipse premium", "UPS.PowerSummary.iModel", 2 },
	{ MGE_UPS_SYSTEMS, "Evolution", 9, "evolution", "UPS.PowerSummary.iModel", 0 },
	{ MGE_UPS_SYSTEMS, "EXtreme", 7, "Pulsar EXtreme", "UPS.PowerSummary.iModel", 0 },
	{ MGE_UPS_SYSTEMS, "PROTECTIONCENTER", 16, "Protection Center", "UPS.PowerSummary.iModel", 0 },
	/* MGE UPS SYSTEMS - US models */
	{ MGE_UPS_SYSTEMS, "EX", 2, "Pulsar EX", "UPS.PowerSummary.iModel", 0 },
  
  /* TODO: add other MGE devices => ESPRIT, GALAXY (3000_10 => 3000 10), ?PwTrust2? */

  { APC, "BackUPS Pro", 11, NULL, "FW", 0 },
  { APC, "Back-UPS ES", 11, NULL, "FW", 0 }, 
  { APC, "Smart-UPS", 9, NULL, "FW",  0 },
  { APC, "BackUPS ", 8, NULL, " ", 0 },

  /* end of structure. */
  { 0, NULL, 0, NULL, NULL, 0 }
};

#define DEFAULT_ONDELAY		30	/* delay between return of utility power */
					/* and powering up of load, in seconds */
					/* CAUTION: ondelay > offdelay */
#define DEFAULT_OFFDELAY	20	/* delay before power off, in seconds */ 


/* TODO: remaining "unused" items => need integration */
#define BATT_MFRDATE		0x850085	/* manufacturer date         */
#define BATT_ICHEMISTRY		0x850089	/* battery type              */
#define BATT_IOEMINFORMATION	0x85008f	/* battery OEM description   */


/* for lookup between HID values and NUT values*/
typedef struct {
	long	hid_value;	/* HID value */
	char	*nut_value;	/* NUT value */
} info_lkp_t;

/* Actual value lookup tables => should be fine for all Mfrs (TODO: validate it!) */
info_lkp_t onbatt_info[] = {
  { 0, "OB" },
  { 1, "OL" },
  { 0, "NULL" }
};
info_lkp_t discharging_info[] = {
  { 1, "DISCHRG" },
  { 0, "NULL" }
};
info_lkp_t charging_info[] = {
  { 1, "CHRG" },
  { 0, "NULL" }
};
info_lkp_t lowbatt_info[] = {
  { 1, "LB" },
  { 0, "NULL" }
};
info_lkp_t overbatt_info[] = {
  { 1, "OVER" },
  { 0, "NULL" }
};
info_lkp_t replacebatt_info[] = {
  { 1, "RB" },
  { 0, "NULL" }
};
info_lkp_t shutdownimm_info[] = {
  { 1, "LB" },
  { 0, "NULL" }
};
info_lkp_t trim_info[] = {
  { 1, "TRIM" },
  { 0, "NULL" }
};
info_lkp_t boost_info[] = {
  { 1, "BOOST" },
  { 0, "NULL" }
};
/* TODO: add BYPASS, OFF, CAL */

info_lkp_t test_write_info[] = {
  { 0, "No test" },
  { 1, "Quick test" },
  { 2, "Deep test" },
  { 3, "Abort test" },
  { 0, "NULL" }
};
info_lkp_t test_read_info[] = {
  { 1, "Done and passed" },
  { 2, "Done and warning" },
  { 3, "Done and error" },
  { 4, "Aborted" },
  { 5, "In progress" },
  { 6, "No test initiated" },
  { 0, "NULL" }
};

/* Structure containing info about one item that can be requested
   from UPS and set in INFO.  If no interpreter functions is defined,
   use sprintf with given format string.  If unit is not NONE, values
   are converted according to the multiplier table
=> TODO: this description must be updated
*/
typedef struct {
	char	*info_type;		/* INFO_ or CMD_ element */
	int		info_flags;		/* flags to set in addinfo */
	float	info_len;		/* length of strings if STR, */
					/* cmd value if CMD, multiplier otherwise. */
	char	*hidpath;		/* Full HID Object path or NULL */
	char	*dfl;			/* default value (hidflags = ABSENT), format otherwise */
	unsigned long hidflags;		/* my flags */
	info_lkp_t *hid2info;		/* lookup table between HID and NUT values */
/*	char *info_HID_format;		*//* FFE: HID format for complex values */
/*	interpreter interpret;		*//* FFE: interpreter fct, NULL if not needed  */
/*	void *next;			*//* next snmp_info_t */
} hid_info_t;

/* TODO: rework flags */
#define HU_FLAG_OK				1				/* show element to upsd. */
#define HU_FLAG_STATIC			2				/* retrieve info only once. */
#define HU_FLAG_SEMI_STATIC		4				/* retrieve info only once. */
#define HU_FLAG_ABSENT			8				/* data is absent in the device, */
													/* use default value. */
#define HU_FLAG_STALE			16				/* data stale, don't try too often. */

/* hints for su_ups_set, applicable only to rw vars */
#define HU_TYPE_CMD				32				/* instant command */

#define HU_CMD_MASK		0x2000

#define HU_INFOSIZE		128

#define MAX_TRY		2		/* max number of GetItem retry */

/* TODO: create an Mfr table (int VendorID, hid_info_t *hid_mfr, ...) */
