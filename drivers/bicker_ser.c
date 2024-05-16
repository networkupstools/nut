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

/* The protocol is detailed in section 14 of the DC/UPSIC user's manual,
 * downloadable somewhere from the Bicker website.
 *
 * Basically, this is a binary protocol without checksums:
 *
 *  1 byte  1 byte  1 byte  1 byte  0..252 bytes  1 byte
 * +-------+-------+-------+-------+--- - - - ---+-------+
 * |  SOH  | Size  | Index |  CMD  |    Data     |  EOT  |
 * +-------+-------+-------+-------+--- - - - ---+-------+
 *
 * where:
 * - `SOH` is the start signal ('\x01')
 * - `Size` is the length of the `Data` field (in bytes) plus 3
 * - `Index` is the command index (always '\x03')
 * - `CMD` is the command code to execute
 * - `Data` is the (optional) argument of the command
 * - `EOT` is the end signal ('\x04')
 *
 * The same format is used for incoming and outcoming packets. Although
 * not explicitly documented, the data returned by the `Data` field
 * seems to be in little-endian order.
 */

#include "config.h"
#include "main.h"
#include "attribute.h"

#include "serial.h"

#define DRIVER_NAME	"Bicker serial protocol"
#define DRIVER_VERSION	"0.01"

#define BICKER_PACKET	(1+1+1+1+252+1)
#define BICKER_SOH	'\x01'
#define BICKER_EOT	'\x04'

upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Nicola Fontana <ntd@entidi.it>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

static ssize_t bicker_send(char cmd, const char *data)
{
	char buf[BICKER_PACKET];
	size_t datalen, buflen;
	ssize_t ret;

	ser_flush_io(upsfd);

	if (data != NULL) {
		datalen = strlen(data);
		if (datalen > 252) {
			upslogx(LOG_ERR, "Data size exceeded: %d > 252",
				(int)datalen);
			return 0;
		}
		memcpy(buf + 4, data, datalen);
	} else {
		datalen = 0;
	}

	buf[0] = BICKER_SOH;
	buf[1] = datalen + 3; /* Packet size must include the header */
	buf[2] = '\x03'; /* Command index is always 3 */
	buf[3] = cmd;
	buf[4 + datalen] = BICKER_EOT;

	/* The full packet includes SOH and EOT bytes too */
	buflen = datalen + 3 + 2;

	ret = ser_send_buf(upsfd, buf, buflen);
	if (ret < 0) {
		upslog_with_errno(LOG_WARNING, "ser_send_buf failed");
		return ret;
	} else if ((size_t) ret != buflen) {
		upslogx(LOG_WARNING, "ser_send_buf returned %d instead of %d",
			(int)ret, (int)buflen);
		return 0;
	}

	upsdebug_hex(3, "bicker_send", buf, buflen);
	return ret;
}

static ssize_t bicker_receive_buf(void *dst, size_t dstlen)
{
	ssize_t ret;

	ret = ser_get_buf(upsfd, dst, dstlen, 1, 1);
	if (ret < 0) {
		upslog_with_errno(LOG_WARNING, "ser_get_buf failed");
		return ret;
	} else if ((size_t) ret != dstlen) {
		upslogx(LOG_WARNING, "ser_get_buf returned %d instead of %d",
			(int)ret, (int)dstlen);
		return 0;
	}

	upsdebug_hex(3, "bicker_receive_buf", dst, dstlen);
	return ret;
}

static ssize_t bicker_receive(char cmd, void *dst, size_t dstlen)
{
	ssize_t ret;
	size_t datalen;
	char buf[BICKER_PACKET];

	ret = bicker_receive_buf(buf, 1);
	if (ret <= 0) {
		return ret;
	} else if (buf[0] != BICKER_SOH) {
		upslogx(LOG_WARNING, "Received 0x%02X instead of SOH (0x%02X)",
			(unsigned)buf[0], (unsigned)BICKER_SOH);
		return 0;
	}

	ret = bicker_receive_buf(buf + 1, 1);
	if (ret <= 0) {
		return ret;
	}

	datalen = buf[1] - 3;

	ret = bicker_receive_buf(buf + 2, datalen + 3);
	if (ret <= 0) {
		return ret;
	} else if (buf[datalen + 4] != BICKER_EOT) {
		upslogx(LOG_WARNING, "Received 0x%02X instead of EOT (0x%02X)",
			(unsigned)buf[datalen + 4], (unsigned)BICKER_EOT);
		return 0;
	} else if (buf[3] != cmd) {
		upslogx(LOG_WARNING, "Commands do not match: sent 0x%02X, received 0x%02X",
			(unsigned)cmd, (unsigned)buf[1]);
		return 0;
	} else if (dstlen < datalen) {
		upslogx(LOG_ERR, "Not enough space for the payload: %d < %d",
			(int)dstlen, (int)datalen);
		return 0;
	}

	memcpy(dst, buf + 4, datalen);

	upsdebug_hex(3, "bicker_receive", buf, datalen + 5);
	return datalen;
}

static ssize_t bicker_read_int16(char cmd, int16_t *dst)
{
	ssize_t ret;

	ret = bicker_send(cmd, NULL);
	if (ret <= 0) {
		return ret;
	}

	/* XXX: by default, data is stored in little-endian so,
	 * on big-endian platforms, a byte swap should be needed */
	return bicker_receive(cmd, dst, 2);
}

void upsdrv_initinfo(void)
{
	dstate_setinfo("device.type", "ups");
	dstate_setinfo("device.mfr", "Bicker Elektronik GmbH");
	/* No way to detect the UPS model within the protocol but the
	 * serial communication is provided by the PSZ-1063 extension
	 * module, so it seems correct to put that into the model */
	dstate_setinfo("device.model", "PSZ-1063");
}

void upsdrv_updateinfo(void)
{
	int16_t data, charge_current;
	ssize_t ret;

	ret = bicker_read_int16('\x25', &data);
	if (ret <= 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("input.voltage", "%.1f", (double) data / 1000);

	ret = bicker_read_int16('\x28', &data);
	if (ret <= 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("input.current", "%.3f", (double) data / 1000);

	ret = bicker_read_int16('\x27', &data);
	if (ret <= 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("output.voltage", "%.3f", (double) data / 1000);

	/* This is a supercap UPS so, in this context,
	 * the "battery" is the supercap stack */
	ret = bicker_read_int16('\x26', &data);
	if (ret <= 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("battery.voltage", "%.3f", (double) data / 1000);

	ret = bicker_read_int16('\x29', &charge_current);
	if (ret <= 0) {
		dstate_datastale();
		return;
	}
	dstate_setinfo("battery.current", "%.3f", (double) charge_current / 1000);

	/* GetChargeStaturRegister returns a 16 bit register:
	 *
	 *  0. SD Shows that the device is in step-down (charging) mode.
	 *  1. SU Shows that the device is in step-up (backup) mode.
	 *  2. CV Shows that the charger is in constant voltage mode.
	 *  3. UV Shows that the charger is in undervoltage lockout.
	 *  4. CL Shows that the device is in input current limit.
	 *  5. CG Shows that the capacitor voltage is above power good threshold.
	 *  6. CS Shows that the capacitor manager is shunting.
	 *  7. CB Shows that the capacitor manager is balancing.
	 *  8. CD Shows that the charger is temporarily disabled for capacitance measurement.
	 *  9. CC Shows that the charger is in constant current mode.
	 * 10. RV Reserved Bit
	 * 11. PF Shows that the input voltage is below the Power Fail Input (PFI) threshold.
	 * 12. RV Reserved Bit
	 * 13. RV Reserved Bit
	 * 14. RV Reserved Bit
	 * 15. RV Reserved Bit
	 */
	ret = bicker_read_int16('\x1B', &data);
	if (ret <= 0) {
		dstate_datastale();
		return;
	}

	status_init();

	/* Check PF (bit 11) to know if the UPS is on line/battery */
	status_set((data & 0x0800) > 0 ? "OB" : "OL");

	/* Check CG (bit 5) to know if the battery is low */
	if ((data & 0x0020) == 0) {
		status_set("LB");
	}

	/* If there is a current of more than 1 A flowing towards
	 * the supercaps, consider the battery in charging mode */
	if (charge_current > 1000) {
		status_set("CHRG");
	}

	status_commit();

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	upslogx(LOG_ERR, "shutdown not supported");
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
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B38400);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
