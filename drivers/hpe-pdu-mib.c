/*  hpe-pdu-mib.c - subdriver to monitor HPE ePDU SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2016	Arnaud Quette <arnaud.quette@free.fr>
 *  2019        Arnaud Quette <ArnaudQuette@eaton.com>
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

#include "hpe-pdu-mib.h"
#include "dstate.h"

#define HPE_EPDU_MIB_VERSION      "0.31"
#define HPE_EPDU_MIB_SYSOID       ".1.3.6.1.4.1.232.165.7"
#define HPE_EPDU_OID_MODEL_NAME	".1.3.6.1.4.1.232.165.7.1.2.1.3.0"

static info_lkp_t hpe_pdu_outlet_status_info[] = {
	{ 1, "off", NULL, NULL },
	{ 2, "on", NULL, NULL },
	{ 3, "pendingOff", NULL, NULL }, /* transitional status */
	{ 4, "pendingOn", NULL, NULL },  /* transitional status */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_outletgroups_status_info[] = {
	{ 1, "N/A", NULL, NULL }, /* notApplicable, if group.type == outlet-section */
	{ 2, "on", NULL, NULL },  /* breakerOn */
	{ 3, "off", NULL, NULL }, /* breakerOff */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_outlet_switchability_info[] = {
	{ 1, "yes", NULL, NULL },
	{ 2, "no", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

/* The physical type of outlet */
static info_lkp_t hpe_pdu_outlet_type_info[] = {
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

static info_lkp_t hpe_pdu_ambient_presence_info[] = {
	{ -1, "unknown", NULL, NULL },
	{ 1, "no", NULL, NULL },  /* disconnected */
	{ 2, "yes", NULL, NULL }, /* connected */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_threshold_status_info[] = {
	{ 1, "good", NULL, NULL },          /* No threshold triggered */
	{ 2, "warning-low", NULL, NULL },   /* Warning low threshold triggered */
	{ 3, "critical-low", NULL, NULL },  /* Critical low threshold triggered */
	{ 4, "warning-high", NULL, NULL },  /* Warning high threshold triggered */
	{ 5, "critical-high", NULL, NULL }, /* Critical high threshold triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_threshold_frequency_status_info[] = {
	{ 1, "good", NULL, NULL },          /* No threshold triggered */
	{ 2, "out-of-range", NULL, NULL },  /* Frequency out of range triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_ambient_drycontacts_info[] = {
	{ -1, "unknown", NULL, NULL },
	{ 0, "unknown", NULL, NULL },
	{ 1, "open", NULL, NULL },
	{ 2, "closed", NULL, NULL },
	{ 3, "bad", NULL, NULL }, /* FIXME: what to do with that? */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_threshold_voltage_alarms_info[] = {
	{ 1, "", NULL, NULL },                       /* No threshold triggered */
	{ 2, "low voltage warning!", NULL, NULL },   /* Warning low threshold triggered */
	{ 3, "low voltage critical!", NULL, NULL },  /* Critical low threshold triggered */
	{ 4, "high voltage warning!", NULL, NULL },  /* Warning high threshold triggered */
	{ 5, "high voltage critical!", NULL, NULL }, /* Critical high threshold triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_threshold_current_alarms_info[] = {
	{ 1, "", NULL, NULL },                       /* No threshold triggered */
	{ 2, "low current warning!", NULL, NULL },   /* Warning low threshold triggered */
	{ 3, "low current critical!", NULL, NULL },  /* Critical low threshold triggered */
	{ 4, "high current warning!", NULL, NULL },  /* Warning high threshold triggered */
	{ 5, "high current critical!", NULL, NULL }, /* Critical high threshold triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_threshold_frequency_alarm_info[] = {
	{ 1, "", NULL, NULL },                         /* No threshold triggered */
	{ 2, "frequency out of range!", NULL, NULL },  /* Frequency out of range triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_threshold_temperature_alarms_info[] = {
	{ 1, "", NULL, NULL },                           /* No threshold triggered */
	{ 2, "low temperature warning!", NULL, NULL },   /* Warning low threshold triggered */
	{ 3, "low temperature critical!", NULL, NULL },  /* Critical low threshold triggered */
	{ 4, "high temperature warning!", NULL, NULL },  /* Warning high threshold triggered */
	{ 5, "high temperature critical!", NULL, NULL }, /* Critical high threshold triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_threshold_humidity_alarms_info[] = {
	{ 1, "", NULL, NULL },                        /* No threshold triggered */
	{ 2, "low humidity warning!", NULL, NULL },   /* Warning low threshold triggered */
	{ 3, "low humidity critical!", NULL, NULL },  /* Critical low threshold triggered */
	{ 4, "high humidity warning!", NULL, NULL },  /* Warning high threshold triggered */
	{ 5, "high humidity critical!", NULL, NULL }, /* Critical high threshold triggered */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_outlet_group_type_info[] = {
	{ 0, "unknown", NULL, NULL },
	{ 1, "unknown", NULL, NULL },
	{ 2, "breaker1pole", NULL, NULL },
	{ 3, "breaker2pole", NULL, NULL },
	{ 4, "breaker3pole", NULL, NULL },
	{ 5, "outlet-section", NULL, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_input_type_info[] = {
	{ 1, "1", NULL, NULL }, /* singlePhase     */
	{ 2, "2", NULL, NULL }, /* splitPhase      */
	{ 3, "3", NULL, NULL }, /* threePhaseDelta */
	{ 4, "3", NULL, NULL }, /* threePhaseWye   */
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t hpe_pdu_outlet_group_phase_info[] = {
	{ 1, "L1", NULL, NULL }, /* singlePhase */
	{ 2, "L1", NULL, NULL }, /* phase1toN   */
	{ 3, "L2", NULL, NULL }, /* phase2toN   */
	{ 4, "L3", NULL, NULL }, /* phase3toN   */
	{ 5, "L1", NULL, NULL }, /* phase1to2   */
	{ 6, "L2", NULL, NULL }, /* phase2to3   */
	{ 7, "L3", NULL, NULL }, /* phase3to1   */
	{ 0, NULL, NULL, NULL }
};

/* Snmp2NUT lookup table for HPE PDU MIB */
static snmp_info_t hpe_pdu_mib[] = {

	/* Device collection */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "HPE",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	/* pdu2Model.0 = STRING: "HP 8.6kVA 208V 30A 3Ph NA/JP maPDU" */
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.1.2.1.3.%i",
		"HPE ePDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* pdu2SerialNumber.0 = STRING: "CN94230105" */
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.1.2.1.7.%i",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "pdu",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	/* pdu2PartNumber.0 = STRING: "H8B52A" */
	{ "device.part", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.1.2.1.6.%i",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* For daisychain, there is only 1 physical interface! */
	{ "device.macaddr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.2.1.2.2.1.6.2",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* Daisychained devices support
	 * Notes: this definition is used to:
	 * - estimate the number of devices, based on the below OID iteration capabilities
	 * - determine the base index of the SNMP OID (ie 0 or 1) */
	/* pdu2NumberPDU.0 = INTEGER: 1 */
	{ "device.count", 0, 1,
		".1.3.6.1.4.1.232.165.7.1.1.0",
		"1", SU_FLAG_STATIC, NULL },

	/* UPS collection */
	{ "ups.mfr", ST_FLAG_STRING, SU_INFOSIZE, NULL, "HPE",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "ups.model", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.1.2.1.3.%i",
		"HPE ePDU", SU_FLAG_STATIC | SU_FLAG_OK, NULL },

	/*	FIXME: use unitName.0	(ePDU)?
	 * { "ups.id", ST_FLAG_STRING, SU_INFOSIZE, AR_OID_DEVICE_NAME,
		"unknown", SU_FLAG_STATIC | SU_FLAG_OK, NULL, NULL }, */
	{ "ups.serial", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.1.2.1.7.%i",
		"", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* FIXME: this entry should be SU_FLAG_SEMI_STATIC */
	/* pdu2FirmwareVersion.0 = STRING: "02.00.0043" */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.1.2.1.5.%i",
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
	/* Note: for daisychain mode, we must handle phase(s) per device, not as a whole */
	/* pdu2InputType.0 = INTEGER: threePhaseWye(4) */
	{ "input.phases", 0, 1, ".1.3.6.1.4.1.232.165.7.2.1.1.1.%i",
		NULL, SU_FLAG_STATIC, &hpe_pdu_input_type_info[0] },

	/* Frequency is measured globally */
	/* pdu2InputFrequency.0 = INTEGER: 500 */
	{ "input.frequency", 0, 0.1, ".1.3.6.1.4.1.232.165.7.2.1.1.2.%i",
		NULL, 0, NULL },
	/* pdu2InputFrequencyStatus.0 = INTEGER: good(1) */
	{ "input.frequency.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.1.1.3.%i",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_frequency_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.1.1.3.%i",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_frequency_alarm_info[0] },
	/* inputCurrentPercentLoad (measured globally)
	 * Current percent load, based on the rated current capacity */
	/* FIXME: input.load is mapped on input.L1.load for both single and 3phase !!! */
	{ "input.load", 0, 1.0, ".1.3.6.1.4.1.534.6.6.7.3.3.1.11.%i.1.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2InputPhaseCurrentPercentLoad.0.1 = INTEGER: 0 */
	{ "input.L1.load", 0, 1.0, ".1.3.6.1.4.1.232.165.7.2.2.1.18.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2InputPhaseCurrentPercentLoad.0.2 = INTEGER: 0 */
	{ "input.L1.load", 0, 1.0, ".1.3.6.1.4.1.232.165.7.2.2.1.18.%i.2",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2InputPhaseCurrentPercentLoad.0.3 = INTEGER: 0 */
	{ "input.L1.load", 0, 1.0, ".1.3.6.1.4.1.232.165.7.2.2.1.18.%i.3",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },

	/* FIXME:
	 * - Voltage is only measured per phase, as mV!
	 *   so input.voltage == input.L1.voltage for both single and 3phase
	 * - As per NUT namespace (http://www.networkupstools.org/docs/developer-guide.chunked/apas01.html#_valid_contexts)
	 *   Voltage has to be expressed either phase-phase or phase-neutral
	 *   This is depending on OID inputVoltageMeasType
	 *   INTEGER {singlePhase (1),phase1toN (2),phase2toN (3),phase3toN (4),phase1to2 (5),phase2to3 (6),phase3to1 (7)
	 * 		=> RFC input.Lx.voltage.context */
	/* pdu2InputPhaseVoltage.0.1 = INTEGER: 216790 */
	{ "input.voltage", 0, 0.001, ".1.3.6.1.4.1.232.165.7.2.2.1.3.%i.1",
		NULL, 0, NULL },
	/* pdu2InputPhaseVoltageThStatus.0.1 = INTEGER: good(1) */
	{ "input.voltage.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.4.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.4.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_voltage_alarms_info[0] },
	/* pdu2InputPhaseVoltageThLowerWarning.0.1 = INTEGER: 190000 */
	{ "input.voltage.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.5.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltageThLowerCritical.0.1 = INTEGER: 180000 */
	{ "input.voltage.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.6.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltageThUpperWarning.0.1 = INTEGER: 255000 */
	{ "input.voltage.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.7.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltageThUpperCritical.0.1 = INTEGER: 265000 */
	{ "input.voltage.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.8.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltage.0.1 = INTEGER: 216790 */
	{ "input.L1.voltage", 0, 0.001, ".1.3.6.1.4.1.232.165.7.2.2.1.3.%i.1",
		NULL, 0, NULL },
	/* pdu2InputPhaseVoltageThStatus.0.1 = INTEGER: good(1) */
	{ "input.L1.voltage.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.4.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_status_info[0] },
	{ "L1.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.4.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_voltage_alarms_info[0] },
	/* pdu2InputPhaseVoltageThLowerWarning.0.1 = INTEGER: 190000 */
	{ "input.L1.voltage.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.5.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltageThLowerCritical.0.1 = INTEGER: 180000 */
	{ "input.L1.voltage.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.6.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltageThUpperWarning.0.1 = INTEGER: 255000 */
	{ "input.L1.voltage.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.7.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltageThUpperCritical.0.1 = INTEGER: 265000 */
	{ "input.L1.voltage.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.8.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltage.0.2 = INTEGER: 216790 */
	{ "input.L2.voltage", 0, 0.001, ".1.3.6.1.4.1.232.165.7.2.2.1.3.%i.2",
		NULL, 0, NULL },
	/* pdu2InputPhaseVoltageThStatus.0.2 = INTEGER: good(1) */
	{ "input.L2.voltage.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.4.%i.2",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_status_info[0] },
	{ "L2.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.4.%i.2",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_voltage_alarms_info[0] },
	/* pdu2InputPhaseVoltageThLowerWarning.0.2 = INTEGER: 190000 */
	{ "input.L2.voltage.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.5.%i.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltageThLowerCritical.0.2 = INTEGER: 180000 */
	{ "input.L2.voltage.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.6.%i.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltageThUpperWarning.0.2 = INTEGER: 255000 */
	{ "input.L2.voltage.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.7.%i.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltageThUpperCritical.0.2 = INTEGER: 265000 */
	{ "input.L2.voltage.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.8.%i.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltage.0.3 = INTEGER: 216790 */
	{ "input.L3.voltage", 0, 0.001, ".1.3.6.1.4.1.232.165.7.2.2.1.3.%i.3",
		NULL, 0, NULL },
	/* pdu2InputPhaseVoltageThStatus.0.3 = INTEGER: good(1) */
	{ "input.L3.voltage.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.4.%i.3",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_status_info[0] },
	{ "L3.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.4.%i.3",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_voltage_alarms_info[0] },
	/* pdu2InputPhaseVoltageThLowerWarning.0.3 = INTEGER: 190000 */
	{ "input.L3.voltage.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.5.%i.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltageThLowerCritical.0.3 = INTEGER: 180000 */
	{ "input.L3.voltage.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.6.%i.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltageThUpperWarning.0.3 = INTEGER: 255000 */
	{ "input.L3.voltage.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.7.%i.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseVoltageThUpperCritical.0.3 = INTEGER: 265000 */
	{ "input.L3.voltage.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.8.%i.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* FIXME:
	 * - input.current is mapped on input.L1.current for both single and 3phase !!! */
	/* pdu2InputPhaseCurrent.0.1 = INTEGER: 185 */
	{ "input.current", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.11.%i.1",
		NULL, 0, NULL },
	/* pdu2InputPhaseCurrentRating.0.1 = INTEGER: 24000 */
	{ "input.current.nominal", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.10.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThStatus.0.1 = INTEGER: good(1) */
	{ "input.current.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.12.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.12.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_current_alarms_info[0] },
	/* pdu2InputPhaseCurrentThLowerWarning.0.1 = INTEGER: 0 */
	{ "input.current.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.13.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThLowerCritical.0.1 = INTEGER: -1 */
	{ "input.current.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.14.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThUpperWarning.0.1 = INTEGER: 19200 */
	{ "input.current.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.15.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThUpperCritical.0.1 = INTEGER: 24000 */
	{ "input.current.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.16.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrent.0.1 = INTEGER: 185 */
	{ "input.L1.current", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.11.%i.1",
		NULL, 0, NULL },
	/* pdu2InputPhaseCurrentRating.0.1 = INTEGER: 24000 */
	{ "input.L1.current.nominal", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.10.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThStatus.0.1 = INTEGER: good(1) */
	{ "input.L1.current.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.12.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_status_info[0] },
	{ "L1.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.12.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_current_alarms_info[0] },
	/* pdu2InputPhaseCurrentThLowerWarning.0.1 = INTEGER: 0 */
	{ "input.L1.current.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.13.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThLowerCritical.0.1 = INTEGER: -1 */
	{ "input.L1.current.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.14.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThUpperWarning.0.1 = INTEGER: 19200 */
	{ "input.L1.current.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.15.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThUpperCritical.0.1 = INTEGER: 24000 */
	{ "input.L1.current.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.16.%i.1",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrent.0.2 = INTEGER: 185 */
	{ "input.L2.current", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.11.%i.2",
		NULL, 0, NULL },
	/* pdu2InputPhaseCurrentRating.0.2 = INTEGER: 24000 */
	{ "input.L2.current.nominal", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.10.%i.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThStatus.0.2 = INTEGER: good(1) */
	{ "input.L2.current.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.12.%i.2",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_status_info[0] },
	{ "L2.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.12.%i.2",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_current_alarms_info[0] },
	/* pdu2InputPhaseCurrentThLowerWarning.0.2 = INTEGER: 0 */
	{ "input.L2.current.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.13.%i.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThLowerCritical.0.2 = INTEGER: -1 */
	{ "input.L2.current.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.14.%i.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThUpperWarning.0.2 = INTEGER: 19200 */
	{ "input.L2.current.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.15.%i.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThUpperCritical.0.2 = INTEGER: 24000 */
	{ "input.L2.current.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.16.%i.2",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrent.0.3 = INTEGER: 185 */
	{ "input.L3.current", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.11.%i.3",
		NULL, 0, NULL },
	/* pdu2InputPhaseCurrentRating.0.3 = INTEGER: 24000 */
	{ "input.L3.current.nominal", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.10.%i.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThStatus.0.3 = INTEGER: good(1) */
	{ "input.L3.current.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.12.%i.3",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_status_info[0] },
	{ "L2.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.2.2.1.12.%i.2",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_current_alarms_info[0] },
	/* pdu2InputPhaseCurrentThLowerWarning.0.3 = INTEGER: 0 */
	{ "input.L3.current.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.13.%i.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThLowerCritical.0.3 = INTEGER: -1 */
	{ "input.L3.current.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.14.%i.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThUpperWarning.0.3 = INTEGER: 19200 */
	{ "input.L3.current.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.15.%i.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPhaseCurrentThUpperCritical.0.3 = INTEGER: 24000 */
	{ "input.L3.current.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.2.2.1.16.%i.3",
		NULL, SU_FLAG_NEGINVALID, NULL },
	/* pdu2InputPowerWatts.0 = INTEGER: 19 */
	{ "input.realpower", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.2.1.1.5.%i",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE | SU_FLAG_OK, NULL },
	/* pdu2InputPhasePowerWatts.0.1 = INTEGER: 19 */
	{ "input.L1.realpower", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.2.2.1.21.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2InputPhasePowerWatts.0.2 = INTEGER: 0 */
	{ "input.L2.realpower", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.2.2.1.21.%i.2",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2InputPhasePowerWatts.0.3 = INTEGER: 0 */
	{ "input.L3.realpower", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.2.2.1.21.%i.3",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* Sum of all phases apparent power, valid for Shark 1ph/3ph only */
	/* pdu2InputPowerVA.0 = INTEGER: 39 */
	{ "input.power", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.2.1.1.4.%i",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_UNIQUE | SU_FLAG_OK, NULL },
	/* pdu2InputPhasePowerVA.0.1 = INTEGER: 40 */
	{ "input.L1.power", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.2.2.1.20.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2InputPhasePowerVA.0.2 = INTEGER: 0 */
	{ "input.L2.power", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.2.2.1.20.%i.2",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2InputPhasePowerVA.0.3 = INTEGER: 0 */
	{ "input.L3.power", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.2.2.1.20.%i.3",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },

	/* TODO: handle statistics */
	/* pdu2InputPowerWattHour.0 = INTEGER: 91819
	{ "unmapped.pdu2InputPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.7.2.1.1.6.0", NULL, SU_FLAG_OK, NULL }, */
	/* pdu2InputPowerWattHourTimer.0 = STRING: "16/10/2017,17:58:53"
	{ "unmapped.pdu2InputPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.7.2.1.1.7.0", NULL, SU_FLAG_OK, NULL }, */
	/* pdu2InputPowerFactor.0 = INTEGER: 483 */
	{ "input.powerfactor", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.2.1.1.8.%i",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },

	/* Ambient collection */
	/* pdu2TemperatureProbeStatus.0.1 = INTEGER: disconnected(1) */
	{ "ambient.present", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.4.2.1.3.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_ambient_presence_info[0] },
	/* pdu2TemperatureThStatus.0.1 = INTEGER: good(1) */
	{ "ambient.temperature.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.4.2.1.5.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.4.2.1.5.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_temperature_alarms_info[0] },
	/* pdu2TemperatureValue.0.1 = INTEGER: 0 */
	{ "ambient.temperature", 0, 0.1,
		".1.3.6.1.4.1.232.165.7.4.2.1.4.%i.1",
		NULL, SU_FLAG_OK, NULL },
	/* Low and high threshold use the respective critical levels */
	/* pdu2TemperatureThLowerCritical.0.1 = INTEGER: 50 */
	{ "ambient.temperature.low", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.232.165.7.4.2.1.7.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.temperature.low.critical", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.232.165.7.4.2.1.7.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2TemperatureThLowerWarning.0.1 = INTEGER: 100 */
	{ "ambient.temperature.low.warning", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.232.165.7.4.2.1.6.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2TemperatureThUpperCritical.0.1 = INTEGER: 650 */
	{ "ambient.temperature.high", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.232.165.7.4.2.1.9.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.temperature.high.critical", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.232.165.7.4.2.1.9.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2TemperatureThUpperWarning.0.1 = INTEGER: 200 */
	{ "ambient.temperature.high.warning", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.232.165.7.4.2.1.8.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2HumidityThStatus.0.1 = INTEGER: good(1) */
	{ "ambient.humidity.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.4.3.1.5.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.4.3.1.5.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_threshold_humidity_alarms_info[0] },
	/* pdu2HumidityValue.0.1 = INTEGER: 0 */
	{ "ambient.humidity", 0, 0.1,
		".1.3.6.1.4.1.232.165.7.4.3.1.4.%i.1",
		NULL, SU_FLAG_OK, NULL },
	/* Low and high threshold use the respective critical levels */
	/* pdu2HumidityThLowerCritical.0.1 = INTEGER: 100 */
	{ "ambient.humidity.low", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.232.165.7.4.3.1.7.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.humidity.low.critical", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.232.165.7.4.3.1.7.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2HumidityThLowerWarning.0.1 = INTEGER: 200 */
	{ "ambient.humidity.low.warning", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.232.165.7.4.3.1.6.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2HumidityThUpperWarning.0.1 = INTEGER: 250 */
	{ "ambient.humidity.high.warning", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.232.165.7.4.3.1.8.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* pdu2HumidityThUpperCritical.0.1 = INTEGER: 900 */
	{ "ambient.humidity.high", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.232.165.7.4.3.1.9.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	{ "ambient.humidity.high.critical", ST_FLAG_RW, 0.1,
		".1.3.6.1.4.1.232.165.7.4.3.1.9.%i.1",
		NULL, SU_FLAG_NEGINVALID | SU_FLAG_OK, NULL },
	/* Dry contacts on TH module */
	/* pdu2ContactState.0.1 = INTEGER: contactBad(3) */
	{ "ambient.contacts.1.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.4.4.1.4.%i.1",
		NULL, SU_FLAG_OK, &hpe_pdu_ambient_drycontacts_info[0] },
	/* pdu2ContactState.0.2 = INTEGER: contactBad(3) */
	{ "ambient.contacts.2.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.4.4.1.4.%i.2",
		NULL, SU_FLAG_OK, &hpe_pdu_ambient_drycontacts_info[0] },

	/* Outlet collection */
	{ "outlet.id", 0, 1, NULL,
		"0", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, NULL, "All outlets",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	/* pdu2OutletCount.0 = INTEGER: 24 */
	{ "outlet.count", 0, 1,
		".1.3.6.1.4.1.232.165.7.1.2.1.12.%i",
		"0", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* outlet template definition
	 * Indexes start from 1, ie outlet.1 => <OID>.1 */
	/* Note: the first definition is used to determine the base index (ie 0 or 1) */
	/* pdu2OutletName.0.%i = STRING: "Outlet L1-%i" */
	{ "outlet.%i.desc", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.5.1.1.2.%i.%i",
		NULL, SU_FLAG_STATIC | SU_FLAG_OK | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* pdu2OutletControlStatus.0.%i = INTEGER: on(2) */
	{ "outlet.%i.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.5.2.1.1.%i.%i",
		NULL, SU_FLAG_OK | SU_OUTLET | SU_TYPE_DAISY_1, &hpe_pdu_outlet_status_info[0] },
	/* Numeric identifier of the outlet, tied to the whole unit */
	{ "outlet.%i.id", 0, 1, NULL, "%i",
		SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK | SU_OUTLET | SU_TYPE_DAISY_1, NULL },


#if 0
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
#endif

	/* pdu2OutletCurrent.0.%i = INTEGER: 0 */
	{ "outlet.%i.current", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.5.1.1.5.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* pdu2OutletCurrentThStatus.0.%i = INTEGER: good(1) */
	{ "outlet.%i.current.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.5.1.1.6.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, &hpe_pdu_threshold_status_info[0] },
	{ "outlet.%i.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.5.1.1.6.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, &hpe_pdu_threshold_current_alarms_info[0] },
	/* pdu2OutletCurrentThLowerWarning.0.%i = INTEGER: 0 */
	{ "outlet.%i.current.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.5.1.1.7.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* pdu2OutletCurrentThLowerCritical.0.%i = INTEGER: -1 */
	{ "outlet.%i.current.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.5.1.1.8.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* pdu2OutletCurrentThUpperWarning.0.1 = INTEGER: 8000 */
	{ "outlet.%i.current.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.5.1.1.9.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* pdu2OutletCurrentThUpperCritical.0.1 = INTEGER: 10000 */
	{ "outlet.%i.current.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.5.1.1.10.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* pdu2OutletWatts.0.1 = INTEGER: 0 */
	{ "outlet.%i.realpower", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.5.1.1.14.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* pdu2OutletVA.0.%i = INTEGER: 0 */
	{ "outlet.%i.power", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.5.1.1.13.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* pdu2OutletControlSwitchable.0.%i = INTEGER: switchable(1) */
	{ "outlet.%i.switchable", ST_FLAG_RW, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.5.2.1.8.%i.%i",
		"no", SU_FLAG_STATIC | SU_OUTLET | SU_FLAG_OK | SU_TYPE_DAISY_1,
		&hpe_pdu_outlet_switchability_info[0] },
	/* pdu2OutletType.0.%i = INTEGER: iecC13(1) */
	{ "outlet.%i.type", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.5.1.1.3.%i.%i",
		"unknown", SU_FLAG_STATIC | SU_OUTLET | SU_TYPE_DAISY_1,
		&hpe_pdu_outlet_type_info[0] },
	/* pdu2OutletPowerFactor.0.%i = INTEGER: 1000 */
	{ "outlet.%i.powerfactor", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.5.1.1.17.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* TODO: handle statistics */
	/* pdu2OutletWh.0.1 = INTEGER: 1167
	 * Note: setting this to zero resets the counter and timestamp => instcmd ???counter???.reset
	{ "unmapped.pdu2OutletWh", 0, 1, ".1.3.6.1.4.1.232.165.7.5.1.1.15.%i.%i", NULL, SU_FLAG_OK, NULL }, */
	/* pdu2OutletWhTimer.0.1 = STRING: "25/03/2016,09:03:26"
	{ "unmapped.pdu2OutletWhTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.7.5.1.1.16.%i.%i", NULL, SU_FLAG_OK, NULL }, */

	/* Outlet groups collection */
	/* pdu2GroupCount.0 = INTEGER: 3 */
	{ "outlet.group.count", 0, 1,
		".1.3.6.1.4.1.232.165.7.1.2.1.11.%i",
		"0", SU_FLAG_STATIC | SU_TYPE_DAISY_1, NULL },
	/* outlet groups template definition
	 * Indexes start from 1, ie outlet.group.1 => <OID>.1 */
	/* Note: the first definition is used to determine the base index (ie 0 or 1) */
	/* pdu2GroupIndex.0.%i = INTEGER: %i */
	{ "outlet.group.%i.id", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.3.1.1.1.%i.%i",
		NULL, SU_FLAG_STATIC | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupName.0.%i = STRING: "Section L1" */
	{ "outlet.group.%i.name", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.3.1.1.2.%i.%i",
		NULL, SU_FLAG_STATIC | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupType.0.%i = INTEGER: breaker2pole(3) */
	{ "outlet.group.%i.type", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.3.1.1.3.%i.%i",
		NULL, SU_FLAG_STATIC | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, &hpe_pdu_outlet_group_type_info[0] },
	/* pdu2GroupVoltageMeasType.0.1 = INTEGER: phase1to2(5) */
	{ "outlet.group.%i.phase", 0, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.3.1.1.4.%i.%i",
		NULL, SU_FLAG_STATIC | SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&hpe_pdu_outlet_group_phase_info[0] },
	/* pdu2groupBreakerStatus.0.%i = INTEGER: breakerOn(2) */
	{ "outlet.group.%i.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.3.1.1.27.%i.%i",
		NULL, SU_FLAG_OK | SU_FLAG_NAINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&hpe_pdu_outletgroups_status_info[0] },
	/* pdu2GroupOutletCount.0.%i = INTEGER: 8 */
	{ "outlet.group.%i.count", 0, 1,
		".1.3.6.1.4.1.232.165.7.3.1.1.26.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupVoltage.0.%i = INTEGER: 216760 */
	{ "outlet.group.%i.voltage", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.3.1.1.5.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupVoltageThStatus.0.%i = INTEGER: good(1) */
	{ "outlet.group.%i.voltage.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.3.1.1.6.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&hpe_pdu_threshold_status_info[0] },
	{ "outlet.group.%i.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.3.1.1.6.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&hpe_pdu_threshold_voltage_alarms_info[0] },
	/* pdu2GroupVoltageThLowerWarning.0.%i = INTEGER: 190000 */
	{ "outlet.group.%i.voltage.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.3.1.1.7.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupVoltageThLowerCritical.0.%i = INTEGER: 180000 */
	{ "outlet.group.%i.voltage.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.3.1.1.8.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupVoltageThUpperWarning.0.%i = INTEGER: 255000 */
	{ "outlet.group.%i.voltage.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.3.1.1.9.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupVoltageThUpperCritical.0.%i = INTEGER: 265000 */
	{ "outlet.group.%i.voltage.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.3.1.1.10.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupCurrent.0.%i = INTEGER: 0 */
	{ "outlet.group.%i.current", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.3.1.1.12.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2groupCurrentRating.0.%i = INTEGER: 16000 */
	{ "outlet.group.%i.current.nominal", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.3.1.1.11.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupCurrentThStatus.0.%i = INTEGER: good(1) */
	{ "outlet.group.%i.current.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.3.1.1.13.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&hpe_pdu_threshold_status_info[0] },
	{ "outlet.group.%i.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.232.165.7.3.1.1.13.%i.%i",
		NULL, SU_OUTLET_GROUP | SU_TYPE_DAISY_1,
		&hpe_pdu_threshold_current_alarms_info[0] },
	/* pdu2GroupCurrentThLowerWarning.0.%i = INTEGER: 0 */
	{ "outlet.group.%i.current.low.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.3.1.1.14.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupCurrentThLowerCritical.0.%i = INTEGER: -1 */
	{ "outlet.group.%i.current.low.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.3.1.1.15.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupCurrentThUpperWarning.0.%i = INTEGER: 12800 */
	{ "outlet.group.%i.current.high.warning", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.3.1.1.16.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupCurrentThUpperCritical.0.%i = INTEGER: 16000 */
	{ "outlet.group.%i.current.high.critical", ST_FLAG_RW, 0.001,
		".1.3.6.1.4.1.232.165.7.3.1.1.17.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupCurrentPercentLoad.0.%i = INTEGER: 0 */
	{ "outlet.group.%i.load", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.3.1.1.19.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupPowerWatts.0.%i = INTEGER: 0 */
	{ "outlet.group.%i.realpower", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.3.1.1.21.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupPowerVA.0.%i = INTEGER: 0 */
	{ "outlet.group.%i.power", 0, 1.0,
		".1.3.6.1.4.1.232.165.7.3.1.1.20.%i.%i",
		NULL, SU_FLAG_NEGINVALID | SU_OUTLET_GROUP | SU_TYPE_DAISY_1, NULL },
	/* pdu2GroupPowerFactor.0.%i = INTEGER: 1000 */
	{ "outlet.group.%i.powerfactor", 0, 0.001,
		".1.3.6.1.4.1.232.165.7.3.1.1.24.%i.%i",
		NULL, SU_OUTLET | SU_TYPE_DAISY_1, NULL },

	/* TODO: handle statistics */
	/* pdu2GroupPowerWattHour.0.%i = INTEGER: 1373
	 * Note: setting this to zero resets the counter and timestamp => instcmd .reset
	{ "unmapped.pdu2GroupPowerWattHour", 0, 1, ".1.3.6.1.4.1.232.165.7.3.1.1.22.%i.%i", NULL, SU_FLAG_OK, NULL }, */
	/* pdu2GroupPowerWattHourTimer.0.%i = STRING: "25/03/2016,09:01:16"
	{ "unmapped.pdu2GroupPowerWattHourTimer", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.232.165.7.3.1.1.23.%i.%i", NULL, SU_FLAG_OK, NULL }, */

	/* instant commands. */
	/* TODO: handle delays (outlet.%i.{on,off}.delay) */
	/* pdu2OutletControlOffCmd.0.%i = INTEGER: -1 */
	{ "outlet.%i.load.off", 0, 1,
		".1.3.6.1.4.1.232.165.7.5.2.1.2.%i.%i",
		"0", SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* pdu2OutletControlOnCmd.0.%i = INTEGER: -1 */
	{ "outlet.%i.load.on", 0, 1,
		".1.3.6.1.4.1.232.165.7.5.2.1.3.%i.%i",
		"0", SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* pdu2OutletControlRebootCmd.0.%i = INTEGER: -1 */
	{ "outlet.%i.load.cycle", 0, 1,
		".1.3.6.1.4.1.232.165.7.5.2.1.4.%i.%i",
		"0", SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* Delayed version, parameter is mandatory (so dfl is NULL)! */
	/* pdu2OutletControlOffCmd.0.%i = INTEGER: -1 */
	{ "outlet.%i.load.off.delay", 0, 1,
		".1.3.6.1.4.1.232.165.7.5.2.1.2.%i.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* pdu2OutletControlOnCmd.0.%i = INTEGER: -1 */
	{ "outlet.%i.load.on.delay", 0, 1,
		".1.3.6.1.4.1.232.165.7.5.2.1.3.%i.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL },
	/* pdu2OutletControlRebootCmd.0.%i = INTEGER: -1 */
	{ "outlet.%i.load.cycle.delay", 0, 1,
		".1.3.6.1.4.1.232.165.7.5.2.1.4.%i.%i",
		NULL, SU_TYPE_CMD | SU_OUTLET | SU_TYPE_DAISY_1, NULL },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};


mib2nut_info_t	hpe_pdu = { "hpe_epdu", HPE_EPDU_MIB_VERSION, NULL, HPE_EPDU_OID_MODEL_NAME, hpe_pdu_mib, HPE_EPDU_MIB_SYSOID, NULL };
