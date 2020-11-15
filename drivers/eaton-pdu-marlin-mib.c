/*  eaton-pdu-marlin-mib.c - data to monitor Eaton ePDUs branded as:
 *                G2 Marlin SW / MI / MO / MA
 *                G3 Shark SW / MI / MO / MA
 *
 *  Copyright (C) 2008 - 2020
 * 		Arnaud Quette <arnaud.quette@gmail.com>
 * 		Arnaud Quette <ArnaudQuette@Eaton.com>
 *  Copyright (C) 2015 - 2017
 * 		Jim Klimov <EvgenyKlimov@Eaton.com>
 *
 *  Supported by Eaton <http://www.eaton.com>
 *   and previously MGE Office Protection Systems <http://www.mgeops.com>
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

#include "eaton-pdu-marlin-mib.h"
#include "dstate.h"

/* Eaton PDU-MIB - Marlin MIB
 * ************************** */

#define EATON_MARLIN_MIB_VERSION	"0.49"
#define EATON_MARLIN_SYSOID			".1.3.6.1.4.1.534.6.6.7"
#define EATON_MARLIN_OID_MODEL_NAME	".1.3.6.1.4.1.534.6.6.7.1.2.1.2.0"

static info_lkp_t marlin_outlet_status_info[] = {
	{ 0, "off", NULL, NULL },
	{ 1, "on", NULL, NULL },
	{ 2, "pendingOff", NULL, NULL }, /* transitional status */
	{ 3, "pendingOn", NULL, NULL },  /* transitional status */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_outletgroups_status_info[] = {
	{ 0, "off", NULL, NULL },
	{ 1, "on", NULL, NULL },
	{ 2, "rebooting", NULL, NULL }, /* transitional status */
	{ 3, "mixed", NULL, NULL },     /* transitional status, not sure what it means! */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_outlet_switchability_info[] = {
	{ 1, "yes", NULL, NULL }, /* switchable */
	{ 2, "no", NULL, NULL }, /* notSwitchable */
	{ 0, NULL, NULL, NULL }
};

/* Overall outlets switchability info for the unit.
 * This is refined per-outlet, depending on user configuration,
 * possibly disabling switchability of some outlets */
static info_lkp_t marlin_unit_switchability_info[] = {
	{ 0, "no", NULL, NULL },  /* unknown */
	{ 1, "yes", NULL, NULL }, /* switched */
	{ 2, "no", NULL, NULL },  /* advancedMonitored */
	{ 3, "yes", NULL, NULL }, /* managed */
	{ 4, "no", NULL, NULL },  /* monitored */
	{ 0, NULL, NULL, NULL }
};

/* The physical type of outlet */
static info_lkp_t marlin_outlet_type_info[] = {
	{ 0, "unknown", NULL, NULL },
	{ 1, "iecC13", NULL, NULL },
	{ 2, "iecC19", NULL, NULL },
	{ 10, "uk", NULL, NULL },
	{ 11, "french", NULL, NULL },
	{ 12, "schuko", NULL, NULL },
	{ 20, "nema515", NULL, NULL },
	{ 21, "nema51520", NULL, NULL },
	{ 22, "nema520", NULL, NULL },
	{ 23, "nemaL520", NULL, NULL },
	{ 24, "nemaL530", NULL, NULL },
	{ 25, "nema615", NULL, NULL },
	{ 26, "nema620", NULL, NULL },
	{ 27, "nemaL620", NULL, NULL },
	{ 28, "nemaL630", NULL, NULL },
	{ 29, "nemaL715", NULL, NULL },
	{ 30, "rf203p277", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_ambient_presence_info[] = {
	{ -1, "unknown", NULL, NULL },
	{ 0, "no", NULL, NULL },  /* disconnected */
	{ 1, "yes", NULL, NULL }, /* connected */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_threshold_status_info[] = {
	{ 0, "good", NULL, NULL },          /* No threshold triggered */
	{ 1, "warning-low", NULL, NULL },   /* Warning low threshold triggered */
	{ 2, "critical-low", NULL, NULL },  /* Critical low threshold triggered */
	{ 3, "warning-high", NULL, NULL },  /* Warning high threshold triggered */
	{ 4, "critical-high", NULL, NULL }, /* Critical high threshold triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_threshold_frequency_status_info[] = {
	{ 0, "good", NULL, NULL },          /* No threshold triggered */
	{ 1, "out-of-range", NULL, NULL },  /* Frequency out of range triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_ambient_drycontacts_info[] = {
	{ -1, "unknown", NULL, NULL },
	{ 0, "open", NULL, NULL },
	{ 1, "closed", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_threshold_voltage_alarms_info[] = {
	{ 0, "", NULL, NULL },                       /* No threshold triggered */
	{ 1, "low voltage warning!", NULL, NULL },   /* Warning low threshold triggered */
	{ 2, "low voltage critical!", NULL, NULL },  /* Critical low threshold triggered */
	{ 3, "high voltage warning!", NULL, NULL },  /* Warning high threshold triggered */
	{ 4, "high voltage critical!", NULL, NULL }, /* Critical high threshold triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_threshold_current_alarms_info[] = {
	{ 0, "", NULL, NULL },                       /* No threshold triggered */
	{ 1, "low current warning!", NULL, NULL },   /* Warning low threshold triggered */
	{ 2, "low current critical!", NULL, NULL },  /* Critical low threshold triggered */
	{ 3, "high current warning!", NULL, NULL },  /* Warning high threshold triggered */
	{ 4, "high current critical!", NULL, NULL }, /* Critical high threshold triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_threshold_frequency_alarm_info[] = {
	{ 0, "", NULL, NULL },                         /* No threshold triggered */
	{ 1, "frequency out of range!", NULL, NULL },  /* Frequency out of range triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_threshold_temperature_alarms_info[] = {
	{ 0, "", NULL, NULL },                           /* No threshold triggered */
	{ 1, "low temperature warning!", NULL, NULL },   /* Warning low threshold triggered */
	{ 2, "low temperature critical!", NULL, NULL },  /* Critical low threshold triggered */
	{ 3, "high temperature warning!", NULL, NULL },  /* Warning high threshold triggered */
	{ 4, "high temperature critical!", NULL, NULL }, /* Critical high threshold triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_threshold_humidity_alarms_info[] = {
	{ 0, "", NULL, NULL },                        /* No threshold triggered */
	{ 1, "low humidity warning!", NULL, NULL },   /* Warning low threshold triggered */
	{ 2, "low humidity critical!", NULL, NULL },  /* Critical low threshold triggered */
	{ 3, "high humidity warning!", NULL, NULL },  /* Warning high threshold triggered */
	{ 4, "high humidity critical!", NULL, NULL }, /* Critical high threshold triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_outlet_group_type_info[] = {
	{ 0, "unknown", NULL, NULL },
	{ 1, "breaker1pole", NULL, NULL },
	{ 2, "breaker2pole", NULL, NULL },
	{ 3, "breaker3pole", NULL, NULL },
	{ 4, "outlet-section", NULL, NULL },
	{ 5, "user-defined", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t marlin_input_type_info[] = {
	{ 1, "1", NULL, NULL }, /* singlePhase     */
	{ 2, "2", NULL, NULL }, /* splitPhase      */
	{ 3, "3", NULL, NULL }, /* threePhaseDelta */
	{ 4, "3", NULL, NULL }, /* threePhaseWye   */
	{ 0, NULL, NULL, NULL }
};

static char marlin_scratch_buf[20];

/* Compute the phase to which an outlet group is connected
 * WRT the number of phase(s) and the outlet group number.
 * Note that the group type (marlin_outlet_group_type_info) is
 *  not considered since this applies to any kind of group */
static const char *marlin_outlet_group_phase_fun(int outlet_group_nb)
{
	const char* str_phases_nb = dstate_getinfo("input.phases");
	int phases_nb = 1;
	if (str_phases_nb && (outlet_group_nb >= 0) ) {
		phases_nb = atoi(str_phases_nb);
		if (phases_nb == 1) {
			return "L1";
		}
		else { /* 3ph assumed, 2ph PDU don't exist! */
			if (outlet_group_nb > 3)
				phases_nb = (outlet_group_nb - 3);
			else
				phases_nb = outlet_group_nb;

			snprintf(marlin_scratch_buf, sizeof(marlin_scratch_buf), "L%i", phases_nb);
			if (phases_nb < 1 || phases_nb > 3)
				upsdebugx(3, "WARNING: %s got %i phases which is an unexpected amount",
				        __func__, phases_nb);

			return marlin_scratch_buf;
		}
	}
	return NULL;
}
static info_lkp_t marlin_outlet_group_phase_info[] = {
	{ 1, "dummy", marlin_outlet_group_phase_fun, NULL },
	{ 0, NULL, NULL, NULL }
};

/* Snmp2NUT lookup table for Eaton Marlin MIB */
static snmp_info_t eaton_marlin_mib[] = {

	/* Device collection */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.1.2.1.2.%i",
		"Eaton Powerware ePDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.1.2.1.4.%i",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "device.part", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.1.2.1.3.%i",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* For daisychain, there is only 1 physical interface! */
	{ "device.macaddr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.2.2.1.6.2",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* Daisychained devices support
	 * Notes: this definition is used to:
	 * - estimate the number of devices, based on the below OID iteration capabilities
	 * - determine the base index of the SNMP OID (ie 0 or 1) */
	{ "device.count", 0, 1, ".1.3.6.1.4.1.534.6.6.7.1.2.1.2.%i",
		"1", SU_FLAG_STATIC, NULL },

	/* UPS collection */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "EATON",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE,
		"1.3.6.1.4.1.534.6.6.7.1.2.1.2.%i",
		"Eaton Powerware ePDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL },

	/*	FIXME: use unitName.0	(ePDU)?
	 * { "ups.id", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_DEVICE_NAME,
		"unknown", SU_FLAG_STATIC | SU_FLAG_OK, NULL }, */
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.1.2.1.4.%i",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* FIXME: this entry should be SU_FLAG_SEMI_STATIC */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.1.2.1.5.%i",
		"", SU_FLAG_OK, NULL },
	{ "ups.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	 /* FIXME: needs a date reformating callback
	 *   2011-8-29,16:27:25.0,+1:0
	 *   Hex-STRING: 07 DB 08 1D 10 0C 36 00 2B 01 00 00
	 * { "ups.date", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.1.2.1.8.0",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	 * { "ups.time", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.1.2.1.8.0",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	 */

	/* Input collection */
	/* Historically, some of these data were previously published as
	 * outlet.{realpower,...}
	 * However, it's more suitable and logic to have these on input.{...}
	 */
	/* Note: the below gives the number of input, not the number of phase(s)! */
	/* inputCount.0; Value (Integer): 1
	{ "input.count", 0, 1, ".1.3.6.1.4.1.534.6.6.7.1.2.1.20.0",
		NULL, SU_FLAG_STATIC, NULL }, */
	/* Note: for daisychain mode, we must handle phase(s) per device, not as a whole */
	/* inputType.%i.1 = INTEGER: singlePhase (1) */
	{ "input.phases", 0, 1, ".1.3.6.1.4.1.534.6.6.7.3.1.1.2.%i.1",
		NULL, SU_FLAG_STATIC, &marlin_input_type_info[0] },

	/* Frequency is measured globally */
	{ "input.frequency", 0, 0.1, ".1.3.6.1.4.1.534.6.6.7.3.1.1.3.%i.1",
		NULL, 0, NULL },
	{ "input.frequency.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.1.1.4.%i.1",
		NULL, SU_FLAG_OK, &marlin_threshold_frequency_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.1.1.4.%i.1",
		NULL, SU_FLAG_OK, &marlin_threshold_frequency_alarm_info[0] },

	/* inputCurrentPercentLoad (measured globally)
	 * Current percent load, based on the rated current capacity */
	/* FIXME: input.load is mapped on input.L1.load for both single and 3phase !!! */
	{ "input.load", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.3.1.11.%i.1.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "input.L1.load", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.3.1.11.%i.1.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "input.L2.load", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.3.1.11.%i.1.2",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "input.L3.load", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.3.1.11.%i.1.3",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },

	/* FIXME:
	 * - Voltage is only measured per phase, as mV!
	 *   so input.voltage == input.L1.voltage for both single and 3phase
	 * - As per NUT namespace (http://www.networkupstools.org/docs/developer-guide.chunked/apas01.html#_valid_contexts)
	 *   Voltage has to be expressed either phase-phase or phase-neutral
	 *   This is depending on OID inputVoltageMeasType
	 *   INTEGER {singlePhase (1),phase1toN (2),phase2toN (3),phase3toN (4),phase1to2 (5),phase2to3 (6),phase3to1 (7)
	 * 		=> RFC input.Lx.voltage.context */
	{ "input.voltage", 0, 0.001, ".1.3.6.1.4.1.534.6.6.7.3.2.1.3.%i.1.1",
		NULL, 0, NULL },
	{ "input.voltage.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.4.%i.1.1",
		NULL, SU_FLAG_OK, &marlin_threshold_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.4.%i.1.1",
		NULL, SU_FLAG_OK, &marlin_threshold_voltage_alarms_info[0] },
	{ "input.voltage.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.5.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.voltage.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.6.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.voltage.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.7.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.voltage.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.8.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L1.voltage", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.3.%i.1.1",
		NULL, 0, NULL },
	{ "input.L1.voltage.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.4.%i.1.1",
		NULL, SU_FLAG_OK, &marlin_threshold_status_info[0] },
	{ "L1.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.4.%i.1.1",
		NULL, SU_FLAG_OK, &marlin_threshold_voltage_alarms_info[0] },
	{ "input.L1.voltage.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.5.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L1.voltage.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.6.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L1.voltage.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.7.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L1.voltage.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.8.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L2.voltage", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.3.%i.1.2",
		NULL, 0, NULL },
	{ "input.L2.voltage.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.4.%i.1.2",
		NULL, SU_FLAG_OK, &marlin_threshold_status_info[0] },
	{ "L2.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.4.%i.1.2",
		NULL, SU_FLAG_OK, &marlin_threshold_voltage_alarms_info[0] },
	{ "input.L2.voltage.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.5.%i.1.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L2.voltage.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.6.%i.1.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L2.voltage.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.7.%i.1.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L2.voltage.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.8.%i.1.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L3.voltage", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.3.%i.1.3",
		NULL, 0, NULL },
	{ "input.L3.voltage.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.4.%i.1.3",
		NULL, SU_FLAG_OK, &marlin_threshold_status_info[0] },
	{ "L3.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.4.%i.1.3",
		NULL, SU_FLAG_OK, &marlin_threshold_voltage_alarms_info[0] },
	{ "input.L3.voltage.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.5.%i.1.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L3.voltage.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.6.%i.1.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L3.voltage.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.7.%i.1.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L3.voltage.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.2.1.8.%i.1.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* FIXME:
	 * - input.current is mapped on input.L1.current for both single and 3phase !!! */
	{ "input.current", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.4.%i.1.1",
		NULL, 0, NULL },
	{ "input.current.nominal", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.3.%i.1.1",
		NULL, 0, NULL },
	{ "input.current.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.5.%i.1.1",
		NULL, SU_FLAG_OK, &marlin_threshold_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.5.%i.1.1",
		NULL, SU_FLAG_OK, &marlin_threshold_current_alarms_info[0] },
	{ "input.current.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.6.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.current.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.7.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.current.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.8.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.current.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.9.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L1.current", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.4.0.1.1",
		NULL, 0, NULL },
	{ "input.L1.current.nominal", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.3.%i.1.1",
		NULL, 0, NULL },
	{ "input.L1.current.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.5.%i.1.1",
		NULL, SU_FLAG_OK, &marlin_threshold_status_info[0] },
	{ "L1.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.5.%i.1.1",
		NULL, SU_FLAG_OK, &marlin_threshold_current_alarms_info[0] },
	{ "input.L1.current.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.6.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L1.current.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.7.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L1.current.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.8.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L1.current.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.9.%i.1.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L2.current", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.4.%i.1.2",
		NULL, 0, NULL },
	{ "input.L2.current.nominal", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.3.%i.1.2",
		NULL, 0, NULL },
	{ "input.L2.current.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.5.%i.1.2",
		NULL, SU_FLAG_OK, &marlin_threshold_status_info[0] },
	{ "L2.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.5.%i.1.2",
		NULL, SU_FLAG_OK, &marlin_threshold_current_alarms_info[0] },
	{ "input.L2.current.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.6.%i.1.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L2.current.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.7.%i.1.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L2.current.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.8.%i.1.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L2.current.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.9.%i.1.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L3.current", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.4.%i.1.3",
		NULL, 0, NULL },
	{ "input.L3.current.nominal", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.3.%i.1.3",
		NULL, 0, NULL },
	{ "input.L3.current.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.5.%i.1.3",
		NULL, SU_FLAG_OK, &marlin_threshold_status_info[0] },
	{ "L3.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.5.%i.1.3",
		NULL, SU_FLAG_OK, &marlin_threshold_current_alarms_info[0] },
	{ "input.L3.current.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.6.%i.1.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L3.current.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.7.%i.1.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L3.current.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.8.%i.1.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	{ "input.L3.current.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.3.3.1.9.%i.1.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* Sum of all phases realpower, valid for Shark 1ph/3ph only */
	{ "input.realpower", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.5.1.4.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE | SU_FLAG_OK, NULL },
	/* Fallback 1: Sum of all phases realpower, valid for Marlin 3ph only */
	{ "input.realpower", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.4.%i.1.4",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE | SU_FLAG_OK, NULL },
	/* Fallback 2: Sum of the phase realpower, valid for Marlin 1ph only */
	{ "input.realpower", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.4.%i.1.2",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "input.L1.realpower", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.4.%i.1.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "input.L2.realpower", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.4.%i.1.2",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "input.L3.realpower", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.4.%i.1.3",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* Sum of all phases apparent power, valid for Shark 1ph/3ph only */
	{ "input.power", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.5.1.3.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE | SU_FLAG_OK, NULL },
	/* Fallback 1: Sum of all phases realpower, valid for Marlin 3ph only */
	{ "input.power", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.3.%i.1.4",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE | SU_FLAG_OK, NULL },
	/* Fallback 2: Sum of the phase realpower, valid for Marlin 1ph only */
	{ "input.power", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.3.%i.1.2",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "input.L1.power", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.3.%i.1.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "input.L2.power", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.3.%i.1.2",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "input.L3.power", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.3.%i.1.3",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },

	/* Ambient collection */
	{ "ambient.present", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.7.1.1.3.%i.1",
		NULL, SU_FLAG_OK, &marlin_ambient_presence_info[0] },
	{ "ambient.temperature.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.7.1.1.5.%i.1",
		NULL, SU_FLAG_OK, &marlin_threshold_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.7.1.1.5.%i.1",
		NULL, SU_FLAG_OK, &marlin_threshold_temperature_alarms_info[0] },
	{ "ambient.temperature", 0, 0.1, ".1.3.6.1.4.1.534.6.6.7.7.1.1.4.%i.1",
		NULL, SU_FLAG_OK, NULL },
	/* Low and high threshold use the respective critical levels */
	{ "ambient.temperature.low", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.534.6.6.7.7.1.1.7.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.temperature.low.critical", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.534.6.6.7.7.1.1.7.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.temperature.low.warning", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.534.6.6.7.7.1.1.6.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.temperature.high", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.534.6.6.7.7.1.1.9.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.temperature.high.warning", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.534.6.6.7.7.1.1.8.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.temperature.high.critical", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.534.6.6.7.7.1.1.9.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.humidity.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.7.2.1.5.%i.1",
		NULL, SU_FLAG_OK, &marlin_threshold_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.7.2.1.5.%i.1",
		NULL, SU_FLAG_OK, &marlin_threshold_humidity_alarms_info[0] },
	{ "ambient.humidity", 0, 0.1, ".1.3.6.1.4.1.534.6.6.7.7.2.1.4.%i.1",
		NULL, SU_FLAG_OK, NULL },
	/* Low and high threshold use the respective critical levels */
	{ "ambient.humidity.low", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.534.6.6.7.7.2.1.7.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.humidity.low.warning", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.534.6.6.7.7.2.1.6.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.humidity.low.critical", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.534.6.6.7.7.2.1.7.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.humidity.high", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.534.6.6.7.7.2.1.9.%i.1", NULL,
		SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.humidity.high.warning", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.534.6.6.7.7.2.1.8.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.humidity.high.critical", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.534.6.6.7.7.2.1.9.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* Dry contacts on TH module */
	{ "ambient.contacts.1.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.7.3.1.4.%i.1",
		NULL, SU_FLAG_OK, &marlin_ambient_drycontacts_info[0] },
	{ "ambient.contacts.2.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.7.3.1.4.%i.2",
		NULL, SU_FLAG_OK, &marlin_ambient_drycontacts_info[0] },

	/* Outlet collection */
	{ "outlet.count", 0, 1, ".1.3.6.1.4.1.534.6.6.7.1.2.1.22.%i",
		"0", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "outlet.id", 0, 1, NULL,
		"0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "All outlets",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	/* UnitType
	 * used to depict the overall outlets switchability of the unit */
	{ "outlet.switchable", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.1.2.1.10.%i",
		"no", 0, &marlin_unit_switchability_info[0] },
	/* The below ones are the same as the input.* equivalent */
	/* FIXME: transition period, TO BE REMOVED, moved to input.* */
	{ "outlet.frequency", 0, 0.1, ".1.3.6.1.4.1.534.6.6.7.3.1.1.3.%i.1",
		NULL, 0, NULL },
	{ "outlet.voltage", 0, 0.001, ".1.3.6.1.4.1.534.6.6.7.3.2.1.3.%i.1.1",
		NULL, 0, NULL },
	{ "outlet.current", 0, 0.01, ".1.3.6.1.4.1.534.6.6.7.3.3.1.4.%i.1.1",
		NULL, 0, NULL },
	{ "outlet.realpower", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.4.%i.1.4",
		NULL, 0, NULL },
	{ "outlet.power", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.4.1.3.%i.1.4",
		NULL, 0, NULL },

	/* outlet template definition
	 * Indexes start from 1, ie outlet.1 => <OID>.1 */
	/* Note: the first definition is used to determine the base index (ie 0 or 1) */
	/* outletName: Outlet friendly name, which can be modified by the user */
	{ "outlet.%i.desc", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.1.1.3.%i.%i",
		NULL, SU_FLAG_STATIC | SU_FLAG_OK | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.6.1.2.%i.%i",
		NULL, SU_FLAG_OK | SU_OUTLET | SU_TYPE_DAISY_1, &marlin_outlet_status_info[0] },
	/* Numeric identifier of the outlet, tied to the whole unit */
	{ "outlet.%i.id", 0, 1,
		".1.3.6.1.4.1.534.6.6.7.6.6.1.7.%i.%i",
		NULL, SU_FLAG_STATIC | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* outletID: Outlet physical name, related to its number in the group
	 * ex: first outlet of the second group (B) is B1 */
	{ "outlet.%i.name", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.1.1.2.%i.%i",
		NULL, SU_FLAG_STATIC | SU_FLAG_OK | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* FIXME: the last part of the OID gives the group number (i.e. %i.1 means "group 1")
	 * Need to address that, without multiple declaration (%i.%i, SU_OUTLET | SU_OUTLET_GROUP)? */
	{ "outlet.%i.groupid", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.2.1.3.%i.%i.1",
		NULL, SU_FLAG_STATIC | SU_FLAG_UNIQUE | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.groupid", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.2.1.3.%i.%i.2",
		NULL, SU_FLAG_STATIC | SU_FLAG_UNIQUE | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.groupid", ST_FLAG_STRING, SU_INFOSIZE,
		 ".1.3.6.1.4.1.534.6.6.7.6.2.1.3.%i.%i.3",
		NULL, SU_FLAG_STATIC | SU_FLAG_UNIQUE | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.groupid", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.2.1.3.%i.%i.4",
		NULL, SU_FLAG_STATIC | SU_FLAG_UNIQUE | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.groupid", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.2.1.3.%i.%i.5",
		NULL, SU_FLAG_STATIC | SU_FLAG_UNIQUE | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.groupid", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.2.1.3.%i.%i.6",
		NULL, SU_FLAG_STATIC | SU_FLAG_UNIQUE | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.current", 0, 0.001, ".1.3.6.1.4.1.534.6.6.7.6.4.1.3.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.current.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.4.1.4.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, &marlin_threshold_status_info[0] },
	{ "outlet.%i.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.4.1.4.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, &marlin_threshold_current_alarms_info[0] },
	{ "outlet.%i.current.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.6.4.1.5.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.current.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.6.4.1.6.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.current.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.6.4.1.7.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.current.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.6.4.1.8.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.realpower", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.6.5.1.3.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.voltage", 0, 0.001, ".1.3.6.1.4.1.534.6.6.7.6.3.1.2.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.voltage.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.3.1.3.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, &marlin_threshold_status_info[0] },
	{ "outlet.%i.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.3.1.3.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, &marlin_threshold_voltage_alarms_info[0] },
	{ "outlet.%i.voltage.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.6.3.1.4.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.voltage.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.6.3.1.5.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.voltage.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.6.3.1.6.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.voltage.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.6.3.1.7.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.power", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.6.5.1.2.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* outletControlSwitchable */
	{ "outlet.%i.switchable", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.6.1.9.%i.%i",
		"no", SU_OUTLET | SU_TYPE_DAISY_1, &marlin_outlet_switchability_info[0] },
	{ "outlet.%i.type", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.6.1.1.5.%i.%i",
		"unknown", SU_FLAG_STATIC | SU_OUTLET | SU_TYPE_DAISY_1, &marlin_outlet_type_info[0] },

	/* TODO: handle statistics
	 * outletWh.0.1
	 * outletWhTimer.0.1
	 */

	/* Outlet groups collection */
	{ "outlet.group.count", 0, 1, ".1.3.6.1.4.1.534.6.6.7.1.2.1.21.%i",
		"0", SU_FLAG_STATIC | SU_TYPE_DAISY_1, NULL },
	/* outlet groups template definition
	 * Indexes start from 1, ie outlet.group.1 => <OID>.1 */
	/* Note: the first definition is used to determine the base index (ie 0 or 1) */
	/* groupID.0.1 = OctetString: A */
	{ "outlet.group.%i.id", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.5.1.1.2.%i.%i",
		NULL, SU_FLAG_STATIC | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupName.0.1 = OctetString: Factory Group 1 */
	/* FIXME: SU_FLAG_SEMI_STATIC or SU_FLAG_SETTING => refreshed from time to time or upon call to setvar */
	{ "outlet.group.%i.name", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.5.1.1.3.%i.%i",
		NULL, SU_FLAG_STATIC | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupType.0.1 = Integer: outletSection  (4) */
	{ "outlet.group.%i.type", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.5.1.1.4.%i.%i",
		NULL, SU_FLAG_STATIC | SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&marlin_outlet_group_type_info[0] },
	{ "outlet.group.%i.phase", 0, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.5.1.1.2.%i.%i",
		NULL, SU_FLAG_STATIC | SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&marlin_outlet_group_phase_info[0] },
	/* groupControlStatus.0.1 = Integer: on  (1) */
	{ "outlet.group.%i.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.5.6.1.2.%i.%i",
		NULL, SU_FLAG_OK | SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&marlin_outletgroups_status_info[0] },
	/* groupChildCount.0.1 = Integer: 12 */
	{ "outlet.group.%i.count", 0, 1,
		".1.3.6.1.4.1.534.6.6.7.5.1.1.6.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupVoltage.0.1 = Integer: 243080 */
	{ "outlet.group.%i.voltage", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.5.3.1.3.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupVoltageThStatus.0.1 = Integer: good (0) */
	{ "outlet.group.%i.voltage.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.5.3.1.4.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&marlin_threshold_status_info[0] },
	{ "outlet.group.%i.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.5.3.1.4.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&marlin_threshold_voltage_alarms_info[0] },
	{ "outlet.group.%i.voltage.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.5.3.1.5.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	{ "outlet.group.%i.voltage.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.5.3.1.6.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	{ "outlet.group.%i.voltage.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.5.3.1.7.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	{ "outlet.group.%i.voltage.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.5.3.1.8.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupCurrent.0.1 = Integer: 0 */
	{ "outlet.group.%i.current", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.5.4.1.3.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupCurrentCapacity.0.1 = Integer: 16000 */
	{ "outlet.group.%i.current.nominal", 0, 0.001,
		".1.3.6.1.4.1.534.6.6.7.5.4.1.2.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupCurrentThStatus.0.1 = Integer: good  (0) */
	{ "outlet.group.%i.current.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.5.4.1.4.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&marlin_threshold_status_info[0] },
	{ "outlet.group.%i.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.6.7.5.4.1.4.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&marlin_threshold_current_alarms_info[0] },
	/* groupCurrentPercentLoad.0.1 = Integer: 0 */
	{ "outlet.group.%i.load", 0, 1.0,
		".1.3.6.1.4.1.534.6.6.7.5.4.1.10.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupCurrentThLowerWarning.0.1 = Integer: 0 */
	{ "outlet.group.%i.current.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.5.4.1.5.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupCurrentThLowerCritical.0.1 = Integer: -1 */
	{ "outlet.group.%i.current.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.5.4.1.6.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupCurrentThUpperWarning.0.1 = Integer: 12800 */
	{ "outlet.group.%i.current.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.5.4.1.7.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupCurrentThUpperCritical.0.1 = Integer: 16000 */
	{ "outlet.group.%i.current.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.534.6.6.7.5.4.1.8.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupWatts.0.1 = Integer: 2670 */
	{ "outlet.group.%i.realpower", 0, 1.0,
		".1.3.6.1.4.1.534.6.6.7.5.5.1.3.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupVA.0.1 = Integer: 3132 */
	{ "outlet.group.%i.power", 0, 1.0,
		".1.3.6.1.4.1.534.6.6.7.5.5.1.2.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },

	/* instant commands. */
	/* Notes:
	 * - load.cycle might be replaced by / mapped on shutdown.reboot
	 * - outletControl{Off,On,Reboot}Cmd values:
	 * 		0-n : Timer
	 * 		-1 : Cancel
	 * 		we currently use "0", so instant On | Off | Reboot... */
	/* no counterpart found!
	{ "outlet.load.off", 0, DO_OFF, AR_OID_OUTLET_STATUS ".0",
		NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.load.on", 0, DO_ON, AR_OID_OUTLET_STATUS ".0",
		NULL, SU_TYPE_CMD, NULL, NULL },
	{ "outlet.load.cycle", 0, DO_CYCLE, AR_OID_OUTLET_STATUS ".0",
		NULL, SU_TYPE_CMD, NULL, NULL }, */

	/* Delays handling:
	 * 0-n :Time in seconds until the group command is issued
	 * -1:Cancel a pending group-level Off/On/Reboot command */
	{ "outlet.%i.load.off", 0, 1, ".1.3.6.1.4.1.534.6.6.7.6.6.1.3.%i.%i",
		"0", SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.load.on", 0, 1, ".1.3.6.1.4.1.534.6.6.7.6.6.1.4.%i.%i",
		"0", SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.load.cycle", 0, 1, ".1.3.6.1.4.1.534.6.6.7.6.6.1.5.%i.%i",
		"0", SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* Delayed version, parameter is mandatory (so dfl is NULL)! */
	{ "outlet.%i.load.off.delay", 0, 1, ".1.3.6.1.4.1.534.6.6.7.6.6.1.3.%i.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.load.on.delay", 0, 1, ".1.3.6.1.4.1.534.6.6.7.6.6.1.4.%i.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	{ "outlet.%i.load.cycle.delay", 0, 1, ".1.3.6.1.4.1.534.6.6.7.6.6.1.5.%i.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL },

	/* Delays handling:
	 * 0-n :Time in seconds until the group command is issued
	 * -1:Cancel a pending group-level Off/On/Reboot command */
	/* groupControlOffCmd.0.1 = Integer: -1 */
	{ "outlet.group.%i.load.off", 0, 1,
		".1.3.6.1.4.1.534.6.6.7.5.6.1.3.%i.%i",
		"0", SU_TYPE_CMD | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupControl0nCmd.0.1 = Integer: -1 */
	{ "outlet.group.%i.load.on", 0, 1,
		".1.3.6.1.4.1.534.6.6.7.5.6.1.4.%i.%i",
		"0", SU_TYPE_CMD | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupControlRebootCmd.0.1 = Integer: -1 */
	{ "outlet.group.%i.load.cycle", 0, 1,
		".1.3.6.1.4.1.534.6.6.7.5.6.1.5.%i.%i",
		"0", SU_TYPE_CMD | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* Delayed version, parameter is mandatory (so dfl is NULL)! */
	{ "outlet.group.%i.load.off.delay", 0, 1,
		".1.3.6.1.4.1.534.6.6.7.5.6.1.3.%i.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupControl0nCmd.0.1 = Integer: -1 */
	{ "outlet.group.%i.load.on.delay", 0, 1,
		".1.3.6.1.4.1.534.6.6.7.5.6.1.4.%i.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* groupControlRebootCmd.0.1 = Integer: -1 */
	{ "outlet.group.%i.load.cycle.delay", 0, 1,
		".1.3.6.1.4.1.534.6.6.7.5.6.1.5.%i.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};


mib2nut_info_t	eaton_marlin = { "eaton_epdu", EATON_MARLIN_MIB_VERSION, NULL, EATON_MARLIN_OID_MODEL_NAME, eaton_marlin_mib, EATON_MARLIN_SYSOID, NULL };
