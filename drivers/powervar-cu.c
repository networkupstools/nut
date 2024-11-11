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

//#include "config.h"		/* Must be first */
//#include <libusb-1.0/libusb.h>


#include "main.h"
#include "nut_stdint.h"
//#include "nut_libusb.h"
//#include "nut_float.h"
//#include <ctype.h>
#include "usb-common.h"
//#include "powervar-c.h"


/* Prototypes to allow setting pointer before function is defined */
int setcmd(const char* varname, const char* setvalue);
int instcmd(const char *cmdname, const char *extra);

#define DRIVER_NAME	"Powervar-CU UPS driver"
#define DRIVER_VERSION	"0.01"

/* USB comm stuff here */
#define POWERVAR_VENDORID	0x4234

static USBDevice_t *hd = NULL;
static USBDevice_t curDevice;
static USBDeviceMatcher_t *reopen_matcher = NULL;
static USBDeviceMatcher_t *regex_matcher = NULL;
static usb_dev_handle *udev;
//static usb_communication_subdriver_t *comm_driver = &usb_subdriver;
//static usb_communication_subdriver_t *comm_driver = 0;

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

	switch (is_usb_device_supported(powervar_usb_device_table, arghd))
	{
		case SUPPORTED:
			return 1;

		case POSSIBLY_SUPPORTED:
			/* by default, reject, unless the productid option is given */
			if (getval("productid")) {
				return 1;
			}
			return 0;

		case NOT_SUPPORTED:
		default:
			return 0;
	}
}


static USBDeviceMatcher_t subdriver_matcher = {
	&subdriver_match_func,
	NULL,
	NULL
};

static enum pvar_model_t {
	PVAR_UNDEFINED = 0,
	PVAR_GTS,
	PVAR_UPM,
	PVAR_DYNAMIC			/* If we can define one in the config file */
} pvar_model = PVAR_UNDEFINED;

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

/* Common CUSPP stuff here */
//static char UpsFamily [SUBBUFFSIZE];		/* Hold family that was found */
//static char UpsProtVersion [SUBBUFFSIZE];	/* Hold protocol version string */

/* Dynamic CUSPP response information positions (0 = data not available) */
/* static uint8_t byPIDProtPos = 0;
static uint8_t byPIDVerPos = 0;

static uint8_t byUIDManufPos = 0;
static uint8_t byUIDModelPos = 0;
static uint8_t byUIDSwverPos = 0;
static uint8_t byUIDSernumPos = 0;
static uint8_t byUIDFamilyPos = 0;
static uint8_t byUIDMfgdtPos = 0;
static uint8_t byUIDCSWVERPos = 0;

static uint8_t byBATStatusPos = 0;
static uint8_t byBATTmleftPos = 0;
static uint8_t byBATEstcrgPos = 0;
static uint8_t byBATVoltPos = 0;
static uint8_t byBATTempPos = 0;

static uint8_t byINPStatusPos = 0;
static uint8_t byINPFreqPos = 0;
static uint8_t byINPVoltPos = 0;
static uint8_t byINPAmpPos = 0;
static uint8_t byINPMaxvltPos = 0;
static uint8_t byINPMinvltPos = 0;

static uint8_t byOUTSourcePos = 0;
static uint8_t byOUTFreqPos = 0;
static uint8_t byOUTVoltPos = 0;
static uint8_t byOUTAmpPos = 0;
static uint8_t byOUTPercntPos = 0;

static uint8_t bySYSInvoltPos = 0;
static uint8_t bySYSInfrqPos = 0;
static uint8_t bySYSOutvltPos = 0;
static uint8_t bySYSOutfrqPos = 0;
static uint8_t bySYSBatdtePos = 0;
static uint8_t bySYSOvrlodPos = 0;
static uint8_t bySYSOutvaPos = 0;

static uint8_t bySETAudiblPos = 0;
static uint8_t bySETAtosrtPos = 0;
static uint8_t bySETOffdlyPos = 0;
static uint8_t bySETOffstpPos = 0;
static uint8_t bySETSrtdlyPos = 0;
static uint8_t bySETRstinpPos = 0;
static uint8_t bySETRsttmpPos = 0;

static uint8_t byALMOnbatPos = 0;
static uint8_t byALMLowbatPos = 0;
static uint8_t byALMBadbatPos = 0;
static uint8_t byALMTempPos = 0;
static uint8_t byALMOvrlodPos = 0;
static uint8_t byALMTstbadPos = 0;
static uint8_t byALMTestngPos = 0;
static uint8_t byALMChngbtPos = 0;

static uint8_t byTSTTimermPos = 0;
static uint8_t byTSTAbortPos = 0;
static uint8_t byTSTBatqckPos = 0;
static uint8_t byTSTBatdepPos = 0;
static uint8_t byTSTBatrunPos = 0;
static uint8_t byTSTBtemtyPos = 0;
static uint8_t byTSTDispPos = 0;
 */

/***********************************
 * USB communication functions     *
 **********************************/



/***************************************
 * CUSPP string handling functions     *
 **************************************/


/* This function finds the position of a substring in a CUSPP Format response. */


/****************************************************************
 * Below are the primary commands that are called by main       *
 ***************************************************************/

void upsdrv_initups(void)
{
	printf ("In upsdrv_initups\n");

 	char *regex_array[USBMATCHER_REGEXP_ARRAY_LIMIT];
//	char *value;
	int r = 1;

	warn_if_bad_usb_port_filename(device_path);

	printf ("After 'warn-if-bad...'\n");

	/* process the UPS selection options */
	regex_array[0] = NULL; /* handled by USB IDs device table */
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor"); /* vendor string */
	regex_array[3] = getval("product"); /* product string */
	regex_array[4] = getval("serial"); /* probably won't see this */
	regex_array[5] = NULL;
	regex_array[6] = getval("device");
	regex_array[7] = NULL;

//	hd = nutusb_open("USB");

	printf ("B4 'is_usb_device...'\n");

	r = is_usb_device_supported (powervar_usb_device_table, hd);

	printf ("After 'is_usb_device...'\n");

	if (r < 0)
	{
		upsdebugx (3, "supported: r is less than 0.");
	}
	else if(r > 0)
	{
		upsdebugx (3, "supported: r is %d.", r);
	}

	/* process the UPS selection options */
	regex_array[0] = NULL; /* handled by USB IDs device table */
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor"); /* vendor string */
	regex_array[3] = getval("product"); /* product string */
	regex_array[4] = getval("serial"); /* probably won't see this */
	regex_array[5] = getval("bus");
	regex_array[6] = getval("device");




//	r = USBNewRegexMatcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
//	if (r==-1) {
//		fatal_with_errno(EXIT_FAILURE, "USBNewRegexMatcher");
//	} else if (r) {
//		fatalx(EXIT_FAILURE, "invalid regular expression: %s", regex_array[r]);
//	}



	/* Keep moving this down below things that work and the next thing being tried */
	fatalx(EXIT_FAILURE, "[%s] USB Development exit.\n", "Move Me");




	/* link the matchers */
//	regex_matcher->next = &subdriver_matcher;

	/* Search for the first supported UPS matching the regular
	 * expression */
//	r = comm_driver->open_dev(&udev, &curDevice, regex_matcher, match_by_unitid);
//	r = comm_driver->open_dev(&udev, &curDevice, regex_matcher, 0);
//	if (r < 1) {
//		fatalx(EXIT_FAILURE, "No matching USB/HID UPS found");
//	}

//	hd = &curDevice;

//	upslogx(1, "Detected a UPS: %s/%s", hd->Vendor ? hd->Vendor : "unknown", hd->Product ? hd->Product : "unknown");

//	dstate_setinfo("ups.vendorid", "%04x", hd->VendorID);
//	dstate_setinfo("ups.productid", "%04x", hd->ProductID);

	/* create a new matcher for later reopening */
//	r = USBNewExactMatcher(&reopen_matcher, hd);
//	if (r) {
//		fatal_with_errno(EXIT_FAILURE, "USBNewExactMatcher");
//	}
	/* link the two matchers */
//	reopen_matcher->next = regex_matcher;

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

	fatalx(EXIT_FAILURE, "[%s] Not a UPS that handles CUSPP\n", "USB TEST");

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
//	ser_close(upsfd, device_path);
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
