/* newhidups.c - Driver for serial/USB HID UPS units
 *
 * Copyright (C) 2003 - 2005
 *   Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
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

#define DRIVER_VERSION		"0.24"

/* --------------------------------------------------------------- */
/*      Supported Manufacturers IDs                                */
/* --------------------------------------------------------------- */

#define MGE_UPS_SYSTEMS		0x0463		/* All models */
#define APC			0x051d		/* All models */
/* Unsupported! (need spec/hardware/help) */
#define MUSTEK			0x0665		/* models: 0x5161... */
#define TRIPPLITE		0x09ae		/* models IDs? */
#define UNITEK			0x0F03		/* models: 0x0001... */


/* --------------------------------------------------------------- */
/*      Model Name formating entries                               */
/* --------------------------------------------------------------- */

typedef struct
{
	char	*iProduct;
	char	*iModel;
	int	comp_size;	/* size of the comparison, -1 for full */
	char	*finalname;
} models_name_t;

/* Driver's parameters */
#define HU_VAR_ONDELAY		"ondelay"
#define HU_VAR_OFFDELAY		"offdelay"
#define HU_VAR_POLLFREQ		"pollfreq"

/* Parameters default values */
#define DEFAULT_ONDELAY		30	/* Delay between return of utility power */
					/* and powering up of load, in seconds */
					/* CAUTION: ondelay > offdelay */
#define DEFAULT_OFFDELAY	20	/* Delay before power off, in seconds */ 
#define DEFAULT_POLLFREQ	30	/* Polling interval, in seconds */
					/* The driver will wait for Interrupt */
					/* and do "light poll" in the meantime */

#define MAX_STRING_SIZE    	128


/* FIXME: remaining "unused" items => need integration */
#define BATT_MFRDATE		0x850085	/* manufacturer date         */
#define BATT_ICHEMISTRY		0x850089	/* battery type              */
#define BATT_IOEMINFORMATION	0x85008f	/* battery OEM description   */


/* --------------------------------------------------------------- */
/* Struct & data for ups.status processing                         */
/* --------------------------------------------------------------- */

typedef struct {
	char	*status_str;	/* ups.status string */
	int	status_value;	/* ups.status value */
} status_lkp_t;

#define STATUS_CAL		1       /* calibration */
#define STATUS_TRIM		2       /* SmartTrim */
#define STATUS_BOOST		4       /* SmartBoost */
#define STATUS_OL		8       /* on line */
#define STATUS_OB		16      /* on battery */
#define STATUS_OVER		32      /* overload */
#define STATUS_LB		64      /* low battery */
#define STATUS_RB		128     /* replace battery */
#define STATUS_BYPASS		256	/* on bypass */
#define STATUS_OFF		512	/* ups is off */
#define STATUS_CHRG		1024	/* charging */
#define STATUS_DISCHRG		2048	/* discharging */

status_lkp_t status_info[] = {
  { "CAL", STATUS_CAL },
  { "TRIM", STATUS_TRIM },
  { "BOOST", STATUS_BOOST },
  { "OL", STATUS_OL },
  { "OB", STATUS_OB },
  { "OVER", STATUS_OVER },
  { "LB", STATUS_LB },
  { "RB", STATUS_RB },
  { "BYPASS", STATUS_BYPASS },
  { "OFF", STATUS_OFF },
  { "CHRG", STATUS_CHRG },
  { "DISCHRG", STATUS_DISCHRG },
  { "NULL", 0 },
};


/* --------------------------------------------------------------- */
/* Struct & data for lookup between HID and NUT values             */
/* (From USB/HID, Power Devices Class standard)                    */
/* --------------------------------------------------------------- */

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
/* FIXME: extend ups.status for BYPASS Manual/Automatic */
info_lkp_t bypass_info[] = {
  { 1, "BYPASS" },
  { 0, "NULL" }
};
info_lkp_t off_info[] = {
  { 0, "OFF" },
  { 0, "NULL" }
};
/* FIXME: add CAL */

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

/* --------------------------------------------------------------- */
/* Structure containing information about how to get/set data      */
/* from/to the UPS and convert these to/from NUT standard          */
/* --------------------------------------------------------------- */

typedef struct {
	char	*info_type;		/* NUT variable name */
	int	info_flags;		/* NUT flags (to set in addinfo) */
	float	info_len;		/* if ST_FLAG_STRING: length of the string */
					/* if HU_TYPE_CMD: command value ; multiplier otherwise */
	char	*hidpath;		/* Full HID Object path (or NULL for server side vars) */
	int	**numericpath;		/* Full HID Object numeric path (for caching purpose, filled at runtime) */
	char	*dfl;			/* if HU_FLAG_ABSENT: default value ; format otherwise */
	unsigned long hidflags;		/* driver's own flags */
	info_lkp_t *hid2info;		/* lookup table between HID and NUT values */

/*	char *info_HID_format;	*//* FFE: HID format for complex values */
/*	interpreter interpret;	*//* FFE: interpreter fct, NULL if not needed  */
/*	void *next;			*//* next hid_info_t */
} hid_info_t;


/* Data walk modes */
#define HU_WALKMODE_INIT		1
#define HU_WALKMODE_QUICK_UPDATE	2
#define HU_WALKMODE_FULL_UPDATE		3

/* TODO: rework flags */
#define HU_FLAG_OK			1		/* show element to upsd. */
#define HU_FLAG_STATIC			2		/* retrieve info only once. */
#define HU_FLAG_SEMI_STATIC		4		/* retrieve info smartly */
#define HU_FLAG_ABSENT			8		/* data is absent in the device, */
							/* use default value. */
#define HU_FLAG_QUICK_POLL		16		/* Mandatory vars	*/		
#define HU_FLAG_STALE			32		/* data stale, don't try too often. */

/* hints for su_ups_set, applicable only to rw vars */
#define HU_TYPE_CMD				64		/* instant command */

#define HU_CMD_MASK		0x2000

#define HU_INFOSIZE		128

#define MAX_TRY		2		/* max number of GetItem retry */

/* TODO: create an Mfr table (int VendorID, hid_info_t *hid_mfr, ...) */
