/*vim ts=4*/
/* powervar-c.c - Driver for Powervar UPM UPS using CUSPP.
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
 * - 2 October 2024.  Bill Elliot
 * Used pieces of oneac.c driver to get jump-started.
 *
 */

#include "main.h"
#include "nut_stdint.h"
#include "nut_libusb.h"

#include "usb-common.h"


/* Prototypes to allow setting pointer before function is defined */
int setcmd(const char* varname, const char* setvalue);
int instcmd(const char *cmdname, const char *extra);
static int send_cmd(const unsigned char *msg, size_t msg_len, unsigned char *reply, size_t reply_len);
static ssize_t PowervarGetResponse (char* chBuff, const size_t BuffSize);

/* Two drivers include the following. Provide some identifier for differences */
#define PVAR_USB	1	/* This is the USB comm driver */
#include "powervar-cx.h"	/* Common driver variables and functions */

#define DRIVER_NAME	"Powervar-CU UPS driver"
#define DRIVER_VERSION	"0.02"

/* USB comm stuff here */
#define POWERVAR_VENDORID	0x4234

static USBDevice_t *hd = NULL;
static USBDevice_t curDevice;
static USBDeviceMatcher_t *reopen_matcher = NULL;
static USBDeviceMatcher_t *regex_matcher = NULL;
static usb_dev_handle *udev;
static usb_communication_subdriver_t *comm_driver = &usb_subdriver;

/* USB IDs device table */
static usb_device_id_t powervar_usb_device_table[] = {
	/* GTS ... */
	{ USB_DEVICE(POWERVAR_VENDORID, 0x0001), NULL },

	/* UPM ... */
	{ USB_DEVICE(POWERVAR_VENDORID, 0x0002), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};

static int subdriver_match_func(USBDevice_t *arghd, void *privdata)
{
	NUT_UNUSED_VARIABLE(privdata);

	/* FIXME? Should we save "arghd" into global "hd" variable?
	 * This was previously shadowed by function argument named "hd"...
	 */
	/* hd = arghd; */

	printf ("In subdriver_match_func...");

	switch (is_usb_device_supported(powervar_usb_device_table, arghd))
	{
		case SUPPORTED:
			printf ("SUPPORTED\n");
			return 1;

		case POSSIBLY_SUPPORTED:
			printf ("POSSIBLY_SUPPORTED\n");
			return 0;

		case NOT_SUPPORTED:
			printf ("NOT_SUPPORTED\n");
			return 0;
		default:
			return 0;
	}
}


static USBDeviceMatcher_t subdriver_matcher = {
	&subdriver_match_func,
	NULL,
	NULL
};

/* static enum pvar_model_t {
	PVAR_UNDEFINED = 0,
	PVAR_GTS,
	PVAR_UPM,
*/
//	PVAR_DYNAMIC			/* If we can define one in the config file */
/*
} pvar_model = PVAR_UNDEFINED;
 */


/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Bill Elliot <bill@wreassoc.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};


/* USB comm stuff here */
#define MAX_SEND_TRIES 10
//#define SEND_WAIT_SEC 0
//#define SEND_WAIT_NSEC (1000*1000*100)

#define MAX_RECV_TRIES 10




int match_by_something(usb_dev_handle *argudev, USBDevice_t *arghd, usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen)
{
	#define BIG_BUFFER	512

	int i, ret = 0;
	unsigned char reply[BIG_BUFFER];

	NUT_UNUSED_VARIABLE(argudev);
	NUT_UNUSED_VARIABLE(arghd);
	NUT_UNUSED_VARIABLE(rdbuf);
	NUT_UNUSED_VARIABLE(rdlen);
//	usb_ctrl_charbufsize sz;

	printf ("In 'match_by_something'\n");

	ret = send_cmd((unsigned char*)"PID", 3, reply, BIG_BUFFER);

	for (i = 0; i < BIG_BUFFER ; i++)
	{
		if (reply[i] == ENDCHAR)
		{
			break;
		}
	}

	reply[i] = 0;
	reply[i+1] = 0;		/* Force trailing null? */
	upsdebugx(3, "'send_cmd' returned '%s', with ret: %d", reply, ret);
	reply[i] = ENDCHAR;

	if (GetSubstringPosition ((const char*)reply, "CUSPP") == 1)
	{
		upsdebugx(3, "UPM CUSPP device found!");
	}


	/* Unit ID might not be supported by all models: */
/*	ret = send_cmd(u_msg, sizeof(u_msg), u_value, sizeof(u_value) - 1);
	if (ret <= 0) {
		upslogx(LOG_INFO, "Unit ID not retrieved (not available on all models)");
	} else {
 */
		/* Translating from two bytes (unsigned chars), so via uint16_t */
/*		unit_id = (uint16_t)((uint16_t)(u_value[1]) << 8) | (uint16_t)(u_value[2]);
		upsdebugx(1, "Retrieved Unit ID: %ld", unit_id);
	}

 	if (rdlen)
	{
		printf ("Showing rdbuf...\n");

		for (sz = 0 ; sz <= rdlen ; sz++)
		{
			printf ("%c", rdbuf[0] > 32 ? rdbuf[0] : '.');
		}

		printf ("\nDone showing rdbuf.\n");
	}
 */

	return 1;


/* 	char *value = getval("upsid");
	long config_unit_id = 0;
	ssize_t ret;
	unsigned char u_msg[] = "U";
	unsigned char u_value[9];

	NUT_UNUSED_VARIABLE(argudev);
	NUT_UNUSED_VARIABLE(arghd);
	NUT_UNUSED_VARIABLE(rdbuf);
	NUT_UNUSED_VARIABLE(rdlen);
 */
	/* If upsid is not defined in the config, return 1 (null behavior - match any device),
	 * otherwise read it from the device and match against what was asked in ups.conf */
/* 	if (value == NULL) {
		return 1;
	} else {
		config_unit_id = atol(value);
	}
 */
	/* Read ups id from the device */
//	if (tl_model != TRIPP_LITE_OMNIVS && tl_model != TRIPP_LITE_SMART_0004) {
		/* Unit ID might not be supported by all models: */
/* 		ret = send_cmd(u_msg, sizeof(u_msg), u_value, sizeof(u_value) - 1);
		if (ret <= 0) {
			upslogx(LOG_INFO, "Unit ID not retrieved (not available on all models)");
		} else {
 */			/* Translating from two bytes (unsigned chars), so via uint16_t */
/* 			unit_id = (uint16_t)((uint16_t)(u_value[1]) << 8) | (uint16_t)(u_value[2]);
			upsdebugx(1, "Retrieved Unit ID: %ld", unit_id);
		}
	}

 */	/* Check if the ups ids match */
/* 	if (config_unit_id == unit_id) {
		upsdebugx(1, "Retrieved Unit ID (%ld) matches the configured one (%ld)",
			unit_id, config_unit_id);
		return 1;
	} else {
		upsdebugx(1, "Retrieved Unit ID (%ld) does not match the configured one (%ld). "
			"Do you have several compatible UPSes? Otherwise, please check if the ID "
			"was set in the previous life of your device (can use upsrw to set another"
			"value).", unit_id, config_unit_id);
		return 0;
	}
 */
}


/***********************************
 * USB communication functions     *
 **********************************/

/*!@brief Send a command to the UPS, and wait for a reply.
 *
 * All of the UPS commands are challenge-response. If a command does not have
 * anything to return, it simply returns the '?'' character.
 *
 * @param[in] msg	Command string, minus the ':' or CR
 * @param[in] msg_len	Use strlen(msg),
 * @param[out] reply	Reply (but check return code for validity)
 * @param[out] reply_len (currently unused)
 *
 * @return number of chars in reply, excluding terminating NUL
 * @return 0 if command was not accepted
 */

static int send_cmd(const unsigned char *msg, size_t msg_len, unsigned char *reply, size_t reply_len)
{
	#define USB_RESPONSE_SIZE	8

	unsigned char buffer_out[40];
	unsigned char response_in[USB_RESPONSE_SIZE + 2];
	int ret = 0, done = 0;
//	int recv_try = 0, send_try = 0;
	size_t i = 0;
	size_t j = 0;

	upsdebugx(3, "send_cmd(msg='%s', msg_len=%u)", msg, (unsigned)msg_len);

	/* Clear buffer areas */
	for(i = 0; i < 40; i++) buffer_out[i] = '\0';
	for(i = 0; i < reply_len; i++) reply[i] = '\0';
	for(i = 0; i < (USB_RESPONSE_SIZE + 2) ; i++) response_in[i] = '\0';

	/* Move command into USB buffer and add terminating character */
	for(i = 0; i < msg_len; i++)
	{
		buffer_out[i] = msg[i];
	}

	buffer_out[i] = ENDCHAR;


//	for(send_try=0; !done && send_try < MAX_SEND_TRIES; send_try++) {

//		upsdebugx(6, "send_cmd send_try %d", send_try+1);

		ret = comm_driver->set_report(udev, 0,
			(usb_ctrl_charbuf)buffer_out,
			(usb_ctrl_charbufsize)sizeof(buffer_out));

		upsdebugx(5, "set_report ret:   %d", ret);

		if(ret != sizeof(buffer_out)) {
			upslogx(1, "libusb_set_report() returned %d instead of %" PRIuSIZE,
				ret, sizeof(buffer_out));
// **Should be ok			return ret;
		}

#if ! defined(__FreeBSD__)
		usleep(1000*100); /* TODO: nanosleep */
#endif

/*  		for(recv_try=0; !done && recv_try < MAX_RECV_TRIES; recv_try++) {
			upsdebugx(7, "send_cmd recv_try %d", recv_try+1);
*/


		for (i = 0 ; (done == 0) && (i < reply_len) ; )
		{
			upsdebugx(5, "Receive Loop: %ld", i);

 			ret = comm_driver->get_interrupt(udev,
				(usb_ctrl_charbuf)response_in,
				(usb_ctrl_charbufsize) USB_RESPONSE_SIZE,
				(usb_ctrl_timeout_msec) 1000);

			upsdebugx(4, "Response: '%s'", response_in);

			if (ret > 0)
			{
				for (j=0 ; j < USB_RESPONSE_SIZE ; j++)
				{
					reply[i] = response_in[j];

					if (response_in[j] == ENDCHAR)
					{
						upsdebugx(5, "<CR> found.");
						done = 1;
						break;
					}

					i++;
				}

				/* Clear response buffer for next chunk */
				for(j = 0; j < USB_RESPONSE_SIZE ; j++) response_in[j] = '\0';
			}
			else
			{
				upsdebugx(5, "Unexpected return value: %d", ret);
				break;
			}
		}



/* 			ret = comm_driver->get_report(udev, 8,
				(usb_ctrl_charbuf)reply,
				(usb_ctrl_charbufsize) reply_len);
 */


/*			if(ret != sizeof(buffer_out)) {
				upslogx(1, "libusb_get_get_report() returned %d instead of %u while sending %s",
					ret, (unsigned)sizeof(buffer_out), buffer_out );
			}
			done = (ret == sizeof(buffer_out)) && (buffer_out[1] == reply[0]);
		}
 */
//	}

/* 	if(ret == sizeof(buffer_out)) {
		upsdebugx(5, "send_cmd: received %s (%s)", hexascdump(reply, sizeof(buffer_out)),
				done ? "OK" : "bad");
	}
 */
//	upsdebugx(((send_try > 2) || (recv_try > 2)) ? 3 : 6,
//			"send_cmd: send_try = %d, recv_try = %d\n", send_try, recv_try);

//	return done ? sizeof(buffer_out) : 0;

	return i;

}


/* TBD, make for USB */

#define RETRIES 4
static ssize_t PowervarGetResponse (char* chBuff, const size_t BuffSize)
{
	/* This is a stub only. Bulk below need to re-work for USB */
	NUT_UNUSED_VARIABLE(chBuff);
	NUT_UNUSED_VARIABLE(BuffSize);
	return 0;	/* Good comms */
}

/*	int Retries = RETRIES;
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
*/
//		return_val = 1;					/* Comms error */
 /* 	}
	else
	{
		if (Retries < RETRIES)
		{
			upsdebugx (2,"PowervarGetResponse recovered (%d)...", Retries);
		}
*/
//		return_val = 0;					/* Good comms */
/*	}

	return return_val;
}
 */

/***************************************
 * CUSPP string handling functions     *
 **************************************/


/* This function finds the position of a substring in a CUSPP Format response. */


/****************************************************************
 * Below are the primary commands that are called by main       *
 ***************************************************************/

void upsdrv_initups(void)
{
	upsdebugx(2, "In upsdrv_initups");

 	char *regex_array[USBMATCHER_REGEXP_ARRAY_LIMIT];
//	char *value;
	int r = 1;

	warn_if_bad_usb_port_filename(device_path);

	/* process the UPS selection options */
	regex_array[0] = NULL; /* handled by USB IDs device table */
	regex_array[1] = getval("productid");
//	regex_array[1] = NULL;
	regex_array[2] = getval("vendor"); /* vendor string */
	regex_array[3] = getval("product"); /* product string */
	regex_array[4] = getval("serial");
	regex_array[5] = NULL;
	regex_array[6] = getval("device");
	regex_array[7] = NULL;

	upsdebugx(3, "B4 RegexMatcher (%ld)", (long)regex_matcher);

	r = USBNewRegexMatcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
	if (r==-1) {
		fatal_with_errno(EXIT_FAILURE, "USBNewRegexMatcher");
	} else if (r) {
		fatalx(EXIT_FAILURE, "invalid regular expression: %s", regex_array[r]);
	}

	upsdebugx(3, "After RegexMatcher (%ld)", (long)regex_matcher);

	/* link the matchers */
	regex_matcher->next = &subdriver_matcher;

	upsdebugx(2, "Trying to open device...)");

	/* Search for the first supported UPS matching the regular expression */
	r = comm_driver->open_dev(&udev, &curDevice, regex_matcher, match_by_something);
	if (r < 1) {
		fatalx(EXIT_FAILURE, "No matching USB/HID UPS found");
	}

	hd = &curDevice;

	upslogx(1, "Detected a UPS: %s/%s", hd->Vendor ? hd->Vendor : "unknown", hd->Product ? hd->Product : "unknown");

//	hd = nutusb_open("USB");

	r = is_usb_device_supported (powervar_usb_device_table, hd);

	if (r < 0)
	{
		upsdebugx (3, "supported: r is less than 0.");
	}
	else if(r > 0)
	{
		upsdebugx (3, "supported: r is %d.", r);
	}

	upsdebugx(3, "VendorID: %04x", hd->VendorID);
	upsdebugx(3, "ProductID: %04x", hd->ProductID);

	/* create a new matcher for later reopening */
	r = USBNewExactMatcher(&reopen_matcher, hd);
	if (r) {
		fatal_with_errno(EXIT_FAILURE, "USBNewExactMatcher");
	}

	/* link the two matchers */
	reopen_matcher->next = regex_matcher;

	sleep (1);
}

/* TBD, Implement commands based on capability of the found UPS. */
/* TBD, Finish implementation of available data */
void upsdrv_initinfo(void)
{
//	int i, j, k;
//	int Vlts = 1;
//	char sFBuff[BUFFSIZE];
//	char sDBuff[BUFFSIZE];
//	char SubBuff[SUBBUFFSIZE];

	printf ("In upsdrv_initinfo\n");

//	fatalx(EXIT_FAILURE, "[%s] Not a UPS that handles CUSPP\n", "USB TEST");

	/* Keep moving this down below things that work and the next thing being tried */
//	comm_driver->close_dev(udev);
//	fatalx(EXIT_FAILURE, "[%s] USB Development exit.\n", "Move Me");


}

/*
	Stuff to follow up on realated to implementation for init

	 Shutdown delay in seconds...can be changed by user
	dstate_setinfo("ups.delay.shutdown", "%s", getval("offdelay"));

	dstate_setflags("ups.delay.shutdown", ST_FLAG_STRING | ST_FLAG_RW);
	dstate_setaux("ups.delay.shutdown", GET_SHUTDOWN_RESP_SIZE);

	 Low and high output trip points
	EliminateLeadingZeroes (sDBuff+73, 3, buffer2, sizeof(buffer2));
	dstate_setinfo("input.transfer.low", "%s", buffer2);
	dstate_setflags("input.transfer.low", ST_FLAG_STRING | ST_FLAG_RW );
	dstate_setaux("input.transfer.low", 3);

	EliminateLeadingZeroes (sDBuff+76, 3, buffer2, sizeof(buffer2));
	dstate_setinfo("input.transfer.high", "%s", buffer2);
	dstate_setflags("input.transfer.high", ST_FLAG_STRING | ST_FLAG_RW);
	dstate_setaux("input.transfer.high", 3);

	 Get output window min/max points from OB or OZ v1.9 or later

	i = atoi(buffer2);		Minimum voltage

	j = atoi(buffer2);		Maximum voltage

	strncpy(buffer2, sDBuff+8, 2);
	buffer2[2]='\0';
	k = atoi(buffer2);		Spread between

	dstate_setinfo("input.transfer.low.min", "%3d", i);
	dstate_setinfo("input.transfer.low.max", "%3d", j-k);
	dstate_setinfo("input.transfer.high.min", "%3d", i+k);
	dstate_setinfo("input.transfer.high.max", "%3d", j);
*/

/* This function is called regularly for data updates. */
void upsdrv_updateinfo(void)
{
//	char sData[BUFFSIZE];
//	char SubString[SUBBUFFSIZE];
//	uint8_t byOnBat = 0;		    /* Keep flag between OUT and BAT groups */
//	uint8_t byBadBat = 0;		    /* Keep flag for 'RB' logic */
//	char chC;			    /* Character being worked with */
//	int timevalue;

	printf ("In upsdrv_updateinfo\n");

	/* Keep moving this down below things that work and the next thing being tried */
	fatalx(EXIT_FAILURE, "[%s] USB Development exit.\n", "Move Me");

		status_init();
		alarm_init();

		status_set("OFF");

	alarm_commit();
	status_commit();

	dstate_dataok();
}

/* Items to look into more for implementation:

 	 Low and high output trip points
	dstate_setinfo("input.transfer.low", "%s", buffer2);
	dstate_setinfo("input.transfer.high", "%s", buffer);

	 Low Batt at time
	timevalue = atoi(buffer2) * 60;		Mins to secs
	dstate_setinfo("battery.runtime.low", "%d", timevalue);

	 Shutdown timer
	dstate_setinfo("ups.timer.shutdown", "%s", buffer2);

	 Restart timer
	ser_send (upsfd, "%s%s", GETX_RESTART_COUNT, COMMAND_END);
	dstate_delinfo("ups.timer.start");
	dstate_setinfo("ups.timer.start", "%s", buffer2);

*/

/**********************************************************
 * Powervar support functions for NUT command calls       *
 *********************************************************/

//static void do_battery_test(void)
//{

//}

/**************************************
 * Handlers for NUT command calls     *
 *************************************/

void upsdrv_help(void)
{
	printf("\n---------\nNOTE:\n");
	printf("You must be talking to a USB unit\n");
}

void upsdrv_cleanup(void)
{
	upsdebugx(2, "In upsdrv_cleanup");

	comm_driver->close_dev(udev);
	USBFreeExactMatcher(reopen_matcher);
	USBFreeRegexMatcher(regex_matcher);
	free(curDevice.Vendor);
	free(curDevice.Product);
	free(curDevice.Serial);
	free(curDevice.Bus);
	free(curDevice.Device);
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	free(curDevice.BusPort);
#endif
}

void upsdrv_makevartable(void)
{
//	addvar(VAR_VALUE, "battesttime", "Change battery test time from the 10 second default.");

//	addvar(VAR_VALUE, "disptesttime", "Change display test time from the 10 second default.");

//	addvar(VAR_VALUE, "offdelay", "Change shutdown delay time from 0 second default.");
}

void upsdrv_shutdown(void)
{
//	ser_send(upsfd, "%s", SHUTDOWN);
}

int instcmd(const char *cmdname, const char *extra)
{
//	int i;
//	char buffer [10];

	upsdebugx(2, "In instcmd with %s and extra %s.", cmdname, extra);

	return STAT_INSTCMD_UNKNOWN;
}


int setcmd(const char* varname, const char* setvalue)
{
	upsdebugx(2, "In setcmd for %s with %s...", varname, setvalue);

	return STAT_SET_UNKNOWN;
}
