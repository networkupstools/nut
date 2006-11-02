/* ablerex-hid.c - subdriver to monitor Ablerex USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2005 Arnaud Quette <arnaud.quette@free.fr>
 *  2005 - 2006 Peter Selinger <selinger@users.sourceforge.net>         
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
 *
 */

#include "newhidups.h"
#include "ablerex-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "dstate.h"   /* for STAT_INSTCMD_HANDLED */
#include "main.h"     /* for getval() */
#include "common.h"

#define ABLEREX_HID_VERSION      "Ablerex HID 0.1"

#define ABLEREX_VENDORID 0xffff

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* ABLEREX usage table */
static usage_lkp_t ablerex_usage_lkp[] = {
	{ "ABLEREX1",	 0x00840039 },
	{ "ABLEREXUPS", 0x00860004 },
	{  "\0", 0x0 }
};

static usage_tables_t ablerex_utab[] = {
	ablerex_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t ablerex_hid2nut[] = {

  { "unmapped.ablerexups.flow.flowid", 0, 0, "ABLEREXUPS.Flow.FlowID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.configvoltage", 0, 0, "ABLEREXUPS.Flow.ConfigVoltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.configfrequency", 0, 0, "ABLEREXUPS.Flow.ConfigFrequency", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.lowvoltagetransfer", 0, 0, "ABLEREXUPS.Flow.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.highvoltagetransfer", 0, 0, "ABLEREXUPS.Flow.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.imanufacturer", 0, 0, "ABLEREXUPS.Flow.iManufacturer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.iproduct", 0, 0, "ABLEREXUPS.Flow.iProduct", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.iserialnumber", 0, 0, "ABLEREXUPS.Flow.iSerialNumber", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.flowid", 0, 0, "ABLEREXUPS.Flow.FlowID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.configvoltage", 0, 0, "ABLEREXUPS.Flow.ConfigVoltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.configfrequency", 0, 0, "ABLEREXUPS.Flow.ConfigFrequency", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.configapparentpower", 0, 0, "ABLEREXUPS.Flow.ConfigApparentPower", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.configactivepower", 0, 0, "ABLEREXUPS.Flow.ConfigActivePower", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.delaybeforestartup", 0, 0, "ABLEREXUPS.Flow.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.flow.delaybeforeshutdown", 0, 0, "ABLEREXUPS.Flow.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.batterysystem.batterysystemid", 0, 0, "ABLEREXUPS.BatterySystem.BatterySystemID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.batterysystem.presentstatus.used", 0, 0, "ABLEREXUPS.BatterySystem.PresentStatus.Used", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.batterysystem.presentstatus.good", 0, 0, "ABLEREXUPS.BatterySystem.PresentStatus.Good", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.batterysystem.voltage", 0, 0, "ABLEREXUPS.BatterySystem.Voltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.batterysystem.temperature", 0, 0, "ABLEREXUPS.BatterySystem.Temperature", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.batterysystem.test", 0, 0, "ABLEREXUPS.BatterySystem.Test", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.batterysystem.test", 0, 0, "ABLEREXUPS.BatterySystem.Test", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.powerconverterid", 0, 0, "ABLEREXUPS.PowerConverter.PowerConverterID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.input.inputid", 0, 0, "ABLEREXUPS.PowerConverter.Input.InputID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.input.flowid", 0, 0, "ABLEREXUPS.PowerConverter.Input.FlowID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.input.presentstatus.good", 0, 0, "ABLEREXUPS.PowerConverter.Input.PresentStatus.Good", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.input.voltage", 0, 0, "ABLEREXUPS.PowerConverter.Input.Voltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.input.frequency", 0, 0, "ABLEREXUPS.PowerConverter.Input.Frequency", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.output.outputid", 0, 0, "ABLEREXUPS.PowerConverter.Output.OutputID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.output.flowid", 0, 0, "ABLEREXUPS.PowerConverter.Output.FlowID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.output.voltage", 0, 0, "ABLEREXUPS.PowerConverter.Output.Voltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.output.frequency", 0, 0, "ABLEREXUPS.PowerConverter.Output.Frequency", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.output.percentload", 0, 0, "ABLEREXUPS.PowerConverter.Output.PercentLoad", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.output.presentstatus.overload", 0, 0, "ABLEREXUPS.PowerConverter.Output.PresentStatus.Overload", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.output.presentstatus.boost", 0, 0, "ABLEREXUPS.PowerConverter.Output.PresentStatus.Boost", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.output.presentstatus.buck", 0, 0, "ABLEREXUPS.PowerConverter.Output.PresentStatus.Buck", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.ablerexups.powerconverter.output.presentstatus.ablerex1", 0, 0, "ABLEREXUPS.PowerConverter.Output.PresentStatus.ABLEREX1", NULL, "%.0f", HU_FLAG_OK, NULL },

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

/* shutdown method for Ablerex */
static int ablerex_shutdown(int ondelay, int offdelay) {
	/* FIXME: ondelay, offdelay currently not used */
	
	/* Default method */
	upsdebugx(2, "Trying load.off.");
        if (instcmd("load.off", NULL) == STAT_INSTCMD_HANDLED) {
                return 1;
        }
	upsdebugx(2, "Shutdown failed.");
        return 0;
}

static char *ablerex_format_model(HIDDevice *hd) {
	return hd->Product;
}

static char *ablerex_format_mfr(HIDDevice *hd) {
	return hd->Vendor ? hd->Vendor : "Ablerex";
}

static char *ablerex_format_serial(HIDDevice *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int ablerex_claim(HIDDevice *hd) {
	if (hd->VendorID != ABLEREX_VENDORID) {
		return 0;
	}
	switch (hd->ProductID) {

	/* accept any known UPS - add devices here as needed */
	case 0000:
		return 1;

	/* by default, reject, unless the productid option is given */
	default:
		if (getval("productid")) {
			return 1;
		} else {
			upsdebugx(1,
"This particular ablerex device (%04x/%04x) is not (or perhaps not yet)\n"
"supported by newhidups. Try running the driver with the '-x productid=%04x'\n"
"option. Please report your results to the NUT developer's mailing list.\n",
						 hd->VendorID, hd->ProductID, hd->ProductID);
			return 0;
		}
	}
}

subdriver_t ablerex_subdriver = {
	ABLEREX_HID_VERSION,
	ablerex_claim,
	ablerex_utab,
	ablerex_hid2nut,
	ablerex_shutdown,
	ablerex_format_model,
	ablerex_format_mfr,
	ablerex_format_serial,
};
