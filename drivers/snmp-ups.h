/*  snmp-ups.h - NUT Meta SNMP driver (support different MIBS)
 *
 *  Based on NET-SNMP API (Simple Network Management Protocol V1-2)
 *
 *  Copyright (C) 
 *   2002-2008  Arnaud Quette <arnaud.quette@free.fr>
 *   2002-2006	Dmitry Frolov <frolov@riss-telecom.ru>
  *  			J.W. Hoogervorst <jeroen@hoogervorst.net>
 *  			Niels Baggesen <niels@baggesen.net>
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

/* TODO list:
- add syscontact/location (to all mib.h or centralized?)
- complete shutdown
- add enum values to OIDs.
- optimize network flow by constructing one big packet (calling snmp_add_null_var
for each OID request we made), instead of sending many small packets
- add support for registration and traps (manager mode),
- complete mib2nut data (add all OID translation to NUT)
- externalize mib2nut data in .m2n files and load at driver startup using parseconf()...
- ... and use Net-SNMP lookup mecanism for OIDs (use string path, not numeric)
- adjust information logging.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net-snmp/net-snmp-config.h>
/* workaround for buggy Net-SNMP config */
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_VERSION
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef HAVE_DMALLOC_H
#include <net-snmp/net-snmp-includes.h>

#include "attribute.h"

#define DEFAULT_POLLFREQ	30		/* in seconds */

/* use explicit booleans */
#ifndef FALSE
typedef enum ebool { FALSE, TRUE } bool_t;
#else
typedef int bool_t;
#endif

/* Common SNMP data and lookup definitions */
/* special functions to interpret items: 
   take UPS answer, return string to set in INFO, max len  
   
   NOTE: FFE means For Future Extensions
   
   */
   
/* typedef void (*interpreter)(char *, char *, int); */

/* for lookup between OID values and INFO_ value */
typedef struct {
	int oid_value;		/* OID value */
	const char *info_value;	/* INFO_* value */
} info_lkp_t;

/* Structure containing info about one item that can be requested
   from UPS and set in INFO.  If no interpreter functions is defined,
   use sprintf with given format string.  If unit is not NONE, values
   are converted according to the multiplier table  
*/
typedef struct {
	const char   *info_type;	/* INFO_ or CMD_ element */
	int           info_flags;	/* flags to set in addinfo */
	float         info_len;		/* length of strings if STR, */
					/* cmd value if CMD, multiplier otherwise. */
	const char   *OID;		/* SNMP OID or NULL */
	const char   *dfl;		/* default value */
	unsigned long flags;		/* my flags */
	info_lkp_t   *oid2info;		/* lookup table between OID and NUT values */
	int          *setvar;           /* variable to set for SU_FLAG_SETINT */
} snmp_info_t;

#define SU_FLAG_OK		(1 << 0)	/* show element to upsd. */
#define SU_FLAG_STATIC		(1 << 1)	/* retrieve info only once. */
#define SU_FLAG_ABSENT		(1 << 2)	/* data is absent in the device, */
						/* use default value. */
#define SU_FLAG_STALE		(1 << 3)	/* data stale, don't try too often. */
#define SU_FLAG_NEGINVALID	(1 << 4)	/* Invalid if negative value */
#define SU_FLAG_UNIQUE		(1 << 5)	/* There can be only be one
						 * provider of this info,
						 * disable the other providers
						 */
#define SU_FLAG_SETINT		(1 << 6)	/* save value */

/* status string components */
#define SU_STATUS_PWR		(0 << 8)	/* indicates power status element */
#define SU_STATUS_BATT		(1 << 8)	/* indicates battery status element */
#define SU_STATUS_CAL		(2 << 8)	/* indicates calibration status element */
#define SU_STATUS_RB		(3 << 8)	/* indicates replace battery status element */
#define SU_STATUS_NUM_ELEM	4
#define SU_STATUS_INDEX(t)	(((t) >> 8) & 7)

/* Phase specific data */
#define SU_PHASES		(0xF << 12)
#define SU_INPHASES		(0x3 << 12)
#define SU_INPUT_1		(1 << 12)	/* only if 1 input phase */
#define SU_INPUT_3		(1 << 13)	/* only if 3 input phases */
#define SU_OUTPHASES		(0x3 << 14)
#define SU_OUTPUT_1		(1 << 14)	/* only if 1 output phase */
#define SU_OUTPUT_3		(1 << 15)	/* only if 3 output phases */

/* hints for su_ups_set, applicable only to rw vars */
#define SU_TYPE_INT		(0 << 16)	/* cast to int when setting value */
#define SU_TYPE_STRING		(1 << 16)	/* cast to string */
#define SU_TYPE_TIME		(2 << 16)	/* cast to int */
#define SU_TYPE_CMD		(3 << 16)	/* instant command */
#define SU_TYPE(t)		((t)->flags & (7 << 16))

#define SU_VAR_COMMUNITY	"community"
#define SU_VAR_VERSION		"snmp_version"
#define SU_VAR_MIBS		"mibs"
#define SU_VAR_SDTYPE		"sdtype"
#define SU_VAR_POLLFREQ		"pollfreq"

#define SU_INFOSIZE		128
#define SU_BUFSIZE		32
#define SU_LARGEBUF		256

#define SU_STALE_RETRY	10	/* retry to retrieve stale element */
				/* after this number of iterations. */
/* modes to snmp_ups_walk. */
#define SU_WALKMODE_INIT	0
#define SU_WALKMODE_UPDATE	1

/* log spew limiters */
#define SU_ERR_LIMIT 10	/* start limiting after this many errors in a row  */
#define SU_ERR_RATE 100	/* only print every nth error once limiting starts */

typedef struct {
	const char *mib_name;
	const char *mib_version;
	const char *oid_pwr_status;
	const char *oid_auto_check;
	snmp_info_t *snmp_info; /* pointer to the good Snmp2Nut lookup data */
	
} mib2nut_info_t;

/* Common SNMP functions */
void nut_snmp_init(const char *type, const char *host, const char *version,
		const char *community);
void nut_snmp_cleanup(void);
struct snmp_pdu *nut_snmp_get(const char *OID);
bool_t nut_snmp_get_str(const char *OID, char *buf, size_t buf_len,
	info_lkp_t *oid2info);
bool_t nut_snmp_get_int(const char *OID, long *pval);
bool_t nut_snmp_set(const char *OID, char type, const char *value);
bool_t nut_snmp_set_str(const char *OID, const char *value);
bool_t nut_snmp_set_int(const char *OID, long value);
void nut_snmp_perror(struct snmp_session *sess,  int status,
	struct snmp_pdu *response, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 4, 5)));
		
void su_startup(void);
void su_cleanup(void);
void su_init_instcmds(void);
void su_setuphandlers(void); /* need to deal with external function ptr */
static void disable_transfer_oids(void);
void su_setinfo(const char *type, const char *value, int flags, int auxdata);
void su_status_set(snmp_info_t *, long value);
snmp_info_t *su_find_info(const char *type);
bool_t snmp_ups_walk(int mode);
bool_t su_ups_get(snmp_info_t *su_info_p);

bool_t load_mib2nut(const char *mib);
	
const char *su_find_infoval(info_lkp_t *oid2info, long value);
long su_find_valinfo(info_lkp_t *oid2info, char* value);

int su_setvar(const char *varname, const char *val);
int su_instcmd(const char *cmdname, const char *extradata);
void su_shutdown_ups(void);

void read_mibconf(char *mib);

struct snmp_session g_snmp_sess, *g_snmp_sess_p;
const char *OID_pwr_status;
int g_pwr_battery;
int pollfreq; /* polling frequency */
int input_phases, output_phases;
