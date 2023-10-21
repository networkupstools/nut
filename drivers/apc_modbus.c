/*  apc_modbus.c - Driver for APC Modbus UPS
 *  Copyright © 2023 Axel Gembe <axel@gembe.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "main.h"
#if defined NUT_MODBUS_HAS_USB
#include "nut_libusb.h"
#include "libhid.h"
#include "hidparser.h"
#endif /* defined NUT_MODBUS_HAS_USB */
#include "timehead.h"
#include "nut_stdint.h"

#include <ctype.h>
#include <stdio.h>

#include <modbus.h>

#define DRIVER_NAME "NUT APC Modbus driver"
#define DRIVER_VERSION "0.01"

#if defined NUT_MODBUS_HAS_USB

/* APC */
#define APC_VENDORID 0x051D

/* USB IDs device table */
static usb_device_id_t apc_modbus_usb_device_table[] = {
	{ USB_DEVICE(APC_VENDORID, 0x0003), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};

#endif /* defined NUT_MODBUS_HAS_USB */

/* Constants */
static const uint8_t modbus_default_slave_id = 1;

static const int modbus_rtu_default_baudrate = 9600;
static const char modbus_rtu_default_parity = 'N';
static const int modbus_rtu_default_databits = 8;
static const int modbus_rtu_default_stopbits = 1;

static const uint16_t modbus_tcp_default_port = 502;

#if defined NUT_MODBUS_HAS_USB
static const HIDNode_t modbus_rtu_usb_usage_rx = 0xff8600fc;
static const HIDNode_t modbus_rtu_usb_usage_tx = 0xff8600fd;
#endif /* defined NUT_MODBUS_HAS_USB */

/* Variables */
static modbus_t *modbus_ctx = NULL;
#if defined NUT_MODBUS_HAS_USB
static USBDevice_t usbdevice;
static USBDeviceMatcher_t *reopen_matcher = NULL;
static USBDeviceMatcher_t *regex_matcher = NULL;
static USBDeviceMatcher_t *best_matcher = NULL;
static int is_usb = 0;
#endif /* defined NUT_MODBUS_HAS_USB */
static int is_open = 0;
static double power_nominal;
static double realpower_nominal;

/* Function declarations */
static int _apc_modbus_read_inventory(void);

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Axel Gembe <axel@gembe.net>\n",
	DRV_BETA,
	{NULL}
};

typedef enum {
	APC_MODBUS_VALUE_TYPE_INT = 0,
	APC_MODBUS_VALUE_TYPE_UINT,
	APC_MODBUS_VALUE_TYPE_STRING
} apc_modbus_value_types;

static const apc_modbus_value_types apc_modbus_value_types_max = APC_MODBUS_VALUE_TYPE_STRING;

typedef struct {
	apc_modbus_value_types type;
	const char *format;
	int scale;
	void *variable_ptr;
	union {
		int64_t int_value;
		uint64_t uint_value;
		char *string_value;
	} data;
} apc_modbus_value_t;

typedef struct {
	int (*apc_to_nut)(const apc_modbus_value_t *value, char *output, size_t output_len);
	int (*nut_to_apc)(const char *value, uint16_t *output, size_t output_len);
} apc_modbus_converter_t;

/* Converts a Modbus string to a C string.
 * See MPAO-98KJ7F_EN section 1.3.3 Strings.
 *
 * Needs to remove leading zeroes, trailing spaces and translate characters in
 * the range 0x20-0x7e as a different character.
 * Zero termination is optional, so the output buffer needs to be 1 byte
 * longer to add zero termination. */
static int _apc_modbus_to_string(const uint16_t *value, const size_t value_len, char *output, size_t output_len)
{
	char *op;
	size_t i;

	if (value == NULL || output == NULL) {
		/* Invalid parameters */
		return 0;
	}

	if ((value_len * sizeof(uint16_t)) + 1 > output_len) {
		/* Output buffer too small */
		return 0;
	}

	op = output;

	/* Find first non-zero register */
	for (i = 0; i < value_len; i++) {
		if (value[i] != 0)
			break;
	}

	if (i < value_len) {
		/* The second character could be zero, skip if it is */
		if (((value[i] & 0xff00) >> 8) == 0) {
			*(op++) = (char)(value[i] & 0xff);
			i++;
		}

		/* For each remaining register, take MSB then LSB into string */
		for (; i < value_len; i++) {
			*(op++) = (char)((value[i] & 0xff00) >> 8);
			*(op++) = (char)(value[i] & 0xff);
		}
	}

	/* Add zero termination */
	*op = 0;

	assert(op < output + output_len);

	/* Find last non-zero/space */
	for (op--; op > output && (*op == 0 || *op == ' '); op--);

	/* Cut off rest of string */
	*(op + 1) = 0;

	/* Translate charaters outside of 0x20-0x7e range to spaces */
	for (; op > output; op--) {
		if (*op < 0x20 || *op > 0x7e) {
			*op = ' ';
		}
	}

	return 1;
}

/* Converts a Modbus integer to a uint64_t.
 * See MPAO-98KJ7F_EN section 1.3.1 Numbers. */
static int _apc_modbus_to_uint64(const uint16_t *value, const size_t value_len, uint64_t *output)
{
	size_t i;

	if (value == NULL || output == NULL) {
		/* Invalid parameters */
		return 0;
	}

	*output = 0;
	for (i = 0; i < value_len; i++) {
		*output = (*output << 16) | value[i];
	}

	return 1;
}

static int _apc_modbus_to_int64(const uint16_t *value, const size_t value_len, int64_t *output)
{
	size_t shiftval;

	if (!_apc_modbus_to_uint64(value, value_len, (uint64_t*)output)) {
		return 0;
	}

	shiftval = (8 * (sizeof(int64_t) - (value_len * sizeof(uint16_t))));
	*output <<= shiftval;
	*output >>= shiftval;

	return 1;
}

static int _apc_modbus_to_double(const apc_modbus_value_t *value, double *output)
{
	int factor;

	if (value == NULL || output == NULL) {
		/* Invalid parameters */
		return 0;
	}

	factor = 1 << value->scale;

	assert(value->type <= apc_modbus_value_types_max);
	switch (value->type) {
		case APC_MODBUS_VALUE_TYPE_INT:
			*output = (double)value->data.int_value / factor;
			break;
		case APC_MODBUS_VALUE_TYPE_UINT:
			*output = (double)value->data.uint_value / factor;
			break;
		case APC_MODBUS_VALUE_TYPE_STRING:
			return 0;
	}

	return 1;
}

static int _apc_modbus_double_to_nut(const apc_modbus_value_t *value, char *output, size_t output_len)
{
	double double_value;
	const char *format;
	int res;

	if (output == NULL || output_len == 0) {
		/* Invalid parameters */
		return 0;
	}

	if (!_apc_modbus_to_double(value, &double_value)) {
		return 0;
	}

	if (value->variable_ptr) {
		*((double*)value->variable_ptr) = double_value;
	}

	format = "%f";
	if (value->format != NULL)
		format = value->format;

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
	res = snprintf(output, output_len, format, double_value);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	if (res < 0 || (size_t)res > output_len) {
		return 0;
	}

	return 1;
}

static apc_modbus_converter_t _apc_modbus_double_conversion = { _apc_modbus_double_to_nut, NULL };

static int _apc_modbus_power_to_nut(const apc_modbus_value_t *value, char *output, size_t output_len)
{
	double double_value;
	const char *format;
	int res;

	if (output == NULL || output_len == 0) {
		/* Invalid parameters */
		return 0;
	}

	if (!_apc_modbus_to_double(value, &double_value)) {
		return 0;
	}

	if (value->variable_ptr) {
		/* variable_ptr points to the nominal value */
		double_value = (double_value / 100.0) * *((double*)value->variable_ptr);
	}

	format = "%f";
	if (value->format != NULL)
		format = value->format;

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
	res = snprintf(output, output_len, format, double_value);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	if (res < 0 || (size_t)res > output_len) {
		return 0;
	}

	return 1;
}

static apc_modbus_converter_t _apc_modbus_power_conversion = { _apc_modbus_power_to_nut, NULL };

static int _apc_modbus_voltage_to_nut(const apc_modbus_value_t *value, char *output, size_t output_len)
{
	if (value == NULL || output == NULL || output_len == 0) {
		/* Invalid parameters */
		return 0;
	}

	if (value->type != APC_MODBUS_VALUE_TYPE_UINT) {
		return 0;
	}

	if (value->data.uint_value == 0xffff) {
		/* Not applicable */
		strncpy(output, "NA", output_len);
		return 1;
	}

	return _apc_modbus_double_to_nut(value, output, output_len);
}

static apc_modbus_converter_t _apc_modbus_voltage_conversion = { _apc_modbus_voltage_to_nut, NULL };

static int _apc_modbus_efficiency_to_nut(const apc_modbus_value_t *value, char *output, size_t output_len)
{
	char *cause;

	if (value == NULL || output == NULL || output_len == 0) {
		/* Invalid parameters */
		return 0;
	}

	if (value->type != APC_MODBUS_VALUE_TYPE_INT) {
		return 0;
	}

	switch (value->data.int_value) {
	case -1:
		cause = "NotAvailable";
		break;
	case -2:
		cause = "LoadTooLow";
		break;
	case -3:
		cause = "OutputOff";
		break;
	case -4:
		cause = "OnBattery";
		break;
	case -5:
		cause = "InBypass";
		break;
	case -6:
		cause = "BatteryCharging";
		break;
	case -7:
		cause = "PoorACInput";
		break;
	case -8:
		cause = "BatteryDisconnected";
		break;
	default:
		return _apc_modbus_double_to_nut(value, output, output_len);
	}

	strncpy(output, cause, output_len);

	return 1;
}

static apc_modbus_converter_t _apc_modbus_efficiency_conversion = { _apc_modbus_efficiency_to_nut, NULL };

static int _apc_modbus_status_change_cause_to_nut(const apc_modbus_value_t *value, char *output, size_t output_len)
{
	char *cause;

	if (value == NULL || output == NULL || output_len == 0) {
		/* Invalid parameters */
		return 0;
	}

	if (value->type != APC_MODBUS_VALUE_TYPE_UINT) {
		return 0;
	}

	switch (value->data.uint_value) {
	case 0:
		cause = "SystemInitialization";
		break;
	case 1:
		cause = "HighInputVoltage";
		break;
	case 2:
		cause = "LowInputVoltage";
		break;
	case 3:
		cause = "DistortedInput";
		break;
	case 4:
		cause = "RapidChangeOfInputVoltage";
		break;
	case 5:
		cause = "HighInputFrequency";
		break;
	case 6:
		cause = "LowInputFrequency";
		break;
	case 7:
		cause = "FreqAndOrPhaseDifference";
		break;
	case 8:
		cause = "AcceptableInput";
		break;
	case 9:
		cause = "AutomaticTest";
		break;
	case 10:
		cause = "TestEnded";
		break;
	case 11:
		cause = "LocalUICommand";
		break;
	case 12:
		cause = "ProtocolCommand";
		break;
	case 13:
		cause = "LowBatteryVoltage";
		break;
	case 14:
		cause = "GeneralError";
		break;
	case 15:
		cause = "PowerSystemError";
		break;
	case 16:
		cause = "BatterySystemError";
		break;
	case 17:
		cause = "ErrorCleared";
		break;
	case 18:
		cause = "AutomaticRestart";
		break;
	case 19:
		cause = "DistortedInverterOutput";
		break;
	case 20:
		cause = "InverterOutputAcceptable";
		break;
	case 21:
		cause = "EPOInterface";
		break;
	case 22:
		cause = "InputPhaseDeltaOutOfRange";
		break;
	case 23:
		cause = "InputNeutralNotConnected";
		break;
	case 24:
		cause = "ATSTransfer";
		break;
	case 25:
		cause = "ConfigurationChange";
		break;
	case 26:
		cause = "AlertAsserted";
		break;
	case 27:
		cause = "AlertCleared";
		break;
	case 28:
		cause = "PlugRatingExceeded";
		break;
	case 29:
		cause = "OutletGroupStateChange";
		break;
	case 30:
		cause = "FailureBypassExpired";
		break;
	default:
		cause = "Unknown";
		break;
	}

	strncpy(output, cause, output_len);

	return 1;
}

static apc_modbus_converter_t _apc_modbus_status_change_cause_conversion = { _apc_modbus_status_change_cause_to_nut, NULL };

static int _apc_modbus_date_to_nut(const apc_modbus_value_t *value, char *output, size_t output_len)
{
	struct tm *tm_info;
	time_t time_stamp;
	const time_t start_offset = 946684800; /* 2000-01-01 00:00 */

	if (value == NULL || output == NULL || output_len == 0) {
		/* Invalid parameters */
		return 0;
	}

	if (value->type != APC_MODBUS_VALUE_TYPE_UINT) {
		return 0;
	}

	time_stamp = ((int64_t)value->data.uint_value * 86400) + start_offset;
	tm_info = gmtime(&time_stamp);
	strftime(output, output_len, "%Y-%m-%d", tm_info);

	return 1;
}

static apc_modbus_converter_t _apc_modbus_date_conversion = { _apc_modbus_date_to_nut, NULL };

typedef struct {
	const char *nut_variable_name;
	size_t modbus_addr;
	size_t modbus_len; /* Number of uint16_t registers */
	apc_modbus_value_types value_type;
	apc_modbus_converter_t *value_converter;
	const char *value_format;
	int value_scale;
	void *variable_ptr;
} apc_modbus_register_t;

/* Values that only need to be updated once on startup */
static apc_modbus_register_t apc_modbus_register_map_inventory[] = {
	{ "ups.firmware",               516,    8,  APC_MODBUS_VALUE_TYPE_STRING,   NULL,                                           "%s",   0,  NULL    },
	{ "ups.model",                  532,    16, APC_MODBUS_VALUE_TYPE_STRING,   NULL,                                           "%s",   0,  NULL    }, /* also device.model, filled automatically */
	{ "ups.serial",                 564,    8,  APC_MODBUS_VALUE_TYPE_STRING,   NULL,                                           "%s",   0,  NULL    }, /* also device.serial, filled automatically */
	{ "ups.power.nominal",          588,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_double_conversion,                 "%.0f", 0,  &power_nominal      },
	{ "ups.realpower.nominal",      589,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_double_conversion,                 "%.0f", 0,  &realpower_nominal  },
	{ "ups.mfr.date",               591,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_date_conversion,                   NULL,   0,  NULL    },
	{ "battery.date",               595,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_date_conversion,                   NULL,   0,  NULL    },
	{ "ups.id",                     596,    8,  APC_MODBUS_VALUE_TYPE_STRING,   NULL,                                           "%s",   0,  NULL    },
	{ NULL, 0, 0, 0, NULL, NULL, 0.0f, NULL }
};

static apc_modbus_register_t apc_modbus_register_map_status[] = {
	{ "input.transfer.reason",      2,      1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_status_change_cause_conversion,    NULL,   0,  NULL    },
	{ NULL, 0, 0, 0, NULL, NULL, 0.0f, NULL }
};

static apc_modbus_register_t apc_modbus_register_map_dynamic[] = {
	{ "battery.runtime",            128,    2,  APC_MODBUS_VALUE_TYPE_UINT,     NULL,                                           "%u",   0,  NULL    },
	{ "battery.charge",             130,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_double_conversion,                 "%.2f", 9,  NULL    },
	{ "battery.voltage",            131,    1,  APC_MODBUS_VALUE_TYPE_INT,      &_apc_modbus_double_conversion,                 "%.2f", 5,  NULL    },
	{ "battery.date.maintenance",   133,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_date_conversion,                   NULL,   0,  NULL    },
	{ "battery.temperature",        135,    1,  APC_MODBUS_VALUE_TYPE_INT,      &_apc_modbus_double_conversion,                 "%.2f", 7,  NULL    },
	{ "ups.load",                   136,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_double_conversion,                 "%.2f", 8,  NULL    },
	{ "ups.realpower",              136,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_power_conversion,                  "%.2f", 8,  &realpower_nominal },
	{ "ups.power",                  138,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_power_conversion,                  "%.2f", 8,  &power_nominal     },
	{ "output.current",             140,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_double_conversion,                 "%.2f", 5,  NULL    },
	{ "output.voltage",             142,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_double_conversion,                 "%.2f", 6,  NULL    },
	{ "output.frequency",           144,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_double_conversion,                 "%.2f", 7,  NULL    },
	{ "experimental.output.energy", 145,    2,  APC_MODBUS_VALUE_TYPE_UINT,     NULL,                                           "%u",   0,  NULL    },
	{ "input.voltage",              151,    1,  APC_MODBUS_VALUE_TYPE_UINT,     &_apc_modbus_voltage_conversion,                "%.2f", 6,  NULL    },
	{ "ups.efficiency",             154,    1,  APC_MODBUS_VALUE_TYPE_INT,      &_apc_modbus_efficiency_conversion,             "%.1f", 7,  NULL    },
	{ "ups.timer.shutdown",         155,    1,  APC_MODBUS_VALUE_TYPE_INT,      NULL,                                           "%d",   0,  NULL    },
	{ "ups.timer.start",            156,    1,  APC_MODBUS_VALUE_TYPE_INT,      NULL,                                           "%d",   0,  NULL    },
	{ "ups.timer.reboot",           157,    2,  APC_MODBUS_VALUE_TYPE_INT,      NULL,                                           "%d",   0,  NULL    },
	{ NULL, 0, 0, 0, NULL, NULL, 0.0f, NULL }
};

static apc_modbus_register_t apc_modbus_register_map_static[] = {
	{ "input.transfer.high",        1026,   1,  APC_MODBUS_VALUE_TYPE_UINT,     NULL,                                           "%u",   0,  NULL    },
	{ "input.transfer.low",         1027,   1,  APC_MODBUS_VALUE_TYPE_UINT,     NULL,                                           "%u",   0,  NULL    },
	{ "ups.delay.shutdown",         1029,   1,  APC_MODBUS_VALUE_TYPE_INT,      NULL,                                           "%d",   0,  NULL    },
	{ "ups.delay.start",            1030,   1,  APC_MODBUS_VALUE_TYPE_INT,      NULL,                                           "%d",   0,  NULL    },
	{ "ups.delay.reboot",           1031,   2,  APC_MODBUS_VALUE_TYPE_INT,      NULL,                                           "%d",   0,  NULL    },
	{ NULL, 0, 0, 0, NULL, NULL, 0.0f, NULL }
};

static void _apc_modbus_close(int free_modbus)
{
	if (modbus_ctx != NULL) {
		modbus_close(modbus_ctx);
		if (free_modbus) {
			modbus_free(modbus_ctx);
			modbus_ctx = NULL;
		}
	}

	is_open = 0;
}

#if defined NUT_MODBUS_HAS_USB
static void _apc_modbus_create_reopen_matcher(void)
{
	int r;

	if (reopen_matcher != NULL) {
		USBFreeExactMatcher(reopen_matcher);
		reopen_matcher = NULL;
	}

	r = USBNewExactMatcher(&reopen_matcher, &usbdevice);
	if (r < 0) {
		fatal_with_errno(EXIT_FAILURE, "USBNewExactMatcher");
	}

	reopen_matcher->next = best_matcher;
	best_matcher = reopen_matcher;
}
#endif /* defined NUT_MODBUS_HAS_USB */

static int _apc_modbus_reopen(void)
{
	dstate_setinfo("driver.state", "reconnect.trying");

	if (modbus_connect(modbus_ctx) < 0) {
		upslogx(LOG_ERR, "%s: Unable to connect Modbus: %s", __func__, modbus_strerror(errno));
		return 0;
	}


#if defined NUT_MODBUS_HAS_USB
	/* We might have matched a new device in the modbus_connect callback.
	 * Because of this we want a new exact matcher. */
	best_matcher = best_matcher->next;
	_apc_modbus_create_reopen_matcher();
#endif /* defined NUT_MODBUS_HAS_USB */

	usleep(1000000);
	modbus_flush(modbus_ctx);

	is_open = 1;

	dstate_setinfo("driver.state", "reconnect.updateinfo");
	_apc_modbus_read_inventory();
	dstate_setinfo("driver.state", "quiet");

	return 1;
}

static useconds_t _apc_modbus_get_time_us(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((useconds_t)tv.tv_sec * (useconds_t)1000000) + (useconds_t)tv.tv_usec;
}

static void _apc_modbus_interframe_delay(void)
{
	/* 4.2.2 Modbus Message RTU Framing, interframe delay */

	static useconds_t last_send_time = 0;
	static const useconds_t inter_frame_delay = 35000;
	useconds_t current_time, delta_time;

	current_time = _apc_modbus_get_time_us();
	delta_time = current_time - last_send_time;

	if (delta_time > inter_frame_delay) {
		/* Already over the inter frame delay */
		goto interframe_delay_exit;
	}

	usleep(inter_frame_delay - delta_time);

interframe_delay_exit:
	last_send_time = current_time;
}

static void _apc_modbus_handle_error(modbus_t *ctx)
{
	static int flush_retries = 0;
	int flush = 0;
#ifdef WIN32
	int wsa_error;
#endif /* WIN32 */

	/*
	 * We could enable MODBUS_ERROR_RECOVERY_LINK but that would just get stuck
	 * in libmodbus until recovery. The only indication of this is that the
	 * program is stuck and debug prints by libmodbus, which we don't want to
	 * enable on release.
	 *
	 * Instead we detect timout errors and do a sleep + flush, on every other
	 * error or when flush didn't work we do a reconnect.
	 */

#ifdef WIN32
	wsa_error = WSAGetLastError();
	if (wsa_error == WSAETIMEDOUT) {
		flush = 1;
	}
#else
	if (errno == ETIMEDOUT) {
		flush = 1;
	}
#endif /* WIN32 */

	if (flush > 0 && flush_retries++ < 5) {
		usleep(1000000);
		modbus_flush(ctx);
	} else {
		flush_retries = 0;
		upslogx(LOG_ERR, "%s: Closing connection", __func__);
		/* Close without free, will retry connection on next update */
		_apc_modbus_close(0);
	}
}

static int _apc_modbus_read_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest)
{
	_apc_modbus_interframe_delay();

	if (modbus_read_registers(ctx, addr, nb, dest) > 0) {
		return 1;
	} else {
		upslogx(LOG_ERR, "%s: Read of %d:%d failed: %s (%s)", __func__, addr, addr + nb, modbus_strerror(errno), device_path);
		_apc_modbus_handle_error(ctx);
		return 0;
	}
}

static int _apc_modbus_update_value(apc_modbus_register_t *regs_info, const uint16_t *regs, const size_t regs_len)
{
	apc_modbus_value_t value;
	char strbuf[33], nutvbuf[128];

	if (regs_info == NULL || regs == NULL || regs_len == 0) {
		/* Invalid parameters */
		return 0;
	}

	value.type = regs_info->value_type;
	value.format = regs_info->value_format;
	value.scale = regs_info->value_scale;
	value.variable_ptr = regs_info->variable_ptr;

	assert(regs_info->value_type <= apc_modbus_value_types_max);
	switch (regs_info->value_type) {
	case APC_MODBUS_VALUE_TYPE_STRING:
		_apc_modbus_to_string(regs, regs_info->modbus_len, strbuf, sizeof(strbuf));
		value.data.string_value = strbuf;
		break;
	case APC_MODBUS_VALUE_TYPE_INT:
		_apc_modbus_to_int64(regs, regs_info->modbus_len, &value.data.int_value);
		break;
	case APC_MODBUS_VALUE_TYPE_UINT:
		_apc_modbus_to_uint64(regs, regs_info->modbus_len, &value.data.uint_value);
		break;
	}

	if (regs_info->value_converter != NULL) {
		/* If we have a converter, use it and set the value as a string */
		if (!regs_info->value_converter->apc_to_nut(&value, nutvbuf, sizeof(nutvbuf))) {
			upslogx(LOG_ERR, "%s: Failed to convert register %" PRIuSIZE ":%" PRIuSIZE, __func__,
				regs_info->modbus_addr, regs_info->modbus_addr + regs_info->modbus_len);
			return 0;
		}
		dstate_setinfo(regs_info->nut_variable_name, "%s", nutvbuf);
	} else {
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
		assert(regs_info->value_type <= apc_modbus_value_types_max);
		switch (regs_info->value_type) {
		case APC_MODBUS_VALUE_TYPE_STRING:
			dstate_setinfo(regs_info->nut_variable_name, regs_info->value_format, value.data.string_value);
			break;
		case APC_MODBUS_VALUE_TYPE_INT:
			dstate_setinfo(regs_info->nut_variable_name, regs_info->value_format, value.data.int_value);
			break;
		case APC_MODBUS_VALUE_TYPE_UINT:
			dstate_setinfo(regs_info->nut_variable_name, regs_info->value_format, value.data.uint_value);
			break;
		}
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	}

	return 1;
}

static int _apc_modbus_process_registers(apc_modbus_register_t* values, const uint16_t *regs, const size_t regs_len, const size_t regs_offset)
{
	size_t i;
	apc_modbus_register_t *v;

	for (i = 0; values[i].nut_variable_name; i++) {
		v = &values[i];

		if ((size_t)v->modbus_addr < regs_offset || (size_t)(v->modbus_addr + v->modbus_len) > regs_offset + regs_len) {
			continue;
		}

		_apc_modbus_update_value(v, regs + (v->modbus_addr - regs_offset), v->modbus_len);
	}

	return 1;
}

static int _apc_modbus_read_inventory(void)
{
	uint16_t regbuf[88];

	/* Inventory Information */
	if (_apc_modbus_read_registers(modbus_ctx, 516, 88, regbuf)) {
		_apc_modbus_process_registers(apc_modbus_register_map_inventory, regbuf, 88, 516);
	} else {
		return 0;
	}

	return 1;
}

void upsdrv_initinfo(void)
{
	if (!_apc_modbus_read_inventory()) {
		fatalx(EXIT_FAILURE, "Can't read inventory information from the UPS");
	}

	dstate_setinfo("ups.mfr", "American Power Conversion"); /* also device.mfr, filled automatically */

	dstate_setinfo("device.type", "ups");
}

void upsdrv_updateinfo(void)
{
	uint16_t regbuf[32];
	uint64_t value;

	if (!is_open) {
		if (!_apc_modbus_reopen()) {
			upsdebugx(2, "Failed to reopen modbus");
			dstate_datastale();
			return;
		}
	}

	alarm_init();
	status_init();

	/* Status Data */
	if (_apc_modbus_read_registers(modbus_ctx, 0, 27, regbuf)) {
		/* UPSStatus_BF, 2 registers */
		_apc_modbus_to_uint64(&regbuf[0], 2, &value);
		if (value & (1 << 1)) {
			status_set("OL");
		}
		if (value & (1 << 2)) {
			status_set("OB");
		}
		if (value & (1 << 3)) {
			status_set("BYPASS");
		}
		if (value & (1 << 4)) {
			status_set("OFF");
		}
		if (value & (1 << 5)) {
			alarm_set("General fault");
		}
		if (value & (1 << 6)) {
			alarm_set("Input not acceptable");
		}
		if (value & (1 << 7)) {
			status_set("TEST");
		}
		if (value & (1 << 13)) {
			status_set("HE"); /* High efficiency / ECO mode*/
		}
		if (value & (1 << 21)) {
			status_set("OVER");
		}

		/* SimpleSignalingStatus_BF, 1 register */
		_apc_modbus_to_uint64(&regbuf[18], 1, &value);
		if (value & (1 << 1)) { /* ShutdownImminent */
			status_set("LB");
		}

		/* BatterySystemError_BF, 1 register */
		_apc_modbus_to_uint64(&regbuf[18], 1, &value);
		if (value & (1 << 1)) { /* NeedsReplacement */
			status_set("RB");
		}

		/* RunTimeCalibrationStatus_BF, 1 register */
		_apc_modbus_to_uint64(&regbuf[24], 1, &value);
		if (value & (1 << 1)) { /* InProgress */
			status_set("CAL");
		}

		_apc_modbus_process_registers(apc_modbus_register_map_status, regbuf, 27, 0);
	} else {
		dstate_datastale();
		return;
	}

	/* Dynamic Data */
	if (_apc_modbus_read_registers(modbus_ctx, 128, 32, regbuf)) {
		/* InputStatus_BF, 1 register */
		_apc_modbus_to_uint64(&regbuf[22], 1, &value);
		if (value & (1 << 5)) {
			status_set("BOOST");
		}
		if (value & (1 << 6)) {
			status_set("TRIM");
		}

		_apc_modbus_process_registers(apc_modbus_register_map_dynamic, regbuf, 32, 128);
	} else {
		dstate_datastale();
		return;
	}

	/* Static Data */
	if (_apc_modbus_read_registers(modbus_ctx, 1026, 7, regbuf)) {
		_apc_modbus_process_registers(apc_modbus_register_map_static, regbuf, 7, 1026);
	} else {
		dstate_datastale();
		return;
	}

	alarm_commit();
	status_commit();
	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	/* TODO: replace with a proper shutdown function */
	upslogx(LOG_ERR, "shutdown not supported");
	set_exit_flag(-1);
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
#if defined NUT_MODBUS_HAS_USB
	nut_usb_addvars();
#endif /* defined NUT_MODBUS_HAS_USB */

#if defined NUT_MODBUS_HAS_USB
	addvar(VAR_VALUE, "porttype", "Modbus port type (serial, tcp, usb, default=usb)");
#else
	addvar(VAR_VALUE, "porttype", "Modbus port type (serial, tcp, default=serial)");
#endif /* defined NUT_MODBUS_HAS_USB */
	addvar(VAR_VALUE, "slaveid", "Modbus slave id (default=1)");
	addvar(VAR_VALUE, "response_timeout_ms", "Modbus response timeout in milliseconds");

	/* Serial RTU parameters */
	addvar(VAR_VALUE, "baudrate", "Modbus serial RTU communication speed in baud (default=9600)");
	addvar(VAR_VALUE, "parity", "Modbus serial RTU parity (N=none, E=even, O=odd, default=N)");
	addvar(VAR_VALUE, "databits", "Modbus serial RTU data bit count (default=8)");
	addvar(VAR_VALUE, "stopbits", "Modbus serial RTU stop bit count (default=1)");
}

#if defined NUT_MODBUS_HAS_USB
static int _apc_modbus_usb_device_match_func(USBDevice_t *hd, void *privdata)
{
	int status;

	NUT_UNUSED_VARIABLE(privdata);

	status = is_usb_device_supported(apc_modbus_usb_device_table, hd);

	switch (status) {
		case POSSIBLY_SUPPORTED:
			/* by default, reject, unless the productid option is given */
			if (getval("productid")) {
				return 1;
			}
			return 0;

		case SUPPORTED:
			return 1;

		case NOT_SUPPORTED:
		default:
			return 0;
	}
}

static USBDeviceMatcher_t apc_modbus_usb_device_matcher = { &_apc_modbus_usb_device_match_func, NULL, NULL };

static void _apc_modbus_usb_lib_to_nut(const modbus_usb_device_t *device, USBDevice_t *out)
{
	/* This makes a USBDevice_t from modbus_usb_device_t so we can use our matchers */

	static char bus_buf[4], device_buf[4], bus_port_buf[4];

	assert(device != NULL);
	assert(out != NULL);

	memset(out, 0, sizeof(USBDevice_t));

	out->VendorID = device->vid;
	out->ProductID = device->pid;
	out->Vendor = device->vendor_str;
	out->Product = device->product_str;
	out->Serial = device->serial_str;
	out->bcdDevice = device->bcd_device;

	snprintf(bus_buf, sizeof(bus_buf), "%03u", device->bus);
	out->Bus = bus_buf;

	snprintf(device_buf, sizeof(device_buf), "%03u", device->device_address);
	out->Device = device_buf;

#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	snprintf(bus_port_buf, sizeof(bus_port_buf), "%03u", device->bus_port);
	out->BusPort = bus_port_buf;
#endif
}

static int _apc_modbus_usb_callback(const modbus_usb_device_t *device)
{
	HIDDesc_t *hid_desc;
	size_t i;
	HIDData_t *hid_cur_item, *hid_rtu_rx = NULL, *hid_rtu_tx = NULL;
	HIDNode_t hid_cur_usage;
	USBDeviceMatcher_t *current_matcher;

	if (device == NULL) {
		upslogx(LOG_ERR, "%s: NULL device passed", __func__);
		return -1;
	}

	_apc_modbus_usb_lib_to_nut(device, &usbdevice);

	current_matcher = best_matcher;
	while (current_matcher != NULL) {
		if (current_matcher->match_function(&usbdevice, current_matcher->privdata) == 1) {
			break;
		}

		current_matcher = current_matcher->next;
	}

	if (current_matcher == NULL) {
		upsdebug_with_errno(1, "%s: Failed to match!", __func__);
		return -1;
	}

	upsdebugx(2, "%s: Matched %s %s (USB VID/PID %04x:%04x)", __func__, device->vendor_str, device->product_str, device->vid, device->pid);

	if (device->hid_report_descriptor_buf == NULL) {
		upslogx(LOG_WARNING, "%s: No HID report descriptor, using defaults", __func__);
		goto usb_callback_exit;
	}

	upsdebugx(2, "%s: Checking %s %s (USB VID/PID %04x:%04x) report descriptors", __func__, device->vendor_str, device->product_str, device->vid, device->pid);

	hid_desc = Parse_ReportDesc(device->hid_report_descriptor_buf, (usb_ctrl_charbufsize)device->hid_report_descriptor_len);
	if (!hid_desc) {
		upsdebug_with_errno(1, "%s: Failed to parse report descriptor!", __func__);
		return -1;
	}

	for (i = 0; i < hid_desc->nitems; i++) {
		hid_cur_item = &hid_desc->item[i];

		hid_cur_usage = hid_cur_item->Path.Node[hid_cur_item->Path.Size - 1];

		if (hid_cur_usage == modbus_rtu_usb_usage_rx) {
			hid_rtu_rx = hid_cur_item;
		} else if (hid_cur_usage == modbus_rtu_usb_usage_tx) {
			hid_rtu_tx = hid_cur_item;
		}
	}

	if (hid_rtu_rx == NULL || hid_rtu_tx == NULL) {
		upsdebugx(1, "%s: No Modbus USB report descriptor found", __func__);
		Free_ReportDesc(hid_desc);
		return -1;
	}

	upsdebugx(1, "%s: Found report ids RX=0x%02x TX=0x%02x", __func__, hid_rtu_rx->ReportID, hid_rtu_tx->ReportID);

	modbus_rtu_usb_set_report_ids(modbus_ctx, hid_rtu_rx->ReportID, hid_rtu_tx->ReportID);

	Free_ReportDesc(hid_desc);

usb_callback_exit:
	dstate_setinfo("ups.vendorid", "%04x", device->vid);
	dstate_setinfo("ups.productid", "%04x", device->pid);

	return 0;
}
#endif /* defined NUT_MODBUS_HAS_USB */

static int _apc_modbus_parse_host_port(const char *input, char *host, size_t host_buf_size, char *port, size_t port_buf_size, const uint16_t default_port) {
	const char *start = input;
	const char *end = input;
	int port_int;
	size_t host_size, port_size;

	if (*start == '[') {
		start++;
		end = strchr(start, ']');
		if (!end) {
			upslogx(LOG_ERR, "%s: Invalid IPv6 notation", __func__);
			return 0;
		}
	} else {
		end = strchr(start, ':');
	}

	if (!end) {
		/* Port is missing, use the default port */
		strncpy(host, start, host_buf_size);
		snprintf(port, port_buf_size, "%u", default_port);
		return 0;
	}

	host_size = (size_t)(end - start);
	port_size = strlen(end + 1);

	if (host_size >= host_buf_size || port_size >= port_buf_size) {
		upslogx(LOG_ERR, "%s: Buffer size too small", __func__);
		return 0;
	}

	strncpy(host, start, host_size);
	strncpy(port, end + 1, port_size);

	port_int = atoi(port);
	if (port_int < 0 || port_int > 65535) {
		upslogx(LOG_ERR, "%s: Port out of range", __func__);
		return 0;
	}

	return 1;
}

void upsdrv_initups(void)
{
	int r;
#if defined NUT_MODBUS_HAS_USB
	char *regex_array[USBMATCHER_REGEXP_ARRAY_LIMIT];
	int has_nonzero_regex;
	size_t i;
#endif /* defined NUT_MODBUS_HAS_USB */
	char *val;
	int rtu_baudrate;
	char rtu_parity;
	int rtu_databits;
	int rtu_stopbits;
	int slaveid;
	uint32_t response_timeout_ms;
	char tcp_host[256];
	char tcp_port[6];

	val = getval("porttype");

	if (val == NULL) {
#if defined NUT_MODBUS_HAS_USB
		val = "usb";
#else
		val = "serial";
#endif /* defined NUT_MODBUS_HAS_USB */
	}

	is_open = 0;

#if defined NUT_MODBUS_HAS_USB
	is_usb = 0;
#endif /* defined NUT_MODBUS_HAS_USB */

	if (!strcasecmp(val, "usb")) {
#if defined NUT_MODBUS_HAS_USB
		is_usb = 1;

		/* We default to USB, this is the most common connection type and does not require additional
		 * parameter, so we can auto-detect these. */

		warn_if_bad_usb_port_filename(device_path);

		regex_array[0] = getval("vendorid");
		regex_array[1] = getval("productid");
		regex_array[2] = getval("vendor");
		regex_array[3] = getval("product");
		regex_array[4] = getval("serial");
		regex_array[5] = getval("bus");
		regex_array[6] = getval("device");
#  if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
		regex_array[7] = getval("busport");
#  else
		if (getval("busport")) {
			upslogx(LOG_WARNING, "\"busport\" is configured for the device, but is not actually handled by current build combination of NUT and libusb (ignored)");
		}
#  endif

		has_nonzero_regex = 0;
		for (i = 0; i < SIZEOF_ARRAY(regex_array); i++) {
			if (regex_array[i] != NULL) {
				has_nonzero_regex = 1;
			}
		}

		best_matcher = &apc_modbus_usb_device_matcher;

		if (has_nonzero_regex > 0) {
			r = USBNewRegexMatcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
			if (r < 0) {
				fatal_with_errno(EXIT_FAILURE, "USBNewRegexMatcher");
			} else if (r) {
				fatalx(EXIT_FAILURE, "invalid regular expression: %s", regex_array[r]);
			}

			regex_matcher->next = best_matcher;
			best_matcher = regex_matcher;
		}

		modbus_ctx = modbus_new_rtu_usb(MODBUS_USB_MODE_APC, _apc_modbus_usb_callback);
#else
		fatalx(EXIT_FAILURE, "driver was not compiled with USB support");
#endif /* defined NUT_MODBUS_HAS_USB */
	} else if (!strcasecmp(val, "tcp")) {
		if (!_apc_modbus_parse_host_port(device_path, tcp_host, sizeof(tcp_host), tcp_port, sizeof(tcp_port), modbus_tcp_default_port)) {
			fatalx(EXIT_FAILURE, "failed to parse host/port");
		}

		modbus_ctx = modbus_new_tcp_pi(tcp_host, tcp_port);
	} else if (!strcasecmp(val, "serial")) {
		val = getval("baudrate");
		rtu_baudrate = val ? atoi(val) : modbus_rtu_default_baudrate;

		val = getval("parity");
		rtu_parity = val ? (char)toupper(val[0]) : modbus_rtu_default_parity;

		val = getval("databits");
		rtu_databits = val ? atoi(val) : modbus_rtu_default_databits;

		val = getval("stopbits");
		rtu_stopbits = val ? atoi(val) : modbus_rtu_default_stopbits;

		modbus_ctx = modbus_new_rtu(device_path, rtu_baudrate, rtu_parity, rtu_databits, rtu_stopbits);
	}

	if (modbus_ctx == NULL) {
		fatalx(EXIT_FAILURE, "Unable to create the libmodbus context: %s", modbus_strerror(errno));
	}

	if (nut_debug_level > 5) {
		modbus_set_debug(modbus_ctx, 1);
	}

	val = getval("slaveid");
	slaveid = val ? atoi(val) : modbus_default_slave_id;
	r = modbus_set_slave(modbus_ctx, slaveid);
	if (r < 0) {
		modbus_free(modbus_ctx);
		fatalx(EXIT_FAILURE, "modbus_set_slave: invalid slave id %d", slaveid);
	}

	val = getval("response_timeout_ms");
	if (val != NULL) {
		response_timeout_ms = (uint32_t)strtoul(val, NULL, 0);

#if (defined NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32) || (defined NUT_MODBUS_TIMEOUT_ARG_sec_usec_uint32_cast_timeval_fields)
		r = modbus_set_response_timeout(modbus_ctx, response_timeout_ms / 1000, (response_timeout_ms % 1000) * 1000);
		if (r < 0) {
			modbus_free(modbus_ctx);
			fatalx(EXIT_FAILURE, "modbus_set_response_timeout: error(%s)", modbus_strerror(errno));
		}
#elif (defined NUT_MODBUS_TIMEOUT_ARG_timeval_numeric_fields)
	{	/* see comments above */
		struct timeval to;
		memset(&to, 0, sizeof(struct timeval));
		to.tv_sec = response_timeout_ms / 1000;
		to.tv_usec = (response_timeout_ms % 1000) * 1000;
		/* void */ modbus_set_response_timeout(modbus_ctx, &to);
	}
/* #elif (defined NUT_MODBUS_TIMEOUT_ARG_timeval) // some un-castable type in fields */
#endif /* NUT_MODBUS_TIMEOUT_ARG_* */
	}

	if (modbus_connect(modbus_ctx) == -1) {
		modbus_free(modbus_ctx);
		fatalx(EXIT_FAILURE, "modbus_connect: unable to connect: %s", modbus_strerror(errno));
	}

#if defined NUT_MODBUS_HAS_USB
	_apc_modbus_create_reopen_matcher();
#endif /* defined NUT_MODBUS_HAS_USB */

	modbus_flush(modbus_ctx);

	is_open = 1;
}


void upsdrv_cleanup(void)
{
	_apc_modbus_close(1);

#if defined NUT_MODBUS_HAS_USB
	USBFreeExactMatcher(reopen_matcher);
	USBFreeExactMatcher(regex_matcher);
#endif /* defined NUT_MODBUS_HAS_USB */
}
