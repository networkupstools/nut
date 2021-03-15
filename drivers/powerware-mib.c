/*  powerware-mib.c - data to monitor Powerware UPS with NUT
 *  (using MIBs described in stdupsv1.mib and Xups.mib)
 *
 *  Copyright (C)
 *       2005-2006 Olli Savia <ops@iki.fi>
 *       2005-2006 Niels Baggesen <niels@baggesen.net>
 *       2015-2019 Arnaud Quette <ArnaudQuette@Eaton.com>
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

#include "powerware-mib.h"
#if WITH_SNMP_LKP_FUN
/* FIXME: shared helper code, need to be put in common */
#include "eaton-pdu-marlin-helpers.h"
#endif

#define PW_MIB_VERSION "0.98"

/* TODO: more sysOID and MIBs support:
 *
 * Powerware UPS (Ingrasys X-SLOT and BD-SLOT): ".1.3.6.1.4.1.534.1"
 * Powerware PXGX cards: ".1.3.6.1.4.1.534.2.12"
 *		PXGX 2000 cards (UPS): Get xupsIdentModel (".1.3.6.1.4.1.534.1.1.2.0")
 * 		PXGX 1000 cards (PDU/RPP/RPM): Get pduNumPanels ".1.3.6.1.4.1.534.6.6.4.1.1.1.4.0"
 */

/* Powerware UPS (Ingrasys X-SLOT and BD-SLOT)
 * Eaton Gigabit Network Card (Genepi) */
#define POWERWARE_SYSOID	".1.3.6.1.4.1.534.1"
/* Powerware UPS newer PXGX UPS cards (BladeUPS, ...) */
#define EATON_PXGX_SYSOID	".1.3.6.1.4.1.534.2.12"

/* SNMP OIDs set */
#define PW_OID_MFR_NAME		"1.3.6.1.4.1.534.1.1.1.0"	/* XUPS-MIB::xupsIdentManufacturer.0 */
#define PW_OID_MODEL_NAME	"1.3.6.1.4.1.534.1.1.2.0"	/* XUPS-MIB::xupsIdentModel.0 */
#define PW_OID_FIRMREV		"1.3.6.1.4.1.534.1.1.3.0"	/* XUPS-MIB::xupsIdentSoftwareVersion.0 */

#define PW_OID_BATT_RUNTIME	"1.3.6.1.4.1.534.1.2.1.0"	/* XUPS-MIB::xupsBatTimeRemaining.0 */
#define PW_OID_BATT_VOLTAGE	"1.3.6.1.4.1.534.1.2.2.0"	/* XUPS-MIB::xupsBatVoltage.0 */
#define PW_OID_BATT_CURRENT	"1.3.6.1.4.1.534.1.2.3.0"	/* XUPS-MIB::xupsBatCurrent.0 */
#define PW_OID_BATT_CHARGE	"1.3.6.1.4.1.534.1.2.4.0"	/* XUPS-MIB::xupsBatCapacity.0 */
#define PW_OID_BATT_STATUS	"1.3.6.1.4.1.534.1.2.5.0"	/* XUPS-MIB::xupsBatteryAbmStatus.0 */

#define PW_OID_IN_FREQUENCY	"1.3.6.1.4.1.534.1.3.1.0"	/* XUPS-MIB::xupsInputFrequency.0 */
#define PW_OID_IN_LINE_BADS	"1.3.6.1.4.1.534.1.3.2.0"	/* XUPS-MIB::xupsInputLineBads.0 */
#define PW_OID_IN_LINES		"1.3.6.1.4.1.534.1.3.3.0"	/* XUPS-MIB::xupsInputNumPhases.0 */
#define PW_OID_IN_VOLTAGE	"1.3.6.1.4.1.534.1.3.4.1.2"	/* XUPS-MIB::xupsInputVoltage */
#define PW_OID_IN_CURRENT	"1.3.6.1.4.1.534.1.3.4.1.3"	/* XUPS-MIB::xupsInputCurrent */
#define PW_OID_IN_POWER		"1.3.6.1.4.1.534.1.3.4.1.4"	/* XUPS-MIB::xupsInputWatts */

#define PW_OID_OUT_LOAD		"1.3.6.1.4.1.534.1.4.1.0"	/* XUPS-MIB::xupsOutputLoad.0 */
#define PW_OID_OUT_FREQUENCY	"1.3.6.1.4.1.534.1.4.2.0"	/* XUPS-MIB::xupsOutputFrequency.0 */
#define PW_OID_OUT_LINES	"1.3.6.1.4.1.534.1.4.3.0"	/* XUPS-MIB::xupsOutputNumPhases.0 */
#define PW_OID_OUT_VOLTAGE	"1.3.6.1.4.1.534.1.4.4.1.2"	/* XUPS-MIB::xupsOutputVoltage */
#define PW_OID_OUT_CURRENT	"1.3.6.1.4.1.534.1.4.4.1.3"	/* XUPS-MIB::xupsOutputCurrent */
#define PW_OID_OUT_POWER	"1.3.6.1.4.1.534.1.4.4.1.4"	/* XUPS-MIB::xupsOutputWatts */
#define PW_OID_POWER_STATUS	"1.3.6.1.4.1.534.1.4.5.0"	/* XUPS-MIB::xupsOutputSource.0 */

#define PW_OID_BY_FREQUENCY	"1.3.6.1.4.1.534.1.5.1.0"	/* XUPS-MIB::xupsBypassFrequency.0 */
#define PW_OID_BY_LINES		"1.3.6.1.4.1.534.1.5.2.0"	/* XUPS-MIB::xupsBypassNumPhases.0 */
#define PW_OID_BY_VOLTAGE	"1.3.6.1.4.1.534.1.5.3.1.2"	/* XUPS-MIB::xupsBypassVoltage */

#define PW_OID_BATTEST_START	"1.3.6.1.4.1.534.1.8.1"		/* XUPS-MIB::xupsTestBattery   set to startTest(1) to initiate test*/

#define PW_OID_CONT_OFFDELAY	"1.3.6.1.4.1.534.1.9.1.0"		/* XUPS-MIB::xupsControlOutputOffDelay */
#define PW_OID_CONT_ONDELAY	"1.3.6.1.4.1.534.1.9.2.0"		/* XUPS-MIB::xupsControlOutputOnDelay */
#define PW_OID_CONT_OFFT_DEL	"1.3.6.1.4.1.534.1.9.3"		/* XUPS-MIB::xupsControlOutputOffTrapDelay */
#define PW_OID_CONT_ONT_DEL	"1.3.6.1.4.1.534.1.9.4"		/* XUPS-MIB::xupsControlOutputOnTrapDelay */
#define PW_OID_CONT_LOAD_SHED_AND_RESTART	"1.3.6.1.4.1.534.1.9.6"		/* XUPS-MIB::xupsLoadShedSecsWithRestart */

#define PW_OID_CONF_OVOLTAGE	"1.3.6.1.4.1.534.1.10.1.0"	/* XUPS-MIB::xupsConfigOutputVoltage.0 */
#define PW_OID_CONF_IVOLTAGE	"1.3.6.1.4.1.534.1.10.2.0"	/* XUPS-MIB::xupsConfigInputVoltage.0 */
#define PW_OID_CONF_POWER	"1.3.6.1.4.1.534.1.10.3.0"	/* XUPS-MIB::xupsConfigOutputWatts.0 */
#define PW_OID_CONF_FREQ	"1.3.6.1.4.1.534.1.10.4.0"	/* XUPS-MIB::xupsConfigOutputFreq.0 */

#define PW_OID_ALARMS		"1.3.6.1.4.1.534.1.7.1.0"		/* XUPS-MIB::xupsAlarms */
#define PW_OID_ALARM_OB		"1.3.6.1.4.1.534.1.7.3"		/* XUPS-MIB::xupsOnBattery */
#define PW_OID_ALARM_LB		"1.3.6.1.4.1.534.1.7.4"		/* XUPS-MIB::xupsLowBattery */

#define IETF_OID_AGENTREV	"1.3.6.1.2.1.33.1.1.4.0"	/* UPS-MIB::upsIdentAgentSoftwareVersion.0 */
#define IETF_OID_IDENT		"1.3.6.1.2.1.33.1.1.5.0"	/* UPS-MIB::upsIdentName.0 */
#define IETF_OID_CONF_OUT_VA	"1.3.6.1.2.1.33.1.9.5.0"	/* UPS-MIB::upsConfigOutputVA.0 */
#define IETF_OID_CONF_RUNTIME_LOW	"1.3.6.1.2.1.33.1.9.7.0"	/* UPS-MIB::upsConfigLowBattTime.0 */
#define IETF_OID_LOAD_LEVEL	"1.3.6.1.2.1.33.1.4.4.1.5"	/* UPS-MIB::upsOutputPercentLoad */
#define IETF_OID_AUTO_RESTART	"1.3.6.1.2.1.33.1.8.5.0"	/* UPS-MIB::upsAutoRestart */

/* Delay before powering off in seconds */
#define DEFAULT_OFFDELAY	30
/* Delay before powering on in seconds */
#define DEFAULT_ONDELAY	20
/* Default shutdown.return delay in seconds */
#define DEFAULT_SHUTDOWNDELAY	0

static info_lkp_t pw_alarm_ob[] = {
	{ 1, "OB"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
} ;

static info_lkp_t pw_alarm_lb[] = {
	{ 1, "LB"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
} ;

static info_lkp_t pw_pwr_info[] = {
	{   1, ""         /* other */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   2, "OFF"       /* none */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   3, "OL"        /* normal */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   4, "BYPASS"    /* bypass */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   5, "OB"        /* battery */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   6, "OL BOOST"  /* booster */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   7, "OL TRIM"   /* reducer */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   8, "OL"        /* parallel capacity */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   9, "OL"        /* parallel redundancy */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{  10, "OL"        /* high efficiency */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	/* Extended status values */
	{ 240, "OB"        /* battery (0xF0) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 100, "BYPASS"    /* maintenanceBypass (0x64) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{  96, "BYPASS"    /* Bypass (0x60) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{  81, "OL"        /* high efficiency (0x51) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{  80, "OL"        /* normal (0x50) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{  64, "OL"        /* UPS supporting load, normal degraded mode (0x40) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{  16, "OFF"       /* none (0x10) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

/* FIXME: mapped to ups.type, but should be output.source or ups.mode (need RFC)
 * to complement the above ups.status
 * along with having ups.type as described hereafter*/
/* FIXME: should be used by ups.mode or output.source (need RFC);
 * Note: this define is not set via project options; code hidden with
 * commit to "snmp-ups: support newer Genepi management cards"
 */
#ifdef USE_PW_MODE_INFO
static info_lkp_t pw_mode_info[] = {
	{   1, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   2, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   3, "normal"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   4, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   5, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   6, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   7, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   8, "parallel capacity"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   9, "parallel redundancy"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{  10, "high efficiency"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	/* Extended status values,
	 * FIXME: check for source and completion */
	{ 240, ""                /* battery (0xF0) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 100, ""                /* maintenanceBypass (0x64) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{  96, ""                /* Bypass (0x60) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{  81, "high efficiency" /* high efficiency (0x51) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{  80, "normal"          /* normal (0x50) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{  64, ""                /* UPS supporting load, normal degraded mode (0x40) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{  16, ""                /* none (0x10) */
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{   0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};
#endif /* USE_PW_MODE_INFO */

/* FIXME: may be standardized
 * extracted from bcmxcp.c->BCMXCP_TOPOLOGY_*, Make some common definitions */
static info_lkp_t pw_topology_info[] = {
	{ 0x0000, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}, /* None; use the Table of Elements */
	{ 0x0010, "Off-line switcher, Single Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0020, "Line-Interactive UPS, Single Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0021, "Line-Interactive UPS, Two Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0022, "Line-Interactive UPS, Three Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0030, "Dual AC Input, On-Line UPS, Single Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0031, "Dual AC Input, On-Line UPS, Two Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0032, "Dual AC Input, On-Line UPS, Three Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0040, "On-Line UPS, Single Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0041, "On-Line UPS, Two Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0042, "On-Line UPS, Three Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0050, "Parallel Redundant On-Line UPS, Single Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0051, "Parallel Redundant On-Line UPS, Two Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0052, "Parallel Redundant On-Line UPS, Three Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0060, "Parallel for Capacity On-Line UPS, Single Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0061, "Parallel for Capacity On-Line UPS, Two Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0062, "Parallel for Capacity On-Line UPS, Three Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0102, "System Bypass Module, Three Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0122, "Hot-Tie Cabinet, Three Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0200, "Outlet Controller, Single Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0x0222, "Dual AC Input Static Switch Module, 3 Phase"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

/* Legacy implementation */
static info_lkp_t pw_battery_abm_status[] = {
	{ 1, "CHRG"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, "DISCHRG"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
/*
	{ 3, "Floating"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
*/
/*
	{ 4, "Resting"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
*/
/*
	{ 5, "Unknown"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
*/
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
} ;

static info_lkp_t pw_abm_status_info[] = {
	{ 1, "charging"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, "discharging"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 3, "floating"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 4, "resting"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 5, "unknown"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},   /* Undefined - ABM is not activated */
	{ 6, "disabled"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  /* ABM Charger Disabled */
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

static info_lkp_t pw_batt_test_info[] = {
	{ 1, "Unknown"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, "Done and passed"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 3, "Done and error"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 4, "In progress"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 5, "Not supported"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 6, "Inhibited"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 7, "Scheduled"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

static info_lkp_t pw_yes_no_info[] = {
	{ 1, "yes"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, "no"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

static info_lkp_t pw_outlet_status_info[] = {
	{ 1, "on"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, "off"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 3, "on"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  /* pendingOff, transitional status */
	{ 4, "off"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}, /* pendingOn, transitional status */
	/* { 5, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  unknown */
	/* { 6, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  reserved */
	{ 7, "off"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}, /* Failed in Closed position */
	{ 8, "on"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  /* Failed in Open position */
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

static info_lkp_t pw_ambient_drycontacts_info[] = {
	{ -1, "unknown"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 1, "opened"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, "closed"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 3, "opened"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}, /* openWithNotice   */
	{ 4, "closed"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}, /* closedWithNotice */
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

#if WITH_SNMP_LKP_FUN
/* Note: eaton_sensor_temperature_unit_fun() is defined in powerware-helpers.c
 * Future work for DMF might provide a same-named routine via LUA-C gateway.
 */

#if WITH_SNMP_LKP_FUN_DUMMY
/* Temperature unit consideration */
const char *eaton_sensor_temperature_unit_fun(long snmp_value)
		{ return "unknown"; }
/* FIXME: please DMF, though this should be in snmp-ups.c or equiv. */
const char *su_temperature_read_fun(long snmp_value)
	{ return "dummy"; };
#endif // WITH_SNMP_LKP_FUN_DUMMY

static info_lkp_t pw_sensor_temperature_unit_info[] = {
	{ 0, "dummy"
#if WITH_SNMP_LKP_FUN
		, eaton_sensor_temperature_unit_fun, NULL, NULL, NULL
#endif
	},
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

static info_lkp_t pw_sensor_temperature_read_info[] = {
	{ 0, "dummy"
#if WITH_SNMP_LKP_FUN
		, su_temperature_read_fun, NULL, NULL, NULL
#endif
	},
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

#else // if not WITH_SNMP_LKP_FUN:

/* FIXME: For now, DMF codebase falls back to old implementation with static
 * lookup/mapping tables for this, which can easily go into the DMF XML file.
 */
static info_lkp_t pw_sensor_temperature_unit_info[] = {
	{ 0, "kelvin"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 1, "celsius"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, "fahrenheit"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

#endif // WITH_SNMP_LKP_FUN

static info_lkp_t pw_ambient_drycontacts_polarity_info[] = {
	{ 0, "normal-opened"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 1, "normal-closed"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

static info_lkp_t pw_ambient_drycontacts_state_info[] = {
	{ 0, "inactive"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 1, "active"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

static info_lkp_t pw_emp002_ambient_presence_info[] = {
	{ 0, "unknown"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, "yes"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},     /* communicationOK */
	{ 3, "no"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},      /* communicationLost */
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

/* extracted from drivers/eaton-pdu-marlin-mib.c -> marlin_threshold_status_info */
static info_lkp_t pw_threshold_status_info[] = {
	{ 0, "good"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},          /* No threshold triggered */
	{ 1, "warning-low"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},   /* Warning low threshold triggered */
	{ 2, "critical-low"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  /* Critical low threshold triggered */
	{ 3, "warning-high"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  /* Warning high threshold triggered */
	{ 4, "critical-high"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}, /* Critical high threshold triggered */
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

/* extracted from drivers/eaton-pdu-marlin-mib.c -> marlin_threshold_xxx_alarms_info */
static info_lkp_t pw_threshold_temperature_alarms_info[] = {
	{ 0, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},                           /* No threshold triggered */
	{ 1, "low temperature warning!"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},   /* Warning low threshold triggered */
	{ 2, "low temperature critical!"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  /* Critical low threshold triggered */
	{ 3, "high temperature warning!"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  /* Warning high threshold triggered */
	{ 4, "high temperature critical!"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}, /* Critical high threshold triggered */
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};
static info_lkp_t pw_threshold_humidity_alarms_info[] = {
	{ 0, ""
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},                        /* No threshold triggered */
	{ 1, "low humidity warning!"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},   /* Warning low threshold triggered */
	{ 2, "low humidity critical!"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  /* Critical low threshold triggered */
	{ 3, "high humidity warning!"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  /* Warning high threshold triggered */
	{ 4, "high humidity critical!"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}, /* Critical high threshold triggered */
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

/* Snmp2NUT lookup table */

static snmp_info_t pw_mib[] = {
	/* FIXME: miss device page! */
	/* UPS page */
	/* info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_MFR_NAME, "",
		SU_FLAG_STATIC, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_MODEL_NAME, "",
		SU_FLAG_STATIC, NULL },
	/* FIXME: the 2 "firmware" entries below should be SU_FLAG_SEMI_STATIC */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_FIRMREV, "",
		0, NULL },
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_AGENTREV, "",
		0, NULL },
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE, IETF_OID_IDENT, "",
		SU_FLAG_STATIC, NULL },
	{ "ups.load", 0, 1.0, PW_OID_OUT_LOAD, "",
		SU_OUTPUT_1, NULL },
	/* FIXME: should be removed in favor of output.power */
	{ "ups.power", 0, 1.0, PW_OID_OUT_POWER ".1", "",
		0, NULL },
	/* Duplicate of the above entry, but pointing at the first index */
	/* xupsOutputWatts.1.0; Value (Integer): 300 */
	{ "ups.power", 0, 1.0, "1.3.6.1.4.1.534.1.4.4.1.4.1.0", "",
		0, NULL },

	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_POWER_STATUS, "OFF",
		SU_STATUS_PWR, &pw_pwr_info[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_ALARM_OB, "",
		SU_STATUS_BATT, &pw_alarm_ob[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_ALARM_LB, "",
		SU_STATUS_BATT, &pw_alarm_lb[0] },
	{ "ups.status", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_BATT_STATUS, "",
		SU_STATUS_BATT, &pw_battery_abm_status[0] },
#ifdef USE_PW_MODE_INFO
	/* FIXME: should be ups.mode or output.source (need RFC) */
	/* Note: this define is not set via project options; code hidden with
	 * commit to "snmp-ups: support newer Genepi management cards" */
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, PW_OID_POWER_STATUS, "",
		SU_FLAG_STATIC | SU_FLAG_OK, &pw_mode_info[0] },
#endif /* USE_PW_MODE_INFO */
	/* xupsTopologyType.0; Value (Integer): 32 */
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.4.1.534.1.13.1.0", "",
		SU_FLAG_STATIC | SU_FLAG_OK, &pw_topology_info[0] },
	/* FIXME: should be removed in favor of their output. equivalent! */
	{ "ups.realpower.nominal", 0, 1.0, PW_OID_CONF_POWER, "",
		0, NULL },
	/* FIXME: should be removed in favor of output.power.nominal */
	{ "ups.power.nominal", 0, 1.0, IETF_OID_CONF_OUT_VA, "",
		0, NULL },
	/* XUPS-MIB::xupsEnvAmbientTemp.0 */
	{ "ups.temperature", 0, 1.0, "1.3.6.1.4.1.534.1.6.1.0", "", 0, NULL },
	/* FIXME: These 2 data needs RFC! */
	/* XUPS-MIB::xupsEnvAmbientLowerLimit.0 */
	{ "ups.temperature.low", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.2.0", "", 0, NULL },
	/* XUPS-MIB::xupsEnvAmbientUpperLimit.0 */
	{ "ups.temperature.high", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.3.0", "", 0, NULL },
	/* XUPS-MIB::xupsTestBatteryStatus */
	{ "ups.test.result", ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.4.1.534.1.8.2.0", "", 0, &pw_batt_test_info[0] },
	/* UPS-MIB::upsAutoRestart */
	{ "ups.start.auto", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.2.1.33.1.8.5.0", "", SU_FLAG_OK, &pw_yes_no_info[0] },
	/* XUPS-MIB::xupsBatteryAbmStatus.0 */
	{ "battery.charger.status", ST_FLAG_STRING, SU_INFOSIZE, "1.3.6.1.4.1.534.1.2.5.0", "", SU_STATUS_BATT, &pw_abm_status_info[0] },

	/* Battery page */
	{ "battery.charge", 0, 1.0, PW_OID_BATT_CHARGE, "",
		0, NULL },
	{ "battery.runtime", 0, 1.0, PW_OID_BATT_RUNTIME, "",
		0, NULL },
	{ "battery.voltage", 0, 1.0, PW_OID_BATT_VOLTAGE, "",
		0, NULL },
	{ "battery.current", 0, 0.1, PW_OID_BATT_CURRENT, "",
		0, NULL },
	{ "battery.runtime.low", 0, 60.0, IETF_OID_CONF_RUNTIME_LOW, "",
		0, NULL },
	{ "battery.date", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.1.2.6.0", NULL, SU_FLAG_OK, NULL },

	/* Output page */
	{ "output.phases", 0, 1.0, PW_OID_OUT_LINES, "", 0, NULL },
	/* XUPS-MIB::xupsOutputFrequency.0 */
	{ "output.frequency", 0, 0.1, "1.3.6.1.4.1.534.1.4.2.0", "", 0, NULL },
	/* XUPS-MIB::xupsConfigOutputFreq.0 */
	{ "output.frequency.nominal", 0, 0.1, "1.3.6.1.4.1.534.1.10.4.0", "", 0, NULL },
	/* XUPS-MIB::xupsOutputVoltage.1 */
	{ "output.voltage", 0, 1.0, "1.3.6.1.4.1.534.1.4.4.1.2.1", "", SU_OUTPUT_1, NULL },
	/* Duplicate of the above entry, but pointing at the first index */
	/* xupsOutputVoltage.1.0; Value (Integer): 230 */
	{ "output.voltage", 0, 1.0, "1.3.6.1.4.1.534.1.4.4.1.2.1.0", "", SU_OUTPUT_1, NULL },
	/* XUPS-MIB::xupsConfigOutputVoltage.0 */
	{ "output.voltage.nominal", 0, 1.0, "1.3.6.1.4.1.534.1.10.1.0", "", 0, NULL },
	/* XUPS-MIB::xupsConfigLowOutputVoltageLimit.0 */
	{ "output.voltage.low", 0, 1.0, ".1.3.6.1.4.1.534.1.10.6.0", "", 0, NULL },
	/* XUPS-MIB::xupsConfigHighOutputVoltageLimit.0 */
	{ "output.voltage.high", 0, 1.0, ".1.3.6.1.4.1.534.1.10.7.0", "", 0, NULL },
	{ "output.current", 0, 1.0, PW_OID_OUT_CURRENT ".1", "",
		SU_OUTPUT_1, NULL },
	/* Duplicate of the above entry, but pointing at the first index */
	/* xupsOutputCurrent.1.0; Value (Integer): 0 */
	{ "output.current", 0, 1.0, "1.3.6.1.4.1.534.1.4.4.1.3.1.0", "",
		SU_OUTPUT_1, NULL },
	{ "output.realpower", 0, 1.0, PW_OID_OUT_POWER ".1", "",
		SU_OUTPUT_1, NULL },
	/* Duplicate of the above entry, but pointing at the first index */
	/* Name/OID: xupsOutputWatts.1.0; Value (Integer): 1200 */
	{ "output.realpower", 0, 1.0, "1.3.6.1.4.1.534.1.4.4.1.4.1.0", "",
		0, NULL },
	/* Duplicate of "ups.realpower.nominal"
	 * FIXME: map either ups or output, but not both (or have an auto-remap) */
	{ "output.realpower.nominal", 0, 1.0, PW_OID_CONF_POWER, "",
		0, NULL },
	{ "output.L1-N.voltage", 0, 1.0, PW_OID_OUT_VOLTAGE ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2-N.voltage", 0, 1.0, PW_OID_OUT_VOLTAGE ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3-N.voltage", 0, 1.0, PW_OID_OUT_VOLTAGE ".3", "",
		SU_OUTPUT_3, NULL },
	{ "output.L1.current", 0, 1.0, PW_OID_OUT_CURRENT ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2.current", 0, 1.0, PW_OID_OUT_CURRENT ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3.current", 0, 1.0, PW_OID_OUT_CURRENT ".3", "",
		SU_OUTPUT_3, NULL },
	{ "output.L1.realpower", 0, 1.0, PW_OID_OUT_POWER ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2.realpower", 0, 1.0, PW_OID_OUT_POWER ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3.realpower", 0, 1.0, PW_OID_OUT_POWER ".3", "",
		SU_OUTPUT_3, NULL },
	/* FIXME: should better be output.Lx.load */
	{ "output.L1.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".1", "",
		SU_OUTPUT_3, NULL },
	{ "output.L2.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".2", "",
		SU_OUTPUT_3, NULL },
	{ "output.L3.power.percent", 0, 1.0, IETF_OID_LOAD_LEVEL ".3", "",
		SU_OUTPUT_3, NULL },
	{ "output.voltage.nominal", 0, 1.0, PW_OID_CONF_OVOLTAGE, "",
		0, NULL },

	/* Input page */
	{ "input.phases", 0, 1.0, PW_OID_IN_LINES, "",
		0, NULL },
	{ "input.frequency", 0, 0.1, PW_OID_IN_FREQUENCY, "",
		0, NULL },
	{ "input.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".0", "",
		SU_INPUT_1, NULL },
	/* Duplicate of the above entry, but pointing at the first index */
	/* xupsInputVoltage.1[.0]; Value (Integer): 245 */
	{ "input.voltage", 0, 1.0, "1.3.6.1.4.1.534.1.3.4.1.2.1", "",
		SU_INPUT_1, NULL },

	/* XUPS-MIB::xupsConfigInputVoltage.0 */
	{ "input.voltage.nominal", 0, 1.0, "1.3.6.1.4.1.534.1.10.2.0", "", 0, NULL },
	{ "input.current", 0, 0.1, PW_OID_IN_CURRENT ".0", "",
		SU_INPUT_1, NULL },
	{ "input.L1-N.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".1", "",
		SU_INPUT_3, NULL },
	{ "input.L2-N.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".2", "",
		SU_INPUT_3, NULL },
	{ "input.L3-N.voltage", 0, 1.0, PW_OID_IN_VOLTAGE ".3", "",
		SU_INPUT_3, NULL },
	{ "input.L1.current", 0, 1.0, PW_OID_IN_CURRENT ".1", "",
		SU_INPUT_3, NULL },
	{ "input.L2.current", 0, 1.0, PW_OID_IN_CURRENT ".2", "",
		SU_INPUT_3, NULL },
	{ "input.L3.current", 0, 1.0, PW_OID_IN_CURRENT ".3", "",
		SU_INPUT_3, NULL },
	{ "input.L1.realpower", 0, 1.0, PW_OID_IN_POWER ".1", "",
		SU_INPUT_3, NULL },
	{ "input.L2.realpower", 0, 1.0, PW_OID_IN_POWER ".2", "",
		SU_INPUT_3, NULL },
	{ "input.L3.realpower", 0, 1.0, PW_OID_IN_POWER ".3", "",
		SU_INPUT_3, NULL },
	{ "input.quality", 0, 1.0, PW_OID_IN_LINE_BADS, "",
		0, NULL },

	/* FIXME: this segfaults! do we assume the same number of bypass phases as input phases?
	{ "input.bypass.phases", 0, 1.0, PW_OID_BY_LINES, "", 0, NULL }, */
	{ "input.bypass.frequency", 0, 0.1, PW_OID_BY_FREQUENCY, "", 0, NULL },
	{ "input.bypass.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".0", "",
		SU_INPUT_1, NULL },
	/* Duplicate of the above entry, but pointing at the first index */
	/* xupsBypassVoltage.1.0; Value (Integer): 244 */
	{ "input.bypass.voltage", 0, 1.0, "1.3.6.1.4.1.534.1.5.3.1.2.1.0", "",
		SU_INPUT_1, NULL },
	{ "input.bypass.L1-N.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".1", "",
		SU_INPUT_3, NULL },
	{ "input.bypass.L2-N.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".2", "",
		SU_INPUT_3, NULL },
	{ "input.bypass.L3-N.voltage", 0, 1.0, PW_OID_BY_VOLTAGE ".3", "",
		SU_INPUT_3, NULL },

	/* Outlet page */
	/* XUPS-MIB::xupsNumReceptacles; Value (Integer): 2 */
	{ "outlet.count", 0, 1, ".1.3.6.1.4.1.534.1.12.1.0", NULL, SU_FLAG_STATIC, NULL },
	/* XUPS-MIB::xupsRecepIndex.X; Value (Integer): X */
	{ "outlet.%i.id", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.1.%i", NULL, SU_FLAG_STATIC | SU_OUTLET, NULL },
	/* This MIB does not provide outlets switchability info. So map to a nearby
		OID, for data activation, and map all values to "yes" */
	{ "outlet.%i.switchable", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.1.%i", NULL, SU_FLAG_STATIC | SU_OUTLET, NULL },
	/* XUPS-MIB::xupsRecepStatus.X; Value (Integer): 1 */
	{ "outlet.%i.status", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.2.%i", NULL, SU_OUTLET, &pw_outlet_status_info[0] },

	/* Ambient collection */
	/* EMP001 (legacy) mapping */
	/* XUPS-MIB::xupsEnvRemoteTemp.0 */
	{ "ambient.temperature", 0, 1.0, "1.3.6.1.4.1.534.1.6.5.0", "", 0, NULL },
	/* XUPS-MIB::xupsEnvRemoteTempLowerLimit.0 */
	{ "ambient.temperature.low", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.9.0", "", 0, NULL },
	/* XUPS-MIB::xupsEnvRemoteTempUpperLimit.0 */
	{ "ambient.temperature.high", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.10.0", "", 0, NULL },
	/* XUPS-MIB::xupsEnvRemoteHumidity.0 */
	{ "ambient.humidity", 0, 1.0, "1.3.6.1.4.1.534.1.6.6.0", "", 0, NULL },
	/* XUPS-MIB::xupsEnvRemoteHumidityLowerLimit.0 */
	{ "ambient.humidity.low", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.11.0", "", 0, NULL },
	/* XUPS-MIB::xupsEnvRemoteHumidityUpperLimit.0 */
	{ "ambient.humidity.high", ST_FLAG_RW, 1.0, "1.3.6.1.4.1.534.1.6.12.0", "", 0, NULL },
	/* XUPS-MIB::xupsContactDescr.n */
	{ "ambient.contacts.1.name", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.1.6.8.1.4.1", "", 0, NULL },
	{ "ambient.contacts.2.name", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.1.6.8.1.4.2", "", 0, NULL },
	/* XUPS-MIB::xupsContactState.n */
	{ "ambient.contacts.1.status", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.1.6.8.1.3.1", "", 0, &pw_ambient_drycontacts_info[0] },
	{ "ambient.contacts.2.status", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.1.6.8.1.3.2", "", 0, &pw_ambient_drycontacts_info[0] },

	/* EMP002 (EATON EMP MIB) mapping, including daisychain support */
	/* Warning: indexes start at '1' not '0'! */
	/* sensorCount.0 */
	{ "ambient.count", ST_FLAG_RW, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.1.0", "", 0, NULL },
	/* CommunicationStatus.n */
	{ "ambient.%i.present", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.8.1.1.4.1.1.%i",
		NULL, SU_AMBIENT_TEMPLATE, &pw_emp002_ambient_presence_info[0] },
	/* sensorName.n: OctetString EMPDT1H1C2 @1 */
	{ "ambient.%i.name", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.3.1.1.%i", "", SU_AMBIENT_TEMPLATE, NULL },
	/* sensorManufacturer.n */
	{ "ambient.%i.mfr", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.2.1.6.%i", "", SU_AMBIENT_TEMPLATE, NULL },
	/* sensorModel.n */
	{ "ambient.%i.model", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.2.1.7.%i", "", SU_AMBIENT_TEMPLATE, NULL },
	/* sensorSerialNumber.n */
	{ "ambient.%i.serial", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.2.1.9.%i", "", SU_AMBIENT_TEMPLATE, NULL },
	/* sensorUuid.n */
	{ "ambient.%i.id", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.2.1.2.%i", "", SU_AMBIENT_TEMPLATE, NULL },
	/* sensorAddress.n */
	{ "ambient.%i.address", 0, 1, ".1.3.6.1.4.1.534.6.8.1.1.2.1.4.%i", "", SU_AMBIENT_TEMPLATE, NULL },
	/* sensorFirmwareVersion.n */
	{ "ambient.%i.firmware", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.2.1.10.%i", "", SU_AMBIENT_TEMPLATE, NULL },
	/* temperatureUnit.1
	 * MUST be before the temperature data reading! */
	{ "ambient.%i.temperature.unit", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.2.5.0", "", SU_AMBIENT_TEMPLATE, &pw_sensor_temperature_unit_info[0] },
	/* temperatureValue.n.1 */
	{ "ambient.%i.temperature", 0, 0.1, ".1.3.6.1.4.1.534.6.8.1.2.3.1.3.%i.1", "", SU_AMBIENT_TEMPLATE,
#if WITH_SNMP_LKP_FUN
	&pw_sensor_temperature_read_info[0]
#else
	NULL
#endif
	},
	{ "ambient.%i.temperature.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.8.1.2.3.1.1.%i.1",
		NULL, SU_AMBIENT_TEMPLATE, &pw_threshold_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.8.1.2.3.1.1.%i.1",
		NULL, SU_AMBIENT_TEMPLATE, &pw_threshold_temperature_alarms_info[0] },
	/* FIXME: ambient.n.temperature.{minimum,maximum} */
	/* temperatureThresholdLowCritical.n.1 */
	{ "ambient.%i.temperature.low.critical", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.2.2.1.6.%i.1", "", SU_AMBIENT_TEMPLATE, NULL },
	/* temperatureThresholdLowWarning.n.1 */
	{ "ambient.%i.temperature.low.warning", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.2.2.1.5.%i.1", "", SU_AMBIENT_TEMPLATE, NULL },
	/* temperatureThresholdHighWarning.n.1 */
	{ "ambient.%i.temperature.high.warning", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.2.2.1.7.%i.1", "", SU_AMBIENT_TEMPLATE, NULL },
	/* temperatureThresholdHighCritical.n.1 */
	{ "ambient.%i.temperature.high.critical", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.2.2.1.8.%i.1", "", SU_AMBIENT_TEMPLATE, NULL },
	/* humidityValue.n.1 */
	{ "ambient.%i.humidity", 0, 0.1, ".1.3.6.1.4.1.534.6.8.1.3.3.1.3.%i.1", "", SU_AMBIENT_TEMPLATE, NULL },
	{ "ambient.%i.humidity.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.8.1.3.3.1.1.%i.1",
		NULL, SU_AMBIENT_TEMPLATE, &pw_threshold_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.8.1.3.3.1.1.%i.1",
		NULL, SU_AMBIENT_TEMPLATE, &pw_threshold_humidity_alarms_info[0] },
	/* FIXME: consider ambient.n.humidity.{minimum,maximum} */
	/* humidityThresholdLowCritical.n.1 */
	{ "ambient.%i.humidity.low.critical", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.3.2.1.6.%i.1", "", SU_AMBIENT_TEMPLATE, NULL },
	/* humidityThresholdLowWarning.n.1 */
	{ "ambient.%i.humidity.low.warning", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.3.2.1.5.%i.1", "", SU_AMBIENT_TEMPLATE, NULL },
	/* humidityThresholdHighWarning.n.1 */
	{ "ambient.%i.humidity.high.warning", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.3.2.1.7.%i.1", "", SU_AMBIENT_TEMPLATE, NULL },
	/* humidityThresholdHighCritical.n.1 */
	{ "ambient.%i.humidity.high.critical", ST_FLAG_RW, 0.1, ".1.3.6.1.4.1.534.6.8.1.3.2.1.8.%i.1", "", SU_AMBIENT_TEMPLATE, NULL },
	/* digitalInputName.n.{1,2} */
	{ "ambient.%i.contacts.1.name", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.4.2.1.1.%i.1", "", SU_AMBIENT_TEMPLATE, NULL },
	{ "ambient.%i.contacts.2.name", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.4.2.1.1.%i.2", "", SU_AMBIENT_TEMPLATE, NULL },
	/* digitalInputPolarity.n */
	{ "ambient.%i.contacts.1.config", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.8.1.4.2.1.3.%i.1", "", SU_AMBIENT_TEMPLATE, &pw_ambient_drycontacts_polarity_info[0] },
	{ "ambient.%i.contacts.2.config", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.8.1.4.2.1.3.%i.2", "", SU_AMBIENT_TEMPLATE, &pw_ambient_drycontacts_polarity_info[0] },
	/* XUPS-MIB::xupsContactState.n */
	{ "ambient.%i.contacts.1.status", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.4.3.1.3.%i.1", "", SU_AMBIENT_TEMPLATE, &pw_ambient_drycontacts_state_info[0] },
	{ "ambient.%i.contacts.2.status", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.4.3.1.3.%i.2", "", SU_AMBIENT_TEMPLATE, &pw_ambient_drycontacts_state_info[0] },

	/* instant commands */
	{ "test.battery.start.quick", 0, 1, PW_OID_BATTEST_START, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* Shed load and restart when line power back on; cannot be canceled */
	{ "shutdown.return", 0, DEFAULT_SHUTDOWNDELAY, PW_OID_CONT_LOAD_SHED_AND_RESTART, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* Cancel output off, by writing 0 to xupsControlOutputOffDelay */
	{ "shutdown.stop", 0, 0, PW_OID_CONT_OFFDELAY, "",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* XUPS-MIB::xupsControlOutputOffDelay */
	/* load off after 1 sec, shortest possible delay; 0 cancels */
	{ "load.off", 0, 1, PW_OID_CONT_OFFDELAY, "1",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* Delayed version, parameter is mandatory (so dfl is NULL)! */
	{ "load.off.delay", 0, 1, PW_OID_CONT_OFFDELAY, NULL,
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* XUPS-MIB::xupsControlOutputOnDelay */
	/* load on after 1 sec, shortest possible delay; 0 cancels */
	{ "load.on", 0, 1, PW_OID_CONT_ONDELAY, "1",
		SU_TYPE_CMD | SU_FLAG_OK, NULL },
	/* Delayed version, parameter is mandatory (so dfl is NULL)! */
	{ "load.on.delay", 0, 1, PW_OID_CONT_ONDELAY, NULL,
		SU_TYPE_CMD | SU_FLAG_OK, NULL },

	/* Delays handling:
	 * 0-n :Time in seconds until the command is issued
	 * -1:Cancel a pending Off/On command */
	/* XUPS-MIB::xupsRecepOffDelaySecs.n */
	{ "outlet.%i.load.off", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.3.%i",
		"0", SU_TYPE_CMD | SU_OUTLET, NULL },
	/* XUPS-MIB::xupsRecepOnDelaySecs.n */
	{ "outlet.%i.load.on", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.4.%i",
		"0", SU_TYPE_CMD | SU_OUTLET, NULL },
	/* Delayed version, parameter is mandatory (so dfl is NULL)! */
	{ "outlet.%i.load.off.delay", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.3.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET, NULL },
	/* XUPS-MIB::xupsRecepOnDelaySecs.n */
	{ "outlet.%i.load.on.delay", 0, 1, ".1.3.6.1.4.1.534.1.12.2.1.4.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET, NULL },

	{ "ups.alarms", 0, 1.0, PW_OID_ALARMS, "",
		0, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
} ;

static alarms_info_t pw_alarms[] = {
	/* xupsLowBattery */
	{ PW_OID_ALARM_LB, "LB", NULL },
	/* xupsOutputOverload */
	{ ".1.3.6.1.4.1.534.1.7.7", "OVER", "Output overload!" },
	/* xupsInternalFailure */
	{ ".1.3.6.1.4.1.534.1.7.8", NULL, "Internal failure!" },
	/* xupsBatteryDischarged */
	{ ".1.3.6.1.4.1.534.1.7.9", NULL, "Battery discharged!" },
	/* xupsInverterFailure */
	{ ".1.3.6.1.4.1.534.1.7.10", NULL, "Inverter failure!" },
	/* xupsOnBypass
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.11", "BYPASS", "On bypass!" },
	/* xupsBypassNotAvailable
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.12", NULL, "Bypass not available!" },
	/* xupsOutputOff
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.13", "OFF", "Output off!" },
	/* xupsInputFailure
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.14", NULL, "Input failure!" },
	/* xupsBuildingAlarm
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.15", NULL, "Building alarm!" },
	/* xupsShutdownImminent */
	{ ".1.3.6.1.4.1.534.1.7.16", NULL, "Shutdown imminent!" },
	/* xupsOnInverter
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.17", NULL, "On inverter!" },
	/* xupsBreakerOpen
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.20", NULL, "Breaker open!" },
	/* xupsAlarmBatteryBad */
	{ ".1.3.6.1.4.1.534.1.7.23", "RB", "Battery bad!" },
	/* xupsOutputOffAsRequested
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.24", "OFF", "Output off as requested!" },
	/* xupsDiagnosticTestFailed
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.25", NULL, "Diagnostic test failure!" },
	/* xupsCommunicationsLost */
	{ ".1.3.6.1.4.1.534.1.7.26", NULL, "Communication with UPS lost!" },
	/* xupsUpsShutdownPending */
	{ ".1.3.6.1.4.1.534.1.7.27", NULL, "Shutdown pending!" },
	/* xupsAmbientTempBad */
	{ ".1.3.6.1.4.1.534.1.7.29", NULL, "Bad ambient temperature!" },
	/* xupsLossOfRedundancy */
	{ ".1.3.6.1.4.1.534.1.7.30", NULL, "Redundancy lost!" },
	/* xupsAlarmTempBad */
	{ ".1.3.6.1.4.1.534.1.7.31", NULL, "Bad temperature!" },
	/* xupsAlarmChargerFailed */
	{ ".1.3.6.1.4.1.534.1.7.32", NULL, "Charger failure!" },
	/* xupsAlarmFanFailure */
	{ ".1.3.6.1.4.1.534.1.7.33", NULL, "Fan failure!" },
	/* xupsAlarmFuseFailure */
	{ ".1.3.6.1.4.1.534.1.7.34", NULL, "Fuse failure!" },
	/* xupsPowerSwitchBad */
	{ ".1.3.6.1.4.1.534.1.7.35", NULL, "Powerswitch failure!" },
	/* xupsModuleFailure */
	{ ".1.3.6.1.4.1.534.1.7.36", NULL, "Parallel or composite module failure!" },
	/* xupsOnAlternatePowerSource
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.37", NULL, "Using alternative power source!" },
	/* xupsAltPowerNotAvailable
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.38", NULL, "Alternative power source unavailable!" },
	/* xupsRemoteTempBad */
	{ ".1.3.6.1.4.1.534.1.7.40", NULL, "Bad remote temperature!" },
	/* xupsRemoteHumidityBad */
	{ ".1.3.6.1.4.1.534.1.7.41", NULL, "Bad remote humidity!" },
	/* xupsAlarmOutputBad */
	{ ".1.3.6.1.4.1.534.1.7.42", NULL, "Bad output condition!" },
	/* xupsAlarmAwaitingPower
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event? */
	{ ".1.3.6.1.4.1.534.1.7.43", NULL, "Awaiting power!" },
	/* xupsOnMaintenanceBypass
	 * FIXME: informational (not an alarm),
	 * to RFC'ed for device.event?
	 * FIXME: NUT currently doesn't distinguish between Maintenance and
	 * Automatic Bypass (both published as "ups.alarm: BYPASS)
	 * Should we make the distinction? */
	{ ".1.3.6.1.4.1.534.1.7.44", "BYPASS", "On maintenance bypass!" },


	/* end of structure. */
	{ NULL, NULL, NULL }
} ;


mib2nut_info_t	powerware = { "pw", PW_MIB_VERSION, NULL, PW_OID_MODEL_NAME, pw_mib, POWERWARE_SYSOID , pw_alarms };
mib2nut_info_t	pxgx_ups = { "pxgx_ups", PW_MIB_VERSION, NULL, PW_OID_MODEL_NAME, pw_mib, EATON_PXGX_SYSOID , pw_alarms };
