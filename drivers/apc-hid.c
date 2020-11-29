/*  apc-hid.c - data to monitor APC USB/HID devices with NUT
 *
 *  Copyright (C)
 *	2003 - 2009	Arnaud Quette <arnaud.quette@free.fr>
 *	2005		John Stamp <kinsayder@hotmail.com>
 *	2005		Peter Selinger <selinger@users.sourceforge.net>
 *	2009 - 2010	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://www.mgeups.com>
 *   and Eaton <http://www.eaton.com>
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

#include "main.h"     /* for getval() */
#include "usbhid-ups.h"
#include "apc-hid.h"
#include "usb-common.h"

#define APC_HID_VERSION "APC HID 0.96"

/* APC */
#define APC_VENDORID 0x051d

/* Tweaks */
static char * tweak_max_report[] = {
	/* Back-UPS ES 700 does NOT overflow. */
	/* Back-UPS ES 725 does NOT overflow. */
	/* Back-UPS ES 525 overflows on ReportID 0x0c
	   (UPS.PowerSummary.RemainingCapacity). */
	"Back-UPS ES 525",
	/* Back-UPS CS 650 overflows on ReportID 0x46 */
	"Back-UPS CS",
	NULL};

/* Don't use interrupt pipe on 5G models (used by proprietary protocol) */
static void *disable_interrupt_pipe(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	if (use_interrupt_pipe == TRUE) {
		/* FIXME? Suggest data from "device" to help the setup below? */
		upslogx(LOG_INFO, "interrupt pipe disabled (add 'pollonly' flag to 'ups.conf' to get rid of this message)");
		use_interrupt_pipe= FALSE;
	}
	return NULL;
}

/* Some models need special tweaks */
static void *general_apc_check(USBDevice_t *device)
{
	int i = 0;

	if (!device->Product) {
		upslogx(LOG_WARNING, "device->Product is NULL so it is not possible to determine whether to activate max_report_size workaround");
		return NULL;
	}

	/* Some models of Back-UPS overflow on some ReportID.
	 * This results in some data not being exposed and IO errors on
	 * WIN32, causing endless reconnection or driver's failure */

	while( tweak_max_report[i] != NULL ) {
		if(!strncmp(device->Product, tweak_max_report[i],
			strlen(tweak_max_report[i]))) {
			max_report_size = 1;
			return NULL;
		}
		i++;
	}
	return NULL;
}

/* USB IDs device table */
static usb_device_id_t apc_usb_device_table[] = {
	/* APC AP9584 Serial->USB kit */
	{ USB_DEVICE(APC_VENDORID, 0x0000), NULL },
	/* various models */
	{ USB_DEVICE(APC_VENDORID, 0x0002), general_apc_check },
	/* various 5G models */
	{ USB_DEVICE(APC_VENDORID, 0x0003), disable_interrupt_pipe },

	/* Terminating entry */
	{ -1, -1, NULL }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static const char *apc_date_conversion_fun(double value)
{
	static char buf[20];
	int year, month, day;

	if ((long)value == 0) {
		return "not set";
	}

	/* APC apparently uses a hexadecimal-as-decimal format, e.g.,
	0x102202 = October 22, 2002 */
	year = ((long)value & 0xf) + 10 * (((long)value>>4) & 0xf);
	month = (((long)value>>16) & 0xf) + 10 * (((long)value>>20) & 0xf);
	day = (((long)value>>8) & 0xf) + 10 * (((long)value>>12) & 0xf);

	/* Y2K conversion - hope that this format will be retired before 2070 :) */
	if (year >= 70) {
		year += 1900;
	} else {
		year += 2000;
	}

	snprintf(buf, sizeof(buf), "%04d/%02d/%02d", year, month, day);

	return buf;
}

static info_lkp_t apc_date_conversion[] = {
	{ 0, NULL, apc_date_conversion_fun, NULL }
};

/* This was determined empirically from observing a BackUPS LS 500 */
static info_lkp_t apcstatusflag_info[] = {
	{ 8, "!off", NULL, NULL },	/* Normal operation */
	{ 16, "!off", NULL, NULL },	/* This occurs briefly during power-on, and corresponds to status 'DISCHRG'. */
	{ 0, "off", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

/* Reason of the last battery transfer (from apcupsd) */
static info_lkp_t apc_linefailcause_vrange_info[] = {
	{ 1, "vrange", NULL, NULL },	/* Low line voltage */
	{ 2, "vrange", NULL, NULL },	/* High line voltage */
	{ 4, "vrange", NULL, NULL },	/* notch, spike, or blackout */
	{ 8, "vrange", NULL, NULL },	/* Notch or blackout */
	{ 9, "vrange", NULL, NULL },	/* Spike or blackout */
	{ 0, "!vrange", NULL, NULL },	/* No transfers have ocurred */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t apc_linefailcause_frange_info[] = {
	{ 7, "frange", NULL, NULL },		/* Input frequency out of range */
	{ 0, "!frange", NULL, NULL },		/* No transfers have ocurred */
	{ 0, NULL, NULL, NULL }
};

#if 0
/* these input.transfer.reason can't be mapped at the moment... */
	{ 3, "ripple", NULL, NULL },		/* Ripple */
	{ 5, "self test", NULL, NULL },	/* Self Test or Discharge Calibration commanded
	                                 * Test usage, front button, or 2 week self test */
	{ 6, "forced", NULL, NULL },		/* DelayBeforeShutdown or APCDelayBeforeShutdown */
	{ 10, "forced", NULL, NULL },		/* Graceful shutdown by accessories */
	{ 11, "self test", NULL, NULL },	/* Test usage invoked */
	{ 12, "self test", NULL, NULL },	/* Front button initiated self test */
	{ 13, "self test", NULL, NULL },	/* 2 week self test */
	{ 0, NULL, NULL, NULL }
#endif

static info_lkp_t apc_sensitivity_info[] = {
	{ 0, "low", NULL, NULL },
	{ 1, "medium", NULL, NULL },
	{ 2, "high", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* APC usage table */
static usage_lkp_t apc_usage_lkp[] = {
	{ "APCGeneralCollection",	0xff860005 },
	{ "APCEnvironment",		0xff860006 },
	{ "APCProbe1",			0xff860007 },
	{ "APCProbe2",			0xff860008 },
	{ "APCBattReplaceDate",		0xff860016 },
	/* usage seen in dumps but unknown:
	 * - ff860018
	 * Path: UPS.Battery.ff860018, Type: Feature, ReportID: 0x48, Offset: 0, Size: 32, Value: 0
	 */
	{ "APCBattCapBeforeStartup",	0xff860019 }, /* FIXME: exploit */
	/* usage seen in dumps but unknown:
	 * - ff86001a
	 * Path: UPS.Battery.ff86001a, Type: Input, ReportID: 0x1b, Offset: 0, Size: 8, Value: 3
	 * Path: UPS.Battery.ff86001a, Type: Feature, ReportID: 0x1b, Offset: 0, Size: 8, Value: 3
	 * - ff86001b
	 * Path: UPS.Battery.ff86001b, Type: Input, ReportID: 0x1c, Offset: 0, Size: 8, Value: 0
	 * Path: UPS.Battery.ff86001b, Type: Feature, ReportID: 0x1c, Offset: 0, Size: 8, Value: 0
	 * - ff860023
	 * Path: UPS.ff860001.ff860023, Type: Feature, ReportID: 0x60, Offset: 0, Size: 16, Value: 0
	 * - ff860024
	 * Path: UPS.Battery.ff860024, Type: Feature, ReportID: 0x47, Offset: 0, Size: 8, Value: 245
	 * Path: UPS.PowerConverter.ff860024, Type: Feature, ReportID: 0x51, Offset: 0, Size: 8, Value: 145
	 * - ff860025
	 * Path: UPS.ff860001.ff860025, Type: Feature, ReportID: 0x62, Offset: 0, Size: 32, Value: 0
	 * - ff860026
	 * Path: UPS.ff860001.ff860026, Type: Feature, ReportID: 0x61, Offset: 0, Size: 8, Value: 10
	 * - ff860027
	 * Path: UPS.ff860027, Type: Feature, ReportID: 0x3e, Offset: 0, Size: 32, Value: 0
	 * - ff860028
	 * Path: UPS.ff860028, Type: Feature, ReportID: 0x3f, Offset: 0, Size: 32, Value: 0
	 * - ff860030
	 * Path: UPS.Output.ff860030, Type: Feature, ReportID: 0x42, Offset: 0, Size: 16, Value: 5.8
	 */
	{ "APC_UPS_FirmwareRevision",	0xff860042 },
	{ "APCLineFailCause",	0xff860052 },
	{ "APCStatusFlag",		0xff860060 },
	{ "APCSensitivity",		0xff860061 },
	{ "APCPanelTest",		0xff860072 }, /* FIXME: exploit */
	{ "APCShutdownAfterDelay",	0xff860076 }, /* FIXME: exploit */
	{ "APC_USB_FirmwareRevision",	0xff860079 }, /* FIXME: exploit */
	{ "APCDelayBeforeReboot",	0xff86007c },
	{ "APCDelayBeforeShutdown",	0xff86007d },
	{ "APCDelayBeforeStartup",	0xff86007e }, /* FIXME: exploit */
	/* usage seen in dumps but unknown:
	 * - ff860080
	 * Path: UPS.PresentStatus.ff860080, Type: Input, ReportID: 0x33, Offset: 12, Size: 1, Value: 0
	 * Path: UPS.PresentStatus.ff860080, Type: Feature, ReportID: 0x33, Offset: 12, Size: 1, Value: 0
	 * Path: UPS.PowerSummary.PresentStatus.ff860080, Type: Input, ReportID: 0x07, Offset: 12, Size: 1, Value: 0
	 * Path: UPS.PowerSummary.PresentStatus.ff860080, Type: Feature, ReportID: 0x07, Offset: 12, Size: 1, Value: 0
	 * - ff860090, ff860091
	 * Path: UPS.ff860090.ff860091, Type: Feature, ReportID: 0x8c, Offset: 0, Size: 8, Value: 1.000000
	 * - ff860092
	 * Path: UPS.ff860090.ff860092, Type: Feature, ReportID: 0x8d, Offset: 0, Size: 8, Value: 25.000000
	 * - ff860093
	 * Path: UPS.ff860090.ff860093, Type: Feature, ReportID: 0x8e, Offset: 0, Size: 8, Value: 83.000000
	 * - ff860094
	 * Path: UPS.ff860090.ff860094, Type: Feature, ReportID: 0x8f, Offset: 0, Size: 8, Value: 0.000000
	 * - ff860095
	 * Path: UPS.ff860090.ff860095, Type: Feature, ReportID: 0x90, Offset: 0, Size: 8, Value: 1.000000
	 * - ff860096
	 * Path: UPS.ff860090.ff860096, Type: Feature, ReportID: 0x91, Offset: 0, Size: 16, Value: 4.000000
	 * - ff860097
	 * Path: UPS.ff860090.ff860097, Type: Feature, ReportID: 0x92, Offset: 0, Size: 16, Value: 4.000000
	 */

	/* Note (Arnaud): BUP stands for BackUPS Pro
	 * This is a HID uncompliant special (manufacturer) collection
	 * FIXME: these need to be used... */
	{ "BUPHibernate",		0x00850058 }, /* FIXME: exploit */
	{ "BUPBattCapBeforeStartup",	0x00860012 }, /* FIXME: exploit */
	{ "BUPDelayBeforeStartup",	0x00860076 }, /* FIXME: exploit */
	{ "BUPSelfTest",		0x00860010 }, /* FIXME: exploit */

	{ NULL, 0 }
};

/*
 * USB USAGE NOTES for APC (from Russell Kroll in the old hidups)
 *
 * FIXME: read 0xff86.... instead of 0x(00)86....?
 *
 *  0x860013 == 44200155090 - capability again
 *           == locale 4, 4 choices, 2 bytes, 00, 15, 50, 90
 *           == minimum charge to return online
 *
 *  0x860060 == "441HMLL" - looks like a 'capability' string
 *           == locale 4, 4 choices, 1 byte each
 *           == line sensitivity (high, medium, low, low)
 *  NOTE! the above does not seem to correspond to my info
 *
 *  0x860062 == D43133136127130
 *           == locale D, 4 choices, 3 bytes, 133, 136, 127, 130
 *           == high transfer voltage
 *
 *  0x860064 == D43103100097106
 *           == locale D, 4 choices, 3 bytes, 103, 100, 097, 106
 *           == low transfer voltage
 *
 *  0x860066 == 441HMLL (see 860060)
 *
 *  0x860074 == 4410TLN
 *           == locale 4, 4 choices, 1 byte, 0, T, L, N
 *           == alarm setting (5s, 30s, low battery, none)
 *
 *  0x860077 == 443060180300600
 *           == locale 4, 4 choices, 3 bytes, 060,180,300,600
 *           == wake-up delay (after power returns)
 */

static usage_tables_t apc_utab[] = {
	apc_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/*      HID2NUT lookup table                                       */
/* --------------------------------------------------------------- */

/* HID2NUT lookup table */
static hid_info_t apc_hid2nut[] = {
  /* Battery page */
  { "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
  { "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", 0, NULL },
  { "battery.runtime", 0, 0, "UPS.Battery.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
  { "battery.runtime.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Battery.RemainingTimeLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "battery.runtime.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.RemainingTimeLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "battery.voltage",  0, 0, "UPS.Battery.Voltage", NULL, "%.1f", 0, NULL },
  { "battery.voltage",  0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", 0, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.Battery.ConfigVoltage", NULL, "%.1f", 0, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.1f", 0, NULL }, /* Back-UPS 500 */
  { "battery.temperature", 0, 0, "UPS.Battery.Temperature", NULL, "%s", 0, kelvin_celsius_conversion },
  { "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", 0, stringid_conversion },
  { "battery.mfr.date", 0, 0, "UPS.Battery.ManufacturerDate", NULL, "%s", 0, date_conversion },
  { "battery.mfr.date", 0, 0, "UPS.PowerSummary.APCBattReplaceDate", NULL, "%s", 0, apc_date_conversion }, /* Back-UPS 500, Back-UPS ES/CyberFort 500 */
  { "battery.date", 0, 0, "UPS.Battery.APCBattReplaceDate", NULL, "%s", 0, apc_date_conversion }, /* Observed values: 0x0 on Back-UPS ES 650, 0x92501 on Back-UPS BF500 whose manufacture date was 2005/01/20 - this makes little sense but at least it's a valid date. */

  /* UPS page */
  { "ups.load", 0, 0, "UPS.Output.PercentLoad", NULL, "%.1f", 0, NULL },
  { "ups.load", 0, 0, "UPS.PowerConverter.PercentLoad", NULL, "%.0f", 0, NULL },
  /* USB HID PDC defaults */
  { "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_FLAG_ABSENT, NULL},
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL},
  { "ups.timer.start", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  { "ups.timer.shutdown", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  { "ups.timer.reboot", 0, 0, "UPS.PowerSummary.DelayBeforeReboot", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  /* used by APC SmartUPS RM */
  { "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_FLAG_ABSENT, NULL},
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL},
  { "ups.timer.start", 0, 0, "UPS.Output.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  { "ups.timer.shutdown", 0, 0, "UPS.Output.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  { "ups.timer.reboot", 0, 0, "UPS.Output.DelayBeforeReboot", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  /* used by APC BackUPS ES */
  { "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.APCGeneralCollection.APCDelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_FLAG_ABSENT, NULL},
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL},
  { "ups.timer.start", 0, 0, "UPS.APCGeneralCollection.APCDelayBeforeStartup", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  { "ups.timer.shutdown", 0, 0, "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  { "ups.timer.reboot", 0, 0, "UPS.APCGeneralCollection.APCDelayBeforeReboot", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  { "ups.test.result", 0, 0, "UPS.Battery.Test", NULL, "%s", 0, test_read_info },
  { "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", 0, beeper_info },
  { "ups.mfr.date", 0, 0, "UPS.ManufacturerDate", NULL, "%s", 0, date_conversion },
  { "ups.mfr.date", 0, 0, "UPS.PowerSummary.ManufacturerDate", NULL, "%s", 0, date_conversion }, /* Back-UPS 500 */
  { "ups.realpower.nominal", 0, 0, "UPS.PowerConverter.ConfigActivePower", NULL, "%.0f", 0, NULL },
  { "ups.realpower.nominal", 0, 0, "UPS.Output.ConfigActivePower", NULL, "%.0f", 0, NULL },

  /* the below one need to be discussed as we might need to complete
   * the ups.test sub collection
   * { "ups.test.panel", 0, 0, "UPS.APCPanelTest", NULL, "%.0f", 0, NULL }, */

  /* Special case: ups.status & ups.alarm */
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, NULL, 0, overload_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL, NULL, 0, timelimitexpired_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BatteryPresent", NULL, NULL, 0, nobattery_info },

  { "BOOL", 0, 0, "UPS.PowerSummary.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info }, /* Back-UPS 500 */
  { "BOOL", 0, 0, "UPS.PowerSummary.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info }, /* Back-UPS 500 */
  { "BOOL", 0, 0, "UPS.PowerSummary.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info }, /* Back-UPS 500 */
  { "BOOL", 0, 0, "UPS.PowerSummary.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info }, /* Back-UPS 500 */
  { "BOOL", 0, 0, "UPS.PowerSummary.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.APCStatusFlag", NULL, NULL, HU_FLAG_QUICK_POLL, apcstatusflag_info }, /* APC Back-UPS LS 500 */

  /* we map 2 times "input.transfer.reason" to be able to clear
   * both vrange (voltage) and frange (frequency) */
  { "BOOL", 0, 0, "UPS.Input.APCLineFailCause", NULL, NULL, 0, apc_linefailcause_vrange_info },
  { "BOOL", 0, 0, "UPS.Input.APCLineFailCause", NULL, NULL, 0, apc_linefailcause_frange_info },

  /* Input page */
  { "input.voltage", 0, 0, "UPS.Input.Voltage", NULL, "%.1f", 0, NULL },
  { "input.voltage.nominal", 0, 0, "UPS.Input.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  /* used by APC BackUPS RS */
  { "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Input.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Input.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "input.sensitivity", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Input.APCSensitivity", NULL, "%s", HU_FLAG_SEMI_STATIC, apc_sensitivity_info },

  /* Output page */
  { "output.voltage", 0, 0, "UPS.Output.Voltage", NULL, "%.1f", 0, NULL },
  { "output.voltage.nominal", 0, 0, "UPS.Output.ConfigVoltage", NULL, "%.1f", 0, NULL },
  { "output.current", 0, 0, "UPS.Output.Current", NULL, "%.2f", 0, NULL },
  { "output.frequency", 0, 0, "UPS.Output.Frequency", NULL, "%.1f", 0, NULL },

  /* Environmental page */
  { "ambient.temperature", 0, 0, "UPS.APCEnvironment.APCProbe1.Temperature", NULL, "%s", 0, kelvin_celsius_conversion },
  { "ambient.humidity", 0, 0, "UPS.APCEnvironment.APCProbe1.Humidity", NULL, "%.1f", 0, NULL },
/*
  { "ambient.temperature", 0, 0, "UPS.APCEnvironment.APCProbe2.Temperature", NULL, "%.1f", 0, kelvin_celsius_conversion },
  { "ambient.humidity", 0, 0, "UPS.APCEnvironment.APCProbe2.Humidity", NULL, "%.1f", 0, NULL },
*/

  /* instant commands. */
  /* test.* split into subset while waiting for extradata support
   * ie: test.battery.start quick
   */
  { "test.battery.start.quick", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "1", HU_TYPE_CMD, NULL },
  { "test.battery.start.quick", 0, 0, "UPS.Battery.Test", NULL, "1", HU_TYPE_CMD, NULL },	/* Back-UPS RS (experimental) */
  { "test.battery.start.deep", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "2", HU_TYPE_CMD, NULL },
  { "test.battery.start.deep", 0, 0, "UPS.Battery.Test", NULL, "2", HU_TYPE_CMD, NULL },	/* Back-UPS RS (experimental) */
  { "test.battery.stop", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "3", HU_TYPE_CMD, NULL },
  { "test.battery.stop", 0, 0, "UPS.Battery.Test", NULL, "3", HU_TYPE_CMD, NULL },	/* Back-UPS RS (experimental) */
  { "test.panel.start", 0, 0, "UPS.APCPanelTest", NULL, "1", HU_TYPE_CMD, NULL },
  { "test.panel.stop", 0, 0, "UPS.APCPanelTest", NULL, "0", HU_TYPE_CMD, NULL },
  { "test.panel.start", 0, 0, "UPS.PowerSummary.APCPanelTest", NULL, "1", HU_TYPE_CMD, NULL }, /* Back-UPS 500 */
  { "test.panel.stop", 0, 0, "UPS.PowerSummary.APCPanelTest", NULL, "0", HU_TYPE_CMD, NULL }, /* Back-UPS 500 */

  /* USB HID PDC defaults */
  { "load.off.delay", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
  { "load.on.delay", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },
  { "shutdown.stop", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD, NULL },
  { "shutdown.reboot", 0, 0, "UPS.PowerSummary.DelayBeforeReboot", NULL, "10", HU_TYPE_CMD, NULL },
  /* used by APC SmartUPS RM */
  { "load.off.delay", 0, 0, "UPS.Output.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
  { "load.on.delay", 0, 0, "UPS.Output.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },
  { "shutdown.stop", 0, 0, "UPS.Output.DelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD, NULL },
  { "shutdown.reboot", 0, 0, "UPS.Output.DelayBeforeReboot", NULL, "10", HU_TYPE_CMD, NULL },
  /* used by APC BackUPS ES */
  { "load.off.delay", 0, 0, "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
  { "load.on.delay", 0, 0, "UPS.APCGeneralCollection.APCDelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },
  { "shutdown.stop", 0, 0, "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD, NULL },
  { "shutdown.reboot", 0, 0, "UPS.APCGeneralCollection.APCDelayBeforeReboot", NULL, "10", HU_TYPE_CMD, NULL },
  /* used by APC BackUPS CS */
  { "shutdown.return", 0, 0, "UPS.Output.APCDelayBeforeReboot", NULL, "1", HU_TYPE_CMD, NULL },

  { "beeper.on", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
  { "beeper.off", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },
  { "beeper.enable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
  { "beeper.disable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "1", HU_TYPE_CMD, NULL },
  { "beeper.mute", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *apc_format_model(HIDDevice_t *hd) {
	static char model[64];
	char *ptr1, *ptr2;

	/* FIXME?: what is the path "UPS.APC_UPS_FirmwareRevision"? */
	snprintf(model, sizeof(model), "%s", hd->Product ? hd->Product : "unknown");
	ptr1 = strstr(model, "FW:");
	if (ptr1)
	{
		*(ptr1 - 1) = '\0';
		ptr1 += strlen("FW:");
		ptr2 = strstr(ptr1, "USB FW:");
		if (ptr2)
		{
			*(ptr2 - 1) = '\0';
			ptr2 += strlen("USB FW:");
			dstate_setinfo("ups.firmware.aux", "%s", ptr2);
		}
		dstate_setinfo("ups.firmware", "%s", ptr1);
	}
	return model;
}

static const char *apc_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "APC";
}

static const char *apc_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int apc_claim(HIDDevice_t *hd) {

	int status = is_usb_device_supported(apc_usb_device_table, hd);

	switch (status) {

		case POSSIBLY_SUPPORTED:
			/* by default, reject, unless the productid option is given */
			if (getval("productid")) {
				return 1;
			}
			possibly_supported("APC", hd);
			return 0;

		case SUPPORTED:
			return 1;

		case NOT_SUPPORTED:
		default:
			return 0;
	}
}

subdriver_t apc_subdriver = {
	APC_HID_VERSION,
	apc_claim,
	apc_utab,
	apc_hid2nut,
	apc_format_model,
	apc_format_mfr,
	apc_format_serial,
};
