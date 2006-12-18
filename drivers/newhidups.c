/* newhidups.c - Driver for USB and serial (MGE SHUT) HID UPS units
 * 
 * Copyright (C)
 *   2003-2005 Arnaud Quette <http://arnaud.quette.free.fr/contact.html>
 *   2005 John Stamp <kinsayder@hotmail.com>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "main.h"
#include "libhid.h"
#include "newhidups.h"
#ifdef SHUT_MODE
	#include "libshut.h"
#else
	#include "libusb.h"
#endif

/* include all known subdrivers */
#include "mge-hid.h"
#ifndef SHUT_MODE
	#include "explore-hid.h"
	#include "apc-hid.h"
	#include "belkin-hid.h"
	#include "tripplite-hid.h"
#endif

/* master list of avaiable subdrivers */
static subdriver_t *subdriver_list[] = {
#ifndef SHUT_MODE
	&explore_subdriver,
#endif
	&mge_subdriver,
#ifndef SHUT_MODE
	&apc_subdriver,
	&belkin_subdriver,
	&tripplite_subdriver,
	NULL
#endif
};

/* pointer to the active subdriver object (set in upsdrv_initups, then
   constant) */
static subdriver_t *subdriver;

/* Global vars */
static HIDDevice *hd;
static HIDDevice curDevice;
static HIDDeviceMatcher_t *reopen_matcher = NULL;
static HIDDeviceMatcher_t *regex_matcher = NULL;
static int pollfreq = DEFAULT_POLLFREQ;
static int ups_status = 0;
static bool data_has_changed = FALSE; /* for SEMI_STATIC data polling */
static time_t lastpoll; /* Timestamp the last polling */
hid_dev_handle *udev;

/* support functions */
static hid_info_t *find_nut_info(const char *varname);
static hid_info_t *find_nut_info_valid(const char *varname);
static hid_info_t *find_hid_info(const char *hidname);
static char *hu_find_infoval(info_lkp_t *hid2info, long value);
static long hu_find_valinfo(info_lkp_t *hid2info, const char* value);
static void process_status_info(char *nutvalue);
static void ups_status_set(void);
static void identify_ups ();
static bool hid_ups_walk(int mode);
static void reconnect_ups(void);

/* ---------------------------------------------------------------------- */
/* data for ups.status processing */

status_lkp_t status_info[] = {
	/* map internal status strings to bit masks */
	{ "online", STATUS_ONLINE },
	{ "dischrg", STATUS_DISCHRG },
	{ "chrg", STATUS_CHRG },
	{ "lowbatt", STATUS_LOWBATT },
	{ "overload", STATUS_OVERLOAD },
	{ "replacebatt", STATUS_REPLACEBATT },
	{ "shutdownimm", STATUS_SHUTDOWNIMM },
	{ "trim", STATUS_TRIM },
	{ "boost", STATUS_BOOST },
	{ "bypass", STATUS_BYPASS },
	{ "off", STATUS_OFF },
	{ "overheat", STATUS_OVERHEAT },
	{ "commfault", STATUS_COMMFAULT },
	{ "depleted", STATUS_DEPLETED },
	{ "timelimitexp", STATUS_TIMELIMITEXP },
	{ "batterypres", STATUS_BATTERYPRES },
	{ "fullycharged", STATUS_FULLYCHARGED },
	{ "awaitingpower", STATUS_AWAITINGPOWER },
	{ "vrange", STATUS_VRANGE },
	{ NULL, 0 },
};

/* ---------------------------------------------------------------------- */
/* value lookup tables and generic lookup functions */

/* Actual value lookup tables => should be fine for all Mfrs (TODO: validate it!) */

/* the purpose of the following status conversions is to collect
   information, not to interpret it. The function
   process_status_info() remembers these values by updating the global
   variable ups_status. Interpretation happens in ups_status_set,
   where they are converted to standard NUT status strings. Notice
   that the below conversions do not yield standard NUT status
   strings; this in indicated being in lower-case characters. 

   The reason to separate the collection of information from its
   interpretation is that not each report received from the UPS may
   contain all the status flags, so they must be stored
   somewhere. Also, there can be more than one status flag triggering
   a certain condition (e.g. a certain UPS might have variables
   low_battery, shutdown_imminent, timelimit_exceeded, and each of
   these would trigger the NUT status LB. But we have to ensure that
   these variables don't unset each other, so they are remembered
   separately)  */

info_lkp_t online_info[] = {
  { 1, "online", NULL },
  { 0, "!online", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t discharging_info[] = {
  { 1, "dischrg", NULL },
  { 0, "!dischrg", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t charging_info[] = {
  { 1, "chrg", NULL },
  { 0, "!chrg", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t lowbatt_info[] = {
  { 1, "lowbatt", NULL },
  { 0, "!lowbatt", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t overload_info[] = {
  { 1, "overload", NULL },
  { 0, "!overload", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t replacebatt_info[] = {
  { 1, "replacebatt", NULL },
  { 0, "!replacebatt", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t shutdownimm_info[] = {
  { 1, "shutdownimm", NULL },
  { 0, "!shutdownimm", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t trim_info[] = {
  { 1, "trim", NULL },
  { 0, "!trim", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t boost_info[] = {
  { 1, "boost", NULL },
  { 0, "!boost", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t overheat_info[] = {
  { 1, "overheat", NULL },
  { 0, "!overheat", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t awaitingpower_info[] = {
  { 1, "awaitingpower", NULL },
  { 0, "!awaitingpower", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t commfault_info[] = {
  { 1, "commfault", NULL },
  { 0, "!commfault", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t vrange_info[] = {
  { 1, "vrange", NULL },
  { 0, "!vrange", NULL },
  { 0, "NULL", NULL }
};
/* FIXME: extend ups.status for BYPASS Manual/Automatic */
info_lkp_t bypass_info[] = {
  { 1, "bypass", NULL },
  { 0, "!bypass", NULL },
  { 0, "NULL", NULL }
};
/* note: this value is reverted (0=set, 1=not set). We report "being
   off" rather than "being on", so that devices that don't implement
   this variable are "on" by default */
info_lkp_t off_info[] = {
  { 0, "off", NULL },
  { 1, "!off", NULL },
  { 0, "NULL", NULL }
};
/* FIXME: add CAL */

info_lkp_t test_write_info[] = {
  { 0, "No test", NULL },
  { 1, "Quick test", NULL },
  { 2, "Deep test", NULL },
  { 3, "Abort test", NULL },
  { 0, "NULL", NULL }
};
info_lkp_t test_read_info[] = {
  { 1, "Done and passed", NULL },
  { 2, "Done and warning", NULL },
  { 3, "Done and error", NULL },
  { 4, "Aborted", NULL },
  { 5, "In progress", NULL },
  { 6, "No test initiated", NULL },
  { 0, "NULL", NULL }
};

info_lkp_t beeper_info[] = {
  { 1, "disabled", NULL },
  { 2, "enabled", NULL },
  { 3, "muted", NULL },
  { 0, "NULL", NULL }
};

info_lkp_t yes_no_info[] = {
	{ 0, "no", NULL },
	{ 1, "yes", NULL },
	{ 0, "NULL", NULL }
};

info_lkp_t on_off_info[] = {
	{ 0, "off", NULL },
	{ 1, "on", NULL },
	{ 0, "NULL", NULL }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *date_conversion_fun(long value) {
  static char buf[20];
  int year, month, day;

  if (value == 0) {
    return "not set";
  }

  year = 1980 + (value >> 9); /* negative value represents pre-1980 date */ 
  month = (value >> 5) & 0x0f;
  day = value & 0x1f;
  
  sprintf(buf, "%04d/%02d/%02d", year, month, day);
  return buf;
}

info_lkp_t date_conversion[] = {
  { 0, NULL, date_conversion_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *hex_conversion_fun(long value) {
	static char buf[20];
	
	sprintf(buf, "%08lx", value);
	return buf;
}

info_lkp_t hex_conversion[] = {
	{ 0, NULL, hex_conversion_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *stringid_conversion_fun(long value) {
	static char buf[20];
	comm_driver->get_string(udev, value, buf);	

	return buf;
}

info_lkp_t stringid_conversion[] = {
	{ 0, NULL, stringid_conversion_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *divide_by_10_conversion_fun(long value) {
	static char buf[20];
	
	sprintf(buf, "%0.1f", value * 0.1);
	return buf;
}

info_lkp_t divide_by_10_conversion[] = {
	{ 0, NULL, divide_by_10_conversion_fun }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static char *kelvin_celsius_conversion_fun(long value) {
	static char buf[20];
	
	/* we should be working with floats, not integers, but integers it
	   is for now */
	sprintf(buf, "%d", (int)(value - 273.15));
	return buf;
}

info_lkp_t kelvin_celsius_conversion[] = {
	{ 0, NULL, kelvin_celsius_conversion_fun }
};

/*!
 * subdriver matcher: only useful for USB mode
 * as SHUT is only supported by MGE UPS SYSTEMS units
 */

#ifndef SHUT_MODE
static int match_function_subdriver(HIDDevice *d, void *privdata) {
	int i;

	for (i=0; subdriver_list[i] != NULL; i++) {
		if (subdriver_list[i]->claim(d)) {
			return 1;
		}
	}
	return 0;
}

static HIDDeviceMatcher_t subdriver_matcher_struct = {
	match_function_subdriver,
	NULL,
	NULL,
};
static HIDDeviceMatcher_t *subdriver_matcher = &subdriver_matcher_struct;
#endif

/* ---------------------------------------------
 * driver functions implementations
 * --------------------------------------------- */
void upsdrv_shutdown(void)
{
	int offdelay = DEFAULT_OFFDELAY;
	int ondelay = DEFAULT_ONDELAY;
	int r;

	/* Retrieve user defined delay settings */
	if ( getval(HU_VAR_ONDELAY) )
		ondelay = atoi( getval(HU_VAR_ONDELAY) );

	if ( getval(HU_VAR_OFFDELAY) )
		offdelay = atoi( getval(HU_VAR_OFFDELAY) );

	/* enforce ondelay > offdelay */
	if (ondelay <= offdelay) {
		ondelay = offdelay + 1;
		upsdebugx(2, "ondelay must be greater than offdelay; setting ondelay = %d (offdelay = %d)", ondelay, offdelay);
	}

	/* Apply specific method */
	r = subdriver->shutdown(ondelay, offdelay);

	if (r == 0) {
		fatalx("Shutdown failed.");
	}
	upsdebugx(2, "Shutdown command succeeded.");
}

/* process instant command and take action. */
int instcmd(const char *cmdname, const char *extradata)
{
	hid_info_t *hidups_item;
	
	upsdebugx(5, "entering instcmd(%s, %s)\n",
		  cmdname, (extradata==NULL)?"":extradata);

	/* Retrieve and check netvar & item_path */	
	hidups_item = find_nut_info_valid(cmdname);
	
	/* Check validity of the found the item */
	if (hidups_item == NULL || hidups_item->info_type == NULL ||
		!(hidups_item->hidflags & HU_FLAG_OK))
	{
		upsdebugx(2, "instcmd: info element unavailable %s\n", cmdname);
		/* TODO: manage items handled "manually" */
		return STAT_INSTCMD_UNKNOWN;
	}

	/* Check if the item is an instant command */
	if (!hidups_item->hidflags & HU_TYPE_CMD)
	{
		upsdebugx(2, "instcmd: %s is not an instant command\n", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}
	
	/* Actual variable setting */
	if (HIDSetItemValue(udev, hidups_item->hidpath,
	    atol(hidups_item->dfl), subdriver->utab))
	{
		upsdebugx(5, "instcmd: SUCCEED\n");
		/* Set the status so that SEMI_STATIC vars are polled */
		data_has_changed = TRUE;
		return STAT_INSTCMD_HANDLED;
	}
	else
		upsdebugx(3, "instcmd: FAILED\n"); /* TODO: HANDLED but FAILED, not UNKNOWN! */
	
	/* TODO: to be completed */
	return STAT_INSTCMD_UNKNOWN;
}

/* set r/w variable to a value. */
int setvar(const char *varname, const char *val)
{
	hid_info_t *hidups_item;
	long newvalue;

	upsdebugx(5, "entering setvar(%s, %s)\n", varname, val);
	
	/* 1) retrieve and check netvar & item_path */	
	hidups_item = find_nut_info(varname);
	
	if (hidups_item == NULL || hidups_item->info_type == NULL ||
		!(hidups_item->hidflags & HU_FLAG_OK))
	{
		upsdebugx(2, "setvar: info element unavailable %s\n", varname);
		return STAT_SET_UNKNOWN;
	}

	/* Checking item writability and HID Path */
	if (!hidups_item->info_flags & ST_FLAG_RW)
	{
		upsdebugx(2, "setvar: not writable %s\n", varname);
		return STAT_SET_UNKNOWN;
	}

	/* handle server side variable */
	if (hidups_item->hidflags & HU_FLAG_ABSENT)
	{
		upsdebugx(2, "setvar: setting server side variable %s\n", varname);
		dstate_setinfo(hidups_item->info_type, "%s", val);
		return STAT_SET_HANDLED;
	}
	else
	{
		/* SHUT_FLAG_ABSENT is the only case of HID Path == NULL */
		if (hidups_item->hidpath == NULL)
		{
			upsdebugx(2, "setvar: ID Path is NULL for %s\n", varname);
			return STAT_SET_UNKNOWN;
		}
	}

	/* Lookup the new value if needed */
	if (hidups_item->hid2info != NULL)
		newvalue = hu_find_valinfo(hidups_item->hid2info, val);
	else
		newvalue = atol(val);

	/* Actual variable setting */
	if (HIDSetItemValue(udev, hidups_item->hidpath, newvalue, subdriver->utab))
	{
		/* FIXME: GetValue(hidups_item->hidpath) to ensure success on non volatile */
		upsdebugx(5, "setvar: SUCCEED\n");
		/* Delay a bit not to flood the device */
		sleep(1);
		/* Set the status so that SEMI_STATIC vars are polled */
		data_has_changed = TRUE;
		return STAT_SET_HANDLED;
	}
	else
		upsdebugx(3, "setvar: FAILED\n"); /* FIXME: HANDLED but FAILED, not UNKNOWN! */
	
	return STAT_SET_UNKNOWN;
}

void upsdrv_help(void)
{
  /* FIXME: to be completed */
}

void upsdrv_makevartable(void) 
{
	char temp [MAX_STRING_SIZE];
	
	sprintf(temp, "Set shutdown delay, in seconds (default=%d).",
		DEFAULT_OFFDELAY);
	addvar (VAR_VALUE, HU_VAR_OFFDELAY, temp);
	
	sprintf(temp, "Set startup delay, in ten seconds units for MGE (default=%d).",
		DEFAULT_ONDELAY);
	addvar (VAR_VALUE, HU_VAR_ONDELAY, temp);
	
	sprintf(temp, "Set polling frequency, in seconds, to reduce data flow (default=%i).",
		DEFAULT_POLLFREQ);
	addvar(VAR_VALUE, HU_VAR_POLLFREQ, temp);
#ifndef SHUT_MODE
	/* allow -x vendor=X, vendorid=X, product=X, productid=X, serial=X */
	addvar(VAR_VALUE, "vendor", "Regular expression to match UPS Manufacturer string");
	addvar(VAR_VALUE, "product", "Regular expression to match UPS Product string");
	addvar(VAR_VALUE, "serial", "Regular expression to match UPS Serial number");
	addvar(VAR_VALUE, "vendorid", "Regular expression to match UPS Manufacturer numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "productid", "Regular expression to match UPS Product numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "bus", "Regular expression to match USB bus name");
	addvar(VAR_FLAG, "explore", "Diagnostic matching of unsupported UPS");
#endif
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools: %s %s - core %s (%s)\n\n",
	       comm_driver->name, comm_driver->version,
	       DRIVER_VERSION, UPS_VERSION);
}

void upsdrv_updateinfo(void) 
{
	hid_info_t *item;
	char *nutvalue;
	int retcode, evtCount = 0;
	HIDEvent *eventlist;
	HIDEvent *p;

	upsdebugx(1, "upsdrv_updateinfo...");


	/* check for device availability to set datastale! */
	if (hd == NULL)
	  {
		upsdebugx(1, "\n=>Got to reconnect!\n");
		reconnect_ups();
	  }

	/* Only do a full update (polling) every pollfreq 
	 * or upon data change (ie setvar/instcmd) */
	if ( (time(NULL) <= (lastpoll + pollfreq))
		 && (data_has_changed != TRUE) )
	  {
		/* Wait for HID notifications on Interrupt pipe */
		if ((evtCount = HIDGetEvents(udev, NULL, &eventlist, subdriver->utab)) > 0)
		  {
			upsdebugx(1, "\n=>Got %i HID Objects...", evtCount);
			
			/* Process pending events (HID notifications on Interrupt pipe) */
			for (p=eventlist; p!=NULL; p=p->next) 
			  {
				/* Check if we are asked to stop (reactivity++) */
				if (exit_flag != 0) {
				  HIDFreeEvents(eventlist);
				  return;
				}
				upsdebugx(3, "Object: %s = %ld", 
						  p->Path,
						  p->Value);
#ifndef SHUT_MODE
				/* special case: fix a horrible Belkin
				 bug.  My Belkin UPS actually sends an
				 incorrect report over the interrupt
				 pipeline - the corresponding feature
				 report is correct. */
				if (subdriver == &belkin_subdriver && strcmp(p->Path, "UPS.PowerSummary.BelowRemainingCapacityLimit") == 0) {
					continue;
				}
#endif
				
				if ((item = find_hid_info(p->Path)) != NULL)
				  {
					/* Does it need value lookup? */
					if (item->hid2info != NULL)
					  {
						nutvalue = hu_find_infoval(item->hid2info, (long)p->Value);
						if (nutvalue != NULL)
						  {
							upsdebugx(2, "%s = %s", item->info_type,nutvalue);
							
							/* Is it ups.status? */
							if (!strncmp(item->info_type, "ups.status", 10))
							  {
								/* bitwise status to process it globally */
								process_status_info(nutvalue);
								ups_status_set();
							  }
							else
							  dstate_setinfo(item->info_type, item->dfl, nutvalue);
						  }
					  }
					else
					  /* FIXME: should we do setinfo() here? */
					  upsdebugx(2, "%s = %ld", item->info_type, p->Value);
				  }
			  }
			dstate_dataok();
			HIDFreeEvents(eventlist);
		  }
		else {
		  retcode = evtCount; /* propagate error code */
		}
		
		/* Quick poll on main ups.status data */
		hid_ups_walk(HU_WALKMODE_QUICK_UPDATE);
	  }
	else /* Polling update */
	  {
		hid_ups_walk(HU_WALKMODE_FULL_UPDATE);
	  }

	/* Reset SEMI_STATIC flag */
	data_has_changed = FALSE;
}


/* Update ups_status to remember this status item. Interpretation is
   done in ups_status_set(). */
static void process_status_info(char *nutvalue)
{
	status_lkp_t *status_item;
	int clear = 0;

	upsdebugx(5, "process_status_info: %s", nutvalue);

	if (*nutvalue == '!') {
		nutvalue++;
		clear = 1;
	}

	for (status_item = status_info; status_item->status_str != NULL ; status_item++)
	{
		if (!strcasecmp(status_item->status_str, nutvalue))
		{
			if (clear) {
				ups_status &= ~status_item->status_mask;
			} else {
				ups_status |= status_item->status_mask;
			}
			break;
		}
	}
}

void upsdrv_initinfo(void)
{
	/* identify unit: fill ups.{mfr, model, serial} */
	identify_ups ();

	/* TODO: load lookup file (WARNING: should be in initups()
	because of -k segfault (=> not calling upsdrv_initinfo())
	*/

	/* Device capabilities enumeration */
	hid_ups_walk(HU_WALKMODE_INIT);

	/* install handlers */
	upsh.setvar = setvar;
	upsh.instcmd = instcmd;
}

void upsdrv_initups(void)
{
	int i;
#ifndef SHUT_MODE
	/*!
	 * SHUT is only supported by MGE UPS SYSTEMS units
	 * So we don't need the regex mechanism
	 */
	int r;
	char *regex_array[6];

	/* enforce use of the "vendorid" option if "explore" is given */
	if (testvar("explore") && getval("vendorid")==NULL) {
		fatalx("must specify \"vendorid\" when using \"explore\"");
	}

        /* process the UPS selection options */
	regex_array[0] = getval("vendorid");
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor");
	regex_array[3] = getval("product");
	regex_array[4] = getval("serial");
	regex_array[5] = getval("bus");

	r = new_regex_matcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
	if (r==-1) {
		fatalx("new_regex_matcher: %s", strerror(errno));
	} else if (r) {
		fatalx("invalid regular expression: %s", regex_array[r]);
	}
	/* link the matchers */
	regex_matcher->next = subdriver_matcher;

#else
	/*!
	 * But SHUT is a serial protocol, so it need
	 * the device path
	 */
	udev = (hid_dev_handle *)xmalloc(sizeof(hid_dev_handle));
	udev->device_path = strdup(device_path);

#endif /* SHUT_MODE */

	/* Search for the first supported UPS matching the regular
	   expression (not for SHUT_MODE) */
	if ((hd = HIDOpenDevice(&udev, &curDevice, regex_matcher, MODE_OPEN)) == NULL)
		fatalx("No matching HID UPS found");
	else
		upslogx(1, "Detected a UPS: %s/%s", hd->Vendor ? hd->Vendor : "unknown", hd->Product ? hd->Product : "unknown");

#ifndef SHUT_MODE
	/* create a new matcher for later reopening */
	reopen_matcher = new_exact_matcher(hd);
	if (!reopen_matcher) {
		upsdebugx(2, "new_exact_matcher: %s", strerror(errno));
	}
	/* link the two matchers */
	reopen_matcher->next = regex_matcher;

#endif /* SHUT_MODE */
	
	/* select the subdriver for this device */
	for (i=0; subdriver_list[i] != NULL; i++) {
		if (subdriver_list[i]->claim(hd)) {
			break;
		}
	}
	subdriver = subdriver_list[i];
	if (!subdriver) {
		upslogx(1, "Manufacturer not supported!");
		upslogx(1, "Contact the NUT Developers with the below information");
		HIDDumpTree(udev, subdriver->utab);
		fatalx("Aborting");
	}

	upslogx(2, "Using subdriver: %s", subdriver->name);

	HIDDumpTree(udev, subdriver->utab);

	/* init polling frequency */
	if ( getval(HU_VAR_POLLFREQ) )
		pollfreq = atoi ( getval(HU_VAR_POLLFREQ) );
}

void upsdrv_cleanup(void)
{
	if (hd != NULL) {
		HIDCloseDevice(udev);
		udev = NULL;
	}
}

/**********************************************************************
 * Support functions
 *********************************************************************/

static void identify_ups ()
{
	char *model;
	char *mfr;
	char *serial;

	upsdebugx (5, "entering identify_ups(0x%04x, 0x%04x)\n", 
			   hd->VendorID,
			   hd->ProductID);

	/* use vendor-specific method for calculating human-readable
	   manufacturer, model, and serial strings */

	model = subdriver->format_model(hd);
	mfr = subdriver->format_mfr(hd);
	serial = subdriver->format_serial(hd);

	/* set corresponding variables */
	if (mfr != NULL) {
		dstate_setinfo("ups.mfr", "%s", mfr);
	}
	if (model != NULL) {
		dstate_setinfo("ups.model", "%s", model);
	}
	if (serial != NULL) {
		dstate_setinfo("ups.serial", "%s", serial);
	}
}

/* walk ups variables and set elements of the info array. */
static bool hid_ups_walk(int mode)
{
	hid_info_t *item;
	float value;
	char *nutvalue;
	int retcode = 0;

	/* 3 modes: HU_WALKMODE_INIT, HU_WALKMODE_QUICK_UPDATE and HU_WALKMODE_FULL_UPDATE */
	
	/* Device data walk ----------------------------- */
	for ( item = subdriver->hid2nut ; item->info_type != NULL ; item++ )
	  {
		/* Check if we are asked to stop (reactivity++) */
		if (exit_flag != 0)
		  return TRUE;

		/* filter data according to mode */
		switch (mode)
		  {
			/* Device capabilities enumeration */
		  case HU_WALKMODE_INIT:
			{		
			  /* Avoid redundancy when multiple defines (RO/RW)
			   * Not applicable to "ups.status" items! */
			  if ((dstate_getinfo(item->info_type) != NULL)
				  && (strncmp(item->info_type, "ups.status", 10)))
				{
				  item->hidflags &= ~HU_FLAG_OK;
				  continue;
				}

			  /* Check instant commands availability */
			  if (item->hidflags & HU_TYPE_CMD)
				{
				  if (HIDGetItemValue(udev, item->hidpath, &value, subdriver->utab) == 1 )
					dstate_addcmd(item->info_type);

				  continue;
				}
			  /* Special case for handling server side variables */
			  if (item->hidflags & HU_FLAG_ABSENT)
				{
				  /* Check if exists (if necessary) before creation */
				  if (item->hidpath != NULL)
					{
					  if ((retcode = HIDGetItemValue(udev, item->hidpath, &value, subdriver->utab)) != 1 )
						continue;
					}
				  else
					{
					  /* Simply set the default value */
					  dstate_setinfo(item->info_type, "%s", item->dfl);
					  dstate_setflags(item->info_type, item->info_flags);
					  continue;
					}
			
				  dstate_setinfo(item->info_type, "%s", item->dfl);
				  dstate_setflags(item->info_type, item->info_flags);

				  /* Set max length for strings, if needed */
				  if (item->info_flags & ST_FLAG_STRING)
					dstate_setaux(item->info_type, item->info_len);
			
				  /* disable reading now 
					 item->shut_flags &= ~SHUT_FLAG_OK;*/
				}
			}
			break; 
		  case HU_WALKMODE_QUICK_UPDATE:
			{
			  /* Quick update only deals with status! */
			  if ( !(item->hidflags & HU_FLAG_QUICK_POLL))
				continue;
			}
			break; 
		  default:
		  case HU_WALKMODE_FULL_UPDATE:
			{
			  /* These doesn't need polling after initinfo() */
			  if ((item->hidflags & HU_FLAG_ABSENT)
				  || (item->hidflags & HU_TYPE_CMD)
				  || (item->hidflags & HU_FLAG_STATIC)
				  /* These need to be polled after user changes (setvar / instcmd) */
				  || ( (item->hidflags & HU_FLAG_SEMI_STATIC) && (data_has_changed == FALSE) ) ) 
				/* FIXME: external condition might change these, ie pushing the HW On/Off button! */
				continue;
			}
			break; 
		  }

		/* Standard variables */
		/* skip elements we shouldn't process / show. */
		if ( ( (mode == HU_WALKMODE_QUICK_UPDATE) || (mode == HU_WALKMODE_FULL_UPDATE) )
			 && !(item->hidflags & HU_FLAG_OK) )
		  continue;

		if ((retcode = HIDGetItemValue(udev, item->hidpath, &value, subdriver->utab)) == 1 )
		  {
			/* deal with status items */
			if (!strncmp(item->info_type, "ups.status", 10))
			  {
				nutvalue = hu_find_infoval(item->hid2info, (long)value);
				if (nutvalue != NULL)
				  {
					/* bitwise status to process it globally */
					process_status_info(nutvalue);
					ups_status_set();
				  }
			  }
			else /* standard items */
			  {
				/* need lookup'ed translation? */
				if (item->hid2info != NULL)
				  {
					nutvalue = hu_find_infoval(item->hid2info, (long)value);
					if (nutvalue != NULL)
					  dstate_setinfo(item->info_type, item->dfl, nutvalue);
				  }
				else
				  dstate_setinfo(item->info_type, item->dfl, value);
				
				if (mode == HU_WALKMODE_INIT)
				  {
					dstate_setflags(item->info_type, item->info_flags);
				  }
			  }
			if (mode == HU_WALKMODE_INIT)
			  {
				/* Set max length for strings */
				if (item->info_flags & ST_FLAG_STRING)
				  dstate_setaux(item->info_type, item->info_len);
			  }

			/* atomic call */
/* 			dstate_dataok(); */

			/* store timestamp */
/* 			lastpoll = time(NULL); */
		}
		else
		{
			if ( (retcode == -EPERM) || (retcode == -EPIPE)
				|| (retcode == -ENODEV) || (retcode == -EACCES)
				|| (retcode == -EIO) || (retcode == -ENOENT) )
				break;
			else
			{
				/* atomic call */
				dstate_dataok();
			}

			if (mode == HU_WALKMODE_INIT)
			{
				/* invalidate item */
				item->hidflags &= ~HU_FLAG_OK;
			}
		}
	} /* end for */

	if (mode == HU_WALKMODE_FULL_UPDATE)
	{
		/* store timestamp */
		lastpoll = time(NULL);
	}

	/* Reserved values: -1/-10 for nul delay, -2 can't get value */
	/* device has been disconnected, try to reconnect */
	if ( (retcode == -EPERM) || (retcode == -EPIPE)
		|| (retcode == -ENODEV) || (retcode == -EACCES)
		|| (retcode == -EIO) || (retcode == -ENOENT) )
	{
		hd = NULL;
		reconnect_ups();
	}
	else {
	  /* atomic call */
	  dstate_dataok();
	}

  return TRUE;
}

static void reconnect_ups(void)
{
	if (hd == NULL)
	{
		upsdebugx(2, "==================================================");
		upsdebugx(2, "= device has been disconnected, try to reconnect =");
		upsdebugx(2, "==================================================");
		
#if defined(SHUT_MODE) || defined(SUN_LIBUSB)
		/* Cause a double free corruption in USB mode on linux! */
		HIDCloseDevice(udev);
#else
		/* but keep udev in SHUT mode, for udev->device_path */
		udev = NULL;
#endif

	  if ((hd = HIDOpenDevice(&udev, &curDevice, reopen_matcher, MODE_REOPEN)) == NULL)
		dstate_datastale();
	}
}

/* Convert the local status information to NUT format and set NUT
   status. */
static void ups_status_set(void)
{
	/* clear status buffer before begining */
	status_init();

	if (ups_status & STATUS_ONLINE) {
		status_set("OL");		/* on line */
	} else {
		status_set("OB");               /* on battery */
	}
	if (ups_status & STATUS_DISCHRG) {
		status_set("DISCHRG");	        /* discharging */         
	}
	if (ups_status & STATUS_CHRG) {
		status_set("CHRG");		/* charging */
	}
	if (ups_status & (STATUS_LOWBATT
			  | STATUS_SHUTDOWNIMM
			  | STATUS_TIMELIMITEXP)) {
		status_set("LB");		/* low battery */
	}
	if (ups_status & STATUS_OVERLOAD) {
		status_set("OVER");		/* overload */
	}
	if (ups_status & STATUS_REPLACEBATT) {
		status_set("RB");		/* replace batt */
	}
	if (ups_status & STATUS_TRIM) {
		status_set("TRIM");		/* SmartTrim */
	}
	if (ups_status & STATUS_BOOST) {
		status_set("BOOST");	        /* SmartBoost */
	}
	if (ups_status & STATUS_BYPASS) {
		status_set("BYPASS");	        /* on bypass */   
	}
	if (ups_status & STATUS_OFF) {
		status_set("OFF");              /* ups is off */
	}
	if (ups_status & STATUS_CAL) {
		status_set("CAL");		/* calibration */
	}
	if (ups_status & STATUS_OVERHEAT) {
		status_set("OVERHEAT");		/* overheat; Belkin, TrippLite */
	}
	if (ups_status & STATUS_COMMFAULT) {
		status_set("COMMFAULT");	/* UPS fault; Belkin, TrippLite */
	}
	if (ups_status & STATUS_DEPLETED) {
		status_set("DEPLETED");		/* battery depleted; Belkin */
	}
	if (ups_status & STATUS_AWAITINGPOWER) {
		status_set("AWAITINGPOWER");	/* awaiting power; Belkin, TrippLite */
	}
	if (ups_status & STATUS_VRANGE) {
		status_set("VRANGE");		/* voltage out of range; TrippLite */
	}
	
	/* Commit the status buffer */
	status_commit();
}

/* find info element definition in info array
 * by NUT varname.
 */
static hid_info_t *find_nut_info(const char *varname)
{
  hid_info_t *hidups_item;

  for (hidups_item = subdriver->hid2nut; hidups_item->info_type != NULL ; hidups_item++) {
    if (!strcasecmp(hidups_item->info_type, varname))
      return hidups_item;
  }

  upsdebugx(2, "find_nut_info: unknown info type: %s\n", varname);
  return NULL;
}

/* find info element definition in info array by NUT varname. Only
 * return items whose HID path actually exists.  By this, we enable
 * multiple alternative definitions of an instant command; the first
 * one that works for *this* UPS will be used. 
 */
static hid_info_t *find_nut_info_valid(const char *varname)
{
  hid_info_t *hidups_item;
  float value;

  for (hidups_item = subdriver->hid2nut; hidups_item->info_type != NULL ; hidups_item++) {
    if (!strcasecmp(hidups_item->info_type, varname))
      if (HIDGetItemValue(udev, hidups_item->hidpath, &value, subdriver->utab) == 1)
	return hidups_item;
  }

  upsdebugx(2, "find_nut_info: unknown info type: %s\n", varname);
  return NULL;
}

/* find info element definition in info array
 * by HID varname.
 */
static hid_info_t *find_hid_info(const char *hidname)
{
	hid_info_t *hidups_item;
	
	for (hidups_item = subdriver->hid2nut; 
		hidups_item->info_type != NULL ; hidups_item++) {

		/* Skip NULL HID path (server side vars) */
		if (hidups_item->hidpath == NULL)
			continue;
	
		if (!strcasecmp(hidups_item->hidpath, hidname))
			return hidups_item;
	}

	upsdebugx(2, "find_hid_info: unknown variable: %s\n", hidname);
	return NULL;
}

/* find the HID Item value matching that NUT value */
/* useful for set with value lookup... */
static long hu_find_valinfo(info_lkp_t *hid2info, const char* value)
{
	info_lkp_t *info_lkp;
	
	for (info_lkp = hid2info; (info_lkp != NULL) &&
		(strcmp(info_lkp->nut_value, "NULL")); info_lkp++) {

		if (!(strcmp(info_lkp->nut_value, value))) {
			upsdebugx(5, "hu_find_valinfo: found %s (value: %ld)\n",
				  info_lkp->nut_value, info_lkp->hid_value);
	
			return info_lkp->hid_value;
		}
	}
	upsdebugx(3, "hu_find_valinfo: no matching HID value for this INFO_* value (%s)", value);
	return -1;
}

/* find the NUT value matching that HID Item value */
static char *hu_find_infoval(info_lkp_t *hid2info, long value)
{
	info_lkp_t *info_lkp;
	char *nut_value;

	upsdebugx(5, "hu_find_infoval: searching for value = %ld\n", value);

	if (hid2info->fun != NULL) {
		nut_value = hid2info->fun(value);
		upsdebugx(5, "hu_find_infoval: found %s (value: %ld)\n",
			nut_value, value);
		return nut_value;
	}

	for (info_lkp = hid2info; (info_lkp != NULL) &&
		(strcmp(info_lkp->nut_value, "NULL")); info_lkp++) {
		if (info_lkp->hid_value == value) {
			upsdebugx(5, "hu_find_infoval: found %s (value: %ld)\n",
					info_lkp->nut_value, value);
	
			return info_lkp->nut_value;
		}
	}
	upsdebugx(3, "hu_find_infoval: no matching INFO_* value for this HID value (%ld)\n", value);
	return NULL;
}

