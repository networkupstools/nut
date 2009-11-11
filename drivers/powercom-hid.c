/* powercom-hid.c - subdriver to monitor PowerCOM USB/HID devices with NUT
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
#include "powercom-hid.h"
#include "extstate.h"	/* for ST_FLAG_STRING */
#include "main.h"	/* for getval() */
#include "common.h"
#include "usb-common.h"

#define POWERCOM_HID_VERSION	"PowerCOM HID 0.1"
/* FIXME: experimental flag to be put in upsdrv_info */

/* PowerCOM */
#define POWERCOM_VENDORID	0x0d9f

/* USB IDs device table */
static usb_device_id_t powercom_usb_device_table[] = {
	/* PowerCOM IMP - IMPERIAL Series */
	{ USB_DEVICE(POWERCOM_VENDORID, 0x00a2), NULL },
	/* PowerCOM SKP - Smart KING Pro (all Smart series) */
	{ USB_DEVICE(POWERCOM_VENDORID, 0x00a3), NULL },
	/* PowerCOM WOW */
	{ USB_DEVICE(POWERCOM_VENDORID, 0x00a4), NULL },
	/* PowerCOM VGD - Vanguard */
	{ USB_DEVICE(POWERCOM_VENDORID, 0x00a5), NULL },
	/* PowerCOM BNT - Black Knight Pro */
	{ USB_DEVICE(POWERCOM_VENDORID, 0x00a6), NULL },

	/* Terminating entry */
	{ -1, -1, NULL }
};


/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* POWERCOM usage table */
static usage_lkp_t powercom_usage_lkp[] = {
	{ "POWERCOM1",	0x0084002f },
	{ "POWERCOM2",	0xff860060 },
	{ "POWERCOM3",	0xff860080 },
	{  NULL, 0 }
};

static usage_tables_t powercom_utab[] = {
	powercom_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t powercom_hid2nut[] = {

  { "unmapped.ups.audiblealarmcontrol", 0, 0, "UPS.AudibleAlarmControl", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.battery.configvoltage", 0, 0, "UPS.Battery.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.battery.delaybeforestartup", 0, 0, "UPS.Battery.DelayBeforeStartup", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.battery.initialized", 0, 0, "UPS.Battery.Initialized", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.battery.manufacturerdate", 0, 0, "UPS.Battery.ManufacturerDate", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.battery.remainingcapacity", 0, 0, "UPS.Battery.RemainingCapacity", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.battery.test", 0, 0, "UPS.Battery.Test", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.battery.voltage", 0, 0, "UPS.Battery.Voltage", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.iname", 0, 0, "UPS.iName", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.input.configvoltage", 0, 0, "UPS.Input.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.input.frequency", 0, 0, "UPS.Input.Frequency", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.input.voltage", 0, 0, "UPS.Input.Voltage", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.output.configvoltage", 0, 0, "UPS.Output.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.output.delaybeforeshutdown", 0, 0, "UPS.Output.DelayBeforeShutdown", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.output.delaybeforestartup", 0, 0, "UPS.Output.DelayBeforeStartup", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.output.frequency", 0, 0, "UPS.Output.Frequency", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.output.percentload", 0, 0, "UPS.Output.PercentLoad", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.output.voltage", 0, 0, "UPS.Output.Voltage", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powercom1", 0, 0, "UPS.POWERCOM1", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powercom2", 0, 0, "UPS.POWERCOM2", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.audiblealarmcontrol", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.capacitymode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.configvoltage", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.delaybeforeshutdown", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.delaybeforestartup", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.designcapacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.fullchargecapacity", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.idevicechemistry", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.imanufacturer", 0, 0, "UPS.PowerSummary.iManufacturer", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.ioeminformation", 0, 0, "UPS.PowerSummary.iOEMInformation", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.iproduct", 0, 0, "UPS.PowerSummary.iProduct", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.iserialnumber", 0, 0, "UPS.PowerSummary.iSerialNumber", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.manufacturerdate", 0, 0, "UPS.PowerSummary.ManufacturerDate", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.acpresent", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.batterypresent", 0, 0, "UPS.PowerSummary.PresentStatus.BatteryPresent", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.belowremainingcapacitylimit", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.charging", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.communicationlost", 0, 0, "UPS.PowerSummary.PresentStatus.CommunicationLost", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.discharging", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.needreplacement", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.overload", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.powercom3", 0, 0, "UPS.PowerSummary.PresentStatus.POWERCOM3", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.remainingtimelimitexpired", 0, 0, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.shutdownimminent", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.shutdownrequested", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownRequested", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.presentstatus.voltagenotregulated", 0, 0, "UPS.PowerSummary.PresentStatus.VoltageNotRegulated", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.remainingcapacity", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.remainingcapacitylimit", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.runtimetoempty", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.powersummary.warningcapacitylimit", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.acpresent", 0, 0, "UPS.PresentStatus.ACPresent", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.batterypresent", 0, 0, "UPS.PresentStatus.BatteryPresent", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.belowremainingcapacitylimit", 0, 0, "UPS.PresentStatus.BelowRemainingCapacityLimit", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.boost", 0, 0, "UPS.PresentStatus.Boost", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.buck", 0, 0, "UPS.PresentStatus.Buck", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.charging", 0, 0, "UPS.PresentStatus.Charging", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.communicationlost", 0, 0, "UPS.PresentStatus.CommunicationLost", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.discharging", 0, 0, "UPS.PresentStatus.Discharging", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.needreplacement", 0, 0, "UPS.PresentStatus.NeedReplacement", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.overload", 0, 0, "UPS.PresentStatus.Overload", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.powercom3", 0, 0, "UPS.PresentStatus.POWERCOM3", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.remainingtimelimitexpired", 0, 0, "UPS.PresentStatus.RemainingTimeLimitExpired", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.shutdownimminent", 0, 0, "UPS.PresentStatus.ShutdownImminent", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.shutdownrequested", 0, 0, "UPS.PresentStatus.ShutdownRequested", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.tested", 0, 0, "UPS.PresentStatus.Tested", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.presentstatus.voltagenotregulated", 0, 0, "UPS.PresentStatus.VoltageNotRegulated", NULL, "%.0f", 0, NULL },
  { "unmapped.ups.shutdownimminent", 0, 0, "UPS.ShutdownImminent", NULL, "%.0f", 0, NULL },

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static char *powercom_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static char *powercom_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "PowerCOM";
}

static char *powercom_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int powercom_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(powercom_usb_device_table, hd->VendorID, hd->ProductID);

	switch (status)
	{
	case POSSIBLY_SUPPORTED:
		/* by default, reject, unless the productid option is given */
		if (getval("productid")) {
			return 1;
		}
		possibly_supported("PowerCOM", hd);
		return 0;

	case SUPPORTED:
		return 1;

	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

subdriver_t powercom_subdriver = {
	POWERCOM_HID_VERSION,
	powercom_claim,
	powercom_utab,
	powercom_hid2nut,
	powercom_format_model,
	powercom_format_mfr,
	powercom_format_serial,
};
