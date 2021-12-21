/* liebert-hid.c - subdriver to monitor Liebert USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2008 Arnaud Quette <arnaud.quette@free.fr>
 *  2005 - 2006 Peter Selinger <selinger@users.sourceforge.net>
 *  2007        Charles Lepple <clepple@gmail.com>
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
#include "usbhid-ups.h"
#include "liebert-hid.h"
#include "usb-common.h"

#define LIEBERT_HID_VERSION     "Phoenixtec/Liebert HID 0.3"
/* FIXME: experimental flag to be put in upsdrv_info */

/* Phoenixtec Power Co., Ltd */
#define LIEBERT_VENDORID	0x06da

/*! USB IDs device table.
 *
 * Note that this subdriver was named before the USB VendorID was determined to
 * actually belong to Phoenixtec. The belkin-hid.c file covers the other
 * Liebert units which share some of the same incorrect exponents as the
 * Belkin HID firmware. */
static usb_device_id_t liebert_usb_device_table[] = {
	/* various models */
	{ USB_DEVICE(LIEBERT_VENDORID, 0xffff), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* LIEBERT usage table */
static usage_lkp_t liebert_usage_lkp[] = {
	{ NULL, 0 }
};

static usage_tables_t liebert_utab[] = {
	liebert_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t liebert_hid2nut[] = {

#if 0
  { "unmapped.ups.powersummary.flowid", 0, 0, "UPS.PowerSummary.FlowID", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.powersummaryid", 0, 0, "UPS.PowerSummary.PowerSummaryID", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.designcapacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.capacitygranularity1", 0, 0, "UPS.PowerSummary.CapacityGranularity1", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.capacitymode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.iproduct", 0, 0, "UPS.PowerSummary.iProduct", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.imanufacturer", 0, 0, "UPS.PowerSummary.iManufacturer", NULL, "%.0f", 0, NULL },
#endif

  { "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.2f", 0, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.2f", 0, NULL },
  { "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
  { "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", 0, stringid_conversion },

  { "ups.load",   0, 0, "UPS.PowerSummary.PercentLoad", NULL, "%.0f", 0, NULL },

  { "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", NULL, "%.1f", 0, NULL },
  { "output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", NULL, "%.2f", 0, NULL },

  { "input.voltage", 0, 0, "UPS.PowerConverter.Input.[1].Voltage", NULL, "%.1f", 0, NULL },
  { "input.frequency", 0, 0, "UPS.PowerConverter.Input.[1].Frequency", NULL, "%.2f", 0, NULL },

  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, "%.0f", HU_FLAG_QUICK_POLL, online_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, "%.0f", HU_FLAG_QUICK_POLL, lowbatt_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, "%.0f", HU_FLAG_QUICK_POLL, charging_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, "%.0f", HU_FLAG_QUICK_POLL, discharging_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, "%.0f", 0, overload_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, "%.0f", 0, shutdownimm_info },

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *liebert_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *liebert_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "Liebert";
}

static const char *liebert_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int liebert_claim(HIDDevice_t *hd) {

	int status = is_usb_device_supported(liebert_usb_device_table, hd);

	switch (status) {

		case POSSIBLY_SUPPORTED:
			/* by default, reject, unless the productid option is given */
			if (getval("productid")) {
				return 1;
			}
			possibly_supported("Liebert", hd);
			return 0;

		case SUPPORTED:
			return 1;

		case NOT_SUPPORTED:
		default:
			return 0;
	}
}

subdriver_t liebert_subdriver = {
	LIEBERT_HID_VERSION,
	liebert_claim,
	liebert_utab,
	liebert_hid2nut,
	liebert_format_model,
	liebert_format_mfr,
	liebert_format_serial,
};
