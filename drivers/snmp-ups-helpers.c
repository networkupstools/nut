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

/* NUT SNMP common functions */
#include "common.h"	/* includes "config.h" which must be the first header */
#include "dmf_stdlib.h"
/*
#include "config.h"
#include "main.h"
#include "nut_float.h"
#include "nut_stdint.h"
*/
#include "snmp-ups.h"
#include "timehead.h" /* time.h => strptime() */

#include <ctype.h> /* for isprint() */

/* Shunt the debugging calls when building self-test DMF driver code */
/* FIXME: Go the next mile to pull common.o etc? We would rather not... */
#ifdef WITH_DMFMIB_SELFTEST
# ifdef upsdebugx
#  undef upsdebugx
# endif
# define upsdebugx(...) do {} while(0)
#endif

/***********************************************************************
 * Subdrivers shared helpers functions
 * Code below is primarily used in snmp-ups driver, but may be part
 * of other compilation units, so separated into a stand-alone file
 **********************************************************************/

/* Temperature handling, to convert back to Celsius */
int temperature_unit = TEMPERATURE_UNKNOWN;

/* Convert a US formated date (mm/dd/yyyy) to an ISO 8601 Calendar date (yyyy-mm-dd) */
const char *su_usdate_to_isodate_info_fun(void *raw_date)
{
	/* Delegate to the shared dmf_stdlib implementation, reused verbatim
	 * by DMF XML mappings and (where enabled) LUA glue code. */
	return nut_usdate_to_isodate_static((const char *)raw_date);
}

info_lkp_t su_convert_to_iso_date_info[] = {
	/* array index = FUNMAP_USDATE_TO_ISODATE: */
	info_lkp_fun_vp2s(1, "dummy", su_usdate_to_isodate_info_fun),
	info_lkp_sentinel
};

#if !(defined WITH_SNMP_LKP_FUN_DUMMY) || !WITH_SNMP_LKP_FUN_DUMMY
/* Process temperature value according to 'temperature_unit' */
const char *su_temperature_read_fun(void *raw_snmp_value)
{
	const long snmp_value = *((long*)raw_snmp_value);

	/* Delegate to the shared dmf_stdlib implementation; NUT_STDLIB_TEMP_*
	 * values are numerically identical to the historical TEMPERATURE_*
	 * macros used by 'temperature_unit' (see snmp-ups.h). */
	const char *result = nut_temperature_deci_to_celsius_static(snmp_value, temperature_unit);

	if (!result)
		upsdebugx(1, "%s: not a known temperature unit for conversion!", __func__);
	else
		upsdebugx(2, "%s: %.1ld => %s", __func__, (snmp_value / 10), result);

	return result;
}
#endif	/* WITH_SNMP_LKP_FUN_DUMMY */
