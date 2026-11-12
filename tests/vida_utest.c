/* vida_utest.c - replay the production VIDA driver at its native USB boundary.
 * Covers complete publication, stale data, identity, two physical ports,
 * reconnect with a changed USB address, and partial ownership cleanup.
 * No hardware or power-control operation is used by this test.
 * Copyright (C) 2026 NUT contributors. GPL version 2 or later.
 */
#include "config.h"
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
	return 0;
}

uint8_t sim_bus(libusb_device *device)
{
	NUT_UNUSED_VARIABLE(device);
	return 1;
}

int sim_ports(libusb_device *device, uint8_t *ports, int capacity)
{
	CHECK(capacity >= 1);
	ports[0] = device == (libusb_device *)&tokens[address_generation % 2][0] ? 10 : 2;
	return 1;
}

int sim_open(libusb_device *device, libusb_device_handle **handle)
{
	/* A new enumeration supplies different libusb objects at the same ports. */
	CHECK(device == device_list[0] || device == device_list[1]);
	*handle = (libusb_device_handle *)device;
	opens++;
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
	return 1;
}

int sim_detach(libusb_device_handle *handle, int interface_number)
{
	NUT_UNUSED_VARIABLE(handle);
	CHECK(interface_number == 0);
	detaches++;
	return 0;
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
	return busy ? LIBUSB_ERROR_BUSY : 0;
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
	if (!connected || value == (uint16_t)fail_query) {
		return LIBUSB_ERROR_NO_DEVICE;
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
	va_start(args, format);
	vsnprintf(value, sizeof(value), format, args);
	va_end(args);
	published++;
	if (!strcmp(name, "ups.load")) {
		snprintf(stored_load, sizeof(stored_load), "%s", value);
	}
	if (!strcmp(name, "battery.charge")) {
		snprintf(stored_charge, sizeof(stored_charge), "%s", value);
	}
	CHECK(strcmp(name, "ups.realpower") && strcmp(name, "battery.runtime"));
	return 1;
}

void sim_dataok(void) { good++; }
void sim_stale(void) { stale++; }
void sim_status_init(void) { stored_status[0] = '\0'; }
void sim_status_set(const char *value) { snprintfcat(stored_status, sizeof(stored_status), "%s ", value); }
void sim_status_commit(void) { }
void sim_alarm_init(void) { }
void sim_alarm_set(const char *value) { NUT_UNUSED_VARIABLE(value); }
void sim_alarm_commit(void) { }
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

int main(void)
{
	test_replay_and_stale();
	test_second_port_and_failed_admission();
	test_malformed_records();
	printf("VIDA production replay: %d checks, %d failures\n", checks, failures);
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
