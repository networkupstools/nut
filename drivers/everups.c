/* everups.c - support for Ever UPS models

   Copyright (C) 2001  Bartek Szady <bszx@bszxdomain.edu.eu.org>

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

#define DRIVER_NAME	"Ever UPS driver"
#define DRIVER_VERSION	"0.03"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Bartek Szady <bszx@bszxdomain.edu.eu.org>",
	DRV_STABLE,
	{ NULL }
};

static	unsigned char	upstype = 0;

static void init_serial(void)
{
	ser_set_dtr(upsfd, 0);
	ser_set_rts(upsfd, 0);
}

static int Code(int tries)
{
	unsigned char cRecv;
	do {
		ser_send_char(upsfd, 208);
		ser_get_char(upsfd, &cRecv, 3, 0);
		if (cRecv==208)
			return 1;
	} while (--tries>0);
	return 0;
}

static int InitUpsType(void)
{
	if (Code(1)) {
		ser_send_char(upsfd, 173);
		ser_get_char(upsfd, &upstype, 3, 0);
		return 1;
	} else
		return 0;
}

static const char *GetTypeUpsName(void)
{
        switch(upstype)
        {
	        case 67: return "NET 500-DPC";
		case 68: return "NET 700-DPC";
		case 69: return "NET 1000-DPC";
		case 70: return "NET 1400-DPC";
		case 71: return "NET 2200-DPC";
		case 73: return "NET 700-DPC (new)";
		case 74: return "NET 1000-DPC (new)";
		case 75: return "NET 1400-DPC (new)";
		case 76: return "NET 500-DPC (new)";
		case 81: return "AP 450-PRO";
		case 82: return "AP 650-PRO";
		default:
			return "Unknown";
	}
}

void upsdrv_initinfo(void)
{
	dstate_setinfo("ups.mfr", "Ever");
	dstate_setinfo("ups.model", "%s", GetTypeUpsName());
}

void upsdrv_updateinfo(void)
{
	int	battery=0,standby=0;
	unsigned char recBuf[2];
	unsigned long acuV;
	unsigned long lineV;
	double	fVal;

	if (!Code(2)) {
		upslog_with_errno(LOG_INFO, "Code failed");
		dstate_datastale();
		return;
	}
	/*Line status*/
	ser_send_char(upsfd, 175);
	ser_get_char(upsfd, recBuf, 3, 0);
	if ((recBuf[0] & 1) !=0)
		standby=1;
	else
		battery=(recBuf[0] &4) !=0;
	if (Code(1)) {  /*Accumulator voltage value*/
		ser_send_char(upsfd, 189);
		ser_get_char(upsfd, recBuf, 3, 0);
		acuV=((unsigned long)recBuf[0])*150;
		acuV/=255;
	} else {
		upslog_with_errno(LOG_INFO, "Code failed");
		dstate_datastale();
		return;
	}
	if (Code(1)) {  /*Line voltage*/
		ser_send_char(upsfd, 245);
		ser_get_buf_len(upsfd, recBuf, 2, 3, 0);
		if ( upstype > 72 && upstype < 77)
			lineV=(recBuf[0]*100+recBuf[1]*25600)/352;
		else
			lineV=(recBuf[0]*100+recBuf[1]*25600)/372;
	} else {
		upslog_with_errno(LOG_INFO, "Code failed");
		dstate_datastale();
		return;
	}

	status_init();

	if (battery && acuV<105)
		status_set("LB");	/* low battery */

	if (battery)
		status_set("OB");	/* on battery */
	else
		status_set("OL");	/* on line */

	status_commit();

	dstate_setinfo("input.voltage", "%03ld", lineV);
	dstate_setinfo("battery.voltage", "%03.2f", (double)acuV /10.0);

	fVal=((double)acuV-95.0)*100.0;
	if (standby)
	  fVal/=(135.5-95.0);
	else
	  fVal/=(124.5-95.0);
	if (fVal>100)
		fVal=100;
	else if (fVal<0)
		fVal=0;

	dstate_setinfo("battery.charge", "%03.1f", fVal);

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	if (!Code(2)) {
		upslog_with_errno(LOG_INFO, "Code failed");
		return;
	}
	ser_send_char(upsfd, 28);
	ser_send_char(upsfd, 1);  /* 1.28 sec */
	if (!Code(1)) {
		upslog_with_errno(LOG_INFO, "Code failed");
		return;
	}
	ser_send_char(upsfd, 13);
	ser_send_char(upsfd, 8);
}

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B300);

	init_serial();
	InitUpsType();
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
