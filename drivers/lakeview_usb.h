/* lakeview_usb.h - driver for UPS with lakeview chipset, such as
   'Sweex Manageable UPS 1000VA' (ca. 2006)

   May also work on 'Kebo UPS-650D', not tested as of 05/23/2007

   Copyright (C) 2007 Peter van Valderen <p.v.valderen@probu.nl>
                      Dirk Teurlings <dirk@upexia.nl>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/* driver version */
#define DRV_VERSION	"0.03"

/* driver definitions */
#define STATUS_REQUESTTYPE	0x21
#define REPLY_REQUESTTYPE	0x81
#define REPLY_PACKETSIZE	6
#define REQUEST_VALUE		0x09
#define MESSAGE_VALUE		0x200
#define INDEX_VALUE		0

/* limit the amount of spew that goes in the syslog when we lose the UPS (from nut_usb.h) */
#define USB_ERR_LIMIT 10        /* start limiting after 10 in a row  */
#define USB_ERR_RATE 10         /* then only print every 10th error */
