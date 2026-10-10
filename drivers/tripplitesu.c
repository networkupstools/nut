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
	offdelay
	startdelay

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
	ups.delay.start (RW)
	ups.firmware
	ups.id (RW) (1)
	ups.load
	ups.mfr
	ups.model
	ups.status
	ups.start.auto (RW)
	ups.test.result
	ups.watchdog.status (RW)
	ups.contacts (1)

    The following instant commands are supported:
	load.off
	load.on
	outlet.n.load.off (one per outlet)
	outlet.n.load.on (one per outlet)
	beeper.mute
	clear.fault.record
	shutdown.reboot
	shutdown.reboot.graceful
	shutdown.return
	shutdown.stayoff
	shutdown.stop
	test.battery.start
	test.battery.stop

    The shutdown-related commands above accept an optional delay, in seconds,
    passed as the "value" argument of upscmd; it takes precedence over the
    "offdelay" setting from ups.conf, which in turn takes precedence over
    the built-in default.  The "load.off", "load.on" and "outlet.n.load.*"
    commands do not take a value: they only switch the outlets.

    "shutdown.return" and "shutdown.stayoff" clear the device return delay
    (SDR), so the load comes back only when mains is (re)applied or not at
    all, while the two reboot commands ask for the load to come back right
    behind the shutdown (just behind it, see reboot_start_delay()).  The
    "startdelay" setting is used by the driver shutdown handler only.

    Because that is what the commands mean on this hardware, the shutdown
    commands change the device auto-reboot setting ("ups.start.auto", ARB);
    the device stores it, so the change outlives the shutdown that needs it,
    and the driver logs it (restore it with upsrw if needed).  The buzzer is
    a device setting as well: enabling/disabling it is done through the
    read/write "ups.beeper.status" variable, while the "beeper.mute" command
    only silences the alarm that is currently sounding.

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

/* Delay, in seconds, before the load is returned (restarted) after a
 * shutdown.  The device command (SDR) takes minutes, so values are rounded
 * up when sent; the default of one minute matches the value this driver
 * used before it became configurable.  Unlike the shutdown action (SDA),
 * a value of 0 is meaningful here and means "return without delay", so it
 * is accepted.  The ups.conf parameter 'startdelay' is expressed in
 * minutes (the device unit), while the ups.delay.start variable uses
 * seconds, as usual for delays in NUT. */
#define DEFAULT_STARTDELAY 60U
#define MIN_STARTDELAY 0U
#define MAX_STARTDELAY 3600U
#define MAX_STARTDELAY_MINUTES (MAX_STARTDELAY / 60U)

/* Power-on (boot) delay: how many seconds the device waits before it starts
 * delivering voltage to its outputs (inverter on, load relays closed) when
 * it is powered up or asked to switch them on.  This is a device setting,
 * not an action, so it is only exposed as a read/write variable (no instant
 * command); 0 means "no delay" and the device field is three digits wide. */
#define MAX_BOOTDELAY 999U

/* Watchdog setting (WDG): the device stores a timeout in seconds (up to
 * three digits, 0 disables it) plus a flag selecting whether the restart
 * alarm is enabled (1) or disabled (2).  The timeout and the flag are
 * preserved when the watchdog is (dis)armed from NUT. */
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
 * historical behaviour), a positive value is the interval itself, and -1
 * asks the driver to work it out by itself (see note_command_timeout()).
 * The vendor software paces its own commands by 1.5 to 3 seconds. */
#define COMMAND_DELAY_AUTO (-1)
#define DEFAULT_COMMAND_DELAY 0
#define DEFAULT_AUTO_COMMAND_DELAY 1000U
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

static long command_delay_conf = DEFAULT_COMMAND_DELAY; /* 'command_delay' as set in ups.conf */
static unsigned int command_delay = 0; /* minimum interval kept between commands, in milliseconds (0 = no waiting) */
static int command_delay_auto = 0; /* 1 when 'command_delay' asked for automatic pacing (-1) */
static int command_sent = 0; /* 1 once a command has been sent (see pace_before_command()) */
static struct timeval command_sent_at; /* when the last command was sent */
static unsigned int offdelay = DEFAULT_OFFDELAY; /* delay in seconds before shutdown */
static int offdelay_from_conf = 0; /* set if 'offdelay' was provided in ups.conf */
static unsigned int startdelay = DEFAULT_STARTDELAY; /* delay in seconds before return */
static unsigned int watchdog_seconds = 0; /* watchdog timeout last reported by the device */
static unsigned int watchdog_alarm = 2; /* watchdog restart-alarm flag last reported */
static int watchdog_known = 0; /* set once the device reported its watchdog setting */
static unsigned int min_low_transfer, max_low_transfer; /* reported transfer voltage ranges */
static unsigned int min_high_transfer, max_high_transfer;

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

/* The device did not answer a command in time.  In automatic mode the
 * interval is raised, once: a device that is slow to digest a command does
 * not get faster again for the rest of the run (command_delay is 0 until
 * this function raises it, so this happens only once).  An explicitly
 * configured value - including 0 - is never touched.  Only POLL commands
 * count: a SET command that goes unanswered may have been carried out all
 * the same, so it says nothing about how fast the device can be driven. */
static void note_command_timeout(char type)
{
	if (type != POLL || !command_delay_auto || command_delay)
		return;

	command_delay = DEFAULT_AUTO_COMMAND_DELAY;
	upslogx(LOG_WARNING, "Read timeout: waiting %u ms between commands "
		"from now on (command_delay=auto); adjust 'command_delay' in "
		"ups.conf to change this", command_delay);
}


static ssize_t do_command(char type, const char *command, const char *parameters, char *response)
{
	char	buffer[SMALLBUF];
	size_t	count;
	ssize_t	ret;

	/* let the device get ready for the next command, then drop whatever
	 * it may have sent in the meantime */
	pace_before_command();
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

	note_command_sent();
	upsdebugx(3, "do_command: %" PRIiSIZE " bytes sent [%s] -> OK", ret, buffer);

	ret = ser_get_buf_len(upsfd, (unsigned char *)buffer, 4, 3, 0);
	if (ret < 0) {
		upsdebug_with_errno(3, "do_command: read");
		return -1;
	}
	if (ret == 0) {
		upsdebugx(3, "do_command: read -> TIMEOUT");
		note_command_timeout(type);
		return -1;
	}

	buffer[ret] = '\0';
	upsdebugx(3, "do_command: %" PRIiSIZE " byted read [%s]", ret, buffer);

	if (!strcmp(buffer, "~00D")) {
		int	c;

		ret = ser_get_buf_len(upsfd, (unsigned char *)buffer, 3, 3, 0);
		if (ret < 0) {
			upsdebug_with_errno(3, "do_command: read");
			return -1;
		}
		if (ret == 0) {
			upsdebugx(3, "do_command: read -> TIMEOUT");
			note_command_timeout(type);
			return -1;
		}

		buffer[ret] = '\0';
		upsdebugx(3, "do_command: %" PRIiSIZE " bytes read [%s]", ret, buffer);

		c = atoi(buffer);
		if (c < 0) {
			upsdebugx(3, "do_command: response not expected to be a negative count!");
			return -1;
		}

		count = (size_t)c;
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
			note_command_timeout(type);
			return -1;
		}

		response[ret] = '\0';
		upsdebugx(3, "do_command: %" PRIiSIZE " bytes read [%s]", ret, response);

		/* Tripp Lite pads their string responses with spaces.
		 * I don't like that, so I remove them.  This is safe to
		 * do with all responses for this protocol, so I just
		 * do that here. */
		str_rtrim(response, ' ');

		return ret;
	}

	if (!strcmp(buffer, "~00A")) {
		return 0;
	}

	return -1;
}

/* Send a SET command and tell whether the device accepted it: it answers
 * with a plain ACK (~00A) for accepted operations and with a rejection
 * (~00R) for ones it does not support, while a timeout or a bad answer
 * makes do_command() return a negative value in all those cases. */
static int send_set_command(const char *command, const char *parameters) {
	return do_command(SET, command, parameters, NULL) >= 0;
}

/* Same, but log the failure and return the matching instant command status. */
static int send_set_or_fail(const char *command, const char *parameters) {
	if (send_set_command(command, parameters))
		return STAT_INSTCMD_HANDLED;

	upslogx(LOG_ERR, "device rejected or did not answer [%s %s]",
		command, NUT_STRARG(parameters));
	return STAT_INSTCMD_FAILED;
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
		if (!send_set_command(IDENTIFICATION, response))
			upslogx(LOG_ERR, "%s: device rejected or did not "
				"answer [%s %s]", __func__, IDENTIFICATION,
				response);
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
	if (!send_set_command(TRANSFER_VOLTAGE, response))
		upslogx(LOG_ERR, "%s: device rejected or did not answer "
			"[%s %s]", __func__, TRANSFER_VOLTAGE, response);
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
	if (!send_set_command(TRANSFER_VOLTAGE, response))
		upslogx(LOG_ERR, "%s: device rejected or did not answer "
			"[%s %s]", __func__, TRANSFER_VOLTAGE, response);
}

static int get_sensitivity(void) {
	char response[MAX_RESPONSE_LENGTH];
	unsigned int i;

	if (do_command(POLL, VOLTAGE_SENSITIVITY, "", response) <= 0)
		return 0;
	for (i = 0; i < SIZEOF_ARRAY(sensitivity); i++) {
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
	char *ptr;
	int mode, oldmode;

	if (enable)
		mode = 1;
	else
		mode = 2;
	if (do_command(POLL, AUTO_REBOOT, "", response) <= 0) {
		/* the current setting is unknown (some devices do not report
		 * it), so it cannot be confirmed: report a failure so the
		 * caller aborts instead of powering the load off on a guess */
		upslogx(LOG_ERR, "%s: no answer for [%s], cannot set or "
			"confirm ups.start.auto", __func__, AUTO_REBOOT);
		return 0;
	}
	ptr = field(response, 0);
	oldmode = ptr ? atoi(ptr) : 0;
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

/* Read the auto-reboot setting (whether the device starts again when mains
 * is (re)applied) and publish it as ups.start.auto; 1 = yes, 2 = no.
 * This is auxiliary information, so a missing or unexpected answer must not
 * mark the whole UPS as stale: a warning is logged and the previously
 * published value is left in place. */
static int get_auto_reboot(void) {
	char response[MAX_RESPONSE_LENGTH];

	if (do_command(POLL, AUTO_REBOOT, "", response) <= 0) {
		upslogx(LOG_WARNING, "%s: could not read the auto-reboot "
			"setting [%s]; keeping the last known value", __func__,
			AUTO_REBOOT);
		return 0;
	}

	switch (atoi(response)) {
	case 1:
		dstate_setinfo("ups.start.auto", "%s", "yes");
		return 1;
	case 2:
		dstate_setinfo("ups.start.auto", "%s", "no");
		return 1;
	default:
		upslogx(LOG_WARNING, "%s: unexpected [%s] response [%s]; "
			"keeping the last known value", __func__, AUTO_REBOOT,
			response);
		return 0;
	}
}

/* Read the battery installation date (BRD field 0, formatted as YYYYMMDD).
 * The second field looks like a scheduled replacement date, but its meaning
 * has not been confirmed for this protocol, so it is only reported in debug
 * output instead of being published as a standard variable. */
static void get_battery_date(void) {
	char response[MAX_RESPONSE_LENGTH];
	char *ptr, *sep;

	if (do_command(POLL, BATTERY_REPLACEMENT_DATE, "", response) <= 0)
		return;

	ptr = field(response, 0);
	if (ptr) {
		sep = strchr(ptr, ';');
		if (sep)
			*sep = '\0';
		if (*ptr)
			dstate_setinfo("battery.date", "%s", ptr);
	}

	ptr = field(response, 1);
	if (ptr)
		upsdebugx(2, "%s: unmapped second field [%s]",
			__func__, ptr);
}

/* Read the power-on (boot) delay from the device.  This is auxiliary
 * information: a missing or unexpected answer must not mark the whole UPS
 * as stale, it logs a warning and only leaves the previously known value in
 * place. */
static int get_boot_delay(void) {
	char response[MAX_RESPONSE_LENGTH];
	unsigned int delay;

	if (do_command(POLL, BOOT_DELAY, "", response) <= 0) {
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

/* Read the watchdog setting and publish ups.watchdog.status.  The device
 * reports "<timeout in seconds>;<restart alarm flag>", where a timeout of 0
 * means that the watchdog is disabled.  This is auxiliary information, so a
 * missing or unexpected answer must not mark the whole UPS as stale: a
 * warning is logged and the previously published value is left in place. */
static int get_watchdog(void) {
	char response[MAX_RESPONSE_LENGTH];
	char *ptr;
	int timeout, alarm;

	if (do_command(POLL, WATCHDOG, "", response) <= 0) {
		upslogx(LOG_WARNING, "%s: could not read the watchdog "
			"setting [%s]; keeping the last known value",
			__func__, WATCHDOG);
		return 0;
	}

	ptr = field(response, 0);
	if (!ptr) {
		upslogx(LOG_WARNING, "%s: unexpected [%s] response [%s]; "
			"keeping the last known value", __func__, WATCHDOG,
			response);
		return 0;
	}
	timeout = atoi(ptr);
	if (timeout < 0 || timeout > (int)MAX_WATCHDOG) {
		upslogx(LOG_WARNING, "%s: unexpected [%s] response [%s]; "
			"keeping the last known value", __func__, WATCHDOG,
			response);
		return 0;
	}
	watchdog_seconds = (unsigned int)timeout;
	watchdog_known = 1;

	ptr = field(response, 1);
	if (ptr) {
		alarm = atoi(ptr);
		if (alarm == 1 || alarm == 2)
			watchdog_alarm = (unsigned int)alarm;
	}

	dstate_setinfo("ups.watchdog.status", "%s",
		watchdog_seconds ? "enabled" : "disabled");
	return 1;
}

/* Arm or disarm the watchdog, keeping the timeout (255 seconds when none
 * was ever reported) and the restart-alarm flag that the device uses. */
static void set_watchdog(int enable) {
	char parm[20];
	unsigned int timeout = 0;

	if (enable) {
		/* arming without a reading uses assumed values, which is
		 * worth telling about: the timeout is not the one the device
		 * would have reported */
		if (!watchdog_known)
			upslogx(LOG_WARNING, "%s: arming the watchdog with an "
				"assumed timeout, the device never reported "
				"its [%s] setting", __func__, WATCHDOG);
		timeout = watchdog_seconds ? watchdog_seconds : DEFAULT_WATCHDOG;
	}

	snprintf(parm, sizeof(parm), "%u;%u", timeout, watchdog_alarm);
	if (!send_set_command(WATCHDOG, parm))
		upslogx(LOG_ERR, "%s: device rejected or did not answer "
			"[%s %s]", __func__, WATCHDOG, parm);
}

/* Read the bypass reason recorded by the device (BPA) and publish it as
 * input.transfer.reason.  This is a live value, so it is refreshed on every
 * update cycle; a missing or unexpected answer must not mark the whole UPS
 * as stale: a warning is logged and the previously published reason is left
 * in place.  The device value is a two-digit code whose meaning is not
 * documented, so it is published as it comes. */
static int get_bypass_reason(void) {
	char response[MAX_RESPONSE_LENGTH];
	char *ptr;

	if (do_command(POLL, BYPASS_REASON, "", response) <= 0) {
		upslogx(LOG_WARNING, "%s: could not read the bypass reason "
			"[%s]; keeping the last known value", __func__,
			BYPASS_REASON);
		return 0;
	}

	ptr = field(response, 0);
	if (!ptr || !*ptr) {
		upslogx(LOG_WARNING, "%s: unexpected [%s] response [%s]; "
			"keeping the last known value", __func__, BYPASS_REASON,
			response);
		return 0;
	}

	dstate_setinfo("input.transfer.reason", "%s", ptr);
	return 1;
}

/* Read the ECO (high efficiency) setting and publish it as
 * input.eco.switchable; 1 = high efficiency, 2 = high quality.
 * The vendor software only ever writes this setting, so a device may not
 * answer the poll: in that case a warning is logged and the published value
 * is left alone. */
static int get_eco_mode(void) {
	char response[MAX_RESPONSE_LENGTH];

	if (do_command(POLL, ECONOMIC_MODE, "", response) <= 0) {
		upslogx(LOG_WARNING, "%s: could not read the ECO setting "
			"[%s]; keeping the last known value", __func__,
			ECONOMIC_MODE);
		return 0;
	}

	switch (atoi(response)) {
	case 1:
		dstate_setinfo("input.eco.switchable", "%s", "ECO");
		return 1;
	case 2:
		dstate_setinfo("input.eco.switchable", "%s", "normal");
		return 1;
	default:
		upslogx(LOG_WARNING, "%s: unexpected [%s] response [%s]; "
			"keeping the last known value", __func__,
			ECONOMIC_MODE, response);
		return 0;
	}
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
	char response[MAX_RESPONSE_LENGTH];

	if (do_command(POLL, ENABLE_BUZZER, "", response) <= 0) {
		upslogx(LOG_WARNING, "%s: could not read the buzzer setting "
			"[%s]; keeping the last known value", __func__,
			ENABLE_BUZZER);
		return 0;
	}

	switch (atoi(response)) {
	case 1:
		dstate_setinfo("ups.beeper.status", "%s", "enabled");
		return 1;
	case 2:
		dstate_setinfo("ups.beeper.status", "%s", "disabled");
		return 1;
	default:
		upslogx(LOG_WARNING, "%s: unexpected [%s] response [%s]; "
			"keeping the last known value", __func__, ENABLE_BUZZER,
			response);
		return 0;
	}
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
		if (do_command(POLL, command, "", response) <= 0) {
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

/* Send the restart (return) delay to the UPS.  The SDR value is expressed
 * in minutes, so it is rounded up to never return the load earlier than
 * requested.  Returns 1 when the device accepted the setting. */
static int set_start_delay(unsigned int delay) {
	char parm[20];

	snprintf(parm, sizeof(parm), "%u", (delay + 59U) / 60U);
	return send_set_command(TSU_SHUTDOWN_RESTART, parm);
}

/* Restart delay (in seconds) to ask for when the load has to come back
 * after a shutdown: one second more than the shutdown delay, so that
 * rounding up to whole minutes (set_start_delay(): the device counts its
 * restart delay in minutes) always lands behind the off.  The device does
 * not restart the load when the restart delay is 0 - an online device
 * switches its outputs off and leaves them off, an offline one is only
 * started up (tested on an SUINT1500RTXL2Ua) - hence it stays 0 then. */
static unsigned int reboot_start_delay(unsigned int delay)
{
	return delay ? delay + 1U : 0U;
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

/* Parse the delay (in seconds) before the load is returned.  A value of 0
 * is meaningful here and means "return without delay", so it is accepted,
 * unlike the shutdown delay. */
static int parse_return_delay(const char *val, unsigned int *delay)
{
	return parse_uint_range(val, MIN_STARTDELAY, MAX_STARTDELAY, delay);
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

static int instcmd(const char *cmdname, const char *extra)
{
	static const struct {
		const char *suffix;	/* command suffix */
		const char *relay;	/* protocol RELAY_* command */
		int maybepower;		/* 1: only possibly affects power state */
	} outlet_commands[] = {
		{ "load.off", RELAY_OFF, 0 },
		{ "load.on", RELAY_ON, 1 }
	};
	/* the shutdown commands share the same skeleton: set the auto-reboot
	 * setting ("ups.start.auto") to what the shutdown needs, set the
	 * restart (SDR) delay and schedule the shutdown action (SDA) */
	static const struct {
		const char *name;	/* instant command name */
		int autoreboot;		/* ARB value to set: 1 = yes, 0 = no */
		int use_offdelay;	/* 1: default value is the offdelay setting */
		int timed_return;	/* 1: the load restarts right behind the off */
	} shutdown_commands[] = {
		{ "shutdown.reboot",          1, 1, 1 },
		{ "shutdown.reboot.graceful", 1, 0, 1 },
		{ "shutdown.return",          1, 1, 0 },
		{ "shutdown.stayoff",         0, 1, 0 }
	};
	int i;
	char parm[20];
	char command[32];	/* "outlet.<n>.load.on" and friends */
	unsigned int outlet, cmd_index;
	unsigned int delay;
	unsigned int fallback;
	int result = STAT_INSTCMD_HANDLED;

	upsdebug_INSTCMD_STARTING(cmdname, extra);

	if (!strcasecmp(cmdname, "load.off")) {
		upslog_INSTCMD_POWERSTATE_CHANGE(cmdname, extra);
		for (i = 0; i < ups.outlets; i++) {
			snprintf(parm, sizeof(parm), "%d;1", i + 1);
			if (send_set_or_fail(RELAY_OFF, parm) != STAT_INSTCMD_HANDLED)
				result = STAT_INSTCMD_FAILED;
		}
		return result;
	}
	if (!strcasecmp(cmdname, "load.on")) {
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		for (i = 0; i < ups.outlets; i++) {
			snprintf(parm, sizeof(parm), "%d;1", i + 1);
			if (send_set_or_fail(RELAY_ON, parm) != STAT_INSTCMD_HANDLED)
				result = STAT_INSTCMD_FAILED;
		}
		return result;
	}
	/* one set of commands per outlet; these take no value */
	for (cmd_index = 0; cmd_index < SIZEOF_ARRAY(outlet_commands); cmd_index++) {
		for (outlet = 1; outlet <= (unsigned int)ups.outlets; outlet++) {
			snprintf(command, sizeof(command), "outlet.%u.%s",
				outlet, outlet_commands[cmd_index].suffix);
			if (strcasecmp(cmdname, command))
				continue;

			if (outlet_commands[cmd_index].maybepower)
				upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
			else
				upslog_INSTCMD_POWERSTATE_CHANGE(cmdname, extra);

			snprintf(parm, sizeof(parm), "%u;1", outlet);
			return send_set_or_fail(outlet_commands[cmd_index].relay,
				parm);
		}
	}
	/* buzzer: BUZ 2 silences the alarm that is currently sounding.
	 * Enabling/disabling the buzzer is a device setting (it survives
	 * restarts), so it is exposed as the read/write ups.beeper.status
	 * variable instead of a pair of instant commands. */
	if (!strcasecmp(cmdname, "beeper.mute")) {
		return send_set_or_fail(BUZZER, "2");
	}
	if (!strcasecmp(cmdname, "clear.fault.record")) {
		/* reset the bypass reason recorded by the device */
		return send_set_or_fail(BYPASS_REASON, "0");
	}
	for (cmd_index = 0; cmd_index < SIZEOF_ARRAY(shutdown_commands);
		cmd_index++) {
		if (strcasecmp(cmdname, shutdown_commands[cmd_index].name))
			continue;

		fallback = shutdown_commands[cmd_index].use_offdelay
			? offdelay : DEFAULT_GRACEFUL_DELAY;
		if (!get_shutdown_delay(cmdname, extra, fallback, &delay))
			return STAT_INSTCMD_FAILED;

		upslog_INSTCMD_POWERSTATE_CHANGE(cmdname, extra);
		/* set the auto-reboot and return-delay prerequisites before
		 * scheduling the shutdown: if either cannot be confirmed,
		 * stop here and leave the load powered rather than power it
		 * off with no guaranteed way to come back on */
		if (!auto_reboot(shutdown_commands[cmd_index].autoreboot)) {
			upslogx(LOG_ERR, "%s: could not set or confirm "
				"ups.start.auto, not powering the load off", cmdname);
			return STAT_INSTCMD_FAILED;
		}
		/* the reboot commands ask the load to come back right behind
		 * the shutdown; the others clear any return delay left over
		 * from an earlier command, so that the load comes back only
		 * when mains is (re)applied, or not at all */
		if (!set_start_delay(shutdown_commands[cmd_index].timed_return
			? reboot_start_delay(delay) : 0U)) {
			upslogx(LOG_ERR, "%s: could not set the return "
				"delay, not powering the load off", cmdname);
			return STAT_INSTCMD_FAILED;
		}
		snprintf(parm, sizeof(parm), "%u", delay);
		if (send_set_or_fail(TSU_SHUTDOWN_ACTION, parm)
		    != STAT_INSTCMD_HANDLED)
			return STAT_INSTCMD_FAILED;
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "shutdown.stop")) {
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		/* SDA 0 cancels a scheduled shutdown; the auto-reboot
		 * setting is left as it is */
		return send_set_or_fail(TSU_SHUTDOWN_ACTION, "0");
	}
	if (!strcasecmp(cmdname, "test.battery.start")) {
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		return send_set_or_fail(TEST, "3");
	}
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
		return send_set_or_fail(TEST, "0");
	}

	upslog_INSTCMD_UNKNOWN(cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

static int setvar(const char *varname, const char *val)
{
	upsdebug_SET_STARTING(varname, val);

	if (!strcasecmp(varname, "ups.id")) {
		set_identification(val);
		get_identification();
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "ups.delay.start")) {
		unsigned int delay;

		if (!parse_return_delay(val, &delay)) {
			upslogx(LOG_ERR, "setvar(%s): invalid return delay "
				"value '%s' (expected %u..%u seconds)",
				varname, val, MIN_STARTDELAY, MAX_STARTDELAY);
			return STAT_SET_FAILED;
		}
		if (!set_start_delay(delay)) {
			upslogx(LOG_ERR, "setvar(%s): device rejected or did "
				"not answer the return delay", varname);
			return STAT_SET_FAILED;
		}
		startdelay = delay;
		dstate_setinfo("ups.delay.start", "%u", startdelay);
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
		if (!strcasecmp(val, "enabled"))
			set_watchdog(1);
		else if (!strcasecmp(val, "disabled"))
			set_watchdog(0);
		else {
			upslogx(LOG_ERR, "setvar(%s): invalid value '%s' "
				"(expected enabled or disabled)", varname, val);
			return STAT_SET_FAILED;
		}
		/* read the value back, so that the published variable
		 * reflects what the device actually accepted */
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
	if (!strcasecmp(varname, "input.transfer.low")) {
		unsigned int voltage;

		if (!parse_transfer_voltage(val, min_low_transfer,
			max_low_transfer, &voltage)) {
			upslogx(LOG_ERR, "setvar(%s): invalid value '%s' "
				"(expected %u..%u)", varname, val,
				min_low_transfer, max_low_transfer);
			return STAT_SET_FAILED;
		}
		set_transfer_voltage_low((int)voltage);
		get_transfer_voltage_low();
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "input.transfer.high")) {
		unsigned int voltage;

		if (!parse_transfer_voltage(val, min_high_transfer,
			max_high_transfer, &voltage)) {
			upslogx(LOG_ERR, "setvar(%s): invalid value '%s' "
				"(expected %u..%u)", varname, val,
				min_high_transfer, max_high_transfer);
			return STAT_SET_FAILED;
		}
		set_transfer_voltage_high((int)voltage);
		get_transfer_voltage_high();
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
	char buf[32];	/* "outlet.<n>.load.on" and friends */
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
		if (ptr) {
			int ipv = atoi(ptr);
			if (ipv >= 0)
				min_low_transfer = (unsigned int)ipv;
		}
		ptr = field(response, 9);
		if (ptr) {
			int ipv = atoi(ptr);
			if (ipv >= 0)
				max_low_transfer = (unsigned int)ipv;
		}
		ptr = field(response, 12);
		if (ptr) {
			int ipv = atoi(ptr);
			if (ipv >= 0)
				min_high_transfer = (unsigned int)ipv;
		}
		ptr = field(response, 11);
		if (ptr) {
			int ipv = atoi(ptr);
			if (ipv >= 0)
				max_high_transfer = (unsigned int)ipv;
		}
	}
	if (do_command(POLL, OUTLET_RELAYS, "", response) > 0) {
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
	/* define things that are settable */
	dstate_setinfo("ups.delay.start", "%u", startdelay);
	dstate_setflags("ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.start", 4);
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
	if (get_transfer_voltage_low() && max_low_transfer) {
		dstate_setflags("input.transfer.low", ST_FLAG_RW);
		for (i = min_low_transfer; i <= max_low_transfer; i++)
			dstate_addenum("input.transfer.low", "%u", i);
	}
	if (get_transfer_voltage_high() && max_high_transfer) {
		dstate_setflags("input.transfer.high", ST_FLAG_RW);
		for (i = min_high_transfer; i <= max_high_transfer; i++)
			dstate_addenum("input.transfer.high", "%u", i);
	}
	/* report the settable ranges, so that the values accepted by setvar()
	 * are visible to clients */
	if (max_low_transfer) {
		dstate_setinfo("input.transfer.low.min", "%u", min_low_transfer);
		dstate_setinfo("input.transfer.low.max", "%u", max_low_transfer);
	}
	if (max_high_transfer) {
		dstate_setinfo("input.transfer.high.min", "%u", min_high_transfer);
		dstate_setinfo("input.transfer.high.max", "%u", max_high_transfer);
	}
	if (get_sensitivity()) {
		dstate_setflags("input.sensitivity", ST_FLAG_RW);
		for (i = 0; i < SIZEOF_ARRAY(sensitivity); i++)
			dstate_addenum("input.sensitivity", "%s",
				sensitivity[i].name);
	}
	/* load.off, load.on and the commands below switch individual outlet
	 * outlets, so they are only registered when the device reports any */
	if (ups.outlets) {
		dstate_addcmd("load.off");
		dstate_addcmd("load.on");
		for (i = 1; i <= (unsigned int)ups.outlets; i++) {
			snprintf(buf, sizeof(buf), "outlet.%u.load.off", i);
			dstate_addcmd(buf);
			snprintf(buf, sizeof(buf), "outlet.%u.load.on", i);
			dstate_addcmd(buf);
		}
	}
	dstate_addcmd("beeper.mute");
	dstate_addcmd("clear.fault.record");
	dstate_addcmd("shutdown.reboot");
	dstate_addcmd("shutdown.reboot.graceful");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");

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
			(double) (atoi(ptr)) / 10.0);
	ptr = field(response, 1);
	if (ptr)
		dstate_setinfo("output.frequency", "%03.1f",
			(double) (atoi(ptr)) / 10.0);
	ptr = field(response, 4);
	if (ptr)
		dstate_setinfo("output.current", "%03.1f",
			(double) (atoi(ptr)) / 10.0);

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
		if (ptr2
		 && atoi(ptr2) > 0
		 && atoi(ptr2) <= 99
		 && atoi(ptr) <= atoi(ptr2)
		) {
			low_battery = 1;
		}
	}
	ptr = field(response, 6);
	if (ptr)
		dstate_setinfo("battery.voltage", "%03.1f",
			(double) (atoi(ptr)) / 10.0);
	ptr = field(response, 7);
	if (ptr)
		dstate_setinfo("battery.current", "%03.1f",
			(double) (atoi(ptr)) / 10.0);
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
				(double) (atoi(ptr)) / 10.0);
		ptr = field(response, 1);
		if (ptr)
			dstate_setinfo("input.frequency", "%03.1f",
				(double) (atoi(ptr)) / 10.0);
	}

	if (do_command(POLL, TEST_RESULT, "", response) > 0) {
		int	r;
		size_t	trsize;

		r = atoi(response);
		trsize = SIZEOF_ARRAY(test_result_names);

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
	/* in case the power is on, tell it to automatically reboot.  if
	 * it is off, this has no effect. */
	if (!auto_reboot(1)) {
		upslogx(LOG_ERR, "%s: could not set or confirm ups.start.auto, "
			"not powering the load off", __func__);
		return;
	}
	if (!set_start_delay(startdelay)) {
		upslogx(LOG_ERR, "%s: could not set the return delay, "
			"not powering the load off", __func__);
		return;
	}
	/* delay before shutdown, in seconds: honor 'offdelay' if the user set
	 * it in ups.conf, otherwise keep the historical default */
	snprintf(parm, sizeof(parm), "%u",
		offdelay_from_conf ? offdelay : 5U);
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
		"(default: 0 = no wait; -1 = automatic)");

	snprintf(msg, sizeof msg, "Set shutdown delay, in seconds (default=%u, range %u..%u).",
		DEFAULT_OFFDELAY, MIN_OFFDELAY, MAX_OFFDELAY);
	addvar(VAR_VALUE, "offdelay", msg);

	/* The return delay is configured in minutes because that is the unit
	 * used by the device (SDR), while it is exposed in seconds through
	 * the ups.delay.start variable, as usual for delays in NUT. */
	snprintf(msg, sizeof msg, "Set return (restart) delay, in minutes "
		"(default=%u, range %u..%u).",
		DEFAULT_STARTDELAY / 60U,
		MIN_STARTDELAY / 60U,
		MAX_STARTDELAY_MINUTES);
	addvar(VAR_VALUE, "startdelay", msg);
}

void upsdrv_initups(void)
{
	const char *val;

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);

	/* Initialize command_delay from configuration.  The value is in
	 * milliseconds: 0 disables the pacing (commands follow each other as
	 * soon as the previous answer is read), -1 asks the driver to pace
	 * them by itself (start without waiting, raise the interval once if a
	 * command times out) and a positive value is the minimum interval to
	 * keep between two commands. */
	command_delay_conf = DEFAULT_COMMAND_DELAY;
	val = getval("command_delay");
	if (val && *val) {
		long temp;

		if (!str_to_long_strict(val, &temp, 10) ||
		    temp < (long)COMMAND_DELAY_AUTO ||
		    temp > (long)MAX_COMMAND_DELAY) {
			fatalx(EXIT_FAILURE, "Invalid command_delay parameter: %s "
				"(expected -1 (auto), 0 (no wait) or 1..%u "
				"milliseconds)", val, MAX_COMMAND_DELAY);
		}
		command_delay_conf = temp;
	} else {
		upsdebugx(2, "Using default command_delay of %ld milliseconds",
			command_delay_conf);
	}

	if (command_delay_conf == (long)COMMAND_DELAY_AUTO) {
		command_delay_auto = 1;
		command_delay = 0;
		upsdebugx(2, "command_delay is automatic: commands are paced by "
			"%u milliseconds once one of them times out",
			DEFAULT_AUTO_COMMAND_DELAY);
	} else {
		command_delay_auto = 0;
		command_delay = (unsigned int)command_delay_conf;
		if (command_delay)
			upsdebugx(2, "Setting command_delay to %u milliseconds",
				command_delay);
		else
			upsdebugx(2, "command_delay is 0: no wait between commands");
	}

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
		offdelay_from_conf = 1;
		upsdebugx(2, "Setting offdelay to %u seconds", offdelay);
	} else {
		upsdebugx(2, "Using default offdelay of %u seconds", offdelay);
	}

	/* Initialize startdelay from configuration.  The parameter is in
	 * minutes (the unit used by the device), the internal value is in
	 * seconds (as used by ups.delay.start). */
	val = getval("startdelay");
	if (val && *val) {
		unsigned int temp;
		if (!str_to_uint_strict(val, &temp, 10) ||
		    temp > MAX_STARTDELAY_MINUTES) {
			fatalx(EXIT_FAILURE, "Invalid startdelay parameter: %s "
				"(expected %u..%u minutes)",
				val, MIN_STARTDELAY / 60U,
				MAX_STARTDELAY_MINUTES);
		}
		startdelay = temp * 60U;
		upsdebugx(2, "Setting startdelay to %u seconds (%u minutes)",
			startdelay, temp);
	} else {
		upsdebugx(2, "Using default startdelay of %u seconds",
			startdelay);
	}
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
