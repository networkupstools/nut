/* nutdrv_vida.c - read-only native NUT driver for VIDA Armour ARM3000.
 *
 * The generic 0001:0000 USB ID is insufficient identification. A mandatory
 * physical bus/port selector and exact HID descriptor admit the device before
 * telemetry is read. Keep one claimed interface per process; no string-ID
 * probing, configuration change, USB reset, test or power command is used.
 * NUT owns scheduling, privilege separation, stale state and publication.
 * Copyright (C) 2026 NUT contributors. GPL version 2 or later.
 */
#include "config.h"
#include "main.h"
#include "usb-common.h"
#include "vida-protocol.h"
#include "vida-report-desc.h"

#if !WITH_LIBUSB_1_0
# error "nutdrv_vida requires libusb-1.0"
#endif

#define DRIVER_NAME "VIDA Armour USB telemetry driver"
#define DRIVER_VERSION "0.02"
#define VIDA_VENDORID 0x0001
#define VIDA_TIMEOUT 2000

upsdrv_info_t upsdrv_info = {
	DRIVER_NAME, DRIVER_VERSION, "NUT contributors (with OpenAI Codex)",
	DRV_BETA, { NULL }
};

/* Also supplies NUT's generated USB permission and compatibility metadata.
 * Shared ID: never infer support from this table without descriptor admission. */
static usb_device_id_t vida_usb_device_table[] = {
	{ USB_DEVICE(VIDA_VENDORID, 0x0000), NULL },
	{ 0, 0, NULL }
};

static libusb_context *vida_context;
static libusb_device_handle *vida_handle;
static regex_t *vida_bus_regex, *vida_port_regex;
static int vida_claimed, vida_detached, vida_lost;
static char vida_bus[4], vida_port[32];

void upsdrv_tweak_prognames(void)
{
	/* The executable has one canonical name and no historical aliases. */
}

/* Match complete port chains, not the temporary USB device address. */
static int vida_device_path(libusb_device *device, char *bus, size_t bus_size,
	char *port, size_t port_size)
{
	uint8_t numbers[8];
	int count, i;
	size_t used = 0;
	snprintf(bus, bus_size, "%03u", (unsigned int)libusb_get_bus_number(device));
	count = libusb_get_port_numbers(device, numbers, sizeof(numbers));
	if (count <= 0) {
		return 0;
	}
	port[0] = '\0';
	for (i = 0; i < count; i++) {
		int length = snprintf(port + used, port_size - used, "%s%03u",
			i ? "." : "", (unsigned int)numbers[i]);
		if (length < 0 || (size_t)length >= port_size - used) {
			return 0;
		}
		used += (size_t)length;
	}
	return 1;
}

/* Restore kernel ownership only when this process detached it. This also
 * releases a partially admitted handle after a claim/descriptor failure. */
static void vida_close(void)
{
	if (!vida_handle) {
		return;
	}
	if (vida_claimed) {
		libusb_release_interface(vida_handle, 0);
	}
	if (vida_detached) {
		libusb_attach_kernel_driver(vida_handle, 0);
	}
	libusb_close(vida_handle);
	vida_handle = NULL;
	vida_claimed = vida_detached = 0;
}

/* Enumerate without string descriptors: this firmware has a broken language
 * descriptor and overloads some other string indices with operational commands.
 * Never select a first match when the configured selectors are ambiguous. */
static int vida_open(void)
{
	libusb_device **devices = NULL, *selected = NULL;
	struct libusb_device_descriptor descriptor;
	ssize_t count, i;
	int matches = 0, result;
	/* One extra byte distinguishes a complete descriptor from a matching
	 * prefix of a longer layout, without probing any string descriptors. */
	unsigned char report[sizeof(vida_report_descriptor) + 1];
	char bus[4], port[32];
	count = libusb_get_device_list(vida_context, &devices);
	if (count < 0) {
		return 0;
	}
	for (i = 0; i < count; i++) {
		if (libusb_get_device_descriptor(devices[i], &descriptor) < 0
		|| descriptor.idVendor != vida_usb_device_table[0].vendorID
		|| descriptor.idProduct != vida_usb_device_table[0].productID
		|| !vida_device_path(devices[i], bus, sizeof(bus), port, sizeof(port))
		|| match_regex(vida_bus_regex, bus) != 1
		|| match_regex(vida_port_regex, port) != 1) {
			continue;
		}
		selected = devices[i];
		matches++;
	}
	if (matches != 1) {
		libusb_free_device_list(devices, 1);
		if (matches > 1) {
			fatalx(EXIT_FAILURE, "Ambiguous VIDA bus/busport selectors: %d devices", matches);
		}
		return 0;
	}
	vida_device_path(selected, vida_bus, sizeof(vida_bus), vida_port, sizeof(vida_port));
	result = libusb_open(selected, &vida_handle);
	libusb_free_device_list(devices, 1);
	if (result < 0) {
		vida_handle = NULL;
		upsdebugx(1, "VIDA open: %s", libusb_error_name(result));
		return 0;
	}
	result = libusb_kernel_driver_active(vida_handle, 0);
	if (result == 1) {
		if (libusb_detach_kernel_driver(vida_handle, 0) < 0) {
			vida_close();
			return 0;
		}
		vida_detached = 1;
	} else if (result < 0 && result != LIBUSB_ERROR_NOT_SUPPORTED) {
		vida_close();
		return 0;
	}
	if (libusb_claim_interface(vida_handle, 0) < 0) {
		vida_close();
		return 0;
	}
	vida_claimed = 1;
	result = libusb_control_transfer(vida_handle, 0x81, 6, 0x2200, 0,
		report, sizeof(report), VIDA_TIMEOUT);
	if (result != (int)sizeof(vida_report_descriptor)
	|| memcmp(report, vida_report_descriptor, sizeof(vida_report_descriptor))) {
		upsdebugx(1, "VIDA HID report descriptor does not match admitted ARM3000 layout");
		vida_close();
		return 0;
	}
	dstate_setinfo("device.bus", "usb");
	dstate_setinfo("driver.parameter.bus.actual", "%s", vida_bus);
	dstate_setinfo("driver.parameter.busport.actual", "%s", vida_port);
	return 1;
}

/* The typed query translator is the sole authority for string indices. */
static int vida_read_record(vida_query_t query, vida_sample_t *sample)
{
	unsigned char raw[VIDA_TRANSFER_LENGTH];
	char text[VIDA_TEXT_LENGTH];
	uint16_t value;
	int result;
	if (!vida_query_value(query, &value)) {
		return 0;
	}
	result = libusb_control_transfer(vida_handle, 0x80, 6, value, 0x0409,
		raw, sizeof(raw), VIDA_TIMEOUT);
	if (result < 0 || result > (int)sizeof(raw)
	|| !vida_decode_string(raw, (size_t)result, text, sizeof(text))
	|| !vida_parse_record(query, text, sample)) {
		upsdebugx(2, "VIDA telemetry query %d failed (%d)", (int)query, result);
		return 0;
	}
	return 1;
}

static void vida_publish(const vida_sample_t *sample)
{
	dstate_setinfo("input.voltage", "%.1f", sample->input_voltage);
	dstate_setinfo("input.voltage.fault", "%.1f", sample->fault_voltage);
	dstate_setinfo("output.voltage", "%.1f", sample->output_voltage);
	dstate_setinfo("ups.load", "%.0f", sample->load);
	dstate_setinfo("input.frequency", "%.1f", sample->frequency);
	dstate_setinfo("battery.voltage", "%.1f", sample->battery_voltage);
	dstate_setinfo("ups.temperature", "%.1f", sample->temperature);
	dstate_setinfo("battery.charge", "%.0f", sample->charge);
	dstate_setinfo("input.voltage.nominal", "%.1f", sample->nominal_voltage);
	dstate_setinfo("input.current.nominal", "%.1f", sample->nominal_current);
	dstate_setinfo("battery.voltage.nominal", "%.1f", sample->nominal_battery_voltage);
	dstate_setinfo("input.frequency.nominal", "%.1f", sample->nominal_frequency);
	dstate_setinfo("ups.firmware", "%s", sample->firmware);
	dstate_setinfo("ups.type", "%s", sample->flags[4] == '1' ? "offline / line interactive" : "online");
	dstate_setinfo("ups.beeper.status", "%s", sample->flags[7] == '1' ? "enabled" : "disabled");
	status_init();
	alarm_init();
	status_set(sample->flags[0] == '1' ? "OB" : "OL");
	if (sample->flags[1] == '1') {
		status_set("LB");
	}
	if (sample->flags[2] == '1') {
		/* Q1 does not distinguish bypass, boost and buck in this bit. */
		alarm_set("Bypass or automatic voltage regulation active");
	}
	if (sample->flags[3] == '1') {
		alarm_set("UPS fault");
	}
	if (sample->flags[5] == '1') {
		status_set("CAL");
	}
	if (sample->flags[6] == '1') {
		alarm_set("UPS shutdown active");
	}
	alarm_commit();
	status_commit();
	dstate_dataok();
}

void upsdrv_updateinfo(void)
{
	vida_sample_t sample;
	int query;
	memset(&sample, 0, sizeof(sample));
	if (!vida_handle && !vida_open()) {
		goto stale;
	}
	for (query = VIDA_STATUS; query < VIDA_QUERY_COUNT; query++) {
		if (!vida_read_record((vida_query_t)query, &sample)) {
			vida_close();
			goto stale;
		}
	}
	vida_publish(&sample);
	if (vida_lost) {
		upslogx(LOG_NOTICE, "VIDA communications established");
	}
	vida_lost = 0;
	return;
stale:
	dstate_datastale();
	if (!vida_lost) {
		upslogx(LOG_WARNING, "VIDA data unavailable; checking configured USB port on each poll");
	}
	vida_lost = 1;
}

void upsdrv_initups(void)
{
	if (!getval("bus") || !getval("busport")) {
		fatalx(EXIT_FAILURE, "VIDA requires explicit bus and busport selectors; USB ID 0001:0000 is shared");
	}
	if (compile_regex(&vida_bus_regex, getval("bus"), REG_EXTENDED)
	|| compile_regex(&vida_port_regex, getval("busport"), REG_EXTENDED)) {
		fatalx(EXIT_FAILURE, "Invalid VIDA bus or busport regular expression");
	}
	if (libusb_init(&vida_context) < 0) {
		fatalx(EXIT_FAILURE, "Cannot initialize VIDA libusb context");
	}
}

void upsdrv_initinfo(void)
{
	dstate_setinfo("ups.mfr", "VIDA");
	dstate_setinfo("ups.model", "Armour ARM3000");
	dstate_setinfo("ups.vendorid", "0001");
	dstate_setinfo("ups.productid", "0000");
	upsdrv_updateinfo();
}

void upsdrv_shutdown(void)
{
	upslogx(LOG_ERR, "VIDA is monitoring-only; UPS power control is not implemented");
	if (handling_upsdrv_shutdown > 0) {
		set_exit_flag(EF_EXIT_FAILURE);
	}
}

void upsdrv_help(void)
{
	printf("Read-only ARM3000 monitoring; requires explicit bus and busport.\n");
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "bus", "Regular expression matching USB bus (three decimal digits)");
	addvar(VAR_VALUE, "busport", "Regular expression matching complete physical port chain, e.g. 010 or 002.003");
}

void upsdrv_cleanup(void)
{
	vida_close();
	if (vida_context) {
		libusb_exit(vida_context);
		vida_context = NULL;
	}
	if (vida_bus_regex) {
		regfree(vida_bus_regex);
		free(vida_bus_regex);
		vida_bus_regex = NULL;
	}
	if (vida_port_regex) {
		regfree(vida_port_regex);
		free(vida_port_regex);
		vida_port_regex = NULL;
	}
}
