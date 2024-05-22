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
 * - `SOH` is the start signal ('\x01')
 * - `Size` is the length (in bytes) of the header and the data field
 * - `Index` is the command index: see AVAILABLE COMMANDS
 * - `CMD` is the command code to execute: see AVAILABLE COMMANDS
 * - `Data` is the (optional) argument of the command
 * - `EOT` is the end signal ('\x04')
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

#include "serial.h"

#define DRIVER_NAME	"Bicker serial protocol"
#define DRIVER_VERSION	"0.01"

#define BICKER_SOH	'\x01'
#define BICKER_EOT	'\x04'
#define BICKER_TIMEOUT	1
#define BICKER_DELAY	20
#define BICKER_RETRIES	3

/* Protocol fixed lengths */
#define BICKER_HEADER	3
#define BICKER_MAXDATA	(255 - BICKER_HEADER)
#define BICKER_PACKET	(1 + BICKER_HEADER + BICKER_MAXDATA + 1)

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
	uint16_t val;
} BickerParameter;

/**
 * Send a packet.
 * @param idx     Command index
 * @param cmd     Command
 * @param data    Source data or NULL for no data field
 * @param datalen Size of the source data field or 0
 * @return        `datalen` on success or -1 on errors.
 */
static ssize_t bicker_send(char idx, char cmd, const void *data, size_t datalen)
{
	char buf[BICKER_PACKET];
	size_t buflen;
	ssize_t ret;

	ser_flush_io(upsfd);

	if (data != NULL) {
		if (datalen > BICKER_MAXDATA) {
			upslogx(LOG_ERR, "Data size exceeded: %d > %d",
				(int)datalen, BICKER_MAXDATA);
			return -1;
		}
		memcpy(buf + 4, data, datalen);
	} else {
		datalen = 0;
	}

	buf[0] = BICKER_SOH;
	buf[1] = datalen + BICKER_HEADER;
	buf[2] = idx;
	buf[3] = cmd;
	buf[1 + BICKER_HEADER + datalen] = BICKER_EOT;
	buflen = 1 + BICKER_HEADER + datalen + 1;

	ret = ser_send_buf(upsfd, buf, buflen);
	if (ret < 0) {
		upslog_with_errno(LOG_WARNING, "ser_send_buf failed");
		return -1;
	} else if ((size_t) ret != buflen) {
		upslogx(LOG_WARNING, "ser_send_buf returned %d instead of %d",
			(int)ret, (int)buflen);
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
static ssize_t bicker_receive(char idx, char cmd, void *data)
{
	ssize_t ret;
	size_t datalen;
	char buf[BICKER_PACKET];

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
			TOUINT(buf[0]), TOUINT(BICKER_SOH));
		return -1;
	}

	datalen = buf[1] - BICKER_HEADER;

	/* Read the rest: command index (1 byte), command (1 byte), data
	 * (datalen bytes) and EOT (1 byte), i.e. `datalen + 3` bytes */
	ret = ser_get_buf_len(upsfd, buf + 2, datalen + 3, BICKER_TIMEOUT, 0);
	if (ret < 0) {
		upslog_with_errno(LOG_WARNING, "ser_get_buf_len failed");
		return -1;
	}

	upsdebug_hex(3, "bicker_receive", buf, ret + 2);

	if ((size_t)ret < datalen + 3) {
		upslogx(LOG_WARNING, "Timeout waiting for the end of the packet");
		return -1;
	} else if (buf[datalen + 4] != BICKER_EOT) {
		upslogx(LOG_WARNING, "Received 0x%02X instead of EOT (0x%02X)",
			TOUINT(buf[datalen + 4]), TOUINT(BICKER_EOT));
		return -1;
	} else if (idx != '\xEE' && buf[2] == '\xEE') {
		/* I found sperimentally that, when the syntax is
		 * formally correct but a feature is not supported,
		 * the device returns "\x01\x03\xEE\x07\x04". */
		upsdebugx(2, "Command is not supported");
		return -1;
	} else if (buf[2] != idx) {
		upslogx(LOG_WARNING, "Indexes do not match: sent 0x%02X, received 0x%02X",
			TOUINT(idx), TOUINT(buf[2]));
		return -1;
	} else if (buf[3] != cmd) {
		upslogx(LOG_WARNING, "Commands do not match: sent 0x%02X, received 0x%02X",
			TOUINT(cmd), TOUINT(buf[3]));
		return -1;
	}

	if (data != NULL) {
		memcpy(data, buf + 4, datalen);
	}

	return datalen;
}

/**
 * Receive a packet with a data field of known size.
 * @param idx     Command index
 * @param cmd     Command
 * @param data    Destination buffer or NULL to discard the data field
 * @param datalen  The expected size of the data field
 * @return        The size of the data field on success or -1 on errors.
 *
 * `data`, if specified, must have at least `datalen` bytes. If
 * `datalen` is less than the received data size, an error is thrown.
 */
static ssize_t bicker_receive_known(char idx, char cmd, void *data, size_t datalen)
{
	ssize_t ret;
	size_t real_datalen;
	char real_data[BICKER_MAXDATA];

	ret = bicker_receive(idx, cmd, real_data);
	if (ret < 0) {
		return ret;
	}

	real_datalen = (size_t)ret;

	if (datalen < real_datalen) {
		upslogx(LOG_ERR, "Not enough space for the payload: %d < %d",
			(int)datalen, (int)real_datalen);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, real_data, real_datalen);
	}

	return real_datalen;
}

/**
 * Execute a command that returns an uint8_t value.
 * @param idx Command index
 * @param cmd Command
 * @param dst Destination for the value
 * @return    The size of the data field on success or -1 on errors.
 */
static ssize_t bicker_read_uint8(char idx, char cmd, uint8_t *dst)
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
 * @param dst Destination for the value of NULL to discard
 * @return    The size of the data field on success or -1 on errors.
 */
static ssize_t bicker_read_uint16(char idx, char cmd, uint16_t *dst)
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
static ssize_t bicker_read_int16(char idx, char cmd, int16_t *dst)
{
	return bicker_read_uint16(idx, cmd, (uint16_t *) dst);
}

/**
 * Execute a command that returns a string.
 * @param idx Command index
 * @param cmd Command
 * @param dst Destination for the string
 *
 * `dst` must have at least BICKER_MAXDATA+1 bytes, the additional byte
 * needed to accomodate the ending '\0'.
 */
static ssize_t bicker_read_string(char idx, char cmd, char *dst)
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

static ssize_t bicker_receive_parameter(BickerParameter *parameter)
{
	ssize_t ret;
	uint8_t data[10];

	ret = bicker_receive_known(0x07, parameter->id, data, 10);
	if (ret < 0) {
		return ret;
	}

	/* The returned `data` is in the format:
	 *   [AA] [bbBB] [ccCC] [ddDD] [EE] [ffFF]
	 * where:
	 *   [AA]   = parameter ID (Byte)
	 *   [BBbb] = minimum value (UInt16)
	 *   [CCcc] = maximum value (UInt16)
	 *   [DDdd] = standard value (UInt16)
	 *   [EE]   = enabled (Bool)
	 *   [FFff] = value (UInt16)
	 */
	parameter->id = data[0];
	parameter->min = WORDLH(data[1], data[2]);
	parameter->max = WORDLH(data[3], data[4]);
	parameter->std = WORDLH(data[5], data[6]);
	parameter->enabled = data[7];
	parameter->val = WORDLH(data[8], data[9]);

	upsdebugx(3, "Parameter %d = %d (%s, min = %d, max = %d, std = %d)",
		  parameter->id, parameter->val,
		  parameter->enabled ? "enabled" : "disabled",
		  parameter->min, parameter->max, parameter->std);

	return ret;
}

/**
 * Get a Bicker parameter.
 * @param parameter In/out parameter struct
 * @return The size of the data field on success or -1 on errors.
 *
 * You must fill at least the `parameter->id` field.
 */
static ssize_t bicker_get(BickerParameter *parameter)
{
	ssize_t ret;

	ret = bicker_send(0x07, parameter->id, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	return bicker_receive_parameter(parameter);
}

/**
 * Set a Bicker parameter.
 * @param parameter In parameter struct
 * @return The size of the data field on success or -1 on errors.
 *
 * You must fill at least the `parameter->id`, `parameter->enabled` and
 * `parameter->val` fields.
 */
static ssize_t bicker_set(BickerParameter *parameter)
{
	ssize_t ret;
	uint8_t data[3];

	/* Format of `data` is "[EE] [ffFF]"
	 * where:
	 *   [EE]   = enabled (Bool)
	 *   [FFff] = value (UInt16)
	 */
	data[0] = parameter->enabled;
	data[1] = LOWBYTE(parameter->val);
	data[2] = HIGHBYTE(parameter->val);
	ret = bicker_send(0x07, parameter->id, data, 3);
	if (ret < 0) {
		return ret;
	}

	return bicker_receive_parameter(parameter);
}

/* For some reason the `seconds` delay (at least on my UPSIC-2403D)
 * is not honored: the shutdown is always delayed by 2 seconds. This
 * fixed delay seems to be independent from the state of the UPS (on
 * line or on battery) and from the dip switches setting.
 *
 * As response I get the same command with `0xE0` in the data field.
 */
static ssize_t bicker_delayed_shutdown(uint8_t seconds)
{
	ssize_t ret;
	uint8_t response;

	ret = bicker_send('\x03', '\x32', &seconds, 1);
	if (ret < 0) {
		return ret;
	}

	ret = bicker_receive_known('\x03', '\x32', &response, 1);
	if (ret >= 0) {
		upslogx(LOG_INFO, "Shutting down in %d seconds: response = 0x%02X",
			(int)seconds, (unsigned)response);
	}

	return ret;
}

static ssize_t bicker_shutdown(void)
{
	const char *str;
	int delay;

	str = dstate_getinfo("ups.delay.shutdown");
	delay = atoi(str);
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
	if (!strcasecmp(varname, "battery.charge.restart")) {
		BickerParameter parameter;
		parameter.id = 0x05;
		parameter.enabled = 1;
		parameter.val = atoi(val);
		dstate_setinfo("battery.charge.restart", "%u", parameter.val);
	} else if (!strcasecmp(varname, "ups.delay.start")) {
		BickerParameter parameter;
		parameter.id = 0x04;
		parameter.enabled = 1;
		parameter.val = atoi(val);
		if (bicker_set(&parameter) >= 0) {
			dstate_setinfo("ups.delay.start", "%u", parameter.val);
		}
		return STAT_SET_HANDLED;
	}

	upslogx(LOG_NOTICE, "setvar: unknown variable [%s]", varname);
	return STAT_SET_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	char string[BICKER_MAXDATA + 1];

	dstate_setinfo("device.type", "ups");

	if (bicker_read_string('\x01', '\x60', string) >= 0) {
		dstate_setinfo("device.mfr", "%s", string);
	}

	if (bicker_read_string('\x01', '\x61', string) >= 0) {
		dstate_setinfo("device.serial", "%s", string);
	}

	if (bicker_read_string('\x01', '\x62', string) >= 0) {
		dstate_setinfo("device.model", "%s", string);
	}

	upsh.instcmd = bicker_instcmd;
	upsh.setvar = bicker_setvar;
}

void upsdrv_updateinfo(void)
{
	const char *str;
	uint8_t u8;
	uint16_t u16;
	int16_t i16;
	ssize_t ret;

	ret = bicker_read_uint16('\x01', '\x41', &u16);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("input.voltage", "%.1f", (double) u16 / 1000);

	ret = bicker_read_uint16('\x01', '\x42', &u16);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("input.current", "%.3f", (double) u16 / 1000);

	ret = bicker_read_uint16('\x01', '\x43', &u16);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("output.voltage", "%.3f", (double) u16 / 1000);

	ret = bicker_read_uint16('\x01', '\x44', &u16);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("output.current", "%.3f", (double) u16 / 1000);

	/* This is a supercap UPS so, in this context,
	 * the "battery" is the supercap stack */
	ret = bicker_read_uint16('\x01', '\x45', &u16);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("battery.voltage", "%.3f", (double) u16 / 1000);

	ret = bicker_read_int16('\x01', '\x46', &i16);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("battery.current", "%.3f", (double) i16 / 1000);

	/* Not implemented for all energy packs: failure acceptable */
	if (bicker_read_uint16('\x01', '\x4A', &u16) >= 0) {
		dstate_setinfo("battery.temperature", "%.1f", (double) u16 - 273.16);
	}

	/* Not implemented for all energy packs: failure acceptable */
	if (bicker_read_uint8('\x01', '\x48', &u8) >= 0) {
		dstate_setinfo("battery.status", "%d%%", u8);
	}

	ret = bicker_read_uint8('\x01', '\x47', &u8);
	if (ret < 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("battery.charge", "%d", u8);

	status_init();

	/* Consider the battery low when its charge is < 30% */
	str = dstate_getinfo("battery.charge.low");
	if (u8 < atoi(str)) {
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
	ret = bicker_read_uint8('\x01', '\x40', &u8);
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
	int retry;

	for (retry = 1; retry <= BICKER_RETRIES; retry++) {
		if (bicker_shutdown() > 0) {
			set_exit_flag(-2);	/* EXIT_SUCCESS */
			return;
		}
	}

	upslogx(LOG_ERR, "Shutdown failed!");
	set_exit_flag(-1);
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

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B38400);
	ser_set_dtr(upsfd, 1);

	/* Adding this variable here because upsdrv_initinfo() is not
	 * called when triggering a forced shutdown and
	 * "ups.delay.shutdown" is needed right there */
	dstate_setinfo("ups.delay.shutdown", "%u", BICKER_DELAY);

	if (bicker_read_string('\x01', '\x63', string) >= 0) {
		dstate_setinfo("ups.firmware", "%s", string);
	}

	if (bicker_read_string('\x01', '\x64', string) >= 0) {
		dstate_setinfo("battery.type", "%s", string);
	}

	dstate_setinfo("battery.charge.low", "%d", 30);

	/* Not implemented on all UPSes */
	if (bicker_read_string('\x01', '\x65', string) >= 0) {
		dstate_setinfo("ups.firmware.aux", "%s", string);
	}

	parameter.id = 0x05;
	if (bicker_get(&parameter) >= 0) {
		/* XXX: it seems to not work */
		dstate_setinfo("battery.charge.restart", "%u",
			       parameter.enabled ? parameter.val : 0);
	}

	parameter.id = 0x04;
	if (bicker_get(&parameter) >= 0) {
		dstate_setinfo("ups.delay.start", "%u",
			       parameter.enabled ? parameter.val : 0);
	}
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
