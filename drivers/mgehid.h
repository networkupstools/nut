/*  mgehid.h - data to monitor MGE UPS SYSTEMS USB/HID devices with NUT
 *
 *  Copyright (C) 2003 
 *  			Arnaud Quette <arnaud.quette@mgeups.fr>
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

#define MGE_HID_VERSION	"MGE HID 0.6"


/* HID2NUT lookup table */
hid_info_t hid_mge[] = {
  /* Server side variables */
  { "driver.version.internal", ST_FLAG_STRING, 5, NULL,
    DRIVER_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  { "driver.version.data", ST_FLAG_STRING, 11, NULL,
    MGE_HID_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  
  /* Battery page */
  { "battery.charge", 0, 1, "UPS.PowerSummary.RemainingCapacity", "%.0f", HU_FLAG_OK, NULL },
  { "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 5, 
    "UPS.PowerSummary.RemainingCapacityLimitSetting", "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  { "battery.charge.low", ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimit",
    "%.0f", HU_FLAG_OK | HU_FLAG_STATIC , NULL }, /* Read only */
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", "%.0f", HU_FLAG_OK, NULL },
  { "battery.temperature", 0, 0, 
    "UPS.BatterySystem.Battery.Temperature", "%.1f", HU_FLAG_OK, NULL },
  { "battery.voltage",  0, 0, "UPS.PowerSummary.Voltage", "%.1f", HU_FLAG_OK, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.BatterySystem.ConfigVoltage",
    "%.1f", HU_FLAG_OK, NULL },

  /* UPS page */
  { "ups.load", 0, 1, "UPS.PowerSummary.PercentLoad", "%.0f", HU_FLAG_OK, NULL },
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerSummary.DelayBeforeShutdown", "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL},
  { "ups.delay.reboot", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerSummary.DelayBeforeReboot", "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL},
  { "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerSummary.DelayBeforeStartup", "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL},
  { "ups.test.result", 0, 0,
    "UPS.BatterySystem.Battery.Test", "%s", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, &test_read_info[0] },
  { "ups.test.interval", ST_FLAG_RW | ST_FLAG_STRING, 6,
    "UPS.BatterySystem.Battery.TestPeriod", "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  { "ups.temperature", 0, 0, 
    "UPS.PowerSummary.Temperature", "%.1f", HU_FLAG_OK, NULL },

  /* Special case: ups.status */
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ACPresent", 
    "%.0f", HU_FLAG_OK, &onbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Discharging", 
    "%.0f", HU_FLAG_OK, &discharging_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Charging", 
    "%.0f", HU_FLAG_OK, &charging_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", 
    "%.0f", HU_FLAG_OK, &lowbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.OverLoad", 
    "%.0f", HU_FLAG_OK, &overbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.NeedReplacement", 
    "%.0f", HU_FLAG_OK, &replacebatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ShutdownImminent", 
    "%.0f", HU_FLAG_OK, &shutdownimm_info[0] },
  { "ups.status", 0, 1, "UPS.PowerConverter.Input.[1].PresentStatus.Buck", 
    "%.0f", HU_FLAG_OK, &trim_info[0] },
  { "ups.status", 0, 1, "UPS.PowerConverter.Input.[1].PresentStatus.Boost", 
    "%.0f", HU_FLAG_OK, &boost_info[0] },

  /* Input page */
  { "input.voltage", 0, 0, "UPS.PowerConverter.Input.[1].Voltage", "%.1f", HU_FLAG_OK, NULL },
  { "input.frequency", 0, 0, "UPS.PowerConverter.Input.[1].Frequency", "%.1f", HU_FLAG_OK, NULL },

  /* same as "input.transfer.boost.low" */
  { "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerConverter.Output.LowVoltageTransfer", "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.boost.low", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerConverter.Output.LowVoltageBoostTransfer", "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.boost.high", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerConverter.Output.HighVoltageBoostTransfer", "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.trim.low", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerConverter.Output.LowVoltageBuckTransfer", "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  /* same as "input.transfer.trim.high" */
  { "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerConverter.Output.HighVoltageTransfer", "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.trim.high", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerConverter.Output.HighVoltageBuckTransfer", "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  
  /* Output page */
  { "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", "%.1f", HU_FLAG_OK, NULL },
  { "output.current", 0, 0, "UPS.PowerConverter.Output.Current", "%.2f", HU_FLAG_OK, NULL },
  { "output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", "%.1f", HU_FLAG_OK, NULL },
  { "output.voltage.target.line", 0, 0, 
    "UPS.PowerSummary.ConfigVoltage", "%.1f", HU_FLAG_OK, NULL },
  { "output.voltage.target.battery", 0, 0,
    "UPS.PowerSummary.ConfigVoltage", "%.1f", HU_FLAG_OK, NULL },
  
  /* Outlet page (using MGE UPS SYSTEMS - PowerShare technology) */
  /* TODO: add an iterative semantic [%x] to factorise outlets */
  /* Warning: the rule is "i+&" for hidpath */
  /*{ "outlet.%i.id", 0, 0, "UPS.OutletSystem.Outlet.[%i+1].OutletID", "%i", HU_FLAG_OK, NULL },	
    { "outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "", "PowerShare Outlet%i%+1 ",
    HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
    { "outlet.%i.switchable", 0, 0, "UPS.OutletSystem.Outlet.[%i%+1].PresentStatus.Switchable",
    "%i", HU_FLAG_OK, NULL },
    { "outlet.%i.switch", ST_FLAG_RW, 0, "UPS.OutletSystem.Outlet.[%i%+1].PresentStatus.SwitchOn/Off",
    "%i", HU_FLAG_OK, NULL },
    { "outlet.%i.autoswitch.charge.low", ST_FLAG_RW, 0,
    "UPS.OutletSystem.Outlet.[%i%+1].RemainingCapacityLimit", "%i", HU_FLAG_OK, NULL },
    { "outlet.%i.delay.shutdown", ST_FLAG_RW, 0, "UPS.OutletSystem.Outlet.[%i%+1].DelayBeforeShutdown",
    "%i", HU_FLAG_OK, NULL },
    { "outlet.%i.delay.start", ST_FLAG_RW, 0, "UPS.OutletSystem.Outlet.[%i%+1].DelayBeforeStartup",
    "%i", HU_FLAG_OK, NULL },
  */

  /* instant commands. */
  /* splited into subset while waiting for extradata support
   * ie: test.battery.start quick
   */
  { "test.battery.start.quick", 0, 0,
    "UPS.BatterySystem.Battery.Test", "1", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] }, /* TODO: lookup needed? */
  { "test.battery.start.deep", 0, 0,
    "UPS.BatterySystem.Battery.Test", "2", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] },
  { "test.battery.stop", 0, 0,
    "UPS.BatterySystem.Battery.Test", "3", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] },
  { "load.off", 0, 0,
    "UPS.PowerSummary.DelayBeforeShutdown", "0", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "load.on", 0, 0,
    "UPS.PowerSummary.DelayBeforeStartup", "0", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },

  /* TODO: beeper.on/off, bypass.start/stop, shutdown.return/stayoff/stop/reboot[.graceful] */

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, 0, NULL }
};
