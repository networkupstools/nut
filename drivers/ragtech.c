/* ragtech.c - driver for Ragtech UPSes (Easy Pro family — NEP/TORO/INNERGIE/OneUP)
 *
 * Target devices: Ragtech UPSes exposing a USB CDC-ACM serial interface
 * (PIC firmware, USB VID 0x04D8, PID 0x000A). Connected as /dev/ttyACM*.
 *
 * Protocol notes (derived from Ragtech's OEM devices.xml — family 10 — and
 * cross-checked against UPS_ESP32_tinySrv, https://github.com/antunesls/UPS_ESP32_tinySrv).
 *
 *   Line config: 8N1 (the CDC-ACM endpoint ignores baud, but the OEM client
 *   sets it to 2560). DTR=0 and RTS=0 MUST be asserted right after opening —
 *   non-zero levels are interpreted by some Ragtech families as a remote
 *   shutdown signal.
 *
 *   The UPS exposes a flat register address space; the client reads contiguous
 *   ranges of bytes by address. Family 10 advertises five ranges (devices.xml):
 *
 *     0x0080..0x009D   30 bytes — main status range, polled every cycle
 *     0x00F3..0x00F3    1 byte  — V_IOUTCALIB (per-unit current calibration)
 *     0x0136..0x0136    1 byte  — V_CAPBATNEW (replacement battery capacity)
 *     0x0171..0x0174    4 bytes — RGB LED + random
 *     0x0202..0x0203    2 bytes — V_OSC53 / V_OSC57 (frequency calibration)
 *
 *   Read command (6 bytes):
 *     0xAA 0x04 ADDR_HI ADDR_LO COUNT CHECKSUM
 *     where CHECKSUM = (ADDR_HI + ADDR_LO + COUNT) & 0xFF.
 *
 *   Reply: 1 + COUNT bytes — SOF=0xAA followed by COUNT raw register bytes.
 *   No checksum in the reply; we resynchronise on SOF.
 *
 *   Write command (6 bytes):
 *     0xAA OPCODE ADDR_HI ADDR_LO VALUE CHECKSUM
 *     CHECKSUM = (ADDR_HI + ADDR_LO + VALUE) & 0xFF.
 *
 *   Four opcodes mapped:
 *     0x01   write byte (reg = VALUE)
 *     0x02   AND mask   (reg = reg & VALUE)   — atomic bit-clear
 *     0x03   OR  mask   (reg = reg | VALUE)   — atomic bit-set
 *     0x04   read range (with VALUE = byte count)
 *
 *   The 0x02/0x03 atomic forms are required for any operation that has
 *   to trip a firmware state machine (e.g. arming the shutdown counter
 *   only works if F_AUTOSTART was modified via 0x02 / 0x03; a plain 0x01
 *   write to the same byte does not). Opcodes 0x05..0x07 were probed
 *   and have no observable effect.
 *
 *   Replies to writes (if any) are not consumed by the OEM client; we
 *   drain whatever comes back.
 *
 *   Family 10 register map (buf[i] = reg(0x80 + i - 1) for the main range):
 *
 *     buf[ 1]  0x80  F_AUTOSTART(0) F_ICBATTERY(2) F_LINESENS(7)
 *     buf[ 8]  0x87  V_CBATTERY      battery.charge      = raw * 0.3930
 *     buf[11]  0x8A  V_VBATTERY      battery.voltage     = raw * 0.0670 (or 0.1340 for 24V models)
 *     buf[12]  0x8B  V_VINPUT        input.voltage       = raw * 1.0600
 *     buf[13]  0x8C  V_IOUTPUT       output.current      = raw * model_imult / iout_calib
 *     buf[14]  0x8D  V_POUTPUT       firmware load %, integer; floors to 0
 *                                   below ~1%. Not used — ups.load is
 *                                   computed as (V_out * I_out) / VA * 100.
 *     buf[15]  0x8E  V_TEMPER        ups.temperature     = raw (°C)
 *     buf[17]  0x90  F_NOBAT(0) F_OLDBAT(1) F_OPCHECKUP(2) F_NOVINPUT(3)
 *                   F_LOVINPUT(4) F_HIVINPUT(5) F_OPBATTERY(6) F_HIPOUTPUT(7)
 *     buf[18]  0x91  F_LOBATTERY(0) F_FOVERTEMP(1) F_FENDBATTERY(2) F_FOVERLOAD(3)
 *                   F_FABNORMALVOUT(4) F_FABNORMALVBAT(5) F_FINVERTER(6) F_FSHORTCIRCUIT(7)
 *     buf[19]  0x92  F_SYNCIN(0) F_SUPERVON(1) F_MOREBAT(2) F_LESSBAT(3)
 *                   F_RCTRLON(5) F_POWERLOON(6) F_NIGHTOFFON(7)
 *     buf[22]  0x95  F_TRANSFINV(7)
 *     buf[24]  0x97  V_FOUTPUT       output.frequency    = interp(V_FOUTPUT, V_OSC53, V_OSC57)
 *     buf[25]  0x98  V_SHUTDOWNTIMER
 *     buf[27]  0x9A  V_MODEL         model id (see ragtech_models[])
 *     buf[28]  0x9B  V_VERSION       ups.firmware        = raw * 0.1
 *     buf[30]  0x9D  V_VOUTPUT       output.voltage      = raw * model_vmult
 *
 *   Frequency formula (OSC57 != OSC53):
 *     fOutput = (53.0 + 4.0 * (V_FOUTPUT - V_OSC53) / (V_OSC57 - V_OSC53)) * 0.9806 + 1.46
 *
 *   The pre-poll handshake "0xFF 0xFE 0x00 0x8E 0x01 0x8F" sent by the OEM
 *   firmware has unknown semantics; treating as opaque wake-up.
 *
 *   Write/action commands (shutdown, fullDischarge, setLineSens, ...) are
 *   declared in the OEM XML as register-level writes (e.g. shutdown writes
 *   V_SHUTDOWNTIMER at 0x98 plus a bit of F_AUTOSTART at 0x80) but the on-wire
 *   encoding of the write command is not yet captured. Strace of the OEM
 *   supsvc against /dev/ttyACM0 should reveal it.
 *
 *
 * Copyright (C) 2026  juslex
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
 */

#include "main.h"
#include "serial.h"
#include "nut_stdint.h"

#define DRIVER_NAME	"Ragtech UPS driver"
#define DRIVER_VERSION	"0.07"

upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"juslex",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define RAGTECH_SOF		0xAA
#define RAGTECH_CMD_READ	0x04
#define RAGTECH_CMD_WRITE	0x01
#define RAGTECH_CMD_AND		0x02
#define RAGTECH_CMD_OR		0x03
#define RAGTECH_MAIN_BASE	0x0080
#define RAGTECH_MAIN_LEN	30
#define RAGTECH_TIMEOUT_SEC	1
#define RAGTECH_TIMEOUT_USEC	0
#define RAGTECH_POST_OPEN_MS	200
#define RAGTECH_INTER_CMD_MS	100

/* Opaque wake-up that the OEM firmware sends once before the first poll. */
static const uint8_t cmd_handshake[] = { 0xFF, 0xFE, 0x00, 0x8E, 0x01, 0x8F };

/* offsets into the 30-byte main range (buf[1..30] = reg 0x80..0x9D) */
#define OFF_R(reg)		(1 + ((reg) - 0x80))
#define OFF_F_FLAGS		OFF_R(0x80)
#define OFF_V_CBATTERY		OFF_R(0x87)
#define OFF_V_VBATTERY		OFF_R(0x8A)
#define OFF_V_VINPUT		OFF_R(0x8B)
#define OFF_V_IOUTPUT		OFF_R(0x8C)
#define OFF_V_POUTPUT		OFF_R(0x8D)
#define OFF_V_TEMPER		OFF_R(0x8E)
#define OFF_F_STATUS		OFF_R(0x90)
#define OFF_F_FAULT		OFF_R(0x91)
#define OFF_F_MISC		OFF_R(0x92)
#define OFF_V_FOUTPUT		OFF_R(0x97)
#define OFF_V_SHUTTIMER		OFF_R(0x98)
#define OFF_V_MODEL		OFF_R(0x9A)
#define OFF_V_VERSION		OFF_R(0x9B)
#define OFF_V_VOUTPUT		OFF_R(0x9D)

/* reg 0x80 bits */
#define B_AUTOSTART		0x01
#define B_ICBATTERY		0x04
#define B_LINESENS		0x80
/* reg 0x90 bits (status) */
#define S_NO_BATTERY		0x01
#define S_OLD_BATTERY		0x02
#define S_SELF_TEST		0x04
#define S_NO_INPUT		0x08
#define S_INPUT_LOW		0x10
#define S_INPUT_HIGH		0x20
#define S_ON_BATTERY		0x40
#define S_OVERLOAD_WARN		0x80
/* reg 0x91 bits (fault) */
#define F_BATTERY_LOW		0x01
#define F_OVERTEMP		0x02
#define F_BATTERY_DEPLETED	0x04
#define F_OVERLOAD		0x08
#define F_OUTPUT_ABNORMAL	0x10
#define F_BATTERY_ABNORMAL	0x20
#define F_INVERTER_FAIL		0x40
#define F_SHORT_CIRCUIT		0x80
/* reg 0x92 bits (misc) */
#define M_SYNC_IN		0x01
#define M_MORE_BAT		0x04
#define M_LESS_BAT		0x08
#define M_REMOTE_CTL		0x20

struct ragtech_model {
	uint8_t id;
	const char *name;
	unsigned int va;
	double imult;		/* V_IOUTPUT scaling */
	double vmult;		/* V_VOUTPUT scaling (0.555 for 115V TI, 1.030 for 220V M2) */
	double bmult;		/* V_VBATTERY scaling (0.067 for 12V, 0.134 for 24V) */
	unsigned int vin_nominal;
	unsigned int vout_nominal;
	double pf;		/* power factor (0.7 per OneUP Nitro 2000 / Easy 2000 TI datasheet;
				 * assumed shared across family 10 until per-model data arrives) */
};

/* family 10 — order matches V_MODEL ids */
static const struct ragtech_model ragtech_models[] = {
	{  0, "Easy 600 TI",   600,  3.2000, 0.5550, 0.0670, 115, 115, 0.7 },
	{  1, "Easy 600 M2",   600,  1.7000, 1.0300, 0.0670, 220, 220, 0.7 },
	{  2, "Easy 700 TI",   700,  3.2000, 0.5550, 0.0670, 115, 115, 0.7 },
	{  3, "Easy 700 M2",   700,  2.3000, 1.0300, 0.0670, 220, 220, 0.7 },
	{  4, "Easy 900 TI",   900,  4.9400, 0.5550, 0.0670, 115, 115, 0.7 },
	{  5, "Easy 900 M2",   900,  2.6400, 1.0300, 0.0670, 220, 220, 0.7 },
	{  6, "Easy 1200 TI", 1200,  6.3700, 0.5550, 0.0670, 115, 115, 0.7 },
	{  7, "Easy 1200 M2", 1200,  3.4100, 1.0300, 0.0670, 220, 220, 0.7 },
	{  8, "Easy 1300 TI", 1300,  9.5600, 0.5550, 0.1340, 115, 115, 0.7 },
	{  9, "Easy 1300 M2", 1300,  5.1100, 1.0300, 0.1340, 220, 220, 0.7 },
	{ 10, "Easy 1400 TI", 1400, 11.1500, 0.5550, 0.1340, 115, 115, 0.7 },
	{ 11, "Easy 1400 M2", 1400,  5.9600, 1.0300, 0.1340, 220, 220, 0.7 },
	{ 12, "Easy 1600 TI", 1600, 12.8000, 0.5550, 0.1340, 115, 115, 0.7 },
	{ 13, "Easy 1600 M2", 1600,  6.8000, 1.0300, 0.1340, 220, 220, 0.7 },
	{ 14, "Easy 1800 TI", 1800, 14.4000, 0.5550, 0.1340, 115, 115, 0.7 },
	{ 15, "Easy 1800 M2", 1800,  7.6500, 1.0300, 0.1340, 220, 220, 0.7 },
	{ 16, "Easy 2000 TI", 2000, 15.9300, 0.5550, 0.1340, 115, 115, 0.7 },
	{ 17, "Easy 2000 M2", 2000,  8.5200, 1.0300, 0.1340, 220, 220, 0.7 },
	{ 18, "Easy 2200 TI", 2200, 17.5200, 0.5550, 0.1340, 115, 115, 0.7 },
	{ 19, "Easy 2200 M2", 2200,  9.3700, 1.0300, 0.1340, 220, 220, 0.7 },
};

/* per-session state populated by upsdrv_initinfo */
static const struct ragtech_model *model;
static struct ragtech_model fallback_model = {
	0xFF, "Unknown", 0, 6.3700, 0.5550, 0.0670, 115, 115, 0.7
};
static double iout_calib = 16.0;	/* read from reg 0xF3 at init */
static uint8_t osc53, osc57;		/* read from 0x202..0x203 at init */
static int shutdown_enabled;		/* opt-in via ups.conf "allow_shutdown" */

static const struct ragtech_model *find_model(uint8_t id)
{
	size_t i;
	for (i = 0; i < sizeof(ragtech_models) / sizeof(ragtech_models[0]); i++)
		if (ragtech_models[i].id == id)
			return &ragtech_models[i];
	return NULL;
}

/* Send a read for `count` registers starting at `addr` and collect the reply.
 * The reply is 1 + count bytes: SOF (0xAA) followed by count register bytes.
 * Returns count on success, -1 on failure. The SOF is consumed; `out` receives
 * only register bytes. */
static ssize_t ragtech_read(uint16_t addr, uint8_t count, uint8_t *out)
{
	uint8_t cmd[6];
	uint8_t scratch[64];
	const size_t reply_len = (size_t)count + 1;
	size_t total = 0;
	int got_sof = 0;
	int attempts = 0;
	ssize_t n;

	cmd[0] = RAGTECH_SOF;
	cmd[1] = RAGTECH_CMD_READ;
	cmd[2] = (addr >> 8) & 0xFF;
	cmd[3] = addr & 0xFF;
	cmd[4] = count;
	cmd[5] = (cmd[2] + cmd[3] + cmd[4]) & 0xFF;

	/* tcflush() — ser_flush_in() does a select+read loop that misses
	 * bytes still queued inside the USB CDC layer under O_NONBLOCK */
	ser_flush_io(upsfd);
	upsdebug_hex(4, "TX", cmd, sizeof(cmd));
	if (ser_send_buf(upsfd, cmd, sizeof(cmd)) != (ssize_t)sizeof(cmd)) {
		upsdebugx(2, "ragtech_read(0x%04X,%u): short TX", addr, count);
		return -1;
	}
	tcdrain(upsfd);
	usleep(50000);	/* let the UPS firmware queue the reply */

	while (total < (size_t)count && attempts++ < 10) {
		/* ser_get_buf returns what it manages to read in one select+read;
		 * ser_get_buf_len would discard partial reads on timeout, which
		 * happens here because the USB CDC layer fragments the reply. */
		n = ser_get_buf(upsfd, scratch, sizeof(scratch),
				RAGTECH_TIMEOUT_SEC, RAGTECH_TIMEOUT_USEC);
		if (n <= 0)
			break;
		upsdebug_hex(4, "RX chunk", scratch, (size_t)n);

		for (ssize_t i = 0; i < n; i++) {
			if (!got_sof) {
				if (scratch[i] == RAGTECH_SOF)
					got_sof = 1;
				continue;
			}
			if (total < (size_t)count)
				out[total++] = scratch[i];
		}
	}

	if (total < (size_t)count) {
		upsdebugx(2, "ragtech_read(0x%04X,%u): short RX (%zu/%u)",
			  addr, count, total, count);
		return -1;
	}
	(void)reply_len;
	return (ssize_t)count;
}

/* Send a 6-byte write-style command (opcode + addr + value). Returns 0 on
 * success, -1 on TX failure. The UPS does not appear to ack; we drain
 * whatever comes back. */
static int ragtech_send_op(uint8_t opcode, uint16_t addr, uint8_t value)
{
	uint8_t cmd[6];
	uint8_t scratch[16];

	cmd[0] = RAGTECH_SOF;
	cmd[1] = opcode;
	cmd[2] = (addr >> 8) & 0xFF;
	cmd[3] = addr & 0xFF;
	cmd[4] = value;
	cmd[5] = (cmd[2] + cmd[3] + cmd[4]) & 0xFF;

	ser_flush_io(upsfd);
	upsdebug_hex(4, "WR", cmd, sizeof(cmd));
	if (ser_send_buf(upsfd, cmd, sizeof(cmd)) != (ssize_t)sizeof(cmd))
		return -1;
	tcdrain(upsfd);
	/* Best-effort drain of any echo / ack the firmware may send. */
	usleep(50000);
	ser_get_buf(upsfd, scratch, sizeof(scratch), 0, 100000);
	return 0;
}

/* Convenience wrappers. */
static int ragtech_write_reg(uint16_t addr, uint8_t value)
{
	return ragtech_send_op(RAGTECH_CMD_WRITE, addr, value);
}
static int ragtech_and_reg(uint16_t addr, uint8_t mask)
{
	/* reg = reg & mask (atomic on the firmware side). */
	return ragtech_send_op(RAGTECH_CMD_AND, addr, mask);
}
static int ragtech_or_reg(uint16_t addr, uint8_t mask)
{
	/* reg = reg | mask (atomic on the firmware side). */
	return ragtech_send_op(RAGTECH_CMD_OR, addr, mask);
}

/* Clear bits using the atomic AND opcode. */
static int ragtech_clear_bits(uint16_t addr, uint8_t bits)
{
	return ragtech_and_reg(addr, (uint8_t)~bits);
}

/* Set bits using the atomic OR opcode. */
static int ragtech_set_bits(uint16_t addr, uint8_t bits)
{
	return ragtech_or_reg(addr, bits);
}

static int instcmd(const char *cmdname, const char *extra)
{
	NUT_UNUSED_VARIABLE(extra);

	/* All shutdown / test instcmds are physically destructive on a UPS
	 * that does not auto-restart, so they are gated behind an explicit
	 * opt-in in ups.conf. */
	if (!shutdown_enabled
	 && (!strcasecmp(cmdname, "shutdown.return")
	  || !strcasecmp(cmdname, "shutdown.stayoff")
	  || !strcasecmp(cmdname, "shutdown.stop")
	  || !strcasecmp(cmdname, "test.battery.start.deep"))) {
		upslogx(LOG_WARNING,
			"%s refused: set 'allow_shutdown' in ups.conf to enable. "
			"WARNING: this UPS firmware does NOT auto-restart after a "
			"coordinated shutdown; the operator must press the power "
			"button when mains return.", cmdname);
		return STAT_INSTCMD_INVALID;
	}

	if (!strcasecmp(cmdname, "shutdown.return")) {
		/* The Easy 2000 TI firmware's shutdown state machine only fires
		 * when F_AUTOSTART is *cleared* via the atomic AND opcode (0x02);
		 * setting it via OR (0x03) followed by writing V_SHUTDOWNTIMER
		 * has been verified to leave the UPS idle. The OEM XML action is
		 *
		 *     shutdown = "F_AUTOSTART=$notmanual; V_SHUTDOWNTIMER=$counter"
		 *
		 * and the OEM `supsvc` always passes `$notmanual = 0` on user-
		 * initiated shutdown, which means real-world traffic only ever
		 * exercises the stayoff variant. Treat shutdown.return as a
		 * stayoff with a warning so upsmon's behaviour is consistent: the
		 * output is cut on schedule, but the operator must press the
		 * power button when mains come back. */
		upslogx(LOG_WARNING,
			"shutdown.return: firmware does not implement auto-restart; "
			"falling back to shutdown.stayoff (manual power-on required)");
		if (ragtech_clear_bits(0x0080, B_AUTOSTART) < 0)
			return STAT_INSTCMD_FAILED;
		if (ragtech_write_reg(0x0098, 30) < 0)
			return STAT_INSTCMD_FAILED;
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		/* OEM-confirmed sequence: AND 0x80 with 0xFE (clear F_AUTOSTART),
		 * then write V_SHUTDOWNTIMER. UPS stays off until power button. */
		if (ragtech_clear_bits(0x0080, B_AUTOSTART) < 0)
			return STAT_INSTCMD_FAILED;
		if (ragtech_write_reg(0x0098, 30) < 0)
			return STAT_INSTCMD_FAILED;
		upslogx(LOG_NOTICE, "shutdown.stayoff: UPS will cut output in ~30s and stay off");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stop")) {
		/* Setting V_SHUTDOWNTIMER back to 0 aborts the countdown. */
		if (ragtech_write_reg(0x0098, 0) < 0)
			return STAT_INSTCMD_FAILED;
		upslogx(LOG_NOTICE, "shutdown.stop: countdown aborted");
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start.deep")) {
		/* devices.xml: fullDischarge = F_OLDBAT=0; F_OPCHECKUP=1; F_TRANSFINV=1 */
		if (ragtech_clear_bits(0x0090, S_OLD_BATTERY) < 0
		 || ragtech_set_bits(0x0090, S_SELF_TEST) < 0
		 || ragtech_set_bits(0x0095, 0x80) < 0)
			return STAT_INSTCMD_FAILED;
		upslogx(LOG_NOTICE, "test.battery.start.deep: full discharge cycle initiated");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static double compute_frequency(uint8_t v_fout)
{
	if (osc57 == osc53)
		return 0.0;
	return (53.0 + 4.0 * ((double)v_fout - (double)osc53)
			/ ((double)osc57 - (double)osc53)) * 0.9806 + 1.46;
}

void upsdrv_initinfo(void)
{
	uint8_t reply[64];
	uint8_t osc[2];
	uint8_t calib;

	dstate_setinfo("ups.mfr", "%s", "Ragtech");
	dstate_setinfo("ups.model", "%s", "Unknown");

	if (ser_send_buf(upsfd, cmd_handshake, sizeof(cmd_handshake))
	    != (ssize_t)sizeof(cmd_handshake)) {
		upslogx(LOG_WARNING, "handshake TX failed");
	}
	usleep(RAGTECH_INTER_CMD_MS * 1000);

	if (ragtech_read(RAGTECH_MAIN_BASE, RAGTECH_MAIN_LEN, reply) < 0)
		fatalx(EXIT_FAILURE, "no reply to initial status poll — is the UPS connected and not held by another program (e.g. Ragtech supsvc)?");

	model = find_model(reply[OFF_V_MODEL - 1]);
	if (!model) {
		upslogx(LOG_WARNING, "unknown model id 0x%02X, using generic 115V profile",
			reply[OFF_V_MODEL - 1]);
		model = &fallback_model;
	}
	dstate_setinfo("ups.model", "%s", model->name);
	if (model->va) {
		dstate_setinfo("ups.power.nominal",     "%u", model->va);
		dstate_setinfo("ups.realpower.nominal", "%u",
			       (unsigned int)(model->va * model->pf + 0.5));
	}
	{
		double v_in_now = reply[OFF_V_VINPUT - 1] * 1.0600;
		dstate_setinfo("input.voltage.nominal", "%u",
			v_in_now > 150.0 ? 220 : 115);
	}
	dstate_setinfo("output.voltage.nominal", "%u", model->vout_nominal);
	dstate_setinfo("battery.voltage.nominal", "%u", model->bmult > 0.1 ? 24 : 12);
	dstate_setinfo("ups.firmware", "%.1f", reply[OFF_V_VERSION - 1] * 0.1);

	if (ragtech_read(0x00F3, 1, &calib) == 1 && calib > 0) {
		iout_calib = (double)calib;
		upsdebugx(1, "iout_calib from reg 0xF3 = %u", calib);
	}
	if (ragtech_read(0x0202, 2, osc) == 2) {
		osc53 = osc[0];
		osc57 = osc[1];
		upsdebugx(1, "osc53=%u osc57=%u", osc53, osc57);
	}

	if (shutdown_enabled) {
		dstate_addcmd("shutdown.return");
		dstate_addcmd("shutdown.stayoff");
		dstate_addcmd("shutdown.stop");
		dstate_addcmd("test.battery.start.deep");
	} else {
		upslogx(LOG_INFO,
			"shutdown / test.battery.start.deep instcmds are disabled "
			"by default. Set 'allow_shutdown' in ups.conf to enable; "
			"the firmware will not auto-restart after a shutdown.");
	}
	upsh.instcmd = instcmd;
}

void upsdrv_updateinfo(void)
{
	uint8_t r[RAGTECH_MAIN_LEN];
	uint8_t st, fa;
	double vout, iout, bcharge;
	int attempt;

	/* CDC-ACM occasionally returns a short/corrupt reply, more often when
	 * output current is high (bus noise). Retry the transaction a couple
	 * of times before declaring data stale, so single-poll glitches do
	 * not surface as COMMBAD/COMMOK flapping in upsmon. */
	for (attempt = 0; attempt < 3; attempt++) {
		if (ragtech_read(RAGTECH_MAIN_BASE, RAGTECH_MAIN_LEN, r) >= 0)
			break;
		usleep(100000);
	}
	if (attempt == 3) {
		dstate_datastale();
		return;
	}

	vout = r[OFF_V_VOUTPUT - 1] * model->vmult;
	iout = (r[OFF_V_IOUTPUT - 1] * model->imult) / iout_calib;
	/* OEM scaling 0.3930 makes raw=255 yield 100.2; clamp so a fully
	 * charged battery never crosses the [0,100] %-defined range. */
	bcharge = r[OFF_V_CBATTERY - 1] * 0.3930;
	if (bcharge > 100.0) bcharge = 100.0;

	dstate_setinfo("battery.charge",  "%.1f", bcharge);
	dstate_setinfo("battery.voltage", "%.2f", r[OFF_V_VBATTERY - 1]  * model->bmult);
	dstate_setinfo("input.voltage",   "%.1f", r[OFF_V_VINPUT - 1]    * 1.0600);
	dstate_setinfo("output.voltage",  "%.1f", vout);
	dstate_setinfo("output.current",  "%.2f", iout);
	/* Reg 0x8D reports load as integer percent and floors to 0 at sub-1%
	 * loads. Compute apparent-power load for accurate light-load readings. */
	dstate_setinfo("ups.load",        "%.1f",
		model->va > 0 ? (vout * iout) / model->va * 100.0 : 0.0);
	dstate_setinfo("ups.temperature", "%u",   r[OFF_V_TEMPER - 1]);
	dstate_setinfo("output.frequency", "%.2f",
		compute_frequency(r[OFF_V_FOUTPUT - 1]));

	st = r[OFF_F_STATUS - 1];
	fa = r[OFF_F_FAULT - 1];

	status_init();
	if (st & S_ON_BATTERY)
		status_set("OB");
	else
		status_set("OL");
	if (fa & F_BATTERY_LOW)
		status_set("LB");
	if (fa & F_OVERLOAD)
		status_set("OVER");
	if (st & S_SELF_TEST)
		status_set("CAL");
	if (st & (S_NO_BATTERY | S_OLD_BATTERY))
		status_set("RB");
	status_commit();

	if (fa & (F_OVERTEMP | F_INVERTER_FAIL | F_SHORT_CIRCUIT))
		alarm_init();
	if (fa & F_OVERTEMP)
		alarm_set("Overtemperature");
	if (fa & F_INVERTER_FAIL)
		alarm_set("Inverter fault");
	if (fa & F_SHORT_CIRCUIT)
		alarm_set("Short circuit");
	if (fa & (F_OVERTEMP | F_INVERTER_FAIL | F_SHORT_CIRCUIT))
		alarm_commit();

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	int handled;

	if (!shutdown_enabled) {
		upslogx(LOG_ERR,
			"upsdrv_shutdown invoked but 'allow_shutdown' is not set "
			"in ups.conf — refusing. This firmware does not auto-restart "
			"after shutdown; a coordinated shutdown would leave the UPS "
			"off until manual power-on.");
		set_exit_flag(EF_EXIT_FAILURE);
		return;
	}

	handled = instcmd("shutdown.return", NULL);
	set_exit_flag(handled == STAT_INSTCMD_HANDLED
		? EF_EXIT_SUCCESS : EF_EXIT_FAILURE);
}

void upsdrv_help(void)
{
}

void upsdrv_tweak_prognames(void)
{
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "iout_calib",
	       "Override per-unit output current calibration (default: read from reg 0xF3)");
	addvar(VAR_FLAG, "allow_shutdown",
	       "Enable the shutdown.* and test.battery.start.deep instcmds. "
	       "WARNING: this UPS firmware does not auto-restart after a "
	       "coordinated shutdown; manual power-button intervention is "
	       "required when mains return. Defaults to disabled.");
}

void upsdrv_initups(void)
{
	const char *v;

	/* CDC-ACM only: NEVER call tcsetattr (it pulses DTR on Linux and the
	 * UPS interprets that as a shutdown signal on some Ragtech families).
	 * Leave the port at whatever line settings the kernel set on enumeration
	 * — CDC-ACM ignores baud at the wire anyway. */
	upsfd = ser_open(device_path);
	usleep(RAGTECH_POST_OPEN_MS * 1000);

	v = getval("iout_calib");
	if (v) {
		double parsed = atof(v);
		if (parsed > 0.0)
			iout_calib = parsed;
	}

	shutdown_enabled = testvar("allow_shutdown") ? 1 : 0;
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
