/* powerpanel.h - Definitions for CyberPower text protocol UPSes

   Copyright (C) 2007  Arjen de Korte <arjen@de-korte.org>
                       Doug Reynolds <mav@wastegate.net>

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

/*
 * Offsets within the buffer that is returned after a "D\r" command
 */
#define POLL_INPUTVOLT	2	/* input voltage */
#define POLL_OUTPUTVOLT	8	/* output voltage */
#define	POLL_LOAD	14	/* load percentage */
#define	POLL_BATTCHARGE	18	/* battery charge */
#define POLL_TEMP	22	/* temperature */
#define	POLL_FREQUENCY	26	/* frequency */
#define	POLL_STATUS	32	/* status byte */

#define ENDCHAR		'\r'	/* replies end with CR */
#define MAXTRIES	5
#define UPSDELAY	50000	/* 50 ms delay required for reliable operation */

#define SER_WAIT_SEC	3	/* allow 3 sec for ser_get calls */
#define SER_WAIT_USEC	0

#define CPS_STAT_CAL	0x08
#define CPS_STAT_LB	0x20
#define CPS_STAT_OB	0x40
#define CPS_STAT_OL	0x90
