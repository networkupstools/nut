/* microdowell-hid.c - subdriver to monitor MicroDowell USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2009	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  2026		Ronny Brandt <ronnybrandt@gmail.com>
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

#include "config.h" /* must be first */

#include "usbhid-ups.h"
#include "microdowell-hid.h"
#include "main.h"	/* for getval() */
#include "usb-common.h"
#include "hidparser.h"

#define MICRODOWELL_HID_VERSION	"MicroDowell HID 0.1"

/* MicroDowell S.p.A. shares this vendor ID with iDowell (idowell-hid.c,
 * which claims 0x075d:0x0300). We claim only the Enterprise USB interface. */
#define MICRODOWELL_VENDORID	0x075d

/* This firmware only answers a GET_REPORT when the host asks for more bytes
 * than the descriptor declares: requesting the declared length (4 to 6 bytes,
 * depending on the report) returns all zeroes, while asking for 16 bytes
 * returns the real payload. Enable the max_report_size tweak so the whole
 * device page (nominal ratings, capacity limits, battery voltage) is readable
 * instead of coming out as zeroes. */
static void *microdowell_check(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	max_report_size = 1;
	return NULL;
}

/* USB IDs device table */
static usb_device_id_t microdowell_usb_device_table[] = {
	/* MicroDowell Enterprise USB, as fitted to the B.8 (800 VA) and
	 * presumably the rest of the Enterprise/B-Box range */
	{ USB_DEVICE(MICRODOWELL_VENDORID, 0x0201), microdowell_check },

	/* Terminating entry */
	{ 0, 0, NULL }
};

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

static usage_lkp_t microdowell_usage_lkp[] = {
	{  NULL, 0 }
};

static usage_tables_t microdowell_utab[] = {
	microdowell_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t microdowell_hid2nut[] = {
#if WITH_UNMAPPED_DATA_POINTS || (defined DEBUG)
	{ "unmapped.ups.flow.[4].flowid", 0, 0, "UPS.Flow.[4].FlowID", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powerconverter.output.outputid", 0, 0, "UPS.PowerConverter.Output.OutputID", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powerconverter.powerconverterid", 0, 0, "UPS.PowerConverter.PowerConverterID", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.capacitygranularity1", 0, 0, "UPS.PowerSummary.CapacityGranularity1", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.capacitymode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.flowid", 0, 0, "UPS.PowerSummary.FlowID", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.powersummaryid", 0, 0, "UPS.PowerSummary.PowerSummaryID", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.presentstatus.undefined", 0, 0, "UPS.PowerSummary.PresentStatus.Undefined", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", HU_FLAG_STATIC, NULL },
#endif	/* if WITH_UNMAPPED_DATA_POINTS || DEBUG */

	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BatteryPresent", NULL, NULL, 0, nobattery_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Good", NULL, NULL, 0, off_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.InternalFailure", NULL, NULL, 0, commfault_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, NULL, 0, overload_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },

	/* device page */
	{ "device.mfr", 0, 0, "UPS.PowerSummary.iManufacturer", NULL, "%s", HU_FLAG_STATIC, stringid_conversion }, /* Read only */
	{ "device.model", 0, 0, "UPS.PowerSummary.iProduct", NULL, "%s", HU_FLAG_STATIC, stringid_conversion }, /* Read only */
	{ "device.serial", 0, 0, "UPS.PowerSummary.iSerialNumber", NULL, "%s", HU_FLAG_STATIC, stringid_conversion }, /* Read only */

	/* battery page */
	{ "battery.capacity.design", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", HU_FLAG_STATIC, NULL }, /* Read only */
	{ "battery.capacity.full", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", HU_FLAG_STATIC, NULL }, /* Read only */
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.charge.low", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_STATIC, NULL }, /* Read only */
	{ "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", HU_FLAG_STATIC, NULL }, /* Read only */
	{ "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_STATIC, stringid_conversion }, /* Read only */
	{ "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", 0, NULL },
	{ "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.1f", HU_FLAG_STATIC, NULL },

	/* Note: this device always reports RunTimeToEmpty as 0xFFFF ("unknown"),
	 * even while on battery, so battery.runtime is deliberately not mapped:
	 * publishing a constant 65535 would be worse than publishing nothing.
	 * Low-battery shutdown is driven by BelowRemainingCapacityLimit instead. */

	/* UPS page */
	{ "ups.power.nominal", 0, 0, "UPS.Flow.[4].ConfigApparentPower", NULL, "%.0f", HU_FLAG_STATIC, NULL },

	/* Note: UPS.PowerSummary.PercentLoad is declared but never populated -
	 * it stayed at 0 throughout a 60 s battery run with a real load, so
	 * ups.load is not mapped either. */

	/* output page */
	{ "output.voltage.nominal", 0, 0, "UPS.Flow.[4].ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "output.frequency.nominal", 0, 0, "UPS.Flow.[4].ConfigFrequency", NULL, "%.0f", HU_FLAG_STATIC, NULL },

	/* Note: UPS.PowerConverter.Output.Voltage is declared at bit offset 16 of
	 * ReportID 0x01, but that field is constant on this firmware. The only
	 * value in that report which tracks the line is at bit offset 8, where
	 * the descriptor declares Output.OutputID. Its scaling is undocumented
	 * (a raw count of roughly 8493 was measured against 228.0 V), so
	 * output.voltage is left unmapped rather than shipping a guessed factor. */

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *microdowell_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *microdowell_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "MicroDowell";
}

static const char *microdowell_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int microdowell_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(microdowell_usb_device_table, hd);

	switch (status)
	{
	case POSSIBLY_SUPPORTED:
		/* by default, reject, unless the productid option is given */
		if (getval("productid")) {
			return 1;
		}
		possibly_supported("MicroDowell", hd);
		return 0;

	case SUPPORTED:
		return 1;

	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

/* microdowell_fix_report_desc: work around two firmware bugs.
 *
 * 1) The descriptor emits PhysicalMinimum 0 / PhysicalMaximum 300 once and
 *    never resets it. Physical extents are global items, so every subsequent
 *    Main item inherits them - including the single-bit PresentStatus flags:
 *
 *        PhyMax = 300, PhyMin = 0, LogMax = 1,     LogMin = 0   (117 items)
 *        PhyMax = 300, PhyMin = 0, LogMax = 255,   LogMin = 0
 *        PhyMax = 300, PhyMin = 0, LogMax = 32767, LogMin = 0
 *        PhyMax = 300, PhyMin = 0, LogMax = 65534, LogMin = -1
 *
 *    logical_to_physical() then rescales a boolean 1 to 300. The info_lkp_t
 *    tables (online_info, charging_info, ...) match on the exact value 1, so
 *    no status flag is ever recognised: a healthy UPS reports "OB OFF" while
 *    it is online and charging. The same rescaling puts battery.charge about
 *    18 % high. Clearing the physical extents makes logical_to_physical()
 *    return the logical value unchanged, which is what this firmware means.
 *
 * 2) In the Input report 0x09 the nine PresentStatus usages are declared as a
 *    single 9-bit item carrying the ACPresent usage, instead of nine 1-bit
 *    items. GetValue() then reads all nine bits, clamps the result to
 *    LogMax = 1, and ACPresent reads true whenever *any* flag is set - so a
 *    discharging UPS still looks online. The Feature items for the same nine
 *    usages are declared correctly (1 bit each at offsets 24..32), so it is
 *    enough to narrow the Input item to its own single bit.
 *
 * 3) UPS.PowerSummary.Voltage in ReportID 0x08 is declared 16 bits wide but with
 *    LogicalMaximum 255. GetValue() derives its bit mask from the logical range,
 *    so it discards everything above bit 7: a battery reading of 270 (27.0 V)
 *    comes back as 270 & 0x7f = 14, i.e. 1.4 V. Any item wider than 8 bits whose
 *    logical maximum is 255 loses data this way, so we widen the maximum to what
 *    the field can actually hold.
 *
 * All three fixes are guarded by predicates that only match the broken encoding,
 * so a corrected firmware revision is left alone.
 */
#define MICRODOWELL_BOGUS_PHYMAX	300L

static int microdowell_fix_report_desc(HIDDevice_t *pDev, HIDDesc_t *pDesc_arg) {
	size_t	i;
	int	retval = 0;
	int	phyfixed = 0;

	int	vendorID = pDev->VendorID;
	int	productID = pDev->ProductID;

	if (!(vendorID == MICRODOWELL_VENDORID && productID == 0x0201)) {
		upsdebugx(3, "NOT Attempting Report Descriptor fix for UPS: "
			"Vendor: %04x, Product: %04x (vendor/product not matched)",
			(unsigned int)vendorID, (unsigned int)productID);
		return 0;
	}

	if (disable_fix_report_desc) {
		upsdebugx(3, "NOT Attempting Report Descriptor fix for UPS: "
			"Vendor: %04x, Product: %04x "
			"(got disable_fix_report_desc in config)",
			(unsigned int)vendorID, (unsigned int)productID);
		return 0;
	}

	upsdebugx(3, "Attempting Report Descriptor fix for UPS: "
		"Vendor: %04x, Product: %04x",
		(unsigned int)vendorID, (unsigned int)productID);

	for (i = 0; i < pDesc_arg->nitems; i++) {
		HIDData_t	*pData = &pDesc_arg->item[i];

		/* (1) drop the stray global physical range */
		if (pData->have_PhyMax && pData->have_PhyMin
		 && pData->PhyMin == 0
		 && pData->PhyMax == MICRODOWELL_BOGUS_PHYMAX
		 && pData->LogMax != MICRODOWELL_BOGUS_PHYMAX
		) {
			pData->have_PhyMin = 0;
			pData->have_PhyMax = 0;
			pData->PhyMin = 0;
			pData->PhyMax = 0;
			phyfixed++;
			retval++;
		}

		/* (3) widen a logical maximum that cannot hold its own field */
		if (pData->Size > 8
		 && pData->Size < (uint8_t)(8 * sizeof(long))
		 && pData->LogMin == 0
		 && pData->LogMax == 255
		) {
			long	newmax = (1L << pData->Size) - 1;

			upsdebugx(3, "Fixing Report Descriptor: ReportID 0x%02x "
				"%s is %i bits wide but declares LogMax 255, "
				"widening to %ld",
				pData->ReportID,
				HIDGetDataItem(pData, microdowell_utab),
				pData->Size, newmax);
			pData->LogMax = newmax;
			pData->assumed_LogMax = true;
			retval++;
		}

		/* (2) narrow the packed 9-bit PresentStatus Input item */
		if (pData->Type == ITEM_INPUT
		 && pData->Size == 9
		 && pData->LogMin == 0
		 && pData->LogMax == 1
		 && pData->Path.Size > 0
		 && pData->Path.Node[pData->Path.Size - 1] == USAGE_BAT_AC_PRESENT
		) {
			upsdebugx(3, "Fixing Report Descriptor: ReportID 0x%02x "
				"ACPresent Input item declared as %i bits, "
				"narrowing to 1",
				pData->ReportID, pData->Size);
			pData->Size = 1;
			retval++;
		}
	}

	if (phyfixed) {
		upsdebugx(3, "Fixing Report Descriptor: dropped bogus "
			"PhysicalMinimum 0 / PhysicalMaximum %ld on %d item(s)",
			(long)MICRODOWELL_BOGUS_PHYMAX, phyfixed);
	}

	return retval;
}

subdriver_t microdowell_subdriver = {
	MICRODOWELL_HID_VERSION,
	microdowell_claim,
	microdowell_utab,
	microdowell_hid2nut,
	microdowell_format_model,
	microdowell_format_mfr,
	microdowell_format_serial,
	microdowell_fix_report_desc,
	NULL,
};
