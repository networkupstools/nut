/* powervar-hid.c - subdriver to monitor Powervar USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2012	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  2013 Charles Lepple <clepple+nut@gmail.com>
 *  2019 AMETEK Powervar - Andrew McCartney <andrew.mccartney@ametek.com>
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
 */

#include "config.h" /* must be first */

#include "usbhid-ups.h"
#include "powervar-hid.h"
#include "main.h"	/* for getval() */
#include "usb-common.h"

#define POWERVAR_HID_VERSION	"Powervar HID 0.19"
/* FIXME: experimental flag to be put in upsdrv_info */

/* Powervar */
#define POWERVAR_VENDORID	0x4234

/* USB IDs device table */
static usb_device_id_t powervar_usb_device_table[] = {
	/* Powervar */
	{ USB_DEVICE(POWERVAR_VENDORID, 0x0002), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};

static usb_communication_subdriver_t *usb = &usb_subdriver;

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* POWERVAR usage table */
static usage_lkp_t powervar_usage_lkp[] = {
	{ "POWERVAR1",	0xff000001 },
	{  NULL, 0 }
};

static usage_tables_t powervar_utab[] = {
	powervar_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t powervar_hid2nut[] = {

	/* Battery page */
	{ "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", 0, stringid_conversion },
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.capacity", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RuntimeToEmpty", NULL, "%.0f", 0, NULL },

/* Special case: ups.status & ups.alarm */
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, 0, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, NULL, 0, overload_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL, NULL, 0, timelimitexpired_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.InternalFailure", NULL, NULL, 0, yes_no_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Good", NULL, NULL, 0, yes_no_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.OverTemperature", NULL, NULL, 0, overheat_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyCharged", NULL, NULL, 0, fullycharged_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyDischarged", NULL, NULL, 0, depleted_info },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *powervar_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *powervar_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "Powervar";
}

static const char *powervar_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int powervar_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(powervar_usb_device_table, hd);

	switch (status)
	{
	case POSSIBLY_SUPPORTED:
		/* by default, reject, unless the productid option is given */
		if (getval("productid")) {
			usb->hid_rep_index = 1;
			return 1;
		}
		possibly_supported("Powervar", hd);
		return 0;

	case SUPPORTED:
		usb->hid_rep_index = 1;
		return 1;

	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

subdriver_t powervar_subdriver = {
	POWERVAR_HID_VERSION,
	powervar_claim,
	powervar_utab,
	powervar_hid2nut,
	powervar_format_model,
	powervar_format_mfr,
	powervar_format_serial,
};
