/* getexponenttest-belkin-hid - check detection of correct multiplication
 * exponent value for miniscule readings from USB HID (as used in the
 * drivers/belkin-hid.c subdriver of usbhid-ups).
 * Eventually may be extended to similar tests for other drivers' methods,
 * or more likely cloned to #include them in similar fashion.
 *
 * See also:
 *  https://github.com/networkupstools/nut/issues/2370
 *
 * Copyright (C)
 *      2024   Jim Klimov <jimklimov+nut@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "config.h"

#include "nut_stdint.h"
#include "nut_float.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

#include "belkin-hid.c"
/* from drivers/belkin-hid.c we test:
extern double liebert_config_voltage_mult, liebert_line_voltage_mult;
const char *liebert_config_voltage_fun(double value);
const char *liebert_line_voltage_fun(double value);
 */

static void Usage(char *name) {
/*
	printf("%s {-c <config_voltage> | -l <line_voltage>} <expected_multiplier> <expected_voltage>\n", name);
	printf("\n");
	printf("%s -c 12\n", name);
	printf("%s -l 0.001212\n", name);
	printf("%s -l 1.39e-06\n", name);
*/
	printf("%s\nIf no arguments are given a builtin set of tests are run.\n", name);
}

static int RunBuiltInTests(char *argv[]) {
	int exitStatus = 0;
	size_t i;

	double	rawValue, value, mult;
	const char	*valueStr;

	static char* methodName[2] = {
		"liebert_config_voltage_mult()   ",
		"liebert_line_voltage_mult()     "
	};

	static struct {
		char *buf;			/* raw voltage as a string (from CLI input, e.g. NUT driver log trace) */
		double expectedRawValue;	/* parsed raw voltage (as seen in USB HID reports) */
		char type;			/* 1 = config, 2 = line */
		double expectedMult;		/* expected liebert_config_voltage_mult or liebert_line_voltage_mult */
		double expectedValue;		/* the expected result of decoding the value in the buffer */
	} testData[] = {
		{.buf = "0.000273",	.expectedRawValue = 0.000273,	.type = 2, .expectedMult = 1e5, .expectedValue = 27.3	},
		{.buf = "0.001212",	.expectedRawValue = 0.001212,	.type = 2, .expectedMult = 1e5, .expectedValue = 121.2	},
		{.buf = "0.002456",	.expectedRawValue = 0.002456,	.type = 2, .expectedMult = 1e5, .expectedValue = 245.6	},
		{.buf = "0.003801",	.expectedRawValue = 0.003801,	.type = 2, .expectedMult = 1e5, .expectedValue = 380.1	},
		{.buf = "0.004151",	.expectedRawValue = 0.004151,	.type = 2, .expectedMult = 1e5, .expectedValue = 415.1	},

		{.buf = "1.39e-06",	.expectedRawValue = 0.00000139,	.type = 2, .expectedMult = 1e7, .expectedValue = 13.9	},
		{.buf = "1.273e-05",	.expectedRawValue = 0.00001273,	.type = 2, .expectedMult = 1e7, .expectedValue = 127.3	},
		{.buf = "2.201e-05",	.expectedRawValue = 0.00002201,	.type = 2, .expectedMult = 1e7, .expectedValue = 220.1	},
		{.buf = "4.201e-05",	.expectedRawValue = 0.00004201,	.type = 2, .expectedMult = 1e7, .expectedValue = 420.1	},

		/* Edge cases - what should not be converted (good enough already) */
		{.buf = "12",	.expectedRawValue = 12.0,	.type = 2, .expectedMult = 1, .expectedValue = 12.0	},
		{.buf = "12.3",	.expectedRawValue = 12.3,	.type = 2, .expectedMult = 1, .expectedValue = 12.3	},
		{.buf = "232.1",	.expectedRawValue = 232.1,	.type = 2, .expectedMult = 1, .expectedValue = 232.1	},
		{.buf = "240",	.expectedRawValue = 240.0,	.type = 2, .expectedMult = 1, .expectedValue = 240.0	},

		/* Config values (nominal battery/input/... voltage) are often integers: */
		{.buf = "24",	.expectedRawValue = 24.0,	.type = 1, .expectedMult = 1, .expectedValue = 24.0	},
		{.buf = "120",	.expectedRawValue = 120.0,	.type = 1, .expectedMult = 1, .expectedValue = 120.0	}
	};

	NUT_UNUSED_VARIABLE(argv);

	for (i = 0; i < SIZEOF_ARRAY(testData); i++) {
		liebert_line_voltage_mult = 1.0;
		liebert_config_voltage_mult = 1.0;

		rawValue = strtod(testData[i].buf, NULL);
		if (!d_equal(rawValue, testData[i].expectedRawValue)) {
			printf(" value '%s' parsing FAIL: got %g expected %g\n",
				testData[i].buf, rawValue, testData[i].expectedRawValue);
			/* Fix testData definition! */
			exitStatus = 1;
		}

		switch (testData[i].type) {
			case 1:
				valueStr = liebert_config_voltage_fun(rawValue);
				mult = liebert_config_voltage_mult;
				/* NOTE: The method does also set a default
				 * liebert_line_voltage_mult if config voltage
				 * is miniscule and not a plain integer */
				break;

			case 2:
				valueStr = liebert_line_voltage_fun(rawValue);
				mult = liebert_line_voltage_mult;
				break;

			default:
				printf(" invalid entry\n");
				continue;
		}

		printf("Test #%" PRIiSIZE "  \t", i + 1);
		value = strtod(valueStr, NULL);
		if (d_equal(value, testData[i].expectedValue) && d_equal(mult, testData[i].expectedMult)) {
			printf("%s\tGOT value %9g\tmult %6g PASS\n",
				(testData[i].type < 1 || testData[i].type > 3 ? "<null>" : methodName[testData[i].type - 1]),
				value, mult);
		} else {
			printf("%s\tGOT value %9g\tmult %6g FAIL"
				"\tEXPECTED v=%7g\tm=%7g"
				"\tORIGINAL (string)'%s'\t=> (double)%g\n",
				(testData[i].type < 1 || testData[i].type > 2 ? "<null>" : methodName[testData[i].type - 1]),
				value, mult,
				testData[i].expectedValue,
				testData[i].expectedMult,
				testData[i].buf, rawValue);
			exitStatus = 1;
		}
	}

	return (exitStatus);
}

/*
static int RunCommandLineTest(char *argv[]) {
	uint8_t reportBuf[64];
	size_t bufSize;
	char *start, *end;
	HIDData_t data;
	long value, expectedValue;

	start = argv[1];
	end = NULL;
	for (bufSize = 0; *start != 0; bufSize++) {
		reportBuf[bufSize] = (uint8_t) strtol(start, (char **)&end, 16);
		if (start == end) break;
		start = end;
	}
	memset((void *)&data, 0, sizeof(data));
	data.Offset = (uint8_t) atoi(argv[2]);
	data.Size = (uint8_t) atoi(argv[3]);
	data.LogMin = strtol(argv[4], 0, 0);
	data.LogMax = strtol(argv[5], 0, 0);
	expectedValue = strtol(argv[6], 0, 0);

	GetValue(reportBuf, &data, &value);

	printf("Test #0 ");
	PrintBufAndData(reportBuf, bufSize, &data);
	if (value == expectedValue) {
		printf(" value %ld PASS\n", value);
		return (0);
	} else {
		printf(" value %ld FAIL expected %ld\n", value, expectedValue);
		return (1);
	}
}
*/

int main (int argc, char *argv[]) {
	int status;

	switch (argc) {
	case 1:
		status = RunBuiltInTests(argv);
		break;
/*
	case 7:
		status = RunCommandLineTest(argv);
		break;
*/
	default:
		Usage(argv[0]);
		status = 2;
	}
	return(status);
}
