/* ecoflow-cdc-protocol.h - read-only EcoFlow CDC telemetry protocol
 *
 * Copyright (C) 2026 Network UPS Tools contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ECOFLOW_CDC_PROTOCOL_H
#define ECOFLOW_CDC_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define ECOFLOW_CDC_REQUEST_SIZE 20
#define ECOFLOW_CDC_MAX_FRAME_SIZE 4096

typedef struct {
	int has_design_capacity_mah;
	uint32_t design_capacity_mah;
	int has_system_temperature;
	double system_temperature;
	int has_battery_temperature;
	double battery_temperature;
	int has_output_power;
	double output_power;
	int has_input_power;
	double input_power;
	int has_ac_input_power;
	double ac_input_power;
	int has_ac_input_frequency;
	double ac_input_frequency;
	int has_ac_input_voltage;
	double ac_input_voltage;
	int has_solar_input_power;
	double solar_input_power;
	int has_rated_output_power;
	uint32_t rated_output_power;
	int has_ac_output_power;
	double ac_output_power;
	int has_ac_output_frequency;
	double ac_output_frequency;
	int has_dc_output_power;
	double dc_output_power;
	int has_usb_a_output_power;
	double usb_a_output_power;
	int has_usb_c_output_power;
	double usb_c_output_power;
	int has_extra_battery_input_power;
	double extra_battery_input_power;
	int has_extra_battery_output_power;
	double extra_battery_output_power;
	int has_charging_runtime;
	uint32_t charging_runtime;
	int has_ems_version;
	uint8_t ems_version[4];
} ecoflow_cdc_metrics_t;

uint16_t ecoflow_cdc_crc16(const uint8_t *data, size_t length);
size_t ecoflow_cdc_build_request(uint32_t sequence, uint8_t *buffer, size_t size);
int ecoflow_cdc_parse_frame(const uint8_t *frame, size_t length,
	uint32_t expected_sequence, ecoflow_cdc_metrics_t *metrics);

#endif /* ECOFLOW_CDC_PROTOCOL_H */
