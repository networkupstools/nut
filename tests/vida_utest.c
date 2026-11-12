/* vida_utest.c - replay the production VIDA driver at its native USB boundary.
 * Covers complete publication, stale data, identity, two physical ports,
 * reconnect with a changed USB address, and partial ownership cleanup.
 * No hardware or power-control operation is used by this test.
 * Copyright (C) 2026 NUT contributors. GPL version 2 or later.
 */
#include "config.h"
#include <math.h>
#include <setjmp.h>

#define libusb_init sim_init
#define libusb_exit sim_exit
#define libusb_get_device_list sim_list
#define libusb_free_device_list sim_free_list
#define libusb_get_device_descriptor sim_descriptor
#define libusb_get_bus_number sim_bus
#define libusb_get_port_numbers sim_ports
#define libusb_open sim_open
#define libusb_close sim_close
#define libusb_kernel_driver_active sim_active
#define libusb_detach_kernel_driver sim_detach
#define libusb_attach_kernel_driver sim_attach
#define libusb_claim_interface sim_claim
#define libusb_release_interface sim_release
#define libusb_control_transfer sim_transfer
#define getval sim_getval
#define fatalx sim_fatalx
#define dstate_setinfo sim_setinfo
#define dstate_dataok sim_dataok
#define dstate_datastale sim_stale
#define status_init sim_status_init
#define status_set sim_status_set
#define status_commit sim_status_commit
#define alarm_init sim_alarm_init
#define alarm_set sim_alarm_set
#define alarm_commit sim_alarm_commit

#include "nutdrv_vida.c"
#include "vida-recordings.h"

static int checks, failures;
static int connected = 1, bad_report, long_report, short_report, fail_query = -1, busy, address_generation;
static int opens, closes, detaches, attaches, claims, releases, exits, lists_freed;
static int reads, writes, good, stale, published;
static unsigned char query_seen[256];
static int tokens[2][2], context_token;
static libusb_device *device_list[2];
static char *bus_pattern = "^001$", *port_pattern = "^010$";
static char stored_load[128], stored_charge[128], stored_status[64];
static char simulated_flags[9] = "00001001";
static jmp_buf expected_failure;
static int expecting_failure;
/* Faults are injected at the transport boundary, leaving all production
 * admission, parsing, publication and cleanup decisions under test. */
enum fault_stage { FAULT_NONE, FAULT_INIT, FAULT_LIST, FAULT_DESCRIPTOR,
	FAULT_VENDOR, FAULT_PRODUCT, FAULT_PATH, FAULT_OPEN, FAULT_ACTIVE,
	FAULT_DETACH, FAULT_CLAIM, FAULT_REPORT };
static enum fault_stage fault;
static int kernel_active = 1, port_depth = 1, query_failure_kind;
static int status_committed, alarm_committed;
static char stored_alarms[256];
static struct observation { char name[64], value[128]; } observations[32];
static size_t observation_count;
int handling_upsdrv_shutdown;
static int shutdown_exit_flag;

#define CHECK(condition) do { checks++; if (!(condition)) { \
	fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); failures++; \
} } while (0)

void sim_fatalx(int status, const char *format, ...)
{
	NUT_UNUSED_VARIABLE(status);
	NUT_UNUSED_VARIABLE(format);
	if (expecting_failure) {
		longjmp(expected_failure, 1);
	}
	abort();
}

char *sim_getval(const char *name)
{
	if (!strcmp(name, "bus")) {
		return bus_pattern;
	}
	if (!strcmp(name, "busport")) {
		return port_pattern;
	}
	return NULL;
}

int sim_init(libusb_context **context)
{
	if (fault == FAULT_INIT) {
		return LIBUSB_ERROR_OTHER;
	}
	*context = (libusb_context *)&context_token;
	return 0;
}

void sim_exit(libusb_context *context)
{
	CHECK(context == (libusb_context *)&context_token);
	exits++;
}

ssize_t sim_list(libusb_context *context, libusb_device ***list)
{
	NUT_UNUSED_VARIABLE(context);
	device_list[0] = (libusb_device *)&tokens[address_generation % 2][0];
	device_list[1] = (libusb_device *)&tokens[address_generation % 2][1];
	*list = device_list;
	if (fault == FAULT_LIST) {
		*list = NULL;
		return LIBUSB_ERROR_IO;
	}
	return connected ? 2 : 0;
}

void sim_free_list(libusb_device **list, int unref)
{
	CHECK(list == device_list && unref == 1);
	lists_freed++;
}

int sim_descriptor(libusb_device *device, struct libusb_device_descriptor *descriptor)
{
	NUT_UNUSED_VARIABLE(device);
	memset(descriptor, 0, sizeof(*descriptor));
	descriptor->idVendor = 1;
	descriptor->idProduct = 0;
	if (fault == FAULT_VENDOR) descriptor->idVendor = 2;
	if (fault == FAULT_PRODUCT) descriptor->idProduct = 1;
	return fault == FAULT_DESCRIPTOR ? LIBUSB_ERROR_IO : 0;
}

uint8_t sim_bus(libusb_device *device)
{
	NUT_UNUSED_VARIABLE(device);
	return 1;
}

int sim_ports(libusb_device *device, uint8_t *ports, int capacity)
{
	int i;
	CHECK(capacity >= port_depth);
	if (fault == FAULT_PATH) return LIBUSB_ERROR_OVERFLOW;
	ports[0] = device == (libusb_device *)&tokens[address_generation % 2][0] ? 10 : 2;
	for (i = 1; i < port_depth; i++) ports[i] = (uint8_t)i;
	return port_depth;
}

int sim_open(libusb_device *device, libusb_device_handle **handle)
{
	/* A new enumeration supplies different libusb objects at the same ports. */
	CHECK(device == device_list[0] || device == device_list[1]);
	*handle = (libusb_device_handle *)device;
	opens++;
	if (fault == FAULT_OPEN) {
		*handle = NULL;
		return LIBUSB_ERROR_ACCESS;
	}
	return 0;
}

void sim_close(libusb_device_handle *handle)
{
	CHECK(handle != NULL);
	closes++;
}

int sim_active(libusb_device_handle *handle, int interface_number)
{
	NUT_UNUSED_VARIABLE(handle);
	CHECK(interface_number == 0);
	return fault == FAULT_ACTIVE ? LIBUSB_ERROR_IO : kernel_active;
}

int sim_detach(libusb_device_handle *handle, int interface_number)
{
	NUT_UNUSED_VARIABLE(handle);
	CHECK(interface_number == 0);
	detaches++;
	return fault == FAULT_DETACH ? LIBUSB_ERROR_ACCESS : 0;
}

int sim_attach(libusb_device_handle *handle, int interface_number)
{
	NUT_UNUSED_VARIABLE(handle);
	CHECK(interface_number == 0);
	attaches++;
	return 0;
}

int sim_claim(libusb_device_handle *handle, int interface_number)
{
	NUT_UNUSED_VARIABLE(handle);
	CHECK(interface_number == 0);
	claims++;
	return busy || fault == FAULT_CLAIM ? LIBUSB_ERROR_BUSY : 0;
}

int sim_release(libusb_device_handle *handle, int interface_number)
{
	NUT_UNUSED_VARIABLE(handle);
	CHECK(interface_number == 0);
	releases++;
	return 0;
}

static int frame(const char *text, unsigned char *data, size_t capacity)
{
	size_t i, length = strlen(text);
	CHECK(2 + 2 * length <= capacity && 2 + 2 * length <= 255);
	memset(data, 0, capacity);
	data[0] = (unsigned char)(2 + 2 * length);
	data[1] = 3;
	for (i = 0; i < length; i++) {
		data[2 + 2 * i] = (unsigned char)text[i];
	}
	return (int)(2 + 2 * length);
}

int sim_transfer(libusb_device_handle *handle, uint8_t type, uint8_t request,
	uint16_t value, uint16_t index, unsigned char *data, uint16_t length,
	unsigned int timeout)
{
	char status[80];
	int device = handle == (libusb_device_handle *)&tokens[address_generation % 2][0] ? 0 : 1;
	CHECK(handle == (libusb_device_handle *)device_list[0] ||
		handle == (libusb_device_handle *)device_list[1]);
	CHECK(request == 6 && timeout == VIDA_TIMEOUT);
	if (!(type & 0x80)) {
		writes++;
		return LIBUSB_ERROR_NOT_SUPPORTED;
	}
	if (type == 0x81) {
		if (fault == FAULT_REPORT) return LIBUSB_ERROR_TIMEOUT;
		CHECK(value == 0x2200 && index == 0 && length == sizeof(vida_report_descriptor) + 1);
		memcpy(data, vida_report_descriptor, sizeof(vida_report_descriptor));
		if (bad_report) {
			data[0] ^= 1;
		}
		if (long_report) {
			data[sizeof(vida_report_descriptor)] = 0x05;
			return (int)sizeof(vida_report_descriptor) + 1;
		}
		return (int)sizeof(vida_report_descriptor) - short_report;
	}
	reads++;
	query_seen[value & 0xff] = 1;
	CHECK(type == 0x80 && index == 0x0409 && length == 102);
	if (!connected) {
		return LIBUSB_ERROR_NO_DEVICE;
	}
	if (value == (uint16_t)fail_query) {
		switch (query_failure_kind) {
		case 0: return LIBUSB_ERROR_NO_DEVICE;
		case 1: return LIBUSB_ERROR_TIMEOUT;
		case 2: return LIBUSB_ERROR_PIPE;
		case 3: return 0;
		case 4: return length + 1;
		case 5: return frame("invalid\r", data, length);
		case 6:
			frame("BL100\r", data, length);
			return 3;
		default:
			CHECK(0);
			return LIBUSB_ERROR_OTHER;
		}
	}
	switch (value) {
	case 0x0303:
		snprintf(status, sizeof(status), "(239.0 000.0 238.0 %03d 49.8 54.1 20.0 %s\r",
			device ? 17 : 25, simulated_flags);
		return frame(status, data, length);
	case 0x030d: return frame("#230.0  10 48.00 50.0\r", data, length);
	case 0x03f3: return frame(device ? "BL095\r" : "BL100\r", data, length);
	case 0x030c: return frame("#                           V4.20     \r", data, length);
	default:
		CHECK(0);
		return LIBUSB_ERROR_NOT_SUPPORTED;
	}
}

int sim_setinfo(const char *name, const char *format, ...)
{
	va_list args;
	char value[128];
	size_t i;
	va_start(args, format);
	vsnprintf(value, sizeof(value), format, args);
	va_end(args);
	published++;
	for (i = 0; i < observation_count; i++) {
		if (!strcmp(observations[i].name, name)) break;
	}
	CHECK(i < sizeof(observations) / sizeof(observations[0]));
	if (i == sizeof(observations) / sizeof(observations[0])) return 0;
	if (i == observation_count) observation_count++;
	snprintf(observations[i].name, sizeof(observations[i].name), "%s", name);
	snprintf(observations[i].value, sizeof(observations[i].value), "%s", value);
	if (!strcmp(name, "ups.load")) {
		snprintf(stored_load, sizeof(stored_load), "%s", value);
	}
	if (!strcmp(name, "battery.charge")) {
		snprintf(stored_charge, sizeof(stored_charge), "%s", value);
	}
	CHECK(strcmp(name, "ups.realpower") && strcmp(name, "battery.runtime"));
	return 1;
}

void sim_dataok(void) { CHECK(status_committed && alarm_committed); good++; }
void sim_stale(void) { stale++; }
void sim_status_init(void) { stored_status[0] = '\0'; status_committed = 0; }
void sim_status_set(const char *value) { snprintfcat(stored_status, sizeof(stored_status), "%s ", value); }
void sim_status_commit(void) { status_committed = 1; }
void sim_alarm_init(void) { stored_alarms[0] = '\0'; alarm_committed = 0; }
void sim_alarm_set(const char *value) { snprintfcat(stored_alarms, sizeof(stored_alarms), "%s;", value); }
void sim_alarm_commit(void) { alarm_committed = 1; }
void set_exit_flag(int flag) { shutdown_exit_flag = flag; }
void addvar(int type, const char *name, const char *description)
{
	NUT_UNUSED_VARIABLE(type);
	NUT_UNUSED_VARIABLE(name);
	NUT_UNUSED_VARIABLE(description);
}

static void test_replay_and_stale(void)
{
	int i, before, before_reads;
	upsdrv_initups();
	for (i = 0; i < 20; i++) {
		upsdrv_updateinfo();
		CHECK(good == i + 1 && !stale);
		CHECK(!strcmp(stored_load, "25") && !strcmp(stored_charge, "100"));
		CHECK(strstr(stored_status, "OL") != NULL);
	}
	CHECK(opens == 1 && claims == 1);
	before = published;
	fail_query = 0x03f3;
	upsdrv_updateinfo();
	CHECK(stale == 1 && good == 20 && published == before);
	CHECK(closes == 1 && attaches == 1 && releases == 1);
	connected = 0;
	upsdrv_updateinfo();
	CHECK(stale == 2 && published == before);
	connected = 1;
	address_generation++;
	fail_query = -1;
	memcpy(simulated_flags, "11001001", 9);
	upsdrv_updateinfo();
	CHECK(good == 21 && opens == 2 && stale == 2);
	CHECK(strstr(stored_status, "OB") && strstr(stored_status, "LB"));
	before_reads = reads;
	CHECK(!vida_read_record((vida_query_t)99, NULL));
	CHECK(reads == before_reads);
	for (i = 0; i < 256; i++) {
		CHECK(!query_seen[i] || i == 3 || i == 12 || i == 13 || i == 243);
	}
	before_reads = reads;
	handling_upsdrv_shutdown = 1;
	upsdrv_shutdown();
	CHECK(reads == before_reads && shutdown_exit_flag == EF_EXIT_FAILURE && !writes);
	upsdrv_cleanup();
	upsdrv_cleanup();
	CHECK(closes == 2 && attaches == 2 && releases == 2 && exits == 1);
}

static void test_second_port_and_failed_admission(void)
{
	int before_reads, before_good, before_releases, before_opens;
	port_pattern = "^002$";
	upsdrv_initups();
	memcpy(simulated_flags, "00001001", 9);
	upsdrv_updateinfo();
	CHECK(!strcmp(vida_port, "002"));
	CHECK(!strcmp(stored_load, "17") && !strcmp(stored_charge, "95"));
	vida_close();
	before_reads = reads;
	before_good = good;
	bad_report = 1;
	upsdrv_updateinfo();
	CHECK(reads == before_reads && good == before_good && vida_handle == NULL);
	bad_report = 0;
	long_report = 1;
	upsdrv_updateinfo();
	CHECK(reads == before_reads && good == before_good && vida_handle == NULL);
	long_report = 0;
	short_report = 1;
	upsdrv_updateinfo();
	CHECK(reads == before_reads && good == before_good && vida_handle == NULL);
	short_report = 0;
	busy = 1;
	before_releases = releases;
	upsdrv_updateinfo();
	CHECK(reads == before_reads && releases == before_releases && vida_handle == NULL);
	busy = 0;
	regfree(vida_port_regex);
	free(vida_port_regex);
	vida_port_regex = NULL;
	CHECK(compile_regex(&vida_port_regex, ".*", REG_EXTENDED) == 0);
	before_opens = opens;
	expecting_failure = 1;
	if (!setjmp(expected_failure)) {
		vida_open();
		CHECK(0);
	}
	expecting_failure = 0;
	CHECK(opens == before_opens);
	upsdrv_cleanup();
}

static void test_malformed_records(void)
{
	vida_sample_t sample;
	unsigned char raw[102];
	char text[51];
	int length, i;
	uint16_t value;
	const char *bad[] = {
		"(239 0 238 25 50 54 20 0000100x",
		"(239 0 238 nan 50 54 20 00001001",
		"(239 0 238 999 50 54 20 00001001",
		"(239 0 238 25 50 54 20 00001001 extra",
		"(239 0 238 25 50 54 20", NULL
	};
	memset(&sample, 0, sizeof(sample));
	for (i = 0; bad[i]; i++) {
		CHECK(!vida_parse_record(VIDA_STATUS, bad[i], &sample));
	}
	CHECK(!vida_parse_record(VIDA_CHARGE, "BL101", &sample));
	CHECK(!vida_parse_record(VIDA_CHARGE, "BL1", &sample));
	CHECK(!vida_parse_record(VIDA_RATINGS, "#230 10 48", &sample));
	CHECK(!vida_parse_record(VIDA_FIRMWARE, "#UNKNOWN", &sample));
	CHECK(!vida_query_value((vida_query_t)99, &value));
	length = frame("BL100\r", raw, sizeof(raw));
	CHECK(vida_decode_string(raw, (size_t)length, text, sizeof(text)));
	CHECK(!vida_decode_string(raw, (size_t)length - 1, text, sizeof(text)));
	raw[1] = 2;
	CHECK(!vida_decode_string(raw, (size_t)length, text, sizeof(text)));
	raw[1] = 3;
	raw[3] = 1;
	CHECK(!vida_decode_string(raw, (size_t)length, text, sizeof(text)));
	raw[3] = 0;
	raw[length - 2] = 0;
	CHECK(!vida_decode_string(raw, (size_t)length, text, sizeof(text)));
}

/* Reset through production cleanup before changing the simulated connection. */
static void reset_fixture(void)
{
	upsdrv_cleanup();
	fault = FAULT_NONE;
	connected = kernel_active = port_depth = 1;
	bad_report = long_report = short_report = busy = query_failure_kind = 0;
	fail_query = -1;
	bus_pattern = "^001$";
	port_pattern = "^010$";
	opens = closes = detaches = attaches = claims = releases = exits = lists_freed = 0;
	reads = writes = good = stale = published = 0;
	observation_count = 0;
	memset(observations, 0, sizeof(observations));
	memset(query_seen, 0, sizeof(query_seen));
	memcpy(simulated_flags, "00001001", 9);
}

static void expect_value(const char *name, const char *expected)
{
	size_t i;
	for (i = 0; i < observation_count; i++) {
		if (!strcmp(observations[i].name, name)) {
			if (strcmp(observations[i].value, expected)) {
				fprintf(stderr, "%s: expected '%s', got '%s'\n", name, expected, observations[i].value);
			}
			CHECK(!strcmp(observations[i].value, expected));
			return;
		}
	}
	fprintf(stderr, "Missing published variable: %s\n", name);
	CHECK(0);
}

static void test_complete_publication(void)
{
	static const struct { const char *name, *value; } expected[] = {
		{ "ups.mfr", "VIDA" }, { "ups.model", "Armour ARM3000" },
		{ "ups.vendorid", "0001" }, { "ups.productid", "0000" },
		{ "device.bus", "usb" }, { "driver.parameter.bus.actual", "001" },
		{ "driver.parameter.busport.actual", "010" },
		{ "input.voltage", "239.0" }, { "input.voltage.fault", "0.0" },
		{ "output.voltage", "238.0" }, { "ups.load", "25" },
		{ "input.frequency", "49.8" }, { "battery.voltage", "54.1" },
		{ "ups.temperature", "20.0" }, { "battery.charge", "100" },
		{ "input.voltage.nominal", "230.0" }, { "input.current.nominal", "10.0" },
		{ "battery.voltage.nominal", "48.0" }, { "input.frequency.nominal", "50.0" },
		{ "ups.firmware", "V4.20" }, { "ups.type", "offline / line interactive" },
		{ "ups.beeper.status", "enabled" }
	};
	size_t i;
	reset_fixture();
	upsdrv_initups();
	upsdrv_initinfo();
	CHECK(observation_count == sizeof(expected) / sizeof(expected[0]));
	for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
		expect_value(expected[i].name, expected[i].value);
	}
	CHECK(!strcmp(stored_status, "OL ") && !stored_alarms[0]);
}

static void test_all_status_combinations_and_clear(void)
{
	unsigned int mask, bit;
	char expected_status[64], expected_alarms[256];
	reset_fixture();
	upsdrv_initups();
	for (mask = 0; mask < 256; mask++) {
		for (bit = 0; bit < 8; bit++) simulated_flags[bit] = mask & (1U << bit) ? '1' : '0';
		snprintf(expected_status, sizeof(expected_status), "%s%s%s",
			mask & 1 ? "OB " : "OL ", mask & 2 ? "LB " : "", mask & 32 ? "CAL " : "");
		snprintf(expected_alarms, sizeof(expected_alarms), "%s%s%s",
			mask & 4 ? "Bypass or automatic voltage regulation active;" : "",
			mask & 8 ? "UPS fault;" : "", mask & 64 ? "UPS shutdown active;" : "");
		upsdrv_updateinfo();
		CHECK(!strcmp(stored_status, expected_status));
		CHECK(!strcmp(stored_alarms, expected_alarms));
		expect_value("ups.type", mask & 16 ? "offline / line interactive" : "online");
		expect_value("ups.beeper.status", mask & 128 ? "enabled" : "disabled");
		/* Every combination returns to a healthy sample, proving clear/replay. */
		memcpy(simulated_flags, "00001001", 9);
		upsdrv_updateinfo();
		CHECK(!strcmp(stored_status, "OL ") && !stored_alarms[0]);
	}
	CHECK(good == 512 && !stale && opens == 1);
}

static void test_acquisition_failures(void)
{
	static const struct { enum fault_stage stage; int open, close, detach, attach, claim, release, free_list; } cases[] = {
		{ FAULT_LIST,       0, 0, 0, 0, 0, 0, 0 },
		{ FAULT_DESCRIPTOR, 0, 0, 0, 0, 0, 0, 1 },
		{ FAULT_VENDOR,     0, 0, 0, 0, 0, 0, 1 },
		{ FAULT_PRODUCT,    0, 0, 0, 0, 0, 0, 1 },
		{ FAULT_PATH,       0, 0, 0, 0, 0, 0, 1 },
		{ FAULT_OPEN,       1, 0, 0, 0, 0, 0, 1 },
		{ FAULT_ACTIVE,     1, 1, 0, 0, 0, 0, 1 },
		{ FAULT_DETACH,     1, 1, 1, 0, 0, 0, 1 },
		{ FAULT_CLAIM,      1, 1, 1, 1, 1, 0, 1 },
		{ FAULT_REPORT,     1, 1, 1, 1, 1, 1, 1 }
	};
	size_t i;
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		reset_fixture();
		upsdrv_initups();
		fault = cases[i].stage;
		upsdrv_updateinfo();
		CHECK(stale == 1 && !good && !published && !reads && !vida_handle);
		CHECK(opens == cases[i].open && closes == cases[i].close);
		CHECK(detaches == cases[i].detach && attaches == cases[i].attach);
		CHECK(claims == cases[i].claim && releases == cases[i].release);
		CHECK(lists_freed == cases[i].free_list);
		fault = FAULT_NONE;
		upsdrv_updateinfo();
		CHECK(good == 1 && stale == 1);
		upsdrv_cleanup();
		CHECK(closes == cases[i].close + 1 && releases == cases[i].release + 1);
		CHECK(attaches == cases[i].attach + 1 && exits == 1);
		upsdrv_cleanup();
		CHECK(exits == 1 && !writes);
	}
	/* No kernel driver, and platforms without kernel detach support. */
	for (i = 0; i < 2; i++) {
		reset_fixture();
		kernel_active = i ? LIBUSB_ERROR_NOT_SUPPORTED : 0;
		upsdrv_initups();
		upsdrv_updateinfo();
		upsdrv_cleanup();
		CHECK(good == 1 && !detaches && !attaches && releases == 1 && closes == 1);
	}
}

static void test_each_query_failure_is_atomic(void)
{
	static const int queries[] = { 0x0303, 0x030d, 0x03f3, 0x030c };
	struct observation before[32];
	size_t query, count;
	int kind, previous_good, previous_reads;
	for (query = 0; query < 4; query++) {
		for (kind = 0; kind < 7; kind++) {
			reset_fixture();
			upsdrv_initups();
			upsdrv_updateinfo();
			memcpy(before, observations, sizeof(before));
			count = observation_count;
			previous_good = good;
			previous_reads = reads;
			fail_query = queries[query];
			query_failure_kind = kind;
			upsdrv_updateinfo();
			CHECK(stale == 1 && good == previous_good && !vida_handle);
			CHECK(reads == previous_reads + (int)query + 1);
			CHECK(observation_count == count && !memcmp(before, observations, sizeof(before)));
			CHECK(closes == 1 && releases == 1 && attaches == 1);
			fail_query = -1;
			upsdrv_updateinfo();
			CHECK(good == previous_good + 1 && opens == 2 && !writes);
		}
	}
}

static void test_recordings_and_framing_boundaries(void)
{
	static const struct { vida_query_t query; const unsigned char *raw; size_t length; const char *text; } records[] = {
		{ VIDA_STATUS, recorded_status, sizeof(recorded_status), "(239.0 000.0 238.0 025 49.8 54.1 20.0 00001001" },
		{ VIDA_RATINGS, recorded_ratings, sizeof(recorded_ratings), "#230.0  10 48.00 50.0" },
		{ VIDA_CHARGE, recorded_battery_charge, sizeof(recorded_battery_charge), "BL100" },
		{ VIDA_FIRMWARE, recorded_firmware, sizeof(recorded_firmware), "#                           V4.20     " }
	};
	unsigned char raw[102];
	char text[51];
	vida_sample_t sample;
	size_t i, length, characters;
	memset(&sample, 0, sizeof(sample));
	for (i = 0; i < sizeof(records) / sizeof(records[0]); i++) {
		CHECK(vida_decode_string(records[i].raw, records[i].length, text, sizeof(text)));
		CHECK(!strcmp(text, records[i].text));
		CHECK(vida_parse_record(records[i].query, text, &sample));
		for (length = 0; length < records[i].length; length++) {
			CHECK(!vida_decode_string(records[i].raw, length, text, sizeof(text)));
		}
		characters = (records[i].length - 2) / 2;
		CHECK(!vida_decode_string(records[i].raw, records[i].length, text, characters));
		CHECK(vida_decode_string(records[i].raw, records[i].length, text, characters + 1));
		memset(raw, 0, sizeof(raw));
		memcpy(raw, records[i].raw, records[i].length);
		CHECK(vida_decode_string(raw, sizeof(raw), text, sizeof(text)));
		raw[sizeof(raw) - 1] = 1;
		CHECK(!vida_decode_string(raw, sizeof(raw), text, sizeof(text)));
		raw[sizeof(raw) - 1] = 0;
		raw[0]--;
		CHECK(!vida_decode_string(raw, sizeof(raw), text, sizeof(text)));
		raw[0] = 2;
		CHECK(!vida_decode_string(raw, sizeof(raw), text, sizeof(text)));
	}
	/* Decimal conversion is checked well below the device's 0.1-unit
	 * resolution; publication above separately checks exact output strings. */
	CHECK(fabs(sample.input_voltage - 239.0) < 1e-9 && fabs(sample.fault_voltage) < 1e-9);
	CHECK(fabs(sample.output_voltage - 238.0) < 1e-9 && fabs(sample.load - 25.0) < 1e-9);
	CHECK(fabs(sample.frequency - 49.8) < 1e-9 && fabs(sample.battery_voltage - 54.1) < 1e-9);
	CHECK(fabs(sample.temperature - 20.0) < 1e-9 && fabs(sample.charge - 100.0) < 1e-9);
	CHECK(!strcmp(sample.flags, "00001001"));
	CHECK(fabs(sample.nominal_voltage - 230.0) < 1e-9 && fabs(sample.nominal_current - 10.0) < 1e-9);
	CHECK(fabs(sample.nominal_battery_voltage - 48.0) < 1e-9 && fabs(sample.nominal_frequency - 50.0) < 1e-9);
	CHECK(!strcmp(sample.firmware, "V4.20"));
}

static void test_numeric_boundaries(void)
{
	static const struct { const char *low, *high, *below, *above; } limits[] = {
		{ "0", "500", "-0.1", "500.1" }, { "0", "500", "-0.1", "500.1" },
		{ "0", "500", "-0.1", "500.1" }, { "0", "150", "-0.1", "150.1" },
		{ "0", "100", "-0.1", "100.1" }, { "0", "100", "-0.1", "100.1" },
		{ "-40", "150", "-40.1", "150.1" }
	};
	static const char *invalid[] = { "nan", "inf", "1e2", "1.2.3", "--1", "1x", "1234567890123456789012345678901234" };
	vida_sample_t sample;
	char text[512];
	size_t field, which, j;
	const char *value;
	for (field = 0; field < 7; field++) {
		for (which = 0; which < 4 + sizeof(invalid) / sizeof(invalid[0]); which++) {
			value = which == 0 ? limits[field].low : which == 1 ? limits[field].high :
				which == 2 ? limits[field].below : which == 3 ? limits[field].above : invalid[which - 4];
			snprintf(text, sizeof(text), "(");
			for (j = 0; j < 7; j++) snprintfcat(text, sizeof(text), "%s ", field == j ? value : "0");
			snprintfcat(text, sizeof(text), "00001001");
			CHECK(vida_parse_record(VIDA_STATUS, text, &sample) == (which < 2));
		}
	}
	CHECK(vida_parse_record(VIDA_CHARGE, "BL000", &sample) && fabs(sample.charge) < 1e-9);
	CHECK(vida_parse_record(VIDA_CHARGE, "BL100", &sample) && fabs(sample.charge - 100.0) < 1e-9);
	CHECK(!vida_parse_record(VIDA_CHARGE, "BL-01", &sample));
	CHECK(!vida_parse_record(VIDA_CHARGE, "BL100x", &sample));
	CHECK(!vida_parse_record(VIDA_RATINGS, "#230 10 48 50 extra", &sample));
	CHECK(!vida_parse_record(VIDA_FIRMWARE, "#V4.20 extra", &sample));
	CHECK(!vida_parse_record(VIDA_FIRMWARE, "#V", &sample));
}

static void expect_initialization_failure(void)
{
	expecting_failure = 1;
	if (!setjmp(expected_failure)) { upsdrv_initups(); CHECK(0); }
	expecting_failure = 0;
}

static void test_initialization_and_port_paths(void)
{
	int mode;
	char bus[4], port[32];
	for (mode = 0; mode < 4; mode++) {
		reset_fixture();
		if (mode == 0) bus_pattern = NULL;
		if (mode == 1) port_pattern = NULL;
		if (mode == 2) port_pattern = "[";
		if (mode == 3) fault = FAULT_INIT;
		expect_initialization_failure();
		upsdrv_cleanup();
		CHECK(!opens && !reads && !vida_context && !vida_bus_regex && !vida_port_regex);
	}
	reset_fixture();
	port_depth = 3;
	CHECK(vida_device_path((libusb_device *)&tokens[address_generation % 2][0], bus, sizeof(bus), port, sizeof(port)));
	CHECK(!strcmp(bus, "001") && !strcmp(port, "010.001.002"));
	CHECK(!vida_device_path((libusb_device *)&tokens[address_generation % 2][0], bus, sizeof(bus), port, 8));
	port_depth = 0;
	CHECK(!vida_device_path((libusb_device *)&tokens[address_generation % 2][0], bus, sizeof(bus), port, sizeof(port)));
	port_depth = 1;
}

int main(void)
{
	test_replay_and_stale();
	test_second_port_and_failed_admission();
	test_malformed_records();
	test_complete_publication();
	test_all_status_combinations_and_clear();
	test_acquisition_failures();
	test_each_query_failure_is_atomic();
	test_recordings_and_framing_boundaries();
	test_numeric_boundaries();
	test_initialization_and_port_paths();
	upsdrv_cleanup();
	printf("VIDA production replay: %d checks, %d failures\n", checks, failures);
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
