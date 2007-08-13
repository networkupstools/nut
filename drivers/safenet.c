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
 *  20031015/Revision 0.1 - Arjen de Korte <arjen@de-korte.org>
 *   - initial release (entirely based on reverse engineering the
 *     serial data stream)
 *  20031022/Revision 0.2 - Arjen de Korte <arjen@de-korte.org>
 *   - status polling command is now "random" (just like the
 *     SafeNet (Windows) driver
 *   - low battery status is "LB" (not "BL")
 *  20031228/Revision 0.3 - Arjen de Korte <arjen@de-korte.org>
 *   - instant command 'shutdown.return' added
 *   - added 'manufacturer', 'modelname' and 'serialnumber'
 *     commandline parameters
 *   - removed experimental driver flag
 *   - documentation & code cleanup
 *  20060108/Revision 0.4 - Arjen de Korte <arjen@de-korte.org>
 *   - minor changes to make sure state changes are not missed
 *   - log errors when instant commands can't be handled
 *   - crude hardware detection which looks for DSR=1
 *  20060128/Revision 0.5 - Arjen de Korte <arjen@de-korte.org>
 *   - removed TRUE/FALSE defines, which were not really
 *     improving the code anyway
 *  20060131/Revision 1.0 - Arjen de Korte <arjen@de-korte.org>
 *   - put all UPS commands in header file (for easy retrieval)
 *   - remove log errors when instant commands can't be handled
 *     (doesn't work reliably on at least one flavor)
 *  20061229/Revision 1.1 - Arjen de Korte <arjen@de-korte.org>
 *   - flush the serial input buffer before sending commands
 *     (to get rid of unsollicited serial PnP replies)
 *  20070211/Revision 1.2 - Arjen de Korte <arjen@de-korte.org>
 *   - wait for three consecutive failed polls before declaring
 *     data stale in order not to bother clients with temporary
 *     problems
 *  20070304/Revision 1.3 - Arjen de Korte <arjen@de-korte.org>
 *   - in battery test mode (CAL state) stop the test when the
 *     the low battery state is reached
 *
 * Copyright (C) 2003-2006  Arjen de Korte <arjen@de-korte.org>
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

#include <stdlib.h>
#include <ctype.h>

#include "main.h"
#include "serial.h"
#include "safenet.h"

#define DRV_VERSION	"1.4"

/*
 * Here we keep the last known status of the UPS
 */
static union	{
	char			reply[10];
	struct safenet		status;
} ups;

static int safenet_command(const char *command)
{
	char	reply[32];
	int	i;

	/*
	 * Get rid of whatever is in the in- and output buffers.
	 */
	ser_flush_io(upsfd);

	/*
	 * Send the command and read back the status line. When we just send
	 * a status polling command, it will return the actual status.
	 */
	ser_send_pace(upsfd, 10000, command);
	upsdebugx(3, "UPS command %s", command);

	/*
	 * Read the reply from the UPS. Allow twice the time needed to read 12
	 * bytes (120 bits (including start & stop bits) / 1200 baud = 100ms).
	 */
	ser_get_line(upsfd, reply, sizeof(reply), '\r', "", 0, 200000);
	upsdebugx(3, "UPS answers %s", (strlen(reply)>0) ? reply : "[INVALID]");

	/*
	 * We check if the reply looks like a valid status.
	 */

	if ((strlen(reply) != 11) || (reply[0] != '$') || (strspn(reply+1, "AB") != 10)) {
		return(-1);
	}

	for (i=0; i<10; i++) {
		ups.reply[i] = ((reply[i+1] == 'B') ? 1 : 0);
	}

	return(0);
}

static void safenet_update()
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

	/*
	 * This is not actually an indication of the UPS in the BYPASS state, it is
	 * an indication that the UPS failed the self diagnostics (which is NOT the
	 * same as a replace battery warning). Since the result is the same (no
	 * backup in case of mains failure) we give it this status. Introduce a new
	 * status here?
	 */
	if (ups.status.systemfail) {
		status_set("BYPASS");
	}

	/*
	 * This is not actually an indication of the UPS in the CAL state, it is an
	 * indication that the UPS is in the system test state. The result of this
	 * test may indicate a battery failure, so this is the closest we can get.
	 * Introduce a new status here?
	 */
	if (ups.status.systemtest) {
		status_set("CAL");
	}

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
		safenet_command(COM_BATT_TEST);
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Stop the UPS selftest
	 */
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		safenet_command(COM_STOP_TEST);
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Start simulated mains failure
	 */
	if (!strcasecmp (cmdname, "test.failure.start")) {
		safenet_command(COM_MAINS_TEST);
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Stop simulated mains failure
	 */
	if (!strcasecmp (cmdname, "test.failure.stop")) {
		safenet_command(COM_STOP_TEST);
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * If beeper is off, toggle beeper state (so it should be ON after this)
	 */
	if (!strcasecmp(cmdname, "beeper.enable")) {
		if (ups.status.silenced) {
			safenet_command(COM_TOGGLE_BEEP);
		}

		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * If beeper is not off, toggle beeper state (so it should be OFF after this)
	 * Unfortunately, this only mutes the beeper, it turns back on for the next
	 * event automatically (no way to stop this, besides side cutters)
	 */
	if (!strcasecmp(cmdname, "beeper.mute")) {
		if (!ups.status.silenced) {
			safenet_command(COM_TOGGLE_BEEP);
		}

		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Toggle beeper state unconditionally
	 */
	if (!strcasecmp(cmdname, "beeper.toggle")) {
		safenet_command(COM_TOGGLE_BEEP);

		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Shutdown immediately and wait for the power to return
	 */
	if (!strcasecmp(cmdname, "shutdown.return")) {
		safenet_command(SHUTDOWN_RETURN);
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Shutdown immediately and reboot after 1 minute
	 */
	if (!strcasecmp(cmdname, "shutdown.reboot")) {
		safenet_command(SHUTDOWN_REBOOT);
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Shutdown in 20 seconds and reboot after 1 minute
	 */
	if (!strcasecmp(cmdname, "shutdown.reboot.graceful")) {
		safenet_command(GRACEFUL_REBOOT);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	int	retry = 3;
	char	*v;

	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
	
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
	dstate_addcmd ("shutdown.reboot.graceful");

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
		safenet_command(COM_STOP_TEST);
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
		safenet_command(SHUTDOWN_RETURN);
	} else {
		safenet_command(SHUTDOWN_REBOOT);
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
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - Generic SafeNet UPS driver %s (%s)\n\n", DRV_VERSION, UPS_VERSION);
}

void upsdrv_initups(void)
{
	/*
	 * Open and lock the serial port and set the speed to 1200 baud.
	 */
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B1200);

	/*
	 * Set DTR and clear RTS to provide power for the serial interface.
	 */
	ser_set_dtr(upsfd, 1);
	ser_set_rts(upsfd, 0);
}

void upsdrv_cleanup(void)
{
	ser_set_dtr(upsfd, 0);
	ser_close(upsfd, device_path);
}
