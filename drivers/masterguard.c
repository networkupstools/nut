/* masterguard.c - support for Masterguard models

   Copyright (C) 2001 Michael Spanier <mail@michael-spanier.de>

   masterguard.c created on 15.8.2001

   OBSOLETION WARNING: Please to not base new development on this
   codebase, instead create a new subdriver for nutdrv_qx which
   generally covers all Megatec/Qx protocol family and aggregates
   device support from such legacy drivers over time.

   FIXME: `if(DEBUG) print(...)` ==> `upsdebugx()`

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

#include "main.h"
#include "serial.h"
#include "nut_stdint.h"

#define DRIVER_NAME	"MASTERGUARD UPS driver"
#define DRIVER_VERSION	"0.28"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Michael Spanier <mail@michael-spanier.de>",
	DRV_STABLE,
	{ NULL }
};

#define UPSDELAY 3
#define MAXTRIES 10
#define UPS_PACE 100	/* 100 us between chars on write */

#define Q1  1
#define Q3  2

#define DEBUG 1

static int     type;
static char    name[31];
static char    firmware[6];

/* Forward decls */
static int instcmd(const char *cmdname, const char *extra);

/********************************************************************
 *
 * Helper function to split a sting into words by splitting at the
 * SPACE character.
 *
 * Adds up to maxlen characters to the char word.
 * Returns NULL on reaching the end of the string.
 *
 ********************************************************************/
static char *StringSplit( char *source, char *word, size_t maxlen )
{
	size_t  i;
	size_t  len;
	size_t  wc=0;

	word[0] = '\0';
	len = strlen( source );
	for( i = 0; i < len && wc < maxlen; i++ )
	{
		if( source[i] == ' ' )
		{
			word[wc] = '\0';
			return source + i + 1;
		}
		word[wc] = source[i];
		wc++;
	}
	word[wc] = '\0';
	return NULL;
}

/********************************************************************
 *
 * Helper function to drop all whitespaces within a string.
 *
 * "word" must be large enought to hold "source", for the worst case
 * "word" has to be exacly the size of "source".
 *
 ********************************************************************/
static void StringStrip( char *source, char *word )
{
	size_t  wc=0;
	size_t  i;
	size_t  len;

	word[0] = '\0';
	len = strlen( source );
	for( i = 0; i < len; i++ )
	{
		if( source[i] == ' ' )
			continue;
		if( source[i] == '\n' )
			continue;
		if( source[i] == '\t' )
			continue;
		word[wc] = source[i];
		wc++;
	}
	word[wc] = '\0';
}

/********************************************************************
 *
 * Function parses the status flags which occure in the Q1 and Q3
 * command. Sets the INFO_STATUS value ( OL, OB, ... )
 *
 ********************************************************************/
static void parseFlags( char *flags )
{
	status_init();

	if( flags[0] == '1' )
		status_set("OB");
	else
		status_set("OL");

	if( flags[1] == '1' )
		status_set("LB");

	if( flags[2] == '1' )
		status_set("BOOST");

/* this has no mapping */
#if 0
	if( flags[3] == '1' )
			setinfo( INFO_ALRM_GENERAL, "1" );
#endif

#if 0
	/* and these are... ? */

	if( flags[5] == '1' )
		status_set("TIP");
	if( flags[6] == '1' )
		status_set("SD");
#endif

	status_commit();

	if( DEBUG )
		printf( "Status is %s\n", dstate_getinfo("ups.status"));
}

/********************************************************************
 *
 * Function parses the response of the query1 ( "Q1" ) command.
 * Also sets various values (IPFreq ... )
 *
 ********************************************************************/
static void query1( char *buf )
{
#	define WORDMAXLEN 255
	char    value[WORDMAXLEN];
	char    word[WORDMAXLEN];
	char    *newPOS;
	char    *oldPOS;
	int     count = 0;

	if( DEBUG )
		printf( "Q1 Buffer is : %s\n" , buf + 1 );
	oldPOS = buf + 1;
	newPOS = oldPOS;

	do
	{
		newPOS = StringSplit( oldPOS, word, WORDMAXLEN );
		StringStrip( word, value);
		oldPOS = newPOS;

		if( DEBUG )
		{
			printf( "value=%s\n", value );
			fflush( stdout );
		}
		switch( count )
		{
			case  0:
				/* IP Voltage */
				dstate_setinfo("input.voltage", "%s", value );
				break;
			case  1:
				/* IP Fault Voltage */
				break;
			case  2:
				/* OP Voltage */
				dstate_setinfo("output.voltage", "%s", value);
				break;
			case  3:
				/* OP Load*/
				dstate_setinfo("ups.load", "%s", value );
				break;
			case  4:
				/* IP Frequency */
				dstate_setinfo("input.frequency", "%s", value);
				break;
			case  5:
				/* Battery Cell Voltage */
				dstate_setinfo("battery.voltage", "%s", value);
				break;
			case  6:
				/* UPS Temperature */
				dstate_setinfo("ups.temperature", "%s", value );
				break;
			case  7:
				/* Flags */
				parseFlags( value );
				break;
			default:
				/* Should never be reached */
				break;
		}
		count ++;
		oldPOS = newPOS;
	}
	while( newPOS != NULL );
}

/********************************************************************
 *
 * Function parses the response of the query3 ( "Q3" ) command.
 * Also sets various values (IPFreq ... )
 *
 ********************************************************************/
static void query3( char *buf )
{
#	define WORDMAXLEN 255
	char    value[WORDMAXLEN];
	char    word[WORDMAXLEN];
	char    *newPOS;
	char    *oldPOS;
	int     count = 0;

	if( DEBUG )
		printf( "Q3 Buffer is : %s\n" , buf+1 );
	oldPOS = buf + 1;
	newPOS = oldPOS;

	do
	{
		newPOS = StringSplit( oldPOS, word, WORDMAXLEN );
		StringStrip( word, value);
		oldPOS = newPOS;

		/* Shortcut */
		if( newPOS == NULL )
			break;

		if( DEBUG )
		{
			printf( "value=%s\n", value );
			fflush( stdout );
		}
		switch( count )
		{
			case  0:
				/* UPS ID */
				break;
			case  1:
				/* Input Voltage */
				dstate_setinfo("input.voltage", "%s", value );
				break;
			case  2:
				/* Input Fault Voltage */
				break;
			case  3:
				/* Output Voltage */
				dstate_setinfo("output.voltage", "%s", value);
				break;
			case  4:
				/* Output Current */
				dstate_setinfo("output.current", "%s", value );
				break;
			case  5:
				/* Input Frequency */
				dstate_setinfo("input.frequency", "%s", value);
				break;
			case  6:
				/* Battery Cell Voltage */
				dstate_setinfo("battery.voltage", "%s", value);
				break;
			case  7:
				/* Temperature */
				dstate_setinfo("ups.temperature", "%s", value );
				break;
			case  8:
				/* Estimated Runtime */
				dstate_setinfo("battery.runtime", "%s", value);
				break;
			case  9:
				/* Charge Status */
				dstate_setinfo("battery.charge", "%s", value);
				break;
			case 10:
				/* Flags */
				parseFlags( value );
				break;
			case 11:
				/* Flags2 */
				break;
			default:
				/* This should never be reached */
				/* printf( "DEFAULT\n" ); */
				break;
		}
		count ++;
		oldPOS = newPOS;
	}
	while( newPOS != NULL );
}

/********************************************************************
 *
 * Function to parse the WhoAmI response of the UPS. Also sets the
 * values of the firmware version and the UPS identification.
 *
 ********************************************************************/
static void parseWH( char *buf )
{
	strncpy( name, buf + 16, 30 );
	name[30] = '\0';
	strncpy( firmware, buf + 4, 5 );
	firmware[5] = '\0';
	if( DEBUG )
		printf( "Name = %s, Firmware Version = %s\n", name, firmware );
}

/********************************************************************
 *
 * Function to parse the old and possible broken WhoAmI response
 * and set the values for the firmware Version and the identification
 * of the UPS.
 *
 ********************************************************************/
static void parseOldWH( char *buf )
{
	strncpy( name, buf + 4, 12 );
	name[12] = '\0';
	strncpy( firmware, buf, 4 );
	firmware[4] = '\0';
	if( DEBUG )
		printf( "Name = %s, Firmware Version = %s\n", name, firmware );
}

/********************************************************************
 *
 * Function to fake a WhoAmI response of a UPS that returns NAK.
 *
 ********************************************************************/
static void fakeWH(void)
{
	strcpy( name, "GenericUPS" );
	strcpy( firmware, "unkn" );
	if( DEBUG )
		printf( "Name = %s, Firmware Version = %s\n", name, firmware );
}

static ssize_t ups_ident( void )
{
	char    buf[255];
	ssize_t ret;

	/* Check presence of Q1 */
	ret = ser_send_pace(upsfd, UPS_PACE, "%s", "Q1\x0D" );
	ret = ser_get_line(upsfd, buf, sizeof(buf), '\r', "", 3, 0);
	ret = (ssize_t)strlen( buf );
	if( ret != 46 )
	{
		/* No Q1 response found */
		type   = 0;
		return -1;
	}
	else
	{
		if( DEBUG )
			printf( "Found Q1\n" );
		type = Q1;
	}

	/* Check presence of Q3 */
	ret = ser_send_pace(upsfd, UPS_PACE, "%s", "Q3\x0D" );
	ret = ser_get_line(upsfd, buf, sizeof(buf), '\r', "", 3, 0);
	ret = (ssize_t)strlen( buf );
	if( ret == 70 )
	{
		if( DEBUG )
			printf( "Found Q3\n" );
		type = Q1 | Q3;
	}

	/* Check presence of WH ( Who am I ) */
	ret = ser_send_pace(upsfd, UPS_PACE, "%s", "WH\x0D" );
	ret = ser_get_line(upsfd, buf, sizeof(buf), '\r', "", 3, 0);
	ret = (ssize_t)strlen( buf );
	if( ret == 112 )
	{
		if( DEBUG )
			printf( "WH found\n" );
		parseWH( buf );
	}
	else if( ret == 53 )
	{
		if( DEBUG )
			printf( "Old (broken) WH found\n" );
		parseOldWH( buf );
	}
	else if( ret == 3 && strcmp(buf, "NAK") == 0 )
	{
		if( DEBUG )
			printf( "WH was NAKed\n" );
		fakeWH( );
	}
	else if( ret > 0 )
	{
		if( DEBUG )
			printf( "WH says <%s> with length %" PRIiSIZE "\n", buf, ret );
		upslog_with_errno( LOG_INFO,
			"New WH String found. Please report to maintainer\n" );
	}
	return 1;
}

/********************************************************************
 *
 *
 *
 *
 ********************************************************************/
void upsdrv_help( void )
{

}

/********************************************************************
 *
 * Function to initialize the fields of the ups driver.
 *
 ********************************************************************/
void upsdrv_initinfo(void)
{
	dstate_setinfo("ups.mfr", "MASTERGUARD");
	dstate_setinfo("ups.model", "unknown");

	/*
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("test.battery.start");
	*/

	dstate_addcmd("shutdown.return");

	/* install handlers */
	upsh.instcmd = instcmd;

	if( strlen( name ) > 0 )
		dstate_setinfo("ups.model", "%s", name);
	if( strlen( firmware ) > 0 )
		dstate_setinfo("ups.firmware", "%s", firmware);
}

/********************************************************************
 *
 * This is the main function. It gets called if the driver wants
 * to update the ups status and the information.
 *
 ********************************************************************/
void upsdrv_updateinfo(void)
{
	char    buf[255];
	ssize_t ret;
	int     lenRSP=0;

	if( DEBUG )
		printf( "update info\n" );

	/* Q3 found ? */
	if( type & Q3 )
	{
		ser_send_pace(upsfd, UPS_PACE, "%s", "Q3\x0D" );
		lenRSP = 70;
	}
	/* Q1 found ? */
	else if( type & Q1 )
	{
		ser_send_pace(upsfd, UPS_PACE, "%s", "Q1\x0D" );
		lenRSP = 46;
	}
	/* Should never be reached */
	else
	{
		fatalx(EXIT_FAILURE, "Error, no Query mode defined. Please file bug against driver.");
	}

	sleep( UPSDELAY );

	buf[0] = '\0';
	ret = ser_get_line(upsfd, buf, sizeof(buf), '\r', "", 3, 0);
	ret = (ssize_t)strlen( buf );

	if( ret != lenRSP )
	{
		if( DEBUG )
			printf( "buf = %s len = %" PRIiSIZE "\n", buf, ret );
		upslog_with_errno( LOG_ERR, "Error in UPS response " );
		dstate_datastale();
		return;
	}

	/* Parse the response from the UPS */
	if( type & Q3 )
	{
		query3( buf );
		dstate_dataok();
		return;
	}
	if( type & Q1 )
	{
		query1( buf );
		dstate_dataok();
		return;
	}
}

/* handler for commands to be sent to UPS */
static
int instcmd(const char *cmdname, const char *extra)
{
	NUT_UNUSED_VARIABLE(extra);

	/* Shutdown UPS */
	if (!strcasecmp(cmdname, "shutdown.return"))
	{
		/* ups will come up within a minute if utility is restored */
		ser_send_pace(upsfd, UPS_PACE, "%s", "S.2R0001\x0D" );

		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

/********************************************************************
 *
 * Called if the driver wants to shutdown the UPS.
 * ( also used by the "-k" command line switch )
 *
 * This cuts the utility from the UPS after 20 seconds and restores
 * the utility one minute _after_ the utility to the UPS has restored
 *
 ********************************************************************/
void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	int	ret = do_loop_shutdown_commands("shutdown.return", NULL);
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(ret == STAT_INSTCMD_HANDLED ? EF_EXIT_SUCCESS : EF_EXIT_FAILURE);
}

/********************************************************************
 *
 * Populate the command line switches.
 *
 * CS:  Cancel the shutdown process
 *
 ********************************************************************/
void upsdrv_makevartable(void)
{
	addvar( VAR_FLAG, "CS", "Cancel Shutdown" );
}

/********************************************************************
 *
 * This is the first function called by the UPS driver.
 * Detects the UPS and handles the command line args.
 *
 ********************************************************************/
void upsdrv_initups(void)
{
	int     count = 0;
	int     fail  = 0;
	int     good  = 0;

	upsdebugx(0,
		"Please note that this driver is deprecated and will not receive\n"
		"new development. If it works for managing your devices - fine,\n"
		"but if you are running it to try setting up a new device, please\n"
		"consider the newer nutdrv_qx instead, which should handle all 'Qx'\n"
		"protocol variants for NUT. (Please also report if your device works\n"
		"with this driver, but nutdrv_qx would not actually support it with\n"
		"any subdriver!)\n");

	/* setup serial port */
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);

	name[0] = '\0';
	firmware[0] = '\0';

	/* probe ups type */
	do
	{
		count++;

		if( ups_ident( ) != 1 )
			fail++;
		/* at least two good identifications */
		if( (count - fail) == 2 )
		{
			good = 1;
			break;
		}
	} while( (count<MAXTRIES) | (good) );

	if( ! good )
	{
		fatalx(EXIT_FAILURE,  "No MASTERGUARD UPS found" );
	}

	upslogx(LOG_INFO, "MASTERGUARD UPS found\n" );

	/* Cancel Shutdown */
	if( testvar("CS") )
	{
		ser_send_pace(upsfd, UPS_PACE, "%s", "C\x0D" );
		fatalx(EXIT_FAILURE, "Shutdown cancelled");
	}
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
