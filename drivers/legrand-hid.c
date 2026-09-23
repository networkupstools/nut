/* legrand-hid.c - subdriver to monitor Legrand USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2012	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  2013 Charles Lepple <clepple+nut@gmail.com>
 *  2018 Gabriele Taormina <gabriele.taormina@legrand.com>, for Legrand
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

#include "common.h"
#include "usbhid-ups.h"
#include "legrand-hid.h"
#include "main.h"
#include "usb-common.h"

#define LEGRAND_HID_VERSION	"Legrand HID 0.32"

/* Legrand */
#define LEGRAND_VENDORID	0x1cb0

/* Legrand ProductIDs */
#define LEGRAND_PID_PDU	0x0038	/* Keor PDU model (800VA) */
#define LEGRAND_PID_SP	0x0032	/* Keor SP model (600, 800, 1000, 1500, 2000VA version) */

/* Some units of the Keor DK / Daker DK range ship a Cypress-based
 * communication board which enumerates under the Cypress vendor ID
 * instead of the Legrand one, while still exposing a standard HID
 * Power Device report descriptor. */
#define LEGRAND_CYPRESS_VENDORID	0x0665
#define LEGRAND_PID_DK	0x5161	/* Keor DK / Daker DK (tested: Keor DK 3k) */

static void *disable_interrupt_pipe(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);
	
	if (!use_interrupt_pipe)
		return NULL;
	use_interrupt_pipe = FALSE;
	upslogx(LOG_INFO, "interrupt pipe disabled (add 'pollonly' flag to 'ups.conf' to get rid of this message)");
	return NULL;
}

/* USB IDs device table */
static usb_device_id_t legrand_usb_device_table[] = {
	{ USB_DEVICE(LEGRAND_VENDORID, LEGRAND_PID_PDU),	disable_interrupt_pipe },	/* Legrand Keor PDU */
	{ USB_DEVICE(LEGRAND_VENDORID, LEGRAND_PID_SP) ,	disable_interrupt_pipe },	/* Legrand Keor SP */
	{ USB_DEVICE(LEGRAND_CYPRESS_VENDORID, LEGRAND_PID_DK),	disable_interrupt_pipe },	/* Legrand Keor DK */

	/* Terminating entry */
	{ 0, 0, NULL }
};

/* --------------------------------------------------------------- */
/* Vendor-specific usage table                                     */
/* --------------------------------------------------------------- */

/* LEGRAND usage table */
static usage_lkp_t legrand_usage_lkp[] = {
	{ NULL, 0 }
};

static usage_tables_t legrand_utab[] = {
	legrand_usage_lkp,
	hid_usage_lkp,
	NULL,
};

static const char *legrand_times10(double value)
{
	static char buf[20];
	snprintf(buf, sizeof(buf), "%0.1f", value * 10);
	return buf;
}

static info_lkp_t legrand_times10_info[] = {
	{ 0, NULL, legrand_times10, NULL },
	{ 0, NULL, NULL, NULL }
};

static const char *legrand_times100k(double value)
{
	static char buf[20];
	snprintf(buf, sizeof(buf), "%0.1f", value * 100000);
	return buf;
}

static info_lkp_t legrand_times100k_info[] = {
	{ 0, NULL, legrand_times100k, NULL },
	{ 0, NULL, NULL, NULL }
};

static const char *legrand_times1M(double value)
{
	static char buf[20];
	snprintf(buf, sizeof(buf), "%0.1f", value * 1000000);
	return buf;
}

static info_lkp_t legrand_times1M_info[] = {
	{ 0, NULL, legrand_times1M, NULL },
	{ 0, NULL, NULL, NULL }
};

static const char *legrand_times10M(double value)
{
	static char buf[20];
	snprintf(buf, sizeof(buf), "%0.1f", value * 10000000);
	return buf;
}

static info_lkp_t legrand_times10M_info[] = {
	{ 0, NULL, legrand_times10M, NULL },
	{ 0, NULL, NULL, NULL }
};

/* Only used for battery.charge, which NUT expresses as a whole percentage */
static const char *legrand_times100M(double value)
{
	static char buf[20];
	snprintf(buf, sizeof(buf), "%0.0f", value * 100000000);
	return buf;
}

static info_lkp_t legrand_times100M_info[] = {
	{ 0, NULL, legrand_times100M, NULL },
	{ 0, NULL, NULL, NULL }
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t legrand_hid2nut[] = {
	/* Input Data */
	{ "input.voltage", 0, 0, "UPS.Input.Voltage", NULL, "%.0f", 0, NULL },
	{ "input.voltage", 0, 0, "UPS.PowerConverter.Input.Voltage", NULL, "%.0f", 0, legrand_times1M_info },
	{ "input.transfer.high", 0, 0, "UPS.Input.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.transfer.high", 0, 0, "UPS.PowerConverter.Output.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_STATIC, legrand_times10_info },
	{ "input.transfer.low", 0, 0, "UPS.Input.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.transfer.low", 0, 0, "UPS.PowerConverter.Output.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.voltage.nominal", 0, 0, "UPS.Input.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.voltage.nominal", 0, 0, "UPS.Flow.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },

	/* Battery Data */
	{ "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, divide_by_10_conversion },
	{ "battery.voltage.nominal", 0, 0, "UPS.BatterySystem.Battery.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.0f", 0, divide_by_10_conversion },
	{ "battery.voltage", 0, 0, "UPS.BatterySystem.Battery.Voltage", NULL, "%.0f", 0, legrand_times100k_info },
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RuntimeToEmpty", NULL, "%.0f", 0, NULL },
	{ "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "battery.charge.low", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_STATIC, NULL },

	/* Output Data */
	{ "output.voltage", 0, 0, "UPS.Output.Voltage", NULL, "%.0f", 0, NULL },
	{ "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", NULL, "%.0f", 0, legrand_times10M_info },
	{ "output.frequency", 0, 0, "UPS.Output.Frequency", NULL, "%.0f", 0, NULL },
	{ "ups.load", 0, 0, "UPS.Output.PercentLoad", NULL, "%.0f", 0, NULL },
	{ "ups.load", 0, 0, "UPS.OutletSystem.Outlet.PercentLoad", NULL, "%.0f", 0, NULL },
	{ "ups.realpower.nominal", 0, 0, "UPS.Output.ConfigActivePower", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "ups.realpower.nominal", 0, 0, "UPS.Flow.ConfigApparentPower", NULL, "%.0f", HU_FLAG_STATIC, NULL },

	/* UPS Status */
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
	{ "BOOL", 0, 0, "UPS.Output.Overload", NULL, NULL, HU_FLAG_QUICK_POLL, overload_info },
	/* Keor DK: flags live under UPS.OutletSystem.PresentStatus; see
	 * legrand_fix_report_desc() for why their offsets need correcting */
	{ "BOOL", 0, 0, "UPS.OutletSystem.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
	{ "BOOL", 0, 0, "UPS.OutletSystem.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },
	{ "BOOL", 0, 0, "UPS.OutletSystem.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },

	/* Keor DK: the unit publishes its readings under UPS.OutletSystem.*,
	 * UPS.Output.* and UPS.BatterySystem.*, with a uniform 1e7 scaling. */
	{ "input.voltage", 0, 0, "UPS.OutletSystem.Voltage", NULL, "%.1f", 0, legrand_times10M_info },
	{ "input.voltage.nominal", 0, 0, "UPS.OutletSystem.ConfigVoltage", NULL, "%.0f", HU_FLAG_STATIC, legrand_times10M_info },
	{ "input.frequency", 0, 0, "UPS.OutletSystem.Frequency", NULL, "%.1f", 0, NULL },
	{ "input.frequency.nominal", 0, 0, "UPS.OutletSystem.ConfigFrequency", NULL, "%.0f", HU_FLAG_STATIC, NULL },
	{ "input.transfer.high", 0, 0, "UPS.OutletSystem.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_STATIC, legrand_times10M_info },
	{ "input.transfer.low", 0, 0, "UPS.OutletSystem.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_STATIC, legrand_times10M_info },
	{ "output.voltage", 0, 0, "UPS.BatterySystem.Voltage", NULL, "%.1f", 0, legrand_times10M_info },
	{ "output.frequency", 0, 0, "UPS.BatterySystem.Frequency", NULL, "%.1f", 0, NULL },
	{ "output.current", 0, 0, "UPS.Output.Current", NULL, "%.1f", 0, NULL },
	{ "output.realpower", 0, 0, "UPS.Output.ActivePower", NULL, "%.0f", 0, legrand_times10M_info },
	{ "output.power", 0, 0, "UPS.Output.ApparentPower", NULL, "%.0f", 0, legrand_times10M_info },
	{ "ups.power.nominal", 0, 0, "UPS.OutletSystem.ConfigApparentPower", NULL, "%.0f", HU_FLAG_STATIC, legrand_times10M_info },
	{ "ups.realpower.nominal", 0, 0, "UPS.OutletSystem.ConfigActivePower", NULL, "%.0f", HU_FLAG_STATIC, legrand_times10M_info },
	{ "battery.charge", 0, 0, "UPS.BatterySystem.Battery.RemainingCapacity", NULL, "%.0f", 0, legrand_times100M_info },
	{ "battery.runtime", 0, 0, "UPS.BatterySystem.Battery.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
	{ "battery.temperature", 0, 0, "UPS.BatterySystem.Temperature", NULL, "%s", 0, kelvin_celsius_conversion },

	/* Delays */
	{ "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL },
	{ "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.OutletSystem.Outlet.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_FLAG_ABSENT, NULL },
	{ "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL },
	{ "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_FLAG_ABSENT, NULL },
	{ "load.off.delay", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
	{ "load.on.delay", 0, 0, "UPS.OutletSystem.Outlet.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },
	{ "load.off.delay", 0, 0, "UPS.Output.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
	{ "load.on.delay", 0, 0, "UPS.Output.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },

	/* Battery Testing */
	{ "test.battery.start.quick", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "1", HU_TYPE_CMD, NULL },
	{ "test.battery.start.deep", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "2", HU_TYPE_CMD, NULL },
	{ "test.battery.stop", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "3", HU_TYPE_CMD, NULL },
	{ "test.battery.start.quick", 0, 0, "UPS.Output.Test", NULL, "1", HU_TYPE_CMD, NULL },
	{ "test.battery.start.deep", 0, 0, "UPS.Output.Test", NULL, "2", HU_TYPE_CMD, NULL },
	{ "test.battery.stop", 0, 0, "UPS.Output.Test", NULL, "3", HU_TYPE_CMD, NULL },

	/* Buzzer */
	{ "ups.beeper.status", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", HU_FLAG_SEMI_STATIC, beeper_info },
	{ "beeper.disable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "1", HU_TYPE_CMD, NULL },
	{ "beeper.enable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
	{ "beeper.mute", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *legrand_format_model(HIDDevice_t *hd)
{
	return hd->Product;
}

static const char *legrand_format_mfr(HIDDevice_t *hd)
{
	return hd->Vendor ? hd->Vendor : "Legrand";
}

static const char *legrand_format_serial(HIDDevice_t *hd)
{
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int legrand_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(legrand_usb_device_table, hd);

	switch (status)
	{
	case POSSIBLY_SUPPORTED:
		/* by default, reject, unless the productid option is given */
		if (getval("productid"))
			return 1;
		possibly_supported("Legrand", hd);
		return 0;

	case SUPPORTED:
		return 1;

	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

/* Keor DK units with the Cypress-based comm board (0665:5161) ship a
 * report descriptor that does not match what the firmware actually sends.
 * Both defects below were measured on a Keor DK 3k; they are applied only
 * to that VID:PID, and can be turned off with "disable_fix_report_desc".
 *
 * 1. Status flags. Feature report 0x32 is declared as a 24-bit anonymous
 *    block followed by 16 named PresentStatus flags (offsets 24..39), but
 *    the unit returns only 3 payload bytes and carries the real flags in
 *    bits 16..23 of the anonymous block. Captured across a mains -> battery
 *    -> mains transition:
 *        online, recharging : 00 00 11  (bits 16, 20)
 *        on battery         : 00 00 20  (bit 21)
 *        instant mains back : 00 00 01  (bit 16 alone)
 *    so bit 16 = ACPresent, bit 20 = Charging, bit 21 = Discharging.
 *    Relocate the named flags there. They also inherit a stale global
 *    Unit/UnitExp (seconds, 10^-2) from report 0x30, which would make a set
 *    flag read as 0.01, so clear it. No low-battery bit was observed; use
 *    the "ignorelb" flag so that LB is derived from battery.charge.low.
 *
 * 2. Battery voltages. UPS.BatterySystem.Battery.Voltage and .ConfigVoltage
 *    share their path with the Keor SP, whose mapping expects a different
 *    exponent. Correct the DK's unit exponents so that the existing entries
 *    yield 81.9 V / 72 V rather than 0.8 V / 0 V, leaving the SP untouched.
 */
static int legrand_fix_report_desc(HIDDevice_t *pDev, HIDDesc_t *pDesc_arg)
{
	size_t	i;
	int	retval = 0;

	if (pDev->VendorID != LEGRAND_CYPRESS_VENDORID
	 || pDev->ProductID != LEGRAND_PID_DK) {
		return 0;
	}

	if (disable_fix_report_desc) {
		upsdebugx(3, "%s: NOT Attempting Report Descriptor fix for Legrand Keor DK "
			"(got disable_fix_report_desc in config)", __func__);
		return 0;
	}

	upsdebugx(3, "%s: Attempting Report Descriptor fix for Legrand Keor DK", __func__);

	for (i = 0; i < pDesc_arg->nitems; i++) {
		HIDData_t	*pData = &pDesc_arg->item[i];
		HIDNode_t	leaf;

		if (pData->Type != ITEM_FEATURE || pData->Path.Size == 0)
			continue;

		leaf = pData->Path.Node[pData->Path.Size - 1];

		if (pData->ReportID == 0x32) {
			uint8_t	offset = 0;

			switch (leaf) {
			case USAGE_BAT_AC_PRESENT:	offset = 16; break;
			case USAGE_BAT_CHARGING:	offset = 20; break;
			case USAGE_BAT_DISCHARGING:	offset = 21; break;
			default: break;
			}

			if (offset) {
				upsdebugx(3, "%s: Fixing Report Descriptor: report 0x32 "
					"usage 0x%08" PRIxMAX " offset %" PRIu8 " -> %" PRIu8
					", Unit 0x%08" PRIxMAX " -> 0, UnitExp %" PRIi8 " -> 0",
					__func__,
					(uintmax_t)leaf, pData->Offset, offset,
					(uintmax_t)pData->Unit, pData->UnitExp);
				pData->Offset = offset;
				/* These are plain booleans, but they inherit the global
				 * Unit/UnitExp (seconds, 10^-2) left over from report 0x30,
				 * which turns a set flag into 0.01 and defeats the lookup. */
				pData->Unit = 0;
				pData->UnitExp = 0;
				retval = 1;
			}
		} else if (pData->ReportID == 0x20 && leaf == USAGE_POW_VOLTAGE) {
			/* UPS.BatterySystem.Battery.Voltage */
			upsdebugx(3, "%s: Fixing Report Descriptor: battery voltage "
				"UnitExp %" PRIi8 " -> %" PRIi8,
				__func__, pData->UnitExp, pData->UnitExp + 2);
			pData->UnitExp += 2;
			retval = 1;
		} else if (pData->ReportID == 0x04 && leaf == USAGE_POW_CONFIG_VOLTAGE) {
			/* UPS.BatterySystem.Battery.ConfigVoltage */
			upsdebugx(3, "%s: Fixing Report Descriptor: battery nominal voltage "
				"UnitExp %" PRIi8 " -> %" PRIi8,
				__func__, pData->UnitExp, pData->UnitExp + 7);
			pData->UnitExp += 7;
			retval = 1;
		}
	}

	return retval;
}

subdriver_t legrand_subdriver = {
	LEGRAND_HID_VERSION,
	legrand_claim,
	legrand_utab,
	legrand_hid2nut,
	legrand_format_model,
	legrand_format_mfr,
	legrand_format_serial,
	legrand_fix_report_desc,
	NULL,
};
