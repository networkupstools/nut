/*  mgehid.h - data to monitor MGE UPS SYSTEMS USB/HID devices with NUT
 *
 *  Copyright (C)  
 *	2003 - 2005	Arnaud Quette <arnaud.quette@free.fr>
 *	2005		John Stamp <kinsayder@hotmail.com>
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

#define APC_HID_VERSION	"APC HID 0.8"

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
	{ NULL, NULL, -1, "Generic APC HID model" }
};


/* HID2NUT lookup table */
hid_info_t hid_apc[] = {
  /* Server side variables */
  { "driver.version.internal", ST_FLAG_STRING, sizeof(DRIVER_VERSION), NULL, NULL,
    DRIVER_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  { "driver.version.data", ST_FLAG_STRING, sizeof(APC_HID_VERSION), NULL, NULL,
    APC_HID_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  
  /* Battery page */
  { "battery.charge", 0, 1, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.charge.low", ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimit", NULL,
    "%.0f", HU_FLAG_OK, NULL }, /* Read only */
  { "battery.charge.warning", ST_FLAG_STRING, 5, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL }, /* Read only */
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.runtime.low", 0, 0, "UPS.Battery.RemainingTimeLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.voltage",  0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.Battery.ConfigVoltage", NULL,
    "%.1f", HU_FLAG_OK, NULL },
  { "battery.temperature", 0, 0,
    "UPS.Battery.Temperature", NULL, "%.1f", HU_FLAG_OK, NULL },

  /* UPS page */
  { "ups.load", 0, 1, "UPS.Output.PercentLoad", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "ups.load", 0, 1, "UPS.PowerConverter.PercentLoad", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL},
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL},
  { "ups.test.result", 0, 0,
    "UPS.Battery.Test", NULL, "%s", HU_FLAG_OK, &test_read_info[0] },
  /* the below one need to be discussed as we might need to complete
   * the ups.test sub collection
   * { "ups.test.panel", 0, 0,
   *   "UPS.APCPanelTest", NULL, "%.0f", HU_FLAG_OK, NULL }, */
  { "ups.temperature", 0, 0,
    "UPS.Battery.Temperature", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "ups.beeper.status", 0, 0, "UPS.AudibleAlarmControl", NULL, "%s", HU_FLAG_OK, &beeper_info[0] },
  { "ups.mfr.date", 0, 0, "UPS.ManufacturerDate", NULL, "%s", HU_FLAG_OK, &date_conversion[0] },
  { "battery.mfr.date", 0, 0, "UPS.Battery.ManufacturerDate", NULL, "%s", HU_FLAG_OK, &date_conversion[0] },
  { "battery.date", 0, 0, "UPS.Battery.APCBattReplaceDate", NULL, "%s", HU_FLAG_OK, &date_conversion[0] },

  /* Special case: ups.status */
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ACPresent", NULL,
    "%.0f", HU_FLAG_OK, &onbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Discharging",NULL, 
    "%.0f", HU_FLAG_OK, &discharging_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Charging", NULL,
    "%.0f", HU_FLAG_OK, &charging_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL,
    "%.0f", HU_FLAG_OK, &shutdownimm_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL,
    "%.0f", HU_FLAG_OK, &lowbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.OverLoad", NULL,
    "%.0f", HU_FLAG_OK, &overbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL,
    "%.0f", HU_FLAG_OK, &replacebatt_info[0] },

  /* Input page */
  { "input.voltage", 0, 0, "UPS.Input.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "input.voltage.nominal", 0, 0, "UPS.Input.ConfigVoltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.transfer.low", 0, 0, "UPS.Input.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.transfer.high", 0, 0, "UPS.Input.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },

  /* Output page */
  { "output.voltage", 0, 0, "UPS.Output.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "output.voltage.target.line", 0, 0,
    "UPS.Output.ConfigVoltage", NULL, "%.1f", HU_FLAG_OK, NULL },

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
  { "test.panel.start", 0, 0,
    "UPS.APCPanelTest", NULL, "1", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "test.panel.stop", 0, 0,
    "UPS.APCPanelTest", NULL, "0", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "load.off", 0, 0,
    "UPS.PowerSummary.DelayBeforeShutdown", NULL, "0", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "load.off", 0, 0,
    "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, "0", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "load.on", 0, 0,
    "UPS.PowerSummary.DelayBeforeStartup", NULL, "0", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },

	/* FIXME (@John): is it a good mapping considering the behaviour? */
	{ "shutdown.return", 0, 0, "UPS.APCGeneralCollection.APCForceShutdown",
		NULL, "1", /* point to good value */
		HU_TYPE_CMD | HU_FLAG_OK, NULL },

  { "shutdown.stop", 0, 0,
    "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, "-1", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "shutdown.stop", 0, 0,
    "UPS.PowerSummary.DelayBeforeShutdown", NULL, "-1", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },

  { "beeper.on", 0, 0, "UPS.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "beeper.off", 0, 0, "UPS.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD | HU_FLAG_OK, NULL },

  /* TODO: beeper.on/off, bypass.start/stop, shutdown.return/stayoff/stop/reboot[.graceful] */

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};
