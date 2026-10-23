/* hashx-usb-protocol-test.c - offline HashX USB framing regression tests
 *
 * Copyright (C) 2026 Network UPS Tools contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "hashx-usb-protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int failures = 0;
static unsigned int checks = 0;

/* Sanitized status frames from the passive Keen 2000 capture. No identity
 * strings, unknown vendor replies or USB padding are retained. */
static const char frame_four[] =
	"#I243.0O245.0L004B100V27.1F50.0H50.0R040S"
	"\x80\x84\xd0\x80\x80\xc0\r";
static const char frame_five[] =
	"#I243.0O245.0L005B100V27.1F49.9H49.9R040S"
	"\x80\x84\xd0\x80\x80\xc0\r";

static void check(int condition, const char *description)
{
	checks++;
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", description);
		failures++;
	}
}

static size_t make_packet(unsigned char *packet, const char *data, size_t length)
{
	memset(packet, 0, HASHX_USB_PACKET_SIZE);
	packet[0] = 0x28U;
	packet[1] = (unsigned char)length;
	memcpy(packet + 2, data, length);
	return length + 2;
}

static void check_reply(const struct hashx_usb_reply *reply, const char *frame)
{
	check(reply->length == HASHX_USB_FRAME_SIZE, "complete reply has exact length");
	check(memcmp(reply->data, frame, HASHX_USB_FRAME_SIZE) == 0,
		"assembled reply preserves captured bytes");
	check(reply->data[HASHX_USB_FRAME_SIZE - 1] == '\r', "final CR is retained");
	check(reply->data[HASHX_USB_FRAME_SIZE] == '\0', "NUL follows the final CR");
}

static void test_captured_fixtures(void)
{
	struct hashx_usb_reply reply;
	unsigned char packet[HASHX_USB_PACKET_SIZE];
	size_t length;
	const char *frames[] = { frame_four, frame_five };
	size_t splits[] = { 28, 29 };
	size_t index;

	for (index = 0; index < sizeof(frames) / sizeof(frames[0]); index++) {
		hashx_usb_reply_reset(&reply);
		length = make_packet(packet, frames[index], splits[index]);
		check(hashx_usb_reply_append(&reply, packet, length) == 0,
			"captured first chunk is incomplete");
		make_packet(packet, frames[index] + splits[index],
			HASHX_USB_FRAME_SIZE - splits[index]);
		check(hashx_usb_reply_append(&reply, packet, sizeof(packet)) == 1,
			"captured second chunk completes padded USB transfer");
		check_reply(&reply, frames[index]);
	}
}

static void test_all_fragment_boundaries(void)
{
	struct hashx_usb_reply reply;
	unsigned char packet[HASHX_USB_PACKET_SIZE];
	size_t split;
	size_t length;

	for (split = 1; split < HASHX_USB_FRAME_SIZE; split++) {
		hashx_usb_reply_reset(&reply);
		length = make_packet(packet, frame_five, split);
		check(hashx_usb_reply_append(&reply, packet, length) == 0,
			"arbitrary first fragment remains incomplete");
		length = make_packet(packet, frame_five + split,
			HASHX_USB_FRAME_SIZE - split);
		check(hashx_usb_reply_append(&reply, packet, length) == 1,
			"arbitrary second fragment completes status");
		check_reply(&reply, frame_five);
	}
}

static void test_zero_load_overload_and_status_schema(void)
{
	char frame[HASHX_USB_FRAME_SIZE + 1];
	size_t offset;

	memcpy(frame, frame_five, sizeof(frame));
	memcpy(frame + 14, "000", 3);
	check(hashx_usb_status_valid(frame, HASHX_USB_FRAME_SIZE),
		"genuine zero load is valid");
	memcpy(frame + 14, "125", 3);
	check(hashx_usb_status_valid(frame, HASHX_USB_FRAME_SIZE),
		"overload above 100 percent is valid");
	memcpy(frame + 18, "101", 3);
	check(!hashx_usb_status_valid(frame, HASHX_USB_FRAME_SIZE),
		"battery capacity above 100 percent is invalid");
	check(!hashx_usb_status_valid(NULL, HASHX_USB_FRAME_SIZE), "NULL status is invalid");
	check(!hashx_usb_status_valid(frame_five, HASHX_USB_FRAME_SIZE - 1),
		"short status is invalid");

	for (offset = 0; offset < HASHX_USB_FRAME_SIZE; offset++) {
		memcpy(frame, frame_five, sizeof(frame));
		frame[offset] = '\0';
		check(!hashx_usb_status_valid(frame, HASHX_USB_FRAME_SIZE),
			"embedded NUL cannot pass the strict status schema");
	}
}

static void test_ignored_packets(void)
{
	struct hashx_usb_reply reply;
	struct hashx_usb_reply saved;
	unsigned char packet[HASHX_USB_PACKET_SIZE];
	size_t length;

	hashx_usb_reply_reset(&reply);
	length = make_packet(packet, frame_five, 29);
	check(hashx_usb_reply_append(&reply, packet, length) == 0, "start pending reply");
	saved = reply;
	memset(packet, 0, sizeof(packet));
	packet[0] = 0x2cU;
	packet[1] = 255U;
	check(hashx_usb_reply_append(&reply, packet, sizeof(packet)) == 0,
		"unrelated report is ignored");
	check(reply.length == saved.length && memcmp(reply.data, saved.data, sizeof(reply.data)) == 0,
		"unrelated report leaves pending bytes unchanged");
	packet[0] = 0x28U;
	packet[1] = 0;
	check(hashx_usb_reply_append(&reply, packet, sizeof(packet)) == 0,
		"empty declared chunk is ignored");
	check(reply.length == saved.length && memcmp(reply.data, saved.data, sizeof(reply.data)) == 0,
		"empty chunk leaves pending bytes unchanged");
	length = make_packet(packet, frame_five + 29, HASHX_USB_FRAME_SIZE - 29);
	check(hashx_usb_reply_append(&reply, packet, length) == 1,
		"interleaved unrelated and empty reports do not corrupt a reply");
	check_reply(&reply, frame_five);
}

static void test_malformed_chunks_reset(void)
{
	struct hashx_usb_reply reply;
	unsigned char packet[HASHX_USB_PACKET_SIZE + 1];
	size_t length;

	hashx_usb_reply_reset(&reply);
	check(hashx_usb_reply_append(&reply, NULL, 0) == -1, "missing packet is malformed");
	check(reply.length == 0 && reply.data[0] == '\0', "missing packet resets reply");
	memset(packet, 0, sizeof(packet));
	packet[0] = 0x28U;
	check(hashx_usb_reply_append(&reply, packet, 1) == -1, "short chunk header is malformed");
	check(hashx_usb_reply_append(&reply, packet, sizeof(packet)) == -1,
		"oversized USB packet is malformed");
	length = make_packet(packet, frame_five, 29);
	check(hashx_usb_reply_append(&reply, packet, length - 1) == -1,
		"truncated declared content cannot be decoded");
	packet[1] = 63;
	check(hashx_usb_reply_append(&reply, packet, HASHX_USB_PACKET_SIZE) == -1,
		"declared content larger than packet cannot be decoded");
	length = make_packet(packet, frame_five, 29);
	check(hashx_usb_reply_append(&reply, packet, length) == 0, "start reply before malformed continuation");
	packet[2] = '\r';
	check(hashx_usb_reply_append(&reply, packet, length) == -1,
		"CR before final status offset is malformed");
	check(reply.length == 0 && reply.data[0] == '\0', "malformed continuation discards pending reply");
	length = make_packet(packet, frame_five, 29);
	check(hashx_usb_reply_append(&reply, packet, length) == 0, "start reply before repeated header");
	check(hashx_usb_reply_append(&reply, packet, length) == -1,
		"new header while partial cannot join response cycles");
	check(reply.length == 0, "repeated header resets pending state");
	length = make_packet(packet, frame_five + 29, HASHX_USB_FRAME_SIZE - 29);
	check(hashx_usb_reply_append(&reply, packet, length) == -1,
		"continuation without initial header is malformed");
	length = make_packet(packet, frame_five, HASHX_USB_FRAME_SIZE);
	packet[3] = 'X';
	check(hashx_usb_reply_append(&reply, packet, length) == -1, "unrecognized status prefix is malformed");
	reply.length = HASHX_USB_FRAME_SIZE + 1;
	check(hashx_usb_reply_append(&reply, packet, length) == -1,
		"corrupted accumulator length is rejected before arithmetic");
	check(reply.length == 0, "corrupted accumulator is reset");
	check(hashx_usb_reply_append(NULL, packet, length) == -1, "NULL accumulator is rejected");
}

static void test_overflow_and_recovery(void)
{
	struct hashx_usb_reply reply;
	unsigned char packet[HASHX_USB_PACKET_SIZE];
	size_t length;

	hashx_usb_reply_reset(&reply);
	length = make_packet(packet, frame_five, 29);
	check(hashx_usb_reply_append(&reply, packet, length) == 0, "start reply before overflow");
	length = make_packet(packet, frame_five + 28, HASHX_USB_FRAME_SIZE - 28);
	check(hashx_usb_reply_append(&reply, packet, length) == -1, "accumulated frame cannot overflow");
	check(reply.length == 0, "overflow resets accumulator");
	length = make_packet(packet, frame_four, HASHX_USB_FRAME_SIZE);
	check(hashx_usb_reply_append(&reply, packet, length) == 1,
		"new complete reply succeeds after rejected overflow");
	check_reply(&reply, frame_four);
}

struct mock_packet {
	unsigned char data[HASHX_USB_PACKET_SIZE];
	int count;
	unsigned int timeout_ms;
};

struct mock_transport {
	struct mock_packet reads[32];
	size_t read_count;
	size_t read_next;
	unsigned int writes;
	int write_result;
};

static void mock_reset(struct mock_transport *mock)
{
	memset(mock, 0, sizeof(*mock));
	mock->write_result = HASHX_USB_PACKET_SIZE;
}

static void mock_add_read(struct mock_transport *mock, const char *data,
	size_t size, int count, unsigned int timeout_ms)
{
	struct mock_packet *event = &mock->reads[mock->read_count++];

	event->count = count;
	event->timeout_ms = timeout_ms;
	if (data)
		make_packet(event->data, data, size);
}

static int mock_read(void *context, unsigned char *packet, size_t size,
	unsigned int timeout_ms)
{
	struct mock_transport *mock = (struct mock_transport *)context;
	const struct mock_packet *event;

	check(size == HASHX_USB_PACKET_SIZE, "mock read uses bounded packet buffer");
	if (mock->read_next >= mock->read_count) {
		check(0, "transaction made an unexpected extra read");
		return -1;
	}
	event = &mock->reads[mock->read_next++];
	check(timeout_ms == event->timeout_ms, "drain and response read timeouts are distinct");
	if (event->count > 0 && event->count <= HASHX_USB_PACKET_SIZE)
		memcpy(packet, event->data, (size_t)event->count);
	return event->count;
}

static int mock_write(void *context, const unsigned char *packet, size_t size,
	unsigned int timeout_ms)
{
	struct mock_transport *mock = (struct mock_transport *)context;
	unsigned char expected[HASHX_USB_PACKET_SIZE];

	memset(expected, 0, sizeof(expected));
	expected[0] = 0x29U;
	expected[1] = 2;
	expected[2] = 'B';
	expected[3] = '\r';
	mock->writes++;
	check(size == sizeof(expected), "status query writes a full 64-byte packet");
	check(timeout_ms == 1000, "status query has bounded write timeout");
	if (size == sizeof(expected))
		check(memcmp(packet, expected, sizeof(expected)) == 0,
			"only B plus CR is sent, with zeroed packet padding");
	return mock->write_result;
}

static void mock_add_reply(struct mock_transport *mock, const char *frame,
	size_t split)
{
	mock_add_read(mock, frame, split, HASHX_USB_PACKET_SIZE, 250);
	mock_add_read(mock, frame + split, HASHX_USB_FRAME_SIZE - split,
		HASHX_USB_PACKET_SIZE, 250);
}

static void test_query_discards_queued_old_reply(void)
{
	struct mock_transport mock;
	struct hashx_usb_reply reply;

	mock_reset(&mock);
	mock_add_read(&mock, frame_five, 29, HASHX_USB_PACKET_SIZE, 1);
	mock_add_read(&mock, frame_five + 29, HASHX_USB_FRAME_SIZE - 29,
		HASHX_USB_PACKET_SIZE, 1);
	mock_add_read(&mock, NULL, 0, 0, 1);
	mock_add_reply(&mock, frame_four, 28);
	check(hashx_usb_status_query(&mock, mock_read, mock_write, &reply) == 1,
		"query discards complete old reply and accepts following fresh reply");
	check(mock.writes == 1 && mock.read_next == mock.read_count,
		"query consumes expected drain and reply reads only");
	check_reply(&reply, frame_four);
}

static void test_query_timeout_then_next_drain(void)
{
	struct mock_transport mock;
	struct hashx_usb_reply reply;

	mock_reset(&mock);
	mock_add_read(&mock, NULL, 0, 0, 1);
	mock_add_read(&mock, frame_five, 29, HASHX_USB_PACKET_SIZE, 250);
	mock_add_read(&mock, NULL, 0, 0, 250);
	mock_add_read(&mock, frame_five + 29, HASHX_USB_FRAME_SIZE - 29,
		HASHX_USB_PACKET_SIZE, 1);
	mock_add_read(&mock, NULL, 0, 0, 1);
	mock_add_reply(&mock, frame_four, 28);
	check(hashx_usb_status_query(&mock, mock_read, mock_write, &reply) == -1,
		"timeout during a fragmented reply fails the transaction");
	check(reply.length == 0 && reply.data[0] == '\0',
		"failed query does not preserve partial or old output data");
	check(hashx_usb_status_query(&mock, mock_read, mock_write, &reply) == 1,
		"next query drains delayed old tail before accepting fresh reply");
	check(mock.writes == 2 && mock.read_next == mock.read_count,
		"two transactions emit exactly two status queries");
	check_reply(&reply, frame_four);
}

static void test_query_short_and_failed_writes(void)
{
	static const int write_results[] = { -1, 0, 63, 65 };
	struct mock_transport mock;
	struct hashx_usb_reply reply;
	size_t index;

	for (index = 0; index < sizeof(write_results) / sizeof(write_results[0]); index++) {
		mock_reset(&mock);
		mock.write_result = write_results[index];
		mock_add_read(&mock, NULL, 0, 0, 1);
		check(hashx_usb_status_query(&mock, mock_read, mock_write, &reply) == -1,
			"failed or non-full write cannot accept telemetry");
		check(reply.length == 0 && mock.writes == 1 && mock.read_next == 1,
			"write failure resets state and stops before response reads");
	}
}

static void test_query_negative_and_oversized_reads(void)
{
	static const int read_results[] = { -1, 65 };
	struct mock_transport mock;
	struct hashx_usb_reply reply;
	size_t index;

	for (index = 0; index < sizeof(read_results) / sizeof(read_results[0]); index++) {
		mock_reset(&mock);
		mock_add_read(&mock, NULL, 0, read_results[index], 1);
		check(hashx_usb_status_query(&mock, mock_read, mock_write, &reply) == -1,
			"negative or oversized drain read fails before query");
		check(mock.writes == 0 && reply.length == 0, "bad drain cannot send a query");
		mock_reset(&mock);
		mock_add_read(&mock, NULL, 0, 0, 1);
		mock_add_read(&mock, frame_five, 29, HASHX_USB_PACKET_SIZE, 250);
		mock_add_read(&mock, NULL, 0, read_results[index], 250);
		check(hashx_usb_status_query(&mock, mock_read, mock_write, &reply) == -1,
			"negative or oversized response read fails without accepting partial data");
		check(mock.writes == 1 && reply.length == 0, "bad response resets partial reply");
	}
}

static void test_query_bounded_drain_and_unknown_replies(void)
{
	struct mock_transport mock;
	struct hashx_usb_reply reply;
	size_t index;

	mock_reset(&mock);
	for (index = 0; index < 8; index++)
		mock_add_read(&mock, frame_five, 29, HASHX_USB_PACKET_SIZE, 1);
	check(hashx_usb_status_query(&mock, mock_read, mock_write, &reply) == -1,
		"continuous queued data exhausts bounded drain");
	check(mock.read_next == 8 && mock.writes == 0 && reply.length == 0,
		"drain exhaustion sends no query and accepts no old data");

	mock_reset(&mock);
	mock_add_read(&mock, NULL, 0, 0, 1);
	for (index = 0; index < 8; index++) {
		mock_add_read(&mock, NULL, 0, HASHX_USB_PACKET_SIZE, 250);
		mock.reads[mock.read_count - 1].data[0] = 0x2cU;
	}
	check(hashx_usb_status_query(&mock, mock_read, mock_write, &reply) == -1,
		"unknown reports exhaust bounded response reads without telemetry");
	check(mock.read_next == 9 && mock.writes == 1 && reply.length == 0,
		"unknown replies cannot run an unbounded loop or preserve stale data");
}

static void test_query_zero_and_malformed_reply(void)
{
	struct mock_transport mock;
	struct hashx_usb_reply reply;
	char zero_frame[HASHX_USB_FRAME_SIZE + 1];

	memcpy(zero_frame, frame_five, sizeof(zero_frame));
	memcpy(zero_frame + 14, "000", 3);
	mock_reset(&mock);
	mock_add_read(&mock, NULL, 0, 0, 1);
	mock_add_reply(&mock, zero_frame, 29);
	check(hashx_usb_status_query(&mock, mock_read, mock_write, &reply) == 1,
		"valid zero load passes the transaction unchanged");
	check_reply(&reply, zero_frame);

	mock_reset(&mock);
	mock_add_read(&mock, NULL, 0, 0, 1);
	mock_add_read(&mock, frame_five, 29, HASHX_USB_PACKET_SIZE, 250);
	mock.reads[1].data[1] = 63;
	check(hashx_usb_status_query(&mock, mock_read, mock_write, &reply) == -1,
		"malformed declared chunk fails transaction before mapping");
	check(reply.length == 0 && mock.writes == 1, "malformed transaction has no output frame");
}

int main(void)
{
	test_captured_fixtures();
	test_all_fragment_boundaries();
	test_zero_load_overload_and_status_schema();
	test_ignored_packets();
	test_malformed_chunks_reset();
	test_overflow_and_recovery();
	test_query_discards_queued_old_reply();
	test_query_timeout_then_next_drain();
	test_query_short_and_failed_writes();
	test_query_negative_and_oversized_reads();
	test_query_bounded_drain_and_unknown_replies();
	test_query_zero_and_malformed_reply();
	printf("HashX USB protocol: %u checks, %u failures\n", checks, failures);
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
