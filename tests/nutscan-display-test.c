/* nutscan-display-test - USB duplicate suggestions in scanner output

   Copyright (C) 2026 Network UPS Tools project

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include "common.h"
#include "nut-scan.h"

#ifdef WIN32
# include <io.h>
# define dup _dup
# define dup2 _dup2
# define fileno _fileno
# define close _close
#else
# include <unistd.h>
#endif

static char *capture_output(void (*display)(nutscan_device_t *), nutscan_device_t *device)
{
	FILE *capture = tmpfile();
	int saved_stdout;
	long length;
	char *output;

	if (!capture) {
		fatal_with_errno(EXIT_FAILURE, "tmpfile");
	}
	fflush(stdout);
	saved_stdout = dup(fileno(stdout));
	if (saved_stdout < 0 || dup2(fileno(capture), fileno(stdout)) < 0) {
		fatal_with_errno(EXIT_FAILURE, "redirect stdout");
	}
	display(device);
	fflush(stdout);
	if (dup2(saved_stdout, fileno(stdout)) < 0) {
		fatal_with_errno(EXIT_FAILURE, "restore stdout");
	}
	close(saved_stdout);
	length = ftell(capture);
	if (length < 0) {
		fatal_with_errno(EXIT_FAILURE, "ftell");
	}
	output = xcalloc((size_t)length + 1, 1);
	rewind(capture);
	if (fread(output, 1, (size_t)length, capture) != (size_t)length) {
		fatal_with_errno(EXIT_FAILURE, "read captured output");
	}
	fclose(capture);
	return output;
}

static nutscan_device_t *new_device(const char *driver)
{
	nutscan_device_t *device = nutscan_new_device();

	if (!device) {
		fatal_with_errno(EXIT_FAILURE, "nutscan_new_device");
	}
	device->type = TYPE_USB;
	device->driver = xstrdup(driver);
	device->port = xstrdup("auto");
	nutscan_add_option_to_device(device, "vendorid", "0764");
	nutscan_add_option_to_device(device, "productid", "0601");
	nutscan_add_option_to_device(device, "product", "OR2200LCDRT2U");
	nutscan_add_option_to_device(device, "vendor", "CPS");
	return device;
}

/* Replace/remove one discovered attribute, retaining the scanner's
 * distinction between active selectors and commented observations.
 */
static void set_option(nutscan_device_t *device, const char *name, const char *value, int commented)
{
	nutscan_options_t **opt = &device->opt;

	while (*opt && strcmp((*opt)->option, name)) {
		opt = &(*opt)->next;
	}
	if (*opt) {
		nutscan_options_t *old = *opt;
		*opt = old->next;
		free(old->option);
		free(old->value);
		free(old->comment_tag);
		free(old);
	}
	if (value) {
		nutscan_add_commented_option_to_device(device, (char *)name, (char *)value,
			commented ? "" : NULL);
	}
}

static int check_case(const char *name, nutscan_device_t **devices, size_t count, unsigned int expected)
{
	char *before, *after, *output;
	size_t i;
	int failed = 0;
	nutscan_device_t *entry = devices[count - 1];

	before = capture_output(nutscan_display_parsable, entry);
	output = capture_output(nutscan_display_ups_conf, entry);
	if (expected && !strstr(output, "association may vary between runs### allow_duplicates\n")) {
		failed = 1;
	}
	free(output);
	/* A second renderer must not append a second suggestion. */
	output = capture_output(nutscan_display_ups_conf_with_sanity_check, entry);
	free(output);
	after = capture_output(nutscan_display_parsable, entry);
	if (strcmp(before, after)) {
		failed = 1;
	}
	free(before);
	free(after);

	for (i = 0; i < count; i++) {
		nutscan_options_t *opt;
		unsigned int suggestions = 0;
		for (opt = devices[i]->opt; opt; opt = opt->next) {
			if (opt->option && !strcmp(opt->option, "allow_duplicates") && opt->comment_tag) {
				suggestions++;
				if (opt->value) {
					failed = 1;
				}
			}
		}
		if (suggestions != ((expected >> i) & 1U)) {
			fprintf(stderr, "%s: device %" PRIuSIZE " expected %u suggestion(s), got %u\n",
				name, i, (expected >> i) & 1U, suggestions);
			failed = 1;
		}
	}
	nutscan_free_device(entry);
	printf("%s: %s\n", failed ? "FAIL" : "PASS", name);
	return failed;
}

int main(void)
{
	/* Each cases row sets or removes one observed attribute for a device pair.
	 * The expected bitmask selects which devices receive a suggestion:
	 * 0 = neither, 1 = first, 2 = second, 3 = both.
	 */
	static const struct {
		const char *name, *option, *first, *second;
		int commented;
		unsigned int expected;
	} cases[] = {
		{ "issue 2997 leading product space", "product", " CP 1500C", " CP 1500C", 0, 3 },
		/* Only the second (broader) configuration can match both observations. */
		{ "issue 2791 missing vendor", "vendor", "CPS", NULL, 0, 2 },
		{ "absent serials", "serial", NULL, NULL, 0, 3 },
		{ "duplicate serials", "serial", "000000", "000000", 0, 3 },
		{ "distinct serials", "serial", "unit-1", "unit-2", 0, 0 },
		{ "one absent serial", "serial", "unit-1", NULL, 0, 2 },
		{ "empty and absent serials", "serial", "", NULL, 0, 3 },
		{ "empty and nonempty serials", "serial", "", "unit-2", 0, 0 },
		{ "commented serials", "serial", "unit-1", "unit-2", 1, 3 },
		{ "distinct vendors", "vendor", "CPS", "other", 0, 0 },
		{ "distinct products", "product", "one", "two", 0, 0 },
		{ "distinct vendor IDs", "vendorid", "0764", "051d", 0, 0 },
		{ "distinct product IDs", "productid", "0601", "0602", 0, 0 },
		{ "case insensitive", "product", "UPS Model", "ups model", 0, 3 },
		{ "regex match", "product", "UPS.*", "UPS model", 0, 1 },
		{ "full string match", "product", "UPS", "UPS model", 0, 0 },
		{ "identical regex metacharacters", "product", "UPS (rack)", "UPS (rack)", 0, 3 },
		{ "invalid regex", "product", "[", "[", 0, 0 },
		{ "commented buses", "bus", "001", "002", 1, 3 },
		{ "active buses", "bus", "001", "002", 0, 0 },
		{ "commented devices", "device", "001", "002", 1, 3 },
		{ "active devices", "device", "001", "002", 0, 0 },
		{ "unknown devices", "device", ".*", ".*", 0, 3 },
		{ "commented bus ports", "busport", "001", "002", 1, 3 },
#if (defined WITH_USB_BUSPORT) && WITH_USB_BUSPORT
		{ "active bus ports", "busport", "001", "002", 0, 0 },
#else
		{ "unsupported bus ports", "busport", "001", "002", 0, 3 },
#endif
		{ "nonmatching bcdDevice", "bcdDevice", "0100", "0200", 1, 3 }
	};
	static const char *supported[] = {
		"usbhid-ups", "nutdrv_qx", "blazer_usb", "riello_usb",
		"tripplite_usb", "apcmicrolink"
	};
	static const char *unsupported[] = {
		"richcomm_usb", "apc_modbus", "bcmxcp_usb", "nutdrv_atcl_usb", "powervar_cx_usb", "unknown"
	};
	nutscan_device_t *devices[3];
	char *output;
	size_t i, j;
	int failed = 0;

	output = capture_output(nutscan_display_ups_conf, NULL);
	if (*output) {
		failed++;
	}
	free(output);

	for (i = 0; i < SIZEOF_ARRAY(cases); i++) {
		for (j = 0; j < 2; j++) {
			devices[j] = new_device("usbhid-ups");
			set_option(devices[j], cases[i].option,
				j ? cases[i].second : cases[i].first, cases[i].commented);
		}
		nutscan_add_device_to_device(devices[0], devices[1]);
		failed += check_case(cases[i].name, devices, 2, cases[i].expected);
	}
	for (i = 0; i < SIZEOF_ARRAY(supported) + SIZEOF_ARRAY(unsupported); i++) {
		const char *driver = i < SIZEOF_ARRAY(supported)
			? supported[i] : unsupported[i - SIZEOF_ARRAY(supported)];
		devices[0] = new_device(driver);
		devices[1] = new_device(driver);
		nutscan_add_device_to_device(devices[0], devices[1]);
		failed += check_case(driver, devices, 2, i < SIZEOF_ARRAY(supported) ? 3 : 0);
	}
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 3; j++) {
			devices[j] = new_device("usbhid-ups");
			set_option(devices[j], "serial", j == 2 ? "unique" : "shared", 0);
		}
		nutscan_add_device_to_device(devices[i ? 2 : 0], devices[1]);
		nutscan_add_device_to_device(devices[1], devices[i ? 0 : 2]);
		failed += check_case(i ? "reversed group" : "three device group", devices, 3, 3);
	}
	devices[0] = new_device("usbhid-ups");
	failed += check_case("single device", devices, 1, 0);
	devices[0] = new_device("usbhid-ups");
	devices[1] = new_device("usbhid-ups");
	set_option(devices[0], "product", " CP 1500C", 0);
	set_option(devices[1], "product", " CP 1500C", 0);
	set_option(devices[0], "serial", "unit-1", 0);
	set_option(devices[1], "serial", "unit-2", 0);
	nutscan_add_device_to_device(devices[0], devices[1]);
	failed += check_case("leading product space with distinct serials", devices, 2, 0);
	devices[0] = new_device("usbhid-ups");
	devices[1] = new_device("nutdrv_qx");
	nutscan_add_device_to_device(devices[0], devices[1]);
	failed += check_case("different drivers", devices, 2, 0);
	devices[0] = new_device("usbhid-ups");
	devices[1] = new_device("usbhid-ups");
	devices[0]->type = TYPE_SNMP;
	nutscan_add_device_to_device(devices[0], devices[1]);
	failed += check_case("mixed discovery types", devices, 2, 0);
	devices[0] = new_device("usbhid-ups");
	devices[1] = new_device("usbhid-ups");
	nutscan_add_option_to_device(devices[0], "allow_duplicates", NULL);
	nutscan_add_device_to_device(devices[0], devices[1]);
	failed += check_case("existing active flag", devices, 2, 2);

	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
