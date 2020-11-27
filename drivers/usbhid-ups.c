/* usbhid-ups.c - Driver for USB and serial (MGE SHUT) HID UPS units
 *
 * Copyright (C)
 *   2003-2012 Arnaud Quette <arnaud.quette@gmail.com>
 *   2005      John Stamp <kinsayder@hotmail.com>
 *   2005-2006 Peter Selinger <selinger@users.sourceforge.net>
 *   2007-2009 Arjen de Korte <adkorte-guest@alioth.debian.org>
 *   2016      Eaton / Arnaud Quette <ArnaudQuette@Eaton.com>
 *
 * This program was sponsored by MGE UPS SYSTEMS, and now Eaton
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
 *
 * TODO list:
 * - set ST_FLAG_RW according to HIDData_t->Attribute (ATTR_DATA_CST-ATTR_NVOL_VOL)
 */

#define DRIVER_NAME	"Generic HID driver"
#define DRIVER_VERSION		"0.43"

#include "main.h"
#include "libhid.h"
#include "usbhid-ups.h"
#include "hidparser.h"
#include "hidtypes.h"

/* include all known subdrivers */
#include "mge-hid.h"

#ifndef SHUT_MODE
	#include "explore-hid.h"
	#include "apc-hid.h"
	#include "belkin-hid.h"
	#include "cps-hid.h"
	#include "liebert-hid.h"
	#include "powercom-hid.h"
	#include "tripplite-hid.h"
	#include "idowell-hid.h"
	#include "openups-hid.h"
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
	&cps_subdriver,
	&liebert_subdriver,
	&powercom_subdriver,
	&tripplite_subdriver,
	&idowell_subdriver,
	&openups_subdriver,
#endif
	NULL
};

upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <arnaud.quette@gmail.com>\n" \
	"Peter Selinger <selinger@users.sourceforge.net>\n" \
	"Arjen de Korte <adkorte-guest@alioth.debian.org>\n" \
	"John Stamp <kinsayder@hotmail.com>",
	/*FIXME: link the subdrivers? do the same as for the mibs! */
#ifndef SHUT_MODE
	DRV_STABLE,
#else
	DRV_EXPERIMENTAL,
#endif
	{ &comm_upsdrv_info, NULL }
};

/* Data walk modes */
typedef enum {
	HU_WALKMODE_INIT = 0,
	HU_WALKMODE_QUICK_UPDATE,
	HU_WALKMODE_FULL_UPDATE
} walkmode_t;

/* pointer to the active subdriver object (changed in callback() function) */
static subdriver_t *subdriver = NULL;

/* Global vars */
static HIDDevice_t *hd = NULL;
static HIDDevice_t curDevice = { 0x0000, 0x0000, NULL, NULL, NULL, NULL, 0 };
static HIDDeviceMatcher_t *subdriver_matcher = NULL;
#ifndef SHUT_MODE
static HIDDeviceMatcher_t *exact_matcher = NULL;
static HIDDeviceMatcher_t *regex_matcher = NULL;
#endif
static int pollfreq = DEFAULT_POLLFREQ;
static int ups_status = 0;
static bool_t data_has_changed = FALSE; /* for SEMI_STATIC data polling */
#ifndef SUN_LIBUSB
bool_t use_interrupt_pipe = TRUE;
#else
bool_t use_interrupt_pipe = FALSE;
#endif
static time_t lastpoll; /* Timestamp the last polling */
hid_dev_handle_t udev;

/* support functions */
static hid_info_t *find_nut_info(const char *varname);
static hid_info_t *find_hid_info(const HIDData_t *hiddata);
static const char *hu_find_infoval(info_lkp_t *hid2info, const double value);
static long hu_find_valinfo(info_lkp_t *hid2info, const char* value);
static void process_boolean_info(const char *nutvalue);
static void ups_alarm_set(void);
static void ups_status_set(void);
static bool_t hid_ups_walk(walkmode_t mode);
static int reconnect_ups(void);
static int ups_infoval_set(hid_info_t *item, double value);
static int callback(hid_dev_handle_t argudev, HIDDevice_t *arghd, unsigned char *rdbuf, int rdlen);
#ifdef DEBUG
static double interval(void);
#endif

/* global variables */
HIDDesc_t	*pDesc = NULL;		/* parsed Report Descriptor */
reportbuf_t	*reportbuf = NULL;	/* buffer for most recent reports */


/* --------------------------------------------------------------- */
/* Struct & data for boolean processing                            */
/* --------------------------------------------------------------- */

/* Note: this structure holds internal status info, directly as
   collected from the hardware; not yet converted to official NUT
   status or alarms */
typedef struct {
	const char	*status_str;	/* ups status string */
	const int	status_mask;	/* ups status mask */
} status_lkp_t;

static status_lkp_t status_info[] = {
	/* map internal status strings to bit masks */
	{ "online", STATUS(ONLINE) },
	{ "dischrg", STATUS(DISCHRG) },
	{ "chrg", STATUS(CHRG) },
	{ "lowbatt", STATUS(LOWBATT) },
	{ "overload", STATUS(OVERLOAD) },
	{ "replacebatt", STATUS(REPLACEBATT) },
	{ "shutdownimm", STATUS(SHUTDOWNIMM) },
	{ "trim", STATUS(TRIM) },
	{ "boost", STATUS(BOOST) },
	{ "bypassauto", STATUS(BYPASSAUTO) },
	{ "bypassman", STATUS(BYPASSMAN) },
	{ "off", STATUS(OFF) },
	{ "cal", STATUS(CAL) },
	{ "overheat", STATUS(OVERHEAT) },
	{ "commfault", STATUS(COMMFAULT) },
	{ "depleted", STATUS(DEPLETED) },
	{ "timelimitexp", STATUS(TIMELIMITEXP) },
	{ "fullycharged", STATUS(FULLYCHARGED) },
	{ "awaitingpower", STATUS(AWAITINGPOWER) },
	{ "fanfail", STATUS(FANFAIL) },
	{ "nobattery", STATUS(NOBATTERY) },
	{ "battvoltlo", STATUS(BATTVOLTLO) },
	{ "battvolthi", STATUS(BATTVOLTHI) },
	{ "chargerfail", STATUS(CHARGERFAIL) },
	{ "vrange", STATUS(VRANGE) },
	{ "frange", STATUS(FRANGE) },
	{ NULL, 0 },
};

/* ---------------------------------------------------------------------- */
/* value lookup tables and generic lookup functions */

/* Actual value lookup tables => should be fine for all Mfrs (TODO: validate it!) */

/* the purpose of the following status conversions is to collect
   information, not to interpret it. The function
   process_boolean_info() remembers these values by updating the global
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
	{ 1, "online", NULL, NULL },
	{ 0, "!online", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t discharging_info[] = {
	{ 1, "dischrg", NULL, NULL },
	{ 0, "!dischrg", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t charging_info[] = {
	{ 1, "chrg", NULL, NULL },
	{ 0, "!chrg", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t lowbatt_info[] = {
	{ 1, "lowbatt", NULL, NULL },
	{ 0, "!lowbatt", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t overload_info[] = {
	{ 1, "overload", NULL, NULL },
	{ 0, "!overload", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t replacebatt_info[] = {
	{ 1, "replacebatt", NULL, NULL },
	{ 0, "!replacebatt", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t trim_info[] = {
	{ 1, "trim", NULL, NULL },
	{ 0, "!trim", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t boost_info[] = {
	{ 1, "boost", NULL, NULL },
	{ 0, "!boost", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t bypass_auto_info[] = {
	{ 1, "bypassauto", NULL, NULL },
	{ 0, "!bypassauto", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t bypass_manual_info[] = {
	{ 1, "bypassman", NULL, NULL },
	{ 0, "!bypassman", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
/* note: this value is reverted (0=set, 1=not set). We report "being
   off" rather than "being on", so that devices that don't implement
   this variable are "on" by default */
info_lkp_t off_info[] = {
	{ 0, "off", NULL, NULL },
	{ 1, "!off", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t calibration_info[] = {
	{ 1, "cal", NULL, NULL },
	{ 0, "!cal", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
/* note: this value is reverted (0=set, 1=not set). We report "battery
   not installed" rather than "battery installed", so that devices
   that don't implement this variable have a battery by default */
info_lkp_t nobattery_info[] = {
	{ 1, "!nobattery", NULL, NULL },
	{ 0, "nobattery", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t fanfail_info[] = {
	{ 1, "fanfail", NULL, NULL },
	{ 0, "!fanfail", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t shutdownimm_info[] = {
	{ 1, "shutdownimm", NULL, NULL },
	{ 0, "!shutdownimm", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t overheat_info[] = {
	{ 1, "overheat", NULL, NULL },
	{ 0, "!overheat", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t awaitingpower_info[] = {
	{ 1, "awaitingpower", NULL, NULL },
	{ 0, "!awaitingpower", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t commfault_info[] = {
	{ 1, "commfault", NULL, NULL },
	{ 0, "!commfault", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t timelimitexpired_info[] = {
	{ 1, "timelimitexp", NULL, NULL },
	{ 0, "!timelimitexp", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t battvoltlo_info[] = {
	{ 1, "battvoltlo", NULL, NULL },
	{ 0, "!battvoltlo", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t battvolthi_info[] = {
	{ 1, "battvolthi", NULL, NULL },
	{ 0, "!battvolthi", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t chargerfail_info[] = {
	{ 1, "chargerfail", NULL, NULL },
	{ 0, "!chargerfail", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t fullycharged_info[] = { /* used by CyberPower and TrippLite */
	{ 1, "fullycharged", NULL, NULL },
	{ 0, "!fullycharged", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t depleted_info[] = {
	{ 1, "depleted", NULL, NULL },
	{ 0, "!depleted", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t vrange_info[] = {
	{ 0, "!vrange", NULL, NULL },
	{ 1, "vrange", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};
info_lkp_t frange_info[] = {
	{ 0, "!frange", NULL, NULL },
	{ 1, "frange", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

info_lkp_t test_write_info[] = {
	{ 0, "No test", NULL, NULL },
	{ 1, "Quick test", NULL, NULL },
	{ 2, "Deep test", NULL, NULL },
	{ 3, "Abort test", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

info_lkp_t test_read_info[] = {
	{ 1, "Done and passed", NULL, NULL },
	{ 2, "Done and warning", NULL, NULL },
	{ 3, "Done and error", NULL, NULL },
	{ 4, "Aborted", NULL, NULL },
	{ 5, "In progress", NULL, NULL },
	{ 6, "No test initiated", NULL, NULL },
	{ 7, "Test scheduled", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

info_lkp_t beeper_info[] = {
	{ 1, "disabled", NULL, NULL },
	{ 2, "enabled", NULL, NULL },
	{ 3, "muted", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

info_lkp_t yes_no_info[] = {
	{ 0, "no", NULL, NULL },
	{ 1, "yes", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

info_lkp_t on_off_info[] = {
	{ 0, "off", NULL, NULL },
	{ 1, "on", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static const char *date_conversion_fun(double value)
{
	static char buf[20];
	int year, month, day;

	if ((long)value == 0) {
		return "not set";
	}

	year = 1980 + ((long)value >> 9); /* negative value represents pre-1980 date */
	month = ((long)value >> 5) & 0x0f;
	day = (long)value & 0x1f;

	snprintf(buf, sizeof(buf), "%04d/%02d/%02d", year, month, day);

	return buf;
}

/* FIXME? Do we need an inverse "nuf()" here? */
info_lkp_t date_conversion[] = {
	{ 0, NULL, date_conversion_fun, NULL }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static const char *hex_conversion_fun(double value)
{
	static char buf[20];

	snprintf(buf, sizeof(buf), "%08lx", (long)value);

	return buf;
}

/* FIXME? Do we need an inverse "nuf()" here? */
info_lkp_t hex_conversion[] = {
	{ 0, NULL, hex_conversion_fun, NULL }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static const char *stringid_conversion_fun(double value)
{
	static char buf[20];

	return HIDGetIndexString(udev, (int)value, buf, sizeof(buf));
}

/* FIXME? Do we need an inverse "nuf()" here? */
info_lkp_t stringid_conversion[] = {
	{ 0, NULL, stringid_conversion_fun, NULL }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static const char *divide_by_10_conversion_fun(double value)
{
	static char buf[20];

	snprintf(buf, sizeof(buf), "%0.1f", value * 0.1);

	return buf;
}

/* FIXME? Do we need an inverse "nuf()" here? */
info_lkp_t divide_by_10_conversion[] = {
	{ 0, NULL, divide_by_10_conversion_fun, NULL }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static const char *kelvin_celsius_conversion_fun(double value)
{
	static char buf[20];

	/* check if the value is in the Kelvin range, to
	 * detect buggy value (already expressed in °C), as found
	 * on some HP implementation */
	if ((value >= 273) && (value <= 373)) {
		/* the value is indeed in °K */
		snprintf(buf, sizeof(buf), "%.1f", value - 273.15);
	}
	else {
		/* else, this is actually °C, not °K! */
		snprintf(buf, sizeof(buf), "%.1f", value);
	}

	return buf;
}

/* FIXME? Do we need an inverse "nuf()" here? */
info_lkp_t kelvin_celsius_conversion[] = {
	{ 0, NULL, kelvin_celsius_conversion_fun, NULL }
};

/*!
 * subdriver matcher: only useful for USB mode
 * as SHUT is only supported by MGE UPS SYSTEMS units
 */

#ifndef SHUT_MODE
static int match_function_subdriver(HIDDevice_t *d, void *privdata) {
	int i;
	NUT_UNUSED_VARIABLE(privdata);

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
	NULL
};
#endif

/* ---------------------------------------------
 * driver functions implementations
 * --------------------------------------------- */
/* process instant command and take action. */
int instcmd(const char *cmdname, const char *extradata)
{
	hid_info_t	*hidups_item;
	const char	*val;
	double		value;

	if (!strcasecmp(cmdname, "beeper.off")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.off' command has been renamed to 'beeper.disable'");
		return instcmd("beeper.disable", NULL);
	}

	if (!strcasecmp(cmdname, "beeper.on")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.on' command has been renamed to 'beeper.enable'");
		return instcmd("beeper.enable", NULL);
	}

	upsdebugx(1, "instcmd(%s, %s)", cmdname, extradata ? extradata : "[NULL]");

	/* Retrieve and check netvar & item_path */
	hidups_item = find_nut_info(cmdname);

	/* Check for fallback if not found */
	if (hidups_item == NULL) {
		upsdebugx(3, "%s: cmdname '%s' not found; checking for alternatives", __func__, cmdname);

		if (!strcasecmp(cmdname, "load.on")) {
			return instcmd("load.on.delay", "0");
		}

		if (!strcasecmp(cmdname, "load.off")) {
			return instcmd("load.off.delay", "0");
		}

		if (!strcasecmp(cmdname, "shutdown.return")) {
			int	ret;

			/* Ensure "ups.start.auto" is set to "yes", if supported */
			if (dstate_getinfo("ups.start.auto")) {
				setvar("ups.start.auto", "yes");
			}

			ret = instcmd("load.on.delay", dstate_getinfo("ups.delay.start"));
			if (ret != STAT_INSTCMD_HANDLED) {
				return ret;
			}

			return instcmd("load.off.delay", dstate_getinfo("ups.delay.shutdown"));
		}

		if (!strcasecmp(cmdname, "shutdown.stayoff")) {
			int	ret;

			/* Ensure "ups.start.auto" is set to "no", if supported */
			if (dstate_getinfo("ups.start.auto")) {
				setvar("ups.start.auto", "no");
			}

			ret = instcmd("load.on.delay", "-1");
			if (ret != STAT_INSTCMD_HANDLED) {
				return ret;
			}

			return instcmd("load.off.delay", dstate_getinfo("ups.delay.shutdown"));
		}

		upsdebugx(2, "instcmd: info element unavailable %s\n", cmdname);
		return STAT_INSTCMD_INVALID;
	}

	upsdebugx(3, "%s: using Path '%s'", __func__, hidups_item->hidpath);

	/* Check if the item is an instant command */
	if (!(hidups_item->hidflags & HU_TYPE_CMD)) {
		upsdebugx(2, "instcmd: %s is not an instant command\n", cmdname);
		return STAT_INSTCMD_INVALID;
	}

	/* If extradata is empty, use the default value from the HID-to-NUT table */
	val = extradata ? extradata : hidups_item->dfl;

	/* Lookup the new value if needed */
	if (hidups_item->hid2info != NULL) {
		value = hu_find_valinfo(hidups_item->hid2info, val);
	} else {
		value = atol(val);
	}

	/* Actual variable setting */
	if (HIDSetDataValue(udev, hidups_item->hiddata, value) == 1) {
		upsdebugx(3, "instcmd: SUCCEED\n");
		/* Set the status so that SEMI_STATIC vars are polled */
		data_has_changed = TRUE;
		return STAT_INSTCMD_HANDLED;
	}

	upsdebugx(3, "instcmd: FAILED\n"); /* TODO: HANDLED but FAILED, not UNKNOWN! */
	return STAT_INSTCMD_FAILED;
}

/* set r/w variable to a value. */
int setvar(const char *varname, const char *val)
{
	hid_info_t	*hidups_item;
	double		value;

	upsdebugx(1, "setvar(%s, %s)", varname, val);

	/* retrieve and check netvar & item_path */
	hidups_item = find_nut_info(varname);

	if (hidups_item == NULL) {
		upsdebugx(2, "setvar: info element unavailable %s\n", varname);
		return STAT_SET_UNKNOWN;
	}

	/* Checking item writability and HID Path */
	if (!(hidups_item->info_flags & ST_FLAG_RW)) {
		upsdebugx(2, "setvar: not writable %s\n", varname);
		return STAT_SET_UNKNOWN;
	}

	/* handle server side variable */
	if (hidups_item->hidflags & HU_FLAG_ABSENT) {
		upsdebugx(2, "setvar: setting server side variable %s\n", varname);
		dstate_setinfo(hidups_item->info_type, "%s", val);
		return STAT_SET_HANDLED;
	}

	/* HU_FLAG_ABSENT is the only case of HID Path == NULL */
	if (hidups_item->hidpath == NULL) {
		upsdebugx(2, "setvar: ID Path is NULL for %s\n", varname);
		return STAT_SET_UNKNOWN;
	}

	/* Lookup the new value if needed */
	if (hidups_item->hid2info != NULL) {
		value = hu_find_valinfo(hidups_item->hid2info, val);
	} else {
		value = atol(val);
	}

	/* Actual variable setting */
	if (HIDSetDataValue(udev, hidups_item->hiddata, value) == 1) {
		upsdebugx(5, "setvar: SUCCEED\n");
		/* Set the status so that SEMI_STATIC vars are polled */
		data_has_changed = TRUE;
		return STAT_SET_HANDLED;
	}

	upsdebugx(3, "setvar: FAILED\n"); /* FIXME: HANDLED but FAILED, not UNKNOWN! */
	return STAT_SET_UNKNOWN;
}

void upsdrv_shutdown(void)
{
	upsdebugx(1, "upsdrv_shutdown...");

	/* Try to shutdown with delay */
	if (instcmd("shutdown.return", NULL) == STAT_INSTCMD_HANDLED) {
		/* Shutdown successful */
		return;
	}

	/* If the above doesn't work, try shutdown.reboot */
	if (instcmd("shutdown.reboot", NULL) == STAT_INSTCMD_HANDLED) {
		/* Shutdown successful */
		return;
	}

	/* If the above doesn't work, try load.off.delay */
	if (instcmd("load.off.delay", NULL) == STAT_INSTCMD_HANDLED) {
		/* Shutdown successful */
		return;
	}

	fatalx(EXIT_FAILURE, "Shutdown failed!");
}

void upsdrv_help(void)
{
	/* FIXME: to be completed */
}

void upsdrv_makevartable(void)
{
	char temp [MAX_STRING_SIZE];

	upsdebugx(1, "upsdrv_makevartable...");

	snprintf(temp, sizeof(temp), "Set low battery level, in %% (default=%s).", DEFAULT_LOWBATT);
	addvar (VAR_VALUE, HU_VAR_LOWBATT, temp);

	snprintf(temp, sizeof(temp), "Set shutdown delay, in seconds (default=%s)", DEFAULT_OFFDELAY);
	addvar(VAR_VALUE, HU_VAR_OFFDELAY, temp);

	snprintf(temp, sizeof(temp), "Set startup delay, in seconds (default=%s)", DEFAULT_ONDELAY);
	addvar(VAR_VALUE, HU_VAR_ONDELAY, temp);

	snprintf(temp, sizeof(temp), "Set polling frequency, in seconds, to reduce data flow (default=%d)",
		DEFAULT_POLLFREQ);
	addvar(VAR_VALUE, HU_VAR_POLLFREQ, temp);

	addvar(VAR_FLAG, "pollonly", "Don't use interrupt pipe, only use polling");

#ifndef SHUT_MODE
	/* allow -x vendor=X, vendorid=X, product=X, productid=X, serial=X */
	nut_usb_addvars();

	addvar(VAR_FLAG, "explore", "Diagnostic matching of unsupported UPS");
	addvar(VAR_FLAG, "maxreport", "Activate tweak for buggy APC Back-UPS firmware");
	addvar(VAR_FLAG, "interruptonly", "Don't use polling, only use interrupt pipe");
	addvar(VAR_VALUE, "interruptsize", "Number of bytes to read from interrupt pipe");
#else
	addvar(VAR_VALUE, "notification", "Set notification type, (ignored, only for backward compatibility)");
#endif
}

#define	MAX_EVENT_NUM	32

void upsdrv_updateinfo(void)
{
	hid_info_t	*item;
	HIDData_t	*event[MAX_EVENT_NUM], *found_data;
	int		i, evtCount;
	double		value;
	time_t		now;

	upsdebugx(1, "upsdrv_updateinfo...");

	time(&now);

	/* check for device availability to set datastale! */
	if (hd == NULL) {
		/* don't flood reconnection attempts */
		if (now < (int)(lastpoll + poll_interval)) {
			return;
		}

		upsdebugx(1, "Got to reconnect!\n");

		if (!reconnect_ups()) {
			lastpoll = now;
			dstate_datastale();
			return;
		}

		hd = &curDevice;

		if (hid_ups_walk(HU_WALKMODE_INIT) == FALSE) {
			hd = NULL;
			return;
		}
	}
#ifdef DEBUG
	interval();
#endif
	/* Get HID notifications on Interrupt pipe first */
	if (use_interrupt_pipe == TRUE) {
		evtCount = HIDGetEvents(udev, event, MAX_EVENT_NUM);
		switch (evtCount)
		{
		case -EBUSY:		/* Device or resource busy */
			upslog_with_errno(LOG_CRIT, "Got disconnected by another driver");
			goto fallthrough_reconnect;
		case -EPERM:		/* Operation not permitted */
		case -ENODEV:		/* No such device */
		case -EACCES:		/* Permission denied */
		case -EIO:		/* I/O error */
		case -ENXIO:		/* No such device or address */
		case -ENOENT:		/* No such file or directory */
		fallthrough_reconnect:
			/* Uh oh, got to reconnect! */
			hd = NULL;
			return;
		default:
			upsdebugx(1, "Got %i HID objects...", (evtCount >= 0) ? evtCount : 0);
			break;
		}
	} else {
		evtCount = 0;
		upsdebugx(1, "Not using interrupt pipe...");
	}

	/* Process pending events (HID notifications on Interrupt pipe) */
	for (i = 0; i < evtCount; i++) {

		if (HIDGetDataValue(udev, event[i], &value, poll_interval) != 1)
			continue;

		if (nut_debug_level >= 2) {
			upsdebugx(2, "Path: %s, Type: %s, ReportID: 0x%02x, Offset: %i, Size: %i, Value: %g",
				HIDGetDataItem(event[i], subdriver->utab),
				HIDDataType(event[i]), event[i]->ReportID,
				event[i]->Offset, event[i]->Size, value);
		}

		/* Skip Input reports, if we don't use the Feature report */
		found_data = FindObject_with_Path(pDesc, &(event[i]->Path), interrupt_only ? ITEM_INPUT:ITEM_FEATURE);
                if(!found_data && !interrupt_only) {
			found_data = FindObject_with_Path(pDesc, &(event[i]->Path), ITEM_INPUT);
		}
		if(!found_data) {
			upsdebugx(2, "Could not find event as either ITEM_INPUT or ITEM_FEATURE?");
			continue;
		}
		item = find_hid_info(found_data);
		if (!item) {
			upsdebugx(3, "NUT doesn't use this HID object");
			continue;
		}

		ups_infoval_set(item, value);
	}
#ifdef DEBUG
	upsdebugx(1, "took %.3f seconds handling interrupt reports...\n", interval());
#endif
	/* clear status buffer before begining */
	status_init();

	/* Do a full update (polling) every pollfreq or upon data change (ie setvar/instcmd) */
	if ((now > (lastpoll + pollfreq)) || (data_has_changed == TRUE)) {
		upsdebugx(1, "Full update...");

		alarm_init();

		if (hid_ups_walk(HU_WALKMODE_FULL_UPDATE) == FALSE)
			return;

		lastpoll = now;
		data_has_changed = FALSE;

		ups_alarm_set();
		alarm_commit();
	} else {
		upsdebugx(1, "Quick update...");

		/* Quick poll data only to see if the UPS is still connected */
		if (hid_ups_walk(HU_WALKMODE_QUICK_UPDATE) == FALSE)
			return;
	}

	ups_status_set();
	status_commit();

	dstate_dataok();
#ifdef DEBUG
	upsdebugx(1, "took %.3f seconds handling feature reports...\n", interval());
#endif
}

void upsdrv_initinfo(void)
{
	char	*val;

	upsdebugx(1, "upsdrv_initinfo...");

	dstate_setinfo("driver.version.data", "%s", subdriver->name);

	/* init polling frequency */
	val = getval(HU_VAR_POLLFREQ);
	if (val) {
		pollfreq = atoi(val);
	}

	dstate_setinfo("driver.parameter.pollfreq", "%d", pollfreq);

	/* ignore (broken) interrupt pipe */
	if (testvar("pollonly")) {
		use_interrupt_pipe = FALSE;
	}

	time(&lastpoll);

	/* install handlers */
	upsh.setvar = setvar;
	upsh.instcmd = instcmd;
}

void upsdrv_initups(void)
{
	int ret;
	char *val;

	upsdebugx(2, "Initializing an USB-connected UPS with library %s " \
		"(NUT subdriver name='%s' ver='%s')",
		dstate_getinfo("driver.version.usb"),
		comm_driver->name, comm_driver->version );

#ifdef SHUT_MODE
	/*!
	 * SHUT is a serial protocol, so it needs
	 * only the device path
	 */
	upsdebugx(1, "upsdrv_initups (SHUT)...");

	subdriver_matcher = device_path;
#else
	char *regex_array[6];

	upsdebugx(1, "upsdrv_initups (non-SHUT)...");

	subdriver_matcher = &subdriver_matcher_struct;

	/* enforce use of the "vendorid" option if "explore" is given */
	if (testvar("explore") && getval("vendorid")==NULL) {
		fatalx(EXIT_FAILURE, "must specify \"vendorid\" when using \"explore\"");
	}

	/* Activate maxreport tweak */
	if (testvar("maxreport")) {
		max_report_size = 1;
	}

	/* process the UPS selection options */
	regex_array[0] = getval("vendorid");
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor");
	regex_array[3] = getval("product");
	regex_array[4] = getval("serial");
	regex_array[5] = getval("bus");

	ret = USBNewRegexMatcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
	switch(ret)
	{
	case 0:
		break;
	case -1:
		fatal_with_errno(EXIT_FAILURE, "HIDNewRegexMatcher()");
#ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif
	default:
		fatalx(EXIT_FAILURE, "invalid regular expression: %s", regex_array[ret]);
#ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif
	}

	/* link the matchers */
	subdriver_matcher->next = regex_matcher;
#endif /* SHUT_MODE */

	/* Search for the first supported UPS matching the
	   regular expression (USB) or device_path (SHUT) */
	ret = comm_driver->open(&udev, &curDevice, subdriver_matcher, &callback);
	if (ret < 1)
		fatalx(EXIT_FAILURE, "No matching HID UPS found");

	hd = &curDevice;

	upsdebugx(1, "Detected a UPS: %s/%s", hd->Vendor ? hd->Vendor : "unknown",
		hd->Product ? hd->Product : "unknown");

	/* Activate Powercom tweaks */
	if (testvar("interruptonly")) {
		interrupt_only = 1;
	}
	val = getval("interruptsize");
	if (val) {
		interrupt_size = atoi(val);
	}

	if (hid_ups_walk(HU_WALKMODE_INIT) == FALSE) {
		fatalx(EXIT_FAILURE, "Can't initialize data from HID UPS");
	}

	if (dstate_getinfo("battery.charge.low")) {
		/* Retrieve user defined battery settings */
		val = getval(HU_VAR_LOWBATT);
		if (val) {
			dstate_setinfo("battery.charge.low", "%ld", strtol(val, NULL, 10));
		}
	}

	if (dstate_getinfo("ups.delay.start")) {
		/* Retrieve user defined delay settings */
		val = getval(HU_VAR_ONDELAY);
		if (val) {
			dstate_setinfo("ups.delay.start", "%ld", strtol(val, NULL, 10));
		}
	}

	if (dstate_getinfo("ups.delay.shutdown")) {
		/* Retrieve user defined delay settings */
		val = getval(HU_VAR_OFFDELAY);
		if (val) {
			dstate_setinfo("ups.delay.shutdown", "%ld", strtol(val, NULL, 10));
		}
	}

	if (find_nut_info("load.off.delay")) {
		/* Adds default with a delay value of '0' (= immediate) */
		dstate_addcmd("load.off");
	}

	if (find_nut_info("load.on.delay")) {
		/* Adds default with a delay value of '0' (= immediate) */
		dstate_addcmd("load.on");
	}

	if (find_nut_info("load.off.delay") && find_nut_info("load.on.delay")) {
		/* Add composite instcmds (require setting multiple HID values) */
		dstate_addcmd("shutdown.return");
		dstate_addcmd("shutdown.stayoff");
	}
}

void upsdrv_cleanup(void)
{
	upsdebugx(1, "upsdrv_cleanup...");

	comm_driver->close(udev);
	Free_ReportDesc(pDesc);
	free_report_buffer(reportbuf);
#ifndef SHUT_MODE
	USBFreeExactMatcher(exact_matcher);
	USBFreeRegexMatcher(regex_matcher);

	free(curDevice.Vendor);
	free(curDevice.Product);
	free(curDevice.Serial);
	free(curDevice.Bus);
#endif
}

/**********************************************************************
 * Support functions
 *********************************************************************/

void possibly_supported(const char *mfr, HIDDevice_t *hd)
{
	upsdebugx(0,
"This %s device (%04x:%04x) is not (or perhaps not yet) supported\n"
"by usbhid-ups. Please make sure you have an up-to-date version of NUT. If\n"
"this does not fix the problem, try running the driver with the\n"
"'-x productid=%04x' option. Please report your results to the NUT user's\n"
"mailing list <nut-upsuser@lists.alioth.debian.org>.\n",
	mfr, hd->VendorID, hd->ProductID, hd->ProductID);
}

/* Update ups_status to remember this status item. Interpretation is
   done in ups_status_set(). */
static void process_boolean_info(const char *nutvalue)
{
	status_lkp_t *status_item;
	int clear = 0;

	upsdebugx(5, "process_boolean_info: %s", nutvalue);

	if (*nutvalue == '!') {
		nutvalue++;
		clear = 1;
	}

	for (status_item = status_info; status_item->status_str != NULL ; status_item++)
	{
		if (strcasecmp(status_item->status_str, nutvalue))
			continue;

		if (clear) {
			ups_status &= ~status_item->status_mask;
		} else {
			ups_status |= status_item->status_mask;
		}

		return;
	}

	upsdebugx(5, "Warning: %s not in list of known values", nutvalue);
}

static int callback(hid_dev_handle_t argudev, HIDDevice_t *arghd, unsigned char *rdbuf, int rdlen)
{
	int i;
	const char *mfr = NULL, *model = NULL, *serial = NULL;
#ifndef SHUT_MODE
	int ret;
#endif
	upsdebugx(2, "Report Descriptor size = %d", rdlen);
	upsdebug_hex(3, "Report Descriptor", rdbuf, rdlen);

	/* Save the global "hd" for this driver instance */
	hd = arghd;
	udev = argudev;

	/* Parse Report Descriptor */
	Free_ReportDesc(pDesc);
	pDesc = Parse_ReportDesc(rdbuf, rdlen);
	if (!pDesc) {
		upsdebug_with_errno(1, "Failed to parse report descriptor!");
		return 0;
	}

	/* prepare report buffer */
	free_report_buffer(reportbuf);
	reportbuf = new_report_buffer(pDesc);
	if (!reportbuf) {
		upsdebug_with_errno(1, "Failed to allocate report buffer!");
		Free_ReportDesc(pDesc);
		return 0;
	}

	/* select the subdriver for this device */
	for (i=0; subdriver_list[i] != NULL; i++) {
		if (subdriver_list[i]->claim(hd)) {
			break;
		}
	}

	subdriver = subdriver_list[i];
	if (!subdriver) {
		upsdebugx(1, "Manufacturer not supported!");
		return 0;
	}

	upslogx(2, "Using subdriver: %s", subdriver->name);

	HIDDumpTree(udev, subdriver->utab);

#ifndef SHUT_MODE
	/* create a new matcher for later matching */
	USBFreeExactMatcher(exact_matcher);
	ret = USBNewExactMatcher(&exact_matcher, hd);
	if (ret) {
		upsdebug_with_errno(1, "USBNewExactMatcher()");
		return 0;
	}

	regex_matcher->next = exact_matcher;
#endif /* SHUT_MODE */

	/* apply subdriver specific formatting */
	mfr = subdriver->format_mfr(hd);
	model = subdriver->format_model(hd);
	serial = subdriver->format_serial(hd);

	if (mfr != NULL) {
		dstate_setinfo("ups.mfr", "%s", mfr);
	} else {
		dstate_delinfo("ups.mfr");
	}

	if (model != NULL) {
		dstate_setinfo("ups.model", "%s", model);
	} else {
		dstate_delinfo("ups.model");
	}

	if (serial != NULL) {
		dstate_setinfo("ups.serial", "%s", serial);
	} else {
		dstate_delinfo("ups.serial");
	}

	dstate_setinfo("ups.vendorid", "%04x", hd->VendorID);
	dstate_setinfo("ups.productid", "%04x", hd->ProductID);

	return 1;
}

#ifdef DEBUG
static double interval(void)
{
	struct timeval		now;
	static struct timeval	last;
	double	ret;

	gettimeofday(&now, NULL);

	ret = now.tv_sec - last.tv_sec	+ ((double)(now.tv_usec - last.tv_usec)) / 1000000;
	last = now;

	return ret;
}
#endif

/* walk ups variables and set elements of the info array. */
static bool_t hid_ups_walk(walkmode_t mode)
{
	hid_info_t	*item;
	double		value;
	int		retcode;

#ifndef SHUT_MODE
	/* extract the VendorId for further testing */
	int vendorID = usb_device((struct usb_dev_handle *)udev)->descriptor.idVendor;
	int productID = usb_device((struct usb_dev_handle *)udev)->descriptor.idProduct;
#endif

	/* 3 modes: HU_WALKMODE_INIT, HU_WALKMODE_QUICK_UPDATE and HU_WALKMODE_FULL_UPDATE */

	/* Device data walk ----------------------------- */
	for (item = subdriver->hid2nut; item->info_type != NULL; item++) {

#ifdef SHUT_MODE
		/* Check if we are asked to stop (reactivity++) in SHUT mode.
		 * In USB mode, looping through this takes well under a second,
		 * so any effort to improve reactivity here is wasted. */
		if (exit_flag != 0)
			return TRUE;
#endif
		/* filter data according to mode */
		switch (mode)
		{
		/* Device capabilities enumeration */
		case HU_WALKMODE_INIT:
			/* Apparently, we are reconnecting, so
			 * NUT-to-HID translation is already good */
			if (item->hiddata != NULL)
				break;

			/* Create the NUT-to-HID mapping */
			item->hiddata = HIDGetItemData(item->hidpath, subdriver->utab);
			if (item->hiddata == NULL)
				continue;

			/* Special case for handling server side variables */
			if (item->hidflags & HU_FLAG_ABSENT) {

				/* already set */
				if (dstate_getinfo(item->info_type))
					continue;

				dstate_setinfo(item->info_type, "%s", item->dfl);
				dstate_setflags(item->info_type, item->info_flags);

				/* Set max length for strings, if needed */
				if (item->info_flags & ST_FLAG_STRING)
					dstate_setaux(item->info_type, item->info_len);

				continue;
			}

			/* Allow duplicates for these NUT variables... */
			if (!strncmp(item->info_type, "ups.alarm", 9)) {
				break;
			}

			/* ...this one doesn't exist yet... */
			if (dstate_getinfo(item->info_type) == NULL) {
				break;
			}

			/* ...but this one does, so don't use it! */
			item->hiddata = NULL;
			continue;

		case HU_WALKMODE_QUICK_UPDATE:
			/* Quick update only deals with status and alarms! */
			if (!(item->hidflags & HU_FLAG_QUICK_POLL))
				continue;

			break;

		case HU_WALKMODE_FULL_UPDATE:
			/* These don't need polling after initinfo() */
			if (item->hidflags & (HU_FLAG_ABSENT | HU_TYPE_CMD | HU_FLAG_STATIC))
				continue;

			/* These need to be polled after user changes (setvar / instcmd) */
			if ( (item->hidflags & HU_FLAG_SEMI_STATIC) && (data_has_changed == FALSE) )
				continue;

			break;

		default:
			fatalx(EXIT_FAILURE, "hid_ups_walk: unknown update mode!");
		}

#ifndef SHUT_MODE
		/* skip report 0x54 for Tripplite SU3000LCD2UHV due to firmware bug */
		if ((vendorID == 0x09ae) && (productID == 0x1330)) {
			if (item->hiddata && (item->hiddata->ReportID == 0x54)) {
				continue;
			}
		}
#endif

		retcode = HIDGetDataValue(udev, item->hiddata, &value, poll_interval);

		switch (retcode)
		{
		case -EBUSY:		/* Device or resource busy */
			upslog_with_errno(LOG_CRIT, "Got disconnected by another driver");
			goto fallthrough_reconnect;
		case -EPERM:		/* Operation not permitted */
		case -ENODEV:		/* No such device */
		case -EACCES:		/* Permission denied */
		case -EIO:		/* I/O error */
		case -ENXIO:		/* No such device or address */
		case -ENOENT:		/* No such file or directory */
		fallthrough_reconnect:
			/* Uh oh, got to reconnect! */
			hd = NULL;
			return FALSE;

		case 1:
			break;	/* Found! */

		case 0:
			continue;

		case -ETIMEDOUT:	/* Connection timed out */
		case -EOVERFLOW:	/* Value too large for defined data type */
#ifdef EPROTO
		case -EPROTO:		/* Protocol error */
#endif
		case -EPIPE:		/* Broken pipe */
		default:
			/* Don't know what happened, try again later... */
			continue;
		}

		upsdebugx(2, "Path: %s, Type: %s, ReportID: 0x%02x, Offset: %i, Size: %i, Value: %g",
			item->hidpath, HIDDataType(item->hiddata), item->hiddata->ReportID,
			item->hiddata->Offset, item->hiddata->Size, value);

		if (item->hidflags & HU_TYPE_CMD) {
			upsdebugx(3, "Adding command '%s' using Path '%s'",
				item->info_type, item->hidpath);
			dstate_addcmd(item->info_type);
			continue;
		}

		/* Process the value we got back (set status bits and
		 * set the value of other parameters) */
		if (ups_infoval_set(item, value) != 1)
			continue;

		if (mode == HU_WALKMODE_INIT) {
			info_lkp_t	*info_lkp;

			dstate_setflags(item->info_type, item->info_flags);

			/* Set max length for strings */
			if (item->info_flags & ST_FLAG_STRING) {
				dstate_setaux(item->info_type, item->info_len);
			}

			/* Set enumerated values, only if the data has ST_FLAG_RW */
			if (!(item->hidflags & HU_FLAG_ENUM) || !(item->info_flags & ST_FLAG_RW)) {
				continue;
			}

			/* Loop on all existing values */
			for (info_lkp = item->hid2info; info_lkp != NULL
				&& info_lkp->nut_value != NULL; info_lkp++) {
				/* Check if this value is supported */
				if (hu_find_infoval(item->hid2info, info_lkp->hid_value) != NULL) {
					dstate_addenum(item->info_type, "%s", info_lkp->nut_value);
				}
			}
		}
	}

	return TRUE;
}

static int reconnect_ups(void)
{
	int ret;

	upsdebugx(4, "==================================================");
	upsdebugx(4, "= device has been disconnected, try to reconnect =");
	upsdebugx(4, "==================================================");

	ret = comm_driver->open(&udev, &curDevice, subdriver_matcher, NULL);

	if (ret > 0) {
		return 1;
	}

	return 0;
}

/* Convert the local status information to NUT format and set NUT
   alarms. */
static void ups_alarm_set(void)
{
	if (ups_status & STATUS(REPLACEBATT)) {
		alarm_set("Replace battery!");
	}
	if (ups_status & STATUS(SHUTDOWNIMM)) {
		alarm_set("Shutdown imminent!");
	}
	if (ups_status & STATUS(FANFAIL)) {
		alarm_set("Fan failure!");
	}
	if (ups_status & STATUS(NOBATTERY)) {
		alarm_set("No battery installed!");
	}
	if (ups_status & STATUS(BATTVOLTLO)) {
		alarm_set("Battery voltage too low!");
	}
	if (ups_status & STATUS(BATTVOLTHI)) {
		alarm_set("Battery voltage too high!");
	}
	if (ups_status & STATUS(CHARGERFAIL)) {
		alarm_set("Battery charger fail!");
	}
	if (ups_status & STATUS(OVERHEAT)) {
		alarm_set("Temperature too high!");	/* overheat; Belkin, TrippLite */
	}
	if (ups_status & STATUS(COMMFAULT)) {
		alarm_set("Internal UPS fault!");	/* UPS fault; Belkin, TrippLite */
	}
	if (ups_status & STATUS(AWAITINGPOWER)) {
		alarm_set("Awaiting power!");		/* awaiting power; Belkin, TrippLite */
	}
	if (ups_status & STATUS(BYPASSAUTO)) {
		alarm_set("Automatic bypass mode!");
	}
	if (ups_status & STATUS(BYPASSMAN)) {
		alarm_set("Manual bypass mode!");
	}
}

/* Return the current value of ups_status */
int ups_status_get(void)
{
	return ups_status;
}

/* Convert the local status information to NUT format and set NUT
   status. */
static void ups_status_set(void)
{
	if (ups_status & STATUS(VRANGE)) {
		dstate_setinfo("input.transfer.reason", "input voltage out of range");
	} else if (ups_status & STATUS(FRANGE)) {
		dstate_setinfo("input.transfer.reason", "input frequency out of range");
	} else {
		dstate_delinfo("input.transfer.reason");
	}

	if (ups_status & STATUS(ONLINE)) {
		status_set("OL");		/* on line */
	} else {
		status_set("OB");		/* on battery */
	}
	if ((ups_status & STATUS(DISCHRG)) &&
		!(ups_status & STATUS(DEPLETED))) {
		status_set("DISCHRG");		/* discharging */
	}
	if ((ups_status & STATUS(CHRG)) &&
		!(ups_status & STATUS(FULLYCHARGED))) {
		status_set("CHRG");		/* charging */
	}
	if (ups_status & (STATUS(LOWBATT) | STATUS(TIMELIMITEXP) | STATUS(SHUTDOWNIMM))) {
		status_set("LB");		/* low battery */
	}
	if (ups_status & STATUS(OVERLOAD)) {
		status_set("OVER");		/* overload */
	}
	if (ups_status & STATUS(REPLACEBATT)) {
		status_set("RB");		/* replace batt */
	}
	if (ups_status & STATUS(TRIM)) {
		status_set("TRIM");		/* SmartTrim */
	}
	if (ups_status & STATUS(BOOST)) {
		status_set("BOOST");		/* SmartBoost */
	}
	if (ups_status & (STATUS(BYPASSAUTO) | STATUS(BYPASSMAN))) {
		status_set("BYPASS");		/* on bypass */
	}
	if (ups_status & STATUS(OFF)) {
		status_set("OFF");		/* ups is off */
	}
	if (ups_status & STATUS(CAL)) {
		status_set("CAL");		/* calibration */
	}
}

/* find info element definition in info array
 * by NUT varname.
 */
static hid_info_t *find_nut_info(const char *varname)
{
	hid_info_t *hidups_item;

	for (hidups_item = subdriver->hid2nut; hidups_item->info_type != NULL ; hidups_item++) {

		if (strcasecmp(hidups_item->info_type, varname))
			continue;

		if (hidups_item->hiddata != NULL)
			return hidups_item;
	}

	upsdebugx(2, "find_nut_info: unknown info type: %s", varname);
	return NULL;
}

/* find info element definition in info array
 * by HID data pointer.
 */
static hid_info_t *find_hid_info(const HIDData_t *hiddata)
{
	hid_info_t *hidups_item;

	if(!hiddata) {
		upsdebugx(2, "%s: hiddata == NULL", __func__);
		return NULL;
	}

	for (hidups_item = subdriver->hid2nut; hidups_item->info_type != NULL ; hidups_item++) {

		/* Skip server side vars */
		if (hidups_item->hidflags & HU_FLAG_ABSENT)
			continue;

		if (hidups_item->hiddata == hiddata)
			return hidups_item;
	}

	return NULL;
}

/* find the HID Item value matching that NUT value */
/* useful for set with value lookup... */
static long hu_find_valinfo(info_lkp_t *hid2info, const char* value)
{
	info_lkp_t	*info_lkp;

	/* if a conversion function is defined, use 'value' as argument for it */
	if (hid2info->nuf != NULL) {
		double	hid_value;
		hid_value = hid2info->nuf(value);
		upsdebugx(5, "hu_find_valinfo: found %g (value: %s)", hid_value, value);
		return hid_value;
	}

	for (info_lkp = hid2info; info_lkp->nut_value != NULL; info_lkp++) {
		if (!(strcmp(info_lkp->nut_value, value))) {
			upsdebugx(5, "hu_find_valinfo: found %s (value: %ld)", info_lkp->nut_value, info_lkp->hid_value);
			return info_lkp->hid_value;
		}
	}

	upsdebugx(3, "hu_find_valinfo: no matching HID value for this INFO_* value (%s)", value);
	return -1;
}

/* find the NUT value matching that HID Item value */
static const char *hu_find_infoval(info_lkp_t *hid2info, const double value)
{
	info_lkp_t	*info_lkp;

	/* if a conversion function is defined, use 'value' as argument for it */
	if (hid2info->fun != NULL) {
		return hid2info->fun(value);
	}

	/* use 'value' as an index for a lookup in an array */
	for (info_lkp = hid2info; info_lkp->nut_value != NULL; info_lkp++) {
		if (info_lkp->hid_value == (long)value) {
			upsdebugx(5, "hu_find_infoval: found %s (value: %ld)", info_lkp->nut_value, (long)value);
			return info_lkp->nut_value;
		}
	}

	upsdebugx(3, "hu_find_infoval: no matching INFO_* value for this HID value (%g)", value);
	return NULL;
}

/* return -1 on failure, 0 for a status update and 1 in all other cases */
static int ups_infoval_set(hid_info_t *item, double value)
{
	const char	*nutvalue;

	/* need lookup'ed translation? */
	if (item->hid2info != NULL){

		if ((nutvalue = hu_find_infoval(item->hid2info, value)) == NULL) {
			upsdebugx(5, "Lookup [%g] failed for [%s]", value, item->info_type);
			return -1;
		}

		/* deal with boolean items */
		if (!strncmp(item->info_type, "BOOL", 4)) {
			process_boolean_info(nutvalue);
			return 0;
		}

		/* deal with alarm items */
		if (!strncmp(item->info_type, "ups.alarm", 9)) {
			alarm_set(nutvalue);
			return 0;
		}

		dstate_setinfo(item->info_type, "%s", nutvalue);
	} else {
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
		dstate_setinfo(item->info_type, item->dfl, value);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	}

	return 1;
}
