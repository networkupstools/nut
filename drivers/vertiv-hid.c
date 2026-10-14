/* vertiv-hid.c - subdriver to monitor Vertiv USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2012	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  2013	Charles Lepple <clepple+nut@gmail.com>
 *  2026	Warren Strong <warren.r.strong@gmail.com>
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  gen-usbhid-subdriver script, from an explore walk of a Vertiv
 *  Liebert GXT5 (USB ID 10af:1000), and then customized by hand.
 *  See https://github.com/networkupstools/nut/issues/3613
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
 */

#include "usbhid-ups.h"
#include "vertiv-hid.h"
#include "usb-common.h"

#define VERTIV_HID_VERSION	"Vertiv HID 0.01"
/* FIXME: experimental flag to be put in upsdrv_info */

/* Vertiv (formerly Emerson Network Power / Liebert).
 * The vendor ID is shared with the Liebert models handled by belkin-hid.c */
#define VERTIV_VENDORID	0x10af

/* HID PDC "Volt" unit code (HID PDC 1.0, section 3.2.3). The GXT5 declares
 * this unit with an exponent of 5 on some items, where 7 would yield plain
 * Volts; see vertiv_fix_report_desc() below. */
#define VERTIV_HID_UNIT_VOLT	0x00f0d121L
#define VERTIV_HID_UNITEXP_VOLT_BAD	5
#define VERTIV_HID_UNITEXP_VOLT_GOOD	7

/* USB IDs device table */
static usb_device_id_t vertiv_usb_device_table[] = {
	/* Vertiv Liebert GXT5 */
	{ USB_DEVICE(VERTIV_VENDORID, 0x1000), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};


/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* VERTIV usage table */
static usage_lkp_t vertiv_usage_lkp[] = {
	/* Seen under UPS.PowerSummary on the GXT5; meaning unknown */
	{ "VERTIV1",	0x00840091 },
	{ NULL, 0 }
};

static usage_tables_t vertiv_utab[] = {
	vertiv_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* Vendor-specific value lookups / conversions                     */
/* --------------------------------------------------------------- */

static char vertiv_conversion_buf[32];

/* The GXT5 reports UPS.PowerSummary.RunTimeToEmpty in MINUTES, while NUT's
 * battery.runtime is defined in seconds. Compare belkin-hid.c, which
 * annotates the Liebert family runtime usage as "(minutes)", and
 * ecoflow-hid.c which applies the same x60 conversion.
 *
 * Observed: a GXT5 at 100% charge and 9% load reports 36-38 (and the value
 * crept upwards while the battery finished charging), which is a plausible
 * autonomy estimate in minutes but an absurd one in seconds. */
static const char *vertiv_runtime_fun(double value)
{
	if (value < 0) {
		return NULL;
	}

	snprintf(vertiv_conversion_buf, sizeof(vertiv_conversion_buf),
		"%.0f", value * 60.0);
	return vertiv_conversion_buf;
}

static info_lkp_t vertiv_runtime_info[] = {
	{ 0, NULL, vertiv_runtime_fun, NULL }
};

/* UPS.PowerSummary.Temperature is declared with a 16-bit range and no unit,
 * and the tested GXT5 always reports 65534 (0xFFFE) there, i.e. "no sensor
 * value available". Suppress that rather than publish a bogus reading. */
static const char *vertiv_temperature_fun(double value)
{
	if (value >= 65534 || value <= 0) {
		return NULL;
	}

	snprintf(vertiv_conversion_buf, sizeof(vertiv_conversion_buf),
		"%.1f", value);
	return vertiv_conversion_buf;
}

static info_lkp_t vertiv_temperature_info[] = {
	{ 0, NULL, vertiv_temperature_fun, NULL }
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

/* NOTE: unlike most HID PDC UPSes, the GXT5 exposes everything under the
 * UPS.PowerSummary collection. It has no UPS.Input, UPS.Output or
 * UPS.Battery collections at all, so e.g. input.voltage and output.voltage
 * can not be served, and ups.load comes from UPS.PowerSummary.PercentLoad.
 */
static hid_info_t vertiv_hid2nut[] = {

	/* Battery page */
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
	/* Not present on the tested GXT5 firmware, kept for PDC-conformant units */
	{ "battery.charge.low", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%s", 0, vertiv_runtime_info },
	/* Reported in Volts once vertiv_fix_report_desc() has repaired the
	 * unit exponent (otherwise reads e.g. 0.40 for a 40 V string) */
	{ "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", 0, NULL },
	/* NOTE: UPS.PowerSummary.ConfigVoltage on this device appears twice
	 * (reports 0x01 and 0x0b) and both instances track the MAINS voltage
	 * (e.g. 238-242, moving run to run), not the battery string nominal.
	 * It is therefore deliberately NOT mapped to battery.voltage.nominal.
	 * The true battery nominal (36 V on the tested unit) does not appear
	 * anywhere in this report descriptor.
	 * NOTE: UPS.PowerSummary.DesignCapacity and FullChargeCapacity read
	 * 100 with CapacityMode = 2 (percent), so they are not Ah values and
	 * are NOT mapped to battery.capacity. */

	/* UPS page */
	{ "ups.load", 0, 0, "UPS.PowerSummary.PercentLoad", NULL, "%.0f", 0, NULL },
	{ "ups.temperature", 0, 0, "UPS.PowerSummary.Temperature", NULL, "%s", 0, vertiv_temperature_info },
	/* NOTE: UPS.PowerSummary.ConfigApparentPower and ConfigActivePower are
	 * declared with the Hertz unit code (0x0000f001) and read 90 and 50 on
	 * the tested unit: 50 is the mains frequency (same as ConfigFrequency)
	 * and 90 is more plausibly a power factor than a VA rating. They are
	 * NOT mapped to ups.power.nominal / ups.realpower.nominal. */
	/* Not present on the tested GXT5 firmware, kept for PDC-conformant units */
	{ "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", 0, beeper_info },

	/* Status (all under PowerSummary, there is no PresentStatus collection) */
	{ "BOOL", 0, 0, "UPS.PowerSummary.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.Overload", NULL, NULL, 0, overload_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.NeedReplacement", NULL, NULL, 0, replacebatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.OverTemperature", NULL, NULL, 0, overheat_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.FullyCharged", NULL, NULL, 0, fullycharged_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.FullyDischarged", NULL, NULL, 0, depleted_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.CommunicationLost", NULL, NULL, 0, commfault_info },

#if WITH_UNMAPPED_DATA_POINTS
	{ "unmapped.ups.powersummary.batterypresent", 0, 0, "UPS.PowerSummary.BatteryPresent", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.capacitymode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.configactivepower", 0, 0, "UPS.PowerSummary.ConfigActivePower", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.configapparentpower", 0, 0, "UPS.PowerSummary.ConfigApparentPower", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.configcurrent", 0, 0, "UPS.PowerSummary.ConfigCurrent", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.configfrequency", 0, 0, "UPS.PowerSummary.ConfigFrequency", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.configvoltage", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.current", 0, 0, "UPS.PowerSummary.Current", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.designcapacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.fullchargecapacity", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.imanufacturer", 0, 0, "UPS.PowerSummary.iManufacturer", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.ioeminformation", 0, 0, "UPS.PowerSummary.iOEMInformation", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.iproduct", 0, 0, "UPS.PowerSummary.iProduct", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.iserialnumber", 0, 0, "UPS.PowerSummary.iSerialNumber", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", 0, NULL },
	/* Reads 1.68e+07 on the tested unit, so not mapped to battery.runtime.low */
	{ "unmapped.ups.powersummary.remainingtimelimit", 0, 0, "UPS.PowerSummary.RemainingTimeLimit", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.test", 0, 0, "UPS.PowerSummary.Test", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.used", 0, 0, "UPS.PowerSummary.Used", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.vertiv1", 0, 0, "UPS.PowerSummary.VERTIV1", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.voltageoutofrange", 0, 0, "UPS.PowerSummary.VoltageOutOfRange", NULL, "%.0f", 0, NULL },
#endif	/* if WITH_UNMAPPED_DATA_POINTS */

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *vertiv_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *vertiv_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "Vertiv";
}

static const char *vertiv_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int vertiv_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(vertiv_usb_device_table, hd);

	switch (status)
	{
	case POSSIBLY_SUPPORTED:
		/* Deliberately strict: reject, even when "productid" is set.
		 *
		 * Vendor 0x10af is shared with belkin-hid.c, which handles the
		 * Liebert models exposing a full HID PDC tree (e.g. the GXE3 /
		 * PowerSure PST 10af:0002 has PowerConverter, BatterySystem,
		 * Flow and OutletSystem collections). This subdriver maps a
		 * PowerSummary-only device and would strip input.voltage,
		 * output.voltage and a real battery.voltage.nominal off those.
		 *
		 * The usual 'if (getval("productid")) return 1;' escape is
		 * unsafe here for that reason: ups.conf commonly sets productid
		 * (nut-scanner emits it), which would let this subdriver claim
		 * a GXE3 if it were ever consulted first. Exact PID match only. */
		possibly_supported("Vertiv", hd);
		return 0;

	case SUPPORTED:
		return 1;

	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

/* The GXT5 firmware declares its Voltage items (UPS.PowerSummary.Voltage on
 * report 0x09 and the second UPS.PowerSummary.ConfigVoltage on report 0x0b)
 * with the HID PDC Volt unit (0x00f0d121) and a Unit Exponent of 5, while
 * the first ConfigVoltage (report 0x01) correctly uses 7. Since NUT's unit
 * table treats Volt as 10^7, exponent 5 yields a factor of 1e-2, so a raw
 * 40 (Volts, matching the ~40.8 V float of the 3 x 12 V string in the
 * tested unit) was published as battery.voltage 0.4, and raw 239 as 2.39
 * where mains was independently measured at ~239 V.
 *
 * Repair the exponent on any Volt-unit item still declaring 5, so the raw
 * value is served in plain Volts. Honours the disable_fix_report_desc
 * driver option like the other fix-ups. */
static int vertiv_fix_report_desc(HIDDevice_t *pDev, HIDDesc_t *pDesc_arg)
{
	HIDData_t	*pData;
	size_t		i;
	int		retval = 0;

	int vendorID = pDev->VendorID;
	int productID = pDev->ProductID;
	if (vendorID != VERTIV_VENDORID || productID != 0x1000) {
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

	for (i = 0; i < pDesc_arg->nitems; i++) {
		pData = &pDesc_arg->item[i];

		if (pData->Unit != VERTIV_HID_UNIT_VOLT) {
			continue;
		}

		if (pData->UnitExp != VERTIV_HID_UNITEXP_VOLT_BAD) {
			continue;
		}

		upsdebugx(3, "Fixing Report Descriptor: "
			"ReportID 0x%02x Offset %u (Volt unit): "
			"set UnitExp = %d (was %d)",
			(unsigned int)pData->ReportID,
			(unsigned int)pData->Offset,
			VERTIV_HID_UNITEXP_VOLT_GOOD,
			(int)pData->UnitExp);
		pData->UnitExp = VERTIV_HID_UNITEXP_VOLT_GOOD;
		retval = 1;
	}

	return retval;
}

subdriver_t vertiv_subdriver = {
	VERTIV_HID_VERSION,
	vertiv_claim,
	vertiv_utab,
	vertiv_hid2nut,
	vertiv_format_model,
	vertiv_format_mfr,
	vertiv_format_serial,
	vertiv_fix_report_desc,
	NULL,
};
