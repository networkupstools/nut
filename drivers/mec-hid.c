/* mec-hid.c - subdriver to monitor MEC USB/HID devices with NUT
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
#include "mec-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "dstate.h"   /* for STAT_INSTCMD_HANDLED */
#include "main.h"     /* for getval() */
#include "common.h"

#define MEC_HID_VERSION      "MEC HID 0.1"

#define MEC_VENDORID 0x0001

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* MEC usage table */
static usage_lkp_t mec_usage_lkp[] = {
	{ "MEC1",	0x00860004 },
	{  "\0", 0x0 }
};

static usage_tables_t mec_utab[] = {
	mec_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t mec_hid2nut[] = {

  { "unmapped.mec1.flow.flowid", 0, 0, "MEC1.Flow.FlowID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.configvoltage", 0, 0, "MEC1.Flow.ConfigVoltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.configfrequency", 0, 0, "MEC1.Flow.ConfigFrequency", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.lowvoltagetransfer", 0, 0, "MEC1.Flow.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.highvoltagetransfer", 0, 0, "MEC1.Flow.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.imanufacturer", 0, 0, "MEC1.Flow.iManufacturer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.iproduct", 0, 0, "MEC1.Flow.iProduct", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.iserialnumber", 0, 0, "MEC1.Flow.iSerialNumber", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.flowid", 0, 0, "MEC1.Flow.FlowID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.configvoltage", 0, 0, "MEC1.Flow.ConfigVoltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.configfrequency", 0, 0, "MEC1.Flow.ConfigFrequency", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.configapparentpower", 0, 0, "MEC1.Flow.ConfigApparentPower", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.configactivepower", 0, 0, "MEC1.Flow.ConfigActivePower", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.delaybeforestartup", 0, 0, "MEC1.Flow.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.flow.delaybeforeshutdown", 0, 0, "MEC1.Flow.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.batterysystem.batterysystemid", 0, 0, "MEC1.BatterySystem.BatterySystemID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.batterysystem.presentstatus.used", 0, 0, "MEC1.BatterySystem.PresentStatus.Used", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.batterysystem.presentstatus.good", 0, 0, "MEC1.BatterySystem.PresentStatus.Good", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.batterysystem.voltage", 0, 0, "MEC1.BatterySystem.Voltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.batterysystem.temperature", 0, 0, "MEC1.BatterySystem.Temperature", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.batterysystem.test", 0, 0, "MEC1.BatterySystem.Test", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.batterysystem.test", 0, 0, "MEC1.BatterySystem.Test", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.powerconverterid", 0, 0, "MEC1.PowerConverter.PowerConverterID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.input.inputid", 0, 0, "MEC1.PowerConverter.Input.InputID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.input.flowid", 0, 0, "MEC1.PowerConverter.Input.FlowID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.input.presentstatus.good", 0, 0, "MEC1.PowerConverter.Input.PresentStatus.Good", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.input.voltage", 0, 0, "MEC1.PowerConverter.Input.Voltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.input.frequency", 0, 0, "MEC1.PowerConverter.Input.Frequency", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.output.outputid", 0, 0, "MEC1.PowerConverter.Output.OutputID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.output.flowid", 0, 0, "MEC1.PowerConverter.Output.FlowID", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.output.voltage", 0, 0, "MEC1.PowerConverter.Output.Voltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.output.frequency", 0, 0, "MEC1.PowerConverter.Output.Frequency", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.output.percentload", 0, 0, "MEC1.PowerConverter.Output.PercentLoad", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.output.presentstatus.overload", 0, 0, "MEC1.PowerConverter.Output.PresentStatus.Overload", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.output.presentstatus.boost", 0, 0, "MEC1.PowerConverter.Output.PresentStatus.Boost", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "unmapped.mec1.powerconverter.output.presentstatus.buck", 0, 0, "MEC1.PowerConverter.Output.PresentStatus.Buck", NULL, "%.0f", HU_FLAG_OK, NULL },

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

/* shutdown method for MEC */
static int mec_shutdown(int ondelay, int offdelay) {
	/* FIXME: ondelay, offdelay currently not used */
	
	/* Default method */
	upsdebugx(2, "Trying load.off.");
        if (instcmd("load.off", NULL) == STAT_INSTCMD_HANDLED) {
                return 1;
        }
	upsdebugx(2, "Shutdown failed.");
        return 0;
}

static char *mec_format_model(HIDDevice *hd) {
	return hd->Product;
}

static char *mec_format_mfr(HIDDevice *hd) {
	return hd->Vendor ? hd->Vendor : "MEC";
}

static char *mec_format_serial(HIDDevice *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int mec_claim(HIDDevice *hd) {
	if (hd->VendorID != MEC_VENDORID) {
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
"This particular mec device (%04x/%04x) is not (or perhaps not yet)\n"
"supported by newhidups. Try running the driver with the '-x productid=%04x'\n"
"option. Please report your results to the NUT developer's mailing list.\n",
						 hd->VendorID, hd->ProductID, hd->ProductID);
			return 0;
		}
	}
}

subdriver_t mec_subdriver = {
	MEC_HID_VERSION,
	mec_claim,
	mec_utab,
	mec_hid2nut,
	mec_shutdown,
	mec_format_model,
	mec_format_mfr,
	mec_format_serial,
};
