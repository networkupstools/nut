/* eaton-ats16-nm2-mib.c - subdriver to monitor Eaton ATS16 SNMP devices with NUT
 * using newer Network-M2 cards
 *
 *  Copyright (C)
 *    2011-2012 Arnaud Quette <arnaud.quette@free.fr>
 *    2016-2020 Eaton (author: Arnaud Quette <ArnaudQuette@Eaton.com>)
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  gen-snmp-subdriver script. It must be customized!
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
 */

#include "eaton-ats16-nm2-mib.h"
#if WITH_SNMP_LKP_FUN
/* FIXME: shared helper code, need to be put in common */
#include "eaton-pdu-marlin-helpers.h"
#endif

#define EATON_ATS16_NM2_MIB_VERSION  "0.20"

#define EATON_ATS16_NM2_SYSOID  ".1.3.6.1.4.1.534.10.2" /* newer Network-M2 */
#define EATON_ATS16_NM2_MODEL   ".1.3.6.1.4.1.534.10.2.1.2.0"

static info_lkp_t eaton_ats16_nm2_source_info[] = {
	{ 1, "init"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, "diagnosis"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 3, "off"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 4, "1"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 5, "2"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 6, "safe"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 7, "fault"
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

static info_lkp_t eaton_ats16_nm2_sensitivity_info[] = {
	{ 1, "normal"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, "high"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 3, "low"
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

static info_lkp_t eaton_ats16_nm2_input_frequency_status_info[] = {
	{ 1, "good"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},          /* No threshold triggered */
	{ 2, "out-of-range"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  /* Frequency out of range triggered */
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

static info_lkp_t eaton_ats16_nm2_input_voltage_status_info[] = {
	{ 1, "good"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},          /* No threshold triggered */
	{ 2, "derated-range"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}, /* Voltage derated */
	{ 3, "out-of-range"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  /* Voltage out of range triggered */
	{ 4, "unknown"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},       /* "missing" */
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

static info_lkp_t eaton_ats16_nm2_test_result_info[] = {
	{ 1, "done and passed"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 2, "done and warning"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 3, "done and error"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 4, "aborted"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 5, "in progress"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},
	{ 6, "no test initiated"
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

static info_lkp_t eaton_ats16_nm2_output_status_info[] = {
	{ 1, "OFF"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}, /* Output not powered */
	{ 2, "OL"
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	},  /* Output powered */
	{ 0, NULL
#if WITH_SNMP_LKP_FUN
		, NULL, NULL, NULL, NULL
#endif
	}
};

/* Note: all the below *emp002* info should be shared with marlin and powerware! */

#if WITH_SNMP_LKP_FUN
/* Note: eaton_sensor_temperature_unit_fun() is defined in eaton-pdu-marlin-helpers.c
 * and su_temperature_read_fun() is in snmp-ups.c
 * Future work for DMF might provide same-named routines via LUA-C gateway.
 */

#if WITH_SNMP_LKP_FUN_DUMMY
/* Temperature unit consideration */
const char *eaton_sensor_temperature_unit_fun(void *raw_snmp_value) {
	/* snmp_value here would be a (long*) */
	NUT_UNUSED_VARIABLE(raw_snmp_value);
	return "unknown";
}
/* FIXME: please DMF, though this should be in snmp-ups.c or equiv. */
const char *su_temperature_read_fun(void *raw_snmp_value) {
	/* snmp_value here would be a (long*) */
	NUT_UNUSED_VARIABLE(raw_snmp_value);
	return "dummy";
};
#endif // WITH_SNMP_LKP_FUN_DUMMY

static info_lkp_t eaton_ats16_nm2_sensor_temperature_unit_info[] = {
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

static info_lkp_t eaton_ats16_nm2_sensor_temperature_read_info[] = {
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
static info_lkp_t eaton_ats16_nm2_sensor_temperature_unit_info[] = {
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

static info_lkp_t eaton_ats16_nm2_ambient_drycontacts_polarity_info[] = {
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

static info_lkp_t eaton_ats16_nm2_ambient_drycontacts_state_info[] = {
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

static info_lkp_t eaton_ats16_nm2_emp002_ambient_presence_info[] = {
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
static info_lkp_t eaton_ats16_nm2_threshold_status_info[] = {
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
static info_lkp_t eaton_ats16_nm2_threshold_temperature_alarms_info[] = {
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
static info_lkp_t eaton_ats16_nm2_threshold_humidity_alarms_info[] = {
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

/* EATON_ATS Snmp2NUT lookup table */
static snmp_info_t eaton_ats16_nm2_mib[] = {

	/* Device collection */
	{ "device.type", ST_FLAG_STRING, SU_INFOSIZE, NULL, "ats", SU_FLAG_STATIC | SU_FLAG_ABSENT | SU_FLAG_OK, NULL },
	/* ats2IdentManufacturer.0 = STRING: EATON */
	{ "device.mfr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.1.0", "Eaton", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* ats2IdentModel.0 = STRING: Eaton ATS */
	{ "device.model", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.2.0", "ATS", SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* FIXME: RFC for device.firmware! */
	/* FIXME: the 2 "firmware" entries below should be SU_FLAG_SEMI_STATIC */
	/* ats2IdentFWVersion.0 = STRING: 00.00.0009 */
	{ "ups.firmware", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.3.0", NULL, SU_FLAG_OK, NULL },
	/* FIXME: RFC for device.firmware.aux! */
	/* ats2IdentRelease.0 = STRING: 1.7.5 */
	{ "ups.firmware.aux", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.7.0", NULL, SU_FLAG_OK, NULL },
	/* ats2IdentSerialNumber.0 = STRING: GA04F23009 */
	{ "device.serial", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.5.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* ats2IdentPartNumber.0 = STRING: EATS16N */
	{ "device.part", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.6.0", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* ats2IdentAgentVersion.0 = STRING: 301F23C28 */
	/* { "unmapped.ats2IdentAgentVersion", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.1.7.0", NULL, SU_FLAG_OK, NULL, NULL }, */
	/* ats2InputDephasing.0 = INTEGER: 2 degrees */
	/* { "unmapped.ats2InputDephasing", 0, 1, ".1.3.6.1.4.1.534.10.2.2.1.1.0", NULL, SU_FLAG_OK, NULL, NULL }, */

	/* Input collection */
	/* ats2InputIndex.source1 = INTEGER: source1(1) */
	{ "input.1.id", 0, 1, ".1.3.6.1.4.1.534.10.2.2.2.1.1.1", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* ats2InputIndex.source2 = INTEGER: source2(2) */
	{ "input.2.id", 0, 1, ".1.3.6.1.4.1.534.10.2.2.2.1.1.2", NULL, SU_FLAG_STATIC | SU_FLAG_OK, NULL },
	/* ats2InputVoltage.source1 = INTEGER: 2292 0.1 V */
	{ "input.1.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.2.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* ats2InputVoltage.source2 = INTEGER: 2432 0.1 V */
	{ "input.2.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.2.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusVoltage.source1 = INTEGER: normalRange(1) */
	{ "input.1.voltage.status", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.5.1", NULL, SU_FLAG_OK, eaton_ats16_nm2_input_voltage_status_info },
	/* ats2InputStatusVoltage.source2 = INTEGER: normalRange(1) */
	{ "input.2.voltage.status", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.5.2", NULL, SU_FLAG_OK, eaton_ats16_nm2_input_voltage_status_info },
	/* ats2InputFrequency.source1 = INTEGER: 500 0.1 Hz */
	{ "input.1.frequency", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.2.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* ats2InputFrequency.source2 = INTEGER: 500 0.1 Hz */
	{ "input.2.frequency", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.2.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusFrequency.source1 = INTEGER: good(1) */
	{ "input.1.frequency.status", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.2.1", NULL, SU_FLAG_OK, eaton_ats16_nm2_input_frequency_status_info },
	/* ats2InputStatusFrequency.source2 = INTEGER: good(1) */
	{ "input.2.frequency.status", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.2.2", NULL, SU_FLAG_OK, eaton_ats16_nm2_input_frequency_status_info },
	/* ats2ConfigSensitivity.0 = INTEGER: normal(1) */
	{ "input.sensitivity", ST_FLAG_RW, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.4.6.0", NULL, SU_FLAG_OK, &eaton_ats16_nm2_sensitivity_info[0] },
	/* ats2OperationMode.0 = INTEGER: source1(4) */
	{ "input.source", ST_FLAG_STRING, 1, ".1.3.6.1.4.1.534.10.2.2.4.0", NULL, SU_FLAG_OK, eaton_ats16_nm2_source_info },
	/* ats2ConfigPreferred.0 = INTEGER: source1(1) */
	{ "input.source.preferred", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.4.5.0", NULL, SU_FLAG_OK, NULL },
	/* ats2InputDephasing = INTEGER: 181 */
	{ "input.phase.shift", 0, 1, ".1.3.6.1.4.1.534.10.2.2.1.1.0", NULL, SU_FLAG_OK, NULL },

	/* Output collection */
	/* ats2OutputVoltage.0 = INTEGER: 2304 0.1 V */
	{ "output.voltage", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.3.1.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigOutputVoltage.0 = INTEGER: 230 1 V */
	{ "output.voltage.nominal", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.4.4.0", NULL, SU_FLAG_OK, NULL },
	/* ats2OutputCurrent.0 = INTEGER: 5 0.1 A */
	{ "output.current", 0, 0.1, ".1.3.6.1.4.1.534.10.2.2.3.2.0", NULL, SU_FLAG_OK, NULL },

	/* UPS collection */
	/* FIXME: RFC for device.test.result! */
	/* ats2ConfigTransferTest.0 = INTEGER: noTestInitiated(6) */
	{ "ups.test.result", 0, 1, ".1.3.6.1.4.1.534.10.2.4.8.0", NULL, SU_FLAG_OK, eaton_ats16_nm2_test_result_info },
	/* FIXME: RFC for device.status! */
	/* ats2StatusOutput.0 = INTEGER: outputPowered(2) */
	{ "ups.status", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.2.0", NULL, SU_FLAG_OK, eaton_ats16_nm2_output_status_info },

	/* Ambient collection */
	/* EMP001 (legacy) mapping for EMP002
	 * Note that NM2 should only be hooked with EMP002, but if any EMP001 was to be
	 * connected, the value may be off by a factor 10 (to be proven) */
	/* ats2EnvRemoteTemp.0 = INTEGER: 0 degrees Centigrade */
	{ "ambient.temperature", 0, 1, ".1.3.6.1.4.1.534.10.2.5.1.0", NULL, SU_FLAG_OK, NULL },
	/* ats2EnvRemoteTempLowerLimit.0 = INTEGER: 5 degrees Centigrade */
	{ "ambient.temperature.low", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.5.5.0", NULL, SU_FLAG_OK, NULL },
	/* ats2EnvRemoteTempUpperLimit.0 = INTEGER: 40 degrees Centigrade */
	{ "ambient.temperature.high", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.5.6.0", NULL, SU_FLAG_OK, NULL },
	/* ats2EnvRemoteHumidity.0 = INTEGER: 0 percent */
	{ "ambient.humidity", 0, 1, ".1.3.6.1.4.1.534.10.2.5.2.0", NULL, SU_FLAG_OK, NULL },
	/* ats2EnvRemoteHumidityLowerLimit.0 = INTEGER: 5 percent */
	{ "ambient.humidity.low", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.5.7.0", NULL, SU_FLAG_OK, NULL },
	/* ats2EnvRemoteHumidityUpperLimit.0 = INTEGER: 90 percent */
	{ "ambient.humidity.high", ST_FLAG_RW, 1, ".1.3.6.1.4.1.534.10.2.5.8.0", NULL, SU_FLAG_OK, NULL },

	/* EMP002 (EATON EMP MIB) mapping, including daisychain support */
	/* Warning: indexes start at '1' not '0'! */
	/* sensorCount.0 */
	{ "ambient.count", ST_FLAG_RW, 1.0, ".1.3.6.1.4.1.534.6.8.1.1.1.0", "", 0, NULL },
	/* CommunicationStatus.n */
	{ "ambient.%i.present", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.8.1.1.4.1.1.%i",
		NULL, SU_AMBIENT_TEMPLATE, &eaton_ats16_nm2_emp002_ambient_presence_info[0] },
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
	{ "ambient.%i.temperature.unit", 0, 1.0, ".1.3.6.1.4.1.534.6.8.1.2.5.0", "", SU_AMBIENT_TEMPLATE, &eaton_ats16_nm2_sensor_temperature_unit_info[0] },
	/* temperatureValue.n.1 */
	{ "ambient.%i.temperature", 0, 0.1, ".1.3.6.1.4.1.534.6.8.1.2.3.1.3.%i.1", "", SU_AMBIENT_TEMPLATE,
#if WITH_SNMP_LKP_FUN
	&eaton_ats16_nm2_sensor_temperature_read_info[0]
#else
	NULL
#endif
	},
	{ "ambient.%i.temperature.status", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.8.1.2.3.1.1.%i.1",
		NULL, SU_AMBIENT_TEMPLATE, &eaton_ats16_nm2_threshold_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.8.1.2.3.1.1.%i.1",
		NULL, SU_AMBIENT_TEMPLATE, &eaton_ats16_nm2_threshold_temperature_alarms_info[0] },
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
		NULL, SU_AMBIENT_TEMPLATE, &eaton_ats16_nm2_threshold_status_info[0] },
	{ "ups.alarm", ST_FLAG_STRING, SU_INFOSIZE,
		".1.3.6.1.4.1.534.6.8.1.3.3.1.1.%i.1",
		NULL, SU_AMBIENT_TEMPLATE, &eaton_ats16_nm2_threshold_humidity_alarms_info[0] },
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
	{ "ambient.%i.contacts.1.config", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.8.1.4.2.1.3.%i.1", "", SU_AMBIENT_TEMPLATE, &eaton_ats16_nm2_ambient_drycontacts_polarity_info[0] },
	{ "ambient.%i.contacts.2.config", ST_FLAG_RW | ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.6.8.1.4.2.1.3.%i.2", "", SU_AMBIENT_TEMPLATE, &eaton_ats16_nm2_ambient_drycontacts_polarity_info[0] },
	/* XUPS-MIB::xupsContactState.n */
	{ "ambient.%i.contacts.1.status", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.4.3.1.3.%i.1", "", SU_AMBIENT_TEMPLATE, &eaton_ats16_nm2_ambient_drycontacts_state_info[0] },
	{ "ambient.%i.contacts.2.status", ST_FLAG_STRING, 1.0, ".1.3.6.1.4.1.534.6.8.1.4.3.1.3.%i.2", "", SU_AMBIENT_TEMPLATE, &eaton_ats16_nm2_ambient_drycontacts_state_info[0] },

#if 0 /* FIXME: Remaining data to be processed */
	/* ats2InputStatusDephasing.0 = INTEGER: normal(1) */
	{ "unmapped.ats2InputStatusDephasing", 0, 1, ".1.3.6.1.4.1.534.10.2.3.1.1.0", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusIndex.source1 = INTEGER: source1(1) */
	{ "unmapped.ats2InputStatusIndex", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusIndex.source2 = INTEGER: source2(2) */
	{ "unmapped.ats2InputStatusIndex", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.1.2", NULL, SU_FLAG_OK, NULL },

	/* ats2InputStatusGood.source1 = INTEGER: voltageAndFreqNormalRange(2) */
	{ "unmapped.ats2InputStatusGood", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusGood.source2 = INTEGER: voltageAndFreqNormalRange(2) */
	{ "unmapped.ats2InputStatusGood", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusInternalFailure.source1 = INTEGER: good(1) */
	{ "unmapped.ats2InputStatusInternalFailure", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusInternalFailure.source2 = INTEGER: good(1) */
	{ "unmapped.ats2InputStatusInternalFailure", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.4.2", NULL, SU_FLAG_OK, NULL },

	/* ats2InputStatusUsed.source1 = INTEGER: poweringLoad(2) */
	{ "unmapped.ats2InputStatusUsed", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.6.1", NULL, SU_FLAG_OK, NULL },
	/* ats2InputStatusUsed.source2 = INTEGER: notPoweringLoad(1) */
	{ "unmapped.ats2InputStatusUsed", 0, 1, ".1.3.6.1.4.1.534.10.2.3.2.1.6.2", NULL, SU_FLAG_OK, NULL },
	/* ats2StatusInternalFailure.0 = INTEGER: good(1) */
	{ "unmapped.ats2StatusInternalFailure", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.1.0", NULL, SU_FLAG_OK, NULL },

	/* ats2StatusOverload.0 = INTEGER: noOverload(1) */
	{ "unmapped.ats2StatusOverload", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.3.0", NULL, SU_FLAG_OK, NULL },
	/* ats2StatusOverTemperature.0 = INTEGER: noOverTemperature(1) */
	{ "unmapped.ats2StatusOverTemperature", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.4.0", NULL, SU_FLAG_OK, NULL },
	/* ats2StatusShortCircuit.0 = INTEGER: noShortCircuit(1) */
	{ "unmapped.ats2StatusShortCircuit", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.5.0", NULL, SU_FLAG_OK, NULL },
	/* ats2StatusCommunicationLost.0 = INTEGER: good(1) */
	{ "unmapped.ats2StatusCommunicationLost", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.6.0", NULL, SU_FLAG_OK, NULL },
	/* ats2StatusConfigurationFailure.0 = INTEGER: good(1) */
	{ "unmapped.ats2StatusConfigurationFailure", 0, 1, ".1.3.6.1.4.1.534.10.2.3.3.7.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigTimeRTC.0 = Wrong Type (should be Counter32): Gauge32: 19191036 */
	{ "unmapped.ats2ConfigTimeRTC", 0, 1, ".1.3.6.1.4.1.534.10.2.4.1.1.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigTimeTextDate.0 = STRING: 08/11/1970 */
	{ "unmapped.ats2ConfigTimeTextDate", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.4.1.2.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigTimeTextTime.0 = STRING: 02/50/36 */
	{ "unmapped.ats2ConfigTimeTextTime", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.4.1.3.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigInputVoltageRating.0 = INTEGER: 1 1 V */
	{ "unmapped.ats2ConfigInputVoltageRating", 0, 1, ".1.3.6.1.4.1.534.10.2.4.2.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigInputFrequencyRating.0 = INTEGER: 50 Hz */
	{ "unmapped.ats2ConfigInputFrequencyRating", 0, 1, ".1.3.6.1.4.1.534.10.2.4.3.0", NULL, SU_FLAG_OK, NULL },

	/* ats2ConfigTransferMode.0 = INTEGER: standard(1) */
	{ "unmapped.ats2ConfigTransferMode", 0, 1, ".1.3.6.1.4.1.534.10.2.4.7.0", NULL, SU_FLAG_OK, NULL },

	/* ats2ConfigBrownoutLow.0 = INTEGER: 202 1 V */
	{ "unmapped.ats2ConfigBrownoutLow", 0, 1, ".1.3.6.1.4.1.534.10.2.4.9.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigBrownoutLowDerated.0 = INTEGER: 189 1 V */
	{ "unmapped.ats2ConfigBrownoutLowDerated", 0, 1, ".1.3.6.1.4.1.534.10.2.4.10.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigBrownoutHigh.0 = INTEGER: 258 1 V */
	{ "unmapped.ats2ConfigBrownoutHigh", 0, 1, ".1.3.6.1.4.1.534.10.2.4.11.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ConfigHysteresisVoltage.0 = INTEGER: 5 1 V */
	{ "unmapped.ats2ConfigHysteresisVoltage", 0, 1, ".1.3.6.1.4.1.534.10.2.4.12.0", NULL, SU_FLAG_OK, NULL },

	/* Ambient collection */
	/* ats2EnvNumContacts.0 = INTEGER: 2 */
	{ "unmapped.ats2EnvNumContacts", 0, 1, ".1.3.6.1.4.1.534.10.2.5.3.0", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactIndex.1 = INTEGER: 1 */
	{ "unmapped.ats2ContactIndex", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.1.1", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactIndex.2 = INTEGER: 2 */
	{ "unmapped.ats2ContactIndex", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.1.2", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactType.1 = INTEGER: notUsed(4) */
	{ "unmapped.ats2ContactType", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.2.1", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactType.2 = INTEGER: notUsed(4) */
	{ "unmapped.ats2ContactType", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.2.2", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactState.1 = INTEGER: open(1) */
	{ "unmapped.ats2ContactState", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.3.1", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactState.2 = INTEGER: open(1) */
	{ "unmapped.ats2ContactState", 0, 1, ".1.3.6.1.4.1.534.10.2.5.4.1.3.2", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactDescr.1 = STRING: Input #1 */
	{ "unmapped.ats2ContactDescr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.5.4.1.4.1", NULL, SU_FLAG_OK, NULL },
	/* ats2ContactDescr.2 = STRING: Input #2 */
	{ "unmapped.ats2ContactDescr", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.534.10.2.5.4.1.4.2", NULL, SU_FLAG_OK, NULL },
#endif /* if 0 */
	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

/* Note: keep the legacy definition intact, to avoid breaking compatibility */

/* FIXME: The lines below are duplicated to fix an issue with the code generator (nut-snmpinfo.py -> line is discarding) */
/* Note:
 *   due to a bug in tools/nut-snmpinfo.py, prepending a 2nd mib2nut_info_t
 *   declaration with a comment line results in data extraction not being
 *   done for all entries in the file. Hence the above comment line being
 *   after its belonging declaration! */

/*mib2nut_info_t	eaton_ats16_nm2 = { "eaton_ats16_nm2", EATON_ATS16_NM2_MIB_VERSION, NULL, EATON_ATS16_NM2_MODEL, eaton_ats16_nm2_mib, EATON_ATS16_NM2_SYSOID, NULL };*/
mib2nut_info_t	eaton_ats16_nm2 = { "eaton_ats16_nm2", EATON_ATS16_NM2_MIB_VERSION, NULL, EATON_ATS16_NM2_MODEL, eaton_ats16_nm2_mib, EATON_ATS16_NM2_SYSOID, NULL };
