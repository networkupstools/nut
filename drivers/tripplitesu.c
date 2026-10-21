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

   There are a few things still to add.  There are a number of other alarm
   conditions that can be reported that would be nice to pass on to the
   user; these would require new variables or new status values.


   The following parameters (ups.conf) are supported:
	lowbatt
	command_delay
	offdelay (seconds)
	startdelay (minutes)
	shutdown_mode

   The following variables are supported (RW = read/write):
	ambient.humidity (1)
	ambient.temperature (1)
	battery.charge
	battery.current (1)
	battery.date
	battery.temperature
	battery.voltage
	battery.voltage.nominal
	input.frequency
	input.eco.switchable (RW)
	input.sensitivity (RW) (1)
	input.transfer.high (RW)
	input.transfer.low (RW)
	input.transfer.reason
	input.voltage
	input.voltage.nominal
	outlet.count
	outlet.delay.start (RW)
	outlet.n.status
	outlet.n.switchable
	output.current (1)
	output.frequency
	output.voltage
	output.voltage.nominal
	ups.beeper.status (RW)
	ups.firmware
	ups.id (RW) (1)
	ups.load
	ups.mfr
	ups.model
	ups.status
	ups.start.auto (RW)
	ups.test.result
	ups.watchdog.alarm (RW)
	ups.watchdog.status (RW)
	ups.watchdog.timeout (RW)
	ups.contacts (1)

    The following instant commands are supported:
	load.off
	load.on
	outlet.n.load.off (one per outlet)
	outlet.n.load.on (one per outlet)
	beeper.mute
	clear.fault.record
	reset.watchdog
	shutdown.reboot
	shutdown.reboot.graceful
	shutdown.return
	shutdown.stayoff
	shutdown.stop
	test.battery.start
	test.battery.stop

    "shutdown.return" and "shutdown.stayoff" accept an optional delay, in
    seconds, passed as the "value" argument of upscmd; it takes precedence
    over the "offdelay" setting from ups.conf, which in turn takes precedence
    over the built-in default.  "shutdown.reboot" and
    "shutdown.reboot.graceful" accept "off[:on]": the shutdown delay in
    seconds and an optional restart delay in minutes, separated by one colon.
    Without a colon, only the shutdown delay is changed and the restart delay
    defaults to "startdelay"; with no value at all, the shutdown delay
    defaults to "offdelay" (60 seconds for the graceful variant) as well.
    Fields that are empty, non-numeric, use ';', '.' or ',', or are out of
    range make the command fail before anything is sent.  The "load.off",
    "load.on", "outlet.n.load.*" and "reset.watchdog" commands do not take a
    value.

    Commands that send more than one frame stop at the first operation the
    device rejects or does not answer, so a failed command is never applied
    partially: the driver logs the operation that failed and sends nothing
    more.  "load.off" and "load.on", for instance, leave the outlets they did
    not reach untouched.

    "shutdown.return" and "shutdown.stayoff" clear the device return delay
    (SDR 0), so the load comes back only when mains is (re)applied or not at
    all, while the two reboot commands ask for the load to come back after
    the restart delay.  The firmware decides how the shutdown and restart
    delays relate; the driver sends both as requested.

    Because that is what the commands mean on this hardware, the shutdown
    commands change the device auto-reboot setting ("ups.start.auto", ARB);
    the device stores it, so the change outlives the shutdown that needs it,
    and the driver logs it (restore it with upsrw if needed).  The buzzer is
    a device setting as well: enabling/disabling it is done through the
    read/write "ups.beeper.status" variable, while the "beeper.mute" command
    only silences the alarm that is currently sounding.  The watchdog is the
    third device setting of that kind: its timeout and restart-alarm flag are
    the read/write "ups.watchdog.timeout" and "ups.watchdog.alarm"
    variables, "ups.watchdog.status" is their enabled/disabled view, and the
    "reset.watchdog" command sends the stored setting again.  A disabled
    watchdog has no timer to kick, so "reset.watchdog" fails then.

    The driver shutdown handler (the "shutdown.default" instant command, run
    by "upsdrvctl shutdown" and by the "driver.killpower" request) turns the
    auto-reboot setting on (ARB), clears the return delay (SDR 0) and then
    schedules the shutdown (SDA) after "offdelay", so that the load is
    powered again only when mains is (re)applied.  The "shutdown_mode"
    setting chooses whether a failure of one of those operations stops the
    handler there ("abort", fail safe) or not ("continue", best effort, the
    default).

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


#include <limits.h>

#include "main.h"
#include "serial.h"
#include "nut_stdint.h"

#define DRIVER_NAME	"Tripp Lite SmartOnline driver"
#define DRIVER_VERSION	"0.13"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Allan N. Hessenflow <allanh@kallisti.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define MAX_RESPONSE_LENGTH 256

/* Delay, in seconds, used by shutdown-related instant commands when the
 * caller does not pass a value and 'offdelay' is not set in ups.conf.
 * Note: a shutdown action value of 0 is used to cancel a scheduled
 * shutdown (see "shutdown.stop"), so it is not accepted as a delay. */
#define DEFAULT_OFFDELAY 10U
#define MIN_OFFDELAY 1U
#define MAX_OFFDELAY 3600U

/* Default delay, in seconds, of the graceful reboot command; unlike the
 * other shutdown commands it is not affected by the 'offdelay' setting. */
#define DEFAULT_GRACEFUL_DELAY 60U

/* Delay, in minutes, before the load is returned (restarted) after a
 * shutdown.  The device command (SDR) takes whole minutes, so this value is
 * used as is, without any rounding or relation to the shutdown delay: the
 * firmware decides how the two delays interact.  Unlike the shutdown action
 * (SDA), a value of 0 is meaningful here and asks the load to return as soon
 * as mains is available.  The ups.conf parameter 'startdelay' and the value
 * used by the reboot commands are both expressed in minutes. */
#define DEFAULT_STARTDELAY_MINUTES 1U
#define MIN_STARTDELAY_MINUTES 0U
#define MAX_STARTDELAY_MINUTES 60U

/* Power-on (boot) delay: how many seconds the device waits before it starts
 * delivering voltage to its outputs (inverter on, load relays closed) when
 * it is powered up or asked to switch them on.  This is a device setting,
 * not an action, so it is only exposed as a read/write variable (no instant
 * command); 0 means "no delay" and the device field is three digits wide. */
#define MAX_BOOTDELAY 999U

/* Watchdog setting (WDG): the device stores a timeout in seconds (up to
 * three digits, 0 disables it) plus a flag selecting whether the restart
 * alarm is enabled (1) or disabled (2).  Both fields are written together,
 * so the driver keeps them apart in the read/write variables
 * ups.watchdog.timeout and ups.watchdog.alarm, and reloads the pair it has
 * when the watchdog is (dis)armed or kicked from NUT. */
#define DEFAULT_WATCHDOG 255U
#define MAX_WATCHDOG 255U

/* Sanity limit on the outlet count reported by the device (LET), so
 * that a garbled answer cannot make the driver publish, register and poll
 * an unbounded number of outlets. */
#define MAX_OUTLETS 16U

/* Command pacing: the device needs time to digest a command before it can
 * accept the next one, so a burst of commands sent back to back (as the
 * driver does while initializing and polling) can make it miss answers.
 * The ups.conf parameter 'command_delay' is the minimum interval to keep
 * between two commands, in milliseconds: 0 disables the pacing (the
 * historical behaviour) and a positive value is the interval itself.  The
 * vendor software paces its own commands by 1.5 to 3 seconds. */
#define DEFAULT_COMMAND_DELAY 0U
#define MAX_COMMAND_DELAY 60000U

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
	int outlets;
	unsigned long commands_available;
} ups;

static unsigned int command_delay = DEFAULT_COMMAND_DELAY; /* minimum interval kept between commands, in milliseconds (0 = no waiting) */
static int command_sent = 0; /* 1 once a command has been sent (see pace_before_command()) */
static struct timeval command_sent_at; /* when the last command was sent */
static unsigned int offdelay = DEFAULT_OFFDELAY; /* delay in seconds before shutdown */
static unsigned int startdelay = DEFAULT_STARTDELAY_MINUTES; /* delay in minutes before return */
static unsigned int watchdog_seconds = 0; /* watchdog timeout currently set on the device */
static unsigned int watchdog_alarm = 2; /* watchdog restart-alarm flag currently set on the device */
static int watchdog_known = 0; /* set once the device reported or accepted its watchdog setting */
static int shutdown_abort_on_error = 0; /* 1 when 'shutdown_mode' asks the driver shutdown handler to stop at the first failure */
/* The two settable transfer voltages: the variable each one is published as
 * and the field it occupies in the TXV reply.  The table order must match the
 * transfer_voltage_id values, which are used as its indexes. */
enum transfer_voltage_id {
	TRANSFER_VOLTAGE_LOW,
	TRANSFER_VOLTAGE_HIGH,
	TRANSFER_VOLTAGE_COUNT
};

static const struct transfer_voltage_setting {
	const char *varname;
	unsigned int field;
} transfer_voltage_settings[TRANSFER_VOLTAGE_COUNT] = {
	{ "input.transfer.low", 0 },
	{ "input.transfer.high", 1 }
};

static unsigned int min_transfer[TRANSFER_VOLTAGE_COUNT]; /* reported minimum */
static unsigned int max_transfer[TRANSFER_VOLTAGE_COUNT]; /* reported maximum */

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
#define BYPASS_REASON                "BPA" /* poll/set */
#define ECONOMIC_MODE                "ECO" /* set; poll is unproven */
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
#define TSU_SHUTDOWN_ACTION          "SDA" /* set */
#define TSU_SHUTDOWN_RESTART         "SDR" /* set */
#define TSU_SHUTDOWN_TYPE            "SDT" /* poll/set */
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


/* Wait until at least 'command_delay' milliseconds have passed since the
 * previous command was sent, so that the device has time to get ready.
 * Nothing is waited for on an idle link, nor when the driver work between
 * two commands (a slow answer, for instance) already took that long: what
 * the device needs is a minimum spacing between commands, not a pause
 * before every single one of them. */
static void pace_before_command(void)
{
	long remaining;

	if (!command_delay || !command_sent)
		return;

	remaining = (long)command_delay - elapsed_since_timeval(&command_sent_at);
	if (remaining <= 0)
		return;

	upsdebugx(3, "%s: waiting %ld ms for the device to digest the "
		"previous command", __func__, remaining);
	usleep((useconds_t)(remaining * 1000L));
}

/* Remember when a command was sent, for pace_before_command(). */
static void note_command_sent(void)
{
	gettimeofday(&command_sent_at, NULL);
	command_sent = 1;
}

/* Send a command and, when 'response' is not NULL, read its reply.  The
 * frame length is bounded by the protocol's three-digit field and by the
 * destination buffer, and the reply is only accepted when it is complete:
 * a short or oversized answer is a failure, never a truncated value.
 * Returns the number of payload bytes read, or -1 on failure. */
static ssize_t do_command(char type, const char *command, const char *parameters,
	char *response, size_t response_size)
{
	char	buffer[SMALLBUF];
	size_t	command_len, parameters_len, payload_len, count, i;
	ssize_t	ret;
	int	frame_len;

	/* let the device get ready for the next command, then drop whatever
	 * it may have sent in the meantime */
	pace_before_command();
	ser_flush_io(upsfd);

	if (response && response_size)
		*response = '\0';

	if (!command)
		return -1;
	if (!parameters)
		parameters = "";

	command_len = strlen(command);
	parameters_len = strlen(parameters);
	if (command_len > 999U || parameters_len > 999U
	    || command_len + parameters_len > 999U) {
		upsdebugx(3, "do_command: request too long [%s%s]",
			command, parameters);
		return -1;
	}
	payload_len = command_len + parameters_len;

	frame_len = snprintf(buffer, sizeof(buffer), "~00%c%03u%s%s",
		type, (unsigned int)payload_len, command, parameters);
	if (frame_len < 0 || (size_t)frame_len >= sizeof(buffer)
	    || (size_t)frame_len != 7U + payload_len) {
		upsdebugx(3, "do_command: request does not fit the buffer");
		return -1;
	}

	ret = ser_send_pace(upsfd, 10000, "%s", buffer);
	if (ret != (ssize_t)frame_len) {
		upsdebug_with_errno(3, "do_command: send [%s]", buffer);
		return -1;
	}

	note_command_sent();
	upsdebugx(3, "do_command: %" PRIiSIZE " bytes sent [%s] -> OK", ret,
		buffer);

	ret = ser_get_buf_len(upsfd, (unsigned char *)buffer, 4, 3, 0);
	if (ret != 4) {
		if (ret < 0)
			upsdebug_with_errno(3, "do_command: read");
		else
			upsdebugx(3, "do_command: short or missing reply");
		return -1;
	}
	buffer[4] = '\0';
	upsdebugx(3, "do_command: %" PRIiSIZE " bytes read [%s]", ret, buffer);

	if (buffer[0] != '~' || buffer[1] != '0' || buffer[2] != '0') {
		upsdebugx(3, "do_command: unexpected reply [%s]", buffer);
		return -1;
	}

	/* SET commands are acknowledged with ~00A; a reply to a POLL
	 * command is a ~00D data frame.  A rejection (~00R) and a reply of
	 * the wrong kind both mean the command failed. */
	if (buffer[3] == ACCEPT)
		return response ? -1 : 0;
	if (buffer[3] != DATA) {
		upsdebugx(3, "do_command: unexpected reply [%s]", buffer);
		return -1;
	}
	if (!response) {
		upsdebugx(3, "do_command: response not expected!");
		return -1;
	}
	if (!response_size) {
		upsdebugx(3, "do_command: no room for the response");
		return -1;
	}

	ret = ser_get_buf_len(upsfd, (unsigned char *)buffer, 3, 3, 0);
	if (ret != 3) {
		if (ret < 0)
			upsdebug_with_errno(3, "do_command: read");
		else
			upsdebugx(3, "do_command: short or missing reply");
		return -1;
	}
	buffer[3] = '\0';
	upsdebugx(3, "do_command: %" PRIiSIZE " bytes read [%s]", ret, buffer);

	for (i = 0; i < 3; i++) {
		if (buffer[i] < '0' || buffer[i] > '9') {
			upsdebugx(3, "do_command: invalid response length [%s]",
				buffer);
			return -1;
		}
	}
	count = (size_t)(buffer[0] - '0') * 100U
		+ (size_t)(buffer[1] - '0') * 10U
		+ (size_t)(buffer[2] - '0');

	if (count >= response_size) {
		upsdebugx(3, "do_command: response of %" PRIuSIZE " bytes "
			"does not fit the %" PRIuSIZE " byte buffer",
			count, response_size);
		return -1;
	}

	if (count == 0)
		return 0;

	ret = ser_get_buf_len(upsfd, (unsigned char *)response, count, 3, 0);
	if (ret != (ssize_t)count) {
		if (ret < 0)
			upsdebug_with_errno(3, "do_command: read");
		else
			upsdebugx(3, "do_command: short reply (%" PRIiSIZE
				" of %" PRIuSIZE " bytes)", ret, count);
		response[0] = '\0';
		return -1;
	}
	response[count] = '\0';
	upsdebugx(3, "do_command: %" PRIiSIZE " bytes read [%s]", ret,
		response);

	/* Tripp Lite pads their string responses with spaces.
	 * I don't like that, so I remove them.  This is safe to
	 * do with all responses for this protocol, so I just
	 * do that here. */
	str_rtrim(response, ' ');

	return ret;
}

/* Send a SET command and tell whether the device accepted it: it answers
 * with a plain ACK (~00A) for accepted operations and with a rejection
 * (~00R) for ones it does not support, while a timeout or a bad answer
 * makes do_command() return a negative value in all those cases. */
static int send_set_command(const char *command, const char *parameters) {
	return do_command(SET, command, parameters, NULL, 0) >= 0;
}

/* Same, but log the failure and return the matching instant command status. */
static int send_set_or_fail(const char *command, const char *parameters) {
	if (send_set_command(command, parameters))
		return STAT_INSTCMD_HANDLED;

	upslogx(LOG_ERR, "device rejected or did not answer [%s %s]",
		command, NUT_STRARG(parameters));
	return STAT_INSTCMD_FAILED;
}

/* Copy field number 'fieldnum' (starting at 0) of a semicolon-separated
 * reply into 'value'.  Returns 0 when the field does not exist or does not
 * fit, so that a malformed reply is rejected instead of being truncated. */
static int get_field(const char *str, unsigned int fieldnum, char *value,
	size_t valuesize)
{
	const char *start = str;
	size_t len;

	if (!str || !value || !valuesize)
		return 0;

	while (fieldnum--) {
		start = strchr(start, ';');
		if (!start)
			return 0;
		start++;
	}

	len = strcspn(start, ";");
	if (len == 0 || len >= valuesize)
		return 0;

	memcpy(value, start, len);
	value[len] = '\0';
	return 1;
}

static int get_identification(void) {
	char response[MAX_RESPONSE_LENGTH];

	if (do_command(POLL, IDENTIFICATION, "", response, sizeof(response)) >= 0) {
		dstate_setinfo("ups.id", "%s", response);
		return 1;
	}

	return 0;
}

static void set_identification(const char *val) {
	char response[MAX_RESPONSE_LENGTH];

	if (do_command(POLL, IDENTIFICATION, "", response, sizeof(response)) < 0)
		return;
	if (strcmp(val, response)) {
		if (strlen(val) >= sizeof(response)) {
			upslogx(LOG_ERR, "%s: value is too long (maximum %"
				PRIuSIZE " characters)", __func__,
				sizeof(response) - 1U);
			return;
		}
		snprintf(response, sizeof(response), "%s", val);
		if (!send_set_command(IDENTIFICATION, response))
			upslogx(LOG_ERR, "%s: device rejected or did not "
				"answer [%s %s]", __func__, IDENTIFICATION,
				response);
	}
}

static int get_transfer_voltage(enum transfer_voltage_id id) {
	const struct transfer_voltage_setting *setting =
		&transfer_voltage_settings[id];
	char response[MAX_RESPONSE_LENGTH];
	char value[32];

	if (do_command(POLL, TRANSFER_VOLTAGE, "", response, sizeof(response)) > 0
	    && get_field(response, setting->field, value, sizeof(value))) {
		dstate_setinfo(setting->varname, "%d", atoi(value));
		return 1;
	}

	return 0;
}

static void set_transfer_voltage(enum transfer_voltage_id id, int val) {
	const struct transfer_voltage_setting *setting =
		&transfer_voltage_settings[id];
	char response[MAX_RESPONSE_LENGTH];
	int voltages[TRANSFER_VOLTAGE_COUNT];
	unsigned int i;

	if (do_command(POLL, TRANSFER_VOLTAGE, "", response, sizeof(response)) <= 0)
		return;
	/* the device takes both voltages at once, and this driver must not
	 * change the other one: leave the setting alone unless both are
	 * readable */
	for (i = 0; i < TRANSFER_VOLTAGE_COUNT; i++) {
		char value[32];

		if (!get_field(response, transfer_voltage_settings[i].field,
			value, sizeof(value)))
			return;
		voltages[i] = atoi(value);
	}
	if (val == voltages[id])
		return;
	voltages[id] = val;
	snprintf(response, sizeof(response), "%d;%d",
		voltages[TRANSFER_VOLTAGE_LOW],
		voltages[TRANSFER_VOLTAGE_HIGH]);
	if (!send_set_command(TRANSFER_VOLTAGE, response))
		upslogx(LOG_ERR, "%s: device rejected or did not answer "
			"[%s %s] for %s", __func__, TRANSFER_VOLTAGE, response,
			setting->varname);
}

/* Store one of the transfer voltage range fields of a RAT reply, leaving the
 * current value alone when the device does not report a usable one. */
static void set_transfer_range(const char *response, unsigned int field,
	unsigned int *target) {
	char value[32];
	int parsed;

	if (!get_field(response, field, value, sizeof(value)))
		return;
	parsed = atoi(value);
	if (parsed >= 0)
		*target = (unsigned int)parsed;
}

static int get_sensitivity(void) {
	char response[MAX_RESPONSE_LENGTH];
	unsigned int i, code;

	if (do_command(POLL, VOLTAGE_SENSITIVITY, "", response, sizeof(response)) <= 0)
		return 0;
	if (!str_to_uint_strict(response, &code, 10))
		return 0;
	for (i = 0; i < SIZEOF_ARRAY(sensitivity); i++) {
		if ((unsigned int)sensitivity[i].code == code) {
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

	for (i = 0; i < SIZEOF_ARRAY(sensitivity); i++) {
		if (!strcasecmp(val, sensitivity[i].name)) {
			snprintf(parm, sizeof(parm), "%u", i);
			if (!send_set_command(VOLTAGE_SENSITIVITY, parm))
				upslogx(LOG_ERR, "%s: device rejected or did "
					"not answer [%s %s]", __func__,
					VOLTAGE_SENSITIVITY, parm);
			break;
		}
	}
}

/* Name the device auto-reboot mode for logging and variable values. */
static const char *auto_reboot_name(int mode) {
	switch (mode) {
	case 1:
		return "yes";
	case 2:
		return "no";
	default:
		return "unknown";
	}
}

static int auto_reboot(int enable) {
	char parm[20];
	char response[MAX_RESPONSE_LENGTH];
	char value[32];
	int mode, oldmode;

	if (enable)
		mode = 1;
	else
		mode = 2;
	if (do_command(POLL, AUTO_REBOOT, "", response, sizeof(response)) <= 0) {
		/* the current setting is unknown (some devices do not report
		 * it), so it cannot be confirmed: report a failure so the
		 * caller aborts instead of powering the load off on a guess */
		upslogx(LOG_ERR, "%s: no answer for [%s], cannot set or "
			"confirm ups.start.auto", __func__, AUTO_REBOOT);
		return 0;
	}
	oldmode = get_field(response, 0, value, sizeof(value)) ? atoi(value) : 0;
	if (oldmode != mode) {
		snprintf(parm, sizeof(parm), "%d", mode);
		if (!send_set_command(AUTO_REBOOT, parm)) {
			upslogx(LOG_ERR, "%s: device rejected or did not answer "
				"[%s %s]", __func__, AUTO_REBOOT, parm);
			return 0;
		}
		/* the device stores this setting, so the change outlives the
		 * shutdown that needs it: make it (and how to undo it) visible */
		upslogx(LOG_INFO, "%s: ups.start.auto changed from '%s' to "
			"'%s' (device setting; restore with "
			"\"upsrw -s ups.start.auto=yes|no\" if needed)",
			__func__, auto_reboot_name(oldmode),
			auto_reboot_name(mode));
	}
	/* keep the published setting in sync with the device, and make it
	 * settable again in case it was not read at startup */
	dstate_setinfo("ups.start.auto", "%s", auto_reboot_name(mode));
	dstate_setflags("ups.start.auto", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.start.auto", 3);
	return 1;
}

/* A device setting that the protocol reports as a single number: 1 selects
 * the first name published in the variable, 2 the second one.  The variable
 * name and both names live in the table, so that reading any of these
 * settings is the same code.  The table order must match the identifiers
 * below, which are used as its indexes. */
enum two_state_setting_id {
	TWO_STATE_AUTO_REBOOT,
	TWO_STATE_ECO_MODE,
	TWO_STATE_BEEPER
};

struct two_state_setting {
	const char *command;	/* protocol POLL command */
	const char *varname;	/* variable the setting is published as */
	const char *what;	/* description used in the log messages */
	const char *on_name;	/* name published for value 1 */
	const char *off_name;	/* name published for value 2 */
};

static const struct two_state_setting two_state_settings[] = {
	{ AUTO_REBOOT, "ups.start.auto", "auto-reboot setting",
		"yes", "no" },
	{ ECONOMIC_MODE, "input.eco.switchable", "ECO setting",
		"ECO", "normal" },
	{ ENABLE_BUZZER, "ups.beeper.status", "buzzer setting",
		"enabled", "disabled" }
};

/* Read a two-state setting (see above) and publish it.  This is auxiliary
 * information, so a missing or unexpected answer must not mark the whole
 * UPS as stale: a warning is logged and the previously published value is
 * left in place.  Returns 1 when the setting was published. */
static int get_two_state_setting(enum two_state_setting_id id) {
	const struct two_state_setting *setting = &two_state_settings[id];
	char response[MAX_RESPONSE_LENGTH];
	unsigned int value;

	if (do_command(POLL, setting->command, "", response, sizeof(response)) <= 0) {
		upslogx(LOG_WARNING, "%s: could not read the %s [%s]; "
			"keeping the last known value", __func__, setting->what,
			setting->command);
		return 0;
	}
	if (!str_to_uint_strict(response, &value, 10)) {
		upslogx(LOG_WARNING, "%s: unexpected [%s] response [%s]; "
			"keeping the last known value", __func__,
			setting->command, response);
		return 0;
	}

	switch (value) {
	case 1:
		dstate_setinfo(setting->varname, "%s", setting->on_name);
		return 1;
	case 2:
		dstate_setinfo(setting->varname, "%s", setting->off_name);
		return 1;
	default:
		upslogx(LOG_WARNING, "%s: unexpected [%s] response [%s]; "
			"keeping the last known value", __func__,
			setting->command, response);
		return 0;
	}
}

/* Read the auto-reboot setting (whether the device starts again when mains
 * is (re)applied) and publish it as ups.start.auto; 1 = yes, 2 = no. */
static int get_auto_reboot(void) {
	return get_two_state_setting(TWO_STATE_AUTO_REBOOT);
}

/* Read the battery installation date (BRD field 0, formatted as YYYYMMDD).
 * The second field looks like a scheduled replacement date, but its meaning
 * has not been confirmed for this protocol, so it is only reported in debug
 * output instead of being published as a standard variable. */
static void get_battery_date(void) {
	char response[MAX_RESPONSE_LENGTH];
	char date[32], replacement[32];

	if (do_command(POLL, BATTERY_REPLACEMENT_DATE, "", response, sizeof(response)) <= 0)
		return;

	if (get_field(response, 0, date, sizeof(date)))
		dstate_setinfo("battery.date", "%s", date);

	if (get_field(response, 1, replacement, sizeof(replacement)))
		upsdebugx(2, "%s: unmapped second field [%s]",
			__func__, replacement);
}

/* Read the power-on (boot) delay from the device.  This is auxiliary
 * information: a missing or unexpected answer must not mark the whole UPS
 * as stale, it logs a warning and only leaves the previously known value in
 * place. */
static int get_boot_delay(void) {
	char response[MAX_RESPONSE_LENGTH];
	unsigned int delay;

	if (do_command(POLL, BOOT_DELAY, "", response, sizeof(response)) <= 0) {
		upslogx(LOG_WARNING, "%s: could not read the power-on delay "
			"[%s]; keeping the last known value", __func__,
			BOOT_DELAY);
		return 0;
	}

	if (!str_to_uint_strict(response, &delay, 10) || delay > MAX_BOOTDELAY) {
		upslogx(LOG_WARNING, "%s: unexpected [%s] response [%s]; "
			"keeping the last known value", __func__, BOOT_DELAY,
			response);
		return 0;
	}

	dstate_setinfo("outlet.delay.start", "%u", delay);
	return 1;
}

/* Write the power-on (boot) delay to the device; the field is three
 * digits wide, so it is zero padded like the vendor software does. */
static void set_boot_delay(unsigned int delay) {
	char parm[20];

	snprintf(parm, sizeof(parm), "%03u", delay);
	if (!send_set_command(BOOT_DELAY, parm))
		upslogx(LOG_ERR, "%s: device rejected or did not answer "
			"[%s %s]", __func__, BOOT_DELAY, parm);
}

/* Publish the watchdog variables from the setting the driver has: the one
 * the device reported, or the one it accepted.  The device stores a timeout
 * and a restart-alarm flag, not a status, so 'enabled' and 'disabled' are
 * derived from a non-zero timeout. */
static void publish_watchdog(void) {
	dstate_setinfo("ups.watchdog.status", "%s",
		watchdog_seconds ? "enabled" : "disabled");
	dstate_setinfo("ups.watchdog.timeout", "%u", watchdog_seconds);
	dstate_setinfo("ups.watchdog.alarm", "%s",
		watchdog_alarm == 1 ? "enabled" : "disabled");
}

/* Read the watchdog setting (WDG) and publish it as ups.watchdog.status
 * (enabled/disabled), ups.watchdog.timeout (seconds, 0 = disabled) and
 * ups.watchdog.alarm (enabled/disabled; the device stores 1 = enabled and
 * 2 = disabled).  The device reports "<timeout in seconds>;<restart alarm
 * flag>", where a timeout of 0 means that the watchdog is disabled.  This is
 * auxiliary information, so a missing or unexpected answer must not mark the
 * whole UPS as stale: a warning is logged and the setting the driver has is
 * left in place.  Both fields are kept because the device stores them
 * together: whenever one of them changes, the driver has to send the other
 * one along. */
static int get_watchdog(void) {
	char response[MAX_RESPONSE_LENGTH];
	char timeout_value[32], alarm_value[32];
	unsigned int timeout, alarm;

	if (do_command(POLL, WATCHDOG, "", response, sizeof(response)) <= 0) {
		upslogx(LOG_WARNING, "%s: could not read the watchdog "
			"setting [%s]; keeping the last known value",
			__func__, WATCHDOG);
		return 0;
	}

	if (!get_field(response, 0, timeout_value, sizeof(timeout_value))
	    || !str_to_uint_strict(timeout_value, &timeout, 10)
	    || timeout > MAX_WATCHDOG
	    || !get_field(response, 1, alarm_value, sizeof(alarm_value))
	    || !str_to_uint_strict(alarm_value, &alarm, 10)
	    || (alarm != 1 && alarm != 2)) {
		upslogx(LOG_WARNING, "%s: unexpected [%s] response [%s]; "
			"keeping the last known value", __func__, WATCHDOG,
			response);
		return 0;
	}

	watchdog_seconds = timeout;
	watchdog_alarm = alarm;
	watchdog_known = 1;

	publish_watchdog();
	return 1;
}

/* Send the watchdog setting (WDG "<timeout>;<restart alarm flag>"): the
 * device stores the two fields together, so both are always written.  The
 * values it accepts are remembered as the setting the driver has, so that
 * the watchdog variables can be published from them without another round
 * trip and disable_watchdog_on_exit() knows whether it has to act even when
 * reading the value back fails.  Returns 1 when the device accepted the
 * setting. */
static int send_watchdog_setting(unsigned int timeout, unsigned int alarm)
{
	char parm[20];

	if (timeout > MAX_WATCHDOG || (alarm != 1 && alarm != 2)) {
		upslogx(LOG_ERR, "%s: refusing to write the watchdog setting "
			"[%u;%u]", __func__, timeout, alarm);
		return 0;
	}

	snprintf(parm, sizeof(parm), "%u;%u", timeout, alarm);
	if (!send_set_command(WATCHDOG, parm)) {
		upslogx(LOG_ERR, "%s: device rejected or did not answer "
			"[%s %s]", __func__, WATCHDOG, parm);
		return 0;
	}

	watchdog_seconds = timeout;
	watchdog_alarm = alarm;
	watchdog_known = 1;
	publish_watchdog();
	return 1;
}

/* Arm or disarm the watchdog, keeping the timeout (255 seconds when the
 * device never reported one) and the restart-alarm flag that the device
 * uses: arming a watchdog whose restart alarm is disabled must not enable
 * that alarm, and the other way round.  Arming a watchdog that was never
 * read uses a timeout that is assumed, which is worth telling about. */
static int set_watchdog(int enable) {
	if (enable && !watchdog_known)
		upslogx(LOG_WARNING, "%s: arming the watchdog with an "
			"assumed timeout, the device never reported "
			"its [%s] setting", __func__, WATCHDOG);

	return send_watchdog_setting(enable
		? (watchdog_seconds ? watchdog_seconds : DEFAULT_WATCHDOG)
		: 0U, watchdog_alarm);
}

/* The "reset.watchdog" instant command: send the setting the device last
 * reported (or accepted) again, which restarts its timer.  The command
 * takes no value, so the watchdog it kicks is the one the driver is
 * configured with.  A watchdog that was never read is left alone, so that a
 * kick cannot arm it with assumed values, and a disabled watchdog is not
 * kicked either: there is no timer to reset, and pretending otherwise would
 * report a watchdog that does not protect the load. */
static int reset_watchdog(void) {
	if (!watchdog_known) {
		upslogx(LOG_ERR, "%s: the device never reported its [%s] "
			"setting, not resetting the watchdog",
			__func__, WATCHDOG);
		return STAT_INSTCMD_FAILED;
	}

	if (!watchdog_seconds) {
		upslogx(LOG_ERR, "%s: the watchdog is disabled (timeout "
			"0), not resetting it", __func__);
		return STAT_INSTCMD_FAILED;
	}

	if (!send_watchdog_setting(watchdog_seconds, watchdog_alarm))
		return STAT_INSTCMD_FAILED;

	return STAT_INSTCMD_HANDLED;
}

/* A watchdog left armed power cycles the load as soon as the device stops
 * hearing from the driver, so disarm it when the driver goes away on
 * purpose: a signal handled in main.c (SIGTERM, SIGINT, SIGQUIT), the
 * 'driver.exit' instant command and the end of a data dump all run
 * upsdrv_cleanup(), which calls this function before closing the port.  A
 * driver restart (for example during a package upgrade) then does not turn
 * into an unexpected outage.  Only a watchdog the driver believes to be
 * armed is touched, so a device whose watchdog is already disabled is left
 * alone and nothing is sent at all.  A driver that crashes or is killed
 * without unwinding never gets here, so the watchdog keeps covering that
 * case. */
static void disable_watchdog_on_exit(void)
{
	if (!watchdog_seconds)
		return;

	upslogx(LOG_INFO, "%s: disabling the watchdog before exiting",
		__func__);
	set_watchdog(0);
}

/* Read the bypass reason recorded by the device (BPA) and publish it as
 * input.transfer.reason.  This is a live value, so it is refreshed on every
 * update cycle; a missing or unexpected answer must not mark the whole UPS
 * as stale: a warning is logged and the previously published reason is left
 * in place.  The device value is a two-digit code whose meaning is not
 * documented, so it is published as it comes. */
static int get_bypass_reason(void) {
	char response[MAX_RESPONSE_LENGTH];
	char reason[32];

	if (do_command(POLL, BYPASS_REASON, "", response, sizeof(response)) <= 0) {
		upslogx(LOG_WARNING, "%s: could not read the bypass reason "
			"[%s]; keeping the last known value", __func__,
			BYPASS_REASON);
		return 0;
	}

	if (!get_field(response, 0, reason, sizeof(reason))) {
		upslogx(LOG_WARNING, "%s: unexpected [%s] response [%s]; "
			"keeping the last known value", __func__, BYPASS_REASON,
			response);
		return 0;
	}

	dstate_setinfo("input.transfer.reason", "%s", reason);
	return 1;
}

/* Read the ECO (high efficiency) setting and publish it as
 * input.eco.switchable; 1 = high efficiency, 2 = high quality.
 * The vendor software only ever writes this setting, so a device may not
 * answer the poll: in that case a warning is logged and the published value
 * is left alone. */
static int get_eco_mode(void) {
	return get_two_state_setting(TWO_STATE_ECO_MODE);
}

/* Switch between high efficiency (1) and high quality (2) operation.  The
 * published value is only updated when the device accepted the setting. */
static void set_eco_mode(int mode) {
	char parm[20];

	snprintf(parm, sizeof(parm), "%d", mode);
	if (!send_set_command(ECONOMIC_MODE, parm)) {
		upslogx(LOG_ERR, "%s: device rejected or did not answer "
			"[%s %s]", __func__, ECONOMIC_MODE, parm);
		return;
	}
	dstate_setinfo("input.eco.switchable", "%s", mode == 1 ? "ECO" : "normal");
}

/* Read the buzzer status (EDB: 1 = enabled, 2 = disabled) and publish it as
 * ups.beeper.status.  The vendor software only ever writes this setting, so
 * a device may not answer the poll: in that case a warning is logged and the
 * published value is left alone.  Note that "muted" (the one-shot action
 * performed by the beeper.mute command) is a transient state that the device
 * does not report. */
static int get_beeper_status(void) {
	return get_two_state_setting(TWO_STATE_BEEPER);
}

/* Enable (1) or disable (2) the buzzer; this is a device setting, unlike
 * the beeper.mute command, which only silences the current alarm. */
static void set_beeper_status(int enable) {
	char parm[20];

	snprintf(parm, sizeof(parm), "%d", enable ? 1 : 2);
	if (!send_set_command(ENABLE_BUZZER, parm)) {
		upslogx(LOG_ERR, "%s: device rejected or did not answer "
			"[%s %s]", __func__, ENABLE_BUZZER, parm);
		return;
	}
	dstate_setinfo("ups.beeper.status", "%s", enable ? "enabled" : "disabled");
}

/* Read the relay status of each outlet (SOL<n>: 0 = powered, 1 = off)
 * and publish it as outlet.n.status.  Errors are not fatal: this is
 * auxiliary information, so a missing answer must not mark the whole UPS
 * as stale. */
static void get_outlet_status(void) {
	char command[8];
	char response[MAX_RESPONSE_LENGTH];
	char varname[32];
	unsigned int i;

	for (i = 1; i <= (unsigned int)ups.outlets; i++) {
		snprintf(command, sizeof(command), "%s%u", RELAY_STATUS, i);
		if (do_command(POLL, command, "", response, sizeof(response)) <= 0) {
			upsdebugx(2, "%s: no response for [%s]",
				__func__, command);
			continue;
		}
		snprintf(varname, sizeof(varname), "outlet.%u.status", i);
		if (!strcmp(response, "0"))
			dstate_setinfo(varname, "%s", "on");
		else if (!strcmp(response, "1"))
			dstate_setinfo(varname, "%s", "off");
		else
			upsdebugx(2, "%s: unexpected [%s] response [%s]",
				__func__, command, response);
	}
}

/* Send the restart (return) delay to the UPS.  SDR takes whole minutes and
 * this value is already expressed in that unit, so it is sent unchanged.
 * Returns 1 when the device accepted the setting. */
static int set_start_delay(unsigned int minutes) {
	char parm[20];

	snprintf(parm, sizeof(parm), "%u", minutes);
	return send_set_command(TSU_SHUTDOWN_RESTART, parm);
}

/* Parse an unsigned integer bounded to the given inclusive range.  Returns
 * 1 on success and stores the result in *result, 0 otherwise. */
static int parse_uint_range(const char *val, unsigned int minval,
	unsigned int maxval, unsigned int *result)
{
	unsigned int tmp;

	if (!val || !*val)
		return 0;

	if (!str_to_uint_strict(val, &tmp, 10))
		return 0;

	if (tmp < minval || tmp > maxval)
		return 0;

	*result = tmp;
	return 1;
}

/* Parse a transfer voltage setting: it must be numeric and, when the device
 * reported a settable range, fall inside it. */
static int parse_transfer_voltage(const char *val, unsigned int minval,
	unsigned int maxval, unsigned int *voltage)
{
	unsigned int tmp;

	/* sanity bounds, also used when the device reported no range */
	if (!parse_uint_range(val, 1U, 999U, &tmp))
		return 0;

	if (maxval && (tmp < minval || tmp > maxval))
		return 0;

	*voltage = tmp;
	return 1;
}

/* Resolve the delay (in seconds) to use for a shutdown-related command:
 * the value passed by the caller takes precedence over the given fallback
 * (the 'offdelay' setting from ups.conf, or the command's own default).
 * Returns 1 on success and stores the result in *delay, 0 otherwise. */
static int get_shutdown_delay(const char *cmdname, const char *extra,
	unsigned int fallback, unsigned int *delay)
{
	*delay = fallback;

	if (extra && *extra) {
		if (!parse_uint_range(extra, MIN_OFFDELAY, MAX_OFFDELAY,
			delay)) {
			upslogx(LOG_ERR, "instcmd(%s): invalid delay value '%s' "
				"(expected %u..%u seconds)",
				cmdname, extra, MIN_OFFDELAY, MAX_OFFDELAY);
			return 0;
		}
	}

	return 1;
}

/* Parse the value of the reboot commands, "off[:on]": an optional
 * shutdown delay in seconds and, after a colon, an optional restart delay
 * in minutes.  An empty value means both defaults (the command's default
 * shutdown delay and startdelay).  A bare number only sets the shutdown
 * delay.  Anything that
 * is not made of decimal digits and at most one colon is rejected, so ';'
 * cannot start a second command field and '.' or ',' cannot look like
 * fractions of a second.  Returns 1 on success. */
static int parse_reboot_delay(const char *cmdname, const char *extra,
	unsigned int default_off, unsigned int *off, unsigned int *on)
{
	const char *sep;
	char off_value[20];
	size_t off_len;

	*off = default_off;
	*on = startdelay;

	if (!extra || !*extra)
		return 1;

	sep = strchr(extra, ':');
	if (!sep) {
		if (!parse_uint_range(extra, MIN_OFFDELAY, MAX_OFFDELAY, off))
			goto invalid;
		return 1;
	}

	/* the shutdown delay is mandatory when a restart delay is given,
	 * and only one colon is accepted */
	off_len = (size_t)(sep - extra);
	if (off_len == 0 || off_len >= sizeof(off_value)
	    || sep[1] == '\0' || strchr(sep + 1, ':') != NULL)
		goto invalid;

	memcpy(off_value, extra, off_len);
	off_value[off_len] = '\0';
	if (!parse_uint_range(off_value, MIN_OFFDELAY, MAX_OFFDELAY, off)
	    || !parse_uint_range(sep + 1, MIN_STARTDELAY_MINUTES,
		MAX_STARTDELAY_MINUTES, on))
		goto invalid;

	return 1;

invalid:
	upslogx(LOG_ERR, "instcmd(%s): invalid value '%s' (expected "
		"off or off:on, with off in %u..%u seconds and on in "
		"%u..%u minutes)", cmdname, extra, MIN_OFFDELAY, MAX_OFFDELAY,
		MIN_STARTDELAY_MINUTES, MAX_STARTDELAY_MINUTES);
	return 0;
}

/* One shutdown-related instant command: the auto-reboot setting (ARB) it
 * needs, where its default shutdown delay comes from and whether the load is
 * expected to restart right behind the off (timed return) or only when mains
 * is (re)applied. */
struct shutdown_command {
	const char *name;	/* instant command name */
	int autoreboot;		/* ARB value to set: 1 = yes, 0 = no */
	int use_offdelay;	/* 1: default value is the offdelay setting */
	int timed_return;	/* 1: the load restarts right behind the off */
};

static const struct shutdown_command shutdown_commands[] = {
	{ "shutdown.reboot",          1, 1, 1 },
	{ "shutdown.reboot.graceful", 1, 0, 1 },
	{ "shutdown.return",          1, 1, 0 },
	{ "shutdown.stayoff",         0, 1, 0 }
};

/* How an instant command affects the power state, for the upshandler.h
 * logging macros: some change it, some may, and many do not touch it. */
#define INSTCMD_POWERSTATE_NONE   0
#define INSTCMD_POWERSTATE_MAYBE  1
#define INSTCMD_POWERSTATE_CHANGE 2

/* Log an instant command before it is sent, using the classification kept in
 * the command tables below.  This is a macro so that the logging macros still
 * report instcmd() as the function that sent the command. */
#define log_instcmd_powerstate(powerstate, cmdname, extra)	do {	\
	if ((powerstate) == INSTCMD_POWERSTATE_CHANGE)			\
		upslog_INSTCMD_POWERSTATE_CHANGE(cmdname, extra);	\
	else if ((powerstate) == INSTCMD_POWERSTATE_MAYBE)		\
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);	\
	} while (0)

/* Commands that switch every outlet bank ("load.off", "load.on") and, with
 * the same name after "outlet.<n>.", a single bank. */
static const struct relay_command {
	const char *name;	/* instant command name */
	const char *relay;	/* protocol RELAY_* command */
	int powerstate;		/* INSTCMD_POWERSTATE_* value */
} relay_commands[] = {
	{ "load.off", RELAY_OFF, INSTCMD_POWERSTATE_CHANGE },
	{ "load.on",  RELAY_ON,  INSTCMD_POWERSTATE_MAYBE }
};

/* Commands that are a single SET frame with a fixed parameter.  The buzzer
 * entry only silences the alarm that is currently sounding: enabling and
 * disabling the buzzer is a device setting (it survives restarts), so it is
 * exposed as the read/write ups.beeper.status variable instead of a pair of
 * instant commands. */
static const struct simple_command {
	const char *name;	/* instant command name */
	const char *command;	/* protocol SET command */
	const char *parameters;	/* parameter sent with it */
	int powerstate;		/* INSTCMD_POWERSTATE_* value */
} simple_commands[] = {
	{ "beeper.mute",        BUZZER,              "2", INSTCMD_POWERSTATE_NONE },
	{ "clear.fault.record", BYPASS_REASON,       "0", INSTCMD_POWERSTATE_NONE },
	{ "shutdown.stop",      TSU_SHUTDOWN_ACTION, "0", INSTCMD_POWERSTATE_MAYBE },
	{ "test.battery.start", TEST,                "3", INSTCMD_POWERSTATE_MAYBE },
	{ "test.battery.stop",  TEST,                "0", INSTCMD_POWERSTATE_MAYBE }
};

/* Switch one outlet bank: the device addresses the banks with "<n>;1",
 * starting at 1, and the RELAY_* command says what to do with it. */
static int switch_outlet(unsigned int outlet, const char *relay) {
	char parm[20];

	snprintf(parm, sizeof(parm), "%u;1", outlet);
	return send_set_or_fail(relay, parm);
}

/* Send 'relay' to every outlet bank.  This is a multi-frame command: stop
 * at the first frame the device rejects or does not answer, so that a
 * failure is not reported as a success (the banks that were not reached are
 * left as they were). */
static int switch_outlets(const char *relay) {
	unsigned int outlet;

	for (outlet = 1; outlet <= (unsigned int)ups.outlets; outlet++) {
		if (switch_outlet(outlet, relay) != STAT_INSTCMD_HANDLED) {
			return STAT_INSTCMD_FAILED;
		}
	}

	return STAT_INSTCMD_HANDLED;
}

/* The shutdown commands share the same skeleton: set the auto-reboot
 * setting ("ups.start.auto") to what the shutdown needs, set the restart
 * (SDR) delay and schedule the shutdown action (SDA). */
static int shutdown_command(const char *cmdname, const char *extra,
	const struct shutdown_command *spec)
{
	char parm[20];
	unsigned int delay, return_delay;
	unsigned int fallback = spec->use_offdelay
		? offdelay : DEFAULT_GRACEFUL_DELAY;

	if (spec->timed_return) {
		/* "off[:on]", both in the units the device uses */
		if (!parse_reboot_delay(cmdname, extra, fallback,
			&delay, &return_delay)) {
			return STAT_INSTCMD_FAILED;
		}
	} else {
		if (!get_shutdown_delay(cmdname, extra, fallback, &delay)) {
			return STAT_INSTCMD_FAILED;
		}
		/* no timed return: only mains coming back may start the
		 * load again */
		return_delay = 0;
	}

	upslog_INSTCMD_POWERSTATE_CHANGE(cmdname, extra);
	/* set the auto-reboot and return-delay prerequisites before
	 * scheduling the shutdown: if either cannot be confirmed, stop here
	 * and leave the load powered rather than power it off with no
	 * guaranteed way to come back on */
	if (!auto_reboot(spec->autoreboot)) {
		upslogx(LOG_ERR, "%s: could not set or confirm "
			"ups.start.auto, not powering the load off", cmdname);
		return STAT_INSTCMD_FAILED;
	}
	if (!set_start_delay(return_delay)) {
		upslogx(LOG_ERR, "%s: could not set the return delay, "
			"not powering the load off", cmdname);
		return STAT_INSTCMD_FAILED;
	}

	snprintf(parm, sizeof(parm), "%u", delay);
	if (send_set_or_fail(TSU_SHUTDOWN_ACTION, parm)
	    != STAT_INSTCMD_HANDLED) {
		return STAT_INSTCMD_FAILED;
	}

	return STAT_INSTCMD_HANDLED;
}

/* Dispatch an instant command.  The command names, their frames and how they
 * affect the power state all come from the tables above, which are also used
 * to register the commands (see add_instant_commands()), so a command cannot
 * be advertised without being handled or handled without being advertised. */
static int instcmd(const char *cmdname, const char *extra)
{
	char command[32];	/* "outlet.<n>.load.on" and friends */
	unsigned int outlet, cmd_index;

	upsdebug_INSTCMD_STARTING(cmdname, extra);

	/* Switching the outlets is a multi-frame command: switch_outlets()
	 * stops at the first frame the device rejects or does not answer, so
	 * that a failure is not reported as a success. */
	for (cmd_index = 0; cmd_index < SIZEOF_ARRAY(relay_commands);
		cmd_index++) {
		if (strcasecmp(cmdname, relay_commands[cmd_index].name))
			continue;

		log_instcmd_powerstate(relay_commands[cmd_index].powerstate,
			cmdname, extra);
		return switch_outlets(relay_commands[cmd_index].relay);
	}
	/* one set of commands per outlet; these take no value */
	for (outlet = 1; outlet <= (unsigned int)ups.outlets; outlet++) {
		for (cmd_index = 0; cmd_index < SIZEOF_ARRAY(relay_commands);
			cmd_index++) {
			snprintf(command, sizeof(command), "outlet.%u.%s",
				outlet, relay_commands[cmd_index].name);
			if (strcasecmp(cmdname, command))
				continue;

			log_instcmd_powerstate(
				relay_commands[cmd_index].powerstate,
				cmdname, extra);
			return switch_outlet(outlet,
				relay_commands[cmd_index].relay);
		}
	}
	/* commands that are a single SET frame */
	for (cmd_index = 0; cmd_index < SIZEOF_ARRAY(simple_commands);
		cmd_index++) {
		if (strcasecmp(cmdname, simple_commands[cmd_index].name))
			continue;

		log_instcmd_powerstate(
			simple_commands[cmd_index].powerstate, cmdname, extra);
		return send_set_or_fail(simple_commands[cmd_index].command,
			simple_commands[cmd_index].parameters);
	}
	/* kick the watchdog: this takes no value, it sends the setting the
	 * driver has, which restarts the device timer */
	if (!strcasecmp(cmdname, "reset.watchdog"))
		return reset_watchdog();
	for (cmd_index = 0; cmd_index < SIZEOF_ARRAY(shutdown_commands);
		cmd_index++) {
		if (!strcasecmp(cmdname, shutdown_commands[cmd_index].name)) {
			return shutdown_command(cmdname, extra,
				&shutdown_commands[cmd_index]);
		}
	}

	upslog_INSTCMD_UNKNOWN(cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

/* Register every instant command the driver supports.  The names come from
 * the same tables instcmd() dispatches on, so the two cannot drift apart. */
static void add_instant_commands(void)
{
	char command[32];	/* "outlet.<n>.load.on" and friends */
	unsigned int outlet, cmd_index;

	for (cmd_index = 0; cmd_index < SIZEOF_ARRAY(simple_commands);
		cmd_index++) {
		dstate_addcmd(simple_commands[cmd_index].name);
	}
	dstate_addcmd("reset.watchdog");
	for (cmd_index = 0; cmd_index < SIZEOF_ARRAY(shutdown_commands);
		cmd_index++) {
		dstate_addcmd(shutdown_commands[cmd_index].name);
	}

	/* the relay commands switch outlets (load.off and load.on all of
	 * them, outlet.n.load.* a single bank), so they are only registered
	 * when the device reports any */
	if (!ups.outlets)
		return;

	for (cmd_index = 0; cmd_index < SIZEOF_ARRAY(relay_commands);
		cmd_index++) {
		dstate_addcmd(relay_commands[cmd_index].name);
	}
	for (outlet = 1; outlet <= (unsigned int)ups.outlets; outlet++) {
		for (cmd_index = 0; cmd_index < SIZEOF_ARRAY(relay_commands);
			cmd_index++) {
			snprintf(command, sizeof(command), "outlet.%u.%s",
				outlet, relay_commands[cmd_index].name);
			dstate_addcmd(command);
		}
	}
}

static int setvar(const char *varname, const char *val)
{
	unsigned int i, voltage;

	upsdebug_SET_STARTING(varname, val);

	if (!strcasecmp(varname, "ups.id")) {
		set_identification(val);
		get_identification();
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "outlet.delay.start")) {
		unsigned int delay;

		if (!parse_uint_range(val, 0U, MAX_BOOTDELAY, &delay)) {
			upslogx(LOG_ERR, "setvar(%s): invalid power-on delay "
				"value '%s' (expected 0..%u seconds)",
				varname, val, MAX_BOOTDELAY);
			return STAT_SET_FAILED;
		}
		set_boot_delay(delay);
		/* read the value back, so that the published variable
		 * reflects what the device actually accepted */
		get_boot_delay();
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "ups.start.auto")) {
		int enable;

		if (!strcasecmp(val, "yes"))
			enable = 1;
		else if (!strcasecmp(val, "no"))
			enable = 0;
		else {
			upslogx(LOG_ERR, "setvar(%s): invalid value '%s' "
				"(expected yes or no)", varname, val);
			return STAT_SET_FAILED;
		}
		/* auto_reboot() logs the reason on failure: a device that
		 * does not report the setting cannot confirm it */
		if (!auto_reboot(enable))
			return STAT_SET_FAILED;
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "ups.beeper.status")) {
		if (!strcasecmp(val, "enabled"))
			set_beeper_status(1);
		else if (!strcasecmp(val, "disabled"))
			set_beeper_status(0);
		else {
			upslogx(LOG_ERR, "setvar(%s): invalid value '%s' "
				"(expected enabled or disabled)", varname, val);
			return STAT_SET_FAILED;
		}
		/* read the value back, so that the published variable
		 * reflects what the device actually accepted */
		get_beeper_status();
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "ups.watchdog.status")) {
		if (!strcasecmp(val, "enabled")) {
			if (!set_watchdog(1))
				return STAT_SET_FAILED;
		} else if (!strcasecmp(val, "disabled")) {
			if (!set_watchdog(0))
				return STAT_SET_FAILED;
		} else {
			upslogx(LOG_ERR, "setvar(%s): invalid value '%s' "
				"(expected enabled or disabled)", varname, val);
			return STAT_SET_FAILED;
		}
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "ups.watchdog.timeout")) {
		unsigned int timeout;

		/* 0 disables the watchdog; the restart-alarm flag the
		 * device stores with it is kept as it is */
		if (!parse_uint_range(val, 0U, MAX_WATCHDOG, &timeout)) {
			upslogx(LOG_ERR, "setvar(%s): invalid timeout value "
				"'%s' (expected 0..%u seconds, 0 disables)",
				varname, val, MAX_WATCHDOG);
			return STAT_SET_FAILED;
		}
		if (!send_watchdog_setting(timeout, watchdog_alarm))
			return STAT_SET_FAILED;
		/* read the value back, so that the published variables
		 * reflect what the device actually accepted */
		get_watchdog();
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "ups.watchdog.alarm")) {
		unsigned int alarm;

		if (!strcasecmp(val, "enabled"))
			alarm = 1;
		else if (!strcasecmp(val, "disabled"))
			alarm = 2;
		else {
			upslogx(LOG_ERR, "setvar(%s): invalid value '%s' "
				"(expected enabled or disabled)", varname, val);
			return STAT_SET_FAILED;
		}
		if (!send_watchdog_setting(watchdog_seconds, alarm))
			return STAT_SET_FAILED;
		/* read the value back, so that the published variables
		 * reflect what the device actually accepted */
		get_watchdog();
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "input.eco.switchable")) {
		if (!strcasecmp(val, "ECO"))
			set_eco_mode(1);
		else if (!strcasecmp(val, "normal"))
			set_eco_mode(2);
		else {
			upslogx(LOG_ERR, "setvar(%s): invalid value '%s' "
				"(expected ECO or normal)", varname, val);
			return STAT_SET_FAILED;
		}
		get_eco_mode();
		return STAT_SET_HANDLED;
	}
	for (i = 0; i < TRANSFER_VOLTAGE_COUNT; i++) {
		const struct transfer_voltage_setting *setting =
			&transfer_voltage_settings[i];

		if (strcasecmp(varname, setting->varname))
			continue;
		if (!parse_transfer_voltage(val, min_transfer[i],
			max_transfer[i], &voltage)) {
			upslogx(LOG_ERR, "setvar(%s): invalid value '%s' "
				"(expected %u..%u)", varname, val,
				min_transfer[i], max_transfer[i]);
			return STAT_SET_FAILED;
		}
		set_transfer_voltage((enum transfer_voltage_id)i, (int)voltage);
		get_transfer_voltage((enum transfer_voltage_id)i);
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "input.sensitivity")) {
		set_sensitivity(val);
		get_sensitivity();
		return STAT_SET_HANDLED;
	}

	upslog_SET_UNKNOWN(varname, val);
	return STAT_SET_UNKNOWN;
}

static int init_comm(void)
{
	size_t i, bit;
	char response[MAX_RESPONSE_LENGTH];

	ups.commands_available = 0;
	/* Repeat enumerate command 2x, firmware bug on some units garbles 1st response */
	if (do_command(POLL, AVAILABLE, "", response, sizeof(response)) <= 0){
		upslogx(LOG_NOTICE, "init_comm: Initial response malformed, retrying in 300ms");
		usleep(3E5);
	}
	if (do_command(POLL, AVAILABLE, "", response, sizeof(response)) <= 0)
		return 0;
	i = strlen(response);
	if (i > sizeof(ups.commands_available) * CHAR_BIT) {
		upslogx(LOG_WARNING, "%s: [%s] response has %" PRIuSIZE
			" bits, only the last %" PRIuSIZE " are used",
			__func__, AVAILABLE, i,
			sizeof(ups.commands_available) * CHAR_BIT);
		i = sizeof(ups.commands_available) * CHAR_BIT;
	}
	for (bit = 0; bit < i; bit++)
		if (response[i - bit - 1] == '1')
			ups.commands_available |= (1UL << bit);

	if (do_command(POLL, MANUFACTURER, "", response, sizeof(response)) <= 0)
		return 0;
	if (strcmp(response, "Tripp Lite"))
		return 0;

	return 1;
}

void upsdrv_initinfo(void)
{
	char response[MAX_RESPONSE_LENGTH];
	char buf[32];	/* "outlet.<n>.load.on" and friends */
	char rangevar[48];	/* "<varname>.min" and "<varname>.max" */
	char value[32];
	unsigned int i;

	if (!init_comm())
		fatalx(EXIT_FAILURE, "Unable to detect Tripp Lite SmartOnline UPS on port %s\n",
			device_path);
	for (i = 0; i < TRANSFER_VOLTAGE_COUNT; i++)
		min_transfer[i] = max_transfer[i] = 0;

	/* get all the read-only fields here */
	if (do_command(POLL, MANUFACTURER, "", response, sizeof(response)) > 0)
		dstate_setinfo("ups.mfr", "%s", response);
	if (do_command(POLL, MODEL, "", response, sizeof(response)) > 0)
		dstate_setinfo("ups.model", "%s", response);
	if (do_command(POLL, VERSION_CMD, "", response, sizeof(response)) > 0)
		dstate_setinfo("ups.firmware", "%s", response);
	if (do_command(POLL, RATINGS, "", response, sizeof(response)) > 0) {
		if (get_field(response, 0, value, sizeof(value)))
			dstate_setinfo("input.voltage.nominal", "%d",
				atoi(value));
		if (get_field(response, 2, value, sizeof(value)))
			dstate_setinfo("output.voltage.nominal", "%d",
				atoi(value));
		if (get_field(response, 14, value, sizeof(value)))
			dstate_setinfo("battery.voltage.nominal", "%d",
				atoi(value));
		/* the transfer voltage ranges: field 10 is the lowest settable
		 * low transfer, 9 the highest settable low transfer, 12 the
		 * lowest settable high transfer and 11 the highest settable
		 * high transfer */
		set_transfer_range(response, 10,
			&min_transfer[TRANSFER_VOLTAGE_LOW]);
		set_transfer_range(response, 9,
			&max_transfer[TRANSFER_VOLTAGE_LOW]);
		set_transfer_range(response, 12,
			&min_transfer[TRANSFER_VOLTAGE_HIGH]);
		set_transfer_range(response, 11,
			&max_transfer[TRANSFER_VOLTAGE_HIGH]);
	}
	if (do_command(POLL, OUTLET_RELAYS, "", response, sizeof(response)) > 0) {
		int outlet_count = atoi(response);

		if (outlet_count > 0 && outlet_count <= (int)MAX_OUTLETS)
			ups.outlets = outlet_count;
		else if (outlet_count != 0)
			upslogx(LOG_WARNING, "%s: ignoring implausible "
				"outlet count [%s]", __func__, response);
	}
	if (ups.outlets > 0) {
		dstate_setinfo("outlet.count", "%d", ups.outlets);
		for (i = 1; i <= (unsigned int)ups.outlets; i++) {
			snprintf(buf, sizeof(buf), "outlet.%u.switchable", i);
			dstate_setinfo(buf, "%s", "yes");
		}
	}
	get_battery_date();
	/* the device settings below (get/set) are published, and made
	 * settable, only when the device actually reports them: an unreadable
	 * setting must not appear with an assumed value, and each get_*()
	 * already warns when it cannot read one */
	if (get_boot_delay()) {
		dstate_setflags("outlet.delay.start",
			ST_FLAG_RW | ST_FLAG_STRING);
		dstate_setaux("outlet.delay.start", 3);
	}
	if (get_auto_reboot()) {
		dstate_setflags("ups.start.auto", ST_FLAG_RW | ST_FLAG_STRING);
		dstate_setaux("ups.start.auto", 3);
	}
	if (get_watchdog()) {
		dstate_setflags("ups.watchdog.status",
			ST_FLAG_RW | ST_FLAG_STRING);
		dstate_setaux("ups.watchdog.status", 8);
		dstate_setflags("ups.watchdog.timeout",
			ST_FLAG_RW | ST_FLAG_STRING);
		dstate_setaux("ups.watchdog.timeout", 3);
		dstate_setflags("ups.watchdog.alarm",
			ST_FLAG_RW | ST_FLAG_STRING);
		dstate_setaux("ups.watchdog.alarm", 8);
	}
	if (get_eco_mode()) {
		dstate_setflags("input.eco.switchable",
			ST_FLAG_RW | ST_FLAG_STRING);
		dstate_setaux("input.eco.switchable", 6);
	}
	if (get_beeper_status()) {
		dstate_setflags("ups.beeper.status",
			ST_FLAG_RW | ST_FLAG_STRING);
		dstate_setaux("ups.beeper.status", 8);
	}
	if (get_identification()) {
		dstate_setflags("ups.id", ST_FLAG_RW | ST_FLAG_STRING);
		dstate_setaux("ups.id", 100);
	}
	for (i = 0; i < TRANSFER_VOLTAGE_COUNT; i++) {
		const struct transfer_voltage_setting *setting =
			&transfer_voltage_settings[i];
		unsigned int voltage;

		if (!get_transfer_voltage((enum transfer_voltage_id)i))
			continue;
		if (!max_transfer[i])
			continue;
		dstate_setflags(setting->varname, ST_FLAG_RW);
		for (voltage = min_transfer[i];
			voltage <= max_transfer[i]; voltage++) {
			dstate_addenum(setting->varname, "%u", voltage);
		}
	}
	/* report the settable ranges, so that the values accepted by setvar()
	 * are visible to clients */
	for (i = 0; i < TRANSFER_VOLTAGE_COUNT; i++) {
		const struct transfer_voltage_setting *setting =
			&transfer_voltage_settings[i];

		if (!max_transfer[i])
			continue;
		snprintf(rangevar, sizeof(rangevar), "%s.min", setting->varname);
		dstate_setinfo(rangevar, "%u", min_transfer[i]);
		snprintf(rangevar, sizeof(rangevar), "%s.max", setting->varname);
		dstate_setinfo(rangevar, "%u", max_transfer[i]);
	}
	if (get_sensitivity()) {
		dstate_setflags("input.sensitivity", ST_FLAG_RW);
		for (i = 0; i < SIZEOF_ARRAY(sensitivity); i++)
			dstate_addenum("input.sensitivity", "%s",
				sensitivity[i].name);
	}
	/* register every command, taking the names from the same tables that
	 * instcmd() dispatches on (load.off, load.on and the outlet commands
	 * are only added when the device reports outlet banks) */
	add_instant_commands();

	/* add all the variables that change regularly */
	upsdrv_updateinfo();

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;

	printf("Detected %s %s on %s\n",
		dstate_getinfo("ups.mfr"),
		dstate_getinfo("ups.model"),
		device_path);
}

void upsdrv_updateinfo(void)
{
	char response[MAX_RESPONSE_LENGTH];
	char value[32];
	char *ptr2;
	int i;
	int flags;
	int contacts_set;
	int low_battery;

	status_init();
	if (do_command(POLL, STATUS_OUTPUT, "", response, sizeof(response)) <= 0) {
		dstate_datastale();
		return;
	}
	/* require output status field to exist */
	if (!get_field(response, 0, value, sizeof(value))) {
		dstate_datastale();
		return;
	}
	switch (atoi(value)) {
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
	if (get_field(response, 6, value, sizeof(value)))
		dstate_setinfo("ups.load", "%d", atoi(value));
	if (get_field(response, 3, value, sizeof(value)))
		dstate_setinfo("output.voltage", "%03.1f",
			(double) (atoi(value)) / 10.0);
	if (get_field(response, 1, value, sizeof(value)))
		dstate_setinfo("output.frequency", "%03.1f",
			(double) (atoi(value)) / 10.0);
	if (get_field(response, 4, value, sizeof(value)))
		dstate_setinfo("output.current", "%03.1f",
			(double) (atoi(value)) / 10.0);

	low_battery = 0;
	if (do_command(POLL, STATUS_BATTERY, "", response, sizeof(response)) <= 0) {
		dstate_datastale();
		return;
	}
	if (get_field(response, 0, value, sizeof(value))
	    && atoi(value) == 2)
		status_set("RB");
	if (get_field(response, 1, value, sizeof(value))
	    && atoi(value))
		low_battery = 1;
	if (get_field(response, 8, value, sizeof(value)))
		dstate_setinfo("battery.temperature", "%d", atoi(value));
	if (get_field(response, 9, value, sizeof(value))) {
		dstate_setinfo("battery.charge", "%d", atoi(value));
		ptr2 = getval("lowbatt");
		if (ptr2
		 && atoi(ptr2) > 0
		 && atoi(ptr2) <= 99
		 && atoi(value) <= atoi(ptr2)
		) {
			low_battery = 1;
		}
	}
	if (get_field(response, 6, value, sizeof(value)))
		dstate_setinfo("battery.voltage", "%03.1f",
			(double) (atoi(value)) / 10.0);
	if (get_field(response, 7, value, sizeof(value)))
		dstate_setinfo("battery.current", "%03.1f",
			(double) (atoi(value)) / 10.0);
	if (low_battery)
		status_set("LB");

	if (do_command(POLL, STATUS_ALARM, "", response, sizeof(response)) <= 0) {
		dstate_datastale();
		return;
	}
	if (get_field(response, 3, value, sizeof(value))
	    && atoi(value))
		status_set("OVER");

	if (do_command(POLL, STATUS_INPUT, "", response, sizeof(response)) > 0) {
		if (get_field(response, 2, value, sizeof(value)))
			dstate_setinfo("input.voltage", "%03.1f",
				(double) (atoi(value)) / 10.0);
		if (get_field(response, 1, value, sizeof(value)))
			dstate_setinfo("input.frequency", "%03.1f",
				(double) (atoi(value)) / 10.0);
	}

	if (do_command(POLL, TEST_RESULT, "", response, sizeof(response)) > 0) {
		int	r;
		size_t	trsize;

		r = atoi(response);
		trsize = SIZEOF_ARRAY(test_result_names);

		if ((r < 0) || (r >= (int) trsize))
			r = 0;

		dstate_setinfo("ups.test.result", "%s", test_result_names[r]);
	}

	if (do_command(POLL, ENVIRONMENT_INFORMATION, "", response, sizeof(response)) > 0) {
		if (get_field(response, 0, value, sizeof(value)))
			dstate_setinfo("ambient.temperature", "%d", atoi(value));
		if (get_field(response, 1, value, sizeof(value)))
			dstate_setinfo("ambient.humidity", "%d", atoi(value));
		flags = 0;
		contacts_set = 0;
		for (i = 0; i < 4; i++) {
			if (get_field(response, 2 + i, value, sizeof(value))) {
				contacts_set = 1;
				if (value[0] == '1')
					flags |= 1 << i;
			}
		}
		if (contacts_set)
			dstate_setinfo("ups.contacts", "%02X", (unsigned int)flags);
	}

	/* the bypass (transfer) reason is a live value: refresh it on every
	 * pass, without marking the whole UPS stale when the device does not
	 * report it */
	get_bypass_reason();

	get_outlet_status();

	/* if we are here, status is valid */
	status_commit();
	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	char	parm[20];

	if (!init_comm())
		printf("Status failed.  Assuming it's on battery and trying a shutdown anyway.\n");
	/* Turn the device auto-reboot setting (ARB) on, unconditionally:
	 * it asks the device to start the load again once mains is
	 * (re)applied, so that every outlet is powered again after the
	 * outage that caused the shutdown.  In case the power is on, this
	 * also tells the device to start again; if it is off, it has no
	 * effect.  A device that does not confirm the setting is a device
	 * the driver cannot tell anything about, so 'shutdown_mode' decides
	 * whether the shutdown is scheduled anyway (best effort) or not
	 * (fail safe). */
	if (!auto_reboot(1)) {
		upslogx(LOG_ERR, "%s: could not set or confirm "
			"ups.start.auto", __func__);
		if (shutdown_abort_on_error) {
			upslogx(LOG_ERR, "%s: shutdown_mode=abort, not "
				"powering the load off", __func__);
			return;
		}
	}
	/* clear the return delay (SDR 0), so that the load comes back only
	 * when mains is (re)applied, as asked by the auto-reboot setting
	 * above, and not on a timer left over from an earlier command */
	if (!set_start_delay(0)) {
		upslogx(LOG_ERR, "%s: could not clear the return delay",
			__func__);
		if (shutdown_abort_on_error) {
			upslogx(LOG_ERR, "%s: shutdown_mode=abort, not "
				"powering the load off", __func__);
			return;
		}
	}
	/* delay before shutdown, in seconds, from 'offdelay' */
	snprintf(parm, sizeof(parm), "%u", offdelay);
	if (!send_set_command(TSU_SHUTDOWN_ACTION, parm))
		upslogx(LOG_ERR, "%s: could not schedule the shutdown", __func__);
}

void upsdrv_help(void)
{
}

/* optionally tweak prognames[] entries */
void upsdrv_tweak_prognames(void)
{
}

/* list flags and values that you want to receive via -x or ups.conf */
void upsdrv_makevartable(void)
{
	char msg[256];

	addvar(VAR_VALUE, "lowbatt", "Set low battery level, in percent");
	addvar(VAR_VALUE, "command_delay",
		"Minimum interval between commands, in milliseconds "
		"(default: 0 = no wait)");

	snprintf(msg, sizeof msg, "Set shutdown delay, in seconds (default=%u, range %u..%u).",
		DEFAULT_OFFDELAY, MIN_OFFDELAY, MAX_OFFDELAY);
	addvar(VAR_VALUE, "offdelay", msg);

	/* The return delay is configured in minutes because that is the unit
	 * used by the device (SDR). */
	snprintf(msg, sizeof msg, "Set return (restart) delay, in minutes "
		"(default=%u, range %u..%u).",
		DEFAULT_STARTDELAY_MINUTES, MIN_STARTDELAY_MINUTES,
		MAX_STARTDELAY_MINUTES);
	addvar(VAR_VALUE, "startdelay", msg);

	/* The driver shutdown handler ("shutdown.default", run by
	 * upsdrvctl shutdown) always turns the device auto-reboot setting
	 * (ARB) on before it schedules the shutdown, so that the load is
	 * powered again when mains is (re)applied.  The device stores the
	 * setting, so it also affects "ups.start.auto"; the shutdown
	 * commands (shutdown.return and friends) set it for their own
	 * needs.  The 'shutdown_mode' setting decides how the handler
	 * reacts to a failed operation: keep sending the remaining ones
	 * (best effort) or stop at the first failure (fail safe).  The
	 * operations are the auto-reboot setting (ARB) and the shutdown
	 * action (SDA). */
	snprintf(msg, sizeof msg, "How the shutdown handler reacts to a "
		"failed operation: continue = keep sending the remaining "
		"ones (best effort), abort = stop at the first failure "
		"(fail safe, default=continue).");
	addvar(VAR_VALUE, "shutdown_mode", msg);
}

void upsdrv_initups(void)
{
	const char *val;

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);

	/* Initialize command_delay from configuration.  The value is in
	 * milliseconds: 0 disables the pacing (commands follow each other as
	 * soon as the previous answer is read), a positive value is the
	 * minimum interval to keep between two commands. */
	command_delay = DEFAULT_COMMAND_DELAY;
	val = getval("command_delay");
	if (val && *val) {
		unsigned int temp;

		if (!str_to_uint_strict(val, &temp, 10)
		    || temp > MAX_COMMAND_DELAY) {
			fatalx(EXIT_FAILURE, "Invalid command_delay parameter: %s "
				"(expected 0 (no wait) or 1..%u milliseconds)",
				val, MAX_COMMAND_DELAY);
		}
		command_delay = temp;
	} else {
		upsdebugx(2, "Using default command_delay of %u milliseconds",
			command_delay);
	}

	if (command_delay)
		upsdebugx(2, "Setting command_delay to %u milliseconds",
			command_delay);
	else
		upsdebugx(2, "command_delay is 0: no wait between commands");

	/* Initialize offdelay from configuration */
	val = getval("offdelay");
	if (val && *val) {
		unsigned int temp;
		if (!str_to_uint_strict(val, &temp, 10) ||
		    temp < MIN_OFFDELAY || temp > MAX_OFFDELAY) {
			fatalx(EXIT_FAILURE, "Invalid offdelay parameter: %s "
				"(expected %u..%u seconds)",
				val, MIN_OFFDELAY, MAX_OFFDELAY);
		}
		offdelay = temp;
		upsdebugx(2, "Setting offdelay to %u seconds", offdelay);
	} else {
		upsdebugx(2, "Using default offdelay of %u seconds", offdelay);
	}

	/* Initialize startdelay from configuration, in whole minutes (the
	 * unit used by the device). */
	val = getval("startdelay");
	if (val && *val) {
		unsigned int temp;
		if (!str_to_uint_strict(val, &temp, 10)
		    || temp > MAX_STARTDELAY_MINUTES) {
			fatalx(EXIT_FAILURE, "Invalid startdelay parameter: %s "
				"(expected %u..%u minutes)",
				val, MIN_STARTDELAY_MINUTES,
				MAX_STARTDELAY_MINUTES);
		}
		startdelay = temp;
		upsdebugx(2, "Setting startdelay to %u minutes", startdelay);
	} else {
		upsdebugx(2, "Using default startdelay of %u minutes",
			startdelay);
	}

	/* Initialize shutdown_mode from configuration: whether a failure of
	 * an operation stops the shutdown handler there (fail safe) or not
	 * (best effort, the default). */
	shutdown_abort_on_error = 0;
	val = getval("shutdown_mode");
	if (val && *val) {
		if (!strcasecmp(val, "continue")) {
			shutdown_abort_on_error = 0;
		} else if (!strcasecmp(val, "abort")) {
			shutdown_abort_on_error = 1;
		} else {
			fatalx(EXIT_FAILURE, "Invalid shutdown_mode "
				"parameter: %s (expected continue or abort)",
				val);
		}
		upsdebugx(2, "Setting shutdown_mode to %s",
			shutdown_abort_on_error ? "abort" : "continue");
	} else {
		upsdebugx(2, "Using default shutdown_mode of continue");
	}
}

void upsdrv_cleanup(void)
{
	disable_watchdog_on_exit();
	ser_close(upsfd, device_path);
}
