/* getvaluetest - check that the bitness/endianness dependent conversions
 * of representation of numeric types between wire protocols and different
 * CPU computations produce expected results.
 *
 * See also:
 *  https://github.com/networkupstools/nut/pull/1055
 *  https://github.com/networkupstools/nut/pull/1040
 *  https://github.com/networkupstools/nut/pull/1024
 *  https://github.com/networkupstools/nut/issues/1023
 *
 * Copyright (C)
 *      2021    Nick Briggs <nicholas.h.briggs@gmail.com>
 *      2022    Jim Klimov <jimklimov+nut@gmail.com>
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

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hidtypes.h"
#include "usb-common.h"
#include "common.h"

void GetValue(const unsigned char *Buf, HIDData_t *pData, long *pValue);

static void Usage(char *name) {
	printf("%s [<buf> <offset> <size> <min> <max> <expect>]\n", name);
	printf("  <buf>     - string of hex digit pairs, space separated\n");
	printf("  <offset>  - offset of the report item value in bits, typically 0..31\n");
	printf("  <size>    - size of the report item value in bits:  typically 1..32\n");
	printf("  <min>     - logical minimum value for report item\n");
	printf("  <max>     - logical maximum value for report item\n");
	printf("  <expect>  - expected value\n");
	printf("\n");
	printf("%s \"0c 64 11 0d\" 8 16 0 65535 3345\n", name);
	printf("\nIf no arguments are given a builtin set of tests are run.\n");
}

static void PrintBufAndData(uint8_t *buf, size_t bufSize, HIDData_t *pData) {
	size_t i;

	printf("buf \"");
	for (i = 0; i < bufSize - 1; i++) {
		printf("%02x ", buf[i]);
	}
	printf("%02x\"", buf[bufSize - 1]);
	printf(" offset %u size %u logmin %ld (0x%lx) logmax %ld (0x%lx)",
		pData->Offset, pData->Size, pData->LogMin, pData->LogMin, pData->LogMax, pData->LogMax);
}

static int RunBuiltInTests(char *argv[]) {
	NUT_UNUSED_VARIABLE(argv);

	int exitStatus = 0;
	size_t i;
	char *next;
	uint8_t reportBuf[64];
	size_t bufSize;
	HIDData_t data;
	long value;

	static struct {
		char *buf;			/* item data, starts with report id byte, then remaining report bytes */
		uint8_t Offset;		/* item offset in bits, typically 0..31 */
		uint8_t Size;		/* item size in bits, typically 1..32 */
		long LogMin, LogMax;	/* logical minimum and maximum values */
		long expectedValue;	/* the expected result of decoding the value in the buffer */
	} testData[] = {
		{.buf = "00 ff ff ff ff", .Offset = 0, .Size = 32, .LogMin = -1, .LogMax = 2147483647, .expectedValue = -1},
		{.buf = "00 ff", .Offset = 0, .Size = 8, .LogMin = -1, .LogMax = 127, .expectedValue = -1},
		{.buf = "00 ff", .Offset = 0, .Size = 8, .LogMin = 0, .LogMax = 127, .expectedValue = 127},
		{.buf = "00 ff", .Offset = 0, .Size = 8, .LogMin = 0, .LogMax = 255, .expectedValue = 255},
		{.buf = "33 00 0a 08 80", .Offset = 0, .Size = 32, .LogMin = 0, .LogMax = 65535, .expectedValue = 2560},
		{.buf = "00 00 08 00 00", .Offset = 0, .Size = 32, .LogMin = 0, .LogMax = 65535, .expectedValue = 2048},
		{.buf = "06 00 00 08", .Offset = 0, .Size = 8, .LogMin = 0, .LogMax = 255, .expectedValue = 0},
		{.buf = "06 00 00 08", .Offset = 8, .Size = 8, .LogMin = 0, .LogMax = 255, .expectedValue = 0},
		{.buf = "06 00 00 08", .Offset = 16, .Size = 8, .LogMin = 0, .LogMax = 255, .expectedValue = 8},
		{.buf = "16 0c 00 00 00", .Offset = 0, .Size = 1, .LogMin = 0, .LogMax = 1, .expectedValue =  0},
		{.buf = "16 0c 00 00 00", .Offset = 1, .Size = 1, .LogMin = 0, .LogMax = 1, .expectedValue =  0},
		{.buf = "16 0c 00 00 00", .Offset = 2, .Size = 1, .LogMin = 0, .LogMax = 1, .expectedValue =  1},
		{.buf = "16 0c 00 00 00", .Offset = 3, .Size = 1, .LogMin = 0, .LogMax = 1, .expectedValue =  1},
		{.buf = "16 0c 00 00 00", .Offset = 4, .Size = 1, .LogMin = 0, .LogMax = 1, .expectedValue =  0},
		{.buf = "16 0c 00 00 00", .Offset = 5, .Size = 1, .LogMin = 0, .LogMax = 1, .expectedValue =  0},
		{.buf = "16 0c 00 00 00", .Offset = 6, .Size = 1, .LogMin = 0, .LogMax = 1, .expectedValue =  0},
		{.buf = "16 0c 00 00 00", .Offset = 7, .Size = 1, .LogMin = 0, .LogMax = 1, .expectedValue =  0},
		{.buf = "16 0c 00 00 00", .Offset = 8, .Size = 1, .LogMin = 0, .LogMax = 1, .expectedValue =  0},
		{.buf = "16 0c 00 00 00", .Offset = 9, .Size = 1, .LogMin = 0, .LogMax = 1, .expectedValue =  0},
		{.buf = "16 0c 00 00 00", .Offset = 10, .Size = 1, .LogMin = 0, .LogMax = 1, .expectedValue =  0}
	};

	for (i = 0; i < sizeof(testData)/sizeof(testData[0]); i++) {
		next = testData[i].buf;
		for (bufSize = 0; *next != 0; bufSize++) {
			reportBuf[bufSize] = (uint8_t) strtol(next, (char **)&next, 16);
		}
		memset((void *)&data, 0, sizeof(data));
		data.Offset = testData[i].Offset;
		data.Size = testData[i].Size;
		data.LogMin = testData[i].LogMin;
		data.LogMax = testData[i].LogMax;

		GetValue(reportBuf, &data, &value);

		printf("Test #%zd ", i + 1);
		PrintBufAndData(reportBuf, bufSize,  &data);
		if (value == testData[i].expectedValue) {
			printf(" value %ld PASS\n", value);
		} else {
			printf(" value %ld FAIL expected %ld\n", value, testData[i].expectedValue);
			exitStatus = 1;
		}
	}

	/* Emulate rdlen calculations in libusb{0,1}.c or
	 * langid calculations in nutdrv_qx.c; in these
	 * cases we take two bytes (cast from usb_ctrl_char
	 * type, may be signed depending on used API version)
	 * from the protocol buffer, and build a platform
	 * dependent representation of a two-byte word.
	 */
	usb_ctrl_char	bufC[2];
	signed char	bufS[2];
	unsigned char	bufU[2];
	int rdlen;

	/* Example from issue https://github.com/networkupstools/nut/issues/1261
	 * where resulting length 0x01a9 should be "425" but ended up "-87" */
	bufC[0] = (usb_ctrl_char)0xa9;
	bufC[1] = (usb_ctrl_char)0x01;

	bufS[0] = (signed char)0xa9;
	bufS[1] = (signed char)0x01;

	bufU[0] = (unsigned char)0xa9;
	bufU[1] = (unsigned char)0x01;

	/* Check different conversion methods and hope current build CPU,
	 * C implementation etc. do not mess up bit-shifting vs rotation,
	 * zeroing high bits, int type width extension et al. If something
	 * is mismatched below, the production NUT code may need adaptations
	 * for that platform to count stuff correctly!
	 */
	printf("\nTesting bit-shifting approaches used in codebase to get 2-byte int lengths from wire bytes:\n");
	printf("(Expected correct value is '425', incorrect '-87' or other)\n");

#define REPORT_VERDICT(expected) { if (expected) { printf(" - PASS\n"); } else { printf(" - FAIL\n"); exitStatus = 1; } }

	rdlen = bufC[0] | (bufC[1] << 8);
	printf(" * reference: no casting, usb_ctrl_char :\t%d\t(depends on libusb API built against)", rdlen);
	REPORT_VERDICT (rdlen == 425 || rdlen == -87)

	rdlen = bufS[0] | (bufS[1] << 8);
	printf(" * reference: no casting, signed char   :\t%d\t(expected '-87' here)", rdlen);
	REPORT_VERDICT (rdlen == -87)

	rdlen = bufU[0] | (bufU[1] << 8);
	printf(" * reference: no casting, unsigned char :\t%d\t(expected '425')", rdlen);
	REPORT_VERDICT (rdlen == 425)


	rdlen = (uint8_t)bufC[0] | ((uint8_t)bufC[1] << 8);
	printf(" * uint8_t casting, usb_ctrl_char :\t%d", rdlen);
	REPORT_VERDICT (rdlen == 425)

	rdlen = (uint8_t)bufS[0] | ((uint8_t)bufS[1] << 8);
	printf(" * uint8_t casting, signed char   :\t%d", rdlen);
	REPORT_VERDICT (rdlen == 425)

	rdlen = (uint8_t)bufU[0] | ((uint8_t)bufU[1] << 8);
	printf(" * uint8_t casting, unsigned char :\t%d", rdlen);
	REPORT_VERDICT (rdlen == 425)


	rdlen = ((uint8_t)bufC[0]) | (((uint8_t)bufC[1]) << 8);
	printf(" * uint8_t casting with parentheses, usb_ctrl_char :\t%d", rdlen);
	REPORT_VERDICT (rdlen == 425)

	rdlen = ((uint8_t)bufS[0]) | (((uint8_t)bufS[1]) << 8);
	printf(" * uint8_t casting with parentheses, signed char   :\t%d", rdlen);
	REPORT_VERDICT (rdlen == 425)

	rdlen = ((uint8_t)bufU[0]) | (((uint8_t)bufU[1]) << 8);
	printf(" * uint8_t casting with parentheses, unsigned char :\t%d", rdlen);
	REPORT_VERDICT (rdlen == 425)


	rdlen = ((uint16_t)(bufC[0]) & 0x00FF) | (((uint16_t)(bufC[1]) & 0x00FF) << 8);
	printf(" * uint16_t casting with 8-bit mask, usb_ctrl_char :\t%d", rdlen);
	REPORT_VERDICT (rdlen == 425)

	rdlen = ((uint16_t)(bufS[0]) & 0x00FF) | (((uint16_t)(bufS[1]) & 0x00FF) << 8);
	printf(" * uint16_t casting with 8-bit mask, signed char   :\t%d", rdlen);
	REPORT_VERDICT (rdlen == 425)

	rdlen = ((uint16_t)(bufU[0]) & 0x00FF) | (((uint16_t)(bufU[1]) & 0x00FF) << 8);
	printf(" * uint16_t casting with 8-bit mask, unsigned char :\t%d", rdlen);
	REPORT_VERDICT (rdlen == 425)


	rdlen = 256 * (uint8_t)(bufC[1]) + (uint8_t)(bufC[0]);
	printf(" * uint8_t casting with multiplication, usb_ctrl_char :\t%d", rdlen);
	REPORT_VERDICT (rdlen == 425)

	rdlen = 256 * (uint8_t)(bufS[1]) + (uint8_t)(bufS[0]);
	printf(" * uint8_t casting with multiplication, signed char   :\t%d", rdlen);
	REPORT_VERDICT (rdlen == 425)

	rdlen = 256 * (uint8_t)(bufU[1]) + (uint8_t)(bufU[0]);
	printf(" * uint8_t casting with multiplication, unsigned char :\t%d", rdlen);
	REPORT_VERDICT (rdlen == 425)


	return (exitStatus);
}

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

int main (int argc, char *argv[]) {
	int status;

	switch (argc) {
	case 1:
		status = RunBuiltInTests(argv);
		break;
	case 7:
		status = RunCommandLineTest(argv);
		break;
	default:
		Usage(argv[0]);
		status = 2;
	}
	return(status);
}
