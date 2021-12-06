/*
 * huawei-ups2000.c - Driver for Huawei UPS2000 (1kVA-3kVA)
 *
 * Note: Huawei UPS2000 (1kVA-3kVA) can be accessed via RS-232,
 * USB, or an optional RMS-MODBUS01B (RS-485) adapter. Only
 * RS-232 and USB are supported, RS-485 is not.
 *
 * The USB port on the UPS is implemented via a MaxLinear RX21V1410
 * USB-to-serial converter, and can be recongized as a standard
 * USB-CDC serial device. Unfortunately, the generic USB-CDC driver
 * is incompatible with the specific chip configuration and cannot
 * be used. A device-specific driver, "xr_serial", must be used.
 *
 * The driver has only been merged to Linux 5.12 or later, via the
 * "xr_serial" kernel module. When the UPS2000 is connected via USB
 * to a supported Linux system, you should see the following logs in
 * "dmesg".
 *
 *     xr_serial 1-1.2:1.1: xr_serial converter detected
 *     usb 1-1.2: xr_serial converter now attached to ttyUSB0
 *
 * The driver must be "xr_serial". If your system doesn't have the
 * necessary device driver, you will get this message instead:
 *
 *     cdc_acm 1-1.2:1.0: ttyACM0: USB ACM device
 *
 * On other operating systems, USB cannot be used due to the absence
 * of the driver. You must use connect UPS2000 to your computer via
 * RS-232, either directly or using an USB-to-RS-232 converter supported
 * by your Linux or BSD kernel.
 *
 * A document describing the protocol implemented by this driver can
 * be found online at:
 *
 *   https://support.huawei.com/enterprise/en/doc/EDOC1000110696
 *
 * Huawei UPS2000 driver implemented by
 *   Copyright (C) 2020, 2021 Yifeng Li <tomli@tomli.me>
 *   The author is not affiliated to Huawei or other manufacturers.
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

#include <stdbool.h>
#include <modbus.h>
#include "main.h"
#include "serial.h"

#define DRIVER_NAME	"NUT Huawei UPS2000 (1kVA-3kVA) RS-232 Modbus driver"
#define DRIVER_VERSION	"0.01"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define MODBUS_SLAVE_ID 1

/*
 * Known UPS models. We only attempt to load the driver if
 * the initial communication indicates the UPS is a known
 * model of the UPS2000 series.
 */
static const char *supported_model[] = {
	"UPS2000", "UPS2000A",
	NULL
};

/*
 * UPS2000 device identification. The information is obtained during
 * initial communication using Modbus command 0x2B (read device identi-
 * fication) to read the object 0x87 (device list). The object contains
 * a list of fields, each with a type, length, and value. The object is
 * parsed by ups2000_device_identification() and filled into the array
 * of struct ups2000_ident.
 *
 * Fields of interest are:
 *
 *     0x87, int32 (Device Count): Only one UPS unit is supported,
 *     the driver aborts if more than one device is detected.
 *
 *     0x88, string (Device Description of the 1st unit): This is a
 *     ASCII string that contains information about the 1st UPS unit.
 *     This string, again, contains a list of fields. They are parsed
 *     further into the array ups2000_desc.
 *
 */
#define UPS2000_IDENT_MAX_FIELDS 9
#define UPS2000_IDENT_MAX_LEN 128
#define UPS2000_IDENT_OFFSET
static struct {
	uint8_t type;
	uint8_t len;
	uint8_t val[UPS2000_IDENT_MAX_LEN];
} ups2000_ident[UPS2000_IDENT_MAX_FIELDS];

/*
 * UPS2000 device description. The information is initially obtained
 * as field 0x88 in the UPS2000 device identification. This field is
 * a semicolon seperated ASCII string that contains multiple fields.
 * It is parsed again by ups2000_device_identification() and filled
 * into the ups2000_desc[] 2D array. The first dimension is used as
 * a key to select the wanted field (defined in the following enmu,
 * the second dimension is a NULL-terminated ASCII string.
 *
 * Note that ups2000_desc[0] is deliberately unused, the array begins
 * at one, allowing mapping from UPS2000_DESC_* to ups2000_desc[]
 * directly without using offsets.
 */
#define UPS2000_DESC_MAX_FIELDS 9
#define UPS2000_DESC_MAX_LEN 128
enum {
	UPS2000_DESC_MODEL = 1,
	UPS2000_DESC_FIRMWARE_REV,
	UPS2000_DESC_PROTOCOL_REV,
	UPS2000_DESC_ESN,
	UPS2000_DESC_DEVICE_ID,    /* currently unused */
	UPS2000_DESC_PARALLEL_ID   /* currently unused */
};
static char ups2000_desc[UPS2000_DESC_MAX_FIELDS][UPS2000_DESC_MAX_LEN] = { { 0 } };

/* global variable for modbus communication */
static modbus_t *modbus_ctx = NULL;

/*
 * How many seconds to wait before switching off/on/reboot the UPS?
 *
 * This can be set at startup time via a command-line argument,
 * or at runtime by writing to RW variables "ups.delay.shutdown"
 * and "ups.delay.start". See ups2000_delay_get/set.
 */
#define UPS2000_DELAY_INVALID 0xFFFF
static uint16_t ups2000_offdelay = UPS2000_DELAY_INVALID;
static uint16_t ups2000_ondelay  = UPS2000_DELAY_INVALID;
static uint16_t ups2000_rebootdelay = UPS2000_DELAY_INVALID;

/*
 * Time when the current shutdown/reboot request is expected
 * to complete. This is used to calculate the ETA, See
 * ups2000_update_timers().
 */
static time_t shutdown_at = 0;
static time_t reboot_at = 0;
static time_t start_at = 0;

/*
 * Is it safe to enter bypass mode? It's checked by ups2000_update_alarm()
 * and used by ups2000_instcmd_bypass_start().
 */
static bool bypass_available = 0;

/* function prototypes */
static int ups2000_update_info(void);
static int ups2000_update_status(void);
static int ups2000_update_alarm(void);
static int ups2000_update_timers(void);
static void ups2000_device_identification(void);
static size_t ups2000_read_serial(uint8_t *buf, size_t buf_len);
static int ups2000_read_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest);
static int ups2000_write_register(modbus_t *ctx, int addr, uint16_t val);
static int ups2000_write_registers(modbus_t *ctx, int addr, int nb, uint16_t *src);
static uint16_t crc16(uint8_t *buffer, uint16_t buffer_length);
static time_t time_seek(time_t t, int seconds);

/* rw variables function prototypes */
static int ups2000_update_rw_var(void);
static int setvar(const char *name, const char *val);
static int ups2000_autostart_set(const uint16_t reg, const char *string);
static int ups2000_autostart_get(const uint16_t reg);
static int ups2000_beeper_set(const uint16_t reg, const char *string);
static int ups2000_beeper_get(const uint16_t reg);
static void ups2000_delay_get(void);
static int ups2000_delay_set(const char *var, const char *string);

/* instant command function prototypes */
static void ups2000_init_instcmd(void);
static int instcmd(const char *cmd, const char *extra);
static int ups2000_instcmd_load_on(const uint16_t reg);
static int ups2000_instcmd_bypass_start(const uint16_t reg);
static int ups2000_instcmd_beeper_toggle(const uint16_t reg);
static int ups2000_instcmd_shutdown_stayoff(const uint16_t reg);
static int ups2000_instcmd_shutdown_return(const uint16_t reg);
static int ups2000_instcmd_shutdown_reboot(const uint16_t reg);
static int ups2000_instcmd_shutdown_reboot_graceful(const uint16_t reg);

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Yifeng Li <tomli@tomli.me>\n",
	DRV_EXPERIMENTAL,
	{ NULL }
};


void upsdrv_initups(void)
{
	int r;

	upsdebugx(2, "upsdrv_initups");

	/*
	 * This is an ugly workaround to a serious problem: libmodbus doesn't
	 * support device identification. Although there's a function called
	 * modbus_send_raw_request() for custom commands, but modbus_receive_
	 * confirmation() assumes a message length in the header, which is
	 * incompatible with device identification - It simply stops reading
	 * in the middle of the message and cannot receive our message. Worse,
	 * there's no public API to receive a raw response.
	 *
	 * See: https://github.com/stephane/libmodbus/issues/231
	 *
	 * Thus, the only thing we could do is opening it as a serial device
	 * for device identification, and reopen it via libmodbus for other
	 * commands as usual. We also have to copy the CRC-16 function from
	 * the libmodbus source code since there's no public API to use that...
	 */
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);
	ser_set_rts(upsfd, 0);
	ser_set_dtr(upsfd, 0);

	modbus_ctx = modbus_new_rtu(device_path, 9600, 'N', 8, 1);
	if (modbus_ctx == NULL)
		fatalx(EXIT_FAILURE, "Unable to create the libmodbus context");

#if LIBMODBUS_VERSION_CHECK(3, 1, 2)
	/* It can take as slow as 1 sec. for the UPS to respond. */
	modbus_set_response_timeout(modbus_ctx, 1, 0);
#else
	{
		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		modbus_set_response_timeout(modbus_ctx, &timeout);
	}
#endif

	r = modbus_set_slave(modbus_ctx, MODBUS_SLAVE_ID);
	if (r < 0) {
		modbus_free(modbus_ctx);
		fatalx(EXIT_FAILURE, "Invalid slave ID %d", MODBUS_SLAVE_ID);
	}

	if (modbus_connect(modbus_ctx) == -1) {
		modbus_free(modbus_ctx);
		fatalx(EXIT_FAILURE, "modbus_connect: unable to connect: %s", modbus_strerror(errno));
	}
}


#define IDENT_REQUEST_LEN 7
#define IDENT_RESPONSE_MAX_LEN 128
#define IDENT_RESPONSE_HEADER_LEN 8
#define IDENT_RESPONSE_CRC_LEN 2
#define IDENT_FIELD_HEADER_LEN 2
static void ups2000_device_identification(void)
{
	static const uint8_t ident_req[IDENT_REQUEST_LEN] = {
		MODBUS_SLAVE_ID,  /* addr */
		0x2B,             /* command: device identification */
		0x0E,             /* MEI type */
		0x03,             /* ReadDevID: extended identification */
		0x87,             /* Object ID: device list */
		0x31, 0x75        /* CRC-16 */
	};

	/*
	 * Response header:
	 * 	0x01, 0x2B, 0x0E, 0x03, 0x03, 0x00, 0x00, 0x02
	 *
	 * Response fields:
	 * 	header: 0x87, 0x04  // type (device counts), length
	 * 	data:   uint32_t
	 * 	(e.g. 0x00, 0x00, 0x00, 0x01)
	 *
	 * 	header: 0x88, 0x??  // type (1st dev desc), length
	 * 	data:   ASCII string
	 * 	(e.g. 1=UPS2000;2=V100R001C01SPC120;3=...)
	 *
	 * 	header: 0x89, 0x??  // type (2nd dev desc), length
	 * 	data:   ASCII string
	 *
	 * 	...
	 * 	header: 0xFF, 0x??  // type (120th dev desc), length
	 * 	data:   ASCII string
	 *
	 * CRC-16:
	 * 	0x??, 0x??
	 */
	static const uint8_t expected_header[IDENT_RESPONSE_HEADER_LEN] = {
		MODBUS_SLAVE_ID,
		0x2B, 0x0E, 0x03, 0x03, 0x00, 0x00, 0x02,
	};

	bool serial_fail = 0;  /* unable to read from serial */
	uint16_t crc16_recv, crc16_calc;  /* resp CRC */
	bool crc16_fail = 0;  /* resp CRC failure */
	uint32_t ups_count = 0;  /* number of UPS in the resp list */
	uint8_t ident_response[IDENT_RESPONSE_MAX_LEN];  /* resp buf */
	size_t ident_response_len;    /* buf len */
	uint8_t *ident_response_end = NULL;  /* buf end marker (excluding CRC) */
	uint8_t *ptr = NULL;  /* buf iteratior */

	/* a desc string copied from ups2000_ident[] */
	char *ups2000_ident_desc = NULL;
	int i;
	ssize_t r;

	/* attempt to obtain a response header with valid CRC. */
	for (i = 0; i < 3; i++) {
		/* step 1: record response length and initialize ptr */
		upsdebugx(2, "ser_send_buf");

		ser_flush_in(upsfd, "", nut_debug_level);
		r = ser_send_buf(upsfd, ident_req, IDENT_REQUEST_LEN);
		if (r != IDENT_REQUEST_LEN) {
			fatalx(EXIT_FAILURE, "unable to send request!\n");
		}

		ident_response_len = ups2000_read_serial(ident_response, IDENT_RESPONSE_MAX_LEN);
		ptr = ident_response;
		ident_response_end = ptr + ident_response_len - IDENT_RESPONSE_CRC_LEN;

		/* step 2: check response length */
		if (ident_response_len == 0) {
			upslogx(LOG_ERR, "unable to read from serial port %s, retry...", device_path);
			serial_fail = 1;
			continue;
		}
		else
			serial_fail = 0;

		upsdebug_hex(2, "ups2000_read_serial() received", ptr, ident_response_len);

		if (ptr + IDENT_RESPONSE_HEADER_LEN > ident_response_end) {
			fatalx(EXIT_FAILURE, "response header too short! "
					     "expected %d, received %zu.",
					     IDENT_RESPONSE_HEADER_LEN, ident_response_len);
		}

		/* step 3: check response CRC-16 */
		crc16_recv = (uint16_t)((uint16_t)(ident_response_end[0]) << 8) | (uint16_t)(ident_response_end[1]);
		if (ident_response_len < IDENT_RESPONSE_CRC_LEN
		|| (((uintmax_t)(ident_response_len) - IDENT_RESPONSE_CRC_LEN) > UINT16_MAX)
		) {
			fatalx(EXIT_FAILURE, "response header shorter than CRC "
					     "or longer than UINT16_MAX!");
		}

		crc16_calc = crc16(ident_response, (uint16_t)(ident_response_len - IDENT_RESPONSE_CRC_LEN));
		if (crc16_recv == crc16_calc) {
			crc16_fail = 0;
			break;
		}
		crc16_fail = 1;
	}

	/* step 4: check serial & CRC-16 verification status */
	if (serial_fail)
		fatalx(EXIT_FAILURE, "unable to read from serial port %s!", device_path);

	if (crc16_fail)
		fatalx(EXIT_FAILURE, "response CRC verification failed!");

	/* step 5: check response header */
	if (memcmp(expected_header, ident_response, IDENT_RESPONSE_HEADER_LEN))
		fatalx(EXIT_FAILURE, "unexpected response header!");

	ptr += IDENT_RESPONSE_HEADER_LEN;

	/* step 6: extract ident fields */
	memset(ups2000_ident, 0x00, sizeof(ups2000_ident));
	for (i = 0; i < UPS2000_IDENT_MAX_FIELDS; i++) {
		uint8_t type, len;

		if (ptr + 2 > ident_response_end)
			break;

		type = *ptr++;
		len = *ptr++;

		if (len + 1 > UPS2000_IDENT_MAX_LEN)
			fatalx(EXIT_FAILURE, "response field too long!");

		ups2000_ident[i].type = type;
		ups2000_ident[i].len = len;
		/*
		 * Always zero-terminate the bytes, in case the data
		 * is an ASCII string (i.e. device desc string), libc
		 * string functions can be used.
		 */
		ups2000_ident[i].val[len] = '\0';

		if (ptr + len > ident_response_end)
			fatalx(EXIT_FAILURE, "response field too short!");

		memcpy(ups2000_ident[i].val, ptr, len);
		ptr += len;
	}

	/* step 7: validate device identification field 0x87 and 0x88 */
	for (i = 0; i < UPS2000_IDENT_MAX_FIELDS; i++) {
		/* only one device is supported */
		if (ups2000_ident[i].type == 0x87) {
			/* so we assume 0x87 must be 1 */
			ups_count =
				(uint32_t)(ups2000_ident[i].val[0]) << 24 |
				(uint32_t)(ups2000_ident[i].val[1]) << 16 |
				(uint32_t)(ups2000_ident[i].val[2]) << 8  |
				(uint32_t)(ups2000_ident[i].val[3]);
		}
		if (ups2000_ident[i].type == 0x88) {
			/*
			 * And only check 0x88, not 0x89, etc. Also copy the
			 * string for later parsing via strtok().
			 */
			ups2000_ident_desc = strdup((char *) ups2000_ident[i].val);
			break;
		}
	}
	if (ups_count != 1)
		fatalx(EXIT_FAILURE, "only 1 UPS is supported, %u found", ups_count);

	if (!ups2000_ident_desc)
		fatalx(EXIT_FAILURE, "device desc string not found");

	/*
	 * step 8: extract fields from the desc string.
	 * (1=UPS2000;2=V100R001C01SPC120;3=...)
	 */
	for (i = 0; i < UPS2000_DESC_MAX_FIELDS; i++) {
		char *key;  /* "1", "2", "3", ... */
		char *val;  /* "UPS2000", "V100R001C01SPC120", ... */
		unsigned int idx = 0;

		if (i == 0)
			key = strtok(ups2000_ident_desc, "=");
		else
			key = strtok(NULL, "=");
		if (!key)
			break;

		val = strtok(NULL, ";");
		if (!val)
			break;

		r = str_to_uint_strict(key, &idx, 10);
		if (!r || idx + 1 > UPS2000_DESC_MAX_FIELDS || idx < 1)
			fatalx(EXIT_FAILURE, "desc index %d is invalid!", idx);

		if (strlen(val) + 1 > UPS2000_DESC_MAX_LEN)
			fatalx(EXIT_FAILURE, "desc field %d too long!", idx);

		memcpy(ups2000_desc[idx], val, strlen(val) + 1);
	}
	free(ups2000_ident_desc);

	/*
	 * step 9: Validate desc fields that we are going to use are valid.
	 *
	 * Note: UPS2000_DESC_DEVICE_ID and UPS2000_DESC_PARALLEL_ID are
	 * currently unused and unchecked.
	 */
	for (i = UPS2000_DESC_MODEL; i <= UPS2000_DESC_ESN; i++) {
		if (strlen(ups2000_desc[i]) == 0)
			fatalx(EXIT_FAILURE, "desc field %d is missing!", i);
	}
}


void upsdrv_initinfo(void)
{
	bool in_list = 0;
	int i = 0;

	upsdebugx(2, "upsdrv_initinfo");

	ups2000_device_identification();

	/* check whether the UPS is a known model */
	for (i = 0; supported_model[i] != NULL; i++) {
		if (!strcmp(supported_model[i],
			    ups2000_desc[UPS2000_DESC_MODEL])) {
			in_list = 1;
		}
	}
	if (!in_list) {
		fatalx(EXIT_FAILURE, "Unknown UPS model %s",
				     ups2000_desc[UPS2000_DESC_MODEL]);
	}

	dstate_setinfo("device.mfr", "Huawei");
	dstate_setinfo("device.type", "ups");
	dstate_setinfo("device.model", "%s",
		       ups2000_desc[UPS2000_DESC_MODEL]);
	dstate_setinfo("device.serial", "%s",
		       ups2000_desc[UPS2000_DESC_ESN]);

	dstate_setinfo("ups.mfr", "Huawei");
	dstate_setinfo("ups.model", "%s",
		       ups2000_desc[UPS2000_DESC_MODEL]);
	dstate_setinfo("ups.firmware", "%s",
		       ups2000_desc[UPS2000_DESC_FIRMWARE_REV]);
	dstate_setinfo("ups.firmware.aux", "%s",
		       ups2000_desc[UPS2000_DESC_PROTOCOL_REV]);
	dstate_setinfo("ups.serial", "%s",
		       ups2000_desc[UPS2000_DESC_ESN]);
	dstate_setinfo("ups.type", "online");

	/* RW variables */
	upsh.setvar = setvar;

	/* instant commands */
	ups2000_init_instcmd();
	upsh.instcmd = instcmd;
}


/*
 * All registers are uint16_t. But the data they represent can
 * be either an integer or a float. This information is used for
 * error checking (int and float have different invalid values).
 */
enum {
	REG_UINT16,
	REG_UINT32, /* occupies two registers */
	REG_FLOAT,  /* actually a misnomer, it should really be called
		       fixed-point number, but we follow the datasheet */
};
#define REG_UINT16_INVALID 0xFFFFU
#define REG_UINT32_INVALID 0xFFFFFFFFU
#define REG_FLOAT_INVALID  0x7FFFU


/*
 * Declare UPS attribute variables, format strings, registers,
 * and their scaling factors in a lookup table to avoid spaghetti
 * code.
 */
static struct {
	const char *name;
	const char *fmt;
	const uint16_t reg;
	const int datatype;    /* only UINT32 occupies 2 regs */
	const float scaling;   /* scale it down to get the original */
} ups2000_var[] =
{
	{ "input.voltage",          "%03.1f", 1000, REG_FLOAT,  10.0  },
	{ "input.frequency",        "%02.1f", 1003, REG_FLOAT,  10.0  },
	{ "input.bypass.voltage",   "%03.1f", 1004, REG_FLOAT,  10.0  },
	{ "input.bypass.frequency", "%03.1f", 1007, REG_FLOAT,  10.0  },
	{ "output.voltage",         "%03.1f", 1008, REG_FLOAT,  10.0  },
	{ "output.current",         "%03.1f", 1011, REG_FLOAT,  10.0  },
	{ "output.frequency",       "%03.1f", 1014, REG_FLOAT,  10.0  },
	{ "output.realpower",       "%02.1f", 1015, REG_FLOAT,   0.01 }, /* 10 / 1 kW */
	{ "output.power",           "%03.1f", 1018, REG_FLOAT,   0.01 }, /* 10 / 1 kVA */
	{ "ups.load",               "%02.1f", 1021, REG_FLOAT,  10.0  },
	{ "ups.temperature",        "%02.1f", 1027, REG_FLOAT,  10.0  },
	{ "battery.voltage",        "%02.1f", 2000, REG_FLOAT,  10.0  },
	{ "battery.charge",         "%02.1f", 2003, REG_UINT16,  1.0  },
	{ "battery.runtime",        "%.0f",   2004, REG_UINT32,  1.0  },
	{ "battery.packs",          "%.0f",   2007, REG_UINT16,  1.0  },
	{ "battery.capacity",       "%.0f",   2033, REG_UINT16,  1.0  },
	{ "ups.power.nominal",      "%.0f",   9009, REG_FLOAT,   0.01 }, /* 10 / 1 kVA */
	{ NULL, NULL, 0, 0, 0 },
};


static int ups2000_update_info(void)
{
	uint16_t reg[3][34];
	int i;
	int r;

	upsdebugx(2, "ups2000_update_info");

	/*
	 * All status registers have an offset of 10000 * ups_number.
	 * We only support 1 UPS, thus it's always 10000. Register
	 * 1000 becomes 11000.
	 */
	r = ups2000_read_registers(modbus_ctx, 11000, 28, reg[0]);
	if (r != 28)
		return 1;

	r = ups2000_read_registers(modbus_ctx, 12000, 34, reg[1]);
	if (r != 34)
		return 1;

	r = ups2000_read_registers(modbus_ctx, 19009, 1, &reg[2][9]);
	if (r != 1)
		return 1;

	for (i = 0; ups2000_var[i].name != NULL; i++) {
		uint16_t reg_id = ups2000_var[i].reg;
		uint8_t page = (uint8_t)(reg_id / 1000 - 1);
		uint8_t idx =  (uint8_t)(reg_id % 1000);
		uint32_t val;
		bool invalid = 0;

		if (page == 8)  /* hack for the lonely register 9009 */
			page = 2;

		if (page > 2 || idx > 33)  /* also suppress compiler warn */
			fatalx(EXIT_FAILURE, "register calculation overflow!\n");

		switch (ups2000_var[i].datatype) {
		case REG_FLOAT:
			val = reg[page][idx];
			if (val == REG_FLOAT_INVALID)
				invalid = 1;
			break;
		case REG_UINT16:
			val = reg[page][idx];
			if (val == REG_UINT16_INVALID)
				invalid = 1;
			break;
		case REG_UINT32:
			val  = reg[page][idx] << 16;
			val |= reg[page][idx + 1];
			if (val == REG_UINT32_INVALID)
				invalid = 1;
			break;
		default:
			fatalx(EXIT_FAILURE, "invalid data type in register table!\n");
		}

		if (invalid) {
			upslogx(LOG_ERR, "register %04d has invalid value %04x,", reg_id, val);
			return 1;
		}

		dstate_setinfo(ups2000_var[i].name, ups2000_var[i].fmt,
			       (float) val / ups2000_var[i].scaling);
	}
	return 0;
}


/*
 * A lookup table of all the status registers and the list of
 * corresponding flags they represent. A register may set multiple
 * status flags, represented by an array of flags_t.
 *
 * There are two types of flags. If the flag is a "status flag"
 * for status_set(), for example, "OL" or "OB", the field
 * "status_name" is used. If the flag is a "data variable" for
 * dstate_setinfo(), the variable name and value is written in
 * "var_name" and "var_val" fields.
 *
 * For each flag, if it's indicated by a specific value in a
 * register, the "val" field is used. If a flag is indicated by
 * a bit, the "bit" field should be used. Fields "val" and "bit"
 * cannot be used at the same time, at least one must be "-1".
 *
 * Also, some important registers indicate basic system status
 * (e.g. whether the UPS is on line power or battery), this info
 * must always be available, and they are always expected to set
 * at least one flag. If the important register does not set any
 * flag, it means we've received an invalid or unknown value,
 * and we must report an error. The "must_set_flag" field is used
 * for this purpose.
 */
static struct {
	const uint16_t reg;
	bool must_set_flag;
	struct flags_t {
		const char *status_name;
		const int16_t val;
		const int bit;
		const char *var_name, *var_val;
	} flags[10];
} ups2000_status_reg[] =
{
	{ 1024, 1, {
		{ "OFF",      0, -1, NULL, NULL },
		{ "BYPASS",   1, -1, NULL, NULL },
		{ "OL",       2, -1, NULL, NULL },
		{ "OB",       3, -1, NULL, NULL },
		{ "OL ECO",   5, -1, NULL, NULL },
		{ NULL,      -1, -1, NULL, NULL },
	}},
	{ 1043, 0, {
		{ "CAL",     -1,  2, NULL, NULL },  /* battery self-test */
		{ "LB",      -1,  6, NULL, NULL },
		{ NULL,      -1, -1, NULL, NULL },
	}},
	/*
	 * Note: 3 = float charging, 4 = equalization charging, but
	 * both of them are reported as "charging", not "floating".
	 * The definition of "floating" in NUT is: "battery has
	 * completed its charge cycle, and waiting to go to resting
	 * mode", which is not true for UPS2000.
	 */
	{ 2002, 1, {
		{ "",         2, -1, "battery.charger.status", "resting"     },
		{ "CHRG",     3, -1, "battery.charger.status", "charging"    },
		{ "CHRG",     4, -1, "battery.charger.status", "charging"    },
		{ "DISCHRG",  5, -1, "battery.charger.status", "discharging" },
		{ NULL,      -1, -1, NULL, NULL },
	}},
	{ 0, 0, { { NULL, -1, -1, NULL, NULL } } }
};


static int ups2000_update_status(void)
{
	int i, j;
	int r;

	upsdebugx(2, "ups2000_update_status");

	for (i = 0; ups2000_status_reg[i].reg != 0; i++) {
		uint16_t reg, val;
		struct flags_t *flag;
		int flag_count = 0;

		reg = ups2000_status_reg[i].reg;
		r = ups2000_read_registers(modbus_ctx, reg + 10000, 1, &val);
		if (r != 1)
			return 1;

		if (val == REG_UINT16_INVALID) {
			upslogx(LOG_ERR, "register %04d has invalid value %04x,", reg, val);
			return 1;
		}

		flag = ups2000_status_reg[i].flags;
		for (j = 0; flag[j].status_name != NULL; j++) {
			/*
			 * if the register is equal to the "val" we are looking
			 * for, or if register has its n-th "bit" set...
			 */
			if ((flag[j].val != -1 && flag[j].val == val) ||
			    (flag[j].bit != -1 && CHECK_BIT(val, flag[j].bit))) {
				/* if it has a corresponding status flag */
				if (strlen(flag[j].status_name) != 0)
					status_set(flag[j].status_name);
				/* or if it has a corresponding dstate variable (or both) */
				if (flag[j].var_name && flag[j].var_val)
					dstate_setinfo(flag[j].var_name, "%s", flag[j].var_val);
				flag_count++;
			}
		}
		if (ups2000_status_reg[i].must_set_flag && flag_count == 0) {
			upslogx(LOG_ERR, "register %04d has invalid value %04x,", reg, val);
			return 1;
		}
	}

	return 0;
}


/*
 * A lookup table of all the alarm registers and the list of
 * corresponding alarms they represent. Each alarm condition is
 * listed by its register base address "reg" and its "bit"
 * position.
 *
 * Each alarm condition has an "alarm_id", "alarm_cause_id",
 * and "alarm_name". In addition, a few alarms conditions also
 * indicates conditions related to batteries that is needed to
 * be set via status_set(), those are listed in "status_name".
 * Unused "status_name" is set to NULL.
 *
 * After an alarm is reported/cleared by the UPS, the "active"
 * flag is changed to reflect its status. The error logging
 * code uses this variable to issue warnings only when needed
 * (i.e. only after a change, avoid issuing the same warning
 * repeatedly).
 */
#define ALARM_CLEAR_AUTO      1
#define ALARM_CLEAR_MANUAL    2
#define ALARM_CLEAR_DEPENDING 3

static struct {
	bool active;             /* runtime: is this alarm currently active? */
	const uint16_t reg;      /* alarm register to check */
	const int bit;           /* alarm bit to check */
	const int alarm_clear;   /* auto or manual clear */
	const int loglevel;      /* warning or error */
	const int alarm_id, alarm_cause_id;
	const char *status_name; /* corresponding NUT status word */
	const char *alarm_name;  /* alarm string */
	const char *alarm_desc;  /* brief explanation */
} ups2000_alarm[] =
{
	{
		false, 40156, 3, ALARM_CLEAR_AUTO, LOG_ALERT,
		30, 1, NULL, "UPS internal overtemperature",
		"The ambient temperature is over 50-degree C. "
		"Startup from standby mode is prohibited.",
	},
	{
		false, 40161, 1, ALARM_CLEAR_AUTO, LOG_WARNING,
		10, 1, NULL, "Abnormal bypass voltage",
		"Bypass input is unavailable or out-of-range. Wait for "
		"bypass input to recover, or change acceptable bypass "
		"range via front panel.",
	},
	{
		false, 40161, 2, ALARM_CLEAR_AUTO, LOG_WARNING,
		10, 2, NULL, "Abnormal bypass frequency",
		"Bypass input is unavailable or out-of-range. Wait for "
		"bypass input to recover, or change acceptable bypass "
		"range via front panel.",
	},
	{
		false, 40163, 3, ALARM_CLEAR_DEPENDING, LOG_WARNING,
		25, 1, NULL, "Battery overvoltage",
		"When the UPS is started, voltage of each battery exceeds 15 V. "
		"Or: current battery voltage exceeds 14.7 V.",
	},
	{
		false, 40164, 1, ALARM_CLEAR_AUTO, LOG_WARNING,
		29, 1, "RB", "Battery needs maintenance",
		"During the last battery self-check, the battery voltage "
		"was lower than the replacement threshold (11 V).",
	},
	{
		false, 40164, 3, ALARM_CLEAR_AUTO, LOG_WARNING,
		26, 1, NULL, "Battery undervoltage",
		NULL,
	},
	{
		false, 40170, 4, ALARM_CLEAR_AUTO, LOG_ALERT,
		22, 1, NULL, "Battery disconnected",
		"Battery is not connected, has loose connection, or faulty.",
	},
	{
		false, 40173, 5, ALARM_CLEAR_AUTO, LOG_ALERT,
		66, 1, "OVER", "Output overload (105%-110%)",
		"UPS will shut down or transfer to bypass mode in 5-10 minutes.",
	},
	{
		false, 40173, 3, ALARM_CLEAR_AUTO, LOG_ALERT,
		66, 2, "OVER", "Output overload (110%-130%)",
		"UPS will shut down or transfer to bypass mode in 30-60 seconds.",
	},
	{
		false, 40174, 0, ALARM_CLEAR_DEPENDING, LOG_ALERT,
		14, 1, NULL, "UPS startup timeout",
		"The inverter output voltage is not within +/- 2 V of the "
		"rated output. Or: battery is overdischarged.",
	},
	{
		false, 40179, 14, ALARM_CLEAR_MANUAL, LOG_ALERT,
		42, 15, NULL, "Rectifier fault (internal fault)",
		"Bus voltage is lower than 320 V.",
	},
	{
		false, 40179, 15, ALARM_CLEAR_MANUAL, LOG_ALERT,
		42, 17, NULL, "Rectifier fault (internal fault)",
		"Bus voltage is higher than 450 V.",
	},
	{
		false, 40180, 1, ALARM_CLEAR_MANUAL, LOG_ALERT,
		42, 18, NULL, "Rectifier fault (internal fault)",
		"Bus voltage is lower than 260 V.",
	},
	{
		false, 40180, 5, ALARM_CLEAR_AUTO, LOG_ALERT,
		42, 24, NULL, "EEPROM fault (internal fault)",
		"Faulty EEPROM. All settings are restored to "
		"factory default and cannot be saved.",
	},
	{
		false, 40180, 6, ALARM_CLEAR_MANUAL, LOG_ALERT,
		42, 27, NULL, "Inverter fault (internal fault)",
		"Inverter output overvoltage, undervoltage or "
		"undercurrent.",
	},
	{
		false, 40180, 7, ALARM_CLEAR_DEPENDING, LOG_ALERT,
		42, 28, NULL, "Inverter fault (internal fault)",
		"The inverter output voltage is lower than 100 V.",
	},
	{
		false, 40180, 10, ALARM_CLEAR_MANUAL, LOG_ALERT,
		42, 31, NULL, "Inverter fault (internal fault)",
		"The difference between the absolute value of the positive bus "
		"voltage and that of the negative bus voltage is 100 V.",
	},
	{
		false, 40180, 11, ALARM_CLEAR_DEPENDING, LOG_ALERT,
		42, 32, NULL, "UPS internal overtemperature",
		"The ambient temperature is over 50 degree C, "
		"switching to bypass mode.",

	},
	{
		false, 40180, 13, ALARM_CLEAR_MANUAL, LOG_ALERT,
		42, 36, NULL, "Charger fault (internal fault)",
		"The charger has no output. Faulty internal connections.",

	},
	{
		false, 40182, 4, ALARM_CLEAR_MANUAL, LOG_ALERT,
		42, 42, NULL, "Charger fault (internal fault)",
		"The charger has no output while the inverter is on, "
		"battery undervoltage. Faulty switching transistor.",
	},
	{
		false, 40182, 13, ALARM_CLEAR_MANUAL, LOG_ALERT,
		66, 3, "OVER", "Output overload shutdown",
		"UPS has shutdown or transferred to bypass mode.",
	},
	{
		false, 40182, 14, ALARM_CLEAR_MANUAL, LOG_ALERT,
		66, 4, "OVER", "Bypass output overload shutdown",
		"UPS has shutdown, bypass output was overload and exceeded "
		"time limit.",
	},
	{ false, 0, -1, -1, -1, -1, -1, NULL, NULL, NULL }
};


/* don't spam the syslog */
static time_t alarm_logged_since = 0;
#define UPS2000_LOG_INTERVAL 600  /* 10 minutes */


static int ups2000_update_alarm(void)
{
	uint16_t val[27];
	int i;
	int r;

	char alarm_buf[128];
	size_t all_alarms_len = 0;

	int alarm_count = 0;
	bool alarm_logged = 0;
	bool alarm_rtfm = 0;
	time_t now = time(NULL);

	upsdebugx(2, "ups2000_update_alarm");

	/*
	 * All alarm registers have an offset of 1024 * ups_number.
	 * We only support 1 UPS, it's always 1024.
	 */
	r = ups2000_read_registers(modbus_ctx, ups2000_alarm[0].reg + 1024, 27, val);
	if (r != 27)
		return 1;

	bypass_available = 1;  /* register 40161 hack, see comments below */

	for (i = 0; ups2000_alarm[i].alarm_id != -1; i++) {
		int idx = ups2000_alarm[i].reg - ups2000_alarm[0].reg;
		if (idx > 26 || idx < 0)
			fatalx(EXIT_FAILURE, "register calculation overflow!\n");

		if (CHECK_BIT(val[idx], ups2000_alarm[i].bit)) {
			if (ups2000_alarm[i].reg == 40161)
				/*
				 * HACK: special treatment for register 40161. If this
				 * register indicates an alarm, we need to lock the
				 * "bypass.on" command as a software foolproof mechanism.
				 * It's written to the global "bypass_available" flag.
				 */
				bypass_available = 0;

			alarm_count++;

			all_alarms_len += snprintf(alarm_buf, 128, "(ID %02d/%02d): %s!",
						   ups2000_alarm[i].alarm_id,
						   ups2000_alarm[i].alarm_cause_id,
						   ups2000_alarm[i].alarm_name);
			alarm_set(alarm_buf);

			if (ups2000_alarm[i].status_name)
				status_set(ups2000_alarm[i].status_name);

			/*
			 * Log the warning only if it's a new alarm, or if a long time
			 * has paseed since we first warned it.
			 */
			if (!ups2000_alarm[i].active ||
			    difftime(now, alarm_logged_since) >= UPS2000_LOG_INTERVAL) {
				int loglevel;
				const char *alarm_word;

				/*
				 * Most text editors have syntax highlighting, adding an
				 * alarm word makes the log more readable
				 */
				loglevel = ups2000_alarm[i].loglevel;
				if (loglevel <= LOG_ERR) {
					alarm_word = "ERROR";
					/*
					 * If at least one error is serious, suggest reading
					 * manual.
					 */
					alarm_rtfm = 1;
				}
				else {
					alarm_word = "WARNING";
				}

				upslogx(loglevel, "%s: alarm %02d, Cause %02d: %s!",
						  alarm_word,
						  ups2000_alarm[i].alarm_id,
						  ups2000_alarm[i].alarm_cause_id,
						  ups2000_alarm[i].alarm_name);

				if (ups2000_alarm[i].alarm_desc)
					upslogx(loglevel, "%s", ups2000_alarm[i].alarm_desc);

				switch (ups2000_alarm[i].alarm_clear) {
				case ALARM_CLEAR_AUTO:
					upslogx(loglevel, "This alarm can be auto cleared.");
					break;
				case ALARM_CLEAR_MANUAL:
					upslogx(loglevel, "This alarm can only be manual cleared "
							  "via front panel.");
					break;
				case ALARM_CLEAR_DEPENDING:
					upslogx(loglevel, "This alarm is auto or manual cleared "
							  "depending on the specific problem.");
				}

				ups2000_alarm[i].active = 1;
				alarm_logged = 1;
			}

		}
		else {
			if (ups2000_alarm[i].active) {
				upslogx(LOG_WARNING, "Cleared alarm %02d, Cause %02d: %s",
						     ups2000_alarm[i].alarm_id,
						     ups2000_alarm[i].alarm_cause_id,
						     ups2000_alarm[i].alarm_name);
				ups2000_alarm[i].active = 0;
				alarm_logged = 1;
			}
		}

	}

	if (alarm_count > 0) {
		/* append this to the alarm string as a friendly reminder */
		all_alarms_len += snprintf(alarm_buf, 128, "Check log for details!");
		alarm_set(alarm_buf);

		/* if the alarm string is too long, replace it with this */
		if (all_alarms_len + 1 > ST_MAX_VALUE_LEN) {
			alarm_init();  /* discard all original alarms */
			snprintf(alarm_buf, 128, "UPS has %d alarms in effect, "
						 "check log for details!", alarm_count);
			alarm_set(alarm_buf);
		}

		/*
		 * If we are doing a syslog, write the final message and refresh the
		 * do-not-spam-the-log timer "alarm_logged_since".
		 */
		if (alarm_logged) {
			upslogx(LOG_WARNING, "UPS has %d alarms in effect.", alarm_count);
			if (alarm_rtfm)
				upslogx(LOG_WARNING, "Read Huawei User Manual for "
						     "troubleshooting information.");
			alarm_logged_since = time(NULL);
		}
	}
	else {
		upsdebugx(2, "UPS has 0 alarms in effect.");

		if (alarm_logged) {
			upslogx(LOG_WARNING, "UPS has cleared all alarms.");
			alarm_logged_since = time(NULL);
		}
	}
	return 0;
}


void upsdrv_updateinfo(void)
{
	int err = 0;

	upsdebugx(2, "upsdrv_updateinfo");
	status_init();
	alarm_init();

	err += ups2000_update_timers();
	err += ups2000_update_alarm();
	err += ups2000_update_info();
	err += ups2000_update_status();
	err += ups2000_update_rw_var();

	if (err > 0) {
		upsdebugx(2, "upsdrv_updateinfo failed, data stale.");
		dstate_datastale();
		return;
	}

	alarm_commit();
	status_commit();
	dstate_dataok();
	upsdebugx(2, "upsdrv_updateinfo done");
}


/*
 * A lookup table of simple RW (configurable) variable "name", and their
 * "getter" and "setter". A "getter" function reads the variable from
 * the UPS, and a "setter" overwrites it.
 *
 * This struct only handles simple variables, delays are handled in another
 * table.
 */
static struct {
	const char *name;
	const uint16_t reg;
	int (*const getter)(const uint16_t);
	int (*const setter)(const uint16_t, const char *);
} ups2000_rw_var[] =
{
	{ "ups.start.auto",     1044, ups2000_autostart_get, ups2000_autostart_set },
	{ "ups.beeper.status",  1046, ups2000_beeper_get,    ups2000_beeper_set    },
	{ NULL, 0, NULL, NULL },
};


/*
 * A specialized lookup table of startup, reboot and shutdown delays,
 * represented by RW variables.
 */
static struct ups2000_delay_t {
	const char *name;             /* RW variable name */
	uint16_t *const global_var;   /* its corresponding global variable */
	const char *varname_cmdline;  /* cmdline argument passed to us */
	const uint16_t min;           /* minimum value allowed (seconds) */
	const uint16_t max;           /* maximum value allowed (seconds) */
	const uint8_t step;           /* can only be set in discrete steps */
	const uint16_t dfault;        /* default value */
} ups2000_rw_delay[] =
{
	/* 5940 = 99 min. */
	{ "ups.delay.shutdown", &ups2000_offdelay,    "offdelay",     6, 5940,  6, 60 },
	{ "ups.delay.reboot",   &ups2000_rebootdelay, "rebootdelay",  6, 5940,  6, 60 },
	{ "ups.delay.start",    &ups2000_ondelay,     "ondelay",     60, 5940, 60, 60 },
	{ NULL, NULL, NULL, 0, 0, 0, 0 },
};
enum {
	SHUTDOWN,
	REBOOT,
	START
};


static int ups2000_update_rw_var(void)
{
	int i;
	int r;

	upsdebugx(2, "ups2000_update_rw_var");

	for (i = 0; ups2000_rw_var[i].name != NULL; i++) {
		r = ups2000_rw_var[i].getter(ups2000_rw_var[i].reg);
		if (r != 0)
			return 1;
	}

	ups2000_delay_get();

	return 0;
}


static int setvar(const char *name, const char *val)
{
	int i;
	int r;

	for (i = 0; ups2000_rw_var[i].name != NULL; i++) {
		if (!strcasecmp(ups2000_rw_var[i].name, name)) {
			r = ups2000_rw_var[i].setter(ups2000_rw_var[i].reg, val);
			goto found;
		}
	}

	for (i = 0; ups2000_rw_delay[i].name != NULL; i++) {
		if (!strcasecmp(ups2000_rw_delay[i].name, name)) {
			r = ups2000_rw_var[i].setter(ups2000_rw_var[i].reg, val);
			goto found;
		}
	}

	return STAT_SET_UNKNOWN;

found:
	if (r == STAT_SET_FAILED)
		upslogx(LOG_ERR, "setvar: setting variable [%s] to [%s] failed", name, val);
	else if (r == STAT_SET_INVALID)
		upslogx(LOG_WARNING, "setvar: [%s] is not valid for variable [%s]", val, name);
	return r;
}


static int ups2000_autostart_get(const uint16_t reg)
{
	/*
	 * "ups.start.auto" is not supported because it overcomplicates
	 * the logic. The driver changes "ups.start.auto" internally to
	 * allow shutdown and reboot commands to do their jobs. If we make
	 * "ups.start.auto" an user configuration, it means we must (1)
	 * watch for UPS front panel updates and apply the user setting to
	 * the driver, and (2) save the restart setting temporally before
	 * restarting, track the UPS restart process, and program the value
	 * back later.
	 *
	 * Not supporting it greatly simplifies the logic - upsdrv_shutdown
	 * always put the UPS in a restartable mode, following the standard
	 * NUT behavior. Worse is better. (To prevent user confusion, we
	 * don't even report this variable, otherwise the user may attempt
	 * to change it using the front panel.
	 */
	NUT_UNUSED_VARIABLE(reg);
	return 0;
}


/*
 * Currently for internal use only, see comments above.
 */
static int ups2000_autostart_set(const uint16_t reg, const char *string)
{
	uint16_t val;
	int r;

	if (!strncasecmp(string, "yes", 3))
		val = 1;
	else if (!strncasecmp(string, "no", 2))
		val = 0;
	else
		return STAT_SET_INVALID;

	r = ups2000_write_register(modbus_ctx, reg + 10000, val);
	if (r != 1)
		return STAT_SET_FAILED;

	return STAT_SET_HANDLED;
}


static int ups2000_beeper_get(const uint16_t reg)
{
	uint16_t val;
	int r;

	r = ups2000_read_registers(modbus_ctx, reg + 10000, 1, &val);
	if (r != 1)
		return -1;

	if (val != 0 && val != 1)
		return -1;

	/*
	 * The register is "beeper disable", but we need to report whether it's
	 * enabled, thus we invert the boolean.
	 */
	if (val == 0)
		dstate_setinfo("ups.beeper.status", "enabled");
	else
		dstate_setinfo("ups.beeper.status", "disabled");

	dstate_setflags("ups.beeper.status", ST_FLAG_RW);
	dstate_addenum("ups.beeper.status", "enabled");
	dstate_addenum("ups.beeper.status", "disabled");

	return 0;
}


static int ups2000_beeper_set(const uint16_t reg, const char *string)
{
	uint16_t val;
	int r;

	if (!strcasecmp(string, "disabled") || !strcasecmp(string, "muted")) {
		/*
		 * Temporary "muted" is not supported. Only permanent "disabled"
		 * is. This is why we only support "beeper.disable" as an instant
		 * command, not "beeper.muted". But when setting it as a variable,
		 * we try to be robust here and treat both as synonyms.
		 */
		val = 1;
	}
	else if (!strcasecmp(string, "enabled"))
		val = 0;
	else
		return STAT_SET_INVALID;

	r = ups2000_write_register(modbus_ctx, reg + 10000, val);
	if (r != 1)
		return STAT_SET_FAILED;

	return STAT_SET_HANDLED;
}


/*
 * Note: variables "ups.delay.{shutdown,start,reboot}" are software-
 * only variables. We only get the user settings, validate its value
 * and store them as global variables. The actual hardware register
 * are only programmed when a shutdown/reboot is issued.
 */
static void ups2000_delay_get(void)
{
	char *cmdline;
	int i;
	int r;

	for (i = 0; ups2000_rw_delay[i].name != NULL; i++) {
		struct ups2000_delay_t *delay;

		delay = &ups2000_rw_delay[i];
		if (*delay->global_var == UPS2000_DELAY_INVALID) {
			cmdline = getval(delay->varname_cmdline);
			if (cmdline) {
				r = ups2000_delay_set(delay->name, cmdline);
				if (r != STAT_SET_HANDLED) {
					upslogx(LOG_ERR, "servar: %s is invalid. "
							 "Reverting to default %s %d seconds",
							 delay->varname_cmdline,
							 delay->varname_cmdline,
							 delay->dfault);
					*delay->global_var = delay->dfault;
				}
			}
			else {
				*delay->global_var = delay->dfault;
				upslogx(LOG_INFO, "setvar: use default %s %d seconds",
					delay->varname_cmdline, delay->dfault);
			}
		}

		dstate_setinfo(delay->name, "%d", *delay->global_var);
		dstate_setflags(delay->name, ST_FLAG_RW);
		dstate_addrange(delay->name, delay->min, delay->max);
	}
}


static int ups2000_delay_set(const char *var, const char *string)
{
	struct ups2000_delay_t *delay_schema = NULL;
	uint16_t delay, delay_rounded;
	int i;
	int r;

	r = str_to_ushort_strict(string, &delay, 10);
	if (!r)
		return STAT_SET_INVALID;

	for (i = 0; ups2000_rw_delay[i].name != NULL; i++) {
		if (!strcmp(ups2000_rw_delay[i].name, var)) {
			delay_schema = &ups2000_rw_delay[i];
			break;
		}
	}

	if (!delay_schema)
		return STAT_SET_UNKNOWN;

	if (delay > delay_schema->max)
		return STAT_SET_INVALID;
	if (delay < delay_schema->min) {
		upslogx(LOG_NOTICE, "setvar: %s [%u] is too low, "
				    "it has been set to %u seconds\n",
				    delay_schema->varname_cmdline, delay,
				    delay_schema->min);
		delay = delay_schema->min;
	}

	if (delay % delay_schema->step != 0) {
		delay_rounded = delay + delay_schema->step - delay % delay_schema->step;
		upslogx(LOG_NOTICE, "setvar: %s [%u] is not a multiple of %d, "
				    "it has been rounded up to %u seconds\n",
				    delay_schema->varname_cmdline, delay,
				    delay_schema->step, delay_rounded);
		delay = delay_rounded;
	}

	*delay_schema->global_var = delay;
	return STAT_SET_HANDLED;
}


/*
 * A lookup table of all instant commands "cmd" and their
 * corresponding registers "reg". For each instant command,
 * it's handled by...
 *
 * 1. One register write, by writing "val1" to "reg1", the
 * simplest case.
 *
 * 2. Two register writes, by writing "val1" to "reg1", and
 * writing "val2" to "reg2". One after another.
 *
 * 3. Calling "*handler_func" and passing "reg1". This is
 * used to handle commands that needs additional processing.
 * If "reg1" is not necessary or unsuitable, "-1" is used.
 */
#define REG_NONE -1, -1

static struct ups2000_cmd_t {
	const char *cmd;
	const int16_t reg1, val1, reg2, val2;
	int (*const handler_func)(const uint16_t);
} ups2000_cmd[] =
{
	{ "test.battery.start.quick", 2028,  1, REG_NONE, NULL },
	{ "test.battery.start.deep",  2021,  1, REG_NONE, NULL },
	{ "test.battery.stop",        2023,  1, REG_NONE, NULL },
	{ "beeper.enable",            1046,  0, REG_NONE, NULL },
	{ "beeper.disable",           1046,  1, REG_NONE, NULL },
	{ "load.off",                 1045,  0, 1030, 1,  NULL },
	{ "bypass.stop",              1029,  1, 1045, 0,  NULL },
	{ "load.on",                  1029, -1, REG_NONE, ups2000_instcmd_load_on                  },
	{ "bypass.start",             REG_NONE, REG_NONE, ups2000_instcmd_bypass_start             },
	{ "beeper.toggle",            1046, -1, REG_NONE, ups2000_instcmd_beeper_toggle            },
	{ "shutdown.stayoff",         1049, -1, REG_NONE, ups2000_instcmd_shutdown_stayoff         },
	{ "shutdown.return",          REG_NONE, REG_NONE, ups2000_instcmd_shutdown_return          },
	{ "shutdown.reboot",          REG_NONE, REG_NONE, ups2000_instcmd_shutdown_reboot          },
	{ "shutdown.reboot.graceful", REG_NONE, REG_NONE, ups2000_instcmd_shutdown_reboot_graceful },
	{ NULL, -1, -1, -1, -1, NULL },
};


static void ups2000_init_instcmd(void)
{
	int i;

	for (i = 0; ups2000_cmd[i].cmd != NULL; i++) {
		dstate_addcmd(ups2000_cmd[i].cmd);
	}
}


static int instcmd(const char *cmd, const char *extra)
{
	int i;
	int status;
	struct ups2000_cmd_t *cmd_action = NULL;
	NUT_UNUSED_VARIABLE(extra);

	for (i = 0; ups2000_cmd[i].cmd != NULL; i++) {
		if (!strcasecmp(cmd, ups2000_cmd[i].cmd)) {
			cmd_action = &ups2000_cmd[i];
		}
	}

	if (!cmd_action) {
		upslogx(LOG_WARNING, "instcmd: command [%s] unknown", cmd);
		return STAT_INSTCMD_UNKNOWN;
	}

	if (cmd_action->handler_func) {
		/* handled by a function */
		status = cmd_action->handler_func(cmd_action->reg1);
	}
	else if (cmd_action->reg1 != -1 && cmd_action->val1 != -1) {
		/* handled by a register write */
		int r = ups2000_write_register(modbus_ctx,
					       10000 + cmd_action->reg1,
					       cmd_action->val1);
		if (r == 1)
			status = STAT_INSTCMD_HANDLED;
		else
			status = STAT_INSTCMD_FAILED;

		/*
		 * if the previous write succeeds and there is an additional
		 * register to write.
		 */
		if (r == 1 && cmd_action->reg2 != -1 && cmd_action->val2 != -1) {
			r = ups2000_write_register(modbus_ctx,
						   10000 + cmd_action->reg2,
						   cmd_action->val2);
			if (r == 1)
				status = STAT_INSTCMD_HANDLED;
			else
				status = STAT_INSTCMD_FAILED;
		}
	}
	else {
		fatalx(EXIT_FAILURE, "invalid ups2000_cmd table!");
	}

	if (status == STAT_INSTCMD_FAILED)
		upslogx(LOG_ERR, "instcmd: command [%s] failed", cmd);
	else if (status == STAT_INSTCMD_HANDLED)
		upslogx(LOG_INFO, "instcmd: command [%s] handled", cmd);
	return status;
}


static int ups2000_instcmd_load_on(const uint16_t reg)
{
	int r;
	const char *status;

	/* force refresh UPS status */
	status_init();
	r = ups2000_update_status();
	if (r != 0) {
		/*
		 * When the UPS status is updated, the code must set either OL, OB, OL ECO,
		 * BYPASS, or OFF. These five options are mutually exclusive. If the register
		 * value is invalid and set none of these flags, failure code 1 is returned.
		 */
		dstate_datastale();
		return STAT_INSTCMD_FAILED;
	}
	status_commit();

	status = dstate_getinfo("ups.status");
	if (strstr(status, "OFF")) {
		/* no warning needed, continue at ups2000_write_register() below */
	}
	else if (strstr(status, "OL") || strstr(status, "OB")) {
		/*
		 * "Turning it on" has no effect if it's already on. Log a warning
		 * while still accepting and executing the command.
		 */
		upslogx(LOG_WARNING, "load.on: UPS is already on.");
		upslogx(LOG_WARNING, "load.on: still executing command anyway.");
	}
	else if (strstr(status, "BYPASS")) {
		/*
		 * If it's in bypass mode, reject this command. The UPS would otherwise
		 * enter normal mode, but "load.on" is not supposed to affect the
		 * normal/bypass status. Also log an error and suggest "bypass.stop".
		 */
		upslogx(LOG_ERR, "load.on error: UPS is already on, and is in bypass mode. "
				 "To enter normal mode, use bypass.stop");
		return STAT_INSTCMD_FAILED;
	}
	else {
		/* unreachable, see comments for r != 0 at the beginning */
		upslogx(LOG_ERR, "load.on error: invalid ups.status (%s) detected. "
				 "Please file a bug report!", status);
		return STAT_INSTCMD_FAILED;
	}

	r = ups2000_write_register(modbus_ctx, 10000 + reg, 1);
	if (r != 1)
		return STAT_INSTCMD_FAILED;
	return STAT_INSTCMD_HANDLED;
}


static int ups2000_instcmd_bypass_start(const uint16_t reg)
{
	int r;
	NUT_UNUSED_VARIABLE(reg);

	/* force update alarms */
	alarm_init();
	r = ups2000_update_alarm();
	if (r != 0)
		return STAT_INSTCMD_FAILED;
	alarm_commit();

	/* bypass input has a power failure, refuse to bypass */
	if (!bypass_available) {
		upslogx(LOG_ERR, "bypass input is abnormal, refuse to enter bypass mode.");
		return STAT_INSTCMD_FAILED;
	}

	/* enable "bypass on shutdown" */
	r = ups2000_write_register(modbus_ctx, 10000 + 1045, 1);
	if (r != 1)
		return STAT_INSTCMD_FAILED;

	/* shutdown */
	r = ups2000_write_register(modbus_ctx, 10000 + 1030, 1);
	if (r != 1)
		return STAT_INSTCMD_FAILED;

	return STAT_INSTCMD_HANDLED;
}


static int ups2000_instcmd_beeper_toggle(const uint16_t reg)
{
	int r;
	const char *string;

	r = ups2000_beeper_get(reg);
	if (r != 0)
		return STAT_INSTCMD_FAILED;

	string = dstate_getinfo("ups.beeper.status");
	if (!strcasecmp(string, "enabled"))
		r = ups2000_beeper_set(reg, "disabled");
	else if (!strcasecmp(string, "disabled"))
		r = ups2000_beeper_set(reg, "enabled");
	else
		return STAT_INSTCMD_FAILED;

	if (r != STAT_SET_HANDLED)
		return STAT_INSTCMD_FAILED;

	return STAT_INSTCMD_HANDLED;
}


/*
 * "ups.shutdown.stayoff": wait an optional offdelay and shutdown.
 * When the grid power returns, stay off.
 */
static int ups2000_instcmd_shutdown_stayoff(const uint16_t reg)
{
	uint16_t val;
	int r;

	r = setvar("ups.start.auto", "no");
	if (r != STAT_SET_HANDLED)
		return STAT_INSTCMD_FAILED;

	val = ups2000_offdelay * 10;  /* scaling factor */
	val /= 60;                    /* convert to minutes */

	r = ups2000_write_register(modbus_ctx, 10000 + reg, val);
	if (r != 1)
		return STAT_INSTCMD_FAILED;

	return STAT_INSTCMD_HANDLED;
}


/*
 * Wait for "offdelay" second, turn off the load. Then, wait
 * for "ondelay" seconds. If the grid power still exists or
 * has returned after the timer, turn on the load. Otherwise,
 * shutdown the UPS. When combined with "ups.start.auto", it
 * guarantees the server can always be restarted even if there
 * is a power race.
 *
 * "shutdown.return", "shutdown.reboot" and "shutdown.reboot.
 * graceful" all rely on this function.
 */
static int ups2000_shutdown_guaranteed_return(uint16_t offdelay, uint16_t ondelay)
{
	int r;
	uint16_t val[2];

	r = setvar("ups.start.auto", "yes");
	if (r != STAT_SET_HANDLED)
		return STAT_INSTCMD_FAILED;

	val[0] = (offdelay * 10) / 60;
	val[1] = ondelay / 60;

	r = ups2000_write_registers(modbus_ctx, 1047 + 10000, 2, val);
	if (r != 2)
		return STAT_INSTCMD_FAILED;

	return STAT_INSTCMD_HANDLED;
}


/*
 * "ups.shutdown.return": wait an optional "offdelay" and shutdown.
 * When the grid power returns, power on the load.
 */
static int ups2000_instcmd_shutdown_return(const uint16_t reg)
{
	int r;
	NUT_UNUSED_VARIABLE(reg);

	r = ups2000_shutdown_guaranteed_return(ups2000_offdelay,
					       ups2000_ondelay);
	if (r == STAT_INSTCMD_HANDLED) {
		shutdown_at = time_seek(time(NULL), ups2000_offdelay);
	}
	return r;
}


/*
 * "ups.shutdown.reboot": shutdown as soon as possible using the
 * smallest "rebootdelay" (inside the UPS, it's the same "ondelay"
 * timer), restart after an "ondelay".
 *
 * In our implementation, it's like "ups.shutdown.return", just
 * with a minimal "ondelay".
 */
static int ups2000_instcmd_shutdown_reboot(const uint16_t reg)
{
	int r;
	NUT_UNUSED_VARIABLE(reg);

	r = ups2000_shutdown_guaranteed_return(ups2000_rw_delay[REBOOT].min,
					       ups2000_ondelay);
	if (r == STAT_INSTCMD_HANDLED) {
		reboot_at = time_seek(time(NULL), ups2000_rw_delay[REBOOT].min);
		start_at = time_seek(reboot_at, ups2000_ondelay);
	}
	return r;
}


/*
 * "ups.shutdown.reboot.graceful": shutdown after a "rebootdelay"
 * (inside the UPS, it's the same "ondelay" timer), restart after
 * an "ondelay".
 *
 * In our implementation, it's like "ups.shutdown.return", just
 * with a "rebootdelay" instead of an "ondelay".
 */
static int ups2000_instcmd_shutdown_reboot_graceful(const uint16_t reg)
{
	int r;
	NUT_UNUSED_VARIABLE(reg);

	r = ups2000_shutdown_guaranteed_return(ups2000_rebootdelay,
					       ups2000_ondelay);
	if (r == STAT_INSTCMD_HANDLED) {
		reboot_at = time_seek(time(NULL), ups2000_rebootdelay);
		start_at = time_seek(reboot_at, ups2000_ondelay);
	}
	return r;
}


/*
 * List of countdown timers and pointers to their corresponding
 * global variables "at_time". They record estimated timestamps
 * when the actions are supposed to be performed.
 */
static struct {
	const char *name;
	time_t *const at_time;
} ups2000_timers[] = {
	{ "ups.timer.reboot",   &reboot_at   },
	{ "ups.timer.shutdown", &shutdown_at },
	{ "ups.timer.start",    &start_at    },
	{ NULL, NULL },
};


static int ups2000_update_timers(void)
{
	time_t now;
	int eta;
	int i;

	now = time(NULL);

	for (i = 0; ups2000_timers[i].name != NULL; i++) {
		if (*ups2000_timers[i].at_time) {
			eta = difftime(*ups2000_timers[i].at_time, now);
			if (eta < 0)
				eta = 0;
			dstate_setinfo(ups2000_timers[i].name, "%d", eta);
		}
		else {
			dstate_setinfo(ups2000_timers[i].name, "%d", -1);
		}
	}
	return 0;
}


void upsdrv_shutdown(void)
{
	int r;

	r = instcmd("shutdown.reboot", "");
	if (r != STAT_INSTCMD_HANDLED)
		fatalx(EXIT_FAILURE, "upsdrv_shutdown failed!");
}


void upsdrv_help(void)
{
}


/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	char msg[64];

	snprintf(msg, 64, "Set shutdown delay, in seconds, 6-second step"
			  " (default=%d)", ups2000_rw_delay[SHUTDOWN].dfault);
	addvar(VAR_VALUE, "offdelay", msg);

	snprintf(msg, 64, "Set reboot delay, in seconds, 6-second step"
			  " (default=%d).", ups2000_rw_delay[REBOOT].dfault);
	addvar(VAR_VALUE, "rebootdelay", msg);

	snprintf(msg, 64, "Set start delay, in seconds, 60-second step"
			  " (default=%d).", ups2000_rw_delay[START].dfault);
	addvar(VAR_VALUE, "ondelay", msg);
}


void upsdrv_cleanup(void)
{
	if (modbus_ctx != NULL) {
		modbus_close(modbus_ctx);
		modbus_free(modbus_ctx);
	}
	ser_close(upsfd, device_path);
}


/*
 * Seek time "t" forward or backward by n "seconds" without assuming
 * the underlying type and format of "time_t". This ensures maximum
 * portability. Although on POSIX and many other systems, "time_t"
 * is guaranteed to be in seconds.
 *
 * On error, abort the program.
 */
static time_t time_seek(time_t t, int seconds)
{
	struct tm time_tm;
	time_t time_output;

	if (!t)
		fatalx(EXIT_FAILURE, "time_seek() failed!");

	if (!gmtime_r(&t, &time_tm))
		fatalx(EXIT_FAILURE, "time_seek() failed!");

	time_tm.tm_sec += seconds;
	time_output = mktime(&time_tm);

	if (time_output == (time_t) -1)
		fatalx(EXIT_FAILURE, "time_seek() failed!");

	return time_output;
}


/*
 * Read bytes from the UPS2000 serial port, until the buffer has
 * nothing left to read (after ser_get_buf() times out). The buffer
 * size is limited to buf_len (inclusive).
 *
 * On error, return 0.
 *
 * In the serial library, ser_get_buf() can be a short read, and
 * ser_get_buf_let() requires a precalculated length, necessiates
 * our own read function.
 */
static size_t ups2000_read_serial(uint8_t *buf, size_t buf_len)
{
	ssize_t bytes = 0;
	size_t total = 0;

	/* wait 400 ms for the device to process our command */
	usleep(400 * 1000);

	while (buf_len > 0) {
		bytes = ser_get_buf(upsfd, buf, buf_len, 1, 0);
		if (bytes == -1)
			return 0;      /* read failure */
		else if (bytes == 0)
			return total;  /* nothing to read */

		total += bytes;        /* increment byte counter */
		buf += bytes;          /* advance buffer position */
		buf_len -= bytes;      /* decrement limiter */
	}
	return 0;  /* buffer exhaustion */
}


/*
 * Retry control. By default, we are in RETRY_ENABLE mode. For each
 * register read, we retry three times (1 sec. between each attempt),
 * before raising a fatal error and giving up. So far so good, but,
 * if the link went down, all operation would fail and the program
 * would become unresponsive due to excessive retrys.
 *
 * To prevent this problem, after the first fatal error, we stop all
 * retry attempts by entering RETRY_DISABLE_TEMPORARY mode, allowing
 * subsequent operation to fail without retry. Later, after the first
 * success is encountered, we move back to RETRY_ENABLE mode.
 */
enum {
	RETRY_ENABLE,
	RETRY_DISABLE_TEMPORARY
};
static int retry_status = RETRY_ENABLE;


/*
 * Read one or more registers using libmodbus.
 *
 * This is simply a wrapper for libmodbus's modbus_read_registers() with
 * retry and workaround logic. When an error has occured, we retry 3 times
 * before giving up, allowing us to recover from non-fatal failures without
 * triggering a data stale.
 */
static int ups2000_read_registers(modbus_t *ctx, int addr, int nb, uint16_t *dest)
{
	int i;
	int r = -1;

	if (addr < 10000)
		upslogx(LOG_ERR, "Invalid register read from %04d detected. "
				 "Please file a bug report!", addr);

	for (i = 0; i < 3; i++) {
		r = modbus_read_registers(ctx, addr, nb, dest);

		/* generic retry for modbus read failures. */
		if (retry_status == RETRY_ENABLE && r != nb) {
			upslogx(LOG_WARNING, "Register %04d has a read failure. Retrying...", addr);
			sleep(1);
			continue;
		}
		else if (r == nb)
			retry_status = RETRY_ENABLE;

		/*
		 * Workaround for buggy register 2002 (battery status). Sometimes
		 * this register returns invalid values. This is a known problem
		 * and it's not fatal, so we use LOG_INFO.
		 */
		if (retry_status == RETRY_ENABLE &&
		    addr == 12002 && (dest[0] < 2 || dest[0] > 5)) {
			upslogx(LOG_INFO, "Battery status has a non-fatal read failure, it's usually harmless. Retrying... ");
			sleep(1);
			continue;
		}
		else if (addr == 12002 && dest[0] >= 2 && dest[0] <= 5)
			retry_status = RETRY_ENABLE;

		return r;
	}

	/* Give up */
	upslogx(LOG_WARNING, "Register %04d has a fatal read failure.", addr);
	retry_status = RETRY_DISABLE_TEMPORARY;
	return r;
}


static int ups2000_write_registers(modbus_t *ctx, int addr, int nb, uint16_t *src)
{
	int i;
	int r = -1;

	if (addr < 10000)
		upslogx(LOG_ERR, "Invalid register write to %04d detected. "
				 "Please file a bug report!", addr);

	for (i = 0; i < 3; i++) {
		r = modbus_write_registers(ctx, addr, nb, src);

		/* generic retry for modbus write failures. */
		if (retry_status == RETRY_ENABLE && r != nb) {
			upslogx(LOG_WARNING, "Register %04d has a write failure. Retrying...", addr);
			sleep(1);
			continue;
		}
		else if (r == nb)
			retry_status = RETRY_ENABLE;

		return r;
	}

	/* Give up */
	upslogx(LOG_WARNING, "Register %04d has a fatal write failure.", addr);
	retry_status = RETRY_DISABLE_TEMPORARY;
	return r;
}


static int ups2000_write_register(modbus_t *ctx, int addr, uint16_t val)
{
	return ups2000_write_registers(ctx, addr, 1, &val);
}


/*
 * The following CRC-16 code was copied from libmodbus.
 *
 * Copyright (C) 2001-2011 Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

/* Table of CRC values for high-order byte */
static const uint8_t table_crc_hi[] = {
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};

/* Table of CRC values for low-order byte */
static const uint8_t table_crc_lo[] = {
	0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
	0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
	0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
	0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
	0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
	0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
	0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
	0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
	0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
	0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
	0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
	0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
	0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
	0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
	0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
	0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
	0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
	0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
	0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
	0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
	0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
	0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
	0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
	0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
	0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
	0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};


static uint16_t crc16(uint8_t * buffer, uint16_t buffer_length)
{
	uint8_t crc_hi = 0xFF;	/* high CRC byte initialized */
	uint8_t crc_lo = 0xFF;	/* low CRC byte initialized */
	unsigned int i;		/* will index into CRC lookup */

	/* pass through message buffer */
	while (buffer_length--) {
		i = crc_hi ^ *buffer++;	/* calculate the CRC  */
		crc_hi = crc_lo ^ table_crc_hi[i];
		crc_lo = table_crc_lo[i];
	}

	return ((uint16_t)((uint16_t)(crc_hi) << 8) | (uint16_t)crc_lo);
}
