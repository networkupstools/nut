/* salicru-hid.c - subdriver to monitor Salicru USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2012	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  2013 Charles Lepple <clepple+nut@gmail.com>
 *  2021 Francois Lacroix <xbgmsharp@gmail.com>
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

#include "usbhid-ups.h"
#include "salicru-hid.h"
#include "main.h"	/* for getval() */
#include "usb-common.h"

#define SALICRU_HID_VERSION	"Salicru HID 0.1"
/* FIXME: experimental flag to be put in upsdrv_info */

/* Salicru */
#define SALICRU_VENDORID	0x2e66

/* USB IDs device table */
static usb_device_id_t salicru_usb_device_table[] = {
	/* Salicru SPS 850 HOME */
        /* https://www.salicru.com/sps-home.html */
	{ USB_DEVICE(SALICRU_VENDORID, 0x0300), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};


/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* SALICRU usage table */
static usage_lkp_t salicru_usage_lkp[] = {
	{  NULL, 0 }
};

static usage_tables_t salicru_utab[] = {
	salicru_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t salicru_hid2nut[] = {

#ifdef DEBUG
  { "experimental.ups.powersummary.imanufacturer", 0, 0, "UPS.PowerSummary.iManufacturer", NULL, "%.0f", 0, NULL },
  { "experimental.ups.powersummary.iproduct", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL },
  { "experimental.ups.powersummary.iserialnumber", 0, 0, "UPS.PowerSummary.iSerialNumber", NULL, "%.0f", 0, NULL },
  { "experimental.ups.powersummary.capacitymode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL },
  { "experimental.ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", 0, NULL },
  { "experimental.ups.powersummary.designcapacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", 0, NULL },
  { "experimental.ups.powersummary.fullchargecapacity", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", 0, NULL },
#endif /* DEBUG */

/* A few more unknow fields
   0.043266	[D1] Path: UPS.ff010004.ff010024.ff0100d0, Type: Feature, ReportID: 0x19, Offset: 0, Size: 8, Value: 0.1
   0.043766	[D1] Path: UPS.ff010004.ff010024.ff0100d1, Type: Feature, ReportID: 0x1a, Offset: 0, Size: 8, Value: 0
   0.044308	[D1] Path: UPS.ff01001d.ff010019.ff010020, Type: Feature, ReportID: 0x25, Offset: 0, Size: 1, Value: 0
   0.044762	[D1] Path: UPS.ff01001d.ff010019.ff010021, Type: Feature, ReportID: 0x2c, Offset: 0, Size: 1, Value: 0
   0.044891	[D1] Path: UPS.ff01001d.ff010019.ff010021, Type: Input, ReportID: 0x2c, Offset: 0, Size: 1, Value: 0
   0.045386	[D1] Path: UPS.ff01001d.ff01001a.ff010001, Type: Feature, ReportID: 0x26, Offset: 0, Size: 1, Value: 0
   0.045888	[D1] Path: UPS.ff01001d.ff01001a.ff010002, Type: Feature, ReportID: 0x27, Offset: 0, Size: 8, Value: 1
   0.047553	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.047659	[D1] Can't retrieve Report 28: Value too large for defined data type
   0.047796	[D1] Path: UPS.ff01001d.ff01001b.ff010040, Type: Feature, ReportID: 0x28, Offset: 0, Size: 8
   0.048272	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.048403	[D1] Can't retrieve Report 28: Value too large for defined data type
   0.048537	[D1] Path: UPS.ff01001d.ff01001b.ff010016, Type: Input, ReportID: 0x28, Offset: 0, Size: 8
   0.049017	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.049147	[D1] Can't retrieve Report 28: Value too large for defined data type
   0.049280	[D1] Path: UPS.ff01001d.ff01001b.ff010018, Type: Feature, ReportID: 0x28, Offset: 8, Size: 8
   0.050944	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.051124	[D1] Can't retrieve Report 28: Value too large for defined data type
   0.051264	[D1] Path: UPS.ff01001d.ff01001b.ff010018, Type: Input, ReportID: 0x28, Offset: 8, Size: 8
   0.052965	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.053137	[D1] Can't retrieve Report 29: Value too large for defined data type
   0.053277	[D1] Path: UPS.ff01001d.ff01001b.ff010015, Type: Feature, ReportID: 0x29, Offset: 0, Size: 8
   0.053804	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.053954	[D1] Can't retrieve Report 29: Value too large for defined data type
   0.054091	[D1] Path: UPS.ff01001d.ff01001b.ff010015, Type: Output, ReportID: 0x29, Offset: 0, Size: 8
   0.054530	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.054664	[D1] Can't retrieve Report 29: Value too large for defined data type
   0.054799	[D1] Path: UPS.ff01001d.ff01001b.ff010017, Type: Feature, ReportID: 0x29, Offset: 8, Size: 8
   0.055285	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.055421	[D1] Can't retrieve Report 29: Value too large for defined data type
   0.055557	[D1] Path: UPS.ff01001d.ff01001b.ff010017, Type: Output, ReportID: 0x29, Offset: 8, Size: 8
   0.056130	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.056273	[D1] Can't retrieve Report 2d: Value too large for defined data type
   0.056409	[D1] Path: UPS.ff01001d.ff01001b.ff010010, Type: Feature, ReportID: 0x2d, Offset: 0, Size: 1
   0.056927	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.057074	[D1] Can't retrieve Report 2d: Value too large for defined data type
   0.057211	[D1] Path: UPS.ff01001d.ff01001b.ff01001e, Type: Feature, ReportID: 0x2d, Offset: 1, Size: 1
   0.057676	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.057825	[D1] Can't retrieve Report 2d: Value too large for defined data type
   0.057962	[D1] Path: UPS.ff01001d.ff01001b.ff01001f, Type: Feature, ReportID: 0x2d, Offset: 2, Size: 1
   0.058416	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.058551	[D1] Can't retrieve Report 2d: Value too large for defined data type
   0.058685	[D1] Path: UPS.ff01001d.ff01001b.ff010010, Type: Input, ReportID: 0x2d, Offset: 0, Size: 1
   0.059156	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.059291	[D1] Can't retrieve Report 2d: Value too large for defined data type
   0.059426	[D1] Path: UPS.ff01001d.ff01001b.ff01001e, Type: Input, ReportID: 0x2d, Offset: 1, Size: 1
   0.059957	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.060112	[D1] Can't retrieve Report 2d: Value too large for defined data type
   0.060248	[D1] Path: UPS.ff01001d.ff01001b.ff01001f, Type: Input, ReportID: 0x2d, Offset: 2, Size: 1
   0.061944	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.062117	[D1] Can't retrieve Report 2a: Value too large for defined data type
   0.062256	[D1] Path: UPS.ff01001d.ff01001b.ff010013, Type: Feature, ReportID: 0x2a, Offset: 0, Size: 1
   0.063941	[D2] libusb_get_report: error sending control message: Value too large for defined data type
   0.064104	[D1] Can't retrieve Report 2b: Value too large for defined data type
   0.064242	[D1] Path: UPS.ff01001d.ff01001b.ff010014, Type: Feature, ReportID: 0x2b, Offset: 0, Size: 1
*/

  /* Battery page */
  { "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", 0, stringid_conversion },
  { "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
  { "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", 0, NULL },
  { "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
  { "battery.runtime.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.RemainingTimeLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.2f", 0, NULL },

  /* UPS page */
  { "ups.load", 0, 0, "UPS.Output.PercentLoad", NULL, "%.0f", 0, NULL },
  { "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", 0, beeper_info },
  { "ups.test.result", 0, 0, "UPS.Output.Test", NULL, "%s", 0, test_read_info },
  { "ups.realpower.nominal", 0, 0, "UPS.Output.ConfigActivePower", NULL, "%.0f", 0, NULL },

  /* Boolean value */
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyCharged", NULL, NULL, 0, fullycharged_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL, NULL, 0, timelimitexpired_info },
  { "BOOL", 0, 0, "UPS.Output.CommunicationLost", NULL, NULL, 0, commfault_info },
  { "BOOL", 0, 0, "UPS.Output.Boost", NULL, NULL, 0, boost_info },
  { "BOOL", 0, 0, "UPS.Output.Overload", NULL, NULL, 0, overload_info },

  /* Input page */
  { "input.frequency", 0, 0, "UPS.Input.Frequency", NULL, "%.1f", 0, NULL },
  { "input.voltage.nominal", 0, 0, "UPS.Input.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "input.voltage", 0, 0, "UPS.Input.Voltage", NULL, "%.1f", 0, NULL },
  /* read-only on this model but probably R/W in other models */
/*
  { "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Input.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
  { "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Input.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
*/
  { "input.transfer.high", 0, 0, "UPS.PowerConverter.Output.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_STATIC, NULL },
  { "input.transfer.low", 0, 0, "UPS.PowerConverter.Output.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_STATIC, NULL },

  /* Output page */
  { "output.frequency", 0, 0, "UPS.Output.Frequency", NULL, "%.1f", 0, NULL },
  { "output.voltage", 0, 0, "UPS.Output.Voltage", NULL, "%.1f", 0, NULL },
  { "output.voltage.nominal", 0, 0, "UPS.Output.ConfigVoltage", NULL, "%.0f", 0, NULL },

  /* instant commands. */
/* Need testing
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
*/

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *salicru_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *salicru_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "Salicru";
}

static const char *salicru_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int salicru_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(salicru_usb_device_table, hd);

	switch (status)
	{
	case POSSIBLY_SUPPORTED:
		/* by default, reject, unless the productid option is given */
		if (getval("productid")) {
			return 1;
		}
		possibly_supported("Salicru", hd);
		return 0;

	case SUPPORTED:
		return 1;

	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

subdriver_t salicru_subdriver = {
	SALICRU_HID_VERSION,
	salicru_claim,
	salicru_utab,
	salicru_hid2nut,
	salicru_format_model,
	salicru_format_mfr,
	salicru_format_serial,
};
