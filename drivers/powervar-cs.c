/*vim ts=4*/
/* powervar-c.c - Serial driver for Powervar UPM UPSs using CUSPP.
 *
 * Supported Powervar UPS families in this driver:
 * UPM (All)
 *
 * Copyright (C)
 *     2024 by Bill Elliot <bill@wreassoc.com>
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
 * History:
 * - 2 December 2024.  Bill Elliot
 * Started to work on common variable and function file so this driver
 *  and the USB driver can make use of the same protocol related functions.
 * - 2 October 2024.  Bill Elliot
 * Used pieces of oneac.c driver to get jump-started.
 *
 */

#include "main.h"
#include "serial.h"
//#include "powervar-c.h"
#include "nut_stdint.h"

/* Prototypes to allow setting pointer before function is defined */
int setcmd(const char* varname, const char* setvalue);
int instcmd(const char *cmdname, const char *extra);
static ssize_t PowervarGetResponse (char* chBuff, const size_t BuffSize);

/* Two drivers include the following. Provide some identifier for differences */
#define PVAR_SERIAL	1	/* This is the serial comm driver */
#include "powervar-cx.h"	/* Common driver defines, variables, and functions */

#define DRIVER_NAME	"Powervar-CS UPS driver"
#define DRIVER_VERSION	"0.02"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Bill Elliot <bill@wreassoc.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

/* Serial comm stuff here */
#define SECS 0			/* Serial function wait time*/
#define USEC 500000		/* Rest of serial function wait time*/

#define COMM_TRIES	3	/* Serial retries before "stale" */


/*******************************************
 * Serial response communication functions *
 *******************************************/

/* Since an installed network card may slightly delay responses from
 *  the UPS allow for a repeat of the get request.
 */
#define RETRIES 4
static ssize_t PowervarGetResponse (char* chBuff, const size_t BuffSize)
{
	int Retries = RETRIES;		/* x/2 seconds max with 500000 USEC */
	ssize_t return_val;

	do
	{
		return_val = ser_get_line(upsfd, chBuff, BuffSize, ENDCHAR, IGNCHARS, SECS, USEC);

		if (return_val > 0)
		{
			break;
		}

		upsdebugx (3, "!PowervarGetResponse retry (%" PRIiSIZE ", %d)...", return_val, Retries);

	} while (--Retries > 0);

	upsdebugx (4,"PowervarGetResponse buffer: %s",chBuff);

	if (Retries == 0)
	{
		upsdebugx (2,"!!PowervarGetResponse timeout...");
		return_val = 1;					/* Comms error */
	}
	else
	{
		if (Retries < RETRIES)
		{
			upsdebugx (2,"PowervarGetResponse recovered (%d)...", Retries);
		}

		return_val = 0;					/* Good comms */
	}

	return return_val;
}


/****************************************************************
 * Below are the primary commands that are called by main       *
 ***************************************************************/

void upsdrv_initups(void)
{
	/* Serial comm init here */
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);

	/*get the UPS in the right frame of mind*/
	ser_send_pace(upsfd, 100, "%s", COMMAND_END);
	ser_send_pace(upsfd, 100, "%s", COMMAND_END);
	sleep (1);
}

/* TBD, Implement commands based on capability of the found UPS. */
/* TBD, Finish implementation of available data */
void upsdrv_initinfo(void)
{
	/* Get serial port ready */
	ser_flush_in(upsfd,"",0);

	PvarCommon_Initinfo ();
}


/* This function is called regularly for data updates. */
void upsdrv_updateinfo(void)
{
	/* Get serial port ready */
	ser_flush_in(upsfd,"",0);

	PvarCommon_Updateinfo ();

	ser_comm_good();
}


/**********************************************************
 * Powervar support functions for NUT command calls       *
 *********************************************************/

static void do_battery_test(void)
{
	if (byTSTBatrunPos)
	{
		char buffer[32];

		if (getval("battesttime") == NULL)
		{
			snprintf(buffer, 3, "%s", DEFAULT_BAT_TEST_TIME);
		}
		else
		{
			snprintf(buffer, 6, "%s", getval("battesttime"));
		}

		ser_send(upsfd, "%s%s%s", TST_BATRUN_REQ, buffer, COMMAND_END);
	}
}

/**************************************
 * Handlers for NUT command calls     *
 *************************************/

void upsdrv_help(void)
{
	printf("\n---------\nNOTE:\n");
	printf("You must set the UPS interface card DIP switch to 9600 BPS\n");
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "battesttime", "Change battery test time from the 10 second default.");

	addvar(VAR_VALUE, "disptesttime", "Change display test time from the 10 second default.");

	addvar(VAR_VALUE, "offdelay", "Change shutdown delay time from 0 second default.");
}

void upsdrv_shutdown(void)
{
//	ser_send(upsfd, "%s", SHUTDOWN);
}

int instcmd(const char *cmdname, const char *extra)
{
//	int i;
	char buffer [10];

	upsdebugx(2, "In instcmd with %s and extra %s.", cmdname, extra);

	if (!strcasecmp(cmdname, "test.battery.start.quick"))
	{
		if (byTSTBatqckPos)
		{
			do_battery_test();
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "test.battery.start.deep"))
	{
		if (byTSTBatdepPos)
		{
			ser_send(upsfd, "%s%s", TST_BATDEP_REQ, COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "test.battery.stop"))
	{
		if (byTSTAbortPos)
		{
			ser_send(upsfd, "%s%s", TST_ABORT_REQ, COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "reset.input.minmax"))
	{
		if (bySETRstinpPos)
		{
			ser_send(upsfd, "%s%s", SET_RSTINP_REQ, COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "beeper.enable"))
	{
		if (bySETAudiblPos)
		{
			ser_send(upsfd, "%s%c%s", SET_AUDIBL_REQ, '1', COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "beeper.disable"))
	{
		if (bySETAudiblPos)
		{
			ser_send(upsfd, "%s%c%s", SET_AUDIBL_REQ, '0', COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "beeper.mute"))
	{
		if (bySETAudiblPos)
		{
			ser_send(upsfd, "%s%c%s", SET_AUDIBL_REQ, '2', COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	if (!strcasecmp(cmdname, "test.panel.start"))
	{
		if (byTSTDispPos)
		{

			if (getval("disptesttime") == NULL)
			{
				snprintf(buffer, 2, "1");
			}
			else
			{
				snprintf(buffer, 4, "%s", getval("disptesttime"));
			}

			ser_send(upsfd,"%s%s%s", TST_DISP_REQ, buffer, COMMAND_END);
			return STAT_INSTCMD_HANDLED;
		}
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}


int setcmd(const char* varname, const char* setvalue)
{
	upsdebugx(2, "In setcmd for %s with %s...", varname, setvalue);



	upslogx(LOG_NOTICE, "setcmd: unknown command [%s]", varname);

	return STAT_SET_UNKNOWN;
}
