/*  mgehid.h - data to monitor MGE UPS SYSTEMS HID (USB and serial) devices
 *
 *  Copyright (C) 2003 - 2005
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

#define MGE_HID_VERSION	"MGE HID 0.7"

/* --------------------------------------------------------------- */
/*      Model Name formating entries                               */
/* --------------------------------------------------------------- */

models_name_t mge_models_names [] =
{
	/* Ellipse models */
	{ "ELLIPSE", "300", -1, "ellipse 300" },
	{ "ELLIPSE", "500", -1, "ellipse 500" },
	{ "ELLIPSE", "650", -1, "ellipse 650" },
	{ "ELLIPSE", "800", -1, "ellipse 800" },
	{ "ELLIPSE", "1200", -1, "ellipse 1200" },
	/* Ellipse Premium models */
	{ "ellipse", "PR500", -1, "ellipse premium 500" },
	{ "ellipse", "PR650", -1, "ellipse premium 650" },
	{ "ellipse", "PR800", -1, "ellipse premium 800" },
	{ "ellipse", "PR1200", -1, "ellipse premium 1200" },
	/* Ellipse "Pro" */
	{ "ELLIPSE", "600", -1, "Ellipse 600" },
	{ "ELLIPSE", "750", -1, "Ellipse 750" },
	{ "ELLIPSE", "1000", -1, "Ellipse 1000" },
	{ "ELLIPSE", "1500", -1, "Ellipse 1500" },
	/* Evolution models */
	{ "Evolution", "500", -1, "Pulsar Evolution 500" },
	{ "Evolution", "800", -1, "Pulsar Evolution 800" },
	{ "Evolution", "1100", -1, "Pulsar Evolution 1100" },
	{ "Evolution", "1500", -1, "Pulsar Evolution 1500" },
	{ "Evolution", "2200", -1, "Pulsar Evolution 2200" },
	{ "Evolution", "3000", -1, "Pulsar Evolution 3000" },
	{ "Evolution", "3000XL", -1, "Pulsar Evolution 3000 XL" },
	/* NOVA models */	
	{ "NOVA AVR", "600", -1, "NOVA 600 AVR" },
	{ "NOVA AVR", "1100", -1, "NOVA 1100 AVR" },
	/* EXtreme C (EMEA) */
	{ "EXtreme", "700C", -1, "Pulsar EXtreme 700C" },
	{ "EXtreme", "1000C", -1, "Pulsar EXtreme 1000C" },
	{ "EXtreme", "1500C", -1, "Pulsar EXtreme 1500C" },
	{ "EXtreme", "1500CCLA", -1, "Pulsar EXtreme 1500C CLA" },
	{ "EXtreme", "2200C", -1, "Pulsar EXtreme 2200C" },
	{ "EXtreme", "3200C", -1, "Pulsar EXtreme 3200C" },
	/* EXtreme C (USA, aka "EX RT") */
	{ "EX", "700RT", -1, "Pulsar EX 700 RT" },
	{ "EX", "1000RT", -1, "Pulsar EX 1000 RT" },
	{ "EX", "1500RT", -1, "Pulsar EX 1500 RT" },
	{ "EX", "2200RT", -1, "Pulsar EX 2200 RT" },
	{ "EX", "3200RT", -1, "Pulsar EX 3200 RT" },
	/* Comet EX RT */
	{ "EX", "5RT", -1, "EX 5 RT" },
	{ "EX", "7RT", -1, "EX 7 RT" },
	{ "EX", "11RT", -1, "EX 11 RT" },

	/* Galaxy 3000 */
	{ "GALAXY", "3000_10", -1, "Galaxy 3000 10 kVA" },
	{ "GALAXY", "3000_15", -1, "Galaxy 3000 15 kVA" },
	{ "GALAXY", "3000_20", -1, "Galaxy 3000 20 kVA" },
	{ "GALAXY", "3000_30", -1, "Galaxy 3000 30 kVA" },

	/* FIXME: To be completed (Comet, Galaxy, Esprit, ...) */

	/* end of structure. */
	{ NULL, NULL, -1, "Generic MGE HID model" }
};

/* --------------------------------------------------------------- */
/*                 Data lookup table (HID <-> NUT)                 */
/* --------------------------------------------------------------- */
hid_info_t hid_mge[] =
{
  /* Server side variables */
  { "driver.version.internal", ST_FLAG_STRING, 5, NULL, NULL,
    DRIVER_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  { "driver.version.data", ST_FLAG_STRING, 11, NULL, NULL,
    MGE_HID_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  
  /* Battery page */
  { "battery.charge", 0, 1, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 5, 
    "UPS.PowerSummary.RemainingCapacityLimitSetting", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  { "battery.charge.low", ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimit", NULL,
    "%.0f", HU_FLAG_OK | HU_FLAG_STATIC , NULL }, /* Read only */
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.temperature", 0, 0, 
    "UPS.BatterySystem.Battery.Temperature", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "battery.voltage",  0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.BatterySystem.ConfigVoltage", NULL,
    "%.1f", HU_FLAG_OK, NULL },

  /* UPS page */
  { "ups.load", 0, 1, "UPS.PowerSummary.PercentLoad", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL},
  { "ups.delay.reboot", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerSummary.DelayBeforeReboot", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL},
  { "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerSummary.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL},
  { "ups.test.result", 0, 0,
    "UPS.BatterySystem.Battery.Test", NULL, "%s", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, &test_read_info[0] },
  { "ups.test.interval", ST_FLAG_RW | ST_FLAG_STRING, 8,
    "UPS.BatterySystem.Battery.TestPeriod", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  { "ups.temperature", 0, 0, 
    "UPS.PowerSummary.Temperature", NULL, "%.1f", HU_FLAG_OK, NULL },
  /* FIXME: miss ups.power */
  { "ups.power.nominal", ST_FLAG_STRING, 5, "UPS.Flow.[4].ConfigApparentPower",
	NULL, "%.0f",HU_FLAG_OK, NULL },

  /* Special case: ups.status */
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, 
    "%.0f", HU_FLAG_OK | HU_FLAG_QUICK_POLL, &onbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Discharging", NULL, 
    "%.0f", HU_FLAG_OK | HU_FLAG_QUICK_POLL, &discharging_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Charging", NULL, 
    "%.0f", HU_FLAG_OK | HU_FLAG_QUICK_POLL, &charging_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL,
    "%.0f", HU_FLAG_OK, &lowbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.OverLoad", NULL,
    "%.0f", HU_FLAG_OK, &overbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL,
    "%.0f", HU_FLAG_OK, &replacebatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL,
    "%.0f", HU_FLAG_OK | HU_FLAG_QUICK_POLL, &shutdownimm_info[0] },
  { "ups.status", 0, 1, "UPS.PowerConverter.Input.[1].PresentStatus.Buck", NULL,
    "%.0f", HU_FLAG_OK, &trim_info[0] },
  { "ups.status", 0, 1, "UPS.PowerConverter.Input.[1].PresentStatus.Boost", NULL,
    "%.0f", HU_FLAG_OK, &boost_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Good", NULL,
    "%.0f", HU_FLAG_OK, &off_info[0] },  
  /* FIXME: extend ups.status for BYPASS: */
  /* Manual bypass */
  { "ups.status", 0, 1, "UPS.PowerConverter.Input[4].PresentStatus.Used", NULL,
    "%.0f", HU_FLAG_OK, &bypass_info[0] },  
  /* Automatic bypass */
  { "ups.status", 0, 1, "UPS.PowerConverter.Input[2].PresentStatus.Used", NULL,
    "%.0f", HU_FLAG_OK, &bypass_info[0] },

  /* Input page */
  { "input.voltage", 0, 0, "UPS.PowerConverter.Input.[1].Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "input.frequency", 0, 0, "UPS.PowerConverter.Input.[1].Frequency", NULL, "%.1f", HU_FLAG_OK, NULL },
  /* same as "input.transfer.boost.low" */
  { "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerConverter.Output.LowVoltageTransfer", NULL, "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.boost.low", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerConverter.Output.LowVoltageBoostTransfer", NULL, "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.boost.high", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerConverter.Output.HighVoltageBoostTransfer", NULL, "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.trim.low", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerConverter.Output.LowVoltageBuckTransfer", NULL, "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  /* same as "input.transfer.trim.high" */
  { "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerConverter.Output.HighVoltageTransfer", NULL, "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.trim.high", ST_FLAG_RW | ST_FLAG_STRING, 5,
    "UPS.PowerConverter.Output.HighVoltageBuckTransfer", NULL, "%.1f", HU_FLAG_OK | HU_FLAG_SEMI_STATIC, NULL },
  
  /* Output page */
  { "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "output.current", 0, 0, "UPS.PowerConverter.Output.Current", NULL, "%.2f", HU_FLAG_OK, NULL },
  { "output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "output.voltage.target.line", 0, 0, 
    "UPS.PowerSummary.ConfigVoltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  /* FIXME: not the good one! */
  { "output.voltage.target.battery", 0, 0,
    "UPS.PowerSummary.ConfigVoltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  
	/* Outlet page (using MGE UPS SYSTEMS - PowerShare technology) */
	/* TODO: add an iterative semantic [%x] to factorise outlets */
	{ "outlet.0.id", 0, 0, "UPS.OutletSystem.Outlet.[1].OutletID", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_STATIC, NULL },
	{ "outlet.0.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "UPS.OutletSystem.Outlet.[1].OutletID",
	  NULL, "Main Outlet", HU_FLAG_ABSENT | HU_FLAG_OK | HU_FLAG_STATIC, NULL },
	{ "outlet.0.switchable", 0, 0, "UPS.OutletSystem.Outlet.[1].PresentStatus.Switchable",
	  NULL, "%.0f", HU_FLAG_OK | HU_FLAG_STATIC, NULL },
	{ "outlet.1.id", 0, 0, "UPS.OutletSystem.Outlet.[2].OutletID", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_STATIC, NULL },	
	{ "outlet.1.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "UPS.OutletSystem.Outlet.[2].OutletID",
	  NULL, "PowerShare Outlet 1", HU_FLAG_ABSENT | HU_FLAG_OK | HU_FLAG_STATIC, NULL },
	{ "outlet.1.switchable", 0, 0, "UPS.OutletSystem.Outlet.[2].PresentStatus.Switchable",
	  NULL, "%.0f", HU_FLAG_OK | HU_FLAG_STATIC, NULL },
	{ "outlet.1.switch", ST_FLAG_RW | ST_FLAG_STRING, 2, "UPS.OutletSystem.Outlet.[2].PresentStatus.SwitchOn/Off",
	  NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "outlet.1.autoswitch.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 3,
	  "UPS.OutletSystem.Outlet.[2].RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "outlet.1.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5,
	  "UPS.OutletSystem.Outlet.[2].DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "outlet.1.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5,
	  "UPS.OutletSystem.Outlet.[2].DelayBeforeStartup", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "outlet.2.id", 0, 0, "UPS.OutletSystem.Outlet.[3].OutletID", NULL, "%.0f", HU_FLAG_OK | HU_FLAG_STATIC, NULL },	
	{ "outlet.2.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "UPS.OutletSystem.Outlet.[3].OutletID",
	  NULL, "PowerShare Outlet 2", HU_FLAG_ABSENT | HU_FLAG_OK | HU_FLAG_STATIC, NULL },
	{ "outlet.2.switchable", 0, 0, "UPS.OutletSystem.Outlet.[3].PresentStatus.Switchable",
	  NULL, "%.0f", HU_FLAG_OK | HU_FLAG_STATIC, NULL },
	{ "outlet.2.switch", ST_FLAG_RW | ST_FLAG_STRING, 2, "UPS.OutletSystem.Outlet.[3].PresentStatus.SwitchOn/Off",
	  NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "outlet.2.autoswitch.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 3,
	  "UPS.OutletSystem.Outlet.[3].RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "outlet.2.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5,
	  "UPS.OutletSystem.Outlet.[3].DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL },
	{ "outlet.2.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5,
	  "UPS.OutletSystem.Outlet.[3].DelayBeforeStartup", NULL, "%.0f", HU_FLAG_OK, NULL },

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
