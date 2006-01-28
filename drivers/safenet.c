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
#include <sys/ioctl.h>

#include "main.h"
#include "serial.h"
#include "safenet.h"

#define DRV_VERSION	"0.4"

#define ENDCHAR		'\r'
#define IGNCHARS	""
#define	UPSDELAY	10000

/*
 * Here we keep the last known status of the UPS
 */
static safenet_u	ups;

static int safenet_command(char *command)
{
	char	reply[256];
	int	i;

	/*
	 * Send the command, give the UPS a little time to digest it and then
	 * read back the status line. Unless we just send a shutdown command,
	 * it will return the actual status.
	 */
	ser_send_pace(upsfd, UPSDELAY, command);
	ser_get_line(upsfd, reply, sizeof(reply), ENDCHAR, IGNCHARS, 1, 0);

	upsdebugx(3, "UPS command %s", command);
	upsdebugx(3, "UPS answers %s", (strlen(reply)>0) ? reply : "NULL");

	/*
	 * Occasionally the UPS suffers from hickups (it sometimes spits out a
	 * PnP (?) identification string) so we check if the reply looks like
	 * a valid status.
	 */
	if ((strlen(reply) != 11) || (reply[0] != '$') || (strspn(reply+1, "AB") != 10))
	{
		return(-1);
	}

	for (i=0; i<10; i++)
	{
		ups.reply[i] = ((reply[i+1] == 'B') ? 1 : 0);
	}

	status_init();

	if (ups.status.onbattery)
	{
		status_set("OB");
	}
	else
	{
		status_set("OL");
	}

	if (ups.status.batterylow)
	{
		status_set("LB");
	}

	if (ups.status.overload)
	{
		status_set("OVER");
	}

	if (ups.status.batteryfail)
	{
		status_set("RB");
	}

	/*
	 * This is not actually an indication of the UPS in the BYPASS state, it is
	 * an indication that the UPS failed the self diagnostics (which is NOT the
	 * same as a replace battery warning). Since the result is the same (no
	 * backup in case of mains failure) we give it this status. Introduce a new
	 * status here?
	 */
	if (ups.status.systemfail)
	{
		status_set("BYPASS");
	}

	/*
	 * This is not actually an indication of the UPS in the CAL state, it is an
	 * indication that the UPS is in the system test state. The result of this
	 * test may indicate a battery failure, so this is the closest we can get.
	 * Introduce a new status here?
	 */
	if (ups.status.systemtest)
	{
		status_set("CAL");
	}

	status_commit();

	return(0);
}

static void shutdown_return(int delay)
{
	char command[16];

	if ((delay < 1) || (delay > 999))
	{
		upslogx(LOG_ERR, "Shutdown delay %d is not within valid range [1..999]", delay);
		return;
	}

	/*
	 * This is ugly! This UPS has a strange mapping of numerals (A=0, B=1,..,J=9)
	 * so we do this here. Anyone for an easier way?
	 */
	snprintf(command, sizeof(command), "ZBAS%c%c%cWLPGE\r", ((delay / 100) + 'A'),
		(((delay % 100) / 10) + 'A'), ((delay % 10) + 'A'));

	/*
	 * The shutdown command is a one way street. After reception of this command,
	 * the UPS no longer accepts commands and will not reply with a status word.
	 * It makes no sense to check for the returnvalue for the command, as it will
	 * always indicate a failure.
	 */
	safenet_command(command);
}

static void shutdown_reboot(int delay, int restart)
{
	char command[16];

	/*
	 * Bounds checking to prevent bogus commands
	 */
	if ((delay < 1) || (delay > 999))
	{
		upslogx(LOG_ERR, "Shutdown delay %d is not within valid range [1..999]", delay);
		return;
	}

	if ((restart < 1) || (restart > 9999))
	{
		upslogx(LOG_ERR, "Restart delay %d is not within valid range [1..9999]", restart);
		return;
	}

	/*
	 * This is ugly! This UPS has a strange mapping of numerals (A=0, B=1,..,J=9)
	 * so we do this here. Anyone for an easier way?
	 */ 
	snprintf(command, sizeof(command), "ZAF%c%c%cR%c%c%c%cO\r", ((delay / 100) + 'A'),
		(((delay % 100) / 10) + 'A'), ((delay % 10) + 'A'),
		((restart / 1000) + 'A'), (((restart % 1000) / 100) + 'A'),
		(((restart % 100) / 10) + 'A'), ((restart % 10) + 'A'));

	/*
	 * The shutdown command is a one way street. After reception of this command,
	 * the UPS no longer accepts commands and will not reply with a status word.
	 * It makes no sense to check for the returnvalue for the command, as it will
	 * always indicate a failure.
	 */
	safenet_command(command);
}

static int instcmd(const char *cmdname, const char *extra)
{
	/*
	 * Start the UPS selftest
	 */
	if (!strcasecmp(cmdname, "test.battery.start"))
	{
		if (safenet_command("ZFSDERBTRFGY\r"))
		{
			upslogx(LOG_ERR, "Instant command %s not completed", cmdname);
		}
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Stop the UPS selftest
	 */
	if (!strcasecmp(cmdname, "test.battery.stop"))
	{
		if (safenet_command("ZGWLEJFICOPR\r"))
		{
			upslogx(LOG_ERR, "Instant command %s not completed", cmdname);
		}
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Start simulated mains failure
	 */
	if (!strcasecmp (cmdname, "test.failure.start"))
	{
		if (safenet_command("ZAVLEJFICOPR\r"))
		{
			upslogx(LOG_ERR, "Instant command %s not completed", cmdname);
		}
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Stop simulated mains failure
	 */
	if (!strcasecmp (cmdname, "test.failure.stop"))
	{
		if (safenet_command("ZGWLEJFICOPR\r"))
		{
			upslogx(LOG_ERR, "Instant command %s not completed", cmdname);
		}
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * If beeper is off, toggle beeper state (so it should be ON after this)
	 */
	if (!strcasecmp(cmdname, "beeper.on"))
	{
		if (ups.status.silenced && safenet_command("ZELWSABPMBEQ\r"))
		{
			upslogx(LOG_ERR, "Instant command %s not completed", cmdname);
		}
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * If beeper is not off, toggle beeper state (so it should be OFF after this)
	 */
	if (!strcasecmp(cmdname, "beeper.off"))
	{
		if (!ups.status.silenced && safenet_command("ZELWSABPMBEQ\r"))
		{
			upslogx(LOG_ERR, "Instant command %s not completed", cmdname);
		}
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Shutdown immediately and wait for the power to return
	 */
	if (!strcasecmp(cmdname, "shutdown.return"))
	{
		shutdown_return(1);
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Shutdown immediately and reboot after 1 minute
	 */
	if (!strcasecmp(cmdname, "shutdown.reboot"))
	{
		shutdown_reboot(1, 1);
		return STAT_INSTCMD_HANDLED;
	}

	/*
	 * Shutdown in 20 seconds and reboot after 1 minute
	 */
	if (!strcasecmp(cmdname, "shutdown.reboot.graceful"))
	{
		shutdown_reboot(20, 1);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	int	retry = 3;
	int	i;
	char	*v;

	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
	
	usleep(100000);

	/*
	 * Very crude hardware detection. If an UPS is attached, it will set DSR
	 * to 1. Bail out if it isn't.
	 */
	ioctl(upsfd, TIOCMGET, &i);
	if ((i & TIOCM_DSR) == 0)
	{
		fatalx("Serial cable problem or nothing attached to %s", device_path);
	}

	/*
	 * Initialize the serial interface of the UPS by sending the magic
	 * string. If it does not respond with a valid status reply,
	 * display an error message and give up.
	 */
	while (safenet_command("ZCADLIOPERJD\r"))
	{
		if (--retry) continue;

		fatalx("SafeNet protocol compatible UPS not found on %s", device_path);
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
 */
void upsdrv_updateinfo(void)
{
	char	command[] = "ZHDGFGDJELBC\r";
	int	i;

	/*
	 * Fill the command portion with random characters from the range
	 * [A...J].
	 */
	for (i = 1; i < 12; i++) command[i] = (random() % 10) + 'A';

	/*
	 * Find which character must be an 'L' and put it there.
	 */
	command[command[1]-'A'+2] = 'L';

	/*
	 * Do a status poll.
	 */
	if (safenet_command(command))
	{
		upsdebugx(1, "UPS serial FAIL");
		ser_comm_fail("Status read failed");
		dstate_datastale();
	}	
	else
	{
		upsdebugx(1, "UPS serial OK");
		ser_comm_good();
		dstate_dataok();
	}
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
	while (safenet_command("ZCADLIOPERJD\r"))
	{
		if (--retry) continue;

		fatalx("SafeNet protocol compatible UPS not found on %s", device_path);
	}

	/*
	 * Since the UPS will happily restart on battery, we must use a
	 * different shutdown command depending on the line status, so
	 * we need to check the status of the UPS here.
	 */
	if (ups.status.onbattery)
	{
		/*
		 * Kill the inverter and wait for the things to
		 * come.
		 */
		upslogx(LOG_NOTICE, "Shutdown and wait for the power to return");
		shutdown_return(1);
	}
	else
	{
		/*
		 * Apparently we're not running on battery right
		 * now. Let's cycle the UPS and try to prevent a
		 * lockup. After 1 minute the UPS reboots.
		 */
		upslogx(LOG_NOTICE, "Shutdown and reboot");
		shutdown_reboot(1, 1);
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
	experimental_driver = 0;
}

void upsdrv_initups(void)
{
	int dtr_bit = TIOCM_DTR;
	int rts_bit = TIOCM_RTS;

	/*
	 * Open and lock the serial port and set the speed to 1200 baud.
	 */
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B1200);

	/*
	 * Set DTR and clear RTS to provide power for the serial interface.
	 */
	ioctl(upsfd, TIOCMBIS, &dtr_bit);
	ioctl(upsfd, TIOCMBIC, &rts_bit);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
