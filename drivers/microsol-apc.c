/* microsol-apc.c - APC Back-UPS BR series UPS driver

   Copyright (C) 2004  Silvino B. Magalhães    <sbm2yk@gmail.com>
                 2019  Roberto Panerai Velloso <rvelloso@gmail.com>
                 2021  Ygor A. S. Regados      <ygorre@tutanota.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

   2021/03/19 - Version 0.70 - Initial release, based on solis driver

*/

#include "config.h" /* must be first */

#include <ctype.h>
#include <stdio.h>
#include "main.h"
#include "serial.h"
#include "nut_float.h"
#include "timehead.h"

#include "microsol-common.h"
#include "microsol-apc.h"

#define DRIVER_NAME	"APC Back-UPS BR series UPS driver"
#define DRIVER_VERSION	"0.71"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Silvino B. Magalhães <sbm2yk@gmail.com>"
	"Roberto Panerai Velloso <rvelloso@gmail.com>"
	"Ygor A. S. Regados <ygorre@tutanota.com>",
	DRV_STABLE,
	{ NULL }
};

#define false 0
#define true 1
#define RESP_END    0xFE
#define ENDCHAR     13		/* replies end with CR */
/* solis commands */
#define CMD_UPSCONT 0xCC
#define CMD_SHUT    0xDD
#define CMD_SHUTRET 0xDE
#define CMD_EVENT   0xCE
#define CMD_DUMP    0xCD

/** Check if UPS model is available here. */
bool_t ups_model_defined(void)
{
	unsigned int model_index;

	for (model_index = 0; MODELS[model_index] != ups_model; model_index++);
	if (model_index == MODEL_COUNT) {
		return 0;
	} else {
		return 1;
	}
}

/** Set UPS model name. */
void set_ups_model(void)
{
	switch (ups_model) {
	case 183:
		model_name = "BZ2200I-BR";
		break;
	case 190:
		model_name = "BZ1500-BR";
		break;
	case 191:
		model_name = "BZ2200BI-BR";
		break;
	default:
		model_name = "Unknown UPS";
	}
}

/**
 * Parse received packet with UPS instantaneous data.
 * This function parses model-specific values, such as voltage and battery times.
 */
void scan_received_pack_model_specific(void)
{
	unsigned int relay_state;
	unsigned int model_index;
	float real_power_curve_1, real_power_curve_2, real_power_curve_3;
	float power_difference_1, power_difference_2, power_difference_3;
	bool_t recharging;

	/* Extract unprocessed data from packet */
	input_voltage = received_packet[2];
	output_voltage = received_packet[1];
	output_current = received_packet[5];
	battery_voltage = received_packet[3];

	relay_state = (received_packet[6] & 0x28) >> 3;

	/* Find array indexes for detected UPS model */
	for (model_index = 0; MODELS[model_index] != ups_model && model_index < MODEL_COUNT - 1; model_index++);
	if (MODELS[model_index] != ups_model) {
		upslogx(LOG_NOTICE, "UPS model not found, using fallback option.");
	}

	/* Start processing according to model */
	nominal_power = NOMINAL_POWER[model_index];

	input_voltage = INPUT_VOLTAGE_MULTIPLIER_A[model_index][output_220v] * input_voltage + INPUT_VOLTAGE_MULTIPLIER_B[model_index][output_220v];

	battery_voltage = BATTERY_VOLTAGE_MULTIPLIER_A[model_index] * battery_voltage + BATTERY_VOLTAGE_MULTIPLIER_B[model_index];

	output_current = OUTPUT_CURRENT_MULTIPLIER_A[model_index][line_unpowered] * output_current + OUTPUT_CURRENT_MULTIPLIER_B[model_index][line_unpowered];

	if (ups_model == 190 && line_unpowered) {
		/* Special calculation for BZ1500 on battery */
		output_voltage = battery_voltage * sqrt(output_voltage / 64.0) * OUTPUT_VOLTAGE_MULTIPLIER_A[model_index][line_unpowered][relay_state]
		    - output_current * OUTPUT_VOLTAGE_MULTIPLIER_B[model_index][line_unpowered][relay_state];
		output_voltage = 1.5091 * output_voltage + 1.5823;

		if (output_current > 4.0)
			output_voltage += output_current * 4.0;

		if (output_current > 3.0)
			output_voltage += output_current * 2.0;
		else if (output_voltage > 0.9)
			output_voltage += output_current / 3.0;
		else
			output_voltage -= 5.0;

		if (output_voltage < 100.0)
			output_voltage = 100;
	} else {
		output_voltage = OUTPUT_VOLTAGE_MULTIPLIER_A[model_index][line_unpowered][relay_state] * output_voltage + OUTPUT_VOLTAGE_MULTIPLIER_B[model_index][line_unpowered][relay_state];
	}

	if (line_unpowered) {
		input_frequency = 0;
		output_frequency = 60;
	} else {
		input_frequency = 0.37 * (257 - ((received_packet[21] + received_packet[22] * 256) >> 8));
		output_frequency = input_frequency;
	}

	apparent_power = output_current * output_voltage;

	real_power = received_packet[7] + 256 * received_packet[8];
	real_power_curve_1 = REAL_POWER_CURVE_SELECTOR_A1[model_index][relay_state] * real_power + REAL_POWER_CURVE_SELECTOR_B1[model_index][relay_state];
	real_power_curve_2 = REAL_POWER_CURVE_SELECTOR_A2[model_index][relay_state] * real_power + REAL_POWER_CURVE_SELECTOR_B2[model_index][relay_state];
	real_power_curve_3 = REAL_POWER_CURVE_SELECTOR_A3[model_index][relay_state] * real_power + REAL_POWER_CURVE_SELECTOR_B3[model_index][relay_state];

	power_difference_1 = fabs(real_power_curve_1 - apparent_power);
	power_difference_2 = fabs(real_power_curve_2 - apparent_power);
	power_difference_3 = fabs(real_power_curve_3 - apparent_power);

	if (power_difference_1 < power_difference_2 && power_difference_1 < power_difference_3) {
		real_power = REAL_POWER_MULTIPLIER_A1[model_index][relay_state] * real_power + REAL_POWER_MULTIPLIER_B1[model_index][relay_state];
	} else if (power_difference_2 < power_difference_3) {
		real_power = REAL_POWER_MULTIPLIER_A2[model_index][relay_state] * real_power + REAL_POWER_MULTIPLIER_B2[model_index][relay_state];
	} else {
		real_power = REAL_POWER_MULTIPLIER_A3[model_index][relay_state] * real_power + REAL_POWER_MULTIPLIER_B3[model_index][relay_state];
	}

	/* If real power is greater than apparent power, invert values */
	if (apparent_power < real_power) {
		apparent_power = apparent_power + real_power;
		real_power = apparent_power - real_power;
		apparent_power = apparent_power - real_power;
	}

	input_current = 1.1 * apparent_power / input_voltage;

	recharging = (0x02 & received_packet[20]) == 0x02;
	battery_charge = (100.0 * (battery_voltage - MIN_BATTERY_VOLTAGE[model_index])) / (MAX_BATTERY_VOLTAGE[model_index][recharging] - MIN_BATTERY_VOLTAGE[model_index]);
}
