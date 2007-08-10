/*  apc-hid.c - data to monitor APC and CyberPower USB/HID devices with NUT
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

#include "usbhid-ups.h"
#include "apc-hid.h"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "dstate.h"   /* for STAT_INSTCMD_HANDLED */
#include "main.h"     /* for getval() */
#include "common.h"

#define APC_HID_VERSION "APC/CyberPower HID 0.9"

#define APC_VENDORID 0x051d /* APC */
#define CPS_VENDORID 0x0764 /* CyberPower */

/* some conversion functions specific to CyberPower */

/* returns statically allocated string - must not use it again before
   done with result! */
static char *watts_to_av_conversion_fun(long value) {
	static char buf[20];
	
	snprintf(buf, sizeof(buf), "%.0f", value * 1.4142136);
	return buf;
}

static info_lkp_t watts_to_av_conversion[] = {
	{ 0, NULL, watts_to_av_conversion_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *apc_date_conversion_fun(long value) {
  static char buf[20];
  int year, month, day;

  if (value == 0) {
    return "not set";
  }

  /* APC apparently uses a hexadecimal-as-decimal format, e.g.,
  0x102202 = October 22, 2002 */
  year = (value & 0xf) + 10 * ((value>>4) & 0xf);
  month = ((value>>16) & 0xf) + 10 * ((value>>20) & 0xf);
  day = ((value>>8) & 0xf) + 10 * ((value>>12) & 0xf);

  /* Y2K conversion - hope that this format will be retired before 2070 :) */
  if (year >= 70) {
    year += 1900;
  } else {
    year += 2000;
  }

  snprintf(buf, sizeof(buf), "%04d/%02d/%02d", year, month, day);
  return buf;
}

info_lkp_t apc_date_conversion[] = {
  { 0, NULL, apc_date_conversion_fun }
};

/* APC has two non-NUT-standard status items: "time limit expired" and
   "battery present". The usbhid-ups driver currently ignores
   batterypres, and maps timelimitexp to LB. CyberPower has the
   non-NUT-standard status item "fully charged". The usbhid-ups driver
   currently ignores it. */
static info_lkp_t timelimitexpired_info[] = {
  { 1, "timelimitexp", NULL },
  { 0, "!timelimitexp", NULL },
  { 0, NULL, NULL }
};

static info_lkp_t batterypresent_info[] = {
  { 1, "batterypres", NULL },
  { 0, "!batterypres", NULL },
  { 0, NULL, NULL }
};

/* This was determined empirically from observing a BackUPS LS 500.
 */
static info_lkp_t apcstatusflag_info[] = {
  { 8, "!off", NULL },  /* Normal operation */
  { 16, "!off", NULL }, /* This occurs briefly during power-on, and corresponds to status 'DISCHRG'. */
  { 0, "off", NULL },
  { 0, NULL, NULL }
};

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* APC usage table */
static usage_lkp_t apc_usage_lkp[] = {
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
  { "driver.version.internal", ST_FLAG_STRING, sizeof(DRIVER_VERSION), NULL, NULL, DRIVER_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  { "driver.version.data", ST_FLAG_STRING, sizeof(APC_HID_VERSION), NULL, NULL, APC_HID_VERSION, HU_FLAG_ABSENT | HU_FLAG_OK, NULL },
  
  /* Battery page */
  { "battery.charge", 0, 1, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.runtime.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.RemainingTimeLimit", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "battery.voltage",  0, 0, "UPS.PowerSummary.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.Battery.ConfigVoltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.1f", HU_FLAG_OK, NULL }, /* CyberPower, Back-UPS 500 */
  { "battery.temperature", 0, 0, "UPS.Battery.Temperature", NULL, "%s", HU_FLAG_OK, &kelvin_celsius_conversion[0] },
  { "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_OK, stringid_conversion },
  { "battery.mfr.date", 0, 0, "UPS.Battery.ManufacturerDate", NULL, "%s", HU_FLAG_OK, &date_conversion[0] },
  { "battery.mfr.date", 0, 0, "UPS.PowerSummary.APCBattReplaceDate", NULL, "%s", HU_FLAG_OK, &apc_date_conversion[0] }, /* Back-UPS 500, Back-UPS ES/CyberFort 500 */
  { "battery.date", 0, 0, "UPS.Battery.APCBattReplaceDate", NULL, "%s", HU_FLAG_OK, &apc_date_conversion[0] }, /* Observed values: 0x0 on Back-UPS ES 650, 0x92501 on Back-UPS BF500 whose manufacture date was 2005/01/20 - this makes little sense but at least it's a valid date. */

  /* UPS page */
  { "ups.load", 0, 1, "UPS.Output.PercentLoad", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "ups.load", 0, 1, "UPS.PowerConverter.PercentLoad", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL},
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL}, /* APC */
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_OK, NULL}, /* CyberPower */
  { "ups.delay.restart", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Output.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_OK, NULL}, /* CyberPower */
  { "ups.test.result", 0, 0, "UPS.Battery.Test", NULL, "%s", HU_FLAG_OK, &test_read_info[0] },
  { "ups.test.result", 0, 0, "UPS.Output.Test", NULL, "%s", HU_FLAG_OK, &test_read_info[0] }, /* CyberPower */
  { "ups.beeper.status", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.AudibleAlarmControl", NULL, "%s", HU_FLAG_OK, &beeper_info[0] },
  { "ups.mfr.date", 0, 0, "UPS.ManufacturerDate", NULL, "%s", HU_FLAG_OK, &date_conversion[0] },
  { "ups.mfr.date", 0, 0, "UPS.PowerSummary.ManufacturerDate", NULL, "%s", HU_FLAG_OK, &date_conversion[0] }, /* Back-UPS 500 */
  { "ups.power.nominal", 0, 0, "UPS.Output.ConfigActivePower", NULL, "%s", HU_FLAG_OK, watts_to_av_conversion }, /* CyberPower */


  /* the below one need to be discussed as we might need to complete
   * the ups.test sub collection
   * { "ups.test.panel", 0, 0, "UPS.APCPanelTest", NULL, "%.0f", HU_FLAG_OK, NULL }, */

  /* Special case: ups.status */
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, "%.0f", HU_FLAG_OK, &online_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Discharging", NULL, "%.0f", HU_FLAG_OK, &discharging_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Charging", NULL, "%.0f", HU_FLAG_OK, &charging_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, "%.0f", HU_FLAG_OK, &shutdownimm_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, "%.0f", HU_FLAG_OK, &lowbatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.Overload", NULL, "%.0f", HU_FLAG_OK, &overload_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, "%.0f", HU_FLAG_OK, &replacebatt_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL, "%.0f", HU_FLAG_OK, &timelimitexpired_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.BatteryPresent", NULL, "%.0f", HU_FLAG_OK, &batterypresent_info[0] },

  { "ups.status", 0, 1, "UPS.PowerSummary.Charging", NULL, "%.0f", HU_FLAG_OK, &charging_info[0] }, /* Back-UPS 500 */
  { "ups.status", 0, 1, "UPS.PowerSummary.Discharging", NULL, "%.0f", HU_FLAG_OK, &discharging_info[0] }, /* Back-UPS 500 */
  { "ups.status", 0, 1, "UPS.PowerSummary.ACPresent", NULL, "%.0f", HU_FLAG_OK, &online_info[0] }, /* Back-UPS 500 */
  { "ups.status", 0, 1, "UPS.PowerSummary.BelowRemainingCapacityLimit", NULL, "%.0f", HU_FLAG_OK, &lowbatt_info[0] }, /* Back-UPS 500 */
  { "ups.status", 0, 1, "UPS.PowerSummary.ShutdownImminent", NULL, "%.0f", HU_FLAG_OK, &shutdownimm_info[0] },
  { "ups.status", 0, 1, "UPS.PowerSummary.APCStatusFlag", NULL, "%.0f", HU_FLAG_OK, &apcstatusflag_info[0] }, /* APC Back-UPS LS 500 */

  { "ups.status", 0, 1, "UPS.PowerSummary.PresentStatus.FullyCharged", NULL, "%.0f", HU_FLAG_OK, &fullycharged_info[0] }, /* CyberPower */
  { "ups.status", 0, 1, "UPS.Output.Overload", NULL, "%.0f", HU_FLAG_OK, &overload_info[0] }, /* CyberPower */
  { "ups.status", 0, 1, "UPS.Output.Boost", NULL, "%.0f", HU_FLAG_OK, &boost_info[0] }, /* CyberPower */

  /* Input page */
  { "input.voltage", 0, 0, "UPS.Input.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "input.voltage.nominal", 0, 0, "UPS.Input.ConfigVoltage", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.transfer.low", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Input.LowVoltageTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },
  { "input.transfer.high", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.Input.HighVoltageTransfer", NULL, "%.0f", HU_FLAG_OK, NULL },

  /* Output page */
  { "output.voltage", 0, 0, "UPS.Output.Voltage", NULL, "%.1f", HU_FLAG_OK, NULL },
  { "output.voltage.nominal", 0, 0, "UPS.Output.ConfigVoltage", NULL, "%.1f", HU_FLAG_OK, NULL },

  /* instant commands. */
  /* test.* split into subset while waiting for extradata support
   * ie: test.battery.start quick
   */
  { "test.battery.start.quick", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "1", HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] }, /* TODO: lookup needed? */
  { "test.battery.start.deep", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "2", HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] },
  { "test.battery.stop", 0, 0, "UPS.BatterySystem.Battery.Test", NULL, "3", HU_TYPE_CMD | HU_FLAG_OK, &test_write_info[0] },
  { "test.panel.start", 0, 0, "UPS.APCPanelTest", NULL, "1", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "test.panel.stop", 0, 0, "UPS.APCPanelTest", NULL, "0", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "test.panel.start", 0, 0, "UPS.PowerSummary.APCPanelTest", NULL, "1", HU_TYPE_CMD | HU_FLAG_OK, NULL }, /* Back-UPS 500 */
  { "test.panel.stop", 0, 0, "UPS.PowerSummary.APCPanelTest", NULL, "0", HU_TYPE_CMD | HU_FLAG_OK, NULL }, /* Back-UPS 500 */

  { "load.off", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "0", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "load.off", 0, 0, "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, "0", HU_TYPE_CMD | HU_FLAG_OK, NULL }, /* APC Backups ES */
  { "load.off", 0, 0, "UPS.Output.DelayBeforeShutdown", NULL, "0", HU_TYPE_CMD | HU_FLAG_OK, NULL }, /* CyberPower */

  { "load.on", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, "0", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "load.on", 0, 0, "UPS.Output.DelayBeforeStartup", NULL, "0", HU_TYPE_CMD | HU_FLAG_OK, NULL }, /* CyberPower */

  /* FIXME (@John): is it a good mapping considering the behaviour? */
  { "shutdown.return", 0, 0, "UPS.APCGeneralCollection.APCForceShutdown", NULL, "1", HU_TYPE_CMD | HU_FLAG_OK, NULL }, /* APC Backups ES */
  
  { "shutdown.stop", 0, 0, "UPS.APCGeneralCollection.APCDelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD | HU_FLAG_OK, NULL }, /* APC Backups ES */
  { "shutdown.stop", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "shutdown.stop", 0, 0, "UPS.Output.DelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD | HU_FLAG_OK, NULL }, /* CyberPower */

  { "beeper.on", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "2", HU_TYPE_CMD | HU_FLAG_OK, NULL },
  { "beeper.off", 0, 0, "UPS.PowerSummary.AudibleAlarmControl", NULL, "3", HU_TYPE_CMD | HU_FLAG_OK, NULL },

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

static char *apc_format_model(HIDDevice_t *hd) {
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

static char *apc_format_mfr(HIDDevice_t *hd) {
	if (hd->Vendor) {
		return hd->Vendor;
	} else if (hd->VendorID == APC_VENDORID) {
		return "APC";
	} else if (hd->VendorID == CPS_VENDORID) {
		return "CPS";
	} else {
		return NULL;
	}
}

static char *apc_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int apc_claim(HIDDevice_t *hd) {
	if (hd->VendorID == APC_VENDORID) {
		switch (hd->ProductID) {
		case  0x0002:
			return 1;  /* accept known UPSs */
		default:
			if (getval("productid")) {
				return 1;
			} else {
			upsdebugx(1,
"This APC device (%04x/%04x) is not (or perhaps not yet) supported\n"
"by usbhid-ups. Please make sure you have an up-to-date version of NUT. If\n"
"this does not fix the problem, try running the driver with the\n"
"'-x productid=%04x' option. Please report your results to the NUT user's\n"
"mailing list <nut-upsuser@lists.alioth.debian.org>.\n",
						 hd->VendorID, hd->ProductID, hd->ProductID);
			return 0;
			}
		}
	} else if (hd->VendorID == CPS_VENDORID) {
		switch (hd->ProductID) {
		case 0x0005:  /* Cyber Power 900AVR/BC900D, CP1200AVR/BC1200D */
			           /* fixme: are the above really HID devices? */
			           /* Dynex DX-800U */
		case 0x0501:  /* Cyber Power AE550, Geek Squad GS1285U */
			return 1;  /* accept known UPSs */
		default:
			if (getval("productid")) {
				return 1;
			} else {
			upsdebugx(1,
"This CyberPower device (%04x/%04x) is not (or perhaps not yet) supported\n"
"by usbhid-ups. Please make sure you have an up-to-date version of NUT. If\n"
"this does not fix the problem, try running the driver with the\n"
"'-x productid=%04x' option. Please report your results to the NUT user's\n"
"mailing list <nut-upsuser@lists.alioth.debian.org>.\n",
						 hd->VendorID, hd->ProductID, hd->ProductID);
			return 0;
			}
		}
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
