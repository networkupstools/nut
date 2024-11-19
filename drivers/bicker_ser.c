/*
 * bicker_ser.c: support for Bicker SuperCapacitors DC UPSes
 *
 * Copyright (C) 2024 - Nicola Fontana <ntd@entidi.it>
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

/* The protocol is reported in many Bicker's manuals but (according to
 * Bicker itself) the best source is the UPS Gen Software's user manual:
 *
 * https://www.bicker.de/media/pdf/ff/cc/fe/en_user_manual_ups-gen2-configuration-software.pdf
 *
 * Basically, this is a binary protocol without checksums:
 *
 *  1 byte  1 byte  1 byte  1 byte  0..252 bytes  1 byte
 * +-------+-------+-------+-------+--- - - - ---+-------+
 * |  SOH  | Size  | Index |  CMD  |    Data     |  EOT  |
 * +-------+-------+-------+-------+--- - - - ---+-------+
 *         |        HEADER         |
 *
 * where:
 * - `SOH` is the start signal (0x01)
 * - `Size` is the length (in bytes) of the header and the data field
 * - `Index` is the command index: see AVAILABLE COMMANDS
 * - `CMD` is the command code to execute: see AVAILABLE COMMANDS
 * - `Data` is the (optional) argument of the command
 * - `EOT` is the end signal (0x04)
 *
 * The same format is used for incoming and outcoming packets. The data
 * returned in the `Data` field is always in little-endian order.
 *
 * AVAILABLE COMMANDS
 * ------------------
 *
 * - Index = 0x01 (GENERIC)
 *   - CMD = 0x40 (status flags)
 *   - CMD = 0x41 (input voltage)
 *   - CMD = 0x42 (input current)
 *   - CMD = 0x43 (output voltage)
 *   - CMD = 0x44 (output current)
 *   - CMD = 0x45 (battery voltage)
 *   - CMD = 0x46 (battery current)
 *   - CMD = 0x47 (battery state of charge)
 *   - CMD = 0x48 (battery state of health)
 *   - CMD = 0x49 (battery cycles)
 *   - CMD = 0x4A (battery temperature)
 *   - CMD = 0x60 (manufacturer)
 *   - CMD = 0x61 (serial number)
 *   - CMD = 0x62 (device name)
 *   - CMD = 0x63 (firmware version)
 *   - CMD = 0x64 (battery pack)
 *   - CMD = 0x65 (firmware core version)
 *   - CMD = 0x66 (CPU temperature)
 *   - CMD = 0x67 (hardware revision)
 *   - CMD = 0x21 (UPS output)
 *   - CMD = 0x2F (shutdown flag)
 *   - CMD = 0x7A (reset parameter settings)
 *
 * - Index = 0x07 (PARAMETER)
 *   - CMD = 0x00 (get/set dummy entry: do not use!)
 *   - CMD = 0x01 (get/set load sensor)
 *   - CMD = 0x02 (get/set maximum backup time)
 *   - CMD = 0x03 (get/set os shutdown by timer)
 *   - CMD = 0x04 (get/set restart delay timer)
 *   - CMD = 0x05 (get/set minimum capacity to start)
 *   - CMD = 0x06 (get/set maximum backup time by in-1)
 *   - CMD = 0x07 (get/set os shutdown by soc)
 *   - CMD = 0x08 (get/set battery soc low threshold)
 *   - CMD = 0x09 (get/set relay event configuration)
 *   - CMD = 0x0A (get/set RS232 port configuration: place holder!)
 *
 * - Index = 0x03 (COMMANDS GOT FROM UPSIC MANUAL)
 *   - CMD = 0x1B (GetChargeStatusRegister)
 *   - CMD = 0x1C (GetMonitorStatusRegister)
 *   - CMD = 0x1E (GetCapacity)
 *   - CMD = 0x1F (GetEsr)
 *   - CMD = 0x20 (GetVCap1Voltage)
 *   - CMD = 0x21 (GetVCap2Voltage)
 *   - CMD = 0x22 (GetVCap3Voltage)
 *   - CMD = 0x23 (GetVCap4Voltage)
 *   - CMD = 0x25 (GetInputVoltage)
 *   - CMD = 0x26 (GetCapStackVoltage)
 *   - CMD = 0x27 (GetOutputVoltage)
 *   - CMD = 0x28 (GetInputCurrent)
 *   - CMD = 0x29 (GetChargeCurrent)
 *   - CMD = 0x31 (StartCapEsrMeasurement)
 *   - CMD = 0x32 (SetTimeToShutdown)
 */

#include "config.h"
#include "main.h"
#include "attribute.h"
#include "nut_stdint.h"

#include "serial.h"

#define DRIVER_NAME	"Bicker serial protocol"
#define DRIVER_VERSION	"0.03"

#define BICKER_SOH	0x01
#define BICKER_EOT	0x04
#define BICKER_TIMEOUT	1
#define BICKER_DELAY	20
#define BICKER_RETRIES	3
#define BICKER_MAXID	0x0A /* Max parameter ID */
#define BICKER_MAXVAL	0xFFFF /* Max parameter value */

/* Protocol lengths */
#define BICKER_HEADER		3
#define BICKER_MAXDATA		(255 - BICKER_HEADER)
#define BICKER_PACKET(datalen)	(1 + BICKER_HEADER + (datalen) + 1)

#define TOUINT(ch)	((unsigned)(uint8_t)(ch))
#define LOWBYTE(w)	((uint8_t)((uint16_t)(w) & 0x00FF))
#define HIGHBYTE(w)	((uint8_t)(((uint16_t)(w) & 0xFF00) >> 8))
#define WORDLH(l,h)	((uint16_t)((l) + ((h) << 8)))

upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Nicola Fontana <ntd@entidi.it>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

typedef struct {
	uint8_t  id;
	uint16_t min;
	uint16_t max;
	uint16_t std;
	uint8_t  enabled;
	uint16_t value;
} BickerParameter;

typedef struct {
	uint8_t     bicker_id;
	const char *nut_name;
	const char *description;
} BickerMapping;

static const BickerMapping bicker_mappings[] = {
	/* Official variables present in docs/nut-names.txt */
	{ 0x02, "ups.delay.shutdown",
		"Interval to wait after shutdown with delay command (seconds)" },
	{ 0x04, "ups.delay.start",
		"Interval to wait before restarting the load (seconds)" },
	{ 0x05, "battery.charge.restart",
		"Minimum battery level for UPS restart after power-off" },
	{ 0x07, "battery.charge.low",
		"Remaining battery level when UPS switches to LB (percent)" },

	/* Unofficial variables under the "experimental" namespace */
	{ 0x01, "experimental.output.current.low",
		"Current threshold under which the power will be cut (mA)" },
	{ 0x03, "experimental.ups.delay.shutdown.signal",
		"Interval to wait before sending the shutdown signal (seconds)" },
	{ 0x06, "experimental.ups.delay.shutdown.signal.masked",
		"Interval to wait with IN1 high before sending the shutdown signal (seconds)" },
	{ 0x08, "experimental.battery.charge.low.empty",
		"Battery level threshold for the empty signal (percent)" },
	{ 0x09, "experimental.ups.relay.mode",
		"Behavior of the relay" },
};

/**
 * Parameter id validation.
 * @param id      Id of the parameter
 * @param context Description of the calling code for the log message
 * @return 1 on valid id, 0 on errors.
 *
 * The id is valid if within the 0x01..BICKER_MAXID range (inclusive).
 */
static int bicker_valid_id(uint8_t id, const char *context)
{
	if (id < 1 || id > BICKER_MAXID) {
		upslogx(LOG_ERR, "%s: parameter id 0x%02X is out of range (0x01..0x%02X)",
			context, (unsigned)id, (unsigned)BICKER_MAXID);
		return 0;
	}
	return 1;
}

/**
 * Send a packet.
 * @param idx     Command index
 * @param cmd     Command
 * @param data    Source data or NULL for no data field
 * @param datalen Size of the source data field or 0
 * @return        `datalen` on success or -1 on errors.
 */
static ssize_t bicker_send(uint8_t idx, uint8_t cmd, const void *data, size_t datalen)
{
	uint8_t buf[BICKER_PACKET(BICKER_MAXDATA)];
	size_t buflen;
	ssize_t ret;

	if (data != NULL) {
		if (datalen > BICKER_MAXDATA) {
			upslogx(LOG_ERR,
				"Data size exceeded: %" PRIuSIZE " > %d",
				datalen, BICKER_MAXDATA);
			return -1;
		}
		memcpy(&buf[1 + BICKER_HEADER], data, datalen);
	} else {
		datalen = 0;
	}

	ser_flush_io(upsfd);

	buflen = BICKER_PACKET(datalen);
	buf[0] = BICKER_SOH;
	buf[1] = BICKER_HEADER + datalen;
	buf[2] = idx;
	buf[3] = cmd;
	buf[buflen - 1] = BICKER_EOT;

	ret = ser_send_buf(upsfd, buf, buflen);
	if (ret < 0) {
		upslog_with_errno(LOG_WARNING, "ser_send_buf failed");
		return -1;
	} else if ((size_t) ret != buflen) {
		upslogx(LOG_WARNING, "ser_send_buf returned %"
			PRIiSIZE " instead of %" PRIuSIZE,
			ret, buflen);
		return -1;
	}

	upsdebug_hex(3, "bicker_send", buf, buflen);
	return datalen;
}

/**
 * Receive a packet with a data field of unknown size.
 * @param idx  Command index
 * @param cmd  Command
 * @param data Destination buffer or NULL to discard the data field
 * @return     The size of the data field on success or -1 on errors.
 *
 * The data field is stored directly in the destination buffer. `data`,
 * if not NULL, must have at least BICKER_MAXDATA bytes.
 */
static ssize_t bicker_receive(uint8_t idx, uint8_t cmd, void *data)
{
	ssize_t ret;
	size_t buflen, datalen;
	uint8_t buf[BICKER_PACKET(BICKER_MAXDATA)];

	/* Read first two bytes (SOH + size) */
	ret = ser_get_buf_len(upsfd, buf, 2, BICKER_TIMEOUT, 0);
	if (ret < 0) {
		upslog_with_errno(LOG_WARNING, "Initial ser_get_buf_len failed");
		return -1;
	} else if (ret < 2) {
		upslogx(LOG_WARNING, "Timeout waiting for response packet");
		return -1;
	} else if (buf[0] != BICKER_SOH) {
		upslogx(LOG_WARNING, "Received 0x%02X instead of SOH (0x%02X)",
			(unsigned)buf[0], (unsigned)BICKER_SOH);
		return -1;
	}

	/* buf[1] (the size field) is BICKER_HEADER + data length, so */
	datalen = buf[1] - BICKER_HEADER;

	/* Read the rest of the packet */
	buflen = BICKER_PACKET(datalen);
	ret = ser_get_buf_len(upsfd, buf + 2, buflen - 2, BICKER_TIMEOUT, 0);
	if (ret < 0) {
		upslog_with_errno(LOG_WARNING, "ser_get_buf_len failed");
		return -1;
	}

	upsdebug_hex(3, "bicker_receive", buf, ret + 2);

	if ((size_t)ret < buflen - 2) {
		upslogx(LOG_WARNING, "Timeout waiting for the end of the packet");
		return -1;
	} else if (buf[buflen - 1] != BICKER_EOT) {
		upslogx(LOG_WARNING, "Received 0x%02X instead of EOT (0x%02X)",
			(unsigned)buf[buflen - 1], (unsigned)BICKER_EOT);
		return -1;
	} else if (idx != 0xEE && buf[2] == 0xEE) {
		/* I found experimentally that, when the syntax is
		 * formally correct but a feature is not supported,
		 * the device returns 0x01 0x03 0xEE 0x07 0x04. */
		upsdebugx(2, "Command is not supported");
		return -1;
	} else if (buf[2] != idx) {
		upslogx(LOG_WARNING, "Indexes do not match: sent 0x%02X, received 0x%02X",
			(unsigned)idx, (unsigned)buf[2]);
		return -1;
	} else if (buf[3] != cmd) {
		upslogx(LOG_WARNING, "Commands do not match: sent 0x%02X, received 0x%02X",
			(unsigned)cmd, (unsigned)buf[3]);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, &buf[1 + BICKER_HEADER], datalen);
	}

	return datalen;
}

/**
 * Receive a packet with a data field of known size.
 * @param idx     Command index
 * @param cmd     Command
 * @param dst     Destination buffer or NULL to discard the data field
 * @param datalen The expected size of the data field
 * @return        `datalen` on success or -1 on errors.
 *
 * `dst`, if not NULL, must have at least `datalen` bytes. If the data
 * is not exactly `datalen` bytes, an error is thrown.
 */
static ssize_t bicker_receive_known(uint8_t idx, uint8_t cmd, void *dst, size_t datalen)
{
	ssize_t ret;
	uint8_t data[BICKER_MAXDATA];

	ret = bicker_receive(idx, cmd, data);
	if (ret < 0) {
		return ret;
	}

	if (datalen != (size_t)ret) {
		upslogx(LOG_ERR, "Data size does not match: expected %"
			PRIuSIZE " but got %" PRIiSIZE " bytes",
			datalen, ret);
		return -1;
	}

	if (dst != NULL) {
		memcpy(dst, data, datalen);
	}

	return datalen;
}

/**
 * Receive the response of a set/get parameter command.
 * @param id  Id of the parameter
 * @param dst Where to store the response or NULL to discard
 * @return The size of the data field on success or -1 on errors.
 */
static ssize_t bicker_receive_parameter(uint8_t id, BickerParameter *dst)
{
	ssize_t ret;
	uint8_t data[10];
	BickerParameter parameter;

	if (!bicker_valid_id(id, "bicker_receive_parameter")) {
		return -1;
	}

	ret = bicker_receive_known(0x07, id, data, sizeof(data));
	if (ret < 0) {
		return ret;
	}

	/* The returned `data` is in the format:
	 *   [AA] [bbBB] [ccCC] [ddDD] [EE] [ffFF]
	 * where:
	 *   [AA]   = parameter id (Byte)
	 *   [BBbb] = minimum value (UInt16)
	 *   [CCcc] = maximum value (UInt16)
	 *   [DDdd] = standard value (UInt16)
	 *   [EE]   = enabled (Bool)
	 *   [FFff] = value (UInt16)
	 */
	parameter.id = data[0];
	parameter.min = WORDLH(data[1], data[2]);
	parameter.max = WORDLH(data[3], data[4]);
	parameter.std = WORDLH(data[5], data[6]);
	parameter.enabled = data[7];
	parameter.value = WORDLH(data[8], data[9]);

	upsdebugx(3, "Parameter %u = %u (%s, min = %u, max = %u, std = %u)",
		  (unsigned)parameter.id, (unsigned)parameter.value,
		  parameter.enabled ? "enabled" : "disabled",
		  (unsigned)parameter.min, (unsigned)parameter.max,
		  (unsigned)parameter.std);

	if (dst != NULL) {
		memcpy(dst, &parameter, sizeof(parameter));
	}

	return ret;
}

/**
 * Execute a command that returns an uint8_t value.
 * @param idx Command index
 * @param cmd Command
 * @param dst Destination for the value
 * @return    The size of the data field on success or -1 on errors.
 */
static ssize_t bicker_read_uint8(uint8_t idx, uint8_t cmd, uint8_t *dst)
{
	ssize_t ret;

	ret = bicker_send(idx, cmd, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	return bicker_receive_known(idx, cmd, dst, 1);
}

/**
 * Execute a command that returns an uint16_t value.
 * @param idx Command index
 * @param cmd Command
 * @param dst Destination for the value or NULL to discard
 * @return    The size of the data field on success or -1 on errors.
 */
static ssize_t bicker_read_uint16(uint8_t idx, uint8_t cmd, uint16_t *dst)
{
	ssize_t ret;
	uint8_t data[2];

	ret = bicker_send(idx, cmd, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	ret = bicker_receive_known(idx, cmd, data, 2);
	if (ret < 0) {
		return ret;
	}

	if (dst != NULL) {
		*dst = WORDLH(data[0], data[1]);
	}

	return ret;
}

/**
 * Execute a command that returns an int16_t value.
 * @param idx Command index
 * @param cmd Command
 * @param dst Destination for the value or NULL to discard
 * @return    The size of the data field on success or -1 on errors.
 */
static ssize_t bicker_read_int16(uint8_t idx, uint8_t cmd, int16_t *dst)
{
	return bicker_read_uint16(idx, cmd, (uint16_t *) dst);
}

/**
 * Execute a command that returns a string.
 * @param idx Command index
 * @param cmd Command
 * @param dst Destination for the string or NULL to discard
 * @return    The size of the data field on success or -1 on errors.
 *
 * `dst`, if not NULL, must have at least BICKER_MAXDATA+1 bytes, the
 * additional byte needed to accomodate the ending '\0'.
 */
static ssize_t bicker_read_string(uint8_t idx, uint8_t cmd, char *dst)
{
	ssize_t ret;

	ret = bicker_send(idx, cmd, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	ret = bicker_receive(idx, cmd, dst);
	if (ret < 0) {
		return ret;
	}

	dst[ret] = '\0';
	return ret;
}

/**
 * Create a read-write Bicker parameter.
 * @param parameter Source information
 * @param mapping   How that parameter is mapped to NUT
 */
static void bicker_new(const BickerParameter *parameter, const BickerMapping *mapping)
{
	const char *varname;

	varname = mapping->nut_name;
	if (parameter->enabled) {
		dstate_setinfo(varname, "%u", (unsigned)parameter->value);
	} else {
		/* dstate_setinfo(varname, "") triggers a GCC warning */
		dstate_setinfo(varname, "%s", "");
	}

	/* Using ST_FLAG_STRING so an empty string can be used
	 * to identify a disabled parameter */
	dstate_setflags(varname, ST_FLAG_RW | ST_FLAG_STRING);

	/* Just tested it: setting a range does not hinder setting
	 * an empty string with `dstate_setinfo(varname, "")` */
	if (parameter->min == BICKER_MAXVAL) {
		/* The device here is likely corrupt:
		 * apply a standard range to try using it anyway */
		upslogx(LOG_WARNING, "Parameter %s is corrupt", varname);
		dstate_addrange(varname, 0, BICKER_MAXVAL);
	} else {
		dstate_addrange(varname, parameter->min, parameter->max);
	}

	/* Maximum value for an uint16_t is 65535, i.e. 5 digits */
	dstate_setaux(varname, 5);
}

/**
 * Get a Bicker parameter.
 * @param id  Id of the parameter
 * @param dst Where to store the response or NULL to discard
 * @return The size of the data field on success or -1 on errors.
 */
static ssize_t bicker_get(uint8_t id, BickerParameter *dst)
{
	ssize_t ret;

	if (!bicker_valid_id(id, "bicker_get")) {
		return -1;
	}

	ret = bicker_send(0x07, id, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	return bicker_receive_parameter(id, dst);
}

/**
 * Set a Bicker parameter.
 * @param id      Id of the parameter
 * @param enabled 0 to disable, 1 to enable
 * @param value   What to set in the value field
 * @param dst     Where to store the response or NULL to discard
 * @return The size of the data field on success or -1 on errors.
 */
static ssize_t bicker_set(uint8_t id, uint8_t enabled, uint16_t value, BickerParameter *dst)
{
	ssize_t ret;
	uint8_t data[3];

	if (!bicker_valid_id(id, "bicker_set")) {
		return -1;
	} else if (enabled > 1) {
		upslogx(LOG_ERR, "bicker_set(0x%02X, %d, %u): enabled must be 0 or 1",
			(unsigned)id, enabled, (unsigned)value);
		return -1;
	}

	/* Format of `data` is "[EE] [ffFF]"
	 * where:
	 *   [EE]   = enabled (Bool)
	 *   [FFff] = value (UInt16)
	 */
	data[0] = enabled;
	data[1] = LOWBYTE(value);
	data[2] = HIGHBYTE(value);
	ret = bicker_send(0x07, id, data, 3);
	if (ret < 0) {
		return ret;
	}

	return bicker_receive_parameter(id, dst);
}

/**
 * Write to a Bicker parameter.
 * @param id  Id of the parameter
 * @param val A string with the value to write
 * @param dst Where to store the response or NULL to discard
 * @return The size of the data field on success or -1 on errors.
 *
 * This function is similar to bicker_set() but accepts string values.
 * If `val` is NULL or empty, the underlying Bicker parameter is
 * disabled and reset to its standard value.
 */
static int bicker_write(uint8_t id, const char *val, BickerParameter *dst)
{
	ssize_t ret;
	BickerParameter parameter;
	uint8_t enabled;
	uint16_t value;

	if (val == NULL || val[0] == '\0') {
		ret = bicker_get(id, &parameter);
		if (ret < 0) {
			return ret;
		}
		enabled = 0;
		value = parameter.std;
	} else {
		enabled = 1;
		value = atoi(val);
	}

	ret = bicker_set(id, enabled, value, &parameter);
	if (ret < 0) {
		return ret;
	}

	if (dst != NULL) {
		memcpy(dst, &parameter, sizeof(parameter));
	}

	return ret;
}

/* For some reason the `seconds` delay (at least on my UPSIC-2403D)
 * is not honored: the shutdown is always delayed by 2 seconds. This
 * fixed delay seems to be independent from the state of the UPS (on
 * line or on battery) and from the DIP switches setting.
 *
 * As response I get the same command with `0xE0` in the data field.
 */
static ssize_t bicker_delayed_shutdown(uint8_t seconds)
{
	ssize_t ret;
	uint8_t response;

	ret = bicker_send(0x03, 0x32, &seconds, 1);
	if (ret < 0) {
		return ret;
	}

	ret = bicker_receive_known(0x03, 0x32, &response, 1);
	if (ret >= 0) {
		upslogx(LOG_INFO, "Shutting down in %d seconds: response = 0x%02X",
			seconds, (unsigned)response);
	}

	return ret;
}

static ssize_t bicker_shutdown(void)
{
	const char *str;
	int delay;

	str = dstate_getinfo("ups.delay.shutdown");
	delay = str != NULL ? atoi(str) : 0;
	if (delay > 255) {
		upslogx(LOG_WARNING, "Shutdown delay too big: %d > 255",
			delay);
		delay = 255;
	}

	return bicker_delayed_shutdown(delay);
}

static int bicker_instcmd(const char *cmdname, const char *extra)
{
	NUT_UNUSED_VARIABLE(extra);

	if (!strcasecmp(cmdname, "shutdown.return")) {
		bicker_shutdown();
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static int bicker_setvar(const char *varname, const char *val)
{
	const BickerMapping *mapping;
	unsigned i;
	BickerParameter parameter;

	/* This should not be needed because when `bicker_write()` is
	 * successful the `parameter` struct is populated but gcc seems
	 * not to be smart enough to realize that and errors out with
	 * "error: ‘parameter...’ may be used uninitialized in this function"
	 */
	parameter.id = 0;
	parameter.min = 0;
	parameter.max = BICKER_MAXVAL;
	parameter.std = 0;
	parameter.enabled = 0;
	parameter.value = 0;

	/* Handle mapped parameters */
	for (i = 0; i < SIZEOF_ARRAY(bicker_mappings); ++i) {
		mapping = &bicker_mappings[i];
		if (!strcasecmp(varname, mapping->nut_name)) {
			if (bicker_write(mapping->bicker_id, val, &parameter) < 0) {
				return STAT_SET_FAILED;
			}

			if (parameter.enabled) {
				dstate_setinfo(varname, "%u",
					       (unsigned)parameter.value);
			} else {
				/* Disabled parameters are removed from NUT */
				dstate_delinfo(varname);
			}
			return STAT_SET_HANDLED;
		}
	}

	upslogx(LOG_NOTICE, "setvar: unknown variable [%s]", varname);
	return STAT_SET_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	char string[BICKER_MAXDATA + 1];

	dstate_setinfo("device.type", "ups");

	if (bicker_read_string(0x01, 0x60, string) >= 0) {
		dstate_setinfo("device.mfr", "%s", string);
	}

	if (bicker_read_string(0x01, 0x61, string) >= 0) {
		dstate_setinfo("device.serial", "%s", string);
	}

	if (bicker_read_string(0x01, 0x62, string) >= 0) {
		dstate_setinfo("device.model", "%s", string);
	}

	dstate_addcmd("shutdown.return");

	upsh.instcmd = bicker_instcmd;
	upsh.setvar = bicker_setvar;
}

void upsdrv_updateinfo(void)
{
	uint8_t u8;
	uint16_t u16;
	int16_t i16;
	ssize_t ret;

	ret = bicker_read_uint16(0x01, 0x41, &u16);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("input.voltage", "%.1f", (double) u16 / 1000);

	ret = bicker_read_uint16(0x01, 0x42, &u16);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("input.current", "%.3f", (double) u16 / 1000);

	ret = bicker_read_uint16(0x01, 0x43, &u16);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("output.voltage", "%.3f", (double) u16 / 1000);

	ret = bicker_read_uint16(0x01, 0x44, &u16);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("output.current", "%.3f", (double) u16 / 1000);

	/* This is a supercap UPS so, in this context,
	 * the "battery" is the supercap stack */
	ret = bicker_read_uint16(0x01, 0x45, &u16);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("battery.voltage", "%.3f", (double) u16 / 1000);

	ret = bicker_read_int16(0x01, 0x46, &i16);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("battery.current", "%.3f", (double) i16 / 1000);

	/* Not implemented for all energy packs: failure acceptable */
	if (bicker_read_uint16(0x01, 0x4A, &u16) >= 0) {
		dstate_setinfo("battery.temperature", "%.1f", (double) u16 - 273.16);
	}

	/* Not implemented for all energy packs: failure acceptable */
	if (bicker_read_uint8(0x01, 0x48, &u8) >= 0) {
		dstate_setinfo("battery.status", "%d%%", u8);
	}

	ret = bicker_read_uint8(0x01, 0x47, &u8);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("battery.charge", "%d", u8);

	status_init();

	/* In `u8` we already have the battery charge */
	if (u8 < atoi(dstate_getinfo("battery.charge.low"))) {
		status_set("LB");
	}

	/* StatusFlags() returns an 8 bit register:
	 * 0. Charging
	 * 1. Discharging
	 * 2. Power present
	 * 3. Battery present
	 * 4. Shutdown received
	 * 5. Overcurrent
	 * 6. ---
	 * 7. ---
	 */
	ret = bicker_read_uint8(0x01, 0x40, &u8);
	if (ret < 0) {
		dstate_datastale();
		return;
	}

	if ((u8 & 0x01) > 0) {
		status_set("CHRG");
	}
	if ((u8 & 0x02) > 0) {
		status_set("DISCHRG");
	}
	dstate_setinfo("battery.charger.status",
		       (u8 & 0x01) > 0 ? "charging" :
		       (u8 & 0x02) > 0 ? "discharging" :
		       "resting");

	status_set((u8 & 0x04) > 0 ? "OL" : "OB");
	if ((u8 & 0x20) > 0) {
		status_set("OVER");
	}

	status_commit();

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	int	retry;

	for (retry = 1; retry <= BICKER_RETRIES; retry++) {
		if (bicker_shutdown() > 0) {
			if (handling_upsdrv_shutdown > 0)
				set_exit_flag(EF_EXIT_SUCCESS);
			return;
		}
	}

	upslogx(LOG_ERR, "Shutdown failed!");
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(EF_EXIT_FAILURE);
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}

void upsdrv_initups(void)
{
	char string[BICKER_MAXDATA + 1];
	BickerParameter parameter;
	const BickerMapping *mapping;
	unsigned i;

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B38400);
	ser_set_dtr(upsfd, 1);

	if (bicker_read_string(0x01, 0x63, string) >= 0) {
		dstate_setinfo("ups.firmware", "%s", string);
	}

	if (bicker_read_string(0x01, 0x64, string) >= 0) {
		dstate_setinfo("battery.type", "%s", string);
	}

	/* Not implemented on all UPSes */
	if (bicker_read_string(0x01, 0x65, string) >= 0) {
		dstate_setinfo("ups.firmware.aux", "%s", string);
	}

	/* Initialize mapped parameters */
	for (i = 0; i < SIZEOF_ARRAY(bicker_mappings); ++i) {
		mapping = &bicker_mappings[i];
		if (bicker_get(mapping->bicker_id, &parameter) >= 0) {
			bicker_new(&parameter, mapping);
		}
	}

	/* Ensure "battery.charge.low" variable is defined */
	if (dstate_getinfo("battery.charge.low") == NULL) {
		dstate_setinfo("battery.charge.low", "%d", 20);
	}
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
