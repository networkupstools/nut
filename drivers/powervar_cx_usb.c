/*vim ts=4*/
/* powervar_cx_usb.c - USB Driver for Powervar UPSs using CUSPP.
 *
 * Supported Powervar UPS families in this driver:
 * UPM Family (All)
 * GTS Family (All)
 *
 * Copyright (C)
 *     2024, 2025 by Bill Elliot <bill@wreassoc.com>
 *     (USB comms based on tripplite_usb.c and other drivers)
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
 * - 7 February 2025, Bill Elliot
 * Working well with USB (or serial for -cs driver)
 * - 2 December 2024.  Bill Elliot
 * Started to work on common variable and function file so this driver and the
 *  serial (-cs) driver can make use of the same protocol related functions.
 * - 2 October 2024.  Bill Elliot
 * Used pieces of oneac.c driver to get jump-started.
 *
 */

#include "main.h"
#include "nut_stdint.h"
#include "nut_libusb.h"

#include "usb-common.h"

/* Two drivers include the following. Provide some identifier for any differences */
#define PVAR_USB	1	/* This is the USB comm driver */
#include "powervar_cx.h"	/* Common driver variables and functions */

#define DRIVER_NAME	"Powervar-CUSSP UPS driver (USB)"
#define DRIVER_VERSION	"1.00"

/* USB comm stuff here */
#define USB_RESPONSE_SIZE	8
#define MAX_CNCT_ATTMPTS	60	/* x calls to upsdrv_updateinfo */

/* Powervar */
#define POWERVAR_VENDORID	0x4234

#define BIG_BUFFER		512

/* Flag indicating a disconnected USB cable...and need to reconnect */
static unsigned int ReconnectFlag = 0;

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


static USBDeviceMatcher_t subdriver_matcher = {
	&subdriver_match_func,
	NULL,
	NULL
};


/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Bill Elliot <bill@wreassoc.com>",
	DRV_BETA,
	{ NULL }
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


static int match_by_something(usb_dev_handle *argudev, USBDevice_t *arghd, usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen)
{
	int i;
	char sData[BIG_BUFFER];
	size_t sDataLen = BIG_BUFFER;

	NUT_UNUSED_VARIABLE(argudev);
	NUT_UNUSED_VARIABLE(arghd);
	NUT_UNUSED_VARIABLE(rdbuf);
	NUT_UNUSED_VARIABLE(rdlen);

	GetUPSData (PID_REQ, sData, sDataLen);

	for (i = 0; i < BIG_BUFFER ; i++)
	{
		if (sData[i] == ENDCHAR)
		{
			break;
		}
	}

	/* Just trying to be nice to the terminal output by keeping the CR out of string... */
	sData[i] = 0;
	upsdebugx(3, "'GetUPSData' returned '%s', i: %d", sData, i);
	sData[i] = ENDCHAR;	/* ... until here. */

	sData[i+1] = 0;		/* Force trailing null */

	if (GetSubstringPosition ((const char*)sData, PID_PROT_DATA) == 1)
	{
		upsdebugx(3, "Powervar CUSPP device found!");
	}

	return 1;
}


/***********************************
 * USB communication functions     *
 **********************************/

/* This function is called to flush the USB hardware of any remaining data */
/* Would like to find a way to avoid 'nut_libusb_get_interrupt: Connection timed out' */
/*  debug level 2 message. */
static void USBFlushReceive (void)
{
	unsigned int uiCount = 0;
	char response_in[USB_RESPONSE_SIZE + 1];

	upsdebugx(3, "Flushing USB receive.");

	while (comm_driver->get_interrupt(udev,
			(usb_ctrl_charbuf)response_in,
			(usb_ctrl_charbufsize) USB_RESPONSE_SIZE,
			(usb_ctrl_timeout_msec) 100) > 0)
	{
		uiCount++;
	}

	upsdebugx(6, "Flush Count: %d.", uiCount);
}


/* This function is called to send the data request to the initialized device. */
/* Return value should be >0 for success. */
size_t SendRequest (const char* sRequest)
{
	char outbuff[40];
	int ret = 0;
	size_t ReqLen = strlen(sRequest);
	size_t i;

	/* Last char reserved for ENDCHAR, so sizeof-1 for the string */
	if (sizeof(outbuff) - 1 < ReqLen) {
		upsdebugx(1, "SendRequest(): request too long, will be truncated");
		/* FIXME: unexpected situation, abort? */
	}

	/* Clear output buffer area */
	memset(outbuff, 0, sizeof(outbuff));

	/* Move command into USB buffer and add terminating character
	 * Not strncpy because we want to add ENDCHAR to a known position anyway
	 */
	for(i = 0; i < ReqLen && i < sizeof(outbuff) - 1; i++)
	{
		outbuff[i] = sRequest[i];
	}

	upsdebugx(3, "SendRequest('%s', size: %" PRIuSIZE ")", outbuff, ReqLen);

	outbuff[i] = ENDCHAR;
	ReqLen++;			/* Add one for added CR */

	ret = comm_driver->set_report(udev, 0,
		(usb_ctrl_charbuf)outbuff,
		(usb_ctrl_charbufsize)ReqLen);

	upsdebugx(5, "set_report ret:   %d", ret);

	if (ret < 0 || (size_t)ret != ReqLen)
	{
		upsdebugx(1, "libusb_set_report() returned %d instead of %" PRIuSIZE, ret, ReqLen);
	}

	if (ret < 1)
		return 0;

	return (size_t)ret;
}

/* Get the response from the UPS */
/* chBuff is the buffer to receive the response into */
/* BuffSize is the size, in chars, of chBuff */
ssize_t PowervarGetResponse (char* chBuff, const size_t BuffSize)
{
	unsigned char response_in[USB_RESPONSE_SIZE + 2];
	int ret = 0, done = 0;
	size_t i = 0;
	size_t j = 0;

	ssize_t return_val = 1;		/* Set up for a bad return */

	memset(response_in, 0, sizeof(response_in));

	for (i = 0 ; (done == 0) && (i < BuffSize) ; )
	{
		upsdebugx(5, "Receive Loop: %" PRIuSIZE, i);

		ret = comm_driver->get_interrupt(udev,
			(usb_ctrl_charbuf)response_in,
			(usb_ctrl_charbufsize) USB_RESPONSE_SIZE,
			(usb_ctrl_timeout_msec) 500);

		if (ret > 0)
		{
			for (j=0 ; j < USB_RESPONSE_SIZE ; j++)
			{
				if (response_in[j] == ENDCHAR)
				{
					upsdebugx(5, "<CR> found in response @ pos: %" PRIuSIZE ".", j);
					done = 1;
					break;
				}

				chBuff[i] = response_in[j];

				i++;
			}

			upsdebugx(5, "Loop Response: '%s'", response_in);

			/* Clear response buffer for next chunk */
			memset(response_in, 0, sizeof(response_in));
		}
		else
		{
			upsdebugx(1, "Unexpected return value: %d", ret);
			break;
		}
	}

	upsdebugx (4,"PowervarGetResponse buffer: '%s'",chBuff);

	if (ret > 0)
	{
		USBFlushReceive ();	/* Clear USB hardware */
		return_val = 0;		/* Comms OK */
	}
	else
	{
		ReconnectFlag = 1;	/* Attempt reconnection */
	}

	return return_val;
}


/****************************************************************
 * Below are the primary commands that are called by main       *
 ***************************************************************/

void upsdrv_initups(void)
{
	char *regex_array[USBMATCHER_REGEXP_ARRAY_LIMIT];
	int ret = 1;

	upsdebugx(2, "In upsdrv_initups");

	warn_if_bad_usb_port_filename(device_path);

	/* Process the UPS selection options */
	regex_array[0] = NULL; /* handled by USB IDs device table */
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor");	/* vendor string */
	regex_array[3] = getval("product"); 	/* product string */
	regex_array[4] = getval("serial");
	regex_array[5] = getval("bus");
	regex_array[6] = getval("device");
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	regex_array[7] = getval("busport");
# else
	if (getval("busport")) {
		upslogx(LOG_WARNING, "\"busport\" is configured for the device, but is not actually handled by current build combination of NUT and libusb (ignored)");
	}
# endif

	ret = USBNewRegexMatcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
	if (ret == -1)
	{
		fatal_with_errno(EXIT_FAILURE, "USBNewRegexMatcher");
	}
	else if (ret)
	{
		fatalx(EXIT_FAILURE, "invalid regular expression: %s", regex_array[ret]);
	}

	/* link the matchers */
	regex_matcher->next = &subdriver_matcher;

	upsdebugx(2, "Trying to open USB device...");

	/* Search for the first supported UPS matching the regular expression */
	ret = comm_driver->open_dev(&udev, &curDevice, regex_matcher, match_by_something);
	if (ret < 1)
	{
		fatalx(EXIT_FAILURE, "No matching USB/HID UPS found");
	}

	hd = &curDevice;

	upsdebugx(1, "Detected a UPS: %s/%s", hd->Vendor ? hd->Vendor : "unknown", hd->Product ? hd->Product : "unknown");

	dstate_setinfo("ups.vendorid", "%04x", hd->VendorID);
	dstate_setinfo("ups.productid", "%04x", hd->ProductID);

	upsdebugx(3, "VendorID: %04x", hd->VendorID);
	upsdebugx(3, "ProductID: %04x", hd->ProductID);

	/* create a new matcher for later reopening */
	ret = USBNewExactMatcher(&reopen_matcher, hd);
	if (ret)
	{
		fatal_with_errno(EXIT_FAILURE, "USBNewExactMatcher");
	}

	/* link the two matchers */
	reopen_matcher->next = regex_matcher;
}

void upsdrv_initinfo(void)
{
	upsdebugx(3, "In upsdrv_initinfo");

	/* Get port ready */
	SendRequest ((const char*)COMMAND_END);		/* Just get device ready -- flush */
	USBFlushReceive ();				/* Just flush response */

	PvarCommon_Initinfo ();
}


/* This function is called regularly for data updates. */
void upsdrv_updateinfo(void)
{
	int ret;
	static int CnctAttempts = 0;

	upsdebugx(3, "In upsdrv_updateinfo");

	if (ReconnectFlag)
	{
		dstate_setinfo("driver.state", "reconnect.trying");
		upslogx(LOG_WARNING, "USB device may be detached.");
		upslogx(LOG_NOTICE, "USB reconnect attempt: %d.", ++CnctAttempts);
		upsdebugx(4, "USB reconnect attempt: %d", CnctAttempts);

		hd = NULL;

		ret = comm_driver->open_dev(&udev, &curDevice, reopen_matcher, match_by_something);
		if (ret < 1)
		{
			if (CnctAttempts >= MAX_CNCT_ATTMPTS)
			{
				upsdebugx(4, "Exceeded max reconnect attemtps.");
				fatalx(EXIT_FAILURE, "Exceeded max reconnect attempts.");
			}
			else
			{
				upslogx(LOG_INFO, "USB reconnect attempt %d failed.", CnctAttempts);
				upslogx(LOG_INFO, "Will try another reconnect in a bit.");
				dstate_datastale();
				return;
			}
		}

		upsdebugx(4, "USB device reconnected!");
		ReconnectFlag = 0;	/* Show good for now */
		CnctAttempts = 0;

		hd = &curDevice;

		upslogx(LOG_NOTICE, "USB reconnect successful");
		dstate_setinfo("driver.state", "reconnect.updateinfo");
		upsdrv_initinfo();

		dstate_setinfo("driver.state", "quiet");
	}

	PvarCommon_Updateinfo ();
}


/**************************************
 * Handlers for NUT command calls     *
 *************************************/

void upsdrv_help(void)
{
	printf("\n---------\nNOTE:\n");
	printf("This driver is for connecting to a Powervar UPS USB port.\n");
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

/* End of powervar_cx_usb.c file */
