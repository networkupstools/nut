/* Bridge driver to read Mac OS X UPS status (as displayed in Energy Saver control panel)
 *
 * Copyright (C) 2011-2012 Charles Lepple <clepple+nut@gmail.com>
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
 */

#include "main.h"

#include <regex.h>

#include "CoreFoundation/CoreFoundation.h"
#include "IOKit/ps/IOPowerSources.h"
#include "IOKit/ps/IOPSKeys.h"

#define DRIVER_NAME	"Mac OS X UPS meta-driver"
#define DRIVER_VERSION	"1.0"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Charles Lepple <clepple+nut@gmail.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#if 0
#define CFRelease(ref) do { upsdebugx(3, "%s:%d: CFRelease(%p)", __FILE__, __LINE__, ref); CFRelease(ref); } while(0)
#endif

static CFStringRef g_power_key = NULL;
static double max_capacity_value = 100.0;

/*! Copy the current power dictionary.
 *
 * Caller must release power dictionary when finished with it.
 */
static CFDictionaryRef copy_power_dictionary(CFTypeRef power_key)
{
	CFTypeRef power_blob;
	CFDictionaryRef power_dictionary;

	power_blob = IOPSCopyPowerSourcesInfo();

	assert(power_blob);
	upsdebugx(6, "%s: Got power_blob:", __func__);
	if(nut_debug_level >= 6) CFShow(power_blob);

	upsdebugx(5, "power_key = ");
	if(nut_debug_level >= 5) CFShow(power_key);
	upsdebugx(6, "end power_key");

	power_dictionary = IOPSGetPowerSourceDescription(power_blob, power_key);

	upsdebugx(5, "Asserting 'power_dictionary': %p", power_dictionary);
	assert(power_dictionary);
	upsdebugx(5, "CFShowing 'power_dictionary'");
	if(nut_debug_level >= 5) CFShow(power_dictionary);

	CFRetain(power_dictionary);

	/* Get a new power_blob next time: */
	CFRelease(power_blob);

	return power_dictionary;
}

void upsdrv_initinfo(void)
{
	/* try to detect the UPS here - call fatal_with_errno(EXIT_FAILURE, ) if it fails */

	char device_name[80] = "";
	CFStringRef device_type_cfstr, device_name_cfstr;
	CFPropertyListRef power_dictionary;
	CFNumberRef max_capacity;

	upsdebugx(1, "upsdrv_initinfo()");

	dstate_setinfo("device.mfr", "(unknown)");

	power_dictionary = copy_power_dictionary(g_power_key);

	device_type_cfstr = CFDictionaryGetValue(power_dictionary, CFSTR(kIOPSTypeKey));
	if(device_type_cfstr && !CFStringCompare(device_type_cfstr, CFSTR(kIOPSInternalBatteryType), 0)) {
		dstate_setinfo("device.type", "battery");
	}

	upsdebugx(2, "Getting 'Name' key");

	device_name_cfstr = CFDictionaryGetValue(power_dictionary, CFSTR(kIOPSNameKey));

	if (!device_name_cfstr) {
		fatalx(EXIT_FAILURE, "Couldn't retrieve 'Name' key from power dictionary.");
	}

	CFRetain(device_name_cfstr);

	CFStringGetCString(device_name_cfstr, device_name, sizeof(device_name), kCFStringEncodingUTF8);
	upsdebugx(2, "Got name: %s", device_name);

	CFRelease(device_name_cfstr);

	dstate_setinfo("device.model", "%s", device_name);

	max_capacity = CFDictionaryGetValue(power_dictionary, CFSTR(kIOPSMaxCapacityKey));
	if(max_capacity) {
		CFRetain(max_capacity);

		CFNumberGetValue(max_capacity, kCFNumberDoubleType, &max_capacity_value);
		CFRelease(max_capacity);

		upsdebugx(3, "Max Capacity = %.f units (usually 100)", max_capacity_value);
		if(max_capacity_value != 100) {
			upsdebugx(1, "Max Capacity: %f != 100", max_capacity_value);
		}
	}

	/* upsh.instcmd = instcmd; */
	CFRelease(power_dictionary);
}

void upsdrv_updateinfo(void)
{
	CFPropertyListRef power_dictionary;
	CFStringRef power_source_state;
	CFNumberRef battery_voltage, battery_runtime;
	CFNumberRef current_capacity;
	CFBooleanRef is_charging;
	double max_capacity_value = 100.0, current_capacity_value;

	upsdebugx(1, "upsdrv_updateinfo()");

	power_dictionary = copy_power_dictionary( g_power_key );
	assert(power_dictionary); /* TODO: call dstate_datastale()? */

	status_init();

	/* Retrieve OL/OB state */
	power_source_state = CFDictionaryGetValue(power_dictionary, CFSTR(kIOPSPowerSourceStateKey));
	assert(power_source_state);
	CFRetain(power_source_state);

	upsdebugx(3, "Power Source State:");
	if(nut_debug_level >= 3) CFShow(power_source_state);

	if(!CFStringCompare(power_source_state, CFSTR(kIOPSACPowerValue), 0)) {
		status_set("OL");
	} else {
		status_set("OB");
	}

	CFRelease(power_source_state);

	/* Retrieve CHRG state */
	is_charging = CFDictionaryGetValue(power_dictionary, CFSTR(kIOPSIsChargingKey));
        if(is_charging) {
		Boolean is_charging_value;

		is_charging_value = CFBooleanGetValue(is_charging);
		if(is_charging_value) {
			status_set("CHRG");
		}
	}

	status_commit();

	/* Retrieve battery voltage */

	battery_voltage = CFDictionaryGetValue(power_dictionary, CFSTR(kIOPSVoltageKey));
	if(battery_voltage) {
		int battery_voltage_value;

		CFNumberGetValue(battery_voltage, kCFNumberIntType, &battery_voltage_value);
		upsdebugx(2, "battery_voltage = %d mV", battery_voltage_value);
		dstate_setinfo("battery.voltage", "%.3f", battery_voltage_value/1000.0);
	}

	/* Retrieve battery runtime */
	battery_runtime = CFDictionaryGetValue(power_dictionary, CFSTR(kIOPSTimeToEmptyKey));
	if(battery_runtime) {
		double battery_runtime_value;

		CFNumberGetValue(battery_runtime, kCFNumberDoubleType, &battery_runtime_value);

		upsdebugx(2, "battery_runtime = %.f minutes", battery_runtime_value);
		if(battery_runtime_value > 0) {
			dstate_setinfo("battery.runtime", "%d", (int)(battery_runtime_value*60));
		} else {
			dstate_delinfo("battery.runtime");
		}
	} else {
		dstate_delinfo("battery.runtime");
	}

	/* Retrieve current capacity */
	current_capacity = CFDictionaryGetValue(power_dictionary, CFSTR(kIOPSCurrentCapacityKey));
	if(current_capacity) {
		CFNumberGetValue(current_capacity, kCFNumberDoubleType, &current_capacity_value);

		upsdebugx(2, "Current Capacity = %.f/%.f units", current_capacity_value, max_capacity_value);
		if(max_capacity_value > 0) {
			dstate_setinfo("battery.charge", "%.f", 100.0 * current_capacity_value / max_capacity_value);
		}
	}

	/* TODO: it should be possible to set poll_interval (and maxage in the
	 * server) to an absurdly large value, and use notify(3) to get
	 * updates.
         */

	/*
	 * poll_interval = 2;
	 */

	dstate_dataok();
	CFRelease(power_dictionary);
}

void upsdrv_shutdown(void)
{
	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */

	/* maybe try to detect the UPS here, but try a shutdown even if
	   it doesn't respond at first if possible */

	/* NOTE: Mac OS X already has shutdown routines - this driver is more
           for monitoring and notification purposes. Still, there is a key that
           might be useful to set in SystemConfiguration land. */
	fatalx(EXIT_FAILURE, "shutdown not supported");

	/* you may have to check the line status since the commands
	   for toggling power are frequently different for OL vs. OB */

	/* OL: this must power cycle the load if possible */

	/* OB: the load must remain off until the power returns */
}

/*
static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send_buf(upsfd, ...);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}
*/

/* TODO:
 There is a configuration file here:
 /Library/Preferences/SystemConfiguration/com.apple.PowerManagement.plist
 with several keys under UPSDefaultThresholds:

  * UPSShutdownAfterMinutes
  * UPSShutdownAtLevel
  * UPSShutdownAtMinutesLeft

 It is not likely that these keys can be written, but they might be good values for NUT variables.

 In conjunction with 'ignorelb' and a delta, this could be used to synthesize a
 LB status right before the computer shuts down.
*/

/*
static int setvar(const char *varname, const char *val)
{
	if (!strcasecmp(varname, "ups.test.interval")) {
		ser_send_buf(upsfd, ...);
		return STAT_SET_HANDLED;
	}

	upslogx(LOG_NOTICE, "setvar: unknown variable [%s]", varname);
	return STAT_SET_UNKNOWN;
}
*/

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	/* allow '-x xyzzy' */
	/* addvar(VAR_FLAG, "xyzzy", "Enable xyzzy mode"); */

	/* allow '-x foo=<some value>' */
	/* addvar(VAR_VALUE, "foo", "Override foo setting"); */

	addvar(VAR_VALUE, "model", "Regular Expression to match power source model name");
}

void upsdrv_initups(void)
{
	CFArrayRef power_source_key_list;
	CFIndex num_keys, index;
	CFDictionaryRef power_dictionary;
	CFTypeRef power_blob;
	CFStringRef potential_key, potential_model;
	char *device_name = device_path, *model_name; /* regex(3) */
	char potential_device_name[80], potential_model_name[80];
        regex_t name_regex, model_regex;
	int ret;

	upsdebugx(3, "upsdrv_initups(): Power Sources blob:");
	/* upsfd = ser_open(device_path); */
	/* ser_set_speed(upsfd, device_path, B1200); */
	power_blob = IOPSCopyPowerSourcesInfo();
	if(!power_blob) {
		fatalx(EXIT_FAILURE, "Couldn't retrieve Power Sources blob");
	}

	if(nut_debug_level >= 3) CFShow(power_blob);

	if(!strcmp(device_name, "auto")) {
		device_name = "/UPS";
	}

	upsdebugx(2, "Matching power supply key names against regex '%s'", device_name);

        ret = regcomp(&name_regex, device_name, REG_EXTENDED|REG_NOSUB|REG_ICASE);

	if(ret) {
		fatalx(EXIT_FAILURE,
				"Failed to compile regex from 'port' parameter: '%s'.",
				device_name);
	}

	if((model_name = getval("model"))) {
		upsdebugx(2, "Matching power supply model names against regex '%s'", model_name);
		ret = regcomp(&model_regex, model_name, REG_EXTENDED|REG_NOSUB|REG_ICASE);

		if(ret) {
			fatalx(EXIT_FAILURE,
					"Failed to compile regex from 'model' parameter: '%s'.",
					model_name);
		}
	}

	power_source_key_list = IOPSCopyPowerSourcesList(power_blob);

	num_keys = CFArrayGetCount(power_source_key_list);
	upsdebugx(1, "Number of power supplies found: %d", (int)num_keys);

	if(nut_debug_level >= 3) CFShow(power_source_key_list);

	if(num_keys < 1) {
		/* bail */
		fatalx(EXIT_FAILURE, "Couldn't find any UPS or battery");
	}

	for(index=0; index < num_keys; index++) {
		potential_key = CFArrayGetValueAtIndex(power_source_key_list, index);
		CFStringGetCString(potential_key, potential_device_name,
				sizeof(potential_device_name), kCFStringEncodingUTF8);
		upsdebugx(1, "    Power supply: %s", potential_device_name);

		ret = regexec(&name_regex, potential_device_name, 0,0,0);

		power_dictionary = copy_power_dictionary(potential_key);

		upsdebugx(2, "Getting 'Name' key (UPS model)");

		potential_model = CFDictionaryGetValue(power_dictionary, CFSTR(kIOPSNameKey));
		CFStringGetCString(potential_model, potential_model_name, sizeof(potential_model_name), kCFStringEncodingUTF8);
		upsdebugx(1, "        model name: %s", potential_model_name);

		CFRelease(power_dictionary);

		/* Does key match? Check model: */
		if (!ret) {
			if(model_name) {
				ret = regexec(&model_regex, potential_model_name, 0,0,0);
				if(!ret) {
					upsdebugx(2, "Matched model name");
					break;
				}
			} else {
				upsdebugx(2, "Matched key name");
				break;
			}
		}
	}

        regfree(&name_regex);
	if(model_name) {
		regfree(&model_regex);
	}

	if(ret) {
		fatalx(EXIT_FAILURE, "Couldn't find UPS or battery matching both 'port' (%s) and 'model' (%s)",
			device_name, model_name);
	}

	g_power_key = potential_key;
	CFRetain(g_power_key);
	upsdebugx(2, "g_power_key = ");
	if(nut_debug_level >= 2) CFShow(g_power_key);
	upsdebugx(2, "end g_power_key.");

	power_dictionary = copy_power_dictionary(g_power_key);
	assert(power_dictionary);
	if(nut_debug_level >= 3) CFShow(power_dictionary);

	/* the upsh handlers can't be done here, as they get initialized
	 * shortly after upsdrv_initups returns to main.
	 */

	/* don't try to detect the UPS here */

	/* do stuff */

	CFRelease(power_dictionary);
}

void upsdrv_cleanup(void)
{
	upsdebugx(1, "Cleanup: release references");
	CFRelease(g_power_key);

	/* free(dynamic_mem); */
	/* ser_close(upsfd, device_path); */
}
