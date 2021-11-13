/*  snmp-ups-helpers.c - Shared helper functions and data mapping tables
 *  for NUT Generic SNMP driver core
 *
 *  Copyright (C)
 *	2015 - 2021	Eaton (author: Arnaud Quette <ArnaudQuette@Eaton.com>)
 *	2016 - 2021	Eaton (author: Jim Klimov <EvgenyKlimov@Eaton.com>)
 *
 *  Sponsored by Eaton <http://www.eaton.com>
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <ctype.h> /* for isprint() */

/* NUT SNMP common functions */
#include "main.h"
#include "nut_float.h"
#include "nut_stdint.h"
#include "snmp-ups.h"
#include "timehead.h" /* time.h => strptime() */

/***********************************************************************
 * Subdrivers shared helpers functions
 * Code below is primarily used in snmp-ups driver, but may be part
 * of other compilation units, so separated into a stand-alone file
 **********************************************************************/

static char su_scratch_buf[255];

/* Convert a US formated date (mm/dd/yyyy) to an ISO 8601 Calendar date (yyyy-mm-dd) */
const char *su_usdate_to_isodate_info_fun(void *raw_date)
{
	const char *usdate = (char *)raw_date;
	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));
	memset(&su_scratch_buf, 0, sizeof(su_scratch_buf));

	upsdebugx(3, "%s: US date = %s", __func__, usdate);

	/* Try to convert from US date string to time */
	/* Note strptime returns NULL upon failure, and a ptr to the last
	   null char of the string upon success. Just try blindly the conversion! */
	strptime(usdate, "%m/%d/%Y", &tm);
	if (strftime(su_scratch_buf, 254, "%F", &tm) != 0) {
		upsdebugx(3, "%s: successfully reformated: %s", __func__, su_scratch_buf);
		return su_scratch_buf;
	}

	return NULL;
}

info_lkp_t su_convert_to_iso_date_info[] = {
	/* array index = FUNMAP_USDATE_TO_ISODATE: */
	{ 1, "dummy", su_usdate_to_isodate_info_fun, NULL },
	{ 0, NULL, NULL, NULL }
};
