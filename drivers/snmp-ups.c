/*  snmp-ups.c - NUT Generic SNMP driver core (supports different MIBs)
 *
 *  Based on NetSNMP API (Simple Network Management Protocol v1-2c-3)
 *
 *  Copyright (C)
 *	2002 - 2014	Arnaud Quette <arnaud.quette@free.fr>
 *	2015 - 2021	Eaton (author: Arnaud Quette <ArnaudQuette@Eaton.com>)
 *	2016 - 2021	Eaton (author: Jim Klimov <EvgenyKlimov@Eaton.com>)
 *	2002 - 2006	Dmitry Frolov <frolov@riss-telecom.ru>
 *			J.W. Hoogervorst <jeroen@hoogervorst.net>
 *			Niels Baggesen <niels@baggesen.net>
 *	2009 - 2010	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *
 *  Sponsored by Eaton <http://www.eaton.com>
 *   and originally by MGE UPS SYSTEMS <http://www.mgeups.com/>
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
#include "main.h"	/* includes "config.h" which must be the first header */
#include "nut_float.h"
#include "nut_stdint.h"
#include "snmp-ups.h"
#include "parseconf.h"

#include <ctype.h> /* for isprint() */

/* include all known mib2nut lookup tables */
#include "apc-mib.h"
#include "mge-mib.h"
#include "netvision-mib.h"
#include "powerware-mib.h"
#include "eaton-pdu-genesis2-mib.h"
#include "eaton-pdu-marlin-mib.h"
#include "eaton-pdu-pulizzi-mib.h"
#include "eaton-pdu-revelation-mib.h"
#include "raritan-pdu-mib.h"
#include "raritan-px2-mib.h"
#include "baytech-mib.h"
#include "compaq-mib.h"
#include "bestpower-mib.h"
#include "cyberpower-mib.h"
#include "delta_ups-mib.h"
#include "huawei-mib.h"
#include "ietf-mib.h"
#include "xppc-mib.h"
#include "eaton-ats16-nmc-mib.h"
#include "eaton-ats16-nm2-mib.h"
#include "apc-ats-mib.h"
#include "apc-pdu-mib.h"
#include "eaton-ats30-mib.h"
#include "emerson-avocent-pdu-mib.h"
#include "hpe-pdu-mib.h"

/* Address API change */
#if ( ! NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol ) && ( ! defined usmAESPrivProtocol )
#define usmAESPrivProtocol usmAES128PrivProtocol
#endif

#ifdef USM_PRIV_PROTO_AES_LEN
# define NUT_securityPrivProtoLen USM_PRIV_PROTO_AES_LEN
#else
# ifdef USM_PRIV_PROTO_AES128_LEN
#  define NUT_securityPrivProtoLen USM_PRIV_PROTO_AES128_LEN
# else
/* FIXME: Find another way to get the size of array(?) to avoid:
 *   error: division 'sizeof (oid * {aka long unsigned int *}) / sizeof (oid {aka long unsigned int})' does not compute the number of array elements [-Werror=sizeof-pointer-div]
 * See also https://bugs.php.net/bug.php?id=37564 for context
 * which is due to most values in /usr/include/net-snmp/librarytransform_oids.h
 * being defined as "oid[10]" or similar arrays, and "backwards compatibility"
 * usmAESPrivProtocol name is an "oid *" pointer.
 */
#  define NUT_securityPrivProtoLen (sizeof(usmAESPrivProtocol)/sizeof(oid))
# endif
#endif

static mib2nut_info_t *mib2nut[] = {
	&apc_ats,			/* This struct comes from : apc-ats-mib.c */
	&apc_pdu_rpdu,		/* This struct comes from : apc-pdu-mib.c */
	&apc_pdu_rpdu2,		/* This struct comes from : apc-pdu-mib.c */
	&apc_pdu_msp,		/* This struct comes from : apc-pdu-mib.c */
	&apc,				/* This struct comes from : apc-mib.c */
	&baytech,			/* This struct comes from : baytech-mib.c */
	&bestpower,			/* This struct comes from : bestpower-mib.c */
	&compaq,			/* This struct comes from : compaq-mib.c */
	&cyberpower,		/* This struct comes from : cyberpower-mib.c */
	&delta_ups,			/* This struct comes from : delta_ups-mib.c */
	&eaton_ats16_nmc,		/* This struct comes from : eaton-ats16-nmc-mib.c */
	&eaton_ats16_nm2,	/* This struct comes from : eaton-ats16-nm2-mib.c */
	&eaton_ats30,		/* This struct comes from : eaton-ats30-mib.c */
	&eaton_marlin,		/* This struct comes from : eaton-mib.c */
	&emerson_avocent_pdu,	/* This struct comes from : emerson-avocent-pdu-mib.c */
	&aphel_revelation,	/* This struct comes from : eaton-mib.c */
	&aphel_genesisII,	/* This struct comes from : eaton-mib.c */
	&pulizzi_switched1,	/* This struct comes from : eaton-mib.c */
	&pulizzi_switched2,	/* This struct comes from : eaton-mib.c */
	&hpe_pdu,			/* This struct comes from : hpe-pdu-mib.c */
	&huawei,			/* This struct comes from : huawei-mib.c */
	&mge,				/* This struct comes from : mge-mib.c */
	&netvision,			/* This struct comes from : netvision-mib.c */
	&powerware,			/* This struct comes from : powerware-mib.c */
	&pxgx_ups,			/* This struct comes from : powerware-mib.c */
	&raritan,			/* This struct comes from : raritan-pdu-mib.c */
	&raritan_px2,		/* This struct comes from : raritan-px2-mib.c */
	&xppc,				/* This struct comes from : xppc-mib.c */
	/*
	 * Prepend vendor specific MIB mappings before IETF, so that
	 * if a device supports both IETF and vendor specific MIB,
	 * the vendor specific one takes precedence (when mibs=auto)
	 */
	&tripplite_ietf,	/* This struct comes from : ietf-mib.c */
	&ietf,				/* This struct comes from : ietf-mib.c */
	/* end of structure. */
	NULL
};

struct snmp_session g_snmp_sess, *g_snmp_sess_p;
const char *OID_pwr_status;
int g_pwr_battery;
int pollfreq; /* polling frequency */
static int quirk_symmetra_threephase = 0;

/* Number of device(s): standard is "1", but daisychain means more than 1 */
static long devices_count = 1;
static int current_device_number = 0;      /* global var to handle daisychain iterations - changed by loops in snmp_ups_walk() and su_addcmd() */
static bool_t daisychain_enabled = FALSE;  /* global var to handle daisychain iterations */
static daisychain_info_t **daisychain_info = NULL;

/* pointer to the Snmp2Nut lookup table */
mib2nut_info_t *mib2nut_info;
/* FIXME: to be trashed */
snmp_info_t *snmp_info;
alarms_info_t *alarms_info;
static const char *mibname;
static const char *mibvers;

#define DRIVER_NAME	"Generic SNMP UPS driver"
#define DRIVER_VERSION		"1.17"

/* driver description structure */
upsdrv_info_t	upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <arnaud.quette@free.fr>\n" \
	"Arnaud Quette <ArnaudQuette@Eaton.com>\n" \
	"Dmitry Frolov <frolov@riss-telecom.ru>\n" \
	"J.W. Hoogervorst <jeroen@hoogervorst.net>\n" \
	"Niels Baggesen <niels@baggesen.net>\n" \
	"Jim Klimov <EvgenyKlimov@Eaton.com>\n" \
	"Arjen de Korte <adkorte-guest@alioth.debian.org>",
	DRV_STABLE,
	{ NULL }
};
/* FIXME: integrate MIBs info? do the same as for usbhid-ups! */

static time_t lastpoll = 0;

/* template OIDs index start with 0 or 1 (estimated stable for a MIB),
 * automatically guessed at the first pass */
static int template_index_base = -1;
/* Not that stable in the end... */
static int device_template_index_base = -1; /* OID index of the 1rst daisychained device */
static int outlet_template_index_base = -1;
static int outletgroup_template_index_base = -1;
static int device_template_offset = -1;

/* sysOID location */
#define SYSOID_OID	".1.3.6.1.2.1.1.2.0"

/* Forward functions declarations */
static void disable_transfer_oids(void);
bool_t get_and_process_data(int mode, snmp_info_t *su_info_p);
int extract_template_number(snmp_info_flags_t template_type, const char* varname);
snmp_info_flags_t get_template_type(const char* varname);

/* ---------------------------------------------
 * driver functions implementations
 * --------------------------------------------- */
void upsdrv_initinfo(void)
{
	snmp_info_t *su_info_p;

	upsdebugx(1, "SNMP UPS driver: entering %s()", __func__);

	dstate_setinfo("driver.version.data", "%s MIB %s", mibname, mibvers);

	/* add instant commands to the info database.
	 * outlet (and groups) commands are processed later, during initial walk */
	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++)
	{
		su_info_p->flags |= SU_FLAG_OK;
		if ((SU_TYPE(su_info_p) == SU_TYPE_CMD)
			&& !(su_info_p->flags & SU_OUTLET)
			&& !(su_info_p->flags & SU_OUTLET_GROUP))
		{
			/* first check that this OID actually exists */
// FIXME: daisychain commands support!
su_addcmd(su_info_p);
/*
			if (nut_snmp_get(su_info_p->OID) != NULL) {
				dstate_addcmd(su_info_p->info_type);
				upsdebugx(1, "upsdrv_initinfo(): adding command '%s'", su_info_p->info_type);
			}
*/
		}
	}

	if (testvar("notransferoids"))
		disable_transfer_oids();

	if (testvar("symmetrathreephase"))
		quirk_symmetra_threephase = 1;
	else
		quirk_symmetra_threephase = 0;

	/* initialize all other INFO_ fields from list */
	if (snmp_ups_walk(SU_WALKMODE_INIT) == TRUE) {
		dstate_dataok();
	}
	else {
		dstate_datastale();
	}

	/* setup handlers for instcmd and setvar functions */
	upsh.setvar = su_setvar;
	upsh.instcmd = su_instcmd;
}

void upsdrv_updateinfo(void)
{
	upsdebugx(1,"SNMP UPS driver: entering %s()", __func__);

	/* only update every pollfreq */
	/* FIXME: only update status (SU_STATUS_*), à la usbhid-ups, in between */
	if (time(NULL) > (lastpoll + pollfreq)) {

		alarm_init();
		status_init();

		/* update all dynamic info fields */
		if (snmp_ups_walk(SU_WALKMODE_UPDATE)) {
			dstate_dataok();
		}
		else {
			dstate_datastale();
		}

		/* Commit status first, otherwise in daisychain mode, "device.0" may
		 * clear the alarm count since it has an empty alarm buffer and if there
		 * is only one device that has alarms! */
		if (daisychain_enabled == FALSE)
			alarm_commit();
		status_commit();
		if (daisychain_enabled == TRUE)
			alarm_commit();

		/* store timestamp */
		lastpoll = time(NULL);
	}
	else {
		/* Just tell everything is ok to upsd */
		dstate_dataok();
	}
}

void upsdrv_shutdown(void)
{
	/*
	This driver will probably never support this. In order to
	be any use, the driver should be called near the end of
	the system halt script. By that time we in all likelyhood
	we won't have network capabilities anymore, so we could
	never send this command to the UPS. This is not an error,
	but a limitation of the interface used.
	*/

	upsdebugx(1, "%s...", __func__);

	/* set shutdown and autostart delay */
	set_delays();
	
	/* Try to shutdown with delay */
	if (su_instcmd("shutdown.return", NULL) == STAT_INSTCMD_HANDLED) {
		/* Shutdown successful */
		return;
	}

	/* If the above doesn't work, try shutdown.reboot */
	if (su_instcmd("shutdown.reboot", NULL) == STAT_INSTCMD_HANDLED) {
		/* Shutdown successful */
		return;
	}

	/* If the above doesn't work, try load.off.delay */
	if (su_instcmd("load.off.delay", NULL) == STAT_INSTCMD_HANDLED) {
		/* Shutdown successful */
		return;
	}

	fatalx(EXIT_FAILURE, "Shutdown failed!");
}

void upsdrv_help(void)
{
	upsdebugx(1, "entering %s", __func__);
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	upsdebugx(1, "entering %s()", __func__);

	addvar(VAR_VALUE, SU_VAR_MIBS,
		"NOTE: You can run the driver binary with '-x mibs=--list' for an up to date listing)\n"
		"Set MIB compliance (default=ietf, allowed: mge,apcc,netvision,pw,cpqpower,...)");
	addvar(VAR_VALUE | VAR_SENSITIVE, SU_VAR_COMMUNITY,
		"Set community name (default=public)");
	addvar(VAR_VALUE, SU_VAR_VERSION,
		"Set SNMP version (default=v1, allowed: v2c,v3)");
	addvar(VAR_VALUE, SU_VAR_POLLFREQ,
		"Set polling frequency in seconds, to reduce network flow (default=30)");
	addvar(VAR_VALUE, SU_VAR_RETRIES,
		"Specifies the number of Net-SNMP retries to be used in the requests (default=5)");
	addvar(VAR_VALUE, SU_VAR_TIMEOUT,
		"Specifies the Net-SNMP timeout in seconds between retries (default=1)");
	addvar(VAR_FLAG, "notransferoids",
		"Disable transfer OIDs (use on APCC Symmetras)");
	addvar(VAR_FLAG, "symmetrathreephase",
		"Enable APCC three phase Symmetra quirks (use on APCC three phase Symmetras)");
	addvar(VAR_VALUE, SU_VAR_SECLEVEL,
		"Set the securityLevel used for SNMPv3 messages (default=noAuthNoPriv, allowed: authNoPriv,authPriv)");
	addvar(VAR_VALUE | VAR_SENSITIVE, SU_VAR_SECNAME,
		"Set the securityName used for authenticated SNMPv3 messages (no default)");
	addvar(VAR_VALUE | VAR_SENSITIVE, SU_VAR_AUTHPASSWD,
		"Set the authentication pass phrase used for authenticated SNMPv3 messages (no default)");
	addvar(VAR_VALUE | VAR_SENSITIVE, SU_VAR_PRIVPASSWD,
		"Set the privacy pass phrase used for encrypted SNMPv3 messages (no default)");

	/* Construct addvar() for SU_VAR_AUTHPROT: */
	{ int comma = 0;
	char tmp_buf[SU_LARGEBUF];
	char *p = tmp_buf;
	char *pn; /* proto name to add */
	size_t remain = sizeof(tmp_buf) - 1;
	int ret;
	NUT_UNUSED_VARIABLE(comma); // potentially, if no protocols are available

	tmp_buf[0] = '\0';

	ret = snprintf(p, remain, "%s",
		"Set the authentication protocol (");
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar()");
	}
	p += ret;
	remain -= (size_t)ret;

#if NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol
	pn = "MD5";
	ret = snprintf(p, remain, "%s%s", (comma++ ? ", " : ""), pn );
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar(%s)", pn);
	}
	p += ret;
	remain -= (size_t)ret;
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol
	pn = "SHA";
	ret = snprintf(p, remain, "%s%s", (comma++ ? ", " : ""), pn );
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar(%s)", pn);
	}
	p += ret;
	remain -= (size_t)ret;
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol
	pn = "SHA256";
	ret = snprintf(p, remain, "%s%s", (comma++ ? ", " : ""), pn );
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar(%s)", pn);
	}
	p += ret;
	remain -= (size_t)ret;
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol
	pn = "SHA384";
	ret = snprintf(p, remain, "%s%s", (comma++ ? ", " : ""), pn );
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar(%s)", pn);
	}
	p += ret;
	remain -= (size_t)ret;
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol
	pn = "SHA512";
	ret = snprintf(p, remain, "%s%s", (comma++ ? ", " : ""), pn );
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar(%s)", pn);
	}
	p += ret;
	remain -= (size_t)ret;
#endif

	pn = "none supported";
	ret = snprintf(p, remain, "%s", (comma++ ? "" : pn) );
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar(%s)", pn);
	}
	p += ret;
	remain -= (size_t)ret;

	ret = snprintf(p, remain, "%s",
		") used for authenticated SNMPv3 messages (default=MD5 if available)");
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar()");
	}
	p += ret;
	remain -= (size_t)ret;

	addvar(VAR_VALUE, SU_VAR_AUTHPROT, tmp_buf);
	} /* Construct addvar() for AUTHPROTO */

	/* Construct addvar() for SU_VAR_PRIVPROT: */
	{ int comma = 0;
	char tmp_buf[SU_LARGEBUF];
	char *p = tmp_buf;
	char *pn; /* proto name to add */
	size_t remain = sizeof(tmp_buf) - 1;
	int ret;
	NUT_UNUSED_VARIABLE(comma); // potentially, if no protocols are available

	tmp_buf[0] = '\0';

	ret = snprintf(p, remain, "%s",
		"Set the privacy protocol (");
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar()");
	}
	p += ret;
	remain -= (size_t)ret;

#if NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol
	pn = "DES";
	ret = snprintf(p, remain, "%s%s", (comma++ ? ", " : ""), pn );
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar(%s)", pn);
	}
	p += ret;
	remain -= (size_t)ret;
#endif
#if NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol || NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol
	pn = "AES";
	ret = snprintf(p, remain, "%s%s", (comma++ ? ", " : ""), pn );
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar(%s)", pn);
	}
	p += ret;
	remain -= (size_t)ret;
#endif
#if NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04
# if NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol
	pn = "AES192";
	ret = snprintf(p, remain, "%s%s", (comma++ ? ", " : ""), pn );
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar(%s)", pn);
	}
	p += ret;
	remain -= (size_t)ret;
# endif
# if NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol
	pn = "AES256";
	ret = snprintf(p, remain, "%s%s", (comma++ ? ", " : ""), pn );
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar(%s)", pn);
	}
	p += ret;
	remain -= (size_t)ret;
# endif
#endif /* NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04 */

	pn = "none supported";
	ret = snprintf(p, remain, "%s", (comma++ ? "" : pn) );
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar(%s)", pn);
	}
	p += ret;
	remain -= (size_t)ret;

	ret = snprintf(p, remain, "%s",
		") used for encrypted SNMPv3 messages (default=DES if available)");
	if (ret < 0 || (uintmax_t)ret > (uintmax_t)remain || (uintmax_t)ret > SIZE_MAX) {
		fatalx(EXIT_FAILURE, "Could not addvar()");
	}
	p += ret;
	remain -= (size_t)ret;

	addvar(VAR_VALUE, SU_VAR_PRIVPROT, tmp_buf);
	} /* Construct addvar() for PRIVPROTO */

	addvar(VAR_VALUE, SU_VAR_ONDELAY,
		"Set start delay time after shutdown");
	addvar(VAR_VALUE, SU_VAR_OFFDELAY,
		"Set delay time before shutdown ");
}

void upsdrv_initups(void)
{
	snmp_info_t *su_info_p, *cur_info_p;
	char model[SU_INFOSIZE];
	bool_t status= FALSE;
	const char *mibs;
	int curdev = 0;

	upsdebugx(1, "SNMP UPS driver: entering %s()", __func__);

	/* Retrieve user's parameters */
	mibs = testvar(SU_VAR_MIBS) ? getval(SU_VAR_MIBS) : "auto";
	if (!strcmp(mibs, "--list")) {
		printf("The 'mibs' argument is '%s', so just listing the mappings this driver knows,\n"
		       "and for 'mibs=auto' these mappings will be tried in the following order until\n"
		       "the first one matches your device\n\n", mibs);
		int i;
		printf("%7s\t%-23s\t%-7s\t%-31s\t%-s\n",
			"NUMBER", "MAPPING NAME", "VERSION",
			"ENTRY POINT OID", "AUTO CHECK OID");
		for (i=0; mib2nut[i] != NULL; i++) {
			printf(" %4d \t%-23s\t%7s\t%-31s\t%-s\n", (i+1),
				mib2nut[i]->mib_name		? mib2nut[i]->mib_name : "<NULL>" ,
				mib2nut[i]->mib_version 	? mib2nut[i]->mib_version : "<NULL>" ,
				mib2nut[i]->sysOID  		? mib2nut[i]->sysOID : "<NULL>" ,
				mib2nut[i]->oid_auto_check	? mib2nut[i]->oid_auto_check : "<NULL>" );
		}
		printf("\nOverall this driver has loaded %d MIB-to-NUT mapping tables\n", i);
		exit(EXIT_SUCCESS);
	}

	/* init SNMP library, etc... */
	nut_snmp_init(progname, device_path);

	/* FIXME: first test if the device is reachable to avoid timeouts! */

	/* Load the SNMP to NUT translation data */
	load_mib2nut(mibs);

	/* init polling frequency */
	if (getval(SU_VAR_POLLFREQ))
		pollfreq = atoi(getval(SU_VAR_POLLFREQ));
	else
		pollfreq = DEFAULT_POLLFREQ;

	/* Get UPS Model node to see if there's a MIB */
/* FIXME: extend and use match_model_OID(char *model) */
	su_info_p = su_find_info("ups.model");
	/* Try to get device.model if ups.model is not available */
	if (su_info_p == NULL)
		su_info_p = su_find_info("device.model");

	if (su_info_p != NULL) {
		/* Daisychain specific: we may have a template (including formatting
		 * string) that needs to be adapted! */
		if (strchr(su_info_p->OID, '%') != NULL)
		{
			upsdebugx(2, "Found template, need to be adapted");
			cur_info_p = (snmp_info_t *)malloc(sizeof(snmp_info_t));
			cur_info_p->info_type = (char *)xmalloc(SU_INFOSIZE);
			cur_info_p->OID = (char *)xmalloc(SU_INFOSIZE);
			snprintf((char*)cur_info_p->info_type, SU_INFOSIZE, "%s", su_info_p->info_type);
			/* Use the daisychain master (0) / 1rst device index */
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
			snprintf((char*)cur_info_p->OID, SU_INFOSIZE, su_info_p->OID, 0);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
		}
		else {
			upsdebugx(2, "Found entry, not a template %s", su_info_p->OID);
			/* Otherwise, just point at what we found */
			cur_info_p = su_info_p;
		}

		/* Actually get the data */
		status = nut_snmp_get_str(cur_info_p->OID, model, sizeof(model), NULL);

		/* Free our malloc, if it was dynamic */
		if (strchr(su_info_p->OID, '%') != NULL) {
			if (cur_info_p->info_type != NULL)
				free((char*)cur_info_p->info_type);
			if (cur_info_p->OID != NULL)
				free((char*)cur_info_p->OID);
			if (cur_info_p != NULL)
				free((char*)cur_info_p);
		}
	}

	if (status == TRUE)
		upslogx(0, "Detected %s on host %s (mib: %s %s)",
			 model, device_path, mibname, mibvers);
	else
		fatalx(EXIT_FAILURE, "%s MIB wasn't found on %s", mibs, g_snmp_sess.peername);
		/* FIXME: "No supported device detected" */

	/* Init daisychain and check if support is required */
	daisychain_init();

	/* Allocate / init the daisychain info structure (for phases only for now)
	 * daisychain_info[0] is the whole chain! (added +1) */
	daisychain_info = (daisychain_info_t**)malloc(
		sizeof(daisychain_info_t) * (size_t)(devices_count + 1)
		);
	for (curdev = 0 ; curdev <= devices_count ; curdev++) {
		daisychain_info[curdev] = (daisychain_info_t*)malloc(sizeof(daisychain_info_t));
		daisychain_info[curdev]->input_phases = (long)-1;
		daisychain_info[curdev]->output_phases = (long)-1;
		daisychain_info[curdev]->bypass_phases = (long)-1;
	}

	/* FIXME: also need daisychain awareness (so init)!
	 * i.e load.off.delay+load.off + device.1.load.off.delay+device.1.load.off + ... */
/* FIXME: daisychain commands support! */
	if (su_find_info("load.off.delay")) {
		/* Adds default with a delay value of '0' (= immediate) */
		dstate_addcmd("load.off");
	}

	if (su_find_info("load.on.delay")) {
		/* Adds default with a delay value of '0' (= immediate) */
		dstate_addcmd("load.on");
	}

	if (su_find_info("load.off.delay") && su_find_info("load.on.delay")) {
		/* Add composite instcmds (require setting multiple OID values) */
		dstate_addcmd("shutdown.return");
		dstate_addcmd("shutdown.stayoff");
	}

	/* set shutdown and autostart delay */
	set_delays();
}

void upsdrv_cleanup(void)
{
	/* General cleanup */
	if (daisychain_info)
		free(daisychain_info);

	/* Net-SNMP specific cleanup */
	nut_snmp_cleanup();
}

/* -----------------------------------------------------------
 * SNMP functions.
 * ----------------------------------------------------------- */

void nut_snmp_init(const char *type, const char *hostname)
{
	char *ns_options = NULL;
	const char *community, *version;
	const char *secLevel = NULL, *authPassword, *privPassword;
	const char *authProtocol, *privProtocol;
	int snmp_retries = DEFAULT_NETSNMP_RETRIES;
	long snmp_timeout = DEFAULT_NETSNMP_TIMEOUT;

	upsdebugx(2, "SNMP UPS driver: entering %s(%s)", __func__, type);

	/* Force numeric OIDs resolution (ie, do not resolve to textual names)
	 * This is mostly for the convenience of debug output */
	ns_options = snmp_out_toggle_options("n");
	if (ns_options != NULL) {
		upsdebugx(2, "Failed to enable numeric OIDs resolution");
	}

	/* Initialize the SNMP library */
	init_snmp(type);

	/* Initialize session */
	snmp_sess_init(&g_snmp_sess);

	g_snmp_sess.peername = xstrdup(hostname);

	/* Net-SNMP timeout and retries */
	if (testvar(SU_VAR_RETRIES)) {
		snmp_retries = atoi(getval(SU_VAR_RETRIES));
	}
	g_snmp_sess.retries = snmp_retries;
	upsdebugx(2, "Setting SNMP retries to %i", snmp_retries);

	if (testvar(SU_VAR_TIMEOUT)) {
		snmp_timeout = atol(getval(SU_VAR_TIMEOUT));
	}
	/* We have to convert from seconds to microseconds */
	g_snmp_sess.timeout = snmp_timeout * ONE_SEC;
	upsdebugx(2, "Setting SNMP timeout to %ld second(s)", snmp_timeout);

	/* Retrieve user parameters */
	version = testvar(SU_VAR_VERSION) ? getval(SU_VAR_VERSION) : "v1";

/* Older CLANG (e.g. clang-3.4) sees short strings in str{n}cmp()
 * arguments as arrays and claims out-of-bounds accesses
 */
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_ARRAY_BOUNDS)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Warray-bounds"
#endif
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Warray-bounds"
#endif
	if ((strncmp(version, "v1", 2) == 0) || (strncmp(version, "v2c", 3) == 0)) {
		g_snmp_sess.version = (strncmp(version, "v1", 2) == 0) ? SNMP_VERSION_1 : SNMP_VERSION_2c;
		community = testvar(SU_VAR_COMMUNITY) ? getval(SU_VAR_COMMUNITY) : "public";
		g_snmp_sess.community = (unsigned char *)xstrdup(community);
		g_snmp_sess.community_len = strlen(community);
	}
	else if (strncmp(version, "v3", 2) == 0) {
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_ARRAY_BOUNDS)
# pragma GCC diagnostic pop
#endif
		/* SNMP v3 related init */
		g_snmp_sess.version = SNMP_VERSION_3;

		/* Security level */
		if (testvar(SU_VAR_SECLEVEL)) {
			secLevel = getval(SU_VAR_SECLEVEL);

			if (strcmp(secLevel, "noAuthNoPriv") == 0)
				g_snmp_sess.securityLevel = SNMP_SEC_LEVEL_NOAUTH;
			else if (strcmp(secLevel, "authNoPriv") == 0)
				g_snmp_sess.securityLevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
			else if (strcmp(secLevel, "authPriv") == 0)
				g_snmp_sess.securityLevel = SNMP_SEC_LEVEL_AUTHPRIV;
			else
				fatalx(EXIT_FAILURE, "Bad SNMPv3 securityLevel: %s", secLevel);
		}
		else
			g_snmp_sess.securityLevel = SNMP_SEC_LEVEL_NOAUTH;

		/* Security name */
		if (testvar(SU_VAR_SECNAME)) {
			g_snmp_sess.securityName = xstrdup(getval(SU_VAR_SECNAME));
			g_snmp_sess.securityNameLen = strlen(g_snmp_sess.securityName);
		}
		else
			fatalx(EXIT_FAILURE, "securityName is required for SNMPv3");

		/* Process mandatory fields, based on the security level */
		authPassword = testvar(SU_VAR_AUTHPASSWD) ? getval(SU_VAR_AUTHPASSWD) : NULL;
		privPassword = testvar(SU_VAR_PRIVPASSWD) ? getval(SU_VAR_PRIVPASSWD) : NULL;

		switch (g_snmp_sess.securityLevel) {
			case SNMP_SEC_LEVEL_AUTHNOPRIV:
				if (authPassword == NULL)
					fatalx(EXIT_FAILURE, "authPassword is required for SNMPv3 in %s mode", secLevel);
				break;
			case SNMP_SEC_LEVEL_AUTHPRIV:
				if ((authPassword == NULL) || (privPassword == NULL))
					fatalx(EXIT_FAILURE, "authPassword and privPassword are required for SNMPv3 in %s mode", secLevel);
				break;
			default:
			case SNMP_SEC_LEVEL_NOAUTH:
				/* nothing else needed */
				break;
		}

		/* Process authentication protocol and key */
		g_snmp_sess.securityAuthKeyLen = USM_AUTH_KU_LEN;
		authProtocol = testvar(SU_VAR_AUTHPROT) ? getval(SU_VAR_AUTHPROT) : "MD5";

#if NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol
		if (strncmp(authProtocol, "MD5", 3) == 0) {
			g_snmp_sess.securityAuthProto = usmHMACMD5AuthProtocol;
			g_snmp_sess.securityAuthProtoLen = sizeof(usmHMACMD5AuthProtocol)/sizeof(oid);
		}
		else
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol
		if (strncmp(authProtocol, "SHA", 3) == 0) {
			g_snmp_sess.securityAuthProto = usmHMACSHA1AuthProtocol;
			g_snmp_sess.securityAuthProtoLen = sizeof(usmHMACSHA1AuthProtocol)/sizeof(oid);
		}
		else
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol
		if (strcmp(authProtocol, "SHA256") == 0) {
			g_snmp_sess.securityAuthProto = usmHMAC192SHA256AuthProtocol;
			g_snmp_sess.securityAuthProtoLen = sizeof(usmHMAC192SHA256AuthProtocol)/sizeof(oid);
		}
		else
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol
		if (strcmp(authProtocol, "SHA384") == 0) {
			g_snmp_sess.securityAuthProto = usmHMAC256SHA384AuthProtocol;
			g_snmp_sess.securityAuthProtoLen = sizeof(usmHMAC256SHA384AuthProtocol)/sizeof(oid);
		}
		else
#endif
#if NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol
		if (strcmp(authProtocol, "SHA512") == 0) {
			g_snmp_sess.securityAuthProto = usmHMAC384SHA512AuthProtocol;
			g_snmp_sess.securityAuthProtoLen = sizeof(usmHMAC384SHA512AuthProtocol)/sizeof(oid);
		}
		else
#endif
			fatalx(EXIT_FAILURE, "Bad SNMPv3 authProtocol: %s", authProtocol);

		/* set the authentication key to a MD5/SHA1 hashed version of our
		 * passphrase (must be at least 8 characters long) */
		if (g_snmp_sess.securityLevel != SNMP_SEC_LEVEL_NOAUTH) {
			if (generate_Ku(g_snmp_sess.securityAuthProto,
				g_snmp_sess.securityAuthProtoLen,
				(const unsigned char *) authPassword, strlen(authPassword),
				g_snmp_sess.securityAuthKey,
				&g_snmp_sess.securityAuthKeyLen) !=
				SNMPERR_SUCCESS) {
				fatalx(EXIT_FAILURE, "Error generating Ku from authentication pass phrase");
			}
		}

		privProtocol = testvar(SU_VAR_PRIVPROT) ? getval(SU_VAR_PRIVPROT) : "DES";

#if NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol
		if (strncmp(privProtocol, "DES", 3) == 0) {
			g_snmp_sess.securityPrivProto = usmDESPrivProtocol;
			g_snmp_sess.securityPrivProtoLen =  sizeof(usmDESPrivProtocol)/sizeof(oid);
		}
		else
#endif
#if NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol || NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol
		if (strncmp(privProtocol, "AES", 3) == 0) {
			g_snmp_sess.securityPrivProto = usmAESPrivProtocol;
			g_snmp_sess.securityPrivProtoLen = NUT_securityPrivProtoLen;
		}
		else
#endif
#if NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04
# if NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol
		if (strcmp(privProtocol, "AES192") == 0) {
			g_snmp_sess.securityPrivProto = usmAES192PrivProtocol;
			g_snmp_sess.securityPrivProtoLen = (sizeof(usmAES192PrivProtocol)/sizeof(oid));
		}
		else
# endif
# if NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol
		if (strcmp(privProtocol, "AES256") == 0) {
			g_snmp_sess.securityPrivProto = usmAES256PrivProtocol;
			g_snmp_sess.securityPrivProtoLen = (sizeof(usmAES256PrivProtocol)/sizeof(oid));
		}
		else
# endif
#endif /* NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04 */
			fatalx(EXIT_FAILURE, "Bad SNMPv3 privProtocol: %s", privProtocol);

		/* set the privacy key to a MD5/SHA1 hashed version of our
		 * passphrase (must be at least 8 characters long) */
		if (g_snmp_sess.securityLevel == SNMP_SEC_LEVEL_AUTHPRIV) {
			g_snmp_sess.securityPrivKeyLen = USM_PRIV_KU_LEN;
			if (generate_Ku(g_snmp_sess.securityAuthProto,
				g_snmp_sess.securityAuthProtoLen,
				(const unsigned char *) privPassword, strlen(privPassword),
				g_snmp_sess.securityPrivKey,
				&g_snmp_sess.securityPrivKeyLen) !=
				SNMPERR_SUCCESS) {
				fatalx(EXIT_FAILURE, "Error generating Ku from privacy pass phrase");
			}
		}
	}
	else
		fatalx(EXIT_FAILURE, "Bad SNMP version: %s", version);

	/* Open the session */
	SOCK_STARTUP; /* MS Windows wrapper, not really needed on Unix! */
	g_snmp_sess_p = snmp_open(&g_snmp_sess);	/* establish the session */
	if (g_snmp_sess_p == NULL) {
		nut_snmp_perror(&g_snmp_sess, 0, NULL, "nut_snmp_init: snmp_open");
		fatalx(EXIT_FAILURE, "Unable to establish communication");
	}
}

void nut_snmp_cleanup(void)
{
	/* close snmp session. */
	if (g_snmp_sess_p) {
		snmp_close(g_snmp_sess_p);
		g_snmp_sess_p = NULL;
	}
	SOCK_CLEANUP; /* wrapper not needed on Unix! */
}

/* Free a struct snmp_pdu * returned by nut_snmp_walk */
static void nut_snmp_free(struct snmp_pdu ** array_to_free)
{
	struct snmp_pdu ** current_element;

	if (array_to_free != NULL) {
		current_element = array_to_free;

		while (*current_element != NULL) {
			snmp_free_pdu(*current_element);
			current_element++;
		}

		free( array_to_free );
	}
}

/* Return a NULL terminated array of snmp_pdu * */
static struct snmp_pdu **nut_snmp_walk(const char *OID, int max_iteration)
{
	int status;
	struct snmp_pdu *pdu, *response = NULL;
	oid name[MAX_OID_LEN];
	size_t name_len = MAX_OID_LEN;
	oid * current_name;
	size_t current_name_len;
	static unsigned int numerr = 0;
	int nb_iteration = 0;
	struct snmp_pdu ** ret_array = NULL;
	int type = SNMP_MSG_GET;

	upsdebugx(3, "%s(%s)", __func__, OID);
	upsdebugx(4, "%s: max. iteration = %i", __func__, max_iteration);

	/* create and send request. */
	if (!snmp_parse_oid(OID, name, &name_len)) {
		upsdebugx(2, "[%s] %s: %s: %s",
			upsname?upsname:device_name, __func__, OID, snmp_api_errstring(snmp_errno));
		return NULL;
	}

	current_name = name;
	current_name_len = name_len;

	while( nb_iteration < max_iteration ) {
		/* Going to a shorter OID means we are outside our sub-tree */
		if( current_name_len < name_len ) {
			break;
		}

		pdu = snmp_pdu_create(type);

		if (pdu == NULL) {
			fatalx(EXIT_FAILURE, "Not enough memory");
		}

		snmp_add_null_var(pdu, current_name, current_name_len);

		status = snmp_synch_response(g_snmp_sess_p, pdu, &response);

		if (!response) {
			break;
		}

		if (!((status == STAT_SUCCESS) && (response->errstat == SNMP_ERR_NOERROR))) {
			if (mibname == NULL) {
				/* We are probing for proper mib - ignore errors */
				snmp_free_pdu(response);
				nut_snmp_free(ret_array);
				return NULL;
			}

			numerr++;

			if ((numerr == SU_ERR_LIMIT) || ((numerr % SU_ERR_RATE) == 0)) {
				upslogx(LOG_WARNING, "[%s] Warning: excessive poll "
						"failures, limiting error reporting (OID = %s)",
						upsname?upsname:device_name, OID);
			}

			if ((numerr < SU_ERR_LIMIT) || ((numerr % SU_ERR_RATE) == 0)) {
				if (type == SNMP_MSG_GETNEXT) {
					upsdebugx(2, "=> No more OID, walk complete");
				}
				else {
					nut_snmp_perror(g_snmp_sess_p, status, response,
							"%s: %s", __func__, OID);
				}
			}

			snmp_free_pdu(response);
			break;
		} else {
			numerr = 0;
		}

		nb_iteration++;
		/* +1 is for the terminating NULL */
		struct snmp_pdu ** new_ret_array = realloc(
			ret_array,
			sizeof(struct snmp_pdu*) * ((size_t)nb_iteration+1)
			);
		if (new_ret_array == NULL) {
			upsdebugx(1, "%s: Failed to realloc thread", __func__);
			break;
		}
		else {
			ret_array = new_ret_array;
		}
		ret_array[nb_iteration-1] = response;
		ret_array[nb_iteration]=NULL;

		current_name = response->variables->name;
		current_name_len = response->variables->name_length;

		type = SNMP_MSG_GETNEXT;
	}

	return ret_array;
}

struct snmp_pdu *nut_snmp_get(const char *OID)
{
	struct snmp_pdu ** pdu_array;
	struct snmp_pdu * ret_pdu;

	if (OID == NULL)
		return NULL;

	upsdebugx(3, "%s(%s)", __func__, OID);

	pdu_array = nut_snmp_walk(OID,1);

	if(pdu_array == NULL) {
		return NULL;
	}

	ret_pdu = snmp_clone_pdu(*pdu_array);

	nut_snmp_free(pdu_array);

	return ret_pdu;
}

static bool_t decode_str(struct snmp_pdu *pdu, char *buf, size_t buf_len, info_lkp_t *oid2info)
{
	size_t len = 0;
	char tmp_buf[SU_LARGEBUF];

	/* zero out buffer. */
	memset(buf, 0, buf_len);

	switch (pdu->variables->type) {
	case ASN_OCTET_STR:
	case ASN_OPAQUE:
		len = pdu->variables->val_len > buf_len - 1 ?
			buf_len - 1 : pdu->variables->val_len;
		/* Test for hexadecimal values */
		int hex = 0, x;
		unsigned char *cp;
		for(cp = pdu->variables->val.string, x = 0; x < (int)pdu->variables->val_len; x++, cp++) {
			if (!(isprint(*cp) || isspace(*cp))) {
				hex = 1;
			}
		}
		if (hex)
			snprint_hexstring(buf, buf_len, pdu->variables->val.string, pdu->variables->val_len);
		else {
			memcpy(buf, pdu->variables->val.string, len);
			buf[len] = '\0';
		}
		break;
	case ASN_INTEGER:
	case ASN_COUNTER:
	case ASN_GAUGE:
		if(oid2info) {
			const char *str;
			/* See union netsnmp_vardata in net-snmp/types.h: "integer" is a "long*" */
			assert(sizeof(pdu->variables->val.integer) == sizeof(long*));
			/* If in future net-snmp headers val becomes not-a-pointer,
			 * compiler should complain about (void*) arg casting here */
			if((str = su_find_infoval(oid2info, pdu->variables->val.integer))) {
				strncpy(buf, str, buf_len-1);
			}
			/* when oid2info returns NULL, don't publish the variable! */
			else {
				/* strncpy(buf, "UNKNOWN", buf_len-1); */
				return FALSE;
			}
			buf[buf_len-1]='\0';
		}
		else {
			int ret = snprintf(buf, buf_len, "%ld", *pdu->variables->val.integer);
			if (ret < 0)
				upsdebugx(3, "Failed to retrieve ASN_GAUGE");
			else
				len = (size_t)ret;
		}
		break;
	case ASN_TIMETICKS:
		/* convert timeticks to seconds */
		{
			int ret = snprintf(buf, buf_len, "%ld", *pdu->variables->val.integer / 100);
			if (ret < 0)
				upsdebugx(3, "Failed to retrieve ASN_TIMETICKS");
			else
				len = (size_t)ret;
		}
		break;
	case ASN_OBJECT_ID:
		snprint_objid (tmp_buf, sizeof(tmp_buf), pdu->variables->val.objid, pdu->variables->val_len / sizeof(oid));
		upsdebugx(2, "Received an OID value: %s", tmp_buf);
		/* Try to get the value of the pointed OID */
		if (nut_snmp_get_str(tmp_buf, buf, buf_len, oid2info) == FALSE) {
			upsdebugx(3, "Failed to retrieve OID value, using fallback");
			/* Otherwise return the last part of the returned OID (ex: 1.2.3 => 3) */
			char *oid_leaf = strrchr(tmp_buf, '.');
			snprintf(buf, buf_len, "%s", oid_leaf+1);
			upsdebugx(3, "Fallback value: %s", buf);
		}
		else
			snprintf(buf, buf_len, "%s", tmp_buf);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

bool_t nut_snmp_get_str(const char *OID, char *buf, size_t buf_len, info_lkp_t *oid2info)
{
	struct snmp_pdu *pdu;
	bool_t ret;

	upsdebugx(3, "Entering %s()", __func__);

	pdu = nut_snmp_get(OID);
	if (pdu == NULL)
		return FALSE;

	ret = decode_str(pdu,buf,buf_len,oid2info);

	if(ret == FALSE) {
		upsdebugx(2, "[%s] unhandled ASN 0x%x received from %s",
			upsname?upsname:device_name, pdu->variables->type, OID);
	}

	snmp_free_pdu(pdu);

	return ret;
}


static bool_t decode_oid(struct snmp_pdu *pdu, char *buf, size_t buf_len)
{
	/* zero out buffer. */
	memset(buf, 0, buf_len);

	switch (pdu->variables->type) {
		case ASN_OBJECT_ID:
			snprint_objid (buf, buf_len, pdu->variables->val.objid,
				pdu->variables->val_len / sizeof(oid));
			upsdebugx(2, "OID value: %s", buf);
			break;
		default:
			return FALSE;
	}

	return TRUE;
}

/* Return the value stored in OID, which is an OID (sysOID for example)
 * and don't try to get the value pointed by this OID (no follow).
 * To achieve the latter behavior, use standard nut_snmp_get_{str,int}() */
bool_t nut_snmp_get_oid(const char *OID, char *buf, size_t buf_len)
{
	struct snmp_pdu *pdu;
	bool_t ret = FALSE;

	/* zero out buffer. */
	memset(buf, 0, buf_len);

	upsdebugx(3, "Entering %s()", __func__);

	pdu = nut_snmp_get(OID);
	if (pdu == NULL)
		return FALSE;

	ret = decode_oid(pdu, buf, buf_len);

	if(ret == FALSE) {
		upsdebugx(2, "[%s] unhandled ASN 0x%x received from %s",
			upsname?upsname:device_name, pdu->variables->type, OID);
	}

	snmp_free_pdu(pdu);

	return ret;
}

bool_t nut_snmp_get_int(const char *OID, long *pval)
{
	char tmp_buf[SU_LARGEBUF];
	struct snmp_pdu *pdu;
	long value;
	char *buf;

	upsdebugx(3, "Entering %s()", __func__);

	pdu = nut_snmp_get(OID);
	if (pdu == NULL)
		return FALSE;

	switch (pdu->variables->type) {
	case ASN_OCTET_STR:
	case ASN_OPAQUE:
		buf = xmalloc(pdu->variables->val_len + 1);
		memcpy(buf, pdu->variables->val.string, pdu->variables->val_len);
		buf[pdu->variables->val_len] = '\0';
		value = strtol(buf, NULL, 0);
		free(buf);
		break;
	case ASN_INTEGER:
	case ASN_COUNTER:
	case ASN_GAUGE:
		value = *pdu->variables->val.integer;
		break;
	case ASN_TIMETICKS:
		/* convert timeticks to seconds */
		value = *pdu->variables->val.integer / 100;
		break;
	case ASN_OBJECT_ID:
		snprint_objid (tmp_buf, sizeof(tmp_buf), pdu->variables->val.objid, pdu->variables->val_len / sizeof(oid));
		upsdebugx(2, "Received an OID value: %s", tmp_buf);
		/* Try to get the value of the pointed OID */
		if (nut_snmp_get_int(tmp_buf, &value) == FALSE) {
			upsdebugx(3, "Failed to retrieve OID value, using fallback");
			/* Otherwise return the last part of the returned OID (ex: 1.2.3 => 3) */
			char *oid_leaf = strrchr(tmp_buf, '.');
			value = strtol(oid_leaf+1, NULL, 0);
			upsdebugx(3, "Fallback value: %ld", value);
		}
		break;
	default:
		upslogx(LOG_ERR, "[%s] unhandled ASN 0x%x received from %s",
			upsname?upsname:device_name, pdu->variables->type, OID);
		return FALSE;
	}

	snmp_free_pdu(pdu);

	if (pval != NULL)
		*pval = value;

	return TRUE;
}

bool_t nut_snmp_set(const char *OID, char type, const char *value)
{
	int status;
	bool_t ret = FALSE;
	struct snmp_pdu *pdu, *response = NULL;
	oid name[MAX_OID_LEN];
	size_t name_len = MAX_OID_LEN;

	upsdebugx(1, "entering %s(%s, %c, %s)", __func__, OID, type, value);

	if (!snmp_parse_oid(OID, name, &name_len)) {
		upslogx(LOG_ERR, "[%s] %s: %s: %s",
			upsname?upsname:device_name, __func__, OID, snmp_api_errstring(snmp_errno));
		return FALSE;
	}

	pdu = snmp_pdu_create(SNMP_MSG_SET);
	if (pdu == NULL)
		fatalx(EXIT_FAILURE, "Not enough memory");

	if (snmp_add_var(pdu, name, name_len, type, value)) {
		upslogx(LOG_ERR, "[%s] %s: %s: %s",
			upsname?upsname:device_name, __func__, OID, snmp_api_errstring(snmp_errno));

		return FALSE;
	}

	status = snmp_synch_response(g_snmp_sess_p, pdu, &response);

	if ((status == STAT_SUCCESS) && (response->errstat == SNMP_ERR_NOERROR))
		ret = TRUE;
	else
		nut_snmp_perror(g_snmp_sess_p, status, response,
			"%s: can't set %s", __func__, OID);

	snmp_free_pdu(response);
	return ret;
}

bool_t nut_snmp_set_str(const char *OID, const char *value)
{
	return nut_snmp_set(OID, 's', value);
}

bool_t nut_snmp_set_int(const char *OID, long value)
{
	char buf[SU_BUFSIZE];

	snprintf(buf, sizeof(buf), "%ld", value);
	return nut_snmp_set(OID, 'i', buf);
}

bool_t nut_snmp_set_time(const char *OID, long value)
{
	char buf[SU_BUFSIZE];

	snprintf(buf, SU_BUFSIZE, "%ld", value * 100);
	return nut_snmp_set(OID, 't', buf);
}

/* log descriptive SNMP error message. */
void nut_snmp_perror(struct snmp_session *sess, int status,
	struct snmp_pdu *response, const char *fmt, ...)
{
	va_list va;
	int cliberr, snmperr;
	char *snmperrstr;
	char buf[SU_LARGEBUF];

	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	if (response == NULL) {
		snmp_error(sess, &cliberr, &snmperr, &snmperrstr);
		upslogx(LOG_ERR, "[%s] %s: %s",
			upsname?upsname:device_name, buf, snmperrstr);
		free(snmperrstr);
	} else if (status == STAT_SUCCESS) {
/* Net-SNMP headers provide and consume errstat with different types:
net-snmp/output_api.h:    const char     *snmp_errstring(int snmp_errorno);
net-snmp/types.h:    long            errstat;
 * Should we match in configure like for "getnameinfo()" arg types?
 * Currently we cast one to another, below (detecting target type could help).
 */
		switch (response->errstat)
		{
		case SNMP_ERR_NOERROR:
			break;
		case SNMP_ERR_NOSUCHNAME:	/* harmless */
			upsdebugx(2, "[%s] %s: %s",
				upsname?upsname:device_name,
				buf,
				(response->errstat > INT_MAX
				    ? "(Net-SNMP errstat value is out of range)"
				    : snmp_errstring((int)response->errstat)
				));
			break;
		default:
			upslogx(LOG_ERR, "[%s] %s: Error in packet: %s",
				upsname?upsname:device_name,
				buf,
				(response->errstat > INT_MAX
				    ? "(Net-SNMP errstat value is out of range)"
				    : snmp_errstring((int)response->errstat)
				));
			break;
		}
	} else if (status == STAT_TIMEOUT) {
		upslogx(LOG_ERR, "[%s] %s: Timeout: no response from %s",
			upsname?upsname:device_name, buf, sess->peername);
	} else {
		snmp_sess_error(sess, &cliberr, &snmperr, &snmperrstr);
		upslogx(LOG_ERR, "[%s] %s: %s",
			upsname?upsname:device_name, buf, snmperrstr);
		free(snmperrstr);
	}
}

/* -----------------------------------------------------------
 * utility functions.
 * ----------------------------------------------------------- */

/* deal with APCC weirdness on Symmetras */
static void disable_transfer_oids(void)
{
	snmp_info_t *su_info_p;

	upslogx(LOG_INFO, "Disabling transfer OIDs");

	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++) {
		if (!strcasecmp(su_info_p->info_type, "input.transfer.low")) {
			su_info_p->flags &= ~SU_FLAG_OK;
			continue;
		}

		if (!strcasecmp(su_info_p->info_type, "input.transfer.high")) {
			su_info_p->flags &= ~SU_FLAG_OK;
			continue;
		}
	}
}

/* Universal function to add or update info element.
 * If value is NULL, use the default one (su_info_p->dfl) if provided */
void su_setinfo(snmp_info_t *su_info_p, const char *value)
{
	info_lkp_t	*info_lkp;
	char info_type[128]; // We tweak incoming "su_info_p->info_type" value in some cases

/* FIXME: Replace hardcoded 128 with a macro above (use {SU_}LARGEBUF?),
 *and same macro or sizeof(info_type) below? */

	upsdebugx(1, "entering %s(%s, %s)", __func__, su_info_p->info_type, (value)?value:"");

/* FIXME: This 20 seems very wrong (should be "128", macro or sizeof? see above) */
	memset(info_type, 0, 20);
	/* pre-fill with the device name for checking */
	snprintf(info_type, 128, "device.%i", current_device_number);

	if ((daisychain_enabled == TRUE) && (devices_count > 1)) {
		/* Only append "device.X" for master and slaves, if not already done! */
		if ((current_device_number > 0) && (strstr(su_info_p->info_type, info_type) == NULL)) {
			/* Special case: we remove "device" from the device collection not to
			 * get "device.X.device.<something>", but "device.X.<something>" */
			if (!strncmp(su_info_p->info_type, "device.", 7)) {
				snprintf(info_type, 128, "device.%i.%s",
					current_device_number, su_info_p->info_type + 7);
			}
			else {
				snprintf(info_type, 128, "device.%i.%s",
					current_device_number, su_info_p->info_type);
			}
		}
		else
			snprintf(info_type, 128, "%s", su_info_p->info_type);
	}
	else
		snprintf(info_type, 128, "%s", su_info_p->info_type);

	upsdebugx(1, "%s: using info_type '%s'", __func__, info_type);

	if (SU_TYPE(su_info_p) == SU_TYPE_CMD)
		return;

	/* ups.status and {ups, Lx, outlet, outlet.group}.alarm have special
	 * handling, not here! */
	if ((strcasecmp(su_info_p->info_type, "ups.status"))
		&& (strcasecmp(strrchr(su_info_p->info_type, '.'), ".alarm")))
	{
		if (value != NULL)
			dstate_setinfo(info_type, "%s", value);
		else if (su_info_p->dfl != NULL)
			dstate_setinfo(info_type, "%s", su_info_p->dfl);
		else {
			upsdebugx(3, "%s: no value nor default provided, aborting...", __func__);
			return;
		}

		dstate_setflags(info_type, su_info_p->info_flags);
		dstate_setaux(info_type, su_info_p->info_len);

		/* Set enumerated values, only if the data has ST_FLAG_RW and there
		 * are lookup values */
/* FIXME: daisychain settings support: check if applicable */
		if ((su_info_p->info_flags & ST_FLAG_RW) && su_info_p->oid2info) {

			upsdebugx(3, "%s: adding enumerated values", __func__);

			/* Loop on all existing values */
			for (info_lkp = su_info_p->oid2info; info_lkp != NULL
				&& info_lkp->info_value != NULL; info_lkp++) {
					dstate_addenum(info_type, "%s", info_lkp->info_value);
			}
		}

		/* Commit the current value, to avoid staleness with huge
		 * data collections on slow devices */
		 dstate_dataok();
	}
}

void su_status_set(snmp_info_t *su_info_p, long value)
{
	const char *info_value = NULL;

	upsdebugx(2, "SNMP UPS driver: entering %s()", __func__);

	if ((info_value = su_find_infoval(su_info_p->oid2info, &value)) != NULL)
	{
		if (info_value[0] != '\0') {
			status_set(info_value);
		}
	}
	/* TODO: else */
}

void su_alarm_set(snmp_info_t *su_info_p, long value)
{
	const char *info_value = NULL;
	const char *info_type = NULL;
	char alarm_info_value[SU_LARGEBUF];
	/* number of the outlet or phase */
	int item_number = -1;

	upsdebugx(2, "SNMP UPS driver: entering %s(%s)", __func__, su_info_p->info_type);

	/* daisychain handling
	 * extract the template part to get the relevant 'info_type' part
	 * ex: device.6.L1.alarm => L1.alarm
	 * ex: device.6.outlet.1.alarm => outlet.1.alarm */
	if (!strncmp(su_info_p->info_type, "device.", 7)) {
		info_type = strchr(su_info_p->info_type + 7, '.') + 1;
	}
	else
		info_type = su_info_p->info_type;

	upsdebugx(2, "%s: using definition %s", __func__, info_type);

	if ((info_value = su_find_infoval(su_info_p->oid2info, &value)) != NULL
		&& info_value[0] != 0)
	{
		/* Special handling for outlet & outlet groups alarms */
		if ((su_info_p->flags & SU_OUTLET)
			|| (su_info_p->flags & SU_OUTLET_GROUP)) {
			/* Extract template number */
			item_number = extract_template_number(su_info_p->flags, info_type);

			upsdebugx(2, "%s: appending %s %i", __func__,
				(su_info_p->flags & SU_OUTLET_GROUP) ? "outlet group" : "outlet", item_number);

			/* Inject in the alarm string */
			snprintf(alarm_info_value, sizeof(alarm_info_value),
				"outlet%s %i %s", (su_info_p->flags & SU_OUTLET_GROUP) ? " group" : "",
				item_number, info_value);
			info_value = &alarm_info_value[0];
		}
		/* Special handling for phase alarms
		 * Note that SU_*PHASE flags are cleared, so match the 'Lx'
		 * start of path */
		if (info_type[0] == 'L') {
			/* Extract phase number */
			item_number = atoi(info_type+1);
			char alarm_info_value_more[SU_LARGEBUF + 32]; /* can sprintf() SU_LARGEBUF plus markup into here */

			upsdebugx(2, "%s: appending phase L%i", __func__, item_number);

			/* Inject in the alarm string */
			snprintf(alarm_info_value_more, sizeof(alarm_info_value_more),
				"phase L%i %s", item_number, info_value);
			info_value = &alarm_info_value_more[0];
		}

		/* Set the alarm value */
		alarm_set(info_value);
	}
	/* TODO: else */
}

/* find info element definition in my info array. */
snmp_info_t *su_find_info(const char *type)
{
	snmp_info_t *su_info_p;

	for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++)
		if (!strcasecmp(su_info_p->info_type, type)) {
			upsdebugx(3, "%s: \"%s\" found", __func__, type);
			return su_info_p;
		}

	upsdebugx(3, "%s: unknown info type (%s)", __func__, type);
	return NULL;
}

/* Counter match the sysOID using {device,ups}.model OID
 * Return TRUE if this OID can be retrieved, FALSE otherwise */
static bool_t match_model_OID()
{
	bool_t retCode = FALSE;
	snmp_info_t *su_info_p, *cur_info_p;
	char testOID_buf[LARGEBUF];

	/* Try to get device.model first */
	su_info_p = su_find_info("device.model");
	/* Otherwise, try to get ups.model */
	if (su_info_p == NULL)
		su_info_p = su_find_info("ups.model");

	if (su_info_p != NULL) {
		/* Daisychain specific: we may have a template (including formatting
		 * string) that needs to be adapted! */
		if (strchr(su_info_p->OID, '%') != NULL)
		{
			upsdebugx(2, "Found template, need to be adapted");
			cur_info_p = (snmp_info_t *)malloc(sizeof(snmp_info_t));
			cur_info_p->info_type = (char *)xmalloc(SU_INFOSIZE);
			cur_info_p->OID = (char *)xmalloc(SU_INFOSIZE);
			snprintf((char*)cur_info_p->info_type, SU_INFOSIZE, "%s", su_info_p->info_type);
			/* Use the daisychain master (0) / 1rst device index */
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
			snprintf((char*)cur_info_p->OID, SU_INFOSIZE, su_info_p->OID, 0);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
		}
		else {
			upsdebugx(2, "Found entry, not a template %s", su_info_p->OID);
			/* Otherwise, just point at what we found */
			cur_info_p = su_info_p;
		}

		upsdebugx(2, "Testing %s using OID %s", cur_info_p->info_type, cur_info_p->OID);
		retCode = nut_snmp_get_str(cur_info_p->OID, testOID_buf, LARGEBUF, NULL);

		/* Free our malloc, if it was dynamic */
		if (strchr(su_info_p->OID, '%') != NULL) {
			if (cur_info_p->info_type != NULL)
				free((char*)cur_info_p->info_type);
			if (cur_info_p->OID != NULL)
				free((char*)cur_info_p->OID);
			if (cur_info_p != NULL)
				free((char*)cur_info_p);
		}
	}

	return retCode;
}

/* Try to find the MIB using sysOID matching.
 * Return a pointer to a mib2nut definition if found, NULL otherwise */
static mib2nut_info_t *match_sysoid()
{
	char sysOID_buf[LARGEBUF];
	oid device_sysOID[MAX_OID_LEN];
	size_t device_sysOID_len = MAX_OID_LEN;
	oid mib2nut_sysOID[MAX_OID_LEN];
	size_t mib2nut_sysOID_len = MAX_OID_LEN;
	int i;

	/* Retrieve sysOID value of this device */
	if (nut_snmp_get_oid(SYSOID_OID, sysOID_buf, sizeof(sysOID_buf)) == TRUE)
	{
		upsdebugx(1, "%s: device sysOID value = %s", __func__, sysOID_buf);

		/* Build OIDs for comparison */
		if (!read_objid(sysOID_buf, device_sysOID, &device_sysOID_len))
		{
			upsdebugx(2, "%s: can't build device_sysOID %s: %s",
				__func__, sysOID_buf, snmp_api_errstring(snmp_errno));

			return NULL;
		}

		/* Now, iterate on mib2nut definitions */
		for (i = 0; mib2nut[i] != NULL; i++)
		{
			upsdebugx(1, "%s: checking MIB %s", __func__, mib2nut[i]->mib_name);

			if (mib2nut[i]->sysOID == NULL)
				continue;

			/* Clear variables */
			memset(mib2nut_sysOID, 0, sizeof(mib2nut_sysOID));
			mib2nut_sysOID_len = MAX_OID_LEN;

			if (!read_objid(mib2nut[i]->sysOID, mib2nut_sysOID, &mib2nut_sysOID_len))
			{
				upsdebugx(2, "%s: can't build OID %s: %s",
					__func__, sysOID_buf, snmp_api_errstring(snmp_errno));

				/* Try to continue anyway! */
				continue;
			}

			/* Now compare these */
			upsdebugx(1, "%s: comparing %s with %s", __func__, sysOID_buf, mib2nut[i]->sysOID);
			if (!netsnmp_oid_equals(device_sysOID, device_sysOID_len, mib2nut_sysOID, mib2nut_sysOID_len))
			{
				upsdebugx(2, "%s: sysOID matches MIB '%s'!", __func__, mib2nut[i]->mib_name);
				/* Counter verify, using {ups,device}.model */
				snmp_info = mib2nut[i]->snmp_info;

				if (match_model_OID() != TRUE)
				{
					upsdebugx(2, "%s: testOID provided and doesn't match MIB '%s'!", __func__, mib2nut[i]->mib_name);
					snmp_info = NULL;
					continue;
				}
				else
					upsdebugx(2, "%s: testOID provided and matches MIB '%s'!", __func__, mib2nut[i]->mib_name);

				return mib2nut[i];
			}
		}

		/* Yell all to call for user report */
		upslogx(LOG_ERR, "No matching MIB found for sysOID '%s'!\n" \
			"Please report it to NUT developers, with an 'upsc' output for your device.\n" \
			"Going back to the classic MIB detection method.",
			sysOID_buf);
	}
	else
		upsdebugx(2, "Can't get sysOID value");

	return NULL;
}

/* Load the right snmp_info_t structure matching mib parameter */
bool_t load_mib2nut(const char *mib)
{
	int	i;
	mib2nut_info_t *m2n = NULL;
	/* Below we have many checks for "auto"; avoid redundant string walks: */
	bool_t mibIsAuto = (0 == strcmp(mib, "auto"));
	bool_t mibSeen = FALSE; /* Did we see the MIB name while walking mib2nut[]? */

	upsdebugx(1, "SNMP UPS driver: entering %s(%s) to detect "
		"proper MIB for device [%s] (host %s)",
		__func__, mib,
		upsname ? upsname : device_name,
		device_path // the "port" from config section is hostname/IP for networked drivers
		);

	/* First, try to match against sysOID, if no MIB was provided.
	 * This should speed up init stage
	 * (Note: sysOID points the device main MIB entry point) */
	if (mibIsAuto)
	{
		upsdebugx(2, "%s: trying the new match_sysoid() method with %s",
			__func__, mib);
		/* Retry at most 3 times, to maximise chances */
		for (i = 0; i < 3 ; i++) {
			upsdebugx(3, "%s: trying the new match_sysoid() method: attempt #%d",
				__func__, (i+1));
			if ((m2n = match_sysoid()) != NULL)
				break;

			if (m2n == NULL)
				upsdebugx(3, "%s: failed with new match_sysoid() method",
					__func__);
			else
				upsdebugx(3, "%s: found something with new match_sysoid() method",
					__func__);
		}
	}

	/* Otherwise, revert to the classic method */
	if (m2n == NULL)
	{
		for (i = 0; mib2nut[i] != NULL; i++) {
			/* Is there already a MIB name provided? */
			if (!mibIsAuto && strcmp(mib, mib2nut[i]->mib_name)) {
				/* "mib" is neither "auto" nor the name in mapping table */
				upsdebugx(2, "%s: skip the \"%s\" entry which "
					" is neither \"auto\" nor a name in the mapping table",
					__func__, mib);
				continue;
			}
			upsdebugx(2, "%s: trying classic sysOID matching method with '%s' mib",
				__func__, mib2nut[i]->mib_name);

			/* Device might not support this MIB, but we want to
			 * track that the name string is valid for diags below
			 */
			if (!mibIsAuto) {
				mibSeen = TRUE;
			}

			/* Classic method: test an OID specific to this MIB */
			snmp_info = mib2nut[i]->snmp_info;

			if (match_model_OID() != TRUE)
			{
				upsdebugx(3, "%s: testOID provided and doesn't match MIB '%s'!",
					__func__, mib2nut[i]->mib_name);
				snmp_info = NULL;
				continue;
			}
			else
				upsdebugx(3, "%s: testOID provided and matches MIB '%s'!",
					__func__, mib2nut[i]->mib_name);

			/* MIB found */
			m2n = mib2nut[i];
			break;
		}
	}

	/* Store the result, if any */
	if (m2n != NULL)
	{
		snmp_info = m2n->snmp_info;
		OID_pwr_status = m2n->oid_pwr_status;
		mibname = m2n->mib_name;
		mibvers = m2n->mib_version;
		alarms_info = m2n->alarms_info;
		upsdebugx(1, "%s: using %s MIB for device [%s] (host %s)",
			__func__, mibname,
			upsname ? upsname : device_name, device_path);
		return TRUE;
	}

	/* Did we find something or is it really an unknown mib */
	if (!mibIsAuto) {
		if (mibSeen) {
			fatalx(EXIT_FAILURE, "Requested 'mibs' value '%s' "
				"did not match this device [%s] (host %s)",
				mib, upsname ? upsname : device_name, device_path);
		} else {
			/* String not seen during mib2nut[] walk -
			 * and if we had no hits, we walked it all
			 */
			fatalx(EXIT_FAILURE, "Unknown 'mibs' value: %s", mib);
		}
	} else {
		fatalx(EXIT_FAILURE, "No supported device detected at [%s] (host %s)",
			upsname ? upsname : device_name, device_path);
	}

	/* Should not get here thanks to fatalx() above, but need to silence a warning */
	return FALSE;
}

/* find the OID value matching that INFO_* value */
long su_find_valinfo(info_lkp_t *oid2info, const char* value)
{
	info_lkp_t *info_lkp;

	for (info_lkp = oid2info; (info_lkp != NULL) &&
		(strcmp(info_lkp->info_value, "NULL")); info_lkp++) {

		if (!(strcmp(info_lkp->info_value, value))) {
			upsdebugx(1, "%s: found %s (value: %s)",
					__func__, info_lkp->info_value, value);

			return info_lkp->oid_value;
		}
	}
	upsdebugx(1, "%s: no matching INFO_* value for this OID value (%s)", __func__, value);
	return -1;
}

/* String reformating function */
const char *su_find_strval(info_lkp_t *oid2info, void *value)
{
	/* First test if we have a generic lookup function */
	if ( (oid2info != NULL) && (oid2info->fun_vp2s != NULL) ) {
		upsdebugx(2, "%s: using generic lookup function (string reformatting)", __func__);
		const char * retvalue = oid2info->fun_vp2s(value);
		upsdebugx(2, "%s: got value '%s'", __func__, retvalue);
		return retvalue;
	}
	upsdebugx(1, "%s: no result value for this OID string value (%s)", __func__, (char*)value);
	return NULL;
}

/* find the INFO_* value matching that OID numeric (long) value */
const char *su_find_infoval(info_lkp_t *oid2info, void *raw_value)
{
	info_lkp_t *info_lkp;
	long value = *((long *)raw_value);

	/* First test if we have a generic lookup function */
	if ( (oid2info != NULL) && (oid2info->fun_vp2s != NULL) ) {
		upsdebugx(2, "%s: using generic lookup function", __func__);
		const char * retvalue = oid2info->fun_vp2s(raw_value);
		upsdebugx(2, "%s: got value '%s'", __func__, retvalue);
		return retvalue;
	}

	/* Otherwise, use the simple values mapping */
	for (info_lkp = oid2info; (info_lkp != NULL) &&
		 (info_lkp->info_value != NULL) && (strcmp(info_lkp->info_value, "NULL")); info_lkp++) {

		if (info_lkp->oid_value == value) {
			upsdebugx(1, "%s: found %s (value: %ld)",
					__func__, info_lkp->info_value, value);

			return info_lkp->info_value;
		}
	}
	upsdebugx(1, "%s: no matching INFO_* value for this OID value (%ld)", __func__, value);
	return NULL;
}

/* FIXME: doesn't work with templates! */
static void disable_competition(snmp_info_t *entry)
{
	snmp_info_t	*p;

	for(p=snmp_info; p->info_type!=NULL; p++) {
		if(p!=entry && !strcmp(p->info_type, entry->info_type)) {
			upsdebugx(2, "%s: disabling %s %s",
					__func__, p->info_type, p->OID);
			p->flags &= ~SU_FLAG_OK;
		}
	}
}

/* set shutdown and/or start delays */
void set_delays(void)
{
	int ondelay, offdelay;
	char su_scratch_buf[255];

	if (getval(SU_VAR_ONDELAY))
		ondelay = atoi(getval(SU_VAR_ONDELAY));
	else
		ondelay = -1;

	if (getval(SU_VAR_OFFDELAY))
		offdelay = atoi(getval(SU_VAR_OFFDELAY));
	else
		offdelay = -1;

	if (ondelay >= 0) {
		sprintf(su_scratch_buf, "%d", ondelay);
		su_setvar("ups.delay.start", su_scratch_buf);
	}

	if (offdelay >= 0) {
		sprintf(su_scratch_buf, "%d", offdelay);
		su_setvar("ups.delay.shutdown", su_scratch_buf);
	}
}

/***********************************************************************
 * Template handling functions
 **********************************************************************/

/* Test if the template is a multiple one, i.e. with a formatting string that
 * contains multiple "%i".
 * Return TRUE if yes (multiple "%i" found), FALSE otherwise */
static bool_t is_multiple_template(const char *OID_template)
{
	bool_t retCode = FALSE;
	char *format_char = NULL;

	if (OID_template) {
		format_char = strchr(OID_template, '%');
		upsdebugx(4, "%s(%s)", __func__, OID_template);
	}
	else
		upsdebugx(4, "%s(NULL)", __func__);

	if (format_char != NULL) {
		if (strchr(format_char + 1, '%') != NULL) {
			retCode = TRUE;
		}
	}

	upsdebugx(4, "%s: has %smultiple template definition",
		__func__, (retCode == FALSE)?"not ":"");

	return retCode;
}

/* Instantiate an snmp_info_t from a template.
 * Useful for outlet and outlet.group templates.
 * Note: remember to adapt info_type, OID and optionaly dfl */
static snmp_info_t *instantiate_info(snmp_info_t *info_template, snmp_info_t *new_instance)
{
	upsdebugx(1, "%s(%s)", __func__, info_template ? info_template->info_type : "n/a");

	/* sanity check */
	if (info_template == NULL)
		return NULL;

	if (new_instance == NULL)
		new_instance = (snmp_info_t *)xmalloc(sizeof(snmp_info_t));

	new_instance->info_type = (char *)xmalloc(SU_INFOSIZE);
	if (new_instance->info_type)
		memset((char *)new_instance->info_type, 0, SU_INFOSIZE);
	if (info_template->OID != NULL) {
		new_instance->OID = (char *)xmalloc(SU_INFOSIZE);
		if (new_instance->OID)
			memset((char *)new_instance->OID, 0, SU_INFOSIZE);
	}
	else
		new_instance->OID = NULL;
	new_instance->info_flags = info_template->info_flags;
	new_instance->info_len = info_template->info_len;
	/* FIXME: check if we need to adapt this one... */
	new_instance->dfl = info_template->dfl;
	new_instance->flags = info_template->flags;
	new_instance->oid2info = info_template->oid2info;

	upsdebugx(2, "instantiate_info: template instantiated");
	return new_instance;
}

/* Free a dynamically allocated snmp_info_t.
 * Useful for outlet and outlet.group templates */
static void free_info(snmp_info_t *su_info_p)
{
	/* sanity check */
	if (su_info_p == NULL)
		return;

	if (su_info_p->info_type != NULL)
		free ((char *)su_info_p->info_type);

	if (su_info_p->OID != NULL)
		free ((char *)su_info_p->OID);

	free (su_info_p);
}

/* return the base SNMP index (0 or 1) to start template iteration on
 * the MIB, based on a test using a template OID */
static int base_snmp_template_index(const snmp_info_t *su_info_p)
{
	if (!su_info_p)
		return -1;

	int base_index = -1;
	char test_OID[SU_INFOSIZE];
	snmp_info_flags_t template_type = get_template_type(su_info_p->info_type);

	if (!su_info_p->OID)
		return base_index;

	upsdebugx(3, "%s: OID template = %s", __func__, su_info_p->OID);

	/* Try to differentiate between template types which may have
	 * different indexes ; and store it to not redo it again */
	switch (template_type) {
		case SU_OUTLET:
			template_index_base = outlet_template_index_base;
			break;
		case SU_OUTLET_GROUP:
			template_index_base = outletgroup_template_index_base;
			break;
		case SU_DAISY:
			template_index_base = device_template_index_base;
			break;
		default:
			/* we should never fall here! */
			upsdebugx(3, "%s: unknown template type '%" PRI_SU_FLAGS "' for %s",
				__func__, template_type, su_info_p->info_type);
	}
	base_index = template_index_base;

	if (template_index_base == -1)
	{
		/* not initialised yet */
		for (base_index = 0 ; base_index < 2 ; base_index++) {
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
			/* Test if this template also includes daisychain, in which case
			 * we just use the current device index */
			if (is_multiple_template(su_info_p->OID) == TRUE) {
				if (su_info_p->flags & SU_TYPE_DAISY_1) {
					snprintf(test_OID, sizeof(test_OID), su_info_p->OID,
						current_device_number + device_template_offset, base_index);
				}
				else {
					snprintf(test_OID, sizeof(test_OID), su_info_p->OID,
						base_index, current_device_number + device_template_offset);
				}
			}
			else {
				snprintf(test_OID, sizeof(test_OID), su_info_p->OID, base_index);
			}
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

			if (nut_snmp_get(test_OID) != NULL) {
				if (su_info_p->flags & SU_FLAG_ZEROINVALID) {
					long value;
					if ((nut_snmp_get_int(test_OID, &value)) && (value!=0)) {
						break;
					}
				}
				else if (su_info_p->flags & SU_FLAG_NAINVALID) {
					char value[SU_BUFSIZE];
					if ((nut_snmp_get_str(test_OID, value, SU_BUFSIZE, NULL))
						&& (strncmp(value, "N/A", 3))) {
						break;
					}
				}
				else {
					break;
				}
			}
		}
		/* Only store if it's a template for outlets or outlets groups,
		 * not for daisychain (which has different index) */
		if (su_info_p->flags & SU_OUTLET)
			outlet_template_index_base = base_index;
		else if (su_info_p->flags & SU_OUTLET_GROUP)
			outletgroup_template_index_base = base_index;
		else
			device_template_index_base = base_index;
	}
	upsdebugx(3, "%s: template_index_base = %i", __func__, base_index);
	return base_index;
}

/* Try to determine the number of items (outlets, outlet groups, ...),
 * using a template definition. Walk through the template until we can't
 * get anymore values. I.e., if we can iterate up to 8 item, return 8 */
static int guestimate_template_count(snmp_info_t *su_info_p)
{
	int base_index = 0;
	char test_OID[SU_INFOSIZE];
	int base_count;
	const char *OID_template = su_info_p->OID;

	upsdebugx(1, "%s(%s)", __func__, OID_template);

	/* Determine if OID index starts from 0 or 1? */
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	snprintf(test_OID, sizeof(test_OID), OID_template, base_index);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	if (nut_snmp_get(test_OID) == NULL) {
		base_index++;
	}
	else {
		if (su_info_p->flags & SU_FLAG_ZEROINVALID) {
			long value;
			if ((nut_snmp_get_int(test_OID, &value)) && (value==0)) {
				base_index++;
			}
		}
	}

	/* Now, actually iterate */
	for (base_count = 0 ;  ; base_count++) {
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
		snprintf(test_OID, sizeof(test_OID), OID_template, base_index + base_count);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
		if (nut_snmp_get(test_OID) == NULL)
			break;
	}

	upsdebugx(3, "%s: %i", __func__, base_count);
	return base_count;
}

/* Process template definition, instantiate and get data or register
 * command
 * type: outlet, outlet.group, device */
static bool_t process_template(int mode, const char* type, snmp_info_t *su_info_p)
{
	/* Default to TRUE, and leave to get_and_process_data() to set
	 * to FALSE when actually getting data from devices, to avoid false
	 * negative with server side data */
	bool_t status = TRUE;
	int cur_template_number = 1;
	int cur_nut_index = 0;
	int template_count = 0;
	int base_snmp_index = 0;
	snmp_info_t cur_info_p;
	char template_count_var[SU_BUFSIZE * 2];
	/* Needed *2 to fit a max size_t in snprintf() below,
	 * even if that should never happen */
	char tmp_buf[SU_INFOSIZE];

	upsdebugx(1, "%s template definition found (%s)...", type, su_info_p->info_type);

	if ((strncmp(type, "device", 6)) && (devices_count > 1) && (current_device_number > 0)) {
		snprintf(template_count_var, sizeof(template_count_var), "device.%i.%s.count", current_device_number, type);
	} else {
		snprintf(template_count_var, sizeof(template_count_var), "%s.count", type);
	}

	if(dstate_getinfo(template_count_var) == NULL) {
		/* FIXME: should we disable it?
		 * su_info_p->flags &= ~SU_FLAG_OK;
		 * or rely on guestimation? */
		template_count = guestimate_template_count(su_info_p);
		/* Publish the count estimation */
		if (template_count > 0) {
			dstate_setinfo(template_count_var, "%i", template_count);
		}
	}
	else {
		template_count = atoi(dstate_getinfo(template_count_var));
	}

	/* Only instantiate templates if needed! */
	if (template_count > 0) {
		/* general init of data using the template */
		instantiate_info(su_info_p, &cur_info_p);

		base_snmp_index = base_snmp_template_index(su_info_p);

		for (cur_template_number = base_snmp_index ;
				cur_template_number < (template_count + base_snmp_index) ;
				cur_template_number++)
		{
			/* Special processing for daisychain:
			 * append 'device.x' to the NUT variable name, except for the
			 * whole daisychain ("device.0") */
			if (!strncmp(type, "device", 6))
			{
				/* Device(s) 1-N (master + slave(s)) need to append 'device.x' */
				if (current_device_number > 0) {
					char *ptr = NULL;
					/* Another special processing for daisychain
					 * device collection needs special appending */
					if (!strncmp(su_info_p->info_type, "device.", 7))
						ptr = (char*)&su_info_p->info_type[7];
					else
						ptr = (char*)su_info_p->info_type;

					snprintf((char*)cur_info_p.info_type, SU_INFOSIZE,
							"device.%i.%s", current_device_number, ptr);
				}
				else
				{
					/* Device 1 ("device.0", whole daisychain) needs no
					 * special processing */
					cur_nut_index = cur_template_number;
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
					snprintf((char*)cur_info_p.info_type, SU_INFOSIZE,
							su_info_p->info_type, cur_nut_index);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
				}
			}
			else /* Outlet and outlet groups templates */
			{
				/* Get the index of the current template instance */
				cur_nut_index = cur_template_number;

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
				/* Special processing for daisychain */
				if (daisychain_enabled == TRUE) {
					/* Device(s) 1-N (master + slave(s)) need to append 'device.x' */
					if ((devices_count > 1) && (current_device_number > 0)) {
						memset(&tmp_buf[0], 0, SU_INFOSIZE);
						strcat(&tmp_buf[0], "device.%i.");
						strcat(&tmp_buf[0], su_info_p->info_type);

						upsdebugx(4, "FORMATTING STRING = %s", &tmp_buf[0]);
						snprintf((char*)cur_info_p.info_type, SU_INFOSIZE,
							&tmp_buf[0], current_device_number, cur_nut_index);
					}
					else {
						// FIXME: daisychain-whole, what to do?
						snprintf((char*)cur_info_p.info_type, SU_INFOSIZE,
							su_info_p->info_type, cur_nut_index);
					}
				}
				else {
					snprintf((char*)cur_info_p.info_type, SU_INFOSIZE,
						su_info_p->info_type, cur_nut_index);
				}
			}

			/* check if default value is also a template */
			if ((cur_info_p.dfl != NULL) &&
				(strstr(su_info_p->dfl, "%i") != NULL)) {
				cur_info_p.dfl = (char *)xmalloc(SU_INFOSIZE);
				snprintf((char *)cur_info_p.dfl, SU_INFOSIZE, su_info_p->dfl, cur_nut_index);
			}

			if (cur_info_p.OID != NULL) {
				/* Special processing for daisychain */
				if (!strncmp(type, "device", 6)) {
					if (current_device_number > 0) {
						snprintf((char *)cur_info_p.OID, SU_INFOSIZE, su_info_p->OID, current_device_number + device_template_offset);
					}
					//else
					// FIXME: daisychain-whole, what to do?
				}
				else {
					/* Special processing for daisychain:
					 * these outlet | outlet groups also include formatting info,
					 * so we have to check if the daisychain is enabled, and if
					 * the formatting info for it are in 1rst or 2nd position */
					if (daisychain_enabled == TRUE) {
						if (su_info_p->flags & SU_TYPE_DAISY_1) {
							snprintf((char *)cur_info_p.OID, SU_INFOSIZE,
								su_info_p->OID, current_device_number + device_template_offset, cur_template_number);
						}
						else {
							snprintf((char *)cur_info_p.OID, SU_INFOSIZE,
								su_info_p->OID, cur_template_number + device_template_offset, current_device_number - device_template_offset);
						}
					}
					else {
						snprintf((char *)cur_info_p.OID, SU_INFOSIZE, su_info_p->OID, cur_template_number);
					}
				}
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

				/* add instant commands to the info database. */
				if (SU_TYPE(su_info_p) == SU_TYPE_CMD) {
					upsdebugx(1, "Adding template command %s", cur_info_p.info_type);
					/* FIXME: only add if "su_ups_get(cur_info_p) == TRUE" */
					if (mode == SU_WALKMODE_INIT)
						dstate_addcmd(cur_info_p.info_type);
				}
				else /* get and process this data */
					status = get_and_process_data(mode, &cur_info_p);
			} else {
				/* server side (ABSENT) data */
				su_setinfo(&cur_info_p, NULL);
			}
			/* set back the flag */
			su_info_p->flags = cur_info_p.flags;
		}
		free((char*)cur_info_p.info_type);
		if (cur_info_p.OID != NULL)
			free((char*)cur_info_p.OID);
		if ((cur_info_p.dfl != NULL) &&
			(strstr(su_info_p->dfl, "%i") != NULL))
			free((char*)cur_info_p.dfl);
	}
	else {
		upsdebugx(1, "No %s present, discarding template definition...", type);
	}
	return status;
}

/* Return the type of template, according to a variable name.
 * Return: SU_OUTLET_GROUP, SU_OUTLET or 0 if not a template */
snmp_info_flags_t get_template_type(const char* varname)
{
	if (!strncmp(varname, "outlet.group", 12)) {
		upsdebugx(4, "outlet.group template");
		return SU_OUTLET_GROUP;
	}
	else if (!strncmp(varname, "outlet", 6)) {
		upsdebugx(4, "outlet template");
		return SU_OUTLET;
	}
	else if (!strncmp(varname, "device", 6)) {
		upsdebugx(4, "device template");
		return SU_DAISY;
	}
	else {
		upsdebugx(2, "Unknown template type: %s", varname);
		return 0;
	}
}

/* Extract the id number of an instantiated template.
 * Example: return '1' for type = 'outlet.1.desc', -1 if unknown */
int extract_template_number(snmp_info_flags_t template_type, const char* varname)
{
	const char* item_number_ptr = NULL;
	int item_number = -1;

	if (template_type & SU_OUTLET_GROUP)
		item_number_ptr = &varname[12];
	else if (template_type & SU_OUTLET)
		item_number_ptr = &varname[6];
	else if (template_type & SU_DAISY)
		item_number_ptr = &varname[6];
	else
		return -1;

	item_number = atoi(++item_number_ptr);
	upsdebugx(3, "%s: item %i", __func__, item_number);
	return item_number;
}

/* Extract the id number of a template from a variable name.
 * Example: return '1' for type = 'outlet.1.desc' */
static int extract_template_number_from_snmp_info_t(const char* varname)
{
	return extract_template_number(get_template_type(varname), varname);
}

/* end of template functions */


/* process a single data from a walk */
bool_t get_and_process_data(int mode, snmp_info_t *su_info_p)
{
	bool_t status = FALSE;

	upsdebugx(1, "%s: %s (%s)", __func__, su_info_p->info_type, su_info_p->OID);

	/* ok, update this element. */
	status = su_ups_get(su_info_p);

	/* set stale flag if data is stale, clear if not. */
	if (status == TRUE) {
		if (su_info_p->flags & SU_FLAG_STALE) {
			upslogx(LOG_INFO, "[%s] %s: data resumed for %s",
				upsname?upsname:device_name, __func__, su_info_p->info_type);
			su_info_p->flags &= ~SU_FLAG_STALE;
		}
		if(su_info_p->flags & SU_FLAG_UNIQUE) {
			/* We should be the only provider of this */
			disable_competition(su_info_p);
			su_info_p->flags &= ~SU_FLAG_UNIQUE;
		}
		dstate_dataok();
	} else {
		if (mode == SU_WALKMODE_INIT) {
			/* handle unsupported vars */
			su_info_p->flags &= ~SU_FLAG_OK;
		} else	{
			if (!(su_info_p->flags & SU_FLAG_STALE)) {
				upslogx(LOG_INFO, "[%s] snmp_ups_walk: data stale for %s",
					upsname?upsname:device_name, su_info_p->info_type);
				su_info_p->flags |= SU_FLAG_STALE;
			}
			dstate_datastale();
		}
	}
	return status;
}

/***********************************************************************
 * Daisychain handling functions
 **********************************************************************/

/*!
 * Daisychained devices support init:
 * Determine the number of device(s) and if daisychain support has to be enabled
 * Set the values of devices_count (internal) and "device.count" (public)
 * Return TRUE if daisychain support is enabled, FALSE otherwise */
bool_t daisychain_init()
{
	snmp_info_t *su_info_p = NULL;

	upsdebugx(1, "Checking if daisychain support has to be enabled");

	su_info_p = su_find_info("device.count");

	if (su_info_p != NULL)
	{
		upsdebugx(1, "Found device.count entry...");

		/* Enable daisychain if there is a device.count entry.
		 * This means that will have templates for entries */
		daisychain_enabled = TRUE;

		/* Try to get the OID value, if it's not a template */
		if ((su_info_p->OID != NULL) &&
			(strstr(su_info_p->OID, "%i") == NULL))
		{
			if (nut_snmp_get_int(su_info_p->OID, &devices_count) == TRUE)
				upsdebugx(1, "There are %ld device(s) present", devices_count);
			else
			{
				upsdebugx(1, "Error: can't get the number of device(s) present!");
				upsdebugx(1, "Falling back to 1 device!");
				devices_count = 1;
			}
		}
		/* Otherwise (template), use the guesstimation function to get
		 * the number of devices present */
		else
		{
			devices_count = guestimate_template_count(su_info_p);
			upsdebugx(1, "Guesstimation: there are %ld device(s) present", devices_count);
		}

		/* Sanity check before data publication */
		if (devices_count < 1) {
			devices_count = 1;
			daisychain_enabled = FALSE;
			upsdebugx(1, "Devices count is less than 1!");
			upsdebugx(1, "Falling back to 1 device and disabling daisychain support!");
		}

		/* Publish the device(s) count */
		if (devices_count > 1) {
			dstate_setinfo("device.count", "%ld", devices_count);

			/* Also publish the default value for mfr and a forged model
			 * for device.0 (whole daisychain) */
			su_info_p = su_find_info("device.mfr");
			if (su_info_p != NULL) {
				su_info_p = su_find_info("ups.mfr");
				if (su_info_p != NULL) {
					su_setinfo(su_info_p, NULL);
				}
			}
			/* Forge model using device.type and number */
			su_info_p = su_find_info("device.type");
			if ((su_info_p != NULL) && (su_info_p->dfl != NULL)) {
				dstate_setinfo("device.model", "daisychain %s (1+%ld)",
					su_info_p->dfl, devices_count - 1);
				dstate_setinfo("device.type", "%s", su_info_p->dfl);
			}
			else {
				dstate_setinfo("device.model", "daisychain (1+%ld)", devices_count - 1);
			}
		}
	}
	else {
		daisychain_enabled = FALSE;
		upsdebugx(1, "No device.count entry found, daisychain support not needed");
	}

	/* Finally, compute and store the base OID index and NUT offset */
	su_info_p = su_find_info("device.model");
	if (su_info_p != NULL) {
		device_template_index_base = base_snmp_template_index(su_info_p);
		upsdebugx(1, "%s: device_template_index_base = %i", __func__, device_template_index_base);
		device_template_offset = device_template_index_base - 1;
		upsdebugx(1, "%s: device_template_offset = %i", __func__, device_template_offset);
	}
	else {
		upsdebugx(1, "%s: No device.model entry found.", __func__);
	}

	upsdebugx(1, "%s: daisychain support is %s", __func__,
		(daisychain_enabled==TRUE)?"enabled":"disabled");

	return daisychain_enabled;
}

/***********************************************************************
 * SNMP handling functions
 **********************************************************************/

/* Process a data with regard to SU_OUTPHASES, SU_INPHASES and SU_BYPPHASES.
 * 3phases related data are disabled if the unit is 1ph, and conversely.
 * If the related phases data (input, output, bypass) is not yet valued,
 * retrieve it first.
 *
 * type: input, output, bypass
 * su_info_p: variable to process flags on
 * Return 0 if OK, 1 if the caller needs to "continue" the walk loop (i.e.
 * skip the present data)
 */
static int process_phase_data(const char* type, long *nb_phases, snmp_info_t *su_info_p)
{
	snmp_info_t *tmp_info_p;
	char tmpOID[SU_INFOSIZE];
	char tmpInfo[SU_INFOSIZE];
	long tmpValue;
	snmp_info_flags_t phases_flag = 0, single_phase_flag = 0, three_phase_flag = 0;

	/* Phase specific data */
	if (!strncmp(type, "input", 5)) {
		phases_flag = SU_INPHASES;
		single_phase_flag = SU_INPUT_1;
		three_phase_flag = SU_INPUT_3;
	}
	else if (!strncmp(type, "output", 6)) {
		phases_flag = SU_OUTPHASES;
		single_phase_flag = SU_OUTPUT_1;
		three_phase_flag = SU_OUTPUT_3;
	}
	else if (!strncmp(type, "input.bypass", 12)) {
		phases_flag = SU_BYPPHASES;
		single_phase_flag = SU_BYPASS_1;
		three_phase_flag = SU_BYPASS_3;
	}
	else {
		upsdebugx(2, "%s: unknown type '%s'", __func__, type);
		return 1;
	}

	/* Init the phase(s) info for this device, if not already done */
	if (*nb_phases == -1) {
		upsdebugx(2, "%s phases information not initialized for device %i",
			type, current_device_number);

		memset(tmpInfo, 0, SU_INFOSIZE);

		/* daisychain specifics... */
		if ( (daisychain_enabled == TRUE) && (current_device_number > 0) ) {
			/* Device(s) 2-N (slave(s)) need to append 'device.x' */
			snprintf(tmpInfo, SU_INFOSIZE,
					"device.%i.%s.phases", current_device_number, type);
		}
		else {
			snprintf(tmpInfo, SU_INFOSIZE, "%s.phases", type);
		}

		if (dstate_getinfo(tmpInfo) == NULL) {
			/* {input,output,bypass}.phases is not yet published,
			 * try to get the template for it */
			snprintf(tmpInfo, SU_INFOSIZE, "%s.phases", type);
			tmp_info_p = su_find_info(tmpInfo);
			if (tmp_info_p != NULL) {
				memset(tmpOID, 0, SU_INFOSIZE);

				/* Daisychain specific: we may have a template (including
				 * formatting string) that needs to be adapted! */
				if (strchr(tmp_info_p->OID, '%') != NULL) {
					upsdebugx(2, "Found template, need to be adapted");
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
					snprintf((char*)tmpOID, SU_INFOSIZE, tmp_info_p->OID, current_device_number + device_template_offset);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
				}
				else {
					/* Otherwise, just point at what we found */
					upsdebugx(2, "Found entry, not a template %s", tmp_info_p->OID);
					snprintf((char*)tmpOID, SU_INFOSIZE, "%s", tmp_info_p->OID);
				}
				/* Actually get the data */
				if (nut_snmp_get_int(tmpOID, &tmpValue) == TRUE) {
					*nb_phases = tmpValue;
				}
				else {
					upsdebugx(2, "Can't get %s value. Defaulting to 1 %s.phase", tmpInfo, type);
					*nb_phases = 1;
					/* FIXME: return something or process using default?! */
				}
			}
			else {
				upsdebugx(2, "No %s entry. Defaulting to 1 %s.phase", tmpInfo, type);
				*nb_phases = 1;
				/* FIXME: return something or process using default?! */
			}
		}
		else {
			*nb_phases = atoi(dstate_getinfo(tmpInfo));
		}
		/* Publish the number of phase(s) */
		dstate_setinfo(tmpInfo, "%ld", *nb_phases);
		upsdebugx(2, "device %i has %ld %s.phases", current_device_number, *nb_phases, type);
	}
	/* FIXME: what to do here?
	else if (*nb_phases == 0) {
		return 1;
	} */


	/* Actual processing of phases related data */
/* FIXME: don't clear SU_INPHASES in daisychain mode!!! ??? */
	if (su_info_p->flags & single_phase_flag) {
		if (*nb_phases == 1) {
			upsdebugx(1, "%s_phases is 1", type);
			su_info_p->flags &= ~phases_flag;
		} else {
			upsdebugx(1, "%s_phases is not 1", type);
			su_info_p->flags &= ~SU_FLAG_OK;
			return 1;
		}
	} else if (su_info_p->flags & three_phase_flag) {
		if (*nb_phases == 3) {
			upsdebugx(1, "%s_phases is 3", type);
			su_info_p->flags &= ~phases_flag;
		} else {
			upsdebugx(1, "%s_phases is not 3", type);
			su_info_p->flags &= ~SU_FLAG_OK;
			return 1;
		}
	} else {
		upsdebugx(1, "%s_phases is %ld", type, *nb_phases);
	}
	return 0; /* FIXME: remap EXIT_SUCCESS to RETURN_SUCCESS */
}


/* walk ups variables and set elements of the info array. */
bool_t snmp_ups_walk(int mode)
{
	long *walked_input_phases, *walked_output_phases, *walked_bypass_phases;
	static unsigned long iterations = 0;
	snmp_info_t *su_info_p;
	bool_t status = FALSE;

	/* Loop through all device(s) */
	/* Note: considering "unitary" and "daisy-chained" devices, we have
	 * several variables (and their values) that can come into play:
	 *  devices_count == 1 (default) AND daisychain_enabled == FALSE => unitary
	 *  devices_count > 1 (AND/OR?) daisychain_enabled == TRUE => a daisy-chain
	 * The current_device_number == 0 in context of daisy-chain means the
	 * "whole device" with composite or summary values that refer to the
	 * chain as a virtual power device (e.g. might be a sum of outlet counts).
	 * If daisychain_enabled == TRUE and current_device_number == 1 then we
	 * are looking at the "master" device (one that we have direct/networked
	 * connectivity to; the current_device_number > 1 is a slave (chained by
	 * some proprietary link not visible from the outside) and represented
	 * through the master - the slaves are not addressable directly. If the
	 * master dies/reboots, connection to the whole chain is interrupted.
	 * The dstate string names for daisychained sub-devices have the prefix
	 * "device." and number embedded (e.g. "device.3.input.phases") except
	 * for the whole (#0) virtual device, so it *seems* similar to unitary.
	 */

	for (current_device_number = 0 ; current_device_number <= devices_count ; current_device_number++)
	{
		/* reinit the alarm buffer, before */
		if (devices_count > 1)
			device_alarm_init();

		/* Loop through all mapping entries for the current_device_number */
		for (su_info_p = &snmp_info[0]; su_info_p->info_type != NULL ; su_info_p++) {

			/* FIXME:
			 * switch(current_device_number) {
			 * case 0: devtype = "daisychain whole"
			 * case 1: devtype = "daisychain master"
			 * default: devtype = "daisychain slave"
			 */
			if (daisychain_enabled == TRUE) {
				upsdebugx(1, "%s: processing device %i (%s)", __func__,
					current_device_number,
					(current_device_number == 1)?"master":"slave"); // FIXME: daisychain
			}

			/* Check if we are asked to stop (reactivity++) */
			if (exit_flag != 0) {
				upsdebugx(1, "%s: aborting because exit_flag was set", __func__);
				return TRUE;
			}

			/* Skip daisychain data count */
			if (mode == SU_WALKMODE_INIT &&
				(!strncmp(su_info_p->info_type, "device.count", 12)))
			{
				su_info_p->flags &= ~SU_FLAG_OK;
				continue;
			}

/* FIXME: daisychain-whole, what to do? */
/* Note that when addressing the FIXME above,
 *   if (current_device_number == 0 && daisychain_enabled == FALSE)
 * then we'd skip it still (unitary device is at current_device_number == 1)...
 */
			/* skip the whole-daisychain for now */
			if (current_device_number == 0) {
				upsdebugx(1, "Skipping daisychain device.0 for now...");
				continue;
			}

			/* skip instcmd, not linked to outlets */
			if ((SU_TYPE(su_info_p) == SU_TYPE_CMD)
				&& !(su_info_p->flags & SU_OUTLET)
				&& !(su_info_p->flags & SU_OUTLET_GROUP)) {
				upsdebugx(1, "SU_CMD_MASK => %s", su_info_p->OID);
				continue;
			}
			/* skip elements we shouldn't show in update mode */
			if ((mode == SU_WALKMODE_UPDATE) && !(su_info_p->flags & SU_FLAG_OK))
				continue;

			/* skip static elements in update mode */
			if ((mode == SU_WALKMODE_UPDATE) && (su_info_p->flags & SU_FLAG_STATIC))
				continue;

			/* Set default value if we cannot fetch it */
			/* and set static flag on this element.
			 * Not applicable to outlets (need SU_FLAG_STATIC tagging) */
			if ((su_info_p->flags & SU_FLAG_ABSENT)
				&& !(su_info_p->flags & SU_OUTLET)
				&& !(su_info_p->flags & SU_OUTLET_GROUP))
			{
				if (mode == SU_WALKMODE_INIT)
				{
					if (su_info_p->dfl)
					{
						if ((daisychain_enabled == TRUE) && (devices_count > 1))
						{
							if (current_device_number == 0)
							{
								su_setinfo(su_info_p, NULL); // FIXME: daisychain-whole, what to do?
							} else {
								status = process_template(mode, "device", su_info_p);
							}
						}
						else {
							/* Set default value if we cannot fetch it from ups. */
							su_setinfo(su_info_p, NULL);
						}
					}
					su_info_p->flags |= SU_FLAG_STATIC;
				}
				continue;
			}

			/* check stale elements only on each PN_STALE_RETRY iteration. */
	/*		if ((su_info_p->flags & SU_FLAG_STALE) &&
					(iterations % SU_STALE_RETRY) != 0)
				continue;
	*/
			/* Filter 1-phase Vs 3-phase according to {input,output,bypass}.phase.
			 * Non matching items are disabled, and flags are cleared at init
			 * time */
			/* Process input phases information */
			walked_input_phases = &daisychain_info[current_device_number]->input_phases;
			if (su_info_p->flags & SU_INPHASES) {
				upsdebugx(1, "Check input_phases (%ld)", *walked_input_phases);
				if (process_phase_data("input", walked_input_phases, su_info_p) == 1)
					continue;
			}

			/* Process output phases information */
			walked_output_phases = &daisychain_info[current_device_number]->output_phases;
			if (su_info_p->flags & SU_OUTPHASES) {
				upsdebugx(1, "Check output_phases (%ld)", *walked_output_phases);
				if (process_phase_data("output", walked_output_phases, su_info_p) == 1)
					continue;
			}

			/* Process bypass phases information */
			walked_bypass_phases = &daisychain_info[current_device_number]->bypass_phases;
			if (su_info_p->flags & SU_BYPPHASES) {
				upsdebugx(1, "Check bypass_phases (%ld)", *walked_bypass_phases);
				if (process_phase_data("input.bypass", walked_bypass_phases, su_info_p) == 1)
					continue;
			}

			/* process template (outlet, outlet group, inc. daisychain) definition */
			if (su_info_p->flags & SU_OUTLET) {
				/* Skip commands after init */
				if ((SU_TYPE(su_info_p) == SU_TYPE_CMD) && (mode == SU_WALKMODE_UPDATE))
					continue;
				else
					status = process_template(mode, "outlet", su_info_p);
			}
			else if (su_info_p->flags & SU_OUTLET_GROUP) {
				/* Skip commands after init */
				if ((SU_TYPE(su_info_p) == SU_TYPE_CMD) && (mode == SU_WALKMODE_UPDATE))
					continue;
				else
					status = process_template(mode, "outlet.group", su_info_p);
			}
			else {
/*				if (daisychain_enabled == TRUE) {
					status = process_template(mode, "device", su_info_p);
				}
				else {
*/					/* get and process this data, including daisychain adaptation */
					status = get_and_process_data(mode, su_info_p);
//				}
			}
		}	/* for (su_info_p... */

		if (devices_count > 1) {
			/* commit the device alarm buffer */
			device_alarm_commit(current_device_number);

			/* reinit the alarm buffer, after, not to pollute "device.0" */
			device_alarm_init();
		}
	}
	iterations++;
	return status;
}

bool_t su_ups_get(snmp_info_t *su_info_p)
{
	static char buf[SU_INFOSIZE];
	bool_t status;
	long value;
	double dvalue;
	const char *strValue = NULL;
	struct snmp_pdu ** pdu_array;
	struct snmp_pdu * current_pdu;
	alarms_info_t * alarms;
	int index = 0;
	char *format_char = NULL;
	snmp_info_t *tmp_info_p = NULL;

	upsdebugx(2, "%s: %s %s", __func__, su_info_p->info_type, su_info_p->OID);

	/* Check if this is a daisychain template */
	if ((format_char = strchr(su_info_p->OID, '%')) != NULL) {
		tmp_info_p = instantiate_info(su_info_p, tmp_info_p);
		if (tmp_info_p != NULL) {
			/* adapt the OID */
			if (su_info_p->OID != NULL) {
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
				snprintf((char *)tmp_info_p->OID, SU_INFOSIZE, su_info_p->OID,
					current_device_number + device_template_offset);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
			}
			else {
				free_info(tmp_info_p);
				return FALSE;
			}

			/* adapt info_type */
			if (su_info_p->info_type != NULL) {
				snprintf((char *)tmp_info_p->info_type, SU_INFOSIZE, "%s", su_info_p->info_type);
			}
			else {
				free_info(tmp_info_p);
				return FALSE;
			}

			su_info_p = tmp_info_p;
		}
		else {
			upsdebugx(2, "%s: can't instantiate template", __func__);
			return FALSE;
		}
	}

	if (!strcasecmp(su_info_p->info_type, "ups.status")) {
/* FIXME: daisychain status support! */
		status = nut_snmp_get_int(su_info_p->OID, &value);
		if (status == TRUE)
		{
			su_status_set(su_info_p, value);
			upsdebugx(2, "=> value: %ld", value);
		}
		else
			upsdebugx(2, "=> Failed");

		free_info(tmp_info_p);
		return status;
	}

	/* Handle 'ups.alarm', 'outlet.n.alarm' and 3phase 'Lx.alarm',
	 * nothing else! */
	if (!strcmp(strrchr(su_info_p->info_type, '.'), ".alarm")) {

		upsdebugx(2, "Processing alarm: %s", su_info_p->info_type);

/* FIXME: daisychain alarms support! */
		status = nut_snmp_get_int(su_info_p->OID, &value);
		if (status == TRUE)
		{
			su_alarm_set(su_info_p, value);
			upsdebugx(2, "=> value: %ld", value);
		}
		else upsdebugx(2, "=> Failed");

		free_info(tmp_info_p);
		return status;
	}

	/* Walk a subtree (array) of alarms, composed of OID references.
	 * The object referenced should not be accessible, but rather when
	 * present, this means that the alarm condition is TRUE.
	 * Only present in powerware-mib.c for now */
	if (!strcasecmp(su_info_p->info_type, "ups.alarms")) {
		status = nut_snmp_get_int(su_info_p->OID, &value);
		if (status == TRUE) {
			upsdebugx(2, "=> %ld alarms present", value);
			if( value > 0 ) {
				pdu_array = nut_snmp_walk(su_info_p->OID, INT_MAX);
				if(pdu_array == NULL) {
					upsdebugx(2, "=> Walk failed");
					return FALSE;
				}

				current_pdu = pdu_array[index];
				while(current_pdu) {
					/* Retrieve the OID name, for comparison */
					if (decode_oid(current_pdu, buf, sizeof(buf)) == TRUE) {
						alarms = alarms_info;
						while( alarms->OID ) {
							if(!strcmp(buf, alarms->OID)) {
								upsdebugx(3, "Alarm OID found => %s", alarms->OID);
								/* Check for ups.status value */
								if (alarms->status_value) {
									upsdebugx(3, "Alarm value (status) found => %s", alarms->status_value);
									status_set(alarms->status_value);
								}
								/* Check for ups.alarm value */
								if (alarms->alarm_value) {
									upsdebugx(3, "Alarm value (alarm) found => %s", alarms->alarm_value);
									alarm_set(alarms->alarm_value);
								}
								break;
							}
							alarms++;
						}
					}
					index++;
					current_pdu = pdu_array[index];
				}
				nut_snmp_free(pdu_array);
			}
		}
		else {
			upsdebugx(2, "=> Failed");
		}

		free_info(tmp_info_p);
		return status;
	}

	/* another special case */
	if (!strcasecmp(su_info_p->info_type, "ambient.temperature")) {
		float temp=0;

		status = nut_snmp_get_int(su_info_p->OID, &value);

		if(status != TRUE) {
			free_info(tmp_info_p);
			return status;
		}

		/* only do this if using the IEM sensor */
		if (!strcmp(su_info_p->OID, APCC_OID_IEM_TEMP)) {
			int	su;
			long	units;

			su = nut_snmp_get_int(APCC_OID_IEM_TEMP_UNIT, &units);

			/* no response, or units == F */
			if ((su == FALSE) || (units == APCC_IEM_FAHRENHEIT))
				temp = (value - 32) / 1.8;
			else
				temp = value;
		}
		else {
			temp = value * su_info_p->info_len;
		}

		snprintf(buf, sizeof(buf), "%.1f", temp);
		su_setinfo(su_info_p, buf);

		free_info(tmp_info_p);
		return TRUE;
	}

	if (su_info_p->info_flags & ST_FLAG_STRING) {
		status = nut_snmp_get_str(su_info_p->OID, buf, sizeof(buf), su_info_p->oid2info);
		if (status == TRUE) {
			if (quirk_symmetra_threephase) {
				if (!strcasecmp(su_info_p->info_type, "input.transfer.low")
				 || !strcasecmp(su_info_p->info_type, "input.transfer.high")) {
					/* Convert from three phase line-to-line voltage to line-to-neutral voltage */
					double tmp_dvalue = atof(buf);
					tmp_dvalue = tmp_dvalue * 0.707;
					snprintf(buf, sizeof(buf), "%.2f", tmp_dvalue);
				}
			}
			/* Check if there is a string reformating function */
			const char *fmt_buf = NULL;
			if ((fmt_buf = su_find_strval(su_info_p->oid2info, buf)) != NULL) {
				snprintf(buf, sizeof(buf), "%s", fmt_buf);
			}
		}
	} else {
		status = nut_snmp_get_int(su_info_p->OID, &value);
		if (status == TRUE) {
			if ((su_info_p->flags&SU_FLAG_NEGINVALID && value<0)
				|| (su_info_p->flags&SU_FLAG_ZEROINVALID && value==0)) {
				su_info_p->flags &= ~SU_FLAG_OK;
				if(su_info_p->flags&SU_FLAG_UNIQUE) {
					disable_competition(su_info_p);
					su_info_p->flags &= ~SU_FLAG_UNIQUE;
				}
				free_info(tmp_info_p);
				return FALSE;
			}
			/* Check if there is a value to be looked up */
			if ((strValue = su_find_infoval(su_info_p->oid2info, &value)) != NULL)
				snprintf(buf, sizeof(buf), "%s", strValue);
			else {
				/* Check if there is a need to publish decimal too,
				 * i.e. if switching to integer does not cause a
				 * loss of precision.
				 * FIXME: Use remainder? is (dvalue%1.0)>0 cleaner?
				 */
				dvalue = value * su_info_p->info_len;
				if (f_equal((int)dvalue, dvalue))
					snprintf(buf, sizeof(buf), "%i", (int)dvalue);
				else
					snprintf(buf, sizeof(buf), "%.2f", (float)dvalue);
			}
		}
	}

	if (status == TRUE) {
		su_setinfo(su_info_p, buf);
		upsdebugx(2, "=> value: %s", buf);
	}
	else
		upsdebugx(2, "=> Failed");

	free_info(tmp_info_p);
	return status;
}

/* Common function for setting OIDs, from a NUT variable name,
 * used by su_setvar() and su_instcmd()
 * Params:
 * @mode: SU_MODE_INSTCMD for instant commands, SU_MODE_SETVAR for settings
 * @varname: name of variable or command to set the OID from
 * @val: value for settings, NULL for commands

 * Returns
 *   STAT_SET_HANDLED if OK,
 *   STAT_SET_INVALID or STAT_SET_UNKNOWN if the command / setting is not supported
 *   STAT_SET_FAILED otherwise
 */
static int su_setOID(int mode, const char *varname, const char *val)
{
	snmp_info_t *su_info_p = NULL;
	bool_t status;
	int retval = STAT_SET_FAILED;
	int cmd_offset = 0;
	long value = -1;
	/* normal (default), outlet, or outlet group variable */
	snmp_info_flags_t vartype = 0;
	int daisychain_device_number = -1;
	/* variable without the potential "device.X" prefix, to find the template */
	char *tmp_varname = NULL;
	char setOID[SU_INFOSIZE];
	/* Used for potentially appending "device.X." to {outlet,outlet.group}.count */
	char template_count_var[SU_BUFSIZE];

	upsdebugx(2, "entering %s(%s, %s, %s)", __func__,
		(mode==SU_MODE_INSTCMD)?"instcmd":"setvar", varname, val);

	memset(setOID, 0, SU_INFOSIZE);
	memset(template_count_var, 0, SU_BUFSIZE);

	/* Check if it's a daisychain setting */
	if (!strncmp(varname, "device", 6)) {
		/* Extract the device number */
		daisychain_device_number = atoi(&varname[7]);
		/* Point at the command, without the "device.x" prefix */
		tmp_varname = strdup(&varname[9]);
		snprintf(template_count_var, 10, "%s", varname);

		upsdebugx(2, "%s: got a daisychain %s (%s) for device %i",
			__func__, (mode==SU_MODE_INSTCMD)?"command":"setting",
			tmp_varname, daisychain_device_number);

		if (daisychain_device_number > devices_count)
			upsdebugx(2, "%s: item is out of bound (%i / %ld)",
				__func__, daisychain_device_number, devices_count);
	}
	else {
		daisychain_device_number = 0;
		tmp_varname = strdup(varname);
	}

	/* skip the whole-daisychain for now:
	 * will send the settings to all devices in the daisychain */
	if ((daisychain_enabled == TRUE) && (devices_count > 1) && (daisychain_device_number == 0)) {
		upsdebugx(2, "daisychain %s for device.0 are not yet supported!",
			(mode==SU_MODE_INSTCMD)?"command":"setting");
		free(tmp_varname);
		return STAT_SET_INVALID;
	}

	/* Check if it is outlet / outlet.group, or standard variable */
	if (strncmp(tmp_varname, "outlet", 6))
		su_info_p = su_find_info(tmp_varname);
	else {
		snmp_info_t *tmp_info_p;
		/* Point the outlet or outlet group number in the string */
		const char *item_number_ptr = NULL;
		/* Store the target outlet or group number */
		int item_number = extract_template_number_from_snmp_info_t(tmp_varname);
		/* Store the total number of outlets or outlet groups */
		int total_items = -1;

		/* Check if it is outlet / outlet.group */
		vartype = get_template_type(tmp_varname);
		if (vartype == SU_OUTLET_GROUP) {
			snprintfcat(template_count_var, SU_BUFSIZE, "outlet.group.count");
			total_items = atoi(dstate_getinfo(template_count_var));
			item_number_ptr = &tmp_varname[12];
		}
		else {
			snprintfcat(template_count_var, SU_BUFSIZE, "outlet.count");
			total_items = atoi(dstate_getinfo(template_count_var));
			item_number_ptr = &tmp_varname[6];
		}
		upsdebugx(3, "Using count variable '%s'", template_count_var);
		item_number = atoi(++item_number_ptr);
		upsdebugx(3, "%s: item %i / %i", __func__, item_number, total_items);

		/* ensure the item number is supported (filtered upstream though)! */
		if (item_number > total_items) {
			/* out of bound item number */
			upsdebugx(2, "%s: item is out of bound (%i / %i)",
				__func__, item_number, total_items);
			return STAT_SET_INVALID;
		}
		/* find back the item template */
		char *item_varname = (char *)xmalloc(SU_INFOSIZE);
		snprintf(item_varname, SU_INFOSIZE, "%s.%s%s",
				(vartype == SU_OUTLET)?"outlet":"outlet.group",
				"%i", strchr(item_number_ptr++, '.'));

		upsdebugx(3, "%s: searching for template\"%s\"", __func__, item_varname);
		tmp_info_p = su_find_info(item_varname);
		free(item_varname);

		/* for an snmp_info_t instance */
		su_info_p = instantiate_info(tmp_info_p, su_info_p);

		/* check if default value is also a template */
		if ((su_info_p->dfl != NULL) &&
			(strstr(tmp_info_p->dfl, "%i") != NULL))
		{
			su_info_p->dfl = (char *)xmalloc(SU_INFOSIZE);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
			snprintf((char *)su_info_p->dfl, SU_INFOSIZE, tmp_info_p->dfl,
				item_number);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
		}
		/* adapt the OID */
		if (su_info_p->OID != NULL) {
			if (mode==SU_MODE_INSTCMD) {
				/* Workaround buggy Eaton Pulizzi implementation
				 * which have different offsets index for data & commands! */
				if (su_info_p->flags & SU_CMD_OFFSET) {
					upsdebugx(3, "Adding command offset");
					cmd_offset++;
				}
			}

			/* Special processing for daisychain:
			 * these outlet | outlet groups also include formatting info,
			 * so we have to check if the daisychain is enabled, and if
			 * the formatting info for it are in 1rst or 2nd position */
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
			if (daisychain_enabled == TRUE) {
				/* Note: daisychain_enabled == TRUE means that we have
				 * daisychain template. However:
				 * * when there are multiple devices, offset "-1" applies
				 *   since device.0 is a fake and actual devices start at
				 *   index 1
				 * * when there is only 1 device, offset doesn't apply since
				 *   the device index is "0"
				 */
				int daisychain_offset = 0;
				if (devices_count > 1)
					daisychain_offset = 1;

				if (su_info_p->flags & SU_TYPE_DAISY_1) {
					snprintf((char *)su_info_p->OID, SU_INFOSIZE, tmp_info_p->OID,
						daisychain_device_number - daisychain_offset, item_number);
				}
				else {
					snprintf((char *)su_info_p->OID, SU_INFOSIZE, tmp_info_p->OID,
						item_number, daisychain_device_number - daisychain_offset);
				}
			}
			else {
				snprintf((char *)su_info_p->OID, SU_INFOSIZE, tmp_info_p->OID, item_number);
			}
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
		}
		/* else, don't return STAT_SET_INVALID for mode==SU_MODE_SETVAR since we
		 * can be setting a server side variable! */
		else {
			if (mode==SU_MODE_INSTCMD) {
				free_info(su_info_p);
				return STAT_INSTCMD_UNKNOWN;
			}
			else {
				/* adapt info_type */
				if (su_info_p->info_type != NULL)
					snprintf((char *)su_info_p->info_type, SU_INFOSIZE, "%s", tmp_varname);
			}
		}
	}

	/* Sanity check */
	if (!su_info_p || !su_info_p->info_type || !(su_info_p->flags & SU_FLAG_OK)) {

		upsdebugx(2, "%s: info element unavailable %s", __func__, varname);

		/* Free template (outlet and outlet.group) */
		free_info(su_info_p);

		if (tmp_varname != NULL)
			free(tmp_varname);

		return STAT_SET_UNKNOWN;
	}

	/* set value into the device, using the provided one, or the default one otherwise */
	if (mode==SU_MODE_INSTCMD) {
		/* Sanity check: commands should either have a value or a default */
		if ( (val == NULL) && (su_info_p->dfl == NULL) ) {
			upsdebugx(1, "%s: cannot execute command '%s': a provided or default value is needed!", __func__, varname);
			return STAT_SET_INVALID;
		}
	}

	if (su_info_p->info_flags & ST_FLAG_STRING) {
		status = nut_snmp_set_str(su_info_p->OID, val ? val : su_info_p->dfl);
	}
	else {
		if (mode==SU_MODE_INSTCMD) {
			if ( !str_to_long(val ? val : su_info_p->dfl, &value, 10) ) {
				upsdebugx(1, "%s: cannot execute command '%s': value is not a number!", __func__, varname);
				return STAT_SET_INVALID;
			}
		}
		else {
			/* non string data may imply a value lookup */
			if (su_info_p->oid2info) {
				value = su_find_valinfo(su_info_p->oid2info, val ? val : su_info_p->dfl);
			}
			else {
				/* Convert value and apply multiplier */
				if ( !str_to_long(val, &value, 10) ) {
					upsdebugx(1, "%s: cannot set '%s': value is not a number!", __func__, varname);
					return STAT_SET_INVALID;
				}
				value = (long)((double)value / su_info_p->info_len);
			}
		}
		/* Actually apply the new value */
		if (SU_TYPE(su_info_p) == SU_TYPE_TIME) {
			status = nut_snmp_set_time(su_info_p->OID, value);
		}
		else {
			status = nut_snmp_set_int(su_info_p->OID, value);
		}
	}

	/* Process result */
	if (status == FALSE) {
		if (mode==SU_MODE_INSTCMD)
			upsdebugx(1, "%s: cannot execute command '%s'", __func__, varname);
		else
			upsdebugx(1, "%s: cannot set value %s on OID %s", __func__, val, su_info_p->OID);

		retval = STAT_SET_FAILED;
	}
	else {
		retval = STAT_SET_HANDLED;
		if (mode==SU_MODE_INSTCMD)
			upsdebugx(1, "%s: successfully sent command %s", __func__, varname);
		else {
			upsdebugx(1, "%s: successfully set %s to \"%s\"", __func__, varname, val);

			/* update info array: call dstate_setinfo, since flags and aux are
			 * already published, and this saves us some processing */
			dstate_setinfo(varname, "%s", val);
		}
	}

	/* Free template (outlet and outlet.group) */
	if (!strncmp(tmp_varname, "outlet", 6))
		free_info(su_info_p);
	free(tmp_varname);

	return retval;
}

/* set r/w INFO_ element to a value.
 * FIXME: make a common function with su_instcmd! */
int su_setvar(const char *varname, const char *val)
{
	return su_setOID(SU_MODE_SETVAR, varname, val);
}

/* Daisychain-aware function to add instant commands:
 * Every command that is valid for a device has to be added for device.0
 * This then allows to composite commands, called on device.0 and executed
 * on all devices of the daisychain */
int su_addcmd(snmp_info_t *su_info_p)
{
	upsdebugx(2, "entering %s(%s)", __func__, su_info_p->info_type);

	if (daisychain_enabled == TRUE) {
/* FIXME?: daisychain */
		for (current_device_number = 1 ; current_device_number <= devices_count ;
			current_device_number++)
		{
			process_template(SU_WALKMODE_INIT, "device", su_info_p);
		}
	}
	else {
		if (nut_snmp_get(su_info_p->OID) != NULL) {
			dstate_addcmd(su_info_p->info_type);
			upsdebugx(1, "%s: adding command '%s'", __func__, su_info_p->info_type);
		}
	}
	return 0;
}

/* process instant command and take action. */
int su_instcmd(const char *cmdname, const char *extradata)
{
	return su_setOID(SU_MODE_INSTCMD, cmdname, extradata);
}

/* FIXME: the below functions can be removed since these were for loading
 * the mib2nut information from a file instead of the .h definitions... */
/* return 1 if usable, 0 if not */
static int parse_mibconf_args(size_t numargs, char **arg)
{
	bool_t ret;

	/* everything below here uses up through arg[1] */
	if (numargs < 6)
		return 0;

	/* <info type> <info flags> <info len> <OID name> <default value> <value lookup> */

	/* special case for setting some OIDs value at driver startup */
	if (!strcmp(arg[0], "init")) {
		/* set value. */
		if (!strncmp(arg[1], "str", 3)) {
			ret = nut_snmp_set_str(arg[3], arg[4]);
		} else {
			ret = nut_snmp_set_int(arg[3], strtol(arg[4], NULL, 0));
		}

		if (ret == FALSE)
			upslogx(LOG_ERR, "%s: cannot set value %s for %s", __func__, arg[4], arg[3]);
		else
			upsdebugx(1, "%s: successfully set %s to \"%s\"", __func__, arg[0], arg[4]);

		return 1;
	}

	/* TODO: create the lookup table */
	upsdebugx(2, "%s, %s, %s, %s, %s, %s", arg[0], arg[1], arg[2], arg[3], arg[4], arg[5]);

	return 1;
}

/* called for fatal errors in parseconf like malloc failures */
static void mibconf_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf (*mib.conf): %s", errmsg);
}

/* load *mib.conf into an snmp_info_t structure */
void read_mibconf(char *mib)
{
	char	fn[SMALLBUF];
	PCONF_CTX_t	ctx;

	upsdebugx(2, "SNMP UPS driver: entering %s(%s)", __func__, mib);

	snprintf(fn, sizeof(fn), "%s/snmp/%s.conf", CONFPATH, mib);

	pconf_init(&ctx, mibconf_err);

	if (!pconf_file_begin(&ctx, fn))
		fatalx(EXIT_FAILURE, "%s", ctx.errmsg);

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			continue;
		}

		if (ctx.numargs < 1)
			continue;

		if (!parse_mibconf_args(ctx.numargs, ctx.arglist)) {
			unsigned int	i;
			char	errmsg[SMALLBUF];

			snprintf(errmsg, sizeof(errmsg),
				"mib.conf: invalid directive");

			for (i = 0; i < ctx.numargs; i++)
				snprintfcat(errmsg, sizeof(errmsg), " %s",
					ctx.arglist[i]);

			upslogx(LOG_WARNING, "%s", errmsg);
		}
	}
	pconf_finish(&ctx);
}
