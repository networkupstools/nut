/* ecoflow-hid.c - subdriver to monitor EcoFlow USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2012	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  2013	Charles Lepple <clepple+nut@gmail.com>
 *  2024	Yevgeniy Kuksenko <2882631+ykuksenko@users.noreply.github.com>
 *  2025	M. Greyson Christoforo <grey@christoforo.net>
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
#include "ecoflow-hid.h"
#include "main.h"	/* for getval() */
#include "usb-common.h"

#define ECOFLOW_HID_VERSION	"EcoFlow HID 0.01"
/* FIXME: experimental flag to be put in upsdrv_info */

/* EcoFlow */
#define ECOFLOW_VENDORID	0x3746

/* USB IDs device table */
static usb_device_id_t ecoflow_usb_device_table[] = {
	/* EcoFlow */
	{ USB_DEVICE(ECOFLOW_VENDORID, 0xffff), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};


/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* ECOFLOW usage table */
static usage_lkp_t ecoflow_usage_lkp[] = {
	{ NULL, 0 }
};

static usage_tables_t ecoflow_utab[] = {
	ecoflow_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t ecoflow_hid2nut[] = {

	{ "ups.power.nominal", 0, 0, "UPS.Flow.[4].ConfigActivePower", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", HU_FLAG_QUICK_POLL, beeper_info },
	{ "experimental.ups.powersummary.averagetimetoempty", 0, 0, "UPS.PowerSummary.AverageTimeToEmpty", NULL, "%.0f", 0, NULL },
	{ "experimental.ups.powersummary.averagetimetofull", 0, 0, "UPS.PowerSummary.AverageTimeToFull", NULL, "%.0f", 0, NULL },
	{ "experimental.ups.powersummary.capacitygranularity1", 0, 0, "UPS.PowerSummary.CapacityGranularity1", NULL, "%.0f", 0, NULL },
	{ "experimental.ups.powersummary.capacitygranularity2", 0, 0, "UPS.PowerSummary.CapacityGranularity2", NULL, "%.0f", 0, NULL },
	{ "experimental.ups.powersummary.capacitymode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL },
	{ "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.1f", HU_FLAG_SEMI_STATIC, NULL },
	{ "ups.timer.reboot", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeReboot", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },
	{ "ups.timer.shutdown", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },
	{ "battery.capacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.capacity", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
	{ "experimental.ups.powersummary.ioeminformation", 0, 0, "UPS.PowerSummary.iOEMInformation", NULL, "%s", HU_FLAG_STATIC, stringid_conversion },
	{ "ups.mfr.date", 0, 0, "UPS.PowerSummary.ManufacturerDate", NULL, "%s", 0, date_conversion },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
	{ "experimental.ups.powersummary.presentstatus.batterypresent", 0, 0, "UPS.PowerSummary.PresentStatus.BatteryPresent", NULL, "%.0f", 0, NULL },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, "%.0f", 0, charging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, "%.0f", 0, discharging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyCharged", NULL, NULL, HU_FLAG_QUICK_POLL, fullycharged_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyDischarged", NULL, NULL, HU_FLAG_QUICK_POLL, depleted_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, HU_FLAG_QUICK_POLL, replacebatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL, NULL, 0, timelimitexpired_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, HU_FLAG_QUICK_POLL, shutdownimm_info },
	{ "experimental.ups.powersummary.presentstatus.shutdownrequested", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownRequested", NULL, "%.0f", 0, NULL },
	{ "experimental.ups.powersummary.presentstatus.voltagenotregulated", 0, 0, "UPS.PowerSummary.PresentStatus.VoltageNotRegulated", NULL, "%.0f", 0, NULL },
	{ "experimental.ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", 0, NULL },
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },
	{ "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.runtime.low", 0, 0, "UPS.PowerSummary.RemainingTimeLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },
	{ "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", HU_FLAG_QUICK_POLL, NULL },
	{ "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", 0, NULL },

/* Please revise values discovered by data walk for mappings to
 * docs/nut-names.txt and group the rest under the ifdef below:
 */
#if WITH_UNMAPPED_DATA_POINTS
	/* ConfigActivePower: seems to be close to Wh ratings of the Devices (260Wh for River3+(286Wh are specs)/1024Wh for Delta3+(1024Wh are specs)) */
	{ "unmapped.ups.flow.[4].configactivepower", 0, 0, "UPS.Flow.[4].ConfigActivePower", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.audiblealarmcontrol", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%.0f", 0, NULL },
	/* AverageTimeToEmpty: not sure what to map this to and the values do not make sense at this point, they update way too slowly. */
	{ "unmapped.ups.powersummary.averagetimetoempty", 0, 0, "UPS.PowerSummary.AverageTimeToEmpty", NULL, "%.0f", 0, NULL },
	/* AverageTimeToFull: not sure what to map this to and the values do not make sense at this point, they update way too slowly. */
	{ "unmapped.ups.powersummary.averagetimetofull", 0, 0, "UPS.PowerSummary.AverageTimeToFull", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.capacitygranularity1", 0, 0, "UPS.PowerSummary.CapacityGranularity1", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.capacitygranularity2", 0, 0, "UPS.PowerSummary.CapacityGranularity2", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.capacitymode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL },
	/* ConfigVoltage: not sure what this is, had a decimal */
	{ "unmapped.ups.powersummary.configvoltage", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.1f", 0, NULL },
	/* DelayBeforeReboot: does not seem to work */
	{ "unmapped.ups.powersummary.delaybeforereboot", 0, 0, "UPS.PowerSummary.DelayBeforeReboot", NULL, "%.0f", 0, NULL },
	/* DelayBeforeShutdown: does not seem to work */
	{ "unmapped.ups.powersummary.delaybeforeshutdown", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.designcapacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.fullchargecapacity", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.idevicechemistry", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.ioeminformation", 0, 0, "UPS.PowerSummary.iOEMInformation", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.manufacturerdate", 0, 0, "UPS.PowerSummary.ManufacturerDate", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.presentstatus.acpresent", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.presentstatus.batterypresent", 0, 0, "UPS.PowerSummary.PresentStatus.BatteryPresent", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.presentstatus.belowremainingcapacitylimit", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.presentstatus.charging", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.presentstatus.discharging", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.presentstatus.fullycharged", 0, 0, "UPS.PowerSummary.PresentStatus.FullyCharged", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.presentstatus.fullydischarged", 0, 0, "UPS.PowerSummary.PresentStatus.FullyDischarged", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.presentstatus.needreplacement", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.presentstatus.remainingtimelimitexpired", 0, 0, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.presentstatus.shutdownimminent", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, "%.0f", 0, NULL },
	/* ShutdownRequested: */
	{ "unmapped.ups.powersummary.presentstatus.shutdownrequested", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownRequested", NULL, "%.0f", 0, NULL },
	/* VoltageNotRegulated: not sure how to trigger this */
	{ "unmapped.ups.powersummary.presentstatus.voltagenotregulated", 0, 0, "UPS.PowerSummary.PresentStatus.VoltageNotRegulated", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.remainingcapacity", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.remainingcapacitylimit", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.remainingtimelimit", 0, 0, "UPS.PowerSummary.RemainingTimeLimit", NULL, "%.0f", 0, NULL },
	{ "unmapped.ups.powersummary.runtimetoempty", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
	/* Voltage: Probably should have a decimal to match ConfigVoltage, does not seem to change even at 0% capacity */
	{ "unmapped.ups.powersummary.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", 0, NULL },
	{ "unmapped.ups.powersummary.warningcapacitylimit", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", 0, NULL },
#endif	/* if WITH_UNMAPPED_DATA_POINTS */

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *ecoflow_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *ecoflow_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "EcoFlow";
}

static const char *ecoflow_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int ecoflow_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(ecoflow_usb_device_table, hd);

	switch (status)
	{
	case POSSIBLY_SUPPORTED:
		/* by default, reject, unless the productid option is given */
		if (getval("productid")) {
			return 1;
		}
		possibly_supported("EcoFlow", hd);
		return 0;

	case SUPPORTED:
		return 1;

	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

subdriver_t ecoflow_subdriver = {
	ECOFLOW_HID_VERSION,
	ecoflow_claim,
	ecoflow_utab,
	ecoflow_hid2nut,
	ecoflow_format_model,
	ecoflow_format_mfr,
	ecoflow_format_serial,
	fix_report_desc,	/* may optionally be customized, see cps-hid.c for example */
};
