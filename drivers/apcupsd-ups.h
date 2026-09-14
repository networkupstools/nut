/* apcupsd-ups.h - NUT client driver to apcupsd

   Copyright (C)
       2005 - 2010  Arnaud Quette <http://arnaud.quette.free.fr/contact.html>

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

#ifndef NUT_APCUPSD_UPS_H_SEEN
#define NUT_APCUPSD_UPS_H_SEEN 1

/* from usbhid-ups.h */
/* --------------------------------------------------------------- */
/* Struct & data for ups.status processing                         */
/* --------------------------------------------------------------- */

typedef struct {
	const char	*status_str;	/* ups.status string */
	int		status_value;	/* ups.status value */
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

/*
static status_lkp_t status_info[] = {
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
*/
/* from usbhid-ups.h */

typedef struct {
	char	hid_value;	/* HID value */
	char	*nut_value;	/* NUT value */
} info_enum_t;

/* --------------------------------------------------------------- */
/* Structure containing information about how to get/set data      */
/* from/to the UPS and convert these to/from NUT standard          */
/* --------------------------------------------------------------- */

typedef struct {
	const char	*apcupsd_item;
	const char	*info_type;	/* NUT variable name */
	int	info_flags;	/* NUT flags (to set in addinfo) */
	float	info_len;	/* if ST_FLAG_STRING: length of the string */
				/* if HU_TYPE_CMD: command value ; multiplier (or max len) otherwise */
	const char	*default_value;	/* if HU_FLAG_ABSENT: default value ; format otherwise */
	int	drv_flags;	/* */
	char	**var_values;	/* all possible values for this variable (allows to check data...) */
				/* FIXME: "void *" so we can have bound or enum */
/*	interpreter interpret;	*/	/* FFE: interpreter fct, NULL if not needed  */
} apcuspd_info_t;

/* data flags */
#define DU_FLAG_NONE			0
#define DU_FLAG_INIT			1		/* intialy show element to upsd */
#define DU_FLAG_STATUS			2
#define DU_FLAG_DATE			4
#define DU_FLAG_TIME			8
#define DU_FLAG_FW1			16
#define DU_FLAG_FW2			32
#define DU_FLAG_PRESERVE 	64

/* ------------ */
/*  Data table  */
/* ------------ */

static apcuspd_info_t nut_data[] =
{
	{ NULL, "ups.mfr", ST_FLAG_STRING | ST_FLAG_RW, 32, "APC", DU_FLAG_INIT, NULL },
	{ "MODEL", "ups.model", ST_FLAG_STRING | ST_FLAG_RW, 32, "Unknown UPS", DU_FLAG_INIT, NULL },
	{ "STATUS", "ups.status", ST_FLAG_STRING | ST_FLAG_RW, 32, "OFF", DU_FLAG_INIT|DU_FLAG_STATUS, NULL },
	{ "SERIALNO", "ups.serial", ST_FLAG_STRING | ST_FLAG_RW, 32, NULL, DU_FLAG_NONE, NULL },
	{ "LOADPCT", "ups.load", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "DATE", "ups.time", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_TIME, NULL },
	{ "DATE", "ups.date", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_DATE, NULL },
	{ "MANDATE", "ups.mfr.date", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_NONE, NULL },
	{ "FIRMWARE", "ups.firmware", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_FW1, NULL },
	{ "FIRMWARE", "ups.firmware.aux", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_FW2, NULL },
	{ "ITEMP", "ups.temperature", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "UPSNAME", "ups.id", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_NONE, NULL },
	{ "DWAKE", "ups.delay.start", ST_FLAG_RW, 1, "%.0f", DU_FLAG_NONE, NULL },
	{ "DSHUTD", "ups.delay.shutdown", ST_FLAG_RW, 1, "%.0f", DU_FLAG_NONE, NULL },
	{ "STESTI", "ups.test.interval", ST_FLAG_RW, 1, "%.0f", DU_FLAG_NONE, NULL },
	{ "SELFTEST", "ups.test.result", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_NONE, NULL },
	{ "LINEV", "input.voltage", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "MAXLINEV", "input.voltage.maximum", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "MINLINEV", "input.voltage.minimum", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "NOMINV", "input.voltage.nominal", 0, 1, "%.0f", DU_FLAG_NONE, NULL },
	{ "NOMOUTV", "output.voltage.nominal", 0, 1, "%.0f", DU_FLAG_NONE, NULL },
	{ "LASTXFER", "input.transfer.reason", ST_FLAG_STRING | ST_FLAG_RW, 32, NULL, DU_FLAG_NONE, NULL },
	{ "LOTRANS", "input.transfer.low", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "HITRANS", "input.transfer.high", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "SENSE", "input.sensitivity", ST_FLAG_STRING | ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "LINEFREQ", "input.frequency", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "OUTPUTV", "output.voltage", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "LINEFREQ", "output.frequency", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "BCHARGE", "battery.charge", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_PRESERVE, NULL },
	{ "MBATTCHG", "battery.charge.low", ST_FLAG_RW, 1, "%.0f", DU_FLAG_NONE, NULL },
	{ "BATTDATE", "battery.date", ST_FLAG_STRING /* | ST_FLAG_RW */, 16, NULL, DU_FLAG_DATE, NULL },
	{ "BATTV", "battery.voltage", 0, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "NOMBATTV", "battery.voltage.nominal", 0, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "TIMELEFT", "battery.runtime", ST_FLAG_RW, 60, "%1.1f", DU_FLAG_PRESERVE, NULL },
	{ "MINTIMEL", "battery.runtime.low", ST_FLAG_RW, 60, "%.0f", DU_FLAG_NONE, NULL },
	{ "RETPCT", "battery.charge.restart", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "NOMPOWER", "ups.realpower.nominal", 0, 1, "%1.1f", DU_FLAG_INIT, NULL },
	{ "LOAD_W", "ups.realpower", 0, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "LOADAPNT", "power.percent", ST_FLAG_RW, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "OUTCURNT", "output.current", 0, 1, "%1.2f", DU_FLAG_NONE, NULL },
	{ "LOAD_VA", "ups.power", 0, 1, "%1.1f", DU_FLAG_NONE, NULL },
	{ "NOMAPNT", "ups.power.nominal", 0, 1, "%.0f", DU_FLAG_INIT, NULL },
	{ NULL, NULL, 0, 0, NULL, DU_FLAG_NONE, NULL }
};

#endif  /* NUT_APCUPSD_UPS_H_SEEN */
