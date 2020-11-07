/* tripplitesu.c - model specific routines for
                   Tripp Lite SmartOnline (SU*) models

   Copyright (C) 2003  Allan N. Hessenflow <allanh@kallisti.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/* Notes:

   The map for commands_available isn't clear.  All of the information
   I have on the Tripp Lite SmartOnline protocol comes from the files
   TUN.tlp and TUNRT.tlp from their drivers, and some experimentation.
   One of those files told me what one bit was for in the AVL response,
   out of 18 that the SU1000RT2U sends or 21 that some other model
   sends.  Later I found a description of the Belkin protocol, which is
   the same.  Unfortunately it gives a definition of the AVL response
   that conflicts with the one bit I found from TUNRT.tlp, and in fact
   the response my SU1000RT2U gives, compared to the commands I have
   found it supports, does not match the Belkin description.  So I'm
   treating the whole field as unknown.  It would be nice to be able to
   use it to determine what variables to make RW.  As a workaround, I'm
   assuming any value I query successfully which also can be set on at
   least one model, can in fact be set on the one I'm talking to.

   I didn't just add Tripp Lite support to the existing Belkin driver
   because I didn't discover that they were the same protocol until
   after I had put more features in this driver than the Belkin driver
   has.  I'm not calling this a unified Belkin/Tripp Lite driver for a
   couple of reasons.  One is that I don't have a Belkin to test with
   (and so I explicitly check for Tripp Lite in this drivers
   initialization - it *may* work fine with a Belkin by simply removing
   that test).  The other reason is that I'd have to come up with a
   name - I don't know a name for the protocol, and I don't know which
   company (if either) originated the protocol.  Picking one of the
   companies arbitrarily to name it after just seems wrong.

   There are a few things still to add.  Primarily there's control of
   individual outlet banks, using (I presume) outlet.n.x variables.
   I'll probably wait for an example of that to show up in some other
   driver before adding that, to try to make sure I do it in the way
   that Russell Kroll envisioned.  It also might be nice to give the
   user control over the delays before shutdown and restart, probably
   with additional driver parameters.  Letting the user turn the buzzer
   off might be nice too; for that I'd have to investigate whether the
   command to do that disables it entirely, or just turns it off during
   the existing alarm condition, to determine whether it would be better
   implemented through variables or instant commands, and then of course
   request that those get added to the list of known names.  Finally,
   there are a number of other alarm conditions that can be reported
   that would be nice to pass on to the user; these would require new
   variables or new status values.


   The following parameters (ups.conf) are supported:
	lowbatt

   The following variables are supported (RW = read/write):
	ambient.humidity (1)
	ambient.temperature (1)
	battery.charge
	battery.current (1)
	battery.temperature
	battery.voltage
	battery.voltage.nominal
	input.frequency
	input.sensitivity (RW) (1)
	input.transfer.high (RW)
	input.transfer.low (RW)
	input.voltage
	input.voltage.nominal
	output.current (1)
	output.frequency
	output.voltage
	output.voltage.nominal
	ups.firmware
	ups.id (RW) (1)
	ups.load
	ups.mfr
	ups.model
	ups.status
	ups.test.result
	ups.contacts (1)

    The following instant commands are supported:
	load.off
	load.on
	shutdown.reboot
	shutdown.reboot.graceful
	shutdown.return
	shutdown.stop
	test.battery.start
	test.battery.stop

    The following ups.status values are supported:
	BOOST (1)
	BYPASS
	LB
	OB
	OFF
	OL
	OVER (2)
	RB (2)
	TRIM (1)

    (1) these items have not been tested because they are not supported
    by my SU1000RT2U.
    (2) these items have not been tested because I haven't tested them.
*/


#include "main.h"
#include "serial.h"

#define DRIVER_NAME		"Tripp Lite SmartOnline driver"
#define DRIVER_VERSION	"0.05"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Allan N. Hessenflow <allanh@kallisti.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define MAX_RESPONSE_LENGTH 256

static const char *test_result_names[] = {
	"No test performed",
	"Passed",
	"In progress",
	"General test failed",
	"Battery failed",
	"Deep battery test failed",
	"Aborted"
};

static struct {
	int code;
	const char *name;
} sensitivity[] = {
	{0, "Normal"},
	{1, "Reduced"},
	{2, "Low"}
};

static struct {
	int outlet_banks;
	unsigned long commands_available;
} ups;

/* bits in commands_available */
#define WDG_AVAILABLE            (1UL <<  1)

/* message types */
#define POLL             'P'
#define SET              'S'
#define ACCEPT           'A'
#define REJECT           'R'
#define DATA             'D'

/* commands */
#define AUTO_REBOOT                  "ARB" /* poll/set */
#define AUTO_TEST                    "ATT" /* poll/set */
#define ATX_REBOOT                   "ATX" /* poll/set */
#define AVAILABLE                    "AVL" /* poll */
#define BATTERY_REPLACEMENT_DATE     "BRD" /* poll/set */
#define BUZZER_TEST                  "BTT" /* set */
#define BATTERY_TEST                 "BTV" /* poll/set */
#define BUZZER                       "BUZ" /* set */
#define ECONOMIC_MODE                "ECO" /* set */
#define ENABLE_BUZZER                "EDB" /* set */
#define ENVIRONMENT_INFORMATION      "ENV" /* poll */
#define OUTLET_RELAYS                "LET" /* poll */
#define MANUFACTURER                 "MNU" /* poll */
#define MODEL                        "MOD" /* poll */
#define RATINGS                      "RAT" /* poll */
#define RELAY_CYCLE                  "RNF" /* set */
#define RELAY_OFF                    "ROF" /* set */
#define RELAY_ON                     "RON" /* set */
#define ATX_RESUME                   "RSM" /* set */
#define SHUTDOWN_ACTION              "SDA" /* set */
#define SHUTDOWN_RESTART             "SDR" /* set */
#define SHUTDOWN_TYPE                "SDT" /* poll/set */
#define RELAY_STATUS                 "SOL" /* poll/set */
#define SELECT_OUTPUT_VOLTAGE        "SOV" /* poll/set */
#define STATUS_ALARM                 "STA" /* poll */
#define STATUS_BATTERY               "STB" /* poll */
#define STATUS_INPUT                 "STI" /* poll */
#define STATUS_OUTPUT                "STO" /* poll */
#define STATUS_BYPASS                "STP" /* poll */
#define TELEPHONE                    "TEL" /* poll/set */
#define TEST_RESULT                  "TSR" /* poll */
#define TEST                         "TST" /* set */
#define TRANSFER_FREQUENCY           "TXF" /* poll/set */
#define TRANSFER_VOLTAGE             "TXV" /* poll/set */
#define BOOT_DELAY                   "UBD" /* poll/set */
#define BAUD_RATE                    "UBR" /* poll/set */
#define IDENTIFICATION               "UID" /* poll/set */
#define VERSION_CMD                  "VER" /* poll */
#define VOLTAGE_SENSITIVITY          "VSN" /* poll/set */
#define WATCHDOG                     "WDG" /* poll/set */


static int do_command(char type, const char *command, const char *parameters, char *response)
{
	char	buffer[SMALLBUF];
	int	count, ret;

	ser_flush_io(upsfd);

	if (response) {
		*response = '\0';
	}

	snprintf(buffer, sizeof(buffer), "~00%c%03d%s%s", type, (int)(strlen(command) + strlen(parameters)), command, parameters);

	ret = ser_send_pace(upsfd, 10000, "%s", buffer);
	if (ret <= 0) {
		upsdebug_with_errno(3, "do_command: send [%s]", buffer);
		return -1;
	}

	upsdebugx(3, "do_command: %d bytes sent [%s] -> OK", ret, buffer);

	ret = ser_get_buf_len(upsfd, (unsigned char *)buffer, 4, 3, 0);
	if (ret < 0) {
		upsdebug_with_errno(3, "do_command: read");
		return -1;
	}
	if (ret == 0) {
		upsdebugx(3, "do_command: read -> TIMEOUT");
		return -1;
	}

	buffer[ret] = '\0';
	upsdebugx(3, "do_command: %d byted read [%s]", ret, buffer);

	if (!strcmp(buffer, "~00D")) {

		ret = ser_get_buf_len(upsfd, (unsigned char *)buffer, 3, 3, 0);
		if (ret < 0) {
			upsdebug_with_errno(3, "do_command: read");
			return -1;
		}
		if (ret == 0) {
			upsdebugx(3, "do_command: read -> TIMEOUT");
			return -1;
		}

		buffer[ret] = '\0';
		upsdebugx(3, "do_command: %d bytes read [%s]", ret, buffer);

		count = atoi(buffer);
		if (count >= MAX_RESPONSE_LENGTH) {
			upsdebugx(3, "do_command: response exceeds expected size!");
			return -1;
		}

		if (count && !response) {
			upsdebugx(3, "do_command: response not expected!");
			return -1;
		}

		if (count == 0) {
			return 0;
		}

		ret = ser_get_buf_len(upsfd, (unsigned char *)response, count, 3, 0);
		if (ret < 0) {
			upsdebug_with_errno(3, "do_command: read");
			return -1;
		}
		if (ret == 0) {
			upsdebugx(3, "do_command: read -> TIMEOUT");
			return -1;
		}

		response[ret] = '\0';
		upsdebugx(3, "do_command: %d bytes read [%s]", ret, response);

		/* Tripp Lite pads their string responses with spaces.
		   I don't like that, so I remove them.  This is safe to
		   do with all responses for this protocol, so I just
		   do that here. */
		str_rtrim(response, ' ');

		return ret;
	}

	if (!strcmp(buffer, "~00A")) {
		return 0;
	}

	return -1;
}

static char *field(char *str, int fieldnum)
{

	while (str && fieldnum--) {
		str = strchr(str, ';');
		if (str)
			str++;
	}
	if (str && *str == ';')
		return NULL;

	return str;
}

static int get_identification(void) {
	char response[MAX_RESPONSE_LENGTH];

	if (do_command(POLL, IDENTIFICATION, "", response) >= 0) {
		dstate_setinfo("ups.id", "%s", response);
		return 1;
	}

	return 0;
}

static void set_identification(const char *val) {
	char response[MAX_RESPONSE_LENGTH];

	if (do_command(POLL, IDENTIFICATION, "", response) < 0)
		return;
	if (strcmp(val, response)) {
		strncpy(response, val, MAX_RESPONSE_LENGTH);
		response[MAX_RESPONSE_LENGTH - 1] = '\0';
		do_command(SET, IDENTIFICATION, response, NULL);
	}
}

static int get_transfer_voltage_low(void) {
	char response[MAX_RESPONSE_LENGTH];
	char *ptr;

	if (do_command(POLL, TRANSFER_VOLTAGE, "", response) > 0) {
		ptr = field(response, 0);
		if (ptr)
			dstate_setinfo("input.transfer.low", "%d", atoi(ptr));
		return 1;
	}

	return 0;
}

static void set_transfer_voltage_low(int val) {
	char response[MAX_RESPONSE_LENGTH];
	char *ptr;
	int high;

	if (do_command(POLL, TRANSFER_VOLTAGE, "", response) <= 0)
		return;
	ptr = field(response, 0);
	if (!ptr || val == atoi(ptr))
		return;
	ptr = field(response, 1);
	if (!ptr)
		return;
	high = atoi(ptr);
	snprintf(response, sizeof(response), "%d;%d", val, high);
	do_command(SET, TRANSFER_VOLTAGE, response, NULL);
}

static int get_transfer_voltage_high(void) {
	char response[MAX_RESPONSE_LENGTH];
	char *ptr;

	if (do_command(POLL, TRANSFER_VOLTAGE, "", response) > 0) {
		ptr = field(response, 1);
		if (ptr)
			dstate_setinfo("input.transfer.high", "%d", atoi(ptr));
		return 1;
	}

	return 0;
}

static void set_transfer_voltage_high(int val) {
	char response[MAX_RESPONSE_LENGTH];
	char *ptr;
	int low;

	if (do_command(POLL, TRANSFER_VOLTAGE, "", response) <= 0)
		return;
	ptr = field(response, 0);
	if (!ptr)
		return;
	low = atoi(ptr);
	ptr = field(response, 1);
	if (!ptr || val == atoi(ptr))
		return;
	snprintf(response, sizeof(response), "%d;%d", low, val);
	do_command(SET, TRANSFER_VOLTAGE, response, NULL);
}

static int get_sensitivity(void) {
	char response[MAX_RESPONSE_LENGTH];
	unsigned int i;

	if (do_command(POLL, VOLTAGE_SENSITIVITY, "", response) <= 0)
		return 0;
	for (i = 0; i < sizeof(sensitivity) / sizeof(sensitivity[0]); i++) {
		if (sensitivity[i].code == atoi(response)) {
			dstate_setinfo("input.sensitivity", "%s",
			               sensitivity[i].name);
			return 1;
		}
	}

	return 0;
}

static void set_sensitivity(const char *val) {
	char parm[20];
	unsigned int i;

	for (i = 0; i < sizeof(sensitivity) / sizeof(sensitivity[0]); i++) {
		if (!strcasecmp(val, sensitivity[i].name)) {
			snprintf(parm, sizeof(parm), "%u", i);
			do_command(SET, VOLTAGE_SENSITIVITY, parm, NULL);
			break;
		}
	}
}

static void auto_reboot(int enable) {
	char parm[20];
	char response[MAX_RESPONSE_LENGTH];
	char *ptr;
	int mode;

	if (enable)
		mode = 1;
	else
		mode = 2;
	if (do_command(POLL, AUTO_REBOOT, "", response) <= 0)
		return;
	ptr = field(response, 0);
	if (!ptr || atoi(ptr) != mode) {
		snprintf(parm, sizeof(parm), "%d", mode);
		do_command(SET, AUTO_REBOOT, parm, NULL);
	}
}

static int instcmd(const char *cmdname, const char *extra)
{
	int i;
	char parm[20];

	if (!strcasecmp(cmdname, "load.off")) {
		for (i = 0; i < ups.outlet_banks; i++) {
			snprintf(parm, sizeof(parm), "%d;1", i + 1);
			do_command(SET, RELAY_OFF, parm, NULL);
		}
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "load.on")) {
		for (i = 0; i < ups.outlet_banks; i++) {
			snprintf(parm, sizeof(parm), "%d;1", i + 1);
			do_command(SET, RELAY_ON, parm, NULL);
		}
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "shutdown.reboot")) {
		auto_reboot(1);
		do_command(SET, SHUTDOWN_RESTART, "1", NULL);
		do_command(SET, SHUTDOWN_ACTION, "10", NULL);
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "shutdown.reboot.graceful")) {
		auto_reboot(1);
		do_command(SET, SHUTDOWN_RESTART, "1", NULL);
		do_command(SET, SHUTDOWN_ACTION, "60", NULL);
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "shutdown.return")) {
		auto_reboot(1);
		do_command(SET, SHUTDOWN_RESTART, "1", NULL);
		do_command(SET, SHUTDOWN_ACTION, "10", NULL);
		return STAT_INSTCMD_HANDLED;
	}
#if 0 /* doesn't seem to work */
	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		auto_reboot(0);
		do_command(SET, SHUTDOWN_ACTION, "10", NULL);
		return STAT_INSTCMD_HANDLED;
	}
#endif
	if (!strcasecmp(cmdname, "shutdown.stop")) {
		do_command(SET, SHUTDOWN_ACTION, "0", NULL);
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "test.battery.start")) {
		do_command(SET, TEST, "3", NULL);
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		do_command(SET, TEST, "0", NULL);
		return STAT_INSTCMD_HANDLED;
	}
	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static int setvar(const char *varname, const char *val)
{

	if (!strcasecmp(varname, "ups.id")) {
		set_identification(val);
		get_identification();
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "input.transfer.low")) {
		set_transfer_voltage_low(atoi(val));
		get_transfer_voltage_low();
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "input.transfer.high")) {
		set_transfer_voltage_high(atoi(val));
		get_transfer_voltage_high();
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "input.sensitivity")) {
		set_sensitivity(val);
		get_sensitivity();
		return STAT_SET_HANDLED;
	}

	upslogx(LOG_NOTICE, "setvar: unknown var [%s]", varname);
	return STAT_SET_UNKNOWN;
}

static int init_comm(void)
{
	int i, bit;
	char response[MAX_RESPONSE_LENGTH];

	ups.commands_available = 0;
	/* Repeat enumerate command 2x, firmware bug on some units garbles 1st response */
	if (do_command(POLL, AVAILABLE, "", response) <= 0){
		upslogx(LOG_NOTICE, "init_comm: Initial response malformed, retrying in 300ms");
		usleep(3E5);
	}
	if (do_command(POLL, AVAILABLE, "", response) <= 0)
		return 0;
	i = strlen(response);
	for (bit = 0; bit < i; bit++)
		if (response[i - bit - 1] == '1')
			ups.commands_available |= (1UL << bit);

	if (do_command(POLL, MANUFACTURER, "", response) <= 0)
		return 0;
	if (strcmp(response, "Tripp Lite"))
		return 0;

	return 1;
}

void upsdrv_initinfo(void)
{
	char response[MAX_RESPONSE_LENGTH];
	unsigned int min_low_transfer, max_low_transfer;
	unsigned int min_high_transfer, max_high_transfer;
	unsigned int i;
	char *ptr;

	if (!init_comm())
		fatalx(EXIT_FAILURE, "Unable to detect Tripp Lite SmartOnline UPS on port %s\n",
		        device_path);
	min_low_transfer = max_low_transfer = 0;
	min_high_transfer = max_high_transfer = 0;

	/* get all the read-only fields here */
	if (do_command(POLL, MANUFACTURER, "", response) > 0)
		dstate_setinfo("ups.mfr", "%s", response);
	if (do_command(POLL, MODEL, "", response) > 0)
		dstate_setinfo("ups.model", "%s", response);
	if (do_command(POLL, VERSION_CMD, "", response) > 0)
		dstate_setinfo("ups.firmware", "%s", response);
	if (do_command(POLL, RATINGS, "", response) > 0) {
		ptr = field(response, 0);
		if (ptr)
			dstate_setinfo("input.voltage.nominal", "%d",
			               atoi(ptr));
		ptr = field(response, 2);
		if (ptr) {
			dstate_setinfo("output.voltage.nominal", "%d",
			               atoi(ptr));
		}
		ptr = field(response, 14);
		if (ptr)
			dstate_setinfo("battery.voltage.nominal", "%d",
			               atoi(ptr));
		ptr = field(response, 10);
		if (ptr)
			min_low_transfer = atoi(ptr);
		ptr = field(response, 9);
		if (ptr)
			max_low_transfer = atoi(ptr);
		ptr = field(response, 12);
		if (ptr)
			min_high_transfer = atoi(ptr);
		ptr = field(response, 11);
		if (ptr)
			max_high_transfer = atoi(ptr);
	}
	if (do_command(POLL, OUTLET_RELAYS, "", response) > 0)
		ups.outlet_banks = atoi(response);
	/* define things that are settable */
	if (get_identification()) {
		dstate_setflags("ups.id", ST_FLAG_RW | ST_FLAG_STRING);
		dstate_setaux("ups.id", 100);
	}
	if (get_transfer_voltage_low() && max_low_transfer) {
		dstate_setflags("input.transfer.low", ST_FLAG_RW);
		for (i = min_low_transfer; i <= max_low_transfer; i++)
			dstate_addenum("input.transfer.low", "%d", i);
	}
	if (get_transfer_voltage_high() && max_low_transfer) {
		dstate_setflags("input.transfer.high", ST_FLAG_RW);
		for (i = min_high_transfer; i <= max_high_transfer; i++)
			dstate_addenum("input.transfer.high", "%d", i);
	}
	if (get_sensitivity()) {
		dstate_setflags("input.sensitivity", ST_FLAG_RW);
		for (i = 0; i < sizeof(sensitivity) / sizeof(sensitivity[0]);
		     i++)
			dstate_addenum("input.sensitivity", "%s",
			               sensitivity[i].name);
	}
	if (ups.outlet_banks) {
		dstate_addcmd("load.off");
		dstate_addcmd("load.on");
	}
	dstate_addcmd("shutdown.reboot");
	dstate_addcmd("shutdown.reboot.graceful");
	dstate_addcmd("shutdown.return");
#if 0 /* doesn't work */
	dstate_addcmd("shutdown.stayoff");
#endif
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");

	/* add all the variables that change regularly */
	upsdrv_updateinfo();

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;

	printf("Detected %s %s on %s\n", dstate_getinfo("ups.mfr"),
	       dstate_getinfo("ups.model"), device_path);
}

void upsdrv_updateinfo(void)
{
	char response[MAX_RESPONSE_LENGTH];
	char *ptr, *ptr2;
	int i;
	int flags;
	int contacts_set;
	int low_battery;

	status_init();
	if (do_command(POLL, STATUS_OUTPUT, "", response) <= 0) {
		dstate_datastale();
		return;
	}
	ptr = field(response, 0);
	/* require output status field to exist */
	if (!ptr) {
		dstate_datastale();
		return;
	}
	switch (atoi(ptr)) {
	case 0:
		status_set("OL");
		break;
	case 1:
		status_set("OB");
		break;
	case 2:
		status_set("BYPASS");
		break;
	case 3:
		status_set("OL");
		status_set("TRIM");
		break;
	case 4:
		status_set("OL");
		status_set("BOOST");
		break;
	case 5:
		status_set("BYPASS");
		break;
	case 6:
		break;
	case 7:
		status_set("OFF");
		break;
	default:
		break;
	}
	ptr = field(response, 6);
	if (ptr)
		dstate_setinfo("ups.load", "%d", atoi(ptr));
	ptr = field(response, 3);
	if (ptr)
		dstate_setinfo("output.voltage", "%03.1f",
		               (double) atoi(ptr) / 10.0);
	ptr = field(response, 1);
	if (ptr)
		dstate_setinfo("output.frequency", "%03.1f",
		               (double) atoi(ptr) / 10.0);
	ptr = field(response, 4);
	if (ptr)
		dstate_setinfo("output.current", "%03.1f",
		               (double) atoi(ptr) / 10.0);

	low_battery = 0;
	if (do_command(POLL, STATUS_BATTERY, "", response) <= 0) {
		dstate_datastale();
		return;
	}
	ptr = field(response, 0);
	if (ptr && atoi(ptr) == 2)
		status_set("RB");
	ptr = field(response, 1);
	if (ptr && atoi(ptr))
		low_battery = 1;
	ptr = field(response, 8);
	if (ptr)
		dstate_setinfo("battery.temperature", "%d", atoi(ptr));
	ptr = field(response, 9);
	if (ptr) {
		dstate_setinfo("battery.charge", "%d", atoi(ptr));
		ptr2 = getval("lowbatt");
		if (ptr2 && atoi(ptr2) > 0 && atoi(ptr2) <= 99 &&
		    atoi(ptr) <= atoi(ptr2))
			low_battery = 1;
	}
	ptr = field(response, 6);
	if (ptr)
		dstate_setinfo("battery.voltage", "%03.1f",
		               (double) atoi(ptr) / 10.0);
	ptr = field(response, 7);
	if (ptr)
		dstate_setinfo("battery.current", "%03.1f",
		               (double) atoi(ptr) / 10.0);
	if (low_battery)
		status_set("LB");

	if (do_command(POLL, STATUS_ALARM, "", response) <= 0) {
		dstate_datastale();
		return;
	}
	ptr = field(response, 3);
	if (ptr && atoi(ptr))
		status_set("OVER");

	if (do_command(POLL, STATUS_INPUT, "", response) > 0) {
		ptr = field(response, 2);
		if (ptr)
			dstate_setinfo("input.voltage", "%03.1f",
			               (double) atoi(ptr) / 10.0);
		ptr = field(response, 1);
		if (ptr)
			dstate_setinfo("input.frequency",
				       "%03.1f", (double) atoi(ptr) / 10.0);
	}

	if (do_command(POLL, TEST_RESULT, "", response) > 0) {
		int	r;
		size_t	trsize;

		r = atoi(response);
		trsize = sizeof(test_result_names) /
			sizeof(test_result_names[0]);

		if ((r < 0) || (r >= (int) trsize))
			r = 0;

		dstate_setinfo("ups.test.result", "%s", test_result_names[r]);
	}

	if (do_command(POLL, ENVIRONMENT_INFORMATION, "", response) > 0) {
		ptr = field(response, 0);
		if (ptr)
			dstate_setinfo("ambient.temperature", "%d", atoi(ptr));
		ptr = field(response, 1);
		if (ptr)
			dstate_setinfo("ambient.humidity", "%d", atoi(ptr));
		flags = 0;
		contacts_set = 0;
		for (i = 0; i < 4; i++) {
			ptr = field(response, 2 + i);
			if (ptr) {
				contacts_set = 1;
				if (*ptr == '1')
					flags |= 1 << i;
			}
		}
		if (contacts_set)
			dstate_setinfo("ups.contacts", "%02X", flags);
	}

	/* if we are here, status is valid */
	status_commit();
	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	char parm[20];

	if (!init_comm())
		printf("Status failed.  Assuming it's on battery and trying a shutdown anyway.\n");
	auto_reboot(1);
	/* in case the power is on, tell it to automatically reboot.  if
	   it is off, this has no effect. */
	snprintf(parm, sizeof(parm), "%d", 1); /* delay before reboot, in minutes */
	do_command(SET, SHUTDOWN_RESTART, parm, NULL);
	snprintf(parm, sizeof(parm), "%d", 5); /* delay before shutdown, in seconds */
	do_command(SET, SHUTDOWN_ACTION, parm, NULL);
}

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x or ups.conf */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "lowbatt", "Set low battery level, in percent");
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
