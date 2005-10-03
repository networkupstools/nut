/*  apc-hid.c - data to monitor APC USB/HID devices with NUT
 *
 *  Copyright (C)  
 *	2003 - 2005	Arnaud Quette <arnaud.quette@free.fr>
 *	2005		John Stamp <kinsayder@hotmail.com>
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
#include "apc-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "dstate.h"   /* for STAT_INSTCMD_HANDLED */
#include "common.h"

#define APC_HID_VERSION "APC HID 0.8"

#define APC_VENDORID 0x051d

/* APC has two non-standard status items: "TLE" (time limit expired)
   and "BP" (battery present). The newhidups driver currently simply
   ignores these, but we add them anyway for completeness. */
info_lkp_t timelimitexpired_info[] = {
  { 1, "TLE", NULL },
  { 0, "NULL", NULL }
};

info_lkp_t batterypresent_info[] = {
  { 1, "BP", NULL },
  { 0, "NULL", NULL }
};

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* APC usage table */
usage_lkp_t apc_usage_lkp[] = {
	{ "APCGeneralCollection",	0xff860005 },
	{ "APCBattReplaceDate",		0xff860016 },
	{ "APCBattCapBeforeStartup",	0xff860019 }, /* FIXME: exploit */
	{ "APC_UPS_FirmwareRevision",	0xff860042 },
	{ "APCStatusFlag",		0xff860060 },
	{ "APCPanelTest",		0xff860072 }, /* FIXME: exploit */
	{ "APCShutdownAfterDelay",	0xff860076 }, /* FIXME: exploit */
	{ "APC_USB_FirmwareRevision",	0xff860079 }, /* FIXME: exploit */
	{ "APCForceShutdown",		0xff86007c },
	{ "APCDelayBeforeShutdown",	0xff86007d },
	{ "APCDelayBeforeStartup",	0xff86007e }, /* FIXME: exploit */

	/* FIXME: what is BUP? To what vendor do these Usages belong?
	 They seem to be here by mistake. -PS */
	{ "BUPHibernate",		0x00850058 }, /* FIXME: exploit */
	{ "BUPBattCapBeforeStartup",	0x00860012 }, /* FIXME: exploit */
	{ "BUPDelayBeforeStartup",	0x00860076 }, /* FIXME: exploit */
	{ "BUPSelfTest",		0x00860010 }, /* FIXME: exploit */

	{  "\0", 0x0 }
};

/*
 * USB USAGE NOTES for APC (from Russell Kroll in the old hidups
 *
 * FIXME: read 0xff86.... instead of 0x(00)86....?
 *
 *  0x860013 == 44200155090 - capability again                   
 *           == locale 4, 4 choices, 2 bytes, 00, 15, 50, 90     
 *           == minimum charge to return online                  
 *
 *  0x860060 == "441HMLL" - looks like a 'capability' string     
 *           == locale 4, 4 choices, 1 byte each                 
 *           == line sensitivity (high, medium, low, low)        
 *  NOTE! the above does not seem to correspond to my info 
 *
 *  0x860062 == D43133136127130                                  
 *           == locale D, 4 choices, 3 bytes, 133, 136, 127, 130 
 *           == high transfer voltage                            
 *
 *  0x860064 == D43103100097106                                  
 *           == locale D, 4 choices, 3 bytes, 103, 100, 097, 106 
 *           == low transfer voltage                             
 *
 *  0x860066 == 441HMLL (see 860060)                                   
 *
 *  0x860074 == 4410TLN                                          
 *           == locale 4, 4 choices, 1 byte, 0, T, L, N          
 *           == alarm setting (5s, 30s, low battery, none)       
 *
 *  0x860077 == 443060180300600                                  
 *           == locale 4, 4 choices, 3 bytes, 060,180,300,600    
 *           == wake-up delay (after power returns)              
 */

static usage_tables_t apc_utab[] = {
	apc_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/*      HID2NUT lookup table                                       */
/* --------------------------------------------------------------- */

/* HID2NUT lookup table */
static hid_info_t apc_hid2nut[] = {
  /* Server side variables */
  { "driver.version.internal", ST_FLAG_STRING, sizeof(DRIVER_VERSION), NULL, NULL,
    DRIVER_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  { "driver.version.data", ST_FLAG_STRING, sizeof(APC_HID_VERSION), NULL, NULL,
    APC_HID_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  
  /* Battery page */
  { "battery.charge", 0, 1, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.runtime.low", ST_FLAG_RW | ST_FLAG_STRING, 0, "UPS.Battery.RemainingTimeLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.voltage",  0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.Battery.ConfigVoltage", NULL,
    "%.1f", HU_FLAG_OK, NULL },
  { "battery.temperature", 0, 0,
    "UPS.Battery.Temperature", NULL, "%.1f", HU_FLAG_OK, NULL },

  /* UPS page */
  { "ups.load", 0, 1, "UPS.Output.PercentLoad", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "ups.load", 0, 1, "UPS.PowerConverter.PercentLoad", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 0,
    "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL},
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 0,
    "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL},
  { "ups.test.result", 0, 0,
    "UPS.Battery.Test", NULL, "%s", HU_FLAG_OK, &test_read_info[0] },
  /* the below one need to be discussed as we might need to complete
   * the ups.test sub collection
   * { "ups.test.panel", 0, 0,
   *   "UPS.APCPanelTest", NULL, "%.0f", HU_FLAG_OK, NULL }, */
  { "ups.temperature", 0, 0,
    "UPS.Battery.Temperature", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "ups.beeper.status", 0, 0, "UPS.AudibleAlarmControl", NULL, "%s", HU_FLAG_OK, &beeper_info[0] },
  { "ups.mfr.date", 0, 0, "UPS.ManufacturerDate", NULL, "%s", HU_FLAG_OK, &date_conversion[0] },
  { "battery.mfr.date", 0, 0, "UPS.Battery.ManufacturerDate", NULL, "%s", HU_FLAG_OK, &date_conversion[0] },
  { "battery.date", 0, 0, "UPS.Battery.APCBattReplaceDate", NULL, "%s", HU_FLAG_OK, &date_conversion[0] },

  /* Special case: ups.status */
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ACPresent", NULL,
    "%.0f", HU_FLAG_OK, &onbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Discharging",NULL, 
    "%.0f", HU_FLAG_OK, &discharging_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Charging", NULL,
    "%.0f", HU_FLAG_OK, &charging_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL,
    "%.0f", HU_FLAG_OK, &shutdownimm_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL,
    "%.0f", HU_FLAG_OK, &lowbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.OverLoad", NULL,
    "%.0f", HU_FLAG_OK, &overbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL,
    "%.0f", HU_FLAG_OK, &replacebatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL,
    "%.0f", HU_FLAG_OK, &timelimitexpired_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.BatteryPresent", NULL,
    "%.0f", HU_FLAG_OK, &batterypresent_info[0] },

  /* Input page */
  { "input.voltage", 0, 0, "UPS.Input.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "input.voltage.nominal", 0, 0, "UPS.Input.ConfigVoltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 0, "UPS.Input.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 0, "UPS.Input.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },

  /* Output page */
  { "output.voltage", 0, 0, "UPS.Output.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "output.voltage.target.line", 0, 0,
    "UPS.Output.ConfigVoltage", NULL, "%.1f", HU_FLAG_OK, NULL },

  /* instant commands. */
  /* splited into subset while waiting for extradata support
   * ie: test.battery.start quick
   */
  { "test.battery.start.quick", 0, 0,
    "UPS.BatterySystem.Battery.Test", NULL, "1", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] }, /* TODO: lookup needed? */
  { "test.battery.start.deep", 0, 0,
    "UPS.BatterySystem.Battery.Test", NULL, "2", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] },
  { "test.battery.stop", 0, 0,
    "UPS.BatterySystem.Battery.Test", NULL, "3", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] },
  { "test.panel.start", 0, 0,
    "UPS.APCPanelTest", NULL, "1", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "test.panel.stop", 0, 0,
    "UPS.APCPanelTest", NULL, "0", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "load.off", 0, 0,
    "UPS.PowerSummary.DelayBeforeShutdown", NULL, "0", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "load.off", 0, 0,
    "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, "0", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "load.on", 0, 0,
    "UPS.PowerSummary.DelayBeforeStartup", NULL, "0", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },

	/* FIXME (@John): is it a good mapping considering the behaviour? */
	{ "shutdown.return", 0, 0, "UPS.APCGeneralCollection.APCForceShutdown",
		NULL, "1", /* point to good value */
		HU_TYPE_CMD | HU_FLAG_OK, NULL },

  { "shutdown.stop", 0, 0,
    "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, "-1", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "shutdown.stop", 0, 0,
    "UPS.PowerSummary.DelayBeforeShutdown", NULL, "-1", /* point to good value */
    HU_TYPE_CMD | HU_FLAG_OK, NULL },

  { "beeper.on", 0, 0, "UPS.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "beeper.off", 0, 0, "UPS.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD | HU_FLAG_OK, NULL },

  /* TODO: beeper.on/off, bypass.start/stop, shutdown.return/stayoff/stop/reboot[.graceful] */

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

/* shutdown method for APC */
static int apc_shutdown(int ondelay, int offdelay) {
	/* FIXME: ondelay, offdelay currently not used */

	/* FIXME: the data (or command) should appear in
	 * the hid2nut table, so that it can be autodetected
	 * upon startup, and then calable through setvar()
	 * or instcmd(), ie below
	 */
	
	/* From apcupsd, usb.c/killpower() */
	/* 1) APCBattCapBeforeStartup */
	/* 2) BackUPS Pro => */
	
	/* Misc method B */
	upsdebugx(2, "Trying APC ForceShutdown style shutdown.");
	if (instcmd("load.off", NULL) == STAT_INSTCMD_HANDLED) {
		return 1;
	}

	upsdebugx(2, "ForceShutdown command failed, trying APC Delay style shutdown.");
	if (instcmd("shutdown.return", NULL) == STAT_INSTCMD_HANDLED) {
		return 1;
	}
	upsdebugx(2, "Delayed Shutdown command failed.");
	return 0;
}

static char *apc_format_model(HIDDevice *hd) {
	char *model;
        char *ptr1, *ptr2;

	/* FIXME?: what is the path "UPS.APC_UPS_FirmwareRevision"? */
	model = hd->Product ? hd->Product : "unknown";
	ptr1 = strstr(model, "FW:");
	if (ptr1)
	{
		*(ptr1 - 1) = '\0';
		ptr1 += strlen("FW:");
		ptr2 = strstr(ptr1, "USB FW:");
		if (ptr2)
		{
			*(ptr2 - 1) = '\0';
			ptr2 += strlen("USB FW:");
			dstate_setinfo("ups.firmware.aux", "%s", ptr2);
		}
		dstate_setinfo("ups.firmware", "%s", ptr1);
	}
	return model;
}

static char *apc_format_mfr(HIDDevice *hd) {
	return hd->Vendor ? hd->Vendor : "APC";
}

static char *apc_format_serial(HIDDevice *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int apc_claim(HIDDevice *hd) {
	if (hd->VendorID == APC_VENDORID) {
		return 1;
	} else {
		return 0;
	}
}

subdriver_t apc_subdriver = {
	APC_HID_VERSION,
	apc_claim,
	apc_utab,
        apc_hid2nut,
	apc_shutdown,
	apc_format_model,
	apc_format_mfr,
	apc_format_serial,
};
