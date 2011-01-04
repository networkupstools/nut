/* idowell-hid.c - subdriver to monitor iDowell USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2009	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
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
 */

#include "usbhid-ups.h"
#include "idowell-hid.h"
#include "main.h"	/* for getval() */
#include "usb-common.h"

#define IDOWELL_HID_VERSION	"iDowell HID 0.1"
/* FIXME: experimental flag to be put in upsdrv_info */

/* iDowell */
#define IDOWELL_VENDORID	0x075d

/* USB IDs device table */
static usb_device_id_t idowell_usb_device_table[] = {
	/* iDowell */
	{ USB_DEVICE(IDOWELL_VENDORID, 0x0300), NULL },

	/* Terminating entry */
	{ -1, -1, NULL }
};


/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* IDOWELL usage table */
static usage_lkp_t idowell_usage_lkp[] = {
	{  NULL, 0 }
};

static usage_tables_t idowell_utab[] = {
	idowell_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t idowell_hid2nut[] = {

  { "unmapped.ups.flow.[4].configapparentpower", 0, 0, "UPS.Flow.[4].ConfigApparentPower", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.flow.[4].configfrequency", 0, 0, "UPS.Flow.[4].ConfigFrequency", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.flow.[4].configvoltage", 0, 0, "UPS.Flow.[4].ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.flow.[4].flowid", 0, 0, "UPS.Flow.[4].FlowID", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powerconverter.output.highvoltagetransfer", 0, 0, "UPS.PowerConverter.Output.HighVoltageTransfer", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powerconverter.output.lowvoltagetransfer", 0, 0, "UPS.PowerConverter.Output.LowVoltageTransfer", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powerconverter.output.outputid", 0, 0, "UPS.PowerConverter.Output.OutputID", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powerconverter.output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powerconverter.powerconverterid", 0, 0, "UPS.PowerConverter.PowerConverterID", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.capacitygranularity1", 0, 0, "UPS.PowerSummary.CapacityGranularity1", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.capacitymode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.delaybeforeshutdown", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.delaybeforestartup", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.designcapacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.flowid", 0, 0, "UPS.PowerSummary.FlowID", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.fullchargecapacity", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.idevicechemistry", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.imanufacturer", 0, 0, "UPS.PowerSummary.iManufacturer", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.iproduct", 0, 0, "UPS.PowerSummary.iProduct", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.iserialnumber", 0, 0, "UPS.PowerSummary.iSerialNumber", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.percentload", 0, 0, "UPS.PowerSummary.PercentLoad", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.powersummaryid", 0, 0, "UPS.PowerSummary.PowerSummaryID", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.acpresent", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.belowremainingcapacitylimit", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.charging", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.communicationlost", 0, 0, "UPS.PowerSummary.PresentStatus.CommunicationLost", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.discharging", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.good", 0, 0, "UPS.PowerSummary.PresentStatus.Good", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.internalfailure", 0, 0, "UPS.PowerSummary.PresentStatus.InternalFailure", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.needreplacement", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.overload", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.shutdownimminent", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.undefined", 0, 0, "UPS.PowerSummary.PresentStatus.Undefined", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.remainingcapacity", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.remainingcapacitylimit", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.runtimetoempty", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *idowell_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *idowell_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "iDowell";
}

static const char *idowell_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int idowell_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(idowell_usb_device_table, hd->VendorID, hd->ProductID);

	switch (status)
	{
	case POSSIBLY_SUPPORTED:
		/* by default, reject, unless the productid option is given */
		if (getval("productid")) {
			return 1;
		}
		possibly_supported("iDowell", hd);
		return 0;

	case SUPPORTED:
		return 1;

	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

subdriver_t idowell_subdriver = {
	IDOWELL_HID_VERSION,
	idowell_claim,
	idowell_utab,
	idowell_hid2nut,
	idowell_format_model,
	idowell_format_mfr,
	idowell_format_serial,
};
