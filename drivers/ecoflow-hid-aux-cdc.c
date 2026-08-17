/* ecoflow-hid-aux-cdc.c - optional EcoFlow CDC companion for usbhid-ups
 *
 * Copyright (C) 2026 Network UPS Tools contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "ecoflow-hid-aux-cdc.h"
#include "ecoflow-cdc-protocol.h"
#include "main.h"

#define ECOFLOW_CDC_PORT "ecoflow_cdc_port"
#define ECOFLOW_CDC_RECONNECT_INTERVAL 30
#define ECOFLOW_CDC_TIMEOUT 3
#define ECOFLOW_CDC_VERSION "EcoFlow CDC 0.01"

#if (defined WITH_SERIAL) && WITH_SERIAL

#include "serial.h"

#include <math.h>
#include <time.h>

static TYPE_FD_SER cdc_fd = ERROR_FD;
static const char *cdc_port = NULL;
static uint32_t cdc_sequence = 0;
static time_t cdc_last_open_attempt = 0;
static unsigned int cdc_failures = 0;

static void ecoflow_cdc_set_state(const char *state)
{
	dstate_setinfo("driver.state.ecoflow.cdc", "%s", state);
}

static void ecoflow_cdc_close(void)
{
	if (!INVALID_FD(cdc_fd)) {
		ser_close(cdc_fd, cdc_port);
		cdc_fd = ERROR_FD;
	}
}

static int ecoflow_cdc_open(void)
{
	time_t now = time(NULL);

	if (!INVALID_FD(cdc_fd))
		return 1;
	if (cdc_last_open_attempt != 0 &&
	    difftime(now, cdc_last_open_attempt) < ECOFLOW_CDC_RECONNECT_INTERVAL)
		return 0;
	cdc_last_open_attempt = now;

	cdc_fd = ser_open_nf(cdc_port);
	if (INVALID_FD(cdc_fd)) {
		ecoflow_cdc_set_state("reconnect.trying");
		return 0;
	}
	if (ser_set_speed_nf(cdc_fd, cdc_port, B115200) != 0) {
		upslogx(LOG_WARNING, "EcoFlow CDC: failed to set %s to 115200 baud", cdc_port);
		ecoflow_cdc_close();
		ecoflow_cdc_set_state("reconnect.trying");
		return 0;
	}

	ser_flush_io(cdc_fd);
	cdc_failures = 0;
	ecoflow_cdc_set_state("quiet");
	upslogx(LOG_INFO, "EcoFlow CDC telemetry enabled on %s", cdc_port);
	return 1;
}

static void ecoflow_cdc_publish(const ecoflow_cdc_metrics_t *metrics)
{
	if (metrics->has_output_power)
		dstate_setinfo("ups.realpower", "%.1f", metrics->output_power);
	if (metrics->has_input_power)
		dstate_setinfo("input.realpower", "%.1f", metrics->input_power);
	if (metrics->has_rated_output_power) {
		dstate_setinfo("ups.realpower.nominal", "%u", metrics->rated_output_power);
		if (metrics->has_output_power && metrics->rated_output_power > 0)
			dstate_setinfo("ups.load", "%.1f",
				100.0 * metrics->output_power / metrics->rated_output_power);
	}
	if (metrics->has_ac_input_frequency)
		dstate_setinfo("input.frequency", "%.1f", metrics->ac_input_frequency);
	if (metrics->has_ac_input_voltage)
		dstate_setinfo("input.voltage", "%.1f", metrics->ac_input_voltage);
	if (metrics->has_ac_output_frequency)
		dstate_setinfo("output.frequency", "%.1f", metrics->ac_output_frequency);
	if (metrics->has_system_temperature)
		dstate_setinfo("ups.temperature", "%.1f", metrics->system_temperature);
	if (metrics->has_battery_temperature)
		dstate_setinfo("battery.temperature", "%.1f", metrics->battery_temperature);
	if (metrics->has_design_capacity_mah)
		dstate_setinfo("battery.capacity.nominal", "%.3f",
			metrics->design_capacity_mah / 1000.0);

	dstate_setinfo("outlet.count", "4");
	dstate_setinfo("outlet.1.desc", "AC output");
	dstate_setinfo("outlet.2.desc", "DC output");
	dstate_setinfo("outlet.3.desc", "USB-A output");
	dstate_setinfo("outlet.4.desc", "USB-C output");
	if (metrics->has_ac_output_power)
		dstate_setinfo("outlet.1.realpower", "%.1f", metrics->ac_output_power);
	if (metrics->has_dc_output_power)
		dstate_setinfo("outlet.2.realpower", "%.1f", metrics->dc_output_power);
	if (metrics->has_usb_a_output_power)
		dstate_setinfo("outlet.3.realpower", "%.1f", metrics->usb_a_output_power);
	if (metrics->has_usb_c_output_power)
		dstate_setinfo("outlet.4.realpower", "%.1f", metrics->usb_c_output_power);

	if (metrics->has_ac_input_power)
		dstate_setinfo("experimental.ecoflow.input.ac.realpower", "%.1f",
			metrics->ac_input_power);
	if (metrics->has_solar_input_power)
		dstate_setinfo("experimental.ecoflow.input.solar.realpower", "%.1f",
			metrics->solar_input_power);
	if (metrics->has_extra_battery_input_power)
		dstate_setinfo("experimental.ecoflow.input.extra_battery.realpower", "%.1f",
			metrics->extra_battery_input_power);
	if (metrics->has_extra_battery_output_power)
		dstate_setinfo("experimental.ecoflow.output.extra_battery.realpower", "%.1f",
			metrics->extra_battery_output_power);
	if (metrics->has_charging_runtime)
		dstate_setinfo("experimental.ecoflow.battery.charge.runtime", "%u",
			metrics->charging_runtime);
	else
		dstate_delinfo("experimental.ecoflow.battery.charge.runtime");
	if (metrics->has_ems_version)
		dstate_setinfo("experimental.ecoflow.ems.firmware", "%u.%u.%u.%u",
			metrics->ems_version[3], metrics->ems_version[2],
			metrics->ems_version[1], metrics->ems_version[0]);
}

static int ecoflow_cdc_poll(void)
{
	uint8_t request[ECOFLOW_CDC_REQUEST_SIZE];
	uint8_t frame[ECOFLOW_CDC_MAX_FRAME_SIZE];
	uint16_t variable_length;
	size_t frame_length;
	ssize_t ret;
	ecoflow_cdc_metrics_t metrics;
	int parsed;

	if (!ecoflow_cdc_open())
		return 0;

	ecoflow_cdc_build_request(cdc_sequence, request, sizeof(request));
	ser_flush_io(cdc_fd);
	ret = ser_send_buf(cdc_fd, request, sizeof(request));
	if (ret != (ssize_t)sizeof(request))
		goto failed;

	ret = ser_get_buf_len(cdc_fd, frame, 4, ECOFLOW_CDC_TIMEOUT, 0);
	if (ret != 4)
		goto failed;
	variable_length = (uint16_t)((uint16_t)frame[2] | ((uint16_t)frame[3] << 8));
	frame_length = 20U + variable_length;
	if (frame_length < 24 || frame_length > sizeof(frame))
		goto failed;
	ret = ser_get_buf_len(cdc_fd, frame + 4, frame_length - 4,
		ECOFLOW_CDC_TIMEOUT, 0);
	if (ret != (ssize_t)(frame_length - 4))
		goto failed;

	parsed = ecoflow_cdc_parse_frame(frame, frame_length, cdc_sequence, &metrics);
	if (parsed != 0) {
		upsdebugx(1, "EcoFlow CDC: rejected response (%d)", parsed);
		goto failed;
	}

	ecoflow_cdc_publish(&metrics);
	cdc_sequence++;
	cdc_failures = 0;
	ecoflow_cdc_set_state("quiet");
	return 1;

failed:
	cdc_failures++;
	if (cdc_failures == 1 || cdc_failures % 100 == 0)
		upslogx(LOG_WARNING, "EcoFlow CDC telemetry poll failed (failure %u); HID monitoring remains active",
			cdc_failures);
	ecoflow_cdc_close();
	ecoflow_cdc_set_state("reconnect.trying");
	return 0;
}

static void ecoflow_cdc_initups(void)
{
	cdc_port = getval(ECOFLOW_CDC_PORT);
	if (cdc_port == NULL || *cdc_port == '\0')
		return;

	dstate_setinfo("driver.version.ecoflow.cdc", "%s", ECOFLOW_CDC_VERSION);
	ecoflow_cdc_open();
}

static void ecoflow_cdc_initinfo(void)
{
	if (cdc_port != NULL)
		ecoflow_cdc_poll();
}

static void ecoflow_cdc_updateinfo(void)
{
	if (cdc_port != NULL)
		ecoflow_cdc_poll();
}

static void ecoflow_cdc_cleanup(void)
{
	ecoflow_cdc_close();
}

#else /* WITH_SERIAL */

static void ecoflow_cdc_initups(void)
{
	if (getval(ECOFLOW_CDC_PORT) != NULL)
		upslogx(LOG_WARNING, "EcoFlow CDC telemetry requested, but NUT was built without serial support");
}

static void ecoflow_cdc_initinfo(void) {}
static void ecoflow_cdc_updateinfo(void) {}
static void ecoflow_cdc_cleanup(void) {}

#endif /* WITH_SERIAL */

static void ecoflow_cdc_makevartable(void)
{
	addvar(VAR_VALUE, ECOFLOW_CDC_PORT,
		"Optional EcoFlow CDC serial port used to enrich HID telemetry");
}

subdriver_aux_t ecoflow_hid_aux_cdc = {
	ecoflow_cdc_makevartable,
	ecoflow_cdc_initups,
	ecoflow_cdc_initinfo,
	ecoflow_cdc_updateinfo,
	ecoflow_cdc_cleanup,
};
