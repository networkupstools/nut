/*  snmp-ups.h - NUT Meta SNMP driver (support different MIBS)
 *
 *  Based on NET-SNMP API (Simple Network Management Protocol V1-2)
 *
 *  Copyright (C)
 *   2002-2010  Arnaud Quette <arnaud.quette@free.fr>
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
- optimize network flow by:
  1) caching OID values (as in usbhid-ups) with timestamping and lifetime
  2) constructing one big packet (calling snmp_add_null_var
     for each OID request we made), instead of sending many small packets
- add support for registration and traps (manager mode)
  => Issue: 1 trap listener for N snmp-ups drivers!
- complete mib2nut data (add all OID translation to NUT)
- externalize mib2nut data in .m2n files and load at driver startup using parseconf()...
- adjust information logging.

- move numeric OIDs into th mib2nut tables and remove defines
- move mib2nut into c files (à la usbhid-ups)?
- add a claim function and move to usbhid-ups style for specific processing
- rework the flagging system
*/

#ifndef SNMP_UPS_H
#define SNMP_UPS_H

/* FIXME: still needed?
 * workaround for buggy Net-SNMP config */
#ifdef PACKAGE_BUGREPORT
#undef PACKAGE_BUGREPORT
#endif

#ifdef PACKAGE_NAME
#undef PACKAGE_NAME
#endif

#ifdef PACKAGE_VERSION
#undef PACKAGE_VERSION
#endif

#ifdef PACKAGE_STRING
#undef PACKAGE_STRING
#endif

#ifdef PACKAGE_TARNAME
#undef PACKAGE_TARNAME
#endif

#ifdef HAVE_DMALLOC_H
#undef HAVE_DMALLOC_H
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

/* Force numeric OIDs by disabling MIB loading */
#define DISABLE_MIB_LOADING 1

/* Parameters default values */
#define DEFAULT_POLLFREQ          30   /* in seconds */
#define DEFAULT_NETSNMP_RETRIES   5
#define DEFAULT_NETSNMP_TIMEOUT   1    /* in seconds */

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
	int oid_value;                      /* SNMP OID value */
	const char *info_value;             /* NUT INFO_* value */
	const char *(*fun)(int snmp_value); /* optional SNMP to NUT mapping function */
	int (*nuf)(const char *nut_value);  /* optional NUT to SNMP mapping function */
} info_lkp_t;

/* Structure containing info about one item that can be requested
   from UPS and set in INFO.  If no interpreter functions is defined,
   use sprintf with given format string.  If unit is not NONE, values
   are converted according to the multiplier table
*/
typedef struct {
	const char   *info_type;	/* INFO_ or CMD_ element */
	int           info_flags;	/* flags to set in addinfo */
	double        info_len;		/* length of strings if STR,
								 * cmd value if CMD, multiplier otherwise. */
	const char   *OID;			/* SNMP OID or NULL */
	const char   *dfl;			/* default value */
	unsigned long flags;		/* my flags */
	info_lkp_t   *oid2info;		/* lookup table between OID and NUT values */
	int          *setvar;		/* variable to set for SU_FLAG_SETINT */
} snmp_info_t;

#define SU_FLAG_OK			(1 << 0)	/* show element to upsd - internal to snmp driver */
#define SU_FLAG_STATIC		(1 << 1)	/* retrieve info only once. */
#define SU_FLAG_ABSENT		(1 << 2)	/* data is absent in the device,
										 * use default value. */
#define SU_FLAG_STALE		(1 << 3)	/* data stale, don't try too often - internal to snmp driver */
#define SU_FLAG_NEGINVALID	(1 << 4)	/* Invalid if negative value */
#define SU_FLAG_UNIQUE		(1 << 5)	/* There can be only be one
						 				 * provider of this info,
						 				 * disable the other providers */
#define SU_FLAG_SETINT		(1 << 6)	/* save value */
#define SU_OUTLET			(1 << 7)	/* outlet template definition */
#define SU_CMD_OFFSET		(1 << 8)	/* Add +1 to the OID index */
/* Notes on outlet templates usage:
 * - outlet.count MUST exist and MUST be declared before any outlet template
 * Otherwise, the driver will try to determine it by itself...
 * - the first outlet template MUST NOT be a server side variable (ie MUST have
 *   a valid OID) in order to detect the base SNMP index (0 or 1)
 */

/* status string components
 * FIXME: these should be removed, since there is no added value.
 * Ie, this can be guessed from info->type! */
 
#define SU_STATUS_PWR		(0 << 8)	/* indicates power status element */
#define SU_STATUS_BATT		(1 << 8)	/* indicates battery status element */
#define SU_STATUS_CAL		(2 << 8)	/* indicates calibration status element */
#define SU_STATUS_RB		(3 << 8)	/* indicates replace battery status element */
#define SU_STATUS_NUM_ELEM	4
#define SU_STATUS_INDEX(t)	(((t) >> 8) & 7)

#define SU_OUTLET_GROUP     (1 << 10)   /* outlet group template definition */

/* Phase specific data */
#define SU_PHASES		(0x3F << 12)
#define SU_INPHASES		(0x3 << 12)
#define SU_INPUT_1		(1 << 12)	/* only if 1 input phase */
#define SU_INPUT_3		(1 << 13)	/* only if 3 input phases */
#define SU_OUTPHASES	(0x3 << 14)
#define SU_OUTPUT_1		(1 << 14)	/* only if 1 output phase */
#define SU_OUTPUT_3		(1 << 15)	/* only if 3 output phases */
#define SU_BYPPHASES	(0x3 << 16)
#define SU_BYPASS_1		(1 << 16)	/* only if 1 bypass phase */
#define SU_BYPASS_3		(1 << 17)	/* only if 3 bypass phases */
/* FIXME: use input.phases and output.phases to replace this */


/* hints for su_ups_set, applicable only to rw vars */
#define SU_TYPE_INT			(0 << 18)	/* cast to int when setting value */
/* Free slot                (1 << 18) */
#define SU_TYPE_TIME		(2 << 18)	/* cast to int */
#define SU_TYPE_CMD			(3 << 18)	/* instant command */
#define SU_TYPE(t)			((t)->flags & (7 << 18))

/* Daisychain template definition */
/* the following 2 flags specify the position of the daisychain device index
 * in the formatting string. This is useful when considering daisychain with
 * templates, such as outlets / outlets groups, which already have a format
 * string specifier */
#define SU_TYPE_DAISY_1		(1 << 19) /* Daisychain index is the 1st specifier */
#define SU_TYPE_DAISY_2		(2 << 19) /* Daisychain index is the 2nd specifier */
#define SU_TYPE_DAISY		((t)->flags & (7 << 19))
#define SU_DAISY			(2 << 19) /* Daisychain template definition */

#define SU_VAR_COMMUNITY	"community"
#define SU_VAR_VERSION		"snmp_version"
#define SU_VAR_RETRIES		"snmp_retries"
#define SU_VAR_TIMEOUT		"snmp_timeout"
#define SU_VAR_MIBS			"mibs"
#define SU_VAR_POLLFREQ		"pollfreq"
/* SNMP v3 related parameters */
#define SU_VAR_SECLEVEL		"secLevel"
#define SU_VAR_SECNAME		"secName"
#define SU_VAR_AUTHPASSWD	"authPassword"
#define SU_VAR_PRIVPASSWD	"privPassword"
#define SU_VAR_AUTHPROT		"authProtocol"
#define SU_VAR_PRIVPROT		"privProtocol"

#define SU_INFOSIZE		128
#define SU_BUFSIZE		32
#define SU_LARGEBUF		256

#define SU_STALE_RETRY	10	/* retry to retrieve stale element */
				/* after this number of iterations. */
				/* FIXME: this is for *all* elements */
/* modes to snmp_ups_walk. */
#define SU_WALKMODE_INIT	0
#define SU_WALKMODE_UPDATE	1

/* modes for su_setOID */
#define SU_MODE_INSTCMD     1
#define SU_MODE_SETVAR      2

/* log spew limiters */
#define SU_ERR_LIMIT 10	/* start limiting after this many errors in a row  */
#define SU_ERR_RATE 100	/* only print every nth error once limiting starts */

typedef struct {
	const char * OID;
	const char *status_value; /* when not NULL, set ups.status to this */
	const char *alarm_value;  /* when not NULL, set ups.alarm to this */
} alarms_info_t;

typedef struct {
	const char	*mib_name;
	const char	*mib_version;
	const char	*oid_pwr_status;
	const char	*oid_auto_check;	/* FIXME: rename to SysOID */
	snmp_info_t	*snmp_info;			/* pointer to the good Snmp2Nut lookup data */
	const char	*sysOID;			/* OID to match against sysOID, aka MIB
									 * main entry point */
	alarms_info_t	*alarms_info;
} mib2nut_info_t;

/* Common SNMP functions */
void nut_snmp_init(const char *type, const char *hostname);
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
void su_setinfo(snmp_info_t *su_info_p, const char *value);
void su_status_set(snmp_info_t *, long value);
snmp_info_t *su_find_info(const char *type);
bool_t snmp_ups_walk(int mode);
bool_t su_ups_get(snmp_info_t *su_info_p);

bool_t load_mib2nut(const char *mib);

const char *su_find_infoval(info_lkp_t *oid2info, long value);
long su_find_valinfo(info_lkp_t *oid2info, const char* value);

int su_setvar(const char *varname, const char *val);
int su_instcmd(const char *cmdname, const char *extradata);
void su_shutdown_ups(void);

void read_mibconf(char *mib);

extern struct snmp_session g_snmp_sess, *g_snmp_sess_p;
extern const char *OID_pwr_status;
extern int g_pwr_battery;
extern int pollfreq; /* polling frequency */
extern int input_phases, output_phases, bypass_phases;

/* Common daisychain structure and functions */

bool_t daisychain_init();
int su_addcmd(snmp_info_t *su_info_p);

/* Structure containing info about each daisychain device, including phases
 * for input, output and bypass */
typedef struct {
	long input_phases;
	long output_phases;
	long bypass_phases;
} daisychain_info_t;


#endif /* SNMP_UPS_H */

