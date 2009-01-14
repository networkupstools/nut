/*  mge-shut.c - monitor MGE UPS for NUT with SHUT protocol
 * 
 *  Copyright (C) 2002 - 2008
 *     Arnaud Quette <arnaud.quette@gmail.com>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://opensource.mgeups.com/>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <string.h>

#include "config.h"
#include "main.h"
#include "serial.h"
#include "timehead.h"
#include "mge-shut.h"
#include "hidparser.h"
#include "hidtypes.h"
#include "common.h" /* for upsdebugx() etc */

/* --------------------------------------------------------------- */
/*                  Define "technical" constants                   */
/* --------------------------------------------------------------- */

#define DRIVER_NAME	"MGE UPS SYSTEMS/SHUT driver"
#define DRIVER_VERSION	"0.68"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <ArnaudQuette@Eaton.com>",
	DRV_STABLE,
	{ NULL }
};

#define MAX_TRY 4

/* global variables */
int commstatus = 0;
int lowbatt = DEFAULT_LOWBATT;
int ondelay = DEFAULT_ONDELAY;
int offdelay = DEFAULT_OFFDELAY;
int notification = DEFAULT_NOTIFICATION;

#define SD_RETURN	0
#define SD_STAYOFF	1

int sdtype = SD_RETURN;

#define BYTESWAP(in) (((in & 0xFF) << 8) + ((in & 0xFF00) >> 8))

/* realign packet data according to Endianess */
static void align_request(hid_packet_t *sd)
{
#if WORDS_BIGENDIAN
        /* Sparc/Mips/... are big endian, USB/SHUT little endian */
	(*sd).wValue    = BYTESWAP((*sd).wValue);
	(*sd).wIndex    = BYTESWAP((*sd).wIndex);
	(*sd).wLength   = BYTESWAP((*sd).wLength);
#endif
}

/* --------------------------------------------------------------- */
/*                      Global structures                          */
/* --------------------------------------------------------------- */

hid_desc_data_t    	hid_descriptor;
device_desc_data_t 	device_descriptor;
static long             hValue;
static HIDDesc_t  	*pDesc = NULL; /* parsed Report Descriptor */
u_char 			raw_buf[4096];

/* --------------------------------------------------------------- */
/*                     Function prototypes                         */
/* --------------------------------------------------------------- */

float expo(int a, int b);
extern long FormatValue(long Value, u_char Size);
static char *hu_find_infoval(info_lkp_t *hid2info, long value);

/* --------------------------------------------------------------- */
/*                    UPS Driver Functions                         */
/* --------------------------------------------------------------- */

void upsdrv_initinfo (void)
{
	mge_info_item_t *item;
	
	upsdebugx(2, "entering initinfo()\n");
	
	/* Get complete Model information */
	shut_identify_ups ();

	printf("Detected %s [%s] on %s\n", dstate_getinfo("ups.model"),
			dstate_getinfo("ups.serial"), device_path);

	/* Device capabilities enumeration ----------------------------- */
	for ( item = mge_info ; item->type != NULL ; item++ ) {
		
		/* Check if we are asked to stop (reactivity++) */
		if (exit_flag != 0)
		  return;

		/* avoid redundancy when multiple defines (RO/RW) */
		if (dstate_getinfo(item->type) != NULL)
			continue;
		
		/* Special case for handling server side variables */
		if (item->shut_flags & SHUT_FLAG_ABSENT) {
			/* Check if exists (if necessary) before creation */
			if (item->item_path != NULL)
			  {
				if (hid_get_value(item->item_path) != 1 )
				  continue;
			  }
			else
			{
			  /* Simply set the default value */
			  dstate_setinfo(item->type, "%s", item->dfl);
			  dstate_setflags(item->type, item->flags);
			  continue;
			}
			
			dstate_setinfo(item->type, "%s", item->dfl);
			dstate_setflags(item->type, item->flags);

			/* Set max length for strings, if needed */
			if (item->flags & ST_FLAG_STRING)
				dstate_setaux(item->type, item->length);
			
			/* disable reading now 
			item->shut_flags &= ~SHUT_FLAG_OK;*/
		} else {
			if (hid_get_value(item->item_path) != 0 ) {

				item->shut_flags &= SHUT_FLAG_OK;
				dstate_setinfo(item->type, item->fmt, hValue);
				dstate_setflags(item->type, item->flags);
				/* Set max length for strings */
				if (item->flags & ST_FLAG_STRING)
				  dstate_setaux(item->type, item->length);
			}
			else {
				item->shut_flags &= ~SHUT_FLAG_OK;
			}
		}
	}

	/* commands ----------------------------------------------- */
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");

	/* install handlers */
	upsh.setvar = hid_set_value; /* setvar; */
	upsh.instcmd = instcmd;
	
}

/* --------------------------------------------------------------- */

void upsdrv_updateinfo (void)
{
	mge_info_item_t *item;
	char *nutvalue;
	
	upsdebugx(2, "entering upsdrv_updateinfo()");
	
	if (commstatus == 0) {
		if (shut_ups_start () != 0) {
			upsdebugx(2, "No communication with UPS, retrying");
			dstate_datastale();
			return;
		} else {
			upsdebugx(2, "Communication with UPS established");
		}
	}

	shut_ups_status();

	/* Device data walk ----------------------------- */
	for ( item = mge_info ; item->type != NULL; item++ ) {

	  /* Check if we are asked to stop (reactivity++) */
	  if (exit_flag != 0)
		return;

    if (item->shut_flags & SHUT_FLAG_ABSENT)
			continue;

    if (item->shut_flags & SHUT_FLAG_OK) {

			if(hid_get_value(item->item_path) != 0 ) {				
				upsdebugx(3, "%s: hValue = %ld",	item->item_path, hValue);
				/* upsdebugx(3, "%s: hValue = %ld (%ld)",
					item->item_path, hValue, hData.LogMax); */
				
				/* need lookup'ed translation */
				if (item->hid2info != NULL)
				  {
					nutvalue = hu_find_infoval(item->hid2info, (long)hValue);
					if (nutvalue != NULL)
					  dstate_setinfo(item->type, "%s", nutvalue);
					else
					  dstate_setinfo(item->type, item->fmt, hValue);
				  }
				else
				  dstate_setinfo(item->type, item->fmt, hValue);

				dstate_dataok();
			} else {
				if (shut_ups_start () != 0)
					dstate_datastale();
			}
		}
	}
}

/* --------------------------------------------------------------- */

void upsdrv_shutdown (void)
{
	char val[5];

	if (sdtype == SD_RETURN) {
		/* set DelayBeforeStartup */
		snprintf(val, sizeof(val), "%d", ondelay);
		hid_set_value("ups.delay.start", val);
	}

	/* set DelayBeforeShutdown */
	snprintf(val, sizeof(val), "%d", offdelay);
	hid_set_value("ups.delay.shutdown", val);
}

/* --------------------------------------------------------------- */

void upsdrv_help (void)
{
	upsdebugx(2, "entering upsdrv_help");
}

/* --------------------------------------------------------------- */

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable (void)
{
	char msg[MAX_STRING];
	
	upsdebugx (2, "entering upsdrv_makevartable()");
  
	snprintf(msg, sizeof(msg), "Set low battery level, in %% (default=%d).",
		DEFAULT_LOWBATT);
	addvar (VAR_VALUE, "lowbatt", msg);
	
	snprintf(msg, sizeof(msg), "Set shutdown delay, in seconds (default=%d).",
		DEFAULT_OFFDELAY);
	addvar (VAR_VALUE, "offdelay", msg);

	snprintf(msg, sizeof(msg), "Set startup delay, in ten seconds units (default=%d).",
		DEFAULT_ONDELAY);
	addvar (VAR_VALUE, "ondelay", msg);
	
	snprintf(msg, sizeof(msg), "Set notification type, 1 = no, 2 = light, 3 = yes (default=%d).",
		DEFAULT_NOTIFICATION);
	addvar (VAR_VALUE, "notification", msg);
}

/* --------------------------------------------------------------- */

void upsdrv_initups (void)
{
	upsdebugx(2, "entering upsdrv_initups()");

	/* initialize serial port */
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);
	setline (1);
  
	/* get battery lowlevel */
	if (getval ("lowbatt"))
		lowbatt = atoi (getval ("lowbatt"));
  
	/* on delay */
	if (getval ("ondelay"))
		ondelay = atoi (getval ("ondelay"));
	
	/* shutdown delay */
	if (getval ("offdelay"))
		offdelay = atoi (getval ("offdelay"));

	/* notification type */
	if (getval ("notification"))
	  	notification = atoi (getval ("notification"));

	/* initialise communication */
	if (shut_ups_start () != 0)
		fatalx(EXIT_FAILURE, "No communication with UPS");
	else
		upsdebugx(2, "Communication with UPS established");

	/* initialise HID communication */
	if(hid_init_device() != 0)
		fatalx(EXIT_FAILURE, "Can't initialise HID device");
}

/* --------------------------------------------------------------- */

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}

/* --------------------------------------------------------------- */

int instcmd(const char *cmdname, const char *extra)
{
	/* Shutdown UPS and return when power is restored */
	if (!strcasecmp(cmdname, "shutdown.return")) {
		sdtype = SD_RETURN;
		upsdrv_shutdown();
		return STAT_INSTCMD_HANDLED;
	}
	
	/* Shutdown UPS and stay off when power is restored */
	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		sdtype = SD_STAYOFF;
		upsdrv_shutdown();
		return STAT_INSTCMD_HANDLED;
	}

	/* Power off the load immediatly */
	if (!strcasecmp(cmdname, "load.off")) {
		/* set DelayBeforeShutdown to 0 */
		hid_set_value("ups.delay.shutdown", "0");
		return STAT_INSTCMD_HANDLED;
	}

	/* Power on the load immediatly */
	if (!strcasecmp(cmdname, "load.on")) {
		/* set DelayBeforeStartup to 0 */
		hid_set_value("ups.delay.start", "0");
		return STAT_INSTCMD_HANDLED;
	}

	/* Start battery test */
	if (!strcasecmp(cmdname, "test.battery.start")) {
		/* set Test to 1 (Quick test) */
		hid_set_value("ups.test.result", "1");
		return STAT_INSTCMD_HANDLED;
	}
	
	/* Stop battery test */
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		/* set Test to 3 (Abort test) */
		hid_set_value("ups.test.result", "3");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}


/*****************************************************************************
 * shut_ups_start ()
 * 
 * initiate communication with the UPS
 *
 * return 0 on success, -1 on failure
 *
 *****************************************************************************/
int shut_ups_start ()
{
	u_char c = SHUT_SYNC, r[1];
	int try;
	
	upsdebugx (2, "entering shut_ups_start()\n");
	r[0] = '\0';

	switch (notification) {
		case OFF_NOTIFICATION:
			c = SHUT_SYNC_OFF;
			break;
		case LIGHT_NOTIFICATION:
			c = SHUT_SYNC_LIGHT;
			break;
		default:
		case COMPLETE_NOTIFICATION:
			c = SHUT_SYNC;
			break;
	}

	/* Sync with the UPS using Complete, Off or light notification */
	for (try = 0; try < MAX_TRY; try++) {
		if ((shut_token_send(c)) == -1) {
			upsdebugx (3, "Communication error while writing to port");
			return -1;
		}
		serial_read (1000, &r[0]);
		if (r[0] == c) {
			commstatus = 1;
			upsdebugx (3, "Syncing and notification setting done");
			return 0;
		}
	}
	commstatus = 0;
	return -1;
}

/**********************************************************************
 * shut_identify_ups ()
 * 
 * Get SHUT device complete name
 *
 * return 0 on success, -1 on failure
 *
 *********************************************************************/
int shut_identify_ups ()
{
	char string[MAX_STRING];
	char model[MAX_STRING];
	char *finalname = NULL;
	int retcode, tries=MAX_TRY;
	
	if (commstatus == 0)
		return -1;
  
	upsdebugx (2, "entering shut_identify_ups(0x%04x, 0x%04x)\n", 
				device_descriptor.dev_desc.iManufacturer,
				device_descriptor.dev_desc.iProduct);

	/* Get strings iModel and iProduct */
	while (tries > 0)
	{
		if (shut_get_string(device_descriptor.dev_desc.iProduct, string, 0x25) > 0)
		{
			strcpy(model, string);
			
			if(hid_get_value("UPS.PowerSummary.iModel") != 0 )
			{
				if((shut_get_string(hValue, string, 0x25)) > 0)
				{
					finalname = get_model_name(model, string);
					upsdebugx (2, "iModel = %s", string);
					tries = 0;
				}
			}
			else
			{
				/* Try with "UPS.Flow.[4].ConfigApparentPower" */
				if(hid_get_value("UPS.Flow.[4].ConfigApparentPower") != 0 )
				{
					snprintf(string, sizeof(string), "%i", (int)hValue);
					finalname = get_model_name(model, string);
				}
				else
					finalname = get_model_name(model, NULL);

				tries = 0;
			}
	
			dstate_setinfo("ups.model", "%s", finalname);
		}
		else
			tries--;
	}
		
	/* Get strings iSerialNumber */
	if (((retcode = shut_get_string(device_descriptor.dev_desc.iSerialNumber, string, 0x25)) > 0)
		&& strcmp(string, "") && string[0] != '\t') {

			dstate_setinfo("ups.serial", "%s", string);
	}
	else
		dstate_setinfo("ups.serial", "unknown");

	/* all went fine */
	return 1;
}

/**********************************************************************
 * shut_wait_ack()
 *
 * wait for an ACK packet
 *
 * returns 0 on success, -1 on error, -2 on NACK, -3 on NOTIFICATION
 * 
 *********************************************************************/
int shut_wait_ack (void)
{
	u_char c[1];

	c[0] = '\0';
	
	serial_read (DEFAULT_TIMEOUT, &c[0]);
	if (c[0] == SHUT_OK) {
		upsdebugx (2, "shut_wait_ack(): ACK received");
		return 0;
	}
	else if (c[0] == SHUT_NOK) {
		upsdebugx (2, "shut_wait_ack(): NACK received");
		return -2;
	}
	else if ((c[0] & SHUT_PKT_LAST) == SHUT_TYPE_NOTIFY) {
		upsdebugx (2, "shut_wait_ack(): NOTIFY received");
		return -3;
	}
	upsdebugx (2, "shut_wait_ack(): Nothing received");
	return -1;
}

/**********************************************************************
 * char_read (char *bytes, int size, int read_timeout)
 *
 * reads size bytes from the serial port
 * 
 * bytes     - buffer to store the data
 * size      - size of the data to get
 * read_timeout - serial timeout (in milliseconds)
 * 
 * return -1 on error, -2 on timeout, nb_bytes_readen on success
 * 
 *********************************************************************/
static int char_read (char *bytes, int size, int read_timeout)
{
	struct timeval serial_timeout;
	fd_set readfs;
	int readen = 0;
	int rc = 0;
	
	FD_ZERO (&readfs);
	FD_SET (upsfd, &readfs);
	
	serial_timeout.tv_usec = (read_timeout % 1000) * 1000;
	serial_timeout.tv_sec = (read_timeout / 1000);
	
	rc = select (upsfd + 1, &readfs, NULL, NULL, &serial_timeout);
	if (0 == rc)
		return -2;			/* timeout */

	if (FD_ISSET (upsfd, &readfs)) {
		int now = read (upsfd, bytes, size - readen);

		if (now < 0) {
			return -1;
		}
		else {
			bytes += now;
			readen += now;
		}
	}
	else {
		return -1;
	}
	return readen;
}

/**********************************************************************
 * serial_read (int read_timeout)
 *  
 * return data one byte at a time
 *
 * read_timeout - serial timeout (in milliseconds)
 *
 * returns 0 on success, -1 on error, -2 on timeout
 * 
 **********************************************************************/
int serial_read (int read_timeout, u_char *readbuf)
{
	static u_char cache[512];
	static u_char *cachep = cache;
	static u_char *cachee = cache;
	int recv;
	*readbuf = '\0';
	
	/* if still data in cache, get it */
	if (cachep < cachee) { 
		*readbuf = *cachep++;
		return 0;
		/* return (int) *cachep++; */
	}
	recv = char_read ((char *)cache, 1, read_timeout);

	if ((recv == -1) || (recv == -2))
		return recv;

	cachep = cache;
	cachee = cache + recv;
	cachep = cache;
	cachee = cache + recv;
	
	if (recv) {
		upsdebugx(5,"received: %02x", *cachep); 
		*readbuf = *cachep++;
		return 0;
	}
	return -1;
}

/**********************************************************************
 * serial_send (char *buf, int len)
 *
 * write the content of buf to the serial port
 *
 * buf       - data to send
 * len       - lenght of data to send
 * 
 * returns number of bytes written on success, -1 on error
 * 
 **********************************************************************/
int serial_send (u_char *buf, int len)
{
	tcflush (upsfd, TCIFLUSH);
	upsdebug_hex (3, "sent", (u_char *)buf, len);
	return write (upsfd, buf, len);
}

/* 
 * Serial HID UPS Transfer (SHUT) functions
 *********************************************************************/
/* Get and parse UPS status */
void  shut_ups_status(void)
{
  int try = 0, retcode = 0;

	/* clear status buffer before begining */
	status_init();
	
	/* Ensure to have at least basic status */
        while (try < MAX_TRY) {
	  if((retcode = hid_get_value("UPS.PowerSummary.PresentStatus.ACPresent")) != 0 ) {
	    try = MAX_TRY;
	    if(hValue == 1){
	      status_set("OL");
	    } else {
	      status_set("OB");
	    }
	  } else { /* retry to get data */
	    try++;
	  }
	}

	if(hid_get_value("UPS.PowerSummary.PresentStatus.Discharging") != 0 ) {
		if(hValue == 1)
			status_set("DISCHRG");
	}

	if(hid_get_value("UPS.PowerSummary.PresentStatus.Charging") != 0 ) {
		if(hValue == 1)
			status_set("CHRG");
	}

	if(hid_get_value("UPS.PowerSummary.PresentStatus.ShutdownImminent") != 0 ) {
		if(hValue == 1)
			status_set("LB");
	}
	
	if(hid_get_value("UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit") != 0 ) {
		if(hValue == 1)
			status_set("LB");
	}

	if(hid_get_value("UPS.PowerSummary.PresentStatus.Overload") != 0 ) {
		if(hValue == 1)
			status_set("OVER");
	}

	if(hid_get_value("UPS.PowerSummary.PresentStatus.NeedReplacement") != 0 ) {
		if(hValue == 1)
			status_set("RB");
	}
  
	if(hid_get_value("UPS.PowerSummary.PresentStatus.Good") != 0 ) {
		if(hValue == 0)
			status_set("OFF");
	}

	/* FIXME: extend ups.status for BYPASS: */
	/* Manual bypass */
	if(hid_get_value("UPS.PowerConverter.Input.[4].PresentStatus.Used") != 0 ) {
		if(hValue == 1)
			status_set("BYPASS");
	}
	/* Automatic bypass */
	if(hid_get_value("UPS.PowerConverter.Input.[2].PresentStatus.Used") != 0 ) {
		if(hValue == 1)
			status_set("BYPASS");
	}

	status_commit();
}

/* Calculate the SHUT checksum for the packet "buf" */
u_char shut_checksum(const u_char *buf, int bufsize)
{
	int i;
	u_char chk=0;
	
	for(i=0; i<bufsize; i++)
		chk^=buf[i];
	return chk;
}

/* Send token (1 char packet) "token" */
int shut_token_send(u_char token)
{
	u_char Buf[1];
	Buf[0]=token;
	return serial_send (Buf, 1);
}

int shut_packet_send (hid_data_t *hdata, int datalen, u_char token)
{
	shut_packet_t SHUTRequest;
	shut_data_t   sdata;
	short Retry=1;
	short Size;
	int i;
	
	upsdebugx (3, "entering shut_packet_send (%i)", datalen);
	
	while(datalen>0 && Retry>0)
	{
		Size=(datalen>=8) ? 8 : datalen;

		/* Packets need only to be sent once
		 * NACK handling should take care of the rest */
		if (Retry == 1) {
			
			/* Forge SHUT Frame */
			SHUTRequest.bType = SHUT_TYPE_REQUEST + token;
			SHUTRequest.bLength = (Size<<4) + Size;
			SHUTRequest.data = *hdata;
			/* memcpy(&SHUTRequest.data.raw_pkt, hdata->raw_pkt, Size); */
			
			sdata.shut_pkt = SHUTRequest;
			sdata.raw_pkt[(Size+3) - 1] = shut_checksum(sdata.shut_pkt.data.raw_pkt, Size);
	
			upsdebugx (4, "shut_checksum = %2x", sdata.raw_pkt[(Size+3)-1]);
	
			serial_send (sdata.raw_pkt, Size+3);
		}
		i = shut_wait_ack ();
		if (i == 0) {
			datalen-=Size;
			Retry=5;
			
			upsdebugx (4, "received ACK");
			break;
		} else if ((i == -1) || (i == -3)) {
			/* retry a finite number of times if something wrong happened while 
			 * sending like a notification or a NACK */
			if (Retry >= MAX_TRY) {
				upsdebugx(2, "Max tries reached while waiting for ACK, still getting errors");
 				return i;
			} else {
				upsdebugx(4, "Retry = %i", Retry);
				/* Send a NACK to get a resend from the UPS */
				shut_token_send(SHUT_NOK);
				Retry++;
			}
		}
	}
	return (datalen==0);
}

int shut_packet_recv (u_char *Buf, int datalen)
{
	u_char   Start[2];
	u_char   Frame[8];
	u_char   Chk[1];
	u_short  Size=8;
	u_short  Pos=0;
	u_char   Retry=0;
	int recv;
	shut_data_t   sdata;

	upsdebugx (4, "entering shut_packet_recv (%i)", datalen);
	
	while(datalen>0 && Retry<3)
	{
		if(serial_read (DEFAULT_TIMEOUT, &Start[0]) >= 0)
		{	  
			sdata.shut_pkt.bType = Start[0];
			if(Start[0]==SHUT_SYNC)
			{
				upsdebugx (4, "received SYNC token");
				memcpy(Buf, Start, 1);
				return 1;
			}
			else 
			{
				/* if(((Start[1] = serial_read (DEFAULT_TIMEOUT)) >= 0) && */
				if((serial_read (DEFAULT_TIMEOUT, &Start[1]) >= 0) &&
					((Start[1]>>4)==(Start[1]&0x0F)))
				{
					upsdebug_hex(3, "Receive", Start, 2); 
					Size=Start[1]&0x0F;
					sdata.shut_pkt.bLength = Size;
					for(recv=0;recv<Size;recv++)
						if(serial_read (DEFAULT_TIMEOUT, &Frame[recv]) < 0)
							break;
						
					upsdebug_hex(3, "Receive", Frame, Size); 
					
					serial_read (DEFAULT_TIMEOUT, &Chk[0]);
					if(Chk[0]==shut_checksum(Frame, Size))
					{
						upsdebugx (4, "shut_checksum: %02x => OK", Chk[0]);
						memcpy(Buf, Frame, Size);
						datalen-=Size;
						Buf+=Size;
						Pos+=Size;
						Retry=0;
						
						shut_token_send(SHUT_OK);
						
						if(Start[0]&SHUT_PKT_LAST) {
							if ((Start[0]&SHUT_PKT_LAST) == SHUT_TYPE_NOTIFY) {
							/* TODO: process notification (dropped for now) */
								upsdebugx (4, "=> notification");
								datalen+=Pos;
								Pos=0;
							}
							else
								return Pos;
						}
						else
							upsdebugx (4, "need more data (%i)!", datalen);
					}
					else
					{
						upsdebugx (4, "shut_checksum: %02x => NOK", Chk[0]);
						shut_token_send(SHUT_NOK);
						Retry++;
					}
				}
				else
					return 0;
			}
		}
		else
			Retry++;
	} /* while */
	return 0;
}

/* 
 * Human Interface Device (HID) functions
 *********************************************************************/

/**********************************************************************
 * shut_get_descriptor(int desctype, u_char *pkt)
 * 
 * get descriptor specified by DescType and return it in Buf
 *
 * desctype  - from shutdataType
 * pkt       - where to store the report received
 *
 * return 0 on success, -1 on failure, -2 on NACK
 *
 *********************************************************************/
int shut_get_descriptor(int desctype, u_char *pkt, int reportlen)
{
	hid_packet_t HIDRequest;
	hid_data_t   data;
	int retcode;
	
	upsdebugx (2, "entering shut_get_descriptor(n %02x, %i)",
				desctype, reportlen);
	
	HIDRequest.bmRequestType = REQUEST_TYPE_USB+(desctype>=HID_DESCRIPTOR?1:0);
	HIDRequest.bRequest = 0x06;
	HIDRequest.wValue = (desctype<<8);
	HIDRequest.wIndex = 0x0000;
	HIDRequest.wLength = reportlen;
	
	align_request(&HIDRequest);

	data.hid_pkt = HIDRequest;
	
/*	if((retcode = shut_packet_send (&data, sizeof(data), SHUT_PKT_LAST)) > 0) */
	if((retcode = shut_packet_send (&data, 8, SHUT_PKT_LAST)) > 0)
	{
		if((retcode = shut_packet_recv (pkt, reportlen)) > 0)
		{
			upsdebug_hex(3, "shut_get_descriptor", pkt, retcode);
			return retcode;
		}
		else
			return retcode;
	}
	return retcode;
}

/**********************************************************************
 * shut_get_string(int index, u_char *pkt, int reportlen)
 * 
 * get descriptor specified by DescType and return it in Buf
 *
 * index     - from shutdataType
 * string    - where to store the string received
 * strlen    - length of string
 *
 * return string size on success, -1 on failure, -2 on NACK
 *
 *********************************************************************/
int shut_get_string(int strindex, char *string, int stringlen)
{
	hid_packet_t HIDRequest;
	hid_data_t   data;
	int retcode;
	u_char buf[MAX_STRING];
	
	upsdebugx (2, "entering shut_get_string(%02x)", strindex);
	
	HIDRequest.bmRequestType = REQUEST_TYPE_USB;
	HIDRequest.bRequest = 0x06;
	HIDRequest.wValue = strindex+(STRING_DESCRIPTOR<<8);
	HIDRequest.wIndex = 0x0000;
	HIDRequest.wLength = (stringlen<<8); /* (reportlen&0xFF)&(reportlen>>8); */
	
	align_request(&HIDRequest);
	
	data.hid_pkt = HIDRequest;
	
	if((retcode = shut_packet_send (&data, 8, SHUT_PKT_LAST)) >0)
	{
		upsdebug_hex(3, "shut_get_string", data.raw_pkt, 8);
		if((retcode = shut_packet_recv (buf, stringlen)) > 0)
		{
			upsdebug_hex(3, "shut_get_string", buf, retcode);
			make_string(buf, retcode, string);
			upsdebugx(2, "string: %s", string);
			return strlen(string);
		}
		else
			return retcode;
	}
	return 0;
}

/**********************************************************************
 * shut_get_report(int id, u_char *pkt, int reportlen)
 * 
 * get report specified by id and return it in pkt
 *
 * id        - from shutdataType
 * pkt       - where to store the string received
 * strlen    - length of string
 *
 * return report size on success, -1 on failure, -2 on NACK
 *
 *********************************************************************/
int shut_get_report(int id, u_char *pkt, int reportlen)
{
	hid_packet_t HIDRequest;
	hid_data_t   data;
	int retcode;
	
	upsdebugx (2, "entering shut_get_report(id: %02x, len: %02x)", id, reportlen);

	HIDRequest.bmRequestType = REQUEST_TYPE_GET_REPORT;
	HIDRequest.bRequest = 0x01;
	HIDRequest.wValue = id+(HID_REPORT_TYPE_FEATURE<<8);
	HIDRequest.wIndex = 0x0000;
	HIDRequest.wLength = reportlen;
	
	align_request(&HIDRequest);
	
	data.hid_pkt = HIDRequest;
	
/*	if((retcode = shut_packet_send (&data, sizeof(data), SHUT_PKT_LAST)) > 0) */
	if((retcode = shut_packet_send (&data, 8, SHUT_PKT_LAST)) > 0)
	{
		if((retcode = shut_packet_recv (pkt, reportlen)) > 0)
		{
			upsdebug_hex(3, "shut_get_report", pkt, retcode);
			return retcode;
		}
		else
			return retcode;
	}
	return retcode;
}

/**********************************************************************
 * shut_set_report(int id, u_char *pkt, int reportlen)
 * 
 * set report specified by id using pkt as value
 *
 * id        - from shutdataType
 * pkt       - what to put in report
 * strlen    - length of report
 *
 * return string size on success, -1 on failure, -2 on NACK
 *
 *********************************************************************/
int shut_set_report(int id, u_char *pkt, int reportlen)
{
	hid_packet_t HIDRequest;
	hid_data_t   data;
	int retcode;
	
	upsdebugx (2, "entering shut_set_report(id: %02x, len: %02x)", id, reportlen);
	
	HIDRequest.bmRequestType = REQUEST_TYPE_SET_REPORT;
	HIDRequest.bRequest = 0x09;
	HIDRequest.wValue = id+(HID_REPORT_TYPE_FEATURE<<8);
	HIDRequest.wIndex = 0x0000;
	HIDRequest.wLength = reportlen;
	
	align_request(&HIDRequest);

	data.hid_pkt = HIDRequest;

	/* first packet to instruct a set command */
	if((retcode = shut_packet_send (&data, sizeof(data), 0x0)) > 0)
	{
		/* second packet to give the actual data */
		memcpy(&data.raw_pkt, pkt, reportlen);
		upsdebug_hex(3, "Set2", pkt, reportlen);

		retcode = shut_packet_send (&data, reportlen, SHUT_PKT_LAST);
	}

	return retcode;
}

/**********************************************************************
 * hid_init_device()
 * 
 * Get Device/HID/Report descriptors from device and initialise
 * HID Parser for further actions
 *
 * return 0 on success, -1 on failure
 *
 *********************************************************************/
int hid_init_device()
{
	int retcode;
	
	/* Get HID descriptor */
	if((retcode = shut_get_descriptor(HID_DESCRIPTOR, hid_descriptor.raw_desc, 0x09)) > 0)
	{
		upsdebug_hex(3, "shut_get_descriptor(hid)", hid_descriptor.raw_desc, retcode);
		
		/* WORKAROUND: need to be fixed */
		hid_descriptor.hid_desc.wDescriptorLength = hid_descriptor.raw_desc[7] +
			(hid_descriptor.raw_desc[8]<<8);
		
		upsdebugx(3, "HID Descriptor: \nbLength: \t\t0x%02x\nbDescriptorType: \t0x%02x\n",
				hid_descriptor.hid_desc.bLength,
				hid_descriptor.hid_desc.bDescriptorType);
		
		upsdebugx(3, "bcdHID: \t\t0x%04x\nbCountryCode: \t\t0x%02x\nbNumDescriptors: \t0x%02x\n",
				hid_descriptor.hid_desc.bcdHID,
				hid_descriptor.hid_desc.bCountryCode,
				hid_descriptor.hid_desc.bNumDescriptors);
		
		upsdebugx(3, "bReportDescriptorType: \t0x%02x\nwDescriptorLength: \t0x%04x",
				hid_descriptor.hid_desc.bReportDescriptorType,
				hid_descriptor.hid_desc.wDescriptorLength);
		
		/* Get Device descriptor */
		if((retcode = shut_get_descriptor(DEVICE_DESCRIPTOR, device_descriptor.raw_desc, 0x12)) > 0)
		{	
			upsdebug_hex(3, "shut_get_descriptor(device)", device_descriptor.raw_desc, retcode);
			
			upsdebugx(2, "Device Descriptor: \nbLength: \t\t0x%02x\nbDescriptorType:\
				\t0x%02x\nbcdUSB: \t\t0x%04x\nbDeviceClass: \t\t0x%02x\nbDeviceSubClass:\
				\t0x%02x\nbDeviceProtocol: \t0x%02x\nbMaxPacketSize0:\
				\t0x%02x\nidVendor: \t\t0x%04x\nidProduct: \t\t0x%04x\nbcdDevice:\
				\t\t0x%04x\niManufacturer: \t\t0x%02x\niProduct:\
				\t\t0x%02x\niSerialNumber: \t\t0x%02x\nbNumConfigurations: \t0x%02x\n",
				device_descriptor.dev_desc.bLength,
				device_descriptor.dev_desc.bDescriptorType,
				device_descriptor.dev_desc.bcdUSB,
				device_descriptor.dev_desc.bDeviceClass, 
				device_descriptor.dev_desc.bDeviceSubClass,
				device_descriptor.dev_desc.bDeviceProtocol,
				device_descriptor.dev_desc.bMaxPacketSize0,
				device_descriptor.dev_desc.idVendor, 
				device_descriptor.dev_desc.idProduct,
				device_descriptor.dev_desc.bcdDevice,
				device_descriptor.dev_desc.iManufacturer,
				device_descriptor.dev_desc.iProduct,
				device_descriptor.dev_desc.iSerialNumber,
				device_descriptor.dev_desc.bNumConfigurations);	

			/* Get Report descriptor */
			if((retcode = shut_get_descriptor(REPORT_DESCRIPTOR, raw_buf,
				hid_descriptor.hid_desc.wDescriptorLength)) > 0) {

				upsdebug_hex(3, "shut_get_descriptor(report)", raw_buf, retcode);
				
				/* Parse Report Descriptor */
				Free_ReportDesc(pDesc);
				pDesc = Parse_ReportDesc(raw_buf, retcode);
				if (!pDesc) {
					fatalx(EXIT_FAILURE, "Failed to parse report descriptor: %s", strerror(errno));
				}
			}
			else
				fatalx(EXIT_FAILURE, "Unable to get Report Descriptor");
		}
		else
			fatalx(EXIT_FAILURE, "Unable to get Device Descriptor");
	}
	else
		fatalx(EXIT_FAILURE, "Unable to get HID Descriptor");

	return 0;
}

/* translate HID string path to numeric path and return path depth */
ushort lookup_path(const char *HIDpath, HIDData_t *data)
{
	ushort i = 0, cond = 1;
	int cur_usage;
	char buf[MAX_STRING];
	char *start, *end; 
	
	strncpy(buf, HIDpath, strlen(HIDpath));
	buf[strlen(HIDpath)] = '\0';
	start = end = buf;
	
	upsdebugx(3, "entering lookup_path(%s)", buf);

	while (cond) {
	
		if ((end = strchr(start, '.')) == NULL) {
			cond = 0;			
		}
		else
			*end = '\0';
	
		upsdebugx(4, "parsing %s", start);
	
		/* lookup code */
		if ((cur_usage = hid_lookup_usage(start)) == -1) {
			upsdebugx(4, "%s wasn't found", start);
			return 0;
		}
		else {
			data->Path.Node[i] = cur_usage;
			i++; 
		}
	
		if(cond)
			start = end +1 ;
	}
	data->Path.Size = i;
	return i;
}

/* Lookup this usage name to find its code (page + index) */
int hid_lookup_usage(char *name)
{	
	int i;
	
	upsdebugx(4, "Looking up %s", name);
	
	if (name[0] == '[') /* manage indexed collection */
		return (0x00FF0000 + atoi(&name[1]));
	else {
		for (i = 0; (usage_lkp[i].usage_code != 0x0); i++)
		{
			if (!strcmp(usage_lkp[i].usage_name, name))
			{
				upsdebugx(4, "hid_lookup_usage: found %04x",
							usage_lkp[i].usage_code);
				return usage_lkp[i].usage_code;
			}
		}
	}
	return -1;
}

/* Get an item value from a HID path */
int hid_get_value(const char *item_path)
{
	int i, retcode;
   HIDData_t hData;
	
	upsdebugx(3, "entering hid_get_value(%s)", item_path);
	
	/* Prepare path of HID object */
	hData.Type = ITEM_FEATURE;
	hData.ReportID = 0;

	if((retcode = lookup_path(item_path, &hData)) > 0) {
		upsdebugx(3, "Path depth = %i\n", retcode);
		
		for (i = 0; i<retcode; i++)
			upsdebugx(4, "%i: Usage(%08x)\n", i, hData.Path.Node[i]);
			
		hData.Path.Size = retcode;
    
		/* Get info on object (reportID, offset and size) */
		if (FindObject(pDesc,&hData) == 1) {
			if (shut_get_report(hData.ReportID, raw_buf, MAX_REPORT_SIZE) > 0) {
				GetValue((const u_char *) raw_buf, &hData, &hValue);
				upsdebug_hex(3, "Object's report", raw_buf, 10);
				upsdebugx(3, "Value = %ld", hValue);
				return 1;
			}
			else
				shut_ups_start();
		}
		else {
			upsdebugx(3, "Can't find object");
			return 0;
		}
	}
	else {
		upsdebugx(3, "Can't lookup object's path");
		return 0;
	}

	return 0;
}

  /* 
   * Internal functions
 ****************************************************************************/

/*
 * Filter and reformat HID strings (suppress space
 * between each letter)
 * Note: string format in HID String Descriptor is
 * Byte1: Size of descriptor(=>string)
 * Byte2: String descriptor type (always 0x03)
 * Byte3 to byteN: UNICODE string (in US: xx 00 for a letter)
 */
void make_string(u_char *buf, int datalen, char *string)
{
	int i,		/* Skip size and type */ 
		j=0;
	
	upsdebugx(4, "String descriptor: size = 0x%02x, type = 0x%02x", buf[0], buf[1]);
	
	/* TODO: add clean support for UNICODE */
	for(i=2;i<datalen;i++) {
		if(buf[i]!=0x00) {      
			string[j]=buf[i];
			j++;
		}
	} 
	string[j++]='\0';
}

/*
 * set RTS to on and DTR to off
 * 
 * set : 1 to set comm
 * set : 0 to stop commupsh.
 */
void setline (int set)
{
	upsdebugx(3, "entering setline(%i)\n", set);

	if (set == 1) {
		ser_set_dtr(upsfd, 0);
		ser_set_rts(upsfd, 1);
	}
	else {
		ser_set_dtr(upsfd, 1);
		ser_set_rts(upsfd, 0);
	}
}

/* exponent function */
float expo(int a, int b)
{
	if (b==0)
		return (float) 1;
	if (b>0)
		return (float) a * expo(a,b-1);
	if (b<0)
		return (float)((float)(1/(float)a) * (float) expo(a,b+1));
	
	/* not reached */
	return -1;
}

/*  Format model names */
char *get_model_name(char *iProduct, char *iModel)
{
  models_name_t *model = NULL;

  upsdebugx(2, "get_model_name(%s, %s)\n", iProduct, iModel);

  /* Search for formatting rules */
  for ( model = models_names ; model->iProduct != NULL ; model++ )
	{
	  upsdebugx(2, "comparing with: %s", model->finalname);
	  if ( (!strncmp(iProduct, model->iProduct, strlen(model->iProduct)))
		   && (!strncmp(iModel, model->iModel, strlen(model->iModel))) )
		{
		  upsdebugx(2, "Found %s\n", model->finalname);
		  break;
		}
	}
  /* FIXME: if we end up with model->iProduct == NULL
   * then process name in a generic way (not yet supported models!)
   */
  return model->finalname;
}

/* set r/w INFO_ element to a value. */
int hid_set_value(const char *varname, const char *val)
{
	int retcode, i, replen;
	mge_info_item_t *shut_info_p;
   HIDData_t hData;
		
	upsdebugx(2, "============== entering hid_set_value(%s, %s) ==============", varname, val);
	
	/* 1) retrieve and check netvar & item_path */	
	shut_info_p = shut_find_info(varname);

	if (shut_info_p == NULL || shut_info_p->type == NULL ||
		!(shut_info_p->flags & SHUT_FLAG_OK))
	{
		upsdebugx(2, "hid_ups_set: info element unavailable %s", varname);
		return STAT_SET_UNKNOWN;
	}

	/* Checking item writability and HID Path */
	if (!shut_info_p->flags & ST_FLAG_RW) {
		upsdebugx(2, "hid_ups_set: not writable %s", varname);
		return STAT_SET_UNKNOWN;
	}

	/* handle server side variable */
	if (shut_info_p->shut_flags & SHUT_FLAG_ABSENT) {
		upsdebugx(2, "hid_ups_set: setting server side variable %s", varname);
		dstate_setinfo(shut_info_p->type, "%s", val);
		return STAT_SET_HANDLED;
	} else {
		/* SHUT_FLAG_ABSENT is the only case of HID Path == NULL */
		if (shut_info_p->item_path == NULL) {
			upsdebugx(2, "hid_ups_set: ID Path is NULL for %s", varname);
			return STAT_SET_UNKNOWN;
		}
	}

	/* Prepare path of HID object */
	hData.Type = ITEM_FEATURE;
	hData.ReportID = 0;

	if((retcode = lookup_path(shut_info_p->item_path, &hData)) > 0) {
		upsdebugx(3, "Path depth = %i\n", retcode);
		
		for (i = 0; i<retcode; i++)
			upsdebugx(4, "%i: Usage(%08x)\n", i, hData.Path.Node[i]);
			
		hData.Path.Size = retcode;
    
		/* Get info on object (reportID, offset and size) */
		if (FindObject(pDesc,&hData) == 1) {
			replen = shut_get_report(hData.ReportID, raw_buf, MAX_REPORT_SIZE);
			
			GetValue((const u_char *) raw_buf, &hData, &hValue);
			
			/* Test if Item is settable */
			if (hData.Attribute != ATTR_DATA_CST) {
				/* Set new value for this item */
				hValue = atol(val);
				SetValue(&hData, raw_buf, hValue);
				shut_set_report(hData.ReportID, raw_buf, replen);
				
				/* check if set succeed ! => disabled for now
				if (shut_get_report(hData.ReportID, raw_buf, MAX_REPORT_SIZE) > 0) {
					GetValue((const u_char *) raw_buf, &hData, &hValue);
					upsdebugx(3, "Value = %d", hValue);
				
					if (hValue != atol(val))
						upsdebugx(3, "FAILED");
					else
						upsdebugx(3, "SUCCEED");
				} else
					upsdebugx(3, "FAILED");
				*/				
				return STAT_SET_HANDLED;
			}
			else
				upsdebugx(3, "Object is constant");
		}
		else
			upsdebugx(3, "Can't find object");
	}
	else
		upsdebugx(3, "Can't lookup object's path");
	
	return STAT_SET_UNKNOWN;
}

/* find info element definition in my info array. */
mge_info_item_t *shut_find_info(const char *varname)
{
	mge_info_item_t *shut_info_p;

	for (shut_info_p = &mge_info[0]; shut_info_p->type != NULL; shut_info_p++)
		if (!strcasecmp(shut_info_p->type, varname))
			return shut_info_p;
		
	fatalx(EXIT_FAILURE, "shut_find_info: unknown info type: %s", varname);
	return NULL;
}

/* find the NUT value matching that HID Item value */
static char *hu_find_infoval(info_lkp_t *hid2info, long value)
{
  info_lkp_t *info_lkp;
  
  upsdebugx(3, "hu_find_infoval: searching for value = %ld\n", value);
  
  for (info_lkp = hid2info; (info_lkp != NULL) &&
	 (strcmp(info_lkp->nut_value, "NULL")); info_lkp++) {
    
    if (info_lkp->hid_value == value) {
      upsdebugx(3, "hu_find_infoval: found %s (value: %ld)\n",
		info_lkp->nut_value, value);
      
      return info_lkp->nut_value;
    }
  }
  upsdebugx(3, "hu_find_infoval: no matching INFO_* value for this HID value (%ld)\n", value);
  return NULL;
}
