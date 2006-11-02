/*  powercom-hid.c - data to monitor PowerCom USB/HID devices with NUT
 *
 *  Copyright (C)  
 *	2003 - 2005	Arnaud Quette <arnaud.quette@free.fr>
 *      2005            Peter Selinger <selinger@users.sourceforge.net>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://www.mgeups.com>
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "newhidups.h"
#include "powercom-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "dstate.h"   /* for STAT_INSTCMD_HANDLED */
#include "common.h"

#define POWERCOM_HID_VERSION "PowerCom HID 0.1"

#define POWERCOM_VENDORID 0x0d9f

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* PowerCom usage table */
usage_lkp_t powercom_usage_lkp[] = {
	{ "POWERCOM_UPS",	      0x00020004 },
	{ "POWERCOM_Coll1",     0x00020010 },
	{ "POWERCOM_Coll2",     0x00020016 },
	{ "POWERCOM_Sub1",      0x0002001a },
	{ "POWERCOM_Sub2",      0x0002001c },
	{ "POWERCOM_Feature1",	0x00020086 },
	{ "POWERCOM_Feature2",	0x00020087 },
	{ "POWERCOM_Feature3",	0x00020058 },
	{ "POWERCOM_Feature4",	0x00020068 },
	{ "POWERCOM_Feature5",	0x00020057 },
	{ "POWERCOM_Feature6",  0x00020056 },
	{ "POWERCOM_Input1",    0x00020083 },
	{ "POWERCOM_Input2",    0x00020085 },
	{ "POWERCOM_Input3",    0x00020036 },
	{ "POWERCOM_Input4",    0x00020030 },
	{ "POWERCOM_Input5",    0x00020084 },
	{ "POWERCOM_Input6",    0x00020032 },
	{ "POWERCOM_Input7",    0x00020035 },
	{ "POWERCOM_Input8",    0x00020081 },
	{ "POWERCOM_Input10",   0x00020082 },
	{  "\0", 0x0 }
};

static usage_tables_t powercom_utab[] = {
	powercom_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/*      HID2NUT lookup table                                       */
/* --------------------------------------------------------------- */

/* HID2NUT lookup table */
static hid_info_t powercom_hid2nut[] = {
  /* Server side variables */
  { "driver.version.internal", ST_FLAG_STRING, sizeof(DRIVER_VERSION), NULL, NULL,
    DRIVER_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  { "driver.version.data", ST_FLAG_STRING, sizeof(POWERCOM_HID_VERSION), NULL, NULL,
    POWERCOM_HID_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  
  /* Unmapped PowerCom items */
  { "powercom.feature1", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Feature1", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.feature2", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Feature2", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.input1", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Input1", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.input2", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Input2", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll1.input3", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll1.POWERCOM_Input3", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll1.input4", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll1.POWERCOM_Input4", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll1.input5", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll1.POWERCOM_Input5", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll2.sub1.input6", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll2.POWERCOM_Sub1.POWERCOM_Input6", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll2.sub1.input4", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll2.POWERCOM_Sub1.POWERCOM_Input4", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll2.sub2.input4", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll2.POWERCOM_Sub2.POWERCOM_Input4", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll2.sub2.input7", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll2.POWERCOM_Sub2.POWERCOM_Input7", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll2.sub2.input8", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll2.POWERCOM_Sub2.POWERCOM_Input8", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll2.sub2.input10", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll2.POWERCOM_Sub2.POWERCOM_Input10", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll2.sub2.input6", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll2.POWERCOM_Sub2.POWERCOM_Input6", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll2.feature3", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll2.POWERCOM_Feature3", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll2.feature4", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll2.POWERCOM_Feature4", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll2.feature5", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll2.POWERCOM_Feature5", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "powercom.coll2.feature6", ST_FLAG_RW, 0, "POWERCOM_UPS.POWERCOM_Coll2.POWERCOM_Feature6", NULL, "%.0f", HU_FLAG_OK, NULL },

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

/* shutdown method for PowerCom */
static int powercom_shutdown(int ondelay, int offdelay) {
	/* FIXME */
	upsdebugx(2, "PowerCom: Shutdown not implemented - failed.");
	return 0;
}

static char *powercom_format_model(HIDDevice *hd) {
	return hd->Product;
}

static char *powercom_format_mfr(HIDDevice *hd) {
	return hd->Vendor ? hd->Vendor : "PowerCom";
}

static char *powercom_format_serial(HIDDevice *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int powercom_claim(HIDDevice *hd) {
	if (hd->VendorID != POWERCOM_VENDORID) {
		return 0;
	}
	switch (hd->ProductID) {

	case  0x0001:
		return 1;  /* accept known UPSs */

	default:
		if (getval("productid")) {
			return 1;
		} else {
			upsdebugx(1,
"This particular PowerCom device (%04x/%04x) is not (or perhaps not yet)\n"
"supported by newhidups. Try running the driver with the '-x productid=%04x'\n"
"option. Please report your results to the NUT developer's mailing list.\n",
						 hd->VendorID, hd->ProductID, hd->ProductID);
			return 0;
		}
	}
}

subdriver_t powercom_subdriver = {
	POWERCOM_HID_VERSION,
	powercom_claim,
	powercom_utab,
   powercom_hid2nut,
	powercom_shutdown,
	powercom_format_model,
	powercom_format_mfr,
	powercom_format_serial,
};
