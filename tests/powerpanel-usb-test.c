/* USB text framing regression tests: fake transfers, no device access.
 * This program is distributed under the GNU GPL version 2 or later.
 */
#include "common.h"
#include "../drivers/powerp-usb.c"

static int failed, input_index, output_count, timeout_mode, truncated_mode;
static const unsigned char *fragments[4];
static size_t fragment_sizes[4];

int LIBUSB_CALL libusb_interrupt_transfer(libusb_device_handle *device,
	unsigned char endpoint, unsigned char *data, int length,
	int *transferred, unsigned int timeout)
{
	NUT_UNUSED_VARIABLE(device);
	NUT_UNUSED_VARIABLE(timeout);
	if (endpoint == 0x02) {
		output_count++;
		if (length != 64 || data[0] != 0x29 || data[1] != 2
		|| data[2] != 'D' || data[3] != '\r') {
			failed++;
		}
		*transferred = length;
		return 0;
	}
	if (timeout_mode || input_index >= 4 || !fragments[input_index]) {
		*transferred = 0;
		return LIBUSB_ERROR_TIMEOUT;
	}
	memset(data, 0, (size_t)length);
	data[0] = 0x28;
	data[1] = (unsigned char)fragment_sizes[input_index];
	memcpy(data+2, fragments[input_index], fragment_sizes[input_index]);
	*transferred = truncated_mode ? 2 : 64;
	input_index++;
	return 0;
}

static void check(int condition, const char *name)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", name);
		failed++;
	}
}

int main(void)
{
	char result[128];
	const char *forbidden[] = {"C\r", "C47:261024000000\r", "T\r", "S01\r", "Z01\r", "D\rC\r", NULL};
	size_t i;
	handle = (libusb_device_handle *)(uintptr_t)1;
	fragments[0] = (const unsigned char *)"#I233O230Q0.";
	fragment_sizes[0] = strlen((const char *)fragments[0]);
	fragments[1] = (const unsigned char *)"7J149E191S\x80\x80\r";
	fragment_sizes[1] = strlen((const char *)fragments[1]);
	check(powerpanel_usb_command("D\r", result, sizeof(result)) > 0, "fragmented response accepted");
	check(strcmp(result, "#I233O230Q0.7J149E191S\x80\x80") == 0, "fragments reassembled exactly");
	for (i = 0; forbidden[i]; i++) {
		int before = output_count;
		check(powerpanel_usb_command(forbidden[i], result, sizeof(result)) < 0, "control command rejected");
		check(output_count == before, "rejected command never sent");
	}
	input_index = 0;
	truncated_mode = 1;
	check(powerpanel_usb_command("D\r", result, sizeof(result)) < 0, "truncated frame rejected");
	truncated_mode = 0;
	input_index = 0;
	check(powerpanel_usb_command("D\r", result, 4) < 0, "response overflow rejected");
	timeout_mode = 1;
	check(powerpanel_usb_command("D\r", result, sizeof(result)) < 0, "timeout bounded");
	handle = NULL;
	check(powerpanel_usb_command("D\r", result, sizeof(result)) < 0, "disconnected handle rejected");
	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
