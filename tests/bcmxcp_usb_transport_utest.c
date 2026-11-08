/* Reproduce BCM/XCP USB multi-frame assembly when a read crosses frames. */

#include "config.h"

#include <stdio.h>
#include <string.h>

static unsigned char usb_chunks[2][128];
static int usb_chunk_lengths[2];
static size_t usb_chunk_index;

#define libusb_interrupt_transfer bcmxcp_test_interrupt_transfer
#define libusb_clear_halt bcmxcp_test_clear_halt
#include "bcmxcp_usb.c"
#undef libusb_interrupt_transfer
#undef libusb_clear_halt

#include "bcmxcp.c"

int LIBUSB_CALL bcmxcp_test_interrupt_transfer(
	libusb_device_handle *dev_handle, unsigned char endpoint,
	unsigned char *data, int length, int *actual_length,
	unsigned int timeout)
{
	NUT_UNUSED_VARIABLE(dev_handle);
	NUT_UNUSED_VARIABLE(endpoint);
	NUT_UNUSED_VARIABLE(timeout);

	if (usb_chunk_index >= 2)
		return LIBUSB_ERROR_TIMEOUT;

	if (usb_chunk_lengths[usb_chunk_index] > length)
		return LIBUSB_ERROR_OVERFLOW;

	memcpy(data, usb_chunks[usb_chunk_index],
		(size_t)usb_chunk_lengths[usb_chunk_index]);
	*actual_length = usb_chunk_lengths[usb_chunk_index];
	usb_chunk_index++;
	return LIBUSB_SUCCESS;
}

int LIBUSB_CALL bcmxcp_test_clear_halt(
	libusb_device_handle *dev_handle, unsigned char endpoint)
{
	NUT_UNUSED_VARIABLE(dev_handle);
	NUT_UNUSED_VARIABLE(endpoint);
	return LIBUSB_SUCCESS;
}

int main(void)
{
	unsigned char frame1[126] = { PW_COMMAND_START_BYTE, 0x01, 121, 0x01 };
	unsigned char frame2[45] = { PW_COMMAND_START_BYTE, 0x01, 40, 0x82 };
	unsigned char answer[PW_CMD_BUFSIZE] = { 0 };
	unsigned char checksum = 0;
	ssize_t result;
	size_t i;

	frame1[4] = PW_ID_BLOCK_REQ;
	for (i = 5; i < sizeof(frame1) - 1; i++)
		frame1[i] = (unsigned char)i;
	for (i = 0; i < sizeof(frame1) - 1; i++)
		checksum -= frame1[i];
	frame1[sizeof(frame1) - 1] = checksum;

	checksum = 0;
	for (i = 4; i < sizeof(frame2) - 1; i++)
		frame2[i] = (unsigned char)(0x80U + i);
	for (i = 0; i < sizeof(frame2) - 1; i++)
		checksum -= frame2[i];
	frame2[sizeof(frame2) - 1] = checksum;

	memcpy(usb_chunks[0], frame1, sizeof(frame1));
	memcpy(usb_chunks[0] + sizeof(frame1), frame2, 2);
	usb_chunk_lengths[0] = 128;
	memcpy(usb_chunks[1], frame2 + 2, sizeof(frame2) - 2);
	usb_chunk_lengths[1] = (int)(sizeof(frame2) - 2);

	upsdev = (usb_dev_handle *)(uintptr_t)1;
	result = get_answer(answer, PW_ID_BLOCK_REQ);

	if (result != 161) {
		fprintf(stderr,
			"expected 161 assembled payload bytes, got %zd after %zu USB chunks\n",
			result, usb_chunk_index);
		return EXIT_FAILURE;
	}

	if (memcmp(answer, frame1 + 4, 121) != 0 ||
		memcmp(answer + 121, frame2 + 4, 40) != 0) {
		fprintf(stderr, "assembled payload differs from the two source frames\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
