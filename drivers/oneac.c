/*vim ts=4*/

/*
 * NUT Oneac EG and ON model specific drivers for UPS units using
 * the Oneac Advanced Interface.  If your UPS is equipped with the
 * Oneac Basic Interface, use the genericups driver
*/

/*
   Copyright (C) 2003  by Eric Lawson <elawson@inficad.com>

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

/* 28 November 2003.  Eric Lawson
 * More or less complete re-write for NUT 1.5.9 
 * This was somewhat easier than trying to beat the old driver code 
 * into submission
*/

#include "main.h"
#include "serial.h"
#include "oneac.h"

#define DRIVER_NAME	"Oneac EG/ON UPS driver"
#define DRIVER_VERSION	"0.51"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Eric Lawson <elawson@inficad.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define SECS 2		/*wait time*/
#define USEC 0		/*rest of wait time*/

void do_battery_test(void)
{
	char buffer[256];

	if (getval("testtime") == NULL)
		snprintf(buffer, 3, "%s", DEFAULT_BAT_TEST_TIME);
	else {
		snprintf(buffer, 3, "%s", getval("testtime"));

	/*the UPS wants this value to always be two characters long*/
	/*so put a zero in front of the string, if needed....      */
		if (strlen(buffer) < 2) {
			buffer[2] = '\0';
			buffer[1] = buffer[0];
			buffer[0] = '0';
		}
	}
	ser_send(upsfd,"%s%s%s",BAT_TEST_PREFIX,buffer,COMMAND_END);
}


/****************************************************************
 *below are the commands that are called by main (part of the   *
 *Above, are functions used only in this oneac driver           *
 ***************************************************************/

int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "test.failure.start")) {
		ser_send(upsfd,"%s%s",SIM_PWR_FAIL,COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start")) {
		do_battery_test();
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send(upsfd,"%s00%s",BAT_TEST_PREFIX,COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "reset.input.minmax")) {
		ser_send(upsfd,"%c%s",RESET_MIN_MAX, COMMAND_END);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}


void upsdrv_initinfo(void)
{
	int i;
	char buffer[256], buffer2[32];
	ser_flush_in(upsfd,"",0);
	ser_send(upsfd,"%c%s",GET_MFR,COMMAND_END);
	ser_get_line(upsfd, buffer, sizeof(buffer),ENDCHAR,IGNCHARS,SECS,USEC);
	if(strncmp(buffer,MFGR, sizeof(MFGR)))
		fatalx(EXIT_FAILURE, "Unable to connect to ONEAC UPS on %s\n",device_path);	
 
	dstate_setinfo("ups.mfr", "%s", buffer);
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("test.failure.start");
	dstate_addcmd("reset.input.minmax");


	upsh.instcmd = instcmd;

	/*set some stuff that shouldn't change after initialization*/
	/*this stuff is common to both the EG and ON family of UPS */

	/*firmware revision*/
	ser_send(upsfd,"%c%s", GET_VERSION, COMMAND_END);
	ser_get_line(upsfd, buffer, sizeof(buffer),ENDCHAR,IGNCHARS,SECS,USEC);
	dstate_setinfo("ups.firmware", "%.3s",buffer);

	/*nominal AC frequency setting --either 50 or 60*/
	ser_send(upsfd,"%c%s", GET_NOM_FREQ, COMMAND_END);
	ser_get_line(upsfd,buffer,sizeof(buffer),ENDCHAR,IGNCHARS,SECS,USEC);
	dstate_setinfo("input.frequency", "%.2s", buffer);


	/*UPS Model (either ON, or EG series of UPS)*/
	
	ser_send(upsfd,"%c%s", GET_FAMILY,COMMAND_END);
	ser_get_line(upsfd,buffer,sizeof(buffer),ENDCHAR,IGNCHARS,SECS,USEC);
	dstate_setinfo("ups.model", "%.2s",buffer);
	printf("Found %.2s family of Oneac UPS\n", buffer);

	if ((strncmp(buffer,FAMILY_ON,2) != 0 && 
		 strncmp(buffer,FAMILY_ON_EXT,2) != 0) || 
		strncmp(buffer,FAMILY_EG,2) == 0) 
		printf("Unknown family of UPS. Assuming EG capabilities.\n");

	/*Get the actual model string for ON UPS reported as OZ family*/
	if (strncmp (dstate_getinfo("ups.model"), FAMILY_ON_EXT, 2) == 0) {
		ser_flush_in(upsfd,"",0);
		ser_send(upsfd,"%c%s",GET_ALL_EXT_2,COMMAND_END);
		ser_get_line(upsfd, buffer, sizeof(buffer),ENDCHAR,IGNCHARS,SECS,USEC);

		/*UPS Model (full string)*/
		memset(buffer2, '\0', 32);
		strncpy(buffer2,&buffer[5], 10);
		for (i = 9; i >= 0 && buffer2[i] == ' '; --i) {
			buffer2[i] = '\0';
		}

		dstate_setinfo("ups.model", "%s", buffer2);
		printf("Found %.10s UPS\n", buffer2);
	}

	/*The ON (OZ) series of UPS supports more stuff than does the EG.
 	*Take care of the ON (OZ) only stuff here
	*/
	if(strncmp (dstate_getinfo("ups.model"), FAMILY_ON, 2) == 0 ||
	   strncmp (dstate_getinfo("ups.model"), FAMILY_ON_EXT, 2) == 0) {
		/*now set the ON specific "static" parameters*/

			/*nominal input voltage*/

		ser_send(upsfd,"%c%s",GET_NOM_VOLTAGE,COMMAND_END);
		ser_get_line(upsfd,buffer,sizeof(buffer),ENDCHAR,IGNCHARS,SECS,USEC);

		switch (buffer[0]) {
			case V120AC:
				dstate_setinfo("output.voltage.nominal", 
					"120");
				break;

			case V230AC:
				dstate_setinfo("output.voltage.nominal", 
					"240");
				break;

			default:
				upslogx(LOG_INFO,"Oneac: "
					"Invalid voltage parameter from UPS");
		}
	}
}

void upsdrv_updateinfo(void)
{
	char buffer[256];
	int ret_value;


	ser_flush_in(upsfd,"",0);  /*just in case*/
	ser_send (upsfd,"%c%s",GET_ALL,COMMAND_END);
	ret_value = ser_get_line(upsfd,buffer,sizeof(buffer),ENDCHAR,
			IGNCHARS,SECS,USEC);
	
	upsdebugx (2,"upsrecv_updateinfo: upsrecv returned: %s\n",buffer);
	if (ret_value < 1)
	{
		ser_comm_fail("Oneac UPS Comm failure on port %s",device_path);
		dstate_datastale();
	}
	else
	{
		status_init();
		/*take care of the UPS status information*/
		switch (buffer[12]) {
			case NORMAL :
				status_set("OL");
				break;
			case ON_BAT_LOW_LINE :
			case ON_BAT_HI_LINE  :
				status_set("OB");
				break;
			case LO_BAT_LOW_LINE :
			case LO_BAT_HI_LINE  :
				status_set("OB LB");
				break;
			case TOO_HOT :
				status_set("OVER OB LB");
				break;
			case FIX_ME :
				dstate_setinfo("ups.test.result","UPS Internal Failure");
				break;
			case BAD_BAT :
				status_set("RB");
				break;
			default :				/*cry for attention, fake a status*/
									/*Would another status be better?*/
				upslogx (LOG_ERR, "Oneac: Unknown UPS status");
				status_set("OL");
		}

		/*take care of the reason why the UPS last transfered to battery*/
		switch (buffer[13]) {
			case XFER_BLACKOUT :
				dstate_setinfo("input.transfer.reason",	"Blackout");
				break;
			case XFER_LOW_VOLT :
				dstate_setinfo("input.transfer.reason",
					"Low Input Voltage");
				break;
			case XFER_HI_VOLT :
				dstate_setinfo("input.transfer.reason",
					"High Input Voltage");
				break;
			case NO_VALUE_YET :
				dstate_setinfo("input.transfer.reason", 
					"No transfer yet.");
				break;
			default :
				upslogx(LOG_INFO,"Oneac: Unknown reason for UPS battery"
				" transfer");
		}
		/* now update info for only the ON family of UPS*/

		if (strncmp(dstate_getinfo("ups.model"), FAMILY_ON, 2) == 0) {
			dstate_setinfo("ups.load", "0%.2s",buffer+31);

			/*battery charge*/
			if(buffer[10] == YES)
				dstate_setinfo("battery.charge", "0%.2s",buffer+33);
			else dstate_setinfo("battery.charge", "100");

			dstate_setinfo("input.voltage", "%.3s",buffer+35);
			dstate_setinfo("input.voltage.minimum", "%.3s",buffer+38);
			dstate_setinfo("input.voltage.maximum", "%.3s",buffer+41);
			dstate_setinfo("output.voltage", "%.3s",buffer+44);
			if (buffer[47] == YES) status_set("BOOST");
		}
		status_commit();
		dstate_dataok();
		ser_comm_good();
	}
}
void upsdrv_shutdown(void)
{
	ser_send(upsfd,"%s",SHUTDOWN);
}


void upsdrv_help(void)
{
	printf("\n---------\nNOTE:\n");
	printf("You must set the UPS interface card DIP switch to 9600BPS\n");
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE,"testtime",
		"Change battery test time from 2 minute default.");
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);

/*get the UPS in the right frame of mind*/
	
	ser_send(upsfd,"%s", COMMAND_END);
	sleep (1);
	ser_send(upsfd,"%s", COMMAND_END);
	sleep (1);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
