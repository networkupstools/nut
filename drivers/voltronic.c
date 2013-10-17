/*
 * voltronic.c: driver core for Voltronic Power UPSes
 *
 * A document describing the protocol implemented by this driver can be
 * found online at http://www.networkupstools.org/ups-protocols/
 *
 * Copyright (C)
 *   2013 - Daniele Pezzini <hyouko@gmail.com>
 * Loosely based on blazer.c - Copyright (C)
 *   2008,2009 - Arjen de Korte <adkorte-guest@alioth.debian.org>
 *   2012 - Arnaud Quette <ArnaudQuette@Eaton.com>
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
#include "voltronic.h"

static int	ondelay = 0;	/* minutes */
static int	offdelay = 30;	/* seconds */

/* Capability vars */
static char	*bypass_alarm, *battery_alarm, *bypass_when_off, *alarm_control, *converter_mode;
static char	*eco_mode, *battery_open_status_check, *bypass_forbidding, *site_fault_detection;
static char	*advanced_eco_mode, *constant_phase_angle, *limited_runtime_on_battery;
/* End of capability vars */

static int	battery_number;	/* Number of batteries that make a pack */

/* These vars are used to prevent data_stale in upsdrv_updateinfo() if we encounter a problem in voltronic_initinfo() */
static int	battery = 1, loadlevel = 1, multi = 1, outlet = 1;

/* Vars used to ignore queries for warnings and operational mode in voltronic_status() if we encounter a problem in voltronic_initinfo() */
static int	warning = 1, mode = 1;

/* Alarm/status buffer */
static char	alarm_buf[SMALLBUF], status_buf[SMALLBUF];

/* voltronic_battery() - Actual infos about battery */
static int voltronic_battery(void)
{
	const struct {
		const char	*var;
		const char	*fmt;
		double	(*conv)(const char *, char **);
	} battery_status[] = {
		{ "battery.voltage", "%.2f", strtod },
		{ "battery.number", "%.0f", strtod },	/* the number of batteries that make a pack */
		{ "battery.packs", "%.0f", strtod },	/* number of battery packs in parallel */
		{ "battery.charge", "%.0f", strtod },
		{ "battery.runtime", "%.0f", strtod },
		{ NULL }
	};

	char	buf[SMALLBUF], *val, *last = NULL;
	int	i;

	/*
	 * > [QBV\r]
	 * < [(026.5 02 01 068 255\r]
	 *    012345678901234567890
	 *    0         1         2
	 */
	if ((voltronic_command("QBV\r", buf, sizeof(buf)) < 21) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for battery informations rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	for (i = 0, val = strtok_r(buf+1, " ", &last); battery_status[i].var; i++, val = strtok_r(NULL, " \r\n", &last)) {

		if (!val) {
			upsdebugx(2, "%s: parsing failed", __func__);
			return -1;
		}

		if (strspn(val, "0123456789.") != strlen(val)) {
			upsdebugx(2, "%s: non numerical value [%s]", __func__, val);
			continue;
		}

		if (i != 1) {	/* Battery runtime (i = 4) is reported by UPS in minutes, nut expects seconds */
			dstate_setinfo(battery_status[i].var, battery_status[i].fmt, battery_status[i].conv(val, NULL)*(i == 4 ? 60 : 1));
		} else {	/* There's no nut variable for battery number */
			battery_number = atoi(val);
		}
	}

	return 0;	

}

/* voltronic_protocol() - Protocol used by UPS */
static int voltronic_protocol(void)
{
	char	buf[SMALLBUF], *prot;

	/*
	 * > [QPI\r]
	 * < [(PI00\r]
	 *    012345
	 *    0
	 */
	if ((voltronic_command("QPI\r", buf, sizeof(buf)) < 6) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for protocol rejected by UPS", __func__);
		return -1;
	}

	if (strncasecmp(buf, "(PI", 3)) {
		upsdebugx(2, "%s: invalid start characters [%.3s]", __func__, strtok(buf, "\r\n"));
		return -1;
	}

	prot = strtok(buf+3, "\n\r");

	if (strspn(prot, "0123489") != strlen(prot)) {	/* Here we exclude non numerical value and other non accepted protocols (hence the restricted comparison target) */
		upslogx(LOG_ERR, "Protocol [PI%s] is not supported by this driver", prot);
		return -1;
	}

	switch(atoi(prot)){
		case 0: dstate_setinfo("ups.firmware.aux", "P00");	break;
		case 1: dstate_setinfo("ups.firmware.aux", "P01");	break;
		case 2: dstate_setinfo("ups.firmware.aux", "P02");	break;
		case 3: dstate_setinfo("ups.firmware.aux", "P03");	break;
		case 8: dstate_setinfo("ups.firmware.aux", "P08");	break;
		case 9: dstate_setinfo("ups.firmware.aux", "P09");	break;
		case 10: dstate_setinfo("ups.firmware.aux", "P10");	break;
		case 14: dstate_setinfo("ups.firmware.aux", "P14");	break;
		case 31: dstate_setinfo("ups.firmware.aux", "P31");	break;
		case 99: dstate_setinfo("ups.firmware.aux", "P99");	break;
		default: upslogx(LOG_ERR, "Protocol [PI%s] is not supported by this driver", prot);	return -1;	break;
	}

	return 0;
}

/* voltronic_fault() - Check if a fault is reported by UPS and its type */
static int voltronic_fault(void)	/* Call this function only if a fault is found in 12bit flag of QGS, otherwise you'll get a fake reply */
{
	char	buf[SMALLBUF], alarm[SMALLBUF];

	/*
	 * > [QFS\r]
	 * < [(OK\r] <- No fault
	 *    0123
	 *    0
	 * < [(14 212.1 50.0 005.6 49.9 006 010.6 343.8 ---.- 026.2 021.8 01101100\r] <- Fault type + Short status
	 *    012345678901234567890123456789012345678901234567890123456789012345678
	 *    0         1         2         3         4         5         6
	 */
	if (voltronic_command("QFS\r", buf, sizeof(buf)) < 4) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for fault type rejected by UPS", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(OK", 3)) {
		upslogx(LOG_INFO, "No fault found");
		return 0;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (!dstate_getinfo("ups.firmware.aux") && voltronic_protocol()) {	/* Check needed since this function may be called in voltronic_initinfo() by voltronic_status() before the check for protocol */
		upsdebugx(2, "%s: failed to get protocol from UPS", __func__);
		return -1;
	}

	strtok(buf, " ");	/* Deleting from buf unneeded values (short status) */

	if ((strspn(buf+1, "0123456789ABC") != 2) || ((buf[1] != '1') && (strspn(buf+2, "0123456789") != 1))) {
		snprintf(alarm, sizeof(alarm), "Unknown fault [%s]", strtok(buf+1, "\r\n"));
	} else if (!strcmp(dstate_getinfo("ups.firmware.aux"), "P31")) {
		if (strpbrk(buf+2, "ABC")) {
			snprintf(alarm, sizeof(alarm), "Unknown fault [%s]", strtok(buf+1, "\r\n"));
		} else {
			switch(atoi(buf+1)){
				case 1: strcpy(alarm, "Fan failure.");	break;
				case 2: strcpy(alarm, "Over temperature fault.");	break;
				case 3: strcpy(alarm, "Battery voltage is too high.");	break;
				case 4: strcpy(alarm, "Battery voltage too low.");	break;
				case 5: strcpy(alarm, "Inverter relay short-circuited.");	break;
				case 6: strcpy(alarm, "Inverter voltage over maximum value.");	break;
				case 7: strcpy(alarm, "Overload fault.");	snprintfcat(status_buf, sizeof(status_buf), " OVER");	break;
				case 8: strcpy(alarm, "Bus voltage exceeds its upper limit.");	break;
				case 9: strcpy(alarm, "Bus soft start fail.");	break;
				case 10: strcpy(alarm, "Unknown fault [Fault code: 10]");	break;
				case 51: strcpy(alarm, "Over current fault.");	break;
				case 52: strcpy(alarm, "Bus voltage below its under limit.");	break;
				case 53: strcpy(alarm, "Inverter soft start fail.");	break;
				case 54: strcpy(alarm, "Self test fail.");	break;
				case 55: strcpy(alarm, "Output DC voltage exceeds its upper limit.");	break;
				case 56: strcpy(alarm, "Battery open fault.");	break;
				case 57: strcpy(alarm, "Current sensor fault.");	break;
				case 58: strcpy(alarm, "Battery short.");	break;
				case 59: strcpy(alarm, "Inverter voltage below its lower limit.");	break;
				default: snprintf(alarm, sizeof(alarm), "Unknown fault [%s]", strtok(buf+1, "\r\n"));	break;
			}
		}
	} else {
		switch(atoi(buf+1)){
			case 1: switch(buf[2]){
					case 'A': strcpy(alarm, "L1 inverter negative power out of acceptable range.");	break;
					case 'B': strcpy(alarm, "L2 inverter negative power out of acceptable range.");	break;
					case 'C': strcpy(alarm, "L3 inverter negative power out of acceptable range.");	break;
					default: strcpy(alarm, "Bus voltage not within default setting.");	break;
				}
			case 2: strcpy(alarm, "Bus voltage over maximum value.");	break;
			case 3: strcpy(alarm, "Bus voltage below minimum value.");	break;
			case 4: strcpy(alarm, "Bus voltage differences out of acceptable range.");	break;
			case 5: strcpy(alarm, "Bus voltage of slope rate drops too fast.");	break;
			case 6: strcpy(alarm, "Over current in PFC input inductor.");	break;
			case 11: strcpy(alarm, "Inverter voltage not within default setting.");	break;
			case 12: strcpy(alarm, "Inverter voltage over maximum value.");	break;
			case 13: strcpy(alarm, "Inverter voltage below minimum value.");	break;
			case 14: strcpy(alarm, "Inverter short-circuited.");	break;
			case 15: strcpy(alarm, "L2 phase inverter short-circuited.");	break;
			case 16: strcpy(alarm, "L3 phase inverter short-circuited.");	break;
			case 17: strcpy(alarm, "L1L2 inverter short-circuited.");	break;
			case 18: strcpy(alarm, "L2L3 inverter short-circuited.");	break;
			case 19: strcpy(alarm, "L3L1 inverter short-circuited.");	break;
			case 21: strcpy(alarm, "Battery SCR short-circuited.");	break;
			case 22: strcpy(alarm, "Line SCR short-circuited.");	break;
			case 23: strcpy(alarm, "Inverter relay open fault.");	break;
			case 24: strcpy(alarm, "Inverter relay short-circuited.");	break;
			case 25: strcpy(alarm, "Input and output wires oppositely connected.");	break;
			case 26: strcpy(alarm, "Battery oppositely connected.");	break;
			case 27: strcpy(alarm, "Battery voltage is too high.");	break;
			case 28: strcpy(alarm, "Battery voltage too low.");	break;
			case 29: strcpy(alarm, "Failure for battery fuse being open-circuited.");	break;
			case 31: strcpy(alarm, "CAN-bus communication fault.");	break;
			case 32: strcpy(alarm, "Host signal circuit fault.");	break;
			case 33: strcpy(alarm, "Synchronous signal circuit fault.");	break;
			case 34: strcpy(alarm, "Synchronous pulse signal circuit fault.");	break;
			case 35: strcpy(alarm, "Parallel cable disconnected.");	break;
			case 36: strcpy(alarm, "Load unbalanced.");	break;
			case 41: strcpy(alarm, "Over temperature fault.");	break;
			case 42: strcpy(alarm, "Communication failure between CPUs in control board.");	break;
			case 43: strcpy(alarm, "Overload fault.");	snprintfcat(status_buf, sizeof(status_buf), " OVER");	break;
			case 44: strcpy(alarm, "Fan failure.");	break;
			case 45: strcpy(alarm, "Charger failure.");	break;
			case 46: strcpy(alarm, "Model fault.");	break;
			case 47: strcpy(alarm, "MCU communication fault.");	break;
			default: snprintf(alarm, sizeof(alarm), "Unknown fault [%s]", strtok(buf+1, "\r\n"));	break;
		}
	}

	snprintfcat(alarm_buf, sizeof(alarm_buf), " UPS Fault! %s", alarm);
	upslogx(LOG_INFO, "Fault found: %s", alarm);

	return 0;
}

/* voltronic_warning() - Check if a warning is reported by UPS and its type */
static int voltronic_warning(void)
{
	char	buf[SMALLBUF], warn[SMALLBUF] = "", unk[SMALLBUF] = "", bitwarns[SMALLBUF] = "", warns[4096] = "";
	int	i;

	/*
	 * > [QWS\r]
	 * < [(0000000100000000000000000000000000000000000000000000000000000000\r]
	 *    012345678901234567890123456789012345678901234567890123456789012345
	 *    0         1         2         3         4         5         6
	 */

	if ((voltronic_command("QWS\r", buf, sizeof(buf)) < 66) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for warning type rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (strspn(buf+1, "01") != (strcspn(buf, "\r\n")-1)) {
		upsdebugx(2, "%s: invalid reply from UPS [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	if (strspn(buf+1, "0") == (strcspn(buf, "\r\n")-1)) {	/* No warnings */
		return 0;
	}

	snprintfcat(alarm_buf, sizeof(alarm_buf), " UPS warnings:");

	for (i = 1; i <= ((int)strcspn(buf, "\r\n") - 1); i++) {
	char	*status = NULL;
	int	u = 0;

		if (buf[i] == '1') {

			switch(i){
				case 1: strcpy(warn, "Battery disconnected.");	break;
				case 2: strcpy(warn, "Neutral not connected.");	break;
				case 3: strcpy(warn, "Site fault.");	break;
				case 4: strcpy(warn, "Phase sequence incorrect.");	break;
				case 5: strcpy(warn, "Phase sequence incorrect in bypass.");	break;
				case 6: strcpy(warn, "Input frequency unstable in bypass.");	break;
				case 7: strcpy(warn, "Battery overcharged.");	break;
				case 8: strcpy(warn, "Low battery.");	status = "LB";	break;
				case 9: strcpy(warn, "Overload alarm.");	status = "OVER";	break;
				case 10: strcpy(warn, "Fan alarm.");	break;
				case 11: strcpy(warn, "EPO enabled.");	break;
				case 12: strcpy(warn, "Unable to turn on UPS.");	break;
				case 13: strcpy(warn, "Over temperature alarm.");	break;
				case 14: strcpy(warn, "Charger alarm.");	break;
				case 15: strcpy(warn, "Remote auto shutdown.");	break;
				case 16: strcpy(warn, "L1 input fuse not working.");	break;
				case 17: strcpy(warn, "L2 input fuse not working.");	break;
				case 18: strcpy(warn, "L3 input fuse not working.");	break;
				case 19: strcpy(warn, "Positive PFC abnormal in L1.");	break;
				case 20: strcpy(warn, "Negative PFC abnormal in L1.");	break;
				case 21: strcpy(warn, "Positive PFC abnormal in L2.");	break;
				case 22: strcpy(warn, "Negative PFC abnormal in L2.");	break;
				case 23: strcpy(warn, "Positive PFC abnormal in L3.");	break;
				case 24: strcpy(warn, "Negative PFC abnormal in L3.");	break;
				case 25: strcpy(warn, "Abnormal in CAN-bus communication.");	break;
				case 26: strcpy(warn, "Abnormal in synchronous signal circuit.");	break;
				case 27: strcpy(warn, "Abnormal in synchronous pulse signal circuit.");	break;
				case 28: strcpy(warn, "Abnormal in host signal circuit.");	break;
				case 29: strcpy(warn, "Male connector of parallel cable not connected well.");	break;
				case 30: strcpy(warn, "Female connector of parallel cable not connected well.");	break;
				case 31: strcpy(warn, "Parallel cable not connected well.");	break;
				case 32: strcpy(warn, "Battery connection not consistent in parallel systems.");	break;
				case 33: strcpy(warn, "AC connection not consistent in parallel systems.");	break;
				case 34: strcpy(warn, "Bypass connection not consistent in parallel systems.");	break;
				case 35: strcpy(warn, "UPS model types not consistent in parallel systems.");	break;
				case 36: strcpy(warn, "Capacity of UPSes not consistent in parallel systems.");	break;
				case 37: strcpy(warn, "Auto restart setting not consistent in parallel systems.");	break;
				case 38: strcpy(warn, "Battery cell over charge.");	break;
				case 39: strcpy(warn, "Battery protection setting not consistent in parallel systems.");	break;
				case 40: strcpy(warn, "Battery detection setting not consistent in parallel systems.");	break;
				case 41: strcpy(warn, "Bypass not allowed setting not consistent in parallel systems.");	break;
				case 42: strcpy(warn, "Converter setting not consistent in parallel systems.");	break;
				case 43: strcpy(warn, "High loss point for frequency in bypass mode not consistent in parallel systems.");	break;
				case 44: strcpy(warn, "Low loss point for frequency in bypass mode not consistent in parallel systems.");	break;
				case 45: strcpy(warn, "High loss point for voltage in bypass mode not consistent in parallel systems.");	break;
				case 46: strcpy(warn, "Low loss point for voltage in bypass mode not consistent in parallel systems.");	break;
				case 47: strcpy(warn, "High loss point for frequency in AC mode not consistent in parallel systems.");	break;
				case 48: strcpy(warn, "Low loss point for frequency in AC mode not consistent in parallel systems.");	break;
				case 49: strcpy(warn, "High loss point for voltage in AC mode not consistent in parallel systems.");	break;
				case 50: strcpy(warn, "Low loss point for voltage in AC mode not consistent in parallel systems.");	break;
				case 51: strcpy(warn, "Warning for locking in bypass mode after 3 consecutive overloads within 30 min.");	break;
				case 52: strcpy(warn, "Warning for three-phase AC input current unbalance.");	break;
				case 53: strcpy(warn, "Warning for a three-phase input current unbalance detected in battery mode.");	break;
				case 54: strcpy(warn, "Warning for Inverter inter-current unbalance.");	break;
				case 55: strcpy(warn, "Programmable outlets cut off pre-alarm.");	break;
				case 56: strcpy(warn, "Warning for Battery replace.");	status = "RB";	break;
				case 57: strcpy(warn, "Abnormal warning on input phase angle.");	break;
				case 58: strcpy(warn, "Warning!! Cover of maintain switch is open.");	break;
				case 62: strcpy(warn, "EEPROM operation error.");	break;
				default: snprintf(warn, sizeof(warn), "Unknown warning from UPS [bit: #%02d]", i);	u++;	break;
			}

			if (status) {
				if (!strstr(status_buf, status)) {	/* Don't store status, if already set */
					snprintfcat(status_buf, sizeof(status_buf), " %s", status);
				}
			}

			upslogx(LOG_INFO, "Warning from UPS: %s", warn);

			if (u) {	/* Unknown warnings */
				snprintfcat(unk, sizeof(unk), ", #%02d", i);
			} else {	/* Known warnings */
				if (strlen(warns) > 0) {
					snprintfcat(bitwarns, sizeof(bitwarns), ", #%02d", i);	/* For too long warnings (total) */
					snprintfcat(warns, sizeof(warns), " %s", warn);	/* For warnings (total) not too long */
				} else {
					snprintf(bitwarns, sizeof(bitwarns), "Known (see log or manual) [bit: #%02d", i);
					snprintf(warns, sizeof(warns), "%s", warn);
				}
			}
		}
	}

	if (strlen(warns) > 0) {
		if (strlen(unk) > 0) {	/* Both known and unknown warnings */
			snprintfcat(warns, sizeof(warns), " Unknown warnings [bit:%s]", unk+1);	/* Removing leading comma from unk */
			snprintfcat(bitwarns, sizeof(bitwarns), "]; Unknown warnings [bit:%s]", unk+1);	/* Removing leading comma from unk */
		} else {	/* Only known warnings */
			snprintfcat(bitwarns, sizeof(bitwarns), "]");
		}
	} else if (strlen(unk) > 0) {	/* Only unknown warnings */
		snprintf(warns, sizeof(warns), "Unknown warnings [bit:%s]", unk+1);	/* Removing leading comma from unk */
		strcpy(bitwarns, warns);
	}

	if ((ST_MAX_VALUE_LEN - strlen(alarm_buf)) > strlen(warns)) {
		snprintfcat(alarm_buf, sizeof(alarm_buf), " %s", warns);	/* If grand total of previous alarms + warnings doesn't exceed value of alarm_buf (=ST_MAX_VALUE_LEN) */
	} else {
		snprintfcat(alarm_buf, sizeof(alarm_buf), " %s", bitwarns);
	}

	return 0;
}

/* voltronic_mode() - Check working mode reported by UPS */
static int voltronic_mode(void)
{
	char	buf[SMALLBUF], *status = NULL, *alarm = NULL, *removestatus = NULL, status_buffer[SMALLBUF] = "";

	/*
	 * > [QMOD\r]
	 * < [(S\r]
	 *    012
	 *    0
	 */

	if (voltronic_command("QMOD\r", buf, sizeof(buf)) < 3) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for mode rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	switch(buf[1]){
		case 'P': alarm = "UPS is going ON";	break;
		case 'S': status = "OFF";	break;
		case 'Y': status = "BYPASS";	break;
		case 'L': status = "OL";	removestatus = " OB";	break;
		case 'B': status = "OB";	removestatus = " OL";	break;
		case 'T': status = "CAL";	break;
		case 'F': alarm = "Fault reported by UPS.";	break;
		case 'E': alarm = "UPS is in ECO Mode.";	break;
		case 'C': alarm = "UPS is in Converter Mode.";	break;
		case 'D': alarm = "UPS is shutting down!";	status = "FSD";	break;
		default: upsdebugx(2, "%s: invalid reply from UPS [%s]", __func__, strtok(buf+1, "\r\n"));	return -1;	break;
	}

	if (alarm && !strstr(alarm_buf, alarm)) {	/* Don't store alarm, if already set */
		snprintfcat(alarm_buf, sizeof(alarm_buf), " %s", alarm);
	}

	if (removestatus && strstr(status_buf, removestatus)) {	/* Remove removestatus from status, if needed */
		if (strlen(status_buf) != strlen(removestatus)) {	/* Case: status = something1+removestatus[+something2] or removestatus+something2 */
			if (strstr(status_buf, removestatus) != status_buf) {	/* Case: status = something1+removestatus[+something2] -> copying something1 to buffer (status_buffer) and then concat something2 */
				snprintf(status_buffer, sizeof(status_buffer), "%.*s", (int)(strstr(status_buf, removestatus) - status_buf), status_buf);
			}	/* Case: status = removestatus+something2 -> cutting removestatus by concat to a blank buffer (status_buffer) something2 */
			snprintfcat(status_buffer, sizeof(status_buffer), "%s", strstr(status_buf, removestatus)+strlen(removestatus));
		}
		strcpy(status_buf, status_buffer);	/* Case: status == removestatus -> copying in status a blank buffer (status_buffer) */
	}

	if (status && !strstr(status_buf, status)) {	/* Don't store status, if already set */
		snprintfcat(status_buf, sizeof(status_buf), " %s", status);
	}

	return 0;
}

/* voltronic_status() - Check UPS status */
static int voltronic_status(void)
{
	const struct {
		const char	*var;
		const char	*fmt;
		double	(*conv)(const char *, char **);
	} status[] = {
		{ "input.voltage", "%.1f", strtod },
		{ "input.frequency", "%.1f", strtod },
		{ "output.voltage", "%.1f", strtod },
		{ "output.frequency", "%.1f", strtod },
		{ "output.current", "%.1f", strtod },
		{ "ups.load", "%.0f", strtod },
		{ "unknown.1", "%.1f", strtod },	/* unknown - maybe DC Bus Voltage (for non programmable outlets?) */
		{ "unknown.2", "%.1f", strtod },	/* unknown - maybe DC Bus Voltage (for programmable outlets?) */
		{ "battery.voltage", "%.2f", strtod },
		{ "unknown.3", "%.1f", strtod },	/* unknown */
		{ "ups.temperature", "%.1f", strtod },
		{ NULL }
	};

	char	buf[SMALLBUF], *val, *last = NULL;
	int	i, ret;

	/*
	 * > [QGS\r]
	 * < [(234.9 50.0 229.8 50.0 000.0 000 369.1 ---.- 026.5 ---.- 018.8 100000000001\r]
	 *    0123456789012345678901234567890123456789012345678901234567890123456789012345
	 *    0         1         2         3         4         5         6         7
	 */
	if ((voltronic_command("QGS\r", buf, sizeof(buf)) < 76) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for status rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	for (i = 0, val = strtok_r(buf+1, " ", &last); status[i].var; i++, val = strtok_r(NULL, " \r\n", &last)) {

		if (i == 6 || i == 7 || i == 9) {	/* Bypassing unknown values */
			continue;
		}

		if (!val) {
			upsdebugx(2, "%s: parsing failed", __func__);
			return -1;
		}

		if (strspn(val, "0123456789.") != strlen(val)) {
			upsdebugx(2, "%s: non numerical value [%s]", __func__, val);
			continue;
		}

		dstate_setinfo(status[i].var, status[i].fmt, status[i].conv(val, NULL));

	}

	if (!val) {
		upsdebugx(2, "%s: parsing failed", __func__);
		return -1;
	}

	if (strspn(val, "01") != 12) {
		upsdebugx(2, "Invalid status [%s]", val);
		return -1;
	}

	if (alarm_control) {	/* The UPS has the ability to enable/disable the alarm (from UPS capability) */
		const char	*beeper;
		beeper = dstate_getinfo("ups.beeper.status");
		if (!beeper || (beeper && strcmp(beeper, "disabled"))) {
			if (val[9] == '0') {	/* Beeper On */
				dstate_setinfo("ups.beeper.status", "enabled");
			} else {
				dstate_setinfo("ups.beeper.status", "muted");
			}
		}
	} else {	/* The UPS lacks the ability to enable/disable the alarm (from UPS capability) */
		if (val[9] == '0') {	/* Beeper On */
			dstate_setinfo("ups.beeper.status", "enabled");
		} else {
			dstate_setinfo("ups.beeper.status", "disabled");
		}
	}

	if (!strncmp(val, "00", 2)) {	/* UPS Type: 00 -> Offline | 01 -> Line-interactive | 10 -> Online */
		dstate_setinfo("ups.type", "offline");
	} else if (!strncmp(val, "01", 2)) {
		dstate_setinfo("ups.type", "line-interactive");	
	} else if (!strncmp(val, "10", 2)) {
		dstate_setinfo("ups.type", "online");
	}

	strcpy(status_buf, "");	/* Resetting status buffer */

	if (val[2] == '1') {	/* Utility Fail (Immediate) */
		snprintfcat(status_buf, sizeof(status_buf), " OB");
	} else {
		snprintfcat(status_buf, sizeof(status_buf), " OL");
	}

	if (val[3] == '1') {	/* Battery Low */
		snprintfcat(status_buf, sizeof(status_buf), " LB");
	}

	strcpy(alarm_buf, "");	/* Resetting alarm buffer */

	if (val[4] == '1') {	/* Bypass/Boost or Buck Active */

		double	vi, vo;

		vi = strtod(dstate_getinfo("input.voltage"),  NULL);
		vo = strtod(dstate_getinfo("output.voltage"), NULL);

		if (vo < 0.5 * vi) {
			upsdebugx(2, "%s: output voltage too low", __func__);
		} else if (vo < 0.95 * vi) {
			snprintfcat(status_buf, sizeof(status_buf), " TRIM");
		} else if (vo < 1.05 * vi) {
			const char	*prot;
			prot = dstate_getinfo("ups.firmware.aux");
			if (prot && (!strcmp(prot, "P00") || !strcmp(prot, "P08"))) {
				snprintfcat(alarm_buf, sizeof(alarm_buf), " UPS is in AVR Mode.");
			} else {
				snprintfcat(status_buf, sizeof(status_buf), " BYPASS");
			}
		} else if (vo < 1.5 * vi) {
			snprintfcat(status_buf, sizeof(status_buf), " BOOST");
		} else {
			upsdebugx(2, "%s: output voltage too high", __func__);
		}
	}

	if (val[7] == '1') {	/* Test in Progress */
		snprintfcat(status_buf, sizeof(status_buf), " CAL");
	}

	if (val[5] == '1') {	/* UPS Fault */
		int	ret;
		for (i = 1; i <= MAXTRIES; i++) {
			ret = voltronic_fault();
			if (ret < 0) {
				upsdebugx(1, "Fault type: handling attempt #%d failed", i);
				continue;
			}
			upsdebugx(1, "Fault type handled in %d tries", i);
			break;
		}
		if (ret) {
			upslogx(LOG_DEBUG, "Error while handling fault type");
			snprintfcat(alarm_buf, sizeof(alarm_buf), " UPS Fault! Error while determining fault type.");
		}
	}

	if (val[8] == '1') {	/* Shutdown Active */
		snprintfcat(alarm_buf, sizeof(alarm_buf), " Shutdown imminent!");
		snprintfcat(status_buf, sizeof(status_buf), " FSD");
	}

	/* Warnings & Operational Mode */
	if (!mode) {	/* #1 - voltronic_mode() */
		voltronic_mode();
	} else if (mode == 1) {
		for (i = 1; i <= MAXTRIES; i++) {	/* Only on startup we'll try MAXTRIES times */
			ret = voltronic_mode();
			if (ret < 0) {
				upsdebugx(1, "UPS Mode read #%d failed", i);
				continue;
			}
			upsdebugx(1, "UPS Mode read in %d tries", i);
			mode = 0;	/* If OK -> the next time we'll try only 1 time.. */
			break;
		}
		if (ret) {
			upslogx(LOG_DEBUG, "Error while reading UPS Mode");
			mode = -1;	/* ..otherwise the next times we'll skip */
		}
	}

	if (!warning) {	/* #2 - voltronic_warning() */
		voltronic_warning();
	} else if (warning == 1) {
		for (i = 1; i <= MAXTRIES; i++) {	/* Only on startup we'll try MAXTRIES times */
			ret = voltronic_warning();
			if (ret < 0) {
				upsdebugx(1, "Warning informations read #%d failed", i);
				continue;
			}
			upsdebugx(1, "Warning informations read in %d tries", i);
			warning = 0;	/* If OK -> the next time we'll try only 1 time */
			break;
		}
		if (ret) {
			upslogx(LOG_DEBUG, "Error while reading warning informations");
			warning = -1;	/* ..otherwise the next times we'll skip */
		}
	}

	status_init();
	alarm_init();
	/* Check whether alarm/status buffer have been changed in the current call of this function, if not we won't store the value from buf+1, since we voided just the first char */
	alarm_set(strlen(alarm_buf) > 0 ? alarm_buf+1 : "");	/* Removing leading white space from alarm_buf */
	status_set(strlen(status_buf) > 0 ? status_buf+1 : "");	/* Removing leading white space from status_buf */

	alarm_commit();
	status_commit();

	return 0;
}

/* voltronic_capability() - Check whether or not the UPS has some capabilities */
static int voltronic_capability(void)
{
	char	buf[SMALLBUF], *enabled, *disabled, *last = NULL;

	/* Total options available: 21, only those whom the UPS is capable of are reported as Enabled or Disabled
	 * > [QFLAG\r]
	 * < [(EpashcDbroegfl\r]
	 *    0123456789012345
	 *    0         1	* min length = ( + E + D + \r = 4
	 */

	if ((voltronic_command("QFLAG\r", buf, sizeof(buf)) < 4) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for capability rejected by UPS", __func__);
		return -1;
	}

	enabled = strtok_r(buf+2, "D", &last);
	disabled = strtok_r(NULL, "\r\n", &last);

	if (!enabled && !disabled) {
		upsdebugx(2, "%s: parsing failed", __func__);
		return -1;
	}

	enabled = enabled ? enabled : "";
	disabled = disabled ? disabled : "";	

	/* I'll treat the following options as variables configurable in ups.conf.
	 * If you want to change the value of these options, simply add the
	 * corresponding variable & value in ups.conf. The driver will check
	 * on startup and then tell the UPS to change them. [see also voltronic_check()]
	 */
	if (strchr(enabled, 'p')) {
		bypass_alarm = "enabled";
	} else if (strchr(disabled, 'p')) {
		bypass_alarm = "disabled";
	}
	if (strchr(enabled, 'b')) {
		battery_alarm = "enabled";
	} else if (strchr(disabled, 'b')) {
		battery_alarm = "disabled";
	}
	if (strchr(enabled, 'r')) {
		dstate_setinfo("ups.start.auto", "yes");
	} else if (strchr(disabled, 'r')) {
		dstate_setinfo("ups.start.auto", "no");
	}
	if (strchr(enabled, 'o')) {
		bypass_when_off = "enabled";
	} else if (strchr(disabled, 'o')) {
		bypass_when_off = "disabled";
	}
	if (strchr(enabled, 'a')) {
		const char	*beeper;
		alarm_control = "enabled";
		beeper = dstate_getinfo("ups.beeper.status");
		if (!beeper || (beeper && strcmp(beeper, "muted"))) {
			dstate_setinfo("ups.beeper.status", "enabled");
		}
		dstate_addcmd("beeper.enable");
		dstate_addcmd("beeper.disable");
	} else if (strchr(disabled, 'a')) {
		alarm_control = "disabled";
		dstate_setinfo("ups.beeper.status", "disabled");
		dstate_addcmd("beeper.enable");
		dstate_addcmd("beeper.disable");
	}
	if (strchr(enabled, 's')) {
		dstate_setinfo("battery.protection", "yes");
	} else if (strchr(disabled, 's')) {
		dstate_setinfo("battery.protection", "no");
	}
	if (strchr(enabled, 'v')) {
		converter_mode = "enabled";
	} else if (strchr(disabled, 'v')) {
		converter_mode = "disabled";
	}
	if (strchr(enabled, 'e')) {
		eco_mode = "enabled";
		dstate_addcmd("bypass.start");
		dstate_addcmd("bypass.stop");
	} else if (strchr(disabled, 'e')) {
		eco_mode = "disabled";
		dstate_addcmd("bypass.start");
		dstate_addcmd("bypass.stop");
	}
	if (strchr(enabled, 'g')) {
		dstate_setinfo("battery.energysave", "yes");
	} else if (strchr(disabled, 'g')) {
		dstate_setinfo("battery.energysave", "no");
	}
	if (strchr(enabled, 'd')) {
		battery_open_status_check = "enabled";
	} else if (strchr(disabled, 'd')) {
		battery_open_status_check = "disabled";
	}
/*	if (strchr(enabled, 'h')) {	unknown/unused
	} else if (strchr(disabled, 'h')) { }		*/
	if (strchr(enabled, 'c')) {
		dstate_setinfo("ups.start.battery", "yes");
	} else if (strchr(disabled, 'c')) {
		dstate_setinfo("ups.start.battery", "no");
	}
	if (strchr(enabled, 'f')) {
		bypass_forbidding = "enabled";
	} else if (strchr(disabled, 'f')) {
		bypass_forbidding = "disabled";
	}
/*	if (strchr(enabled, 't')) { 	unknown/unused
	} else if (strchr(disabled, 't')) { }		*/
	if (strchr(enabled, 'j')) {
		dstate_setinfo("outlet.0.switchable", "yes");
	} else if (strchr(disabled, 'j')) {
		dstate_setinfo("outlet.0.switchable", "no");
	}
/*	if (strchr(enabled, 'k')) { 	unknown/unused
	} else if (strchr(disabled, 'k')) { }		*/
/*	if (strchr(enabled, 'i')) { 	unknown/unused
	} else if (strchr(disabled, 'i')) { }		*/
	if (strchr(enabled, 'l')) {
		site_fault_detection = "enabled";
	} else if (strchr(disabled, 'l')) {
		site_fault_detection = "disabled";
	}
	if (strchr(enabled, 'n')) {
		advanced_eco_mode = "enabled";
	} else if (strchr(disabled, 'n')) {
		advanced_eco_mode = "disabled";
	}
	if (strchr(enabled, 'q')) {
		constant_phase_angle = "enabled";
	} else if (strchr(disabled, 'q')) {
		constant_phase_angle = "disabled";
	}
	if (strchr(enabled, 'w')) {
		limited_runtime_on_battery = "enabled";
	} else if (strchr(disabled, 'w')) {
		limited_runtime_on_battery = "disabled";
	}
/*	if (strchr(enabled, 'm')) { 	unknown/unused
	} else if (strchr(disabled, 'm')) { }		*/
/*	if (strchr(enabled, 'z')) { 	unknown/unused
	} else if (strchr(disabled, 'z')) { }		*/

	return 0;
}

/* voltronic_check() - Try and change UPS capability according to user configuration in ups.conf */
static int voltronic_check(void)	/* This function returns 0 on success, rtn if provided value in ups.conf isn't acceptable and for errors while changing options in UPS, 1 if UPS isn't OFF and it can't reset options, -1 on other errors */
{
	const struct {
		const char	*type;	/* Name of the option, used also to get the value from ups.conf */
		const char	*acr;	/* The corresponding acronym used by UPS */
		const char	*match;	/* The value reported by UPS */
	} capability[] = {
		{ "bypass_alarm", "P", bypass_alarm },
		{ "battery_alarm", "B", battery_alarm },
		{ "auto_reboot", "R", dstate_getinfo("ups.start.auto")  },
		{ "bypass_when_off", "O", bypass_when_off },
		{ "alarm_control", "A", alarm_control },
		{ "battery_protection", "S", dstate_getinfo("battery.protection") },
		{ "converter_mode", "V", converter_mode },
		{ "eco_mode", "E", eco_mode },
		{ "energy_saving", "G", dstate_getinfo("battery.energysave") },
		{ "battery_open_status_check", "D", battery_open_status_check },
/*		{ "unknown", "H", unknown },	unknown/unused	*/
		{ "cold_start", "C", dstate_getinfo("ups.start.battery") },
		{ "bypass_forbidding", "F", bypass_forbidding },
/*		{ "unknown", "T", unknown },	unknown/unused	*/
		{ "outlet_control", "J", dstate_getinfo("outlet.0.switchable") },
/*		{ "unknown", "K", unknown },	unknown/unused	*/
/*		{ "unknown", "I", unknown },	unknown/unused	*/
		{ "site_fault_detection", "L", site_fault_detection },
		{ "advanced_eco_mode", "N", advanced_eco_mode },
		{ "constant_phase_angle", "Q", constant_phase_angle },
		{ "limited_runtime_on_battery", "W", limited_runtime_on_battery },
/*		{ "unknown", "M", unknown },	unknown/unused	*/
/*		{ "unknown", "Z", unknown },	unknown/unused	*/
		{ NULL }
	};

	char	buf[SMALLBUF] = "", buf2[SMALLBUF] = "", match[SMALLBUF], *val;
	const char	*act_status;
	int	i, ret, rtn = 0, check = 0;

	if (testvar("reset_to_default")) {	/* If reset_to_default is set in ups.conf, the function will skip other steps */
		act_status = dstate_getinfo("ups.status") ? dstate_getinfo("ups.status") : "unset";
		if (!strstr(act_status, "OFF")) {	/* UPS capability options can be reset only when the UPS is in 'Standby Mode' (=OFF) (from QMOD) */
			upslogx(LOG_ERR, "UPS capability options can be reset only when the UPS is in Standby Mode");
			return 1;
		}
		voltronic_command("PF\r", buf, sizeof(buf));
		if (!strncasecmp(buf, "(ACK", 4)) {
			upslogx(LOG_INFO, "UPS capability: UPS successfully reset options to safe default values");
			check++;
		} else {
			upslogx(LOG_ERR, "UPS capability: UPS failed to reset options to safe default values");
			return -1;
		}
	} else {
		for (i = 0; capability[i].type; i++) {
			if (!capability[i].match) {
				upslogx(LOG_INFO, "UPS capability: the UPS can't handle the change of [%s] option", capability[i].type);
				continue;
			}

			if (!strcmp(capability[i].match, "no")) {
				strcpy(match, "disabled");
			} else if (!strcmp(capability[i].match, "yes")) {
				strcpy(match, "enabled");
			} else {
				strcpy(match, capability[i].match);
			}

			/* Logging current state of each option whom the UPS is capable of */
			upslogx(LOG_INFO, "UPS capability: [%s] option is currently %s", capability[i].type, match);

			val = getval(capability[i].type);

			if (!val) {	/* Skipping to the next option if this one is not set in ups.conf */
				continue;
			}

			if (strcasecmp(val, match)) {
				if (!strcasecmp(val, "disabled")) {
					snprintf(buf, sizeof(buf), "PD%s\r", capability[i].acr);
				} else if (!strcasecmp(val, "enabled")) {
					snprintf(buf, sizeof(buf), "PE%s\r", capability[i].acr);
				} else {
					upslogx(LOG_ERR, "UPS capability - %s: [%s] is not within acceptable values [enabled/disabled]", capability[i].type, val);
					rtn++;
					continue;
				}
			} else {
				upslogx(LOG_INFO, "UPS capability: %s is already %s", capability[i].type, match);
				continue;
			}

			voltronic_command(buf, buf2, sizeof(buf2));

			if (!strncasecmp(buf2, "(ACK", 4)) {
				upslogx(LOG_INFO, "UPS capability: UPS changed successfully [%s] option from [%s] to [%s]", capability[i].type, match, val);
				check++;
			} else {
				upslogx(LOG_ERR, "UPS capability: UPS failed to change [%s] option from [%s] to [%s]", capability[i].type, match, val);
				rtn++;
			}
		}
	}

	if (!check) {	/* No options have been changed -> no need to update capability */
		return rtn;
	}

	/* Updating NUT vars, if needed */
	for (i = 1; i <= MAXTRIES; i++) {
		ret = voltronic_capability();
		if (ret < 0) {
			upsdebugx(1, "UPS capability updating attempt #%d failed", i);
			continue;
		}
		upslogx(LOG_INFO, "UPS capability updated in %d tries", i);
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Error while updating UPS capability");
		return -1;
	}

	return rtn;
}

/* voltronic_eco()
 * Check voltage/frequency limits for ECO Mode (aka FRE = Free Run Energy) = energy saving = bypass voltage to ouput if within acceptable range
 * and try and change them according to values provided by user in ups.conf
 */
static int voltronic_eco(void)	/* This functions returns -1 on errors, 0 on success, -2 if ECO Mode is not supported by UPS, rtn>0 if non-blocking errors occur */
{
	char	buf[SMALLBUF] = "", buf2[SMALLBUF] = "", *val, *last = NULL;
	const char	*prot;

	struct {
		struct {
			struct {
				int	act;	/* Actual maximum voltage for ECO Mode */
				int	match;	/* Maximum voltage for ECO Mode set in ups.conf */
				struct {
					int	lower;	/* Lower limit */
					int	upper;	/* Upper limit */
				} limits;	/* Range of accepted values for maximum voltage for ECO Mode */
			} max;
			struct {
				int	act;	/* Actual minimum voltage for ECO Mode */
				int	match;	/* Minimum voltage for ECO Mode set in ups.conf */
				struct {
					int	lower;	/* Lower limit */
					int	upper;	/* Upper limit */
				} limits;	/* Range of accepted values for minimum voltage for ECO Mode */
			} min;
		} volt;
		struct {
			struct {
				double	act;	/* Actual maximum frequency for ECO Mode */
				double	match;	/* Maximum frequency for ECO Mode set in ups.conf */
				struct {
					double	lower;	/* Lower limit */
					double	upper;	/* Upper limit */
				} limits;	/* Range of accepted values for maximum frequency for ECO Mode */
			} max;
			struct {
				double	act;	/* Actual minimum frequency for ECO Mode */
				double	match;	/* Minimum frequency for ECO Mode set in ups.conf */
				struct {
					double	lower;	/* Lower limit */
					double	upper;	/* Upper limit */
				} limits;	/* Range of accepted values for minimum frequency for ECO Mode */
			} min;
		} freq;
	} eco;

	int	ovn, rtn = 0;
	double	ofn;

	/* ECO Mode - Voltage limits
	 * > [QHE\r]
	 * < [(242 218\r]
	 *    012345678
	 *    0
	 */

	if ((voltronic_command("QHE\r", buf, sizeof(buf)) < 9) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for maximum and minimum voltage for ECO Mode rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (strspn(buf+1, "0123456789 ") != (strlen(buf)-2)) {
		upsdebugx(2, "%s: non numerical value [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	eco.volt.max.act = atoi(strtok_r(buf+1, " ", &last) ? buf+1 : "-1");
	val = strtok_r(NULL, " \r\n", &last);
	eco.volt.min.act = atoi(val ? val : "-1");

	if ((eco.volt.max.act < 0) || (eco.volt.min.act < 0)) {
		upsdebugx(2, "%s: parsing failed", __func__);
		return -1;
	}

	ovn = atoi(dstate_getinfo("output.voltage.nominal") ? dstate_getinfo("output.voltage.nominal") : "-1");

	if (ovn == -1) {	/* Since query for ratings (QRI) is not mandatory to run this driver, skip next steps if we can't get the value of output voltage nominal */
		upsdebugx(2, "%s: unable to get output voltage nominal", __func__);
		return -1;
	}

	prot = dstate_getinfo("ups.firmware.aux");

	if (!strcmp(prot, "P01") || !strcmp(prot, "P09")) {
	/* For P01/P09 */
		if (ovn >= 200) {
			eco.volt.min.limits.lower = ovn - 24;
			eco.volt.min.limits.upper = ovn - 7;
			eco.volt.max.limits.lower = ovn + 7;
			eco.volt.max.limits.upper = ovn + 24;
		} else {
			eco.volt.min.limits.lower = ovn - 12;
			eco.volt.min.limits.upper = ovn - 3;
			eco.volt.max.limits.lower = ovn + 3;
			eco.volt.max.limits.upper = ovn + 12;
		}
	} else if (!strcmp(prot, "P02") || !strcmp(prot, "P03") || !strcmp(prot, "P10") || !strcmp(prot, "P14") || !strcmp(prot, "P99")) {
	/* For P02/P03/P10/P14/P99 */
		if (ovn >= 200) {
			eco.volt.min.limits.lower = ovn - 24;
			eco.volt.min.limits.upper = ovn - 11;
			eco.volt.max.limits.lower = ovn + 11;
			eco.volt.max.limits.upper = ovn + 24;
		} else {
			eco.volt.min.limits.lower = ovn - 12;
			eco.volt.min.limits.upper = ovn - 5;
			eco.volt.max.limits.lower = ovn + 5;
			eco.volt.max.limits.upper = ovn + 12;
		}
	} else {
	/* For P00/P08/P31 */
		upslogx(LOG_INFO, "Your UPS doesn't support ECO Mode");
		return -2;
	}

	/* Let the people know actual values and limits for max/min voltage for ECO Mode */
	upslogx(LOG_INFO, "ECO Mode - Voltage limits - Accepted range of values (V): minimum [%03d-%03d] / maximum [%03d-%03d]", eco.volt.min.limits.lower, eco.volt.min.limits.upper, eco.volt.max.limits.lower, eco.volt.max.limits.upper);
	upslogx(LOG_INFO, "ECO Mode - Voltage limits - Actual values (V): minimum [%03d] / maximum [%03d]", eco.volt.min.act, eco.volt.max.act);

	/* Populating NUT variables */
	dstate_setinfo("input.transfer.low", "%03d", eco.volt.min.act);
	dstate_setinfo("input.transfer.high", "%03d", eco.volt.max.act);
	dstate_setinfo("input.transfer.low.min", "%03d", eco.volt.min.limits.lower);
	dstate_setinfo("input.transfer.low.max", "%03d", eco.volt.min.limits.upper);
	dstate_setinfo("input.transfer.high.min", "%03d", eco.volt.max.limits.lower);
	dstate_setinfo("input.transfer.high.max", "%03d", eco.volt.max.limits.upper);

	if (testvar("reset_to_default")) {	/* If reset_to_default is set in ups.conf, the function won't change voltage limits */
		upslogx(LOG_INFO, "Voltage Limits for ECO Mode won't be changed (reset_to_default option)");
	} else {
		val = getval("max_eco_volt");

		if (val && strspn(val, "0123456789") != strlen(val)) {
			upslogx(LOG_ERR, "Maximum voltage for Eco Mode (max_eco_volt): non numerical value [%s]", val);
			val = "-1";
			rtn++;
		}

		eco.volt.max.match = val ? atoi(val) : -1;

		val = getval("min_eco_volt");

		if (val && strspn(val, "0123456789") != strlen(val)) {
			upslogx(LOG_ERR, "Minimum voltage for Eco Mode (min_eco_volt): non numerical value [%s]", val);
			val = "-1";
			rtn++;
		}

		eco.volt.min.match = val ? atoi(val) : -1;

		/* Try and change values according to vars set in ups.conf */
		if ((eco.volt.max.match != -1) && (eco.volt.max.act != eco.volt.max.match)) {
			if ((eco.volt.max.limits.lower <= eco.volt.max.match) && (eco.volt.max.match <= eco.volt.max.limits.upper)) {
				snprintf(buf, sizeof(buf), "HEH%03d\r", eco.volt.max.match);
				voltronic_command(buf, buf2, sizeof(buf2));
				if (!strncasecmp(buf2, "(ACK", 4)) {
					upslogx(LOG_INFO, "Maximum voltage for ECO Mode changed from %03d V to %03d V", eco.volt.max.act, eco.volt.max.match);
					dstate_setinfo("input.transfer.high", "%03d", eco.volt.max.match);
				} else {
					upslogx(LOG_ERR, "Failed to change maximum voltage for ECO Mode from %03d V to %03d V", eco.volt.max.act, eco.volt.max.match);
					rtn++;
				}
			} else {
				upslogx(LOG_ERR, "Value for maximum voltage for ECO Mode (max_eco_volt) [%03d] V out of range of accepted values [%03d-%03d] V", eco.volt.max.match, eco.volt.max.limits.lower, eco.volt.max.limits.upper);
				rtn++;
			}
		}

		if ((eco.volt.min.match != -1) && (eco.volt.min.act != eco.volt.min.match)) {
			if ((eco.volt.min.limits.lower <= eco.volt.min.match) && (eco.volt.min.match <= eco.volt.min.limits.upper)) {
				snprintf(buf, sizeof(buf), "HEL%03d\r", eco.volt.min.match);
				voltronic_command(buf, buf2, sizeof(buf2));
				if (!strncasecmp(buf2, "(ACK", 4)) {
					upslogx(LOG_INFO, "Minimum voltage for ECO Mode changed from %03d V to %03d V", eco.volt.min.act, eco.volt.min.match);
					dstate_setinfo("input.transfer.low", "%03d", eco.volt.min.match);
				} else {
					upslogx(LOG_ERR, "Failed to change minimum voltage for ECO Mode from %03d V to %03d V", eco.volt.min.act, eco.volt.min.match);
					rtn++;
				}
			} else {
				upslogx(LOG_ERR, "Value for minimum voltage for ECO Mode (min_eco_volt) [%03d] V out of range of accepted values [%03d-%03d] V", eco.volt.min.match, eco.volt.min.limits.lower, eco.volt.min.limits.upper);
				rtn++;
			}
		}
	}

	/* ECO Mode - Frequency Limits
	 * > [QFRE\r]
	 * < [(53.0 47.0\r]
	 *    01234567890
	 *    0         1
	 */

	if ((voltronic_command("QFRE\r", buf, sizeof(buf)) < 11) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for maximum and minimum frequency for ECO Mode rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (strspn(buf+1, "0123456789. ") != (strlen(buf)-2)) {
		upsdebugx(2, "%s: non numerical value [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	eco.freq.max.act = strtod(strtok_r(buf+1, " ", &last) ? buf+1 : "-1", NULL);
	val = strtok_r(NULL, " \r\n", &last);
	eco.freq.min.act = strtod(val ? val : "-1", NULL);

	if ((eco.freq.max.act < 0) || (eco.freq.min.act < 0)) {
		upsdebugx(2, "%s: parsing failed", __func__);
		return -1;
	}

	ofn = strtod(dstate_getinfo("output.frequency.nominal") ? dstate_getinfo("output.frequency.nominal") : "-1", NULL);

	if (ofn == -1) {	/* Since query for ratings (QRI) is not mandatory to run this driver, skip next steps if we can't get the value of output frequency nominal */
		upsdebugx(2, "%s: unable to get output frequency nominal", __func__);
		return -1;
	}

	if (!strcmp(prot, "P01") || !strcmp(prot, "P09")) {
	/* For P01/P09 */
		if (ofn == 60.0) {
			eco.freq.min.limits.lower = 50.0;
			eco.freq.min.limits.upper = 57.0;
			eco.freq.max.limits.lower = 63.0;
			eco.freq.max.limits.upper = 70.0;
		} else if (ofn == 50.0) {
			eco.freq.min.limits.lower = 40.0;
			eco.freq.min.limits.upper = 47.0;
			eco.freq.max.limits.lower = 53.0;
			eco.freq.max.limits.upper = 60.0;
		} else {
			return -1;
		}
	} else if (!strcmp(prot, "P02") || !strcmp(prot, "P03") || !strcmp(prot, "P10") || !strcmp(prot, "P14") || !strcmp(prot, "P99")) {
	/* For P02/P03/P10/P14/P99 */
		if (ofn == 60.0) {
			eco.freq.min.limits.lower = 56.0;
			eco.freq.min.limits.upper = 58.0;
			eco.freq.max.limits.lower = 62.0;
			eco.freq.max.limits.upper = 64.0;
		} else if (ofn == 50.0) {
			eco.freq.min.limits.lower = 46.0;
			eco.freq.min.limits.upper = 48.0;
			eco.freq.max.limits.lower = 52.0;
			eco.freq.max.limits.upper = 54.0;
		} else {
			return -1;
		}
	} else {
	/* For P00/08/P31 */
		upslogx(LOG_INFO, "Your UPS doesn't support ECO Mode");
		return -2;
	}

	/* Let the people know actual values and limits for max/min frequency for Bypass Mode */
	upslogx(LOG_INFO, "ECO Mode - Frequency limits - Accepted range of values (Hz): minimum [%04.1f-%04.1f] / maximum [%04.1f-%04.1f]", eco.freq.min.limits.lower, eco.freq.min.limits.upper, eco.freq.max.limits.lower, eco.freq.max.limits.upper);
	upslogx(LOG_INFO, "ECO Mode - Frequency limits - Actual values (Hz): minimum [%04.1f] / maximum [%04.1f]", eco.freq.min.act, eco.freq.max.act);

	/* Populating NUT variables */
	dstate_setinfo("input.frequency.low", "%04.1f", eco.freq.min.act);
	dstate_setinfo("input.frequency.high", "%04.1f", eco.freq.max.act);

	if (testvar("reset_to_default")) {	/* If reset_to_default is set in ups.conf, the function won't change frequency limits */
		upslogx(LOG_INFO, "Frequency Limits for ECO Mode won't be changed (reset_to_default option)");
	} else {
		val = getval("max_eco_freq");

		if (val && strspn(val, "0123456789.") != strlen(val)) {
			upslogx(LOG_ERR, "Maximum frequency for ECO Mode (max_eco_freq): non numerical value [%s]", val);
			val = "-1";
			rtn++;
		}

		eco.freq.max.match = val ? strtod(val, NULL) : -1;

		val = getval("min_eco_freq");

		if (val && strspn(val, "0123456789.") != strlen(val)) {
			upslogx(LOG_ERR, "Minimum frequency for ECO Mode (min_eco_freq): non numerical value [%s]", val);
			val = "-1";
			rtn++;
		}

		eco.freq.min.match = val ? strtod(val, NULL) : -1;

		/* Try and change values according to vars set in ups.conf */
		if ((eco.freq.max.match != -1) && (eco.freq.max.act != eco.freq.max.match)) {
			if ((eco.freq.max.limits.lower <= eco.freq.max.match) && (eco.freq.max.match <= eco.freq.max.limits.upper)) {
				snprintf(buf, sizeof(buf), "FREH%04.1f\r", eco.freq.max.match);
				voltronic_command(buf, buf2, sizeof(buf2));
				if (!strncasecmp(buf2, "(ACK", 4)) {
					upslogx(LOG_INFO, "Maximum frequency for ECO Mode changed from %04.1f Hz to %04.1f Hz", eco.freq.max.act, eco.freq.max.match);
					dstate_setinfo("input.frequency.high", "%04.1f", eco.freq.max.match);
				} else {
					upslogx(LOG_ERR, "Failed to change maximum frequency for ECO Mode from %04.1f Hz to %04.1f Hz", eco.freq.max.act, eco.freq.max.match);
					rtn++;
				}
			} else {
				upslogx(LOG_ERR, "Value for maximum frequency for ECO Mode (max_eco_freq) [%04.1f] Hz out of range of accepted values [%04.1f-%04.1f] Hz", eco.freq.max.match, eco.freq.max.limits.lower, eco.freq.max.limits.upper);
				rtn++;
			}
		}

		if ((eco.freq.min.match != -1) && (eco.freq.min.act != eco.freq.min.match)) {
			if ((eco.freq.min.limits.lower <= eco.freq.min.match) && (eco.freq.min.match <= eco.freq.min.limits.upper)) {
				snprintf(buf, sizeof(buf), "FREL%04.1f\r", eco.freq.min.match);
				voltronic_command(buf, buf2, sizeof(buf2));
				if (!strncasecmp(buf2, "(ACK", 4)) {
					upslogx(LOG_INFO, "Minimum frequency for ECO Mode changed from %04.1f Hz to %04.1f Hz", eco.freq.min.act, eco.freq.min.match);
					dstate_setinfo("input.frequency.low", "%04.1f", eco.freq.min.match);
				} else {
					upslogx(LOG_ERR, "Failed to change minimum frequency for ECO Mode from %04.1f Hz to %04.1f Hz", eco.freq.min.act, eco.freq.min.match);
					rtn++;
				}
			} else {
				upslogx(LOG_ERR, "Value for minimum frequency for ECO Mode (min_eco_freq) [%04.1f] Hz out of range of accepted values [%04.1f-%04.1f] Hz", eco.freq.min.match, eco.freq.min.limits.lower, eco.freq.min.limits.upper);
				rtn++;
			}
		}
	}
	
	return rtn;
}

/* voltronic_bypass()
 * Check voltage/frequency limits for Bypass Mode (if UPS is overload and input voltage/frequency
 * are within acceptable range, UPS will enter bypass mode or bypass mode can be set through front panel)
 * and try and change them according to values provided by user in ups.conf
 */
static int voltronic_bypass(void)	/* This function returns -1 on errors, 0 on success, -2 if Bypass Mode is not supported by UPS, rtn>0 if non-blocking errors occur */
{
	char	buf[SMALLBUF] = "", buf2[SMALLBUF] = "", *val, *last = NULL;
	const char	*prot;

	struct {
		struct {
			struct {
				int	act;	/* Actual maximum voltage for Bypass Mode */
				int	match;	/* Maximum voltage for Bypass Mode set in ups.conf */
				struct {
					int	lower;	/* Lower limit */
					int	upper;	/* Upper limit */
				} limits;	/* Range of accepted values for maximum voltage for Bypass Mode */
			} max;
			struct {
				int	act;	/* Actual minimum voltage for Bypass Mode */
				int	match;	/* Minimum voltage for Bypass Mode set in ups.conf */
				struct {
					int	lower;	/* Lower limit */
					int	upper;	/* Upper limit */
				} limits;	/* Range of accepted values for minimum voltage for Bypass Mode */
			} min;
		} volt;
		struct {
			struct {
				double	act;	/* Actual maximum frequency for Bypass Mode */
				double	match;	/* Maximum frequency for Bypass Mode set in ups.conf */
				struct {
					double	lower;	/* Lower limit */
					double	upper;	/* Upper limit */
				} limits;	/* Range of accepted values for maximum frequency for Bypass Mode */
			} max;
			struct {
				double	act;	/* Actual minimum frequency for Bypass Mode */
				double	match;	/* Minimum frequency for Bypass Mode set in ups.conf */
				struct {
					double	lower;	/* Lower limit */
					double	upper;	/* Upper limit */
				} limits;	/* Range of accepted values for minimum frequency for Bypass Mode */
			} min;
		} freq;
	} bypass;

	int	ivn, rtn = 0;
	double	ofn;

	/* Bypass Mode - Voltage Limits
	 * > [QBYV\r]
	 * < [(264 170\r]
	 *    012345678
	 *    0
	 */

	if ((voltronic_command("QBYV\r", buf, sizeof(buf)) < 9) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for maximum and minimum voltage for Bypass Mode rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (strspn(buf+1, "0123456789 ") != (strlen(buf)-2)) {
		upsdebugx(2, "%s: non numerical value [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	bypass.volt.max.act = atoi(strtok_r(buf+1, " ", &last) ? buf+1 : "-1");
	val = strtok_r(NULL, " \r\n", &last);
	bypass.volt.min.act = atoi(val ? val : "-1");

	if ((bypass.volt.max.act < 0) || (bypass.volt.min.act < 0)) {
		upsdebugx(2, "%s: parsing failed", __func__);
		return -1;
	}

	ivn = atoi(dstate_getinfo("input.voltage.nominal") ? dstate_getinfo("input.voltage.nominal") : "-1");

	if (ivn == -1) {	/* Since query for ratings (QMD) is not mandatory to run this driver, skip next steps if we can't get the value of input voltage nominal */
		upsdebugx(2, "%s: unable to get input voltage nominal", __func__);
		return -1;
	}

	prot = dstate_getinfo("ups.firmware.aux");

	if (!strcmp(prot, "P01")) {
	/* For P01 */
		if (ivn >= 200) {
			bypass.volt.min.limits.lower = 170;
			bypass.volt.min.limits.upper = 220;
			bypass.volt.max.limits.lower = 230;
			bypass.volt.max.limits.upper = 264;
		} else {
			bypass.volt.min.limits.lower = 85;
			bypass.volt.min.limits.upper = 115;
			bypass.volt.max.limits.lower = 120;
			bypass.volt.max.limits.upper = 140;
		}
	} else if (!strcmp(prot, "P02") || !strcmp(prot, "P03") || !strcmp(prot, "P10") || !strcmp(prot, "P14")) {
	/* For P02/P03/P10/P14 */
		if (ivn >= 200) {
			bypass.volt.min.limits.lower = 110;
			bypass.volt.min.limits.upper = 209;
			bypass.volt.max.limits.lower = 231;
			bypass.volt.max.limits.upper = 276;
		} else {
			bypass.volt.min.limits.lower = 55;
			bypass.volt.min.limits.upper = 104;
			bypass.volt.max.limits.lower = 115;
			bypass.volt.max.limits.upper = 138;
		}
	} else if (!strcmp(prot, "P09")) {
	/* For P09 */
		bypass.volt.min.limits.lower = 60;
		bypass.volt.min.limits.upper = 140;
		bypass.volt.max.limits.lower = 60;
		bypass.volt.max.limits.upper = 140;
	} else if (!strcmp(prot, "P99")) {
	/* For P99 */
		if (ivn >= 200) {
			bypass.volt.min.limits.lower = 149;
			bypass.volt.min.limits.upper = 209;
			bypass.volt.max.limits.lower = 231;
			bypass.volt.max.limits.upper = 261;
		} else {
			bypass.volt.min.limits.lower = 50;
			bypass.volt.min.limits.upper = 104;
			bypass.volt.max.limits.lower = 115;
			bypass.volt.max.limits.upper = 132;
		}
	} else {
	/* For P00/P08 */
		upslogx(LOG_INFO, "Your UPS doesn't support Bypass Mode");
		return -2;
	}

	/* Let the people know actual values and limits for max/min voltage for Bypass Mode */
	upslogx(LOG_INFO, "Bypass Mode - Voltage limits - Accepted range of values (V): minimum [%03d-%03d] / maximum [%03d-%03d]", bypass.volt.min.limits.lower, bypass.volt.min.limits.upper, bypass.volt.max.limits.lower, bypass.volt.max.limits.upper);
	upslogx(LOG_INFO, "Bypass Mode - Voltage limits - Actual values (V): minimum [%03d] / maximum [%03d]", bypass.volt.min.act, bypass.volt.max.act);

	if (testvar("reset_to_default")) {	/* If reset_to_default is set in ups.conf, the function won't change voltage limits */
		upslogx(LOG_INFO, "Voltage Limits for Bypass Mode won't be changed (reset_to_default option)");
	} else {
		val = getval("max_bypass_volt");

		if (val && strspn(val, "0123456789") != strlen(val)) {
			upslogx(LOG_ERR, "Maximum voltage for Bypass Mode (max_bypass_volt): non numerical value [%s]", val);
			val = "-1";
			rtn++;
		}

		bypass.volt.max.match = val ? atoi(val) : -1;

		val = getval("min_bypass_volt");

		if (val && strspn(val, "0123456789") != strlen(val)) {
			upslogx(LOG_ERR, "Minimum voltage for Bypass Mode (min_bypass_volt): non numerical value [%s]", val);
			val = "-1";
			rtn++;
		}

		bypass.volt.min.match = val ? atoi(val) : -1;

		/* Try and change values according to vars set in ups.conf */
		if ((bypass.volt.max.match != -1) && (bypass.volt.max.act != bypass.volt.max.match)) {
			if ((bypass.volt.max.limits.lower <= bypass.volt.max.match) && (bypass.volt.max.match <= bypass.volt.max.limits.upper)) {
				snprintf(buf, sizeof(buf), "PHV%03d\r", bypass.volt.max.match);
				voltronic_command(buf, buf2, sizeof(buf2));
				if (!strncasecmp(buf2, "(ACK", 4)) {
					upslogx(LOG_INFO, "Maximum voltage for Bypass Mode changed from %03d V to %03d V", bypass.volt.max.act, bypass.volt.max.match);
				} else {
					upslogx(LOG_ERR, "Failed to change maximum voltage for Bypass Mode from %03d V to %03d V", bypass.volt.max.act, bypass.volt.max.match);
					rtn++;
				}
			} else {
				upslogx(LOG_ERR, "Value for maximum voltage for Bypass Mode (max_bypass_volt) [%03d] V out of range of accepted values [%03d-%03d] V", bypass.volt.max.match, bypass.volt.max.limits.lower, bypass.volt.max.limits.upper);
				rtn++;
			}
		}

		if ((bypass.volt.min.match != -1) && (bypass.volt.min.act != bypass.volt.min.match)) {
			if ((bypass.volt.min.limits.lower <= bypass.volt.min.match) && (bypass.volt.min.match <= bypass.volt.min.limits.upper)) {
				snprintf(buf, sizeof(buf), "PLV%03d\r", bypass.volt.min.match);
				voltronic_command(buf, buf2, sizeof(buf2));
				if (!strncasecmp(buf2, "(ACK", 4)) {
					upslogx(LOG_INFO, "Minimum voltage for Bypass Mode changed from %03d V to %03d V", bypass.volt.min.act, bypass.volt.min.match);
				} else {
					upslogx(LOG_ERR, "Failed to change minimum voltage for Bypass Mode from %03d V to %03d V", bypass.volt.min.act, bypass.volt.min.match);
					rtn++;
				}
			} else {
				upslogx(LOG_ERR, "Value for minimum voltage for Bypass Mode (min_bypass_volt) [%03d] V out of range of accepted values [%03d-%03d] V", bypass.volt.min.match, bypass.volt.min.limits.lower, bypass.volt.min.limits.upper);
				rtn++;
			}
		}
	}
	
	/* Bypass Mode - Frequency Limits
	 * > [QBYF\r]
	 * < [(53.0 47.0\r]
	 *    01234567890
	 *    0         1
	 */

	if ((voltronic_command("QBYF\r", buf, sizeof(buf)) < 11) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for maximum and minimum frequency for Bypass Mode rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (strspn(buf+1, "0123456789. ") != (strlen(buf)-2)) {
		upsdebugx(2, "%s: non numerical value [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	bypass.freq.max.act = strtod(strtok_r(buf+1, " ", &last) ? buf+1 : "-1", NULL);
	val = strtok_r(NULL, " \r\n", &last);
	bypass.freq.min.act = strtod(val ? val : "-1", NULL);

	if ((bypass.freq.max.act < 0) || (bypass.freq.min.act < 0)) {
		upsdebugx(2, "%s: parsing failed", __func__);
		return -1;
	}

	ofn = strtod(dstate_getinfo("output.frequency.nominal") ? dstate_getinfo("output.frequency.nominal") : "-1", NULL);

	if (ofn == -1) {	/* Since query for ratings (QRI) is not mandatory to run this driver, skip next steps if we can't get the value of output frequency nominal */
		upsdebugx(2, "%s: unable to get output frequency nominal", __func__);
		return -1;
	}

	if (!strcmp(prot, "P01") || !strcmp(prot, "P09")) {
	/* For P01/P09 */
		if (ofn == 60.0) {
			bypass.freq.min.limits.lower = 50.0;
			bypass.freq.min.limits.upper = 59.0;
			bypass.freq.max.limits.lower = 61.0;
			bypass.freq.max.limits.upper = 70.0;
		} else if (ofn == 50.0) {
			bypass.freq.min.limits.lower = 40.0;
			bypass.freq.min.limits.upper = 49.0;
			bypass.freq.max.limits.lower = 51.0;
			bypass.freq.max.limits.upper = 60.0;
		} else {
			return -1;
		}
	} else if (!strcmp(prot, "P02") || !strcmp(prot, "P03") || !strcmp(prot, "P10") || !strcmp(prot, "P14") || !strcmp(prot, "P99")) {
	/* For P02/P03/P10/P14/P99 */
		if (ofn == 60.0) {
			bypass.freq.min.limits.lower = 56.0;
			bypass.freq.min.limits.upper = 59.0;
			bypass.freq.max.limits.lower = 61.0;
			bypass.freq.max.limits.upper = 64.0;
		} else if (ofn == 50.0) {
			bypass.freq.min.limits.lower = 46.0;
			bypass.freq.min.limits.upper = 49.0;
			bypass.freq.max.limits.lower = 51.0;
			bypass.freq.max.limits.upper = 54.0;
		} else {
			return -1;
		}
	} else {
	/* For P00/08/P31 */
		upslogx(LOG_INFO, "Your UPS doesn't support Bypass Mode");
		return -2;
	}

	/* Let the people know actual values and limits for max/min frequency for Bypass Mode */
	upslogx(LOG_INFO, "Bypass Mode - Frequency limits - Accepted range of values (Hz): minimum [%04.1f-%04.1f] / maximum [%04.1f-%04.1f]", bypass.freq.min.limits.lower, bypass.freq.min.limits.upper, bypass.freq.max.limits.lower, bypass.freq.max.limits.upper);
	upslogx(LOG_INFO, "Bypass Mode - Frequency limits - Actual values (Hz): minimum [%04.1f] / maximum [%04.1f]", bypass.freq.min.act, bypass.freq.max.act);

	if (testvar("reset_to_default")) {	/* If reset_to_default is set in ups.conf, the function won't change frequency limits */
		upslogx(LOG_INFO, "Frequency Limits for ECO Mode won't be changed (reset_to_default option)");
	} else {
		val = getval("max_bypass_freq");

		if (val && strspn(val, "0123456789.") != strlen(val)) {
			upslogx(LOG_ERR, "Maximum frequency for Bypass Mode (max_bypass_freq): non numerical value [%s]", val);
			val = "-1";
			rtn++;
		}

		bypass.freq.max.match = val ? strtod(val, NULL) : -1;

		val = getval("min_bypass_freq");

		if (val && strspn(val, "0123456789.") != strlen(val)) {
			upslogx(LOG_ERR, "Minimum frequency for Bypass Mode (min_bypass_freq): non numerical value [%s]", val);
			val = "-1";
			rtn++;
		}

		bypass.freq.min.match = val ? strtod(val, NULL) : -1;

		/* Try and change values according to vars set in ups.conf */
		if ((bypass.freq.max.match != -1) && (bypass.freq.max.act != bypass.freq.max.match)) {
			if ((bypass.freq.max.limits.lower <= bypass.freq.max.match) && (bypass.freq.max.match <= bypass.freq.max.limits.upper)) {
				snprintf(buf, sizeof(buf), "PGF%04.1f\r", bypass.freq.max.match);
				voltronic_command(buf, buf2, sizeof(buf2));
				if (!strncasecmp(buf2, "(ACK", 4)) {
					upslogx(LOG_INFO, "Maximum frequency for Bypass Mode changed from %04.1f Hz to %04.1f Hz", bypass.freq.max.act, bypass.freq.max.match);
				} else {
					upslogx(LOG_ERR, "Failed to change maximum frequency for Bypass Mode from %04.1f Hz to %04.1f Hz", bypass.freq.max.act, bypass.freq.max.match);
					rtn++;
				}
			} else {
				upslogx(LOG_ERR, "Value for maximum frequency for Bypass Mode (max_bypass_freq) [%04.1f] Hz out of range of accepted values [%04.1f-%04.1f] Hz", bypass.freq.max.match, bypass.freq.max.limits.lower, bypass.freq.max.limits.upper);
				rtn++;
			}
		}

		if ((bypass.freq.min.match != -1) && (bypass.freq.min.act != bypass.freq.min.match)) {
			if ((bypass.freq.min.limits.lower <= bypass.freq.min.match) && (bypass.freq.min.match <= bypass.freq.min.limits.upper)) {
				snprintf(buf, sizeof(buf), "PSF%04.1f\r", bypass.freq.min.match);
				voltronic_command(buf, buf2, sizeof(buf2));
				if (!strncasecmp(buf2, "(ACK", 4)) {
					upslogx(LOG_INFO, "Minimum frequency for Bypass Mode changed from %04.1f Hz to %04.1f", bypass.freq.min.act, bypass.freq.min.match);
				} else {
					upslogx(LOG_ERR, "Failed to change minimum frequency for Bypass Mode from %04.1f Hz to %04.1f", bypass.freq.min.act, bypass.freq.min.match);
					rtn++;
				}
			} else {
				upslogx(LOG_ERR, "Value for minimum frequency for Bypass Mode (min_bypass_freq) [%04.1f] Hz out of range of accepted values [%04.1f-%04.1f] Hz", bypass.freq.min.match, bypass.freq.min.limits.lower, bypass.freq.min.limits.upper);
				rtn++;
			}
		}
	}

	return rtn;
}

/* voltronic_batt_numb() - Change value of number of batteries according to value set by user in ups.conf */
/* Note: changing number of batteries will change the UPS's estimation on battery charge/runtime */
static int voltronic_batt_numb(void)	/* This function returns 0 on success, 1 if provided value in ups.conf isn't acceptable, -1 on other errors */
{
	char	buf[SMALLBUF] = "", buf2[SMALLBUF];

	struct {
		int	act;	/* Actual number of batteries as reported by UPS */
		int	match;	/* Number of batteries set in ups.conf -> int */
		const char	*conf;	/* Number of batteries set in ups.conf */
	} batt_numb;

	/* Set number of batteries to n (integer, 1-9)
	 * > [BATNn\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	batt_numb.conf = getval("battnumb");

	if (!batt_numb.conf || !battery_number) {	/* Skipping if there's no option set in ups.conf or if we can't get number of batteries from UPS */
		return 0;
	}

	if (strspn(batt_numb.conf, "0123456789") != strlen(batt_numb.conf)) {
		upslogx(LOG_ERR, "Number of batteries (battnumb): non numerical value [%s]", batt_numb.conf);
		return 1;
	}

	batt_numb.act = battery_number;
	batt_numb.match = atoi(batt_numb.conf);

	/* Try and change values according to vars set in ups.conf */
	if (batt_numb.act != batt_numb.match) {
		if ((1 <= batt_numb.match) && (batt_numb.match <= 9)) {
			snprintf(buf, sizeof(buf), "BATN%1d\r", batt_numb.match);
			voltronic_command(buf, buf2, sizeof(buf2));
			if (!strncasecmp(buf2, "(ACK", 4)) {
				upslogx(LOG_INFO, "Number of batteries changed from %1d to: %1d", batt_numb.act, batt_numb.match);
			} else {
				upslogx(LOG_ERR, "Failed to change number of batteries from %1d to: %1d", batt_numb.act, batt_numb.match);
				return -1;
			}
		} else {
			upslogx(LOG_ERR, "Value for number of batteries (battnumb) [%1d] out of range of accepted values [1..9]", batt_numb.match);
			return 1;
		}
	} else {
		upslogx(LOG_INFO, "Number of batteries is already set to: %1d", batt_numb.act);
	}

	return 0;
}

/* voltronic_batt_packs() - Change value of number of battery packs to value set by user in ups.conf */
/* Note: changing number of battery packs will change the UPS's estimation on battery charge/runtime */
static int voltronic_batt_packs(void)	/* This function returns 0 on success, 1 if provided value in ups.conf isn't acceptable, -1 on other errors */
{
	char	buf[SMALLBUF] = "", buf2[SMALLBUF];

	struct {
		int	act;	/* Actual number of battery packs as reported by UPS */
		int	match;	/* Numbers of battery packs set in ups.conf -> int */
		const char	*conf;	/* Numbers of battery packs set in ups.conf*/
	} batt_packs;

	/* Set number of battery packs in parallel to n (integer, 01-99)
	 * > [BATGNn\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	batt_packs.conf = getval("battpacks");

	if (!batt_packs.conf || !dstate_getinfo("battery.packs")) {	/* Skipping if there's no option set in ups.conf or if we can't get number of battery packs from UPS */
		return 0;
	}

	if (strspn(batt_packs.conf, "0123456789") != strlen(batt_packs.conf)) {
		upslogx(LOG_ERR, "Number of battery packs (battpacks): non numerical value [%s]", batt_packs.conf);
		return 1;
	}

	batt_packs.act = atoi(dstate_getinfo("battery.packs"));
	batt_packs.match = atoi(batt_packs.conf);

	/* Try and change values according to vars set in ups.conf */
	if (batt_packs.act != batt_packs.match) {
		if ((1 <= batt_packs.match) && (batt_packs.match <= 99)) {
			snprintf(buf, sizeof(buf), "BATGN%02d\r", batt_packs.match);
			voltronic_command(buf, buf2, sizeof(buf2));
			if (!strncasecmp(buf2, "(ACK", 4)) {
				upslogx(LOG_INFO, "Number of battery packs changed from %02d to: %02d", batt_packs.act, batt_packs.match);
			} else {
				upslogx(LOG_ERR, "Failed to change number of battery packs from %02d to: %02d", batt_packs.act, batt_packs.match);
				return -1;
			}
		} else {
			upslogx(LOG_ERR, "Value for number of battery packs (battpacks) [%d] out of range of accepted values [1..99]", batt_packs.match);
			return 1;
		}
	} else {
		upslogx(LOG_INFO, "Number of battery packs is already set to: %02d", batt_packs.act);
	}

	return 0;
}

/* voltronic_p31b() - P31 UPSes only - Query UPS for type of battery and try to change it to value set in ups.conf */
static int voltronic_p31b(void)	/* This function returns 0 on success, 1 if provided value in ups.conf isn't acceptable, -1 on other errors */
{
	char	buf[SMALLBUF] = "", buf2[SMALLBUF];

	struct {
		int	act;	/* Actual type of battery as reported by UPS */
		const char	*match;	/* Type of battery set in ups.conf */
		const char	**op;	/* Human readable type of battery */
	} batt_type = { .op = (const char*[]){ "Li", "Flooded", "AGM" } };

	int	i;

	/* Query UPS for battery type (Only P31)
	 * > [QBT\r]
	 * < [(01\r]	<- 00="Li", 01="Flooded" or 02="AGM"
	 *    0123
	 *    0
	 */

	if (voltronic_command("QBT\r", buf, sizeof(buf)) < 4) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for battery type rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if ((buf[1] != '0') || (strspn(buf+2, "012") != 1)) {
		upsdebugx(2, "%s: invalid battery type reported by UPS [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	batt_type.act = atoi(buf+1);

	upslogx(LOG_INFO, "Actual battery type as reported by UPS: %s", batt_type.op[batt_type.act]);
	dstate_setinfo("battery.type", "%s", batt_type.op[batt_type.act]);

	batt_type.match = getval("batt_type");

	if (!batt_type.match) {	/* Nothing to do.. */
		return 0;
	}

	for (i = 0; batt_type.op[i]; i++) {
		if (!strcasecmp(batt_type.match, batt_type.op[i]) && (i != batt_type.act)) {
			snprintf(buf, sizeof(buf), "PBT%02d\r", i);
			voltronic_command(buf, buf2, sizeof(buf2));
			if (!strncasecmp(buf2, "(ACK", 4)) {
				upslogx(LOG_INFO, "Battery type changed from %s to: %s", batt_type.op[batt_type.act], batt_type.op[i]);
				dstate_setinfo("battery.type", "%s", batt_type.op[i]);
				return 0;
			} else {
				upslogx(LOG_ERR, "Failed to change battery type from %s to: %s", batt_type.op[batt_type.act], batt_type.op[i]);
				return -1;
			}
		} else if (!strcasecmp(batt_type.match, batt_type.op[i]) && (i == batt_type.act)) {
			upslogx(LOG_INFO, "Battery type is already set to %s", batt_type.op[i]);
			return 0;
		}
	}

	upslogx(LOG_ERR, "Battery type (batt_type) [%s] out of accepted range [Li, Flooded, AGM]", batt_type.match);

	return 1;
}

/* voltronic_p31g() - P31 UPSes only - Query UPS for device grid working range type and try to change it to value set in ups.conf */
static int voltronic_p31g(void)	/* This function returns 0 on success, 1 if provided value in ups.conf isn't acceptable, -1 on other errors */
{
	char	buf[SMALLBUF] = "", buf2[SMALLBUF];
	int	i;

	struct {
		int	act;	/* Actual device grid working range as reported by UPS */
		const char	*match;	/* Device grid working range set in ups.conf */
		const char	**type;	/* Human readable type of working range */
	} work_range = { .type = (const char*[]){ "Appliance", "UPS", NULL } };

	/* Query UPS for device grid working range (Only P31)
	 * > [QGR\r]
	 * < [(01\r]	<- 00=Appliance, 01=UPS
	 *    0123
	 *    0
	 */

	if (voltronic_command("QGR\r", buf, sizeof(buf)) < 4) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for device grid working range rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if ((buf[1] != '0') || (strspn(buf+2, "01") != 1)) {
		upsdebugx(2, "%s: invalid device grid working range reported by UPS [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	work_range.act = atoi(buf+1);

	upslogx(LOG_INFO, "Actual device grid working range as reported by UPS: %s", work_range.type[work_range.act]);

	work_range.match = getval("work_range_type");

	if (!work_range.match) {	/* Nothing to do.. */
		return 0;
	}

	for (i = 0; work_range.type[i]; i++) {
		if (!strcasecmp(work_range.match, work_range.type[i]) && (i != work_range.act)) {
			snprintf(buf, sizeof(buf), "PGR%02d\r", i);
			voltronic_command(buf, buf2, sizeof(buf2));
			if (!strncasecmp(buf2, "(ACK", 4)) {
				upslogx(LOG_INFO, "Device grid working range changed from %s to: %s", work_range.type[work_range.act], work_range.type[i]);
				return 0;
			} else {
				upslogx(LOG_ERR, "Failed to change device grid working range from %s to: %s", work_range.type[work_range.act], work_range.type[i]);
				return -1;
			}
		} else if (!strcasecmp(work_range.match, work_range.type[i]) && (i == work_range.act)) {
			upslogx(LOG_INFO, "Device grid working range is already set to %s", work_range.type[i]);
			return 0;
		}
	}

	upslogx(LOG_ERR, "Device grid working range (work_range_type) [%s] out of accepted range [UPS, Appliance]", work_range.match);

	return 1;
}

/* voltronic_ratings1() - Query UPS for ratings #1 */
static int voltronic_ratings1(void)
{
	const struct {
		const char	*var;
		const char	*fmt;
		double		(*conv)(const char *, char **);
	} rating[] = {
		{ "output.voltage.nominal", "%.1f", strtod },
		{ "output.current.nominal", "%.0f", strtod },
		{ "battery.voltage.nominal", "%.1f", strtod },
		{ "output.frequency.nominal", "%.1f", strtod },
		{ NULL }
	};

	char	buf[SMALLBUF], *val, *last = NULL;
	int	i;

	/*
	 * > [QRI\r]
	 * < [(230.0 004 024.0 50.0\r]
	 *    0123456789012345678901
	 *    0         1         2
	 */
	if ((voltronic_command("QRI\r", buf, sizeof(buf)) < 22) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for rated informations #1 rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	for (i = 0, val = strtok_r(buf+1, " ", &last); rating[i].var; i++, val = strtok_r(NULL, " \r\n", &last)) {

		if (!val) {
			upsdebugx(2, "%s: parsing failed", __func__);
			return -1;
		}

		if (strspn(val, "0123456789.") != strlen(val)) {
			upsdebugx(2, "%s: non numerical value [%s]", __func__, val);
			continue;
		}

		dstate_setinfo(rating[i].var, rating[i].fmt, rating[i].conv(val, NULL));
	}

	return 0;
}

/* voltronic_ratings2() - Query UPS for ratings #2 */
static int voltronic_ratings2(void)
{
	const struct {
		const char	*var;
		const int	len;
	} information[] = {
		{ "device.model", 15 },
		{ "ups.power.nominal", 7 },
		{ NULL }
	};

	const struct {
		const char	*var;
		const char	*fmt;
	} infos[] = {
		{ "output.powerfactor", "%.1f" },	/* UPS report a value expressed in % so -> output.powerfactor*100 e.g. opf = 0,8 -> ups = 80 */
		{ "input.phases", "%.0f" },
		{ "output.phases", "%.0f" },
		{ "input.voltage.nominal", "%.1f" },
		{ "output.voltage.nominal", "%.1f" },	/* redundant with value from QRI */
		/*{ "battery.number", "%.0f" },*/	/* redundant with value from QBV */
		/*{ "battery.voltage.nominal", "%.1f" },*/	/* as *per battery* vs *per pack* reported by QRI [voltronic_ratings1()] */
		{ NULL }
	};

	char	buf[SMALLBUF], val[SMALLBUF], *val2, *last = NULL;
	int	i, index;

	/*
	 * > [QMD\r]
	 * < [(#######OLHVT1K0 ###1000 80 1/1 230 230 02 12.0\r]	<- Some UPS may reply with spaces instead of '#'
	 *    012345678901234567890123456789012345678901234567
	 *    0         1         2         3         4
	 */

	if ((voltronic_command("QMD\r", buf, sizeof(buf)) < 48) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for rated informations #2 rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	for (i = 0, index = 1; information[i].var; index += information[i++].len+1) {
		snprintf(val, sizeof(val), "%.*s", information[i].len, &buf[index]);

		if (strchr(val, '#')) {
			if (strspn(val, "#") == strlen(val)) {	/* If the UPS report a ##..# value we'll log it but we don't store it in a nut variable */
				upslogx(LOG_INFO, "UPS reported a non-significant value for %s [%s]", information[i].var, val);
				continue;
			}
			dstate_setinfo(information[i].var, "%s", ltrim(val, '#'));
		} else {
			if (strspn(val, " ") == strlen(val)) {	/* If the UPS report a blank value we'll log it but we don't store it in a nut variable */
				upslogx(LOG_INFO, "UPS reported a non-significant value for %s [%s]", information[i].var, val);
				continue;
			}
			dstate_setinfo(information[i].var, "%s", ltrim(val, ' '));
		}
	}

	for (i = 0, val2 = strtok_r(buf+25, " ", &last); infos[i].var; i++, val2 = strtok_r(NULL, " /\r\n", &last)) {

		if (!val2) {
			upsdebugx(2, "%s: parsing failed", __func__);
			return -1;
		}

		if (strspn(val2, "0123456789.") != strlen(val2)) {
			upsdebugx(2, "%s: non numerical value [%s]", __func__, val2);
			continue;
		}

		if (dstate_getinfo(infos[i].var)) {	/* skipping if info has been already obtained */
			continue;
		}

		dstate_setinfo(infos[i].var, infos[i].fmt, i == 0 ? strtod(val2, NULL)/100 : strtod(val2, NULL));
	}

	return 0;
}

/* voltronic_manufacturer() - Query UPS for manufacturer */
static int voltronic_manufacturer(void)
{

	char	buf[SMALLBUF];

	/*
	 * > [QMF\r]
	 * < [(#######BOH\r]	<- I don't know if it has a fixed length (-> so min length = ( + \r = 2). '#' may be replaced by spaces
	 *    012345678901
	 *    0         1
	 */

	if ((voltronic_command("QMF\r", buf, sizeof(buf)) < 2)) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for manufacturer rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (strchr(buf+1, '#')) {
		if (strspn(buf+1, "#") == (strlen(buf) - 2)) {	/* If the UPS report a ##..# manufacturer we'll log it but we don't store it in device.mfr */
			upslogx(LOG_INFO, "UPS reported a non-significant manufacturer [%s]", strtok(buf+1, "\r\n"));
			return 0;
		}
		dstate_setinfo("device.mfr", "%s", ltrim(strtok(buf+1, "\r\n"), '#'));
	} else {
		if (strspn(buf+1, " ") == (strlen(buf) - 2)) {	/* If the UPS report a blank manufacturer we'll log it but we don't store it in device.mfr */
			upslogx(LOG_INFO, "UPS reported a non-significant manufacturer [%s]", strtok(buf+1, "\r\n"));
			return 0;
		}
		dstate_setinfo("device.mfr", "%s", ltrim(strtok(buf+1, "\r\n"), ' '));
	}

	return 0;
}

/* voltronic_firmware() - Query for UPS firmware version */
static int voltronic_firmware(void)
{

	char	buf[SMALLBUF];

	/*
	 * > [QVFW\r]
	 * < [(VERFW:00322.02\r]
	 *    0123456789012345
	 *    0         1
	 */

	if ((voltronic_command("QVFW\r", buf, sizeof(buf)) < 16) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for firmware rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (strncasecmp(buf, "(VERFW:", 7)) {
		upsdebugx(2, "%s: invalid reply from UPS [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	if (strspn(buf+1, "0.") == (strlen(buf) - 8)) {	/* If the UPS report a 00..0 firmware we'll log it but we don't store it in ups.firmware */
		upslogx(LOG_INFO, "UPS reported a non-significant firmware [%s]", strtok(buf+7, "\r\n"));
		return 0;
	}

	dstate_setinfo("ups.firmware", "%s", strtok(buf+7, "\r\n"));

	return 0;
}

/* voltronic_serial() - Query for UPS serial number */
static int voltronic_serial(void)
{

	char	buf[SMALLBUF];

	/*
	 * > [QID\r]
	 * < [(12345679012345\r]	<- As far as I know it hasn't a fixed length -> min length = ( + \r = 2
	 *    0123456789012345
	 *    0         1
	 */

	if ((voltronic_command("QID\r", buf, sizeof(buf)) < 2)) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for serial rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (strspn(buf+1, "0") == (strlen(buf) - 2)) {	/* If the UPS report a 00..0 serial we'll log it but we don't store it in device.serial */
		upslogx(LOG_INFO, "UPS reported a non-significant serial [%s]", strtok(buf+1, "\r\n"));
		return 0;
	}

	dstate_setinfo("device.serial", "%s", strtok(buf+1, "\r\n"));

	return 0;
}

/* voltronic_vendor() - Query UPS for vendor infos */
static int voltronic_vendor(void)	/* Old way - Retained as a double-check control for infos not get with newer queries */
{
	const struct {
		const char	*var;
		const int	len;
	} information[] = {
		{ "device.mfr", 15 },
		{ "device.model", 10 },
		{ "ups.firmware", 10 },
		{ NULL }
	};

	char	buf[SMALLBUF];
	int	i, index;

	/*
	 * > [I\r]
	 * < [#-------------   ------     VT12046Q  \r]
	 *    012345678901234567890123456789012345678
	 *    0         1         2         3
	 */
	if ((voltronic_command("I\r", buf, sizeof(buf)) < 39) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for vendor informations rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '#') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	for (i = 0, index = 1; information[i].var; index += information[i++].len+1) {
		char	val[SMALLBUF];

		snprintf(val, sizeof(val), "%.*s", information[i].len, &buf[index]);

		if (dstate_getinfo(information[i].var) || ((int)strspn(val, " ") == information[i].len)) {	/* skipping if info has been already obtained or if val is blank */
			continue;
		}

		dstate_setinfo(information[i].var, "%s", rtrim(val, ' '));
	}

	return 0;
}

/* voltronic_ratings3() - Query UPS for ratings #3 */
static int voltronic_ratings3(void)	/* Old way - Retained as a double-check control and for infos not get with newer queries */
{
	const struct {
		const char	*var;
		const char	*fmt;
		double		(*conv)(const char *, char **);
	} rating[] = {
		{ "input.voltage.nominal", "%.0f", strtod },
		{ "input.current.nominal", "%.1f", strtod },
		{ "battery.voltage.nominal", "%.1f", strtod },
		{ "input.frequency.nominal", "%.0f", strtod },
		{ NULL }
	};

	char	buf[SMALLBUF], *val, *last = NULL;
	int	i;

	/*
	 * > [F\r]
	 * < [#220.0 000 024.0 50.0\r]
	 *    0123456789012345678901
	 *    0         1         2
	 */
	if ((voltronic_command("F\r", buf, sizeof(buf)) < 22) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for rated informations #3 rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '#') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	for (i = 0, val = strtok_r(buf+1, " ", &last); rating[i].var; i++, val = strtok_r(NULL, " \r\n", &last)) {

		if (!val) {
			upsdebugx(2, "%s: parsing failed", __func__);
			return -1;
		}

		if (strspn(val, "0123456789.") != strlen(val)) {
			upsdebugx(2, "%s: non numerical value [%s]", __func__, val);
			continue;
		}

		if (dstate_getinfo(rating[i].var)) {	/* skipping if info has been already obtained */
			continue;
		}

		dstate_setinfo(rating[i].var, rating[i].fmt, rating[i].conv(val, NULL));
	}

	return 0;
}

/* voltronic_outlet(number of outlet, whether voltronic_initinfo() or not) - Query UPS for outlets status and delay time and try to change it to value set in ups.conf */
static int voltronic_outlet(const int outlet_numb, const int init)	/* This function returns -1 if UPS can't switch outlets, -2 if outlet #outlet_numb is not programmable, 2 on delaytime errors if provided value in ups.conf isn't acceptable, 1 on other errors, 0 on success */
{
	char	buf[SMALLBUF] = "", buf2[SMALLBUF] = "", buf3[SMALLBUF] = "", query[SMALLBUF] = "", *val;

	struct {
		int	act;	/* Actual delay for outlet "outlet_numb" as reported by UPS */
		int	match;	/* Delay for outlet "outlet_numb" set in ups.conf */
	} delay;

	/* Query UPS for programmable outlet n (1-4) status
	 * > [QSK1\r]
	 * < [(1\r]	<-  if outlet is on -> (1, if off -> (0 - (NAK -> outlet is not programmable
	 *    012
	 *    0
	 */

	if (!dstate_getinfo("outlet.0.switchable")) {	/* If outlet.0.switchable is unset -> UPS hasn't reported 'j' in the capability flag.. */
		upslogx(LOG_INFO, "UPS has no programmable outlet.");
		return -1;
	} else if  (!strcmp(dstate_getinfo("outlet.0.switchable"), "no")) {	/* ..if set to "no" -> UPS has 'j' capability, but it's disabled */
		upslogx(LOG_INFO, "Outlet switchability is disabled. You can enable it through outlet_control option in ups.conf");
		return -1;
	}

	snprintf(query, sizeof(query), "QSK%.1d\r", outlet_numb);

	if ((voltronic_command(query, buf, sizeof(buf)) < 3) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return 1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {	/* If UPS reply (NAK the outlet is not programmable */
		if (init == 1) {	/* Log only if in voltronic_initinfo() */
			upslogx(LOG_INFO, "UPS outlet #%d is not programmable.", outlet_numb);
		}
		snprintf(buf2, sizeof(buf2), "outlet.%.1d.switchable", outlet_numb);
		dstate_setinfo(buf2, "no");
		return -2;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return 1;
	}

	snprintf(buf2, sizeof(buf2), "outlet.%.1d.switchable", outlet_numb);
	snprintf(buf3, sizeof(buf3), "outlet.%.1d.status", outlet_numb);

	switch(buf[1]) {
		case '1': dstate_setinfo(buf2, "yes");	dstate_setinfo(buf3, "on");	break;
		case '0': dstate_setinfo(buf2, "yes");	dstate_setinfo(buf3, "off");	break;
		default: upsdebugx(2, "%s: invalid reply from UPS [%s]", __func__, strtok(buf+1, "\r\n"));	return 1;
	}

	if (init == 0) {	/* Quit function if not in voltronic_initinfo() */
		return 0;
	}

	snprintf(buf, sizeof(buf), "outlet.%.1d.load.on", outlet_numb);
	dstate_addcmd(buf);
	snprintf(buf, sizeof(buf), "outlet.%.1d.load.off", outlet_numb);
	dstate_addcmd(buf);

	/* Query UPS for programmable outlet n (1-4) delay time before it shuts down the load when on battery mode
	 * > [QSKT1\r]
	 * < [(008\r]	<-  if delay time si set (by PSK[1-4]n) it'll report n (minutes) otherwise it'll report (NAK (also if outlet is not programmable)
	 *    01234
	 *    0
	 */

	snprintf(query, sizeof(query), "QSKT%.1d\r", outlet_numb);

	if ((voltronic_command(query, buf, sizeof(buf)) < 5) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return 1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return 1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upslogx(LOG_INFO, "Delay time before programmable outlet %.1d shuts down the load when on battery mode not set in UPS.", outlet_numb);
		delay.act = -1;
	} else {
		if (strspn(buf+1, "0123456789") != (strlen(buf)-2)) {
			upsdebugx(2, "%s: non numerical value [%s]", __func__, strtok(buf+1, "\r\n"));
			return 1;
		}

		delay.act = atoi(buf+1);

		snprintf(buf, sizeof(buf), "outlet.%.1d.delay.shutdown", outlet_numb);

		dstate_setinfo(buf, "%03d", delay.act * 60);
	}

	snprintf(buf, sizeof(buf), "outlet%.1d_delay", outlet_numb);
	val = getval(buf);

	if (!val) {	/* Skipping if there's no option set in ups.conf */
		return 0;
	}

	if (strspn(val, "0123456789") != strlen(val)) {
		upslogx(LOG_ERR, "Delay time for programmable outlet %.1d: non numerical value [%s]", outlet_numb, val);
		return 2;
	}

	delay.match = atoi(val);

	if (delay.match < 0 || delay.match > 999) {
		upslogx(LOG_ERR, "Delay time [%03d] for programmable outlet %.1d out of acceptable range [0..999] (minutes)", delay.match, outlet_numb);
		return 2;
	}

	if (delay.match != delay.act) {
		snprintf(buf, sizeof(buf), "PSK%.1d%03d\r", outlet_numb, delay.match);
		voltronic_command(buf, buf2, sizeof(buf2));
		if (!strncasecmp(buf2, "(ACK", 4)) {
			upslogx(LOG_INFO, "Delay time for programmable outlet %.1d successfully changed from %03d to: %03d minutes", outlet_numb, delay.act, delay.match);
			snprintf(buf, sizeof(buf), "outlet.%.1d.delay.shutdown", outlet_numb);
			dstate_setinfo(buf, "%03d", delay.match * 60);
		} else {
			upslogx(LOG_ERR, "Failed to change delay time for programmable outlet %.1d from %03d to: %03d minutes", outlet_numb, delay.act, delay.match);
			return 1;
		}
	} else {
		upslogx(LOG_INFO, "Delay time for programmable outlet %.1d is already %03d minutes", outlet_numb, delay.act);
	}

	return 0;
}

/* voltronic_batt_low() - Query UPS for actual battery low voltage and try to change it to value set in ups.conf */
/* Note: changing battery low voltage will change the UPS's estimation on battery charge/runtime */
static int voltronic_batt_low(void)	/* This function returns 0 on success, 1 if provided value in ups.conf isn't acceptable, -1 on other errors */
{
	char	buf[SMALLBUF] = "", buf2[SMALLBUF] = "", *val;
	struct {
		int	act;	/* Actual value for battery low voltage as reported by UPS */
		int	match;	/* Value for battery low voltage set in ups.conf */
		int	min;	/* Lower limit for battery low voltage */
		int	max;	/* Upper limit for battery low voltage */
	} battlow;

	/* Query UPS for voltage for Battery Low Voltage
	 * > [RE0\r]
	 * < [#20\r]
	 *    012
	 *    0
	 */

	if (voltronic_command("RE0\r", buf, sizeof(buf)) < 3) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for battery low voltage rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '#') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	battlow.act = atoi(buf+1);
	dstate_setinfo("battery.voltage.low", "%.2d", battlow.act);

	/* Since query for ratings (QRI) is not mandatory to run this driver, skip next steps if we can't get the value of output voltage nominal */
	if (!dstate_getinfo("output.voltage.nominal")) {
		upsdebugx(2, "%s: unable to get the value of output voltage nominal", __func__);
		return -1;
	}
	/* Since query for ratings (QRI) is not mandatory to run this driver, skip next steps if we can't get the value of output current nominal */
	if (!dstate_getinfo("output.current.nominal")) {
		upsdebugx(2, "%s: unable to get the value of output current nominal", __func__);
		return -1;
	}

	if ((atoi(dstate_getinfo("output.voltage.nominal")) * atoi(dstate_getinfo("output.current.nominal"))) < 1000) {
		battlow.min = 20;
		battlow.max = 24;
	} else {
		battlow.min = 20;
		battlow.max = 28;
	}

	/* Let the people know actual values and limits for battery low voltage */
	upslogx(LOG_INFO, "Battery Low Voltage - Accepted values (V): %.2d-%.2d", battlow.min, battlow.max);
	upslogx(LOG_INFO, "Battery Low Voltage - Actual value (V): %.2d", battlow.act);
	dstate_setinfo("battery.voltage.low", "%.2d", battlow.act);

	/* Set voltage for Battery Low to n (integer, 20..24/20..28)
	 * > [W0En\r]
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	val = getval("battlow");

	if (!val) {	/* Skipping if there's no option set in ups.conf */
		return 0;
	}

	if (strspn(val, "0123456789") != strlen(val)) {
		upslogx(LOG_ERR, "Battery Low Voltage: non numerical value [%s]", val);
		return 1;
	}

	battlow.match = atoi(val);

	/* Try and change values according to vars set in ups.conf */
	if (battlow.act != battlow.match) {
		if ((battlow.min <= battlow.match) && (battlow.match <= battlow.max)) {
			snprintf(buf, sizeof(buf), "W0E%.2d\r", battlow.match);
			voltronic_command(buf, buf2, sizeof(buf2));
			if (!strncasecmp(buf2, "(ACK", 4)) {
				upslogx(LOG_INFO, "Voltage for low battery changed from %.2d V to: %.2d V", battlow.act, battlow.match);
				dstate_setinfo("battery.voltage.low", "%.2d", battlow.match);
			} else {
				upslogx(LOG_ERR, "Failed to change voltage for low battery from %.2d V to: %.2d V", battlow.act, battlow.match);
				return -1;
			}
		} else {
			upslogx(LOG_ERR, "Value for voltage for low battery (battlow) [%.2d] V out of range of accepted values [%.2d-%.2d] V", battlow.match, battlow.min, battlow.max);
			return 1;
		}
	} else {
		upslogx(LOG_INFO, "Battery Low Voltage is already %.2d V", battlow.act);
	}

	return 0;
}

/* voltronic_multi() - Query UPS for multi-phase voltages/frequencies */
static int voltronic_multi(void)	/* This function return -1 on errors, 0 on success, 1 if not in three-phase, rtn<0 on other errors */
{
	/* > [Q3**\r]
	 * < [(123.4 123.4 123.4 123.4 123.4 123.4\r]	<- Q3PV
	 * < [(123.4 123.4 123.4 123.4 123.4 123.4\r]	<- Q3OV
	 * < [(123 123 123\r]	<- Q3PC
	 * < [(123 123 123\r]	<- Q3OC
	 * < [(123 123 123\r]	<- Q3LD
	 * < [(123.4 123.4 123.4\r]	<- Q3YV - P09 protocol
	 * < [(123.4 123.4 123.4 123.4 123.4 123.4\r]	<- Q3YV - P10/P03 protocols
	 *    0123456789012345678901234567890123456
	 *    0         1         2         3
	 *
	 * P09 = 2-phase input/2-phase output
	 * Q3PV	(Input Voltage L1 | Input Voltage L2 | Input Voltage L3 | Input Voltage L1-L2 | Input Voltage L1-L3 | Input Voltage L2-L3
	 * Q3OV	(Output Voltage L1 | Output Voltage L2 | Output Voltage L3 | Output Voltage L1-L2 | Output Voltage L1-L3 | Output Voltage L2-L3
	 * Q3PC	(Input Current L1 | Input Current L2 | Input Current L3
	 * Q3OC	(Output Current L1 | Output Current L2 | Output Current L3
	 * Q3LD	(Output Load Level L1 | Output Load Level L2 | Output Load Level L3
	 * Q3YV	(Output Bypass Voltage L1 | Output Bypass Voltage L2 | Output Bypass Voltage L3
	 *
	 * P10 = 3-phase input/3-phase output / P03 = 3-phase input/ 1-phase output
	 * Q3PV	(Input Voltage L1 | Input Voltage L2 | Input Voltage L3 | Input Voltage L1-L2 | Input Voltage L2-L3 | Input Voltage L1-L3
	 * Q3OV	(Output Voltage L1 | Output Voltage L2 | Output Voltage L3 | Output Voltage L1-L2 | Output Voltage L2-L3 | Output Voltage L1-L3
	 * Q3PC	(Input Current L1 | Input Current L2 | Input Current L3
	 * Q3OC	(Output Current L1 | Output Current L2 | Output Current L3
	 * Q3LD	(Output Load Level L1 | Output Load Level L2 | Output Load Level L3
	 * Q3YV	(Output Bypass Voltage L1 | Output Bypass Voltage L2 | Output Bypass Voltage L3 | Output Bypass Voltage L1-L2 | Output Bypass Voltage L2-L3 | Output Bypass Voltage L1-L3
	 *
	 */

	/*	From Q3PV	*/
	char	*involt[] = { "input.L1-N.voltage", "input.L2-N.voltage", "input.L3-N.voltage", "input.L1-L2.voltage", "input.L2-L3.voltage", "input.L1-L3.voltage", NULL };
	/*	From Q3OV	*/
	char	*outvolt[] = { "output.L1-N.voltage", "output.L2-N.voltage", "output.L3-N.voltage", "output.L1-L2.voltage", "output.L2-L3.voltage", "output.L1-L3.voltage", NULL };
	/*	From Q3PC	*/
	char	*incurrent[] = { "input.L1.current", "input.L2.current", "input.L3.current", NULL };
	/*	From Q3OC	*/
	char	*outcurrent[] = { "output.L1.current", "output.L2.current", "output.L3.current", NULL };
	/*	From Q3LD	*/
	char	*outload[] = { "output.L1.power.percent", "output.L2.power.percent", "output.L3.power.percent", NULL };
	/*	From Q3YV	*/
	char	*outbypass[] = { "output.bypass.L1-N.voltage", "output.bypass.L2-N.voltage", "output.bypass.L3-N.voltage", "output.bypass.L1-L2.voltage", "output.bypass.L2-L3.voltage", "output.bypass.L1-L3.voltage", NULL };

	struct {
		const char	*cmd;
		char	**type;
		int	len;
		const char	*fmt;
		const char	*desc;
		double		(*conv)(const char *, char **);
	} multiquery[] = {
		{ "Q3PV\r", involt, 37, "%.1f", "Query for Input Voltage for multi-phase UPSes", strtod },
		{ "Q3PC\r", incurrent, 13, "%.1f", "Query for Input Current for multi-phase UPSes", strtod },
		{ "Q3OV\r", outvolt, 37, "%.1f", "Query for Output Voltage for multi-phase UPSes", strtod },
		{ "Q3OC\r", outcurrent, 13, "%.1f", "Query for Output Current for multi-phase UPSes", strtod },
		{ "Q3LD\r", outload, 13, "%.0f", "Query for Output Load Level for multi-phase UPSes", strtod },
		{ "Q3YV\r", outbypass, 37, "%.1f", "Query for Output Load Level for multi-phase UPSes", strtod },
		{ NULL }
	};

	char	buf[SMALLBUF], *val, *last = NULL;
	int	i, j, rtn = 0;

	if (!strcmp(dstate_getinfo("ups.firmware.aux"), "P09")) {
		involt[4] = "input.L1-L3.voltage";	/* Quite useless since P09 UPSes should be 2-phase input/output */
		involt[5] = "input.L2-L3.voltage";	/* Quite useless since P09 UPSes should be 2-phase input/output */
		outvolt[4] = "output.L1-L3.voltage";	/* Quite useless since P09 UPSes should be 2-phase input/output */
		outvolt[5] = "output.L2-L3.voltage";	/* Quite useless since P09 UPSes should be 2-phase input/output */
		outbypass[3] = NULL;
		outbypass[4] = NULL;
		outbypass[5] = NULL;
		multiquery[5].len = 19;
	} else if (!strcmp(dstate_getinfo("ups.firmware.aux"), "P03")) {	/* P03 = 3-phase input/1-phase output UPSes */
		multiquery[2].cmd = NULL;
		multiquery[3].cmd = NULL;
		multiquery[4].cmd = NULL;
		multiquery[5].cmd = NULL;
	} else if (strcmp(dstate_getinfo("ups.firmware.aux"), "P10")) {
		return 1;	/* As far as I know only P03/P09/P10 UPSes have multi-phase capability */
	}

	if (!dstate_getinfo("input.phases") || !dstate_getinfo("output.phases")) {
		upsdebugx(2, "%s: unable to get number of input/output phases", __func__);
		return -1;
	}

	if ((atoi(dstate_getinfo("input.phases")) < 2) && (atoi(dstate_getinfo("output.phases")) < 2)) {	/* Quit function if not multi-phase */
		upslogx(LOG_INFO, "The UPS is not running on multi-phase input nor it is powering load with multi-phase output");
		return 1;
	}

	for (i = 0; multiquery[i].cmd; i++) {

		if ((voltronic_command(multiquery[i].cmd, buf, sizeof(buf)) < multiquery[i].len) && (strncasecmp(buf, "(NAK", 4))) {
			upsdebugx(2, "%s: %s - short reply", __func__, multiquery[i].desc);
			rtn--;
			continue;
		}

		if (!strncasecmp(buf, "(NAK", 4)) {	/* Not all queries are accepted by multi-phase UPSes, so if a query is rejected we'll skip it but we won't report an error (rtn--) */
			upsdebugx(2, "%s: %s rejected by UPS", __func__, multiquery[i].desc);
			continue;
		}

		if (buf[0] != '(') {
			upsdebugx(2, "%s: %s - invalid start character [%02x]", __func__, multiquery[i].desc, buf[0]);
			rtn--;
			continue;
		}

		for (j = 0, val = strtok_r(buf+1, " ", &last); multiquery[i].type[j]; j++, val = strtok_r(NULL, " \r\n", &last)) {

			if (!val) {
				upsdebugx(2, "%s: %s - parsing failed", __func__, multiquery[i].desc);
				rtn--;
				break;
			}

			if (!strcmp(dstate_getinfo("ups.firmware.aux"), "P09") && strchr(multiquery[i].type[j], '3')) {	/* P09 = two-phase input/output UPSes */
				continue;
			}

			if (strspn(val, "0123456789.") != strlen(val)) {
				upsdebugx(2, "%s: %s - non numerical value [%s]", __func__, multiquery[i].desc, val);
				continue;
			}

			dstate_setinfo(multiquery[i].type[j], multiquery[i].fmt, multiquery[i].conv(val, NULL));
		}
	}

	return rtn;
}

/* voltronic_load_level() - Query UPS for last seen minimum/maximum load level */
static int voltronic_load_level(void)
{

	char	buf[SMALLBUF], *val, *last = NULL;

	struct {
		double	min;	/* Last seen minimum load level */
		double	max;	/* Last seen maximum load level */
	} loadlevel;

	/* Query UPS for last seen min/max load level
	 * > [QLDL\r]
	 * < [(021 023\r]	<- minimum load level - maximum load level
	 *    012345678
	 *    0
	 */

	if ((voltronic_command("QLDL\r", buf, sizeof(buf)) < 9) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for last seen maximum and minimum load level rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (strspn(buf+1, "0123456789 ") != (strlen(buf) - 2)) {
		upsdebugx(2, "%s: non numerical value [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	loadlevel.min = strtod(strtok_r(buf+1, " ", &last) ? buf+1 : "-1", NULL);
	val = strtok_r(NULL, " \r\n", &last);
	loadlevel.max = strtod(val ? val : "-1", NULL);

	if ((loadlevel.min < 0) || (loadlevel.max < 0)) {
		upsdebugx(2, "%s: parsing failed", __func__);
		return -1;
	}

	dstate_setinfo("output.power.minimum.percent", "%.0f", loadlevel.min);
	dstate_setinfo("output.power.maximum.percent", "%.0f", loadlevel.max);

	return 0;
}

/* voltronic_phase() - Query UPS for actual input/output phase angles and try and change output phase angle to value set in ups.conf */
static int voltronic_phase(void)	/* This function returns 0 on success, 1 if provided value in ups.conf isn't acceptable, -1 on other errors */
{

	char	buf[SMALLBUF], buf2[SMALLBUF], *val, *last = NULL;

	struct {
		int	input;	/* Actual input phase angle as reported by UPS */
		struct {
			int	act;	/* Actual output phase angle as reported by UPS */
			int	match;	/* Value for output phase angle provided in ups.conf */
		} output;
	} phaseangle;

	/* Query UPS for Phase Angle
	 * > [QPD\r]
	 * < [(000 120\r]	<- Input Phase Angle - Output Phase Angle
	 *    012345678
	 *    0
	 */

	if ((voltronic_command("QPD\r", buf, sizeof(buf)) < 9) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for input and output phase angle rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (strspn(buf+1, "0123456789 ") != (strlen(buf)-2)) {
		upsdebugx(2, "%s: non numerical value [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	phaseangle.input = atoi(strtok_r(buf+1, " ", &last) ? buf+1 : "-1");
	val = strtok_r(NULL, " \r\n", &last);
	phaseangle.output.act = atoi(val ? val : "-1");

	if ((phaseangle.input < 0) || (phaseangle.output.act < 0)) {
		upsdebugx(2, "%s: parsing failed", __func__);
		return -1;
	}

	/* Let the people know actual values of input and output phase angle */
	upslogx(LOG_INFO, "Input Phase Angle - Actual value: %03d°", phaseangle.input);
	upslogx(LOG_INFO, "Output Phase Angle - Actual value: %03d°", phaseangle.output.act);

	/*
	 * > [PPDn\r]	Set output phase angle to n (000, 120, 180 or 240)°
	 * < [(ACK\r]
	 *    01234
	 *    0
	 */

	val = getval("output_phase_angle");

	if (!val) {	/* Skipping if there's no option set in ups.conf */
		return 0;
	}

	if (strspn(val, "0123456789") != strlen(val)) {
		upslogx(LOG_ERR, "Output Phase Angel: non numerical value [%s]", val);
		return 1;
	}

	phaseangle.output.match = atoi(val);

	/* Try and change values according to vars set in ups.conf */
	if (phaseangle.output.act != phaseangle.output.match) {
		if ((phaseangle.output.match == 0) || (phaseangle.output.match == 120) || (phaseangle.output.match == 180) || (phaseangle.output.match == 240)) {
			snprintf(buf, sizeof(buf), "PPD%03d\r", phaseangle.output.match);
			voltronic_command(buf, buf2, sizeof(buf2));
			if (!strncasecmp(buf2, "(ACK", 4)) {
				upslogx(LOG_INFO, "Output phase angle changed from %03d° to: %03d°", phaseangle.output.act, phaseangle.output.match);
			} else {
				upslogx(LOG_ERR, "Failed to change output phase angle from %03d° to: %03d°", phaseangle.output.act, phaseangle.output.match);
				return -1;
			}
		} else {
			upslogx(LOG_ERR, "Value for output phase angle (output_phase_angle) [%03d]° out of range of accepted values [000, 120, 180, 240]°", phaseangle.output.match);
			return 1;
		}	
	} else {
		upslogx(LOG_INFO, "Output phase angle is already %03d°", phaseangle.output.act);
	}

	return 0;
}

/* voltronic_parallel() - Query UPS for master/slave for UPS in parallel */
static int voltronic_parallel(void)
{

	char	buf[SMALLBUF];

	/* Query UPS for master/slave for UPS in parallel
	 * > [QPAR\r]
	 * < [(001\r]	<- 001 for master UPS, 002 and 003 for slave UPSes
	 *    01234
	 *    0
	 */

	if ((voltronic_command("QPAR\r", buf, sizeof(buf)) < 5) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for master/slave for parallel UPSes rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (strspn(buf+1, "0123456789") != (strlen(buf) - 2)) {
		upsdebugx(2, "%s: non numerical value [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	if (strncmp(buf+1, "00", 2) || (strspn(buf+3, "123") != 1)) {
		upsdebugx(2, "%s: invalid reply from UPS [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	upslogx(LOG_INFO, "This UPS is *%s* in a system of UPS in parallel", atoi(buf+1) == 1 ? "master" : "slave");

	return 0;
}

/* voltronic_qbdr() - TESTING NEEDED - I don't really know what this query is meant for (Baud Rate?) */
static int voltronic_qbdr(void)	/* ?? - Need testing with UPS */
{

	char	buf[SMALLBUF];

	/* Query UPS for ??
	 * > [QBDR\r]
	 * < [(1234\r]	<- unknown reply - My UPS (NAK at me
	 *    012345
	 *    0
	 */

	if (!testvar("testing")) {	/* Exec this function only if testing is invoked in ups.conf */
		return 0;
	}

	if ((voltronic_command("QBDR\r", buf, sizeof(buf)) < 5) && (strncasecmp(buf, "(NAK", 4))) {
		upsdebugx(2, "%s: short reply", __func__);
		return -1;
	}

	if (!strncasecmp(buf, "(NAK", 4)) {
		upsdebugx(2, "%s: query for QBDR rejected by UPS", __func__);
		return -1;
	}

	if (buf[0] != '(') {
		upsdebugx(2, "%s: invalid start character [%02x]", __func__, buf[0]);
		return -1;
	}

	if (strspn(buf+1, "0123456789.") != (strlen(buf) - 2)) {
		upsdebugx(2, "%s: non numerical value [%s]", __func__, strtok(buf+1, "\r\n"));
		return -1;
	}

	upslogx(LOG_INFO, "QBDR - ??: [%s]", strtok(buf+1, "\r\n"));

	return 0;
}

/* voltronic_instcmd(name, extra) - Instant command handling */
static int voltronic_instcmd(const char *cmdname, const char *extra)
{
	const struct {
		const char *cmd;
		const char *ups;
	} instcmd[] = {
		{ "load.off", "SOFF\r" },
		{ "load.on", "SON\r" },
		{ "shutdown.stop", "CS\r" },
		{ "test.battery.start.deep", "TL\r" },
		{ "test.battery.start.quick", "T\r" },
		{ "test.battery.stop", "CT\r" },
		{ NULL }
	};

	char	buf[SMALLBUF] = "", buf2[SMALLBUF] = "";
	int	i;

	for (i = 0; instcmd[i].cmd; i++) {

		if (strcasecmp(cmdname, instcmd[i].cmd)) {
			continue;
		}

		snprintf(buf, sizeof(buf), "%s", instcmd[i].ups);

		/* If a command is invalid or rejected, the UPS will reply (NAK, on success UPS will reply (ACK */
		voltronic_command(buf, buf2, sizeof(buf2));
		if (!strncasecmp(buf2, "(ACK", 4)) {
			upslogx(LOG_INFO, "instcmd: command [%s] handled", cmdname);
			return STAT_INSTCMD_HANDLED;
		} else {
			upslogx(LOG_ERR, "instcmd: command [%s] failed", cmdname);
			return STAT_INSTCMD_FAILED;
		}
	}

	if (!strcasecmp(cmdname, "beeper.toggle")) {
		/* If the UPS is beeping then we can call BZOFF
		 * if you previusly set BZOFF we can call BZON
		 * if the beeper is not disabled
		 */
		if (alarm_control) {	/* If the UPS can disable/enable alarm (from UPS capability) */
			if (!strcmp(dstate_getinfo("ups.beeper.status"), "enabled")) {
				snprintf(buf, sizeof(buf), "BZOFF\r");
			} else if (!strcmp(dstate_getinfo("ups.beeper.status"), "muted")) {
				snprintf(buf, sizeof(buf), "BZON\r");
			} else {	/* Beeper disabled */
				upslogx(LOG_INFO, "The beeper is already disabled");
				return STAT_INSTCMD_FAILED;
			}
		} else {
			if (!strcmp(dstate_getinfo("ups.beeper.status"), "enabled")) {
				snprintf(buf, sizeof(buf), "BZOFF\r");
			} else if (!strcmp(dstate_getinfo("ups.beeper.status"), "disabled")) {
				snprintf(buf, sizeof(buf), "BZON\r");
			}
		}
	} else if (!strcasecmp(cmdname, "beeper.enable")) {
		snprintf(buf, sizeof(buf), "PEA\r");
	} else if (!strcasecmp(cmdname, "beeper.disable")) {
		snprintf(buf, sizeof(buf), "PDA\r");
	} else if (!strcasecmp(cmdname, "shutdown.return")) {
		/* Sn: Shutdown after n (-> offdelay) minutes and then turn on when mains is back
		 * SnRm: Shutdown after n (-> offdelay) minutes and then turn on after m (-> ondelay) minutes
		 * Accepted Values for offdelay: .2 -> .9 , 01 -> 99
		 * Accepted Values for ondelay: 0001 -> 9999
		 */
		if (ondelay == 0) {
			if (offdelay < 60) {
				snprintf(buf, sizeof(buf), "S.%d\r", offdelay / 6);
			} else {
				snprintf(buf, sizeof(buf), "S%02d\r", offdelay / 60);
			}
		} else if (offdelay < 60) {
			snprintf(buf, sizeof(buf), "S.%dR%04d\r", offdelay / 6, ondelay);
		} else {
			snprintf(buf, sizeof(buf), "S%02dR%04d\r", offdelay / 60, ondelay);
		}
	} else if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		/* SnR0000
		 * Shutdown after n (-> offdelay) minutes and stay off
		 * Accepted Values for offdelay: .2 -> .9 , 01 -> 99
		 */
		if (offdelay < 60) {
			snprintf(buf, sizeof(buf), "S.%dR0000\r", offdelay / 6);
		} else {
			snprintf(buf, sizeof(buf), "S%02dR0000\r", offdelay / 60);
		}
	} else if (!strcasecmp(cmdname, "test.battery.start")) {
		/* Accepted Values for test time: .2 -> .9 (.2=12sec ..), 01 -> 99 (minutes)
		 * -> you have to invoke test.battery.start + number of seconds [12..5940]
		 */
		int	delay = extra ? strtol(extra, NULL, 10) : 600;
		if (extra && strspn(extra, "0123456789") != strlen(extra)) {
			upslogx(LOG_ERR, "Battery test time: non numerical value [%s]", extra);
			return STAT_INSTCMD_FAILED;
		}
		if ((delay < 12) || (delay > 5940)) {
			upslogx(LOG_ERR, "Battery test time '%d' out of range [12..5940] seconds", delay);
			return STAT_INSTCMD_FAILED;
		} else if (delay < 60) {	/* case: time < 1 minute */
			delay -= (delay % 6);
			snprintf(buf, sizeof(buf), "T.%d\r", delay / 6);
		} else {	/* time > 1 minute */
			delay -= (delay % 60);
			snprintf(buf, sizeof(buf), "T%02d\r", delay / 60);
		}
	} else if (!strcasecmp(cmdname, "outlet.1.load.off")) {
		snprintf(buf, sizeof(buf), "SKOFF1\r");
	} else if (!strcasecmp(cmdname, "outlet.1.load.on")) {
		snprintf(buf, sizeof(buf), "SKON1\r");
	} else if (!strcasecmp(cmdname, "outlet.2.load.off")) {
		snprintf(buf, sizeof(buf), "SKOFF2\r");
	} else if (!strcasecmp(cmdname, "outlet.2.load.on")) {
		snprintf(buf, sizeof(buf), "SKON2\r");
	} else if (!strcasecmp(cmdname, "outlet.3.load.off")) {
		snprintf(buf, sizeof(buf), "SKOFF3\r");
	} else if (!strcasecmp(cmdname, "outlet.3.load.on")) {
		snprintf(buf, sizeof(buf), "SKON3\r");
	} else if (!strcasecmp(cmdname, "outlet.4.load.off")) {
		snprintf(buf, sizeof(buf), "SKOFF4\r");
	} else if (!strcasecmp(cmdname, "outlet.4.load.on")) {
		snprintf(buf, sizeof(buf), "SKON4\r");
	} else if (!strcasecmp(cmdname, "bypass.start")) {
		snprintf(buf, sizeof(buf), "PEE\r");
	} else if (!strcasecmp(cmdname, "bypass.stop")) {
		snprintf(buf, sizeof(buf), "PDE\r");
	} else {
		upslogx(LOG_ERR, "instcmd: command [%s] not found", cmdname);
		return STAT_INSTCMD_UNKNOWN;
	}

	/*
	 * If a command is invalid or rejected, the UPS will reply "(NAK".
	 * In case of success it will reply "(ACK".
	 */

	voltronic_command(buf, buf2, sizeof(buf2));

	if (!strncasecmp(buf2, "(ACK", 4)) {
		upslogx(LOG_INFO, "instcmd: command [%s] handled", cmdname);
		if (!strcasecmp(cmdname, "beeper.disable")) {
			dstate_setinfo("ups.beeper.status", "disabled");
			alarm_control = "disabled";
		} else if (!strcasecmp(cmdname, "beeper.enable") && !strcmp(dstate_getinfo("ups.beeper.status"), "disabled")) {
			dstate_setinfo("ups.beeper.status", "enabled");
			alarm_control = "enabled";
		}
		if (!strcasecmp(cmdname, "bypass.stop")) {
			eco_mode = "disabled";
		} else if (!strcasecmp(cmdname, "bypass.start")) {
			eco_mode = "enabled";
		}

		return STAT_INSTCMD_HANDLED;
	} else {
		upslogx(LOG_ERR, "instcmd: command [%s] failed", cmdname);
		return STAT_INSTCMD_FAILED;
	}
}

/* voltronic_makevartable() - Adding Flag/vars */
void voltronic_makevartable(void)
{
	/* Vars for shutdown & shutdown/return */
	addvar(VAR_VALUE, "ondelay", "Delay before UPS startup [1..9999] (minutes)");
	addvar(VAR_VALUE, "offdelay", "Delay before UPS shutdown [6..5940] (seconds)");
	addvar(VAR_FLAG, "stayoff", "If invoked the UPS won't return after a shutdown when FSD arises");
	/* Capability vars */
	addvar(VAR_FLAG, "reset_to_default", "Reset capability options and their limits to safe default values");
	addvar(VAR_VALUE, "bypass_alarm", "Alarm (BEEP!) at Bypass Mode [enabled/disabled]");
	addvar(VAR_VALUE, "battery_alarm", "Alarm (BEEP!) at Battery Mode [enabled/disabled]");
	addvar(VAR_VALUE, "auto_reboot", "Auto reboot [enabled/disabled]");
	addvar(VAR_VALUE, "bypass_when_off", "Bypass when UPS is Off [enabled/disabled]");
	addvar(VAR_VALUE, "alarm_control", "Alarm (BEEP!) Control [enabled/disabled]");
	addvar(VAR_VALUE, "battery_protection", "Battery deep discharge protection [enabled/disabled]");
	addvar(VAR_VALUE, "converter_mode", "Converter Mode [enabled/disabled]");
	addvar(VAR_VALUE, "eco_mode", "ECO Mode [enabled/disabled]");
	addvar(VAR_VALUE, "energy_saving", "Green power function (Energy Saving -> Auto Off when no load) [enabled/disabled]");
	addvar(VAR_VALUE, "battery_open_status_check", "Battery Open Status Check [enabled/disabled]");
	addvar(VAR_VALUE, "cold_start", "Cold Start [enabled/disabled]");
	addvar(VAR_VALUE, "bypass_forbidding", "Bypass not allowed (Bypass Forbidding) [enabled/disabled]");
	addvar(VAR_VALUE, "outlet_control", "Programmable outlets control (battery mode) [enabled/disabled]");
	addvar(VAR_VALUE, "site_fault_detection", "Site fault detection [enabled/disabled]");
	addvar(VAR_VALUE, "advanced_eco_mode", "Advanced ECO Mode [enabled/disabled]");
	addvar(VAR_VALUE, "constant_phase_angle", "Constant Phase Angle Function (Output and input phase angles are not equal) [enabled/disabled]");
	addvar(VAR_VALUE, "limited_runtime_on_battery", "Limited runtime on battery mode [enabled/disabled]");
	/* End of capability vars */
	/* Vars for limits of options */
	addvar(VAR_VALUE, "max_eco_volt", "Maximum voltage for ECO Mode");
	addvar(VAR_VALUE, "min_eco_volt", "Minimum voltage for ECO Mode");
	addvar(VAR_VALUE, "max_eco_freq", "Maximum frequency for ECO Mode");
	addvar(VAR_VALUE, "min_eco_freq", "Minimum frequency for ECO Mode");
	addvar(VAR_VALUE, "max_bypass_volt", "Maxmimum voltage for Bypass Mode");
	addvar(VAR_VALUE, "min_bypass_volt", "Minimum voltage for Bypass Mode");
	addvar(VAR_VALUE, "max_bypass_freq", "Maximum frequency for Bypass Mode");
	addvar(VAR_VALUE, "min_bypass_freq", "Minimum frequency for Bypass Mode");
	/* Vars for P31 UPSes */
	addvar(VAR_VALUE, "batt_type", "Battery type for P31 UPSes [Li/Flooded/AGM]");
	addvar(VAR_VALUE, "work_range_type", "Device grid working range for P31 UPSes [Appliance/UPS]");
	/* Vars for programmable outlet control */
	addvar(VAR_VALUE, "outlet1_delay", "Delay time before programmable outlet 1 shuts down the load when on battery mode [0..999] (minutes)");
	addvar(VAR_VALUE, "outlet2_delay", "Delay time before programmable outlet 2 shuts down the load when on battery mode [0..999] (minutes)");
	addvar(VAR_VALUE, "outlet3_delay", "Delay time before programmable outlet 3 shuts down the load when on battery mode [0..999] (minutes)");
	addvar(VAR_VALUE, "outlet4_delay", "Delay time before programmable outlet 4 shuts down the load when on battery mode [0..999] (minutes)");
	/* Var for output phase angle */
	addvar(VAR_VALUE, "output_phase_angle", "Changes output phase angle to the provided value [000, 120, 180, 240]°");
	/* Vars for battery & battery packs number */
	addvar(VAR_VALUE, "battpacks", "Set number of battery packs in parallel to n (integer, 01-99)");
	addvar(VAR_VALUE, "battnumb", "Set number of batteries to n (integer, 1-9)");
	addvar(VAR_VALUE, "battlow", "Set low battery voltage");
	/* For testing purposes */
	addvar(VAR_FLAG, "testing", "If invoked the driver will exec also commands that still need testing");
	/**/
}

/* voltronic_initups() - Check if value for on/off delay are OK */
void voltronic_initups(void)
{
	const char	*val;

	val = getval("ondelay");
	if (val) {
		if (strspn(val, "0123456789") != strlen(val)) {
			fatalx(EXIT_FAILURE, "Start delay: non numerical value [%s]", val);
		}		
		ondelay = strtol(val, NULL, 10);
	}

	if ((ondelay < 0) || (ondelay > 9999)) {
		fatalx(EXIT_FAILURE, "Start delay '%d' out of range [0..9999] (minutes)", ondelay);
	}

	val = getval("offdelay");
	if (val) {
		if (strspn(val, "0123456789") != strlen(val)) {
			fatalx(EXIT_FAILURE, "Shutdown delay: non numerical value [%s]", val);
		}
		offdelay = strtol(val, NULL, 10);
	}

	if ((offdelay < 12) || (offdelay > 5940)) {
		fatalx(EXIT_FAILURE, "Shutdown delay '%d' out of range [12..5940] (seconds)", offdelay);
	}

	/* Truncate to nearest setable value */
	if (offdelay < 60) {
		offdelay -= (offdelay % 6);
	} else {
		offdelay -= (offdelay % 60);
	}
}

/* voltronic_initinfo() - Find supported UPS and try and get/change its infos/settings */
void voltronic_initinfo(void)
{
	int	retry, i;
	int	ret, pro;

	/* voltronic_status() & voltronic_protocol() are mandatory to run this driver */
	upsdebugx(2, "Trying to get status and protocol from UPS...");

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* Querying UPS for status */
		ret = voltronic_status();
		if (ret < 0) {
			upsdebugx(2, "Status read %d failed", retry);
			continue;
		}
		upsdebugx(2, "Status read in %d tries", retry);
		break;
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* Querying UPS for protocol */
		pro = voltronic_protocol();
		if (pro < 0) {
			upsdebugx(2, "Protocol read %d failed", retry);
			continue;
		}
		upsdebugx(2, "Protocol read in %d tries", retry);
		break;
	}

	if (!ret && pro < 0) {
		fatalx(EXIT_FAILURE, "Uh-oh! Your UPS is not supported by this driver. Try instead the more generic blazer. It should work.. more or less");
	} else if (!ret && !pro) {
		upslogx(LOG_INFO, "Supported UPS detected");
	} else {
		fatalx(EXIT_FAILURE, "No supported UPS detected");
	}

	/* Rated informations */
	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #1 - voltronic_ratings1() */
		ret = voltronic_ratings1();
		if (ret < 0) {
			upsdebugx(1, "Ratings #1 read %d failed", retry);
			continue;
		}
		upslogx(LOG_INFO, "Ratings #1 read in %d tries", retry);
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Error while reading rated informations #1");
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #2 - voltronic_ratings2() */
		ret = voltronic_ratings2();
		if (ret < 0) {
			upsdebugx(1, "Ratings #2 read %d failed", retry);
			continue;
		}
		upslogx(LOG_INFO, "Ratings #2 read in %d tries", retry);
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Error while reading rated informations #2");
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #3 - voltronic_ratings3() */
		ret = voltronic_ratings3();
		if (ret < 0) {
			upsdebugx(1, "Ratings #3 read %d failed", retry);
			continue;
		}
		upslogx(LOG_INFO, "Ratings #3 read in %d tries", retry);
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Error while reading rated informations #3");
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #4 - voltronic_vendor() */
		ret = voltronic_vendor();
		if (ret < 0) {
			upsdebugx(1, "Vendor informations read %d failed", retry);
			continue;
		}
		upslogx(LOG_INFO, "Vendor informations read in %d tries", retry);
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Error while reading vendor informations");
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #5 - voltronic_firmware() */
		ret = voltronic_firmware();
		if (ret < 0) {
			upsdebugx(1, "Firmware version read %d failed", retry);
			continue;
		}
		upslogx(LOG_INFO, "Firmware version read in %d tries", retry);
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Firmware version unavailable");
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #6 - voltronic_serial() */
		ret = voltronic_serial();
		if (ret < 0) {
			upsdebugx(1, "Serial number read %d failed", retry);
			continue;
		}
		upslogx(LOG_INFO, "Serial number read in %d tries", retry);
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Serial number unavailable");
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #7 - voltronic_manufacturer() */
		ret = voltronic_manufacturer();
		if (ret < 0) {
			upsdebugx(1, "Manufacturer read %d failed", retry);
			continue;
		}
		upslogx(LOG_INFO, "Manufacturer read in %d tries", retry);
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Manufacturer information unavailable");
	}
	/* End of rated informations */

	/* Populating NUT vars.. */
	/* If we have problems here we won't use the problematic function in upsdr_updateinfo(), 
	 * so the driver will be able to use voltronic_status() to check basic status
	 * and give us at least a minimal protection - Valid also for voltronic_outlet()
	 */
	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #1 - voltronic_battery() */
		ret = voltronic_battery();
		if (ret < 0) {
			upsdebugx(1, "Battery informations read #%d failed", retry);
			continue;
		}
		upslogx(LOG_INFO, "Battery informations read in %d tries", retry);
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Error while reading battery informations");
		battery = 0;
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #2 - voltronic_load_level() */
		ret = voltronic_load_level();
		if (ret < 0) {
			upsdebugx(1, "Last seen minimum/maximum load level: read #%d failed", retry);
			continue;
		}
		upslogx(LOG_INFO, "Last seen minimum/maximum load level read in %d tries", retry);
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Error while reading minimum/maximum load level");
		loadlevel = 0;
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #3 - voltronic_multi() */
		ret = voltronic_multi();	/* need voltronic_ratings2() */
		if (ret < 0) {
			upsdebugx(1, "Multi-phase voltage/frequency read #%d failed", retry);
			continue;
		}
		if (ret > 0) {
			upslogx(LOG_INFO, "Multi-phase voltage/frequency handled in %d tries", retry);
			multi = 0;	/* If UPS is not multi-phase, we'll skip voltronic_multi() in upsdrv_updateinfo() */
			break;
		}
		upslogx(LOG_INFO, "Multi-phase voltage/frequency read in %d tries", retry);
		break;
	}
	if (ret < 0) {
		upslogx(LOG_DEBUG, "Error while reading multi-phase voltage/frequency");
		multi = 0;
	}

	/* Capability informations and settings */
	for (retry = 1; retry <= MAXTRIES; retry++) {
		ret = voltronic_capability();
		if (ret < 0) {
			upsdebugx(1, "UPS capability read %d failed", retry);
			continue;
		}
		upslogx(LOG_INFO, "UPS capability read in %d tries", retry);

		for (i = 1; i <= MAXTRIES; i++) {
			pro = voltronic_check();	/* need voltronic_capability() (& voltronic_mode() for resetability) */
			if (pro < 0) {
				upsdebugx(1, "Capability options' handling attempt #%d failed", i);
				continue;
			}
			if (!pro) {
				upslogx(LOG_INFO, "Capability options handled in %d tries", i);
			}
			break;
		}
		if (pro) {
			upslogx(LOG_DEBUG, "Error while handling capability options");
		}
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "UPS capability information unavailable");
	}

	/* Other options/settings.. */
	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #1 - voltronic_batt_low() */
		ret = voltronic_batt_low();	/* need voltronic_ratings1() / voltronic_ratings2() */
		if (ret < 0) {
			upsdebugx(1, "Battery low voltage options: handling attempt #%d failed", retry);
			continue;
		}
		if (!ret) {
			upslogx(LOG_INFO, "Battery low voltage options handled in %d tries", retry);
		}
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Error while handling battery low voltage options");
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #2 - voltronic_batt_numb() */
		ret = voltronic_batt_numb();	/* need voltronic_battery() */
		if (ret < 0) {
			upsdebugx(1, "Number of batteries: handling attempt #%d failed", retry);
			continue;
		}
		if (!ret) {
			upslogx(LOG_INFO, "Number of batteries handled in %d tries", retry);
		}
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Error while handling number of batteries");
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #3 - voltronic_batt_packs() */
		ret = voltronic_batt_packs();	/* need voltronic_battery() */
		if (ret < 0) {
			upsdebugx(1, "Number of battery packs in parallel: handling attempt #%d failed", retry);
			continue;
		}
		if (!ret) {
			upslogx(LOG_INFO, "Number of battery packs in parallel handled in %d tries", retry);
		}
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Error while handling number of battery packs in parallel");
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #4 - voltronic_eco() */
		ret = voltronic_eco();	/* need voltronic_ratings1() / voltronic_ratings2() */
		if (ret == -1) {
			upsdebugx(1, "ECO Mode - Voltage and frequency limits: handling attempt #%d failed", retry);
			continue;
		}
		if (!ret || ret == -2) {
			upslogx(LOG_INFO, "ECO Mode - Voltage and frequency limits handled in %d tries", retry);
		}
		break;
	}
	if (ret && ret != -2) {
		upslogx(LOG_DEBUG, "Error while handling ECO Mode voltage and frequency limits");
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #5 - voltronic_bypass() */
		ret = voltronic_bypass();	/* need voltronic_ratings1() & voltronic_ratings2() */
		if (ret == -1) {
			upsdebugx(1, "Bypass Mode - Voltage and frequency limits: handling attempt #%d failed", retry);
			continue;
		}
		if (!ret || ret == -2) {
			upslogx(LOG_INFO, "Bypass Mode - Voltage and frequency limits handled in %d tries", retry);
		}
		break;
	}
	if (ret && ret != -2) {
		upslogx(LOG_DEBUG, "Error while handling Bypass Mode voltage and frequency limits");
	}

	for (i = 1, ret=0; i < 5 && ret != -1; i++) {	/* #6 - voltronic_outlet() */
		for (retry = 1; retry <= MAXTRIES; retry++) {
			ret = voltronic_outlet(i, 1);
			if (ret == 1) {
				upsdebugx(1, "Programmable outlet #%d: handling attempt #%d failed", i, retry);
				continue;
			}
			if (!ret || (ret == -2)) {
				upslogx(LOG_INFO, "Programmable outlet #%d handled in %d tries", i, retry);
			}
			break;
		}
		if (ret > 0) {
			upslogx(LOG_DEBUG, "Error while handling programmable outlet #%d", i);
		}
		/* See explanation on 'Populating NUT vars..' - To use in % (modulo) in upsdrv_updateinfo(): 1 -> 2, 2 -> 3, 3 -> 5, 4 -> 7 */
		outlet = outlet * ((ret == 1 || ret == -2) ? (i == 1 ? 2 : i == 2 ? 3 : i == 3 ? 5 : i == 4 ? 7 : 1) : 1);
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #7 - voltronic_parallel() */
		ret = voltronic_parallel();
		if (ret < 0) {
			upsdebugx(1, "Master/slave for UPS in parallel: determining attempt #%d failed", retry);
			continue;
		}
		upslogx(LOG_INFO, "Master/slave for UPS in parallel determined in %d tries", retry);
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Error while determining master/slave for UPS in parallel");
	}

	for (retry = 1; retry <= MAXTRIES; retry++) {	/* #8 - voltronic_phase() */
		ret = voltronic_phase();
		if (ret < 0) {
			upsdebugx(1, "Input/Output phase angles: handling attempt #%d failed", retry);
			continue;
		}
		if (!ret) {
			upslogx(LOG_INFO, "Input/Output phase angles handled in %d tries", retry);
		}
		break;
	}
	if (ret) {
		upslogx(LOG_DEBUG, "Error while handling input/output phase angles");
	}

	/* For P31 UPSes */
	if (!strcmp(dstate_getinfo("ups.firmware.aux"), "P31")) {	/* Query & command work only on P31 UPSes */
		for (retry = 1; retry <= MAXTRIES; retry++) {	/* #9 - voltronic_p31b() */
			ret = voltronic_p31b();
			if (ret < 0) {
				upsdebugx(1, "Battery type for P31 UPSes: handling attempt #%d failed", retry);
				continue;
			}
			if (!ret) {
				upslogx(LOG_INFO, "Battery type for P31 UPSes handled in %d tries", retry);
			}
			break;
		}
		if (ret) {
			upslogx(LOG_DEBUG, "Error while handling battery type for P31 UPSes");
		}

		for (retry = 1; retry <= MAXTRIES; retry++) {	/* #10 - voltronic_p31g() */
			ret = voltronic_p31g();
			if (ret < 0) {
				upsdebugx(1, "Device grid working range for P31 UPSes: handling attempt #%d failed", retry);
				continue;
			}
			if (!ret) {
				upslogx(LOG_INFO, "Device grid working range for P31 UPSes handled in %d tries", retry);
			}
			break;
		}
		if (ret) {
			upslogx(LOG_DEBUG, "Error while handling device grid working range for P31 UPSes");
		}
	}

	/* The following function need testing with UPS, since I don't know what the UPS will reply to the query */
	if (testvar("testing")) {
		for (retry = 1; retry <= MAXTRIES; retry++) {	/* #1 - voltronic_qbdr() */
			ret = voltronic_qbdr();
			if (ret < 0) {
				upsdebugx(1, "voltronic_qbdr: read #%d failed", retry);
				continue;
			}
			upslogx(LOG_INFO, "voltronic_qbdr read in %d tries", retry);
			break;
		}
		if (ret) {
			upslogx(LOG_DEBUG, "Error while reading voltronic_qbdr");
		}

	}

	/* Register delay in nut variables */
	dstate_setinfo("ups.delay.start", "%d", 60 * ondelay);
	dstate_setinfo("ups.delay.shutdown", "%d", offdelay);

	/* Populating instant commands */
	dstate_addcmd("beeper.toggle");
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.start.deep");
	dstate_addcmd("test.battery.start.quick");
	dstate_addcmd("test.battery.stop");

	upsh.instcmd = voltronic_instcmd;
}

/* upsdrv_updateinfo() - Update UPS status/infos */
void upsdrv_updateinfo(void)
{
	static int	retry = 0;

	if (voltronic_status() || (battery && voltronic_battery()) || (loadlevel && voltronic_load_level()) || (multi && (voltronic_multi() < 0)) || ((dstate_getinfo("outlet.0.switchable") && !strcmp(dstate_getinfo("outlet.0.switchable"), "yes")) ? (((outlet % 2) && voltronic_outlet(1, 0) > 0) || ((outlet % 3) && voltronic_outlet(2, 0) > 0) || ((outlet % 5) && voltronic_outlet(3, 0) > 0) || ((outlet % 7) && voltronic_outlet(4, 0) > 0)) : 1==0)) {

		if (retry < MAXTRIES) {
			upsdebugx(1, "Communications with UPS lost: status read failed!");
			retry++;
		} else if (retry == MAXTRIES) {
			upslogx(LOG_WARNING, "Communications with UPS lost: status read failed!");
			retry++;
		} else {
			dstate_datastale();
		}

		return;
	}

	if (retry > MAXTRIES) {
		upslogx(LOG_NOTICE, "Communications with UPS re-established");
	}

	retry = 0;

	dstate_dataok();
}

/* upsdrv_shutdown() - Try and shutdown UPS */
void upsdrv_shutdown(void)
{
	int	retry;

	/* Stop pending shutdowns */
	for (retry = 1; retry <= MAXTRIES; retry++) {

		if (voltronic_instcmd("shutdown.stop", NULL) != STAT_INSTCMD_HANDLED) {
			continue;
		}

		break;

	}

	if (retry > MAXTRIES) {
		upslogx(LOG_NOTICE, "No shutdown pending");
	}

	/* Shutdown */
	for (retry = 1; retry <= MAXTRIES; retry++) {

		if (testvar("stayoff")) {	/* If you set stayoff in ups.conf when FSD arises the UPS will shutdown after *offdelay* seconds and won't return.. */
			if (voltronic_instcmd("shutdown.stayoff", NULL) != STAT_INSTCMD_HANDLED) {
				continue;
			}			
		} else {	/* ..otherwise (standard behaviour) the UPS will shutdown after *offdelay* seconds and then turn on after *ondelay* minutes (if mains meanwhile returned) */
			if (voltronic_instcmd("shutdown.return", NULL) != STAT_INSTCMD_HANDLED) {
				continue;
			}
		}

		fatalx(EXIT_SUCCESS, "Shutting down in %d seconds", offdelay);
	}

	fatalx(EXIT_FAILURE, "Shutdown failed!");
}
