/*vim ts=4*/
/* powervar-cs.c - Serial driver for Powervar UPM UPSs using CUSPP.
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
#include "nut_stdint.h"

/* Prototypes to allow setting pointer before function is defined */
int setcmd(const char* varname, const char* setvalue);
int instcmd(const char *cmdname, const char *extra);
static size_t SendRequest (const char* sRequest);
static ssize_t PowervarGetResponse (char* chBuff, const size_t BuffSize);

/* Two drivers include the following. Provide some identifier for differences */
#define PVAR_SERIAL	1	/* This is the serial comm driver */
#include "powervar-cx.h"	/* Common driver defines, variables, and functions */

#define DRIVER_NAME	"Powervar-CS UPS driver"
#define DRIVER_VERSION	"0.04"

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

/* This function is called to send the request to the initialized device. */
static size_t SendRequest (const char* sRequest)
{
	ssize_t Ret;

	Ret = ser_send(upsfd, "%s%c", sRequest, ENDCHAR);

	return (size_t)Ret;
}

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
 * Below are the primary commands that are called by NUT main   *
 ***************************************************************/

void upsdrv_initups(void)
{
	uint32_t ulBaud;

	/* Serial comm init here */
	upsfd = ser_open(device_path);

	/* See if there is a custom baudrate available -- generally not */
	if (getval("pvbaud") == NULL)
	{
		printf ("Setting baud to 9600.\n");
		ser_set_speed(upsfd, device_path, B9600);

		upsdebugx (4,"Serial baud set to 9600.");
	}
	else
	{
		/* Custom firmware is needed to allow for other baud rates */
		ulBaud = atoi(getval("pvbaud"));

		if (ulBaud == 38400)
		{
			printf ("Setting baud to 38400.\n");
			ser_set_speed(upsfd, device_path, B38400);
			upsdebugx (4,"Serial baud set to 38400.");
		}
		else if (ulBaud == 57600)
		{
			printf ("Setting baud to 57600.\n");
			ser_set_speed(upsfd, device_path, B57600);
			upsdebugx (4,"Serial baud set to 57600.");
		}
		else if (ulBaud == 115200)	/* The only other baud known to be available. */
		{
			printf ("Setting baud to 115200.\n");
			ser_set_speed(upsfd, device_path, B115200);
			upsdebugx (4,"Serial baud set to 115200.");
		}
		else
		{
			upsdebugx (4,"Serial baud not set!! (%d).", ulBaud);
		}
	}

	/*get the UPS in the right frame of mind*/
	ser_send_pace(upsfd, 100, "%s", COMMAND_END);
	ser_send_pace(upsfd, 100, "%s", COMMAND_END);
	sleep (1);
}

/* This function is called on driver startup to initialize variables/commands */
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

/* End of powervar-cs.c file */
