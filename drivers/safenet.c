/*
 * safenet.c - model specific routines for following units:
 *
 * - Fairstone L525/-625/-750
 * - Fenton P400/-600/-800
 * - Gemini  UPS625/-1000
 * - Powerwell PM525A/-625A/-800A/-1000A/-1250A
 * - Repotec RPF525/-625/-800/-1000
 * - Soltec Winmate 525/625/800/1000
 * - Sweex 500/1000
 * - others using SafeNet software and serial interface
 *
 * Status:
 *  20081102/Revision 1.41 - Arjen de Korte <adkorte-guest@alioth.debian.org>
 *   - allow more time for reading reply to command
 *  20081106/Revision 1.5 - Arjen de Korte <adkorte-guest@alioth.debian.org>
 *   - changed communication with UPS
 *   - improved handling of battery & system test
 *  20081228/Revision 1.6 - Arjen de Korte <adkorte-guest@alioth.debian.org>
 *   - add ondelay and offdelay
 *
 * Copyright (C) 2003-2008  Arjen de Korte <adkorte-guest@alioth.debian.org>
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
#include "serial.h"
#include "safenet.h"

#define DRIVER_NAME	"Generic SafeNet UPS driver"
#define DRIVER_VERSION	"1.6"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arjen de Korte <adkorte-guest@alioth.debian.org>",
	DRV_STABLE,
	{ NULL }
};

/*
 * Here we keep the last known status of the UPS
 */
static union	{
	char			reply[10];
	struct safenet		status;
} ups;

static int ondelay = 1;		/* minutes */
static int offdelay = 30;	/* seconds */

static int safenet_command(const char *command)
{
	char	reply[32];
	size_t	i;
	ssize_t	ret;

	/*
	 * Get rid of whatever is in the in- and output buffers.
	 */
	ser_flush_io(upsfd);

	/*
	 * Send the command and read back the status line. When we just send
	 * a status polling command, it will return the actual status.
	 */
	ret = ser_send(upsfd, "%s", command);

	if (ret < 0) {
		upsdebug_with_errno(3, "send");
		return -1;
	}

	if (ret == 0) {
		upsdebug_with_errno(3, "send: timeout");
		return -1;
	}

	upsdebugx(3, "send: %.*s", (int)strcspn(command, "\r"), command);

	/*
	 * Read the reply from the UPS.
	 */
	ret = ser_get_buf(upsfd, reply, sizeof(reply), 1, 0);

	if (ret < 0) {
		upsdebug_with_errno(3, "read");
		return -1;
	}

	if (ret == 0) {
		upsdebugx(3, "read: timeout");
		return -1;
	}

	upsdebugx(3, "read: %.*s", (int)strcspn(reply, "\r"), reply);

	/*
	 * We check if the reply looks like a valid status.
	 */
	if ((ret != 12) || (reply[0] != '$') || (strspn(reply+1, "AB") != 10)) {
		return -1;
	}

	for (i = 0; i < 10; i++) {
		ups.reply[i] = ((reply[i+1] == 'B') ? 1 : 0);
	}

	return 0;
}

static void safenet_update(void)
{
	status_init();

	if (ups.status.onbattery) {
		status_set("OB");
	} else {
		status_set("OL");
	}

	if (ups.status.batterylow) {
		status_set("LB");
	}

	if (ups.status.overload) {
		status_set("OVER");
	}

	if (ups.status.batteryfail) {
		status_set("RB");
	}

	if (ups.status.systemtest) {
		status_set("CAL");
	}

	alarm_init();

	if (ups.status.systemfail) {
		alarm_set("System selftest fail!");
	}

	alarm_commit();

	status_commit();
}

static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "beeper.off")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.off' command has been renamed to 'beeper.mute' for this driver");
		return instcmd("beeper.mute", NULL);
	}

	if (!strcasecmp(cmdname, "beeper.on")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.on' command has been renamed to 'beeper.enable'");
		return instcmd("beeper.enable", NULL);
	}

	/*
	 * Start the UPS selftest
	 */
	if (!strcasecmp(cmdname, "test.battery.start")) {
		if (safenet_command(COM_BATT_TEST)) {
			return STAT_INSTCMD_FAILED;
		} else {
			return STAT_INSTCMD_HANDLED;
		}
	}

	/*
	 * Stop the UPS selftest
	 */
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		if (safenet_command(COM_STOP_TEST)) {
			return STAT_INSTCMD_FAILED;
		} else {
			return STAT_INSTCMD_HANDLED;
		}
	}

	/*
	 * Start simulated mains failure
	 */
	if (!strcasecmp (cmdname, "test.failure.start")) {
		if (safenet_command(COM_MAINS_TEST)) {
			return STAT_INSTCMD_FAILED;
		} else {
			return STAT_INSTCMD_HANDLED;
		}
	}

	/*
	 * Stop simulated mains failure
	 */
	if (!strcasecmp (cmdname, "test.failure.stop")) {
		if (safenet_command(COM_STOP_TEST)) {
			return STAT_INSTCMD_FAILED;
		} else {
			return STAT_INSTCMD_HANDLED;
		}
	}

	/*
	 * If beeper is off, toggle beeper state (so it should be ON after this)
	 */
	if (!strcasecmp(cmdname, "beeper.enable")) {
		if (ups.status.silenced && safenet_command(COM_TOGGLE_BEEP)) {
			return STAT_INSTCMD_FAILED;
		} else {
			return STAT_INSTCMD_HANDLED;
		}
	}

	/*
	 * If beeper is not off, toggle beeper state (so it should be OFF after this)
	 * Unfortunately, this only mutes the beeper, it turns back on for the next
	 * event automatically (no way to stop this, besides side cutters)
	 */
	if (!strcasecmp(cmdname, "beeper.mute")) {
		if (!ups.status.silenced && safenet_command(COM_TOGGLE_BEEP)) {
			return STAT_INSTCMD_FAILED;
		} else {
			return STAT_INSTCMD_HANDLED;
		}
	}

	/*
	 * Toggle beeper state unconditionally
	 */
	if (!strcasecmp(cmdname, "beeper.toggle")) {
		if (safenet_command(COM_TOGGLE_BEEP)) {
			return STAT_INSTCMD_FAILED;
		} else {
			return STAT_INSTCMD_HANDLED;
		}

	}

	/*
	 * Shutdown and wait for the power to return
	 */
	if (!strcasecmp(cmdname, "shutdown.return")) {
		char	command[] = SHUTDOWN_RETURN;

		command[4] += ((offdelay % 1000) / 100);
		command[5] += ((offdelay % 100) / 10);
		command[6] +=  (offdelay % 10);

		safenet_command(command);
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Shutdown and reboot
	 */
	if (!strcasecmp(cmdname, "shutdown.reboot")) {
		char	command[] = SHUTDOWN_REBOOT;

		command[3] += ((offdelay % 1000) / 100);
		command[4] += ((offdelay % 100) / 10);
		command[5] +=  (offdelay % 10);

		command[7] += ((ondelay % 10000) / 1000);
		command[8] += ((ondelay % 1000) / 100);
		command[9] += ((ondelay % 100) / 10);
		command[10] += (ondelay % 10);

		safenet_command(command);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	int	retry = 3;
	char	*v;

	usleep(100000);

	/*
	 * Very crude hardware detection. If an UPS is attached, it will set DSR
	 * to 1. Bail out if it isn't.
	 */
	if (!ser_get_dsr(upsfd)) {
		fatalx(EXIT_FAILURE, "Serial cable problem or nothing attached to %s", device_path);
	}

	/*
	 * Initialize the serial interface of the UPS by sending the magic
	 * string. If it does not respond with a valid status reply,
	 * display an error message and give up.
	 */
	while (safenet_command(COM_INITIALIZE)) {
		if (--retry) {
			continue;
		}

		fatalx(EXIT_FAILURE, "SafeNet protocol compatible UPS not found on %s", device_path);
	}

	/*
	 * Read the commandline settings for the following parameters, since we can't
	 * autodetect them.
	 */
	dstate_setinfo("ups.mfr", "%s", ((v = getval("manufacturer")) != NULL) ? v : "unknown");
	dstate_setinfo("ups.model", "%s", ((v = getval("modelname")) != NULL) ? v : "unknown");
	dstate_setinfo("ups.serial", "%s", ((v = getval("serialnumber")) != NULL) ? v : "unknown");

	dstate_setinfo("ups.delay.start", "%d", 60 * ondelay);
	dstate_setinfo("ups.delay.shutdown", "%d", offdelay);

	/*
	 * These are the instant commands we support.
	 */
	dstate_addcmd ("test.battery.start");
	dstate_addcmd ("test.battery.stop");
	dstate_addcmd ("test.failure.start");
	dstate_addcmd ("test.failure.stop");
	dstate_addcmd ("beeper.on");
	dstate_addcmd ("beeper.off");
	dstate_addcmd ("beeper.enable");
	dstate_addcmd ("beeper.mute");
	dstate_addcmd ("beeper.toggle");
	dstate_addcmd ("shutdown.return");
	dstate_addcmd ("shutdown.reboot");

	upsh.instcmd = instcmd;
}

/*
 * The status polling commands are *almost* random. Whatever the  reason
 * is, there is a certain pattern in them. The first character after the
 * start character 'Z' determines how many positions there are between
 * that character and the single 'L' character that's in each command (A=0,
 * B=1,...,J=9). The rest is filled with random (?) data [A...J]. But why?
 * No idea. The UPS *does* check if the polling commands match this format.
 * And as the SafeNet software uses "random" polling commands, so do we.
 *
 * Note: if you don't use ASCII, the characters will be different!
 */
void upsdrv_updateinfo(void)
{
	char	command[] = COM_POLL_STAT;
	int	i;
	static int	retry = 0;

	/*
	 * Fill the command portion with random characters from the range
	 * [A...J].
	 */
	for (i = 1; i < 12; i++) {
		command[i] = (random() % 10) + 'A';
	}

	/*
	 * Find which character must be an 'L' and put it there.
	 */
	command[command[1]-'A'+2] = 'L';

	/*
	 * Do a status poll.
	 */
	if (safenet_command(command)) {
		ser_comm_fail("Status read failed");

		if (retry < 2) {
			retry++;
		} else {
			dstate_datastale();
		}

		return;
	}

	ser_comm_good();
	retry = 0;

	if (ups.status.systemtest && ups.status.batterylow) {

		/*
		 * Don't update status after stopping battery test, to
		 * allow UPS to update the status flags (OB+LB glitch)
		 */
		if (safenet_command(COM_STOP_TEST)) {
			upslogx(LOG_WARNING, "Can't terminate battery test!");
		} else {
			upslogx(LOG_INFO, "Battery test finished");
			return;
		}
	}

	safenet_update();

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	int	retry = 3;

	/*
	 * Since we may have arrived here before the hardware is initialized,
	 * try to initialize it here.
	 *
	 * Initialize the serial interface of the UPS by sending the magic
	 * string. If it does not respond with a valid status reply,
	 * display an error message and give up.
	 */
	while (safenet_command(COM_INITIALIZE)) {
		if (--retry) {
			continue;
		}

		fatalx(EXIT_FAILURE, "SafeNet protocol compatible UPS not found on %s", device_path);
	}

	/*
	 * Since the UPS will happily restart on battery, we must use a
	 * different shutdown command depending on the line status, so
	 * we need to check the status of the UPS here.
	 */
	if (ups.status.onbattery) {
		instcmd("shutdown.return", NULL);
	} else {
		instcmd("shutdown.reboot", NULL);
	}
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "manufacturer", "manufacturer [unknown]");
	addvar(VAR_VALUE, "modelname", "modelname [unknown]");
	addvar(VAR_VALUE, "serialnumber", "serialnumber [unknown]");

	addvar(VAR_VALUE, "ondelay", "Delay before UPS startup (minutes)");
	addvar(VAR_VALUE, "offdelay", "Delay before UPS shutdown (seconds)");
}

void upsdrv_initups(void)
{
	struct termios	tio;
	const char	*val;

	/*
	 * Open and lock the serial port and set the speed to 1200 baud.
	 */
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B1200);

	if (tcgetattr(upsfd, &tio)) {
		fatal_with_errno(EXIT_FAILURE, "tcgetattr");
	}

	/*
	 * Use canonical mode input processing (to read reply line)
	 */
	tio.c_lflag |= ICANON;	/* Canonical input (erase and kill processing) */

	tio.c_cc[VEOF] = _POSIX_VDISABLE;
	tio.c_cc[VEOL] = '\r';
	tio.c_cc[VERASE] = _POSIX_VDISABLE;
	tio.c_cc[VINTR]  = _POSIX_VDISABLE;
	tio.c_cc[VKILL]  = _POSIX_VDISABLE;
	tio.c_cc[VQUIT]  = _POSIX_VDISABLE;
	tio.c_cc[VSUSP]  = _POSIX_VDISABLE;
	tio.c_cc[VSTART] = _POSIX_VDISABLE;
	tio.c_cc[VSTOP]  = _POSIX_VDISABLE;

	if (tcsetattr(upsfd, TCSANOW, &tio)) {
		fatal_with_errno(EXIT_FAILURE, "tcsetattr");
	}

	/*
	 * Set DTR and clear RTS to provide power for the serial interface.
	 */
	ser_set_dtr(upsfd, 1);
	ser_set_rts(upsfd, 0);

	val = getval("ondelay");
	if (val) {
		ondelay = strtol(val, NULL, 10);
	}

	if ((ondelay < 0) || (ondelay > 9999)) {
		fatalx(EXIT_FAILURE, "Start delay '%d' out of range [0..9999]", ondelay);
	}

	val = getval("offdelay");
	if (val) {
		offdelay = strtol(val, NULL, 10);
	}

	if ((offdelay < 0) || (offdelay > 999)) {
		fatalx(EXIT_FAILURE, "Shutdown delay '%d' out of range [0..999]", offdelay);
	}
}

void upsdrv_cleanup(void)
{
	ser_set_dtr(upsfd, 0);
	ser_close(upsfd, device_path);
}
