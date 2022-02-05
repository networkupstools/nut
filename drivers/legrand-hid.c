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

#define LEGRAND_HID_VERSION	"Legrand HID 0.2"

/* Legrand VendorID and ProductID */
#define LEGRAND_VID	0x1cb0	/* Legrand */
#define LEGRAND_PID_PDU	0x0038	/* Keor PDU model (800VA) */
#define LEGRAND_PID_SP	0x0032	/* Keor SP model (600, 800, 1000, 1500, 2000VA version) */

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
	{ USB_DEVICE(LEGRAND_VID, LEGRAND_PID_PDU),	disable_interrupt_pipe },	/* Legrand Keor PDU */
	{ USB_DEVICE(LEGRAND_VID, LEGRAND_PID_SP),	disable_interrupt_pipe },	/* Legrand Keor SP */

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
	{ "beeper.enable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD, NULL },
	{ "beeper.disable", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD, NULL },

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

subdriver_t legrand_subdriver = {
	LEGRAND_HID_VERSION,
	legrand_claim,
	legrand_utab,
	legrand_hid2nut,
	legrand_format_model,
	legrand_format_mfr,
	legrand_format_serial,
	fix_report_desc,
};
