/* cps-hid.c - subdriver to monitor CPS USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2008 Arnaud Quette <arnaud.quette@free.fr>
 *  2005 - 2006 Peter Selinger <selinger@users.sourceforge.net>
 *  2020 - 2025 Jim Klimov <jimklimov+nut@gmail.com>
 *  2024        Alejandro González <me@alegon.dev>
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  gen-usbhid-subdriver script. It must be customized.
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

#include "main.h"     /* for getval() */
#include "nut_float.h"
#include "hidparser.h" /* for FindObject_with_ID_Node() */
#include "usbhid-ups.h"
#include "cps-hid.h"
#include "usb-common.h"

#define CPS_HID_VERSION      "CyberPower HID 0.83"

/* Cyber Power Systems */
#define CPS_VENDORID 0x0764

/* ST Microelectronics */
#define STMICRO_VENDORID	0x0483
/* Please note that USB vendor ID 0x0483 is from ST Microelectronics -
 * with actual product IDs delegated to different OEMs.
 * Devices handled in this driver are marketed under Cyber Energy brand.
 */

/* Values for correcting the HID on some models
 * where LogMin and LogMax are set incorrectly in the HID.
 */
#define CPS_VOLTAGE_LOGMIN 0
#define CPS_VOLTAGE_LOGMAX 511 /* Includes safety margin. */

/*! Battery voltage scale factor.
 * For some devices, the reported battery voltage is off by factor
 * of 1.5 so we need to apply a scale factor to it to get the real
 * battery voltage. By default, the factor is 1 (no scaling).
 * Similarly, some firmwares do not report the exponent well, so
 * frequency values are seen as e.g. "499.0" (in "0.1 Hz" units not
 * explicitly stated), instead of "49.9 Hz".
 */
static double	battery_scale = 1, input_freq_scale = 1, output_freq_scale = 1;
static int	might_need_battery_scale = 0, might_need_freq_scale = 0;
static int	battery_scale_checked = 0, input_freq_scale_checked = 0, output_freq_scale_checked = 0;

/*! If the ratio of the battery voltage to the nominal battery voltage exceeds
 * this factor, we assume that the battery voltage needs to be scaled by 2/3.
 */
static const double battery_voltage_sanity_check = 1.4;

static void *cps_battery_scale(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	might_need_battery_scale = 1;
	might_need_freq_scale = 1;

	return NULL;
}

/* USB IDs device table */
static usb_device_id_t cps_usb_device_table[] = {
	/* 900AVR/BC900D */
	{ USB_DEVICE(CPS_VENDORID, 0x0005), NULL },
	/* Dynex DX-800U?, CP1200AVR/BC1200D, CP825AVR-G, CP1000AVRLCD, CP1000PFCLCD, CP1500C, CP550HG, etc. */
	{ USB_DEVICE(CPS_VENDORID, 0x0501), &cps_battery_scale },
	/* OR2200LCDRM2U, OR700LCDRM1U, PR6000LCDRTXL5U, CP1350EPFCLCD */
	{ USB_DEVICE(CPS_VENDORID, 0x0601), NULL },

	/* Cyber Energy branded devices by CPS */
	{ USB_DEVICE(STMICRO_VENDORID, 0xa430), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};

/*! Adjusts frequency if it is order(s) of magnitude off
 * is_input = 1 for input.frequency and 0 for output.frequency
 */
static void cps_adjust_frequency_scale(double freq_report, int is_input)
{
	const char *freq_low_str, *freq_high_str, *freq_nom_str;
	double freq_low = 0, freq_high = 0, freq_nom = 0;

	if ((is_input && input_freq_scale_checked) || (!is_input && output_freq_scale_checked))
		return;

	/* May be not available from device itself; but still
	 * may be set by user as default/override options;
	 * if not, we default for 50Hz and/or 60Hz range +- 10%.
	 */
	freq_nom_str = dstate_getinfo(is_input ? "input.frequency.nominal" : "output.frequency.nominal");
	freq_low_str = dstate_getinfo(is_input ? "input.frequency.low" : "output.frequency.low");
	freq_high_str = dstate_getinfo(is_input ? "input.frequency.high" : "output.frequency.high");

	if (freq_nom_str)
		freq_nom = strtod(freq_nom_str, NULL);
	if (freq_low_str)
		freq_low = strtod(freq_low_str, NULL);
	if (freq_high_str)
		freq_high = strtod(freq_high_str, NULL);

	if (d_equal(freq_nom, 0)) {
		if (45 < freq_low && freq_low <= 50)
			freq_nom = 50;
		else if (50 <= freq_high && freq_high <= 55)
			freq_nom = 50;
		else if (45 < freq_report && freq_report <= 55)
			freq_nom = 50;
		else if (450 < freq_report && freq_report <= 550)
			freq_nom = 50;
		else if (55 < freq_low && freq_low <= 60)
			freq_nom = 60;
		else if (60 <= freq_high && freq_high <= 65)
			freq_nom = 60;
		else if (55 < freq_report && freq_report <= 65)
			freq_nom = 60;
		else if (550 < freq_report && freq_report <= 650)
			freq_nom = 60;

		upsdebugx(3, "%s: '%sput.frequency.nominal' is %s, guessed %0.1f%s",
			__func__, is_input ? "in" : "out", NUT_STRARG(freq_nom_str),
			d_equal(freq_nom, 0) ? 55 : freq_nom,
			d_equal(freq_nom, 0) ? " (50 or 60Hz range)" : "");
	}

	if (d_equal(freq_low, 0)) {
		if (d_equal(freq_nom, 0))
			freq_low = 45.0;
		else
			freq_low = freq_nom * 0.95;

		upsdebugx(3, "%s: '%sput.frequency.low' is %s, defaulting to %0.1f",
			__func__, is_input ? "in" : "out", NUT_STRARG(freq_low_str), freq_low);
	}

	if (d_equal(freq_high, 0)) {
		if (d_equal(freq_nom, 0))
			freq_high = 65.0;
		else
			freq_high = freq_nom * 1.05;

		upsdebugx(3, "%s: '%sput.frequency.high' is %s, defaulting to %0.1f",
			__func__, is_input ? "in" : "out", NUT_STRARG(freq_high_str), freq_high);
	}

	if (freq_low <= freq_report && freq_report <= freq_high) {
		if (is_input) {
			input_freq_scale = 1.0;
			input_freq_scale_checked = 1;
		} else {
			output_freq_scale = 1.0;
			output_freq_scale_checked = 1;
		}
		/* We should be here once per freq type... */
		upsdebugx(1, "%s: Determined scaling factor "
			"needed for '%sput.frequency': 1.0",
			__func__, is_input ? "in" : "out");
	}
	else
	if (freq_low <= freq_report/10.0 && freq_report/10.0 <= freq_high) {
		if (is_input) {
			input_freq_scale = 0.1;
			input_freq_scale_checked = 1;
		} else {
			output_freq_scale = 0.1;
			output_freq_scale_checked = 1;
		}
		/* We should be here once per freq type... */
		upsdebugx(1, "%s: Determined scaling factor "
			"needed for '%sput.frequency': 0.1",
			__func__, is_input ? "in" : "out");
	}
	else
	{
		/* We might return here, so do not log too loudly */
		upsdebugx(2, "%s: Could not determine scaling factor "
			"needed for '%sput.frequency', will report "
			"it as is (and might detect better later)",
			__func__, is_input ? "in" : "out");
	}
}

/* returns statically allocated string - must not use it again before
   done with result! */
static const char *cps_input_freq_fun(double value)
{
	static char	buf[8];

	if (might_need_freq_scale) {
		cps_adjust_frequency_scale(value, 1);
	}

	upsdebugx(5, "%s: input_freq_scale = %.3f", __func__, input_freq_scale);
	snprintf(buf, sizeof(buf), "%.1f", input_freq_scale * value);

	return buf;
}

static info_lkp_t cps_input_freq[] = {
	{ 0, NULL, &cps_input_freq_fun, NULL }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static const char *cps_output_freq_fun(double value)
{
	static char	buf[8];

	if (might_need_freq_scale) {
		cps_adjust_frequency_scale(value, 0);
	}

	upsdebugx(5, "%s: output_freq_scale = %.3f", __func__, output_freq_scale);
	snprintf(buf, sizeof(buf), "%.1f", output_freq_scale * value);

	return buf;
}

static info_lkp_t cps_output_freq[] = {
	{ 0, NULL, &cps_output_freq_fun, NULL }
};

/*! Adjusts @a battery_scale if voltage is well above nominal.
 */
static void cps_adjust_battery_scale(double batt_volt)
{
	const char *batt_volt_nom_str;
	double batt_volt_nom;

	if(battery_scale_checked) {
		return;
	}

	batt_volt_nom_str = dstate_getinfo("battery.voltage.nominal");
	if(!batt_volt_nom_str) {
		upsdebugx(2, "%s: 'battery.voltage.nominal' not available yet; skipping scale determination", __func__);
		return;
	}

	batt_volt_nom = strtod(batt_volt_nom_str, NULL);
	if(d_equal(batt_volt_nom, 0)) {
		upsdebugx(3, "%s: 'battery.voltage.nominal' is %s", __func__, batt_volt_nom_str);
		return;
	}

	if( (batt_volt / batt_volt_nom) > battery_voltage_sanity_check ) {
		upslogx(LOG_INFO, "%s: battery readings will be scaled by 2/3", __func__);
		battery_scale = 2.0/3;
	}

	battery_scale_checked = 1;
}

/* returns statically allocated string - must not use it again before
   done with result! */
static const char *cps_battvolt_fun(double value)
{
	static char	buf[8];

	if(might_need_battery_scale) {
		cps_adjust_battery_scale(value);
	}

	upsdebugx(5, "%s: battery_scale = %.3f", __func__, battery_scale);
	snprintf(buf, sizeof(buf), "%.1f", battery_scale * value);

	return buf;
}

static info_lkp_t cps_battvolt[] = {
	{ 0, NULL, &cps_battvolt_fun, NULL }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static const char *cps_battcharge_fun(double value)
{
	static char	buf[8];

	/* clamp battery charge to 100% */
	snprintf(buf, sizeof(buf), "%.0f", value < 100.0 ? value : 100.0);

	return buf;
}

static info_lkp_t cps_battcharge[] = {
	{ 0, NULL, &cps_battcharge_fun, NULL }
};

static const char *cps_battstatus_fun(double value)
{
	static char	buf[8];

	/* assumes `UPS.PowerSummary.FullChargeCapacity` is in %, which should be for */
	/* UPSes that conform to the USB HID spec, given how we read `battery.charge` */
	snprintf(buf, sizeof(buf), "%.0f%%", value);

	return buf;
}

static info_lkp_t cps_battstatus[] = {
	{ 0, NULL, &cps_battstatus_fun, NULL }
};

static info_lkp_t cps_sensitivity_info[] = {
	{ 0, "low", NULL, NULL },
	{ 1, "normal", NULL, NULL },
	{ 2, "high", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* CPS usage table */
static usage_lkp_t cps_usage_lkp[] = {
	{ "CPSFirmwareVersion", 0xff0100d0 },
	{ "CPSInputSensitivity",    0xff010043 },
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
#if WITH_UNMAPPED_DATA_POINTS
  { "unmapped.ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.capacitymode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.designcapacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.capacitygranularity1", 0, 0, "UPS.PowerSummary.CapacityGranularity1", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.capacitygranularity2", 0, 0, "UPS.PowerSummary.CapacityGranularity2", NULL, "%.0f", 0, NULL },
#endif	/* if WITH_UNMAPPED_DATA_POINTS */

  /* Battery page */
  { "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", 0, stringid_conversion },
  { "battery.mfr.date", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Battery.ManufacturerDate", NULL, "%s", HU_FLAG_SEMI_STATIC, date_conversion },
  { "battery.mfr.date", 0, 0, "UPS.PowerSummary.iOEMInformation", NULL, "%s", 0, stringid_conversion },
  { "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", 0, NULL },
  { "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%s", 0, cps_battcharge },
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
  { "battery.runtime.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.RemainingTimeLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%s", 0, cps_battvolt },
  { "battery.status", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%s", 0, cps_battstatus },

  /* UPS page */
  { "ups.load", 0, 0, "UPS.Output.PercentLoad", NULL, "%.0f", 0, NULL },
  { "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", 0, beeper_info },
  { "ups.test.result", 0, 0, "UPS.Output.Test", NULL, "%s", 0, test_read_info },
  { "ups.power", 0, 0, "UPS.Output.ApparentPower", NULL, "%.0f", 0, NULL },
  { "ups.power.nominal", 0, 0, "UPS.Output.ConfigApparentPower", NULL, "%.0f", 0, NULL },
  { "ups.realpower", 0, 0, "UPS.Output.ActivePower", NULL, "%.0f", 0, NULL },
  { "ups.realpower.nominal", 0, 0, "UPS.Output.ConfigActivePower", NULL, "%.0f", 0, NULL },
  { "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.DelayBeforeStartup", NULL, DEFAULT_ONDELAY_CPS, HU_FLAG_ABSENT, NULL},
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY_CPS, HU_FLAG_ABSENT, NULL},
  { "ups.timer.start", 0, 0, "UPS.Output.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  { "ups.timer.shutdown", 0, 0, "UPS.Output.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  { "ups.timer.reboot", 0, 0, "UPS.Output.DelayBeforeReboot", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
  { "ups.firmware", 0, 0, "UPS.PowerSummary.CPSFirmwareVersion", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
  { "ups.temperature", 0, 0, "UPS.PowerSummary.Temperature", NULL, "%s", 0, kelvin_celsius_conversion },

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
  /* FIXME: Check if something like "UPS.Flow([N]?).ConfigFrequency"
   *  is available for "input.frequency.nominal" */
  { "input.frequency", 0, 0, "UPS.Input.Frequency", NULL, "%.1f", 0, cps_input_freq },
  { "input.voltage.nominal", 0, 0, "UPS.Input.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "input.voltage", 0, 0, "UPS.Input.Voltage", NULL, "%.1f", 0, NULL },
  { "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Input.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Input.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  /* used by CP1350EPFCLCD; why oh why "UPS.Output"?.. */
  { "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "input.sensitivity", ST_FLAG_RW | ST_FLAG_STRING, 0, "UPS.Output.CPSInputSensitivity", NULL, "%s", HU_FLAG_SEMI_STATIC | HU_FLAG_ENUM, cps_sensitivity_info },

  /* Output page */
  /* FIXME: Check if something like "UPS.Flow([N]?).ConfigFrequency"
   *  is available for "output.frequency.nominal" */
  { "output.frequency", 0, 0, "UPS.Output.Frequency", NULL, "%.1f", 0, cps_output_freq },
  { "output.voltage", 0, 0, "UPS.Output.Voltage", NULL, "%.1f", 0, NULL },
  { "output.voltage.nominal", 0, 0, "UPS.Output.ConfigVoltage", NULL, "%.0f", 0, NULL },

  /* instant commands. */
  { "test.battery.start.quick", 0, 0, "UPS.Output.Test", NULL, "1", HU_TYPE_CMD, NULL },
  { "test.battery.start.deep", 0, 0, "UPS.Output.Test", NULL, "2", HU_TYPE_CMD, NULL },
  { "test.battery.stop", 0, 0, "UPS.Output.Test", NULL, "3", HU_TYPE_CMD, NULL },
  { "load.off.delay", 0, 0, "UPS.Output.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY_CPS, HU_TYPE_CMD, NULL },
  { "load.on.delay", 0, 0, "UPS.Output.DelayBeforeStartup", NULL, DEFAULT_ONDELAY_CPS, HU_TYPE_CMD, NULL },
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

static const char *cps_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *cps_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "CPS";
}

static const char *cps_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int cps_claim(HIDDevice_t *hd) {

	int status = is_usb_device_supported(cps_usb_device_table, hd);

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

/* CPS Models like CP900EPFCLCD/CP1500PFCLCDa return a syntactically
 * legal but incorrect Report Descriptor whereby the Input High Transfer
 * Max/Min values are used for the Output Voltage Usage Item limits.
 * Additionally the Input Voltage LogMax is set incorrectly for EU models.
 * This corrects them by finding and applying fixed
 * voltage limits as being more appropriate.
 */

static int cps_fix_report_desc(HIDDevice_t *pDev, HIDDesc_t *pDesc_arg) {
	HIDData_t *pData;
	int	retval = 0;

	int vendorID = pDev->VendorID;
	int productID = pDev->ProductID;
	if (vendorID != CPS_VENDORID || (productID != 0x0501 && productID != 0x0601)) {
		upsdebugx(3,
			"NOT Attempting Report Descriptor fix for UPS: "
			"Vendor: %04x, Product: %04x "
			"(vendor/product not matched)",
			(unsigned int)vendorID,
			(unsigned int)productID);
		return 0;
	}

	if (disable_fix_report_desc) {
		upsdebugx(3,
			"NOT Attempting Report Descriptor fix for UPS: "
			"Vendor: %04x, Product: %04x "
			"(got disable_fix_report_desc in config)",
			(unsigned int)vendorID,
			(unsigned int)productID);
		return 0;
	}

	upsdebugx(3, "Attempting Report Descriptor fix for UPS: "
		"Vendor: %04x, Product: %04x",
		(unsigned int)vendorID,
		(unsigned int)productID);

	/* Apply the fix cautiously by looking for input voltage,
	 * high voltage transfer and output voltage report usages.
	 * If the output voltage log min/max equals high voltage
	 * transfer log min/max, then the bug is present.
	 *
	 * To fix it set both the input and output voltages to our
	 * pre-defined settings CPS_VOLTAGE_LOGMIN/CPS_VOLTAGE_LOGMAX.
	 */

	if ((pData=FindObject_with_ID_Node(pDesc_arg, 16 /* 0x10 */, USAGE_POW_HIGH_VOLTAGE_TRANSFER))) {
		long hvt_logmin = pData->LogMin;
		long hvt_logmax = pData->LogMax;
		upsdebugx(4, "Original Report Descriptor: hvt input "
			"LogMin: %ld LogMax: %ld", hvt_logmin, hvt_logmax);

		if ((pData=FindObject_with_ID_Node(pDesc_arg, 18 /* 0x12 */, USAGE_POW_VOLTAGE))) {
			long output_logmin = pData->LogMin;
			long output_logmax = pData->LogMax;
			upsdebugx(4, "Original Report Descriptor: output "
				"LogMin: %ld LogMax: %ld",
				output_logmin, output_logmax);

			if (hvt_logmin == output_logmin && hvt_logmax == output_logmax) {
				pData->LogMin = CPS_VOLTAGE_LOGMIN;
				pData->LogMax = CPS_VOLTAGE_LOGMAX;
				upsdebugx(3, "Fixing Report Descriptor: "
					"set Output Voltage LogMin = %d, LogMax = %d",
					CPS_VOLTAGE_LOGMIN, CPS_VOLTAGE_LOGMAX);

				if ((pData=FindObject_with_ID_Node(pDesc_arg, 15 /* 0x0F */, USAGE_POW_VOLTAGE))) {
					long input_logmin = pData->LogMin;
					long input_logmax = pData->LogMax;
					upsdebugx(4, "Original Report Descriptor: input "
						"LogMin: %ld LogMax: %ld",
						input_logmin, input_logmax);

					/* TOTHINK: Should this be still about
					 * the *HIGH* Voltage Transfer? Or LOW?
					 */
					if (hvt_logmin == input_logmin && hvt_logmax == input_logmax) {
						pData->LogMin = CPS_VOLTAGE_LOGMIN;
						pData->LogMax = CPS_VOLTAGE_LOGMAX;
						upsdebugx(3, "Fixing Report Descriptor: "
							"set Input Voltage LogMin = %d, LogMax = %d",
							CPS_VOLTAGE_LOGMIN, CPS_VOLTAGE_LOGMAX);
					}
				}

				retval = 1;
			}
		}
	}

	if ((pData=FindObject_with_ID_Node(pDesc_arg, 18 /* 0x12 */, USAGE_POW_VOLTAGE))) {
		HIDData_t *output_pData = pData;
		long output_logmin = output_pData->LogMin;
		long output_logmax = output_pData->LogMax;
		bool output_logmax_assumed = output_pData->assumed_LogMax;

		if ((pData=FindObject_with_ID_Node(pDesc_arg, 15 /* 0x0F */, USAGE_POW_VOLTAGE))) {
			HIDData_t *input_pData = pData;
			long input_logmin = input_pData->LogMin;
			long input_logmax = input_pData->LogMax;
			bool input_logmax_assumed = input_pData->assumed_LogMax;

			if ( (output_logmax_assumed || input_logmax_assumed)
			/* &&   output_logmax != input_logmax */
			) {
				/* We often get 0x0F ReportdId LogMax=65535
				 * and 0x12 ReportdId LogMax=255 because of
				 * wrong encoding. See e.g. analysis at
				 * https://github.com/networkupstools/nut/issues/1512#issuecomment-1224652911
				 */
				upsdebugx(4, "Original Report Descriptor: output 0x12 "
					"LogMin: %ld LogMax: %ld (assumed: %s) Size: %" PRIu8,
					output_logmin, output_logmax,
					output_logmax_assumed ? "yes" : "no",
					output_pData->Size);
				upsdebugx(4, "Original Report Descriptor: input 0x0f "
					"LogMin: %ld LogMax: %ld (assumed: %s) Size: %" PRIu8,
					input_logmin, input_logmax,
					input_logmax_assumed ? "yes" : "no",
					input_pData->Size);

				/* First pass: try our hard-coded limits */
				if (output_logmax_assumed && output_logmax < CPS_VOLTAGE_LOGMAX) {
					output_logmax = CPS_VOLTAGE_LOGMAX;
				}

				if (input_logmax_assumed && input_logmax < CPS_VOLTAGE_LOGMAX) {
					input_logmax = CPS_VOLTAGE_LOGMAX;
				}

				/* Second pass: align the two */
				if (output_logmax_assumed && output_logmax < input_logmax) {
					output_logmax = input_logmax;
				} else if (input_logmax_assumed && input_logmax < output_logmax) {
					input_logmax = output_logmax;
				}

				/* Second pass: cut off according to bit-size
				 * of each value */
				if (input_logmax_assumed
				 && input_pData->Size > 1
				 && input_pData->Size <= sizeof(long)*8
				) {
					/* Note: usually values are signed, but
					 * here we are about compensating for
					 * poorly encoded maximums, so limit by
					 * 2^(size)-1, e.g. for "size==16" the
					 * limit should be "2^16 - 1 = 65535";
					 * note that in HIDParse() we likely
					 * set 65535 here in that case. See
					 * also comments there (hidparser.c)
					 * discussing signed/unsigned nuances.
					 */
					/* long sizeMax = (1L << (input_pData->Size - 1)) - 1; */
					long sizeMax = (1L << (input_pData->Size)) - 1;
					if (input_logmax > sizeMax) {
						input_logmax = sizeMax;
					}
				}

				if (output_logmax_assumed
				 && output_pData->Size > 1
				 && output_pData->Size <= sizeof(long)*8
				) {
					/* See comment above */
					/* long sizeMax = (1L << (output_pData->Size - 1)) - 1; */
					long sizeMax = (1L << (output_pData->Size)) - 1;
					if (output_logmax > sizeMax) {
						output_logmax = sizeMax;
					}
				}

				if (input_logmax != input_pData->LogMax) {
					upsdebugx(3, "Fixing Report Descriptor: "
						"set Input Voltage LogMax = %ld",
						input_logmax);
					input_pData->LogMax = input_logmax;
					retval = 1;
				}

				if (output_logmax != output_pData->LogMax) {
					upsdebugx(3, "Fixing Report Descriptor: "
						"set Output Voltage LogMax = %ld",
						output_logmax);
					output_pData->LogMax = output_logmax;
					retval = 1;
				}
			}
		}
	}

	if (!retval) {
		/* We did not `return 1` above, so... */
		upsdebugx(3,
			"SKIPPED Report Descriptor fix for UPS: "
			"Vendor: %04x, Product: %04x "
			"(problematic conditions not matched)",
			(unsigned int)vendorID,
			(unsigned int)productID);
	}

	return retval;
}

subdriver_t cps_subdriver = {
	CPS_HID_VERSION,
	cps_claim,
	cps_utab,
	cps_hid2nut,
	cps_format_model,
	cps_format_mfr,
	cps_format_serial,
	cps_fix_report_desc,
};
