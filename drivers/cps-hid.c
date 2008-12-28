/* cps-hid.c - subdriver to monitor CPS USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2008 Arnaud Quette <arnaud.quette@free.fr>
 *  2005 - 2006 Peter Selinger <selinger@users.sourceforge.net>         
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  path-to-subdriver script. It must be customized.
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

#include "usbhid-ups.h"
#include "cps-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "main.h"     /* for getval() */
#include "common.h"
#include "usb-common.h"

#define CPS_HID_VERSION      "CyberPower HID 0.2"

/* Cyber Power Systems */
#define CPS_VENDORID 0x0764

/*
 * For some devices, the reported battery voltage is off by factor
 * of 1.5 so we need to apply a scale factor to it to get the real
 * battery voltage. By default, the factor is 1 (no scaling).
 */
static double	battery_scale = 1;

static void *cps_battery_scale(void)
{
	battery_scale = 0.667;
	return NULL;
}

/* USB IDs device table */
static usb_device_id_t cps_usb_device_table[] = {
	/* 900AVR/BC900D, CP1200AVR/BC1200D */
	{ USB_DEVICE(CPS_VENDORID, 0x0005), NULL },
	/* Dynex DX-800U? */
	{ USB_DEVICE(CPS_VENDORID, 0x0501), &cps_battery_scale },

	/* Terminating entry */
	{ -1, -1, NULL }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *cps_battvolt_fun(double value)
{
	static char	buf[8];

	snprintf(buf, sizeof(buf), "%.1f", battery_scale * value);

	return buf;
}

static info_lkp_t cps_battvolt[] = {
	{ 0, NULL, &cps_battvolt_fun }
};

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* CPS usage table */
static usage_lkp_t cps_usage_lkp[] = {
	{  NULL, 0x0 }
};

static usage_tables_t cps_utab[] = {
	cps_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t cps_hid2nut[] = {
  /* { "unmapped.ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", 0, NULL }, */
  /* { "unmapped.ups.powersummary.capacitymode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL }, */
  /* { "unmapped.ups.powersummary.designcapacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", 0, NULL }, */
  /* { "unmapped.ups.powersummary.capacitygranularity1", 0, 0, "UPS.PowerSummary.CapacityGranularity1", NULL, "%.0f", 0, NULL }, */
  /* { "unmapped.ups.powersummary.capacitygranularity2", 0, 0, "UPS.PowerSummary.CapacityGranularity2", NULL, "%.0f", 0, NULL }, */
  /* { "unmapped.ups.powersummary.fullchargecapacity", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", 0, NULL }, */

  /* Battery page */
  { "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", 0, stringid_conversion },
  { "battery.mfr.date", 0, 0, "UPS.PowerSummary.iOEMInformation", NULL, "%s", 0, stringid_conversion },
  { "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", 0, NULL },
  { "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
  { "battery.runtime.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.RemainingTimeLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%s", 0, cps_battvolt },

  /* UPS page */
  { "ups.load", 0, 0, "UPS.Output.PercentLoad", NULL, "%.0f", 0, NULL },
  { "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", 0, beeper_info },
  { "ups.test.result", 0, 0, "UPS.Output.Test", NULL, "%s", 0, test_read_info },
  { "ups.realpower.nominal", 0, 0, "UPS.Output.ConfigActivePower", NULL, "%.0f", 0, NULL },
  { "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_FLAG_ABSENT, NULL},
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL},
  { "ups.timer.start", 0, 0, "UPS.Output.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  { "ups.timer.shutdown", 0, 0, "UPS.Output.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  { "ups.timer.reboot", 0, 0, "UPS.Output.DelayBeforeReboot", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},

  /* Special case: ups.status & ups.alarm */
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyCharged", NULL, NULL, 0, fullycharged_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL, NULL, 0, timelimitexpired_info },
  { "BOOL", 0, 0, "UPS.Output.Boost", NULL, NULL, 0, boost_info },
  { "BOOL", 0, 0, "UPS.Output.Overload", NULL, NULL, 0, overload_info },

  /* Input page */
  { "input.voltage.nominal", 0, 0, "UPS.Input.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "input.voltage", 0, 0, "UPS.Input.Voltage", NULL, "%.1f", 0, NULL },
  { "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Input.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Input.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },

  /* Output page */
  { "output.voltage", 0, 0, "UPS.Output.Voltage", NULL, "%.1f", 0, NULL },
  { "output.voltage.nominal", 0, 0, "UPS.Output.ConfigVoltage", NULL, "%.0f", 0, NULL },

  /* instant commands. */
  { "test.battery.start.quick", 0, 0, "UPS.Output.Test", NULL, "1", HU_TYPE_CMD, NULL },
  { "test.battery.start.deep", 0, 0, "UPS.Output.Test", NULL, "2", HU_TYPE_CMD, NULL },
  { "test.battery.stop", 0, 0, "UPS.Output.Test", NULL, "3", HU_TYPE_CMD, NULL },
  { "load.off.delay", 0, 0, "UPS.Output.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
  { "load.on.delay", 0, 0, "UPS.Output.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },
  { "shutdown.stop", 0, 0, "UPS.Output.DelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD, NULL },
  { "shutdown.reboot", 0, 0, "UPS.Output.DelayBeforeReboot", NULL, "10", HU_TYPE_CMD, NULL },
  { "beeper.on", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
  { "beeper.off", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },
  { "beeper.enable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
  { "beeper.disable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "1", HU_TYPE_CMD, NULL },
  { "beeper.mute", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static char *cps_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static char *cps_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "CPS";
}

static char *cps_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int cps_claim(HIDDevice_t *hd) {

	int status = is_usb_device_supported(cps_usb_device_table, hd->VendorID,
								 hd->ProductID);

	switch (status) {

		case POSSIBLY_SUPPORTED:
			/* by default, reject, unless the productid option is given */
			if (getval("productid")) {
				return 1;
			}
			possibly_supported("CyberPower", hd);
			return 0;

		case SUPPORTED:
			return 1;

		case NOT_SUPPORTED:
		default:
			return 0;
	}
}

subdriver_t cps_subdriver = {
	CPS_HID_VERSION,
	cps_claim,
	cps_utab,
	cps_hid2nut,
	cps_format_model,
	cps_format_mfr,
	cps_format_serial,
};
