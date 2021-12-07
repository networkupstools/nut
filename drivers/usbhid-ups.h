/* usbhid-ups.h - Driver for serial/USB HID UPS units
 *
 * Copyright (C)
 *  2003-2009 Arnaud Quette <http://arnaud.quette.free.fr/contact.html>
 *  2005-2006 Peter Selinger <selinger@users.sourceforge.net>
 *  2007-2009 Arjen de Korte <adkorte-guest@alioth.debian.org>
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

#ifndef USBHID_UPS_H
#define USBHID_UPS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "libhid.h"

extern hid_dev_handle_t	udev;
extern bool_t	 	use_interrupt_pipe;	/* Set to FALSE if interrupt reports should not be used */

/* Driver's parameters */
#define HU_VAR_LOWBATT		"lowbatt"
#define HU_VAR_ONDELAY		"ondelay"
#define HU_VAR_OFFDELAY		"offdelay"
#define HU_VAR_POLLFREQ		"pollfreq"

/* Parameters default values */
#define DEFAULT_LOWBATT		"30"	/* percentage of battery charge to consider the UPS in low battery state  */
#define DEFAULT_ONDELAY		"30"	/* Delay between return of utility power */
					/* and powering up of load, in seconds */
					/* CAUTION: ondelay > offdelay */
#define DEFAULT_OFFDELAY	"20"	/* Delay before power off, in seconds */
#define DEFAULT_POLLFREQ	30	/* Polling interval, in seconds */
					/* The driver will wait for Interrupt */
					/* and do "light poll" in the meantime */

#ifndef MAX_STRING_SIZE
#define MAX_STRING_SIZE	128
#endif


/* --------------------------------------------------------------- */
/* Struct & data for lookup between HID and NUT values             */
/* (From USB/HID, Power Devices Class standard)                    */
/* --------------------------------------------------------------- */

typedef struct {
	const long	hid_value;	/* HID value */
	const char	*nut_value;	/* NUT value */
	const char	*(*fun)(double hid_value);	/* optional HID to NUT mapping */
	double	(*nuf)(const char *nut_value);		/* optional NUT to HID mapping */
} info_lkp_t;

/* accessor on the status */
extern unsigned ups_status_get(void);

/* declarations of public lookup tables */
/* boolean status values from UPS */
extern info_lkp_t online_info[];
extern info_lkp_t discharging_info[];
extern info_lkp_t charging_info[];
extern info_lkp_t lowbatt_info[];
extern info_lkp_t overload_info[];
extern info_lkp_t replacebatt_info[];
extern info_lkp_t trim_info[];
extern info_lkp_t boost_info[];
extern info_lkp_t bypass_auto_info[];
extern info_lkp_t bypass_manual_info[];
extern info_lkp_t off_info[];
extern info_lkp_t calibration_info[];
extern info_lkp_t nobattery_info[];
extern info_lkp_t fanfail_info[];
extern info_lkp_t shutdownimm_info[];
extern info_lkp_t overheat_info[];
extern info_lkp_t awaitingpower_info[];
extern info_lkp_t commfault_info[];
extern info_lkp_t timelimitexpired_info[];
extern info_lkp_t battvoltlo_info[];
extern info_lkp_t battvolthi_info[];
extern info_lkp_t chargerfail_info[];
extern info_lkp_t emergency_stop_info[];
extern info_lkp_t fullycharged_info[];
extern info_lkp_t depleted_info[];

/* input.transfer.reason */
extern info_lkp_t vrange_info[];
extern info_lkp_t frange_info[];

/* non specific */
extern info_lkp_t test_write_info[];
extern info_lkp_t test_read_info[];
extern info_lkp_t beeper_info[];
extern info_lkp_t yes_no_info[];
extern info_lkp_t on_off_info[];
extern info_lkp_t date_conversion[];
extern info_lkp_t hex_conversion[];
extern info_lkp_t stringid_conversion[];
extern info_lkp_t divide_by_10_conversion[];
extern info_lkp_t kelvin_celsius_conversion[];

/* ---------------------------------------------------------------------- */
/* data for processing boolean values from UPS */

#define	STATUS(x)	((unsigned)1<<x)

typedef enum {
	ONLINE = 0,	/* on line */
	DISCHRG,	/* discharging */
	CHRG,		/* charging */
	LOWBATT,	/* low battery */
	OVERLOAD,	/* overload */
	REPLACEBATT,	/* replace battery */
	SHUTDOWNIMM,	/* shutdown imminent */
	TRIM,		/* SmartTrim */
	BOOST,		/* SmartBoost */
	BYPASSAUTO,	/* on automatic bypass */
	BYPASSMAN,	/* on manual/service bypass */
	OFF,		/* ups is off */
	CAL,		/* calibration */
	OVERHEAT,	/* overheat; Belkin, TrippLite */
	COMMFAULT,	/* UPS fault; Belkin, TrippLite */
	DEPLETED,	/* battery depleted; Belkin */
	TIMELIMITEXP,	/* time limit expired; APC */
	FULLYCHARGED,	/* battery full; CyberPower */
	AWAITINGPOWER,	/* awaiting power; Belkin, TrippLite */
	FANFAIL,	/* fan failure; MGE */
	NOBATTERY,	/* battery missing; MGE */
	BATTVOLTLO,	/* battery voltage too low; MGE */
	BATTVOLTHI,	/* battery voltage too high; MGE */
	CHARGERFAIL,	/* battery charger failure; MGE */
	VRANGE,		/* voltage out of range */
	FRANGE		/* frequency out of range */
} status_bit_t;

/* --------------------------------------------------------------- */
/* Structure containing information about how to get/set data      */
/* from/to the UPS and convert these to/from NUT standard          */
/* --------------------------------------------------------------- */

typedef struct {
	const char	*info_type;		/* NUT variable name */
	int	info_flags;		/* NUT flags (to set in addinfo) */
	int	info_len;		/* if ST_FLAG_STRING: length of the string */
					/* if HU_TYPE_CMD: command value */
	const char	*hidpath;		/* Full HID Object path (or NULL for server side vars) */
	HIDData_t *hiddata;		/* Full HID Object data (for caching purpose, filled at runtime) */
	const char	*dfl;			/* if HU_FLAG_ABSENT: default value ; format otherwise */
	unsigned long hidflags;		/* driver's own flags */
	info_lkp_t *hid2info;		/* lookup table between HID and NUT values */
								/* if HU_FLAG_ENUM is set, hid2info is also used
								 * as enumerated values (dstate_addenum()) */

/*	char *info_HID_format;	*//* FFE: HID format for complex values */
/*	interpreter interpret;	*//* FFE: interpreter fct, NULL if not needed  */
/*	void *next;			*//* next hid_info_t */
} hid_info_t;

/* TODO: rework flags */
#define HU_FLAG_STATIC			2		/* retrieve info only once. */
#define HU_FLAG_SEMI_STATIC		4		/* retrieve info smartly */
#define HU_FLAG_ABSENT			8		/* data is absent in the device, */
							/* use default value. */
#define HU_FLAG_QUICK_POLL		16		/* Mandatory vars	*/
#define HU_FLAG_STALE			32		/* data stale, don't try too often. */
#define HU_FLAG_ENUM			128		/* enum values exist */

/* hints for su_ups_set, applicable only to rw vars */
#define HU_TYPE_CMD				64		/* instant command */

#define HU_CMD_MASK		0x2000

#define HU_INFOSIZE		128

#define MAX_TRY		2		/* max number of GetItem retry */

/* --------------------------------------------------------------- */
/*      Subdriver interface                                        */
/* --------------------------------------------------------------- */

/* subdriver structure. Each subdriver is intended to support a
 * particular manufacturer (e.g. MGE, APC, Belkin), or a particular
 * range of models. */

typedef struct {
	const char *name;                  /* name of this subdriver */
	int (*claim)(HIDDevice_t *hd); /* return 1 if device covered by
				      * this subdriver */
	usage_tables_t *utab;        /* points to array of usage tables */
	hid_info_t *hid2nut;         /* main table of vars and instcmds */
	const char *(*format_model)(HIDDevice_t *hd);  /* driver-specific methods */
	const char *(*format_mfr)(HIDDevice_t *hd);    /* for preparing human-    */
	const char *(*format_serial)(HIDDevice_t *hd); /* readable information    */
} subdriver_t;

/* the following functions are exported for the benefit of subdrivers */
int instcmd(const char *cmdname, const char *extradata);
int setvar(const char *varname, const char *val);

void possibly_supported(const char *mfr, HIDDevice_t *hd);

#endif /* USBHID_UPS_H */
