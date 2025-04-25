/* arduino-hid.c - subdriver to monitor Arduino USB/HID devices with NUT
 *
 *  This was written assuming it would be used to communicate with code
 *  using this Arduino library: https://github.com/abratchik/HIDPowerDevice
 *
 *  Copyright (C)
 *  2003 - 2012	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  2013 Charles Lepple <clepple+nut@gmail.com>
 *  2021 Alex Bratchik <alexbratchik@yandex.com>
 *  2023 Kelly Byrd <kbyrd@memcpy.com>
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
#include "arduino-hid.h"
#include "main.h"	/* for getval() */
#include "usb-common.h"

#define ARDUINO_HID_VERSION	"Arduino HID 0.21"
/* FIXME: experimental flag to be put in upsdrv_info */

/* Arduino */
#define ARDUINO_VENDORID	0x2341
#define ARDUINO_VENDORID2	0x2A03

/* USB IDs device table */
static usb_device_id_t arduino_usb_device_table[] = {
	/* Arduino Leonardo, Leonardo ETH and Pro Micro*/
	{ USB_DEVICE(ARDUINO_VENDORID, 0x0036), NULL },
	{ USB_DEVICE(ARDUINO_VENDORID, 0x8036), NULL },
	{ USB_DEVICE(ARDUINO_VENDORID2, 0x0036), NULL },
	{ USB_DEVICE(ARDUINO_VENDORID2, 0x8036), NULL },
	{ USB_DEVICE(ARDUINO_VENDORID2, 0x0040), NULL },
	{ USB_DEVICE(ARDUINO_VENDORID2, 0x8040), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};

static usb_communication_subdriver_t *usb = &usb_subdriver;


/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* ARDUINO usage table */
static usage_lkp_t arduino_usage_lkp[] = {
	{  NULL, 0 }
};

static usage_tables_t arduino_utab[] = {
	arduino_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t arduino_hid2nut[] = {

	/* USB HID PDC defaults */
	{ "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_FLAG_ABSENT, NULL},
	{ "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL},
	{ "ups.timer.start", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
	{ "ups.timer.shutdown", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
	{ "ups.timer.reboot", 0, 0, "UPS.PowerSummary.DelayBeforeReboot", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},

	/* USB HID PDC defaults */
	{ "load.off.delay", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
	{ "load.on.delay", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },
	{ "shutdown.stop", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD, NULL },
	{ "shutdown.reboot", 0, 0, "UPS.PowerSummary.DelayBeforeReboot", NULL, "10", HU_TYPE_CMD, NULL },

	/* Battery */
	{ "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_STATIC, stringid_conversion},
	/* In the sample for the Arduino library, this isn't a date */
	/*{ "battery.mfr.date", 0, 0, "UPS.PowerSummary.iOEMInformation", NULL, "%s", HU_FLAG_STATIC, stringid_conversion},*/
	{ "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.2f", HU_FLAG_QUICK_POLL, NULL},
	{ "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.2f", HU_FLAG_QUICK_POLL, NULL},
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
	{ "battery.runtime.low", 0, 0, "UPS.PowerSummary.RemainingTimeLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	/* Parse `*Capacity` values assuming `UPS.PowerSummary.CapacityMode` is set to 2, which means the `*Capacity`
	   units are percent. The Arduino HID Powerdevice library is capable of doing other modes, if the Sketch wants
	   to use them, but it appears most drivers just assume percent.
	*/
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.charge.low", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL},
	{ "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL},

	/* USB HID PresentStatus Flags
	   TODO: Parse these into battery.charger.status
	*/
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, NULL, 0, overload_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL, NULL, 0, timelimitexpired_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BatteryPresent", NULL, NULL, 0, nobattery_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyCharged", NULL, NULL, HU_FLAG_QUICK_POLL, fullycharged_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyDischarged", NULL, NULL, HU_FLAG_QUICK_POLL, depleted_info },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *arduino_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *arduino_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "Arduino";
}

static const char *arduino_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int arduino_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(arduino_usb_device_table, hd);

	switch (status) {
		case POSSIBLY_SUPPORTED:
			/* by default, reject, unless the productid option is given */
			if (getval("productid")) {
				if (!getval("usb_hid_ep_in"))
					usb->hid_ep_in=4;
				if (!getval("usb_hid_ep_out"))
					usb->hid_ep_out=5;
				if (!getval("usb_hid_rep_index"))
					usb->hid_rep_index = 2;
				return 1;
			}
			possibly_supported("Arduino", hd);
			return 0;

		case SUPPORTED:
			if (!getval("usb_hid_ep_in"))
				usb->hid_ep_in=4;
			if (!getval("usb_hid_ep_out"))
				usb->hid_ep_out=5;
			if (!getval("usb_hid_rep_index"))
				usb->hid_rep_index = 2;
			return 1;

		case NOT_SUPPORTED:
		default:
			return 0;
	}
}

subdriver_t arduino_subdriver = {
	ARDUINO_HID_VERSION,
	arduino_claim,
	arduino_utab,
	arduino_hid2nut,
	arduino_format_model,
	arduino_format_mfr,
	arduino_format_serial,
	fix_report_desc,
};
