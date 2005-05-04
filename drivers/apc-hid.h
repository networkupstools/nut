/*  mgehid.h - data to monitor MGE UPS SYSTEMS USB/HID devices with NUT
 *
 *  Copyright (C) 2004 
 *  			Arnaud Quette <arnaud.quette@free.fr>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://www.mgeups.com>
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#define APC_HID_VERSION	"APC HID 0.6"

/* --------------------------------------------------------------- */
/*      Model Name formating entries                               */
/* --------------------------------------------------------------- */

models_name_t apc_models_names [] =
{

  { "BackUPS 500", "500", -1, "BackUPS 500" },

/*   { "BackUPS Pro", 11, NULL, "FW", 0 }, */
/*   { "Back-UPS ES", 11, NULL, "FW", 0 },  */
/*   { "Smart-UPS", 9, NULL, "FW",  0 }, */
/*   { "BackUPS ", 8, NULL, " ", 0 }, */


	/* end of structure. */
	{ NULL, NULL, -1, "Generic MGE HID model" }
};


/* HID2NUT lookup table */
hid_info_t hid_apc[] = {
  /* Server side variables */
  { "driver.version.internal", ST_FLAG_STRING, 5, NULL, NULL,
    DRIVER_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  { "driver.version.data", ST_FLAG_STRING, 11, NULL, NULL,
    APC_HID_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  
  /* Battery page */
  { "battery.charge", 0, 1, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.charge.low", ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimit", NULL,
    "%.0f", HU_FLAG_OK, NULL }, /* Read only */
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.voltage",  0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.Battery.ConfigVoltage", NULL,
    "%.1f", HU_FLAG_OK, NULL },

  /* UPS page */
  { "ups.load", 0, 1, "UPS.DelayBeforeShutdown.PercentLoad", NULL, "%.0f", HU_FLAG_OK, NULL }, /* FIXME: strange path! */
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL},
  { "ups.test.result", 0, 0,
    "UPS.Battery.Test", NULL, "%s", HU_FLAG_OK, &test_read_info[0] },

  /* Special case: ups.status */
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ACPresent", NULL,
    "%.0f", HU_FLAG_OK, &onbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Discharging",NULL, 
    "%.0f", HU_FLAG_OK, &discharging_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Charging", NULL,
    "%.0f", HU_FLAG_OK, &charging_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL,
    "%.0f", HU_FLAG_OK, &lowbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.OverLoad", NULL,
    "%.0f", HU_FLAG_OK, &overbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL,
    "%.0f", HU_FLAG_OK, &replacebatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL,
    "%.0f", HU_FLAG_OK, &shutdownimm_info[0] },

  /* Input page */
  
  /* Output page */
  
  /* Outlet page (using MGE UPS SYSTEMS - PowerShare technology) */

  /* instant commands. */
  /* splited into subset while waiting for extradata support
   * ie: test.battery.start quick
   */
  { "test.battery.start.quick", 0, 0,
    "UPS.BatterySystem.Battery.Test", NULL, "1", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] }, /* TODO: lookup needed? */
  { "test.battery.start.deep", 0, 0,
    "UPS.BatterySystem.Battery.Test", NULL, "2", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] },
  { "test.battery.stop", 0, 0,
    "UPS.BatterySystem.Battery.Test", NULL, "3", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] },
  { "load.off", 0, 0,
    "UPS.PowerSummary.DelayBeforeShutdown", NULL, "0", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "load.on", 0, 0,
    "UPS.PowerSummary.DelayBeforeStartup", NULL, "0", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },

  /* TODO: beeper.on/off, bypass.start/stop, shutdown.return/stayoff/stop/reboot[.graceful] */

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};
