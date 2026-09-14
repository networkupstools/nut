/* tripplite.h - model specific routines for Tripp Lite SmartUPS models
   (tested with:
   "SMART 700" [on back -- "SmartPro UPS" on front], "SMART700SER")

   tripplite.c was derived from Russell Kroll's bestups.c by Rik Faith.

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
   Copyright (C) 2001  Rickard E. (Rik) Faith <faith@alephnull.com>
   Copyright (C) 2004,2008  Nicholas J. Kain <nicholas@kain.us>

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

#ifndef NUT_TRIPPLITE_H_SEEN
#define NUT_TRIPPLITE_H_SEEN 1

#define ENDCHAR '\n'           /* replies end with CR LF -- use LF to end */
#define IGNCHAR '\r'           /* ignore CR */
#define MAXTRIES 3             /* max number of times we try to detect ups */
#define SER_WAIT_SEC  1        /* allow 1.0 sec for ser_get calls */
#define SER_WAIT_USEC 0
#define DEFAULT_OFFDELAY   64  /* seconds (max 0xFF) */
#define DEFAULT_STARTDELAY 60  /* seconds (max 0xFFFFFF) */
#define DEFAULT_BOOTDELAY  64  /* seconds (max 0xFF) */

/* Sometimes the UPS seems to return bad data, so we range check it. */
#define INVOLT_MAX 270
#define INVOLT_MIN 0
#define FREQ_MAX 80.0
#define FREQ_MIN 30.0
#define TEMP_MAX 100
#define TEMP_MIN 0
#define LOAD_MAX 100
#define LOAD_MIN 0

/* We calculate battery charge (q) as a function of voltage (V).
 * It seems that this function probably varies by firmware revision or
 * UPS model - the Windows monitoring software gives different q for a
 * given V than the old open source Tripp Lite monitoring software.
 *
 * The discharge curve should be the same for any given battery chemistry,
 * so it should only be necessary to specify the minimum and maximum
 * voltages likely to be seen in operation.
 */

/* Interval notation for Q% = 10% <= [minV, maxV] <= 100%  */
#define MAX_VOLT 13.4          /* Max battery voltage (100%) */
#define MIN_VOLT 11.0          /* Min battery voltage (10%) */

#endif	/* NUT_TRIPPLITE_H_SEEN */
