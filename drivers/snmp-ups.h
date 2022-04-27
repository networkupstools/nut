/*  snmp-ups.h - NUT Meta SNMP driver (support different MIBS)
 *
 *  Based on NET-SNMP API (Simple Network Management Protocol V1-2)
 *
 *  Copyright (C)
 *   2002-2010  Arnaud Quette <arnaud.quette@free.fr>
 *   2015-2021  Eaton (author: Arnaud Quette <ArnaudQuette@Eaton.com>)
 *   2016-2021  Eaton (author: Jim Klimov <EvgenyKlimov@Eaton.com>)
 *   2002-2006	Dmitry Frolov <frolov@riss-telecom.ru>
 *  			J.W. Hoogervorst <jeroen@hoogervorst.net>
 *  			Niels Baggesen <niels@baggesen.net>
 *
 *  Sponsored by Eaton <http://www.eaton.com>
 *   and originally by MGE UPS SYSTEMS <http://opensource.mgeups.com/>
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

#ifndef ONE_SEC
/* This macro name disappeared from net-snmp sources and headers
 * after v5.9 tag, and was replaced by explicit expression below: */
# define ONE_SEC (1000L * 1000L)
#endif

/* Force numeric OIDs by disabling MIB loading */
#ifdef DISABLE_MIB_LOADING
# undef DISABLE_MIB_LOADING
#endif
#define DISABLE_MIB_LOADING 1

/* Parameters default values */
#define DEFAULT_POLLFREQ          30   /* in seconds */
#define DEFAULT_NETSNMP_RETRIES   5
#define DEFAULT_NETSNMP_TIMEOUT   1    /* in seconds */
#define DEFAULT_SEMISTATICFREQ    10   /* in snmpwalk update cycles */

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

#ifndef WITH_SNMP_LKP_FUN
/* Recent addition of fun/nuf hooks in info_lkp_t is not well handled by
 * all corners of the codebase, e.g. not by DMF. So at least until that
 * is fixed, (TODO) we enable those bits of code only optionally during
 * a build for particular usage. Conversely, experimenters can define
 * this macro to a specific value while building the codebase and see
 * what happens under different conditions ;)
 */
# if (defined WITH_DMFMIB) && (WITH_DMFMIB != 0)
#  define WITH_SNMP_LKP_FUN 0
# else
#  define WITH_SNMP_LKP_FUN 1
# endif
#endif

#ifndef WITH_SNMP_LKP_FUN_DUMMY
# define WITH_SNMP_LKP_FUN_DUMMY 0
#endif

/* for lookup between OID values and INFO_ value */
typedef struct {
	int oid_value;                      /* SNMP OID value */
	const char *info_value;             /* NUT INFO_* value */
#if WITH_SNMP_LKP_FUN
/* FIXME: Currently we do not have a way to provide custom C code
 * via DMF - keep old approach until we get the ability, e.g. by
 * requiring a LUA implementation to be passed alongside C lookups.
 */
/*
 * Currently there are a few cases using a "fun_vp2s" type of lookup
 * function, while the "nuf_s2l" type was added for completeness but
 * is not really handled and does not have real consumers in the
 * existing NUT codebase (static mib2nut tables in *-mib.c files).
 * Related to su_find_infoval() (long* => string), su_find_valinfo()
 * (string => long) and su_find_strval() (char* => string) routines
 * defined below.
 */
	const char *(*fun_vp2s)(void *snmp_value);  /* optional SNMP to NUT mapping function, converting a pointer to SNMP data (e.g. numeric or string) into a NUT string */
	long (*nuf_s2l)(const char *nut_value);     /* optional NUT to SNMP mapping function, converting a NUT string into SNMP numeric data */
#endif /* WITH_SNMP_LKP_FUN */
} info_lkp_t;

/* Structure containing info about one item that can be requested
   from UPS and set in INFO.  If no interpreter functions is defined,
   use sprintf with given format string.  If unit is not NONE, values
   are converted according to the multiplier table
*/
typedef uint32_t snmp_info_flags_t; /* To extend when 32 bits become too congested */
#define PRI_SU_FLAGS	PRIu32

typedef struct {
	char         *info_type;  /* INFO_ or CMD_ element */
	int           info_flags; /* flags to set in addinfo: see ST_FLAG_*
	                           * defined in include/extstate.h */
	double        info_len;   /* length of strings if ST_FLAG_STRING,
	                           * multiplier otherwise. */
	char         *OID;        /* SNMP OID or NULL */
	char         *dfl;        /* default value */
	snmp_info_flags_t flags;  /* snmp-ups internal flags: see SU_* bit-shifts
	                           * defined below (SU_FLAG*, SU_TYPE*, SU_STATUS*
	                           * and others for outlets, phases, daisy-chains,
	                           * etc.)
	                           * NOTE that some *-mib.c mappings can specify
	                           * a zero in this field... better fix that in
	                           * favor of explicit values with a meaning!
	                           * Current code treats such zero values as
	                           * "OK if avail, otherwise discarded".
	                           * NOTE: With C99+ a "long" is guaranteed to be
	                           * at least 4 bytes; consider "unsigned long long"
	                           * when/if we get more than 32 flag values.
	                           */
	info_lkp_t   *oid2info;   /* lookup table between OID and NUT values */
} snmp_info_t;

/* "flags" bits 0..9 */
#define SU_FLAG_OK			(1UL << 0)	/* show element to upsd -
										 * internal to snmp driver */
#define SU_FLAG_STATIC		(1UL << 1)	/* retrieve info only once. */
#define SU_FLAG_ABSENT		(1UL << 2)	/* data is absent in the device,
										 * use default value. */
#define SU_FLAG_STALE		(1UL << 3)	/* data stale, don't try too often -
										 * internal to snmp driver */
#define SU_FLAG_NEGINVALID	(1UL << 4)	/* Invalid if negative value */
#define SU_FLAG_UNIQUE		(1UL << 5)	/* There can be only be one
						 				 * provider of this info,
						 				 * disable the other providers */
/* Note: older releases defined the following flag, but removed it by 2.7.5:
 * #define SU_FLAG_SETINT	(1UL << 6)*/	/* save value */
#define SU_FLAG_ZEROINVALID	(1UL << 6)	/* Invalid if "0" value */
#define SU_FLAG_NAINVALID	(1UL << 7)	/* Invalid if "N/A" value */
#define SU_CMD_OFFSET		(1UL << 8)	/* Add +1 to the OID index */

#define SU_FLAG_SEMI_STATIC	(1UL << 9)	/* Refresh this entry once in several walks
                                      	 * (for R/W values user can set on device,
                                      	 * like descriptions or contacts) */

/* Notes on outlet templates usage:
 * - outlet.count MUST exist and MUST be declared before any outlet template
 * Otherwise, the driver will try to determine it by itself...
 * - the first outlet template MUST NOT be a server side variable (ie MUST have
 *   a valid OID) in order to detect the base SNMP index (0 or 1)
 */

/* "flags" bit 10 */
#define SU_OUTLET_GROUP		(1UL << 10)	/* outlet group template definition */
#define SU_OUTLET			(1UL << 11)	/* outlet template definition */

/* Phase specific data */
/* "flags" bits 12..17 */
#define SU_PHASES			(0x0000003F << 12)
#define SU_INPHASES			(0x00000003 << 12)
#define SU_INPUT_1			(1UL << 12)	/* only if 1 input phase */
#define SU_INPUT_3			(1UL << 13)	/* only if 3 input phases */
#define SU_OUTPHASES		(0x00000003 << 14)
#define SU_OUTPUT_1			(1UL << 14)	/* only if 1 output phase */
#define SU_OUTPUT_3			(1UL << 15)	/* only if 3 output phases */
#define SU_BYPPHASES		(0x00000003 << 16)
#define SU_BYPASS_1			(1UL << 16)	/* only if 1 bypass phase */
#define SU_BYPASS_3			(1UL << 17)	/* only if 3 bypass phases */
/* FIXME: use input.phases and output.phases to replace this */

/* hints for su_ups_set, applicable only to rw vars */
/* "flags" bits 18..20 */
#define SU_TYPE_INT			(1UL << 18)	/* cast to int when setting value */
#define SU_TYPE_TIME		(1UL << 19)	/* cast to int */
#define SU_TYPE_CMD			(1UL << 20)	/* instant command */
/* The following helper macro is used like:
 *   if (SU_TYPE(su_info_p) == SU_TYPE_CMD) { ... }
 */
#define SU_TYPE(t)			((t)->flags & (7UL << 18))

/* Daisychain template definition */
/* the following 2 flags specify the position of the daisychain device index
 * in the formatting string. This is useful when considering daisychain with
 * templates, such as outlets / outlets groups, which already have a format
 * string specifier */
/* "flags" bits 21..23 (and 24 reserved for DMF) */
#define SU_TYPE_DAISY_1		(1UL << 21)	/* Daisychain index is the 1st %i specifier in a template with more than one */
#define SU_TYPE_DAISY_2		(1UL << 22)	/* Daisychain index is the 2nd %i specifier in a template with more than one */
#define SU_TYPE_DAISY(t)	((t)->flags & (11UL << 21))	/* Mask the SU_TYPE_DAISY_{1,2,MASTER_ONLY} but not SU_DAISY */
#define SU_DAISY			(1UL << 23)	/* Daisychain template definition - set at run-time for devices with detected "device.count" over 1 */
/* NOTE: Previously SU_DAISY had same bit-flag value as SU_TYPE_DAISY_2 */
#define SU_TYPE_DAISY_MASTER_ONLY	(1UL << 24)	/* Only valid for daisychain master (device.1) */

/* Free slot: (1UL << 25) */

#define SU_AMBIENT_TEMPLATE	(1UL << 26)	/* ambient template definition */

/* Reserved slot -- to import from DMF branch codebase:
//#define SU_FLAG_FUNCTION	(1UL << 27)
*/

/* status string components
 * FIXME: these should be removed, since there is no added value.
 * Ie, this can be guessed from info->type! */

/* "flags" bits 28..31 */
#define SU_STATUS_PWR		(1UL << 28)	/* indicates power status element */
#define SU_STATUS_BATT		(1UL << 29)	/* indicates battery status element */
#define SU_STATUS_CAL		(1UL << 30)	/* indicates calibration status element */
#define SU_STATUS_RB		(1UL << 31)	/* indicates replace battery status element */
#define SU_STATUS_NUM_ELEM	4			/* Obsolete? No references found in codebase */
#define SU_STATUS_INDEX(t)	(((unsigned long)(t) >> 28) & 15UL)

/* Despite similar names, definitons below are not among the bit-flags ;) */
#define SU_VAR_COMMUNITY	"community"
#define SU_VAR_VERSION		"snmp_version"
#define SU_VAR_RETRIES		"snmp_retries"
#define SU_VAR_TIMEOUT		"snmp_timeout"
#define SU_VAR_SEMISTATICFREQ	"semistaticfreq"
#define SU_VAR_MIBS			"mibs"
#define SU_VAR_POLLFREQ		"pollfreq"
/* SNMP v3 related parameters */
#define SU_VAR_SECLEVEL		"secLevel"
#define SU_VAR_SECNAME		"secName"
#define SU_VAR_AUTHPASSWD	"authPassword"
#define SU_VAR_PRIVPASSWD	"privPassword"
#define SU_VAR_AUTHPROT		"authProtocol"
#define SU_VAR_PRIVPROT		"privProtocol"

#define SU_VAR_ONDELAY		"ondelay"
#define SU_VAR_OFFDELAY		"offdelay"

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
bool_t nut_snmp_get_oid(const char *OID, char *buf, size_t buf_len);
bool_t nut_snmp_get_int(const char *OID, long *pval);
bool_t nut_snmp_set(const char *OID, char type, const char *value);
bool_t nut_snmp_set_str(const char *OID, const char *value);
bool_t nut_snmp_set_int(const char *OID, long value);
bool_t nut_snmp_set_time(const char *OID, long value);
void nut_snmp_perror(struct snmp_session *sess,  int status,
	struct snmp_pdu *response, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 4, 5)));

void su_startup(void);
void su_cleanup(void);
void su_init_instcmds(void);
void su_setuphandlers(void); /* need to deal with external function ptr */
void su_setinfo(snmp_info_t *su_info_p, const char *value);
void su_status_set(snmp_info_t *, long value);
void su_alarm_set(snmp_info_t *, long value);
snmp_info_t *su_find_info(const char *type);
bool_t snmp_ups_walk(int mode);
bool_t su_ups_get(snmp_info_t *su_info_p);

bool_t load_mib2nut(const char *mib);

/* Practical logic around lookup functions, see fun_vp2s and nuf_s2l
 * fields in struct info_lkp_t */
const char *su_find_infoval(info_lkp_t *oid2info, void *value);
long su_find_valinfo(info_lkp_t *oid2info, const char* value);
const char *su_find_strval(info_lkp_t *oid2info, void *value);

/*****************************************************
 * Common conversion structs and functions provided by snmp-ups-helpers.c
 * so they can be used and so "shared" by different subdrivers
 *****************************************************/

const char *su_usdate_to_isodate_info_fun(void *raw_date);
extern info_lkp_t su_convert_to_iso_date_info[];
/* Name the mapping location in that array for consumers to reference */
#define FUNMAP_USDATE_TO_ISODATE 0

/* Process temperature value according to 'temperature_unit' */
const char *su_temperature_read_fun(void *raw_snmp_value);

/* Temperature handling, to convert back to Celsius (NUT standard) */
extern int temperature_unit;

#define TEMPERATURE_UNKNOWN    0
#define TEMPERATURE_CELSIUS    1
#define TEMPERATURE_KELVIN     2
#define TEMPERATURE_FAHRENHEIT 3

/*****************************************************
 * End of Subdrivers shared helpers functions
 *****************************************************/

int su_setvar(const char *varname, const char *val);
int su_instcmd(const char *cmdname, const char *extradata);
void su_shutdown_ups(void);

void set_delays(void);

void read_mibconf(char *mib);

extern struct snmp_session g_snmp_sess, *g_snmp_sess_p;
extern const char *OID_pwr_status;
extern int g_pwr_battery;
extern int pollfreq; /* polling frequency */
extern int input_phases, output_phases, bypass_phases;
extern int semistaticfreq; /* semistatic entry update frequency */

/* pointer to the Snmp2Nut lookup table */
extern mib2nut_info_t *mib2nut_info;
/* FIXME: to be trashed */
extern snmp_info_t *snmp_info;
extern alarms_info_t *alarms_info;

/* Common daisychain structure and functions */

bool_t daisychain_init(void);
int su_addcmd(snmp_info_t *su_info_p);

/* Structure containing info about each daisychain device, including phases
 * for input, output and bypass */
typedef struct {
	long input_phases;
	long output_phases;
	long bypass_phases;
} daisychain_info_t;

#endif /* SNMP_UPS_H */
