/* dummy-ups.h - new testing driver

   Copyright (C) 2005  Arnaud Quette <http://arnaud.quette.free.fr/contact.html>

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

/* This file list all valid data with their type and info.
 * this are then enable through a definition file, specified
 * as the "port" parameter (only the file name, not the path).
 * The format of this file is the same as an upsc dump:
 * <varname>: <value>
 * ...
 * Once the driver is loaded:
 * - change the value by using upsrw
 * - ?you can add new variables and change variable values by
 *   editing the definition file?
 */

/* from usbhid-ups.h */
/* --------------------------------------------------------------- */
/* Struct & data for ups.status processing                         */
/* --------------------------------------------------------------- */

typedef struct {
	char	*status_str;	/* ups.status string */
	int	status_value;	/* ups.status value */
} status_lkp_t;

#define STATUS_CAL		1       /* calibration */
#define STATUS_TRIM		2       /* SmartTrim */
#define STATUS_BOOST	4       /* SmartBoost */
#define STATUS_OL		8       /* on line */
#define STATUS_OB		16      /* on battery */
#define STATUS_OVER		32      /* overload */
#define STATUS_LB		64      /* low battery */
#define STATUS_RB		128     /* replace battery */
#define STATUS_BYPASS	256		/* on bypass */
#define STATUS_OFF		512		/* ups is off */
#define STATUS_CHRG		1024	/* charging */
#define STATUS_DISCHRG	2048	/* discharging */

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
	char	*info_type;	/* NUT variable name */
	int	info_flags;	/* NUT flags (to set in addinfo) */
	float	info_len;	/* if ST_FLAG_STRING: length of the string */
				/* if HU_TYPE_CMD: command value ; multiplier (or max len) otherwise */
	char	*default_value;	/* if HU_FLAG_ABSENT: default value ; format otherwise */
	int	drv_flags;	/* */
	char	**var_values;	/* all possible values for this variable (allows to check data...) */
				/* FIXME: "void *" so we can have bound or enum */
/*	interpreter interpret;	*/	/* FFE: interpreter fct, NULL if not needed  */
} dummy_info_t;

/* data flags */
#define DU_FLAG_NONE			0
#define DU_FLAG_INIT			1		/* intialy show element to upsd */
#define DU_TYPE_CMD			2		

/* --------------------------------------------------------------- */
/*  Data table (all possible info from NUT, then enable upon cong  */
/* --------------------------------------------------------------- */

/* FIXME: need to enforce value check with enum or bounds */
dummy_info_t nut_data[] =
{
	/* Essential variables, loaded before parsing the definition file */
	{ "ups.mfr", ST_FLAG_STRING | ST_FLAG_RW, 32, "Dummy Manufacturer", DU_FLAG_INIT, NULL },
	{ "ups.model", ST_FLAG_STRING | ST_FLAG_RW, 32, "Dummy UPS", DU_FLAG_INIT, NULL },
	{ "ups.status", ST_FLAG_STRING | ST_FLAG_RW, 32, "OL", DU_FLAG_INIT, NULL },
	/* Other known variables */
	{ "ups.serial", ST_FLAG_STRING | ST_FLAG_RW, 32, NULL, DU_FLAG_NONE, NULL },
	{ "ups.load", ST_FLAG_RW, 32, NULL, DU_FLAG_NONE, NULL },
	{ "ups.alarm", ST_FLAG_STRING | ST_FLAG_RW, 32, NULL, DU_FLAG_NONE, NULL },
	{ "ups.time", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_NONE, NULL },
	{ "ups.date", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_NONE, NULL },
	{ "ups.mfr.date", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_NONE, NULL },
	{ "ups.serial", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_NONE, NULL },
	{ "ups.firmware", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_NONE, NULL },
	{ "ups.firmware.aux", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_NONE, NULL },
	{ "ups.temperature", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "ups.id", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_NONE, NULL },
	{ "ups.delay.start", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "ups.delay.reboot", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "ups.delay.shutdown", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "ups.test.interval", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "ups.test.result", ST_FLAG_STRING | ST_FLAG_RW, 16, NULL, DU_FLAG_NONE, NULL },
/*
ups.display.language
ups.contacts
ups.power
ups.power.nominal
*/
	{ "input.voltage", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "input.voltage.maximum", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "input.voltage.minimum", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "input.voltage.nominal", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "input.transfer.reason", ST_FLAG_STRING | ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "input.transfer.low", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "input.transfer.high", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "input.sensitivity", ST_FLAG_STRING | ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "input.quality", ST_FLAG_STRING | ST_FLAG_RW, 6, NULL, DU_FLAG_NONE, NULL },
	{ "input.frequency", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "input.transfer.boost.low", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "input.transfer.boost.high", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "input.transfer.trim.low", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "input.transfer.trim.high", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "output.voltage", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "output.frequency", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "output.current", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
/*
output.voltage.nominal
*/
	{ "battery.charge", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "battery.charge.low", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "battery.voltage", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "battery.current", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "battery.runtime", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "battery.runtime.low", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "battery.charge.restart", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "battery.temperature", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
	{ "battery.voltage.nominal", ST_FLAG_RW, 1, NULL, DU_FLAG_NONE, NULL },
/*
battery.alarm.threshold
battery.date
battery.packs
battery.packs.bad

ambient.temperature
ambient.temperature.alarm
ambient.temperature.high
ambient.temperature.low
ambient.humidity
ambient.humidity.alarm
ambient.humidity.high
ambient.humidity.low

FIXME: how to manage these?
outlet.n.id
outlet.n.desc
outlet.n.switch
outlet.n.status
outlet.n.switchable
outlet.n.autoswitch.charge.low
outlet.n.delay.shutdown
outlet.n.delay.start

driver.name
driver.version
driver.version.internal | Internal driver version    | 1.23.45         |
driver.parameter.xxx
driver.flag.xxx

No need for these!
server.info
server.version

Cmds
load.off
load.on
shutdown.return
shutdown.stayoff
shutdown.stop
shutdown.reboot
shutdown.reboot.graceful
test.panel.start
test.panel.stop
test.failure.start
test.failure.stop
test.battery.start
test.battery.stop
calibrate.start
calibrate.stop
bypass.start
bypass.stop
reset.input.minmax
reset.watchdog
beeper.enable
beeper.disable
*/

	/* end of structure. */
	{ NULL, 0, 0, NULL, DU_FLAG_NONE, NULL }
};
