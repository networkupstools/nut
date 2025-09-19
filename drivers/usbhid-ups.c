/* usbhid-ups.c - Driver for USB and serial (MGE SHUT) HID UPS units
 *
 * Copyright (C)
 *   2003-2022 Arnaud Quette <arnaud.quette@gmail.com>
 *   2005      John Stamp <kinsayder@hotmail.com>
 *   2005-2006 Peter Selinger <selinger@users.sourceforge.net>
 *   2007-2009 Arjen de Korte <adkorte-guest@alioth.debian.org>
 *   2016      Eaton / Arnaud Quette <ArnaudQuette@Eaton.com>
 *   2020-2025 Jim Klimov <jimklimov+nut@gmail.com>
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
#define DRIVER_VERSION	"0.68"

#define HU_VAR_WAITBEFORERECONNECT "waitbeforereconnect"

#include "main.h"	/* Must be first, includes "config.h" */
#include "nut_stdint.h"
#include "nut_float.h"
#include "libhid.h"
#include "usbhid-ups.h"
#include "hidparser.h"
#include "hidtypes.h"
#include "common.h"
#ifdef WIN32
#include "wincompat.h"
#endif	/* WIN32 */

/* include all known subdrivers */
#include "mge-hid.h"

#if !((defined SHUT_MODE) && SHUT_MODE)
	/* explore stub goes first, others alphabetically */
#	include "explore-hid.h"
#	include "apc-hid.h"
#	include "arduino-hid.h"
#	include "belkin-hid.h"
#	include "cps-hid.h"
#	include "delta_ups-hid.h"
#	include "ecoflow-hid.h"
#	include "ever-hid.h"
#	include "idowell-hid.h"
#	include "legrand-hid.h"
#	include "liebert-hid.h"
#	include "openups-hid.h"
#	include "powercom-hid.h"
#	include "powervar-hid.h"
#	include "salicru-hid.h"
#	include "tripplite-hid.h"
#endif	/* !SHUT_MODE => USB */

/* Reference list of available subdrivers */
static subdriver_t *subdriver_list[] = {
#if !((defined SHUT_MODE) && SHUT_MODE)
	&explore_subdriver,
#endif	/* !SHUT_MODE => USB */
	/* mge-hid.c supports both SHUT and USB */
	&mge_subdriver,
#if !((defined SHUT_MODE) && SHUT_MODE)
	&apc_subdriver,
	&arduino_subdriver,
	&belkin_subdriver,
	&cps_subdriver,
	&delta_ups_subdriver,
	&ecoflow_subdriver,
	&ever_subdriver,
	&idowell_subdriver,
	&legrand_subdriver,
	&liebert_subdriver,
	&openups_subdriver,
	&powercom_subdriver,
	&powervar_subdriver,
	&salicru_subdriver,
	&tripplite_subdriver,
#endif	/* !SHUT_MODE => USB */
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
#if !((defined SHUT_MODE) && SHUT_MODE)
	DRV_STABLE,
#else	/* SHUT_MODE */
	DRV_EXPERIMENTAL,
#endif	/* SHUT_MODE / USB */
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
static HIDDevice_t curDevice = { 0x0000, 0x0000, NULL, NULL, NULL, NULL, 0, NULL
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	, NULL
#endif
};
static HIDDeviceMatcher_t *subdriver_matcher = NULL;
#if !((defined SHUT_MODE) && SHUT_MODE)
static HIDDeviceMatcher_t *exact_matcher = NULL;
static HIDDeviceMatcher_t *regex_matcher = NULL;
#endif	/* !SHUT_MODE => USB */
static int pollfreq = DEFAULT_POLLFREQ;
static unsigned ups_status = 0;
static bool_t data_has_changed = FALSE; /* for SEMI_STATIC data polling */
#ifndef SUN_LIBUSB
bool_t use_interrupt_pipe = TRUE;
#else
bool_t use_interrupt_pipe = FALSE;
#endif
static size_t interrupt_pipe_EIO_count = 0; /* How many times we had I/O errors since last reconnect? */

/**
 * How many times do we tolerate having "0 HID objects" in a row?
 * Default -1 means indefinitely, but when some controllers hang,
 * this is a clue that we want to fully restart the connection.
 */
static long interrupt_pipe_no_events_tolerance = -1;
/* How many times did we actually have "Got 0 HID objects" in a row? */
static long interrupt_pipe_no_events_count = 0;
/* How HIDGetEvents() below reports no events found */
#define	NUT_LIBUSB_CODE_NO_EVENTS	0

static time_t lastpoll; /* Timestamp the last polling */
hid_dev_handle_t udev = HID_DEV_HANDLE_CLOSED;

/**
 * Track when calibration started, whether known from UPS status flags
 * or interpreted from OL&DISCHRG combo on some devices (see below).
 * The last_calibration_start is reset to 0 when the status becomes
 * inactive, and last_calibration_finish is incremented every time.
 */
static time_t last_calibration_start = 0;
static time_t last_calibration_finish = 0;

/**
 * CyberPower UT series sometime need a bit of help deciding their online status.
 * This quirk is to enable the special handling of OL & DISCHRG at the same time
 * as being OB (on battery power/no mains power). Enabled by device config flag.
 * NOTE: Also known by legacy alias "onlinedischarge", deprecated but tolerated
 * since NUT v2.8.2.
 */
static int onlinedischarge_onbattery = 0;

/**
 * Some UPS models (e.g. APC were seen to do so) report OL & DISCHRG when they
 * are in calibration mode. This usually happens after a few seconds reporting
 * an "OFF" state as well, while the hardware is switching to on-battery mode.
 */
static int onlinedischarge_calibration = 0;

/**
 * If/when an UPS reports OL & DISCHRG and we do not use any other special
 * settings (e.g. they do not match the actual device capabilities/behaviors),
 * the driver logs messages about the unexpected situation - on every cycle
 * if must be. This setting allows to throttle frequency of such messages.
 * A value of 0 is small enough to log on every processing cycle (old noisy
 * de-facto default before NUT v2.8.2 which this throttle alleviates).
 * A value of -1 (any negative passed via configuration) disables repeated
 * messages completely.
 * Default (-2) below is not final: if this throttle variable is *not* set
 * in the driver configuration section, and...
 * - If the device reports a battery.charge, the driver would default to
 *   only repeat reports about OL & DISCHRG when this charge changes from
 *   what it was when the previous report was made, independent of time;
 * - Otherwise 30 sec.
 * If both the throttle is set and battery.charge is reported (and changes
 * over time), then hitting either trigger allows the message to be logged.
 */
static int onlinedischarge_log_throttle_sec = -2;
/**
 * When did we last emit the message?
 */
static time_t onlinedischarge_log_throttle_timestamp = 0;
/**
 * Last known battery charge (rounded to whole percent units)
 * as of when we last actually logged about OL & DISCHRG.
 * Gets reset to -1 whenever this condition is not present.
 */
static int onlinedischarge_log_throttle_charge = -1;
/**
 * If battery.charge is served and equals or exceeds this value,
 * suppress logging about OL & DISCHRG if battery.charge varied
 * since last logged message. Defaults to 100% as some devices
 * only report this state combo when fully charged (probably
 * they try to prolong battery life by not over-charging it).
 */
static int onlinedischarge_log_throttle_hovercharge = 100;

/**
 * Per https://github.com/networkupstools/nut/issues/2347 some
 * APC BXnnnnMI devices made (flashed?) in 2023-2024 irregularly
 * but frequently spew a series of state changes:
 * * (maybe OL+DISCHRG),
 * * LB,
 * * RB,
 * * <all ok>
 * within a couple of seconds. If this tunable is positive, we
 * would only report the device states on the bus if they persist
 * that long (or more), only then assuming they reflect a real
 * problematic state and not some internal calibration.
 */
static int lbrb_log_delay_sec = 0;
/**
 * By default we only act on (lbrb_log_delay_sec>0) when the device
 * is in calibration mode of whatever nature (directly reported or
 * assumed from other flag combos). With this flag we do not check
 * for calibration and only look at LB + RB timestamps.
 */
static int lbrb_log_delay_without_calibrating = 0;
/**
 * When did we last enter the situation? (if more than lbrb_log_delay_sec
 * ago, then set the device status and emit the message)
 */
static time_t last_lb_start = 0;
static time_t last_rb_start = 0;

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
static int callback(hid_dev_handle_t argudev, HIDDevice_t *arghd,
					usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen);
#ifdef DEBUG
static double interval(void);
#endif

/* global variables */
HIDDesc_t	*pDesc = NULL;		/* parsed Report Descriptor */
reportbuf_t	*reportbuf = NULL;	/* buffer for most recent reports */
int disable_fix_report_desc = 0; /* by default we apply fix-ups for broken USB encoding, etc. */

/* --------------------------------------------------------------- */
/* Struct & data for boolean processing                            */
/* --------------------------------------------------------------- */

/* Note: this structure holds internal status info, directly as
   collected from the hardware; not yet converted to official NUT
   status or alarms */
typedef struct {
	const char	*status_str;			/* ups status string */
	const unsigned int	status_mask;	/* ups status mask */
} status_lkp_t;

static status_lkp_t status_info[] = {
	/* map internal status strings to bit masks */
	{ "online", STATUS(ONLINE) },
	{ "offline", STATUS(OFFLINE) },
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
	{ "ecomode", STATUS(ECOMODE) },	/* Should not get used (at least not via */
	{ "essmode", STATUS(ESSMODE) },	/* ups.status), but tracked just in case */
	{ "off", STATUS(OFF) },
	{ "cal", STATUS(CALIB) },
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
	{ 0, "offline", NULL, NULL },	/* previously was "!online" but that proved ambiguous */
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

/* Should not get used (at least not via ups.status), but tracked just in case.
 * Currently referenced in mge-hid.c */
info_lkp_t eco_mode_info[] = {
	{ 0, "normal", NULL, NULL },
	{ 1, "ecomode", NULL, NULL },
	{ 2, "essmode", NULL, NULL },
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
	/* Per  spec https://www.usb.org/sites/default/files/pdcv11.pdf (page 38):
	 * 4.2.6 Battery Settings -> ManufacturerDate
	 *   The date the pack was manufactured in a packed integer.
	 *   The date is packed in the following fashion:
	 *   (year – 1980)*512 + month*32 + day.
	 */
	static char buf[32];
	long year, month, day;

	if ((long)value == 0) {
		return "not set";
	}

	/* TOTHINK: About the comment below...
	 * Does bit-shift keep the negativeness on all architectures?
	 */
	/* negative value represents pre-1980 date: */
	year = 1980 + ((long)value >> 9);
	month = ((long)value >> 5) & 0x0f;
	day = (long)value & 0x1f;

	snprintf(buf, sizeof(buf), "%04ld/%02ld/%02ld", year, month, day);

	return buf;
}

static double date_conversion_reverse(const char* date_string)
{
	long year, month, day;
	long date;

	sscanf(date_string, "%04ld/%02ld/%02ld", &year, &month, &day);
	if(year - 1980 > 127 || month > 12 || day > 31)
		return 0;
	date = (year - 1980) << 9;
	date += month << 5;
	date += day;

	return (double) date;
}

info_lkp_t date_conversion[] = {
	{ 0, NULL, date_conversion_fun, date_conversion_reverse }
};

/* returns statically allocated string - must not use it again before
   done with result! */
static const char *hex_conversion_fun(double value)
{
	static char buf[20];

	snprintf(buf, sizeof(buf), "%08lx", (unsigned long)value);

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
static const char *divide_by_100_conversion_fun(double value)
{
       static char buf[20];

       snprintf(buf, sizeof(buf), "%0.1f", value * 0.01);

       return buf;
}

/* FIXME? Do we need an inverse "nuf()" here? */
info_lkp_t divide_by_100_conversion[] = {
       { 0, NULL, divide_by_100_conversion_fun, NULL }
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

static void analyze_mapping_usage(void) {
	/* Check if the subdriver code (mappings) and the device report
	 * sit together well. Note that for yet-unknown concepts, the
	 * NUT driver developers can either raise a discussion on how
	 * to best formalize that concept via docs/nut-names.txt, or
	 * temporarily place them into "experimental.*" or "unmapped.*"
	 * namespaces.
	 *
	 * Later also check that all defined mappings were used?
	 * TBH, this is unlikely in practice, so of little value
	 * (unless we are troubleshooting and under 5 or 10 data
	 * points are served from actually the device, and not
	 * from user configs or driver fallbacks).
	 */
	size_t	d, unused_count = 0, halfused_count = 0;
	size_t	unused_bufsize = LARGEBUF, halfused_bufsize = LARGEBUF, unused_prevlen = 0, halfused_prevlen = 0, used_mappings = 0, known_mappings = 0;
	int	ret_printf;
	char	*unused_names = NULL, *halfused_names = NULL;
	hid_info_t	*hidups_item;

	/* FIXME? this activity is limited to when debugging is enabled, even
	 *  if some of the messages below can be posted visibly at level 0.
	 */
	if (nut_debug_level < 1)
		return;

	upsdebugx(1, "%s: checking if the subdriver code (mappings) "
		"consults all data points from the device report",
		__func__);

	if (!pDesc) {
		upsdebugx(1, "%s: SKIP: pDesc==null", __func__);
		return;
	}

	unused_names = xcalloc(unused_bufsize, sizeof(char));
	halfused_names = xcalloc(halfused_bufsize, sizeof(char));

	for (d = 0; d < pDesc->nitems; d++) {
		HIDData_t	*pData = &pDesc->item[d];

		if (pData && !(pData->mapping_handled)) {
			char	*pName = HIDGetDataItem(pData, subdriver->utab);
			const char	*pType = HIDDataType(pData);
			int	retry = 0;
			char	**pNames = &unused_names;
			size_t	*pCount = &unused_count, *pPrevLen = &unused_prevlen, *pBufSize = &unused_bufsize;

			if (!pName) {
				upsdebugx(2, "%s: error getting a Report Path name, skipped", __func__);
				continue;
			} else {
				/* check if this is a half-used name */
				for (hidups_item = subdriver->hid2nut; hidups_item->info_type != NULL ; hidups_item++) {
					/* Note: using a shortcut with mapping table and
					 * its "hidpath" strings here, to avoid stringifying
					 * all paths known from report descriptor - more so
					 * in a nested loop. */
					if (hidups_item->hidpath && hidups_item->hiddata
					&& !strcasecmp(hidups_item->hidpath, pName)
					&& hidups_item->hiddata->mapping_handled
					) {
						upsdebugx(5, "%s: Path '%s' is half-used: "
							"Type '%s' was not touched via mapping, "
							"but '%s' was used",
							__func__, NUT_STRARG(pName),
							NUT_STRARG(pType),
							NUT_STRARG(HIDDataType(hidups_item->hiddata))
							);
						pNames = &halfused_names;
						pCount = &halfused_count;
						pPrevLen = &halfused_prevlen;
						pBufSize = &halfused_bufsize;
						break;
					}
				}
			}

			/* We may overflow the pre-allocated buffer,
			 * so we loop here until snprintf() succeeds
			 * or we are known to have failed completely.
			 */
			do {
				retry = 0;
				if (!*pNames) {
					break;
				}

				upsdebugx(5, "%s: adding '%s (%s)' (%" PRIuSIZE " bytes) "
					"to buffer of %" PRIuSIZE "/%" PRIuSIZE " bytes",
					__func__, NUT_STRARG(pName), NUT_STRARG(pType),
					pName ? strlen(pName) : 0,
					*pPrevLen, *pBufSize);

				ret_printf = snprintf(*pNames + *pPrevLen, *pBufSize - *pPrevLen - 1, "%s%s (%s)",
					*pCount ? ", " : "", NUT_STRARG(pName), NUT_STRARG(pType));

				upsdebugx(6, "%s: snprintf() returned %d", __func__, ret_printf);
				(*pNames)[*pBufSize - 1] = '\0';

				if (ret_printf < 0) {
					upsdebugx(1, "%s: error collecting names, might not report unused descriptor names", __func__);
				} else if ((size_t)ret_printf + *pPrevLen >= *pBufSize) {
					if (*pBufSize < SIZE_MAX - LARGEBUF) {
						*pBufSize = *pBufSize + LARGEBUF;
						upsdebugx(1, "%s: buffer overflowed, trying to re-allocate as %" PRIuSIZE, __func__, *pBufSize);
							*pNames = realloc(*pNames, *pBufSize);

						if (!*pNames) {
							upsdebugx(1, "%s: buffer overflowed, will not report unused descriptor names", __func__);
						} else {
							upsdebugx(5, "%s: buffer overflowed, but reallocated successfully - retrying", __func__);
							/* Retry this loop */
							retry = 1;
						}
					} else {
						upsdebugx(1, "%s: buffer overflowed, might not report unused descriptor names", __func__);
					}
				} else {
					*pPrevLen += (size_t)ret_printf;
				}
			} while (retry);

			*pCount = *pCount + 1;
		}
	}

	if (unused_count) {
		upsdebugx(1, "%s: %" PRIuSIZE " items are present in the "
			"report descriptor from HID UPS, but %" PRIuSIZE " "
			"of them were completely not used by name via the "
			"mapping defined in the selected NUT subdriver %s: %s",
			__func__, pDesc->nitems, unused_count,
			subdriver->name, NUT_STRARG(unused_names));
	}

	if (halfused_count) {
		upsdebugx(1, "%s: %" PRIuSIZE " items are present in the "
			"report descriptor from HID UPS, but %" PRIuSIZE " "
			"of them have several Types named by same Path value, "
			"where at least one of the names was used and other(s) "
			"were not used by the mapping defined in "
			"the selected NUT subdriver %s: %s",
			__func__, pDesc->nitems, halfused_count,
			subdriver->name, NUT_STRARG(halfused_names));
	}

	if (unused_names)
		free(unused_names);
	if (halfused_names)
		free(halfused_names);

	/* Check that all defined mappings were used? */
	for (hidups_item = subdriver->hid2nut; hidups_item->info_type != NULL ; hidups_item++) {
		known_mappings++;

		if (hidups_item && hidups_item->hiddata
		&& hidups_item->hiddata->mapping_handled
		) {
			used_mappings++;
		}
	}

	/* We arbitrarily declare that having under 10 known or used
	 * mappings is few enough to be loud about this */
	upsdebugx( (known_mappings < 10 || used_mappings < 10) ? 0 : 5,
		"%s: %" PRIuSIZE " mapping entries are defined, and "
		"%" PRIuSIZE " were actually used from USB HID report, "
		"in the selected NUT subdriver %s",
		__func__, known_mappings, used_mappings,
		subdriver->name);

	if (known_mappings < 10 || used_mappings < 10)
		upsdebugx(0, "Please check %s/docs/developer-guide.chunked/new-drivers.html#hid-subdrivers "
			"for suggestions how you can help improve this driver.", NUT_WEBSITE_BASE);
}

static subdriver_t *match_function_subdriver_name(int fatal_mismatch) {
	char	*subdrv = getval("subdriver");
	subdriver_t	*info = NULL;

	/* Pick up the subdriver name if set explicitly */
	if (subdrv) {
		int	i, flag_HAVE_LIBREGEX = 0;
#if (defined HAVE_LIBREGEX && HAVE_LIBREGEX)
		int	res;
		size_t	len;
		regex_t	*regex_ptr = NULL;
		flag_HAVE_LIBREGEX = 1;
#endif

		upsdebugx(2,
			"%s: matching a subdriver by explicit "
			"name%s: '%s'...",
			__func__,
			flag_HAVE_LIBREGEX ?	"/regex" : "",
			subdrv);

		/* First try exact match for strings like "TrippLite HID 0.85"
		 * Not likely to hit (due to versions etc.), but worth a try :)
		 */
		for (i=0; subdriver_list[i] != NULL; i++) {
			if (strcmp_null(subdrv, subdriver_list[i]->name) == 0) {
				info = subdriver_list[i];
				goto found;
			}
		}

#if (defined HAVE_LIBREGEX && HAVE_LIBREGEX)
		/* Then try a case-insensitive regex like "tripplite.*"
		 * if so provided by caller */
		upsdebugx(2, "%s: retry matching by regex 'as is'", __func__);
		res = compile_regex(&regex_ptr, subdrv, REG_ICASE | REG_EXTENDED);
		if (res == 0 && regex_ptr != NULL) {
			for (i=0; subdriver_list[i] != NULL; i++) {
				res = match_regex(regex_ptr, subdriver_list[i]->name);
				if (res == 1) {
					free(regex_ptr);
					info = subdriver_list[i];
					goto found;
				}
			}
		}

		if (regex_ptr) {
			free(regex_ptr);
			regex_ptr = NULL;
		}

		/* Then try a case-insensitive regex like "tripplite.*"
		 * with automatically added ".*" */
		len = strlen(subdrv);
		if (
			(len < 3 || (subdrv[len-2] != '.' && subdrv[len-1] != '*'))
			&& len < (LARGEBUF-3)
		) {
			char	buf[LARGEBUF];
			upsdebugx(2, "%s: retry matching by regex with added '.*'", __func__);
			snprintf(buf, sizeof(buf), "%s.*", subdrv);
			res = compile_regex(&regex_ptr, buf, REG_ICASE | REG_EXTENDED);
			if (res == 0 && regex_ptr != NULL) {
				for (i=0; subdriver_list[i] != NULL; i++) {
					res = match_regex(regex_ptr, subdriver_list[i]->name);
					if (res == 1) {
						free(regex_ptr);
						info = subdriver_list[i];
						goto found;
					}
				}
			}

			if (regex_ptr) {
				free(regex_ptr);
				regex_ptr = NULL;
			}
		}
#endif	/* HAVE_LIBREGEX */

		if (fatal_mismatch) {
			fatalx(EXIT_FAILURE,
				"Configuration requested subdriver '%s' but none matched",
				subdrv);
		} else {
			upslogx(LOG_WARNING,
				"Configuration requested subdriver '%s' but none matched; "
				"will try USB matching by other fields",
				subdrv);
		}
	}

	/* No match (and non-fatal mismatch mode), or no
	 * "subdriver" was specified in configuration */
	return NULL;

found:
	upsdebugx(2, "%s: found a match: %s", __func__, info->name);
	if (!getval("vendorid") || !getval("productid")) {
		if (fatal_mismatch) {
			fatalx(EXIT_FAILURE,
				"When specifying a subdriver, "
				"'vendorid' and 'productid' "
				"are mandatory.");
		} else {
			upslogx(LOG_WARNING,
				"When specifying a subdriver, "
				"'vendorid' and 'productid' "
				"are highly recommended.");
		}
	}

	return info;
}

/*!
 * subdriver matcher: only useful for USB mode
 * as SHUT is only supported by MGE UPS SYSTEMS units
 */

#if !((defined SHUT_MODE) && SHUT_MODE)
static int match_function_subdriver(HIDDevice_t *d, void *privdata) {
	int	i;
	NUT_UNUSED_VARIABLE(privdata);

	if (match_function_subdriver_name(1)) {
		/* This driver can handle this device. Guessing so... */
		return 1;
	}

	upsdebugx(2, "%s (non-SHUT mode): matching a device...", __func__);

	for (i=0; subdriver_list[i] != NULL; i++) {
		if (subdriver_list[i]->claim(d)) {
			return 1;
		}
	}

	upsdebugx(2, "%s (non-SHUT mode): failed to match a subdriver "
		"to vendor and/or product ID",
		__func__);
	return 0;
}

static HIDDeviceMatcher_t subdriver_matcher_struct = {
	match_function_subdriver,
	NULL,
	NULL
};
#endif	/* !SHUT_MODE => USB */

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
			"The 'beeper.off' command has been "
			"renamed to 'beeper.disable'");
		return instcmd("beeper.disable", NULL);
	}

	if (!strcasecmp(cmdname, "beeper.on")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.on' command has been "
			"renamed to 'beeper.enable'");
		return instcmd("beeper.enable", NULL);
	}

	upsdebug_INSTCMD_STARTING(cmdname, extradata);

	/* Retrieve and check netvar & item_path */
	hidups_item = find_nut_info(cmdname);

	/* Check for fallback if not found */
	if (hidups_item == NULL) {
		/* Process alias/fallback names */

		upsdebugx(3, "%s: cmdname '%s' not found; "
			"checking for alternatives",
			__func__, cmdname);

		if (!strcasecmp(cmdname, "load.on")) {
			return instcmd("load.on.delay", "0");
		}

		if (!strcasecmp(cmdname, "load.off")) {
			return instcmd("load.off.delay", "0");
		}

		if (!strcasecmp(cmdname, "shutdown.return")) {
			int	ret;

			/* Ensure "ups.start.auto" is set to "yes",
			 * if supported */
			if (dstate_getinfo("ups.start.auto")) {
				setvar("ups.start.auto", "yes");
			}

			ret = instcmd("load.on.delay", dstate_getinfo("ups.delay.start"));
			if (ret != STAT_INSTCMD_HANDLED) {
				return ret;
			}

			/* Some UPS's (e.g. TrippLive AVR750U w/ 3024 protocol) don't accept
			 * commands that arrive too rapidly, so add this arbitary wait,
			 * which has proven to be long enough to avoid this problem in practice */
			usleep(125000);

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

			/* Some UPS's (e.g. TrippLive AVR750U w/ 3024 protocol) don't accept
			 * commands that arrive too rapidly, so add this arbitary wait,
			 * which has proven to be long enough to avoid this problem in practice */
			usleep(125000);

			return instcmd("load.off.delay", dstate_getinfo("ups.delay.shutdown"));
		}

		/* FIXME: ..._UNKNOWN? */
		upsdebugx(2, "instcmd: info element unavailable %s", cmdname);
		upslog_INSTCMD_INVALID(cmdname, extradata);
		return STAT_INSTCMD_INVALID;
	}

	upsdebugx(3, "%s: using Path '%s'",
		__func__,
		(hidups_item->hidpath ? hidups_item->hidpath : "[NULL]")
	);

	/* Check if the item is an instant command */
	if (!(hidups_item->hidflags & HU_TYPE_CMD)) {
		upsdebugx(2, "instcmd: %s is not an instant command", cmdname);
		return STAT_INSTCMD_INVALID;
	}

	/* If extradata is empty, use the default value from the HID-to-NUT table */
	val = extradata ? extradata : hidups_item->dfl;
	if (!val && hidups_item->hidflags & HU_FLAG_PARAM_REQUIRED) {
		upsdebugx(2, "instcmd: %s requires an explicit or default parameter", cmdname);
		return STAT_INSTCMD_CONVERSION_FAILED;
	}

	/* Lookup the new value if needed */
	if (hidups_item->hid2info != NULL) {
		/* item->nuf() is expected to handle NULL if it must */
		value = hu_find_valinfo(hidups_item->hid2info, val);
		if (d_equal(value, -1) && errno == EINVAL) {
			upsdebugx(2, "instcmd: %s does not support parameter %s",
				cmdname, NUT_STRARG(val));
			return STAT_INSTCMD_CONVERSION_FAILED;
		}
	} else {
		if (!val) {
			/* If we end up with atol(NULL) below, it should return
			 * 0 on error anyway (on platforms where it would not
			 * crash instead due to the NULL), so we make it portably
			 * explicit here.
			 */
			/* FIXME: Look up data points (maybe via override.* or
			 * default.* settings) for delay/etc. when handling
			 * commands like shutdown.* or load.* ?
			 */
			upsdebugx(4, "instcmd: %s got no explicit nor default parameter, "
				"but does not require one: falling back to 0", cmdname);
			value = 0;
		} else {
			value = atol(val);
		}
	}

	/* Actual variable setting (as far as firmware is concerned) */
	{	/* scoping + workaround for "error: the address of `argtmp` will always evaluate as `true`" */
		char	argtmp[LARGEBUF], *s = NULL;
		if (snprintf(argtmp, sizeof(argtmp), "%s (%f)",
			NUT_STRARG(extradata), value) > 0)
			s = argtmp;
		upslog_INSTCMD_POWERSTATE_CHECKED(cmdname, s);
	}
	if (HIDSetDataValue(udev, hidups_item->hiddata, value) == 1) {
		upsdebugx(3, "instcmd: SUCCEED");
		/* Set the status so that SEMI_STATIC vars are polled */
		data_has_changed = TRUE;
		return STAT_INSTCMD_HANDLED;
	}

	/* upsdebugx(3, "instcmd: FAILED"); / * FIXME: return HANDLED but FAILED, not UNKNOWN! */
	upslog_INSTCMD_FAILED(cmdname, extradata);
	return STAT_INSTCMD_FAILED;
}

/* set r/w variable to a value. */
int setvar(const char *varname, const char *val)
{
	hid_info_t	*hidups_item;
	double		value;

	upsdebug_SET_STARTING(varname, val);

	/* retrieve and check netvar & item_path */
	hidups_item = find_nut_info(varname);

	if (hidups_item == NULL) {
		upsdebugx(2, "setvar: info element unavailable %s", varname);
		upslog_SET_UNKNOWN(varname, val);
		return STAT_SET_UNKNOWN;
	}

	/* Checking item writability and HID Path */
	if (!(hidups_item->info_flags & ST_FLAG_RW)) {
		upsdebugx(2, "setvar: not writable %s", varname);
		return STAT_SET_UNKNOWN;
	}

	/* handle server side variable */
	if (hidups_item->hidflags & HU_FLAG_ABSENT) {
		upsdebugx(2, "setvar: setting server side variable %s", varname);
		dstate_setinfo(hidups_item->info_type, "%s", val);
		return STAT_SET_HANDLED;
	}

	/* HU_FLAG_ABSENT is the only case of HID Path == NULL */
	if (hidups_item->hidpath == NULL) {
		upsdebugx(2, "setvar: ID Path is NULL for %s", varname);
		return STAT_SET_UNKNOWN;
	}

	/* FIXME: This code did not use "dfl"; should it start to?
	 * If val is empty, use the default value from the HID-to-NUT table */
	/* if (!val) val = hidups_item->dfl; */

	if (!val && hidups_item->hidflags & HU_FLAG_PARAM_REQUIRED) {
		upsdebugx(2, "setvar: %s requires an explicit or default parameter", varname);
		return STAT_SET_CONVERSION_FAILED;
	}

	/* Lookup the new value if needed */
	if (hidups_item->hid2info != NULL) {
		/* item->nuf() is expected to handle NULL if it must */
		value = hu_find_valinfo(hidups_item->hid2info, val);
		if (d_equal(value, -1) && errno == EINVAL) {
			upsdebugx(2, "setvar: %s does not support parameter %s",
				varname, NUT_STRARG(val));
			return STAT_SET_CONVERSION_FAILED;
		}
	} else {
		if (!val) {
			/* If we end up with atol(NULL) below, it should return
			 * 0 on error anyway (on platforms where it would not
			 * crash instead due to the NULL), so we make it portably
			 * explicit here.
			 */
			upsdebugx(4, "setvar: %s got no explicit nor default parameter, "
				"but does not require one: falling back to 0", varname);
			value = 0;
		} else {
			value = atol(val);
		}
	}

	/* Actual variable setting */
	if (HIDSetDataValue(udev, hidups_item->hiddata, value) == 1) {
		upsdebugx(5, "setvar: SUCCEED");
		/* Set the status so that SEMI_STATIC vars are polled */
		data_has_changed = TRUE;
		return STAT_SET_HANDLED;
	}

	/* upsdebugx(3, "setvar: FAILED"); / * FIXME: return HANDLED but FAILED, not UNKNOWN! */
	upslog_SET_FAILED(varname, val);
	return STAT_SET_UNKNOWN;
}

void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	char	*cmd_used = NULL;

	upsdebugx(1, "%s...", __func__);

	/* By default:
	 * - Try to shutdown with delay
	 * - If the above doesn't work, try shutdown.reboot
	 * - If the above doesn't work, try load.off.delay
	 * - Finally, try shutdown.stayoff
	 */
	if (do_loop_shutdown_commands("shutdown.return,shutdown.reboot,load.off.delay,shutdown.stayoff", &cmd_used) == STAT_INSTCMD_HANDLED) {
		upslogx(LOG_INFO, "Shutdown successful with '%s'", NUT_STRARG(cmd_used));
		if (handling_upsdrv_shutdown > 0)
			set_exit_flag(EF_EXIT_SUCCESS);
		return;
	}

	upslogx(LOG_ERR, "Shutdown failed!");
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(EF_EXIT_FAILURE);
}

void upsdrv_help(void)
{
	size_t i;
	printf("\nAcceptable values for 'subdriver' via -x or ups.conf "
		"in this driver (exact names here, case-insensitive "
		"sub-strings may be used, as well as regular expressions): ");

	for (i = 0; subdriver_list[i] != NULL; i++) {
		if (i>0)
			printf(", ");
		printf("\"%s\"", subdriver_list[i]->name);
	}
	printf("\n");
}

void upsdrv_makevartable(void)
{
	char temp [MAX_STRING_SIZE];

	upsdebugx(1, "upsdrv_makevartable...");

	snprintf(temp, sizeof(temp),
		"Set low battery level, in %% (default=%s)",
		DEFAULT_LOWBATT);
	addvar (VAR_VALUE, HU_VAR_LOWBATT, temp);

	snprintf(temp, sizeof(temp),
		"Set shutdown delay, in seconds (default=%s, or %s for CPS devices)",
		DEFAULT_OFFDELAY, DEFAULT_OFFDELAY_CPS);
	addvar(VAR_VALUE, HU_VAR_OFFDELAY, temp);

	snprintf(temp, sizeof(temp),
		"Set startup delay, in seconds (default=%s, or %s for CPS devices)",
		DEFAULT_ONDELAY, DEFAULT_ONDELAY_CPS);
	addvar(VAR_VALUE, HU_VAR_ONDELAY, temp);

	snprintf(temp, sizeof(temp),
		"Set polling frequency, in seconds, to reduce data flow "
		"(default=%d, or %d for CPS devices)",
		DEFAULT_POLLFREQ, DEFAULT_POLLFREQ_CPS);
	addvar(VAR_VALUE, HU_VAR_POLLFREQ, temp);

	addvar(VAR_FLAG, "pollonly", "Don't use interrupt pipe, only use polling (recommended for CPS devices)");

	addvar(VAR_VALUE, "interrupt_pipe_no_events_tolerance", "How many times in a row do we tolerate \"Got 0 HID objects\" from USB interrupts?");

	addvar(VAR_FLAG, "onlinedischarge",
		"Set to treat discharging while online as being offline/on-battery (DEPRECATED, use onlinedischarge_onbattery)");

	addvar(VAR_FLAG, "onlinedischarge_onbattery",
		"Set to treat discharging while online as being offline/on-battery");

	addvar(VAR_FLAG, "onlinedischarge_calibration",
		"Set to treat discharging while online as doing calibration");

	addvar(VAR_VALUE, "onlinedischarge_log_throttle_sec",
		"Set to throttle log messages about discharging while online (only so often)");

	addvar(VAR_VALUE, "onlinedischarge_log_throttle_hovercharge",
		"Set to throttle log messages about discharging while online (only if battery.charge is under this value)");

	addvar(VAR_VALUE, "lbrb_log_delay_sec",
		"Set to delay status-setting (and log messages) about device in LB or LB+RB state");

	addvar(VAR_FLAG, "lbrb_log_delay_without_calibrating",
		"Set to apply lbrb_log_delay_sec even if device is not calibrating");

	addvar(VAR_FLAG, "disable_fix_report_desc",
		"Set to disable fix-ups for broken USB encoding, etc. which we apply by default on certain vendors/products");

	addvar(VAR_FLAG, "powercom_sdcmd_byte_order_fallback",
		"Set to use legacy byte order for Powercom HID shutdown commands. Either it was wrong forever, or some older devices/firmwares had it the other way around");

#if !((defined SHUT_MODE) && SHUT_MODE)
	addvar(VAR_VALUE, "subdriver", "Explicit USB HID subdriver selection");

	/* allow -x vendor=X, vendorid=X, product=X, productid=X, serial=X */
	nut_usb_addvars();

	addvar(VAR_FLAG, "explore",
		"Diagnostic matching of unsupported UPS");
	addvar(VAR_FLAG, "maxreport",
		"Activate tweak for buggy APC Back-UPS firmware");
	addvar(VAR_FLAG, "interruptonly",
		"Don't use polling, only use interrupt pipe");
	addvar(VAR_VALUE, "interruptsize",
		"Number of bytes to read from interrupt pipe");
	addvar(VAR_VALUE, HU_VAR_WAITBEFORERECONNECT,
		"Seconds to wait before trying to reconnect");

#else	/* SHUT_MODE */
	addvar(VAR_VALUE, "notification",
		"Set notification type (ignored, only for backward compatibility)");
#endif	/* SHUT_MODE / USB */
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
		if (now < (lastpoll + poll_interval)) {
			return;
		}

		upsdebugx(1, "Got to reconnect!");
		if (use_interrupt_pipe == TRUE && interrupt_pipe_EIO_count > 0) {
			upsdebugx(0, "Reconnecting. If you saw \"nut_libusb_get_interrupt: Input/Output Error\" "
				"or similar message in the log above, try setting \"pollonly\" flag in \"ups.conf\" "
				"options section for this driver!");
		}

		if (!reconnect_ups()) {
			lastpoll = now;
			dstate_datastale();
			return;
		}

		hd = &curDevice;
		interrupt_pipe_EIO_count = 0;
		interrupt_pipe_no_events_count = 0;

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
		case LIBUSB_ERROR_BUSY:      /* Device or resource busy */
			upslog_with_errno(LOG_CRIT, "Got disconnected by another driver");
			goto fallthrough_reconnect;
		case NUT_LIBUSB_CODE_NO_EVENTS:	/* No HID Events */
			interrupt_pipe_no_events_count++;
			upsdebugx(1, "Got 0 HID objects (%ld times in a row, tolerance is %ld)...",
				interrupt_pipe_no_events_count, interrupt_pipe_no_events_tolerance);
			if (interrupt_pipe_no_events_tolerance >= 0
			 && interrupt_pipe_no_events_tolerance < interrupt_pipe_no_events_count
			) {
				goto fallthrough_reconnect;
			}
			break;
#if WITH_LIBUSB_0_1 /* limit to libusb 0.1 implementation */
		case -EPERM:		/* Operation not permitted */
#endif
		case LIBUSB_ERROR_NO_DEVICE: /* No such device */
		case LIBUSB_ERROR_ACCESS:    /* Permission denied */
#if WITH_LIBUSB_0_1         /* limit to libusb 0.1 implementation */
		case -ENXIO:		    /* No such device or address */
#endif
		case LIBUSB_ERROR_NOT_FOUND: /* No such file or directory */
		case LIBUSB_ERROR_NO_MEM:    /* Insufficient memory */
		fallthrough_reconnect:
			/* Uh oh, got to reconnect! */
			dstate_setinfo("driver.state", "reconnect.trying");
			hd = NULL;
			return;
		case LIBUSB_ERROR_IO:        /* I/O error */
			/* Uh oh, got to reconnect, with a special suggestion! */
			dstate_setinfo("driver.state", "reconnect.trying");
			interrupt_pipe_EIO_count++;
			hd = NULL;
			return;
		default:
			upsdebugx(1, "Got %i HID objects...", (evtCount >= 0) ? evtCount : 0);
			if (evtCount > 0)
				interrupt_pipe_no_events_count = 0;
			else
				upsdebugx(1, "Got unhandled result from HIDGetEvents(): %i\n"
					"Please report it to NUT developers, with an 'upsc' output for your device,\n"
					"versions of NUT and libusb used, and verbose driver debug log if possible.",
					evtCount);
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
			upsdebugx(2,
				"Path: %s, Type: %s, ReportID: 0x%02x, "
				"Offset: %i, Size: %i, Value: %g",
				HIDGetDataItem(event[i], subdriver->utab),
				HIDDataType(event[i]), event[i]->ReportID,
				event[i]->Offset, event[i]->Size, value);
		}

		/* Skip Input reports, if we don't use the Feature report */
		found_data = FindObject_with_Path(pDesc, &(event[i]->Path), interrupt_only ? ITEM_INPUT:ITEM_FEATURE);
		if (!found_data && !interrupt_only) {
			found_data = FindObject_with_Path(pDesc, &(event[i]->Path), ITEM_INPUT);
		}
		if (!found_data) {
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
	upsdebugx(1, "took %.3f seconds handling interrupt reports...",
		interval());
#endif
	/* clear status buffer before beginning */
	status_init();
	buzzmode_init();

	/* Do a full update (polling) every pollfreq
	 * or upon data change (ie setvar/instcmd) */
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
	buzzmode_commit();
	status_commit();

	dstate_dataok();
#ifdef DEBUG
	upsdebugx(1, "took %.3f seconds handling feature reports...",
		interval());
#endif
}

void upsdrv_initinfo(void)
{
	char	*val;

	upsdebugx(1, "upsdrv_initinfo...");

	dstate_setinfo("driver.version.data", "%s", subdriver->name);

	/* init polling frequency for full updates */
	val = getval(HU_VAR_POLLFREQ);
	if (val) {
		pollfreq = atoi(val);
#if !((defined SHUT_MODE) && SHUT_MODE)
	} else {
		/* note, there is also 'pollinterval'/poll_interval (C var)
		 * common delay between main.c loops */
		if (subdriver == &cps_subdriver) {
			upslogx(LOG_INFO, "Defaulting '%s' to %d for CPS devices",
				HU_VAR_POLLFREQ, DEFAULT_POLLFREQ_CPS);
			pollfreq = DEFAULT_POLLFREQ_CPS;
		}
#endif
	}

	dstate_setinfo("driver.parameter.pollfreq", "%d", pollfreq);

	/* ignore (broken) interrupt pipe */
	if (testvar("pollonly")) {
		use_interrupt_pipe = FALSE;
#if !((defined SHUT_MODE) && SHUT_MODE)
	} else {
		if (subdriver == &cps_subdriver) {
			upslogx(LOG_WARNING, "You may want to set 'pollonly' "
				"flag on CPS devices");
		}
#endif
	}

	val = getval("interrupt_pipe_no_events_tolerance");
	if (!val || !str_to_long(val, &interrupt_pipe_no_events_tolerance, 10)) {
		interrupt_pipe_no_events_tolerance = -1;
		if (val)
			upslogx(LOG_WARNING, "Invalid setting for interrupt_pipe_no_events_tolerance: '%s', defaulting to %ld",
				val, interrupt_pipe_no_events_tolerance);
	}
	dstate_setinfo("driver.parameter.interrupt_pipe_no_events_tolerance", "%ld", interrupt_pipe_no_events_tolerance);

	time(&lastpoll);

	/* install handlers */
	upsh.setvar = setvar;
	upsh.instcmd = instcmd;
}

void upsdrv_initups(void)
{
	int ret;
	char *val;

#if (defined SHUT_MODE) && SHUT_MODE
	/*!
	 * SHUT is a serial protocol, so it needs
	 * only the device path
	 */
	upsdebugx(1, "upsdrv_initups (SHUT)...");

	subdriver_matcher = device_path;
#else	/* !SHUT_MODE => USB */
	char *regex_array[USBMATCHER_REGEXP_ARRAY_LIMIT];

	upsdebugx(1, "upsdrv_initups (non-SHUT)...");

	upsdebugx(2, "Initializing an USB-connected UPS with library %s " \
		"(NUT subdriver name='%s' ver='%s')",
		dstate_getinfo("driver.version.usb"),
		comm_driver->name, comm_driver->version );

	warn_if_bad_usb_port_filename(device_path);

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
	regex_array[6] = getval("device");
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	regex_array[7] = getval("busport");
#else
	if (getval("busport")) {
		upslogx(LOG_WARNING, "\"busport\" is configured for the device, but is not actually handled by current build combination of NUT and libusb (ignored)");
	}
#endif

	ret = USBNewRegexMatcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
	switch(ret)
	{
	case 0:
		break;
	case -1:
		fatal_with_errno(EXIT_FAILURE, "HIDNewRegexMatcher()");
#ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);
		/* Should not get here in practice, but
		 * compiler is afraid we can fall through */
#endif
	default:
		fatalx(EXIT_FAILURE, "invalid regular expression: %s", regex_array[ret]);
#ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);
		/* Should not get here in practice, but
		 * compiler is afraid we can fall through */
#endif
	}

	/* link the matchers */
	subdriver_matcher->next = regex_matcher;
#endif /* SHUT_MODE / USB */

	/* First activate the few tweaks which can impact device detection */

	/* Activate Powercom tweaks */
	if (testvar("interruptonly")) {
		interrupt_only = 1;
	}

	val = getval("interruptsize");
	if (val) {
		int ipv = atoi(val);
		if (ipv > 0) {
			interrupt_size = (unsigned int)ipv;
		} else {
			fatalx(EXIT_FAILURE, "Error: invalid interruptsize: %d", ipv);
		}
	}

	if (testvar("disable_fix_report_desc")) {
		disable_fix_report_desc = 1;
	}

	/* Search for the first supported UPS matching the
	   regular expression (USB) or device_path (SHUT) */
	ret = comm_driver->open_dev(&udev, &curDevice, subdriver_matcher, &callback);
	if (ret < 1)
		fatalx(EXIT_FAILURE, "No matching HID UPS found");

	hd = &curDevice;

	upsdebugx(1, "Detected a UPS: %s/%s",
		hd->Vendor ? hd->Vendor : "unknown",
		hd->Product ? hd->Product : "unknown");

	/* Later activate the relatively cosmetic tweaks */

	/* Activate Cyberpower/APC tweaks */
	if (testvar("onlinedischarge") || testvar("onlinedischarge_onbattery")) {
		onlinedischarge_onbattery = 1;
	}

	if (testvar("onlinedischarge_calibration")) {
		onlinedischarge_calibration = 1;
	}

	val = getval("onlinedischarge_log_throttle_sec");
	if (val) {
		int ipv = atoi(val);
		if (ipv == 0 && strcmp("0", val)) {
			onlinedischarge_log_throttle_sec = 30;
			upslogx(LOG_WARNING,
				"Warning: invalid value for "
				"onlinedischarge_log_throttle_sec: %s, "
				"defaulting to %d",
				val, onlinedischarge_log_throttle_sec);
		} else {
			if (ipv < 0) {
				/* Has a specific meaning: user said be quiet */
				onlinedischarge_log_throttle_sec = -1;
			} else {
				onlinedischarge_log_throttle_sec = ipv;
			}
		}
	}

	val = getval("onlinedischarge_log_throttle_hovercharge");
	if (val) {
		int ipv = atoi(val);
		if (ipv < 1 || ipv > 100) {
			onlinedischarge_log_throttle_hovercharge = 100;
			upslogx(LOG_WARNING,
				"Warning: invalid value for "
				"onlinedischarge_log_throttle_hovercharge: %s, "
				"defaulting to %d",
				val, onlinedischarge_log_throttle_hovercharge);
		} else {
			onlinedischarge_log_throttle_hovercharge = ipv;
		}
	}

	val = getval("lbrb_log_delay_sec");
	if (val) {
		int ipv = atoi(val);
		if ((ipv == 0 && strcmp("0", val)) || (ipv < 0)) {
			lbrb_log_delay_sec = 3;
			upslogx(LOG_WARNING,
				"Warning: invalid value for "
				"lbrb_log_delay_sec: %s, "
				"defaulting to %d",
				val, lbrb_log_delay_sec);
		} else {
			lbrb_log_delay_sec = ipv;
		}
	} else {
		/* Activate APC BXnnnMI/BXnnnnMI/BVKnnnM2/BVKnnnnM2/BKnnnM2[_-]CH tweaks, for details see
		 * https://github.com/networkupstools/nut/issues/2347
		 * https://github.com/networkupstools/nut/issues/2565
		 * https://github.com/networkupstools/nut/issues/2944
		 * https://github.com/networkupstools/nut/issues/3006
		 */
		size_t	productLen = hd->Product ? strlen(hd->Product) : 0;

		/* FIXME: Consider also ups.mfr.date as 2023 or newer?
		 * Eventually up to some year this gets fixed?
		 */
		if (hd->Vendor && productLen > 0
		&& (!strcmp(hd->Vendor, "APC") || !strcmp(hd->Vendor, "American Power Conversion"))
		&& ((
			    productLen > 6	/* BXnnnMI at least */
			&&  (strstr(hd->Product, " BX") || strstr(hd->Product, "BX") == hd->Product)
			&&   hd->Product[productLen - 2] == 'M' && hd->Product[productLen - 1] == 'I'
			) || (
			    productLen > 7	/* BVKnnnM2 at least */
			&&  (strstr(hd->Product, " BVK") || strstr(hd->Product, "BVK") == hd->Product)
			&&   hd->Product[productLen - 2] == 'M' && hd->Product[productLen - 1] == '2'
			) || (
			    productLen > 9	/* BKnnnM2-CH at least, e.h. "Back-UPS BK650M2_CH" */
			&&  (strstr(hd->Product, " BK") || strstr(hd->Product, "BK") == hd->Product)
			&&   hd->Product[productLen - 5] == 'M' && hd->Product[productLen - 4] == '2'
			&&  (hd->Product[productLen - 3] == '-' || hd->Product[productLen - 3] == '_')
			&&   hd->Product[productLen - 2] == 'C' && hd->Product[productLen - 1] == 'H'
			)
		)) {
			int	got_lbrb_log_delay_without_calibrating = testvar("lbrb_log_delay_without_calibrating") ? 1 : 0,
				got_onlinedischarge_calibration = testvar("onlinedischarge_calibration") ? 1 : 0,
				got_onlinedischarge_log_throttle_sec = testvar("onlinedischarge_log_throttle_sec") ? 1 : 0;

			lbrb_log_delay_sec = 3;

			upslogx(LOG_INFO, "Defaulting lbrb_log_delay_sec=%d "
				"for %s model %s%s%s%s%s%s%s%s%s%s",
				lbrb_log_delay_sec,
				hd->Vendor, hd->Product,

				!got_lbrb_log_delay_without_calibrating
				|| !got_onlinedischarge_calibration
				|| !got_onlinedischarge_log_throttle_sec
				? "; consider also setting the " : "",

				!got_lbrb_log_delay_without_calibrating
				? "lbrb_log_delay_without_calibrating " : "",

				!got_lbrb_log_delay_without_calibrating
				&& (!got_onlinedischarge_calibration
				 || !got_onlinedischarge_log_throttle_sec)
				? "and/or " : "",

				!got_onlinedischarge_calibration
				? "onlinedischarge_calibration " : "",

				(!got_lbrb_log_delay_without_calibrating
				|| !got_onlinedischarge_calibration )
				&& !got_onlinedischarge_log_throttle_sec
				? "and/or " : "",

				!got_onlinedischarge_log_throttle_sec
				? "onlinedischarge_log_throttle_sec " : "",

				!got_lbrb_log_delay_without_calibrating
				|| !got_onlinedischarge_calibration
				|| !got_onlinedischarge_log_throttle_sec
				? "flag" : "",

				2 > ( got_lbrb_log_delay_without_calibrating
				    + got_onlinedischarge_calibration
				    + got_onlinedischarge_log_throttle_sec)
				? "(s)" : "",

				!got_lbrb_log_delay_without_calibrating
				|| !got_onlinedischarge_calibration
				|| !got_onlinedischarge_log_throttle_sec
				? " in your configuration" : "");
		}
	}

	if (testvar("lbrb_log_delay_without_calibrating")) {
		lbrb_log_delay_without_calibrating = 1;
	}

	upsdebugx(1, "%s: Performing an initial UPS data walk with subdriver %s...",
		__func__, subdriver->name);
	if (hid_ups_walk(HU_WALKMODE_INIT) == FALSE) {
		fatalx(EXIT_FAILURE, "Can't initialize data from HID UPS");
	} else {
		analyze_mapping_usage();
	}

	if (!ups_status)
		upslogx(LOG_WARNING, "%s: No flag bits for 'ups.status' were explicitly reported; "
			"it is possible a wrong 'subdriver' option was requested or detected "
			"(in case of problems with device data, consider testing with other "
			"explicit driver option 'subdriver' values)",
			__func__);

	upsdebugx(1, "%s: Optionally adjust some threshold values, if applicable and requested to...", __func__);

	/* Set values below from user settings only if supported by UPS */
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
			long	l = strtol(val, NULL, 10);
#if !((defined SHUT_MODE) && SHUT_MODE)
			if (subdriver == &cps_subdriver
			 && (l < 60 || l % 60)
			) {
				upslogx(LOG_WARNING, "CPS devices tend to round delays by 60 sec down (ondelay=120 is the suggested minimum; see more in the man page)");
			}
#endif
			dstate_setinfo("ups.delay.start", "%ld", l);
		}
	}

	if (dstate_getinfo("ups.delay.shutdown")) {
		/* Retrieve user defined delay settings */
		val = getval(HU_VAR_OFFDELAY);
		if (val) {
			long	l = strtol(val, NULL, 10);
#if !((defined SHUT_MODE) && SHUT_MODE)
			if (subdriver == &cps_subdriver
			 && (l > 0 && (l < 60 || l % 60))
			) {
				/* Note: zero and negative values may
				 * have special meanings for the firmware */
				upslogx(LOG_WARNING, "CPS devices tend to round delays by 60 sec down (offdelay=60 is the suggested minimum; see more in the man page)");
			}
#endif
			dstate_setinfo("ups.delay.shutdown", "%ld", l);
		}
	}

	upsdebugx(1, "%s: Optionally enable instant commands related to shutdown, if applicable...", __func__);

	/* Enable instant commands below only if supported by UPS */
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

	upsdebugx(1, "%s: finished", __func__);
}

void upsdrv_cleanup(void)
{
	upsdebugx(1, "upsdrv_cleanup...");

	comm_driver->close_dev(udev);
	Free_ReportDesc(pDesc);
	free_report_buffer(reportbuf);
#if !((defined SHUT_MODE) && SHUT_MODE)
	USBFreeExactMatcher(exact_matcher);
	USBFreeRegexMatcher(regex_matcher);

	free(curDevice.Vendor);
	free(curDevice.Product);
	free(curDevice.Serial);
	free(curDevice.Bus);
	free(curDevice.Device);
# if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	free(curDevice.BusPort);
# endif
#endif	/* !SHUT_MODE => USB */
}

/**********************************************************************
 * Support functions
 *********************************************************************/

void possibly_supported(const char *mfr, HIDDevice_t *arghd)
{
	upsdebugx(0,
"This %s device (%04x:%04x) is not (or perhaps not yet) supported\n"
"by usbhid-ups. Please make sure you have an up-to-date version of NUT.\n"
"If this does not fix the problem, try running the driver with the\n"
"'-x productid=%04x' option, or iterate with explicit '-x subdriver=...'\n"
"option. Please report your results to the NUT user's mailing list\n"
"at <nut-upsuser@lists.alioth.debian.org>.\n",
	mfr, arghd->VendorID, arghd->ProductID, arghd->ProductID);

	if (arghd->VendorID == 0x06da) {
		upsdebugx(0,
"Please note that this Vendor ID is also known in devices supported by nutdrv_qx");
	}
}

/* Update ups_status to remember this status item. Interpretation is
   done in ups_status_set(). */
static void process_boolean_info(const char *nutvalue)
{
	status_lkp_t *status_item;
	int clear = 0;

	upsdebugx(5, "process_boolean_info: %s", nutvalue);

	/* Only neuter the other if we know the opposite to be true */
	if (!strcmp(nutvalue, "online"))
		process_boolean_info("!offline");
	else if (!strcmp(nutvalue, "offline"))
		process_boolean_info("!online");

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

static int callback(
	hid_dev_handle_t argudev,
	HIDDevice_t *arghd,
	usb_ctrl_charbuf rdbuf,
	usb_ctrl_charbufsize rdlen)
{
	int i;
	const char *mfr = NULL, *model = NULL, *serial = NULL;
#if !((defined SHUT_MODE) && SHUT_MODE)
	int ret;
#endif	/* !SHUT_MODE => USB */
	upsdebugx(2, "Report Descriptor size = %" PRI_NUT_USB_CTRL_CHARBUFSIZE, rdlen);

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE
# pragma GCC diagnostic ignored "-Wtautological-unsigned-zero-compare"
#endif
	if ((uintmax_t)rdlen < (uintmax_t)SIZE_MAX) {
		upsdebug_hex(3, "Report Descriptor", rdbuf, (size_t)rdlen);
	}
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TYPE_LIMITS) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_CONSTANT_OUT_OF_RANGE_COMPARE) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_TAUTOLOGICAL_UNSIGNED_ZERO_COMPARE) )
# pragma GCC diagnostic pop
#endif

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
	subdriver = match_function_subdriver_name(0);
	if (!subdriver) {
		for (i=0; subdriver_list[i] != NULL; i++) {
			if (subdriver_list[i]->claim(hd)) {
				break;
			}
		}

		subdriver = subdriver_list[i];
	}

	if (!subdriver) {
		upsdebugx(1, "Manufacturer or model not supported!");
		return 0;
	}

	upslogx(LOG_INFO, "Using subdriver: %s", subdriver->name);

	if (subdriver->fix_report_desc(arghd, pDesc)) {
		upsdebugx(2, "Report Descriptor Fixed");
	}
	upsdebugx(1, "%s: calling HIDDumpTree(); in case of problems with device data "
		"please note that a wrong subdriver could have been chosen above; "
		"consider testing others with an explicit driver option",
		__func__);
	HIDDumpTree(udev, arghd, subdriver->utab);

#if !((defined SHUT_MODE) && SHUT_MODE)
	/* create a new matcher for later matching */
	USBFreeExactMatcher(exact_matcher);
	ret = USBNewExactMatcher(&exact_matcher, hd);
	if (ret) {
		upsdebug_with_errno(1, "USBNewExactMatcher()");
		return 0;
	}

	regex_matcher->next = exact_matcher;
#endif	/* !SHUT_MODE => USB */

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

/* default subdriver function which doesn't attempt to fix
 * any issues in the parsed HID Report Descriptor */
int fix_report_desc(HIDDevice_t *arg_pDev, HIDDesc_t *arg_pDesc) {
	NUT_UNUSED_VARIABLE(arg_pDev);
	NUT_UNUSED_VARIABLE(arg_pDesc);

/* Implementations should honor the user's toggle:
 *	if (disable_fix_report_desc) return 0;
 */
	return 0;
}

/* walk ups variables and set elements of the info array. */
static bool_t hid_ups_walk(walkmode_t mode)
{
	hid_info_t	*item;
	double		value;
	int		retcode;

#if !((defined SHUT_MODE) && SHUT_MODE)
	/* extract the VendorId for further testing */
	int vendorID = curDevice.VendorID;
	int productID = curDevice.ProductID;
#endif	/* !SHUT_MODE => USB */

	/* 3 modes: HU_WALKMODE_INIT, HU_WALKMODE_QUICK_UPDATE
	 * and HU_WALKMODE_FULL_UPDATE */

	/* Device data walk ----------------------------- */
	for (item = subdriver->hid2nut; item->info_type != NULL; item++) {

#if (defined SHUT_MODE) && SHUT_MODE
		/* Check if we are asked to stop (reactivity++) in SHUT mode.
		 * In USB mode, looping through this takes well under a second,
		 * so any effort to improve reactivity here is wasted. */
		if (exit_flag != 0)
			return TRUE;
#endif	/* SHUT_MODE */

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

			/* Sanity-check that if we setvar() via mapping table
			 * which refers us to a method, that such method entry
			 * is not NULL (then we might get ERR READONLY later)
			 */
			if (ST_FLAG_RW && item->hid2info) {
				if (
					(item->hid2info->fun && !(item->hid2info->nuf) && (item->info_flags & ST_FLAG_RW))
				||	(item->hid2info->nuf && !(item->hid2info->fun))
				) {
					/* Only one of these pointers is set - expecting
					 * iteration for the other values until a sentinel
					 * entry which then must exist; is it right there
					 * in the next table line? */
					if (item->hid2info[1].nut_value == NULL) {
						/* The first line of that table is the
						 * only meaningful one */
						upsdebugx(1, "%s: WARNING: values for '%s' are handled with a mapping table "
							"which only defines one dynamic method and can not let us %s "
							"(or, technically it would map one value '%s'='%ld')."
							"This may be a bug - please raise an issue with NUT maintainers "
							"if this proves to be a practical problem with that data point.",
							__func__, NUT_STRARG(item->info_type),
							(item->hid2info->fun ? "write (setvar)" : "read"),
							NUT_STRARG(item->hid2info->nut_value),
							item->hid2info->hid_value
						);
					}
				}
			}

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
			if (item->hidflags & (HU_FLAG_ABSENT | HU_TYPE_CMD))
				continue;

			/* These don't need polling after initinfo() normally
			 * However in "pollonly" mode we use these to detect "Data stale"
			 * condition (e.g. cable disconnected) by failing the reads:
			 */
			if ((item->hidflags & HU_FLAG_STATIC) && use_interrupt_pipe)
				continue;

			/* These need to be polled after user changes (setvar / instcmd)
			 * or to detect "Data stale" in "pollonly" mode
			 */
			if (   (item->hidflags & HU_FLAG_SEMI_STATIC)
				&& (data_has_changed == FALSE)
				&& use_interrupt_pipe
			)
				continue;

			break;

#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wcovered-switch-default"
# pragma clang diagnostic ignored "-Wunreachable-code"
#endif
			/* All enum cases defined as of the time of coding
			 * have been covered above. Handle later definitions,
			 * memory corruptions and buggy inputs below...
			 */
		default:
			fatalx(EXIT_FAILURE, "hid_ups_walk: unknown update mode!");
		}	/* end of: switch(mode) */
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif

#if !((defined SHUT_MODE) && SHUT_MODE)
		/* skip report 0x54 for Tripplite SU3000LCD2UHV due to firmware bug */
		if ((vendorID == 0x09ae) && (productID == 0x1330)) {
			if (item->hiddata && (item->hiddata->ReportID == 0x54)) {
				continue;
			}
		}
#endif	/* !SHUT_MODE => USB */

		retcode = HIDGetDataValue(udev, item->hiddata, &value, poll_interval);

		switch (retcode)
		{
		case LIBUSB_ERROR_BUSY:      /* Device or resource busy */
			upslog_with_errno(LOG_CRIT, "Got disconnected by another driver");
			goto fallthrough_reconnect;

#if WITH_LIBUSB_0_1 /* limit to libusb 0.1 implementation */
		case -EPERM:		/* Operation not permitted */
#endif
		case LIBUSB_ERROR_NO_DEVICE: /* No such device */
		case LIBUSB_ERROR_ACCESS:    /* Permission denied */
#if WITH_LIBUSB_0_1           /* limit to libusb 0.1 implementation */
		case -ENXIO:		  /* No such device or address */
#endif
		case LIBUSB_ERROR_NOT_FOUND: /* No such file or directory */
		case LIBUSB_ERROR_NO_MEM:    /* Insufficient memory */
		fallthrough_reconnect:
			/* Uh oh, got to reconnect! */
			dstate_setinfo("driver.state", "reconnect.trying");
			hd = NULL;
			return FALSE;

		case LIBUSB_ERROR_IO:        /* I/O error */
			/* Uh oh, got to reconnect, with a special suggestion! */
			dstate_setinfo("driver.state", "reconnect.trying");
			interrupt_pipe_EIO_count++;
			hd = NULL;
			return FALSE;

		case 1:
			break;	/* Found! */

		case 0:
			continue;

		case LIBUSB_ERROR_TIMEOUT:   /* Connection timed out */
/* libusb win32 does not know EPROTO and EOVERFLOW,
 * it only returns EIO for any IO errors */
#ifndef WIN32
		case LIBUSB_ERROR_OVERFLOW:  /* Value too large for defined data type */
# if EPROTO && WITH_LIBUSB_0_1
		case -EPROTO:		/* Protocol error */
# endif
#endif	/* !WIN32 */
		case LIBUSB_ERROR_PIPE:      /* Broken pipe */
		default:
			/* Don't know what happened, try again later... */
			upsdebugx(1, "HIDGetDataValue unknown retcode '%i'", retcode);
			continue;
		}

		upsdebugx(2,
			"Path: %s, Type: %s, ReportID: 0x%02x, "
			"Offset: %i, Size: %i, Value: %g",
			item->hidpath, HIDDataType(item->hiddata),
			item->hiddata->ReportID,
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

		if (mode == HU_WALKMODE_INIT || (!use_interrupt_pipe)) {
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
			for (
				info_lkp = item->hid2info;
				info_lkp != NULL && info_lkp->nut_value != NULL;
				info_lkp++
			) {
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
	char	*val;
	int wait_before_reconnect = 0;

	dstate_setinfo("driver.state", "reconnect.trying");

	/* Init time to wait before trying to reconnect (seconds) */
	val = getval(HU_VAR_WAITBEFORERECONNECT);
	if (val) {
		wait_before_reconnect = atoi(val);
	}

	/* Try to close the previous handle */
	if (udev == HID_DEV_HANDLE_CLOSED) {
		upsdebugx(4, "Not closing comm_driver previous handle: already closed");
	} else {
		upsdebugx(4, "Closing comm_driver previous handle");
		comm_driver->close_dev(udev);
		udev = HID_DEV_HANDLE_CLOSED;
	}

	upsdebugx(4, "===================================================================");
	if (wait_before_reconnect > 0 ) {
		upsdebugx(4, " device has been disconnected, trying to reconnect in %i seconds", wait_before_reconnect);
		sleep(wait_before_reconnect);
		upsdebugx(4, " trying to reconnect");
	} else {
		upsdebugx(4, " device has been disconnected, try to reconnect");
	}
	upsdebugx(4, "===================================================================");

	upsdebugx(4, "Opening comm_driver ...");
	ret = comm_driver->open_dev(&udev, &curDevice, subdriver_matcher, NULL);
	upsdebugx(4, "Opening comm_driver returns ret=%i", ret);
	if (ret > 0) {
		return 1;
	}

	dstate_setinfo("driver.state", "quiet");
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
unsigned ups_status_get(void)
{
	return ups_status;
}

/** Helper to both status_set("CAL") and track last_calibration_start timestamp */
static void status_set_CAL(void)
{
	/* Note: dstate tokens can only be set, not cleared; a
	 * dstate_init() wipes the whole internal buffer though. */
	int	wasSet = status_get("CAL");
	time_t	now;

	time(&now);

	/* A few sanity checks */
	if (wasSet) {
		if (!last_calibration_start) {
			upsdebugx(2, "%s: status was already set but not time-stamped: CAL", __func__);
		} else {
			upsdebugx(2, "%s: status was already set %f sec ago : CAL",
				__func__, difftime(now, last_calibration_start));
		}
	} else {
		if (last_calibration_finish) {
			upsdebugx(2, "%s: starting a new calibration, last one finished %f sec ago",
				__func__, difftime(now, last_calibration_finish));
		} else {
			upsdebugx(2, "%s: starting a new calibration, first in this driver's lifetime",
				__func__);
		}
	}

	if (!last_calibration_start) {
		last_calibration_start = now;
	}

	if (!wasSet) {
		status_set("CAL");		/* calibration */
	}
}

/* Convert the local status information to NUT format and set NUT
   status. */
static void ups_status_set(void)
{
	int	isCalibrating = 0;

	if (ups_status & STATUS(VRANGE)) {
		dstate_setinfo("input.transfer.reason", "input voltage out of range");
	} else if (ups_status & STATUS(FRANGE)) {
		dstate_setinfo("input.transfer.reason", "input frequency out of range");
	} else {
		dstate_delinfo("input.transfer.reason");
	}

	/* Report calibration mode first, because it looks like OFF or OB
	 * for many implementations (literally, it is a temporary OB state
	 * managed by the UPS hardware to become OL later... if it guesses
	 * correctly when to do so), and may cause false alarms for us to
	 * raise FSD urgently. So we first let upsmon know it is just a drill.
	 */
	if (ups_status & STATUS(CALIB)) {
		status_set_CAL();		/* calibration */
	}

	if ((!(ups_status & STATUS(DISCHRG))) && (
		onlinedischarge_log_throttle_timestamp != 0
		|| onlinedischarge_log_throttle_charge != -1
	)) {
		upsdebugx(1,
			"%s: seems that UPS [%s] was in OL+DISCHRG state "
			"previously, but no is longer discharging now.",
			__func__, upsname);
		onlinedischarge_log_throttle_timestamp = 0;
		onlinedischarge_log_throttle_charge = -1;
	}

	if ((ups_status & STATUS(OFFLINE))) {
		status_set("OB");		/* on battery */
		if ((ups_status & STATUS(ONLINE))) {
			upsdebugx(1,
				"%s: seems that UPS [%s] reports both online and on-battery "
				"power states at the same time",
				__func__, upsname);
		}
	}

	if (!(ups_status & STATUS(ONLINE))) {
		if ((ups_status & STATUS(OFFLINE))) {
			status_set("OB");		/* on battery */
		} else {
			if ((ups_status & STATUS(DISCHRG))) {
				upsdebugx(1,
					"%s: seems that UPS [%s] does not report a power state, "
					"but it is discharging - assuming it is on battery now",
					__func__, upsname);

				status_set("OB");
			} else {
				upsdebugx(1,
					"%s: seems that UPS [%s] does not report a power state",
					__func__, upsname);
			}
		}
	} else if ((ups_status & STATUS(DISCHRG))) {
		/* We are known to be online AND discharging... */
		int	do_logmsg = 0, current_charge = 0;

		/* if online but discharging */
		if (onlinedischarge_calibration) {
			/* if we treat OL+DISCHRG as calibrating */
			status_set_CAL();	/* calibration */
		}

		if (onlinedischarge_onbattery) {
			/* if we treat OL+DISCHRG as being offline */
			status_set("OB");	/* on battery */
		}

		if (!onlinedischarge_onbattery && !onlinedischarge_calibration) {
			/* Some situation not managed by end-user's hints */
			if (!(ups_status & STATUS(CALIB))) {
				/* if in OL+DISCHRG unknowingly, warn user,
				 * unless we throttle it this time - see below */
				do_logmsg = 1;
			}
			/* if we're presumably calibrating */
			status_set("OL");	/* on line */
		}

		if (do_logmsg) {
			/* Any throttling to apply? */
			const char *s;

			/* First disable, then enable if OK for noise*/
			do_logmsg = 0;

			/* Time or not, did the charge change since last log?
			 * Reminder: "onlinedischarge_log_throttle_charge" is
			 * the last-reported charge in OL+DISCHRG situation,
			 * as the battery remainder trickles down and we only
			 * report the changes (throttling the message stream).
			 * The "onlinedischarge_log_throttle_hovercharge" lets
			 * us ignore sufficiently high battery charges, where
			 * the user configuration (or defaults at 100%) tell
			 * us this is just about the battery not accepting the
			 * external power *all* the time so its charge "hovers"
			 * (typically between 90%-100%) to benefit the chemical
			 * process back-end of the battery and its life time.
			 */
			if ((s = dstate_getinfo("battery.charge"))) {
				/* NOTE: exact "0" may mean a conversion error: */
				current_charge = atoi(s);
				if (current_charge > 0
				&&  current_charge != onlinedischarge_log_throttle_charge
				) {
					/* Charge has changed, but is it
					 * now low enough to worry? */
					if (current_charge
					    < onlinedischarge_log_throttle_hovercharge
					) {
						upsdebugx(3, "%s: current "
							"battery.charge=%d is under "
							"onlinedischarge_log_throttle_hovercharge=%d "
							"(previous onlinedischarge_log_throttle_charge=%d): %s",
							__func__, current_charge,
							onlinedischarge_log_throttle_hovercharge,
							onlinedischarge_log_throttle_charge,
							(current_charge > onlinedischarge_log_throttle_charge ? "charging" : "draining"));
						do_logmsg = 1;
					} else {
						/* All seems OK, don't spam log
						 * unless running at a really
						 * high debug verbosity */
						upsdebugx(5, "%s: current "
							"battery.charge=%d "
							"is okay compared to "
							"onlinedischarge_log_throttle_hovercharge=%d "
							"(previous onlinedischarge_log_throttle_charge=%d): %s",
							__func__, current_charge,
							onlinedischarge_log_throttle_hovercharge,
							onlinedischarge_log_throttle_charge,
							(current_charge > onlinedischarge_log_throttle_charge ? "charging" : "draining"));
					}
				}
			} else {
				/* Should we default the time throttle? */
				if (onlinedischarge_log_throttle_sec == -2) {
					onlinedischarge_log_throttle_sec = 30;
					/* Report once, so almost loud */
					upsdebugx(1, "%s: seems battery.charge "
						"is not served by this device "
						"or subdriver; defaulting "
						"onlinedischarge_log_throttle_sec "
						"to %d",
						__func__,
						onlinedischarge_log_throttle_sec);
				}
			}

			/* Do we track and honor time since last log? */
			if (onlinedischarge_log_throttle_timestamp > 0
			&&  onlinedischarge_log_throttle_sec >= 0
			) {
				time_t	now;
				time(&now);

				if ((now - onlinedischarge_log_throttle_timestamp)
				    >= onlinedischarge_log_throttle_sec
				) {
					/* Enough time elapsed */
					do_logmsg = 1;
				}
			}
		}

		if (do_logmsg) {
			/* If OL+DISCHRG, and not-throttled against log spam */
			char	msg_charge[LARGEBUF];
			msg_charge[0] = '\0';

			/* Remember when we last logged this message */
			time(&onlinedischarge_log_throttle_timestamp);

			if (current_charge > 0
			&&  current_charge != onlinedischarge_log_throttle_charge
			) {
				/* Charge has changed, report and remember this */
				if (onlinedischarge_log_throttle_charge < 0) {
					/* First sequential message like this */
					snprintf(msg_charge, sizeof(msg_charge),
						"Battery charge is currently %d. ",
						current_charge);
				} else {
					snprintf(msg_charge, sizeof(msg_charge),
						"Battery charge changed from %d to %d "
						"since last such report (%s). ",
						onlinedischarge_log_throttle_charge,
						current_charge,
						(current_charge > onlinedischarge_log_throttle_charge ? "charging" : "draining"));
				}
				onlinedischarge_log_throttle_charge = current_charge;
			}

			upslogx(LOG_WARNING, "%s: seems that UPS [%s] is in OL+DISCHRG state now. %s"
			"Is it calibrating (perhaps you want to set 'onlinedischarge_calibration' option)? "
			"Note that some UPS models (e.g. CyberPower UT series) emit OL+DISCHRG when "
			"in fact offline/on-battery (perhaps you want to set 'onlinedischarge_onbattery' option).",
			__func__, upsname, msg_charge);
		}
	} else if ((ups_status & STATUS(ONLINE))) {
		/* we get here if simply online, not discharging */
		status_set("OL");
	}

	isCalibrating = status_get("CAL");

	if ((ups_status & STATUS(DISCHRG))
	&& !(ups_status & STATUS(DEPLETED))
	) {
		status_set("DISCHRG");		/* discharging */
	}

	if ((ups_status & STATUS(CHRG))
	&& !(ups_status & STATUS(FULLYCHARGED))
	) {
		status_set("CHRG");		/* charging */
	}

	if (ups_status & (STATUS(LOWBATT) | STATUS(TIMELIMITEXP) | STATUS(SHUTDOWNIMM))) {
		if (lbrb_log_delay_sec < 1
		|| (!isCalibrating && !lbrb_log_delay_without_calibrating)
		|| !(ups_status & STATUS(ONLINE))	/* assume actual power failure, do not delay */
		) {
			/* Quick and easy decision */
			status_set("LB");		/* low battery */
		} else {
			time_t	now;
			time(&now);

			if (!last_lb_start) {
				last_lb_start = now;
			} else {
				if (difftime(now, last_lb_start) > lbrb_log_delay_sec) {
					/* Patience expired */
					status_set("LB");	/* low battery */
				} else {
					upsdebugx(2, "%s: throttling LB status due to lbrb_log_delay_sec", __func__);
				}
			}
		}
	} else {
		last_lb_start = 0;
	}

	if (ups_status & STATUS(OVERLOAD)) {
		status_set("OVER");		/* overload */
	}

	if ((ups_status & STATUS(REPLACEBATT)) || (ups_status & STATUS(NOBATTERY))) {
		if (lbrb_log_delay_sec < 1
		|| (!isCalibrating && !lbrb_log_delay_without_calibrating)
		|| !last_lb_start	/* Calibration ended (not LB anymore) */
		|| !(ups_status & STATUS(ONLINE))	/* assume actual power failure, do not delay */
		) {
			/* Quick and easy decision */
			status_set("RB");		/* replace batt */
		} else {
			time_t	now;
			time(&now);

			if (!last_rb_start) {
				last_rb_start = now;
			} else {
				if (difftime(now, last_rb_start) > lbrb_log_delay_sec) {
					/* Patience expired */
					status_set("RB");	/* replace batt */
				} else {
					upsdebugx(2, "%s: throttling RB status due to lbrb_log_delay_sec", __func__);
				}
			}
		}
	} else {
		last_rb_start = 0;
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

	if (ups_status & STATUS(ECOMODE)) {
		buzzmode_set("vendor:default:ECO");	/* on ECO(HE) Mode,
						 * should not happen
						 * via ups.status anymore */
	}

	if (ups_status & STATUS(ESSMODE)) {
		buzzmode_set("vendor:default:ESS");	/* on ESS Mode,
						 * should not happen
						 * via ups.status anymore */
	}

	if (ups_status & STATUS(OFF)) {
		status_set("OFF");		/* ups is off */
	}

	if (!isCalibrating) {
		if (last_calibration_start) {
			time(&last_calibration_finish);
			upsdebugx(2, "%s: calibration is no longer in place, took %f sec",
				__func__, difftime(last_calibration_finish, last_calibration_start));
		}
		last_calibration_start = 0;
	}
}

/* find info element definition in info array
 * by NUT varname, or NULL if not found.
 */
static hid_info_t *find_nut_info(const char *varname)
{
	hid_info_t *hidups_item;

	if (!varname) {
		upsdebugx(2, "%s: varname == NULL", __func__);
		errno = EINVAL;
		return NULL;
	}

	if (!*varname) {
		upsdebugx(2, "%s: varname is an empty string", __func__);
		errno = EINVAL;
		return NULL;
	}

	for (hidups_item = subdriver->hid2nut; hidups_item->info_type != NULL ; hidups_item++) {
		if (strcasecmp(hidups_item->info_type, varname))
			continue;

		if (hidups_item->hiddata != NULL) {
			errno = 0;
			return hidups_item;
		}
	}

	upsdebugx(2, "%s: unknown info type: %s", __func__, varname);
	errno = EINVAL;
	return NULL;
}

/* find info element definition in info array
 * by HID data pointer, or NULL if not found.
 */
static hid_info_t *find_hid_info(const HIDData_t *hiddata)
{
	hid_info_t *hidups_item;

	if (!hiddata) {
		upsdebugx(2, "%s: hiddata == NULL", __func__);
		errno = EINVAL;
		return NULL;
	}

	for (hidups_item = subdriver->hid2nut; hidups_item->info_type != NULL ; hidups_item++) {
		/* Skip server side vars */
		if (hidups_item->hidflags & HU_FLAG_ABSENT)
			continue;

		if (hidups_item->hiddata == hiddata) {
			errno = 0;
			return hidups_item;
		}
	}

	errno = EINVAL;
	return NULL;
}

/* Find the HID Item value matching that NUT value;
 * useful for setvar() with a value lookup...
 * This method (and hid2info->nuf(value)) can return
 * a long value to be set, or "-1" AND errno==EINVAL
 * to propagate an error.
 */
static long hu_find_valinfo(info_lkp_t *hid2info, const char* value)
{
	info_lkp_t	*info_lkp;

	errno = 0;

	/* if a conversion function is defined, use 'value' as argument for it */
	if (hid2info->nuf != NULL) {
		double	hid_value;
		hid_value = hid2info->nuf(value);
		upsdebugx(5, "hu_find_valinfo: found %g (value: '%s'%s)",
			hid_value, value,
			(errno == EINVAL ? ", invalid input" : "")
		);
		return hid_value;
	}

	for (info_lkp = hid2info; info_lkp->nut_value != NULL; info_lkp++) {
		if (!(strcmp(info_lkp->nut_value, value))) {
			upsdebugx(5,
				"hu_find_valinfo: found %s (value: %ld)",
				info_lkp->nut_value, info_lkp->hid_value);
			return info_lkp->hid_value;
		}
	}

	upsdebugx(3,
		"hu_find_valinfo: no matching HID value for this INFO_* value (%s)",
		value);
	errno = EINVAL;
	return -1;
}

/* find the NUT value matching that HID Item value
 * This method (and hid2info->fun(value)) can return
 * a string value to be set, or NULL AND errno==EINVAL
 * to propagate an error (in practice, a NULL suffices
 * to not-set a dstate).
 */
static const char *hu_find_infoval(info_lkp_t *hid2info, const double value)
{
	info_lkp_t	*info_lkp;

	errno = 0;

	/* if a conversion function is defined, use 'value' as argument for it */
	if (hid2info->fun != NULL) {
		return hid2info->fun(value);
	}

	/* use 'value' as an index for a lookup in an array */
	for (info_lkp = hid2info; info_lkp->nut_value != NULL; info_lkp++) {
		if (info_lkp->hid_value == (long)value) {
			upsdebugx(5,
				"hu_find_infoval: found %s (value: %ld)",
				info_lkp->nut_value, (long)value);
			return info_lkp->nut_value;
		}
	}

	upsdebugx(3,
		"hu_find_infoval: no matching INFO_* value for this HID value (%g)",
		value);
	errno = EINVAL;
	return NULL;
}

/* return -1 on failure, 0 for a status update and 1 in all other cases */
static int ups_infoval_set(hid_info_t *item, double value)
{
	const char	*nutvalue = NULL;
	const char	*pType = (nut_debug_level > 0 && item->hiddata) ? HIDDataType(item->hiddata) : NULL;

	/* need lookup'ed translation? */
	if (item->hid2info != NULL) {
		if ((nutvalue = hu_find_infoval(item->hid2info, value)) == NULL) {
			upsdebugx(5, "%s: Lookup [%g] failed for [%s]", __func__, value, item->info_type);
			return -1;
		}
		/* clause continued below after this message... */
	}

	if (item->hiddata != NULL) {
		if (!item->hiddata->mapping_handled) {
			upsdebugx(5, "%s: setting report descriptor mapping for '%s' (%s) as handled",
				__func__, NUT_STRARG(item->hidpath), NUT_STRARG(pType));
			item->hiddata->mapping_handled = TRUE;
		} else {
			upsdebugx(5, "%s: report descriptor mapping for '%s' (%s) was already set as handled",
				__func__, NUT_STRARG(item->hidpath), NUT_STRARG(pType));
		}
	} else {
		upsdebugx(5, "%s: got no report descriptor mapping for '%s'",
			__func__, NUT_STRARG(item->hidpath));
	}

	if (nutvalue != NULL) {
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
		dstate_setinfo_dynamic(item->info_type, item->dfl, "%f", value);
	}

	return 1;
}
